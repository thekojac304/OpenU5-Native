// Batch 22 -- Lord British's Castle basement chests, traced through the REAL
// device runtime with the REAL shipped data.
//
// The Batch 21B probes proved the core can hydrate the three authored vault
// chests, but both of them supplied their own QuestWorldServices and their own
// reload callback, so neither exercised the AlphaRuntime object callbacks or
// the device's command_reload(). This test does:
//
//   * the data is native/assets/openu5-alpha1-resources.bin, loaded by the
//     production AlphaResourcePack exactly as initialize() loads it off the
//     card (npcs.bin -> resources_.npc_locations, smallmaps.bin, the overworld
//     and the location table);
//   * the runtime is the production tdeck::AlphaRuntime (Batch 11 seam), so
//     every pool operation goes through object_reserve/object_append/
//     object_erase and every reload through AlphaRuntime::command_reload;
//   * the two ways a player reaches the basement are both driven:
//       A. (E)nter from the overworld castle tile, then (K)limb down the
//          authored ladder -- raw key input through UiInputAdapter/UiSession;
//       B. the Developer Teleport screen -- the identical
//          apply_debug_teleport(context_, request) call UiDebugMenu makes on
//          the device, over the runtime's own CommandContext;
//   * the final check composes the snapshot with the same
//     compose_world_presentation(context_, ...) call AlphaRuntime::render()
//     makes, so "in the pool" and "painted" are separate assertions.
//
// The tracked cells come from the device report and GAMEPLAY_INTEGRATION_
// AUDIT.md, Batch 21B: chests at (16,21), (17,22), (13,23) and the (9,9)
// control object, all authored with z = 0xFF (runtime floor -1). Slots are
// located by coordinate, never by an assumed slot number.
#include "../main/alpha_runtime.h"
#include "../main/ui_input_adapter.h"

#include "openu5/debug_map_picker.h"
#include "openu5/presentation.h"
#include "openu5/world.h"

#include <cstdio>
#include <memory>

using namespace openu5;

namespace {

int g_failures = 0, g_checks = 0;
void expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-4s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
}

constexpr uint8_t kCastle = 17;
constexpr int16_t kBasement = -1;
struct Cell { int x, y, type; };
constexpr Cell kChests[3] = {{16, 21, 1}, {17, 22, 1}, {13, 23, 1}};
constexpr Cell kControl = {9, 9, 30};

const tdeck::AlphaResourceOwners *g_owners = nullptr;

const MapData *small_map(int16_t floor) {
    const auto &w = g_owners->world;
    for (size_t i = 0; i < w.small_map_count; ++i)
        if (w.small_maps[i].id.location == kCastle && w.small_maps[i].id.floor == floor)
            return &w.small_maps[i];
    return nullptr;
}

struct Harness {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    int64_t clock_us = 0;

    explicit Harness(int hour) {
        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world = g_owners->world;
        hf.location_x = g_owners->location_x; hf.location_y = g_owners->location_y;
        hf.location_count = g_owners->location_count;
        hf.npc_locations = g_owners->npc_locations;
        rt->attach_host_test_fixture(hf);
        auto &g = rt->game();
        g.party.character_count = g.party.party_size = 1;
        auto &ch = g.party.characters[0];
        std::snprintf(ch.name, sizeof(ch.name), "Avatar");
        ch.current_hp = ch.max_hp = 200; ch.status = 'G'; ch.party_status = 0; ch.character_class = 'A';
        ch.dexterity = 30;
        g.party.active_character = 255;
        g.time.hour = uint8_t(hour);
        g.torch_turns = 500; g.torches = 5;
        // Stand on Lord British's Castle's own overworld tile (location_at
        // returns index + 1, so table slot 16 is location 17).
        g.position.map = {0, 0};
        g.position.xy = {g_owners->location_x[kCastle - 1], g_owners->location_y[kCastle - 1]};
    }
    bool key(uint8_t code) {
        tdeck::RawInputEvent raw{};
        raw.kind = tdeck::RawInputKind::Keyboard;
        raw.code = code;
        raw.transition = tdeck::KeyTransition::Pressed;
        raw.timestamp_us = (clock_us += 100000);
        return rt->handle(raw);
    }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    const std::vector<QuestObject> &pool() const { return rt->objects_for_test(); }

