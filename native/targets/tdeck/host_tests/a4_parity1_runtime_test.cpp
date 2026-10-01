// Alpha 4 A4-PARITY1 (targets/tdeck/ALPHA4_UI.md section 9) -- the parity fixes read off
// the REAL AlphaRuntime on the REAL tdeck_board.cpp (the A4-UI2 harness), raw keys only.
//
//   C  the Crown of Lord British (NEW-1, P1b)
//      C1 in Blackthorn's palace (location 18) a party that CARRIES the crown casts: CAST
//         0x0e45 reads g_crown [0x57b4], possession (.gam 0x20E; written only by the Get,
//         SJOG 0x16e6) -- not "worn"
//      C2 control: without the crown the same cast is "Absorbed!"
//      C3 (U)se crown writes the time-spell byte 0x1c, permanent (CAST 0x193e -> 0x1764 ->
//         set_time_spell, CAST2 0x08f8); a second use removes it ("Removed!")
//      C4 the worn crown survives a save and a load (it rides the time spell, .gam
//         0x2D4 / 0x2E8); the invented worn flag never did
//   D  the dungeon's command keys (NEW-2, D-4): DUNGEON.OVL hands every key it does not own
//      to the shared kernel dispatcher (0x07a0 -> 0x3178)
//      D1 M is Mix ("Mix Reagents", the spell list for a mix), not Cast
//      D2 N is New order (the party picker), not "What?"
//      D3 B / E / F / T / X print the binary's own refusal and spend a turn; P refuses with
//         no turn (DATA.OVL DS 0xa13a+0x4252, 0xa156, 0xa164+0x42e4, 0xa22c, 0xa280+0x4368,
//         0xa1d4)
//      D4 Y asks "Yell what?" and answers "No effect!" with no turn (CMDS 0x14ac, DS 0x453a)
//   W  the wishing well (D-50): the kernel stristr (0x6f1e) folds case
//      W1 "horse" in Paws summons the horse ("Poof!"), as "Horse" did
//      W2 "HORSE" too; W3 "HHorse" does not (0x6f1e's skip after a partial match)
//
//   a4_parity1_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin>
#include "a4_ui2_harness.h"
#include "openu5/debug_map_picker.h"
#include "openu5/magic.h"

using namespace a4_ui2;

