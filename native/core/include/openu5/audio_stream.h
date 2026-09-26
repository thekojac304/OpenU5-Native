#pragma once

#include <cstddef>
#include <cstdint>

#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/music_synth.h"
#include "openu5/sfx_synth.h"

// Alpha 3 A3-04A -- the audio task's portable half (ALPHA3_AUDIO.md section
// 18): the block pump that keeps the device's DMA ring full, the music
// library parsed once at boot, and the performance counters the Developer
// "Audio performance" diagnostic reads.
//
// Why it lives here and not in tdeck_audio.cpp: A3-04's device loop was
// "device-only; not host-tested" (section 17.9), and that is where its
// real-time behaviour was decided. The loop's policy -- prime the ring, keep
// it full, drain it before the clocks stop, count what went wrong -- is now
// production code the host drives against a model of the ESP-IDF driver's
// descriptor ring (a3_04a_audio_stream), and the device keeps only the I2S
// calls and the FreeRTOS queues.
//
// Everything here is audio-task-side: no locks, no clock of its own (the
// caller injects one), no allocation once the library is loaded.
namespace openu5 {

// ---------------------------------------------------------------------------
// Stream geometry (section 18.7).
//
// The DMA descriptor ring IS the render-ahead PCM ring: kAudioRingBlocks
// descriptors of kAudioBlockFrames each, filled by the audio task and drained
// by the I2S DMA. 64 ms ahead of the speaker, the same total A3-02..A3-04
// shipped (4 x 256 frames), cut into 8 ms blocks instead of 16 ms ones: at
// the same latency and the same 2 KiB of DMA memory the ring now rides out a
// stall of (kAudioRingBlocks - 1) blocks minus one block's render time
// (~52 ms at the measured synth cost) instead of ~40 ms.
// ---------------------------------------------------------------------------
constexpr uint32_t kAudioBlockFrames = 128;
constexpr uint32_t kAudioRingBlocks = 8;
constexpr uint32_t kAudioBlockUs = kAudioBlockFrames * 1000000u / kSfxOutputRateHz;
static_assert(kAudioBlockFrames * 1000000u % kSfxOutputRateHz == 0, "a block must be a whole number of microseconds");
/** The device's music chip: OPL2, 9 voices (ALPHA3_AUDIO.md section 17.3). */
constexpr OplChipKind kDeviceMusicChip = OplChipKind::Opl2;

// ---------------------------------------------------------------------------
// The music library: the bank and all 16 songs, parsed ONCE at boot from the
// retained audio-pack payload (section 18.9). A3-04 parsed a song's XMI --
// an allocation, a stable sort and a copy -- on the audio task at every song
// switch; now a switch only points the player at a track that already exists.
// ---------------------------------------------------------------------------
class MusicLibrary {
  public:
    /**
     * Parses the bank and every song the payload carries. Call once, from a
     * single thread, before anything plays. A song that fails to parse stays
     * silent (track() returns nullptr); an unparsable bank silences them all.
     * Returns the number of playable songs.
     */
    size_t load(const AudioPackPayload *payload);
    bool loaded() const { return bank_ok_; }
    const MilesOplBank &bank() const { return bank_; }
    /** nullptr: no library, no such song, or it failed to parse. */
    const MusicTrack *track(MusicSong) const;
    size_t playable_songs() const;
    /** Events across every parsed song (the resident cost, 8 bytes each). */
    size_t event_count() const;

