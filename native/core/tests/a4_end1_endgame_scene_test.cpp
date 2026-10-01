// Alpha 4 A4-END1 (targets/tdeck/ALPHA4_UI.md section 7) -- the ending as a
// pure sequencer: openu5::EndgameScene against what ENDGAME.OVL does, read
// instruction by instruction (re/tools/a4_end1_endgame_listing.py). The text
// is the user's own ENDMSG.DAT and the room MISCMAPS.DAT[0x210] (argument 1:
// original/u5/ultima5). The device half (pacing, pixels, input, music) is
// a4_end1_ending_runtime.
#include "openu5/endgame_scene.h"
#include "openu5/scene_timing.h"
#include "openu5/state.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int g_checks = 0, g_failures = 0;
void check(bool ok, const char *id, const std::string &what) {
    ++g_checks;
    if (!ok) ++g_failures;
    std::printf("  %-5s %-5s %s\n", ok ? "GREEN" : "RED", id, what.c_str());
}
std::string esc(const std::string &s) {
    std::string out;
    for (char c : s) out += c == '\n' ? std::string("\\n") : std::string(1, c);
    return out;
}

std::vector<uint8_t> read_file(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
}

// What the overlay did, in order: "T:" a print, "C:" a speaker cue, "M:" a
// patch selector, and the waits advance() returned.
struct Recorder {
    std::vector<std::string> log;
    std::string console;
    static void text(void *p, const char *s, bool first) {
        auto &r = *static_cast<Recorder *>(p);
        r.log.push_back(std::string(first ? "T1:" : "T:") + s);
        r.console += s;
    }
    static void cue(void *p, EndgameCue c) {
        static const char *n[] = {"footstep", "revive", "orb"};
        static_cast<Recorder *>(p)->log.push_back(std::string("C:") + n[int(c)]);
    }
    static void music(void *p, EndgameMusic m, uint8_t scene) {
        static const char *n[] = {"reunion", "story", "finale"};
        static_cast<Recorder *>(p)->log.push_back(std::string("M:") + n[int(m)] + ":" + std::to_string(scene));
    }
    EndgameSink sink() { return {this, text, cue, music}; }
};

const char *wait_name(EndgameWait w) {
    static const char *n[] = {"frames", "hold", "key", "yesno", "dissolve", "forever"};
    return n[int(w)];
}

struct World {
    GameState game{};
    std::vector<std::string> records;
    std::vector<const char *> pointers;
    uint8_t room[kEndgameRoomCells]{};
};

bool load_world(World &w, const std::string &dir) {
    const auto endmsg = read_file(dir + "/ENDMSG.DAT");
    const auto misc = read_file(dir + "/MISCMAPS.DAT");
    if (endmsg.empty() || misc.size() < 0x210 + 0xb0) return false;
    std::string cur;
    for (uint8_t b : endmsg) {
        if (b) cur += char(b);
        else { w.records.push_back(cur); cur.clear(); }
        if (w.records.size() == 11) break;
    }
    for (auto &r : w.records) w.pointers.push_back(r.c_str());
    for (int row = 0; row < 11; ++row)
        for (int col = 0; col < 11; ++col) w.room[row * 11 + col] = misc[0x210 + row * 16 + col];
    return w.records.size() == 11;
}

void party(GameState &g, int size) {
    static const char *names[] = {"Kojac", "Iolo", "Shamino", "Dupre", "Jaana", "Katrina"};
    static const char cls[] = {'A', 'B', 'F', 'F', 'M', 'B'};
    g.party.character_count = 16;
    g.party.party_size = size;
    for (int i = 0; i < 6; ++i) {
        auto &c = g.party.characters[i];
        std::snprintf(c.name, sizeof(c.name), "%s", names[i]);
        c.character_class = cls[i];
        c.status = 'G';
        c.max_hp = uint16_t(100 + i);
        c.current_hp = uint16_t(50 + i);
    }
    g.time.year = 139;
    g.time.month = 4;
    g.time.day = 10;
}

