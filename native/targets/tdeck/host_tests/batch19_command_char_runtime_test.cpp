// Batch 19 / R-25 -- the kernel 0x4988 command-character picker, through the
// REAL production stack.
//
// Same seam as batch18_well_ceremony_test.cpp (Batch 18) and
// batch14_zstats_runtime_test.cpp (Batch 14): this target links and drives the
// unmodified alpha_runtime.cpp, so every assertion below travels the path
// device firmware runs:
//
//   RawInputEvent -> tdeck::UiInputAdapter::translate()   (production)
//     -> openu5::UiSession::handle_input()                (production)
//     -> AlphaRuntime::dispatch_ui() / modal()            (production)
//     -> openu5::world_look() / search_world() / Cast     (production)
//
// Why this file exists. R-25 was never a core defect -- look.cpp's crystal-ball
// arm computed the right vision all along. What was broken was the GLUE that
// decides which member the core is handed, and whether it is handed one at
// all: the device raised a fabricated "Peer into it?" yes/no and then
// dispatched CrystalBall with Command::member still at its never-set -1, which
// the core rejected outright. Only production routing can expose that, and
// native/core/tests/batch19_command_char_test.cpp (which drives world_look()
// directly) cannot see the prompt the device actually opens.
//
// Reference authority: re/notes/resolve-command-char-178c-acta.md (kernel
// 0x4988 read whole, 0x4988-0x4a83) plus the three caller derivations --
// LOOKOBJ.OVL:0x09ea (re/notes/bola-144-acta.md section 2), SJOG cmd_search
// 0x095c -> 0x09a0 (re/notes/cmds.md section 11) and CAST.OVL:0x0dd5
// (re/notes/cast-input.md section 9).
#include "../main/alpha_runtime.h"
#include "../main/ui_input_adapter.h"

#include "openu5/command_char.h"
#include "openu5/commands.h"
#include "openu5/ui_session.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace openu5;

namespace {

int g_failures = 0, g_checks = 0;

void expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-6s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
}

// Tile 41 (0x29) is the crystal ball -- cmd_look's FIRST comparison at
// LOOKOBJ 0x09e4, which is why it never prints "Thou dost see".
constexpr uint8_t kBallTile = 41;
constexpr uint8_t kFloorTile = 5;

struct Member { const char *name; char status; uint8_t intelligence; };

struct Fixture {
    std::vector<uint8_t> tiles{std::vector<uint8_t>(256 * 256, kFloorTile)};
    std::unique_ptr<tdeck::AlphaRuntime> owner{new tdeck::AlphaRuntime()};
    tdeck::AlphaRuntime &rt = *owner;
    int64_t clock_us = 1'000'000;

    explicit Fixture(const std::vector<Member> &party, int active = 0xff) {
        // A crystal ball one cell NORTH of the party, so (L)ook + trackball-up
        // hits it.
        tiles[size_t(127) * 256 + 128] = kBallTile;

        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world = WorldData{tiles.data(), tiles.data(), tiles.size(), tiles.size()};
        rt.attach_host_test_fixture(hf);

        auto &g = rt.game();
        g.party.character_count = uint8_t(party.size());
        g.party.party_size = int32_t(party.size());
        g.party.active_character = uint8_t(active);
        for (size_t i = 0; i < party.size(); ++i) {
            auto &ch = g.party.characters[i];
            std::snprintf(ch.name, sizeof(ch.name), "%s", party[i].name);
            ch.status = party[i].status;
            ch.intelligence = party[i].intelligence;
            ch.dexterity = 15; ch.strength = 15;
            ch.current_hp = ch.max_hp = 100;
            ch.character_class = 'A';
            ch.level = 5;
            ch.current_mp = 50;
        }
        g.position.xy.x = 128; g.position.xy.y = 128;
        g.position.map.location = 0; g.position.map.floor = 0; // overworld
        g.gold = 100;
        g.torch_turns = 50;
    }