  private:
    MilesOplBank bank_{};
    bool bank_ok_ = false;
    MusicTrack tracks_[kMusicSongCount]{};
    bool parsed_[kMusicSongCount]{};
};

// ---------------------------------------------------------------------------
// The sink: the device's DMA descriptor ring (ESP-IDF i2s_std, TX), or the
// host test's model of it. Semantics are the driver's, not an idealization:
//  - preload() only while the channel is off; it fills descriptors in order
//    from the first one and says false once every descriptor holds a block;
//  - enable() starts the DMA at the FIRST descriptor, playing whatever each
//    descriptor holds (disable() does not clear them);
//  - write() waits for a descriptor the DMA has finished, then copies;
//  - the DMA never waits: a descriptor nobody refilled plays again as the
//    silence the driver's auto-clear left in it, and the driver reports it.
// ---------------------------------------------------------------------------
class PcmRingSink {
  public:
    virtual ~PcmRingSink() = default;
    virtual bool preload(const int16_t *block) = 0;
    virtual bool enable() = 0;
    virtual void disable() = 0;
    /** false = timed out or failed; the block was not queued. */
    virtual bool write(const int16_t *block) = 0;
    /** Monotonic count of descriptors the DMA finished (the driver's on_sent). */
    virtual uint32_t blocks_played() const = 0;
    /** Monotonic count of the driver's send-queue overflows: a descriptor played with nothing new in it. */
    virtual uint32_t underrun_events() const = 0;
};

/** The caller's clock; now_us == nullptr disables every timing (counts still work). */
struct AudioClock {
    void *context = nullptr;
    uint64_t (*now_us)(void *) = nullptr;
};

// ---------------------------------------------------------------------------
// Performance counters (section 18.5). One window, from the last reset.
// Every duration is microseconds; "_x100" fields are averages times 100.
// ---------------------------------------------------------------------------
struct AudioPerfSnapshot {
    uint32_t seq = 0;         // bumped by every publish (device) -- lets a reader wait for a fresh one
    uint32_t window_us = 0;   // wall time since the last reset
    uint32_t block_us = kAudioBlockUs;
    uint32_t block_frames = kAudioBlockFrames;
    uint32_t ring_blocks = kAudioRingBlocks;
    uint32_t blocks = 0;      // blocks rendered (primed + written)
    uint32_t written = 0;     // blocks written while the channel ran
    // One block's render: music synth + SFX synth + mix.
    uint32_t render_min_us = 0, render_avg_us = 0, render_max_us = 0, render_p95_us = 0, render_p99_us = 0;
    uint32_t music_avg_us = 0, music_max_us = 0;
    // i2s write: the copy plus the wait for a free descriptor (the wait is the healthy case).
    uint32_t write_avg_us = 0, write_max_us = 0;
    // Interval between two consecutive deliveries (the audio task's own schedule).
    uint32_t period_avg_us = 0, period_max_us = 0;
    // Blocks not yet finished playing (the one playing included), sampled
    // just before each write. ring_blocks = full (the write will wait for
    // the DMA); 0 = the speaker had already run dry.
    uint32_t fill_min = 0, fill_max = 0, fill_avg_x100 = 0;
    uint32_t underruns = 0;          // writes that found the ring already empty (software view)
    uint32_t hw_underruns = 0;       // descriptors the DMA replayed empty (driver view)
    uint32_t missed_deadlines = 0;   // blocks that took longer to render than they last
    uint32_t write_failures = 0, enable_failures = 0;
    uint32_t runaway_yields = 0;     // times the task yielded because writes stopped blocking
    uint32_t voices_max = 0, voices_avg_x100 = 0;      // MIDI voices keyed/held
    uint32_t channels_max = 0, channels_avg_x100 = 0;  // OPL channels sounding -- the synth's cost driver
    uint32_t cpu_permille = 0;       // render time / window
    uint32_t sfx_submitted = 0, sfx_during_music = 0, sfx_pending_max = 0, sfx_queue_max = 0;
    uint32_t music_switches = 0;
    uint32_t stack_free_min = 0;     // bytes; the device fills it (uxTaskGetStackHighWaterMark)
    bool music_active = false;
    MusicSong song = MusicSong::None;
};

class AudioPerfCounters {
  public:
    // Render-time histogram: 1 % of a block per bucket, up to two blocks, plus overflow.
    static constexpr size_t kBucketsPerBlock = 100;
    static constexpr size_t kBuckets = 2 * kBucketsPerBlock;

    void reset(uint64_t now_us);
    void on_render(uint32_t render_us, uint32_t music_us, uint32_t voices, uint32_t channels, uint32_t sfx_pending);
    void on_write(uint32_t fill_before, uint32_t write_us, uint32_t period_us, bool ok, bool had_period);
    void on_underrun() { ++underruns_; }
    void on_write_failure() { ++write_failures_; }
    void on_hw_underruns(uint32_t n) { hw_underruns_ += n; }
    void on_enable_failure() { ++enable_failures_; }
    void on_runaway_yield() { ++runaway_yields_; }
    void on_sfx(bool music_active) {
        ++sfx_submitted_;
        if (music_active) ++sfx_during_music_;
    }
    void on_sfx_queue_depth(uint32_t depth) {
        if (depth > sfx_queue_max_) sfx_queue_max_ = depth;
    }
    void on_music_switch() { ++music_switches_; }
    void snapshot(uint64_t now_us, AudioPerfSnapshot &out) const;

