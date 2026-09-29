// Alpha 4 UI Batch 2 (targets/tdeck/ALPHA4_UI.md section 2.2) -- the music
// across a party death and Lord British's resurrection, through the REAL
// AlphaRuntime (real Board, paced scenes, the real audio pack) against a
// recording audio backend.
//
// The hardware report: the party dies in battle while music plays, and the
// music plays on through the whole death/resurrection sequence.
//
// The reference: the patch author "made sure that Death and the Blackthorn
// capture sequence were both handled properly: no music should be playing
// during these sequences" (History.txt). TOWN 0x1862 / MAINOUT 0x0af0 /
// DUNGEON 0x1014 stop the music (selector 0x03) before party_refuge
// (BLCKTHRN 0x0910) and re-enable the location rule after it; the next key
// poll, at the castle prompt, derives Lord British's castle -> The Missing
// Monarch (re/notes/music-location-mapping.md section 1.4). game/src/main.ts
// runRefugeScene: music.play("silence") first, resumeMusic() right after
// resolveRefuge().
//
//   D  a combat wipe (the enemy's blow, inside render(), no key): silence from
//      the Refuge's first beat, through every beat and the getkey, and The
//      Missing Monarch at the frame the resurrection resolves -- with no key;
//      SFX unchanged; a session music mute and Music Volume 0 % keep the
//      music off throughout and, restored afterwards, start the castle's
//      song, not the battle's; the scene's timing is the same with or without
//      music; the out-of-combat Refuge (A3-HF9's path) resumes the same way
//
//   a4_ui2_death_music_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin>
#include "a4_ui2_harness.h"
#include "openu5/combat.h"
#include "openu5/debug_developer.h"
#include "openu5/frontend_settings.h"
#include "openu5/narrative_scene.h"
#include "openu5/outdoor.h"
#include "openu5/presentation.h"
#include "openu5/world.h"

namespace tdeck {
std::string &a3_host_settings_text();
bool &a3_host_settings_enabled();
} // namespace tdeck

using namespace a4_ui2;

