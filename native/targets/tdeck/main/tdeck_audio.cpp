#include "tdeck_audio.h"

#include <cstdio>

#include "esp_err.h"
#include "esp_log.h"
#include "tdeck_pins.h"

namespace tdeck {
namespace {
constexpr char kTag[] = "OpenU5-Audio";
constexpr uint32_t kChunkFrames = 256; // 16 ms at 16 kHz; one DMA buffer
} // namespace

bool TdeckAudioBackend::play_sfx(const openu5::SfxRequest &request) {
    // A cue with no A3-02 program is declined here, on the game thread, so it
    // never costs a queue slot or wakes the audio task.
    if (!openu5::sfx_supported(request.id)) return false;
    if (!ensure_started()) return false;
    const Command command{request, epoch_.load()};
    if (xQueueSend(queue_, &command, 0) == pdTRUE) return true; // never waits
    queue_full_.fetch_add(1);
    return false;
}

void TdeckAudioBackend::stop_sfx() {
    // The audio task sees the new epoch before it takes the next command, so
    // everything posted before this call is dropped and the playing cue fades
    // out in 2 ms. Nothing to do if the task was never started.
    epoch_.fetch_add(1);
}

void TdeckAudioBackend::set_gain(openu5::AudioChannel channel, uint16_t gain_q15) {
    if (channel == openu5::AudioChannel::Sfx) sfx_gain_.store(gain_q15);
}

bool TdeckAudioBackend::ensure_started() {
    if (task_) return true;
    if (failed_) return false;
    auto fail = [&](const char *step, esp_err_t err) {
        std::snprintf(error_, sizeof(error_), "%s: %s", step, esp_err_to_name(err));
        ESP_LOGE(kTag, "AUDIO_BACKEND failed %s", error_);
        if (tx_) {
            i2s_del_channel(tx_);
            tx_ = nullptr;
        }
        if (queue_) {
            vQueueDelete(queue_);
            queue_ = nullptr;
        }
        failed_ = true;
        return false;
    };

    i2s_chan_config_t channel{};
    channel.id = I2S_NUM_0;
    channel.role = I2S_ROLE_MASTER;
    channel.dma_desc_num = 4;
    channel.dma_frame_num = kChunkFrames;
    channel.auto_clear_after_cb = true; // an underrun sends silence, never a stale buffer
    channel.auto_clear_before_cb = false;
    channel.allow_pd = false;
    channel.intr_priority = 0;
    channel.tx_destination = I2S_DESTINATION_DMA;
    channel.rx_destination = I2S_DESTINATION_DMA;
    esp_err_t err = i2s_new_channel(&channel, &tx_, nullptr);
    if (err != ESP_OK) return fail("i2s_new_channel", err);

    i2s_std_config_t config{};
    config.clk_cfg.sample_rate_hz = kSampleRateHz;
    config.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    config.clk_cfg.bclk_div = 8;
    config.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
    config.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    config.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    config.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH; // the one sample in both slots
    config.slot_cfg.ws_width = 16;
    config.slot_cfg.ws_pol = false;
    config.slot_cfg.bit_shift = true; // Philips (LilyGO SimpleTone: I2S_COMM_FORMAT_STAND_I2S)
#if SOC_I2S_HW_VERSION_1
    config.slot_cfg.msb_right = true;
#else
    config.slot_cfg.left_align = true;
    config.slot_cfg.big_endian = false;
    config.slot_cfg.bit_order_lsb = false;
#endif
    config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    config.gpio_cfg.bclk = pins::kSpeakerI2sBclk;
    config.gpio_cfg.ws = pins::kSpeakerI2sWs;
    config.gpio_cfg.dout = pins::kSpeakerI2sDout;
    config.gpio_cfg.din = I2S_GPIO_UNUSED;
    err = i2s_channel_init_std_mode(tx_, &config);
    if (err != ESP_OK) return fail("i2s_channel_init_std_mode", err);

    queue_ = xQueueCreate(kQueueDepth, sizeof(Command));
    if (!queue_) return fail("xQueueCreate", ESP_ERR_NO_MEM);
    // Core 1 runs no game task; the game loop and input capture are on core 0.
    if (xTaskCreatePinnedToCore(task_entry, "openu5-audio", 4096, this, 3, &task_, 1) != pdPASS) {
        task_ = nullptr;
        return fail("xTaskCreatePinnedToCore", ESP_ERR_NO_MEM);
    }
    ESP_LOGI(kTag, "AUDIO_BACKEND i2s ready rate=%u bits=16 mono bclk=%d ws=%d dout=%d core=1 synth=pc-speaker",
             unsigned(kSampleRateHz), int(pins::kSpeakerI2sBclk), int(pins::kSpeakerI2sWs),
             int(pins::kSpeakerI2sDout));
    return true;
}

void TdeckAudioBackend::task_entry(void *self) { static_cast<TdeckAudioBackend *>(self)->run(); }

// The audio task. It blocks in exactly two places: the queue while nothing
// sounds, and i2s_channel_write while something does -- never a busy loop, so
// it cannot starve the idle task's watchdog on core 1.
void TdeckAudioBackend::run() {
    int16_t chunk[kChunkFrames];
    size_t written = 0;
    bool enabled = false;
    for (;;) {
        Command command{};
        bool got = xQueueReceive(queue_, &command, player_.idle() ? portMAX_DELAY : 0) == pdTRUE;
        // The epoch first: a flush that happened while these commands were in
        // flight makes them stale.
        player_.sync_epoch(epoch_.load());
        while (got) {
            player_.submit(command.request, command.epoch);
            got = xQueueReceive(queue_, &command, 0) == pdTRUE;
        }
        if (player_.idle()) {
            if (enabled) {
                // Two chunks of silence flush the DMA ring, then the clocks go
                // off so the amplifier idles between effects.
                for (auto &s : chunk) s = 0;
                for (int i = 0; i < 2; ++i) i2s_channel_write(tx_, chunk, sizeof(chunk), &written, 200);
                i2s_channel_disable(tx_);
                enabled = false;
                const auto &st = player_.stats();
                ESP_LOGD(kTag, "SFX idle started=%lu queued=%lu coalesced=%lu preempted=%lu overflowed=%lu stale=%lu queue_full=%lu",
                         (unsigned long)st.started, (unsigned long)st.queued, (unsigned long)st.coalesced,
                         (unsigned long)st.preempted, (unsigned long)st.overflowed, (unsigned long)st.stale,
                         (unsigned long)queue_full_.load());
            }
            continue;
        }
        if (!enabled) {
            const esp_err_t on = i2s_channel_enable(tx_);
            if (on != ESP_OK) {
                ESP_LOGE(kTag, "AUDIO_BACKEND enable failed: %s", esp_err_to_name(on));
                player_.flush(); // drop it rather than spin on a dead channel
                player_.render(chunk, kChunkFrames, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
            enabled = true;
        }
        // The live SFX gain, applied once inside render(): a volume change
        // (0 included) is heard within one chunk.
        player_.render(chunk, kChunkFrames, sfx_gain_.load());
        i2s_channel_write(tx_, chunk, sizeof(chunk), &written, 200);
    }
}

} // namespace tdeck