    bool key(uint8_t code) {
        tdeck::RawInputEvent raw{};
        raw.kind = tdeck::RawInputKind::Keyboard;
        raw.code = code;
        raw.transition = tdeck::KeyTransition::Pressed;
        raw.timestamp_us = (clock_us += 100000);
        return rt.handle(raw);
    }
    bool ball(tdeck::RawInputKind kind) {
        tdeck::RawInputEvent raw{};
        raw.kind = kind;
        raw.timestamp_us = (clock_us += 100000);
        return rt.handle(raw);
    }
    // The physical Mic key's short press is the device's Cancel/ESC.
    bool cancel() {
        tdeck::RawInputEvent down{};
        down.kind = tdeck::RawInputKind::Keyboard;
        down.column = tdeck::kMicrophoneKeyColumn;
        down.row = tdeck::kMicrophoneKeyRow;
        down.transition = tdeck::KeyTransition::Pressed;
        down.timestamp_us = (clock_us += 100000);
        rt.handle(down);
        tdeck::RawInputEvent up = down;
        up.transition = tdeck::KeyTransition::Released;
        up.timestamp_us = (clock_us += 50000);
        return rt.handle(up);
    }

    // (L)ook north at the ball, reaching the picker the way a player does.
    void look_at_ball() { key('l'); ball(tdeck::RawInputKind::TrackballUp); }

    UiMode mode() const { return const_cast<tdeck::AlphaRuntime &>(rt).ui()->mode(); }
    const char *prompt() const {
        auto *ui = const_cast<tdeck::AlphaRuntime &>(rt).ui();
        return ui->prompt() ? ui->prompt() : "";
    }
    std::string transcript() const {
        std::string out;
        auto *ui = const_cast<tdeck::AlphaRuntime &>(rt).ui();
        for (size_t i = 0; i < ui->transcript_size(); ++i) {
            const auto *b = ui->transcript_at(i);
            if (b) { out += b->text; out += '\n'; }
        }
        return out;
    }
    bool said(const char *needle) const { return transcript().find(needle) != std::string::npos; }
    bool saw_vision() const { return said("Strange vision!") || said("Death vision!"); }
};

// --- C1: the fabricated yes/no is gone --------------------------------------
void test_prompt_is_the_roster_picker() {
    std::printf("C1. (L)ook at a crystal ball opens the reference's own picker\n");
    Fixture f({{"Iolo", 'G', 20}, {"Shamino", 'G', 20}});
    f.look_at_ball();
    expect(f.mode() == UiMode::PartySelection, "C1-1",
           "two eligible members -> the roster picker of @0x4a02, not a yes/no");
    expect(std::strcmp(f.prompt(), command_char_prompt()) == 0, "C1-2",
           "the prompt is DS 0xa3c4 'Player: '");
    expect(!f.said("Peer into it?"), "C1-3",
           "the fabricated 'Peer into it?' is gone -- LOOKOBJ case 0x29 raises no yes/no");
    expect(!f.said("Thou dost see"), "C1-4",
           "and no 'Thou dost see' prefix: DS 0x751c lives in the branch the jne at 0x09e8 "
           "takes for everything that is NOT the ball");
    expect(!f.saw_vision(), "C1-5", "the roll waits for the pick (0x09ea precedes 0x09f6)");
}

// --- C2: the feature actually works end to end ------------------------------
void test_pick_runs_the_vision() {
    std::printf("C2. Answering the picker runs the crystal ball on that member\n");
    Fixture f({{"Iolo", 'G', 99}, {"Shamino", 'G', 99}});
    f.look_at_ball();
    f.key('\r'); // confirm the highlighted row
    expect(f.mode() != UiMode::PartySelection, "C2-1", "the picker closes on the answer");
    expect(f.saw_vision(), "C2-2",
           "a vision is emitted -- R-25's whole symptom was that NOTHING happened, because "
           "the dispatch carried member == -1 and look.cpp rejected it");
    // INT 99 can never be beaten by a 1..30 roll, so this arm is deterministic.
    expect(f.said("Strange vision!"), "C2-3",
           "INT 99 > any rand(1,30) -> the win branch at 0x0a2a");
    expect(f.rt.game().party.characters[0].current_hp == 100 &&
               f.rt.game().party.characters[1].current_hp == 100,
           "C2-4", "the win branch costs no HP");
}

