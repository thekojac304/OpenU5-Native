// Batch 19 -- R-25.  The shared command-character picker (kernel 0x4988) and
// the crystal ball (LOOKOBJ cmd_look case 0x29) that is its first caller.
//
// Every assertion here drives a PRODUCTION entry point -- the seam itself, and
// world_look() for both the (L)ook route and the CommandKind::CrystalBall arm
// -- never a local re-implementation of the picker.  The reference derivation
// is re/notes/resolve-command-char-178c-acta.md (body read whole, ULTIMA.EXE
// 0x4988-0x4a83) and re/notes/bola-144-acta.md sections 2 and 2.2.
//
// SCOPE. The picker in the binary is BLOCKING and owns a modal, so in this port
// it is resolved at the caller/UI boundary, not inside world_look() -- the same
// split the reference port makes (game.ts::look() returns a bare
// crystal-ball-prompt for EVERY tile 0x29; ui/pickers.ts::pickCommandChar runs
// the picker).  gameplay_parity pins that event stream, so this file covers the
// seam's decisions and the core's own crystal-ball tail, while the branch
// routing, the "Player: " modal, the "Disabled!" re-ask and the "None!" exits
// are proven through the real AlphaRuntime in
// native/targets/tdeck/host_tests/batch19_command_char_runtime_test.cpp.
#include "openu5/command_char.h"
#include "openu5/look.h"
#include "openu5/ui_session.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0;
int failures = 0;
const char *label = "";
// Reports EVERY failing assertion instead of stopping at the first, so one RED
// run enumerates the whole divergence surface in a single log rather than
// yielding one finding per rebuild.
void check(bool value) {
    ++checks;
    if (!value) {
        ++failures;
        std::cerr << "batch19 command-char check " << checks << " (" << label << ") failed\n";
    }
}

// Scripted draws so the INT-vs-roll comparison at LOOKOBJ 0x0a0e-0x0a10 is a
// decision under test, not a coin flip.  Every draw is counted so a test can
// also assert that a branch consumed NO randomness at all (bola-144 section 8:
// nothing but the ball's own path may move the stream).
struct Dice {
    std::vector<int32_t> scripted;
    size_t next = 0;
    int draws = 0;
    static int32_t roll(void *context, int32_t lo, int32_t hi) {
        auto &d = *static_cast<Dice *>(context);
        ++d.draws;
        if (d.next < d.scripted.size()) return d.scripted[d.next++];
        return lo + (hi - lo) / 2;
    }
    Rand source() { return {this, roll}; }
};

struct Probe {
    UiSession *ui = nullptr;
    std::vector<std::string> messages;
    std::vector<GameEventKind> kinds;
    int gem_views = 0;
    bool gem_from_crystal = false;
    static void emit(void *context, const GameEvent &event) {
        auto &probe = *static_cast<Probe *>(context);
        probe.kinds.push_back(event.kind);
        if (event.kind == GameEventKind::Message) probe.messages.emplace_back(event.text ? event.text : "");
        if (event.kind == GameEventKind::GemView) { ++probe.gem_views; probe.gem_from_crystal = event.gem_from_crystal; }
        if (probe.ui) probe.ui->consume(event);
    }
    bool saw(GameEventKind k) const {
        for (auto v : kinds) if (v == k) return true;
        return false;
    }
    bool said(const std::string &text) const {
        for (const auto &m : messages) if (m == text) return true;
        return false;
    }
};

// One party slot.  `status` is the byte the census at 0x49dc sweeps
// ([0x55b3 + n*0x20]); 'G' = 0x47 and 'P' = 0x50 are the two eligible values.
struct Slot { const char *name; char status; uint8_t intelligence; uint16_t hp; };

struct World {
    std::vector<uint8_t> tiles = std::vector<uint8_t>(256U * 256U, 5);
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    WorldData world{};
    LookServices services{};
    UiTextBlock storage[16]{};
    UiSession ui{{storage, 16}, {}, {38, 8, 12}};
    std::unique_ptr<CommandContext> held;

    explicit World(const std::vector<Slot> &party, int active = 0xff) {
        // Party at (54,66) facing a crystal ball (tile 0x29 = 41) at (55,66).
        tiles[66U * 256U + 55U] = 41;
        game.position = {{54, 66}, {0, 0}};
        game.party.character_count = uint8_t(party.size());
        game.party.party_size = int32_t(party.size());
        game.party.active_character = uint8_t(active);
        for (size_t i = 0; i < party.size(); ++i) {
            auto &ch = game.party.characters[i];
            std::snprintf(ch.name, sizeof(ch.name), "%s", party[i].name);
            ch.status = party[i].status;
            ch.intelligence = party[i].intelligence;
            ch.current_hp = party[i].hp;
            ch.max_hp = party[i].hp;
        }
        services.describe = [](void *, int32_t) { return "a crytal sphere"; };
    }
    ActiveMap map() { return ActiveMap{{0, 0}, MapKind::Overworld, {256, 256, true}, tiles.data(), -1}; }
    CommandContext &context() {
        held.reset(new CommandContext{game, turn, travel, commands, world});
        held->look = &services;
        return *held;
    }
};

