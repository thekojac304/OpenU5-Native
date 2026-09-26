#include "tdeck_audio.h"

#include <cstdio>

#include "esp_err.h"
#include "esp_log.h"
#include "tdeck_pins.h"

namespace tdeck {
namespace {
constexpr char kTag[] = "OpenU5-Audio";
constexpr uint32_t kChunkFrames = 256;
// The diagnostic tone: two square-wave notes, the PC speaker's own waveform,
// at a quarter of full scale before the SFX gain, with 3 ms edges so the
// amplifier does not click. 880 Hz for 120 ms, then 1320 Hz for 180 ms.
constexpr int32_t kToneAmplitude = 8192;
constexpr uint32_t kFadeFrames = TdeckAudioBackend::kSampleRateHz * 3 / 1000;
struct Note { uint32_t hz, ms; };
constexpr Note kNotes[] = {{880, 120}, {1320, 180}};
} // namespace

bool TdeckAudioBackend::play_sfx(const openu5::SfxRequest &request) {
    if (request.id != openu5::SfxId::DiagnosticTone) return false; // A3-02 adds the effects
    if (!ensure_started()) return false;
    return xQueueSend(queue_, &request, 0) == pdTRUE; // never waits
}

void TdeckAudioBackend::stop_sfx() {
    if (!queue_) return;
    xQueueReset(queue_);
    stop_.store(true);
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

    queue_ = xQueueCreate(4, sizeof(openu5::SfxRequest));
    if (!queue_) return fail("xQueueCreate", ESP_ERR_NO_MEM);
    // Core 1 runs no game task; the game loop and input capture are on core 0.
    if (xTaskCreatePinnedToCore(task_entry, "openu5-audio", 3072, this, 3, &task_, 1) != pdPASS) {
        task_ = nullptr;
        return fail("xTaskCreatePinnedToCore", ESP_ERR_NO_MEM);
    }
    ESP_LOGI(kTag, "AUDIO_BACKEND i2s ready rate=%u bits=16 mono bclk=%d ws=%d dout=%d core=1",
             unsigned(kSampleRateHz), int(pins::kSpeakerI2sBclk), int(pins::kSpeakerI2sWs),
             int(pins::kSpeakerI2sDout));
    return true;
}

void TdeckAudioBackend::task_entry(void *self) { static_cast<TdeckAudioBackend *>(self)->run(); }

void TdeckAudioBackend::run() {
    openu5::SfxRequest request{};
    for (;;) {
        if (xQueueReceive(queue_, &request, portMAX_DELAY) != pdTRUE) continue;
        stop_.store(false);
        if (request.id != openu5::SfxId::DiagnosticTone) continue;
        const esp_err_t on = i2s_channel_enable(tx_);
        if (on != ESP_OK) {
            ESP_LOGE(kTag, "AUDIO_BACKEND enable failed: %s", esp_err_to_name(on));
            continue;
        }
        render_tone();
        i2s_channel_disable(tx_); // clocks off between sounds: the amplifier idles
    }
}

// Runs on the audio task only. The blocking writes pace this task against the
// DMA ring; the game thread never waits on them.
void TdeckAudioBackend::render_tone() {
    int16_t chunk[kChunkFrames];
    size_t written = 0;
    uint32_t total = 0;
    for (const auto &note : kNotes) total += kSampleRateHz * note.ms / 1000;
    uint32_t frame = 0;
    for (const auto &note : kNotes) {
        const uint32_t frames = kSampleRateHz * note.ms / 1000;
        uint32_t phase = 0;
        for (uint32_t done = 0; done < frames && !stop_.load();) {
            // The live SFX gain (AudioService::attach sends it before any
            // request), so a volume change during the tone is heard, 0 included.
            const uint16_t gain = sfx_gain_.load();
            const uint32_t n = frames - done < kChunkFrames ? frames - done : kChunkFrames;
            for (uint32_t i = 0; i < n; ++i, ++frame) {
                phase += note.hz;
                if (phase >= kSampleRateHz) phase -= kSampleRateHz;
                int32_t level = phase < kSampleRateHz / 2 ? kToneAmplitude : -kToneAmplitude;
                const uint32_t edge = frame < kFadeFrames ? frame : total - frame < kFadeFrames ? total - frame : kFadeFrames;
                level = level * int32_t(edge) / int32_t(kFadeFrames);
                chunk[i] = openu5::apply_gain_q15(int16_t(level), gain);
            }
            i2s_channel_write(tx_, chunk, n * sizeof(int16_t), &written, 200);
            done += n;
        }
    }
    for (auto &s : chunk) s = 0;
    for (int i = 0; i < 2; ++i) i2s_channel_write(tx_, chunk, sizeof(chunk), &written, 200);
}

} // namespace tdeck
