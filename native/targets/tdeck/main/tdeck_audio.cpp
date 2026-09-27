#include "tdeck_audio.h"

#include <cstdio>

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "tdeck_pins.h"

namespace tdeck {
namespace {
constexpr char kTag[] = "OpenU5-Audio";
/** One DMA descriptor takes 8 ms to play; a write that waited this long has hit a dead channel. */
constexpr uint32_t kWriteTimeoutMs = 200;
/** The audio task copies its window out for the game thread every 16 blocks (128 ms). */
constexpr uint32_t kPublishEveryBlocks = 16;
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

bool TdeckAudioBackend::start_music(openu5::MusicSong song, uint16_t gain_q15) {
    if (!ensure_started()) return false;
    music_gain_.store(gain_q15);
    const MusicCommand command{song, false};
    xQueueOverwrite(music_queue_, &command); // never waits; always "succeeds" (length-1 latest-wins)
    return true;
}

void TdeckAudioBackend::stop_music() {
    if (!music_queue_) return; // never started: there is nothing playing to stop
    const MusicCommand command{openu5::MusicSong::None, true};
    xQueueOverwrite(music_queue_, &command);
}

void TdeckAudioBackend::set_gain(openu5::AudioChannel channel, uint16_t gain_q15) {
    if (channel == openu5::AudioChannel::Sfx) sfx_gain_.store(gain_q15);
    else music_gain_.store(gain_q15);
}

void TdeckAudioBackend::set_music_library(const openu5::AudioPackPayload *library) {
    const int64_t t0 = esp_timer_get_time();
    const size_t playable = library_.load(library);
    const int64_t t1 = esp_timer_get_time();
    pump_.set_music_library(&library_);
    if (library && !library_.loaded())
        ESP_LOGE(kTag, "AUDIO_BACKEND music library bank failed to parse: songs will stay silent");
    else if (library)
        ESP_LOGI(kTag, "AUDIO_BACKEND music library songs=%u/%u events=%u parse_us=%lld", unsigned(playable),
                 unsigned(openu5::kMusicSongCount), unsigned(library_.event_count()), (long long)(t1 - t0));
}

bool TdeckAudioBackend::perf_snapshot(openu5::AudioPerfSnapshot &out) const {
    taskENTER_CRITICAL(&perf_lock_);
    out = published_;
    const bool valid = published_valid_;
    taskEXIT_CRITICAL(&perf_lock_);
    return valid;
}

void TdeckAudioBackend::perf_reset() { perf_reset_requested_.store(true); }

uint64_t TdeckAudioBackend::clock_us(void *) { return uint64_t(esp_timer_get_time()); }

// The I2S DMA ISR (core 0, where ensure_started() allocated it). One call per
// finished descriptor, and one per descriptor the DMA had to replay empty
// (the driver's send queue overflowed: nobody refilled it in time).
bool IRAM_ATTR TdeckAudioBackend::on_sent(i2s_chan_handle_t, i2s_event_data_t *, void *context) {
    auto *self = static_cast<TdeckAudioBackend *>(context);
    self->isr_played_ = self->isr_played_ + 1;
    return false;
}

bool IRAM_ATTR TdeckAudioBackend::on_send_q_ovf(i2s_chan_handle_t, i2s_event_data_t *, void *context) {
    auto *self = static_cast<TdeckAudioBackend *>(context);
    self->isr_overflows_ = self->isr_overflows_ + 1;
    return false;
}

bool TdeckAudioBackend::I2sRingSink::preload(const int16_t *block) {
    size_t loaded = 0;
    return i2s_channel_preload_data(owner_.tx_, block, kBlockBytes, &loaded) == ESP_OK && loaded == kBlockBytes;
}

bool TdeckAudioBackend::I2sRingSink::enable() {
    const esp_err_t on = i2s_channel_enable(owner_.tx_);
    if (on != ESP_OK) ESP_LOGE(kTag, "AUDIO_BACKEND enable failed: %s", esp_err_to_name(on));
    return on == ESP_OK;
}

void TdeckAudioBackend::I2sRingSink::disable() { i2s_channel_disable(owner_.tx_); }

bool TdeckAudioBackend::I2sRingSink::write(const int16_t *block) {
    size_t written = 0;
    owner_.active_ = 0; // A3-04C: blocked until the DMA frees a descriptor
    const bool ok = i2s_channel_write(owner_.tx_, block, kBlockBytes, &written, kWriteTimeoutMs) == ESP_OK &&
                    written == kBlockBytes;
    owner_.active_ = 1;
    return ok;
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
        if (music_queue_) {
            vQueueDelete(music_queue_);
            music_queue_ = nullptr;
        }
        failed_ = true;
        return false;
    };

    // The descriptor ring is the render-ahead buffer (ALPHA3_AUDIO.md section
    // 18.7): the pump keeps all of it full, so its depth is both the stall
    // tolerance and the SFX latency. A3-02..A3-04: 4 x 256 frames.
    i2s_chan_config_t channel{};
    channel.id = I2S_NUM_0;
    channel.role = I2S_ROLE_MASTER;
    channel.dma_desc_num = openu5::kAudioRingBlocks;
    channel.dma_frame_num = openu5::kAudioBlockFrames;
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