void test_low_intelligence_takes_damage() {
    std::printf("C3. A low-INT member takes the Death vision and exactly 1 HP\n");
    Fixture f({{"Iolo", 'G', 0}, {"Shamino", 'G', 0}});
    f.look_at_ball();
    f.key('\r');
    expect(f.said("Death vision!"), "C3-1",
           "INT 0 can never exceed a 1..30 roll -> the lose branch at 0x0a12");
    const auto &p = f.rt.game().party;
    const int hurt = (p.characters[0].current_hp == 99) + (p.characters[1].current_hp == 99);
    expect(hurt == 1, "C3-2", "apply_damage(idx,1) at 0x0a19 costs exactly 1 HP");
    expect(p.characters[0].current_hp + p.characters[1].current_hp == 199, "C3-3",
           "and hits exactly ONE member -- the one the picker returned");
}

// --- C4: cancel ------------------------------------------------------------
void test_cancel_says_none() {
    std::printf("C4. Cancelling the picker prints None! and does nothing else\n");
    Fixture f({{"Iolo", 'G', 20}, {"Shamino", 'G', 20}});
    f.look_at_ball();
    f.cancel();
    expect(f.said(command_char_none()), "C4-1",
           "the common epilogue at @0x4a5f prints DS 0xa3da 'None!' whenever the picker "
           "leaves with -1");
    expect(!f.saw_vision(), "C4-2",
           "and 0x09f0's `inc ax / jne` skips the whole tail: no roll, no message, no damage");
    expect(f.rt.game().party.characters[0].current_hp == 100, "C4-3", "no HP moves on cancel");
}

// --- C5: the branches that must NOT prompt ----------------------------------
void test_active_character_skips_the_prompt() {
    std::printf("C5. An active character resolves directly (@0x49b2), with no prompt\n");
    Fixture f({{"Iolo", 'G', 99}, {"Shamino", 'G', 99}}, 1);
    f.look_at_ball();
    expect(f.mode() != UiMode::PartySelection, "C5-1",
           "g_active_char != 0xFF is returned straight back -- the routine never asks");
    expect(f.said("Strange vision!"), "C5-2", "and the vision runs immediately");
}

void test_single_eligible_auto_selects() {
    std::printf("C6. One eligible member auto-selects (@0x49fa), with no prompt\n");
    Fixture f({{"Iolo", 'D', 99}, {"Shamino", 'G', 0}});
    f.look_at_ball();
    expect(f.mode() != UiMode::PartySelection, "C6-1",
           "`cmp [bp-6],1` / `jle 0x4a5f`: a single eligible member is not put to a vote");
    // Shamino (INT 0) is the only 'G'/'P' member, so the auto-pick is
    // observable: the LOSS proves member 1 was chosen, not the INT-99 member 0.
    expect(f.said("Death vision!"), "C6-2",
           "and it is the ELIGIBLE member who is used -- INT 0 loses, which member 0's INT 99 "
           "could not do");
    expect(f.rt.game().party.characters[1].current_hp == 99, "C6-3",
           "the damage lands on member 1, not on the old 'else member 0' fallback");
}

void test_zero_eligible_says_none() {
    std::printf("C7. Zero eligible members print None! and stop (@0x49fa fall-through)\n");
    Fixture f({{"Iolo", 'D', 99}, {"Shamino", 'S', 99}});
    f.look_at_ball();
    expect(f.mode() != UiMode::PartySelection, "C7-1", "nothing is asked");
    expect(f.said(command_char_none()), "C7-2",
           "[bp-8] still holds the 0xFFFF of @0x4990, so the epilogue prints DS 0xa3da");
    expect(!f.saw_vision(), "C7-3", "and no vision runs");
}