  private:
    uint64_t start_us_ = 0;
    uint32_t blocks_ = 0, written_ = 0, fills_ = 0, periods_ = 0;
    uint64_t render_sum_ = 0, music_sum_ = 0, write_sum_ = 0, period_sum_ = 0, fill_sum_ = 0;
    uint64_t voices_sum_ = 0, channels_sum_ = 0;
    uint32_t render_min_ = UINT32_MAX, render_max_ = 0, music_max_ = 0, write_max_ = 0, period_max_ = 0;
    uint32_t fill_min_ = UINT32_MAX, fill_max_ = 0;
    uint32_t underruns_ = 0, hw_underruns_ = 0, missed_ = 0, write_failures_ = 0, enable_failures_ = 0;
    uint32_t runaway_yields_ = 0, voices_max_ = 0, channels_max_ = 0;
    uint32_t sfx_submitted_ = 0, sfx_during_music_ = 0, sfx_pending_max_ = 0, sfx_queue_max_ = 0;
    uint32_t music_switches_ = 0;
    uint32_t histogram_[kBuckets + 1]{};
};

// ---------------------------------------------------------------------------
// The pump: one per audio task. It owns both players (the A3-02 SfxPlayer
// and the A3-04 MusicSongPlayer), renders one block at a time -- music and
// SFX, each at its own live gain, summed and saturated (section 17.9, no
// ducking) -- and drives the ring:
//
//   Off      nothing to play, channel off; the caller blocks on its queue.
//   Priming  channel off; render ring_blocks blocks back to back and preload
//            every descriptor, THEN enable -- so playback starts with a full
//            ring of fresh audio (A3-04 enabled first and let the DMA replay
//            whatever the descriptors still held from the previous sound).
//   Running  per step: render one block, write it (the write waits for the
//            DMA to free a descriptor -- that wait paces the task). Once
//            ring_blocks blocks of pure silence were written in a row, every
//            real block has played: turn the channel off (A3-04 wrote two,
//            which cut the last ~32 ms of a sound when the ring was four deep).
// ---------------------------------------------------------------------------
class AudioRingPump {
  public:
    enum class State : uint8_t { Off, Priming, Running };
    /**
     * Runaway guard (section 18.12): a write issued while the ring still has
     * a free descriptor cannot wait for the DMA. A healthy stream catches up
     * within one ring of such writes after a stall; this many in a row means
     * the producer is slower than real time and the task never sleeps.
     */
    static constexpr uint32_t kRunawayWrites = 4 * kAudioRingBlocks;

    explicit AudioRingPump(size_t ring_blocks = kAudioRingBlocks) : ring_blocks_(ring_blocks) {}

    void set_clock(AudioClock clock) { clock_ = clock; }
    /** Boot, before the task starts. nullptr = no music (every play_music is silence). */
    void set_music_library(const MusicLibrary *library) { library_ = library; }

    // ---- commands (audio task, between blocks) ----
    void sync_epoch(uint32_t epoch) { sfx_.sync_epoch(epoch); }
    SfxAdmit submit_sfx(const SfxRequest &, uint32_t epoch);
    /** None, a song the library lacks, or a song that failed to parse: silence, never a guess. */
    void play_music(MusicSong);
    void stop_music();
    /** A bring-up failure: drop everything and stay silent. */
    void fail_silent();

    // ---- the loop ----
    /** Something to render: a cue sounding or pending, or a song playing. */
    bool has_work() const { return !sfx_.idle() || music_.active(); }
    /** Channel off and nothing to play: the caller may block on its command queue. */
    bool sleeping() const { return state_ == State::Off && !has_work(); }
    /**
     * One pass. Renders and primes/writes one block, or turns the channel
     * off. True = the caller should yield the CPU briefly (the enable failed,
     * or writes stopped waiting for the DMA for kRunawayWrites blocks).
     */
    bool step(PcmRingSink &sink, uint16_t sfx_gain_q15, uint16_t music_gain_q15);

    State state() const { return state_; }
    MusicSong song() const { return song_; }
    const SfxPlayer &sfx() const { return sfx_; }
    const MusicSongPlayer &music() const { return music_; }
    size_t ring_blocks() const { return ring_blocks_; }
    /** The block most recently rendered (tests: what was just handed to the sink). */
    const int16_t *last_block() const { return block_; }

