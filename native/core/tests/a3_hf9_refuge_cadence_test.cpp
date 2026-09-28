// Alpha 3 A3-HF9 (H-185 / D-42) -- the Refuge's cadence and its karma getkey,
// on the real check_refuge script and the real NarrativeScenePacer.
//
//   a3_hf9_refuge_cadence
//
// BLCKTHRN.OVL party_refuge 0x0910 (near calls through base 0xA290): what the
// original blocks in after each thing it shows, read from the bytes --
//
//   0946 delay(10)                       the world is still drawn
//   095f "An unending darkness..."       0962-098c black + RECT_DISSOLVE (no timer)
//   09d6 "Thou hast found refuge."       09dd delay(14)
//   09e4 "No evil lives here..."         09eb delay(28)
//   09f2 "But thy slumber is disturbed!" 0a0d-0a49 six tone_sweeps, 260,000 samples
//   0a4f "Someone shouts ... AVENTARI"   0a56 delay(6)     (one print)
//   0a7c FIZZLE_IN 0x5e (no timer)       0a90 delay(4)
//   0aae FIZZLE_IN 0x5f (no timer)       0ac2 delay(4)
//   0ac9 "There is a peal of thunder!"   0acc, 0acf screen_shake_fx 0x3072 twice
//   0af5 FIZZLE_IN 0x174 (no timer)
//   0b03-0b3b the KARMA.DAT record, quoted  0b3e GETKEY (0x266c): no timeout
//   0b45 "Strange words are intoned."    0b4c delay(4); 0b54-0bb1 one tone_sweep
//                                        of 0x7530 samples per member
//   0bba "Vertigo..."                    0bc1 delay(4)
//   0bc4-0bfa black + RECT_DISSOLVE      0bfd the karma floor, the castle
//
// The device's units, stated here and not taken from the model: one tick 55 ms
// (delay/run_n_frames, A), tone_sweep at 25,806 samples/s (B), a render-bound
// fizzle/dissolve held one tick (D), the device shake 8 x 117 ms (C).
//
//   C1  the script: every delay at its instruction, the one getkey, the holds
//   C2  cadence: every beat released at the binary's instant, nothing earlier
//   C3  the getkey: no clock ends it, one key ends it, a key ends only it
//   C4  keys during the busy loops are swallowed, never kept for the getkey
//   C5  after the key: the rest is deferred to its own clock
//   C6  state: nothing mutates before the scene ends; the speech is the death karma's
//   C7  the unpaced harness drains the whole scene with no key
//   C8  cancel at the getkey leaves nothing behind
//   C9  unchanged: TrollSneak's cadence, and no getkey in it
#include "openu5/commands.h"
#include "openu5/narrative_scene.h"
#include "openu5/quest_world.h"
#include "openu5/scene_timing.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
bool check(bool good, const std::string &id, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s %s\n", good ? "GREEN" : "RED", id.c_str(), label.c_str());
    return good;
}
std::string n(long long v) { return std::to_string(v); }

constexpr int64_t kTick = 55;
constexpr int64_t sweep_ms(int64_t samples) { return samples * 1000 / 25806; }
constexpr int64_t kSlumberMs = sweep_ms(3 * 0xc350 + 0x7530 + 2 * 0x9c40); // DS 0x372c: 10,075
constexpr int64_t kShakeMs = 8 * 117;                                     // 936
constexpr int64_t kFizzleMs = kTick;                                      // D floor
constexpr int64_t kStepMs = 5;                                            // the device frame
/** A hold of `want` ms is seen at the first 5 ms frame at or after it. */
bool at(int64_t got, int64_t want) { return got >= want && got < want + kStepMs; }

struct Objects {
    std::vector<QuestObject> values;
};
size_t obj_count(void *p) { return static_cast<Objects *>(p)->values.size(); }
QuestObject obj_read(void *p, size_t i) { return static_cast<Objects *>(p)->values[i]; }
bool obj_reserve(void *, size_t) { return true; }
void obj_append(void *p, const QuestObject &o) { static_cast<Objects *>(p)->values.push_back(o); }
void obj_erase(void *p, size_t i) {
    auto &v = static_cast<Objects *>(p)->values;
    v.erase(v.begin() + ptrdiff_t(i));
}
void obj_write(void *p, size_t i, const QuestObject &o) { static_cast<Objects *>(p)->values[i] = o; }
const char *karma_record(void *, int32_t index) {
    static const char *records[] = {"\"record 0\"", "\"record 1\"", "\"record 2\"", "\"record 3\"",
                                    "\"record 4\"", "\"record 5\""};
    return index >= 0 && index < 6 ? records[index] : nullptr;
}

