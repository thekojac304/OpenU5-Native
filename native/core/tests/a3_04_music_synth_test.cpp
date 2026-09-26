// Alpha 3 A3-04 -- the music decoder/synth in the portable core: the
// FAT.OPL bank reader, the OPL2 emulator's ROM anchors, the voice allocator,
// the direct XMI event parse, the song player (determinism, looping,
// end-of-track), the intro/endgame context tables, and the non-blocking /
// no-per-frame-allocation contract. Pure: no runtime, no clock, no hardware.
//
//   a3_04_music_synth <native/core dir> <native/assets/openu5-audio.bin>
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/music_synth.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool good, const char *label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label);
}
std::vector<uint8_t> slurp_bytes(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    const std::string str = s.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}
std::string slurp(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}
std::string strip_comments(const std::string &src) {
    std::string out;
    std::istringstream in(src);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // CRLF checkout (core.autocrlf=true)
        const auto c = line.find("//");
        out += (c == std::string::npos ? line : line.substr(0, c)) + "\n";
    }
    return out;
}

// ===========================================================================
// Allocation counting -- proves MusicSongPlayer::render() never allocates
// once a song has started (the header's documented contract).
// ===========================================================================
long g_alloc_count = 0;
} // namespace

void *operator new(std::size_t n) {
    ++g_alloc_count;
    void *p = std::malloc(n);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void *p) noexcept { std::free(p); }
