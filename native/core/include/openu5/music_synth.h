#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// Alpha 3 A3-04 -- the music decoder/synth: FAT.OPL bank reader, OPL2/OPL3
// voice allocator, OPL emulator and XMI song player. Architecture and
// derivations: native/targets/tdeck/ALPHA3_AUDIO.md section 17.
//
// This is a line-for-line port of the TypeScript reference the project
// already built and shipped in the browser game (game/src/ui/opl/{bank,
// voices,chip,sequencer}.ts) -- not a fresh design. Every deliberate
// departure is called out where it happens; everything else is the same
// algorithm, the same constants, the same order of operations. That
// reference is itself original work (re/notes/music-location-mapping.md):
// an OPL2/OPL3 emulator written from the chip's documented, publicly
// known behaviour (attenuation domain, envelope phases, waveform table
// shapes), not a port of DBOPL/Nuked-OPL or any other third party emulator.
// No third-party code is reused here; none needed a license audit.
//
// One departure this port makes and the browser reference does not: it
// skips the XMI -> Standard MIDI File -> re-parse round trip
// (extractor/src/audio/xmi2midi.ts + opl/sequencer.ts parseSmf). The XMI
// EVNT chunk is walked directly into a timed event list here
// (parse_xmi_events). This changes nothing observable: xmi2midi.ts always
// emits a *fixed* tempo (500000 us/quarter, division 60 = 120 XMI ticks/s)
// and ignores every tempo meta in the source file "as xmi2mid/wildmidi do"
// (its own comment); parseSmf then reads that fixed tempo back. Baking in
// 120 ticks/s here is the same number, arrived at without serializing to
// MIDI bytes and re-parsing them. A host test cross-checks a few known
// XMI byte sequences against hand-computed tick times to prove the two
// routes agree.
//
// Everything in this file is PURE signal processing: no GameState, no
// clock, no RNG, no hardware. It renders PCM from (XMI bytes, FAT.OPL
// bytes) pairs the caller already validated (audio_pack.h). A device
// backend drives it from its own audio task, same as sfx_synth.h's
// SpeakerVoice/SfxPlayer; a host test drives it directly.
namespace openu5 {

// ---------------------------------------------------------------------------
// FAT.OPL -- the Miles AIL Global Timbre Library reader (game/src/ui/opl/
// bank.ts parseMilesOplBank, ported). The format is documented there and in
// audio_pack.h validate_timbre_bank, which checks structure only; this
// class keeps the register bytes so a voice can be programmed from them.
// ---------------------------------------------------------------------------
constexpr uint8_t kMilesBankMelodic = 0;
constexpr uint8_t kMilesBankPercussion = 127;

/** One OPL operator's five registers, exactly as FAT.OPL stores them. */
struct OplOperatorDef {
    uint8_t am_vib_eg_ksr_mult = 0; // reg 0x20+off
    uint8_t ksl_tl = 0;             // reg 0x40+off (bits 7-6 KSL, 5-0 TL)
    uint8_t attack_decay = 0;       // reg 0x60+off
    uint8_t sustain_release = 0;    // reg 0x80+off
    uint8_t waveform = 0;           // reg 0xE0+off, 0..3 (2-op OPL2 range)
};

/** One timbre: two operators plus the channel's 0xC0 (feedback/connection). */
struct OplTimbre {
    uint8_t bank = 0;
    uint8_t patch = 0;
    uint8_t fixed_note = 0; // percussion only: the fixed MIDI note it plays at
    OplOperatorDef modulator{};
    OplOperatorDef carrier{};
    uint8_t feedback_connection = 0; // bits 3-1 feedback, bit 0 connection (1=additive)
};

/** FAT.OPL holds 181 (128 melodic + 53 percussion); headroom for any Miles bank this small. */
constexpr size_t kMaxOplTimbres = 256;

class MilesOplBank {
  public:
    /**
     * Parses in place; `data` must outlive every `get`/`melodic`/`percussion`
     * call (nothing is copied out of the register bytes). Same rejects as
     * validate_timbre_bank (audio_pack.h), plus: more timbres than
     * kMaxOplTimbres. Never throws; false leaves the bank empty.
     */
    bool load(const uint8_t *data, size_t size);
    bool loaded() const { return count_ > 0; }
    size_t count() const { return count_; }