/** A wiped party of `members`, a real context, and the script check_refuge emits. */
struct World {
    std::vector<uint8_t> tiles = std::vector<uint8_t>(32 * 32, 5);
    MapData small{{17, 1}, nullptr, 0};
    WorldData world{};
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    Objects objects{};
    QuestWorldServices quest{};
    CommandContext context{game, turn, travel, commands, world};
    RefugeScript last{};
    int emitted = 0;
    World(int members, int karma) {
        small.tiles = tiles.data();
        small.size = tiles.size();
        world.small_maps = &small;
        world.small_map_count = 1;
        game.position = {{4, 4}, {13, 0}};
        game.karma = uint8_t(karma);
        game.party.character_count = game.party.party_size = uint8_t(members);
        for (int i = 0; i < members; ++i) {
            auto &ch = game.party.characters[i];
            ch.status = 'D';
            ch.current_hp = 0;
            ch.max_hp = 40;
        }
        quest.context = &objects;
        quest.count = obj_count;
        quest.read = obj_read;
        quest.reserve = obj_reserve;
        quest.append = obj_append;
        quest.erase = obj_erase;
        quest.write = obj_write;
        quest.karma_record = karma_record;
        context.quest_world = &quest;
    }
};

/** What the owner saw, and when (ms from the scene's start). */
struct Seen {
    struct Beat {
        int64_t t;
        std::string text, sfx;
        RefugePhase phase;
        bool shake;
    };
    std::vector<Beat> beats;
    int64_t now = 0;
    static void on_beat(void *p, const NarrativeSceneBeat &b) {
        auto &s = *static_cast<Seen *>(p);
        s.beats.push_back({s.now, b.text ? b.text : "", b.sfx ? b.sfx : "", b.phase, b.shake});
    }
    NarrativeSceneSink sink() { return {this, on_beat}; }
    int64_t first(const std::string &text) const {
        for (const auto &b : beats)
            if (b.text == text) return b.t;
        return -1;
    }
    int64_t phase(RefugePhase p) const {
        for (const auto &b : beats)
            if (b.phase == p) return b.t;
        return -1;
    }
};
void no_forward(void *, const GameEvent &) {}

struct Pacer {
    NarrativeSceneStep steps[64]{};
    char arena[2048]{};
    NarrativeScenePacer pacer;
    Seen seen;
    int64_t base = 100000;
    Pacer(bool paced = true) {
        pacer.attach({steps, 64, arena, sizeof(arena)});
        pacer.set_paced(paced);
    }
    static void take(void *p, const GameEvent &e) { static_cast<Pacer *>(p)->pacer.enqueue(e); }
    CommandStatus start(World &w) {
        const auto status = check_refuge(w.context, {this, take});
        pump();
        return status;
    }
    void pump() { pacer.pump(uint32_t(base + seen.now), seen.sink(), {nullptr, no_forward}); }
    /** Advance the device clock in 5 ms frames, pumping each one. */
    void run(int64_t ms) {
        for (int64_t t = 0; t < ms; t += kStepMs) {
            seen.now += kStepMs;
            pump();
        }
    }
    /** Frames until `done` (or `max` ms). */
    bool until(bool (*done)(const Pacer &), int64_t max) {
        for (int64_t t = 0; t < max; t += kStepMs) {
            if (done(*this)) return true;
            seen.now += kStepMs;
            pump();
        }
        return done(*this);
    }
};