// (L)ook east at the crystal ball through the real world_look().
CommandStatus look_at_ball(World &w, Probe &probe, Dice &dice) {
    Command command{};
    command.kind = CommandKind::Look;
    command.direction = Direction::East;
    auto &ctx = w.context();
    return world_look(ctx, command, w.map(), {&probe, Probe::emit}, dice.source());
}

// Answer the picker with `member` (-1 = cancelled) through the real
// CommandKind::CrystalBall arm -- the arm AlphaRuntime dispatches to.
CommandStatus answer(World &w, Probe &probe, Dice &dice, int member) {
    Command command{};
    command.kind = CommandKind::CrystalBall;
    command.member = int16_t(member);
    auto &ctx = w.context();
    return world_look(ctx, command, w.map(), {&probe, Probe::emit}, dice.source());
}
} // namespace

int main() {
    // ---------------------------------------------------------------- seam --
    // Branch order is the binary's, not a convenience: active (@0x49b2) is
    // tested BEFORE the eligibility census (@0x49dc), and the census decides
    // auto-single (@0x49fa) before the prompt (@0x4a02).
    label = "seam: active character wins, without an eligibility test";
    {
        PartyState p{};
        p.character_count = 3; p.party_size = 3; p.active_character = 2;
        p.characters[0].status = 'G'; p.characters[1].status = 'G'; p.characters[2].status = 'D';
        const auto r = resolve_command_char(p);
        // @0x49b2 compares g_active_char with 0xFF and nothing else -- a
        // disabled ACTIVE character is still returned directly.
        check(r.outcome == CommandCharOutcome::Resolved);
        check(r.member == 2);
    }

    label = "seam: no active, exactly one eligible -> auto-select that one";
    {
        PartyState p{};
        p.character_count = 3; p.party_size = 3; p.active_character = 0xff;
        p.characters[0].status = 'D'; p.characters[1].status = 'P'; p.characters[2].status = 'D';
        const auto r = resolve_command_char(p);
        check(r.outcome == CommandCharOutcome::Resolved);
        // 'P' (0x50) is eligible exactly like 'G' (0x47), and the auto-single
        // is member 1 -- NOT member 0, which is the old native fallback.
        check(r.member == 1);
    }

    label = "seam: no active, zero eligible -> None!, no prompt";
    {
        PartyState p{};
        p.character_count = 2; p.party_size = 2; p.active_character = 0xff;
        p.characters[0].status = 'D'; p.characters[1].status = 'C';
        const auto r = resolve_command_char(p);
        check(r.outcome == CommandCharOutcome::None);
        check(r.member == -1);
    }

    label = "seam: no active, two or more eligible -> prompt";
    {
        PartyState p{};
        p.character_count = 3; p.party_size = 3; p.active_character = 0xff;
        p.characters[0].status = 'D'; p.characters[1].status = 'G'; p.characters[2].status = 'P';
        const auto r = resolve_command_char(p);
        check(r.outcome == CommandCharOutcome::Prompt);
        check(r.member == -1);
    }

    label = "seam: eligibility predicate is exactly 'G' and 'P'";
    {
        check(command_char_eligible('G'));
        check(command_char_eligible('P'));
        const char ineligible[] = {'D', 'S', 'C', 'A', 'R', '\0', 'g', 'p'};
        for (char s : ineligible) check(!command_char_eligible(s));
    }

    label = "seam: active outside the roster is not an active character";
    {
        PartyState p{};
        p.character_count = 2; p.party_size = 2; p.active_character = 5;
        p.characters[0].status = 'G'; p.characters[1].status = 'G';
        // Out of range is treated as "none set", so the census runs and both
        // members are eligible -> prompt.
        check(resolve_command_char(p).outcome == CommandCharOutcome::Prompt);
    }

    label = "seam: post-pick gate accepts only 'G'/'P' (@0x4a2e)";
    {
        PartyState p{};
        p.character_count = 3; p.party_size = 3; p.active_character = 0xff;
        p.characters[0].status = 'G'; p.characters[1].status = 'D'; p.characters[2].status = 'P';
        check(command_char_accepts(p, 0));
        check(!command_char_accepts(p, 1));
        check(command_char_accepts(p, 2));
        check(!command_char_accepts(p, -1));
        check(!command_char_accepts(p, 3));
    }

    // -------------------------------------------------- crystal-ball core --
    // What the CORE owns is the tail of LOOKOBJ's branch: the roll, the
    // comparison, the damage and the gem view.  The picker's own branches and
    // its three strings belong to the caller-side seam and are proven in
    // native/targets/tdeck/host_tests/batch19_command_char_runtime_test.cpp,
    // which drives the real AlphaRuntime.
    label = "ball: (L)ook raises the prompt and touches nothing else";
    {
        World w({{"Iolo", 'G', 20, 30}}, 0);
        Probe probe{&w.ui};
        Dice dice{};
        check(look_at_ball(w, probe, dice) == CommandStatus::Success);
        check(probe.saw(GameEventKind::CrystalBallPrompt));
        // Tile 0x29 is cmd_look's FIRST comparison (0x09e4), and DS 0x751c
        // plus the LOOK2 phrase live in the branch the jne at 0x09e8 takes.
        check(!probe.said("Thou dost see a crytal sphere"));
        // game.ts look() returns a bare crystal-ball-prompt: no roll here, so
        // merely looking cannot move the stream.  gameplay_parity pins this.
        check(dice.draws == 0);
        check(w.game.party.characters[0].current_hp == 30);
    }

    label = "ball: the vision runs on the member the picker returned";
    {
        World w({{"Iolo", 'G', 31, 30}, {"Shamino", 'G', 2, 30}});
        Probe probe{&w.ui};
        Dice dice{{10}};
        check(answer(w, probe, dice, 1) == CommandStatus::Success);
        check(dice.draws == 1);              // the ONE roll of 0x09f6.
        check(probe.said("Death vision!"));
        // 0x0a19 pushes idx: the damage lands on the CHOSEN member, and the
        // INT read at 0x0a08 is that member's, not member 0's.
        check(w.game.party.characters[1].current_hp == 29);
        check(w.game.party.characters[0].current_hp == 30);
        check(probe.saw(GameEventKind::PartyChanged));
    }

    label = "ball: winning gives Strange vision! and a gem view that costs no gem";
    {
        World w({{"Iolo", 'G', 31, 30}});
        Probe probe{&w.ui};
        Dice dice{{30}};                     // INT 31 > 30 -> the win branch.
        check(answer(w, probe, dice, 0) == CommandStatus::Success);
        check(probe.said("Strange vision!"));
        check(probe.gem_views == 1);
        // The dec [g_gems] lives at 0x3428 inside the (V) case, OUTSIDE this
        // route -- gem_from_crystal is what tells the device to charge nothing.
        check(probe.gem_from_crystal);
        check(w.game.party.characters[0].current_hp == 30);
    }

    label = "ball: the tie LOSES (ja at 0x0a10 is unsigned, INT must EXCEED)";
    {
        World w({{"Iolo", 'G', 17, 30}});
        Probe probe{&w.ui};
        Dice dice{{17}};
        check(answer(w, probe, dice, 0) == CommandStatus::Success);
        check(probe.said("Death vision!"));
        check(w.game.party.characters[0].current_hp == 29);
    }

    label = "ball: an out-of-range member is still refused, and costs no roll";
    {
        World w({{"Iolo", 'G', 20, 30}});
        Probe probe{&w.ui};
        Dice dice{};
        // Native's own safety net, unchanged by this batch: the picker never
        // produces these, so reaching the core with one is a caller bug.
        check(answer(w, probe, dice, -1) == CommandStatus::Rejected);
        check(answer(w, probe, dice, 7) == CommandStatus::Rejected);
        check(dice.draws == 0);
        check(probe.gem_views == 0);
    }

    label = "look: the neighbouring prompts are untouched";
    {
        World w({{"Iolo", 'G', 20, 30}}, 0);
        w.tiles[66U * 256U + 55U] = 161;     // the well
        Probe probe{&w.ui};
        Dice dice{};
        check(look_at_ball(w, probe, dice) == CommandStatus::Success);
        check(probe.saw(GameEventKind::WellDropPrompt));
        check(!probe.saw(GameEventKind::CrystalBallPrompt));
    }

    label = "look: a plain tile still gets its Thou dost see prefix";
    {
        World w({{"Iolo", 'G', 20, 30}}, 0);
        w.tiles[66U * 256U + 55U] = 5;
        Probe probe{&w.ui};
        Dice dice{};
        check(look_at_ball(w, probe, dice) == CommandStatus::Success);
        check(probe.said("Thou dost see a crytal sphere"));
        check(dice.draws == 0);
        check(!probe.saw(GameEventKind::CrystalBallPrompt));
    }

    label = "strings are the DATA.OVL literals, newline-pruned like siblings";
    {
        check(std::strcmp(command_char_prompt(), "Player: ") == 0);   // DS 0xa3c4
        check(std::strcmp(command_char_disabled(), "Disabled!") == 0); // DS 0xa3ce
        check(std::strcmp(command_char_none(), "None!") == 0);         // DS 0xa3da
    }


    if (failures) {
        std::cerr << failures << " of " << checks << " Batch 19 checks FAILED\n";
        return 1;
    }
    std::cout << checks << " Batch 19 command-character picker and crystal-ball checks passed\n";
}