// Drives advance() and satisfies every timed wait at once, recording each
// wait; stops at the first Key/YesNo/Dissolve/Forever wait.
struct Driver {
    EndgameScene scene;
    Recorder rec;
    std::vector<std::string> waits;
    uint32_t frames = 0, hold_ms = 0;
    EndgameWait run() {
        for (;;) {
            const auto w = scene.advance(rec.sink());
            waits.push_back(std::string(wait_name(w)) + (w == EndgameWait::Frames || w == EndgameWait::Hold
                                                             ? ":" + std::to_string(scene.wait_amount())
                                                             : ""));
            if (scene.phase() == EndgamePhase::Stranded) return w; // the wander loop never stops
            if (w == EndgameWait::Frames) { frames += scene.wait_amount(); scene.elapsed(); continue; }
            if (w == EndgameWait::Hold) { hold_ms += scene.wait_amount(); scene.elapsed(); continue; }
            return w;
        }
    }
    std::string pos(int slot) const {
        const auto &a = scene.actor(slot);
        return a.active ? std::to_string(a.col) + "," + std::to_string(a.row) : "-";
    }
};

std::string row_text(const EndgameScene &s, int row, uint8_t *flags = nullptr) {
    std::string out;
    uint8_t f = 0;
    for (int col = 0; col < kEndgameScrollCols; ++col) {
        const auto &c = s.scroll()[row * kEndgameScrollCols + col];
        out += (c.flags & kEndgameCellPrinted) ? char(c.ch) : '.';
        f |= c.flags;
    }
    if (flags) *flags = f;
    return out;
}
std::string centred(const std::string &text, int col) {
    std::string out(size_t(kEndgameScrollCols), '.');
    for (size_t i = 0; i < text.size(); ++i) out[size_t(col) + i] = text[i];
    return out;
}
} // namespace