// --- C1: the script -----------------------------------------------------------------
void test_script() {
    std::printf("\nC1 the script check_refuge emits, against the bytes\n");
    World w(2, 50);
    GameEvent got{};
    struct Grab {
        World *w;
        static void emit(void *p, const GameEvent &e) {
            auto &g = *static_cast<Grab *>(p);
            if (e.kind == GameEventKind::Refuge && e.refuge) {
                g.w->last = *e.refuge;
                ++g.w->emitted;
            }
        }
    } grab{&w};
    (void)got;
    check(check_refuge(w.context, {&grab, Grab::emit}) == CommandStatus::Success && w.emitted == 1, "C1.0",
          "a wiped party emits one Refuge script");
    const auto &b = w.last.beats;
    const int16_t delays[17] = {10, -1, 14, 28, -1, -1, 6, 4, 4, -1, -1, -1, -1, -1, 4, 4, -1};
    bool delays_ok = true;
    std::string got_delays;
    for (int i = 0; i < 17; ++i) {
        delays_ok = delays_ok && b[i].delay == delays[i];
        got_delays += n(b[i].delay) + (i < 16 ? "," : "");
    }
    check(delays_ok, "C1.1", "every delay is a `call 0x7e6a` at its instruction, and no other: " + got_delays);
    check(!b[0].scene && !b[0].message && !b[0].sfx && b[0].delay == 10, "C1.2",
          "0x0946 delay(10) comes FIRST, before the darkness line (was after it)");
    int keys = 0, at = -1;
    for (int i = 0; i < 17; ++i)
        if (b[i].key_wait) ++keys, at = i;
    check(keys == 1 && at == 13 && b[13].message && std::strcmp(b[13].message, "\"record 2\"") == 0 &&
              b[13].delay < 0,
          "C1.3", "exactly one getkey (0x0b3e), on the karma speech, and no timed delay there (was 8)");
    check(b[10].sfx && b[11].sfx && b[10].shake && b[11].shake && b[10].delay < 0 && b[11].delay < 0 &&
              b[12].delay < 0,
          "C1.4", "two screen_shake_fx, no delay after the second or after the apparition (were 2 and 2)");
    check(b[4].sweep_samples == 260000 && b[14].sweep_samples == 2 * 0x7530, "C1.5",
          "the slumber melody (260,000 samples) and one 0x7530 revival tone per member (2 members)");
    check(b[1].fizzle && b[7].fizzle && b[8].fizzle && b[12].fizzle && b[16].fizzle &&
              !b[2].fizzle && !b[4].fizzle,
          "C1.6", "the render-bound dissolves and fizzles: darkness, both ghosts, the apparition, vertigo");
}