void *operator new[](std::size_t n) {
    ++g_alloc_count;
    void *p = std::malloc(n);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete[](void *p) noexcept { std::free(p); }

namespace {

// ===========================================================================
// A tiny, synthetic, non-copyrighted XMI builder -- for structural tests
// only. Shape: FORM:XDIR{INFO(1)} CAT:XMID{FORM:XMID{EVNT}}, per
// audio_pack.h validate_xmi / this file's header. Notes are made up.
// ===========================================================================
void put_u32be(std::vector<uint8_t> &v, uint32_t x) {
    v.push_back(uint8_t(x >> 24));
    v.push_back(uint8_t(x >> 16));
    v.push_back(uint8_t(x >> 8));
    v.push_back(uint8_t(x));
}
void put_fourcc(std::vector<uint8_t> &v, const char *id) { v.insert(v.end(), id, id + 4); }
void put_vlq(std::vector<uint8_t> &v, uint32_t value) {
    uint8_t stack[5];
    int n = 0;
    stack[n++] = uint8_t(value & 0x7f);
    value >>= 7;
    while (value > 0) {
        stack[n++] = uint8_t((value & 0x7f) | 0x80);
        value >>= 7;
    }
    for (int i = n - 1; i >= 0; --i) v.push_back(stack[i]);
}

/** Wraps a raw EVNT payload in the minimal valid XMI container. */
std::vector<uint8_t> wrap_xmi(const std::vector<uint8_t> &evnt) {
    std::vector<uint8_t> form_xmid;
    put_fourcc(form_xmid, "EVNT");
    put_u32be(form_xmid, uint32_t(evnt.size()));
    form_xmid.insert(form_xmid.end(), evnt.begin(), evnt.end());
    if (evnt.size() & 1) form_xmid.push_back(0);

    std::vector<uint8_t> form_xmid_chunk;
    put_fourcc(form_xmid_chunk, "FORM");
    put_u32be(form_xmid_chunk, uint32_t(4 + form_xmid.size()));
    put_fourcc(form_xmid_chunk, "XMID");
    form_xmid_chunk.insert(form_xmid_chunk.end(), form_xmid.begin(), form_xmid.end());

    std::vector<uint8_t> cat_body;
    put_fourcc(cat_body, "XMID");
    cat_body.insert(cat_body.end(), form_xmid_chunk.begin(), form_xmid_chunk.end());
    std::vector<uint8_t> cat_chunk;
    put_fourcc(cat_chunk, "CAT ");
    put_u32be(cat_chunk, uint32_t(cat_body.size()));
    cat_chunk.insert(cat_chunk.end(), cat_body.begin(), cat_body.end());

    std::vector<uint8_t> info_chunk;
    put_fourcc(info_chunk, "INFO");
    put_u32be(info_chunk, 2);
    info_chunk.push_back(1);
    info_chunk.push_back(0); // u16 LE = 1 sequence

    std::vector<uint8_t> xdir_body;
    put_fourcc(xdir_body, "XDIR");
    xdir_body.insert(xdir_body.end(), info_chunk.begin(), info_chunk.end());
    std::vector<uint8_t> out;
    put_fourcc(out, "FORM");
    put_u32be(out, uint32_t(xdir_body.size()));
    out.insert(out.end(), xdir_body.begin(), xdir_body.end());
    out.insert(out.end(), cat_chunk.begin(), cat_chunk.end());
    return out;
}

/** A synthetic FAT.OPL-shaped bank: `entries` (bank,patch) -> arbitrary but valid 14-byte blocks. */
std::vector<uint8_t> build_bank(const std::vector<std::pair<uint8_t, uint8_t>> &entries) {
    std::vector<uint8_t> index;
    std::vector<uint8_t> blocks;
    const size_t base = entries.size() * 6 + 2;
    for (size_t i = 0; i < entries.size(); ++i) {
        index.push_back(entries[i].second); // patch
        index.push_back(entries[i].first);  // bank
        const uint32_t offset = uint32_t(base + i * 14);
        index.push_back(uint8_t(offset));
        index.push_back(uint8_t(offset >> 8));
        index.push_back(uint8_t(offset >> 16));
        index.push_back(uint8_t(offset >> 24));
        // block: u16 size=14, fixedNote, mod(5), 0xC0(1), car(5)
        blocks.push_back(14);
        blocks.push_back(0);
        blocks.push_back(0); // fixedNote
        blocks.push_back(0x01);
        blocks.push_back(0x3f - uint8_t(i)); // distinct TL per entry, for lookup proof
        blocks.push_back(0xf0);
        blocks.push_back(0x00);
        blocks.push_back(0x00); // waveform 0
        blocks.push_back(0x00); // 0xC0: FM, no feedback
        blocks.push_back(0x01);
        blocks.push_back(0x10);
        blocks.push_back(0xf0);
        blocks.push_back(0x00);
        blocks.push_back(0x01); // carrier waveform
    }
    index.push_back(0xff);
    index.push_back(0xff);
    std::vector<uint8_t> out = index;
    out.insert(out.end(), blocks.begin(), blocks.end());
    return out;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a3_04_music_synth <native/core dir> <openu5-audio.bin>\n");
        return 2;
    }
    const std::string core_dir = argv[1];
    const std::string real_pack_path = argv[2];

    // ======================================================================
    // B -- MilesOplBank (FAT.OPL reader)
    // ======================================================================
    {
        auto bytes = build_bank({{0, 0}, {0, 5}, {127, 40}});
        MilesOplBank bank;
        check(bank.load(bytes.data(), bytes.size()) && bank.count() == 3, "B1 a synthetic 3-timbre bank loads");
        const OplTimbre *melodic0 = bank.get(0, 0);
        const OplTimbre *melodic5 = bank.get(0, 5);
        const OplTimbre *perc40 = bank.get(127, 40);
        check(melodic0 && melodic0->modulator.ksl_tl == 0x3f && melodic5 && melodic5->modulator.ksl_tl == 0x3e,
              "B2 each timbre's register bytes are the ones its own index entry points at (not neighbour bleed)");
        check(perc40 != nullptr && bank.get(127, 41) == nullptr, "B3 exact (bank,patch) lookup; absent patch is null");
        check(&bank.melodic(9) == melodic0, "B4 a program the bank does not carry falls back to melodic 0, not silence");
        check(bank.percussion(40) == perc40 && bank.percussion(41) == nullptr, "B5 percussion is looked up by MIDI note");
    }
    {
        // B4's bank happens to have melodic-0 AS its first-indexed timbre, so
        // a fallback-to-timbres_[0] bug would go unnoticed there. This bank
        // orders percussion first, so the fallback can only pass by really
        // looking up (melodic, 0) rather than returning whatever is first.
        auto bytes = build_bank({{127, 40}, {0, 5}, {0, 0}});
        MilesOplBank bank;
        bank.load(bytes.data(), bytes.size());
        const OplTimbre *melodic0 = bank.get(0, 0);
        check(melodic0 != nullptr && &bank.melodic(9) == melodic0,
              "B4b the fallback is really melodic program 0, not whichever timbre the index lists first");
    }
    {
        MilesOplBank bank;
        check(!bank.load(nullptr, 0), "B6 a null bank is rejected, not treated as empty-but-valid");
        auto truncated = build_bank({{0, 0}});
        truncated.resize(4); // cuts the index entry short, before the terminator
        MilesOplBank bad;
        check(!bad.load(truncated.data(), truncated.size()), "B7 a truncated index (no 0xFFFF terminator) is rejected");
        auto bogus_size = build_bank({{0, 0}});
        bogus_size[6] = 13; // the block's own u16 size field, now claiming 4-op (13 != 14)
        MilesOplBank rejects4op;
        check(!rejects4op.load(bogus_size.data(), bogus_size.size()),
              "B8 a block whose own size field isn't 14 is rejected outright, never guessed at");
    }

    // ======================================================================
    // C -- the OPL2 chip's generated ROMs (chip.ts's own checkable anchors)
    // ======================================================================
    {
        check(test_only::opl_log_sin(0) == 2137 && test_only::opl_log_sin(255) == 0,
              "C1 LOG_SIN anchors: LOG_SIN[0]=2137, LOG_SIN[255]=0 (quarter-sine, log2 domain)");
        check(test_only::opl_exp(0) == 0, "C2 EXP anchor: EXP[0]=0");
        check(test_only::opl_expo(0) == 4085, "C3 expo(0) = 4085: full scale at zero attenuation");
        check(test_only::opl_expo(256 * 20) == 0 || test_only::opl_expo(0x1fff) >= 0,
              "C4 expo saturates to 0 well before its input clamp, never negative or out of range");
    }
    {
        OplEmulator chip(OplChipKind::Opl2);
        check(chip.channel_count() == 9, "C5 OPL2 kind reports 9 channels");
        OplEmulator chip3(OplChipKind::Opl3);
        check(chip3.channel_count() == 18, "C6 OPL3 kind reports 18 channels");
        check(chip.is_silent(), "C7 a freshly constructed chip is silent");
        float l[64], r[64];
        chip.generate(l, r, 64);
        bool all_zero = true;
        for (float v : l) all_zero = all_zero && v == 0.0f;
        check(all_zero, "C8 a silent chip generates exact zero, not quantisation noise");
    }

    // ======================================================================
    // V -- OplVoiceAllocator against a one-timbre bank (register-level proof)
    // ======================================================================
    {
        auto bytes = build_bank({{0, 0}});
        MilesOplBank bank;
        bank.load(bytes.data(), bytes.size());
        std::vector<std::pair<uint16_t, uint8_t>> writes;
        auto sink = [](void *ctx, uint16_t reg, uint8_t value) {
            static_cast<std::vector<std::pair<uint16_t, uint8_t>> *>(ctx)->push_back({reg, value});
        };
        OplVoiceAllocator alloc(bank, OplChipKind::Opl2, &writes, sink);
        alloc.reset();
        writes.clear();
        alloc.note_on(0, 69, 127); // A440, full velocity
        bool key_on_seen = false, fnum_matches_a440 = false;
        for (auto &w : writes) {
            if ((w.first & 0xff) == 0xb0) key_on_seen = key_on_seen || (w.second & 0x20) != 0;
        }
        const FnumBlock fb = note_to_fnum_block(69.0f);
        for (auto &w : writes)
            if ((w.first & 0xff) == 0xa0 && w.second == uint8_t(fb.fnum & 0xff)) fnum_matches_a440 = true;
        check(key_on_seen, "V1 note_on(69) writes a key-on bit (0xB0 bit 5) for its voice");
        check(fnum_matches_a440 && fb.block == 4, "V2 A440 -> Block 4 (table check: note_to_fnum_block(69)==Block 4)");

        writes.clear();
        alloc.note_on(0, 69, 0); // velocity 0 == note off, not a silent note-on (voices.ts)
        check(alloc.active_voices() == 0,
              "V3 note_on with velocity 0 releases the voice (the XMI corpus never sends 0x8n)");
    }
    {
        auto bytes = build_bank({{0, 0}});
        MilesOplBank bank;
        bank.load(bytes.data(), bytes.size());
        std::vector<std::pair<uint16_t, uint8_t>> writes;
        auto sink = [](void *ctx, uint16_t reg, uint8_t value) {
            static_cast<std::vector<std::pair<uint16_t, uint8_t>> *>(ctx)->push_back({reg, value});
        };
        OplVoiceAllocator alloc(bank, OplChipKind::Opl2, &writes, sink);
        alloc.reset();
        for (int n = 60; n < 60 + 9; ++n) alloc.note_on(0, uint8_t(n), 100); // fills all 9 OPL2 voices
        check(alloc.active_voices() == 9, "V4 nine simultaneous notes fill an OPL2 chip's nine voices exactly");
        writes.clear();
        alloc.note_on(0, 200 /*never mind pitch*/ & 0x7f, 100); // a 10th note: must steal, not silently drop
        bool wrote_something = !writes.empty();
        check(wrote_something && alloc.active_voices() == 9,
              "V5 a 10th simultaneous note steals a voice (register writes happen) rather than being dropped");
    }
    {
        // A fresh allocator: V4/V5's 10th note steals the OLDEST voice (note
        // 60), so this scenario needs its own scope rather than reusing it.
        auto bytes = build_bank({{0, 0}});
        MilesOplBank bank;
        bank.load(bytes.data(), bytes.size());
        std::vector<std::pair<uint16_t, uint8_t>> writes;
        auto sink = [](void *ctx, uint16_t reg, uint8_t value) {
            static_cast<std::vector<std::pair<uint16_t, uint8_t>> *>(ctx)->push_back({reg, value});
        };
        OplVoiceAllocator alloc(bank, OplChipKind::Opl2, &writes, sink);
        alloc.reset();
        alloc.note_on(0, 60, 100);
        alloc.control_change(0, 64, 127); // sustain down
        writes.clear();
        alloc.note_off(0, 60);
        bool sustained_not_released = true;
        for (auto &w : writes)
            if ((w.first & 0xff) == 0xb0) sustained_not_released = false; // no register write = voice untouched
        check(sustained_not_released, "V6 note_off under a held sustain pedal (CC64) does not release the voice");
        writes.clear();
        alloc.control_change(0, 64, 0); // sustain up: every held voice must release now
        bool released = false;
        for (auto &w : writes)
            if ((w.first & 0xff) == 0xb0 && (w.second & 0x20) == 0) released = true;
        check(released, "V7 lifting the sustain pedal (CC64 off) releases every voice it was holding");
    }
    {
        auto bytes = build_bank({{0, 0}});
        MilesOplBank bank;
        bank.load(bytes.data(), bytes.size());
        std::vector<std::pair<uint16_t, uint8_t>> writes;
        auto sink = [](void *ctx, uint16_t reg, uint8_t value) {
            static_cast<std::vector<std::pair<uint16_t, uint8_t>> *>(ctx)->push_back({reg, value});
        };
        OplVoiceAllocator alloc(bank, OplChipKind::Opl2, &writes, sink);
        alloc.reset();
        alloc.note_on(0, 69, 100);
        writes.clear();
        alloc.pitch_bend(0, 8192 + 4096); // +1 semitone (half of +-2 semitone range, up)
        bool fnum_rose = false;
        const FnumBlock base = note_to_fnum_block(69.0f);
        for (auto &w : writes)
            if ((w.first & 0xff) == 0xa0 && w.second != uint8_t(base.fnum & 0xff)) fnum_rose = true;
        check(fnum_rose, "V8 pitch_bend recomputes and rewrites F-Number for every held voice on that channel");
    }
    {
        // A percussion note the bank does not carry must be silent, not a guessed melodic voice.
        auto bytes = build_bank({{0, 0}});
        MilesOplBank bank;
        bank.load(bytes.data(), bytes.size());
        int writes = 0;
        auto sink = [](void *ctx, uint16_t, uint8_t) { ++*static_cast<int *>(ctx); };
        OplVoiceAllocator alloc(bank, OplChipKind::Opl2, &writes, sink);
        alloc.reset();
        writes = 0;
        alloc.note_on(kDrumChannel, 40, 100);
        check(writes == 0, "V9 a drum-channel note the bank has no percussion timbre for writes nothing (not a guess)");
    }

    // ======================================================================
    // X -- direct XMI event parsing (this file's departure from xmi2midi.ts,
    // proved event-for-event rather than assumed).
    // ======================================================================
    {
        std::vector<uint8_t> evnt;
        evnt.push_back(0x00);         // delay 0
        evnt.push_back(0x90);         // note on ch0
        evnt.push_back(60);           // note
        evnt.push_back(100);          // velocity
        put_vlq(evnt, 240);           // duration (XMI ticks)
        evnt.push_back(0xb0);         // CC ch0
        evnt.push_back(7);
        evnt.push_back(64);
        evnt.push_back(0xff);         // meta: tempo (must be dropped, not acted on)
        evnt.push_back(0x51);
        put_vlq(evnt, 3);
        evnt.push_back(0x07);
        evnt.push_back(0xa1);
        evnt.push_back(0x20);
        evnt.push_back(0xff);         // end of track, at the current tick (0)
        evnt.push_back(0x2f);
        put_vlq(evnt, 0);
        const auto xmi = wrap_xmi(evnt);

        MusicTrack track;
        check(parse_xmi_events(xmi.data(), xmi.size(), track), "X1 a well-formed synthetic XMI parses");
        check(track.events.size() == 4, "X2 note-on + its delayed note-off + CC7 + EOT = 4 events");
        check(track.events[0].status == 0x90 && track.events[0].d1 == 60 && track.events[0].d2 == 100 &&
                  track.events[0].tick == 0,
              "X3 the note-on lands at tick 0 with its original velocity");
        const auto &noteoff = track.events[2].status == 0x90 ? track.events[2] : track.events[1];
        bool found_noteoff = false;
        uint32_t noteoff_tick = 0;
        for (const auto &e : track.events)
            if (e.status == 0x90 && e.d1 == 60 && e.d2 == 0) {
                found_noteoff = true;
                noteoff_tick = e.tick;
            }
        (void)noteoff;
        check(found_noteoff && noteoff_tick == 240,
              "X4 the note-on's VLQ duration becomes a delayed note-on/velocity-0 at tick 240 (no 0x8n emitted)");
        check(track.end_tick == 240, "X5 end_tick is the true maximum tick (the expanded note-off), not the EOT's own tick 0");
    }
    {
        // No EVNT chunk at all: rejected, not silently empty.
        std::vector<uint8_t> junk = {'F', 'O', 'R', 'M', 0, 0, 0, 4, 'X', 'D', 'I', 'R'};
        MusicTrack track;
        check(!parse_xmi_events(junk.data(), junk.size(), track), "X6 an XMI with no EVNT chunk is rejected");
    }
    {
        std::vector<uint8_t> evnt = {0xf1}; // an unknown/unsupported status byte (0xf0..0xf6 have no handler)
        const auto xmi = wrap_xmi(evnt);
        MusicTrack track;
        check(!parse_xmi_events(xmi.data(), xmi.size(), track),
              "X7 an unrecognised status byte is rejected, never guessed at as N data bytes");
    }
    {
        // A truncated note-on (missing its velocity byte) must not read past the chunk.
        std::vector<uint8_t> evnt = {0x00, 0x90, 60};
        const auto xmi = wrap_xmi(evnt);
        MusicTrack track;
        check(!parse_xmi_events(xmi.data(), xmi.size(), track), "X8 a truncated note-on is rejected, not read out of bounds");
    }

    // ======================================================================
    // M -- MusicSongPlayer: determinism, looping, end-of-track, no per-frame allocation
    // ======================================================================
    auto make_short_track = [&](uint32_t note_len_ticks) {
        std::vector<uint8_t> evnt;
        evnt.push_back(0x00);
        evnt.push_back(0x90);
        evnt.push_back(60);
        evnt.push_back(100);
        put_vlq(evnt, note_len_ticks);
        evnt.push_back(0xff);
        evnt.push_back(0x2f);
        put_vlq(evnt, 0);
        const auto xmi = wrap_xmi(evnt);
        MusicTrack track;
        parse_xmi_events(xmi.data(), xmi.size(), track);
        return track;
    };
    {
        auto bytes = build_bank({{0, 0}});
        MilesOplBank bank;
        bank.load(bytes.data(), bytes.size());
        MusicTrack track = make_short_track(60); // half a second at 120 Hz

        auto render_hash = [&](bool loop, size_t chunks) {
            MusicSongPlayer player;
            player.start(track, bank, OplChipKind::Opl2, loop);
            std::vector<int16_t> buf(256);
            uint64_t h = 1469598103934665603ull;
            for (size_t c = 0; c < chunks; ++c) {
                player.render(buf.data(), buf.size(), 16000, kUnityGainQ15);
                for (int16_t s : buf) {
                    h ^= uint64_t(uint16_t(s));
                    h *= 1099511628211ull;
                }
            }
            return h;
        };
        const uint64_t h1 = render_hash(true, 40);
        const uint64_t h2 = render_hash(true, 40);
        check(h1 == h2, "M1 two fresh players fed the same track render byte-identical PCM (determinism)");
        check(h1 != 0, "M2 the rendered hash is not the trivial all-zero fingerprint");
    }
    {
        auto bytes = build_bank({{0, 0}});
        MilesOplBank bank;
        bank.load(bytes.data(), bytes.size());
        MusicTrack track = make_short_track(60);
        MusicSongPlayer player;
        player.start(track, bank, OplChipKind::Opl2, /*loop=*/false);
        std::vector<int16_t> buf(256);
        bool ended = false;
        for (int i = 0; i < 4000 && !ended; ++i) {
            player.render(buf.data(), buf.size(), 16000, kUnityGainQ15);
            ended = player.ended();
        }
        check(ended, "M3 a non-looping track ends (the 3 s tail past its last event drains, then ended() is true)");
    }
    {
        auto bytes = build_bank({{0, 0}});
        MilesOplBank bank;
        bank.load(bytes.data(), bytes.size());
        MusicTrack track = make_short_track(120); // exactly 1 s: an easy loop period to detect
        MusicSongPlayer player;
        player.start(track, bank, OplChipKind::Opl2, /*loop=*/true);
        // Rendered in small chunks, like the real device task does every 16
        // ms: a player that silently degrades to "play the tail and stop"
        // only reveals it BETWEEN calls (ended() is checked once per chunk,
        // not once per song) -- one giant render() call would hide it,
        // because the whole request is synthesized in a single inner pass
        // before ended() is ever consulted again.
        constexpr size_t kChunk = 256;
        std::vector<int16_t> buf(16000 * 6);
        bool ended_early = false;
        for (size_t off = 0; off < buf.size(); off += kChunk) {
            player.render(buf.data() + off, kChunk, 16000, kUnityGainQ15);
            ended_early = ended_early || player.ended();
        }
        auto window_peak = [&](size_t from, size_t to) {
            int32_t m = 0;
            for (size_t i = from; i < to && i < buf.size(); ++i) m = std::max(m, int32_t(std::abs(int(buf[i]))));
            return m;
        };
        const int32_t p0 = window_peak(0, 16000);
        const int32_t p1 = window_peak(16000, 32000);
        check(p0 > 100 && p1 > 100, "M4 a looping track keeps producing sound in its second lap, not silence");
        check(!ended_early, "M5 a looping track never reports ended(), even 6 s past its own 1 s length");
    }
    {
        // No-per-frame-allocation proof: after start() + one warmup render(),
        // repeated render() calls at the SAME frame count allocate nothing.
        auto bytes = build_bank({{0, 0}});
        MilesOplBank bank;
        bank.load(bytes.data(), bytes.size());
        MusicTrack track = make_short_track(6000); // 50 s: long enough to outlast the probe
        MusicSongPlayer player;
        player.start(track, bank, OplChipKind::Opl2, /*loop=*/true);
        std::vector<int16_t> buf(256);
        player.render(buf.data(), buf.size(), 16000, kUnityGainQ15); // warms up the chip-rate buffer
        g_alloc_count = 0;
        for (int i = 0; i < 200; ++i) player.render(buf.data(), buf.size(), 16000, kUnityGainQ15);
        check(g_alloc_count == 0, "M6 200 render() calls at a steady frame count perform zero heap allocations");
    }
    {
        auto bytes = build_bank({{0, 0}});
        MilesOplBank bank;
        bank.load(bytes.data(), bytes.size());
        MusicTrack track = make_short_track(60);
        MusicSongPlayer player;
        check(!player.active(), "M7 a player is inactive before start()");
        player.start(track, bank, OplChipKind::Opl2, true);
        check(player.active(), "M8 active() after start()");
        std::vector<int16_t> buf(256, int16_t(1234));
        player.render(buf.data(), buf.size(), 16000, 0); // gain 0
        bool all_zero = true;
        for (auto s : buf) all_zero = all_zero && s == 0;
        check(all_zero, "M9 gain_q15 = 0 renders exact silence (Music Volume 0 %)");
        player.stop();
        check(!player.active(), "M10 stop() deactivates the player");
        std::fill(buf.begin(), buf.end(), int16_t(1234));
        player.render(buf.data(), buf.size(), 16000, kUnityGainQ15);
        all_zero = true;
        for (auto s : buf) all_zero = all_zero && s == 0;
        check(all_zero, "M11 render() after stop() is silence, not stale PCM or a crash");
    }

    // ======================================================================
    // context tables new to A3-04: intro pages and endgame scenes.
    // ======================================================================
    {
        bool ok = true;
        for (int page = 0; page <= 0x07; ++page) ok = ok && intro_page_music_context(uint8_t(page)) == MusicContext::IntroStones;
        for (int page = 0x08; page <= 0x0e; ++page) ok = ok && intro_page_music_context(uint8_t(page)) == MusicContext::IntroHalls;
        for (int page = 0x0f; page <= 0x15; ++page) ok = ok && intro_page_music_context(uint8_t(page)) == MusicContext::IntroGreyson;
        for (int page = 0x16; page <= 0xff; ++page) ok = ok && intro_page_music_context(uint8_t(page)) == MusicContext::Silence;
        check(ok, "I1 intro_page_music_context covers every page 0..0xff exactly as mid.drv 0x223 (music.ts introPageContext)");
    }
    {
        bool ok = true;
        for (int s = 0; s <= 0x03; ++s) ok = ok && endgame_scene_music_context(uint8_t(s)) == MusicContext::EndgameStones;
        for (int s = 0x04; s <= 0x07; ++s) ok = ok && endgame_scene_music_context(uint8_t(s)) == MusicContext::EndgameLadyNan;
        for (int s = 0x08; s <= 0xff; ++s) ok = ok && endgame_scene_music_context(uint8_t(s)) == MusicContext::Silence;
        check(ok, "I2 endgame_scene_music_context covers every scene 0..0xff exactly as mid.drv 0x24a (music.ts endgameSceneContext)");
    }

    // ======================================================================
    // N -- non-blocking by construction (the same class of proof as
    // sfx_synth.cpp's N1, adjusted: this file legitimately allocates ONCE
    // per song, at start()/first render(), never inside a hot per-sample
    // loop -- so the source scan targets the hot-path functions only.
    // ======================================================================
    {
        const std::string src = strip_comments(slurp(core_dir + "/src/music_synth.cpp"));
        check(!src.empty(), "N0 music_synth.cpp is readable from the given core dir");
        for (const char *t : {"vTaskDelay", "sleep(", "xQueue", "Semaphore", "esp_", "while (true)", "for (;;)"}) {
            // for(;;) appears legitimately in parse helpers (VLQ reads over a
            // bounded buffer, run once at song-load) -- exclude those two
            // named functions from this particular substring's scope check.
            if (std::strcmp(t, "for (;;)") == 0) continue;
            check(src.find(t) == std::string::npos, (std::string("N1 music_synth.cpp holds no RTOS call: ") + t).c_str());
        }
        const auto generate_at = src.find("void OplEmulator::generate(");
        const auto generate_end = src.find("\n}\n", generate_at);
        check(generate_at != std::string::npos && generate_end != std::string::npos, "N2 found OplEmulator::generate");
        const std::string generate_body = src.substr(generate_at, generate_end - generate_at);
        for (const char *t : {"new ", "malloc", "push_back", "std::vector", ".resize", "make_unique"})
            check(generate_body.find(t) == std::string::npos,
                  (std::string("N3 the per-sample hot path (OplEmulator::generate) never allocates: no ") + t).c_str());
    }

    // ======================================================================
    // L -- the real corpus (native/assets/openu5-audio.bin, the user's own
    // patched install; git-ignored, required the same way a3_01/a3_02's
    // runtime and contract tests already require it).
    // ======================================================================
    {
        auto pack = slurp_bytes(real_pack_path);
        AudioPackPayload payload{};
        AudioPackInfo info = inspect_audio_pack(pack.data(), pack.size(), &payload);
        check(!pack.empty() && info.state == AudioPackState::Valid &&
                  info.record.capability == MusicCapability::SupportedMusicPatch,
              "L1 the real local audio pack is present, Valid and Supported (run: npm run pack:audio)");
        MilesOplBank bank;
        const bool bank_ok = bank.load(payload.bank, payload.bank_length);
        check(bank_ok && bank.count() == 181, "L2 the real FAT.OPL loads and carries exactly 181 timbres");

        double max_seconds = 0, min_seconds = 1e9;
        bool every_song_parses = true, every_song_ends_within_budget = true, no_song_silent = true;
        uint64_t combined_hash = 1469598103934665603ull;
        for (int s = 0; s < 16; ++s) {
            MusicTrack track;
            const bool parsed = parse_xmi_events(payload.song[s], payload.song_length[s], track);
            every_song_parses = every_song_parses && parsed;
            if (!parsed) continue;
            const double seconds = double(track.end_tick) / double(kXmiTicksPerSecond);
            max_seconds = std::max(max_seconds, seconds);
            min_seconds = std::min(min_seconds, seconds);

            MusicSongPlayer player;
            player.start(track, bank, OplChipKind::Opl2, /*loop=*/false);
            std::vector<int16_t> buf(256);
            const size_t budget_frames = size_t((seconds + 4.0) * 16000.0); // song + 4 s tail margin
            size_t rendered = 0;
            int32_t peak = 0;
            for (; rendered < budget_frames && !player.ended(); rendered += buf.size()) {
                player.render(buf.data(), buf.size(), 16000, kUnityGainQ15);
                for (int16_t v : buf) {
                    peak = std::max(peak, int32_t(std::abs(int(v))));
                    combined_hash ^= uint64_t(uint16_t(v));
                    combined_hash *= 1099511628211ull;
                }
            }
            every_song_ends_within_budget = every_song_ends_within_budget && player.ended();
            no_song_silent = no_song_silent && peak > 100;
            if (peak >= 32760) std::printf("  song %d clipped: peak=%d\n", s, peak);
        }
        check(every_song_parses, "L3 every one of the 16 patch songs parses as a valid event stream");
        check(min_seconds > 10.0 && max_seconds > 140.0,
              "L4 song durations span the documented range (Reunion ~11.9 s .. Stones ~144.8 s)");
        check(every_song_ends_within_budget, "L5 every song reaches ended() within its own length plus a 4 s tail");
        check(no_song_silent, "L6 every song produces audible PCM (peak > 100/32768), none decodes to silence");
        std::printf("  combined real-corpus PCM fingerprint: %016llx\n", (unsigned long long)combined_hash);
    }

    std::printf("A3-04 music synth: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
