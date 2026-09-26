#pragma once

#include <atomic>
#include <cstdint>

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "openu5/audio.h"
#include "openu5/sfx_synth.h"

namespace tdeck {

// The T-Deck Plus speaker (A3-01) driving the PC-speaker synthesizer (A3-02).
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
// timeout, bumps the flush epoch, or stores a gain -- it never waits. The
// AUDIO task (core 1, where no game task runs) owns the openu5::SfxPlayer:
// it drains the queue into the player's policy, renders 16 ms chunks at the
// live SFX gain and blocks only in i2s_channel_write. A flush (load, Return
// to Title) is an epoch: requests posted before it are dropped as stale, so
// no effect of the old world can sound in the new one.
//
// Nothing here can stall the game loop: the I2S channel, its DMA buffers and
// the task are created on the first accepted request, and a bring-up failure
// leaves the game silent and playable.
class TdeckAudioBackend final : public openu5::AudioBackend {
  public:
    static constexpr uint32_t kSampleRateHz = openu5::kSfxOutputRateHz;
    static constexpr uint32_t kQueueDepth = 16;

    bool play_sfx(const openu5::SfxRequest &) override;
    void stop_sfx() override;
    bool start_music(openu5::MusicSong, uint16_t) override { return false; } // A3-04
    void stop_music() override {}
    void set_gain(openu5::AudioChannel, uint16_t gain_q15) override;
    /** Empty until a bring-up step fails; then the ESP-IDF error of that step. */
    const char *last_error() const { return error_; }

  private:
    struct Command {
        openu5::SfxRequest request{};
        uint32_t epoch = 0;
    };
    bool ensure_started();
    static void task_entry(void *);
    void run();

    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_ = nullptr;
    i2s_chan_handle_t tx_ = nullptr;
    std::atomic<uint16_t> sfx_gain_{0};
    std::atomic<uint32_t> epoch_{0};
    std::atomic<uint32_t> queue_full_{0};
    openu5::SfxPlayer player_{}; // audio task only
    bool failed_ = false;
    char error_[64]{};
};

} // namespace tdeck
