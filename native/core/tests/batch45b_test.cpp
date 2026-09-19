// Batch 4.5B -- Blackthorn capture production-resource integration.
//
// Root cause (native/targets/tdeck/main/alpha_runtime.cpp): AlphaRuntime
// wires ShrineServices::data but never wires ShrineServices::context or
// ::record. On real T-Deck hardware every blackthorn_action(Capture/Answer)
// call and every shrine.cpp Codex/mantra lookup therefore hits
// blackthorn.cpp's `for(int i=0;i<12;++i) if(!record(c,i)) return
// InvalidContext;` gate (or shrine.cpp's equivalent `!record(20+v)` guard)
// before a single narrative event is emitted -- exactly the silent
// capture/interrogation the field report describes. Existing host tests
// never caught this because every prior fixture (see
// batch4_group_a_test.cpp's A7/A8) supplies its own placeholder
// ShrineServices::record; that seam is correct to keep (B5 below), but it
// also means nothing previously modeled the actual T-Deck wiring gap.
//
// This suite reproduces that gap with a ShrineServices built the way
// AlphaRuntime built it (RED: data set, record/context null) and proves the
// same call sequence advances once record is wired the way AlphaRuntime now
// wires it (GREEN): a bounds-checked lookup via
// native/targets/tdeck/main/misc_records.h's misc_text_record(), fed from a
// [count|offsets|text] buffer encoded exactly like
// native/tools/u5pack/alpha1.ts's encodeStringRecords() encodes the real
// extracted MISCMSG.DAT records in game/assets/ds-strings.json.
//
//   B1 -- production wiring resolves every Blackthorn- and shrine-required
//         record index once wired (not just non-empty: real content).
//   B2 -- the exact previously-silent blackthorn_turn_effect(Capture) RED
//         entry point emits the real capture/blindfold/drag/chains/greeting
//         narrative and arms the interrogation once GREEN.
//   B3 -- representative records are checked against the real extracted
//         text, not just non-empty.
//   B4 -- a full multi-round interrogation (wrong answer, then right
//         answer) proves repeated lookups work, not just the first line,
//         and that the party-member-kill consequence still fires.
//   B6 -- badge-worn (time_spell escape) still takes the password branch
//         instead of Capture, unaffected by resource wiring.
//   B7 -- an unavailable resource (record==nullptr) fails safely
//         (InvalidContext, no crash) for both Blackthorn and the shrine
//         Codex path that shares the same resource.
#include "openu5/blackthorn.h"
#include "openu5/commands.h"
#include "openu5/quest.h"
#include "openu5/save_json.h"
#include "openu5/shrine.h"
#include "openu5/world.h"

#include "misc_records.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const std::string &what) {
    ++checks;
    std::fprintf(stderr, "[%s] batch45b check %d: %s\n", ok ? "PASS" : "FAIL", checks, what.c_str());
    if (!ok) ++failures;
}