    const OplTimbre *get(uint8_t bank, uint8_t patch) const;
    /** Program not in the bank falls back to melodic 0 (or timbre[0]): heard, not silent. */
    const OplTimbre &melodic(uint8_t program) const;
    const OplTimbre *percussion(uint8_t note) const;

  private:
    OplTimbre timbres_[kMaxOplTimbres]{};
    size_t count_ = 0;
};

// ---------------------------------------------------------------------------
// OPL emulator (game/src/ui/opl/chip.ts OplEmulator, ported). Register
// writes in, PCM out; it knows nothing about MIDI, XMI or timbres.
// ---------------------------------------------------------------------------
enum class OplChipKind : uint8_t { Opl2, Opl3 };
/** The chip's native rate; every generate() call runs at this rate. */
constexpr uint32_t kOplClockHz = 49716;

class OplEmulator {
  public:
    explicit OplEmulator(OplChipKind kind = OplChipKind::Opl2);
    OplChipKind kind() const { return kind_; }
    size_t channel_count() const { return channel_count_; }

    void write_reg(uint16_t reg, uint8_t value);
    /** count samples at kOplClockHz into left/right, starting at offset. Never allocates. */
    void generate(float *left, float *right, size_t count, size_t offset = 0);
    bool is_silent() const;

  private:
    static constexpr size_t kMaxChannels = 18;

    struct Operator {
        bool am = false, vib = false, eg_sustain = false, ksr = false;
        uint8_t mult = 0, ksl = 0, tl = 0;
        uint8_t attack_rate = 0, decay_rate = 0, release_rate = 0;
        uint16_t sustain_level = 0;
        uint8_t waveform = 0;
        uint32_t phase = 0;
        uint8_t eg_state = 0; // Off/Attack/Decay/Sustain/Release
        int32_t eg_level = 511;
        int32_t ksl_atten = 0;
        int32_t prev1 = 0, prev2 = 0;

        void update_ksl(uint16_t fnum, uint8_t block);
        void key_on();
        void key_off();
        uint8_t rate_for(uint8_t reg, uint8_t key_code) const;
        void advance_envelope(uint32_t eg_counter, uint8_t key_code);
        /** Returns the operator's signed magnitude (~-4085..4085); always integer-valued. */
        int32_t sample(uint32_t phase_inc, int32_t modulation, int32_t tremolo);
    };

    struct Channel {
        Operator mod{}, car{};
        uint16_t fnum = 0;
        uint8_t block = 0;
        bool keyed = false;
        uint8_t feedback = 0;
        bool additive = false;
        bool left = true, right = true;
        uint8_t key_code() const { return uint8_t((block << 1) | ((fnum >> 9) & 1)); }
        void update_ksl();
    };

    Operator *op(size_t channel, bool carrier);
    uint32_t phase_inc(const Channel &, const Operator &) const;
    void advance_clocks();
    int32_t tremolo_atten() const;

    OplChipKind kind_;
    size_t channel_count_;
    Channel channels_[kMaxChannels]{};
    uint32_t eg_counter_ = 0;
    uint32_t tremolo_counter_ = 0, tremolo_pos_ = 0;
    uint32_t vibrato_counter_ = 0, vibrato_pos_ = 0;
    bool tremolo_depth_ = false, vibrato_depth_ = false;
    bool opl3_enabled_ = false;
};

// ---------------------------------------------------------------------------
// Voice allocator (game/src/ui/opl/voices.ts OplVoiceAllocator, ported).
// MIDI-ish events in, register writes out (via a sink so nothing allocates
// on the audio path -- the same reason sfx_synth.h's SfxPlayer never does).
// ---------------------------------------------------------------------------
constexpr uint32_t kBendRangeSemitones = 2; // the corpus never sends RPN 0/6: GM default holds
constexpr uint8_t kDrumChannel = 9;

/** note (with bend already applied, in semitones) -> OPL F-Number/Block. */
struct FnumBlock { uint16_t fnum; uint8_t block; };
FnumBlock note_to_fnum_block(float note_with_bend);
/** TL is attenuation (0 = loudest); vol01 in [0,1] scales it, KSL bits kept. */
uint8_t scale_tl(uint8_t ksl_tl, float vol01);

class OplVoiceAllocator {
  public:
    using RegSink = void (*)(void *ctx, uint16_t reg, uint8_t value);