// --- C8: the Disabled! re-ask ----------------------------------------------
void test_disabled_pick_reasks() {
    std::printf("C8. Picking a disabled member says Disabled! and RE-ASKS (@0x4a4e)\n");
    // Three members so the census sees two eligible and still prompts, with a
    // dead member 0 sitting under the picker's initial cursor.
    Fixture f({{"Iolo", 'D', 99}, {"Shamino", 'G', 99}, {"Dupre", 'G', 99}});
    f.look_at_ball();
    expect(f.mode() == UiMode::PartySelection, "C8-1", "two eligible -> the picker opens");
    f.key('\r'); // the cursor starts on member 0, who is 'D'
    expect(f.said(command_char_disabled()), "C8-2",
           "a pick whose status byte is neither 'G' nor 'P' prints DS 0xa3ce 'Disabled!'");
    expect(!f.saw_vision(), "C8-3", "and does NOT act on that member");
    expect(f.mode() == UiMode::PartySelection, "C8-4",
           "di is still 0 at @0x4a55, so @0x4a57 jumps back to the prompt -- a re-ask, not a "
           "rejection");
    expect(std::strcmp(f.prompt(), command_char_prompt()) == 0, "C8-5",
           "and the re-asked prompt is the same 'Player: ' row");
}

// --- C9: (S)earch, the second 0x4988 caller ---------------------------------
void test_search_asks_for_its_searcher() {
    std::printf("C9. (S)earch routes its searcher through the same picker\n");
    Fixture f({{"Iolo", 'G', 20}, {"Shamino", 'G', 20}});
    f.key('s');
    f.ball(tdeck::RawInputKind::TrackballUp); // SJOG 0x097e: the direction comes FIRST
    expect(f.mode() == UiMode::PartySelection, "C9-1",
           "SJOG cmd_search 0x095c asks for the direction at 0x097e and calls the picker at "
           "0x09a0 -- so the prompt comes after the aim, and it exists at all");
    expect(std::strcmp(f.prompt(), command_char_prompt()) == 0, "C9-2",
           "under the same DS 0xa3c4 'Player: ' row -- one routine, one string");
}

void test_search_with_active_character_does_not_ask() {
    std::printf("C10. (S)earch with an active character does not ask\n");
    Fixture f({{"Iolo", 'G', 20}, {"Shamino", 'G', 20}}, 0);
    f.key('s');
    f.ball(tdeck::RawInputKind::TrackballUp);
    expect(f.mode() != UiMode::PartySelection, "C10-1",
           "branch 2 resolves without a prompt, so the common case is unchanged for a player "
           "who keeps an active character");
    expect(f.said("Thou dost find") || f.said("nothing of note"), "C10-2",
           "and the search itself still runs");
}

void test_search_cancel_says_none() {
    std::printf("C11. Cancelling (S)earch's picker prints None! and searches nothing\n");
    Fixture f({{"Iolo", 'G', 20}, {"Shamino", 'G', 20}});
    f.key('s');
    f.ball(tdeck::RawInputKind::TrackballUp);
    f.cancel();
    expect(f.said(command_char_none()), "C11-1", "same -1 epilogue as every other caller");
    expect(!f.said("Thou dost find"), "C11-2", "and the parked search is dropped");
}

// --- C12: (C)ast, the third 0x4988 caller -----------------------------------
void test_cast_asks_before_the_spell_menu() {
    std::printf("C12. (C)ast resolves WHO casts before the spell menu opens\n");
    Fixture f({{"Iolo", 'G', 20}, {"Shamino", 'G', 20}});
    f.rt.game().spell_quantities[0] = 3;
    f.key('c');
    expect(f.mode() == UiMode::PartySelection, "C12-1",
           "CAST.OVL:0x0dd5 calls the picker BEFORE the 'Spell name:' prompt of 0x11d9 -- the "
           "OCR corpus of 49 original LP routes has 70 'Cast... Player: <name> Spell name:' "
           "rows and zero in the other order");
    expect(std::strcmp(f.prompt(), command_char_prompt()) == 0, "C12-2", "same 'Player: ' row");
    f.key('\r');
    expect(f.mode() == UiMode::SpellSelection, "C12-3",
           "and only THEN does the spell menu open");
}

void test_cast_with_active_character_goes_straight_to_the_menu() {
    std::printf("C13. (C)ast with an active character opens the spell menu directly\n");
    Fixture f({{"Iolo", 'G', 20}, {"Shamino", 'G', 20}}, 0);
    f.rt.game().spell_quantities[0] = 3;
    f.key('c');
    expect(f.mode() == UiMode::SpellSelection, "C13-1",
           "branch 2 again: an active character is never put to a vote");
}

