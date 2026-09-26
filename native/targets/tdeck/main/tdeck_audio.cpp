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
    music_library_ = library;
    music_bank_loaded_ = library && library->bank && music_bank_.load(library->bank, library->bank_length);
    if (library && !music_bank_loaded_)
        ESP_LOGE(kTag, "AUDIO_BACKEND music library bank failed to parse: songs will stay silent");
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
    music_queue_ = xQueueCreate(1, sizeof(MusicCommand)); // depth 1: xQueueOverwrite keeps only the latest
    if (!music_queue_) return fail("xQueueCreate(music)", ESP_ERR_NO_MEM);
    // Core 1 runs no game task; the game loop and input capture are on core 0.
    // A3-04's OPL2 synth adds real per-sample work (ALPHA3_AUDIO.md section
    // 17.4); the stack grew from 4096 to 6144 B for its headroom, still well
    // inside PSRAM/internal RAM budget.
    if (xTaskCreatePinnedToCore(task_entry, "openu5-audio", 6144, this, 3, &task_, 1) != pdPASS) {
        task_ = nullptr;
        return fail("xTaskCreatePinnedToCore", ESP_ERR_NO_MEM);
    }
    ESP_LOGI(kTag, "AUDIO_BACKEND i2s ready rate=%u bits=16 mono bclk=%d ws=%d dout=%d core=1 synth=pc-speaker+opl2",
             unsigned(kSampleRateHz), int(pins::kSpeakerI2sBclk), int(pins::kSpeakerI2sWs),
             int(pins::kSpeakerI2sDout));
    return true;
}

void TdeckAudioBackend::apply_music_command(const MusicCommand &command) {
    if (command.stop) {
        music_player_.stop();
        music_song_ = openu5::MusicSong::None;
        return;
    }
    if (command.song == music_song_ && music_player_.active()) return; // AudioService already de-dupes, but be sure
    if (!music_bank_loaded_ || !music_library_) {
        music_player_.stop();
        music_song_ = openu5::MusicSong::None;
        return; // no library or an unparsable bank: silence, never a guess
    }
    const size_t id = size_t(command.song);
    if (id >= openu5::kMusicSongCount || !music_library_->song[id]) {
        music_player_.stop();
        music_song_ = openu5::MusicSong::None;
        return;
    }
    auto track = std::make_unique<openu5::MusicTrack>();
    if (!openu5::parse_xmi_events(music_library_->song[id], music_library_->song_length[id], *track)) {
        ESP_LOGE(kTag, "AUDIO_BACKEND song %u failed to parse: staying silent", unsigned(id));
        music_player_.stop();
        music_song_ = openu5::MusicSong::None;
        return;
    }
    // stop() FIRST: it clears music_player_'s internal pointer into the old
    // music_track_ before that object is destroyed by the reassignment below.
    // Never a window where the player could hold a dangling track pointer.
    music_player_.stop();
    music_track_ = std::move(track);
    music_player_.start(*music_track_, music_bank_, kMusicChip, /*loop=*/true);
    music_song_ = command.song;
}

void TdeckAudioBackend::task_entry(void *self) { static_cast<TdeckAudioBackend *>(self)->run(); }

// The audio task. It blocks in exactly three places: the SFX queue for a
// bounded wait while both channels are silent (kIdleWaitTicks -- bounded,
// not portMAX_DELAY, because a start_music() while idle arrives on the
// SEPARATE music_queue_ and must not wait behind a SFX-only block), and
// i2s_channel_write while either channel sounds -- never a busy loop, so it
// cannot starve the idle task's watchdog on core 1.
void TdeckAudioBackend::run() {
    constexpr TickType_t kIdleWaitTicks = pdMS_TO_TICKS(20);
    int16_t sfx_chunk[kChunkFrames];
    int16_t music_chunk[kChunkFrames];
    int16_t mixed[kChunkFrames];
    size_t written = 0;
    bool enabled = false;
    for (;;) {
        Command command{};
        const bool both_idle = player_.idle() && !music_player_.active();
        bool got = xQueueReceive(queue_, &command, both_idle ? kIdleWaitTicks : 0) == pdTRUE;
        // The epoch first: a flush that happened while these commands were in
        // flight makes them stale.
        player_.sync_epoch(epoch_.load());
        while (got) {
            player_.submit(command.request, command.epoch);
            got = xQueueReceive(queue_, &command, 0) == pdTRUE;
        }
        MusicCommand music_command{};
        while (xQueueReceive(music_queue_, &music_command, 0) == pdTRUE) apply_music_command(music_command);

        if (player_.idle() && !music_player_.active()) {
            if (enabled) {
                // Two chunks of silence flush the DMA ring, then the clocks go
                // off so the amplifier idles between effects.
                for (auto &s : mixed) s = 0;
                for (int i = 0; i < 2; ++i) i2s_channel_write(tx_, mixed, sizeof(mixed), &written, 200);
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
                music_player_.stop();
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
            enabled = true;
        }
        // Each channel's own live gain, applied once inside its render(): a
        // volume change (0 included) is heard within one chunk. SFX and
        // music never duck one another (ALPHA3_AUDIO.md section 9) -- they
        // are summed and saturated, the same as two independent hardware
        // paths mixing in the air would be.
        player_.render(sfx_chunk, kChunkFrames, sfx_gain_.load());
        if (music_player_.active()) music_player_.render(music_chunk, kChunkFrames, kSampleRateHz, music_gain_.load());
        else for (auto &s : music_chunk) s = 0;
        for (uint32_t i = 0; i < kChunkFrames; ++i) {
            const int32_t sum = int32_t(sfx_chunk[i]) + int32_t(music_chunk[i]);
            mixed[i] = int16_t(sum > 32767 ? 32767 : sum < -32768 ? -32768 : sum);
        }
        i2s_channel_write(tx_, mixed, sizeof(mixed), &written, 200);
    }
}

} // namespace tdeck