namespace {
AudioPackInfo g_audio{};
const std::vector<Member> kParty = {{"Avatar", 'G', 100}, {"Iolo", 'G', 100}, {"Shamino", 'G', 100}};
constexpr int kMembers = 3;

// --- The arena: A3-HF3's troll fight (a3_hf3_combat_hit_runtime_test.cpp) ------
CombatState &cs(Run &h) { return h.rt->combat_state_for_test(); }
CommandContext &ctx(Run &h) { return h.rt->command_context_for_test(); }
bool in_combat(Run &h) { return ctx(h).combat && cs(h).initialized; }
int troll_tile(CommandContext &c) {
    for (size_t i = 0; i < c.outdoor->resources->enemy_count; ++i)
        if (const auto *d = c.outdoor->resources->enemies[i]; d && d->index == 41) return d->tile;
    return -1;
}
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
CombatActor *player_turn(Run &h) {
    auto &s = cs(h);
    if (!in_combat(h) || s.current < 0 || s.current >= s.count) return nullptr;
    auto &a = s.actors[s.current];
    if (a.enemy || a.member == 255 || a.status != CombatStatus::Active) return nullptr;
    return h.rt->ui()->mode() == UiMode::Combat ? &a : nullptr;
}
CombatActor *member_actor(Run &h, int member) {
    auto &s = cs(h);
    for (int i = 0; i < s.count; ++i)
        if (s.actors[i].member == member) return &s.actors[i];
    return nullptr;
}
CombatActor *lone_enemy(Run &h, int ex, int ey) {
    auto &s = cs(h);
    CombatActor *e = nullptr;
    for (int i = 0; i < s.count; ++i) {
        auto &a = s.actors[i];
        if (!a.enemy || a.status != CombatStatus::Active) continue;
        if (!e) e = &a;
        else a.status = CombatStatus::Fled;
    }
    if (e) {
        e->position = {int16_t(ex), int16_t(ey)};
        e->speed = e->strength = 30; // always hits
        e->defense = 0;
        e->attack = 50;              // more than the last member has left
        e->hp = e->max_hp = 200;
    }
    return e;
}
bool enter_arena(Run &h) {
    int gx = -1, gy = -1;
    if (!inland_grass(gx, gy)) return false;
    DebugTeleportRequest r{};
    r.kind = DebugDestinationKind::Britannia;
    r.x = gx;
    r.y = gy;
    if (apply_debug_teleport(ctx(h), r).status != DebugTeleportStatus::Applied) return false;
    h.render(true);
    auto &c = ctx(h);
    c.outdoor->enemies.clear();
    OutdoorEnemy troll{};
    troll.definition = 41;
    troll.tile = troll_tile(c);
    troll.x = h.rt->game().position.xy.x + 1;
    troll.y = h.rt->game().position.xy.y;
    c.outdoor->enemies.push_back(troll);
    h.key(' ');
    h.run(1500);
    for (int t = 0; t < 20000 && in_combat(h) && !player_turn(h); t += 5) h.run(5);
    return in_combat(h) && player_turn(h);
}
/** Iolo alone stands, beside the troll, with 3 HP; the others already fell. */
bool stage_last_stand(Run &h) {
    auto *e = lone_enemy(h, 6, 5);
    const int far[kMembers][2] = {{0, 0}, {0, 10}, {10, 10}};
    for (int m = 0; m < kMembers; ++m)
        if (auto *a = member_actor(h, m)) {
            a->position = m == 1 ? CombatPoint{5, 5} : CombatPoint{int16_t(far[m][0]), int16_t(far[m][1])};
            a->speed = 0;
            a->defense = 0;
            if (m != 1) {
                a->status = CombatStatus::Dead;
                a->hp = 0;
                h.rt->game().party.characters[m].status = 'D';
                h.rt->game().party.characters[m].current_hp = 0;
            }
        }
    auto *iolo = member_actor(h, 1);
    if (iolo) iolo->hp = 3;
    h.rt->game().party.characters[1].current_hp = 3;
    h.render(true);
    return e && iolo;
}

/** What one death shows, frame by frame. */
struct Death {
    bool arena = false, staged = false;
    MusicSong arena_song = MusicSong::None;
    int64_t kill_us = -1, scene_us = -1, darkness_us = -1, getkey_us = -1, end_us = -1;
    bool keys_after_kill = false;            // any key between the blow and the getkey (must be none)
    MusicContext context_at_scene = MusicContext::Count;
    bool silent_every_frame = true;          // context Silence on every scene frame
    size_t starts_in_scene = 0;              // start_music calls from the scene's start to its end
    size_t stops_by_darkness = 0;            // stop_music calls from the blow to the darkness line
    MusicContext context_at_end = MusicContext::Count;
    MusicSong song_at_end = MusicSong::None;
    size_t starts_after_kill = 0;
    int last_start_after_kill = -1;
    uint8_t location_at_end = 0;
    size_t slumber = 0, thunder = 0, revival = 0;
};

/** Pass player turns (Space) until an enemy step lowers Iolo; the blow lands inside render(). */
int64_t wait_for_blow(Run &h) {
    const auto hp0 = h.rt->game().party.characters[1].current_hp;
    for (int t = 0; t < 20000 && in_combat(h); t += 5) {
        if (player_turn(h)) h.key(' ', false, false, 5000);
        else h.run(5);
        const auto &c = h.rt->game().party.characters[1];
        if (c.current_hp != hp0 || c.status != 'G') return Run::now();
    }
    return -1;
}

/** From the blow (or the world Space) to the resurrection: frames only, one Space at the getkey. */
void follow(Run &h, Death &d) {
    const auto &p = h.rt->narrative_pacer();
    const size_t mark = h.audio.calls.size();
    for (int t = 0; t < 60000 && d.getkey_us < 0; t += 5) {
        h.run(5);
        if (d.scene_us < 0 && p.active()) {
            d.scene_us = Run::now();
            d.context_at_scene = h.rt->audio().current_music_context();
        }
        if (d.scene_us >= 0 && p.active() && h.rt->audio().current_music_context() != MusicContext::Silence)
            d.silent_every_frame = false;
        if (d.darkness_us < 0 && h.transcript().find("An unending darkness") != std::string::npos) d.darkness_us = Run::now();
        if (p.awaiting_key()) d.getkey_us = Run::now();
    }
    if (d.getkey_us >= 0) h.key(' ', false, false, 5000);
    for (int t = 0; t < 60000 && p.active(); t += 5) {
        if (h.rt->audio().current_music_context() != MusicContext::Silence) d.silent_every_frame = false;
        h.run(5);
    }
    if (!p.active()) d.end_us = Run::now();
    d.context_at_end = h.rt->audio().current_music_context();
    d.song_at_end = h.rt->audio().current_song();
    d.location_at_end = uint8_t(h.rt->game().position.map.location);
    for (size_t i = mark; i < h.audio.calls.size(); ++i) {
        const auto &c = h.audio.calls[i];
        if (c.what == "music") {
            ++d.starts_after_kill;
            d.last_start_after_kill = c.value;
            if (d.scene_us >= 0 && c.us >= d.scene_us && (d.end_us < 0 || c.us < d.end_us)) ++d.starts_in_scene;
        }
        if (c.what == "stop_music" && d.darkness_us >= 0 && c.us <= d.darkness_us) ++d.stops_by_darkness;
        if (c.what == "sfx" && d.scene_us >= 0 && c.us >= d.scene_us) {
            d.slumber += c.value == int(SfxId::RefugeSlumber);
            d.thunder += c.value == int(SfxId::RefugeThunder);
            d.revival += c.value == int(SfxId::RefugeRevival);
        }
    }
}

/** A battle lost to the troll's blow. `setup` runs before the fight (mutes, settings). */
template <class F> Death combat_death(Run &h, F setup) {
    Death d;
    setup(h);
    d.arena = enter_arena(h);
    d.arena_song = h.rt->audio().current_song();
    d.staged = d.arena && stage_last_stand(h);
    d.kill_us = d.staged ? wait_for_blow(h) : -1;
    if (d.kill_us >= 0) follow(h, d);
    return d;
}

void test_combat_death() {
    std::printf("D  a party wiped in battle, then Lord British's resurrection\n");
    Run h(kParty, true, &g_audio);
    const Death d = combat_death(h, [](Run &) {});
    check(d.arena && d.arena_song == MusicSong::Engagement,
          "D0 control: the troll fight plays Engagement and Melee (the song this defect carried into the death)");
    check(d.kill_us >= 0 && d.scene_us >= 0 && !in_combat(h),
          "D1 control: the troll's blow ends the battle inside render() -- no key after it -- and the Refuge begins");
    check(d.context_at_scene == MusicContext::Silence && d.stops_by_darkness >= 1,
          std::string("D2 the music stops when the Refuge begins (context at its first frame: ") +
              music_context_name(d.context_at_scene) + "; stop_music x" + n(long(d.stops_by_darkness)) +
              " before \"An unending darkness engulfs thee...\")");
    check(d.silent_every_frame && d.starts_in_scene == 0 && d.getkey_us >= 0,
          "D3 silence through every beat, the karma getkey and the key that ends it (" + n(long(d.starts_in_scene)) +
              " songs started during the scene)");
    check(d.end_us >= 0 && d.location_at_end == 0x11 && d.context_at_end == MusicContext::Castle &&
              d.song_at_end == MusicSong::Monarch && d.starts_after_kill == 1 &&
              d.last_start_after_kill == int(MusicSong::Monarch),
          std::string("D4 at the frame the resurrection resolves (Lord British's castle), The Missing Monarch starts -- "
                      "no key needed, started once (context ") +
              music_context_name(d.context_at_end) + ", song " + music_song_title(d.song_at_end) + ", " +
              n(long(d.starts_after_kill)) + " start(s) since the blow)");
    check(d.slumber == 1 && d.thunder == 2 && d.revival == 1,
          "D5 control: the scene's sounds are unchanged: slumber x" + n(long(d.slumber)) + ", thunder x" +
              n(long(d.thunder)) + ", revival x" + n(long(d.revival)) + " (one cue carrying the member count)");

    // A session music mute (Alt+Shift+M) before the fight.
    Run m(kParty, true, &g_audio);
    const size_t before_mute = m.audio.calls.size();
    const Death dm = combat_death(m, [](Run &r) { r.key('m', true, true); });
    size_t starts = 0;
    for (size_t i = before_mute; i < m.audio.calls.size(); ++i) starts += m.audio.calls[i].what == "music";
    const bool muted = m.rt->audio().music_muted();
    m.key('m', true, true); // restore
    const MusicSong restored = m.rt->audio().current_song();
    check(dm.end_us >= 0 && muted && starts == 0 && dm.context_at_scene == MusicContext::Silence &&
              dm.context_at_end == MusicContext::Castle,
          "D6 muted: no song starts through the fight, the death or the resurrection; the context still follows "
          "(Silence in the scene, Castle after it)");
    check(restored == MusicSong::Monarch,
          std::string("D7 unmuting after the resurrection starts the castle's song, not the battle's (") +
              music_song_title(restored) + ")");
    check(dm.slumber == d.slumber && dm.thunder == d.thunder && dm.revival == d.revival,
          "D8 the music mute leaves the scene's sounds alone (same slumber / thunder / revival counts)");

    // Music Volume 0 % (settings.json), raised in the Settings page afterwards.
    tdeck::a3_host_settings_enabled() = true;
    FrontendSettings zero{};
    zero.music_volume = 0;
    encode_settings(zero, tdeck::a3_host_settings_text());
    Run z(kParty, true, &g_audio);
    const Death dz = combat_death(z, [](Run &) {});
    tdeck::a3_host_settings_enabled() = false;
    size_t zero_starts = 0;
    for (const auto &c : z.audio.calls) zero_starts += c.what == "music";
    z.key('m', true);                                 // System Menu
    for (int i = 0; i < 3; ++i) z.down();             // Settings
    z.key('\r');
    for (int i = 0; i < SystemMenuSession::kMusicVolumeRow; ++i) z.down();
    z.ball(RawInputKind::TrackballRight);             // 0 % -> 10 %
    const MusicSong raised = z.rt->audio().current_song();
    check(dz.end_us >= 0 && zero_starts == 0 && dz.context_at_end == MusicContext::Castle && raised == MusicSong::Monarch,
          std::string("D9 Music Volume 0 %: silent throughout; raised after the resurrection, the castle's song starts (") +
              music_song_title(raised) + ")");
    check(d.getkey_us - d.kill_us == dm.getkey_us - dm.kill_us && d.getkey_us - d.kill_us == dz.getkey_us - dz.kill_us &&
              d.end_us - d.getkey_us == dm.end_us - dm.getkey_us,
          "D10 the scene's timing is the same with music, muted and at 0 %: getkey " + n((d.getkey_us - d.kill_us) / 1000) +
              " ms after the blow, resolved " + n((d.end_us - d.getkey_us) / 1000) + " ms after the key");
}

void test_world_death() {
    std::printf("W  the out-of-combat Refuge (A3-HF9's path: a wiped party and one Space)\n");
    Run h(kParty, true, &g_audio);
    h.run(50);
    const MusicSong before = h.rt->audio().current_song();
    for (int i = 0; i < kMembers; ++i) {
        h.rt->game().party.characters[i].status = 'D';
        h.rt->game().party.characters[i].current_hp = 0;
    }
    h.run(10);
    Death d;
    d.kill_us = Run::now();
    h.key(' ');
    if (h.rt->narrative_pacer().active()) {
        d.scene_us = Run::now();
        d.context_at_scene = h.rt->audio().current_music_context();
    }
    follow(h, d);
    check(before != MusicSong::None && d.context_at_scene == MusicContext::Silence && d.silent_every_frame,
          std::string("W1 control: the area's song (") + music_song_title(before) +
              ") stops with the Space that raises the Refuge, silence throughout (A3-HF9 N1.13)");
    check(d.end_us >= 0 && d.location_at_end == 0x11 && d.song_at_end == MusicSong::Monarch && d.starts_after_kill == 1,
          std::string("W2 the resurrection resolves into The Missing Monarch at once, no key needed (") +
              music_song_title(d.song_at_end) + ")");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a4_ui2_death_music_runtime <pack> <openu5-audio.bin>\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load -- the music checks cannot run\n");
        return 1;
    }
    g_audio = tdeck::load_audio_pack_info(argv[2]);
    if (g_audio.state != AudioPackState::Valid) {
        std::printf("RED the audio pack is not valid -- run `npm run pack:audio`\n");
        return 1;
    }
    test_combat_death();
    test_world_death();
    std::printf("A4-UI2 death music runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
