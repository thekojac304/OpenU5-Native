#pragma once

#include <atomic>
#include <cstdint>

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/audio_stream.h"

namespace tdeck {

// The T-Deck Plus speaker (A3-01) driving the PC-speaker synthesizer (A3-02)
// and the OPL2 music synth (A3-04), fed by the A3-04A block pump.
//
// Output path (LilyGO's board definition, examples/UnitTest/utilities.h and
// examples/SimpleTone/SimpleTone.ino): an I2S-input speaker amplifier on
// BCK GPIO 7, WS GPIO 5, DOUT GPIO 6, no MCLK, no control bus, powered by the
// board's peripheral enable (GPIO 10, already driven high by Board). One slot
// of 16-bit PCM; the amplifier has no volume register, so the gain is applied
// to the samples.
//
// Threads. The GAME thread (core 0) only ever: checks that the cue has an
// A3-02 program (openu5::sfx_supported), posts {request, epoch} with a ZERO
// timeout, bumps the flush epoch, stores a gain, overwrites the length-1
// music command queue (xQueueOverwrite: "the latest wants song X/stop", not
// a backlog), or copies the latest published performance window under a
// spinlock -- it never waits. The AUDIO task (core 1, where no game task
// runs) owns openu5::AudioRingPump (ALPHA3_AUDIO.md section 18): it drains
// both queues between blocks, and the pump renders one 8 ms block of music +
// SFX (each at its own live gain, summed and saturated -- section 9) and
// hands it to the DMA descriptor ring, which IS the render-ahead buffer:
// kAudioRingBlocks x kAudioBlockFrames = 64 ms ahead of the speaker. The task
// blocks only in i2s_channel_write (waiting for the DMA to free a descriptor
// -- that wait paces it) or, when silent with the channel off, in a bounded
// wait on the SFX queue. A flush (load, Return to Title) is an SFX epoch
// only; music is untouched (AudioService::flush_for_load()'s policy).
//
// Nothing here can stall the game loop: the I2S channel, its DMA buffers and
// the task are created on the first accepted SFX or music request, and a
// bring-up failure leaves the game silent and playable.
class TdeckAudioBackend final : public openu5::AudioBackend, public openu5::AudioPerfSource {
  public:
    static constexpr uint32_t kSampleRateHz = openu5::kSfxOutputRateHz;
    static constexpr uint32_t kQueueDepth = 16;
    /** A3-04 raised it 4096 -> 6144 B for the OPL synth; A3-04A moved the block buffers off it (section 18.9). */
    static constexpr uint32_t kTaskStackBytes = 6144;
    /** One DMA descriptor, mono 16-bit. */
    static constexpr size_t kBlockBytes = openu5::kAudioBlockFrames * sizeof(int16_t);

    bool play_sfx(const openu5::SfxRequest &) override;
    void stop_sfx() override;
    bool start_music(openu5::MusicSong, uint16_t gain_q15) override;
    void stop_music() override;
    void set_gain(openu5::AudioChannel, uint16_t gain_q15) override;
    /** Empty until a bring-up step fails; then the ESP-IDF error of that step. */
    const char *last_error() const { return error_; }

    /**
     * A3-04 / A3-04A. The audio pack's song/bank bytes, kept resident by
     * alpha_audio.cpp's RetainedAudioPayload for as long as the process
     * runs. Called once at boot, from the single-threaded startup path,
     * strictly before any start_music() can reach the audio task. A3-04A:
     * the bank AND all 16 songs are parsed here, once (MusicLibrary), so a
     * song switch on the audio task never parses or allocates. A null or
     * unparsable library leaves music silent, never a crash.
     */
    void set_music_library(const openu5::AudioPackPayload *library);
    const openu5::MusicLibrary &music_library() const { return library_; }

    // openu5::AudioPerfSource (Developer > Diagnostics > Audio performance).
    bool perf_snapshot(openu5::AudioPerfSnapshot &) const override;
    void perf_reset() override;

  private:
    struct Command {
        openu5::SfxRequest request{};
        uint32_t epoch = 0;
    };
    struct MusicCommand {
        openu5::MusicSong song = openu5::MusicSong::None;
        bool stop = false;
    };
    // The DMA descriptor ring, as the pump sees it (ESP-IDF i2s_std TX).
    class I2sRingSink final : public openu5::PcmRingSink {
      public:
        explicit I2sRingSink(TdeckAudioBackend &owner) : owner_(owner) {}
        bool preload(const int16_t *block) override;
        bool enable() override;
        void disable() override;
        bool write(const int16_t *block) override;
        uint32_t blocks_played() const override { return owner_.isr_played_; }
        uint32_t underrun_events() const override { return owner_.isr_overflows_; }

      private:
        TdeckAudioBackend &owner_;
    };

    bool ensure_started();
    static void task_entry(void *);
    void run();
    /** Audio task: copy the pump's window out for the game thread. */
    void publish_perf();
    static bool on_sent(i2s_chan_handle_t, i2s_event_data_t *, void *);
    static bool on_send_q_ovf(i2s_chan_handle_t, i2s_event_data_t *, void *);
    static uint64_t clock_us(void *);

    QueueHandle_t queue_ = nullptr;
    QueueHandle_t music_queue_ = nullptr; // depth 1; xQueueOverwrite always keeps the latest
    TaskHandle_t task_ = nullptr;
    i2s_chan_handle_t tx_ = nullptr;
    std::atomic<uint16_t> sfx_gain_{0};
    std::atomic<uint16_t> music_gain_{0};
    std::atomic<uint32_t> epoch_{0};
    std::atomic<uint32_t> queue_full_{0};
    std::atomic<bool> perf_reset_requested_{false};
    // Written only by the I2S DMA ISR (on_sent / on_send_q_ovf), read by the
    // audio task: single writer, aligned 32-bit, so a plain volatile is enough.
    volatile uint32_t isr_played_ = 0;
    volatile uint32_t isr_overflows_ = 0;
    openu5::MusicLibrary library_{};  // parsed once in set_music_library(); read-only after
    openu5::AudioRingPump pump_{};    // audio task only
    I2sRingSink sink_{*this};         // audio task only
    uint32_t blocks_since_publish_ = 0, stack_free_min_ = UINT32_MAX; // audio task only
    mutable portMUX_TYPE perf_lock_ = portMUX_INITIALIZER_UNLOCKED;
    openu5::AudioPerfSnapshot published_{}; // under perf_lock_
    bool published_valid_ = false;          // under perf_lock_
    bool failed_ = false;
    char error_[64]{};
};

} // namespace tdeck
