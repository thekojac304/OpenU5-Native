// Batch 18 / R-26 -- the wishing-well ceremony, through the REAL production stack.
//
// Same seam as alpha_runtime_integration_test.cpp (Batch 11) and
// batch14_zstats_runtime_test.cpp (Batch 14): this target links and drives the
// unmodified alpha_runtime.cpp, so every assertion below travels the exact
// path device firmware runs:
//
//   RawInputEvent -> tdeck::UiInputAdapter::translate()   (production)
//     -> openu5::UiSession::handle_input()                (production)
//     -> AlphaRuntime::dispatch_ui() -> modal()           (production)
//     -> openu5::world_look()'s DropCoin arm              (production)
//
// Why this file exists. The whole well ceremony -- description line, prompt
// type, and the answer dispatch -- lives in the T-Deck glue, and no host test
// in the project reached it. gameplay_parity's corpus drives the CORE (the
// driver's "coin" op), which was always correct; what nobody exercised was the
// layer that decides WHICH member value the core is handed, and whether it is
// handed one at all. R-26 lived in exactly that blind spot from the original
// audit baseline through Batch 17.
//
// Reference authority (LOOKOBJ.OVL, via game/src):
//   * game/src/main.ts "well-drop-prompt": hud.message("a well.\n\nDrop a
//     coin?") -- LOOKOBJ 0x0048 / DATA.OVL DS 0x720c -- then a prompt of type
//     "yesno-esc", i.e. ESC is a valid answer and it means No.
//   * game/src/core/game.ts dropCoin(yes): !yes -> "No\n" (LOOKOBJ 0x0068 ->
//     DS 0x7222) and return; yes -> "Yes\n" (LOOKOBJ 0x006e -> DS 0x7226)
//     BEFORE the gold check, then the wish prompt only when gold > 0
//     (LOOKOBJ 0x0075 cmp g_gold,0 -> silent ret, no "Thou hast no coin!").
//   * game/src/core/world/cmd-strings.ts WELL_UI.wish = "\nThy wish?" --
//     DATA.OVL DS 0x722c, printed by LOOKOBJ 0x007f, getstring max 0xC.
//
// native/core/src/look.cpp's DropCoin arm already implements all of that
// byte-for-byte (say(cmd.member ? "Yes\n" : "No\n")). Nothing here changes it.
#include "../main/alpha_runtime.h"
#include "../main/ui_input_adapter.h"

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

// Tile 161 is the well (look.cpp:46 routes it to GameEventKind::WellDropPrompt).
constexpr uint8_t kWellTile = 161;
constexpr uint8_t kFloorTile = 5;

struct Fixture {
    // Per-fixture world: this suite MUTATES a tile, so it must not share the
    // other suites' const overworld.
    std::vector<uint8_t> tiles{std::vector<uint8_t>(256 * 256, kFloorTile)};
    std::unique_ptr<tdeck::AlphaRuntime> owner{new tdeck::AlphaRuntime()};
    tdeck::AlphaRuntime &rt = *owner;
    int64_t clock_us = 1'000'000;

