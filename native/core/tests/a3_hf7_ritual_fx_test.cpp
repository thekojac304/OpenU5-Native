// Alpha 3 A3-HF7 (H-184 / D-41) -- the rite's effect model and the pacer's
// Effect hold, in isolation. The runtime test (a3_hf7_ritual_fx_runtime) drives
// the real rite, Codex and screen; this one pins, on hand-built event streams
// shaped exactly as shrine.cpp emits them, what each event does to the
// viewport mask, how long the presentation holds after it, and the queue's
// contract for an Effect: timed, never ended by a key, stacked correctly
// against the HF6 getkeys, and never taken by the unpaced cadence.
#include "openu5/dialogue_pacer.h"
#include "openu5/ritual_fx.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool good, const char *id, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s %s\n", good ? "GREEN" : "RED", id, label.c_str());
}
std::string n(long long v) { return std::to_string(v); }

GameEvent ev(GameEventKind k, const char *text = nullptr, int32_t note = 0) {
    GameEvent e{};
    e.kind = k;
    e.text = text;
    e.note = note;
    return e;
}
GameEvent msg(const char *t) { return ev(GameEventKind::Message, t); }
GameEvent sfx(const char *id) { return ev(GameEventKind::Sfx, id); }
GameEvent quake(int32_t note = 0) { return ev(GameEventKind::Quake, nullptr, note); }

// Walks a stream through the model the way the pacer does: ask, then present.
struct Walk {
    std::string holds, masks;
};
Walk walk(RitualFx &fx, const std::vector<GameEvent> &events) {
    Walk w;
    for (const auto &e : events) {
        w.holds += n(fx.hold_after_ms(e)) + ",";
        fx.present(e);
        w.masks += n(fx.mask()) + ",";
    }
    return w;
}

const std::vector<GameEvent> kWellDone = {msg("WELL DONE!"), ev(GameEventKind::RitualInvert), sfx("shrine-well-done"),
                                          quake(), sfx("quake"), msg("Intelligence +1\n"),
                                          ev(GameEventKind::PartyChanged)};
const std::vector<GameEvent> kDonation = {msg("ALAKAZAM!\n"), ev(GameEventKind::RitualInvert, "donation"),
                                          sfx("shrine-donation"), ev(GameEventKind::PartyChanged)};
const std::vector<GameEvent> kCeremony = {quake(4), sfx("quake"), quake(15), sfx("quake"), quake(4), sfx("quake"),
                                          msg("A STRANGE WIND..."), ev(GameEventKind::ShrineKeyWait)};

void test_model() {
    std::printf("\nP the model (openu5/ritual_fx.h)\n");
    check(tone_sweep_ms(kWellDoneSweepSamples) == 5347 && tone_sweep_ms(kDonationSweepSamples) == 7130 &&
              kWellDoneSweepSamples == 138000 && kDonationSweepSamples == 184000,
          "P0", "the sweep windows: 2 x 460 x 0x96 = 138,000 and 2 x 460 x 0xc8 = 184,000 samples (5,347 / 7,130 ms)");
    RitualFx fx;
    auto w = walk(fx, kWellDone);
    check(w.masks == "0,15,15,15,15,15,0," && w.holds == "0,0,5347,936,0,0,550,", "P1",
          "WELL DONE: XOR 15 at the invert, held through the sweeps (5347) and the shake (936), restored by the "
          "last event with run_n_frames(10) (550) -- holds " + w.holds + " masks " + w.masks);
    check(!fx.active(), "P1B", "and nothing is left active");
    w = walk(fx, kDonation);
    check(w.masks == "0,15,15,0," && w.holds == "0,0,7130,550,", "P2",
          "donation: XOR 15, its own sweeps (7130), no shake, restored -- holds " + w.holds + " masks " + w.masks);
    w = walk(fx, kCeremony);
    check(w.masks == "4,4,11,11,15,15,15,0," && w.holds == "936,0,936,0,936,0,55,0,", "P3",
          "Codex: the XORs accumulate 4, 11, 15 over three held shakes; the text holds one tick; the getkey "
          "restores -- holds " + w.holds + " masks " + w.masks);
    check(!fx.active(), "P3B", "and nothing is left active");
    const std::vector<GameEvent> plain = {quake(), sfx("quake"), quake(), ev(GameEventKind::PartyChanged),
                                          msg("A line"), ev(GameEventKind::ShrineKeyWait)};
    w = walk(fx, plain);
    check(w.masks == "0,0,0,0,0,0," && w.holds == "0,0,0,0,0,0,", "P4",
          "outside a rite a quake, a party change, a line and a getkey do nothing and hold nothing (the shard, "
          "the Word of Power and HF6 keep their behaviour)");
    fx.present(ev(GameEventKind::RitualInvert));
    fx.present(quake(4));
    fx.clear();
    check(fx.mask() == 0 && !fx.active() && fx.hold_after_ms(ev(GameEventKind::PartyChanged)) == 0, "P5",
          "clear() (a load) leaves no mask and no pending restore");
}

