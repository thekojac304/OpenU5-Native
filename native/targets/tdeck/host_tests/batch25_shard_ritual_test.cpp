// Batch 25 -- H-118: "the shard ritual leaves world Move permanently dead".
//
// Hardware (Batch 20): Developer -> Certification -> Flame/Shard Test, then
// (U)se a shard at the Empath Abbey ritual cell. Only the "Use item" echo
// appeared -- no ritual text -- and from then on a direction produced nothing
// at all (no echo, no "Blocked!"), across a Developer Teleport and a Load,
// while (L)ook's "Look-"/direction echoes and (Z)-stats still answered. Only
// a power cycle recovered.
//
// That signature is NOT the ritual. CAST.OVL 0x15b4 prints its "Gem Shard ...
// Thou dost hold above thee" header before any gate, so a silent Use means
// dispatch_world_command() refused the UseItem itself -- and Move, and Look's
// core half, with it. Every one of those is refused by ONE gate in
// commands.cpp: `exit != c.commands.awaiting_exit` (and its siblings
// awaiting_troll / Blackthorn's tribute/arrest/password flags). Everything
// that still worked lives above the core: UiSession echoes, Z-stats
// (AlphaRuntime), Alt+D, Alt+M, Alt+L. Neither a teleport nor a load clears
// CommandState, so the lock survives both; a power cycle rebuilds it.
//
// How a flag outlives its question: TOWN.OVL 0x0798 prints "Dost thou wish
// to leave?" and 0x07ac-0x07b4 loops on getkey (0xa49c) until Y, N or ESC --
// the 1988 game cannot reach its command loop with the question open. The
// port splits that loop into core state (awaiting_exit) plus a UiSession
// yes/no modal, and the modal is the only thing that can answer. Opening the
// Developer menu parks the modal in debug_return_mode_, but
// AlphaRuntime::synchronize_after_debug() runs after EVERY input (Alt+D
// included) and calls UiSession::set_base_mode(), whose Developer branch
// overwrote that register unconditionally -- unlike its ordinary branch,
// which never replaces a modal. Closing the menu then restored Exploration:
// the question vanished, awaiting_exit stayed true, and every world command
// from then on was refused in silence. The Flame/Shard Certification is only
// ever reached through Alt+D, so any prompt on screen at that moment was lost
// right before the ritual.
//
// Same seam and data as batch24_reload_parity: the real AlphaRuntime, the
// shipped resource pack, the real Developer menu and raw keyboard/trackball
// events through production routing. Nothing here calls a core function to
// reach a state the player could not reach with keys, except the one
// staging line that stands the party beside the castle's west edge.
#include "../main/alpha_runtime.h"
#include "../main/keyboard_matrix.h"

#include "openu5/world.h"
#include "openu5/world_commands.h"

#include <cstdio>
#include <cstring>
#include <memory>

using namespace openu5;

namespace {

int g_failures = 0, g_checks = 0;
bool expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-5s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
    return ok;
}

constexpr uint8_t kCastle = 17, kEmpath = 31;
constexpr int kFlameX = 15, kFlameY = 3;   // CAST DS 0x4882/0x4886 idx 1; floor DS 0x488e[1] = 1
constexpr int16_t kFlameFloor = 1;

const tdeck::AlphaResourceOwners *g_owners = nullptr;

struct Harness {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    int64_t clock_us = 0;
    size_t mark = 0;

