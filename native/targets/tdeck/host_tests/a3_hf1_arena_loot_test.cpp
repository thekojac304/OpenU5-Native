// Alpha 3 gameplay hotfix A3-HF1 -- the troll-encounter reward chest whose
// loot "Get" answered "Nothing to get!", through the REAL AlphaRuntime.
//
//   a3_hf1_arena_loot <openu5-alpha1-resources.bin> [--scan]
//
// Hardware report (Alpha 3 build, A3-04B): a random troll encounter by the
// bridge near Britain, won normally; a chest appeared, it opened, and every
// Get toward it answered "Nothing to get!".
//
// The flow is the device's own, end to end: the shipped pack's Britannia map,
// the real bridge nearest Britain, a roaming troll (enemy 41) standing next to
// the party, a raw-key Pass that lets outdoor_tick() launch the encounter
// through outdoor_start(), the fight fought with raw keys (a + trackball
// reticle + Enter), the chest the real Engine::kill() drops, then (O)pen and
// (G)et by raw key + trackball direction, exactly as on the T-Deck. The toll
// entry (TrollSneak preamble -> "Pay toll?" -> n) is driven the same way.
//
// The two inventory arms share one seed, one fight, one chest and one loot
// list. Both start from the Developer "Combat" preset (a strong party, so the
// fight is short). NORMAL then puts the counters back to ordinary values;
// MAXED keeps the preset's 99 / 9999 -- the state the Developer "Combat" and
// "Stocked Inventory" presets leave every counter a chest can grant in.
//
// Original (SJOG.OVL get_item_switch 0x1458): every item arm adds through the
// saturating helpers (ULTIMA.EXE 0x3ef0 add_byte_capped / 0x3f14
// add_word_capped, or an inline inc + clamp to 0x63) and then ALWAYS clears
// the object slot (0x177a -> call 0x7af4). Only the jump-table default
// (0x1750, "Nothing to get!" DS 0x8d92, unknown type) and the chest arm
// (0x1482, "Open it first!") leave the object where it is.
//
//   L  the encounter, the fight, the chest, the Open (the report's preconditions)
//   N  NORMAL inventory: one item per Get, the pile empties
//   M  MAXED inventory: one item per Get, named, the counter stays at its cap,
//      the pile empties -- RED before A3-HF1 ("Nothing to get!" every time,
//      the pile never shrinks)
//   T  the toll entry (TrollSneak -> refuse) shares the reward path exactly
//   P  save/load and power cycle around the arena (no schema change)
//   E  edge cases: each side, near-full, unknown record, nested chest, no
//      reward, leaving loot behind, other terrain, repeated encounter
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/combat.h"
#include "openu5/debug_developer.h"
#include "openu5/loot.h"
#include "openu5/outdoor.h"
#include "openu5/world.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;

void batch37_reset_screen();

namespace {
int checks = 0, failures = 0;
void check(bool good, const char *label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label);
}
const tdeck::AlphaResourceOwners *pack = nullptr;
size_t g_dungeon_count = 0;
constexpr int64_t kClockStartUs = 5'000'000;

