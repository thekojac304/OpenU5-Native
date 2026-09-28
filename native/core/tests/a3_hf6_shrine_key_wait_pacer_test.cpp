// Alpha 3 A3-HF6 (H-183) -- the ShrineKeyWait marker in DialoguePacer, in
// isolation. The runtime test (a3_hf6_shrine_key_wait_runtime) drives the real
// rite and Codex; this one pins the queue's contract for the bare marker on
// hand-built events: it is a Key pause when idle and when queued, it is
// delivered in its place, one key releases exactly one section, the unpaced
// cadence never takes it, and a TLK line's pause is unchanged.
#include "openu5/dialogue_pacer.h"

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

// "M:<text>" for a Message, "W" for the marker, "Q" for a Quake, "L:<text>" for
// a TLK line.
struct Sink {
    std::vector<std::string> seen;
    EventSink sink() {
        return {this, [](void *p, const GameEvent &e) {
                    auto &s = *static_cast<Sink *>(p);
                    if (e.kind == GameEventKind::ShrineKeyWait) s.seen.push_back("W");
                    else if (e.kind == GameEventKind::Quake) s.seen.push_back("Q");
                    else if (e.kind == GameEventKind::Dialogue && e.dialogue && e.dialogue->output)
                        s.seen.push_back("L:" + std::string(e.dialogue->output->text.begin(), e.dialogue->output->text.end()));
                    else s.seen.push_back("M:" + std::string(e.text ? e.text : "(null)"));
                }};
    }
    std::string joined() const {
        std::string out;
        for (const auto &x : seen) out += x + ",";
        return out;
    }
};

GameEvent message(const char *text) {
    GameEvent e{};
    e.kind = GameEventKind::Message;
    e.text = text;
    return e;
}
GameEvent marker() {
    GameEvent e{};
    e.kind = GameEventKind::ShrineKeyWait;
    return e;
}
GameEvent quake() {
    GameEvent e{};
    e.kind = GameEventKind::Quake;
    return e;
}

struct Bench {
    DialoguePacerStep steps[16]{};
    char text[512]{};
    DialoguePacer pacer;
    Sink out;
    explicit Bench(uint32_t pause_ms = kTalkPauseMs) {
        pacer.attach({steps, 16, text, sizeof(text)});
        pacer.set_pause_ms(pause_ms);
    }
    // The core's delivery: every event offered; the ones not taken are
    // forwarded by the caller, as AlphaRuntime::consume_event does.
    void emit(const GameEvent &e, uint32_t now = 0) {
        if (!pacer.offer(e, now, out.sink())) out.sink().emit(&out, e);
    }
};

void test() {
    {
        Bench b;
        b.emit(message("ordained"));
        b.emit(marker());
        b.emit(message("page"));
        b.emit(marker());
        b.emit(message("return"));
        check(b.out.joined() == "M:ordained,W," && b.pacer.awaiting_key(), "U1",
              "idle: the text before the marker passes, the marker is delivered in its place and holds for a key (" +
                  b.out.joined() + ")");
        b.pacer.pump(1000000, b.out.sink());
        check(b.pacer.awaiting_key() && b.out.seen.size() == 2, "U2", "no clock ends it (pumped a thousand seconds on)");
        b.pacer.advance_key(1000000, b.out.sink());
        check(b.out.joined() == "M:ordained,W,M:page,W," && b.pacer.awaiting_key(), "U3",
              "one key releases exactly one section and stops at the queued marker (" + b.out.joined() + ")");
        b.pacer.advance_key(1000000, b.out.sink());
        check(b.out.joined() == "M:ordained,W,M:page,W,M:return," && !b.pacer.holding() && b.pacer.queued() == 0, "U4",
              "the next key releases the rest and the pacer is idle");
        check(!b.pacer.advance_key(1000000, b.out.sink()), "U5", "a key with nothing held is not the pacer's");
    }
    {
        // The Codex's last getkey (0x0e5b) has nothing after it: still a wait.
        Bench b;
        b.emit(message("last page"));
        b.emit(marker());
        check(b.pacer.awaiting_key() && b.pacer.queued() == 0, "U6", "a trailing marker holds with an empty queue");
        b.pacer.advance_key(0, b.out.sink());
        check(!b.pacer.holding(), "U7", "and one key ends it");
    }
    {
        // A side effect between two markers keeps its place (the ceremony's quakes).
        Bench b;
        b.emit(marker());
        b.emit(message("page"));
        b.emit(marker());
        b.emit(quake());
        b.emit(message("wind"));
        b.pacer.advance_key(0, b.out.sink());
        check(b.out.joined() == "W,M:page,W,", "U8", "the quake after the second marker waits with its section");
        b.pacer.advance_key(0, b.out.sink());
        check(b.out.joined() == "W,M:page,W,Q,M:wind,", "U9", "and lands, in order, with the key that ends it");
    }
    {
        Bench b(0);
        b.emit(message("a"));
        b.emit(marker());
        b.emit(message("b"));
        check(b.out.joined() == "M:a,W,M:b," && !b.pacer.holding(), "U10",
              "the unpaced cadence (the reference's automation rule) takes no marker");
    }
    {
        // A TLK Timed pause holding the turn: a marker queued behind it turns
        // into a key wait when the pause ends, the TLK pause itself unchanged.
        Bench b;
        DialogueOutput o;
        o.kind = DialogueOutputKind::Line;
        o.text = u"verse";
        o.pause = DialoguePause::Timed;
        DialogueEvent d;
        d.kind = DialogueEventKind::Output;
        d.output = &o;
        GameEvent tlk{};
        tlk.kind = GameEventKind::Dialogue;
        tlk.dialogue = &d;
        b.emit(tlk, 0);
        b.emit(marker(), 0);
        b.emit(message("after"), 0);
        check(b.pacer.state() == DialoguePacerState::Timed, "U11", "a TLK Pause still holds on its clock");
        b.pacer.pump(kTalkPauseMs, b.out.sink());
        check(b.out.joined() == "L:verse,W," && b.pacer.awaiting_key(), "U12",
              "at its end the queued marker is released and holds for a key (" + b.out.joined() + ")");
        b.pacer.advance_key(kTalkPauseMs, b.out.sink());
        check(b.out.joined() == "L:verse,W,M:after," && !b.pacer.holding(), "U13", "one key releases the rest");
    }
    {
        Bench b;
        b.emit(marker());
        b.emit(message("never"));
        b.pacer.cancel();
        check(!b.pacer.holding() && b.pacer.queued() == 0 && b.out.joined() == "W,", "U14",
              "cancel (a successful load) drops the wait and its section");
    }
    check(paced_event_pause(marker()) == DialoguePause::Key && paced_event_pause(message("x")) == DialoguePause::None &&
              dialogue_event_pause(marker()) == DialoguePause::None,
          "U15", "only the marker is a Key pause; dialogue_event_pause (TLK lines only) is unchanged");
}
} // namespace

int main() {
    test();
    std::printf("\nA3-HF6 shrine key-wait pacer: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
