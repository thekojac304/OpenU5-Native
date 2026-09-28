#!/usr/bin/env python
"""Alpha 3 A3-HF5 -- mutation validation of the TLK pause pacing.

Each mutation changes ONE production anchor -- the DialoguePacer queue
(native/core/src/dialogue_pacer.cpp, its cadence constant in
dialogue_pacer.h) or its device wiring (alpha_runtime.cpp) -- rebuilds the
host targets below and records which checks turn RED. The file is restored
byte for byte and touched (a restored file is older than the mutated object:
without the touch ninja keeps the mutant). A mutation that leaves every target
GREEN is a SURVIVOR and fails this run; one that does not build is INVALID
(not killed).

Targets: a3_hf5_dialogue_pacing_runtime (the real runtime), a3_hf5_dialogue_pacer
(the queue alone) and a3_hf4_load_transient_runtime (the load cleanup the
pacer joins).

Usage: python native/core/tools/a3_hf5_mutation_check.py <build-dir> <log> [D1,D2,...]
(the optional third argument re-runs only the named mutations)
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
TARGETS = ['a3_hf5_dialogue_pacing_runtime', 'a3_hf5_dialogue_pacer', 'a3_hf4_load_transient_runtime']

PACER = 'native/core/src/dialogue_pacer.cpp'
HDR = 'native/core/include/openu5/dialogue_pacer.h'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
RELEASE_POP = ('        const auto step = storage_.steps[head_];\n        head_ = (head_ + 1) % storage_.step_capacity;\n'
               '        --count_;\n        ++released_;\n        deliver(step, out);\n        if (step.kind == ')
INPUT_TAIL = ('        if(!dialogue_pacer_.holding())drain_pending_npc_initiation();\n'
              '        dirty_=true;dirty_reason_="dialogue-pause";\n        return true;\n    }\n')

MUTATIONS = [
    # --- the defect itself: cadence ---
    ('D1 every line is shown at once (HEAD behaviour)', PACER,
     '        if (pause == DialoguePause::None) return false;\n        ++released_;',
     '        if (pause == DialoguePause::None || pause != DialoguePause::None) return false;\n        ++released_;'),
    ('D2 the device runs unpaced', RT, 'dialogue_pacer_.set_pause_ms(paced?openu5::kTalkPauseMs:0);',
     'dialogue_pacer_.set_pause_ms(0);'),
    ('D3 wrong delay: half a Pause (14 ticks)', HDR,
     'constexpr uint32_t kTalkPauseMs = run_n_frames_ms(kTalkPauseTicks);',
     'constexpr uint32_t kTalkPauseMs = run_n_frames_ms(kTalkPauseTicks / 2);'),
    ('D4 wrong delay: 30 ticks', HDR,
     'constexpr uint32_t kTalkPauseMs = run_n_frames_ms(kTalkPauseTicks);',
     'constexpr uint32_t kTalkPauseMs = run_n_frames_ms(kTalkPauseTicks + 2);'),
    ('D5 a Timed pause never runs out (the queue never advances on its own)', PACER,
     '    if (state_ == DialoguePacerState::Timed && int32_t(now_ms - resume_at_ms_) >= 0) release(now_ms, out);',
     '    if (state_ == DialoguePacerState::Key && int32_t(now_ms - resume_at_ms_) >= 0) release(now_ms, out);'),
    ('D6 a KeyWait times out like a Pause', PACER,
     '    state_ = pause == DialoguePause::Key ? DialoguePacerState::Key : DialoguePacerState::Timed;',
     '    state_ = pause == DialoguePause::Key ? DialoguePacerState::Timed : DialoguePacerState::Timed;'),
    ('D7 the harness contract is broken: unpaced fixtures pace too', RT,
     'dialogue_pacer_.set_pause_ms(paced?openu5::kTalkPauseMs:0);',
     'dialogue_pacer_.set_pause_ms(openu5::kTalkPauseMs);'),
    # --- order and completeness ---
    ('D8 the queue releases newest first', PACER, RELEASE_POP,
     '        const auto step = storage_.steps[(head_ + count_ - 1) % storage_.step_capacity];\n'
     '        --count_;\n        ++released_;\n        deliver(step, out);\n        if (step.kind == '),
    ('D9 one queued line is skipped', PACER,
     '    storage_.steps[(head_ + count_) % storage_.step_capacity] = step;\n    ++count_;',
     '    if (step.kind == DialoguePacerStepKind::Line && step.pause == DialoguePause::None && !count_) return true;\n'
     '    storage_.steps[(head_ + count_) % storage_.step_capacity] = step;\n    ++count_;'),
    ('D10 the paused line itself is swallowed', PACER,
     '        if (out.emit) out.emit(out.context, e);\n        hold(pause, now_ms);',
     '        hold(pause, now_ms);'),
    ('D11 a release does not stop at the next pause', PACER,
     '            hold(step.pause, now_ms);\n            break;', '            (void)now_ms;'),
    ('D12 two paced turns interleave', PACER, '    if (push(e)) return true;\n    collapse(out);',
     '    if (dialogue_event_pause(e) != DialoguePause::None) {\n        if (out.emit) out.emit(out.context, e);\n'
     '        return true;\n    }\n    if (push(e)) return true;\n    collapse(out);'),
    ('D13 a collapse drops the queue instead of releasing it', PACER,
     '        ++released_;\n        deliver(step, out);\n    }\n    head_ = text_used_ = 0;',
     '        ++released_;\n        (void)step;\n        (void)out;\n    }\n    head_ = text_used_ = 0;'),
    ('D14 a borrowed payload is queued (no collapse)', PACER, '        if (borrows_payload(e)) return false;\n',
     '        if (borrows_payload(e) && e.kind == GameEventKind::Moved) return false;\n'),
    ('D15 queued plain-event text is lost', PACER, '        e.text = at;\n        out.emit(out.context, e);',
     '        (void)at;\n        out.emit(out.context, e);'),
    ('D16 the rune flag is lost', PACER, '            step.rune = d->output->rune;\n', ''),
    ('D17 ordinary combat text is paced', PACER,
     '        if (pause == DialoguePause::None) return false;\n        ++released_;',
     '        if (pause == DialoguePause::None && e.kind != GameEventKind::Combat) return false;\n        ++released_;'),
    # --- input ---
    ('D18 a key does not cut a Timed pause', PACER,
     '    if (state_ == DialoguePacerState::Idle) return false;\n    release(now_ms, out);',
     '    if (state_ != DialoguePacerState::Key) return false;\n    release(now_ms, out);'),
    ('D19 the key that ends a pause also reaches the game', RT, INPUT_TAIL,
     '        if(!dialogue_pacer_.holding())drain_pending_npc_initiation();\n'
     '        dirty_=true;dirty_reason_="dialogue-pause";\n    }\n'),
    ('D20 no key-wait cue on the handheld', RT,
     '    if(dialogue_pacer_.awaiting_key()){std::snprintf(text,sizeof(text),"Enter: continue");return text;}\n', ''),
    # --- load / menu / approach ---
    ('D21 a successful load keeps the queued speech', RT, '    dialogue_pacer_.cancel();\n', ''),
    ('D22 a load cancels on failure too (queue dropped by any Alt+L)', RT,
     'bool ok=save_.load(context_,outdoor_,terrain_,actors_,retained_,ms);if(ok){',
     'bool ok=save_.load(context_,outdoor_,terrain_,actors_,retained_,ms);dialogue_pacer_.cancel();if(ok){'),
    ('D23 the pause runs on behind the System Menu', RT,
     '    if(system_menu_.active()){\n        assert(!frontend_.active() && "system menu cannot overlap frontend state");',
     '    if(system_menu_.active()){\n        service_dialogue_pacer();\n'
     '        assert(!frontend_.active() && "system menu cannot overlap frontend state");'),
    ('D24 an NPC approach is drained under the paused speech', RT,
     '    if(dialogue_pacer_.holding())return;\n    const auto kind=pending_npc_initiation_;',
     '    const auto kind=pending_npc_initiation_;'),
    ('D25 the approach is never drained after the speech', RT,
     '        drain_pending_npc_initiation();\n    }\n    return true;\n}', '    }\n    return true;\n}'),
    # --- presentation ---
    ('D26 a release does not mark the frame dirty', RT,
     '    if(service_dialogue_pacer()){dirty_=true;dirty_reason_="dialogue-pause";}',
     '    service_dialogue_pacer();'),
]


def run(cmd):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def build():
    return run([CMAKE, '--build', BUILD, '--target'] + TARGETS)


def test():
    results = []
    for t in TARGETS:
        code, out = run([os.path.join(BUILD, t + '.exe'), RES])
        reds = [l for l in out.splitlines() if l.startswith(('RED', 'FAIL', '[FAIL'))]
        results.append((t, code, reds))
    return results


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-HF5 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
    survivors = invalid = 0
    only = set(sys.argv[3].split(',')) if len(sys.argv) > 3 else None
    selected = [m for m in MUTATIONS if not only or m[0].split()[0] in only]
    for label, rel, old, new in selected:
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        eol = '\r\n' if '\r\n' in text else '\n'
        o, n = old.replace('\n', eol), new.replace('\n', eol)
        count = text.count(o)
        if count != 1:
            lines.append('%s: ANCHOR NOT UNIQUE (%d) in %s' % (label, count, rel))
            invalid += 1
            continue
        try:
            open(path, 'wb').write(text.replace(o, n).encode('utf-8'))
            code, out = build()
            if code != 0:
                lines.append('%s [%s]: BUILD FAILED (INVALID, not killed)' % (label, rel))
                lines += ['    ' + l for l in out.splitlines() if 'error' in l][:4]
                invalid += 1
                continue
            results = test()
        finally:
            open(path, 'wb').write(original)
            os.utime(path, None)
        killed = any(code != 0 for _, code, _ in results)
        survivors += 0 if killed else 1
        lines.append('%s [%s]: %s' % (label, rel, 'KILLED' if killed else 'SURVIVED'))
        for t, code, reds in results:
            if code or reds:
                lines.append('    %s exit=%d  %d RED' % (t, code, len(reds)))
                lines += ['        ' + r[:200] for r in reds[:12]]
    bcode, _ = build()
    restored = test()
    lines.append('')
    lines.append('restored: build exit=%d; %s' % (bcode, ', '.join('%s exit=%d' % (t, c) for t, c, _ in restored)))
    lines.append('mutations: %d, killed: %d, survived: %d, invalid: %d' %
                 (len(selected), len(selected) - survivors - invalid, survivors, invalid))
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    sys.exit(1 if survivors or invalid or bcode or any(c for _, c, _ in restored) else 0)


if __name__ == '__main__':
    main()