int main(int argc, char **argv) {
    const std::string dir = argc > 1 ? argv[1] : "original/u5/ultima5";
    World base;
    if (!load_world(base, dir)) {
        std::printf("cannot read ENDMSG.DAT / MISCMAPS.DAT under %s\n", dir.c_str());
        return 2;
    }
    std::printf("A4-END1 EndgameScene -- ENDGAME.OVL, %s\n", dir.c_str());
    check(base.room[4 * 11 + 5] == 0x44 && base.room[9 * 11 + 5] == 0x9d && base.room[1 * 11 + 4] == 0xab,
          "S0", "MISCMAPS[0x210]: floor at the gate cell (5,4), the mirror at (5,9), the bed head at (4,1)");

    // -- E: entry, Lord British's walk, the party out of the mirror ----------
    World w = base;
    party(w.game, 3);
    w.game.party.characters[1].status = 'D';
    w.game.party.characters[1].current_hp = 0;
    w.game.wooden_box = true;
    Driver d;
    check(!d.scene.active() && d.scene.start(w.game, w.room, w.pointers.data(), w.pointers.size()) &&
              d.scene.phase() == EndgamePhase::Throne,
          "E1", "start() mounts the overlay in the throne room");
    auto w0 = d.scene.advance(d.rec.sink());
    check(w0 == EndgameWait::Frames && d.scene.wait_amount() == 0x28 && d.pos(31) == "5,8" &&
              d.scene.actor(31).tile == 0x7c && d.rec.log.size() == 1 && d.rec.log[0] == "M:reunion:0",
          "E2", "0x0650-0x06f6: the patch's sel 0x15, Lord British (0x7c) alone at (5,8), run_n_frames(0x28)");
    check(d.scene.advance(d.rec.sink()) == EndgameWait::Frames && d.scene.wait_amount() == 0x28,
          "E3", "advance() before the wait is satisfied re-runs nothing");
    d.scene.elapsed();
    // One step of Lord British: the move, then sprite_step_redraw's 2 frames,
    // the footstep (a hold), 3 frames.
    auto w1 = d.scene.advance(d.rec.sink());
    const std::string after_first = d.pos(31);
    const uint32_t a1 = d.scene.wait_amount();
    d.scene.elapsed();
    auto w2 = d.scene.advance(d.rec.sink());
    const uint32_t a2 = d.scene.wait_amount();
    d.scene.elapsed();
    auto w3 = d.scene.advance(d.rec.sink());
    const uint32_t a3 = d.scene.wait_amount();
    d.scene.elapsed();
    check(after_first == "5,7" && w1 == EndgameWait::Frames && a1 == 2 && w2 == EndgameWait::Hold &&
              a2 == tone_sweep_ms(555) && w3 == EndgameWait::Frames && a3 == 3 && d.rec.log.back() == "C:footstep",
          "E4", "a step: moved first, then frames 2 / footstep hold " + std::to_string(a2) + " ms / frames 3");
    // Run to the greeting.
    std::vector<std::string> lb_path{after_first};
    for (;;) {
        const auto w = d.scene.advance(d.rec.sink());
        if (lb_path.back() != d.pos(31)) lb_path.push_back(d.pos(31));
        if (w != EndgameWait::Frames && w != EndgameWait::Hold) break;
        if (d.scene.actor(0).active) break;
        d.scene.elapsed();
    }
    std::string path;
    for (auto &p : lb_path) path += p + " ";
    check(path == "5,7 5,6 5,5 5,4 5,3 ", "E5", "Lord British walks 0x06f9-0x070a to (5,3): " + path);
    check(d.pos(0) == "5,9" && d.scene.actor(0).tile == 0x4c && d.scene.wait() == EndgameWait::Frames &&
              d.scene.wait_amount() == 1,
          "E6", "member 0 (A -> 0x4c) appears in the mirror (5,9), run_n_frames(1) (0x07c7)");
    d.scene.elapsed();
    const size_t log_before = d.rec.log.size();
    const auto wg = d.run();
    std::string revive_line;
    for (size_t i = log_before; i < d.rec.log.size(); ++i)
        if (d.rec.log[i].rfind("T1:", 0) == 0) revive_line = d.rec.log[i];
    check(revive_line == "T1:\nIolo lives!\n", "E7", "the first print is member 1's revive: " + esc(revive_line));
    const auto &iolo = w.game.party.characters[1];
    check(iolo.status == 'G' && iolo.current_hp == iolo.max_hp, "E8",
          "0x075a-0x0765: the dead member stands up 'G' with full HP");
    bool revive_sweep = false, revive_hold = false;
    for (auto &l : d.rec.log) revive_sweep |= l == "C:revive";
    for (auto &x : d.waits) revive_hold |= x == "hold:" + std::to_string(tone_sweep_ms(0x9c40));
    check(revive_sweep && revive_hold && tone_sweep_ms(0x9c40) == 1550, "E9",
          "0x078f: the revive sweep holds 40,000 samples = 1,550 ms");
    check(wg == EndgameWait::Key && d.pos(0) == "5,5" && d.pos(1) == "4,6" && d.pos(2) == "6,6" && d.pos(31) == "5,3",
          "E10", "the lineup DATA 0x3e5a/0x3e60: 5,5 / 4,6 / 6,6, Lord British at 5,3");
    const std::string greeting = w.records[0] + "Kojac" + "!\"\n\n";
    std::string console = d.rec.console;
    check(console.size() >= greeting.size() && console.compare(console.size() - greeting.size(), greeting.size(), greeting) == 0,
          "E11", "0x0833-0x0848: ENDMSG 0x00 + the roster name + DS 0x84ae, then a key");
    check(d.frames > 0x28 * 2, "E12", "the walk-in is paced: " + std::to_string(d.frames) + " frames so far");
    {   // move_sprite_toward 0x0510 on a tie (|dcol| == |drow|) moves the column:
        // rows only when |dcol| < |drow|. Members 1 and 3 meet a tie on the way.
        World t = base;
        party(t.game, 4);
        Driver p;
        p.scene.start(t.game, t.room, t.pointers.data(), t.pointers.size());
        std::string path1, path3, last1 = "-", last3 = "-";
        for (int guard = 0; guard < 4000; ++guard) {
            const auto w = p.scene.advance(p.rec.sink());
            const std::string a1 = p.pos(1), a3 = p.pos(3);
            if (a1 != last1) { if (a1 != "-" && a1 != "5,9") path1 += a1 + " "; last1 = a1; }
            if (a3 != last3) { if (a3 != "-" && a3 != "5,9") path3 += a3 + " "; last3 = a3; }
            if (w != EndgameWait::Frames && w != EndgameWait::Hold) break;
            p.scene.elapsed();
        }
        check(path1 == "5,8 5,7 4,7 4,6 " && path3 == "4,9 4,8 3,8 3,7 ", "E13",
              "0x0510 on a tie moves the column: member 1 " + path1 + "/ member 3 " + path3);
    }

    // -- Q: the two box questions are the player's (D-56) ---------------------
    check(d.scene.key(u'z', d.rec.sink()) && d.scene.ready(), "Q0", "any key ends the greeting's getkey (0x0848)");
    auto wq = d.run();
    check(wq == EndgameWait::YesNo && d.rec.log.back() == "T:" + w.records[1], "Q1",
          "0x084b: ENDMSG 0x21 \"Didst thou bring my box?\" ... You reply:, then the Y/N loop");
    const bool refused = !d.scene.key(u'x', d.rec.sink()) && !d.scene.key(u'\r', d.rec.sink()) &&
                         !d.scene.key(u' ', d.rec.sink()) && !d.scene.key(0x1b, d.rec.sink());
    check(refused && !d.scene.ready(), "Q2", "0x0852-0x0874: any key but Y/N is read again (x, Enter, Space, Esc)");
    check(d.scene.key(u'n', d.rec.sink()), "Q3", "a lower-case n answers (getkey upper-cases, kernel 0x2032)");
    wq = d.run();
    check(wq == EndgameWait::YesNo && d.rec.log[d.rec.log.size() - 2] == "T:No\n\n" && d.rec.log.back() == "T:" + w.records[2],
          "Q4", "'N' echoes No (DS 0x84ba) and asks the second question (ENDMSG 0x49)");
    check(d.scene.key(u'Y', d.rec.sink()), "Q5", "'Y' answers the second question");
    auto wv = d.run();
    check(d.scene.victory() && d.rec.log.size() > 2, "Q6", "'N' then 'Y' with the box takes the victory branch (0x08b9)");

    // -- V: the victory --------------------------------------------------------
    bool echo_yes = false, box_line = false;
    for (auto &l : d.rec.log) { echo_yes |= l == "T:Yes\n\n"; box_line |= l == "T:" + w.records[3]; }
    check(echo_yes && box_line && d.pos(6) == "5,4" && d.scene.actor(6).tile == 0x0e && d.pos(0) == "5,5",
          "V1", "0x08cc-0x091b: the Avatar steps to 5,4 and back, the box (0x0e) at 5,4, ENDMSG 0xab");
    check(wv == EndgameWait::Key && d.rec.log.back() == "T:\n\nHe says:\n\n", "V2", "0x0925: DS 0x84cc, then a key");
    std::string speech;
    for (int r = 4; r <= 8; ++r) {
        d.scene.key(u' ', d.rec.sink());
        const auto ws = d.run();
        speech += ws == EndgameWait::Key && d.rec.log.back() == "T:" + w.records[size_t(r)] ? "k" : "x";
    }
    check(speech == "kkkkk", "V3", "0x092f-0x095e: ENDMSG 0xd3, 0x128, 0x167, 0x1c9, 0x211, a key after each: " + speech);
    d.scene.key(u' ', d.rec.sink());
    auto wf = d.run();
    check(wf == EndgameWait::Key && d.rec.log.back() == "T:" + w.records[9] && d.scene.actor(6).tile == 0x08,
          "V4", "0x0961-0x0970: ENDMSG 0x24b \"FOLLOW!\" and the orb (tile 8) in the box's slot, then a key");
    d.waits.clear();
    d.scene.key(u' ', d.rec.sink());
    auto worb = d.scene.advance(d.rec.sink());
    check(worb == EndgameWait::Hold && d.scene.wait_amount() == tone_sweep_ms(0xc350) && d.rec.log.back() == "C:orb" &&
              d.scene.actor(6).tile == 0x08,
          "V5", "0x0987: the orb sweep holds 50,000 samples = " + std::to_string(tone_sweep_ms(0xc350)) + " ms, orb still up");
    d.scene.elapsed();
    std::string stages;
    for (int i = 0; i < 16; ++i) {
        const auto w8 = d.scene.advance(d.rec.sink());
        stages += std::to_string(d.scene.gate_stage()) + (w8 == EndgameWait::Frames ? "/" + std::to_string(d.scene.wait_amount()) : "?") + " ";
        d.scene.elapsed();
    }
    check(!d.scene.actor(6).active && d.scene.room()[4 * 11 + 5] == 0xdc &&
              stages == "1/1 2/1 3/1 4/1 5/1 6/1 7/1 8/1 9/1 10/1 11/1 12/1 13/1 14/1 15/1 16/4 ",
          "V6", "0x098a-0x09b2: orb gone, 0xdc poked at (5,4), stages " + stages);
    PresentationSnapshot snap{};
    OriginalRng pose_rng(7);
    d.waits.clear();
    for (int guard = 0; guard < 2000; ++guard) {
        const auto w9 = d.scene.advance(d.rec.sink());
        if (w9 == EndgameWait::Frames && d.scene.wait_amount() == 1 && d.scene.gate_stage() == 15) break;
        d.scene.elapsed();
    }
    check(!d.scene.actor(31).active && !d.scene.actor(0).active && !d.scene.actor(1).active && !d.scene.actor(2).active,
          "V7", "0x09b5-0x0a24: Lord British, then every member, walk into the gate and vanish");
    d.scene.compose(snap, pose_rng);
    check(snap.gate_x == 5 && snap.gate_y == 4 && snap.gate_rows == 15 && snap.endgame_recolor, "V8",
          "0x0a27: the gate shrinks from stage 15 (a partial cell, 15 rows)");
    std::string closing;
    for (int i = 0; i < 15; ++i) {
        d.scene.elapsed();
        const auto wc = d.scene.advance(d.rec.sink());
        closing += std::to_string(d.scene.gate_stage()) + (wc == EndgameWait::Dissolve ? "D" : "") + " ";
    }
    check(closing == "14 13 12 11 10 9 8 7 6 5 4 3 2 1 0D " && d.scene.phase() == EndgamePhase::Dissolve &&
              d.scene.gate_cell_restored(),
          "V9", "0x0a2c-0x0a6d: stages 14..1, the floor blitted over the gate, then the fizzle: " + closing);
    d.scene.compose(snap, pose_rng);
    check(snap.tiles[4 * 11 + 5] == 0x44 && snap.gate_rows == 0, "V10", "the dissolving frame shows floor at the gate cell");
    check(!d.scene.key(u' ', d.rec.sink()), "V11", "no key acts during the fizzle (fn34 reads none in the endgame)");
    d.scene.dissolve_done();
    std::string pages;
    for (int p = 0; p < kEndgameStoryPages; ++p) {
        const auto wp = d.scene.advance(d.rec.sink());
        pages += std::to_string(d.scene.story_page()) + (wp == EndgameWait::Key ? "k" : "?") +
                 (d.rec.log.back() == "M:story:" + std::to_string(p + 1) ? "m" : "-") + " ";
        d.scene.key(u' ', d.rec.sink());
    }
    check(pages == "0km 1km 2km 3km 4km 5km ", "V12",
          "story_screens: pages 0..5, each waits a key, the patch's sel 0x18 gets BL = page + 1: " + pages);
    const auto wend = d.scene.advance(d.rec.sink());
    check(wend == EndgameWait::Forever && d.scene.phase() == EndgamePhase::Scroll && d.rec.log.back() == "M:finale:0",
          "V13", "0x01ed-0x04f9: the scroll, then the loop (the patch's sel 0x1b); no more waits");
    check(!d.scene.key(u' ', d.rec.sink()) && d.scene.advance(d.rec.sink()) == EndgameWait::Forever, "V14",
          "the loop at 0x04f9 reads no key in 1988: nothing ends it");

    // -- S: the proclamation on the 40x25 text screen ------------------------
    uint8_t f = 0;
    struct Line { int row; const char *text; bool inverse, runes; };
    const Line lines[] = {
        {1, "Be it known that on", true, false}, {2, "the Tenth Day of", true, false},
        {3, "the Fourth Month", true, false},    {4, "of the Year", true, false},
        {5, "One Hundred", true, false},         {6, "Thirty-Nine", true, false},
        {8, "Kojac the Avatar", true, false},    {10, "saved the life", true, false},
        {11, "of our sovereign", true, false},   {12, "Lord British, thereby", true, false},
        {13, "saving our people", true, false},  {14, "and our land.", true, false},
        {16, "[E@QUE_@OF@[E@AVATAR", true, true}, {17, "IS@FOREVER", true, true},
        {21, "Report now, thy Quest compleat in", false, false}, {22, "5 days", false, false},
        {23, "to Lord British at Origin Systems!", false, false},
    };
    bool all = true;
    std::string bad;
    for (const auto &l : lines) {
        const int col = (40 - int(std::strlen(l.text))) / 2;
        const auto got = row_text(d.scene, l.row, &f);
        const bool ok = got == centred(l.text, col) && bool(f & kEndgameCellInverse) == l.inverse &&
                        bool(f & kEndgameCellRunes) == l.runes;
        if (!ok) { all = false; bad += " row" + std::to_string(l.row) + "=" + got; }
    }
    for (int r : {0, 7, 9, 15, 18, 19, 20, 24})
        if (row_text(d.scene, r) != std::string(40, '.')) { all = false; bad += " blank" + std::to_string(r); }
    check(all, "S1", "endgame_datestamp: 17 lines centred at (40 - len) / 2 from row 1, reverse on the parchment," +
                        std::string(" runes on rows 16-17, the report in rows 21-23") + bad);
    // The date words and the playtime arithmetic.
    struct Date { int y, m, d; const char *day, *month, *year2, *report; };
    const Date dates[] = {
        {139, 4, 5, "Fifth", "Fourth", "Thirty-Nine", ""},          // the start date: no unit at all
        {140, 1, 1, "First", "First", "Forty", "9 months, 24 days"},  // borrows a month and a year
        {141, 13, 28, "Twenty-Eighth", "Thirteenth", "Forty-One", "2 years, 9 months, 23 days"},
        {200, 5, 20, "Twentieth", "Fifth", "", "61 years, 1 month, 15 days"},
    };
    std::string dbad;
    for (const auto &dt : dates) {
        World w2 = base;
        party(w2.game, 1);
        w2.game.time.year = dt.y;
        w2.game.time.month = dt.m;
        w2.game.time.day = dt.d;
        w2.game.wooden_box = true;
        Driver d2;
        d2.scene.start(w2.game, w2.room, w2.pointers.data(), w2.pointers.size());
        for (int guard = 0; guard < 200; ++guard) {
            const auto x = d2.run();
            if (x == EndgameWait::Forever) break;
            if (x == EndgameWait::YesNo) d2.scene.key(u'y', d2.rec.sink());
            else if (x == EndgameWait::Key) d2.scene.key(u' ', d2.rec.sink());
            else if (x == EndgameWait::Dissolve) d2.scene.dissolve_done();
        }
        const std::string day = std::string("the ") + dt.day + " Day of";
        const std::string month = std::string("the ") + dt.month + " Month";
        if (row_text(d2.scene, 2) != centred(day, (40 - int(day.size())) / 2)) dbad += " day:" + row_text(d2.scene, 2);
        if (row_text(d2.scene, 3) != centred(month, (40 - int(month.size())) / 2)) dbad += " month:" + row_text(d2.scene, 3);
        const std::string y2 = dt.year2;
        if (!y2.empty() && row_text(d2.scene, 6) != centred(y2, (40 - int(y2.size())) / 2)) dbad += " year:" + row_text(d2.scene, 6);
        if (y2.empty() && row_text(d2.scene, 6) != std::string(40, '.')) dbad += " year0:" + row_text(d2.scene, 6);
        const std::string rep = dt.report;
        if (!rep.empty() && row_text(d2.scene, 22) != centred(rep, (40 - int(rep.size())) / 2)) dbad += " report:" + row_text(d2.scene, 22);
        if (rep.empty() && row_text(d2.scene, 22) != std::string(40, '.')) dbad += " report0:" + row_text(d2.scene, 22);
    }
    check(dbad.empty(), "S2", "spell_ordinal / spell_cardinal / the 13x28 playtime (start date, borrows, 28th, year 200)" + dbad);
    const auto p = endgame_scroll_playtime(139, 3, 1);
    check(p.years == -1 && p.months == 11 && p.days == 24, "S3", "0x0407: a date before 139/4/5 borrows into -1 years");

    // -- N: stranded -----------------------------------------------------------
    auto stranded = [&](bool box, const char *answers, int size, Driver &dd, World &ww) {
        ww = base;
        party(ww.game, size);
        ww.game.wooden_box = box;
        dd.scene.start(ww.game, ww.room, ww.pointers.data(), ww.pointers.size());
        const char *a = answers;
        for (int guard = 0; guard < 400; ++guard) {
            const auto x = dd.run();
            if (x == EndgameWait::YesNo) dd.scene.key(char16_t(*a++), dd.rec.sink());
            else if (x == EndgameWait::Key) dd.scene.key(u' ', dd.rec.sink());
            else if (x == EndgameWait::Dissolve) { dd.scene.dissolve_done(); }
            else break;
            if (dd.scene.phase() == EndgamePhase::Stranded) break;
        }
    };
    Driver n1;
    World nw;
    stranded(true, "NN", 3, n1, nw);
    bool chair = false, isee = false;
    for (auto &l : n1.rec.log) { chair |= l == "T:" + nw.records[10]; isee |= l == "T:\"I see...\n"; }
    check(!n1.scene.victory() && isee && chair && n1.scene.phase() == EndgamePhase::Stranded, "N1",
          "box but N, N: \"I see...\" (DS 0x84da), 40 frames, ENDMSG 0x2d5 -- the stranded room, no gate");
    check(n1.pos(2) == "8,6" && n1.pos(31) == "4,1" && n1.pos(0) == "8,4", "N2",
          "0x0a8b-0x0ac4: slot 2 to the chair 8,6, Lord British to the bed 4,1, the Avatar to the chair 8,4");
    n1.scene.compose(snap, pose_rng);
    const int lb = 1 * 11 + 4, av = 4 * 11 + 8, m2 = 6 * 11 + 8;
    check(snap.tiles[lb] == 0x11a && snap.tiles[av] == 0x132 && snap.tiles[m2] >= 0x138 && snap.tiles[m2] <= 0x13b,
          "N3", "0x51b8 poses: SleepingInBed 0x11a, SitChairDown 0x132 (no food south), SitChairUpEat 0x138+r (food north)");
    std::set<int> eat;
    for (int i = 0; i < 64; ++i) { n1.scene.compose(snap, pose_rng); eat.insert(snap.tiles[m2]); }
    check(eat.size() == 4, "N4", "0x51a0's rand(0,3) animates the eating pose: " + std::to_string(eat.size()) + " frames seen");
    Driver n2;
    World nw2;
    stranded(false, "Y", 6, n2, nw2);
    check(!n2.scene.victory() && n2.scene.phase() == EndgamePhase::Stranded, "N5",
          "'Y' without the box (0x08c2) is stranded too");
    bool finale_once = 0;
    int finales = 0;
    for (auto &l : n2.rec.log) finales += l == "M:finale:0";
    finale_once = finales == 1;
    std::set<std::string> visited;
    int wander_frames = 0;
    for (int i = 0; i < 400; ++i) {
        const auto x = n2.scene.advance(n2.rec.sink());
        if (x != EndgameWait::Frames || n2.scene.wait_amount() != 1) break;
        ++wander_frames;
        n2.scene.elapsed();
        for (int s : {1, 3, 4, 5}) visited.insert(std::to_string(s) + ":" + n2.pos(s));
    }
    bool on_floor = true, apart = true, fixed = true;
    for (int s : {1, 3, 4, 5}) {
        const auto &a = n2.scene.actor(s);
        on_floor &= nw2.room[a.row * 11 + a.col] == 0x44;
        for (int t : {0, 1, 2, 3, 4, 5, 31})
            if (t != s && n2.scene.actor(t).active && n2.scene.actor(t).col == a.col && n2.scene.actor(t).row == a.row) apart = false;
    }
    fixed = n2.pos(31) == "4,1" && n2.pos(0) == "8,4" && n2.pos(2) == "8,6";
    check(wander_frames == 400 && finale_once && visited.size() > 8 && on_floor && apart && fixed, "N6",
          "0x0ac9: wander_sprite(1,3,4,5) forever, 1 frame each, floor only, never onto another actor; the sitters stay (" +
              std::to_string(visited.size()) + " cells)");

    // -- C: the arena compositor ------------------------------------------------
    World cw = base;
    party(cw.game, 2);
    Driver c1;
    c1.scene.start(cw.game, cw.room, cw.pointers.data(), cw.pointers.size());
    for (;;) {
        const auto x = c1.scene.advance(c1.rec.sink());
        if (c1.scene.actor(0).active || x == EndgameWait::Key) break;
        c1.scene.elapsed();
    }
    std::set<int> mirror;
    for (int i = 0; i < 64; ++i) { c1.scene.compose(snap, pose_rng); mirror.insert(snap.tiles[9 * 11 + 5]); }
    check(mirror.size() == 4 && *mirror.begin() == 0x13c && *mirror.rbegin() == 0x13f && snap.actor_ids[9 * 11 + 5] == 0,
          "C1", "a member in the mirror cell (0x9d) is LordBritishMirror 0x13c+r (0x52a2), not its class tile");
    check(snap.tiles[3 * 11 + 5] == 0x17c && snap.actor_ids[3 * 11 + 5] == 0x50000U + 31 && snap.actor_seeds[3 * 11 + 5] == 0x7c,
          "C2", "Lord British on the floor is 0x17c with an actor id (his frames animate, 0x4552)");
    check(snap.endgame_recolor && snap.visible[0] && snap.tiles[0] == 0xff && snap.center.x == 5, "C3",
          "the room is fully visible and flagged for the fn36 recolor");
    check(endgame_recolored_tile(0x44) && endgame_recolored_tile(0xdc) && endgame_recolored_tile(0x13b) &&
              !endgame_recolored_tile(0x9d) && !endgame_recolored_tile(0x17c) && !endgame_recolored_tile(0x4d) &&
              kEndgameRecolorLut[4] == 2 && kEndgameRecolorLut[1] == 5 && kEndgameRecolorLut[15] == 15,
          "C4", "fn36's list (22 tiles: floor, gate, eating poses; not the mirror, Lord British or the walls) and LUT");

    // -- F: the fizzle's order ----------------------------------------------------
    EndgameFizzle fz;
    std::vector<uint8_t> seen(320 * 200, 0);
    uint16_t x = 0, y = 0, fx = 0, fy = 0, lx = 0, ly = 0;
    uint32_t n = 0, dup = 0;
    while (fz.next(x, y)) {
        if (!n) { fx = x; fy = y; }
        lx = x; ly = y;
        if (x >= 320 || y >= 200 || seen[y * 320 + x]++) ++dup;
        ++n;
    }
    check(n == 64000 && !dup && fx == 1 && fy == 0 && lx == 0 && ly == 0 && fz.produced() == 64000, "F1",
          "fn34 over 320x200: taps 0xB400 from seed 1, every pixel once, (1,0) first, (0,0) last");
    EndgameFizzle small(5, 3); // 15 positions, 4-bit LFSR (taps 0x0c), seed 1
    std::string order;
    while (small.next(x, y)) order += std::to_string(y * 5 + x) + " ";
    check(order == "1 12 6 3 13 10 5 14 7 11 9 8 4 2 0 ", "F2", "a 5x3 rect, 4-bit taps 0x0c: " + order);

    std::printf("A4-END1 EndgameScene: %d/%d checks GREEN\n", g_checks - g_failures, g_checks);
    return g_failures ? 1 : 0;
}
