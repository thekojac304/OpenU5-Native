// Batch 4.5D -- Blackthorn capture/interrogation SCENE presentation (audit
// R-32).
//
// Before this batch the T-Deck played the whole capture as plain transcript
// text over the ordinary Palace-lobby world view: no blindfold, no throne
// room, Blackthorn never visibly present, no sacrifice staging, and the party
// simply appearing in the jail. The original (BLCKTHRN.OVL 0x060e plus its
// anim_vm cutscene interpreter at 0x00be) and the TypeScript reference
// (game/src/core/world/blackthorn-scene.ts + ui/blackthorn-scene-pacer.ts)
// both stage it properly.
//
// Every case here drives the REAL entry points -- blackthorn_turn_effect's
// hostile-guard Capture trigger and blackthorn_action(Answer) -- through the
// real UiSession, with the throne-room grid read out of the REAL packed
// resource file, never a hand-typed copy.
//
// T1  scene begins on capture, world presentation stops being the source,
//     and no gameplay position is rewritten merely to draw it
// T2  beats arrive in the reference order (blackout -> drag -> throne)
// T3  Blackthorn is visibly staged at the interrogation phase
// T4  the narrative is paced, not dumped in one burst
// T5  the interrogation prompt is live with the scene still up
// T6  a wrong answer reaches the warning consequence phase
// T7  the sacrifice stages the victim at the same point the roster mutates
// T8  the scene exits cleanly and the party is in the jail
// T9  transcript paging during the scene changes nothing else
// T10 the immediate-subdual branch still skips the scene entirely
// T11 the packed blackthorn-scene.bin resource resolves and matches the room
#include "openu5/blackthorn.h"
#include "openu5/blackthorn_scene.h"
#include "openu5/commands.h"
#include "openu5/presentation.h"
#include "openu5/save_json.h"
#include "openu5/shrine.h"
#include "openu5/ui_session.h"
#include "openu5/world.h"

#include "misc_records.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const std::string &what) {
    ++checks;
    std::fprintf(stderr, "[%s] blackthorn_scene check %d: %s\n", ok ? "PASS" : "FAIL", checks,
                 what.c_str());
    if (!ok) ++failures;
}

std::vector<std::string> load_miscmsg(const char *path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    save::Json doc;
    if (!in || save::parse_json(buf.str(), doc) != save::JsonError::None) return {};
    std::vector<std::string> out;
    for (const auto &row : doc["MISCMSG.DAT"].values) {
        std::string ascii;
        for (char16_t c : row.string) {
            if (c > 127) return {};
            ascii += char(c);
        }
        out.push_back(std::move(ascii));
    }
    return out;
}

