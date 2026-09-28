// Alpha 3 A3-HF5 -- DialoguePacer, the TLK Pause/KeyWait queue, in isolation.
//
// The runtime test (a3_hf5_dialogue_pacing_runtime) drives the real game; this
// one pins the queue's own contract on hand-built events: what it takes, what
// it never takes, order, copies, overflow, cancellation and the cadence.
#include "openu5/combat.h"
#include "openu5/dialogue_pacer.h"

#include <cstdio>
#include <cstring>
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

// What the sink saw, flattened to strings: "L:<text>|<pause>", "P:<question>",
// "E:<kind>/<value>", "M:<text>", "END", "H:<handoff>", "G:<kind>:<text>".
struct Sink {
    std::vector<std::string> seen;
    EventSink sink() {
        return {this, [](void *p, const GameEvent &e) {
                    auto &s = *static_cast<Sink *>(p);
                    if (e.kind == GameEventKind::Dialogue && e.dialogue) {
                        const auto &d = *e.dialogue;
                        if (d.kind == DialogueEventKind::Output && d.output) {
                            const auto &o = *d.output;
                            if (o.kind == DialogueOutputKind::Line)
                                s.seen.push_back("L:" + std::string(o.text.begin(), o.text.end()) + "|" +
                                                 std::to_string(int(o.pause)) + (o.rune ? "R" : "") + "@" +
                                                 std::to_string(d.location) + "/" + std::to_string(d.slot));
                            else if (o.kind == DialogueOutputKind::Prompt)
                                s.seen.push_back("P:" + std::to_string(o.question));
                            else
                                s.seen.push_back("E:" + std::to_string(int(o.effect.kind)) + "/" +
                                                 std::to_string(o.effect.value));
                        } else if (d.kind == DialogueEventKind::EffectMessage)
                            s.seen.push_back("M:" + std::string(d.message.begin(), d.message.end()));
                        else if (d.kind == DialogueEventKind::Ended)
                            s.seen.push_back("END");
                        else
                            s.seen.push_back("H:" + std::to_string(int(d.handoff)));
                    } else
                        s.seen.push_back("G:" + std::to_string(int(e.kind)) + ":" + (e.text ? e.text : "(null)"));
                }};
    }
};

// A dialogue event the way dialogue_orchestration.cpp emits one: the payload
// is borrowed for the call only.
struct Emitted {
    DialogueOutput o;
    DialogueEvent d;
    GameEvent e;
};
Emitted line(const char16_t *text, DialoguePause pause = DialoguePause::None, bool rune = false) {
    Emitted x;
    x.o.kind = DialogueOutputKind::Line;
    x.o.text = text;
    x.o.pause = pause;
    x.o.rune = rune;
    x.d.kind = DialogueEventKind::Output;
    x.d.location = 17;
    x.d.slot = 9;
    x.e.kind = GameEventKind::Dialogue;
    return x;
}
Emitted prompt(bool question) {
    Emitted x;
    x.o.kind = DialogueOutputKind::Prompt;
    x.o.question = question;
    x.d.kind = DialogueEventKind::Output;
    x.e.kind = GameEventKind::Dialogue;
    return x;
}
Emitted effect(DialogueEffectKind kind, int32_t value) {
    Emitted x;
    x.o.kind = DialogueOutputKind::Effect;
    x.o.effect = {kind, value};
    x.d.kind = DialogueEventKind::Output;
    x.e.kind = GameEventKind::Dialogue;
    return x;
}
Emitted other(DialogueEventKind kind, const char16_t *message = u"", DialogueHandoff handoff = DialogueHandoff::None) {
    Emitted x;
    x.d.kind = kind;
    x.d.message = message;
    x.d.handoff = handoff;
    x.e.kind = GameEventKind::Dialogue;
    return x;
}
bool offer(DialoguePacer &p, Emitted x, uint32_t now, Sink &s) {
    x.d.output = &x.o;
    x.e.dialogue = &x.d;
    return p.offer(x.e, now, s.sink());
}
GameEvent message(const char *text) {
    GameEvent e;
    e.kind = GameEventKind::Message;
    e.text = text;
    return e;
}