namespace {
CommandContext &ctx(Run &h) { return h.rt->command_context_for_test(); }
std::vector<std::string> blocks(Run &h) {
    std::vector<std::string> out;
    for (size_t i = 0; i < h.rt->ui()->transcript_size(); ++i)
        if (const auto *b = h.rt->ui()->transcript_at(i)) out.emplace_back(b->text);
    return out;
}
// Every transcript line from `from` on, newlines split, empty lines dropped.
std::vector<std::string> lines_since(Run &h, size_t from) {
    std::vector<std::string> out;
    const auto all = blocks(h);
    for (size_t i = from; i < all.size(); ++i) {
        std::string cur;
        for (char ch : all[i] + "\n") {
            if (ch == '\n') {
                if (!cur.empty()) out.push_back(cur);
                cur.clear();
            } else cur += ch;
        }
    }
    return out;
}
std::string join(const std::vector<std::string> &v) {
    std::string s;
    for (const auto &x : v) s += (s.empty() ? "" : " | ") + x;
    return s;
}
bool contains(const std::vector<std::string> &v, const std::string &x) {
    for (const auto &l : v)
        if (l == x) return true;
    return false;
}
void type(Run &h, const char *text) {
    for (const char *p = text; *p; ++p) h.key(uint8_t(*p));
}
bool teleport(Run &h, DebugDestinationKind kind, int location, int floor = 0) {
    DebugTeleportRequest r{};
    r.kind = kind;
    r.location = location;
    r.floor = floor;
    r.standard_entry = true;
    const bool ok = apply_debug_teleport(ctx(h), r).status == DebugTeleportStatus::Applied;
    h.render(true);
    h.run(50);
    return ok;
}

// ---- C: the crown -------------------------------------------------------------
// Mani (4) is legal in a town (DS 0x1c90 index 4), so outside the palace it simply casts.
std::vector<std::string> cast_mani_in_palace(bool carries_crown, int &mp_spent) {
    Run h({{"Avatar", 'G', 40}});
    auto &g = h.rt->game();
    auto &m = g.party.characters[0];
    m.current_mp = 30;
    m.intelligence = 30;
    m.level = 3;
    std::fill_n(g.spell_quantities, 48, uint8_t(0));
    g.spell_quantities[int(SpellId::Mani)] = 3;
    g.quest.artifacts[1] = carries_crown;
    if (!teleport(h, DebugDestinationKind::SmallMap, 18)) return {"<teleport failed>"};
    const size_t from = blocks(h).size();
    const int mp = m.current_mp;
    h.key('c');
    h.key('\r'); // the only spell in the list: Mani
    h.key('\r'); // on whom: the Avatar
    h.run(3000);
    mp_spent = mp - int(g.party.characters[0].current_mp);
    return lines_since(h, from);
}

void test_crown() {
    std::printf("C  the crown\n");
    {
        int spent = 0;
        const auto with = cast_mani_in_palace(true, spent);
        check(!contains(with, "Absorbed!") && spent > 0,
              "C1 carrying the crown (not worn), Mani casts in Blackthorn's palace (mp spent " + n(spent) +
                  "): " + join(with));
    }
    {
        int spent = 0;
        const auto without = cast_mani_in_palace(false, spent);
        check(contains(without, "Absorbed!") && spent == 0,
              "C2 control: without the crown the same cast is Absorbed! and costs nothing: " + join(without));
    }
    Run h({{"Avatar", 'G', 40}});
    auto &g = h.rt->game();
    g.quest.artifacts[1] = true;
    teleport(h, DebugDestinationKind::Britannia, 0);
    auto use_crown = [&]() -> std::vector<std::string> {
        const size_t from = blocks(h).size();
        h.key('u');
        UiSelectionView v{};
        for (size_t i = 0; i < 40 && h.rt->ui()->selection_view(v); ++i) {
            if (v.current.label && std::strstr(v.current.label, "Crown")) break;
            h.down();
        }
        h.key('\r');
        h.run(200);
        return lines_since(h, from);
    };
    ctx(h).turn.time_spell = 'Q';
    ctx(h).turn.spell_turns = 9;
    const auto on = use_crown();
    const bool worn = ctx(h).turn.time_spell == '\x1c' && ctx(h).turn.spell_turns == 255;
    check(contains(on, "Thou dost don the Crown of Lord British...") && worn,
          "C3a Use crown: the don line, time spell 0x1c permanent (was Quickness): " + join(on) + " spell=" +
              n(int(uint8_t(ctx(h).turn.time_spell))) + " turns=" + n(ctx(h).turn.spell_turns));
    // C4: save, change the byte, load: the worn crown comes back from the save.
    h.key('s', true);
    h.run(200);
    ctx(h).turn.time_spell = 0;
    ctx(h).turn.spell_turns = 0;
    h.key('l', true);
    h.run(500);
    check(ctx(h).turn.time_spell == '\x1c' && ctx(h).turn.spell_turns == 255,
          "C4 the worn crown survives Alt+S / Alt+L (spell=" + n(int(uint8_t(ctx(h).turn.time_spell))) + ")");
    const auto off = use_crown();
    check(contains(off, "Removed!") && ctx(h).turn.time_spell == 0,
          "C3b a second Use removes it and clears the time spell: " + join(off));
}

// ---- D: the dungeon keys -------------------------------------------------------
struct KeyResult {
    std::vector<std::string> lines;
    int64_t turns = 0;
    UiMode mode = UiMode::Dungeon;
    UiRequestId request = UiRequestId::None;
};
KeyResult dungeon_key(const char *keys) {
    KeyResult r;
    Run h({{"Avatar", 'G', 200}});
    auto &g = h.rt->game();
    std::fill_n(g.reagent_quantities, 8, uint8_t(5));
    if (!teleport(h, DebugDestinationKind::Dungeon, 33) || !ctx(h).dungeon) {
        r.lines = {"<not in a dungeon>"};
        return r;
    }
    const size_t from = blocks(h).size();
    const auto turns = g.turns_since_start;
    for (const char *p = keys; *p; ++p) h.key(uint8_t(*p));
    h.run(100);
    r.lines = lines_since(h, from);
    r.turns = int64_t(g.turns_since_start) - int64_t(turns);
    r.mode = h.rt->ui()->mode();
    r.request = h.rt->ui()->request();
    return r;
}

void test_dungeon() {
    std::printf("D  the dungeon keys\n");
    {
        const auto r = dungeon_key("m");
        check(contains(r.lines, "Mix Reagents") && !contains(r.lines, "Cast...") && r.request == UiRequestId::Custom,
              "D1 M underground is Mix (CMDS 0x1AD8): " + join(r.lines));
    }
    {
        const auto r = dungeon_key("n");
        check(!contains(r.lines, "What?") && r.mode == UiMode::PartySelection && r.request == UiRequestId::Party,
              "D2 N underground opens New order's party picker (CMDS 0x0DDC): " + join(r.lines));
    }
    struct Refusal {
        const char *key;
        std::vector<std::string> lines;
        bool turn;
    } refusals[] = {
        {"b", {"Board", "Not here!"}, true},       {"e", {"Enter what?"}, true},
        {"f", {"Fire-What?"}, true},               {"t", {"Talk-Funny, no response!"}, true},
        {"x", {"X-it what?"}, true},               {"p", {"Push", "Not here!"}, false},
    };
    for (const auto &f : refusals) {
        const auto r = dungeon_key(f.key);
        bool lines = r.lines.size() >= f.lines.size();
        for (size_t i = 0; lines && i < f.lines.size(); ++i) lines = r.lines[i] == f.lines[i];
        check(lines && (r.turns > 0) == f.turn && !contains(r.lines, "What?"),
              std::string("D3 ") + f.key + " refuses with the binary's string" + (f.turn ? " and a turn" : ", no turn") +
                  " (turns +" + n(r.turns) + "): " + join(r.lines));
    }
    {
        const auto r = dungeon_key("yabc\r");
        check(contains(r.lines, "Yell") && contains(r.lines, "No effect!") && r.turns == 0 && !contains(r.lines, "What?"),
              "D4 Y asks for a word and answers No effect!, no turn (CMDS 0x14ac): " + join(r.lines) + " turns +" +
                  n(r.turns));
    }
}

// ---- W: the wishing well -------------------------------------------------------
std::vector<std::string> wish(const char *word, int &gold_spent) {
    Run h({{"Avatar", 'G', 40}});
    auto &g = h.rt->game();
    if (!teleport(h, DebugDestinationKind::SmallMap, 22)) return {"<teleport failed>"}; // Paws
    const int gold = g.gold;
    GameEvent e{};
    e.kind = GameEventKind::WellWishPrompt; // LOOKOBJ's "Thy wish?" after the coin's Yes
    ctx(h).events.emit(ctx(h).events.context, e);
    h.render(true);
    const size_t from = blocks(h).size();
    type(h, word);
    h.key('\r');
    h.run(500);
    gold_spent = gold - int(g.gold);
    return lines_since(h, from);
}

void test_well() {
    std::printf("W  the wishing well\n");
    int spent = 0;
    auto r = wish("horse", spent);
    check(contains(r, "Poof!") && spent == 1, "W1 'horse' in Paws: Poof! (0x6f1e folds case): " + join(r));
    r = wish("HORSE", spent);
    check(contains(r, "Poof!") && spent == 1, "W2 'HORSE' too: " + join(r));
    r = wish("HHorse", spent);
    check(!contains(r, "Poof!") && contains(r, "No effect...") && spent == 1,
          "W3 'HHorse' is no match: 0x6f1e skips past a partial match: " + join(r));
    r = wish("Horse", spent);
    check(contains(r, "Poof!"), "W4 control: 'Horse' as typed in 1988 still works: " + join(r));
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a4_parity1_runtime <pack> <openu5-audio.bin>\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load\n");
        return 1;
    }
    test_crown();
    test_dungeon();
    test_well();
    std::printf("A4-PARITY1 runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
