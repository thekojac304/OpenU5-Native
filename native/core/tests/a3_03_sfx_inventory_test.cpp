// Alpha 3 A3-03 -- the remaining PC-speaker effects in the portable core:
// the SFX inventory (every cue id and every original call site classified),
// the A3-03 programs against the binary's own constants, the rumble and the
// ladder loops measured from rendered PCM, the ambient scan / ticker, and the
// ambient priority policy. Pure: no runtime, no clock.
//
//   a3_03_sfx_inventory <native/core dir>
//
//   I  inventory / completeness: statuses, the census bijection, routes
//   D  presentation-time derivations: keys, negatives, source literals
//   P  programs from the binary (re/tools/a3_03_cue_sites.py)
//   R  the rumble (screen_shake_rumble 0x3072) and the ladders, measured
//   A  ambient_sfx_tick 0x4102: classes, nearest, phase, chimes
//   Q  policy: ambient never queues nor preempts; repeats that are real calls play
#include "openu5/ambient_sfx.h"
#include "openu5/audio.h"
#include "openu5/presentation.h"
#include "openu5/scene_timing.h"
#include "openu5/sfx_inventory.h"
#include "openu5/sfx_synth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
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
std::string slurp(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}
std::vector<int32_t> voice_pcm(const SpeakerProgram &p, uint16_t *rumble = nullptr) {
    uint16_t noise = kNoiseSeed;
    SpeakerVoice v;
    v.start(p, &noise, rumble);
    std::vector<int32_t> out;
    int32_t buf[256];
    for (size_t got; (got = v.render(buf, 256)) > 0;) out.insert(out.end(), buf, buf + got);
    return out;
}
std::vector<int32_t> cue_pcm(SfxId id, int32_t param = 0) {
    SpeakerProgram p;
    compile_sfx(id, param, p);
    return voice_pcm(p);
}
double pitch_hz(const std::vector<int32_t> &s, size_t from, size_t to) {
    size_t n = 0, first = 0, last = 0;
    for (size_t i = from + 1; i < to && i < s.size(); ++i)
        if (s[i - 1] <= 0 && s[i] > 0) {
            if (!n) first = i;
            last = i;
            ++n;
        }
    return n > 1 ? double(n - 1) * kSfxOutputRateHz / double(last - first) : 0.0;
}
bool near(double a, double b, double rel) { return std::fabs(a - b) <= std::fabs(b) * rel; }
double sweep_hz(uint16_t inc) { return double(inc) * kSpeakerSweepRate / 65536.0; }
uint64_t samples(const SpeakerProgram &p) { return p.half_samples() / 2; }
SfxRequest req(SfxId id, int32_t param = 0) {
    SfxRequest r;
    r.id = id;
    r.param = param;
    r.gain_q15 = kUnityGainQ15;
    return r;
}
std::vector<int16_t> player_pcm(SfxPlayer &p, size_t frames) {
    std::vector<int16_t> out(frames);
    p.render(out.data(), frames, kUnityGainQ15);
    return out;
}
size_t nonzero(const std::vector<int16_t> &s) {
    return size_t(std::count_if(s.begin(), s.end(), [](int16_t v) { return v != 0; }));
}
/** Every source file of native/core/src + include, comments included (literals are what matter). */
std::string core_sources(const std::string &core_dir, std::initializer_list<const char *> skip) {
    std::string all;
    const std::filesystem::path root(core_dir);
    for (const auto &dir : {root / "src", root / "include" / "openu5"})
        for (const auto &f : std::filesystem::directory_iterator(dir)) {
            const auto ext = f.path().extension().string();
            if (ext != ".cpp" && ext != ".h" && ext != ".inc") continue;
            bool skipped = false;
            for (const char *s : skip) skipped = skipped || f.path().filename().string() == s;
            if (!skipped) all += slurp(f.path().string());
        }
    return all;
}
/** A C string literal as it appears in source: newlines escaped. */
std::string as_literal(const std::string &text) {
    std::string out;
    for (char ch : text) out += ch == '\n' ? std::string("\\n") : std::string(1, ch);
    return out;
}