// Real MISCMSG.DAT records, extracted from the user's own original data
// (extractor/src/parsers/ds-strings.ts) into game/assets/ds-strings.json.
// Loaded from disk rather than hand-copied, so B3's fidelity checks compare
// against the actual extraction, not a second hand-typed guess.
std::vector<std::string> load_miscmsg(const char *ds_strings_path) {
    std::ifstream in(ds_strings_path, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    save::Json doc;
    if (!in || save::parse_json(buf.str(), doc) != save::JsonError::None) return {};
    const auto &misc = doc["MISCMSG.DAT"];
    std::vector<std::string> out;
    for (const auto &row : misc.values) {
        std::string ascii;
        for (char16_t c : row.string) {
            if (c > 127) return {};
            ascii += char(c);
        }
        out.push_back(std::move(ascii));
    }
    return out;
}

void put_u32le(std::vector<uint8_t> &out, size_t at, uint32_t v) {
    out[at] = uint8_t(v);
    out[at + 1] = uint8_t(v >> 8);
    out[at + 2] = uint8_t(v >> 16);
    out[at + 3] = uint8_t(v >> 24);
}
uint32_t get_u32le(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

// Exactly native/tools/u5pack/alpha1.ts's encodeStringRecords(): u32 count,
// then (count+1) little-endian u32 offsets into a blob of NUL-terminated
// records. This is the misc-records.bin/shop-records.bin wire format
// AlphaResourcePack::load() decodes on real hardware.
std::vector<uint8_t> encode_records(const std::vector<std::string> &rows) {
    std::vector<uint32_t> offsets(rows.size() + 1, 0);
    std::string text;
    for (size_t i = 0; i < rows.size(); ++i) {
        offsets[i] = uint32_t(text.size());
        text += rows[i];
        text += '\0';
    }
    offsets[rows.size()] = uint32_t(text.size());
    std::vector<uint8_t> out(4 + offsets.size() * 4 + text.size(), 0);
    put_u32le(out, 0, uint32_t(rows.size()));
    for (size_t i = 0; i < offsets.size(); ++i) put_u32le(out, 4 + i * 4, offsets[i]);
    std::memcpy(out.data() + 4 + offsets.size() * 4, text.data(), text.size());
    return out;
}

// Decodes a packed buffer exactly like AlphaResourcePack::load() decodes
// misc-records.bin/shop-records.bin: fread the count, fread the offset
// table into a properly-typed/aligned array, then hand the remaining bytes
// to the shared validator both this test and production call.
struct DecodedRecords {
    std::vector<uint32_t> offsets;
    std::string text;
    uint32_t count = 0;
    bool valid = false;
};
DecodedRecords decode_records(const std::vector<uint8_t> &buffer) {
    DecodedRecords d;
    if (buffer.size() < 4) return d;
    d.count = get_u32le(buffer.data());
    const size_t dir = 4 + size_t(d.count + 1) * 4;
    if (dir > buffer.size()) return d;
    d.offsets.resize(d.count + 1);
    for (uint32_t i = 0; i <= d.count; ++i) d.offsets[i] = get_u32le(buffer.data() + 4 + i * 4);
    d.text.assign(reinterpret_cast<const char *>(buffer.data() + dir), buffer.size() - dir);
    d.valid = tdeck::validate_misc_text_records(d.offsets.data(), d.text.data(), d.text.size(), d.count);
    return d;
}

// Real 32x32 walkable small map, Palace of Blackthorn (location 18) --
// the same shape native/core/tests/batch4_group_a_test.cpp's GuardWorld
// fixture uses for the real town-turn trigger.
struct World {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    NpcActors actors{};
    BlackthornSession blackthorn{};
    std::vector<uint8_t> large = std::vector<uint8_t>(65536, 5), small = std::vector<uint8_t>(1024, 5);
    MapData local{};
    WorldData world_data{};
    CommandContext ctx;
    std::vector<std::pair<GameEventKind, std::string>> events;

    explicit World() : ctx(game, turn, travel, commands, world_data) {
        local = MapData{{18, 0}, small.data(), small.size()};
        world_data = WorldData{large.data(), large.data(), large.size(), large.size(), &local, 1};
        ctx.actors = &actors;
        ctx.blackthorn = &blackthorn;
        ctx.events = {this, &World::on_event};
        game.position.map = {18, 0};
        game.position.xy = {10, 10};
        game.time.hour = 10;
        game.party.party_size = game.party.character_count = 2;
        std::strcpy(game.party.characters[0].name, "Avatar");
        std::strcpy(game.party.characters[1].name, "Iolo");
        game.party.characters[0].status = game.party.characters[1].status = 'G';
    }
    static void on_event(void *p, const GameEvent &e) {
        auto &w = *static_cast<World *>(p);
        w.events.emplace_back(e.kind, e.text ? e.text : "");
    }
    NpcActor &add_guard() {
        auto &n = actors.actors[actors.count++];
        n.location = 18;
        n.z = 0;
        n.x = 10;
        n.y = 9; // Manhattan distance 1 from (10,10): adjacent() eligible.
        n.schedule.slot = 7;
        n.schedule.type = 112; // Guard type blackthorn_turn_effect requires for Capture.
        n.schedule.dialog = 255;
        for (auto &t : n.schedule.times) t = uint8_t(game.time.hour);
        n.schedule.ai[0] = 6;
        return n;
    }
    bool has(GameEventKind kind, const char *substring) const {
        for (auto &e : events)
            if (e.first == kind && e.second.find(substring) != std::string::npos) return true;
        return false;
    }
};

Rand no_rand() {
    return {nullptr, [](void *, int32_t lo, int32_t) { return lo; }};
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: batch45b_tests <path to game/assets/ds-strings.json>\n");
        return 2;
    }
    const auto miscmsg = load_miscmsg(argv[1]);
    if (miscmsg.size() < 45) {
        std::fprintf(stderr, "could not load MISCMSG.DAT records from %s (got %zu)\n", argv[1], miscmsg.size());
        return 2;
    }

    // Real ShrineData: one shrine (Compassion / mantra "MU"), matching the
    // pick_interrogation_shrine() default (no shrine yet destroyed -> index 0).
    static constexpr char16_t kVirtue[] = u"Compassion", kMantra[] = u"MU";
    ShrineData shrine_data{};
    shrine_data.count = 1;
    shrine_data.virtues[0] = kVirtue;
    shrine_data.mantras[0] = kMantra;

    // ---- B2/B7 RED: the exact pre-fix AlphaRuntime wiring (data set,
    // record/context left null) reproduced against the real Capture entry
    // point (blackthorn_turn_effect, the town-turn hostile-guard trigger).
    {
        World w;
        w.add_guard();
        ShrineSession session{};
        ShrineServices svc{session};
        svc.data = &shrine_data; // AlphaRuntime does wire this today.
        // svc.record / svc.context: intentionally left null -- the bug.
        w.ctx.shrine_services = &svc;

        const bool consumed = blackthorn_turn_effect(w.ctx, CommandEffect::Capture, w.ctx.events, no_rand());
        check(consumed, "B2 RED setup: the adjacent hostile guard still consumes the turn (matches "
                         "hardware: the party IS captured/relocated)");
        check(w.events.empty(),
              "B2 RED: reproduces the hardware defect exactly -- with ShrineServices::record unset "
              "(the real pre-fix AlphaRuntime wiring), blackthorn_action(Capture) hits blackthorn.cpp's "
              "record(c,0..11) InvalidContext gate before any narrative event is emitted, so the "
              "capture is completely silent (no blindfold/drag/chains/greeting/interrogation text)");
        check(w.blackthorn.shrine == -1,
              "B7: the interrogation session must never arm itself when the resource is unavailable "
              "-- no crash, no dangling pointer, no out-of-range read, just a no-op capture");

        // The same resource also serves shrine.cpp's Codex path (READ FIRST
        // item 3): with the identical unwired ShrineServices, the Codex
        // action must fail the same safe way, not crash.
        w.game.quest.shrine_quest = 1; // Shrine 0's mantra has been learned.
        ShrineInput codex;
        codex.action = ShrineAction::Codex;
        const auto codex_result = execute_shrine(w.ctx, codex);
        check(codex_result.status == CommandStatus::InvalidContext,
              "B7: shrine.cpp's Codex lookup shares ShrineServices::record with Blackthorn -- an "
              "unavailable resource must fail safely there too (InvalidContext, no crash)");
    }

    // ---- Production-shaped resource: encode the real MISCMSG.DAT records
    // the way alpha1.ts packs misc-records.bin, then decode them the way
    // AlphaResourcePack::load() decodes it. Both directions, plus the
    // lookup itself, are the exact functions/format production uses.
    const auto packed = encode_records(miscmsg);
    const auto decoded = decode_records(packed);
    check(decoded.valid, "B1: the packed misc-records.bin buffer round-trips through the real "
                          "production validate_misc_text_records() decode");
    tdeck::MiscTextRecords records{decoded.offsets.data(), decoded.text.data(), decoded.count};

    // ---- B1: every Blackthorn-required (0-11) and shrine-required (12-44)
    // index resolves to real, non-empty content once wired.
    {
        bool all_present = true;
        for (int32_t i = 0; i < 45; ++i) all_present = all_present && tdeck::misc_text_record(records, i) != nullptr;
        check(all_present,
              "B1: every MISCMSG index blackthorn.cpp (0-11) and shrine.cpp (12-19 mantras, 20-27 "
              "Codex lessons, 40-44 ceremony) require resolves to a real record once "
              "ShrineServices::record is wired to the production resource-backed lookup");
    }

    // ---- B3: representative record fidelity against the real extraction,
    // not just non-empty. Record 4 is checked against a literal excerpt of
    // the real extracted text (an independent anchor, not a round-trip of
    // the same load); every Blackthorn index is also checked for exact
    // equality against the loaded ds-strings.json content.
    {
        const char *r4 = tdeck::misc_text_record(records, 4);
        check(r4 && std::string(r4) == "With a wave of Blackthorn's hand, the pendulum blade falls!",
              "B3: record 4 matches the real extracted MISCMSG.DAT text exactly (independent literal "
              "anchor, not a hand-modernized paraphrase)");
        const char *r11 = tdeck::misc_text_record(records, 11);
        check(r11 && std::string(r11).find("Avatarhood") != std::string::npos,
              "B3: record 11 (the interrogation greeting) contains the real extracted \"Avatarhood\" "
              "text");
        bool exact = true;
        for (int32_t i = 0; i < 12; ++i) {
            const char *got = tdeck::misc_text_record(records, i);
            exact = exact && got && miscmsg[size_t(i)] == got;
        }
        check(exact, "B3: all 12 Blackthorn-required records resolve to byte-exact copies of the real "
                      "extracted ds-strings.json MISCMSG.DAT text (record ordering/punctuation/line "
                      "breaks preserved, nothing modernized)");
    }

    // ---- B2 GREEN + B4: the same silent RED entry point now plays the
    // full capture and a complete multi-round interrogation.
    {
        World w;
        w.add_guard();
        ShrineSession session{};
        ShrineServices svc{session};
        svc.data = &shrine_data;
        svc.context = &records;
        svc.record = [](void *p, int32_t index) {
            return tdeck::misc_text_record(*static_cast<const tdeck::MiscTextRecords *>(p), index);
        };
        w.ctx.shrine_services = &svc;

        const bool consumed = blackthorn_turn_effect(w.ctx, CommandEffect::Capture, w.ctx.events, no_rand());
        check(consumed, "B2 GREEN setup: the same hostile guard still consumes the turn");
        check(w.has(GameEventKind::Message, "subdued and blindfolded"),
              "B2 GREEN: the blindfold narrative is now emitted");
        check(w.has(GameEventKind::Message, "Strong guards drag thee away"),
              "B2 GREEN: the drag narrative is now emitted");
        check(w.has(GameEventKind::Message, "chained and manacled"),
              "B2 GREEN: the chains narrative is now emitted");
        check(w.has(GameEventKind::Message, "Blackthorn says") && w.has(GameEventKind::Message, "Ah, Avatar"),
              "B2 GREEN: Blackthorn's real greeting (with the real party member name) is now emitted");
        check(w.has(GameEventKind::Message, "Avatarhood"),
              "B2 GREEN: record 11's real interrogation lead-in is now emitted");
        check(w.has(GameEventKind::BlackthornPrompt, "Mystic Shrine of Compassion"),
              "B2 GREEN: the real first interrogation question (record 0 + the real shrine virtue "
              "text) is now emitted, ending the capture in AwaitingResponse instead of silence");
        check(w.blackthorn.shrine == 0 && w.blackthorn.round == 0,
              "B2 GREEN: the interrogation session is now actually armed (shrine=0, round=0), unlike "
              "the RED case above");

        // B4 round 1: wrong mantra. living=2 (>1) so this must advance to
        // round 1 using records 7/8, not resolve the shrine.
        w.events.clear();
        static constexpr char16_t kWrong[] = u"WRONG";
        auto wrong_result = blackthorn_action(w.ctx, BlackthornAction::Answer, kWrong, false, w.ctx.events, no_rand());
        check(wrong_result == CommandStatus::AwaitingResponse && w.blackthorn.round == 1,
              "B4: a wrong first answer (living>1) advances the real state machine to round 1 instead "
              "of resolving -- proves this isn't a single-record fixture");
        check(w.has(GameEventKind::Message, "laughing at me") && w.has(GameEventKind::Message, "sand has fallen"),
              "B4: round-1 escalation narrative (the real records 7 and 8) is emitted from a second, "
              "independent record lookup, not a cached copy of record 0");
        check(w.has(GameEventKind::BlackthornPrompt, "Now tell me, what is the Mantra") &&
                  w.has(GameEventKind::BlackthornPrompt, "Compassion"),
              "B4: the second-round question (record 1's real text this time, not record 0's) still "
              "carries the real shrine virtue text");

        // B4 round 2: correct mantra (case-insensitive, matching quest_text_contains).
        w.events.clear();
        static constexpr char16_t kRight[] = u"mu";
        auto right_result = blackthorn_action(w.ctx, BlackthornAction::Answer, kRight, false, w.ctx.events, no_rand());
        check(right_result == CommandStatus::Success && w.blackthorn.shrine == -1,
              "B4: the correct mantra resolves the interrogation for real (Success, session cleared)");
        check(w.game.quest.shrine_destroyed[0] == 255,
              "B4: the real shrine_destroyed mutation still fires now that the record gate no longer "
              "blocks it");
        check(w.game.party.party_size == 1 && w.game.party.characters[15].party_status == 127,
              "B4: the reference's party-member-kill consequence still fires when living>1, even on "
              "a correct answer -- resource wiring changes only whether the narrative is silent, "
              "never gameplay outcomes (sacrifice() still moves the victim to the urn slot and "
              "shrinks party_size)");
        check(w.has(GameEventKind::Message, "esteem for thine honesty"),
              "B4: the real record-5 \"merciful death\" text is emitted for the successful-but-costly "
              "resolution");
    }

    // ---- B6: badge-worn must still take the password branch, unaffected
    // by resource wiring (no regression to the badge boundary).
    {
        World w;
        w.add_guard();
        ShrineSession session{};
        ShrineServices svc{session};
        svc.data = &shrine_data;
        svc.context = &records;
        svc.record = [](void *p, int32_t index) {
            return tdeck::misc_text_record(*static_cast<const tdeck::MiscTextRecords *>(p), index);
        };
        w.ctx.shrine_services = &svc;
        w.turn.time_spell = '\x1d'; // Worn Black Badge.

        const bool consumed = blackthorn_turn_effect(w.ctx, CommandEffect::Capture, w.ctx.events, no_rand());
        check(consumed, "B6: the worn-badge branch still consumes the turn");
        check(w.blackthorn.password && w.blackthorn.shrine == -1,
              "B6: wearing the Black Badge still takes the GuardPassword branch instead of Capture -- "
              "the interrogation session is never armed, exactly as before this resource-wiring fix");
        check(w.has(GameEventKind::GuardPasswordPrompt, "password"),
              "B6: the real GuardPasswordPrompt is emitted instead of any capture narrative");
        check(!w.has(GameEventKind::Message, "subdued and blindfolded"),
              "B6: no capture narrative leaks through when the badge is worn");
    }

    std::fprintf(stderr, "batch45b: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