    /** `sink` receives every register write in order; never null. */
    OplVoiceAllocator(const MilesOplBank &bank, OplChipKind chip, void *sink_ctx, RegSink sink);

    OplChipKind chip() const { return chip_; }
    size_t voice_count() const { return voice_count_; }

    /** Mutes every voice and leaves registers in a known state (call once before use). */
    void reset();
    void note_on(uint8_t channel, uint8_t note, uint8_t velocity);
    void note_off(uint8_t channel, uint8_t note);
    void program_change(uint8_t channel, uint8_t program);
    void pitch_bend(uint8_t channel, uint16_t value14);
    void control_change(uint8_t channel, uint8_t controller, uint8_t value);
    /** channel < 0: every channel. */
    void all_notes_off(int channel = -1);

    size_t active_voices() const;

  private:
    static constexpr size_t kMaxVoices = 18;
    static constexpr size_t kVoicesPerArray = 9;

    struct ChannelState {
        uint8_t program = 0;
        uint8_t volume = 100, expression = 127, pan = 64;
        float bend_semitones = 0;
        bool sustain = false;
    };
    struct Voice {
        int8_t midi_ch = -1;
        int16_t note = -1, played_note = -1;
        const OplTimbre *timbre = nullptr;
        uint8_t velocity = 0;
        bool keyed = false, sustained = false;
        uint16_t fnum = 0;
        uint8_t block = 0;
        uint32_t age_on = 0, age_off = 0;
    };

    void emit(uint16_t reg, uint8_t value) { sink_(sink_ctx_, reg, value); }
    size_t array_base(size_t voice) const { return voice < kVoicesPerArray ? 0x000 : 0x100; }
    size_t slot(size_t voice) const { return voice % kVoicesPerArray; }
    size_t op_offset(size_t voice) const;
    float volume_of(const ChannelState &, uint8_t velocity) const;
    uint8_t stereo_for(const ChannelState &, const OplTimbre &) const;
    void write_operator(size_t base, size_t off, const OplOperatorDef &, uint8_t ksl_tl);
    void write_timbre(size_t voice, float vol01);
    void write_pitch(size_t voice, bool key_on);
    size_t pick_voice(uint8_t midi_ch, uint8_t note) const;
    void release_voice(size_t voice);
    void refresh_pitch(uint8_t channel);
    void refresh_volume(uint8_t channel);
    void all_notes_off_impl(int channel);

