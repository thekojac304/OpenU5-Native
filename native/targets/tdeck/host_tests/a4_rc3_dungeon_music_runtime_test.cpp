// Alpha 4 RC3 hotfix (targets/tdeck/ALPHA4_UI.md section 15) -- the music a
// dungeon plays, through the REAL AlphaRuntime (real Board, the real audio pack)
// against a recording audio backend, raw keys only.
//
// The hardware report (RC2): Developer > Teleport > Deceit left the surface music
// playing. Ordinary dungeon actions did not change it. A battle room started the
// combat song, and leaving the battle restored the SURFACE song. Leaving the
// dungeon normally restored the right overworld song; teleporting back in left it
// playing again.
//
// The cause is not the teleport. AlphaRuntime::sync_music() derived the song from
// game_.position -- the SURFACE position, which a dungeon session never rewrites
// (dungeon_input_test D-LOC-2c: "the surface return context is deliberately
// untouched") -- so an underground party was always "standing" where it entered.
// The A3-04 location tests set position.map to 0x21 by hand, a state the game
// never produces. These tests enter the dungeon the way a player does.
//
//   N  a normal dungeon entry and exit (the control: the same defect, no Developer)
//   T  Developer teleport, by the real menu keys: surface -> dungeon, dungeon ->
//      surface, dungeon -> underworld, a surface place -> a different one
//   C  teleport -> battle -> leave the battle: combat music, then the DUNGEON's
//   S  same-context teleports start nothing; a frigate source does not follow in
//   A  audio edge cases: muted, Music Volume 0 %, a stock pack with no music
//
//   a4_rc3_dungeon_music_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin> <stock audio fixture>
#include "a4_ui2_harness.h"
#include "openu5/debug_map_picker.h"
#include "openu5/frontend_settings.h"
#include "openu5/quest_state.h"

namespace tdeck {
std::string &a3_host_settings_text();
bool &a3_host_settings_enabled();
} // namespace tdeck

using namespace a4_ui2;

