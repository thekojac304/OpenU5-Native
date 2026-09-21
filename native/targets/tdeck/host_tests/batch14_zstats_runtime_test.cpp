// Batch 14 / R-22 -- (Z)-stats navigation, through the REAL production stack.
//
// Like alpha_runtime_integration_test.cpp (Batch 11), this target links and
// drives the unmodified alpha_runtime.cpp, so every assertion below travels
// the exact path device firmware runs:
//
//   RawInputEvent -> tdeck::UiInputAdapter::translate()   (production)
//     -> openu5::UiSession::handle_input()                (production)
//     -> AlphaRuntime::dispatch_ui() -> dispatch()        (production)
//     -> the Z-stats modal in AlphaRuntime::handle()      (production)
//
// Nothing here calls a formatter directly: the pages asserted on are the ones
// AlphaRuntime::zstats_view() hands to the renderer, and they are reached only
// by feeding keys and trackball events in.
//
// Reference authority: re/notes/ztats-layout.md sections 1b, 2, 3 and 8
// (cmd_zstats 0x0a3a's key loop, the 17-slot page axis, render_item_list's
// sub-loop, and select_player 0x0000 running FIRST).
#include "../main/alpha_runtime.h"
#include "../main/ui_input_adapter.h"

#include "openu5/commands.h"
#include "openu5/ui_session.h"
#include "openu5/zstats.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

using namespace openu5;

namespace {

int g_failures = 0, g_checks = 0;

void expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-6s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
}

std::vector<uint8_t> g_world_tiles(256 * 256, 5);

struct Fixture {
    std::unique_ptr<tdeck::AlphaRuntime> owner{new tdeck::AlphaRuntime()};
    tdeck::AlphaRuntime &rt = *owner;
    int64_t clock_us = 1'000'000;

