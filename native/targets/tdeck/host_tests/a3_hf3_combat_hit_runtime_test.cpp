// Alpha 3 A3-HF3 -- D-63, combat hit feedback (ALPHA3_AUDIO.md section 27),
// through the REAL AlphaRuntime driving the REAL tdeck_board.cpp over the fake
// ST7789 (A3-04E's harness). It observes only what the device shows:
//
//   * a roster row is in REVERSE VIDEO when most of its rectangle on the panel
//     is the text colour instead of black (Board::draw_text_box, kernel 0x2a28);
//   * the arena cell carries the MARKER when its 16x16 pixels in the composed
//     viewport are tile 0's (ULTIMA.EXE 0x359f: blit_tile(cell, 0), opaque).
//
// The fights use the real dice (the troll encounter of A3-HF1's seed), with the
// combatants' stats set so that the attack in question must hit. The timing
// checks (H5/H6) feed Attacked events through the runtime's own event sink --
// the core's envelope, not a private seam -- to put two hits inside one cue.
//
//   a3_hf3_combat_hit_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin>
#include "../main/alpha_audio.h"
#include "../main/alpha_runtime.h"
#include "../main/idle_service.h"
#include "../main/native_renderer.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "fake_tdeck_bus.h"
#include "openu5/audio.h"
#include "openu5/combat.h"
#include "openu5/debug_developer.h"
#include "openu5/hud.h"
#include "openu5/outdoor.h"
#include "openu5/presentation.h"
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
namespace bus = openu5_host_bus;

namespace {
int checks = 0, failures = 0;
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}
const tdeck::AlphaResourceOwners *pack = nullptr;
size_t g_dungeon_count = 0;
AudioPackInfo g_audio{};
constexpr int64_t kClockStartUs = 5'000'000;
constexpr uint32_t kSeed = 2; // A3-HF1's roaming-troll seed
constexpr int kMembers = 3;
const char *const kNames[kMembers] = {"Avatar", "Iolo", "Shamino"};

// The fixture's patterned tiles (alpha_runtime_host_fixture.cpp): byte b of tile
// t is t*37 + b*11 + (b>>3)*7, palette entry i is i*0x1111, and expand_tile
// puts the high nibble on the left.
void tile_pixels(int tile, uint16_t (&out)[256]) {
    for (int b = 0; b < 128; ++b) {
        const uint8_t v = uint8_t(tile * 37 + b * 11 + (b >> 3) * 7);
        const int y = b / 8, x = (b % 8) * 2;
        out[y * 16 + x] = uint16_t((v >> 4) * 0x1111);
        out[y * 16 + x + 1] = uint16_t((v & 15) * 0x1111);
    }
}