// --- C2..C5: cadence, the getkey, keys, the deferred rest -----------------------------
void test_cadence() {
    std::printf("\nC2 cadence, C3 the getkey, C4 swallowed keys, C5 the deferred rest\n");
    World w(2, 50);
    Pacer p;
    check(p.start(w) == CommandStatus::Success && p.pacer.active() && p.pacer.modal(), "C2.0",
          "the scene is modal from its first beat");
    check(!p.pacer.mounted() && p.seen.first("An unending darkness engulfs thee...") < 0, "C2.1",
          "during delay(10) the viewport is not the scene's yet and no line is shown");
    // C4: a key in every busy loop before the getkey (one per 500 ms).
    int refused = 0, offered = 0;
    for (int64_t t = 0; t < 60000 && !p.pacer.awaiting_key(); t += kStepMs) {
        if (t % 500 == 0) {
            ++offered;
            refused += p.pacer.advance_key() ? 0 : 1;
        }
        p.seen.now += kStepMs;
        p.pump();
    }
    const int64_t dark = p.seen.first("An unending darkness engulfs thee...");
    const int64_t refuge = p.seen.first("Thou hast found refuge.");
    const int64_t evil = p.seen.first("No evil lives here, only peace and darkness.");
    const int64_t slumber = p.seen.first("But thy slumber is disturbed!");
    const int64_t shout = p.seen.first("Someone shouts");
    const int64_t fortis = p.seen.first("\"FORTIS FORTUNA AVENTARI\"");
    const int64_t left = p.seen.phase(RefugePhase::GhostLeft);
    const int64_t both = p.seen.phase(RefugePhase::GhostBoth);
    const int64_t thunder = p.seen.first("There is a peal of thunder!");
    const int64_t apparition = p.seen.phase(RefugePhase::Apparition);
    const int64_t speech = p.seen.first("\"record 2\"");
    std::printf("    darkness %lld, refuge %lld, evil %lld, slumber %lld, shout %lld, ghosts %lld/%lld, thunder %lld, "
                "apparition %lld, speech %lld ms\n",
                (long long)dark, (long long)refuge, (long long)evil, (long long)slumber, (long long)shout,
                (long long)left, (long long)both, (long long)thunder, (long long)apparition, (long long)speech);
    check(dark == 10 * kTick && p.seen.phase(RefugePhase::Void) == dark, "C2.2",
          "the darkness line and the black viewport after delay(10) = 550 ms (got " + n(dark) + ")");
    check(refuge == dark + kFizzleMs, "C2.3",
          "\"Thou hast found refuge.\" right after the dissolve's one-tick floor (got +" + n(refuge - dark) + ")");
    check(evil == refuge + 14 * kTick, "C2.4", "\"No evil lives here\" after delay(14) = 770 ms (got +" +
                                                   n(evil - refuge) + ")");
    check(slumber == evil + 28 * kTick, "C2.5",
          "\"But thy slumber\" after delay(28) = 1,540 ms (got +" + n(slumber - evil) + ")");
    check(shout == slumber + kSlumberMs && fortis == shout, "C2.6",
          "the shout after the six slumber sweeps, 10,075 ms, as one print (got +" + n(shout - slumber) + ")");
    check(left == fortis + 6 * kTick, "C2.7", "the left figure after delay(6) = 330 ms (got +" + n(left - fortis) + ")");
    check(both == left + kFizzleMs + 4 * kTick && thunder == both + kFizzleMs + 4 * kTick, "C2.8",
          "each figure: its fizzle floor + delay(4) = 275 ms (got +" + n(both - left) + ", +" + n(thunder - both) + ")");
    int shakes = 0;
    int64_t shake1 = -1, shake2 = -1;
    for (const auto &b : p.seen.beats)
        if (b.shake) (shakes++ ? shake2 : shake1) = b.t;
    check(shakes == 2 && shake1 == thunder && at(shake2 - thunder, kShakeMs) && at(apparition - shake2, kShakeMs),
          "C2.9", "two shakes of 936 ms each, the first with the line, then the apparition (got " + n(shakes) +
                      " at +" + n(shake1 - thunder) + "/+" + n(shake2 - thunder) + ", apparition +" +
                      n(apparition - thunder) + ")");
    check(speech == apparition + kFizzleMs, "C2.10",
          "the karma speech right after the apparition's fizzle floor (got +" + n(speech - apparition) + ")");
    check(offered > 20 && refused == offered, "C4.1",
          "a key in any busy loop before the getkey is swallowed (" + n(refused) + "/" + n(offered) + " refused)");
    check(p.pacer.awaiting_key(), "C4.2", "and none of them was kept: the getkey still waits for its own key");

    // C3: no clock ends the getkey.
    const size_t shown = p.seen.beats.size();
    p.run(10 * 60 * 1000);
    check(p.pacer.awaiting_key() && p.seen.beats.size() == shown &&
              p.seen.first("Strange words are intoned.") < 0,
          "C3.1", "ten minutes later the speech still waits; nothing after it is shown");
    check(w.game.karma == 50 && w.game.party.characters[0].status == 'D' && w.game.position.map.location == 13,
          "C6.1", "during the getkey nothing has mutated: karma 50, the party fallen, still at location 13");
    const int64_t key = p.seen.now;
    check(p.pacer.advance_key(), "C3.2", "one key ends the getkey");
    check(!p.pacer.advance_key(), "C3.3", "a second key in the same frame ends nothing (one key, one wait)");
    p.pump();
    const int64_t words = p.seen.first("Strange words are intoned.");
    check(words == key, "C5.1", "\"Strange words are intoned.\" at the key's frame (got +" + n(words - key) + ")");
    check(p.seen.first("Vertigo...") < 0 && !p.pacer.advance_key(), "C5.2",
          "\"Vertigo...\" is not shown with it, and the next key is swallowed");
    p.until([](const Pacer &q) { return !q.pacer.active(); }, 20000);
    const int64_t vertigo = p.seen.first("Vertigo...");
    const int64_t flash = p.seen.phase(RefugePhase::Vertigo);
    const int64_t revival = sweep_ms(2 * 0x7530);
    check(vertigo == words + 4 * kTick + sweep_ms(2 * 0x7530), "C5.3",
          "\"Vertigo...\" after delay(4) + two revival tones (" + n(4 * kTick + revival) + " ms; got +" +
              n(vertigo - words) + ")");
    check(flash == vertigo + 4 * kTick, "C5.4", "the last dissolve after delay(4) (got +" + n(flash - vertigo) + ")");
    const auto done = p.seen.now;
    check(!p.pacer.active() && p.pacer.take_completion() == NarrativeScene::Refuge &&
              done - flash >= kFizzleMs && done - flash <= kFizzleMs + kStepMs,
          "C5.5", "the scene ends one tick after the last dissolve, and reports its completion once");
    check(w.game.karma == 50 && w.game.party.characters[0].status == 'D', "C6.2",
          "still nothing mutated when the scene ends: the owner applies resolve_refuge");
    check(resolve_refuge(w.context, {nullptr, no_forward}) == CommandStatus::Success && w.game.karma == 75 &&
              w.game.party.characters[0].status == 'G' && w.game.position.map.location == 17,
          "C6.3", "resolve_refuge (0x0bfd): karma floored to 75, the party revived, in the castle");
}