struct Storage {
    std::vector<DialoguePacerStep> steps;
    std::vector<char> text;
    Storage(size_t n = 64, size_t bytes = 4096) : steps(n), text(bytes) {}
    DialoguePacerStorage view() { return {steps.data(), steps.size(), text.data(), text.size()}; }
};
DialoguePacer paced(Storage &st) {
    DialoguePacer p;
    p.attach(st.view());
    p.set_pause_ms(kTalkPauseMs);
    return p;
}
std::string join(const std::vector<std::string> &v) {
    std::string s;
    for (const auto &x : v) s += (s.empty() ? "" : " ; ") + x;
    return s;
}
} // namespace

int main() {
    check(kTalkPauseTicks == 28 && kTalkPauseMs == 1540 && kTalkPauseMs == run_n_frames_ms(28), "P0",
          "the Pause is run_n_frames(28): TALK 0x0fae cmp si,0x1c, 55 ms ticks -> " + std::to_string(kTalkPauseMs) + " ms");

    {   // P1 -- what an idle pacer never takes.
        Storage st;
        auto p = paced(st);
        Sink s;
        const bool plain = offer(p, line(u"This one."), 0, s);
        const auto msg = message("Funny, no response!");
        const bool text = p.offer(msg, 0, s.sink());
        CombatEvent ce{};
        ce.text = "Missed!";
        GameEvent combat;
        combat.kind = GameEventKind::Combat;
        combat.combat = &ce;
        const bool fight = p.offer(combat, 0, s.sink());
        const bool pr = offer(p, prompt(false), 0, s);
        check(!plain && !text && !fight && !pr && s.seen.empty() && !p.holding(), "P1",
              "idle: an unpaused line, a message, combat text and a prompt all pass straight through (the caller forwards)");
    }
    {   // P2-P4 -- a Timed pause parks the rest of the turn, in order.
        Storage st;
        auto p = paced(st);
        Sink s;
        std::u16string verse = u"Ho eyo he hum! ";
        const bool first = offer(p, line(verse.c_str()), 1000, s); // unpaused: caller forwards
        const bool gap = offer(p, line(u"", DialoguePause::Timed), 1000, s);
        const auto held = s.seen;
        offer(p, line(u"Bounce! "), 1000, s);
        offer(p, line(u"", DialoguePause::Timed), 1000, s);
        offer(p, line(u"Didst thou enjoy that?"), 1000, s);
        offer(p, prompt(true), 1000, s);
        const auto m = message("Something was stolen!");
        const bool queued_msg = p.offer(m, 1000, s.sink());
        check(!first && gap && held.size() == 1 && held[0] == "L:|2@17/9" && p.state() == DialoguePacerState::Timed &&
                  p.resume_at_ms() == 1000 + kTalkPauseMs && queued_msg && p.queued() == 5,
              "P2", "the paused line itself goes out at once, the pause starts there, and everything after it queues (" +
                        join(held) + ", queued " + std::to_string(p.queued()) + ")");
        p.pump(1000 + kTalkPauseMs - 1, s.sink());
        const auto early = s.seen.size();
        p.pump(1000 + kTalkPauseMs, s.sink());
        check(early == 1 && s.seen.size() == 3 && s.seen[1] == "L:Bounce! |0@17/9" && s.seen[2] == "L:|2@17/9" &&
                  p.state() == DialoguePacerState::Timed && p.resume_at_ms() == 1000 + 2 * kTalkPauseMs,
              "P3", "not a millisecond early; at 28 ticks it releases up to the next paused line and re-arms from there (" +
                        join(s.seen) + ")");
        p.pump(1000 + 2 * kTalkPauseMs, s.sink());
        check(join(s.seen) == "L:|2@17/9 ; L:Bounce! |0@17/9 ; L:|2@17/9 ; L:Didst thou enjoy that?|0@17/9 ; P:1 ; "
                              "G:" + std::to_string(int(GameEventKind::Message)) + ":Something was stolen!" &&
                  !p.holding() && p.queued() == 0,
              "P4", "the tail comes out whole and in order, the prompt and the trailing message behind the last line");
    }
    {   // P5 -- a KeyWait never times out; a key ends either kind.
        Storage st;
        auto p = paced(st);
        Sink s;
        offer(p, line(u"Welcome. ", DialoguePause::Key), 0, s);
        offer(p, line(u"To the castle! ", DialoguePause::Key), 0, s);
        offer(p, line(u"That's ME!"), 0, s);
        p.pump(600000, s.sink());
        const auto after_ten_minutes = s.seen.size();
        const bool k1 = p.advance_key(600001, s.sink());
        const auto one = s.seen.size();
        const bool still = p.awaiting_key();
        p.advance_key(600002, s.sink());
        const bool idle_key = p.advance_key(600003, s.sink());
        check(after_ten_minutes == 1 && k1 && one == 2 && still && s.seen.size() == 3 && !p.holding() && !idle_key,
              "P5", "a KeyWait holds through ten minutes of clock; each key releases one section; an idle pacer "
                    "refuses a key (" + join(s.seen) + ")");
    }
    {   // P6 -- an event it cannot own collapses the queue, in order, first.
        Storage st;
        auto p = paced(st);
        Sink s;
        offer(p, line(u"I beg to differ! ", DialoguePause::Timed), 0, s);
        offer(p, line(u"So very kind! "), 0, s);
        CombatEvent ce{};
        GameEvent combat;
        combat.kind = GameEventKind::CombatStarted;
        combat.combat = &ce;
        const bool taken = p.offer(combat, 10, s.sink());
        check(!taken && !p.holding() && p.collapsed() == 1 && s.seen.size() == 2 && s.seen[1] == "L:So very kind! |0@17/9",
              "P6", "a borrowed payload it cannot copy releases everything queued first and is handed back to the caller");
    }
    {   // P7 -- storage limits collapse, never drop.
        Storage st(3, 4096);
        auto p = paced(st);
        Sink s;
        offer(p, line(u"a", DialoguePause::Timed), 0, s);
        for (auto t : {u"b", u"c", u"d"}) offer(p, line(t), 0, s);
        const bool fourth = offer(p, line(u"e"), 0, s);
        Storage tiny(64, 8);
        auto q = paced(tiny);
        Sink s2;
        offer(q, line(u"a", DialoguePause::Timed), 0, s2);
        const bool big = offer(q, line(u"far too long for eight bytes"), 0, s2);
        check(!fourth && s.seen.size() == 4 && s.seen[3] == "L:d|0@17/9" && p.collapsed() == 1 && !big &&
                  q.collapsed() == 1 && !q.holding(),
              "P7", "a full step ring or text arena releases the queue in order and hands the overflowing event back");
    }
    {   // P8 -- cancel drops everything and returns to idle.
        Storage st;
        auto p = paced(st);
        Sink s;
        offer(p, line(u"one", DialoguePause::Timed), 0, s);
        offer(p, line(u"two"), 0, s);
        p.cancel();
        p.pump(100000, s.sink());
        const bool next = offer(p, line(u"fresh"), 100000, s);
        check(s.seen.size() == 1 && !p.holding() && p.queued() == 0 && !next, "P8",
              "cancel(): the queued tail is never shown and the pacer is idle again");
    }
    {   // P9 -- zero cadence = synchronous (harness / automation contract).
        Storage st;
        DialoguePacer p;
        p.attach(st.view());
        Sink s;
        const bool took = offer(p, line(u"x", DialoguePause::Timed), 0, s) || offer(p, line(u"y", DialoguePause::Key), 0, s);
        check(!took && !p.holding() && s.seen.empty(), "P9", "with a 0 ms cadence nothing is ever taken");
    }
    {   // P10 -- copies, not borrows: the source may die right after offer().
        Storage st;
        auto p = paced(st);
        Sink s;
        offer(p, line(u"first", DialoguePause::Timed), 0, s);
        std::u16string text = u"survives";
        offer(p, line(text.c_str(), DialoguePause::None, true), 0, s);
        text.assign(u"CLOBBERED");
        char buffer[16];
        std::snprintf(buffer, sizeof buffer, "%s", "kept");
        p.offer(message(buffer), 0, s.sink());
        std::snprintf(buffer, sizeof buffer, "%s", "gone");
        offer(p, effect(DialogueEffectKind::Gold, 100), 0, s);
        offer(p, other(DialogueEventKind::EffectMessage, u"Thou hast 100 gold!"), 0, s);
        offer(p, other(DialogueEventKind::Handoff, u"", DialogueHandoff::QuestEnd), 0, s);
        offer(p, other(DialogueEventKind::Ended), 0, s);
        GameEvent sfx;
        sfx.kind = GameEventKind::Sfx;
        sfx.text = "alarm";
        sfx.note = 3;
        p.offer(sfx, 0, s.sink());
        p.pump(kTalkPauseMs, s.sink());
        const std::string want = "L:first|2@17/9 ; L:survives|0R@17/9 ; G:" + std::to_string(int(GameEventKind::Message)) +
                                 ":kept ; E:" + std::to_string(int(DialogueEffectKind::Gold)) + "/100 ; M:Thou hast 100 "
                                 "gold! ; H:" + std::to_string(int(DialogueHandoff::QuestEnd)) + " ; END ; G:" +
                                 std::to_string(int(GameEventKind::Sfx)) + ":alarm";
        check(join(s.seen) == want, "P10",
              "every queued kind is a copy: text, rune flag, effect, effect message, handoff, end, a plain event (" +
                  join(s.seen) + ")");
    }
    {   // P11 -- two conversations never interleave.
        Storage st;
        auto p = paced(st);
        Sink s;
        offer(p, line(u"A1", DialoguePause::Timed), 0, s);
        offer(p, line(u"A2"), 0, s);
        offer(p, other(DialogueEventKind::Ended), 0, s);
        offer(p, line(u"B1", DialoguePause::Timed), 0, s); // a second talk queued behind the first
        offer(p, line(u"B2"), 0, s);
        p.pump(kTalkPauseMs, s.sink());
        const auto mid = join(s.seen);
        p.pump(2 * kTalkPauseMs, s.sink());
        check(mid == "L:A1|2@17/9 ; L:A2|0@17/9 ; END ; L:B1|2@17/9" &&
                  join(s.seen) == mid + " ; L:B2|0@17/9" && !p.holding(),
              "P11", "a second paced turn waits behind the first and keeps its own pause");
    }
    {   // P12 -- the arena is reclaimed: long play never runs out.
        Storage st(64, 256);
        auto p = paced(st);
        Sink s;
        bool collapsed = false;
        uint32_t t = 0;
        for (int round = 0; round < 200; ++round) {
            offer(p, line(u"verse", DialoguePause::Timed), t, s);
            offer(p, line(u"a line of about forty characters, twice"), t, s);
            offer(p, line(u"a line of about forty characters, again"), t, s);
            t += kTalkPauseMs;
            p.pump(t, s.sink());
            collapsed |= p.collapsed() != 0;
        }
        check(!collapsed && s.seen.size() == 600, "P12", "200 paced turns through a 256-byte arena: never a collapse");
    }
    {   // P13 -- dialogue_event_pause.
        auto a = line(u"x", DialoguePause::Key);
        a.d.output = &a.o;
        a.e.dialogue = &a.d;
        auto b = prompt(true);
        b.d.output = &b.o;
        b.e.dialogue = &b.d;
        check(dialogue_event_pause(a.e) == DialoguePause::Key && dialogue_event_pause(b.e) == DialoguePause::None &&
                  dialogue_event_pause(message("x")) == DialoguePause::None,
              "P13", "only a Line output carries a pause");
    }
    std::printf("\nA3-HF5 dialogue pacer: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