    Harness() {
        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world = g_owners->world;
        hf.location_x = g_owners->location_x; hf.location_y = g_owners->location_y;
        hf.location_count = g_owners->location_count;
        hf.npc_locations = g_owners->npc_locations;
        hf.pack = g_owners;
        rt->attach_host_test_fixture(hf);
        auto &g = rt->game();
        g.party.character_count = g.party.party_size = 1;
        auto &ch = g.party.characters[0];
        std::snprintf(ch.name, sizeof(ch.name), "Avatar");
        ch.current_hp = ch.max_hp = 900; ch.status = 'G'; ch.party_status = 0; ch.character_class = 'A';
        ch.dexterity = 30; ch.strength = 30;
        g.party.active_character = 255;
        g.time.hour = 12; g.time.minute = 0;
        g.position.map = {0, 0};
        g.position.xy = {g_owners->location_x[kCastle - 1], g_owners->location_y[kCastle - 1]};
    }
    GameState &g() { return rt->game(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    const UiSession &ui() const { return *rt->ui(); }
    UiMode mode() const { return rt->ui()->mode(); }

    bool raw_key(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent raw{};
        raw.kind = tdeck::RawInputKind::Keyboard;
        raw.code = code;
        raw.modifiers.alt = alt;
        raw.transition = tdeck::KeyTransition::Pressed;
        raw.timestamp_us = (clock_us += 100000);
        return rt->handle(raw);
    }
    bool key(uint8_t code) { return raw_key(code); }
    void text(const char *s) { while (*s) key(uint8_t(*s++)); }
    bool ball(tdeck::RawInputKind kind) {
        tdeck::RawInputEvent raw{};
        raw.kind = kind;
        raw.timestamp_us = (clock_us += 100000);
        return rt->handle(raw);
    }
    bool north() { return ball(tdeck::RawInputKind::TrackballUp); }
    bool south() { return ball(tdeck::RawInputKind::TrackballDown); }
    bool west() { return ball(tdeck::RawInputKind::TrackballLeft); }
    // The device's Cancel: a SHORT Mic press (matrix 0,6), press then release.
    void mic() {
        tdeck::RawInputEvent raw{};
        raw.kind = tdeck::RawInputKind::Keyboard;
        raw.column = tdeck::kMicrophoneKeyColumn; raw.row = tdeck::kMicrophoneKeyRow;
        raw.transition = tdeck::KeyTransition::Pressed; raw.timestamp_us = (clock_us += 100000);
        rt->handle(raw);
        raw.transition = tdeck::KeyTransition::Released; raw.timestamp_us = (clock_us += 100000);
        rt->handle(raw);
    }

    // Transcript since the last mark() -- what the player saw for one step.
    void set_mark() { mark = ui().transcript_size(); }
    bool saw(const char *needle) const {
        for (size_t i = mark; i < ui().transcript_size(); ++i) {
            const auto *b = ui().transcript_at(i);
            if (b && std::strstr(b->text, needle)) return true;
        }
        return false;
    }
    void show() const {
        for (size_t i = mark; i < ui().transcript_size(); ++i)
            if (const auto *b = ui().transcript_at(i)) std::printf("         | %s\n", b->text);
    }
    bool at(uint8_t loc, int16_t floor, int x, int y) const {
        const auto &p = rt->game().position;
        return p.map.location == loc && p.map.floor == floor && p.xy.x == x && p.xy.y == y;
    }
    void where() const {
        const auto &p = rt->game().position;
        std::printf("         party L%u/F%d (%u,%u) ui=%d awaiting_exit=%d\n", unsigned(p.map.location),
                    int(p.map.floor), unsigned(p.xy.x), unsigned(p.xy.y), int(mode()),
                    int(rt->commands().awaiting_exit));
    }

    // Alt+D -> Certification (root index 14, one step "up" from row 0) ->
    // Flame/Shard (row 3) -> effect sheet -> Run Certification -> Back, Back.
    // Exactly the hardware route; the menu's own apply_debug_certification()
    // grants the three shards and teleports to (15,3) on Empath Abbey floor 1.
    void flame_shard_certification() {
        raw_key('d', true);
        north();
        key('\r');
        for (int i = 0; i < 3; ++i) south();
        key('\r');
        key('\r');
        key('\b');
        key('\b');
    }
    // (U)se, cursor down the real inventory list to `name`, Confirm.
    bool use(const char *name) {
        key('u');
        UiSelectionView v{};
        for (int i = 0; i < 32 && rt->ui()->selection_view(v); ++i) {
            if (v.current.label && std::strstr(v.current.label, name)) {
                key('\r');
                return true;
            }
            south();
        }
        return false;
    }
    // (L)ook with a direction: true only when the CORE answered ("Thou dost
    // see ..."), not merely the UiSession's "Look-"/direction echoes.
    bool look_north() {
        set_mark();
        key('l');
        north();
        return saw("Look-") && saw("north") && saw("Thou dost see");
    }
    // 'z' opens the member picker first (a party of one included), Confirm
    // picks, Space closes (cmd_zstats 0x0a78) -- the batch14 route.
    bool zstats_round_trip() {
        key('z');
        key('\r');
        const bool opened = rt->zstats_active();
        key(' ');
        return opened && !rt->zstats_active();
    }
    // Castle 17, standing on its west edge, then one step West: "Leave this place?".
    bool raise_exit_question() {
        key('e');
        g().position.xy = {0, 15};
        west();
        return mode() == UiMode::YesNo && ui().request() == UiRequestId::TownExit &&
               rt->commands().awaiting_exit;
    }
};

// ---------------------------------------------------------------------------
// R: H-118 as it happened -- a pending question, then the Certification.
// ---------------------------------------------------------------------------
void test_pending_question_survives_developer(bool answer_yes) {
    const char *R[] = {"R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11"};
    const char *Y[] = {"Y1", "Y2", "Y3", "Y4", "Y5", "Y6", "Y7", "Y8", "Y9", "Y10", "Y11"};
    const char **k = answer_yes ? Y : R;
    std::printf("%s pending \"Leave this place?\" -> Alt+D Flame/Shard Certification -> answer %s\n",
                answer_yes ? "Y" : "R", answer_yes ? "Y" : "N");
    Harness h;
    expect(h.raise_exit_question(), k[0], "precondition: the castle's west edge raised \"Leave this place?\"");
    h.flame_shard_certification();
    expect(h.at(kEmpath, kFlameFloor, kFlameX, kFlameY) && h.g().quest.shards[0] && h.g().quest.shards[1] &&
               h.g().quest.shards[2],
           k[1], "the real menu route granted the three shards and teleported to Empath Abbey F1 (15,3)");
    if (!expect(h.mode() == UiMode::YesNo && h.ui().request() == UiRequestId::TownExit, k[2],
                "** leaving the Developer menu returns to the question it interrupted (TOWN 0x07ac loop) **"))
        h.where();
    // Answer only a question that is actually on screen: on the unfixed tree
    // it is gone, and a stray 'n' would open New Order instead -- the RED run
    // then shows the bare hardware signature (Z-stats alive, Move/Use dead).
    h.set_mark();
    if (h.mode() == UiMode::YesNo) h.key(answer_yes ? 'y' : 'n');
    if (!expect(!h.rt->commands().awaiting_exit && h.mode() == UiMode::Exploration, k[3],
                "** the answer reaches the core: awaiting_exit clears, Exploration owns input **"))
        h.where();

    if (answer_yes) {
        // Y = exit: the question belonged to whichever town the party is in
        // when it is answered -- Empath Abbey now -- so the party is outside.
        expect(h.g().position.map.location == 0, k[4], "'Y' left the town the party now stands in (overworld)");
        const auto before = h.g().position.xy;
        h.set_mark();
        h.south();
        const bool moved = h.g().position.xy.y != before.y || h.saw("Blocked!") || h.saw("South");
        if (!expect(moved && h.saw("South"), k[5], "** an ordinary overworld Move answers at once (echo \"South\") **"))
            h.show();
        expect(h.look_north(), k[6], "Look reaches the core (\"Thou dost see ...\")");
        expect(h.zstats_round_trip(), k[7], "Z-stats opens and closes");
        return;
    }

    // N = stay: one town turn, and the party is still on the ritual cell.
    expect(h.at(kEmpath, kFlameFloor, kFlameX, kFlameY), k[4], "'N' kept the party on the ritual cell");
    h.set_mark();
    const bool used = h.use("Hatred");
    const bool ritual = h.saw("Thou dost hold above thee the evil Shard of Hatred") &&
                        h.saw("...and cast it into the Flame of Love!");
    if (!expect(used && ritual, k[5], "** (U)se Shard of Hatred prints the CAST 0x15b4 header and the Flame line **"))
        h.show();
    expect(h.mode() == UiMode::Exploration && h.g().quest.shards[1], k[6],
           "no Shadowlord above: no doom, the shard is kept, control returns to Exploration");
    h.set_mark();
    h.south();
    if (!expect(h.at(kEmpath, kFlameFloor, kFlameX, kFlameY + 1) && h.saw("South"), k[7],
                "** the very next direction is an ordinary world Move: (15,3) -> (15,4), echo \"South\" **")) {
        h.where();
        h.show();
    }
    expect(h.look_north(), k[8], "Look reaches the core (\"Thou dost see ...\"), not just its echoes");
    expect(h.zstats_round_trip(), k[9], "Z-stats opens and closes");
    h.set_mark();
    h.north();
    expect(h.at(kEmpath, kFlameFloor, kFlameX, kFlameY) && h.saw("North"), k[10],
           "Move after Z-stats: back to the ritual cell (15,3)");
}

// ---------------------------------------------------------------------------
// S: every exit of the shard interaction itself, from a clean world.
// ---------------------------------------------------------------------------
bool move_answers(Harness &h, int x, int y) {
    h.set_mark();
    h.south();
    const bool ok = h.at(kEmpath, kFlameFloor, x, y + 1) && h.saw("South") && !h.rt->commands().awaiting_exit;
    h.north();
    return ok && h.at(kEmpath, kFlameFloor, x, y);
}

void test_shard_exit_paths() {
    std::printf("S shard (U)se exit paths at the Flame of Love, each followed by Move/Look/Z-stats\n");
    Harness h;
    h.flame_shard_certification();
    expect(h.at(kEmpath, kFlameFloor, kFlameX, kFlameY) && h.mode() == UiMode::Exploration, "S1",
           "precondition: the Certification from a clean world ends in Exploration on (15,3)");

    // Cancel inside the item list, with the device's Cancel (short Mic).
    h.set_mark();
    h.key('u');
    const bool listed = h.mode() == UiMode::InventorySelection;
    h.mic();
    expect(listed && h.mode() == UiMode::Exploration && !h.saw("Gem Shard"), "S2",
           "Mic inside the (U)se list closes it; nothing is used");
    expect(move_answers(h, kFlameX, kFlameY), "S3", "Move answers after the cancelled list");

    // Wrong shard for this Flame: header, then "No effect!" (CAST 0x162f-0x1654).
    h.set_mark();
    const bool used = h.use("Falsehood");
    expect(used && h.saw("Thou dost hold above thee the evil Shard of Falsehood") && h.saw("No effect!"), "S4",
           "Shard of Falsehood at the Flame of Love: header + \"No effect!\"");
    // CAST 0x1a2c -> 0x15b4 has no getdir: no direction prompt is ever armed.
    expect(h.mode() == UiMode::Exploration, "S5", "no direction/target prompt follows a shard (reference: none)");
    expect(move_answers(h, kFlameX, kFlameY), "S6", "Move answers after the wrong-flame attempt");

    // Wrong place: one step off the cell.
    h.south();
    h.set_mark();
    h.use("Hatred");
    expect(h.saw("Shard of Hatred") && h.saw("No effect!") && !h.saw("Flame of Love"), "S7",
           "Shard of Hatred one cell south of the ritual cell: header + \"No effect!\"");
    h.set_mark();
    h.north();
    expect(h.at(kEmpath, kFlameFloor, kFlameX, kFlameY) && h.saw("North"), "S8",
           "Move answers after the wrong-place attempt");

    // Full success: summon Astaroth from one cell south (CMDS 0x1030 puts the
    // Shadowlord at party_y-2, i.e. on the Flame (15,2)), step onto the
    // ritual cell and cast the Shard of Hatred (CAST 0x16c1/0x16c9 -> 0x1708).
    h.south();
    h.key('y');
    h.text("ASTAROTH");
    h.key('\r');
    expect(h.g().quest.summoned == 1, "S9", "(Y)ell ASTAROTH one cell south summons idx 1 onto the Flame");
    h.north();
    h.set_mark();
    h.use("Hatred");
    const bool doom = h.saw("Flame of Love!") && h.saw("The doom of the Shadowlord Astaroth is wrought!");
    if (!expect(doom && !h.g().quest.shards[1] && h.g().quest.summoned == -1, "S10",
                "Shard of Hatred under Astaroth: doom wrought, shard consumed, summon cleared"))
        h.show();
    expect(h.mode() == UiMode::Exploration && move_answers(h, kFlameX, kFlameY), "S11",
           "** Move answers immediately after the successful ritual **");
    expect(h.look_north(), "S12", "Look reaches the core after the ritual");
    expect(h.zstats_round_trip(), "S13", "Z-stats opens and closes after the ritual");
    expect(move_answers(h, kFlameX, kFlameY), "S14", "Move answers after closing Z-stats");
}

// ---------------------------------------------------------------------------
// U: every core-owned question the Developer overlay can interrupt.
// ---------------------------------------------------------------------------
void test_developer_returns_to_every_prompt() {
    std::printf("U Alt+D -> Back returns to whatever it interrupted\n");
    struct Case { const char *id; GameEventKind kind; UiRequestId request; UiMode mode; };
    const Case cases[] = {
        {"U1", GameEventKind::TownExitPrompt, UiRequestId::TownExit, UiMode::YesNo},
        {"U2", GameEventKind::TrollTollPrompt, UiRequestId::TrollToll, UiMode::YesNo},
        {"U3", GameEventKind::GuardTributePrompt, UiRequestId::GuardTribute, UiMode::YesNo},
        {"U4", GameEventKind::GuardArrestPrompt, UiRequestId::GuardArrest, UiMode::YesNo},
        {"U5", GameEventKind::GuardPasswordPrompt, UiRequestId::GuardPassword, UiMode::TextEntry},
    };
    for (const auto &c : cases) {
        Harness h;
        GameEvent e;
        e.kind = c.kind;
        // The production sink: AlphaRuntime::dispatch_event -> UiSession::consume.
        h.ctx().events.emit(h.ctx().events.context, e);
        const bool armed = h.mode() == c.mode && h.ui().request() == c.request;
        h.raw_key('d', true);
        const bool menu = h.mode() == UiMode::DebugMenu;
        h.key('\b');
        char what[96];
        std::snprintf(what, sizeof(what), "%s: Alt+D, Back -> the same prompt is back on screen",
                      c.request == UiRequestId::TownExit ? "Leave this place?" :
                      c.request == UiRequestId::TrollToll ? "Pay toll?" :
                      c.request == UiRequestId::GuardTribute ? "Pay tribute?" :
                      c.request == UiRequestId::GuardArrest ? "Wilt thou come quietly?" : "Password?");
        expect(armed && menu && h.mode() == c.mode && h.ui().request() == c.request, c.id, what);
    }
    // Controls: the overlay still returns to plain Exploration and to an
    // armed direction prompt (Look-) exactly as before.
    {
        Harness h;
        h.raw_key('d', true);
        h.key('\b');
        expect(h.mode() == UiMode::Exploration, "U6", "control: Exploration -> Alt+D, Back -> Exploration");
        h.key('l');
        h.raw_key('d', true);
        h.key('\b');
        expect(h.mode() == UiMode::TargetSelection, "U7", "an armed \"Look-\" -> Alt+D, Back -> \"Look-\" again");
        h.set_mark();
        h.north();
        expect(h.mode() == UiMode::Exploration && h.saw("Thou dost see"), "U8",
               "the restored \"Look-\" completes into a core Look");
    }
    // A debug teleport into a dungeon while the menu is open still lands in
    // the Dungeon view (H-114 / Batch 9D rule: dungeon is authoritative).
    {
        Harness h;
        h.raw_key('d', true);
        h.rt->dungeon_state_for_test().active = true;   // what a Dungeon teleport leaves behind
        h.south();                                      // any menu input runs synchronize_after_debug()
        h.key('\b');
        h.key('\b');
        expect(h.mode() == UiMode::Dungeon, "U9", "control: an authoritative Dungeon change made under the menu still wins");
    }
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: batch25_shard_ritual_test <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;

    test_pending_question_survives_developer(false);
    test_pending_question_survives_developer(true);
    test_shard_exit_paths();
    test_developer_returns_to_every_prompt();

    std::printf("\nbatch25_shard_ritual: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