// --- C6: the speech is the DEATH karma's ----------------------------------------------
void test_karma() {
    std::printf("\nC6 the speech is chosen by the karma at death (0x0b03 karma/20)\n");
    for (int karma : {10, 39, 40, 90}) {
        World w(1, karma);
        Pacer p;
        p.start(w);
        p.until([](const Pacer &q) { return q.pacer.awaiting_key(); }, 30000);
        const std::string want = "\"record " + n(std::min(karma / 20, 4)) + "\"";
        check(p.pacer.awaiting_key() && p.seen.first(want) >= 0 && w.game.karma == karma, "C6.k" + n(karma),
              "karma " + n(karma) + ": " + want + " waits for its key; karma untouched");
        p.pacer.advance_key();
        p.until([](const Pacer &q) { return !q.pacer.active(); }, 30000);
        resolve_refuge(w.context, {nullptr, no_forward});
        check(w.game.karma == (karma < 75 ? 75 : karma), "C6.f" + n(karma),
              "after the scene: karma " + n(w.game.karma) + " (the 0x0bfd floor only raises)");
    }
}

// --- C7: the unpaced harness ---------------------------------------------------------
void test_unpaced() {
    std::printf("\nC7 the unpaced harness contract\n");
    World w(3, 50);
    Pacer p(false);
    p.start(w);
    check(!p.pacer.active() && !p.pacer.awaiting_key() && p.seen.beats.size() == 17 &&
              p.seen.first("Vertigo...") >= 0 && p.pacer.take_completion() == NarrativeScene::Refuge,
          "C7", "unpaced, all 17 beats drain in one pump with no key, and the completion is reported");
}

// --- C8: cancel at the getkey -----------------------------------------------------------
void test_cancel() {
    std::printf("\nC8 cancel at the getkey\n");
    World w(1, 50);
    Pacer p;
    p.start(w);
    p.until([](const Pacer &q) { return q.pacer.awaiting_key(); }, 30000);
    const bool was = p.pacer.awaiting_key();
    p.pacer.cancel();
    check(was && !p.pacer.active() && !p.pacer.awaiting_key() && !p.pacer.mounted() &&
              p.pacer.take_completion() == NarrativeScene::None && !p.pacer.advance_key(),
          "C8.1", "cancel drops the wait, the stage and the completion; a later key ends nothing");
    // A new scene after it starts from its own first beat.
    World again(1, 50);
    Pacer q;
    q.start(again);
    q.run(100);
    check(q.pacer.active() && !q.pacer.awaiting_key() && q.seen.beats.size() == 1 && q.seen.beats[0].text.empty() &&
              !q.pacer.mounted(),
          "C8.2", "a new Refuge after it starts over at its delay(10), before any line");
}

// --- C9: TrollSneak unchanged --------------------------------------------------------
void test_troll() {
    std::printf("\nC9 TrollSneak unchanged\n");
    Pacer p;
    TrollSneakScript script;
    script.beats[script.count++] = {"\nThou spieth trolls under the bridge!\n\n", 10, false};
    script.beats[script.count++] = {".", 5, true};
    script.beats[script.count++] = {"Trolls evaded!\n", -1, false};
    GameEvent e;
    e.kind = GameEventKind::TrollSneak;
    e.troll_sneak = &script;
    p.pacer.enqueue(e);
    p.pump();
    const int64_t first = p.seen.beats.empty() ? -1 : p.seen.beats[0].t;
    p.until([](const Pacer &q) { return !q.pacer.active(); }, 5000);
    check(p.seen.beats.size() == 3 && first == 0 && p.seen.beats[1].t == 10 * kTick &&
              p.seen.beats[2].t == 15 * kTick && !p.pacer.awaiting_key(),
          "C9", "run_n_frames(10) then (5), no getkey, exactly as before");
}
} // namespace

int main() {
    test_script();
    test_cadence();
    test_karma();
    test_unpaced();
    test_cancel();
    test_troll();
    std::printf("\na3_hf9_refuge_cadence: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