// Pinned by --scan (see main): on the shipped pack, seed 2 is a one-troll
// roaming fight whose chest (untrapped, contents 15) opens into seven items
// -- three equipment, gold, keys, torches, food. kTollSeed is the first seed
// whose step onto the bridge fires the troll and whose refused toll fight
// drops a chest (a clumsy Avatar; see encounter()). kQuietSeed wins with no
// chest at all.
constexpr uint32_t kRoamingSeed = 2, kTollSeed = 113, kQuietSeed = 5;

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    explicit Run(uint32_t seed) {
        openu5_host_virtual_clock_us() = kClockStartUs;
        batch37_reset_screen();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.dungeons = pack->dungeons;
        f.dungeon_count = g_dungeon_count;
        f.enemy_defs = pack->combat_enemy_views;
        f.enemy_def_count = pack->combat_enemy_count;
        f.render_pixels = true;
        f.indexed_test_tiles = true;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.time.hour = 12;
        g.time.minute = 0;
        g.party.character_count = g.party.party_size = 1;
        g.party.active_character = 255;
        auto &m = g.party.characters[0];
        std::snprintf(m.name, sizeof(m.name), "Avatar");
        m.status = 'G';
        m.character_class = 'A';
        m.level = 1;
        m.current_hp = m.max_hp = 100;
        m.strength = m.dexterity = m.intelligence = 20;
        g.rng.seed(seed);
        rt->render(board, true);
    }
    int64_t now() const { return openu5_host_virtual_clock_us(); }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = now();
        rt->handle(e);
    }
    void key(uint8_t code, bool alt = false) {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = code;
        e.modifiers.alt = alt;
        raw(e);
        rt->render(board);
    }
    void ball(RawInputKind k) {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
        rt->render(board);
    }
    void run(int64_t ms) {
        for (int64_t t = 0; t < ms; t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
        }
    }
    // The transcript is a ring: once full, old blocks fall off the front. A
    // mark is the whole block list, and since() is what was appended after it
    // (the shortest shift that lines the old tail up with the new head).
    std::vector<std::string> mark() const {
        std::vector<std::string> out;
        for (size_t i = 0; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i)) out.push_back(b->text);
        return out;
    }
    std::string since(const std::vector<std::string> &before) const {
        const auto after = mark();
        size_t shift = before.size();
        for (size_t k = 0; k <= before.size(); ++k)
            if (before.size() - k <= after.size() && std::equal(before.begin() + ptrdiff_t(k), before.end(), after.begin())) {
                shift = k;
                break;
            }
        std::string out;
        for (size_t i = before.size() - shift; i < after.size(); ++i) out += after[i] + "\n";
        return out;
    }
    std::string transcript() const { return since({}); }
    CombatState &cs() { return rt->combat_state_for_test(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    GameState &game() { return rt->game(); }
    bool combat() { return ctx().combat && cs().initialized; }
    UiMode mode() const { return rt->ui()->mode(); }
    /** The acting party member, once the runtime has handed the turn to one (else null). */
    CombatActor *player_turn() {
        auto &s = cs();
        if (!combat() || s.current < 0 || s.current >= s.count) return nullptr;
        auto &a = s.actors[s.current];
        if (a.enemy || a.member == 255 || a.status != CombatStatus::Active) return nullptr;
        return mode() == UiMode::Combat ? &a : nullptr;
    }
    /** Let the enemy beats run until a party member holds the turn (or the arena closes). */
    CombatActor *settle(int budget_ms = 20000) {
        for (int t = 0; t < budget_ms && combat(); t += 50) {
            if (auto *a = player_turn()) return a;
            openu5_host_virtual_clock_us() += 50000;
            rt->render(board);
        }
        return player_turn();
    }
    void dir(int dx, int dy) {
        ball(dx > 0 ? RawInputKind::TrackballRight
             : dx < 0 ? RawInputKind::TrackballLeft
             : dy > 0 ? RawInputKind::TrackballDown
                      : RawInputKind::TrackballUp);
    }
    bool teleport(int x, int y) {
        DebugTeleportRequest r{};
        r.kind = DebugDestinationKind::Britannia;
        r.x = x;
        r.y = y;
        const bool ok = apply_debug_teleport(ctx(), r).status == DebugTeleportStatus::Applied;
        rt->render(board, true);
        return ok;
    }
    int piles_at(int x, int y) {
        int n = 0;
        for (int i = 0; i < cs().pile_count; ++i) n += cs().piles[i].position.x == x && cs().piles[i].position.y == y;
        return n;
    }
    LootGrant top_at(int x, int y) {
        for (int i = cs().pile_count - 1; i >= 0; --i)
            if (cs().piles[i].position.x == x && cs().piles[i].position.y == y) return {cs().piles[i].id, cs().piles[i].quantity};
        return {-1, -1};
    }
};

int enemies_alive(const CombatState &s) {
    int n = 0;
    for (int i = 0; i < s.count; ++i)
        if (s.actors[i].enemy && (s.actors[i].status == CombatStatus::Active || s.actors[i].status == CombatStatus::Sleeping)) ++n;
    return n;
}
bool arena_free(const CombatState &s, int x, int y) {
    if (x < 0 || y < 0 || x >= kCombatGrid || y >= kCombatGrid) return false;
    const int k = y * kCombatGrid + x;
    if (s.loot[k] == 1 || s.loot[k] == 129) return false;
    for (int i = 0; i < s.count; ++i)
        if (s.actors[i].position.x == x && s.actors[i].position.y == y &&
            (s.actors[i].status == CombatStatus::Active || s.actors[i].status == CombatStatus::Sleeping))
            return false;
    return true;
}

/** The real overworld bridge nearest Britain, and the land cell beside it on the bridge's axis. */
struct Spot {
    int x = -1, y = -1, land_x = -1, land_y = -1, tile = -1, britain_x = -1, britain_y = -1, distance = -1;
};
Spot find_bridge(CommandContext &ctx) {
    Spot out;
    int bx = -1, by = -1;
    for (int x = 0; x < 256 && bx < 0; ++x)
        for (int y = 0; y < 256; ++y)
            if (location_at(ctx.locations, uint8_t(x), uint8_t(y)) == 2) { bx = x; by = y; break; }
    out.britain_x = bx;
    out.britain_y = by;
    if (bx < 0) return out;
    const auto m = get_active_map(pack->world, MapId{LocationId(0), 0});
    if (m.error != Error::None) return out;
    int best = 1 << 30;
    for (int y = by - 24; y <= by + 24; ++y)
        for (int x = bx - 24; x <= bx + 24; ++x) {
            const int t = m.value.tile_at(x & 255, y & 255);
            if (t != 0x6a && t != 0x6b) continue;
            const int nx[] = {x + 1, x - 1, x, x}, ny[] = {y, y, y + 1, y - 1};
            for (int k = 0; k < 4; ++k) {
                const int n = m.value.tile_at(nx[k] & 255, ny[k] & 255);
                if (n < 0 || n == 0x6a || n == 0x6b || !is_passable(n, TransportMode::Foot).value) continue;
                const int d = (x - bx) * (x - bx) + (y - by) * (y - by);
                if (d < best) {
                    best = d;
                    out.x = x & 255; out.y = y & 255; out.tile = t;
                    out.land_x = nx[k] & 255; out.land_y = ny[k] & 255;
                }
            }
        }
    if (out.x >= 0) out.distance = std::max(std::abs(out.x - bx), std::abs(out.y - by));
    return out;
}
/** Two orthogonally adjacent plain-grass cells near Britain, off any bridge (edge case 6). */
Spot find_grass(const Spot &near) {
    Spot out;
    const auto m = get_active_map(pack->world, MapId{LocationId(0), 0});
    if (m.error != Error::None) return out;
    for (int r = 2; r < 20 && out.x < 0; ++r)
        for (int y = near.britain_y - r; y <= near.britain_y + r && out.x < 0; ++y)
            for (int x = near.britain_x - r; x <= near.britain_x + r && out.x < 0; ++x)
                if (m.value.tile_at(x & 255, y & 255) == 5 && m.value.tile_at((x + 1) & 255, y & 255) == 5) {
                    out.x = x & 255; out.y = y & 255; out.land_x = (x + 1) & 255; out.land_y = y & 255; out.tile = 5;
                }
    return out;
}

struct Counters {
    int gold, food, keys, gems, torches;
    int potions[8], scrolls[8], equipment[48];
};
Counters counters(const GameState &g) {
    Counters s{g.gold, g.food, g.keys, g.gems, g.torches, {}, {}, {}};
    for (int i = 0; i < 8; ++i) { s.potions[i] = g.potion_quantities[i]; s.scrolls[i] = g.scroll_quantities[i]; }
    for (int i = 0; i < 48; ++i) s.equipment[i] = g.equipment_quantities[i];
    return s;
}
/** The counter one loot record lands in, and by how much (SJOG 0x1458's arms). */
int *field(Counters &s, LootGrant v, int &add, int &cap) {
    cap = 99;
    add = 1;
    switch (v.id) {
    case 2: cap = 9999; add = v.quantity; return &s.gold;
    case 15: cap = 9999; add = v.quantity; return &s.food;
    case 7: add = v.quantity & 127; return &s.keys;
    case 8: add = v.quantity; return &s.gems;
    case 13: add = v.quantity; return &s.torches;
    case 3: return v.quantity >= 0 && v.quantity < 8 ? &s.potions[v.quantity] : nullptr;
    case 4: return &s.scrolls[v.quantity & 7];
    case 5: case 6: case 9: case 10: case 11: case 12:
        add = v.quantity == 27 || v.quantity == 29 ? 5 : 1;
        return v.quantity >= 0 && v.quantity < 48 ? &s.equipment[v.quantity] : nullptr;
    default: return nullptr;
    }
}
bool same_counters(const Counters &a, const Counters &b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }

enum class Entry { Roaming, Toll };
enum class Stock { Normal, Maxed };

struct Outcome {
    bool bridge = false, attacked = false, trolls = false, won = false, lingering = false;
    int chest = -1, chest_x = -1, chest_y = -1, chest_encoded = 0, chest_contents = -1;
    bool opened = false, found_line = false;
    std::vector<LootGrant> loot; // at the chest cell after Open, bottom -> top
    int gets = 0, refusals = 0, taken = 0, piles_left = -1;
    bool deltas_ok = true, names_ok = true, others_untouched = true;
    std::string log;
};

bool adjacent_to(const CombatActor &a, int cx, int cy, int &dx, int &dy) {
    dx = cx - a.position.x;
    dy = cy - a.position.y;
    return (std::abs(dx) == 1 && dy == 0) || (dx == 0 && std::abs(dy) == 1);
}
/** One greedy trackball step toward the nearest free orthogonal neighbour of (cx,cy). */
void step_toward(Run &h, const CombatActor &a, int cx, int cy) {
    int best = 1 << 30, gx = -1, gy = -1;
    const int nx[] = {cx - 1, cx + 1, cx, cx}, ny[] = {cy, cy, cy - 1, cy + 1};
    for (int k = 0; k < 4; ++k) {
        if (!arena_free(h.cs(), nx[k], ny[k])) continue;
        const int d = std::abs(nx[k] - a.position.x) + std::abs(ny[k] - a.position.y);
        if (d < best) { best = d; gx = nx[k]; gy = ny[k]; }
    }
    if (gx < 0) { h.key(' '); return; }
    const int dx = gx - a.position.x, dy = gy - a.position.y;
    if (dx && arena_free(h.cs(), a.position.x + (dx > 0 ? 1 : -1), a.position.y)) h.dir(dx, 0);
    else if (dy && arena_free(h.cs(), a.position.x, a.position.y + (dy > 0 ? 1 : -1))) h.dir(0, dy);
    else if (dx) h.dir(dx, 0);
    else h.key(' ');
}

/** Fight the arena with raw keys: approach the nearest troll, (A)ttack it through the reticle. */
bool fight(Run &h) {
    for (int turn = 0; turn < 400 && h.combat() && enemies_alive(h.cs()); ++turn) {
        auto *a = h.settle();
        if (!a || !enemies_alive(h.cs())) break;
        const CombatActor *target = nullptr;
        int best = 1 << 30;
        for (int i = 0; i < h.cs().count; ++i) {
            const auto &e = h.cs().actors[i];
            if (!e.enemy || e.status != CombatStatus::Active) continue;
            const int d = std::max(std::abs(e.position.x - a->position.x), std::abs(e.position.y - a->position.y));
            if (d < best) { best = d; target = &e; }
        }
        if (!target) break;
        if (best <= std::max<int>(1, a->range)) {
            const int tx = target->position.x, ty = target->position.y;
            h.key('a');
            if (h.mode() != UiMode::TargetSelection) return false;
            int rx = a->position.x, ry = a->position.y;
            while (rx != tx) { h.dir(tx > rx ? 1 : -1, 0); rx += tx > rx ? 1 : -1; }
            while (ry != ty) { h.dir(0, ty > ry ? 1 : -1); ry += ty > ry ? 1 : -1; }
            h.key('\r');
        } else
            step_toward(h, *a, target->position.x, target->position.y);
    }
    return h.combat() && !enemies_alive(h.cs());
}

void set_stock(GameState &g, Stock stock) {
    if (stock == Stock::Maxed) return; // the Combat preset's own 99 / 9999
    g.gold = 400; g.food = 200; g.keys = 3; g.gems = 2; g.torches = 4;
    for (auto &v : g.potion_quantities) v = 1;
    for (auto &v : g.scroll_quantities) v = 1;
    for (int i = 0; i < 48; ++i) g.equipment_quantities[i] = 1;
}
int troll_tile(CommandContext &ctx) {
    for (size_t i = 0; i < ctx.outdoor->resources->enemy_count; ++i)
        if (const auto *d = ctx.outdoor->resources->enemies[i]; d && d->index == 41) return d->tile;
    return -1;
}
/** A roaming troll next to the party, as outdoor_tick's spawner leaves one; a Pass lets it attack. */
bool roaming_attack(Run &h, int troll_x, int troll_y) {
    auto &ctx = h.ctx();
    ctx.outdoor->enemies.clear();
    OutdoorEnemy troll{};
    troll.definition = 41;
    troll.tile = troll_tile(ctx);
    troll.x = troll_x;
    troll.y = troll_y;
    ctx.outdoor->enemies.push_back(troll);
    h.key(' ');
    h.run(1500);
    return h.combat();
}

/** Encounter + fight + locate the chest. Leaves `h` in the lingering victory arena. */
Outcome encounter(Run &h, Entry entry, Stock stock, const Spot &spot) {
    Outcome o;
    auto &ctx = h.ctx();
    if (entry == Entry::Roaming) {
        o.bridge = spot.x >= 0 && h.teleport(spot.x, spot.y);
        if (!o.bridge) return o;
        apply_debug_preset(ctx, DebugPreset::Combat);
        set_stock(h.game(), stock);
        h.rt->render(h.board, true);
        o.attacked = roaming_attack(h, spot.land_x, spot.land_y);
    } else {
        // Step from the land onto the bridge: outdoor_turn rolls the trolls
        // (TrollSneak preamble), then "Pay toll?" -- refused with 'n'.
        o.bridge = spot.x >= 0 && h.teleport(spot.land_x, spot.land_y);
        if (!o.bridge) return o;
        apply_debug_preset(ctx, DebugPreset::Combat);
        set_stock(h.game(), stock);
        // The preset maxes Dexterity to 30, and the 1988 sneak roll (turn.cpp,
        // rand(1,30) > dex) can never beat 30 -- a maxed party always evades.
        // A clumsy Avatar can be caught; everything else stays the preset's.
        h.game().party.characters[0].dexterity = 1;
        ctx.outdoor->enemies.clear();
        h.rt->render(h.board, true);
        h.dir(spot.x - spot.land_x, spot.y - spot.land_y);
        h.run(4000);
        if (!ctx.commands.awaiting_troll) return o;
        h.key('n');
        h.run(1500);
        o.attacked = h.combat();
    }
    if (!o.attacked) return o;
    o.trolls = true;
    for (int i = 0; i < h.cs().count; ++i)
        if (h.cs().actors[i].enemy) o.trolls = o.trolls && h.cs().actors[i].enemy->index == 41;
    o.won = fight(h);
    o.lingering = h.combat() && h.cs().victory && !h.cs().ended;
    if (!o.won) return o;
    for (int k = 0; k < kCombatCells; ++k)
        if ((h.cs().loot[k] == 1 || h.cs().loot[k] == 129) && h.cs().chest_state[k] == CombatChestState::Unopened) {
            o.chest = k; o.chest_x = k % kCombatGrid; o.chest_y = k / kCombatGrid;
            o.chest_encoded = h.cs().loot[k]; o.chest_contents = h.cs().chest_contents[k];
            break;
        }
    return o;
}
/** (O)pen from whichever side the acting member reaches first. */
void open_chest(Run &h, Outcome &o) {
    const auto m = h.mark();
    for (int turn = 0; turn < 60 && h.combat() && !o.opened; ++turn) {
        auto *a = h.settle();
        if (!a) break;
        int dx, dy;
        if (adjacent_to(*a, o.chest_x, o.chest_y, dx, dy)) {
            h.key('o');
            h.dir(dx, dy);
            o.opened = h.cs().chest_state[o.chest] == CombatChestState::Consumed;
        } else
            step_toward(h, *a, o.chest_x, o.chest_y);
    }
    o.found_line = h.since(m).find("Found:") != std::string::npos;
    for (int i = 0; i < h.cs().pile_count; ++i)
        if (h.cs().piles[i].position.x == o.chest_x && h.cs().piles[i].position.y == o.chest_y)
            o.loot.push_back({h.cs().piles[i].id, h.cs().piles[i].quantity});
}
/** (G)et until the pile at the chest is gone, or `max_gets` tries. */
void get_all(Run &h, Outcome &o, int max_gets) {
    for (int turn = 0; turn < 160 && h.combat() && o.gets < max_gets; ++turn) {
        const int here = h.piles_at(o.chest_x, o.chest_y);
        if (!here) break;
        auto *a = h.settle();
        if (!a) break;
        int dx, dy;
        if (!adjacent_to(*a, o.chest_x, o.chest_y, dx, dy)) { step_toward(h, *a, o.chest_x, o.chest_y); continue; }
        const LootGrant top = h.top_at(o.chest_x, o.chest_y);
        auto before = counters(h.game());
        const auto m = h.mark();
        h.key('g');
        h.dir(dx, dy);
        ++o.gets;
        const int after_here = h.piles_at(o.chest_x, o.chest_y);
        const auto said = h.since(m);
        const auto after = counters(h.game());
        const bool refused = said.find("Nothing to get!") != std::string::npos;
        o.log += "    Get: top {" + std::to_string(top.id) + "," + std::to_string(top.quantity) + "} -> " +
                 (refused ? "\"Nothing to get!\"" : "taken") + ", pile " + std::to_string(here) + " -> " +
                 std::to_string(after_here) + "\n";
        if (refused) { ++o.refusals; o.others_untouched = o.others_untouched && same_counters(before, after); continue; }
        if (after_here != here - 1) { o.deltas_ok = false; continue; }
        ++o.taken;
        char name[96]{};
        loot_item_name(top, name, sizeof(name));
        o.names_ok = o.names_ok && said.find(name) != std::string::npos;
        int add = 0, cap = 0;
        int *was = field(before, top, add, cap);
        if (!was) { o.deltas_ok = false; continue; }
        *was = std::min(cap, *was + add); // the one field this record may move
        auto expect = before;
        o.deltas_ok = o.deltas_ok && same_counters(expect, after);
    }
    o.piles_left = h.piles_at(o.chest_x, o.chest_y);
}
Outcome play(uint32_t seed, Entry entry, Stock stock, int max_gets = 24) {
    Run h(seed);
    const auto spot = find_bridge(h.ctx());
    auto o = encounter(h, entry, stock, spot);
    if (o.chest < 0) return o;
    open_chest(h, o);
    if (o.opened) get_all(h, o, max_gets);
    return o;
}
void report(const char *tag, const Outcome &o) {
    std::printf("  %s: attacked=%d trolls=%d won=%d lingering=%d chest=(%d,%d) encoded=%d contents=%d opened=%d loot=%zu "
                "gets=%d refusals=%d taken=%d left=%d\n%s",
                tag, o.attacked, o.trolls, o.won, o.lingering, o.chest_x, o.chest_y, o.chest_encoded, o.chest_contents,
                o.opened, o.loot.size(), o.gets, o.refusals, o.taken, o.piles_left, o.log.c_str());
}
/** Stage one record at the chest cell with the acting member on side (sx,sy) of it, then Get toward it. */
struct Staged {
    std::string said;
    int before_here = 0, after_here = 0;
    Counters before{}, after{};
};
Staged staged_get(Run &h, int cx, int cy, int sx, int sy, LootGrant record) {
    Staged s;
    auto *a = h.settle();
    if (!a) return s;
    a->position = {int16_t(cx + sx), int16_t(cy + sy)};
    h.cs().piles[h.cs().pile_count++] = {{int16_t(cx), int16_t(cy)}, int16_t(record.id), int16_t(record.quantity)};
    h.rt->render(h.board, true);
    s.before_here = h.piles_at(cx, cy);
    s.before = counters(h.game());
    const auto m = h.mark();
    h.key('g');
    h.dir(-sx, -sy);
    s.said = h.since(m);
    s.after_here = h.piles_at(cx, cy);
    s.after = counters(h.game());
    return s;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report_out{};
    if (source.open(argv[1], report_out) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report_out) != ESP_OK) return 2;
    pack = &owners;
    g_dungeon_count = report_out.dungeon_count;

    if (argc >= 3 && std::strcmp(argv[2], "--scan") == 0) {
        for (uint32_t seed = 1; seed <= 200; ++seed) {
            const auto r = play(seed, Entry::Roaming, Stock::Normal);
            const auto t = play(seed, Entry::Toll, Stock::Normal);
            std::printf("SCAN seed %u roaming: won=%d chest=%d loot=%zu taken=%d | toll: attacked=%d won=%d chest=%d loot=%zu taken=%d\n",
                        seed, r.won, r.chest, r.loot.size(), r.taken, t.attacked, t.won, t.chest, t.loot.size(), t.taken);
        }
        return 0;
    }

    // ---- L: the report's preconditions --------------------------------------------
    {
        Run h(kRoamingSeed);
        const auto spot = find_bridge(h.ctx());
        std::printf("  Britain entrance (%d,%d); nearest bridge (%d,%d) tile 0x%02x, %d cells away; land side (%d,%d)\n",
                    spot.britain_x, spot.britain_y, spot.x, spot.y, spot.tile, spot.distance, spot.land_x, spot.land_y);
        check(spot.britain_x >= 0 && spot.x >= 0 && spot.distance <= 24,
              "L1 the shipped Britannia map has a bridge (0x6a/0x6b) near Britain's entrance");
    }
    const auto normal = play(kRoamingSeed, Entry::Roaming, Stock::Normal);
    const auto maxed = play(kRoamingSeed, Entry::Roaming, Stock::Maxed);
    report("NORMAL", normal);
    report("MAXED", maxed);
    check(normal.attacked && normal.trolls, "L2 a roaming troll next to the party attacks on a Pass (outdoor_tick -> outdoor_start, enemy 41)");
    check(normal.won && normal.lingering, "L3 the fight is won with raw keys; the victory arena lingers for its loot");
    check(normal.chest >= 0 && (normal.chest_encoded == 1 || normal.chest_encoded == 129) && normal.chest_contents == 15,
          "L4 the troll's death drops a reward chest in the arena (Engine::kill, contents = troll treasure 15)");
    check(normal.opened && normal.found_line && normal.loot.size() >= 2,
          "L5 (O)pen by raw key + direction opens it: \"Found:\" and its loot lies on the chest cell");
    check(maxed.chest == normal.chest && maxed.chest_encoded == normal.chest_encoded && maxed.opened &&
              maxed.loot.size() == normal.loot.size() &&
              std::equal(maxed.loot.begin(), maxed.loot.end(), normal.loot.begin(),
                         [](LootGrant a, LootGrant b) { return a.id == b.id && a.quantity == b.quantity; }),
          "L6 the MAXED arm is the same fight, the same chest cell and the same loot, record for record");

    // ---- N: ordinary inventory -------------------------------------------------------
    check(normal.refusals == 0 && normal.taken == int(normal.loot.size()) && normal.piles_left == 0,
          "N1 NORMAL: every Get takes one item and the pile empties");
    check(normal.taken > 0 && normal.deltas_ok && normal.names_ok, "N2 NORMAL: each Get names its item and moves exactly its own counter");

    // ---- M: Developer-maxed inventory (the hardware state) ---------------------------
    check(maxed.refusals == 0, "M1 MAXED: no Get answers \"Nothing to get!\" while loot lies there (SJOG 0x1458 has no full-pack refusal)");
    check(maxed.taken == int(maxed.loot.size()) && maxed.piles_left == 0,
          "M2 MAXED: every Get takes one item and the pile empties (0x177a clears the slot unconditionally)");
    check(maxed.taken > 0 && maxed.deltas_ok && maxed.names_ok && maxed.others_untouched,
          "M3 MAXED: each item is named and its counter stays at the cap (add_byte_capped 0x3ef0 / add_word_capped 0x3f14)");

    // ---- T: the toll entry (TrollSneak preamble) -------------------------------------
    {
        const auto tn = play(kTollSeed, Entry::Toll, Stock::Normal);
        const auto tm = play(kTollSeed, Entry::Toll, Stock::Maxed);
        report("TOLL NORMAL", tn);
        report("TOLL MAXED", tm);
        check(tn.attacked && tn.trolls && tn.won && tn.chest >= 0 && tn.opened,
              "T1 the bridge toll (TrollSneak -> \"Pay toll?\" -> n) fights the same trolls and drops a chest the same way");
        check(tn.taken == int(tn.loot.size()) && tn.piles_left == 0 && tn.refusals == 0, "T2 toll entry, NORMAL: every item is taken");
        check(tm.refusals == 0 && tm.taken == int(tm.loot.size()) && tm.piles_left == 0 && tm.deltas_ok,
              "T3 toll entry, MAXED: every item is taken at the cap -- the same Get, the same defect, the same fix");
    }

    // ---- P: save / load / power cycle around the arena -------------------------------
    {
        Run h(kRoamingSeed);
        const auto spot = find_bridge(h.ctx());
        auto o = encounter(h, Entry::Roaming, Stock::Maxed, spot);
        // P1: save in the victory arena with the chest still closed, load it back.
        h.key('s', true);
        const bool saved = h.transcript().find("Save complete") != std::string::npos;
        h.key('l', true);
        h.run(500);
        const bool arena_after_load = h.combat();
        const bool chest_after_load = o.chest >= 0 && h.combat() && h.cs().chest_state[o.chest] == CombatChestState::Unopened &&
                                      (h.cs().loot[o.chest] == 1 || h.cs().loot[o.chest] == 129);
        std::printf("  P: saved=%d arena live after load=%d chest still closed=%d world objects=%zu\n", saved, arena_after_load,
                    chest_after_load, h.rt->objects_for_test().size());
        check(saved && arena_after_load == chest_after_load, "P1 save before Open -> load: the arena is either kept whole or not at all");
        if (chest_after_load) {
            open_chest(h, o);
            // P2: save with the loot on the ground, load, and take it all.
            h.key('s', true);
            h.key('l', true);
            h.run(500);
            const int here = h.combat() ? h.piles_at(o.chest_x, o.chest_y) : -1;
            get_all(h, o, 24);
            check(here == int(o.loot.size()) && o.piles_left == 0 && o.refusals == 0,
                  "P2 save after Open -> load: the loot is still on the ground and every item is still collectible");
        } else
            check(h.rt->objects_for_test().empty(), "P2 an arena the save does not keep leaves no phantom chest or loot in the world");
    }
    {
        // P3 / E8 / E9: take the loot, leave, save; load; then a new runtime (power cycle) loads it.
        Run h(kRoamingSeed);
        const auto spot = find_bridge(h.ctx());
        auto o = encounter(h, Entry::Roaming, Stock::Normal, spot);
        open_chest(h, o);
        get_all(h, o, 24);
        h.key('\b'); // Back: leave the victory arena
        h.run(2000);
        const auto kept = counters(h.game());
        const size_t objects = h.rt->objects_for_test().size();
        h.key('s', true);
        h.key('l', true);
        h.run(500);
        check(!h.combat() && same_counters(kept, counters(h.game())) && h.rt->objects_for_test().size() == objects,
              "E8 load after the victory: the taken loot stays in the pack, no chest or loot reappears in the world");
        Run cold(99);
        cold.key('l', true);
        cold.run(500);
        check(same_counters(kept, counters(cold.game())) && cold.rt->objects_for_test().size() == objects && !cold.combat(),
              "E9 power cycle (fresh runtime + Load): the same pack, no duplicate reward, no phantom chest");
    }

    // ---- E: edge cases -------------------------------------------------------------
    {
        Run h(kRoamingSeed);
        const auto spot = find_bridge(h.ctx());
        auto o = encounter(h, Entry::Roaming, Stock::Maxed, spot);
        open_chest(h, o);
        // clear the real loot so each staged record is alone on the cell
        h.cs().pile_count = 0;
        const int cx = o.chest_x, cy = o.chest_y;
        const int sides[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        int sides_ok = 0, sides_tried = 0;
        for (const auto &sd : sides) {
            if (!arena_free(h.cs(), cx + sd[0], cy + sd[1]) && !(h.player_turn() && h.player_turn()->position.x == cx + sd[0] &&
                                                                h.player_turn()->position.y == cy + sd[1]))
                continue;
            ++sides_tried;
            const auto s = staged_get(h, cx, cy, sd[0], sd[1], {8, 1});
            sides_ok += s.after_here == s.before_here - 1 && s.after.gems == 99 && s.said.find("1 gem!") != std::string::npos;
        }
        std::printf("  E1: %d of %d free sides\n", sides_ok, sides_tried);
        check(sides_tried >= 2 && sides_ok == sides_tried, "E1 Get from each free side of the chest takes the item (gems at 99 stay 99)");

        h.game().gems = 98;
        auto s = staged_get(h, cx, cy, -1, 0, {8, 2});
        check(s.after_here == s.before_here - 1 && s.after.gems == 99 && s.said.find("2 gems!") != std::string::npos,
              "E2 near-full: 98 gems + 2 gems -> 99, the item is taken and named");

        s = staged_get(h, cx, cy, -1, 0, {0, 1});
        check(s.after_here == s.before_here && s.said.find("Nothing to get!") != std::string::npos && same_counters(s.before, s.after),
              "E3 an unknown record still answers \"Nothing to get!\" and stays (the 0x1458 jump-table default, 0x1750)");
        h.cs().pile_count = 0;

        s = staged_get(h, cx, cy, -1, 0, {1, 5});
        check(s.after_here == s.before_here && s.said.find("Open it first!") != std::string::npos && same_counters(s.before, s.after),
              "E4 a nested chest on top still answers \"Open it first!\" and stays (0x1482)");
        h.cs().pile_count = 0;

        h.game().gold = 9990;
        s = staged_get(h, cx, cy, -1, 0, {2, 38});
        check(s.after_here == s.before_here - 1 && s.after.gold == 9999 && s.said.find("38 gold!") != std::string::npos,
              "E5 gold that overflows the cap: 9990 + 38 -> 9999, taken and named");

        // leaving loot on the ground: Back ends the arena, nothing is granted or promoted
        h.cs().piles[h.cs().pile_count++] = {{int16_t(cx), int16_t(cy)}, 8, 1};
        const auto before_exit = counters(h.game());
        const size_t objects = h.rt->objects_for_test().size();
        h.key('\b');
        h.run(2000);
        check(!h.combat() && same_counters(before_exit, counters(h.game())) && h.rt->objects_for_test().size() == objects,
              "E6 leaving with loot still on the ground: nothing is granted and nothing is promoted to the world (R-03)");
    }
    {
        const auto q = play(kQuietSeed, Entry::Roaming, Stock::Maxed);
        check(q.won && q.chest < 0 && q.loot.empty(), "E7 a victory with no reward drops no chest and no loot");
    }
    {
        // other terrain: plain grass near Britain, no bridge
        Run h(kRoamingSeed);
        const auto grass = find_grass(find_bridge(h.ctx()));
        Outcome o;
        o.bridge = grass.x >= 0 && h.teleport(grass.x, grass.y);
        apply_debug_preset(h.ctx(), DebugPreset::Combat);
        h.rt->render(h.board, true);
        o.attacked = o.bridge && roaming_attack(h, grass.land_x, grass.land_y);
        o.won = o.attacked && fight(h);
        // repeated encounters at the same spot until one drops a chest
        int fights = 0;
        for (; fights < 8; ++fights) {
            if (fights) {
                if (h.combat()) {
                    h.key('\b');
                    h.run(2000);
                }
                h.teleport(grass.x, grass.y); // the same spot every time
                o.attacked = roaming_attack(h, grass.land_x, grass.land_y);
                o.won = o.attacked && fight(h);
            }
            o.chest = -1;
            for (int k = 0; k < kCombatCells && o.won; ++k)
                if ((h.cs().loot[k] == 1 || h.cs().loot[k] == 129) && h.cs().chest_state[k] == CombatChestState::Unopened) {
                    o.chest = k; o.chest_x = k % kCombatGrid; o.chest_y = k / kCombatGrid;
                    break;
                }
            if (o.chest >= 0) break;
        }
        if (o.chest >= 0) { open_chest(h, o); get_all(h, o, 24); }
        std::printf("  E10: grass (%d,%d), %d fight(s) before a chest\n", grass.x, grass.y, fights + 1);
        report("GRASS MAXED", o);
        check(o.won && o.chest >= 0 && o.opened && o.refusals == 0 && o.taken == int(o.loot.size()) && o.piles_left == 0,
              "E10 other terrain (grass) and repeated encounters at one spot: the maxed pack takes every item");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