/** The recording backend: every SFX the service submits, with the virtual time. */
struct Recorder final : AudioBackend {
    struct Sub {
        int64_t us;
        SfxId id;
    };
    std::vector<Sub> subs;
    bool play_sfx(const SfxRequest &r) override {
        subs.push_back({openu5_host_virtual_clock_us(), r.id});
        return true;
    }
    void stop_sfx() override {}
    bool start_music(MusicSong, uint16_t) override { return true; }
    void stop_music() override {}
    void set_gain(AudioChannel, uint16_t) override {}
    size_t count(SfxId id) const {
        return size_t(std::count_if(subs.begin(), subs.end(), [&](const Sub &s) { return s.id == id; }));
    }
};

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    tdeck::IdleService idle{};
    Recorder audio{};
    explicit Run(uint32_t seed) {
        openu5_host_virtual_clock_us() = kClockStartUs;
        bus::install();
        bus::model() = bus::Model{};
        bus::model().timed = false;
        board.initialize_display();
        openu5_host_virtual_clock_us() = kClockStartUs;
        idle.attach(bus::idle_passes());
        board.set_idle_service(&idle);
        rt->attach_idle_service(&idle);
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
        f.patterned_test_tiles = true;
        rt->attach_host_test_fixture(f);
        rt->configure_audio(g_audio, &audio);
        auto &g = rt->game();
        g.time.hour = 12;
        g.time.minute = 0;
        g.food = 900;
        g.party.character_count = g.party.party_size = kMembers;
        g.party.active_character = 255;
        for (int i = 0; i < kMembers; ++i) {
            auto &m = g.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), "%s", kNames[i]);
            m.status = 'G';
            m.character_class = i ? 'F' : 'A';
            m.level = 1;
            m.current_hp = m.max_hp = 100;
            m.strength = m.dexterity = m.intelligence = 20;
        }
        g.rng.seed(seed);
        render(true);
    }
    int64_t now() const { return openu5_host_virtual_clock_us(); }
    void render(bool force = false) { rt->render(board, force); }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = now();
        rt->handle(e);
    }
    void key(uint8_t code, bool alt = false, int64_t advance_us = 100000) {
        openu5_host_virtual_clock_us() += advance_us;
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = code;
        e.modifiers.alt = alt;
        raw(e);
        render();
    }
    void cancel() {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent down{};
        down.kind = RawInputKind::Keyboard;
        down.column = tdeck::kMicrophoneKeyColumn;
        down.row = tdeck::kMicrophoneKeyRow;
        down.transition = tdeck::KeyTransition::Pressed;
        raw(down);
        openu5_host_virtual_clock_us() += 50000;
        tdeck::RawInputEvent up = down;
        up.transition = tdeck::KeyTransition::Released;
        raw(up);
        render();
    }
    void ball(RawInputKind k) {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
        render();
    }
    void dir(int dx, int dy) {
        ball(dx > 0 ? RawInputKind::TrackballRight
             : dx < 0 ? RawInputKind::TrackballLeft
             : dy > 0 ? RawInputKind::TrackballDown
                      : RawInputKind::TrackballUp);
    }
    /** Advance the clock in 5 ms frames. */
    void run(int64_t ms) {
        for (int64_t t = 0; t < ms; t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            render();
        }
    }
    CombatState &cs() { return rt->combat_state_for_test(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    bool combat() { return ctx().combat && cs().initialized; }
    UiMode mode() const { return rt->ui()->mode(); }

    // --- what the panel shows ------------------------------------------------
    /** Roster row `row` (party order) is in reverse video on the panel. */
    bool inverted(int row) const {
        const uint16_t *p = bus::gram();
        int black = 0, total = 0;
        for (int y = 4 + row * 8; y < 4 + row * 8 + 8; ++y)
            for (int x = kHudRightX; x < kHudRightX + kHudRightW; ++x, ++total) black += p[y * 320 + x] == 0;
        return black * 2 < total;
    }
    int inverted_rows() const {
        int k = 0;
        for (int r = 0; r < kMembers; ++r) k += inverted(r);
        return k;
    }
    uint64_t row_hash(int row) const {
        const uint16_t *p = bus::gram();
        uint64_t h = 1469598103934665603ull;
        for (int y = 4 + row * 8; y < 4 + row * 8 + 8; ++y)
            for (int x = kHudRightX; x < kHudRightX + kHudRightW; ++x) h = (h ^ p[y * 320 + x]) * 1099511628211ull;
        return h;
    }
    /** The arena cell's 16x16 pixels in the composed viewport are tile `t`'s. */
    bool cell_is_tile(int cx, int cy, int t) const {
        const uint16_t *vp = rt->composed_viewport();
        if (!vp || cx < 0 || cy < 0 || cx > 10 || cy > 10) return false;
        uint16_t want[256];
        tile_pixels(t, want);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                if (vp[(cy * 16 + y) * 176 + cx * 16 + x] != want[y * 16 + x]) return false;
        return true;
    }
    /** How many of the 121 cells show tile 0. */
    int marker_cells() const {
        int k = 0;
        for (int y = 0; y < 11; ++y)
            for (int x = 0; x < 11; ++x) k += cell_is_tile(x, y, 0);
        return k;
    }
    /** A3HF3_TRACE=1: what the panel shows now (rows inverted, marked cells). */
    void trace(const char *tag) const {
        if (!std::getenv("A3HF3_TRACE")) return;
        std::printf("TRACE %-10s t=%lld rows=%d%d%d marks=%d mode=%d\n", tag, (long long)(now() / 1000), inverted(0),
                    inverted(1), inverted(2), marker_cells(), int(mode()));
    }
    /** The same Attacked envelope the core emits, through the runtime's own sink. */
    void emit_combat(const CombatEvent &ce) {
        GameEvent ge{};
        ge.kind = GameEventKind::Combat;
        ge.combat = &ce;
        ctx().events.emit(ctx().events.context, ge);
    }
};

// --- The arena (A3-HF1's troll fight) --------------------------------------
int troll_tile(CommandContext &ctx) {
    for (size_t i = 0; i < ctx.outdoor->resources->enemy_count; ++i)
        if (const auto *d = ctx.outdoor->resources->enemies[i]; d && d->index == 41) return d->tile;
    return -1;
}
bool troll_attack(Run &h) {
    auto &ctx = h.ctx();
    ctx.outdoor->enemies.clear();
    OutdoorEnemy troll{};
    troll.definition = 41;
    troll.tile = troll_tile(ctx);
    troll.x = h.rt->game().position.xy.x + 1;
    troll.y = h.rt->game().position.xy.y;
    ctx.outdoor->enemies.push_back(troll);
    h.key(' ');
    h.run(1500);
    return h.combat();
}
CombatActor *player_turn(Run &h) {
    auto &s = h.cs();
    if (!h.combat() || s.current < 0 || s.current >= s.count) return nullptr;
    auto &a = s.actors[s.current];
    if (a.enemy || a.member == 255 || a.status != CombatStatus::Active) return nullptr;
    return h.mode() == UiMode::Combat ? &a : nullptr;
}
CombatActor *settle(Run &h, int budget_ms = 20000) {
    for (int t = 0; t < budget_ms && h.combat(); t += 5) {
        if (auto *a = player_turn(h)) return a;
        h.run(5);
    }
    return player_turn(h);
}
CombatActor *member_actor(Run &h, int member) {
    auto &s = h.cs();
    for (int i = 0; i < s.count; ++i)
        if (s.actors[i].member == member) return &s.actors[i];
    return nullptr;
}
/** One enemy stays (placed at `ex,ey`); every other enemy leaves the arena. */
CombatActor *lone_enemy(Run &h, int ex, int ey) {
    auto &s = h.cs();
    CombatActor *e = nullptr;
    for (int i = 0; i < s.count; ++i) {
        auto &a = s.actors[i];
        if (!a.enemy || a.status != CombatStatus::Active) continue;
        if (!e) e = &a;
        else a.status = CombatStatus::Fled;
    }
    if (e) {
        e->position = {int16_t(ex), int16_t(ey)};
        e->speed = e->strength = 30; // ds 0 vs 30: (0 - 30 + 30) / 2 = 0 <= r30(): always hits
        e->defense = 0;
        e->attack = 7;
        e->hp = e->max_hp = 200;
    }
    return e;
}
/** Park the members: `near` beside the enemy, the others far away. */
void place(Run &h, int near, int nx, int ny) {
    const int far[kMembers][2] = {{0, 0}, {0, 10}, {10, 10}};
    for (int m = 0; m < kMembers; ++m) {
        auto *a = member_actor(h, m);
        if (!a) continue;
        a->position = m == near ? CombatPoint{int16_t(nx), int16_t(ny)} : CombatPoint{int16_t(far[m][0]), int16_t(far[m][1])};
        a->speed = m == near ? 0 : 90; // the struck one never dodges (ds = 0)
        a->strength = 90;
        a->defense = 0;
    }
}
/** Pass the player turns until an enemy step lowers `member`'s HP; returns the frame time or -1. */
int64_t wait_for_hit(Run &h, int member, int budget_ms = 20000) {
    const auto hp0 = h.rt->game().party.characters[member].current_hp;
    const char status0 = h.rt->game().party.characters[member].status;
    for (int t = 0; t < budget_ms && h.combat(); t += 5) {
        if (player_turn(h)) h.key(' ', false, 5000);
        else h.run(5);
        const auto &c = h.rt->game().party.characters[member];
        if (c.current_hp != hp0 || c.status != status0) return h.now();
    }
    return -1;
}
/** A 5x3 block of grass with nothing animated within 6 cells (A3-04F's find_grass(false)). */
bool inland_grass(int &gx, int &gy) {
    const auto m = get_active_map(pack->world, MapId{LocationId(0), 0});
    if (m.error != Error::None) return false;
    for (int y = 60; y < 200; ++y)
        for (int x = 60; x < 200; ++x) {
            bool ok = true;
            for (int dy = -1; dy <= 1 && ok; ++dy)
                for (int dx = -2; dx <= 2 && ok; ++dx) ok = m.value.tile_at(x + dx, y + dy) == 5;
            for (int dy = -6; dy <= 6 && ok; ++dy)
                for (int dx = -6; dx <= 6 && ok; ++dx)
                    ok = tile_animation_kind(m.value.tile_at(x + dx, y + dy)) == TileAnimationKind::Static;
            if (ok) {
                gx = x;
                gy = y;
                return true;
            }
        }
    return false;
}
bool enter_arena(Run &h) {
    int gx = -1, gy = -1;
    if (!inland_grass(gx, gy)) return false;
    DebugTeleportRequest r{};
    r.kind = DebugDestinationKind::Britannia;
    r.x = gx;
    r.y = gy;
    if (apply_debug_teleport(h.ctx(), r).status != DebugTeleportStatus::Applied) return false;
    h.render(true);
    return troll_attack(h) && settle(h);
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <openu5-alpha1-resources.bin> <openu5-audio.bin>\n", argv[0]);
        return 2;
    }
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report_out{};
    if (source.open(argv[1], report_out) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report_out) != ESP_OK) return 2;
    pack = &owners;
    g_dungeon_count = report_out.dungeon_count;
    g_audio = tdeck::load_audio_pack_info(argv[2]);

    const int kMs = 174; // the cue: noise_burst(0x28/0xa, 0xbb8) = 9,000 half samples at 2 x 25,806 Hz
    const int kGap = 55;  // the restore between two queued cues (native choice)

    // ---- H4 / H1 / H2: an enemy hits a named party member ------------------
    {
        Run h(kSeed);
        const bool in = enter_arena(h);
        auto *e = lone_enemy(h, 6, 5);
        place(h, 1, 5, 5); // Iolo beside the troll
        h.render(true);
        const int struck = 1;
        const bool pre = h.inverted_rows() == 0 && h.marker_cells() == 0;
        const size_t heavy0 = h.audio.count(SfxId::CombatHitHeavy);
        const int64_t t0 = in && e ? wait_for_hit(h, struck) : -1;
        const bool hit = t0 >= 0;
        const bool row_now = hit && h.inverted(struck) && h.inverted_rows() == 1;
        const bool marker_now = hit && h.cell_is_tile(5, 5, 0) && h.marker_cells() == 1;
        const bool sound_now = hit && h.audio.count(SfxId::CombatHitHeavy) == heavy0 + 1 &&
                               h.audio.subs.back().id == SfxId::CombatHitHeavy && h.audio.subs.back().us == t0;
        check(in && e && hit && pre, "H0 control: the troll fight is entered, the struck member loses HP, nothing is inverted or marked before");
        check(row_now, "H1 an enemy hit on Iolo (row 2) puts exactly that roster row in reverse video, in the frame of the hit");
        check(marker_now, "H4 the same hit puts tile 0 (the 0x359f marker) on Iolo's arena cell, and on no other cell");
        check(sound_now && row_now, "S1 the cue starts with its sound: CombatHitHeavy submitted once, at the frame the row inverts");
        h.run(kMs - 10);
        const bool held = hit && h.inverted(struck) && h.cell_is_tile(5, 5, 0);
        h.run(20);
        const bool restored = hit && h.inverted_rows() == 0 && h.marker_cells() == 0;
        check(held, "H1b the cue is held for the burst: row and marker still up 164 ms after the hit");
        check(restored, "H2 after 174 ms the row is back to normal and the cell shows the arena again");
    }

    // ---- H3: a party member hits the enemy ---------------------------------
    {
        Run h(kSeed);
        bool in = enter_arena(h);
        auto *e = lone_enemy(h, 6, 5);
        auto *a = in ? player_turn(h) : nullptr;
        bool hit = false, marker = false, no_row = false, sound = false;
        if (a && e) {
            place(h, a->member, 5, 5);
            a->speed = 90; // as 90 vs ds 30: always hits
            h.render(true);
            const int16_t hp0 = e->hp;
            const size_t light0 = h.audio.count(SfxId::CombatHit);
            h.key('a');
            if (h.mode() == UiMode::TargetSelection) {
                h.dir(1, 0);
                h.key('\r', false, 5000);
            }
            hit = e->hp < hp0;
            marker = hit && h.cell_is_tile(6, 5, 0) && h.marker_cells() == 1;
            no_row = hit && h.inverted_rows() == 0;
            sound = hit && h.audio.count(SfxId::CombatHit) == light0 + 1;
        }
        check(in && a && e && hit, "H3a control: the member's bare-hand strike lands on the troll (its HP falls)");
        check(marker, "H3 a hit on the enemy puts the tile-0 marker on the enemy's cell");
        check(hit && no_row, "H3c control: an enemy struck inverts NO roster row");
        check(sound, "S2 control: the enemy hit plays CombatHit once (the existing cue, unchanged)");
        h.run(kMs + 10);
        check(hit && h.marker_cells() == 0 && h.inverted_rows() == 0, "H3d the enemy's marker is gone after 174 ms");
    }

    // ---- H5 / H6 / H7 / H9: timing through the runtime's event sink -----------
    {
        Run h(kSeed);
        const bool in = enter_arena(h);
        auto *e = lone_enemy(h, 6, 5);
        place(h, 1, 5, 5);
        auto *m0 = member_actor(h, 0), *m1 = member_actor(h, 1), *m2 = member_actor(h, 2);
        if (m0) m0->position = {4, 4};
        if (m2) m2->position = {4, 6};
        h.render(true);
        // Make it Iolo's turn: the renderer boxes the active combatant's cell, so
        // the cues below fall on the two members whose turn it is not.
        for (int k = 0; k < 40; ++k) {
            auto *p = player_turn(h);
            if (p && p->member == 1) break;
            if (p) h.key(' ', false, 5000);
            else h.run(5);
        }
        h.run(400); // any enemy-step cue from the passes is over
        const bool ready = in && e && m0 && m1 && m2 && player_turn(h) && player_turn(h)->member == 1;
        CombatEvent hit_a{}, hit_b{};
        if (ready) {
            hit_a.kind = hit_b.kind = CombatEventKind::Attacked;
            hit_a.actor = hit_b.actor = e->id;
            hit_a.target = m0->id;
            hit_b.target = m2->id;
            hit_a.hit = hit_b.hit = 1;
            hit_a.damage = hit_b.damage = 1;
            hit_a.text = "Avatar hit!";
            hit_b.text = "Shamino hit!";
        }
        // H5: Avatar then Shamino in the same instant.
        bool h5 = ready, awake = ready, rests = ready;
        if (ready) {
            h.trace("h5-before");
            rests = h.rt->loop_may_sleep(); // control: a player's turn, nothing timed
            h.emit_combat(hit_a);
            h.emit_combat(hit_b);
            h.render();
            h.trace("h5-t0");
            h5 = h5 && h.inverted(0) && !h.inverted(2) && h.cell_is_tile(4, 4, 0) && h.marker_cells() == 1;
            h.run(kMs - 9);
            h5 = h5 && h.inverted(0) && !h.inverted(2);
            h.run(10); // 175: the first is over, the restore gap
            h5 = h5 && h.inverted_rows() == 0 && h.marker_cells() == 0;
            awake = !h.rt->loop_may_sleep(); // the second cue is still owed
            h.run(kGap);  // 230: the second
            h5 = h5 && h.inverted(2) && !h.inverted(0) && h.cell_is_tile(4, 6, 0) && h.marker_cells() == 1;
            h.run(kMs + 10);
            h5 = h5 && h.inverted_rows() == 0 && h.marker_cells() == 0;
            rests = rests && h.rt->loop_may_sleep();
        }
        check(awake && rests, "L1 the main loop does not sleep while a cue is owed (it is timed from the last), and sleeps again after");
        check(h5, "H5 two party hits in one instant: Avatar's cue first (174 ms), a visible restore, then Shamino's; nothing left inverted");
        // H6: Avatar twice. First let the last restore window lapse: a cue that
        // arrives inside one waits for it (the pacer's rule), which is not H6's question.
        h.run(kGap + 10);
        bool h6 = ready;
        if (ready) {
            h.emit_combat(hit_a);
            h.emit_combat(hit_a);
            h.render();
            h6 = h6 && h.inverted(0);
            h.run(kMs + 1);
            h6 = h6 && !h.inverted(0) && h.marker_cells() == 0; // the gap between the two cues
            h.run(kGap);
            h6 = h6 && h.inverted(0) && h.cell_is_tile(4, 4, 0);
            h.run(kMs + 10);
            h6 = h6 && h.inverted_rows() == 0 && h.marker_cells() == 0;
        }
        check(h6, "H6 the same member hit twice shows two cues with a restored row between them, then none");
        // H7: events that are not a hit.
        bool h7 = ready;
        if (ready) {
            CombatEvent miss = hit_a;
            miss.hit = 0;
            miss.text = nullptr;
            CombatEvent moved{};
            moved.kind = CombatEventKind::Moved;
            moved.actor = e->id;
            CombatEvent nothing{};
            nothing.kind = CombatEventKind::Message;
            nothing.text = "Nothing!";
            nothing.actor = m1->id;
            CombatEvent food{};
            food.kind = CombatEventKind::Message;
            food.text = "A troll stole some food!";
            food.actor = e->id;
            CombatEvent passes{};
            passes.kind = CombatEventKind::Message;
            passes.text = "Iolo passes out!";
            passes.actor = m1->id;
            CombatEvent died{};
            died.kind = CombatEventKind::Died;
            died.actor = e->id;
            died.target = m2->id;
            died.text = "Shamino killed!";
            const size_t heavy0 = h.audio.count(SfxId::CombatHitHeavy), light0 = h.audio.count(SfxId::CombatHit);
            for (const auto *ev : {&miss, &moved, &nothing, &food, &passes, &died}) {
                h.emit_combat(*ev);
                h.render();
                h7 = h7 && h.inverted_rows() == 0 && h.marker_cells() == 0;
            }
            // Their own text cues ('Nothing!', the theft, Died's burst) are A3-03's and untouched;
            // none of them is a 0x3564 hit burst.
            h7 = h7 && h.audio.count(SfxId::CombatHitHeavy) == heavy0 && h.audio.count(SfxId::CombatHit) == light0;
        }
        check(h7, "H7 control: a miss, a move, 'Nothing!', food theft, 'passes out!' and a lone Died put up no marker and no row");
        // Status-only strikes: the poisoning and the sleep strike are hits in the original (0x3564 before 0x194A).
        h.run(kGap + 10); // the previous restore window lapses
        bool st = ready;
        if (ready) {
            CombatEvent poisoned{};
            poisoned.kind = CombatEventKind::Message;
            poisoned.actor = e->id;
            poisoned.target = m2->id;
            poisoned.text = "Shamino is poisoned!";
            const size_t heavy0 = h.audio.count(SfxId::CombatHitHeavy);
            h.emit_combat(poisoned);
            h.render();
            st = st && h.inverted(2) && h.inverted_rows() == 1 && h.cell_is_tile(4, 6, 0) &&
                 h.audio.count(SfxId::CombatHitHeavy) == heavy0 + 1;
            h.run(kMs + kGap + 10);
            CombatEvent slept = poisoned;
            slept.target = e->id;
            slept.actor = m1->id;
            slept.text = "troll slept!";
            const size_t light0 = h.audio.count(SfxId::CombatHit);
            h.emit_combat(slept);
            h.render();
            st = st && h.inverted_rows() == 0 && h.cell_is_tile(6, 5, 0) && h.audio.count(SfxId::CombatHit) == light0 + 1;
            h.run(kMs + kGap + 10);
            st = st && h.inverted_rows() == 0 && h.marker_cells() == 0;
        }
        check(st, "H10 'is poisoned!' / 'slept!' (strikes inside 0x194A) cue like a hit: row + marker + the side's burst");
        // H9: the row cache (A3-04F) -- poisoned row, HP changing while inverted, a menu during the cue.
        h.run(kGap + 10); // the previous restore window lapses
        bool h9 = ready;
        if (ready) {
            h.rt->game().party.characters[0].status = 'P';
            h.render(true);
            const uint64_t before = h.row_hash(0);
            h.emit_combat(hit_a);
            h.render();
            const uint64_t on = h.row_hash(0);
            const bool cache_on = h.inverted(0) && on != before;
            // A second hit while the first cue shows: the core commits the HP, then emits
            // (combat.cpp damage()). The row's TEXT changes under an unchanged inversion.
            h.run(20);
            h.rt->game().party.characters[0].current_hp = 55;
            h.emit_combat(hit_a);
            h.render();
            const bool cache_text = h.inverted(0) && h.row_hash(0) != on;
            // The HP is put back before the queued cue ends, so the redraw at the end of
            // the last cue is what must reproduce the poisoned row exactly.
            h.rt->game().party.characters[0].current_hp = 100;
            h.run(2 * kMs + kGap);
            const bool cache_back = h.inverted_rows() == 0 && h.row_hash(0) == before;
            if (std::getenv("A3HF3_TRACE"))
                std::printf("TRACE h9 on=%d text=%d back=%d\n", int(cache_on), int(cache_text), int(cache_back));
            h9 = h9 && cache_on && cache_text && cache_back;
            // A Developer-menu visit during a cue: leaving it repaints the whole panel, which
            // must draw the still-live inversion (forced rows), then nothing is left stuck.
            h.run(kGap + 10);
            h.emit_combat(hit_a);
            h.render();
            const bool shown = h.inverted(0);
            h.key('d', true, 20000); // t = 20 ms
            h.cancel();              // t = 170 ms: the cue is still up
            h.render();
            const bool during = h.inverted(0) && h.inverted_rows() == 1 && h.cell_is_tile(4, 4, 0);
            h.run(20);
            if (std::getenv("A3HF3_TRACE"))
                std::printf("TRACE h9 cache=%d shown=%d during=%d rows=%d marks=%d mode=%d\n", int(h9), int(shown),
                            int(during), h.inverted_rows(), h.marker_cells(), int(h.mode()));
            h9 = h9 && shown && during && h.inverted_rows() == 0 && h.marker_cells() == 0 && h.mode() == UiMode::Combat;
        }
        check(h9, "H9 the retained rows: a poisoned row flashes and comes back byte-identical, HP redraws while reversed, a menu mid-cue leaves nothing stuck");
    }

    // ---- H8: a hit that kills ------------------------------------------------
    {
        Run h(kSeed);
        const bool in = enter_arena(h);
        auto *e = lone_enemy(h, 6, 5);
        place(h, 1, 5, 5);
        if (e) e->attack = 50; // more than Iolo has left
        auto *iolo = member_actor(h, 1);
        if (iolo) iolo->hp = 3;
        h.rt->game().party.characters[1].current_hp = 3;
        h.render(true);
        const int64_t t0 = in && e && iolo ? wait_for_hit(h, 1) : -1;
        const bool dead = t0 >= 0 && h.rt->game().party.characters[1].status == 'D';
        const bool cue = dead && h.inverted(1) && h.cell_is_tile(5, 5, 0);
        h.run(kMs + 10);
        const bool after = dead && h.inverted_rows() == 0 && !h.cell_is_tile(5, 5, 0) && h.combat();
        check(dead && cue, "H8 a killing blow on Iolo still gets the cue (0x3564 precedes 0x194A's death): row + marker");
        check(after, "H8b the cue clears and the death wins: no row inverted, the cell no longer marked, the fight goes on");
    }
    {
        // Victory does not end a fight in native (latch(): "VICTORY!", the party walks out), so the last
        // enemy's killing blow keeps the arena: its cue is drawn there like any other.
        Run h(kSeed);
        const bool in = enter_arena(h);
        auto *e = lone_enemy(h, 6, 5);
        auto *a = in ? player_turn(h) : nullptr;
        bool won = false, shown = false, cleared = false;
        if (a && e) {
            place(h, a->member, 5, 5);
            a->speed = 90;
            e->hp = 1;
            h.render(true);
            h.key('a');
            if (h.mode() == UiMode::TargetSelection) {
                h.dir(1, 0);
                h.key('\r', false, 5000);
            }
            won = h.cs().victory && e->status == CombatStatus::Dead && h.combat();
            shown = won && h.cell_is_tile(6, 5, 0);
            h.run(kMs + 20);
            cleared = won && h.marker_cells() == 0 && h.combat();
        }
        check(in && a && e && won, "H8c control: the final blow on the last enemy wins; the arena stays up (native's latch)");
        check(shown && cleared, "H8d the last enemy's killing blow gets its marker, which then clears in the arena");
    }
    {
        // A party wipe DOES end the fight (end(): "BATTLE IS LOST!"): the killing blow's cue must still be
        // seen before the teardown takes the arena away -- in the original 0x3564 runs before 0x194A.
        Run h(kSeed);
        const bool in = enter_arena(h);
        auto *e = lone_enemy(h, 6, 5);
        place(h, 1, 5, 5);
        for (int m : {0, 2})
            if (auto *d = member_actor(h, m)) {
                d->status = CombatStatus::Dead;
                d->hp = 0;
                h.rt->game().party.characters[m].status = 'D';
                h.rt->game().party.characters[m].current_hp = 0;
            }
        auto *iolo = member_actor(h, 1);
        if (e) e->attack = 50;
        if (iolo) iolo->hp = 3;
        h.rt->game().party.characters[1].current_hp = 3;
        h.render(true);
        const int64_t t0 = in && e && iolo ? wait_for_hit(h, 1) : -1;
        const bool lost = t0 >= 0 && h.cs().ended && !h.cs().victory;
        const bool shown = lost && h.ctx().combat && h.cell_is_tile(5, 5, 0) && h.inverted(1);
        h.key('x', false, 5000); // a key during the hold is not owed (the original flushes at 0x0d1f)
        h.run(kMs + 20);
        const bool torn = lost && !h.ctx().combat && h.mode() != UiMode::Combat;
        check(lost, "H8e control: killing the last standing member loses the fight (combat ended)");
        check(shown, "H8f the wiping blow's cue is drawn in the arena before the teardown");
        check(torn, "H8g the teardown follows the cue: the arena is gone and the UI has left combat");
    }

    std::printf("a3_hf3_combat_hit_runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
