#pragma once

#include <atomic>
#include <cstdint>

#include <memory>

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/music_synth.h"
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
// timeout, bumps the flush epoch, stores a gain, or overwrites the length-1
// music command queue (xQueueOverwrite: "the latest wants song X/stop", not
// a backlog) -- it never waits. The AUDIO task (core 1, where no game task
// runs) owns the openu5::SfxPlayer AND the A3-04 openu5::MusicSongPlayer: it
// drains both queues, renders 16 ms chunks of each at their own live gain,
// mixes them (a saturating sum -- the patched original's MIDI card and PC
// speaker were separate hardware and both sounded at once, ALPHA3_AUDIO.md
// section 9), and blocks only in i2s_channel_write or a short bounded wait
// when both channels are silent (so a new music command is never more than
// that wait late). A flush (load, Return to Title) is an SFX epoch only:
// requests posted before it are dropped as stale; music is untouched (the
// same policy AudioService already documents for flush_for_load()).
//
// Nothing here can stall the game loop: the I2S channel, its DMA buffers and
// the task are created on the first accepted SFX or music request, and a
// bring-up failure leaves the game silent and playable.
class TdeckAudioBackend final : public openu5::AudioBackend {
  public:
    static constexpr uint32_t kSampleRateHz = openu5::kSfxOutputRateHz;
    static constexpr uint32_t kQueueDepth = 16;
    /** OPL2 (9 voices): a real period AdLib card's own polyphony, and the
     * device's CPU budget (ALPHA3_AUDIO.md section 17.4). The browser
     * reference defaults to OPL3 (18) only to avoid voice stealing that a
     * real 1988-2001 AdLib listener would also have heard. */
    static constexpr openu5::OplChipKind kMusicChip = openu5::OplChipKind::Opl2;

    bool play_sfx(const openu5::SfxRequest &) override;
    void stop_sfx() override;
    bool start_music(openu5::MusicSong, uint16_t gain_q15) override;
    void stop_music() override;
    void set_gain(openu5::AudioChannel, uint16_t gain_q15) override;
    /** Empty until a bring-up step fails; then the ESP-IDF error of that step. */
    const char *last_error() const { return error_; }

    /**
     * A3-04. The audio pack's song/bank bytes, kept resident by
     * alpha_audio.cpp's RetainedAudioPayload for as long as the process
     * runs. Called once at boot, from the single-threaded startup path,
     * strictly before any start_music() can reach the audio task -- so
     * parsing the bank here (not on the audio task) races with nothing.
     * `library` must outlive this backend (it does: main.cpp gives it a
     * `static` RetainedAudioPayload, same lifetime as the backend itself).
     * A null or unparsable library leaves the backend silent for music
     * (start_music always returns false), never a crash.
     */
    void set_music_library(const openu5::AudioPackPayload *library);

  private:
    struct Command {
        openu5::SfxRequest request{};
        uint32_t epoch = 0;
    };
    struct MusicCommand {
        openu5::MusicSong song = openu5::MusicSong::None;
        bool stop = false;
    };
    bool ensure_started();
    static void task_entry(void *);
    void run();
    /** Audio task only: services a dequeued MusicCommand (parses/starts/stops). */
    void apply_music_command(const MusicCommand &);

    QueueHandle_t queue_ = nullptr;
    QueueHandle_t music_queue_ = nullptr; // depth 1; xQueueOverwrite always keeps the latest
    TaskHandle_t task_ = nullptr;
    i2s_chan_handle_t tx_ = nullptr;
    std::atomic<uint16_t> sfx_gain_{0};
    std::atomic<uint16_t> music_gain_{0};
    std::atomic<uint32_t> epoch_{0};
    std::atomic<uint32_t> queue_full_{0};
    openu5::SfxPlayer player_{}; // audio task only
    const openu5::AudioPackPayload *music_library_ = nullptr; // set once at boot; read-only after
    openu5::MilesOplBank music_bank_{};                        // parsed once in set_music_library()
    bool music_bank_loaded_ = false;
    openu5::MusicSongPlayer music_player_{};             // audio task only
    std::unique_ptr<openu5::MusicTrack> music_track_;    // audio task only; music_player_ points into it
    openu5::MusicSong music_song_ = openu5::MusicSong::None; // audio task's idea of "currently loaded"
    bool failed_ = false;
    char error_[64]{};
};

} // namespace tdeck