    const MilesOplBank &bank_;
    OplChipKind chip_;
    size_t voice_count_;
    void *sink_ctx_;
    RegSink sink_;
    ChannelState channels_[16]{};
    Voice voices_[kMaxVoices]{};
    uint32_t age_ = 0;
};

// ---------------------------------------------------------------------------
// XMI event parsing (extractor/src/audio/xmi2midi.ts evntToMidi's event
// extraction, ported -- without the MIDI re-encode; see the file header).
// ---------------------------------------------------------------------------
/** One already-timed, already-expanded MIDI-ish message. */
struct MusicEvent {
    uint32_t tick = 0;       // XMI ticks, fixed 120/second
    uint8_t status = 0;      // 0x80.. (note off/on, CC, program, bend); 0xFF = end-of-track marker
    uint8_t d1 = 0, d2 = 0;
};

/** XMI ticks per second (the driver's own fixed rate; see file header). */
constexpr uint32_t kXmiTicksPerSecond = 120;

struct MusicTrack {
    // Sized exactly once, when the song is parsed (never during render()):
    // no fixed cap, so no song is ever silently truncated.
    std::vector<MusicEvent> events;
    uint32_t end_tick = 0; // last event's tick; the loop point (no cue carries a tail)
};

/**
 * Parses the FIRST EVNT chunk of an XMI file (every song in the patch has
 * exactly one, audio_pack.h validate_xmi already proved it) into a flat,
 * time-sorted event list: delay bytes accumulate into ticks, a Note On's
 * VLQ duration becomes a delayed Note Off (velocity 0, XMI carries no 0x8n
 * at all -- ported unchanged from voices.ts's own measurement of the
 * corpus), tempo metas (0xFF 0x51) are dropped (kXmiTicksPerSecond is
 * already that fixed tempo), End of Track (0xFF 0x2F) ends the scan.
 * False on any structural error. Allocates once (`out.events`); never
 * called from a render path.
 */
bool parse_xmi_events(const uint8_t *data, size_t size, MusicTrack &out);

// ---------------------------------------------------------------------------
// Test-only: the OPL ROMs' checkable anchors (chip.ts's own `__tables`
// export). Not part of the playback contract.
// ---------------------------------------------------------------------------
namespace test_only {
uint16_t opl_log_sin(int index);
uint16_t opl_exp(int index);
int32_t opl_expo(int32_t att);
} // namespace test_only

// ---------------------------------------------------------------------------
// Song player (game/src/ui/opl/sequencer.ts OplSongPlayer, ported). One
// MIDI-ish track + one bank -> a continuous PCM stream at any output rate.
// ---------------------------------------------------------------------------
class MusicSongPlayer {
  public:
    /**
     * `track` and `bank` must outlive the player (their bytes are read live,
     * not copied -- the caller keeps the audio pack payload resident, see
     * ALPHA3_AUDIO.md section 17.7). `loop`: the patch's own behaviour is to
     * restart the same song on every key poll once it ends (measured: no
     * song carries an AIL loop controller), so the default is true.
     */
    void start(const MusicTrack &track, const MilesOplBank &bank, OplChipKind chip = OplChipKind::Opl2,
               bool loop = true);
    void stop();
    bool active() const { return active_; }
    /** True once a non-looping track has run its tail out. */
    bool ended() const { return ended_; }

    /**
     * Renders `frames` MONO samples at `output_rate_hz`, mixed down from the
     * chip's stereo bus and scaled by `gain_q15` ONCE, saturating -- the same
     * contract as sfx_synth.h SfxPlayer::render(). Silence (and returns) if
     * not active. Never allocates: start() sizes the chip-rate buffer for
     * the first render() call's frame count once (growing later only if a
     * bigger request arrives -- the device always asks for the same chunk
     * size, so in practice this allocates exactly once per song, never per
     * frame).
     */
    void render(int16_t *mono_out, size_t frames, uint32_t output_rate_hz, uint16_t gain_q15);

  private:
    void dispatch(const MusicEvent &);
    void fill_chip(float *l, float *r, size_t n, size_t base = 0);
    void ensure_capacity(size_t frames);
    static void sink_trampoline(void *ctx, uint16_t reg, uint8_t value);

    const MusicTrack *track_ = nullptr;
    const MilesOplBank *bank_ = nullptr;
    OplEmulator chip_{OplChipKind::Opl2};
    std::unique_ptr<OplVoiceAllocator> alloc_; // (re)built once per start(), never in render()
    bool loop_ = true;
    bool active_ = false, ended_ = false;
    size_t event_index_ = 0;
    double song_sample_ = 0; // chip-rate samples since song start
    double loop_sample_ = 0, end_sample_ = 0;

    std::unique_ptr<float[]> chip_l_, chip_r_;
    size_t chip_capacity_ = 0;
    size_t have_ = 0;
    double read_pos_ = 0;
    double ratio_ = 1.0; // kOplClockHz / output_rate_hz
    uint32_t last_output_rate_ = 0;
};

} // namespace openu5