// DATA.OVL tables restated from the bytes (fileoff = DS + 0x10), so a wrong
// table in production cannot also be the oracle.
constexpr uint16_t kOrdainedInc[7] = {0x0ce4, 0x0f55, 0x0f55, 0x0f55, 0x0f55, 0x0e74, 0x0f55};
constexpr uint16_t kOrdainedCount[7] = {0x1b58, 0x1770, 0x0bb8, 0x0bb8, 0x0bb8, 0x0bb8, 0x1f40};
constexpr uint16_t kSlumberInc[6] = {0x1130, 0x101d, 0x0e53, 0x0b75, 0x0ce4, 0x0ce4};
constexpr uint16_t kSlumberCount[6] = {0xc350, 0xc350, 0xc350, 0x7530, 0x9c40, 0x9c40};
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    const std::string core_dir = argv[1];

    // ======================================================================
    // I -- INVENTORY / COMPLETENESS
    // ======================================================================
    size_t site_count = 0;
    const SfxSite *sites = sfx_sites(site_count);
    {
        bool statuses = true, notes = true;
        std::map<SfxStatus, int> by_status;
        for (size_t i = 1; i < kSfxIdCount; ++i) {
            const auto id = SfxId(i);
            const auto st = sfx_status(id);
            ++by_status[st];
            // Implemented <=> the synthesizer has a program: a supported id is
            // never left unclassified and an unsupported id is never "Implemented".
            statuses = statuses && ((st == SfxStatus::Implemented) == sfx_supported(id));
            notes = notes && sfx_status_note(id) && std::strlen(sfx_status_note(id)) > 8 &&
                    (st == SfxStatus::Implemented || std::strcmp(sfx_status_note(id), "no program") != 0);
        }
        for (const auto &kv : by_status) std::printf("  %s: %d\n", sfx_status_name(kv.first), kv.second);
        check(statuses, "I1 every SfxId is Implemented exactly when compile_sfx has its program (none silently accepted)");
        check(notes, "I1 every id that does not sound says why (a reason, never 'no program')");
        // A4-END1: endgame-orb left the set -- the ending's sequencer plays it.
        const std::set<SfxId> declined = {SfxId::CastSpell, SfxId::SpellCast, SfxId::PotionUsed, SfxId::ScrollUsed,
                                          SfxId::LineSpray, SfxId::CombatReject, SfxId::InvalidMagic,
                                          SfxId::TitleFizzle, SfxId::TitleCrackle, SfxId::BardSong};
        bool exact = true;
        for (size_t i = 1; i < kSfxIdCount; ++i)
            exact = exact && ((sfx_status(SfxId(i)) != SfxStatus::Implemented) == (declined.count(SfxId(i)) == 1));
        check(exact, "I1 the silent set is exactly the ten classified ids (A3-03 remaining list, less A4-END1's endgame-orb)");
        check(sfx_status(SfxId::BardSong) == SfxStatus::DeferredPresentation &&
                  sfx_status(SfxId::EndgameOrb) == SfxStatus::Implemented &&
                  sfx_status(SfxId::TitleFizzle) == SfxStatus::DeferredPresentation &&
                  sfx_status(SfxId::LineSpray) == SfxStatus::EvidenceUnknown &&
                  sfx_status(SfxId::CastSpell) == SfxStatus::IntentionallySilent,
              "I1 the remaining ids carry their adjudicated status (presentation / evidence / marker)");
    }
    {
        // The census bijection: every call the A3-01 census found is a row, and
        // every census row is a call the census found.
        const std::string log = slurp(core_dir + "/a3-01-sound-census.log");
        std::set<std::string> census, table;
        static const std::regex row(R"re(^\s+(\S+\.(?:EXE|OVL))\s+([0-9a-f]{4}) (\S+)\s)re");
        std::istringstream in(log);
        for (std::string line; std::getline(in, line);) {
            std::smatch m;
            if (std::regex_search(line, m, row)) census.insert(m[1].str() + ":" + m[2].str() + ":" + m[3].str());
        }
        size_t hand = 0;
        for (size_t i = 0; i < site_count; ++i) {
            char key[64];
            std::snprintf(key, sizeof(key), "%s:%04x:%s", sites[i].module, unsigned(sites[i].offset), sites[i].primitive);
            if (sites[i].census) table.insert(key);
            else ++hand;
        }
        std::vector<std::string> missing, extra;
        for (const auto &c : census)
            if (!table.count(c)) missing.push_back(c);
        for (const auto &t : table)
            if (!census.count(t)) extra.push_back(t);
        for (const auto &m : missing) std::printf("  unclassified census site: %s\n", m.c_str());
        for (const auto &e : extra) std::printf("  table row not in the census: %s\n", e.c_str());
        std::printf("  census %zu sites, table %zu census rows + %zu found by hand\n", census.size(), table.size(), hand);
        check(census.size() == 121 && missing.empty() && extra.empty(),
              "I2 all 121 census call sites are classified, and no row invents a census site");
        check(hand == 5, "I2 the five calls the linear census misses are classified too (0x429c, 0x42c4, SJOG 0x1d32, EGA.DRV x2)");
    }
    {
        bool routed = true, covered = true;
        std::set<SfxId> implemented_sites;
        for (size_t i = 0; i < site_count; ++i) {
            const auto &s = sites[i];
            if (s.cue == SfxId::None) {
                // a primitive's own body, or an owner nobody has mapped yet
                routed = routed && (s.status == SfxStatus::Implemented ? std::strstr(s.what, "own") != nullptr
                                                                        : s.status != SfxStatus::DeferredMusic);
                continue;
            }
            if (s.status == SfxStatus::Implemented) {
                routed = routed && sfx_supported(s.cue) && (s.also == SfxId::None || sfx_supported(s.also));
                implemented_sites.insert(s.cue);
                if (s.also != SfxId::None) implemented_sites.insert(s.also);
            }
        }
        for (size_t i = 1; i < kSfxIdCount; ++i) {
            const auto id = SfxId(i);
            if (sfx_origin(id) != SfxOrigin::Original || sfx_status(id) != SfxStatus::Implemented) continue;
            if (!implemented_sites.count(id)) {
                std::printf("  implemented cue without a site: %s\n", sfx_cue(id));
                covered = false;
            }
        }
        check(routed, "I3 every Implemented site names a cue the synthesizer plays (or is a primitive's own body)");
        check(covered, "I3 every Implemented original cue is backed by at least one classified call site");
        size_t silent_sites = 0;
        for (size_t i = 0; i < site_count; ++i) silent_sites += sites[i].status != SfxStatus::Implemented;
        std::printf("  %zu of %zu sites not implemented (EvidenceUnknown / NoNativeEvent / DeferredPresentation)\n",
                    silent_sites, site_count);
    }
    {
        // I4 every cue the core emits is routed or explicitly a marker.
        const std::string src = core_sources(core_dir, {"audio.cpp", "audio.h", "sfx_synth.cpp", "sfx_synth.h",
                                                        "sfx_inventory.cpp", "sfx_inventory.h"});
        std::set<std::string> dropped;
        for (size_t i = 1; i < kSfxIdCount; ++i) {
            const auto id = SfxId(i);
            if (!sfx_cue(id)) continue;
            const std::string lit = std::string("\"") + sfx_cue(id) + "\"";
            if (src.find(lit) == std::string::npos) continue; // not emitted by the core
            const auto st = sfx_status(id);
            const bool marker = id == SfxId::SpellCast || id == SfxId::PotionUsed || id == SfxId::ScrollUsed ||
                                id == SfxId::InvalidMagic;
            if (st != SfxStatus::Implemented && !marker) dropped.insert(sfx_cue(id));
        }
        for (const auto &d : dropped) std::printf("  emitted but silent: %s\n", d.c_str());
        check(dropped.empty(), "I4 no cue the core emits is silently dropped (only the four magic markers are silent)");
        // The Camp keeps the 1988 sound-OFF lute branch (Batch 51 13.5): nothing
        // emits the bard's song, whatever SFX Volume says -- the volume is a gain,
        // never [0xa9ce], so no gameplay branch can depend on it.
        const std::string gameplay = core_sources(core_dir, {"audio.cpp", "audio.h", "frontend.cpp", "frontend.h",
                                                            "frontend_settings.cpp", "frontend_settings.h",
                                                            "system_menu.cpp", "system_menu.h"});
        check(src.find("\"bard-song\"") == std::string::npos && gameplay.find("sound_volume") == std::string::npos,
              "I5 the bard's lute is not emitted and no core rule reads the SFX Volume (gain only, not the sound flag)");
    }

    // ======================================================================
    // D -- DERIVATIONS
    // ======================================================================
    {
        struct Key { const char *text; SfxId id; bool combat; bool suffix; };
        const Key keys[] = {
            {"VICTORY!", SfxId::VictoryFanfare, true, false},
            {"Escape!", SfxId::CombatEscape, true, false},
            {"Blocked!", SfxId::MoveBlocked, true, false},
            {"All must use the same exit!", SfxId::MoveBlocked, true, false},
            {" is absorbed!", SfxId::CombatAbsorbed, true, true},
            {" passes out!", SfxId::CombatCharm, true, true},
            {" possessed!", SfxId::CombatCharm, true, true},
            {" gates in a daemon!", SfxId::CombatSummon, true, true},
            {" grazed!", SfxId::CombatGrazed, true, true},
            {" dragged under!", SfxId::CombatDraggedUnder, true, true},
            {"ARGH!", SfxId::CombatEngulfed, true, false},
            {" regurgitated!", SfxId::CombatRegurgitated, true, true},
            {" stole some food!", SfxId::CombatFoodStolen, true, true},
            {"Absorbed!\n", SfxId::SpellZap, true, false},
            {"\nThou dost find\nPlague!", SfxId::SearchFail, true, false},
            {"COLLISION!", SfxId::ShipCollision, false, false},
            {"DROWNING!!!", SfxId::ShipSinking, false, false},
            {"\nWHIRLPOOL!\n", SfxId::ShipSinking, false, false},
            {"\nSomething was stolen!\n", SfxId::TheftDetected, false, false},
            {"\nPoof!\n", SfxId::WishGranted, false, false},
            {"Absorbed!", SfxId::SpellZap, false, false},
            {"The Sceptre is reclaimed!\n", SfxId::SceptreReclaimed, false, false},
            {"\n\nNo effect!\n", SfxId::ShardNoEffect, false, false},
        };
        const std::string src = core_sources(core_dir, {"sfx_inventory.cpp", "sfx_inventory.h"});
        bool mapped = true, literal = true;
        for (const auto &k : keys) {
            const std::string text = k.suffix ? std::string("Someone") + k.text : std::string(k.text);
            const auto got = k.combat ? sfx_for_combat_text(text.c_str(), false) : sfx_for_world_text(text.c_str());
            if (got != k.id) {
                std::printf("  %s -> %s, want %s\n", as_literal(text).c_str(), sfx_cue(got) ? sfx_cue(got) : "none", sfx_cue(k.id));
                mapped = false;
            }
            // the key must still be what the core prints (a text drift breaks it here)
            const std::string lit = as_literal(k.text);
            if (src.find(k.suffix ? lit : "\"" + lit + "\"") == std::string::npos &&
                !(k.suffix && src.find(lit.substr(1)) != std::string::npos)) {
                std::printf("  derivation key not printed by the core: %s\n", lit.c_str());
                literal = false;
            }
        }
        check(mapped, "D1 each adjacent message maps to the speaker call that follows it in the binary");
        check(literal, "D1 every derivation key is a message the native core really prints (drift guard)");
        const bool negatives =
            sfx_for_combat_text("VICTORY!", true) == SfxId::None &&          // the Ended line: never
            sfx_for_combat_text("BATTLE IS LOST!", false) == SfxId::None &&  // 0x0cda: no fanfare
            sfx_for_combat_text("BATTLE IS LOST!", true) == SfxId::None &&
            sfx_for_combat_text("Leave!", false) == SfxId::None &&
            sfx_for_combat_text("Escape-Not yet!", false) == SfxId::None &&
            sfx_for_combat_text("Escape-Not here!", false) == SfxId::None &&
            sfx_for_combat_text("Trolls escapes!", false) == SfxId::None &&
            sfx_for_combat_text(nullptr, false) == SfxId::None &&
            sfx_for_world_text("Poof!") == SfxId::None &&           // the invisibility potion
            sfx_for_world_text("No effect!") == SfxId::None &&      // every other No effect
            sfx_for_world_text("\nNo effect!\n") == SfxId::None &&
            sfx_for_world_text("Docked!") == SfxId::None &&         // MAINOUT 0x02e5: no burst
            sfx_for_world_text("BREAKING UP!") == SfxId::None &&
            sfx_for_world_text("Key broke!\n") == SfxId::None &&    // door / dungeon chest jimmy: mute
            sfx_for_world_text(nullptr) == SfxId::None;
        check(negatives, "D2 no sound where the binary has none: Ended VICTORY!, a lost battle, Leave!, refusals, Docked!, door jimmy");
        // The "\n\nNo effect!\n" key is the shard's alone.
        size_t count = 0;
        for (size_t at = src.find("\"\\n\\nNo effect!\\n\""); at != std::string::npos;
             at = src.find("\"\\n\\nNo effect!\\n\"", at + 1))
            ++count;
        check(count == 1, "D2 the shard's '\\n\\nNo effect!\\n' is printed by exactly one core site");
    }

    // ======================================================================
    // P -- PROGRAMS FROM THE BINARY
    // ======================================================================
    {
        SpeakerProgram p;
        compile_sfx(SfxId::VictoryFanfare, 0, p);
        bool fanfare = p.count == 4;
        for (int i = 0; i < 3 && fanfare; ++i)
            fanfare = p.segments[i].kind == SpeakerPrimitive::Sweep && p.segments[i].value == 0x11f8 &&
                      p.segments[i].iterations == 0x2a30 && p.segments[i].start == 0x12c && p.segments[i].delta == 6;
        fanfare = fanfare && p.segments[3].value == 0x17d4 && p.segments[3].iterations == 0x5460 && p.segments[3].delta == 3;
        const auto pcm = voice_pcm(p);
        const size_t note = size_t(uint64_t(0x2a30) * kSfxOutputRateHz / kSpeakerSweepRate);
        // Mid-note windows: at both ends of a note the duty is within a percent
        // of 0 or 1 and the pulses are narrower than one output sample.
        const size_t last_len = pcm.size() - 3 * note;
        const double first = pitch_hz(pcm, note / 3, 2 * note / 3),
                     last = pitch_hz(pcm, 3 * note + last_len / 3, 3 * note + 2 * last_len / 3);
        std::printf("  fanfare %.0f Hz x3 then %.0f Hz, %.3f s\n", first, last, double(samples(p)) / kSpeakerSweepRate);
        check(fanfare && samples(p) == 54000 && near(first, sweep_hz(0x11f8), 0.02) && near(last, sweep_hz(0x17d4), 0.02),
              "P1 victory fanfare 0x4368: 3 x TS(4600,1,10800,300,6) + TS(6100,1,21600,300,3) -- two pitches, 2.09 s");
    }
    {
        SpeakerProgram shard, siren, donation, welldone, ordained;
        compile_sfx(SfxId::ShardSweep, 0, shard);
        compile_sfx(SfxId::ShrineDonation, 0, donation);
        compile_sfx(SfxId::ShrineWellDone, 0, welldone);
        compile_sfx(SfxId::ShrineOrdained, 0, ordained);
        auto ladder = [](const SpeakerProgram &p, uint16_t inc, uint32_t count) {
            return p.count == 2 && p.segments[0].calls == 460 && p.segments[1].calls == 460 && p.segments[0].value == inc &&
                   p.segments[0].start == 0x7d0 && p.segments[0].call_stride == 0x32 && p.segments[1].start == 0x61a8 &&
                   p.segments[1].call_stride == -0x32 && p.segments[0].delta == 0 && p.segments[1].delta == 0 &&
                   p.segments[0].iterations == 460u * count && p.segments[1].iterations == 460u * count;
        };
        check(ladder(shard, 0xa50, 0xc8) && samples(shard) == kBlackthornSirenSamples,
              "P2 shard ritual CAST 0x15dd-0x162a: 2 x 460 calls TS(0xa50,1,0xc8,si,0), 7.13 s (= the Blackthorn siren hold)");
        check(ladder(donation, 0xa8c, 0xc8) && ladder(welldone, 0xc1c, 0x96) && samples(welldone) == 2u * 460u * 0x96,
              "P3 shrine ALAKAZAM (0xa8c, 0xc8) and WELL DONE (0xc1c, 0x96): the same two 460-call loops");
        bool melody = ordained.count == 7;
        uint64_t total = 0;
        for (int i = 0; i < 7 && melody; ++i) {
            melody = ordained.segments[i].value == kOrdainedInc[i] && ordained.segments[i].iterations == kOrdainedCount[i];
            total += kOrdainedCount[i];
        }
        check(melody && samples(ordained) == total, "P3 ORDAINED: seven table notes (DS 0x4be6/0x4bf4/0x4c02/0x4c10)");
        (void)siren;
    }
    {
        struct One { SfxId id; uint16_t inc; uint32_t count; uint16_t start; int16_t step; const char *what; };
        const One ones[] = {
            {SfxId::Moongate, 0x170c, 0x7530, 0x7d0, 2, "moongate 0x48e5"},
            {SfxId::Sceptre, 0x1450, 0xc350, 0x1388, 1, "sceptre CAST 0x198f"},
            {SfxId::SceptreReclaimed, 0xfd2, 0xfde8, 1, 1, "sceptre reclaimed 0x6221"},
            {SfxId::ShadowlordAnnounce, 0x19c8, 0xea60, 0x7d0, 1, "shadowlord TOWN 0x11e9"},
            {SfxId::SpellZap, 0x2648, 0x6d60, 0x3e8, 2, "absorbed zap"},
            {SfxId::CombatCharm, 0xc1c, 0x7530, 0x3e8, 2, "passes out / possessed"},
            {SfxId::CombatSummon, 0xac8, 0x1388, 0x3e8, 0xf, "gates in a daemon"},
            {SfxId::AmbientClockChime, 0xc2c, 0x7d0, 0x4e20, -10, "clock chime 0x428b"},
        };
        bool all = true;
        for (const auto &o : ones) {
            SpeakerProgram p;
            compile_sfx(o.id, 0, p);
            const bool ok = p.count == 1 && p.segments[0].kind == SpeakerPrimitive::Sweep && p.segments[0].value == o.inc &&
                            p.segments[0].iterations == o.count && p.segments[0].start == o.start && p.segments[0].delta == o.step;
            if (!ok) std::printf("  wrong program: %s\n", o.what);
            all = all && ok;
        }
        SpeakerProgram s3;
        compile_sfx(SfxId::Sceptre, 3, s3);
        all = all && s3.count == 4 && s3.segments[3].kind == SpeakerPrimitive::Noise && s3.segments[3].value == 0x7d0;
        check(all, "P4 single-sweep cues carry the binary's five pushes; the sceptre adds one NB(10,3000,2000) per dissolved field");
        SpeakerProgram shop;
        compile_sfx(SfxId::ShopTransaction, 0, shop);
        check(shop.count == 6 && shop.segments[0].value == 0x100e && shop.segments[1].start == 0x6b6c &&
                  shop.segments[3].start == 0x9c40 && shop.segments[5].start == 0x8ca0 && shop.segments[5].delta == -2,
              "P4 the healer jingle SHOPPES 0x13b0: six sweeps, three mirrored pairs");
    }
    {
        struct G { SfxId id; SpeakerPrimitive kind; uint16_t value; uint32_t iters; uint32_t half; const char *what; };
        const G gs[] = {
            {SfxId::CombatEscape, SpeakerPrimitive::Glide, 0x4b0, 40, 48, "escape glide"},
            {SfxId::CombatAbsorbed, SpeakerPrimitive::Glide, 0x4b0, 40, 48, "absorbed glide"},
            {SfxId::CombatGrazed, SpeakerPrimitive::Glide, 0x4b0, 40, 48, "grazed glide"},
            {SfxId::CombatDraggedUnder, SpeakerPrimitive::Glide, 0x4b0, 40, 48, "dragged glide"},
            {SfxId::CombatFoodStolen, SpeakerPrimitive::Glide, 0x320, 50, 48, "food stolen glide"},
            {SfxId::TheftDetected, SpeakerPrimitive::Glide, 0x320, 50, 48, "theft glide"},
            {SfxId::ShardNoEffect, SpeakerPrimitive::Glide, 0x320, 50, 48, "shard no effect glide"},
            {SfxId::ShipSinking, SpeakerPrimitive::Glide, 0x294, 195, 40 * 48, "sinking glide 660->150"},
            {SfxId::ShipCollision, SpeakerPrimitive::Noise, 0x12c, 20, 100 * 3, "collision NB(100,2000,300)"},
            {SfxId::CombatEngulfed, SpeakerPrimitive::Noise, 0x1f4, 75, 40 * 3, "ARGH NB(40,3000,500)"},
            {SfxId::CombatRegurgitated, SpeakerPrimitive::Noise, 0x258, 7000, 3, "regurgitated NB(1,7000,600)"},
            {SfxId::WishGranted, SpeakerPrimitive::Noise, 0x7d0, 300, 30, "wish NB(10,3000,2000)"},
            {SfxId::SearchFail, SpeakerPrimitive::Noise, 0x1f4, 75, 120, "plague NB(40,3000,500)"},
            {SfxId::AmbientFountain, SpeakerPrimitive::Noise, 0x61a8, 3, 30, "fountain NB(10,30,25000)"},
            {SfxId::AmbientWaterfall, SpeakerPrimitive::Noise, 0x2710, 3, 60, "waterfall NB(20,60,10000)"},
            {SfxId::AmbientClockTick, SpeakerPrimitive::Tone, 0xbb8, 1, 3 * 48, "tick beep(3000,3)"},
            {SfxId::AmbientClockTock, SpeakerPrimitive::Tone, 0x7d0, 1, 3 * 48, "tock beep(2000,3)"},
            {SfxId::IntroThunder, SpeakerPrimitive::Noise, 0x2710, 3, 60, "intro thunder NB(20,60,10000)"},
            {SfxId::IntroSummon, SpeakerPrimitive::Noise, 0xfa0, 1200, 3, "intro summon NB(1,1200,4000)"},
        };
        bool all = true;
        for (const auto &g : gs) {
            SpeakerProgram p;
            compile_sfx(g.id, 0, p);
            const bool ok = p.count == 1 && p.segments[0].kind == g.kind && p.segments[0].value == g.value &&
                            p.segments[0].iterations == g.iters && p.segments[0].iteration_half_samples == g.half;
            if (!ok) std::printf("  wrong program: %s (iters %u half %u)\n", g.what, p.segments[0].iterations,
                                 p.segments[0].iteration_half_samples);
            all = all && ok;
        }
        SpeakerProgram chime4, chime0;
        compile_sfx(SfxId::IntroChime, 4, chime4);
        compile_sfx(SfxId::IntroChime, 0, chime0);
        all = all && chime0.segments[0].value == 0xbb8 && chime4.segments[0].value == 0x7d0;
        check(all, "P5 glides / bursts / beeps: the pushes of every message-adjacent, ambient and intro site");
        const auto fountain = cue_pcm(SfxId::AmbientFountain), waterfall = cue_pcm(SfxId::AmbientWaterfall);
        std::printf("  fountain %zu frames (%.2f ms), waterfall %zu frames\n", fountain.size(), fountain.size() / 16.0,
                    waterfall.size());
        check(!fountain.empty() && fountain.size() < 32 && waterfall.size() < 64,
              "P5 a fountain / waterfall tick is a click of a few ms (45 / 90 samples), never a held tone");
    }
    {
        SpeakerProgram trap;
        compile_sfx(SfxId::TrapdoorFall, 3, trap);
        const auto &ramp = trap.segments[0];
        const bool shape = trap.count == 4 && ramp.kind == SpeakerPrimitive::Glide && ramp.value == 1000 && ramp.delta == -1 &&
                           ramp.iterations == 750 && ramp.iteration_half_samples == 40u * 48u &&
                           trap.segments[1].kind == SpeakerPrimitive::Noise && trap.segments[3].value == 0x1f4;
        SpeakerProgram only;
        only.segments[0] = ramp;
        only.count = 1;
        const auto pcm = voice_pcm(only);
        const size_t step = 40 * 24 * 16000 / kSpeakerSweepRate;
        const double top = pitch_hz(pcm, 40, 20 * step), bottom = pitch_hz(pcm, pcm.size() - 20 * step, pcm.size() - 40);
        std::printf("  trapdoor ramp %.1f s, %.0f Hz -> %.0f Hz\n", pcm.size() / 16000.0, top, bottom);
        check(shape && top > bottom && near(top, 995, 0.03) && near(bottom, 260, 0.05) && pcm.size() / 16000.0 > 27.5,
              "P6 location-29 trapdoor TOWN 0x0fb3: set_tone 1000 -> 251 one per delay(0x28), ~28 s, then a burst per member");
        SpeakerProgram slumber, revival;
        compile_sfx(SfxId::RefugeSlumber, 0, slumber);
        compile_sfx(SfxId::RefugeRevival, 4, revival);
        bool refuge = slumber.count == 6 && revival.count == 4;
        for (int i = 0; i < 6 && refuge; ++i)
            refuge = slumber.segments[i].value == kSlumberInc[i] && slumber.segments[i].iterations == kSlumberCount[i];
        for (int i = 0; i < 4 && refuge; ++i) refuge = revival.segments[i].value == uint16_t(0x8e30 / (i + 7));
        check(refuge && samples(slumber) == 260000,
              "P7 Refuge: the slumber tables DS 0x3720.. (10.1 s) and one revival tone per member, inc = 0x8e30 / (i + 7)");
    }

    // ======================================================================
    // R -- THE RUMBLE AND THE LADDERS, MEASURED
    // ======================================================================
    {
        SpeakerProgram q;
        compile_sfx(SfxId::Quake, 0, q);
        uint16_t word = kRumbleSeed;
        const auto pcm = voice_pcm(q, &word);
        const size_t want = size_t(uint64_t(kQuakeDurationMs) * kSfxOutputRateHz / 1000);
        // Half-cycles: runs of one sign, away from the 4 ms edges.
        std::vector<size_t> runs;
        size_t run = 0;
        for (size_t i = 80; i + 80 < pcm.size(); ++i) {
            if (i > 80 && ((pcm[i] > 0) != (pcm[i - 1] > 0))) {
                runs.push_back(run);
                run = 0;
            }
            ++run;
        }
        size_t lo = SIZE_MAX, hi = 0;
        for (size_t k = 1; k + 1 < runs.size(); ++k) lo = std::min(lo, runs[k]), hi = std::max(hi, runs[k]);
        // 150 Hz -> 53 frames per half-cycle; 19 Hz -> 421 frames.
        std::printf("  rumble %zu frames, %zu half-cycles, %zu..%zu frames each\n", pcm.size(), runs.size(), lo, hi);
        check(q.count == 1 && q.segments[0].kind == SpeakerPrimitive::Rumble && pcm.size() >= want - 1 && pcm.size() <= want,
              "R1 the quake is one rumble exactly as long as the shake the device draws (8 x 117 ms)");
        check(runs.size() > 20 && lo >= 52 && hi <= 422 && hi > 2 * lo,
              "R1 its half-cycles are drawn from rand_range(0x13, 0x96): 19..150 Hz, varied, a low rumble");
        check(word != kRumbleSeed, "R1 the draws advance the rumble's own word (never the game RNG)");
        uint16_t w2 = kRumbleSeed;
        bool bounded = true;
        for (int i = 0; i < 20000; ++i) {
            const auto v = speaker_rumble_draw(w2, kRumbleLowest, kRumbleHighest);
            bounded = bounded && v >= 0x13 && v <= 0x96;
        }
        uint16_t w3 = kRumbleSeed, lo3 = 0xffff, hi3 = 0;
        for (int i = 0; i < 20000; ++i) {
            const auto v = speaker_rumble_draw(w3, kRumbleLowest, kRumbleHighest);
            lo3 = std::min(lo3, v), hi3 = std::max(hi3, v);
        }
        check(bounded && lo3 == 0x13 && hi3 == 0x96, "R2 rand_range 0x2092's arithmetic: the closed interval [0x13, 0x96]");
        SpeakerProgram refuge;
        compile_sfx(SfxId::RefugeThunder, 0, refuge);
        check(refuge.count == 1 && refuge.segments[0].kind == SpeakerPrimitive::Rumble &&
                  refuge.segments[0].iteration_half_samples == q.segments[0].iteration_half_samples,
              "R3 a peal of thunder is one more 0x3072 call: the same rumble (BLCKTHRN 0x0acc / 0x0acf)");
    }
    {
        // The ladder's duty moves call by call: early calls of the rising leg
        // have a wide duty (bx small), the last ones a narrower duty.
        SpeakerProgram shard;
        compile_sfx(SfxId::ShardSweep, 0, shard);
        const auto pcm = voice_pcm(shard);
        const size_t per_call = size_t(uint64_t(0xc8) * kSfxOutputRateHz / kSpeakerSweepRate); // ~124 frames
        auto duty = [&](size_t call) {
            size_t pos = 0, n = 0;
            for (size_t i = call * per_call + 8; i < (call + 1) * per_call - 8 && i < pcm.size(); ++i, ++n) pos += pcm[i] > 0;
            return n ? double(pos) / double(n) : 0.0;
        };
        const double first = duty(4), mid = duty(455), back = duty(464), end = duty(915);
        const double hz = pitch_hz(pcm, per_call * 300, per_call * 455);
        std::printf("  shard ladder positive fraction: call 4 %.2f, 455 %.2f, 464 %.2f, 915 %.2f; %.0f Hz\n", first, mid, back,
                    end, hz);
        check(near(hz, sweep_hz(0xa50), 0.03) && std::fabs(mid - back) < 0.1 && std::fabs(first - end) < 0.1 &&
                  std::fabs(first - mid) > 0.05 && pcm.size() > size_t(7.0 * 16000),
              "R4 the 920 calls hold one pitch (0xa50 = 1040 Hz) while the duty walks out and back: 7.1 s");
    }

    // ======================================================================
    // A -- AMBIENT (ambient_sfx_tick 0x4102)
    // ======================================================================
    {
        check(ambient_tile_class(0xfa) == 1 && ambient_tile_class(0xfb) == 1 && ambient_tile_class(0xd4) == 2 &&
                  ambient_tile_class(0xd7) == 2 && ambient_tile_class(0xd8) == 3 && ambient_tile_class(0xdb) == 3 &&
                  ambient_tile_class(0xfc) == 0 && ambient_tile_class(0xdc) == 0 && ambient_tile_class(0x80) == 0 &&
                  ambient_tile_class(0x1d8) == 0 && ambient_tile_class(-1) == 0,
              "A1 classes: clock 0xfa/0xfb, waterfall 0xd4-0xd7, fountain 0xd8-0xdb; bellows, moongate, sprites silent");
        AmbientWindow w{};
        for (auto &t : w.tiles) t = 0x44;
        check(ambient_nearest_class(w) == 0, "A2 an empty window: no class");
        w.tiles[0] = 0xd8; // the far corner (distance 50 < 0x33) still counts
        check(ambient_nearest_class(w) == 3, "A2 the farthest corner of the 11 x 11 window is in range");
        w.tiles[5 * 11 + 3] = 0xfa; // a clock two cells west of the party wins
        check(ambient_nearest_class(w) == 1, "A2 the NEAREST sounding object wins");
        AmbientWindow tie{};
        for (auto &t : tie.tiles) t = 0x44;
        tie.tiles[4 * 11 + 5] = 0xd4; // north, x = 5
        tie.tiles[5 * 11 + 4] = 0xd8; // west, x = 4: first in the x-outer scan
        check(ambient_nearest_class(tie) == 3, "A2 a tie goes to the first in scan order (x outer, y inner)");
    }
    {
        AmbientTicker t;
        int fountains = 0;
        for (int i = 0; i < 16; ++i) fountains += t.tick(3) == SfxId::AmbientFountain;
        check(fountains == 16 && t.phase() == 0, "A3 a fountain sounds on every tick (no phase gate); the phase wraps at 8");
        AmbientTicker c;
        std::vector<SfxId> seq;
        for (int i = 0; i < 16; ++i) seq.push_back(c.tick(1));
        const bool ticktock = seq[0] == SfxId::AmbientClockTick && seq[4] == SfxId::AmbientClockTock &&
                              seq[8] == SfxId::AmbientClockTick && seq[1] == SfxId::None && seq[7] == SfxId::None;
        check(ticktock, "A3 a clock ticks at phase 0 and tocks at phase 4, silent between");
        AmbientTicker h;
        h.rearm(15); // 3 pm -> three chimes
        std::vector<SfxId> hs;
        for (int i = 0; i < 24; ++i) hs.push_back(h.tick(1));
        const bool chimes = hs[0] == SfxId::AmbientClockChime && hs[4] == SfxId::AmbientClockChime &&
                            hs[8] == SfxId::AmbientClockChime && hs[12] == SfxId::AmbientClockTock &&
                            hs[16] == SfxId::AmbientClockTick && h.chimes() == 0;
        check(chimes, "A4 after the clock moves, the clock strikes the 12-hour hour (3 pm: three), then ticks again");
        AmbientTicker z;
        z.rearm(0);
        check(z.chimes() == 12 && ambient_chime_hour(12) == 12 && ambient_chime_hour(13) == 1,
              "A4 0x5164: midnight strikes 12, 13 h strikes 1");
        AmbientTicker away;
        away.rearm(5);
        for (int i = 0; i < 40; ++i) away.tick(0);
        check(away.chimes() == 0, "A4 the chimes run down even with no clock in view (0x430e is outside the switch)");
        away.reset();
        check(away.phase() == 0 && away.chimes() == 0, "A5 reset (load / title) starts both counters over");
    }

    // ======================================================================
    // Q -- POLICY
    // ======================================================================
    {
        SfxPlayer p;
        check(p.submit(req(SfxId::AmbientFountain)) == SfxAdmit::Started, "Q1 an idle voice plays the ambient tick");
        player_pcm(p, 200);
        p.submit(req(SfxId::ApparitionChord)); // a 2.3 s scene cue
        int skipped = 0;
        for (int i = 0; i < 100; ++i) skipped += p.submit(req(SfxId::AmbientFountain)) == SfxAdmit::Skipped;
        check(skipped == 100 && p.pending() == 0 && p.playing() == SfxId::ApparitionChord,
              "Q1 ambience never queues and never cuts a cue: 100 ticks during a scene chord are skipped");
        SfxPlayer c;
        c.submit(req(SfxId::AmbientClockChime)); // 77 ms
        player_pcm(c, 16);
        const auto a = c.submit(req(SfxId::CombatHit));
        const auto tail = player_pcm(c, SfxPlayer::kReleaseFrames + 2);
        check(a == SfxAdmit::Preempted && c.playing() == SfxId::CombatHit,
              "Q2 a gameplay cue cuts a playing ambient tick at once (2 ms fade) and starts");
        SfxPlayer f;
        size_t sounding = 0;
        for (int tick = 0; tick < 40; ++tick) { // 40 ticks of 55 ms next to a fountain
            f.submit(req(SfxId::AmbientFountain));
            sounding += nonzero(player_pcm(f, 880));
        }
        check(f.idle() && f.stats().started == 40 && sounding > 40 * 20 && sounding < 40 * 40 && f.stats().overflowed == 0,
              "Q3 standing by a fountain for 40 ticks: 40 short bursts, each ends, nothing piles up, no stuck tone");
        SfxPlayer q;
        q.submit(req(SfxId::Quake));
        q.submit(req(SfxId::Quake));
        q.submit(req(SfxId::Quake));
        SfxPlayer s;
        for (int i = 0; i < 3; ++i) s.submit(req(SfxId::ShadowlordAnnounce));
        SfxPlayer m;
        for (int i = 0; i < 3; ++i) m.submit(req(SfxId::CombatDamage));
        check(q.pending() == 2 && s.pending() == 2 && m.pending() == 2 && q.stats().coalesced == 0,
              "Q4 repeats that are real calls play: three shakes, three Stonegate drones, a burst per damaged member");
        SfxPlayer b;
        for (int i = 0; i < 5; ++i) b.submit(req(SfxId::MoveBlocked));
        check(b.pending() == 1 && b.stats().coalesced == 3 && sfx_coalesces(SfxId::MoveStep) && !sfx_coalesces(SfxId::Quake),
              "Q4 only a held key's step / bump coalesces");
        check(sfx_class(SfxId::AmbientFountain) == SfxClass::Ambient && sfx_class(SfxId::VictoryFanfare) == SfxClass::Combat &&
                  sfx_class(SfxId::ShadowlordAnnounce) == SfxClass::Ordinary && sfx_class(SfxId::RefugeThunder) == SfxClass::Scene &&
                  std::strcmp(sfx_class_name(SfxClass::Ambient), "ambient") == 0,
              "Q5 classes: ambient lowest, the fanfare is combat (FIFO), the drone ordinary, Refuge cues follow the pacer");
        (void)tail;
    }

    std::printf("A3-03 sfx inventory: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