    // ---- diagnostics ----
    void reset_perf() { perf_.reset(now()); }
    void note_sfx_queue_depth(uint32_t depth) { perf_.on_sfx_queue_depth(depth); }
    void perf(AudioPerfSnapshot &out) const;

  private:
    uint64_t now() const { return clock_.now_us ? clock_.now_us(clock_.context) : 0; }
    void render_block(uint16_t sfx_gain_q15, uint16_t music_gain_q15);

    size_t ring_blocks_;
    AudioClock clock_{};
    const MusicLibrary *library_ = nullptr;
    SfxPlayer sfx_{};
    MusicSongPlayer music_{};
    MusicSong song_ = MusicSong::None;
    State state_ = State::Off;
    uint32_t primed_ = 0;       // blocks preloaded in this Priming phase
    uint32_t silent_run_ = 0;   // blocks in a row rendered with nothing to play
    uint32_t written_ = 0;      // blocks handed to the ring since the channel was enabled
    uint32_t played_base_ = 0, underrun_base_ = 0;
    uint32_t nonblocking_run_ = 0;
    uint64_t last_delivery_us_ = 0;
    bool has_delivery_ = false;
    AudioPerfCounters perf_{};
    int16_t block_[kAudioBlockFrames]{};
    int16_t sfx_block_[kAudioBlockFrames]{};
    int16_t music_block_[kAudioBlockFrames]{};
};

// ---------------------------------------------------------------------------
// Diagnostics seam: what the Developer "Audio performance" rows read. The
// device backend implements it; a null source means "no audio hardware".
// Both calls are game-thread-safe and never wait.
// ---------------------------------------------------------------------------
class AudioPerfSource {
  public:
    virtual ~AudioPerfSource() = default;
    /** The newest published window; false if the audio task never ran. */
    virtual bool perf_snapshot(AudioPerfSnapshot &) const = 0;
    /** Ask the audio task to start a new window (applied before its next block). */
    virtual void perf_reset() = 0;
};

/**
 * The compact result the diagnostic prints (section 18.14): up to
 * `max_lines` lines of at most 63 characters. Returns the lines written.
 */
size_t format_audio_perf(const AudioPerfSnapshot &, char (*lines)[64], size_t max_lines);

// ---------------------------------------------------------------------------
// Developer > Diagnostics > "Audio performance" (section 18.14): a pure
// timeline the runtime drives from its frame loop, so the game thread never
// waits. Settle (the song starts and the ring fills), then MusicOnly -- the
// idle test: one known track, no SFX, no movement -- then MusicSfx, the same
// track with a cue every kSfxEveryMs. Each measured phase is its own window.
// ---------------------------------------------------------------------------
class AudioBenchmark {
  public:
    static constexpr uint32_t kSettleMs = 2000;
    static constexpr uint32_t kMusicOnlyMs = 30000;
    static constexpr uint32_t kMusicSfxMs = 15000;
    static constexpr uint32_t kSfxEveryMs = 100;
    /** The worst case measured on the real corpus (section 18.4): the most sounding channels and the densest events. */
    static constexpr MusicSong kSong = MusicSong::Theme;

    enum class Phase : uint8_t { Idle, Settle, MusicOnly, MusicSfx, Done };
    struct Actions {
        bool reset_perf = false;     // start a new measurement window
        bool capture_idle = false;   // the MusicOnly window just ended: read it
        bool capture_stress = false; // the MusicSfx window just ended: read it
        bool finished = false;       // restore the game's own music
        SfxId sfx = SfxId::None;     // play this cue now
    };

    void start(uint32_t now_ms);
    /** Advance to `now_ms`; at most one phase edge and one cue per call. */
    Actions tick(uint32_t now_ms);
    bool running() const { return phase_ != Phase::Idle && phase_ != Phase::Done; }
    Phase phase() const { return phase_; }
    uint32_t sfx_played() const { return sfx_played_; }

  private:
    Phase phase_ = Phase::Idle;
    uint32_t phase_start_ms_ = 0, next_sfx_ms_ = 0, sfx_played_ = 0;
};

/**
 * What A3-04's synth would have spent per block on the function-local-static
 * guard alone, from the guard's measured cost on this device (section 18.3):
 * four guarded table reads per sounding channel per chip sample, at the
 * window's average sounding channels. Microseconds per block.
 */
uint32_t legacy_guard_us_per_block(uint32_t guard_ns, uint32_t channels_avg_x100);

} // namespace openu5