// A pacer with the model wired in, and a sink that presents like the runtime.
struct Bench {
    DialoguePacerStep steps[16]{};
    char text[512]{};
    DialoguePacer pacer;
    RitualFx fx;
    std::vector<std::string> seen;
    explicit Bench(uint32_t pause_ms = 1540, size_t capacity = 16) {
        pacer.attach({steps, capacity, text, sizeof(text)});
        pacer.set_pause_ms(pause_ms);
        pacer.set_effect_hold({this, [](void *p, const GameEvent &e) { return static_cast<Bench *>(p)->fx.hold_after_ms(e); }});
    }
    EventSink sink() {
        return {this, [](void *p, const GameEvent &e) {
                    auto &b = *static_cast<Bench *>(p);
                    b.fx.present(e);
                    b.seen.push_back(e.kind == GameEventKind::Quake ? "Q" + n(e.note)
                                     : e.kind == GameEventKind::ShrineKeyWait ? std::string("W")
                                     : e.kind == GameEventKind::Sfx ? "S:" + std::string(e.text ? e.text : "")
                                     : e.kind == GameEventKind::Message ? "M:" + std::string(e.text ? e.text : "")
                                                                       : "E" + n(int(e.kind)));
                }};
    }
    // The runtime's consume_event: offer, else forward.
    void emit(const GameEvent &e, uint32_t now) {
        if (!pacer.offer(e, now, sink())) sink().emit(this, e);
    }
    std::string joined() const {
        std::string out;
        for (const auto &x : seen) out += x + ",";
        return out;
    }
};

void test_pacer() {
    std::printf("\nQ the pacer's Effect hold\n");
    {
        Bench b;
        for (const auto &e : kWellDone) b.emit(e, 1000);
        check(b.joined() == "M:WELL DONE!,E" + n(int(GameEventKind::RitualInvert)) + ",S:shrine-well-done," &&
                  b.pacer.in_effect() && b.fx.mask() == 15,
              "Q1", "WELL DONE from idle: the sweep cue is delivered and holds the rest (" + b.joined() + ")");
        check(b.pacer.advance_key(2000, b.sink()) && b.pacer.in_effect() && b.seen.size() == 3, "Q2",
              "a key inside the effect is swallowed: taken, nothing released");
        b.pacer.pump(1000 + 5346, b.sink());
        check(b.seen.size() == 3, "Q3", "one ms before the sweeps end nothing moves");
        b.pacer.pump(1000 + 5347, b.sink());
        check(b.seen.size() == 4 && b.seen.back() == "Q0" && b.pacer.in_effect(), "Q4",
              "at 5347 ms the shake is released and holds in turn");
        b.pacer.pump(1000 + 5347 + 936, b.sink());
        check(b.fx.mask() == 0 && b.pacer.in_effect() && b.seen.back() == "E" + n(int(GameEventKind::PartyChanged)),
              "Q5", "after the shake the reward line and the restore; run_n_frames(10) still holds (" + b.joined() + ")");
        b.pacer.pump(1000 + 5347 + 936 + 550, b.sink());
        check(!b.pacer.holding() && b.pacer.queued() == 0, "Q6", "then the pacer is idle");
    }
    {
        Bench b;
        b.emit(msg("the page"), 0);
        b.emit(ev(GameEventKind::ShrineKeyWait), 0); // 0x0d9f
        for (const auto &e : kCeremony) b.emit(e, 0);
        b.emit(msg("Thou dost read:"), 0);
        b.emit(ev(GameEventKind::ShrineKeyWait), 0); // 0x0e16
        check(b.pacer.awaiting_key() && b.fx.mask() == 0 && b.joined() == "M:the page,W,", "Q7",
              "the ceremony waits behind the page's getkey with the viewport normal");
        b.pacer.advance_key(100, b.sink());
        check(b.pacer.in_effect() && b.fx.mask() == 4 && b.joined() == "M:the page,W,Q4,", "Q8",
              "the key releases the first pulse and its shake, and the effect holds");
        b.pacer.advance_key(200, b.sink());
        b.pacer.advance_key(300, b.sink());
        check(b.fx.mask() == 4 && b.seen.size() == 3, "Q9", "two more keys inside the shake release nothing");
        b.pacer.pump(100 + 936, b.sink());
        b.pacer.pump(100 + 1872, b.sink());
        check(b.fx.mask() == 15, "Q10", "the pulses accumulate on the shakes' clock: 15 after the third");
        b.pacer.pump(100 + 2808, b.sink());
        check(b.fx.mask() == 15 && b.pacer.in_effect() && b.seen.back() == "M:A STRANGE WIND...", "Q11",
              "the text prints over the negative and holds one tick");
        b.pacer.pump(100 + 2863, b.sink());
        check(b.fx.mask() == 0 && b.pacer.awaiting_key() && b.seen.back() == "W", "Q12",
              "then 0x0df8 takes over and the viewport is restored");
        b.pacer.advance_key(5000, b.sink());
        check(b.seen.back() == "W" && b.seen[b.seen.size() - 2] == "M:Thou dost read:" && b.pacer.awaiting_key(), "Q13",
              "one key = one getkey after the effect");
    }
    {
        Bench b(0);
        for (const auto &e : kWellDone) b.emit(e, 0);
        check(!b.pacer.holding() && b.seen.size() == kWellDone.size() && b.fx.mask() == 0, "Q14",
              "unpaced (pause 0): nothing is held, the stream drains and ends normal");
    }
    {
        Bench b(1540, 2);
        for (const auto &e : kWellDone) b.emit(e, 0);
        check(b.seen.size() == kWellDone.size() && b.fx.mask() == 0 && b.pacer.collapsed() == 1,
              "Q15", "a queue too small collapses in order and still ends normal (" + b.joined() + ")");
    }
}
} // namespace

int main() {
    test_model();
    test_pacer();
    std::printf("\nA3-HF7 ritual effect model: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