namespace {
AudioPackInfo g_audio{}, g_stock{};
const std::vector<Member> kParty = {{"Avatar", 'G', 200}, {"Iolo", 'G', 200}, {"Shamino", 'G', 200}};

CommandContext &ctx(Run &h) { return h.rt->command_context_for_test(); }
MusicContext context(Run &h) { return h.rt->audio().current_music_context(); }
MusicSong song(Run &h) { return h.rt->audio().current_song(); }
size_t starts(const Run &h) { return h.audio.count("music"); }
std::string what(Run &h) {
    return std::string(music_context_name(context(h))) + "/" + music_song_title(song(h));
}
bool in_dungeon(Run &h) { return h.rt->dungeon_state().active && ctx(h).dungeon; }
bool in_combat(Run &h) { return ctx(h).combat && h.rt->combat_state_for_test().initialized; }

/** The Developer > Teleport ordinal of a destination, in the picker's own order. */
size_t ordinal(Run &h, DebugDestinationKind kind, int location) {
    for (size_t i = 0; i < debug_destination_count(ctx(h)); ++i) {
        const auto d = debug_destination_at(ctx(h), i);
        if (d.error != Error::None || d.value.kind != kind) continue;
        if (kind == DebugDestinationKind::Britannia || kind == DebugDestinationKind::Underworld ||
            d.value.location == location)
            return i;
    }
    return size_t(-1);
}
constexpr DebugDestinationKind kDungeon = DebugDestinationKind::Dungeon;
constexpr DebugDestinationKind kSmall = DebugDestinationKind::SmallMap;
constexpr DebugDestinationKind kBritannia = DebugDestinationKind::Britannia;
constexpr DebugDestinationKind kUnderworld = DebugDestinationKind::Underworld;
constexpr int kDeceit = 33, kDespise = 34, kCastle = 17, kCity = 1;

/** Enter on the cursor's row, type `digits`, Enter: one numeric Developer field. */
void edit_field(Run &h, const std::string &digits) {
    h.key('\r');
    for (char c : digits) h.key(uint8_t(c));
    h.key('\r');
}
/**
 * Alt+D, Teleport, Destination <ordinal>, Teleport -- the keys a player presses.
 * Britannia has no standard entrance (the default cell is the sea at 0,0, which the
 * picker refuses), so it is entered at explicit grass coordinates, "Use default
 * entrance" off -- the way a tester reaches the overworld.
 */
void dev_teleport(Run &h, DebugDestinationKind kind, int location) {
    h.key('d', true);
    h.key('\r'); // the Teleport category
    edit_field(h, std::to_string(ordinal(h, kind, location))); // Destination
    if (kind == kBritannia) {
        h.down();
        h.down(); // X
        edit_field(h, "80");
        h.down(); // Y
        edit_field(h, "80");
        h.down(); // Use default entrance
        edit_field(h, "0");
        h.down(); // Teleport
    } else {
        for (int i = 0; i < 5; ++i) h.down();
    }
    h.key('\r'); // Teleport
}
void close_menu(Run &h) {
    h.key('\b');
    h.key('\b');
}

// --- the arena: Deceit's authored slime room, one ladder down from (5,3) ----------
/** At Deceit floor 0 (5,3), where (K)limb Down opens the authored room fight. */
bool stand_above_room(Run &h) {
    auto &d = h.rt->dungeon_state_for_test();
    if (!d.active) return false;
    d.pos.floor = 0;
    d.pos.x = 5;
    d.pos.y = 3;
    d.pos.facing = DungeonFacing::North;
    return true;
}
CombatActor *player_turn(Run &h) {
    auto &s = h.rt->combat_state_for_test();
    if (!in_combat(h) || s.current < 0 || s.current >= s.count) return nullptr;
    auto &a = s.actors[s.current];
    if (a.enemy || a.member == 255 || a.status != CombatStatus::Active) return nullptr;
    return h.rt->ui()->mode() == UiMode::Combat ? &a : nullptr;
}
/** Every conscious member steps off the top edge: the reference's own way out. */
bool walk_off(Run &h) {
    for (int t = 0; t < 4000 && in_combat(h); ++t) {
        if (auto *a = player_turn(h)) {
            a->position.y = 0;
            h.up();
        } else h.run(5);
    }
    return !in_combat(h);
}
bool fight(Run &h) {
    if (!stand_above_room(h)) return false;
    h.key('k');
    h.run(1500);
    for (int t = 0; t < 4000 && in_combat(h) && !player_turn(h); t += 5) h.run(5);
    return in_combat(h);
}

/** Britannia's open grass, as a journey begins; one key poll derives the song. */
void place_on_surface(Run &h) {
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {80, 80};
    h.run(50);
    h.ball(RawInputKind::TrackballDown);
    h.run(50);
}
void start_on_surface(Run &h, const char *label) {
    place_on_surface(h);
    check(context(h) == MusicContext::Overworld && song(h) == MusicSong::BritannicLands,
          std::string(label) + ": the party starts on the surface with Britannic Lands playing (" + what(h) + ")");
}
// --- N: normal entry and exit ---------------------------------------------------------
void test_normal() {
    std::printf("N  a normal dungeon entry and exit (no Developer menu anywhere)\n");
    Run h(kParty, false, &g_audio);
    start_on_surface(h, "N0");
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {pack->location_x[kDeceit - 1], pack->location_y[kDeceit - 1]};
    set_quest_flag(g.quest, QuestFlag::Word33);
    h.key('e');
    check(in_dungeon(h) && h.rt->dungeon_state().pos.dungeon == kDeceit,
          "N1 control: (E)nter at Deceit's mouth opens the real dungeon session");
    check(context(h) == MusicContext::Dungeon && song(h) == MusicSong::HallsOfDoom,
          "N2 entering Deceit by the game's own command plays Halls of Doom, with no further key (" + what(h) + ")");
    h.ball(RawInputKind::TrackballLeft); // ordinary dungeon actions: turn about
    h.ball(RawInputKind::TrackballLeft);
    check(context(h) == MusicContext::Dungeon && song(h) == MusicSong::HallsOfDoom,
          "N3 and ordinary dungeon actions keep it (" + what(h) + ")");
    h.key('s', true); // Alt+S: save inside the dungeon
    // Climb out: the entry cell is on floor 0; the authored way up is a 0x10 ladder.
    auto &d = h.rt->dungeon_state_for_test();
    bool found = false;
    for (int y = 0; y < 8 && !found; ++y)
        for (int x = 0; x < 8 && !found; ++x)
            if ((d.cells[y * 8 + x] >> 4) == 1) {
                d.pos.floor = 0;
                d.pos.x = uint8_t(x);
                d.pos.y = uint8_t(y);
                found = true;
            }
    h.key('k');
    if (in_dungeon(h)) h.key('u'); // the up/down prompt, where the cell offers both
    check(found && !in_dungeon(h) && context(h) == MusicContext::Overworld && song(h) == MusicSong::BritannicLands,
          "N4 climbing out restores Britannic Lands (" + what(h) + ")");
    h.key('l', true); // Alt+L: back into the saved dungeon
    h.run(500);
    check(in_dungeon(h) && context(h) == MusicContext::Dungeon && song(h) == MusicSong::HallsOfDoom,
          "N5 loading the dungeon save from the surface resumes Halls of Doom (" + what(h) + ")");
}

// --- T: Developer teleport -----------------------------------------------------------
void test_teleport() {
    std::printf("T  Developer teleport\n");
    Run h(kParty, false, &g_audio);
    start_on_surface(h, "T0");
    const size_t before = starts(h);
    dev_teleport(h, kDungeon, kDeceit);
    check(in_dungeon(h) && h.rt->dungeon_state().pos.dungeon == kDeceit,
          "T1 control: Developer > Teleport > Deceit lands in the real dungeon session");
    check(context(h) == MusicContext::Dungeon && song(h) == MusicSong::HallsOfDoom && starts(h) == before + 1,
          "T2 surface -> dungeon: Halls of Doom starts at once, while the menu is still open, exactly once (" +
              what(h) + ", starts +" + n(long(starts(h) - before)) + ")");
    close_menu(h);
    for (int i = 0; i < 4; ++i) h.ball(RawInputKind::TrackballRight); // turn on the spot
    check(context(h) == MusicContext::Dungeon && song(h) == MusicSong::HallsOfDoom && starts(h) == before + 1,
          "T3 closing the menu and taking ordinary dungeon actions keeps it, with no restart (" + what(h) + ")");

    dev_teleport(h, kBritannia, 0);
    check(!in_dungeon(h) && context(h) == MusicContext::Overworld && song(h) == MusicSong::BritannicLands,
          "T4 dungeon -> Britannia: Britannic Lands at once (" + what(h) + ")");
    close_menu(h);
    dev_teleport(h, kDungeon, kDeceit);
    check(context(h) == MusicContext::Dungeon && song(h) == MusicSong::HallsOfDoom,
          "T5 and teleporting back in plays the dungeon's song again, not the overworld's (" + what(h) + ")");
    close_menu(h);
    dev_teleport(h, kUnderworld, 0);
    check(!in_dungeon(h) && context(h) == MusicContext::Underworld && song(h) == MusicSong::WorldsBelow,
          "T6 dungeon -> Underworld: Worlds Below (" + what(h) + ")");
    close_menu(h);

    dev_teleport(h, kSmall, kCastle);
    check(context(h) == MusicContext::Castle && song(h) == MusicSong::Monarch,
          "T7 Underworld -> Lord British's castle: The Missing Monarch (" + what(h) + ")");
    close_menu(h);
    dev_teleport(h, kSmall, kCity);
    check(context(h) == MusicContext::Cities && song(h) == MusicSong::Tarantella,
          "T8 castle -> a City of Virtue: Villager Tarantella (" + what(h) + ")");
    close_menu(h);
    dev_teleport(h, kBritannia, 0);
    check(context(h) == MusicContext::Overworld && song(h) == MusicSong::BritannicLands,
          "T9 a city -> Britannia: Britannic Lands (" + what(h) + ")");
}

// --- C: teleport -> battle -> leave the battle ---------------------------------------
void test_combat() {
    std::printf("C  the hardware sequence: teleport, fight, leave the fight\n");
    Run h(kParty, false, &g_audio);
    start_on_surface(h, "C0");
    dev_teleport(h, kDungeon, kDeceit);
    close_menu(h);
    check(context(h) == MusicContext::Dungeon && song(h) == MusicSong::HallsOfDoom,
          "C1 teleported into Deceit: Halls of Doom (" + what(h) + ")");
    const bool fighting = fight(h);
    check(fighting && context(h) == MusicContext::Combat && song(h) == MusicSong::Engagement,
          "C2 a dungeon room fight plays the combat song (" + what(h) + ")");
    const bool left = walk_off(h);
    h.run(100);
    check(left && !in_combat(h) && in_dungeon(h),
          "C3 control: the party leaves the room by the board edge and is back in the dungeon");
    check(context(h) == MusicContext::Dungeon && song(h) == MusicSong::HallsOfDoom,
          "C4 leaving the battle restores the DUNGEON's song, not the surface song the party left (" + what(h) + ")");
    h.ball(RawInputKind::TrackballLeft);
    check(song(h) == MusicSong::HallsOfDoom, "C5 and the next key leaves it there (" + what(h) + ")");
}

// --- S: same context / source state -------------------------------------------------
void test_same_context() {
    std::printf("S  repeated teleports and a stale source\n");
    Run h(kParty, false, &g_audio);
    start_on_surface(h, "S0");
    dev_teleport(h, kDungeon, kDeceit);
    close_menu(h);
    const size_t after_first = starts(h);
    dev_teleport(h, kDungeon, kDeceit);
    close_menu(h);
    dev_teleport(h, kDungeon, kDespise);
    close_menu(h);
    check(h.rt->dungeon_state().pos.dungeon == kDespise && context(h) == MusicContext::Dungeon &&
              song(h) == MusicSong::HallsOfDoom && starts(h) == after_first,
          "S1 Deceit -> Deceit -> Despise: the same effective context, so no further start_music (starts +" +
              n(long(starts(h) - after_first)) + ")");
    dev_teleport(h, kBritannia, 0);
    close_menu(h);
    dev_teleport(h, kBritannia, 0);
    check(context(h) == MusicContext::Overworld && song(h) == MusicSong::BritannicLands,
          "S2 Britannia -> Britannia stays Britannic Lands (" + what(h) + ")");

    // A party that teleports off a frigate has left it: the dungeon does not inherit the ship's song.
    Run f(kParty, false, &g_audio);
    start_on_surface(f, "S3a");
    f.rt->turn().transport_tile = 0x20;
    f.key(' '); // a poll with the frigate tile showing
    const MusicSong afloat = song(f);
    dev_teleport(f, kDungeon, kDeceit);
    check(afloat == MusicSong::Hornpipe && context(f) == MusicContext::Dungeon && song(f) == MusicSong::HallsOfDoom,
          std::string("S3 from a frigate (") + music_song_title(afloat) + ") into Deceit: Halls of Doom, not the ship's song (" +
              what(f) + ")");
}

// --- A: audio edge cases ---------------------------------------------------------------
void test_audio_edges() {
    std::printf("A  muted, Music Volume 0 %%, a stock pack\n");
    {
        Run h(kParty, false, &g_audio);
        start_on_surface(h, "A0");
        h.key('m', true, true); // Alt+Shift+M: mute music for the session
        const size_t base = starts(h);
        dev_teleport(h, kDungeon, kDeceit);
        close_menu(h);
        check(h.rt->audio().music_muted() && starts(h) == base && context(h) == MusicContext::Dungeon,
              "A1 muted: teleport into Deceit starts nothing, and the context is the dungeon's (" + what(h) + ")");
        h.key('m', true, true); // unmute
        check(song(h) == MusicSong::HallsOfDoom && starts(h) == base + 1,
              "A2 unmuting plays the destination's song, not the surface's (" + what(h) + ")");
    }
    {
        tdeck::a3_host_settings_enabled() = true;
        FrontendSettings zero{};
        zero.music_volume = 0;
        zero.trackball_speed = openu5::kTrackballSpeedLegacy; // the one-pulse-per-step speed these routes assume
        encode_settings(zero, tdeck::a3_host_settings_text());
        Run z(kParty, false, &g_audio);
        tdeck::a3_host_settings_enabled() = false;
        place_on_surface(z);
        dev_teleport(z, kDungeon, kDeceit);
        close_menu(z);
        check(starts(z) == 0 && context(z) == MusicContext::Dungeon,
              "A3 Music Volume 0 %: teleport into Deceit is silent and the context is the dungeon's (" + what(z) + ")");
        z.key('m', true); // System Menu > Settings > Music Volume
        for (int i = 0; i < 3; ++i) z.down();
        z.key('\r');
        for (int i = 0; i < SystemMenuSession::kMusicVolumeRow; ++i) z.down();
        z.ball(RawInputKind::TrackballRight); // 0 % -> 10 %
        check(song(z) == MusicSong::HallsOfDoom,
              "A4 raising the volume plays the destination's song, not the surface's (" + what(z) + ")");
    }
    {
        Run s(kParty, false, &g_stock);
        place_on_surface(s);
        dev_teleport(s, kDungeon, kDeceit);
        close_menu(s);
        dev_teleport(s, kBritannia, 0);
        check(!s.rt->audio().has_music() && s.audio.count("music") == 0 && s.audio.count("stop_music") == 0 &&
                  context(s) == MusicContext::Overworld,
              "A5 a stock pack (no music): the teleports call neither start_music nor stop_music; the context still follows");
        close_menu(s);
        dev_teleport(s, kDungeon, kDeceit);
        check(context(s) == MusicContext::Dungeon && s.audio.count("music") == 0 && s.audio.count("stop_music") == 0,
              "A6 and into a dungeon: the context is the dungeon's, nothing is invented (" + what(s) + ")");
    }
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: a4_rc3_dungeon_music_runtime <pack> <openu5-audio.bin> <stock fixture>\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load -- the music checks cannot run\n");
        return 1;
    }
    g_audio = tdeck::load_audio_pack_info(argv[2]);
    g_stock = tdeck::load_audio_pack_info(argv[3]);
    if (g_audio.state != AudioPackState::Valid) {
        std::printf("RED the audio pack is not valid -- run `npm run pack:audio`\n");
        return 1;
    }
    // Deceit's room 0 is a dungeon room: the fixture needs the arenas (batch53a's table).
    static std::vector<DungeonArena> arenas;
    for (size_t i = 0; i < pack->combat_map_count; ++i) arenas.push_back({pack->combat_map_views[i], pack->combat_sprites + i * 16});
    g_arenas = arenas.data();
    g_arena_count = arenas.size();
    test_normal();
    test_teleport();
    test_combat();
    test_same_context();
    test_audio_edges();
    std::printf("A4-RC3 dungeon music runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