uint32_t u32le(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

// The throne room as the REAL pack carries it. Reading it out of
// openu5-alpha1-resources.bin (rather than embedding a copy) is also T11:
// if the packer entry is missing, malformed or the wrong shape, every scene
// case below fails loudly instead of silently testing a private literal.
struct PackedRoom {
    std::vector<int16_t> tiles;
    bool found = false, valid = false;
};
PackedRoom load_packed_room(const char *pack_path) {
    PackedRoom room;
    std::ifstream in(pack_path, std::ios::binary);
    if (!in) return room;
    std::ostringstream buf;
    buf << in.rdbuf();
    const std::string blob = buf.str();
    const auto *bytes = reinterpret_cast<const uint8_t *>(blob.data());
    if (blob.size() < 32 || std::memcmp(bytes, "OU5A1RES", 8) != 0) return room;
    const uint32_t header = uint32_t(bytes[12]) | uint32_t(bytes[13]) << 8;
    const uint32_t stride = uint32_t(bytes[14]) | uint32_t(bytes[15]) << 8;
    const uint32_t count = u32le(bytes + 16);
    for (uint32_t i = 0; i < count; ++i) {
        const size_t at = header + size_t(i) * stride;
        if (at + stride > blob.size()) break;
        if (std::strncmp(reinterpret_cast<const char *>(bytes + at), "blackthorn-scene.bin", 31) != 0)
            continue;
        room.found = true;
        const uint32_t offset = u32le(bytes + at + 32), length = u32le(bytes + at + 36);
        if (offset + length > blob.size() || length < 8) return room;
        const uint8_t *entry = bytes + offset;
        const uint32_t cols = u32le(entry), rows = u32le(entry + 4);
        if (cols != uint32_t(kBlackthornSceneCols) || rows != uint32_t(kBlackthornSceneRows)) return room;
        if (length != 8 + size_t(cols) * rows * sizeof(int16_t)) return room;
        room.tiles.resize(size_t(cols) * rows);
        for (size_t c = 0; c < room.tiles.size(); ++c)
            room.tiles[c] = int16_t(uint16_t(entry[8 + c * 2]) | uint16_t(entry[9 + c * 2]) << 8);
        room.valid = true;
        return room;
    }
    return room;
}

// The Palace of Blackthorn (location 18), an adjacent hostile guard, and the
// production Blackthorn/shrine/scene wiring -- the same shape
// native/core/tests/batch45b_test.cpp's fixture uses, plus the scene services
// AlphaRuntime supplies on hardware and a UiSession behind a scene pacer, so
// the modal/prompt/transcript assertions observe the real chain.
struct World {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    NpcActors actors{};
    BlackthornSession blackthorn{};
    ShrineSession shrine{};
    ShrineData shrine_data{};
    ShrineServices shrine_services{shrine};
    BlackthornSceneState scene_state{};
    BlackthornSceneScript scene_script{};
    BlackthornSceneServices scene_services{};
    std::vector<uint8_t> large = std::vector<uint8_t>(65536, 5), small = std::vector<uint8_t>(1024, 5);
    MapData local{};
    WorldData world_data{};
    CommandContext ctx;

    // Presentation side.
    BlackthornScenePacer pacer{};
    std::vector<BlackthornSceneStep> steps = std::vector<BlackthornSceneStep>(160);
    std::vector<char> text = std::vector<char>(8192);
    std::vector<int16_t> grid = std::vector<int16_t>(kBlackthornSceneCells);
    UiTextBlock blocks[128]{};
    UiSession *ui = nullptr;

    // What actually reached the session, in release order.
    std::vector<std::pair<GameEventKind, std::string>> released;
    int explosion_dx = 0, explosion_dy = 0, explosion_bursts = 0;

    explicit World(const std::vector<std::string> &miscmsg, const int16_t *room)
        : ctx(game, turn, travel, commands, world_data) {
        local = MapData{{18, 0}, small.data(), small.size()};
        world_data = WorldData{large.data(), large.data(), large.size(), large.size(), &local, 1};
        ctx.actors = &actors;
        ctx.blackthorn = &blackthorn;
        game.position.map = {18, 0};
        game.position.xy = {10, 10};
        game.time.hour = 10;
        game.party.party_size = game.party.character_count = 2;
        std::strcpy(game.party.characters[0].name, "Avatar");
        std::strcpy(game.party.characters[1].name, "Iolo");
        game.party.characters[0].character_class = 'A';
        game.party.characters[1].character_class = 'B';
        game.party.characters[0].status = game.party.characters[1].status = 'G';

        records = miscmsg;
        shrine_data.count = 1;
        shrine_data.virtues[0] = {kVirtue, 10};
        shrine_data.mantras[0] = {kMantra, 2};
        shrine_services.data = &shrine_data;
        shrine_services.context = this;
        shrine_services.record = [](void *p, int32_t i) -> const char * {
            auto &w = *static_cast<World *>(p);
            return i >= 0 && size_t(i) < w.records.size() ? w.records[size_t(i)].c_str() : nullptr;
        };
        ctx.shrine_services = &shrine_services;

        if (room) {
            scene_services.capture_tiles = room;
            scene_services.state = &scene_state;
            scene_services.script = &scene_script;
            ctx.blackthorn_scene = &scene_services;
        }

        ui = new UiSession({blocks, 128});
        ui->set_base_mode(UiMode::Exploration);
        pacer.attach({steps.data(), steps.size(), text.data(), text.size(), grid.data()});
        pacer.set_unit_ms(55);
        ctx.events = {this, &World::on_event};
    }
    ~World() { delete ui; }
    World(const World &) = delete;
    World &operator=(const World &) = delete;

    static constexpr char16_t kVirtue[] = u"Compassion";
    static constexpr char16_t kMantra[] = u"MU";
    std::vector<std::string> records;

    // The exact AlphaRuntime rule: offer every event to the pacer first, and
    // only hand it to the session when the pacer did not take it.
    static void on_event(void *p, const GameEvent &e) {
        auto &w = *static_cast<World *>(p);
        if (w.pacer.enqueue(e)) return;
        w.deliver(e);
    }
    void deliver(const GameEvent &e) {
        released.emplace_back(e.kind, e.text ? e.text : "");
        if (e.kind == GameEventKind::CellExplosion) {
            explosion_dx = e.cell_fx.dx;
            explosion_dy = e.cell_fx.dy;
            explosion_bursts = e.cell_fx.bursts;
        }
        ui->consume(e);
    }
    EventSink release_sink() {
        return {this, [](void *p, const GameEvent &e) { static_cast<World *>(p)->deliver(e); }};
    }
    void pump(uint32_t now_ms) { pacer.pump(now_ms, release_sink()); }
    /** Tick the pacer at its own 55 ms unit for `units` units of scene time. */
    void advance(uint32_t &clock, int units) {
        for (int i = 0; i < units; ++i) {
            clock += 55;
            pump(clock);
        }
    }
    /** Run the clock forward far enough to drain every timed beat. */
    void drain(uint32_t &clock, uint32_t steps_ms = 55, int iterations = 4000) {
        for (int i = 0; i < iterations; ++i) {
            if (pacer.state() != BlackthornPacerState::Running) {
                if (!pacer.awaiting_key()) break;
                pacer.advance_key();
            }
            clock += steps_ms;
            pump(clock);
        }
    }
    NpcActor &add_guard() {
        auto &n = actors.actors[actors.count++];
        n.location = 18;
        n.z = 0;
        n.x = 10;
        n.y = 9;
        n.schedule.slot = 7;
        n.schedule.type = 112;
        n.schedule.dialog = 255;
        for (auto &t : n.schedule.times) t = uint8_t(game.time.hour);
        n.schedule.ai[0] = 6;
        return n;
    }
    size_t messages() const {
        size_t n = 0;
        for (const auto &e : released) n += e.first == GameEventKind::Message;
        return n;
    }
    bool saw(GameEventKind kind, const char *needle) const {
        for (const auto &e : released)
            if (e.first == kind && e.second.find(needle) != std::string::npos) return true;
        return false;
    }
    std::string page_text() const {
        std::string out;
        UiRenderedLine lines[128]{};
        const auto n = ui->visible_lines(lines, 128, 40);
        for (size_t i = 0; i < n; ++i) out += std::string(lines[i].text, lines[i].length) + "\n";
        return out;
    }
    /** Everything still reachable by scrolling back, gathered the way a player
     *  would reach it: page up to the top, then page back down again. */
    std::string reviewable_transcript() {
        std::string out = page_text();
        UiAction up{};
        up.kind = UiActionKind::PageUp;
        UiAction down{};
        down.kind = UiActionKind::PageDown;
        size_t pages = 0;
        for (;;) {
            const auto before = ui->scroll_offset_lines();
            ui->handle_input(up);
            if (ui->scroll_offset_lines() == before || ++pages > 64) break;
            out += page_text();
        }
        for (size_t i = 0; i <= pages; ++i) ui->handle_input(down);
        return out;
    }
};
constexpr char16_t World::kVirtue[];
constexpr char16_t World::kMantra[];

Rand no_rand() {
    return {nullptr, [](void *, int32_t lo, int32_t) { return lo; }};
}

const BlackthornStageSlot &slot_of(const BlackthornSceneView &v, int slot) {
    return v.stage.slots[slot];
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 3) {
        std::fprintf(stderr,
                     "usage: blackthorn_scene_tests <game/assets/ds-strings.json> "
                     "<native/assets/openu5-alpha1-resources.bin>\n");
        return 2;
    }
    const auto miscmsg = load_miscmsg(argv[1]);
    if (miscmsg.size() < 45) {
        std::fprintf(stderr, "could not load MISCMSG.DAT records from %s\n", argv[1]);
        return 2;
    }

    // ---- T11: the packed room resource.
    const auto room = load_packed_room(argv[2]);
    check(room.found,
          "T11: the alpha resource pack carries a `blackthorn-scene.bin` entry -- the throne-room "
          "grid was never packed for the native engine before this batch");
    check(room.valid,
          "T11: `blackthorn-scene.bin` decodes as the 11x11 int16 room AlphaResourcePack::load() "
          "expects (cols, rows, then cols*rows tiles)");
    if (!room.valid) {
        std::fprintf(stderr, "blackthorn_scene: %d checks, %d failures\n", checks, failures);
        return 1;
    }
    // Anchors from MISCMAPS.DAT record 0, independent of the packer: the two
    // throne manacles the Avatar and the first companion are chained to, and
    // the empty torture chair the sacrifice leaves behind.
    check(room.tiles[5 * 11 + 3] == 0x85 && room.tiles[5 * 11 + 7] == 0x85,
          "T11: the packed room has the two manacle cells (0x85) at (3,5) and (7,5) the seating "
          "table places the Avatar and the first companion on");
    check(room.tiles[7 * 11 + 5] == kBlackthornTortureAfterTile,
          "T11: the packed room has the torture chair (0x80) at (5,7)");

    uint32_t clock = 1000;

    // ---- T1 / T2 / T3 / T4: the capture itself.
    {
        World w(miscmsg, room.tiles.data());
        w.add_guard();
        const auto position_before = w.game.position;

        const bool consumed =
            blackthorn_turn_effect(w.ctx, CommandEffect::Capture, w.ctx.events, no_rand());
        check(consumed, "T1: the real hostile-guard Capture trigger still fires");
        check(w.pacer.active() && w.pacer.state() == BlackthornPacerState::Running,
              "T1: the capture puts a scene presentation on the air (RED before this batch: no "
              "scene state existed at all)");

        // T4: the burst has NOT happened. Only the first print is out; every
        // later line is still behind its beats.
        w.pump(clock);
        const auto after_first_pump = w.messages();
        check(after_first_pump == 1,
              "T4: exactly one narrative line is released before the blindfold beats run -- the "
              "old behaviour emitted all seven plus the prompt in one synchronous burst");
        check(w.saw(GameEventKind::Message, "subdued and blindfolded"),
              "T4: and that one line is the blindfold print (BLCKTHRN 0x0652)");

        // T1: the view is now the scene, not the ordinary world.
        auto view = w.pacer.view();
        check(view.phase == BlackthornScenePhase::Blackout,
              "T1/T2: the first beat is the blindfold blackout (0x0676 set_color(0) + 0x0689 "
              "fill_rect), not the Palace lobby");
        {
            const auto snapshot = compose_blackthorn_presentation(view);
            bool all_hidden = true;
            for (int i = 0; i < kPresentationCells; ++i)
                all_hidden = all_hidden && snapshot.tiles[i] == kPresentationHidden;
            check(all_hidden,
                  "T1: the composed presentation for the blackout phase is an entirely blanked "
                  "window -- the ordinary world snapshot is no longer the render source");
        }
        check(w.game.position.map.location == position_before.map.location &&
                  w.game.position.map.floor == position_before.map.floor &&
                  w.game.position.xy.x == position_before.xy.x &&
                  w.game.position.xy.y == position_before.xy.y,
              "T1: the party's semantic position is untouched by staging the scene -- the room is "
              "drawn, the world is not teleported");

        // T4 continued: the second line only arrives after the blackout's own
        // beats (pause(2) + five delay_ticks(5) drags = 27 units) elapse.
        w.advance(clock, 20);
        check(w.messages() == 1,
              "T4: the drag line is still withheld partway through the blindfold beats");
        w.advance(clock, 12);
        check(w.messages() == 2 && w.saw(GameEventKind::Message, "drag thee away"),
              "T4: the drag line (0x06b0) appears only once the blindfold beats complete -- paced, "
              "not dumped");

        // T2: the room mounts only after the eighteen further drags.
        check(w.pacer.view().phase == BlackthornScenePhase::Blackout,
              "T2: the room has not mounted yet -- eighteen more drags come first (0x06b9-0x06e0)");
        w.drain(clock);
        view = w.pacer.view();
        check(view.phase == BlackthornScenePhase::Throne && view.tiles,
              "T2: the throne room mounts (0x06e5-0x07c9) after the drag beats, replacing the "
              "blackout");
        check(view.tiles && view.tiles[5 * 11 + 3] == 0x85,
              "T2: the mounted room is the packed MISCMAPS room, not an improvised grid");

        // T3: everybody is staged where the binary stages them.
        check(slot_of(view, 0).visible && slot_of(view, 0).x == 3 && slot_of(view, 0).y == 5,
              "T3: the Avatar is chained in the throne-side manacle at (3,5)");
        check(slot_of(view, 1).visible && slot_of(view, 1).x == 7 && slot_of(view, 1).y == 5,
              "T3: the companion is chained at (7,5)");
        check(slot_of(view, 0).tile == 0x14c && slot_of(view, 1).tile == 0x144,
              "T3: both are staged with their CLASS sprite (Avatar1 / Bard1), the DS 0x1ade table");
        check(slot_of(view, 6).visible && slot_of(view, 6).x == 1 && slot_of(view, 6).y == 5 &&
                  slot_of(view, 6).tile == kBlackthornGuardTile,
              "T3: guard A marched in and has stepped out towards the Avatar (anim_vm 0x370e)");
        check(slot_of(view, 7).visible && slot_of(view, 7).x == 9 && slot_of(view, 7).y == 9,
              "T3: guard B is posted behind the prisoners at (9,9) (anim_vm 0x3702)");
        check(slot_of(view, 8).visible && slot_of(view, 8).tile == kBlackthornTile &&
                  slot_of(view, 8).x == 5 && slot_of(view, 8).y == 5,
              "T3: BLACKTHORN HIMSELF is visibly staged before the throne at (5,5) with his own "
              "sprite (0x178) -- the headline R-32 defect: he was never staged at all");
        {
            const auto snapshot = compose_blackthorn_presentation(view);
            check(snapshot.tiles[5 * 11 + 5] == kBlackthornTile,
                  "T3: and the composed presentation actually paints him into the window");
        }

        // ---- T5: the interrogation prompt, with the scene still up.
        check(w.pacer.mounted() && w.pacer.state() == BlackthornPacerState::Prompt,
              "T5: the scene stays mounted while the question waits (the reference keeps the "
              "throne room behind every round)");
        check(w.ui->mode() == UiMode::TextEntry && w.ui->request() == UiRequestId::Blackthorn,
              "T5: the BlackthornPrompt modal is live");
        check(std::string(w.ui->prompt()) == "Your response?",
              "T5: and its prompt is still exactly \"Your response?\"");
        const auto text = w.reviewable_transcript();
        check(text.find("blindfolded") != std::string::npos &&
                  text.find("Footsteps") != std::string::npos,
              "T5: the whole prior narrative is still reviewable by scrolling the transcript back");
        check(w.saw(GameEventKind::BlackthornPrompt, "Mystic Shrine of Compassion"),
              "T5: the real interrogation question reached the session");

        // ---- T9: paging during the live scene.
        const auto mode_before = w.ui->mode();
        const auto request_before = w.ui->request();
        const auto phase_before = w.pacer.view().phase;
        const auto released_before = w.released.size();
        UiAction page{};
        page.kind = UiActionKind::PageUp;
        w.ui->handle_input(page);
        page.kind = UiActionKind::PageDown;
        w.ui->handle_input(page);
        check(w.pacer.mounted() && w.pacer.view().phase == phase_before,
              "T9: paging the transcript leaves the scene mounted and on the same beat");
        check(w.ui->mode() == mode_before && w.ui->request() == request_before &&
                  std::string(w.ui->prompt()) == "Your response?",
              "T9: and leaves the text-entry modal and its prompt intact");
        check(w.released.size() == released_before,
              "T9: paging dispatches no command and advances no scene step");

        // ---- T6: a deterministic wrong answer.
        static constexpr char16_t kWrong[] = u"NOPE";
        const auto messages_before = w.messages();
        const auto status = blackthorn_action(w.ctx, BlackthornAction::Answer, {kWrong, 4}, false,
                                              w.ctx.events, no_rand());
        check(status == CommandStatus::AwaitingResponse && w.blackthorn.round == 1,
              "T6: the wrong answer advances the real interrogation to round 1");
        check(w.pacer.state() == BlackthornPacerState::Running,
              "T6: the consequence is staged, not dumped -- the warning segment takes the air");
        check(w.messages() == messages_before,
              "T6: nothing of the consequence is released synchronously -- the whole tail is "
              "deferred behind the scene, which is exactly what stops the burst");
        w.advance(clock, 1);
        check(w.messages() == messages_before + 1 && !w.saw(GameEventKind::Message, "Iolo die!"),
              "T6: the first tick releases only the first half of the warning (rec7); rec8 waits "
              "behind anim_vm 0x36da, exactly as the binary interleaves them at 0x0523");
        w.drain(clock);
        view = w.pacer.view();
        check(view.tiles && view.tiles[7 * 11 + 5] == kBlackthornTortureBodyTile,
              "T6/T7: the companion has been dragged to the torture table and the cell is now the "
              "chained body (0x82)");
        check(!slot_of(view, 1).visible && slot_of(view, 1).x == 5 && slot_of(view, 1).y == 7,
              "T7: the victim's object is cleared but its coordinates are RETAINED at the table "
              "(anim_vm op 9) -- what the sacrifice's explosion later aims at");
        check(view.tiles && view.tiles[9 * 11 + 5] == kBlackthornHourglassFullTile,
              "T6: the hourglass is planted full at (5,9)");
        check(w.saw(GameEventKind::Message, "Iolo die!"),
              "T6: and the second half of the warning names the real companion");
        check(w.ui->mode() == UiMode::TextEntry && std::string(w.ui->prompt()) == "Your response?",
              "T6: round 1's question is prompted with the scene still active");

        // ---- T7 / T8: correct answer -> sacrifice staging -> jail.
        static constexpr char16_t kRight[] = u"MU";
        const auto roster_before = w.game.party.party_size;
        const bool victim_alive_before = w.game.party.characters[1].status != 'D';
        const auto result = blackthorn_action(w.ctx, BlackthornAction::Answer, {kRight, 2}, false,
                                              w.ctx.events, no_rand());
        check(result == CommandStatus::Success && w.blackthorn.shrine == -1,
              "T7: the matching mantra resolves the interrogation through the real engine");
        check(w.game.party.party_size == roster_before - 1 && victim_alive_before,
              "T7: the semantic roster mutation happens at the same reference point it always did "
              "(sacrifice_member), untouched by the staging -- R-23 is neither fixed nor changed "
              "here");
        int victim_x = 0, victim_y = 0;
        sacrifice_victim_cell(w.scene_state, victim_x, victim_y);
        check(victim_x == 5 && victim_y == 7,
              "T7: the sacrifice targets the torture table the victim was dragged to, not the "
              "empty manacle he was originally chained in");
        w.drain(clock);
        check(w.saw(GameEventKind::CellExplosion, ""),
              "T7: the sacrifice raises the explosion FX the port had no event for at all");
        // The explosion aims at slot 1's retained coordinates -- the table at
        // (5,7), i.e. dx=0, dy=+2 from the window's (5,5) centre.
        check(w.explosion_dx == 0 && w.explosion_dy == 2 && w.explosion_bursts == 1,
              "T7: the explosion burst is aimed at that cell as an offset from the scene centre, "
              "one burst, exactly as explosion_fx_at_cell is called at 0x041e");
        check(w.pacer.view().tiles == nullptr || !w.pacer.mounted(),
              "T8: the escorted-exit finale (anim_vm 0x369e) takes the scene back down");
        check(!w.pacer.active() && w.pacer.state() == BlackthornPacerState::Idle,
              "T8: the pacer returns to idle -- the ordinary world renderer resumes and the scene "
              "cannot be left stranded");
        check(w.game.position.map.location == 18 && w.game.position.map.floor == -1 &&
                  w.game.position.xy.x == 10 && w.game.position.xy.y == 7,
              "T8: the party lands in the jail at the deterministic (10,7), location 18, floor -1");
        check(w.saw(GameEventKind::MapChanged, "") && w.saw(GameEventKind::PartyChanged, ""),
              "T8: and the deferred map/party events are released after the scene, never before");
        check(w.pacer.dropped_steps() == 0,
              "T8: nothing was dropped -- the deferred queue and text arena were large enough for "
              "the whole sequence");
    }

    // ---- T10: the immediate-subdual branch, which has no interrogation and
    // therefore no scene, must be completely unchanged.
    {
        World w(miscmsg, room.tiles.data());
        w.add_guard();
        for (int i = 0; i < 8; ++i) w.game.quest.shrine_destroyed[i] = 255;
        w.game.quest.destroyed_count = 8;
        const bool consumed =
            blackthorn_turn_effect(w.ctx, CommandEffect::Capture, w.ctx.events, no_rand());
        check(consumed, "T10: the subdual branch still fires from the same trigger");
        check(!w.pacer.active() && !w.pacer.mounted(),
              "T10: with every shrine already fallen there is no throne scene at all (BLCKTHRN "
              "0x0665 jumps straight to the deposit) -- no scene is staged and nothing is "
              "deferred");
        check(w.messages() == 1 && w.saw(GameEventKind::Message, "subdued and blindfolded"),
              "T10: the single blindfold line is emitted immediately, as before");
        check(w.game.position.map.location == 18 && w.game.position.map.floor == -1 &&
                  w.game.position.xy.x == 10 && w.game.position.xy.y == 7,
              "T10: and the party is deposited in the jail right away");
    }

    // ---- The text-only degradation: with no packed room wired, the capture
    // must emit byte-for-byte the stream every existing parity fixture and
    // pure harness already observes. This is the discriminant that keeps
    // gameplay_parity/quest_parity untouched by this batch.
    {
        World w(miscmsg, nullptr);
        w.add_guard();
        blackthorn_turn_effect(w.ctx, CommandEffect::Capture, w.ctx.events, no_rand());
        check(!w.pacer.active(), "degradation: no scene services means no scene");
        check(w.messages() == 7,
              "degradation: the seven throne-room prints are emitted synchronously, exactly as "
              "before this batch");
        bool any_scene = false, any_keywait = false;
        for (const auto &e : w.released) {
            any_scene = any_scene || e.first == GameEventKind::BlackthornScene;
            any_keywait = any_keywait || e.first == GameEventKind::ShrineKeyWait;
        }
        check(!any_scene && !any_keywait,
              "degradation: and no scene or key-wait event is added to the stream -- the parity "
              "fixtures see an unchanged event sequence");
    }

    std::fprintf(stderr, "blackthorn_scene: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