    explicit Fixture(int32_t gold = 100) {
        // A well one cell NORTH of the party, so (L)ook + trackball-up hits it.
        tiles[size_t(127) * 256 + 128] = kWellTile;

        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world = WorldData{tiles.data(), tiles.data(), tiles.size(), tiles.size()};
        rt.attach_host_test_fixture(hf);

        auto &g = rt.game();
        g.party.character_count = g.party.party_size = 1;
        auto &ch = g.party.characters[0];
        ch.intelligence = 15; ch.dexterity = 15; ch.strength = 15;
        ch.current_hp = ch.max_hp = 100;
        ch.status = 'G';
        ch.character_class = 'A';
        g.position.xy.x = 128; g.position.xy.y = 128;
        g.position.map.location = 0; g.position.map.floor = 0; // overworld
        g.gold = gold;
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

    // (L)ook north at the well, reaching the prompt the way a player does.
    void look_at_well() { key('l'); ball(tdeck::RawInputKind::TrackballUp); }

    UiMode mode() const { return const_cast<tdeck::AlphaRuntime &>(rt).ui()->mode(); }
    const char *prompt() const {
        auto *ui = const_cast<tdeck::AlphaRuntime &>(rt).ui();
        return ui->prompt() ? ui->prompt() : "";
    }

    // The whole transcript, newest included, as one searchable string.
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
};

// --- W1: reaching the prompt ------------------------------------------------
void test_prompt() {
    std::printf("W1. Looking at a well raises the reference's own prompt\n");
    Fixture f;
    f.look_at_well();
    expect(f.mode() == UiMode::YesNo, "W1-1",
           "(L)ook at tile 161 opens the yes/no prompt (look.cpp:46 -> WellDropPrompt)");
    expect(std::strcmp(f.prompt(), "Drop a coin?") == 0, "W1-2",
           "the prompt is the reference's own 'Drop a coin?' (DATA.OVL DS 0x720c)");
    expect(f.said("a well."), "W1-3",
           "the object description 'a well.' is printed with it -- LOOKOBJ 0x0048 prints the "
           "description and the prompt in ONE print, the same shape the fountain already uses");
}

// --- W2: the 'N' answer -----------------------------------------------------
void test_answer_no() {
    std::printf("W2. Answering No echoes 'No' and spends nothing\n");
    Fixture f;
    const auto gold0 = f.rt.game().gold;
    f.look_at_well();
    f.key('n');
    expect(f.said("No"), "W2-1",
           "dropCoin(false) prints 'No' (LOOKOBJ 0x0068 -> DS 0x7222); the answer must reach "
           "the core at all");
    expect(f.mode() == UiMode::Exploration, "W2-2", "and the prompt closes back to Exploration");
    expect(f.rt.game().gold == gold0, "W2-3", "No spends no gold");
    expect(!f.said("wish"), "W2-4", "No does not open the wish row");
}

// --- W3: ESC means No -------------------------------------------------------
void test_cancel_means_no() {
    std::printf("W3. ESC/Cancel is the No answer, not an ignored key\n");
    Fixture f;
    const auto gold0 = f.rt.game().gold;
    f.look_at_well();
    f.cancel();
    expect(f.mode() == UiMode::Exploration, "W3-1",
           "the reference prompt type is 'yesno-esc' (main.ts well-drop-prompt): ESC is a valid "
           "answer, so the modal must close");
    expect(f.said("No"), "W3-2", "and it is the No answer -- it echoes 'No', exactly as 'n' does");
    expect(f.rt.game().gold == gold0, "W3-3", "cancelling spends no gold");
}

// --- W4: the 'Y' answer with gold ------------------------------------------
void test_answer_yes_with_gold() {
    std::printf("W4. Answering Yes with gold echoes 'Yes' and opens the wish row\n");
    Fixture f(100);
    f.look_at_well();
    f.key('y');
    expect(f.said("Yes"), "W4-1", "dropCoin(true) echoes 'Yes' (LOOKOBJ 0x006e -> DS 0x7226)");
    expect(f.mode() == UiMode::TextEntry, "W4-2",
           "with gold > 0 the wish row opens (LOOKOBJ 0x007f)");
    expect(std::strcmp(f.prompt(), "Thy wish?") == 0, "W4-3",
           "the wish row is DATA.OVL DS 0x722c 'Thy wish?', not an invented phrasing");
}

// --- W5: the 'Y' answer without gold ---------------------------------------
void test_answer_yes_without_gold() {
    std::printf("W5. Answering Yes with no gold echoes 'Yes' and stops, silently\n");
    Fixture f(0);
    f.look_at_well();
    f.key('y');
    expect(f.said("Yes"), "W5-1",
           "the Yes echo precedes the gold check in the binary (0x006e before 0x0075)");
    expect(f.mode() == UiMode::Exploration, "W5-2",
           "no wish row: LOOKOBJ 0x0075's cmp g_gold,0 returns silently");
    expect(!f.said("no coin"), "W5-3",
           "and it returns with NO text -- 'Thou hast no coin!' is a fabrication the reference "
           "does not print");
}

// --- W6: R-34, the general invariant behind W4-3 ----------------------------
//
// W4-3's cause is NOT specific to the well. UiSession::finish_modal() clears
// prompt_/input_ AFTER dispatching the answer, and an answer is allowed to
// arm the next modal synchronously inside that dispatch -- so the freshly-set
// prompt of the follow-up modal is wiped while its mode_ survives, leaving
// mode == <modal> with an empty prompt row. Every chained modal in the
// project is affected; this group pins a SECOND, independent one so a future
// change cannot "fix the well" without fixing the mechanism.
//
// The chain used here is the Rel Hur scroll (Y-21/Y-31): AlphaRuntime::modal()
// answers the (U)se picker by calling ui_->begin_target(UseTarget,
// "Direction?", c) -- reference main.ts raises the ordinary getdir for scroll
// id 1 -- and that call happens inside finish_modal()'s own dispatch.
void test_chained_modal_prompt_survives() {
    std::printf("W6. R-34 -- a modal armed by a modal answer keeps its prompt\n");
    Fixture f;
    f.rt.game().scroll_quantities[1] = 1; // Rel Hur: the one owned, selectable row

    f.key('u');
    expect(f.mode() == UiMode::InventorySelection, "W6-1", "(U)se opens the item picker");
    f.key('\r');
    expect(f.mode() == UiMode::TargetSelection, "W6-2",
           "answering with the Rel Hur scroll arms the direction prompt (alpha_runtime.cpp "
           "modal(), Inventory arm)");
    expect(std::strcmp(f.prompt(), "Direction?") == 0, "W6-3",
           "and that prompt SURVIVES: finish_modal() must not clear prompt_ after a dispatch "
           "that armed a new modal");
}

} // namespace

int main() {
    std::printf("Batch 18 / R-26 -- wishing-well ceremony through production routing\n\n");
    test_prompt();
    test_answer_no();
    test_cancel_means_no();
    test_answer_yes_with_gold();
    test_answer_yes_without_gold();
    test_chained_modal_prompt_survives();
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 205 : 0;
}
