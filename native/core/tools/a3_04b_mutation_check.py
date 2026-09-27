#!/usr/bin/env python
"""Alpha 3 A3-04B -- mutation validation (ALPHA3_AUDIO.md section 19.13).

Each mutation edits production code or build configuration (anchored
replacements), rebuilds the A3-04B targets plus the A3-04A stream and A3-04
synth targets, and runs all four; the files are then restored byte for byte
and touched (a restored file is older than the mutated object: without the
touch ninja keeps the mutant). A mutation that leaves every check GREEN is a
SURVIVOR; one that does not compile is INVALID. Either fails this run: every
kill must be a failing check.

D1 re-creates A3-04A's reporting (the report lines appended to the gameplay
transcript, nothing on the Developer screen): its RED lines are the host-side
RED proof of the invisible-diagnostic defect section 19.2 fixes.

Usage: python native/core/tools/a3_04b_mutation_check.py <build-dir> <log> [ID,ID,...]
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
RESOURCES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')

TESTS = {
    'perf': [os.path.join(BUILD, 'a3_04b_perf.exe'), CORE, AUDIO_PACK],
    'runtime': [os.path.join(BUILD, 'a3_04b_perf_runtime.exe'), RESOURCES],
    'stream': [os.path.join(BUILD, 'a3_04a_audio_stream.exe'), CORE, AUDIO_PACK],
    'synth': [os.path.join(BUILD, 'a3_04_music_synth.exe'), CORE, AUDIO_PACK],
}
TARGETS = ['a3_04b_perf', 'a3_04b_perf_runtime', 'a3_04a_audio_stream', 'a3_04_music_synth']

SYN = 'native/core/src/music_synth.cpp'
SYN_H = 'native/core/include/openu5/music_synth.h'
STREAM = 'native/core/src/audio_stream.cpp'
STREAM_H = 'native/core/include/openu5/audio_stream.h'
AUDIO = 'native/core/src/audio.cpp'
PERF = 'native/core/src/perf_report.cpp'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
LF = 'native/targets/tdeck/main/audio_iram.lf'
FW_CMAKE = 'native/targets/tdeck/main/CMakeLists.txt'
DEFAULTS = 'native/targets/tdeck/sdkconfig.defaults'

# (id, label, [(file, old, new), ...])
MUTATIONS = [
    # ---- the synth's cached values and fast paths (bit-exactness) ----
    ('S1', 'the envelope cache is not refreshed after an attack/decay rate write (0x60)', [
        (SYN, '        o->attack_rate = v >> 4;\n        o->decay_rate = v & 0x0f;\n        refresh_channel(channels_[chBase + m.channel]);',
         '        o->attack_rate = v >> 4;\n        o->decay_rate = v & 0x0f;')]),
    ('S2', 'the envelope cache is not refreshed when the envelope changes state', [
        (SYN, '    if (eg_state != state) refresh_envelope(key_code); // the next rate applies from the next sample, as before',
         '    (void)state;\n    (void)key_code;')]),
    ('S3', 'a vibrato step does not refresh the vibrato operators\' phase step', [
        (SYN, '        refresh_vibrato(); // A3-04B: once per ~1,019 chip samples, before this sample\'s phase steps', '')]),
    ('S4', 'a vibrato-depth write (0xBD) does not refresh the phase steps', [
        (SYN, '        refresh_vibrato(); // A3-04B: the depth scales every vibrato operator\'s phase step', '')]),
    ('S5', 'the silent floor one below the exp ROM\'s real limit (3071)', [
        (SYN_H, '    static constexpr int32_t kSilentFloor = 12 << 8;', '    static constexpr int32_t kSilentFloor = (12 << 8) - 1;')]),
    ('S6', 'total level / key scaling not refreshed on a 0x40 write', [
        (SYN, '        o.update_ksl(ch.fnum, ch.block);\n        refresh_channel(ch);', '        o.update_ksl(ch.fnum, ch.block);')]),
    ('S7', 'the mono shortcut taken for the OPL3 chip too (its stereo bus mixed as left only)', [
        (SYN, '    mono_ = chip == OplChipKind::Opl2;', '    mono_ = true;')]),
    ('S8', 'the integer rounding rounds half down', [
        (SYN, '    const int32_t magnitude = int32_t((product + (uint64_t(1) << (shift - 1))) >> shift);',
         '    const int32_t magnitude = int32_t((product + (uint64_t(1) << (shift - 1)) - 1) >> shift);')]),
    ('S9', 'the exact ceil returns the floor', [
        (SYN, '    return double(t) < x ? t + 1 : t;', '    return t;')]),
    ('S10', 'the phase step truncated to 16 bits', [
        (SYN, '    return ((uint32_t(fnum) << ch.block) * uint32_t(tables().mult2[o.mult])) >> 1;',
         '    return (((uint32_t(fnum) << ch.block) * uint32_t(tables().mult2[o.mult])) >> 1) & 0xffffu;')]),
    # ---- Music Volume 0 %, the producer, SFX ----
    ('Z1', 'muted music is still fully rendered (volume 0 keeps the song, at zero gain)', [
        (AUDIO, '        has_music() && music_volume_ > 0 ? song_for_context(context_) : MusicSong::None;',
         '        has_music() ? song_for_context(context_) : MusicSong::None;')]),
    ('Z2', 'a restart keeps the old song\'s buffered chip samples', [
        (SYN, '    active_ = true;\n    have_ = 0;', '    active_ = true;')]),
    ('Z3', 'SFX starve while the music is muted (pending cues no longer wake the pump)', [
        (STREAM_H, '    bool has_work() const { return !sfx_.idle() || music_.active(); }',
         '    bool has_work() const { return music_.active(); }')]),
    ('K1', 'the producer renders ahead of the ring (two renders per write)', [
        (STREAM, '    render_block(sfx_gain_q15, music_gain_q15);\n    silent_run_ = work ? 0 : silent_run_ + 1;\n    // Blocks not yet finished',
         '    render_block(sfx_gain_q15, music_gain_q15);\n    render_block(sfx_gain_q15, music_gain_q15);\n    silent_run_ = work ? 0 : silent_run_ + 1;\n    // Blocks not yet finished')]),
    ('K2', 'the producer never yields (the runaway guard never fires)', [
        (STREAM, '        if (++nonblocking_run_ >= kRunawayWrites) {', '        if (++nonblocking_run_ >= 0xffffffffu) {')]),
    ('K3', 'the ring is primed with one block instead of all of it (no render-ahead high water)', [
        (STREAM, '        if (loaded && primed_ < ring_blocks_) return false;', '        if (loaded && primed_ < 1) return false;')]),
    # ---- the mix counter ----
    ('C1', 'the clipping counter disabled', [
        (STREAM, '        clipped += over ? 1u : 0u;', '        (void)over;')]),
    ('C2', 'the clipping counter counts only the positive limit', [
        (STREAM, '        const bool over = sum > 32767 || sum < -32768;', '        const bool over = sum > 32767;')]),
    # ---- the render counters ----
    ('R1', 'render stats never update (no frame is counted)', [
        (RT, '        render_perf_.on_frame(uint64_t(frame_t0),uint32_t(tft_t0-frame_t0),uint32_t(tiles_end-start),\n'
             '                              uint32_t(tft_t1-tft_t0),uint32_t(tft_t1-frame_t0));', '')]),
    ('R2', 'a frame exactly one animation tick long counts as late', [
        (PERF, '    if (total_us > kLateFrameUs) ++late_;', '    if (total_us >= kLateFrameUs) ++late_;')]),
    ('R3', 'an input is never marked shown', [
        (RT, '        if(input_pending_us_>=0){render_perf_.on_input_shown(uint32_t(tft_t1-input_pending_us_));input_pending_us_=-1;}',
         '')]),
    ('R4', 'Developer screen redraws counted as gameplay frames', [
        (RT, '    if(!debug_mode&&e==ESP_OK){\n        render_perf_.on_frame(', '    if(e==ESP_OK){\n        render_perf_.on_frame(')]),
    ('R5', 'the frame percentile reports the bucket\'s lower edge', [
        (PERF, '            const uint32_t edge = b < kBuckets ? uint32_t((b + 1) * kBucketUs) : frame_max_;',
         '            const uint32_t edge = b < kBuckets ? uint32_t(b * kBucketUs) : frame_max_;')]),
    # ---- the report on the Developer screen ----
    ('D1', 'A3-04A\'s reporting: the lines go to the gameplay transcript, the Developer screen shows none', [
        (RT, '    perf_report_open_=in_menu&&perf_report_count_>0;\n    perf_report_pending_=!in_menu&&perf_report_count_>0;',
         '    for(size_t i=0;i<perf_report_count_&&ui_;++i)ui_->append(openu5::UiTextChannel::System,perf_report_lines_[i]);\n'
         '    perf_report_open_=false;\n    perf_report_pending_=false;')]),
    ('D2', 'the diagnostic results discarded (the report is built and never shown)', [
        (RT, '    perf_report_open_=in_menu&&perf_report_count_>0;', '    perf_report_open_=false;')]),
    ('D3', 'the report does not stay: any key dismisses it (no scrolling)', [
        (RT, '    if(up){if(perf_report_top_>0)--perf_report_top_;}\n    else if(down){if(perf_report_top_<last)++perf_report_top_;}',
         '    if(up||down)perf_report_open_=false;')]),
    ('D4', 'a report finished with the menu closed is dropped', [
        (RT, '    perf_report_pending_=!in_menu&&perf_report_count_>0;', '    perf_report_pending_=false;')]),
    ('D5', 'the benchmark shows no progress on the Developer screen', [
        (RT, '        if(audio_bench_.running())std::snprintf(out.status,sizeof(out.status),"Audio perf %lu/%lus %s: keep still",',
         '        if(false)std::snprintf(out.status,sizeof(out.status),"Audio perf %lu/%lus %s: keep still",')]),
    ('D6', 'the report loses its render section', [
        (PERF, '    if (const RenderPerfSnapshot *r = in.render) {', '    if (const RenderPerfSnapshot *r = nullptr) {')]),
    ('D7', 'the live read does not start a new window', [
        (RT, '    r.publish_perf_report("AUDIO/RENDER PERF  live window",has_audio?&a:nullptr,"Since last read",nullptr,nullptr,false);\n    r.reset_perf_windows();',
         '    r.publish_perf_report("AUDIO/RENDER PERF  live window",has_audio?&a:nullptr,"Since last read",nullptr,nullptr,false);')]),
    # ---- the firmware placement (host scans) ----
    ('I1', 'generate() left out of the IRAM fragment', [
        (LF, '    music_synth:_ZN6openu511OplEmulator8generateEPfS1_jj (noflash)\n', '')]),
    ('I2', 'jump tables allowed again in the IRAM synth file', [
        (FW_CMAKE, '    PROPERTIES COMPILE_OPTIONS "-O2;-ffp-contract=off;-fno-jump-tables")',
         '    PROPERTIES COMPILE_OPTIONS "-O2;-ffp-contract=off")')]),
    ('I3', 'FreeRTOS run-time statistics dropped from the firmware configuration', [
        (DEFAULTS, 'CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y\n', '')]),
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
    lines = ['# A3-04B mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
    failed = 0
    ran = 0
    for mid, label, edits in MUTATIONS:
        if ONLY and mid not in ONLY:
            continue
        ran += 1
        originals = {}
        ok = True
        results = {}
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
                failed += 1
                continue
            code, out = build()
            if code != 0:
                failed += 1
                lines.append('%s %s: INVALID (does not build)' % (mid, label))
                lines += ['    ' + l for l in out.splitlines() if 'error' in l][:4]
                continue
            results = run_tests()
        finally:
            for path, data in originals.items():
                open(path, 'wb').write(data)
                os.utime(path, None)
        killed = any(code != 0 and reds for code, reds in results.values())
        failed += 0 if killed else 1
        lines.append('%s %s: %s' % (mid, label, 'KILLED' if killed else 'SURVIVED'))
        for name, (code, reds) in results.items():
            if code != 0:
                lines.append('    %-8s exit=%d  %d RED' % (name, code, len(reds)))
                lines += ['        ' + r[:200] for r in reds[:6]]
        print(lines[-1] if len(lines) else '', flush=True)
    code, _ = build()
    restored = run_tests()
    lines.append('')
    lines.append('restored build exit=%d; ' % code + ', '.join('%s exit=%d' % (k, v[0]) for k, v in restored.items()))
    lines.append('mutations: %d, killed by a failing check: %d, survived or invalid: %d' % (ran, ran - failed, failed))
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines[-3:]))
    sys.exit(1 if failed or code or any(v[0] for v in restored.values()) else 0)


if __name__ == '__main__':
    main()