    // A3-04A: the driver's own view of the ring, for the pump's counters.
    i2s_event_callbacks_t callbacks{};
    callbacks.on_sent = &TdeckAudioBackend::on_sent;
    callbacks.on_send_q_ovf = &TdeckAudioBackend::on_send_q_ovf;
    err = i2s_channel_register_event_callback(tx_, &callbacks, this);
    if (err != ESP_OK) return fail("i2s_channel_register_event_callback", err);

    queue_ = xQueueCreate(kQueueDepth, sizeof(Command));
    if (!queue_) return fail("xQueueCreate", ESP_ERR_NO_MEM);
    music_queue_ = xQueueCreate(1, sizeof(MusicCommand)); // depth 1: xQueueOverwrite keeps only the latest
    if (!music_queue_) return fail("xQueueCreate(music)", ESP_ERR_NO_MEM);
    // Core 1 runs no game task; the game loop and input capture are on core 0.
    if (xTaskCreatePinnedToCore(task_entry, "openu5-audio", kTaskStackBytes, this, 3, &task_, 1) != pdPASS) {
        task_ = nullptr;
        return fail("xTaskCreatePinnedToCore", ESP_ERR_NO_MEM);
    }
    ESP_LOGI(kTag,
             "AUDIO_BACKEND i2s ready rate=%u bits=16 mono bclk=%d ws=%d dout=%d core=1 synth=pc-speaker+opl2 "
             "ring=%ux%u frames (%u ms ahead)",
             unsigned(kSampleRateHz), int(pins::kSpeakerI2sBclk), int(pins::kSpeakerI2sWs),
             int(pins::kSpeakerI2sDout), unsigned(openu5::kAudioRingBlocks), unsigned(openu5::kAudioBlockFrames),
             unsigned(openu5::kAudioRingBlocks * openu5::kAudioBlockUs / 1000));
    return true;
}

void TdeckAudioBackend::task_entry(void *self) { static_cast<TdeckAudioBackend *>(self)->run(); }

void TdeckAudioBackend::publish_perf() {
    blocks_since_publish_ = 0;
    const uint32_t stack_free = uint32_t(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t));
    if (stack_free < stack_free_min_) stack_free_min_ = stack_free;
    openu5::AudioPerfSnapshot snapshot{};
    pump_.perf(snapshot);
    snapshot.stack_free_min = stack_free_min_;
    taskENTER_CRITICAL(&perf_lock_);
    snapshot.seq = published_.seq + 1;
    published_ = snapshot;
    published_valid_ = true;
    taskEXIT_CRITICAL(&perf_lock_);
}

// The audio task. It blocks in exactly two places: i2s_channel_write inside
// pump_.step() while the channel runs (the DMA freeing a descriptor paces
// it), and a bounded wait on the SFX queue while the pump sleeps (bounded,
// not portMAX_DELAY, because a start_music() arrives on the SEPARATE
// music_queue_ and must not wait behind a SFX-only block). It never logs,
// parses or allocates here; the pump's runaway guard yields a tick if the
// producer ever falls behind real time for good (section 18.12), so the
// idle task on core 1 -- and the task watchdog -- always get to run.
void TdeckAudioBackend::run() {
    constexpr TickType_t kIdleWaitTicks = pdMS_TO_TICKS(20);
    pump_.set_clock({this, &TdeckAudioBackend::clock_us});
    pump_.reset_perf();
    active_ = 1;
    for (;;) {
        Command command{};
        // A3-04C: the flag says "blocked" only around a wait that can block.
        const bool may_block = pump_.sleeping();
        if (may_block) active_ = 0;
        bool got = xQueueReceive(queue_, &command, may_block ? kIdleWaitTicks : 0) == pdTRUE;
        active_ = 1;
        pump_.note_sfx_queue_depth(uint32_t(uxQueueMessagesWaiting(queue_)) + (got ? 1u : 0u));
        // The epoch first: a flush that happened while these commands were in
        // flight makes them stale.
        pump_.sync_epoch(epoch_.load());
        while (got) {
            pump_.submit_sfx(command.request, command.epoch);
            got = xQueueReceive(queue_, &command, 0) == pdTRUE;
        }
        MusicCommand music_command{};
        while (xQueueReceive(music_queue_, &music_command, 0) == pdTRUE) {
            if (music_command.stop) pump_.stop_music();
            else pump_.play_music(music_command.song);
        }
        if (perf_reset_requested_.exchange(false)) {
            pump_.reset_perf();
            blocks_since_publish_ = kPublishEveryBlocks; // publish the fresh, empty window right away
        }
        pump_.set_music_bypass(bypass_requested_.load()); // A3-04C probe, between blocks
        if (pump_.step(sink_, sfx_gain_.load(), music_gain_.load())) {
            active_ = 0;
            vTaskDelay(1);
            active_ = 1;
        }
        if (++blocks_since_publish_ >= kPublishEveryBlocks) publish_perf();
    }
}

} // namespace tdeck
