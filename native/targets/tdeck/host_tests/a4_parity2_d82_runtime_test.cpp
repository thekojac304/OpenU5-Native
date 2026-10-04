// Alpha 4 A4-PARITY2 D-82 (targets/tdeck/ALPHA4_UI.md section 16) -- the New Journey's underworld seed, through the REAL
// AlphaRuntime (real Board, raw keys, the real alpha_save.cpp over the fake SD card) plus the core function itself.
//
// Derivation (native/core/a4-parity2-findings/D82-FINAL.md): a new journey's SAVED.OOL is `zeros(256) ++ INIT.OOL`
// (FONT.OVL 0x0e1f / INTRO 0x1dea) -- INIT.OOL is the raw image of the UNDER block's object table (32 records of 8
// bytes: +0 tile byte, +2 x, +3 y, +4 floor, +5 hull, +7 skiffs). The shipped INIT.OOL holds five records, all at
// floor 0xFF: slot 23 a skiff (0x29) at (14,242), slots 24..27 four bodies (0x1e) beside the Amulet's cell. Native
// started every journey with an EMPTY underworld table (the pack's init.ool carried the data and nothing read it).
//
//   N1 the pack's initial_ool is exactly `zeros(256) ++ INIT.OOL` (and the file itself, when the install is given)
//   N2 a New Journey seeds the four bodies as prop objects at (0,255) -- tile 0x11e, slots 24..27 -- and no ship
//   N3 ... and the skiff as ONE persistent Class C override (0,255)(14,242)=0x129 (a vehicle, not an object)
//   N4 Journey Onward (Continue) restores exactly that -- no duplicates, nothing lost: seeded ONCE, at the seam
//   N5 Load Game of a save taken BEFORE the journey seeds nothing (the seam is CreateInitialSave alone)
//   N6 CORE: a refused reservation places NOTHING and returns false (all-or-nothing)
//   N7 CORE: an unknown class byte, a floor byte that disagrees with its block, slot 0 and a short image place nothing;
//            a frigate record is a ship object with its hull and skiffs; a horse and a carpet are overrides
//
//   a4_parity2_d82_runtime <openu5-alpha1-resources.bin> [original/u5/ultima5]
#include "a4_ui2_harness.h"
#include "../main/alpha_save.h"
#include "openu5/quest_world.h"
#include "openu5/world_terrain.h"
#include "sd_shims/host_sd_card.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace a4_ui2;
namespace fs = std::filesystem;

