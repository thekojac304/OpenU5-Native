#include "openu5/audio_stream.h"

#include <cstdio>
#include <cstring>

// Alpha 3 A3-04A -- see audio_stream.h and ALPHA3_AUDIO.md section 18.
namespace openu5 {

// ===========================================================================
// MusicLibrary
// ===========================================================================
size_t MusicLibrary::load(const AudioPackPayload *payload) {
    bank_ok_ = false;
    for (size_t i = 0; i < kMusicSongCount; ++i) {
        parsed_[i] = false;
        tracks_[i] = MusicTrack{};
    }
    if (!payload || !payload->bank) return 0;
    bank_ok_ = bank_.load(payload->bank, payload->bank_length);
    if (!bank_ok_) return 0;
    for (size_t i = 0; i < kMusicSongCount; ++i)
        parsed_[i] = payload->song[i] && parse_xmi_events(payload->song[i], payload->song_length[i], tracks_[i]);
    return playable_songs();
}

const MusicTrack *MusicLibrary::track(MusicSong song) const {
    const size_t id = size_t(song);
    if (!bank_ok_ || id >= kMusicSongCount || !parsed_[id]) return nullptr;
    return &tracks_[id];
}

size_t MusicLibrary::playable_songs() const {
    size_t n = 0;
    for (size_t i = 0; i < kMusicSongCount; ++i)
        if (bank_ok_ && parsed_[i]) ++n;
    return n;
}

size_t MusicLibrary::event_count() const {
    size_t n = 0;
    for (size_t i = 0; i < kMusicSongCount; ++i)
        if (parsed_[i]) n += tracks_[i].events.size();
    return n;
}

// ===========================================================================
// AudioPerfCounters
// ===========================================================================
void AudioPerfCounters::reset(uint64_t now_us) {
    *this = AudioPerfCounters{};
    start_us_ = now_us;
}

void AudioPerfCounters::on_render(uint32_t render_us, uint32_t music_us, uint32_t voices, uint32_t channels,
                                  uint32_t sfx_pending, uint32_t clipped) {
    ++blocks_;
    if (clipped) {
        mix_clipped_ += clipped;
        ++mix_clipped_blocks_;
    }
    render_sum_ += render_us;
    music_sum_ += music_us;
    if (render_us < render_min_) render_min_ = render_us;
    if (render_us > render_max_) render_max_ = render_us;
    if (music_us > music_max_) music_max_ = music_us;
    // A block that took longer to render than it lasts: the producer cannot
    // keep up. A3-04's synth did this on every block (section 18.3).
    if (render_us > kAudioBlockUs) ++missed_;
    const uint32_t bucket = uint32_t(uint64_t(render_us) * kBucketsPerBlock / kAudioBlockUs);
    ++histogram_[bucket < kBuckets ? bucket : kBuckets];
    voices_sum_ += voices;
    channels_sum_ += channels;
    if (voices > voices_max_) voices_max_ = voices;
    if (channels > channels_max_) channels_max_ = channels;
    if (sfx_pending > sfx_pending_max_) sfx_pending_max_ = sfx_pending;
}

void AudioPerfCounters::on_write(uint32_t fill_before, uint32_t write_us, uint32_t period_us, bool ok,
                                 bool had_period) {
    if (ok) ++written_;
    else ++write_failures_;
    ++fills_;
    fill_sum_ += fill_before;
    if (fill_before < fill_min_) fill_min_ = fill_before;
    if (fill_before > fill_max_) fill_max_ = fill_before;
    write_sum_ += write_us;
    if (write_us > write_max_) write_max_ = write_us;
    if (had_period) {
        ++periods_;
        period_sum_ += period_us;
        if (period_us > period_max_) period_max_ = period_us;
    }
}

void AudioPerfCounters::snapshot(uint64_t now_us, AudioPerfSnapshot &out) const {
    const uint32_t seq = out.seq;
    out = AudioPerfSnapshot{};
    out.seq = seq;
    out.window_us = now_us > start_us_ ? uint32_t(now_us - start_us_) : 0;
    out.blocks = blocks_;
    out.written = written_;
    if (blocks_) {
        out.render_min_us = render_min_;
        out.render_avg_us = uint32_t(render_sum_ / blocks_);
        out.render_max_us = render_max_;
        out.music_avg_us = uint32_t(music_sum_ / blocks_);
        out.music_max_us = music_max_;
        out.voices_avg_x100 = uint32_t(voices_sum_ * 100 / blocks_);
        out.channels_avg_x100 = uint32_t(channels_sum_ * 100 / blocks_);
        // Percentiles from the histogram: the upper edge of the bucket the
        // rank falls in (resolution 1 % of a block, 80 us at 8 ms blocks),
        // never above the true maximum; past two blocks, the maximum.
        const uint64_t rank95 = (uint64_t(blocks_) * 95 + 99) / 100, rank99 = (uint64_t(blocks_) * 99 + 99) / 100;
        uint64_t seen = 0;
        for (size_t b = 0; b <= kBuckets; ++b) {
            const uint64_t before = seen;
            seen += histogram_[b];
            const uint32_t edge = b < kBuckets ? uint32_t((b + 1) * kAudioBlockUs / kBucketsPerBlock) : render_max_;
            const uint32_t clamped = edge < render_max_ ? edge : render_max_;
            if (before < rank95 && seen >= rank95) out.render_p95_us = clamped;
            if (before < rank99 && seen >= rank99) out.render_p99_us = clamped;
        }
        if (out.window_us) out.cpu_permille = uint32_t(render_sum_ * 1000 / out.window_us);
    }
    if (fills_) {
        out.fill_min = fill_min_;
        out.fill_max = fill_max_;
        out.fill_avg_x100 = uint32_t(fill_sum_ * 100 / fills_);
        out.write_avg_us = uint32_t(write_sum_ / fills_);
        out.write_max_us = write_max_;
    }
    if (periods_) {
        out.period_avg_us = uint32_t(period_sum_ / periods_);
        out.period_max_us = period_max_;
    }
    out.underruns = underruns_;
    out.hw_underruns = hw_underruns_;
    out.missed_deadlines = missed_;
    out.write_failures = write_failures_;
    out.enable_failures = enable_failures_;
    out.runaway_yields = runaway_yields_;
    out.voices_max = voices_max_;
    out.channels_max = channels_max_;
    out.sfx_submitted = sfx_submitted_;
    out.sfx_during_music = sfx_during_music_;
    out.sfx_pending_max = sfx_pending_max_;
    out.sfx_queue_max = sfx_queue_max_;
    out.music_switches = music_switches_;
    out.mix_clipped = mix_clipped_;
    out.mix_clipped_blocks = mix_clipped_blocks_;
}

// ===========================================================================
// AudioRingPump
// ===========================================================================
SfxAdmit AudioRingPump::submit_sfx(const SfxRequest &request, uint32_t epoch) {
    const SfxAdmit admit = sfx_.submit(request, epoch);
    if (admit != SfxAdmit::Stale && admit != SfxAdmit::Unsupported) perf_.on_sfx(music_.active());
    return admit;
}

void AudioRingPump::play_music(MusicSong song) {
    if (song == song_ && music_.active()) return; // AudioService already de-dupes; be sure anyway
    const MusicTrack *track = library_ ? library_->track(song) : nullptr;
    if (!track) {
        stop_music(); // None, unknown, or unparsable: silence, never a guess
        return;
    }
    // Tracks live in the library for the life of the process, so there is
    // no dangling-pointer ordering to respect (A3-04 had to stop() before
    // freeing the track it had just parsed); start() rebuilds the chip and
    // the voice allocator in place and clears the resampler's buffer.
    music_.start(*track, library_->bank(), kDeviceMusicChip, /*loop=*/true);
    song_ = song;
    perf_.on_music_switch();
}

void AudioRingPump::stop_music() {
    music_.stop();
    song_ = MusicSong::None;
}

void AudioRingPump::fail_silent() {
    sfx_.flush();
    stop_music();
    state_ = State::Off;
    primed_ = 0;
    silent_run_ = 0;
}

void AudioRingPump::render_block(uint16_t sfx_gain_q15, uint16_t music_gain_q15) {
    const uint64_t t0 = now();
    // Each channel at its own live gain, applied once inside its own render()
    // (section 17.9): a volume change reaches the very next block, and never
    // touches a block already handed to the ring.
    if (music_.active()) music_.render(music_block_, kAudioBlockFrames, kSfxOutputRateHz, music_gain_q15);
    else std::memset(music_block_, 0, sizeof(music_block_));
    const uint64_t t1 = now();
    sfx_.render(sfx_block_, kAudioBlockFrames, sfx_gain_q15);
    // Summed and saturated: the MIDI card and the PC speaker were separate
    // hardware mixing in the air -- no ducking, neither replaces the other.
    // A3-04B: every saturated sample is counted (section 19.10) -- a harsh
    // "static" on the device is either this or a dry ring, and the report
    // says which.
    uint32_t clipped = 0;
    for (uint32_t i = 0; i < kAudioBlockFrames; ++i) {
        const int32_t sum = int32_t(sfx_block_[i]) + int32_t(music_block_[i]);
        const bool over = sum > 32767 || sum < -32768;
        clipped += over ? 1u : 0u;
        block_[i] = int16_t(sum > 32767 ? 32767 : sum < -32768 ? -32768 : sum);
    }
    const uint64_t t2 = now();
    perf_.on_render(uint32_t(t2 - t0), uint32_t(t1 - t0), uint32_t(music_.active_voices()),
                    uint32_t(music_.sounding_channels()), uint32_t(sfx_.pending()), clipped);
}

bool AudioRingPump::step(PcmRingSink &sink, uint16_t sfx_gain_q15, uint16_t music_gain_q15) {
    switch (state_) {
    case State::Off:
        if (!has_work()) return false;
        state_ = State::Priming;
        primed_ = 0;
        silent_run_ = 0;
        [[fallthrough]];
    case State::Priming: {
        const bool work = has_work();
        render_block(sfx_gain_q15, music_gain_q15);
        silent_run_ = work ? 0 : silent_run_ + 1;
        const bool loaded = sink.preload(block_);
        if (loaded) ++primed_;
        else perf_.on_write_failure(); // cannot happen with the driver: counted, never hidden
        if (loaded && primed_ < ring_blocks_) return false;
        // Every descriptor holds fresh audio: now start the clocks.
        if (!sink.enable()) {
            perf_.on_enable_failure();
            fail_silent();
            return true;
        }
        state_ = State::Running;
        written_ = primed_;
        played_base_ = sink.blocks_played();
        underrun_base_ = sink.underrun_events();
        nonblocking_run_ = 0;
        has_delivery_ = false;
        return false;
    }
    case State::Running:
        break;
    }

    const uint32_t overflows = sink.underrun_events() - underrun_base_;
    if (overflows) {
        perf_.on_hw_underruns(overflows);
        underrun_base_ += overflows;
    }
    const bool work = has_work();
    // When the write of block j returns, block j - ring_blocks has finished
    // playing (the write waited for exactly that descriptor). So once
    // ring_blocks silent blocks follow the last real one, it has played --
    // and the clocks can stop without cutting it.
    if (!work && silent_run_ >= ring_blocks_) {
        sink.disable();
        state_ = State::Off;
        return false;
    }
    render_block(sfx_gain_q15, music_gain_q15);
    silent_run_ = work ? 0 : silent_run_ + 1;
    // Blocks not yet finished playing (the one playing included), taken just
    // before the write -- after the render, so a descriptor the DMA finished
    // meanwhile counts. The DMA keeps finishing descriptors while the task is
    // late, so `played` can overtake `written`: the ring ran dry. Re-base so
    // the count means "queued" again from here on.
    uint32_t played = sink.blocks_played() - played_base_;
    if (played > written_) {
        played_base_ += played - written_;
        played = written_;
    }
    const uint32_t fill = written_ - played;
    if (fill == 0) perf_.on_underrun();
    const uint64_t w0 = now();
    const bool ok = sink.write(block_);
    const uint64_t w1 = now();
    if (ok) ++written_;
    const uint32_t write_us = uint32_t(w1 - w0);
    perf_.on_write(fill, write_us, has_delivery_ ? uint32_t(w1 - last_delivery_us_) : 0, ok, has_delivery_);
    last_delivery_us_ = w1;
    has_delivery_ = true;
    // Runaway guard (section 18.12): with a free descriptor already waiting
    // (fill below a full ring) this write could not wait for the DMA. If that
    // held for kRunawayWrites blocks, the producer is slower than real time
    // and the task never sleeps -- yield, so the core's idle task (and its
    // watchdog) still run. A3-04's synth sat in exactly this state.
    if (fill < ring_blocks_) {
        if (++nonblocking_run_ >= kRunawayWrites) {
            nonblocking_run_ = 0;
            perf_.on_runaway_yield();
            return true;
        }
    } else {
        nonblocking_run_ = 0;
    }
    return false;
}

void AudioRingPump::perf(AudioPerfSnapshot &out) const {
    perf_.snapshot(now(), out);
    out.music_active = music_.active();
    out.song = song_;
    out.ring_blocks = uint32_t(ring_blocks_);
}

// ===========================================================================
// format_audio_perf -- the compact result lines (section 18.14)
// ===========================================================================
namespace {
void ms(char *out, size_t size, uint32_t us) {
    std::snprintf(out, size, "%lu.%02lu", (unsigned long)(us / 1000), (unsigned long)((us % 1000) / 10));
}
} // namespace

size_t format_audio_perf(const AudioPerfSnapshot &s, char (*lines)[64], size_t max_lines) {
    // Literal formats only (so -Wformat checks every one); a line past
    // max_lines goes to a scratch buffer and is not counted.
    char scratch[64];
    size_t n = 0;
    auto next = [&]() -> char * { return n < max_lines ? lines[n++] : scratch; };
    // ms() writes at most 10 characters ("4294967.29"); sized to that so
    // every line provably fits its 64 bytes (-Wformat-truncation).
    char a[11], b[11], c[11];
    const char *title = s.music_active ? music_song_title(s.song) : "no music";
    std::snprintf(next(), 64, "%.26s %lu.%lus", title, (unsigned long)(s.window_us / 1000000),
                  (unsigned long)((s.window_us / 100000) % 10));
    std::snprintf(next(), 64, "missed %lu  underrun %lu  hw %lu", (unsigned long)s.missed_deadlines,
                  (unsigned long)s.underruns, (unsigned long)s.hw_underruns);
    ms(a, sizeof a, s.render_avg_us);
    ms(b, sizeof b, s.render_p99_us);
    ms(c, sizeof c, s.render_max_us);
    std::snprintf(next(), 64, "render avg %s p99 %s max %s", a, b, c);
    ms(a, sizeof a, s.music_max_us);
    std::snprintf(next(), 64, "block %lu.%lums  music max %s  CPU %lu%%", (unsigned long)(s.block_us / 1000),
                  (unsigned long)((s.block_us / 100) % 10), a, (unsigned long)((s.cpu_permille + 5) / 10));
    ms(a, sizeof a, s.period_max_us);
    std::snprintf(next(), 64, "sched max %s  min buffered %lu ms", a,
                  (unsigned long)(s.fill_min * s.block_us / 1000));
    std::snprintf(next(), 64, "voices max %lu  OPL ch avg %lu.%lu max %lu", (unsigned long)s.voices_max,
                  (unsigned long)(s.channels_avg_x100 / 100), (unsigned long)((s.channels_avg_x100 / 10) % 10),
                  (unsigned long)s.channels_max);
    std::snprintf(next(), 64, "SFX %lu (%lu w/music) q%lu p%lu", (unsigned long)s.sfx_submitted,
                  (unsigned long)s.sfx_during_music, (unsigned long)s.sfx_queue_max, (unsigned long)s.sfx_pending_max);
    std::snprintf(next(), 64, "stack min %lu B  runaway %lu  fail %lu", (unsigned long)s.stack_free_min,
                  (unsigned long)s.runaway_yields, (unsigned long)(s.write_failures + s.enable_failures));
    return n;
}

// ===========================================================================
// AudioBenchmark
// ===========================================================================
const char *AudioBenchmark::phase_name(Phase p) {
    switch (p) {
    case Phase::Idle: return "idle";
    case Phase::Settle: return "settling";
    case Phase::MusicOnly: return "music alone";
    case Phase::MusicSfx: return "music + SFX";
    case Phase::Done: return "done";
    }
    return "";
}

void AudioBenchmark::start(uint32_t now_ms) {
    phase_ = Phase::Settle;
    started_ms_ = now_ms;
    phase_start_ms_ = now_ms;
    next_sfx_ms_ = 0;
    sfx_played_ = 0;
}

AudioBenchmark::Actions AudioBenchmark::tick(uint32_t now_ms) {
    Actions a{};
    const uint32_t elapsed = now_ms - phase_start_ms_;
    switch (phase_) {
    case Phase::Idle:
    case Phase::Done:
        break;
    case Phase::Settle:
        if (elapsed >= kSettleMs) {
            phase_ = Phase::MusicOnly;
            phase_start_ms_ = now_ms;
            a.reset_perf = true;
        }
        break;
    case Phase::MusicOnly:
        if (elapsed >= kMusicOnlyMs) {
            phase_ = Phase::MusicSfx;
            phase_start_ms_ = now_ms;
            next_sfx_ms_ = now_ms;
            a.capture_idle = true;
            a.reset_perf = true;
        }
        break;
    case Phase::MusicSfx:
        if (elapsed >= kMusicSfxMs) {
            phase_ = Phase::Done;
            a.capture_stress = true;
            a.finished = true;
            break;
        }
        if (int32_t(now_ms - next_sfx_ms_) >= 0) {
            // A step, a wall bump and the two arena hits, in turn: the cues a
            // player actually triggers most while music plays. A late frame
            // plays one cue, never a burst of catch-up cues.
            static constexpr SfxId kCues[] = {SfxId::MoveStep, SfxId::CombatHit, SfxId::MoveBlocked,
                                              SfxId::CombatHitHeavy};
            a.sfx = kCues[sfx_played_ % (sizeof(kCues) / sizeof(kCues[0]))];
            ++sfx_played_;
            next_sfx_ms_ += kSfxEveryMs; // a fixed cadence, not "100 ms after whenever the frame came"
            if (int32_t(now_ms - next_sfx_ms_) >= 0) next_sfx_ms_ = now_ms + kSfxEveryMs; // a late frame: resync
        }
        break;
    }
    return a;
}

uint32_t legacy_guard_us_per_block(uint32_t guard_ns, uint32_t channels_avg_x100) {
    // chip samples per block = kAudioBlockFrames * kOplClockHz / kSfxOutputRateHz
    const uint64_t calls_x100 = uint64_t(4) * channels_avg_x100 * kAudioBlockFrames * kOplClockHz / kSfxOutputRateHz;
    return uint32_t(calls_x100 * guard_ns / 100 / 1000);
}

} // namespace openu5