void test_cast_cancel_says_none_and_opens_nothing() {
    std::printf("C14. Cancelling (C)ast's picker prints None! and opens no spell menu\n");
    Fixture f({{"Iolo", 'G', 20}, {"Shamino", 'G', 20}});
    f.rt.game().spell_quantities[0] = 3;
    f.key('c');
    f.cancel();
    expect(f.said(command_char_none()), "C14-1", "the -1 epilogue again");
    expect(f.mode() != UiMode::SpellSelection, "C14-2", "and the spell menu never opens");
}

// --- C15: the neighbours this batch must not disturb ------------------------
void test_well_and_fountain_prompts_unchanged() {
    std::printf("C15. Splitting tile 0x29 out left the well and fountain alone\n");
    Fixture f({{"Iolo", 'G', 20}, {"Shamino", 'G', 20}});
    f.tiles[size_t(127) * 256 + 128] = 161; // the well
    f.look_at_ball();
    expect(f.mode() == UiMode::YesNo, "C15-1",
           "the well still raises its yes/no (R-26, Batch 18)");
    expect(std::strcmp(f.prompt(), "Drop a coin?") == 0, "C15-2", "with its own prompt");

    Fixture g({{"Iolo", 'G', 20}, {"Shamino", 'G', 20}});
    g.tiles[size_t(127) * 256 + 128] = 216; // a fountain
    g.look_at_ball();
    expect(g.mode() == UiMode::PartySelection, "C15-3", "the fountain still picks a drinker");
    expect(std::strcmp(g.prompt(), "Who will drink?") == 0, "C15-4",
           "under ITS prompt, not the picker's -- pickMember flavour text, a different routine");
}

void test_ordinary_look_unchanged() {
    std::printf("C16. An ordinary (L)ook is untouched by the new first cut\n");
    Fixture f({{"Iolo", 'G', 20}, {"Shamino", 'G', 20}});
    f.tiles[size_t(127) * 256 + 128] = kFloorTile;
    f.look_at_ball();
    expect(f.said("Thou dost see"), "C16-1", "the generic branch still prints its prefix");
    expect(f.mode() != UiMode::PartySelection, "C16-2", "and opens no picker");
}

void test_chained_modal_prompt_survives() {
    std::printf("C17. R-34 regression -- a modal armed by a modal answer keeps its prompt\n");
    Fixture f({{"Iolo", 'G', 20}}, 0);
    f.rt.game().scroll_quantities[1] = 1; // Rel Hur: the one owned, selectable row
    f.key('u');
    expect(f.mode() == UiMode::InventorySelection, "C17-1", "(U)se opens the item picker");
    f.key('\r');
    expect(f.mode() == UiMode::TargetSelection, "C17-2",
           "answering with the Rel Hur scroll arms the direction prompt");
    expect(std::strcmp(f.prompt(), "Direction?") == 0, "C17-3",
           "and that prompt SURVIVES -- the crystal ball's own prompt-armed-by-a-prompt "
           "re-ask rides on the same finish_modal() contract");
}

} // namespace

int main() {
    std::printf("Batch 19 / R-25 -- kernel 0x4988 picker through production routing\n\n");
    test_prompt_is_the_roster_picker();
    test_pick_runs_the_vision();
    test_low_intelligence_takes_damage();
    test_cancel_says_none();
    test_active_character_skips_the_prompt();
    test_single_eligible_auto_selects();
    test_zero_eligible_says_none();
    test_disabled_pick_reasks();
    test_search_asks_for_its_searcher();
    test_search_with_active_character_does_not_ask();
    test_search_cancel_says_none();
    test_cast_asks_before_the_spell_menu();
    test_cast_with_active_character_goes_straight_to_the_menu();
    test_cast_cancel_says_none_and_opens_nothing();
    test_well_and_fountain_prompts_unchanged();
    test_ordinary_look_unchanged();
    test_chained_modal_prompt_survives();
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 205 : 0;
}
