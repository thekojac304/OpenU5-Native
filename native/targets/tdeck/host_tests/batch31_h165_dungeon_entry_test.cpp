// H-165: a town-exit question cannot be carried into a dungeon by Developer.
// TOWN.OVL 0x0798-0x07b4 blocks on Y/N/Esc; MAINOUT.OVL 0x0790 is reached
// only from the later E command. The Developer menu is a native-only escape
// from that blocking loop. Use the shipped pack and the real runtime/menu.
#include "../main/alpha_runtime.h"
#include "openu5/quest_state.h"
#include <cstdio>
#include <cstring>
#include <memory>

using namespace openu5;
namespace {
int checks = 0, failures = 0;
void expect(bool ok, const char *name) {
    ++checks;
    std::printf("%s %s\n", ok ? "GREEN" : "RED", name);
    if (!ok) ++failures;
}
const tdeck::AlphaResourceOwners *owners;
size_t dungeon_count;
struct Harness {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    int64_t tick = 0;
    Harness() {
        tdeck::AlphaRuntime::HostTestFixture f;
        f.world = owners->world;
        f.location_x = owners->location_x; f.location_y = owners->location_y;
        f.location_count = owners->location_count;
        f.npc_locations = owners->npc_locations;
        f.dungeons = owners->dungeons; f.dungeon_count = dungeon_count;
        f.pack = owners;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.party.character_count = g.party.party_size = 1;
        auto &ch = g.party.characters[0];
        std::snprintf(ch.name, sizeof(ch.name), "Avatar");
        ch.current_hp = ch.max_hp = 900; ch.status = 'G'; ch.character_class = 'A';
        ch.dexterity = ch.strength = 30;
        g.party.active_character = 255;
        g.time.hour = 12;
        g.position.map = {0, 0};
        g.position.xy = {owners->location_x[16], owners->location_y[16]};
    }
    void key(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent r{};
        r.kind = tdeck::RawInputKind::Keyboard; r.code = code;
        r.modifiers.alt = alt; r.transition = tdeck::KeyTransition::Pressed;
        r.timestamp_us = (tick += 100000); rt->handle(r);
    }
    void south() {
        tdeck::RawInputEvent r{};
        r.kind = tdeck::RawInputKind::TrackballDown;
        r.timestamp_us = (tick += 100000); rt->handle(r);
    }
    void west() {
        tdeck::RawInputEvent r{};
        r.kind = tdeck::RawInputKind::TrackballLeft;
        r.timestamp_us = (tick += 100000); rt->handle(r);
    }
    bool question() {
        key('e');
        rt->game().position.xy = {0, 15}; // stage at castle west edge
        west();
        return rt->ui()->mode() == UiMode::YesNo &&
               rt->ui()->request() == UiRequestId::TownExit &&
               rt->commands().awaiting_exit;
    }
    // Raw input through Developer -> Teleport -> Destination 34 (Deceit)
    // -> Teleport. Destination ordinal is sourced from the live pack.
    void teleport_deceit() {
        key('d', true); key('\r'); key('\r'); key('3'); key('4'); key('\r');
        for (int i = 0; i < 5; ++i) south();
        key('\r');
    }
    void close_menu() { key('\b'); key('\b'); }
};
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners loaded{};
    if (pack.load(loaded, report) != ESP_OK || report.dungeon_count != 8) return 2;
    owners = &loaded; dungeon_count = report.dungeon_count;
    Harness h;
    expect(h.question(), "H165-1 authentic castle exit question is pending");
    const auto before = h.rt->game().position;
    const auto rng = h.rt->game().rng.get_seed();
    const auto turns = h.rt->game().turns_since_start;
    h.teleport_deceit();
    expect(!h.rt->dungeon_state().active && !h.rt->command_context_for_test().dungeon,
           "H165-2 dungeon teleport is refused while exit answer is pending");
    expect(!h.rt->dungeon_state().active && !h.rt->command_context_for_test().dungeon &&
           h.rt->game().position.map.location == before.map.location &&
           h.rt->game().position.xy.x == before.xy.x && h.rt->game().position.xy.y == before.xy.y &&
           h.rt->game().rng.get_seed() == rng && h.rt->game().turns_since_start == turns,
           "H165-3 refusal preserves world, dungeon, RNG and turn");
    h.close_menu();
    expect(h.rt->ui()->mode() == UiMode::YesNo &&
           h.rt->ui()->request() == UiRequestId::TownExit && h.rt->commands().awaiting_exit,
           "H165-4 original question is still answerable");
    h.key('n');
    expect(!h.rt->commands().awaiting_exit && h.rt->ui()->mode() == UiMode::Exploration,
           "H165-5 declined answer clears original lock");
    h.teleport_deceit();
    expect(h.rt->dungeon_state().active && h.rt->command_context_for_test().dungeon,
           "H165-6 answered route enters the real dungeon session");
    const auto &d = h.rt->dungeon_state();
    expect(d.pos.dungeon == 33 && d.pos.floor == 0 && d.pos.x == 1 && d.pos.y == 1 &&
           d.pos.facing == DungeonFacing::South &&
           std::memcmp(d.cells, owners->dungeons[0].cells, sizeof(d.cells)) == 0,
           "H165-7 Developer entry uses authentic Deceit data and entry state");
    Harness normal;
    normal.rt->game().position.map = {0, 0};
    normal.rt->game().position.xy = {owners->location_x[32], owners->location_y[32]};
    set_quest_flag(normal.rt->game().quest, QuestFlag::Word33);
    normal.key('e');
    const auto &original_equivalent = normal.rt->dungeon_state();
    expect(original_equivalent.active && original_equivalent.pos.dungeon == d.pos.dungeon &&
           original_equivalent.pos.floor == d.pos.floor &&
           original_equivalent.pos.x == d.pos.x && original_equivalent.pos.y == d.pos.y &&
           original_equivalent.pos.facing == d.pos.facing &&
           std::memcmp(original_equivalent.cells, d.cells, sizeof(d.cells)) == 0,
           "H165-8 answered Developer entry matches normal E dungeon entry");
    h.close_menu();
    expect(h.rt->ui()->mode() == UiMode::Dungeon,
           "H165-9 resolved Developer entry yields dungeon controls");
    Harness town;
    expect(town.question(), "H165-10 independent pending town question");
    DebugTeleportRequest small{};
    small.kind = DebugDestinationKind::SmallMap;
    small.location = 31; small.floor = 1; small.x = 15; small.y = 3;
    expect(apply_debug_teleport(town.rt->command_context_for_test(), small).status ==
               DebugTeleportStatus::Applied && town.rt->commands().awaiting_exit &&
               town.rt->game().position.map.location == 31,
           "H165-11 non-dungeon Developer relocation retains Batch 25 policy");
    std::printf("batch31_h165_dungeon_entry: %d/%d GREEN\n", checks - failures, checks);
    return failures ? 1 : 0;
}