    explicit Fixture(int members = 3) {
        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world = WorldData{g_world_tiles.data(), g_world_tiles.data(),
                              g_world_tiles.size(), g_world_tiles.size()};
        rt.attach_host_test_fixture(hf);

        auto &g = rt.game();
        g.party.character_count = uint8_t(members);
        g.party.party_size = members;
        g.party.active_character = 0;
        static const char *names[] = {"Alric", "Brianne", "Cordell", "Delwyn", "Enid", "Foster"};
        for (int i = 0; i < members; ++i) {
            auto &c = g.party.characters[i];
            std::snprintf(c.name, sizeof(c.name), "%s", names[i]);
            c.gender = 0x0b;
            c.character_class = 'A';
            c.status = 'G';
            // Deliberately distinct per member so a page showing the wrong
            // member is visible as a wrong number, not a plausible one.
            c.strength = uint8_t(10 + i);
            c.dexterity = uint8_t(20 + i);
            c.intelligence = uint8_t(30 + i);
            c.current_hp = uint16_t(40 + i);
            c.max_hp = uint16_t(50 + i);
            c.exp = uint16_t(60 + i);
            c.level = uint8_t(1 + i);
            c.current_mp = uint8_t(2 + i);
            c.helmet = c.armor = c.weapon = c.shield = c.ring = c.amulet = kEquipmentNothing;
        }
        g.position.xy.x = 128; g.position.xy.y = 128;
        g.position.map.location = 0; g.position.map.floor = 0;
        g.food = 900; g.gold = 100; g.keys = 1; g.gems = 2; g.torches = 3;
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
    bool ball(tdeck::RawInputKind kind) {
        tdeck::RawInputEvent raw{};
        raw.kind = kind;
        raw.timestamp_us = (clock_us += 100000);
        return rt.handle(raw);
    }
    bool next_page() { return ball(tdeck::RawInputKind::TrackballRight); }
    bool prev_page() { return ball(tdeck::RawInputKind::TrackballLeft); }

    // Open Z and confirm the member the picker starts on, reaching the page
    // axis the way a player does.
    void open_stats() { key('z'); key('\r'); }

    int64_t turns() const {
        return const_cast<tdeck::AlphaRuntime &>(rt).game().turns_since_start;
    }
    UiMode mode() const { return rt.ui()->mode(); }
};

// --- Z1 -------------------------------------------------------------------
void test_routing() {
    std::printf("Z1. Command routing\n");
    Fixture f;
    expect(f.mode() == UiMode::Exploration, "Z1-0", "the fixture starts in Exploration");

    f.key('z');
    expect(f.mode() == UiMode::PartySelection, "Z1-1",
           "(Z) routes through production into the member picker -- select_player 0x0000 "
           "runs FIRST, before any page");
    expect(!f.rt.zstats_active(), "Z1-2",
           "no page is open yet: the picker is a distinct phase");

    f.key('\r');
    expect(f.rt.zstats_active(), "Z1-3",
           "confirming a member opens the page axis");
    expect(f.rt.zstats_view().kind == ZStatsPageKind::Stats, "Z1-4",
           "and the first page is that member's STATS page (page = member*2)");
    expect(f.rt.zstats_view().page == 0 && f.rt.zstats_view().member == 0, "Z1-5",
           "member 0 -> axis slot 0");
    expect(std::strcmp(f.rt.zstats_view().banner, "Alric") == 0, "Z1-6",
           "the banner names the chosen member");
}

// --- Z7 -------------------------------------------------------------------
void test_member_navigation() {
    std::printf("Z7. Character navigation\n");
    Fixture f;
    f.key('z');
    f.ball(tdeck::RawInputKind::TrackballDown); // picker cursor -> member 1
    f.key('\r');
    expect(f.rt.zstats_active() && f.rt.zstats_view().member == 1, "Z7-1",
           "moving the picker cursor before confirming opens the OTHER member");
    expect(std::strcmp(f.rt.zstats_view().banner, "Brianne") == 0, "Z7-2",
           "and its banner is that member's name");

    // Digit jumps inside the page loop: cmd_zstats 0x0b12 bounds them by
    // g_party_size.
    f.key('3');
    expect(f.rt.zstats_view().member == 2 && f.rt.zstats_view().page == 4, "Z7-3",
           "'3' jumps straight to member 3's stats page (0x0b12, digit-1)");
    f.key('1');
    expect(f.rt.zstats_view().member == 0 && f.rt.zstats_view().page == 0, "Z7-4",
           "'1' jumps back to member 1");
    f.key('6');
    expect(f.rt.zstats_view().member == 0 && f.rt.zstats_view().page == 0, "Z7-5",
           "a digit beyond the party size is bounded, not honoured (jae at 0x0b12)");
    f.key('0');
    expect(f.rt.zstats_view().kind == ZStatsPageKind::Provisions, "Z7-6",
           "'0' jumps to the provisions page (0x0b37)");
}

// --- Z8 -------------------------------------------------------------------
void test_page_navigation() {
    std::printf("Z8. Page navigation through the whole family\n");
    Fixture f;
    f.open_stats();

    const ZStatsPageKind want[] = {
        ZStatsPageKind::Arms,  ZStatsPageKind::Stats, ZStatsPageKind::Arms,
        ZStatsPageKind::Stats, ZStatsPageKind::Arms,  ZStatsPageKind::Provisions,
        ZStatsPageKind::List,  ZStatsPageKind::List,  ZStatsPageKind::List,
        ZStatsPageKind::List,  ZStatsPageKind::Stats};
    bool ring_ok = true;
    int seen_pages[11]{};
    for (int i = 0; i < 11; ++i) {
        f.next_page();
        seen_pages[i] = f.rt.zstats_view().page;
        if (f.rt.zstats_view().kind != want[i]) ring_ok = false;
    }
    expect(ring_ok, "Z8-1",
           "forward paging walks stats/arms for all three members, then Provisions and the "
           "four lists, then wraps to member 1's stats page");
    expect(seen_pages[10] == 0, "Z8-2", "the wrap lands back on axis slot 0, not on a dead slot");
    expect(f.rt.zstats_active(), "Z8-3", "navigating never closes the modal (only Space/ESC do)");

    // The four list titles, in the binary's order.
    const char *titles[] = {"Reagents", "Spells", "Items", "Armaments"};
    bool titles_ok = true;
    f.key('0'); // provisions
    for (const char *title : titles) {
        f.next_page();
        if (std::strcmp(f.rt.zstats_view().banner, title) != 0) titles_ok = false;
    }
    expect(titles_ok, "Z8-4",
           "the four lists are Reagents, Spells, Items, Armaments in that order");

    // Backward is the mirror image, including the 0 -> 0x10 wrap.
    f.key('1');
    f.prev_page();
    expect(f.rt.zstats_view().page == kZStatsPageArmaments, "Z8-5",
           "paging back from slot 0 wraps to Armaments (gate 0xac6)");
    f.next_page();
    expect(f.rt.zstats_view().page == 0, "Z8-6", "and forward again returns to slot 0");
}

// --- Z9 -------------------------------------------------------------------
void test_modal_isolation() {
    std::printf("Z9. Modal input isolation\n");
    Fixture f;
    const auto pos0 = f.rt.game().position;
    const auto turns0 = f.turns();
    f.open_stats();

    // Every direction, twice round the ring: not one of them may reach the
    // world as a Move.
    for (int i = 0; i < 8; ++i) {
        f.ball(tdeck::RawInputKind::TrackballUp);
        f.ball(tdeck::RawInputKind::TrackballDown);
        f.ball(tdeck::RawInputKind::TrackballLeft);
        f.ball(tdeck::RawInputKind::TrackballRight);
    }
    expect(std::memcmp(&f.rt.game().position, &pos0, sizeof(pos0)) == 0, "Z9-1",
           "32 direction events while the modal is open move the player not at all");
    expect(f.turns() == turns0, "Z9-2", "and charge no turn");

    // Action letters that are real world commands outside the modal.
    const auto gems0 = f.rt.game().gems;
    const auto torches0 = f.rt.game().torches;
    f.key('i'); // Ignite torch
    f.key('p'); // Peer at gem
    f.key('k'); // Klimb
    f.key('e'); // Enter
    f.key('s'); // Search
    expect(f.rt.game().gems == gems0 && f.rt.game().torches == torches0, "Z9-3",
           "world command letters consume no resource while the stats modal owns input");
    expect(f.turns() == turns0, "Z9-4", "and dispatch no turn-charging command");
    expect(f.rt.zstats_active(), "Z9-5", "the modal is still open after all of them");
    expect(f.mode() == UiMode::Exploration, "Z9-6",
           "and none of them armed a prompt or picker behind the modal");

    // 'z' itself: cmd_zstats' loop has no cmp of 'z' -- it opens, it never
    // re-enters or closes.
    const int page_before = f.rt.zstats_view().page;
    f.key('z');
    expect(f.rt.zstats_active() && f.rt.zstats_view().page == page_before, "Z9-7",
           "'z' inside the loop is swallowed with no effect (no 'z' compare at 0x0a3a)");
}

// --- Z10 / Z11 ------------------------------------------------------------
void test_exit_and_turns() {
    std::printf("Z10/Z11. Exit semantics and turn accounting\n");
    Fixture f;
    const auto turns0 = f.turns();
    f.open_stats();
    expect(f.turns() == turns0, "Z11-1", "opening Z-stats charges no turn");

    f.next_page(); f.next_page(); f.next_page();
    expect(f.turns() == turns0, "Z11-2", "browsing pages charges no turn");

    const auto rng0 = f.rt.game().rng;
    f.next_page(); f.key('0'); f.key('2');
    expect(std::memcmp(&f.rt.game().rng, &rng0, sizeof(rng0)) == 0, "Z11-3",
           "and draws no RNG -- the display consumes zero rolls (zstats.md, RNG section)");

    f.key(' ');
    expect(!f.rt.zstats_active(), "Z10-1", "Space closes the modal (0x0a78)");
    expect(f.mode() == UiMode::Exploration, "Z10-2", "and gameplay UI is restored");
    expect(f.turns() == turns0, "Z11-4", "closing charges no turn either");

    // Gameplay resumes immediately.
    const auto pos_before = f.rt.game().position;
    f.ball(tdeck::RawInputKind::TrackballUp);
    expect(std::memcmp(&f.rt.game().position, &pos_before, sizeof(pos_before)) != 0, "Z10-3",
           "the very next direction moves the player again");
    expect(f.turns() == turns0 + 1, "Z10-4", "and charges exactly one turn");

    // ESC (the device's Mic short press) closes the same way.
    Fixture g;
    g.open_stats();
    g.cancel();
    expect(!g.rt.zstats_active() && g.mode() == UiMode::Exploration, "Z10-5",
           "Cancel/ESC closes the modal too (0x0a81)");

    // Cancelling the PICKER, before any page opens, must also return cleanly.
    Fixture h;
    h.key('z');
    h.cancel();
    expect(!h.rt.zstats_active() && h.mode() == UiMode::Exploration, "Z10-6",
           "abandoning the member picker leaves no half-open modal behind");
}

// --- Z14 ------------------------------------------------------------------
void test_lifecycle() {
    std::printf("Z14. Repeated open/navigate/close lifecycle\n");
    Fixture f;
    const auto turns0 = f.turns();
    bool stable = true;
    for (int cycle = 0; cycle < 6; ++cycle) {
        f.open_stats();
        if (!f.rt.zstats_active()) stable = false;
        // Every reopen must start on the stats page of the member the picker
        // confirmed, never on a page left over from the previous cycle.
        if (f.rt.zstats_view().kind != ZStatsPageKind::Stats) stable = false;
        if (f.rt.zstats_view().list_scroll != 0) stable = false;
        f.next_page(); f.next_page(); f.next_page(); f.next_page();
        f.key(' ');
        if (f.rt.zstats_active()) stable = false;
        if (f.mode() != UiMode::Exploration) stable = false;
    }
    expect(stable, "Z14-1",
           "six open/navigate/close cycles leave no stale page, scroll or mode");
    expect(f.turns() == turns0, "Z14-2", "and charge no turn across any of them");

    // A member chosen in one cycle must not leak into the next cycle's page
    // without the picker confirming it again.
    f.key('z');
    f.ball(tdeck::RawInputKind::TrackballDown);
    f.ball(tdeck::RawInputKind::TrackballDown);
    f.key('\r');
    expect(f.rt.zstats_view().member == 2, "Z14-3", "a later cycle can still pick any member");
    f.key(' ');
    f.key('z');
    f.key('\r');
    expect(f.rt.zstats_active() && f.rt.zstats_view().kind == ZStatsPageKind::Stats, "Z14-4",
           "and reopening straight away is sane");
}

// --- Z13 ------------------------------------------------------------------
void test_party_edges() {
    std::printf("Z13. Party-size and dead-member edge cases\n");
    Fixture solo(1);
    solo.open_stats();
    expect(solo.rt.zstats_active() && solo.rt.zstats_view().page == 0, "Z13-1",
           "a solo party opens its one member's stats page");
    solo.next_page();
    expect(solo.rt.zstats_view().kind == ZStatsPageKind::Arms, "Z13-2", "then its Arms page");
    solo.next_page();
    expect(solo.rt.zstats_view().kind == ZStatsPageKind::Provisions, "Z13-3",
           "then straight to Provisions -- the dead window is skipped");
    bool solo_ring = true;
    for (int i = 0; i < 24; ++i) {
        solo.next_page();
        const int page = solo.rt.zstats_view().page;
        if (page >= 2 && page < kZStatsPageProvisions) solo_ring = false;
        if (!solo.rt.zstats_active()) solo_ring = false;
    }
    expect(solo_ring, "Z13-4",
           "24 more steps never land in the dead window and never close the modal");

    // A dead member is still a roster member with a viewable sheet.
    Fixture f(3);
    f.rt.game().party.characters[1].status = 'D';
    f.rt.game().party.characters[1].current_hp = 0;
    f.open_stats();
    f.key('2');
    expect(f.rt.zstats_active() && f.rt.zstats_view().member == 1, "Z13-5",
           "a dead member's page is reachable");
    expect(std::strstr(f.rt.zstats_view().rows[1].text, "Dead") != nullptr, "Z13-6",
           "and reports Dead rather than corrupting the page");

    // A roster that shrinks while the modal is open must not strand the page.
    f.key('3');
    f.rt.game().party.party_size = 1;
    f.rt.game().party.character_count = 1;
    f.next_page();
    const auto view = f.rt.zstats_view();
    expect(view.page < 2 || view.page >= kZStatsPageProvisions, "Z13-7",
           "shrinking the roster under an open page clamps it out of the dead window");
    expect(view.row_count > 0, "Z13-8", "and the page still composes");
}

// --- list scrolling through real input ------------------------------------
void test_list_scrolling() {
    std::printf("Z8b. List scrolling through production input\n");
    Fixture f;
    auto &g = f.rt.game();
    for (int i = 0; i < 12; ++i) g.equipment_quantities[i] = i + 1;
    f.open_stats();
    f.key('0');
    f.next_page(); f.next_page(); f.next_page(); f.next_page(); // -> Armaments
    expect(std::strcmp(f.rt.zstats_view().banner, "Armaments") == 0, "Z8b-1",
           "reached the Armaments list");
    expect(f.rt.zstats_view().list_total == 12 &&
               f.rt.zstats_view().row_count == kZStatsListRows,
           "Z8b-2", "twelve owned entries, seven rows shown");

    f.ball(tdeck::RawInputKind::TrackballDown);
    expect(f.rt.zstats_view().list_scroll == 1 &&
               f.rt.zstats_view().kind == ZStatsPageKind::List,
           "Z8b-3",
           "inside an overflowing list, down SCROLLS by one instead of changing page "
           "(render_item_list sub-loop 0x086c)");
    f.ball(tdeck::RawInputKind::TrackballUp);
    expect(f.rt.zstats_view().list_scroll == 0, "Z8b-4", "and up scrolls back");

    f.next_page();
    expect(f.rt.zstats_view().page == 0, "Z8b-5",
           "left/right still change page from inside a list (exit arm 0x948)");

    // A list that fits has no scroll, so up/down fall through to the axis --
    // exactly the refinement of ztats-layout.md 8.5.
    f.key('0');
    f.next_page(); // Reagents, empty
    const int page_before = f.rt.zstats_view().page;
    f.ball(tdeck::RawInputKind::TrackballDown);
    expect(f.rt.zstats_view().page != page_before, "Z8b-6",
           "with nothing to scroll, down cycles the axis as it does off a list");
}

} // namespace

int main() {
    std::printf("Batch 14 / R-22 -- Z-stats navigation through production routing\n\n");
    test_routing();
    test_member_navigation();
    test_page_navigation();
    test_modal_isolation();
    test_exit_and_turns();
    test_lifecycle();
    test_party_edges();
    test_list_scrolling();
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
