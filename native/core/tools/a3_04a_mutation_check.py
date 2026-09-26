#!/usr/bin/env python
"""Alpha 3 A3-04A -- mutation validation of the real-time audio path.

Each mutation edits production code (one or more anchored replacements),
rebuilds the A3-04A test target plus the synth target and runs both; the files
are then restored byte for byte and touched (a restored file is older than the
mutated object: without the touch ninja keeps the mutant). A mutation that
leaves every check GREEN is a SURVIVOR and fails this run.

L1 reproduces A3-04's own device sequencing (enable first, then write; a
two-block drain) inside the pump, so its RED lines are the host-side RED proof
of the sequencing defects section 18.8 fixes.

Usage: python native/core/tools/a3_04a_mutation_check.py <build-dir> <log> [ID,ID,...]
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
CORE = os.path.join(ROOT, 'native', 'core')
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
ONLY = set(sys.argv[3].split(',')) if len(sys.argv) > 3 else None
AUDIO_PACK = os.path.join(ROOT, 'native/assets/openu5-audio.bin')

TESTS = {
    'stream': [os.path.join(BUILD, 'a3_04a_audio_stream.exe'), CORE, AUDIO_PACK],
    'synth': [os.path.join(BUILD, 'a3_04_music_synth.exe'), CORE, AUDIO_PACK],
}
TARGETS = ['a3_04a_audio_stream', 'a3_04_music_synth']

STREAM = 'native/core/src/audio_stream.cpp'
STREAM_H = 'native/core/include/openu5/audio_stream.h'
SYN = 'native/core/src/music_synth.cpp'
FW_CMAKE = 'native/targets/tdeck/main/CMakeLists.txt'
DEV = 'native/targets/tdeck/main/tdeck_audio.cpp'

# (id, label, [(file, old, new), ...])
MUTATIONS = [
    ('M1', 'render-ahead disabled: the channel starts after ONE preloaded block', [
        (STREAM, '        if (loaded && primed_ < ring_blocks_) return false;',
         '        if (loaded && primed_ < 1) return false;')]),
    ('M2', 'ring buffer too shallow: 3 descriptors (24 ms) instead of 8', [
        (STREAM_H, 'constexpr uint32_t kAudioRingBlocks = 8;', 'constexpr uint32_t kAudioRingBlocks = 3;')]),
    ('M3', 'producer fails to refill: every 50th block is rendered and never written', [
        (STREAM, '    const bool ok = sink.write(block_);',
         '    const bool ok = written_ % 50 == 49 ? false : sink.write(block_);')]),
    ('M4', 'consumer skips a block: two blocks rendered per write', [
        (STREAM, '    render_block(sfx_gain_q15, music_gain_q15);\n    silent_run_ = work ? 0 : silent_run_ + 1;\n    // Blocks not yet finished',
         '    render_block(sfx_gain_q15, music_gain_q15);\n    render_block(sfx_gain_q15, music_gain_q15);\n    silent_run_ = work ? 0 : silent_run_ + 1;\n    // Blocks not yet finished')]),
    ('M5', 'SFX overwrite the music buffer instead of summing with it', [
        (STREAM, '        const int32_t sum = int32_t(sfx_block_[i]) + int32_t(music_block_[i]);',
         '        const int32_t sum = sfx_block_[i] != 0 ? int32_t(sfx_block_[i]) : int32_t(music_block_[i]);')]),
    ('M6', 'a music switch keeps the old track\'s buffered chip samples', [
        (SYN, '    active_ = true;\n    have_ = 0;', '    active_ = true;')]),
    ('M7', 'a music switch is ignored while a song plays (the old track keeps going)', [
        (STREAM, '    if (song == song_ && music_.active()) return; // AudioService already de-dupes; be sure anyway',
         '    if (music_.active()) return; // AudioService already de-dupes; be sure anyway')]),
    ('M8', 'a volume update is lost: any non-zero Music Volume renders at unity', [
        (STREAM, '    if (music_.active()) music_.render(music_block_, kAudioBlockFrames, kSfxOutputRateHz, music_gain_q15);',
         '    if (music_.active()) music_.render(music_block_, kAudioBlockFrames, kSfxOutputRateHz, uint16_t(music_gain_q15 ? kUnityGainQ15 : 0));')]),
    ('M9', 'volume cross-wired: the SFX gain is applied to the music', [
        (STREAM, '    render_block(sfx_gain_q15, music_gain_q15);\n    silent_run_ = work ? 0 : silent_run_ + 1;\n    // Blocks not yet finished',
         '    render_block(sfx_gain_q15, sfx_gain_q15);\n    silent_run_ = work ? 0 : silent_run_ + 1;\n    // Blocks not yet finished')]),
    ('M10', 'a stall underruns undetected by the writer (dry-ring check removed)', [
        (STREAM, '    if (fill == 0) perf_.on_underrun();', '')]),
    ('M11', 'a stall underruns undetected by the driver counter (overflows dropped)', [
        (STREAM, '        perf_.on_hw_underruns(overflows);', '')]),
    ('M12', 'after an underrun the fill count is never re-based (played overtakes written)', [
        (STREAM, '    if (played > written_) {\n        played_base_ += played - written_;\n        played = written_;\n    }', '')]),
    ('M13', 'the runaway guard never yields', [
        (STREAM, '        if (++nonblocking_run_ >= kRunawayWrites) {', '        if (++nonblocking_run_ >= 0xffffffffu) {')]),
    ('M14', 'the drain is two blocks deep (A3-04\'s), not a whole ring', [
        (STREAM, '    if (!work && silent_run_ >= ring_blocks_) {', '    if (!work && silent_run_ >= 2) {')]),
    ('L1', 'A3-04\'s device sequencing: enable first (no preload) and a two-block drain', [
        (STREAM, '        const bool loaded = sink.preload(block_);', '        const bool loaded = false;'),
        (STREAM, '        else perf_.on_write_failure(); // cannot happen with the driver: counted, never hidden',
         '        else {}'),
        (STREAM, '    if (!work && silent_run_ >= ring_blocks_) {', '    if (!work && silent_run_ >= 2) {')]),
    ('M15', 'the synth\'s tables go back behind a function-local static (A3-04\'s per-sample mutex)', [
        (SYN, 'const OplTables kTables;\nconst OplTables &tables() { return kTables; }',
         'const OplTables &tables() {\n    static const OplTables t;\n    return t;\n}')]),
    ('M16', 'the firmware builds the synth at the project-wide -Og again', [
        (FW_CMAKE, '    PROPERTIES COMPILE_OPTIONS "-O2;-ffp-contract=off")', '    PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")')]),
    ('M17', 'the driver\'s overflow callback is never registered on the device', [
        (DEV, '    callbacks.on_send_q_ovf = &TdeckAudioBackend::on_send_q_ovf;\n', '')]),
    ('M18', 'the chip-rate buffer regrows mid-stream (an allocation on the audio task)', [
        (SYN, '    ensure_capacity(needed);', '    ensure_capacity(needed + (read_pos_ > 0.5 ? 1 : 0));')]),
    ('M19', 'the benchmark cue cadence drifts with the frame rate', [
        (STREAM, '            next_sfx_ms_ += kSfxEveryMs; // a fixed cadence, not "100 ms after whenever the frame came"',
         '            next_sfx_ms_ = now_ms + kSfxEveryMs; // a fixed cadence, not "100 ms after whenever the frame came"')]),
    ('M20', 'a render exactly one block long counts as a missed deadline', [
        (STREAM, '    if (render_us > kAudioBlockUs) ++missed_;', '    if (render_us >= kAudioBlockUs) ++missed_;')]),
    ('M21', 'the percentile reports the bucket\'s lower edge', [
        (STREAM, 'uint32_t((b + 1) * kAudioBlockUs / kBucketsPerBlock)', 'uint32_t(b * kAudioBlockUs / kBucketsPerBlock)')]),
]


def run(cmd, cwd):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def build():
    return run([CMAKE, '--build', BUILD, '--target'] + TARGETS, ROOT)


def run_tests():
    results = {}
    for name, cmd in TESTS.items():
        code, out = run(cmd, ROOT)
        reds = [l for l in out.splitlines() if l.startswith('RED')]
        results[name] = (code, reds)
    return results


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-04A mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
    survivors = 0
    ran = 0
    for mid, label, edits in MUTATIONS:
        if ONLY and mid not in ONLY:
            continue
        ran += 1
        originals = {}
        ok = True
        try:
            for rel, old, new in edits:
                path = os.path.join(ROOT, rel)
                if path not in originals:
                    originals[path] = open(path, 'rb').read()
                text = open(path, 'rb').read().decode('utf-8')
                eol = '\r\n' if '\r\n' in text else '\n'
                o, n = old.replace('\n', eol), new.replace('\n', eol)
                if text.count(o) != 1:
                    lines.append('%s %s: ANCHOR NOT UNIQUE (%d) in %s' % (mid, label, text.count(o), rel))
                    ok = False
                    break
                open(path, 'wb').write(text.replace(o, n).encode('utf-8'))
            if not ok:
                survivors += 1
                continue
            code, out = build()
            if code != 0:
                lines.append('%s %s: BUILD FAILED (counts as killed)' % (mid, label))
                lines += ['    ' + l for l in out.splitlines() if 'error' in l][:4]
                continue
            results = run_tests()
        finally:
            for path, data in originals.items():
                open(path, 'wb').write(data)
                os.utime(path, None)
        killed = any(code != 0 for code, _ in results.values())
        survivors += 0 if killed else 1
        lines.append('%s %s: %s' % (mid, label, 'KILLED' if killed else 'SURVIVED'))
        for name, (code, reds) in results.items():
            if code != 0:
                lines.append('    %-8s exit=%d  %d RED' % (name, code, len(reds)))
                lines += ['        ' + r for r in reds[:8]]
    code, _ = build()
    restored = run_tests()
    lines.append('')
    lines.append('restored build exit=%d; ' % code + ', '.join('%s exit=%d' % (k, v[0]) for k, v in restored.items()))
    lines.append('mutations: %d, killed: %d, survived: %d' % (ran, ran - survivors, survivors))
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines[-3:]))
    sys.exit(1 if survivors or code or any(v[0] for v in restored.values()) else 0)


if __name__ == '__main__':
    main()
