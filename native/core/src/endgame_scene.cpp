#include "openu5/endgame_scene.h"

#include <cstdio>
#include <cstring>

#include "openu5/scene_timing.h"
#include "openu5/state.h"

namespace openu5 {

// EGA.DRV 0x2d4d, the low-nibble table fn36 ax=4 passes every listed tile
// through (0x2d5d; its <<4 twin for the high nibble is 0x2d3d).
const uint8_t kEndgameRecolorLut[16] = {0x0, 0x5, 0x4, 0x4, 0x2, 0x1, 0x2, 0x7,
                                        0x8, 0xc, 0xc, 0xc, 0xa, 0x9, 0xe, 0xf};

bool endgame_recolored_tile(int32_t tile) {
    // EGA.DRV 0x2cb0-0x2d33: `mov si, tile * 0x80; call 0x2d5d`, in this order.
    static constexpr uint16_t kList[] = {0x44, 0x5c, 0x5d, 0x90, 0x92, 0x94, 0x96, 0x9b,
                                         0xab, 0xac, 0xaf, 0xb0, 0xb1, 0xbf, 0xdc, 0x108,
                                         0x10e, 0x11a, 0x138, 0x139, 0x13a, 0x13b};
    for (const auto t : kList)
        if (t == tile) return true;
    return false;
}

namespace {

constexpr uint8_t kLbSlot = 31;   // 0x5d52 = 0x5c5a + 31 x 8
constexpr uint8_t kItemSlot = 6;  // 0x5c8a: the box, then the orb
constexpr uint8_t kLbTile = 0x7c; // 0x06db, drawn 0x17c (LordBritish1)
constexpr uint8_t kBoxTile = 0x0e;
constexpr uint8_t kOrbTile = 0x08;
constexpr uint8_t kFloor = 0x44;
constexpr uint8_t kGateTile = 0xdc;
constexpr uint8_t kMirror = 0x9d;
// DATA.OVL 0x3e5a / 0x3e60: where member i stands to be greeted (0x07d4 / 0x07db).
constexpr uint8_t kLineupCol[6] = {5, 4, 6, 3, 5, 7};
constexpr uint8_t kLineupRow[6] = {5, 6, 6, 7, 7, 7};
// sprite_step_redraw's footstep, kernel 0x433e: noise_burst(0x3e8, 0x19, 1),
// delay(0x14), noise_burst(0x5dc, 0x19, 1) -- 25 x 1.5 + 20 x 24 + 25 x 1.5
// = 555 speaker samples. [B]
constexpr uint32_t kFootstepMs = tone_sweep_ms(555);
constexpr uint32_t kReviveSweepMs = tone_sweep_ms(0x9c40); // 0x0783 a2
constexpr uint32_t kOrbSweepMs = tone_sweep_ms(0xc350);    // 0x097b a2

// DATA.OVL 0x3e0a / 0x3e2e / 0x3e40, the number words spell_cardinal 0x028c and
// spell_ordinal 0x02d6 print. Index 0 of the units table is a NULL pointer in
// the binary, which prints nothing.
constexpr const char *kUnits[21] = {"", "One", "Two", "Three", "Four", "Five", "Six", "Seven",
                                    "Eight", "Nine", "Ten", "Eleven", "Twelve", "Thirteen",
                                    "Fourteen", "Fifteen", "Sixteen", "Seventeen", "Eighteen",
                                    "Nineteen", "Twenty"};
constexpr const char *kTens[10] = {"", "", "Twenty", "Thirty", "Forty", "Fifty", "Sixty",
                                   "Seventy", "Eighty", "Ninety"};
constexpr const char *kOrdinals[13] = {"", "First", "Second", "Third", "Fourth", "Fifth", "Sixth",
                                       "Seventh", "Eighth", "Ninth", "Tenth", "Eleventh", "Twelfth"};

// The roster's class letter -> byte[0x1ade + strchr("AMBFDTPRS", class)] (0x07a1-0x07b2).
uint8_t class_tile(char c) {
    return c == 'M' ? 0x40 : c == 'B' ? 0x44 : c == 'F' ? 0x48 : 0x4c;
}

int32_t rand_range(OriginalRng &rng, int32_t lo, int32_t hi) { return rng.next(lo, hi).value; }

} // namespace

EndgamePlaytime endgame_scroll_playtime(int year, int month, int day) {
    // endgame_datestamp 0x0407-0x043b.
    EndgamePlaytime p{year - 0x8b, month - 4, day - 5};
    if (p.days < 0) { p.days += 0x1c; --p.months; }
    if (p.months < 0) { p.months += 0x0d; --p.years; }
    return p;
}

EndgameFizzle::EndgameFizzle(uint16_t width, uint16_t height) : width_(width), height_(height) {
    // cs:[0x254d], widths 2..16: the taps of a maximal Galois LFSR per bit width.
    static constexpr uint16_t kTaps[15] = {0x03, 0x06, 0x0c, 0x14, 0x30, 0x60, 0xb8, 0x110,
                                           0x240, 0x500, 0xca0, 0x1b00, 0x3500, 0x6000, 0xb400};
    uint32_t n = uint32_t(width) * height - 1, bits = 0;
    while (n) { ++bits; n >>= 1; }
    taps_ = kTaps[bits < 2 ? 0 : bits > 16 ? 14 : bits - 2];
}

bool EndgameFizzle::next(uint16_t &x, uint16_t &y) {
    while (!done_) {
        if (!state_) { // 0x2600: the loop is over; (0,0), the position the LFSR never visits
            done_ = true;
            x = y = 0;
            ++produced_;
            return true;
        }
        const uint32_t pos = state_; // 0x25b6-0x25c3: plot state, then step it
        const uint32_t stepped = (state_ & 1) ? (state_ >> 1) ^ taps_ : state_ >> 1; // 0x25ec-0x25f3
        state_ = stepped == 1 ? 0 : stepped; // 0x25f8: back at the seed ends the loop
        if (pos / width_ >= height_) continue; // 0x25bf: off the rect, no plot
        x = uint16_t(pos % width_);
        y = uint16_t(pos / width_);
        ++produced_;
        return true;
    }
    return false;
}

enum class EndgameScene::Pc : uint8_t {
    Entry, LbWalk, MemberNext, MemberRevived, MemberPlace, MemberWalk, Greeting, BoxQuestion,
    FirstAnswer, SecondAnswer, Branch, AvatarForward, AvatarBack, BoxPlaced, HeSays, Speech,
    Follow, Orb, GateOpen, GateRise, LbIntoGate, MembersIntoGate, GateClose, Teardown, Story,
    StoryNext, ISee, Chair, StrandedWalks, Wander
};

bool EndgameScene::start(GameState &game, const uint8_t *room, const char *const *records,
                         size_t record_count) {
    if (!room || !records || record_count < 11) return false;
    *this = EndgameScene{};
    game_ = &game;
    records_ = records;
    record_count_ = record_count;
    // 0x0684-0x06b7: MISCMAPS.DAT[0x210] (11 rows of 0x10) into the 0x20-stride arena map.
    std::memcpy(room_, room, sizeof(room_));
    rng_.seed(game.rng.get_seed());
    pc_ = Pc::Entry;
    phase_ = EndgamePhase::Throne;
    ready_ = true;
    return true;
}

void EndgameScene::cancel() { *this = EndgameScene{}; }

EndgameWait EndgameScene::frames(uint32_t n) { amount_ = n; return wait_for(EndgameWait::Frames); }
EndgameWait EndgameScene::hold(uint32_t ms) { amount_ = ms; return wait_for(EndgameWait::Hold); }
EndgameWait EndgameScene::wait_for(EndgameWait w) {
    wait_ = w;
    ready_ = false;
    if (w == EndgameWait::Key || w == EndgameWait::YesNo) amount_ = 0;
    return w;
}

void EndgameScene::elapsed() {
    if (wait_ == EndgameWait::Frames || wait_ == EndgameWait::Hold) ready_ = true;
}

void EndgameScene::dissolve_done() {
    if (wait_ == EndgameWait::Dissolve) ready_ = true;
}

bool EndgameScene::key(char16_t ch, const EndgameSink &) {
    if (!active() || ready_) return false;
    if (wait_ == EndgameWait::Key) { ready_ = true; return true; }
    if (wait_ != EndgameWait::YesNo) return false;
    // getkey_with_redraw returns kernel 0x2032's upper case; the loops only leave on 'Y' / 'N'.
    const char16_t up = ch >= u'a' && ch <= u'z' ? char16_t(ch - 0x20) : ch;
    if (up != u'Y' && up != u'N') return false;
    answer_ = uint8_t(up);
    ready_ = true;
    return true;
}

bool EndgameScene::step_toward(uint8_t slot, uint8_t col, uint8_t row) {
    // move_sprite_toward 0x0510(slot, col, row): one step on the longer axis
    // (rows when |dcol| < |drow|), 0 when inactive or already there.
    auto &a = actors_[slot];
    if (!a.active || (a.col == col && a.row == row)) return false;
    const int dcol = a.col > col ? a.col - col : col - a.col;
    const int drow = a.row > row ? a.row - row : row - a.row;
    if (dcol < drow) a.row = uint8_t(row < a.row ? a.row - 1 : a.row + 1);
    else a.col = uint8_t(a.col > col ? a.col - 1 : a.col + 1);
    return true;
}

void EndgameScene::print(const EndgameSink &sink, const char *text) {
    if (sink.text && text) sink.text(sink.context, text, first_print_);
    first_print_ = false;
}

void EndgameScene::print_record(const EndgameSink &sink, size_t index) {
    if (index < record_count_) print(sink, records_[index]);
}

EndgameWait EndgameScene::advance(const EndgameSink &sink) {
    if (!active() || !ready_) return wait_;
    ready_ = false;
    auto &party = game_->party;
    const int party_size = party.party_size < 0 ? 0 : party.party_size > 6 ? 6 : party.party_size;
    for (;;) {
        // sprite_step_redraw 0x04fe, after every step move_sprite_toward takes:
        // run_n_frames(2), the footstep, run_n_frames(3).
        if (step_) {
            switch (step_++) {
            case 1: return frames(2);
            case 2:
                if (sink.cue) sink.cue(sink.context, EndgameCue::Footstep);
                return hold(kFootstepMs);
            case 3: return frames(3);
            default: step_ = 0; break;
            }
        }
        switch (pc_) {
        case Pc::Entry:
            // 0x0650-0x06f6. 0x0655 (the patch's 0x0aff) selects Joyous Reunion and
            // redraws the status panel; the room is mounted (start()); the actor
            // table is cleared and Lord British stands at (5,8); 40 frames.
            if (sink.music) sink.music(sink.context, EndgameMusic::Reunion, 0);
            for (auto &a : actors_) a = {};
            actors_[kLbSlot] = {kLbTile, 5, 8, true};
            pc_ = Pc::LbWalk;
            return frames(0x28);
        case Pc::LbWalk: // 0x06f9-0x070a
            if (step_toward(kLbSlot, 5, 3)) { step_ = 1; continue; }
            member_ = 0;
            pc_ = Pc::MemberNext;
            break;
        case Pc::MemberNext: {
            // 0x070c-0x0743 / 0x081c: each member in roster order.
            if (member_ >= party_size || member_ >= party.character_count) {
                pc_ = Pc::Greeting;
                return frames(0x28); // 0x082c
            }
            const auto &ch = party.characters[member_];
            if (ch.status != 'D') { pc_ = Pc::MemberPlace; break; }
            // 0x0743-0x078f: put_char('\n'), the name, " lives!\n", the
            // viewport XOR (8,8)-(0xb7,0xb7) and the sweep that holds it.
            char line[32];
            std::snprintf(line, sizeof(line), "\n%s lives!\n", ch.name);
            print(sink, line);
            inverted_ = true;
            if (sink.cue) sink.cue(sink.context, EndgameCue::ReviveSweep);
            pc_ = Pc::MemberRevived;
            return hold(kReviveSweepMs);
        }
        case Pc::MemberRevived: {
            // 0x075a-0x0765 (status 'G', HP = max) shows at 0x0792's status redraw.
            auto &ch = party.characters[member_];
            ch.status = 'G';
            ch.current_hp = ch.max_hp;
            pc_ = Pc::MemberPlace;
            break;
        }
        case Pc::MemberPlace:
            // 0x0795-0x07cb: the class tile at (5,9), the mirror, then one frame
            // (which also repaints over the XOR).
            actors_[member_] = {class_tile(party.characters[member_].character_class), 5, 9, true};
            inverted_ = false;
            pc_ = Pc::MemberWalk;
            return frames(1);
        case Pc::MemberWalk: // 0x07ce-0x0802
            if (step_toward(member_, kLineupCol[member_], kLineupRow[member_])) { step_ = 1; continue; }
            ++member_;
            pc_ = Pc::MemberNext;
            break;
        case Pc::Greeting:
            // 0x0833-0x0848: ENDMSG 0x00, the name at DS 0x55a8 (roster slot 0), DS 0x84ae.
            print_record(sink, 0);
            print(sink, party.character_count ? party.characters[0].name : "");
            print(sink, "!\"\n\n");
            pc_ = Pc::BoxQuestion;
            return wait_for(EndgameWait::Key);
        case Pc::BoxQuestion: // 0x084b-0x0852
            print_record(sink, 1);
            pc_ = Pc::FirstAnswer;
            return wait_for(EndgameWait::YesNo);
        case Pc::FirstAnswer:
            // 0x0861 / 0x0876: the echo (DS 0x84b4 / 0x84ba); only 'N' asks again (0x087f).
            print(sink, answer_ == 'Y' ? "Yes\n\n" : "No\n\n");
            if (answer_ != 'N') { pc_ = Pc::Branch; break; }
            print_record(sink, 2); // 0x0884
            pc_ = Pc::SecondAnswer;
            return wait_for(EndgameWait::YesNo);
        case Pc::SecondAnswer: // 0x089a / 0x08b0 (DS 0x84c0 / 0x84c6)
            print(sink, answer_ == 'Y' ? "Yes\n\n" : "No\n\n");
            pc_ = Pc::Branch;
            break;
        case Pc::Branch:
            // 0x08b9-0x08c9: 'Y' and g_wooden_box (DS 0x57bf), else 0x0a76.
            if (answer_ == 'Y' && game_->wooden_box) {
                victory_ = true;
                pc_ = Pc::AvatarForward;
                return frames(8); // 0x08cc
            }
            pc_ = Pc::ISee;
            break;
        case Pc::AvatarForward: // 0x08d3-0x08e3
            if (step_toward(0, 5, 4)) { step_ = 1; continue; }
            pc_ = Pc::AvatarBack;
            break;
        case Pc::AvatarBack: // 0x08e5-0x08f2
            if (step_toward(0, 5, 5)) { step_ = 1; continue; }
            pc_ = Pc::BoxPlaced;
            return frames(4); // 0x08f4
        case Pc::BoxPlaced:
            // 0x08fb-0x091e: the box at (5,4), ENDMSG 0xab, 40 frames.
            actors_[kItemSlot] = {kBoxTile, 5, 4, true};
            print_record(sink, 3);
            pc_ = Pc::HeSays;
            return frames(0x28);
        case Pc::HeSays: // 0x0925-0x092c (DS 0x84cc)
            print(sink, "\n\nHe says:\n\n");
            member_ = 4;
            pc_ = Pc::Speech;
            return wait_for(EndgameWait::Key);
        case Pc::Speech: // 0x092f-0x095e: ENDMSG 0xd3, 0x128, 0x167, 0x1c9, 0x211, a key each
            print_record(sink, member_);
            if (++member_ > 8) pc_ = Pc::Follow;
            return wait_for(EndgameWait::Key);
        case Pc::Follow: // 0x0961-0x0970: ENDMSG 0x24b, the orb in the box's slot
            print_record(sink, 9);
            actors_[kItemSlot].tile = kOrbTile;
            pc_ = Pc::Orb;
            return wait_for(EndgameWait::Key);
        case Pc::Orb: // 0x0973-0x0987
            if (sink.cue) sink.cue(sink.context, EndgameCue::OrbSweep);
            pc_ = Pc::GateOpen;
            return hold(kOrbSweepMs);
        case Pc::GateOpen: // 0x098a-0x09a0: the orb goes, 0xdc at (5,4), stage 1
            actors_[kItemSlot] = {};
            room_[kEndgameGateRow * kEndgameRoomSide + kEndgameGateCol] = kGateTile;
            gate_stage_ = 1;
            pc_ = Pc::GateRise;
            return frames(1);
        case Pc::GateRise: // 0x09a3-0x09b2: stages 2..15, then 16 (the whole gate) for 4 frames
            if (++gate_stage_ < 0x10) return frames(1);
            pc_ = Pc::LbIntoGate;
            return frames(4);
        case Pc::LbIntoGate: // 0x09b5-0x09d4
            if (step_toward(kLbSlot, 5, 4)) { step_ = 1; continue; }
            actors_[kLbSlot] = {};
            member_ = 0;
            pc_ = Pc::MembersIntoGate;
            return frames(1);
        case Pc::MembersIntoGate: // 0x09d7-0x0a24
            if (member_ >= party_size) {
                gate_stage_ = 0x0f; // 0x0a27
                pc_ = Pc::GateClose;
                return frames(1);
            }
            if (step_toward(member_, 5, 4)) { step_ = 1; continue; }
            actors_[member_] = {};
            ++member_;
            return frames(1);
        case Pc::GateClose: // 0x0a30-0x0a37: stages 15..1
            if (--gate_stage_) return frames(1);
            pc_ = Pc::Teardown;
            break;
        case Pc::Teardown:
            // 0x0a39-0x0a6a: the floor blitted over the gate cell, the backbuffer
            // cleared to black, then story_screens 0x004b fizzles the screen to it.
            gate_floor_ = true;
            phase_ = EndgamePhase::Dissolve;
            pc_ = Pc::Story;
            return wait_for(EndgameWait::Dissolve);
        case Pc::Story:
            // 0x004e-0x0185: page 0 is composed and shown at once; from then on
            // each wait (0x017e, 0x01e6) shows page i while the poll passes BL =
            // i + 1 to the patch (0x0aee).
            phase_ = EndgamePhase::Story;
            if (sink.music) sink.music(sink.context, EndgameMusic::StoryScene, uint8_t(page_ + 1));
            pc_ = Pc::StoryNext;
            return wait_for(EndgameWait::Key);
        case Pc::StoryNext:
            if (page_ + 1 < kEndgameStoryPages) { ++page_; pc_ = Pc::Story; break; }
            // 0x01ed-0x0225: ENDSC.16 at (40,0) on black; endgame_datestamp
            // prints; the loop 0x04f9 (the patch selects Rule Britannia there).
            phase_ = EndgamePhase::Scroll;
            compose_scroll();
            if (sink.music) sink.music(sink.context, EndgameMusic::Finale, 0);
            return wait_for(EndgameWait::Forever);
        case Pc::ISee: // 0x0a76-0x0a81 (DS 0x84da)
            print(sink, "\"I see...\n");
            pc_ = Pc::Chair;
            return frames(0x28);
        case Pc::Chair:
            // 0x0a84-0x0a8f: ENDMSG 0x2d5, slot 0 one row up, sprite_step_redraw.
            print_record(sink, 10);
            --actors_[0].row;
            member_ = 0;
            moved_ = false;
            pc_ = Pc::StrandedWalks;
            step_ = 1;
            continue;
        case Pc::StrandedWalks: {
            // 0x0a92-0x0ac4: slot 2 to the eating chair, Lord British to the bed,
            // slot 0 to the other chair, round after round until nobody moves.
            static constexpr uint8_t kSlot[3] = {2, kLbSlot, 0}, kCol[3] = {8, 4, 8}, kRow[3] = {6, 1, 4};
            if (member_ == 3) {
                if (!moved_) {
                    phase_ = EndgamePhase::Stranded;
                    wander_index_ = 0;
                    // 0x0ae5 -> 0x0b1f -> 0x0b18: the patch selects Rule Britannia on every pass.
                    if (sink.music) sink.music(sink.context, EndgameMusic::Finale, 0);
                    pc_ = Pc::Wander;
                    break;
                }
                member_ = 0;
                moved_ = false;
            }
            const uint8_t i = member_++;
            if (step_toward(kSlot[i], kCol[i], kRow[i])) { moved_ = true; step_ = 1; }
            continue;
        }
        case Pc::Wander: {
            // 0x0ac9-0x0ae2: wander_sprite(1), (3), (4), (5), forever; each ends in run_n_frames(1).
            static constexpr uint8_t kOrder[4] = {1, 3, 4, 5};
            wander(kOrder[wander_index_]);
            wander_index_ = uint8_t((wander_index_ + 1) & 3);
            return frames(1);
        }
        }
    }
}

uint8_t EndgameScene::tile_at(int col, int row) const {
    if (col < 0 || row < 0 || col >= kEndgameRoomSide || row >= kEndgameRoomSide) return 0;
    return room_[row * kEndgameRoomSide + col];
}

bool EndgameScene::wander(uint8_t slot) {
    // wander_sprite 0x05a2: a coin, then up to 8 random directions until one
    // lands on visible floor (0x0607: the visible buffer, where an actor's cell
    // reads 0 -- the compositor clears it, 0x5388 -- so nobody walks into anyone).
    auto &a = actors_[slot];
    if (!a.active || !rand_range(rng_, 0, 1)) return false;
    for (int attempt = 0; attempt < 8; ++attempt) {
        int col = a.col, row = a.row;
        switch (rand_range(rng_, 0, 3)) {
        case 0: ++col; break;
        case 1: --col; break;
        case 2: ++row; break;
        default: --row; break;
        }
        if (tile_at(col, row) != kFloor) continue;
        bool occupied = false;
        for (const auto &o : actors_)
            if (o.active && o.col == col && o.row == row) occupied = true;
        if (occupied) continue;
        a.col = uint8_t(col);
        a.row = uint8_t(row);
        return true;
    }
    return false;
}

void EndgameScene::compose(PresentationSnapshot &s, OriginalRng &pose_rng) const {
    s = PresentationSnapshot{};
    s.center = {kEndgameRoomSide / 2, kEndgameRoomSide / 2};
    for (int i = 0; i < kEndgameRoomCells; ++i) {
        s.tiles[i] = room_[i];
        s.visible[i] = 1;
    }
    const int gate = kEndgameGateRow * kEndgameRoomSide + kEndgameGateCol;
    if (gate_floor_) s.tiles[gate] = kFloor; // 0x0a45: blitted over the gate, the map untouched
    bool actor_cell[kEndgameRoomCells]{};
    // 0x542a: slots 31 down to 0, so a lower slot drawn on the same cell wins.
    for (int slot = kEndgameActorSlots - 1; slot >= 0; --slot) {
        const auto &a = actors_[slot];
        if (!a.active || !a.tile || a.col >= kEndgameRoomSide || a.row >= kEndgameRoomSide) continue;
        const int cell = a.row * kEndgameRoomSide + a.col;
        const uint8_t under = room_[cell];
        uint8_t pose = a.tile;
        bool posed = false;
        // The pose selector 0x51b8: its gate (0x51bf-0x51e6) admits 0x1c,
        // 0x12-0x15, 0x28-0x2b and 0x40-0x7f (0x524a refuses >= 0x80).
        const uint8_t t = a.tile;
        if ((t == 0x1c || (t >= 0x12 && t < 0x16) || (t >= 0x28 && t < 0x2c) || t >= 0x40) && t < 0x80) {
            posed = true;
            if (under == 0xab) pose = 0x1a;                                         // 0x529a SleepingInBed
            else if (under == kMirror || under == 0x9e) pose = uint8_t(0x3c + rand_range(pose_rng, 0, 3)); // 0x52a2
            else if (under == 0x92) {                                               // 0x52aa: table to the south
                const uint8_t south = tile_at(a.col, a.row + 1);
                pose = south == 0x9a || south == 0x9c ? uint8_t(0x34 + rand_range(pose_rng, 0, 3)) : 0x32;
            } else if (under == 0x90) {                                             // 0x52da: table to the north
                const uint8_t north = tile_at(a.col, a.row - 1);
                pose = north == 0x9b || north == 0x9c ? uint8_t(0x38 + rand_range(pose_rng, 0, 3)) : 0x30;
            } else if (under == 0x91 || under == 0x93) pose = uint8_t(0x30 + (under & 3)); // 0x530e
            else if (under == 0x84) pose = uint8_t(0x60 + rand_range(pose_rng, 0, 3));    // 0x5276
            else if (under == 0x85) pose = uint8_t(0x64 + rand_range(pose_rng, 0, 3));    // 0x5280
            else {
                posed = false;
                // 0x532c-0x534e: standing south of a mirror lights the reflection in it.
                if (a.row > 0 && tile_at(a.col, a.row - 1) == kMirror)
                    s.tiles[cell - kEndgameRoomSide] = 0x9e;
            }
        }
        s.tiles[cell] = int16_t(0x100 + pose);
        s.actor_ids[cell] = posed ? 0 : 0x50000U + uint32_t(slot);
        s.actor_seeds[cell] = posed ? 0 : uint8_t(pose & 0xfc);
        actor_cell[cell] = true;
    }
    // 0x56e6-0x5706: [0x5887] 1..15 draws the gate cell through 0x1112 -- unless an actor stands on it.
    if (gate_stage_ > 0 && gate_stage_ < 0x10 && room_[gate] == kGateTile && !gate_floor_ && !actor_cell[gate]) {
        s.gate_x = kEndgameGateCol;
        s.gate_y = kEndgameGateRow;
        s.gate_rows = gate_stage_;
    }
    s.endgame_recolor = true;
    for (int i = 0; i < kEndgameRoomCells; ++i) {
        const auto k = tile_animation_kind(s.tiles[i]);
        s.animated[i] = k == TileAnimationKind::TileCycle || k == TileAnimationKind::WaterScroll ||
                        k == TileAnimationKind::WaterComposite || k == TileAnimationKind::FireNoise ||
                        k == TileAnimationKind::ActorProgram;
        s.any_animated |= s.animated[i] != 0;
    }
}

// ---------------------------------------------------------------------------
// endgame_datestamp 0x0326 on the 40x25 text screen (window 0, white on black).
// ---------------------------------------------------------------------------

void EndgameScene::put_char(uint8_t c) {
    // put_char 0x16ba. 0xfd toggles reverse video (0x17a5), 0xfc centres (0x1799).
    if (c == 0xfd) { scroll_inverse_ = !scroll_inverse_; return; }
    if (c == 0xfc) { scroll_center_ = true; return; }
    if (c == 0xfb) { scroll_center_ = false; return; }
    if (c == '\n' || c == '\r') {
        if (c == '\n') ++cursor_row_;
        cursor_col_ = 0;
        return;
    }
    if (cursor_row_ < kEndgameScrollRows && cursor_col_ < kEndgameScrollCols) {
        auto &cell = scroll_[cursor_row_ * kEndgameScrollCols + cursor_col_];
        cell.ch = uint8_t(c & 0x7f);
        cell.flags = uint8_t(kEndgameCellPrinted | (font_ ? kEndgameCellRunes : 0) |
                             (scroll_inverse_ ? kEndgameCellInverse : 0));
    }
    if (++cursor_col_ >= kEndgameScrollCols) { cursor_col_ = 0; ++cursor_row_; } // 0x1735-0x1745
}

void EndgameScene::scroll_print(const char *s) {
    // print_string 0x1850, window 0 (x1 - x0 = 39): line by line, each line
    // centred at col (avail - last) / 2 (0x19ea) while 0xfc is on. Every line
    // endgame_datestamp prints fits (at most 34 columns), so the word-wrap
    // branch (0x1927-0x19c7) is never taken and is not modelled.
    for (size_t i = 0; s[i];) {
        size_t n = 0;
        while (s[i + n] && s[i + n] != '\n' && s[i + n] != '\r') ++n;
        if (!n) { put_char(uint8_t(s[i++])); continue; }
        if (scroll_center_) {
            const int avail = 39 - cursor_col_, last = int(n) - 1;
            const int col = (avail - last) / 2;
            if (col >= 0 && col < kEndgameScrollCols) cursor_col_ = uint8_t(col); // 0x1bf2's own bounds
        }
        for (size_t k = 0; k < n; ++k) put_char(uint8_t(s[i + k]));
        i += n;
    }
}

void EndgameScene::accum(const char *s) {
    // text_accum_char 0x023a: up to 0x27 characters; flushed through
    // print_string when the buffer ends in '\n'.
    while (*s && accum_length_ < 0x27) accum_[accum_length_++] = *s++;
    accum_[accum_length_] = 0;
    if (accum_length_ && accum_[accum_length_ - 1] == '\n') {
        scroll_print(accum_);
        accum_length_ = 0;
    }
}

void EndgameScene::accum_cardinal(int n) {
    if (n >= 0 && n < 21) { accum(kUnits[n]); return; } // 0x0290-0x0299
    if (n < 0 || n >= 100) return;
    accum(kTens[n / 10]);                               // 0x029c-0x02ad
    if (n % 10) { accum("-"); accum(kUnits[n % 10]); } // 0x02bd-0x02ce (DS 0x82c6)
}

void EndgameScene::accum_ordinal(int n) {
    if (n >= 0 && n < 13) { accum(kOrdinals[n]); return; }     // 0x02d9
    if (n < 20) { accum_cardinal(n); accum("th"); return; }    // 0x02ea (DS 0x831e)
    accum("Twent");                                            // 0x02fc (DS 0x8322)
    if (n == 20) { accum("ieth"); return; }                    // DS 0x8328
    accum("y-");                                               // DS 0x832e
    if (n - 20 < 13) accum(kOrdinals[n - 20]);                 // 0x0315: table 0x3e18 = 0x3e40 - 40
}

void EndgameScene::compose_scroll() {
    for (auto &cell : scroll_) cell = {};
    const auto &t = game_->time;
    cursor_col_ = 0;
    cursor_row_ = 1; // 0x0333 0x1bf2(0, 1)
    font_ = 0;
    put_char(0xfd); // 0x033a
    put_char(0xfc); // 0x0341
    scroll_print("Be it known that on\n");
    accum_length_ = 0; // 0x034b
    accum("the ");
    accum_ordinal(t.day);
    accum(" Day of\n");
    accum("the ");
    accum_ordinal(t.month);
    accum(" Month\n");
    scroll_print("of the Year\n");
    accum_cardinal(t.year / 100);
    accum(" Hundred\n");
    accum_cardinal(t.year % 100);
    accum("\n\n");
    accum(game_->party.character_count ? game_->party.characters[0].name : ""); // DS 0x55a8
    accum(" the Avatar\n\n");
    scroll_print("saved the life\n");
    scroll_print("of our sovereign\n");
    scroll_print("Lord British, thereby\n");
    scroll_print("saving our people\n");
    scroll_print("and our land.\n\n");
    font_ = 1; // 0x03e2 0x1c9e(1)
    scroll_print("[E@QUE_@OF@[E@AVATAR\n");
    scroll_print("IS@FOREVER\n\n\n\n");
    font_ = 0;      // 0x03f6
    put_char(0xfd); // 0x03fd
    scroll_print("Report now, thy Quest compleat in\n");
    const auto p = endgame_scroll_playtime(t.year, t.month, t.day);
    char number[12];
    if (p.years) { // 0x043e-0x047d
        std::snprintf(number, sizeof(number), "%d", p.years);
        accum(number);
        accum(" year");
        if (p.years > 1) accum("s");
        if (p.months || p.days) accum(", ");
    }
    if (p.months) { // 0x0480-0x04b9
        std::snprintf(number, sizeof(number), "%d", p.months);
        accum(number);
        accum(" month");
        if (p.months > 1) accum("s");
        if (p.days) accum(", ");
    }
    if (p.days) { // 0x04bc-0x04e8
        std::snprintf(number, sizeof(number), "%d", p.days);
        accum(number);
        accum(" day");
        if (p.days > 1) accum("s");
    }
    accum("\n"); // 0x04eb (DS 0x845a)
    scroll_print("to Lord British at Origin Systems!");
}

} // namespace openu5