namespace {
const std::vector<Member> kParty = {{"Kojac", 'G', 100}, {"Iolo", 'G', 100}};

/** The five documented INIT.OOL records (UNDER block): {slot, tile byte, x, y}. */
struct Rec { int slot, b, x, y; };
const Rec kRecords[5] = {{23, 0x29, 14, 242}, {24, 0x1e, 103, 226}, {25, 0x1e, 105, 227}, {26, 0x1e, 107, 227}, {27, 0x1e, 108, 225}};

void full_roster(Run &h) {
    auto &g = h.rt->game();
    for (int i = int(kParty.size()); i < 16; ++i) {
        auto &m = g.party.characters[i];
        std::snprintf(m.name, sizeof(m.name), "%s", "Spare");
        m.status = 'G';
        m.current_hp = m.max_hp = 100;
        m.character_class = 'F';
        m.level = 1;
        m.strength = m.dexterity = m.intelligence = 20;
    }
    g.party.character_count = 16;
    h.render(true);
}
void key_quiet(Run &h, uint8_t code) {
    openu5_host_virtual_clock_us() += 100000;
    tdeck::RawInputEvent e{};
    e.kind = RawInputKind::Keyboard;
    e.transition = tdeck::KeyTransition::Pressed;
    e.code = e.base_code = code;
    h.raw(e);
}
void create(Run &h, const char *name) {
    for (const char *c = name; *c; ++c) key_quiet(h, uint8_t(*c));
    key_quiet(h, '\r');
    key_quiet(h, 'f');
    for (int i = 0; i < 7; ++i) key_quiet(h, uint8_t((i & 1) ? 'b' : 'a'));
}

struct World {
    int bodies = 0, ships = 0, other255 = 0, skiff_cells = 0, cells255 = 0;
    bool body_positions = true, skiff_at = false;
    bool slots = true;
};
World look(Run &h) {
    World w;
    for (const auto &o : h.rt->objects_for_test()) {
        if (o.location != 0 || o.floor != 255) continue;
        if (o.prop && o.tile == 0x11e) {
            ++w.bodies;
            bool known = false;
            for (const auto &r : kRecords)
                if (r.b == 0x1e && r.x == o.x && r.y == o.y && r.slot == o.slot) known = true;
            w.body_positions = w.body_positions && known;
        } else if (o.ship) ++w.ships;
        else if (!o.plot) ++w.other255;
    }
    auto &c = h.rt->command_context_for_test();
    for (const auto &t : c.terrain->persistent) {
        if (t.map.location != 0 || t.map.floor != 255) continue;
        ++w.cells255;
        if (t.x == 14 && t.y == 242 && t.tile == 0x129) { ++w.skiff_cells; w.skiff_at = true; }
    }
    return w;
}

// ---- the core function over a plain vector pool -----------------------------------------------------------------------
struct Pool {
    std::vector<openu5::QuestObject> v;
    size_t room = 100;
    bool refuse = false;
    openu5::QuestWorldServices s{};
    Pool() {
        s.context = this;
        s.count = [](void *p) { return static_cast<Pool *>(p)->v.size(); };
        s.read = [](void *p, size_t i) { return static_cast<Pool *>(p)->v[i]; };
        s.reserve = [](void *p, size_t n) { auto *q = static_cast<Pool *>(p); return !q->refuse && q->v.size() + n <= q->room; };
        s.append = [](void *p, const openu5::QuestObject &o) { static_cast<Pool *>(p)->v.push_back(o); };
        s.erase = [](void *p, size_t i) { auto *q = static_cast<Pool *>(p); q->v.erase(q->v.begin() + long(i)); };
    }
};
std::vector<uint8_t> image(std::initializer_list<std::array<int, 7>> recs, int block = 1) {
    std::vector<uint8_t> o(0x200, 0);
    for (const auto &r : recs) { // {slot, b, x, y, floor, hull, skiffs}
        uint8_t *p = o.data() + block * 0x100 + r[0] * 8;
        p[0] = uint8_t(r[1]); p[2] = uint8_t(r[2]); p[3] = uint8_t(r[3]); p[4] = uint8_t(r[4]); p[5] = uint8_t(r[5]); p[7] = uint8_t(r[6]);
    }
    return o;
}

void test_runtime(const char *original) {
    std::printf("\nN  the New Journey seam (real runtime)\n");
    const uint8_t *ool = pack->initial_ool;
    const size_t olen = pack->initial_ool_size;
    bool n1 = ool && olen == 0x200;
    for (size_t i = 0; n1 && i < 256; ++i) n1 = ool[i] == 0;
    int records = 0;
    for (int slot = 0; n1 && slot < 32; ++slot) {
        const uint8_t *r = ool + 256 + slot * 8;
        if (!r[0]) continue;
        ++records;
        bool ok = false;
        for (const auto &d : kRecords) ok = ok || (d.slot == slot && r[0] == d.b && r[2] == d.x && r[3] == d.y && r[4] == 0xff);
        n1 = n1 && ok;
    }
    n1 = n1 && records == 5;
    if (original && n1) {
        std::ifstream in(fs::path(original) / "INIT.OOL", std::ios::binary);
        std::string b((std::istreambuf_iterator<char>(in)), {});
        if (b.size() == 256) n1 = std::memcmp(b.data(), ool + 256, 256) == 0;
    }
    check(n1, "N1 the pack's initial_ool is zeros(256) ++ INIT.OOL: five records, the documented skiff and four bodies, all floor 0xFF");

    host_sd::format_card();
    {
        std::error_code ec;
        fs::create_directories(fs::path(host_sd::root()) / "sd/ultima5/saves", ec);
    }
    tdeck::AlphaSaveService::reserve_dma_headroom();
    Run h(kParty);
    full_roster(h);
    const World before = look(h);
    // A save taken BEFORE the journey: Slot 1 (no seed in it).
    h.key('m', true);
    h.down();
    h.key('\r');   // Save Game
    h.key('\r');   // Slot 1 (empty: saves at once)
    if (h.rt->system_menu_open()) h.key('m', true);
    h.return_to_title();
    h.key(' ');
    key_quiet(h, 'c');
    create(h, "Nova");
    full_roster(h);
    const World w = look(h);
    check(before.bodies == 0 && before.skiff_cells == 0, "N0 CONTROL: the harness game before the journey has no seed");
    check(w.bodies == 4 && w.body_positions && w.ships == 0 && w.other255 == 0,
          "N2 the New Journey seeds four body props at (0,255), tile 0x11e, slots 24..27, at the documented cells; no ship (" + n(w.bodies) + " bodies, " + n(w.ships) + " ships, " + n(w.other255) + " other)");
    check(w.skiff_cells == 1 && w.skiff_at && w.cells255 == 1,
          "N3 the skiff is ONE persistent Class C override (0,255)(14,242)=0x129 and the only persistent underworld cell (" + n(w.cells255) + " cells)");
    // The effective tile at the skiff's cell is the skiff (what the renderer and Board read).
    {
        auto &c = h.rt->command_context_for_test();
        const int eff = c.terrain->effective(c.world, {0, 255}, 14, 242);
        check(eff == 0x129, "N3b the effective tile at (14,242) is 0x129 (a boardable skiff), got " + n(eff));
    }

    // Continue: the same seeds, once.
    h.return_to_title();
    h.key(' ');
    h.key('j');
    h.key('\r');
    const World c = look(h);
    check(c.bodies == 4 && c.body_positions && c.ships == 0 && c.skiff_cells == 1 && c.cells255 == 1,
          "N4 Journey Onward restores the seeds exactly: four bodies, one skiff cell, no duplicates (" + n(c.bodies) + "/" + n(c.skiff_cells) + ")");

    // Load Game of the pre-journey save: no seed (the seam is CreateInitialSave alone).
    h.key('m', true);
    h.down();
    h.down();
    h.key('\r');   // Load Game
    // the selection starts on CURRENT (Nova's slot): walk to Slot 1
    for (int i = 0; i < 3 && h.rt->system_menu_view().selected_line != 0; ++i) h.down();
    h.key('\r');   // Slot 1 (the harness game, saved before the journey)
    if (h.rt->system_menu_open()) h.key('m', true);
    const World l = look(h);
    check(l.bodies == 0 && l.skiff_cells == 0 && std::strcmp(h.rt->game().party.characters[0].name, "Kojac") == 0,
          "N5 loading the pre-journey save seeds nothing: leader " + std::string(h.rt->game().party.characters[0].name) + ", " + n(l.bodies) + " bodies, " + n(l.skiff_cells) + " skiffs");
}

void test_core() {
    std::printf("\nC  seed_new_journey_underworld (core)\n");
    {
        Pool p;
        openu5::WorldTerrain t;
        const auto img = image({{23, 0x29, 14, 242, 255, 0, 0}, {24, 0x1e, 103, 226, 255, 0, 0}, {25, 0x1e, 105, 227, 255, 0, 0}});
        p.refuse = true;
        const bool ok = openu5::seed_new_journey_underworld(img.data(), img.size(), p.s, t);
        check(!ok && p.v.empty() && t.persistent.empty(),
              "N6 a refused pool reservation places nothing at all (no skiff, no body) and returns false");
        p.refuse = false;
        const bool ok2 = openu5::seed_new_journey_underworld(img.data(), img.size(), p.s, t);
        check(ok2 && p.v.size() == 2 && t.persistent.size() == 1 && p.v[0].prop && p.v[0].tile == 0x11e && p.v[0].slot == 24,
              "N6b CONTROL: with room, the same image places two bodies and the skiff");
    }
    {
        Pool p;
        openu5::WorldTerrain t;
        // a block-1 record whose +4 says Britannia; slot 0; an unknown class byte 0x7e; a short image
        const auto img = image({{9, 0x29, 1, 1, 0, 0, 0}, {0, 0x1e, 2, 2, 255, 0, 0}, {5, 0x7e, 3, 3, 255, 0, 0}});
        const bool ok = openu5::seed_new_journey_underworld(img.data(), img.size(), p.s, t);
        const bool short_ok = openu5::seed_new_journey_underworld(img.data(), 0x1ff, p.s, t);
        check(ok && short_ok && p.v.empty() && t.persistent.empty(),
              "N7 floor-mismatch, slot 0, unknown class byte and a short image place nothing");
    }
    {
        Pool p;
        openu5::WorldTerrain t;
        const auto img = image({{3, 0x20, 50, 60, 255, 77, 2}, {4, 0x10, 10, 11, 255, 0, 0}, {5, 0x1b, 12, 13, 255, 0, 0}, {6, 0x2b, 14, 15, 255, 0, 0}});
        const bool ok = openu5::seed_new_journey_underworld(img.data(), img.size(), p.s, t);
        const bool ship = p.v.size() == 1 && p.v[0].ship && p.v[0].hull == 77 && p.v[0].skiffs == 2 && p.v[0].tile == 0x120 && p.v[0].slot == 3 && p.v[0].floor == 255;
        check(ok && ship && t.persistent.size() == 3 && t.persistent[0].tile == 0x110 && t.persistent[1].tile == 0x11b && t.persistent[2].tile == 0x12b,
              "N7b a frigate is a ship object with its hull and skiffs; a horse, a carpet and a skiff are Class C overrides");
        // Block 0 (Britannia, floor 0) records seed too, at floor 0.
        Pool q;
        openu5::WorldTerrain t0;
        const auto b0 = image({{2, 0x29, 7, 8, 0, 0, 0}}, 0);
        openu5::seed_new_journey_underworld(b0.data(), b0.size(), q.s, t0);
        check(t0.persistent.size() == 1 && t0.persistent[0].map.floor == 0 && t0.persistent[0].tile == 0x129,
              "N7c a block-0 record seeds on floor 0 (the BRIT block shares the rule)");
    }
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: a4_parity2_d82_runtime <pack> [original/u5/ultima5]\n");
        return 2;
    }
    const char *original = argc > 2 ? argv[2] : nullptr;
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load -- the New Journey checks cannot run\n");
        return 1;
    }
    const auto dir = fs::temp_directory_path() / "openu5-a4-parity2-d82-card";
    host_sd::set_root(dir.string());
    test_core();
    test_runtime(original);
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::printf("A4-PARITY2 D-82 runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