    int count_at(const Cell &c, int16_t floor, bool chest) const {
        int n = 0;
        for (const auto &o : pool())
            if (o.location == kCastle && o.floor == floor && o.x == c.x && o.y == c.y &&
                (chest ? o.chest && o.tile == 257 : o.prop && o.tile == 256 + c.type))
                ++n;
        return n;
    }
    bool vault_in_pool() const {
        for (const auto &c : kChests) if (count_at(c, kBasement, true) != 1) return false;
        return count_at(kControl, kBasement, false) == 1;
    }
    void dump(const char *label) const {
        std::printf("       [%s] player=L%u/F%d %u,%u pool=%zu\n", label,
                    unsigned(rt->game().position.map.location), int(rt->game().position.map.floor),
                    unsigned(rt->game().position.xy.x), unsigned(rt->game().position.xy.y), pool().size());
        for (const auto &o : pool())
            if (o.location == kCastle && o.floor == kBasement)
                std::printf("         obj floor=%ld x=%ld y=%ld tile=%ld chest=%d prop=%d\n", long(o.floor),
                            long(o.x), long(o.y), long(o.tile), o.chest, o.prop);
    }
    // AlphaRuntime::render()'s own world arm: get_active_map(resources_.world,
    // position.map) then compose_world_presentation(context_, ...). The party
    // stands inside the vault so all three chests are in the 11x11 window.
    bool vault_painted() {
        auto &g = rt->game();
        g.position.xy = {15, 22};
        const auto active = get_active_map(g_owners->world, g.position.map);
        if (active.error != Error::None) return false;
        const auto s = compose_world_presentation(ctx(), active.value, g.position.xy, 0x11c);
        bool ok = true;
        for (const auto &c : kChests) {
            const int at = (c.y - 22 + 5) * kPresentationWindow + (c.x - 15 + 5);
            std::printf("         snapshot (%d,%d) visible=%d tile=%d\n", c.x, c.y, s.visible[at], s.tiles[at]);
            ok &= s.tiles[at] == 257;
        }
        return ok;
    }
};

// T1: the shipped npcs.bin, as the production loader indexes it.
void test_source_table() {
    std::printf("T1 source table (production AlphaResourcePack, shipped npcs.bin)\n");
    const auto &t = g_owners->npc_locations[kCastle - 1];
    expect(t.location == kCastle && t.slots && t.count == 32, "T1a", "location 17 table is present with 32 slots");
    auto authored = [&](const Cell &c) {
        for (size_t i = 0; i < t.count; ++i) {
            const auto &n = t.slots[i];
            if (n.slot && n.type == c.type && n.x[0] == c.x && n.y[0] == c.y && n.z[0] == 0xFF &&
                n.x[1] == c.x && n.y[1] == c.y && n.x[2] == c.x && n.y[2] == c.y)
                return true;
        }
        return false;
    };
    bool chests = true;
    for (const auto &c : kChests) chests &= authored(c);
    expect(chests, "T1b", "three type-1 chests authored at (16,21) (17,22) (13,23), z=0xFF, every schedule");
    expect(authored(kControl), "T1c", "type-30 control object authored at (9,9), z=0xFF");
}

// T2: the real walk-in route, keys only.
void test_enter_and_klimb(int hour) {
    std::printf("T2 (E)nter the castle, (K)limb down -- hour %d\n", hour);
    Harness h(hour);
    h.key('e');
    const auto &g = h.rt->game();
    expect(g.position.map.location == kCastle && g.position.map.floor == 0, "T2a", "(E)nter lands on floor 0");
    expect(h.vault_in_pool(), "T2b", "entry hydrated the three basement chests + control into AlphaRuntime's pool");
    // The authored down-ladder: tile 201 on floor 0 above a 200 on floor -1.
    const auto *ground = small_map(0), *below = small_map(kBasement);
    int lx = -1, ly = -1;
    for (int i = 0; ground && below && i < 1024 && lx < 0; ++i)
        if (ground->tiles[i] == 201 && below->tiles[i] == 200) { lx = i % 32; ly = i / 32; }
    expect(lx >= 0, "T2c", "authored floor-0 ladder down to the basement found in smallmaps.bin");
    if (lx < 0) return;
    h.rt->game().position.xy = {uint8_t(lx), uint8_t(ly)};
    h.key('k');
    expect(g.position.map.location == kCastle && g.position.map.floor == kBasement, "T2d", "(K)limb reaches floor -1");
    const bool pooled = h.vault_in_pool();
    expect(pooled, "T2e", "pool still holds all three chests + control after the klimb");
    if (!pooled) h.dump("T2e");
    expect(h.vault_painted(), "T2f", "first basement snapshot paints tile 257 on all three chest cells");
}

// T3: the Developer Teleport screen, straight into the basement.
void test_debug_teleport(bool standard_entry) {
    std::printf("T3 Developer Teleport -> Lord British's Castle, %s\n",
                standard_entry ? "Default Entrance (floor 0)" : "manual basement cell");
    Harness h(12);
    DebugTeleportRequest r;
    r.kind = DebugDestinationKind::SmallMap;
    r.location = kCastle;
    r.floor = standard_entry ? 0 : kBasement;
    r.x = 12; r.y = 7;
    r.standard_entry = standard_entry;
    const auto out = apply_debug_teleport(h.ctx(), r);
    expect(out.status == DebugTeleportStatus::Applied, "T3a", "teleport applied");
    const bool pooled = h.vault_in_pool();
    expect(pooled, "T3b", "teleport hydrated the three basement chests + control into the pool");
    if (!pooled) h.dump("T3b");
    h.rt->game().position.map.floor = kBasement;
    expect(h.vault_painted(), "T3c", "basement snapshot paints tile 257 on all three chest cells");
    // A second teleport to the same place must not stack a second vault.
    apply_debug_teleport(h.ctx(), r);
    int total = 0;
    for (const auto &c : kChests) total += h.count_at(c, kBasement, true);
    expect(total == 3, "T3d", "re-teleporting leaves exactly three chests (no duplicates)");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: batch22_basement_objects_test <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;

    test_source_table();
    test_enter_and_klimb(12);
    test_enter_and_klimb(3);
    test_debug_teleport(false);
    test_debug_teleport(true);

    std::printf("\nbatch22_basement_objects: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
