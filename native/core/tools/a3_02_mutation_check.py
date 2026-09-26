#!/usr/bin/env python
"""Alpha 3 A3-02 -- mutation validation of the synthesizer and SFX guards.

Each mutation changes ONE production line, rebuilds, runs the A3-02 tests
(and the A3-01 ones, which still guard the service and the device source
shape) and records which checks turn RED; the file is then restored byte for
byte and touched (a restored file is older than the mutated object: without
the touch ninja keeps the mutant -- the Batch 26 lesson). A mutation that
leaves every test GREEN is a SURVIVOR and fails this run.

Usage: python native/core/tools/a3_02_mutation_check.py <build-dir> <log>
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
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
AUDIO_PACK = os.path.join(ROOT, 'native/assets/openu5-audio.bin')
STOCK = os.path.join(CORE, 'fixtures/a3-01-audio-stock.bin')

TESTS = {
    'synth': [os.path.join(BUILD, 'a3_02_sfx_synth.exe'), os.path.join(CORE, 'fixtures/a3-02-sfx-reference.txt'), CORE],
    'runtime': [os.path.join(BUILD, 'a3_02_sfx_runtime.exe'), RES, AUDIO_PACK],
    'a3-01 contract': [os.path.join(BUILD, 'a3_01_audio_contract.exe'), os.path.join(ROOT, 'game/src/core/sfx.ts'),
                       os.path.join(ROOT, 'game/src/ui/music.ts'), CORE, STOCK, AUDIO_PACK],
    'a3-01 runtime': [os.path.join(BUILD, 'a3_01_audio_runtime.exe'), RES, AUDIO_PACK, STOCK],
}
TARGETS = ['a3_02_sfx_synth', 'a3_02_sfx_runtime', 'a3_01_audio_contract', 'a3_01_audio_runtime']

SYN = 'native/core/src/sfx_synth.cpp'
AUD = 'native/core/src/audio.cpp'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
DEV = 'native/targets/tdeck/main/tdeck_audio.cpp'
BT = 'native/core/src/blackthorn_scene.cpp'

MUTATIONS = [
    ('M1 sweep direction reversed (bx -= step)', SYN,
     '        bx_ = uint16_t(bx_ + s.delta); // add bx, step',
     '        bx_ = uint16_t(bx_ - s.delta); // add bx, step'),
    ('M2 wrong harpsichord frequency table (digits 1 and 2 swapped)', SYN,
     '{0x1eab, 0x0c2c, 0x0da9, 0x0f56,', '{0x1eab, 0x0da9, 0x0c2c, 0x0f56,'),
    ('M3 wrong ceremony pitch table (idx 3)', SYN,
     '{8810, 7830, 7060, 6550, 5950,', '{8810, 7830, 7060, 6050, 5950,'),
    ('M4 0 % is not silent (gain floor of 1)', SYN,
     '            out[done + i] = apply_gain_q15(int16_t(v), gain_q15);',
     '            out[done + i] = apply_gain_q15(int16_t(v), gain_q15 ? gain_q15 : 64);'),
    ('M5 the service no longer mutes at 0 %', AUD,
     'if (!backend_ || sfx_volume_ == 0) {', 'if (!backend_) {'),
    ('M6 the Camp scene waits on the audio path (holds while audio refuses)', RT,
     '    narrative_pacer_.pump(uint32_t(esp_timer_get_time()/1000),{this,narrative_beat},{this,release_scene_event});',
     '    if(!audio_.stats().sfx_refused)narrative_pacer_.pump(uint32_t(esp_timer_get_time()/1000),{this,narrative_beat},{this,release_scene_event});'),
    ('M7 the Blackthorn scene waits on the audio path (stretches while sound is submitted)', RT,
     '    blackthorn_pacer_.pump(uint32_t(esp_timer_get_time()/1000),ui_->event_sink());',
     '    blackthorn_pacer_.pump(uint32_t(esp_timer_get_time()/1000-100*audio_.stats().sfx_submitted),ui_->event_sink());'),
    ('M8 a load does not clear SFX', RT, '    audio_.flush_for_load();\n', ''),
    ('M9 Return to Title does not clear SFX', RT, '        audio_.stop_sfx();\n        frontend_.start(', '        frontend_.start('),
    ('M10 harpsichord wrong note index (digit + 1)', SYN,
     'kInstrumentNotes[clamp_index(param, 9)]', 'kInstrumentNotes[clamp_index(param + 1, 9)]'),
    ('M11 queue overflow refuses the producer instead of dropping the oldest', SYN,
     '    const bool full = pending_count_ == kPendingDepth;\n    push(r);',
     '    const bool full = pending_count_ == kPendingDepth;\n    if (full) return SfxAdmit::Overflowed;\n    push(r);'),
    ('M12 the device producer waits on a full queue', DEV,
     'if (xQueueSend(queue_, &command, 0) == pdTRUE) return true; // never waits',
     'if (xQueueSend(queue_, &command, portMAX_DELAY) == pdTRUE) return true; // never waits'),
    ('M13 gain applied twice', SYN,
     '            out[done + i] = apply_gain_q15(int16_t(v), gain_q15);',
     '            out[done + i] = apply_gain_q15(apply_gain_q15(int16_t(v), gain_q15), gain_q15);'),
    ('M14 SFX volume touches the music channel', AUD,
     '    if (backend_) backend_->set_gain(AudioChannel::Sfx, volume_to_gain_q15(sfx_volume_));\n}',
     '    if (backend_) backend_->set_gain(AudioChannel::Sfx, volume_to_gain_q15(sfx_volume_));\n'
     '    if (backend_) backend_->set_gain(AudioChannel::Music, volume_to_gain_q15(sfx_volume_));\n}'),
    ('M15 a cancelled tone keeps generating', SYN,
     '    pending_head_ = pending_count_ = 0;\n    voice_.release(kReleaseFrames);',
     '    pending_head_ = pending_count_ = 0;'),
    ('M16 the duty cycle is not modelled (fixed 50 % gate)', SYN,
     '        gate_ = dx_ > bx_;             // cmp dx, bx; ja -> gate on',
     '        gate_ = dx_ > 0x7fff;          // cmp dx, bx; ja -> gate on'),
    ('M17 noise draws from an open interval (the inc cx dropped)', SYN,
     '    const uint16_t span = uint16_t(band - 0x64 + 1);', '    const uint16_t span = uint16_t(band - 0x64);'),
    ('M18 durations off the calibration (frames at 2x rate)', SYN,
     '    return uint32_t(uint64_t(s.iterations) * s.iteration_half_samples * kSfxOutputRateHz / kSpeakerHalfSampleRate);',
     '    return uint32_t(uint64_t(s.iterations) * s.iteration_half_samples * kSfxOutputRateHz / kSpeakerSweepRate);'),
    ('M19 an unaudited cue is accepted (moongate)', SYN,
     '    case SfxId::WaterfallFall:\n    case SfxId::TimeSpell:',
     '    case SfxId::WaterfallFall: case SfxId::Moongate:\n    case SfxId::TimeSpell:'),
    ('M20 harpsichord notes are coalesced like ordinary cues', SYN,
     '    if (cls != SfxClass::Instrument && pending_count_) {', '    if (pending_count_) {'),
    ('M21 a scene cue does not preempt', SYN,
     '    if (cls >= SfxClass::Scene) {', '    if (cls > SfxClass::Diagnostic) {'),
    ('M22 the transport ignores the flush epoch', SYN,
     '    if (epoch != epoch_) {\n        ++stats_.stale;', '    if (epoch + 1 == 0 && epoch != epoch_) {\n        ++stats_.stale;'),
    ('M23 the Blackthorn beat cue is not routed', BT,
     '            if (step.beat.sfx != BlackthornSfx::None && cue_sink_.cue) cue_sink_.cue(cue_sink_.context, step.beat.sfx);\n', ''),
    ('M24 the ceremony event is not routed', RT,
     '    if(e.kind==openu5::GameEventKind::MagicCeremony){audio_.play_sfx(openu5::SfxId::TimeSpell,std::clamp(e.note,int32_t(0),int32_t(8)));return;}',
     ''),
    ('M25 the combat burst ignores the target side', RT,
     'if(combat_.actors[i].id==id)return combat_.actors[i].member!=255;',
     'if(combat_.actors[i].id==id)return false;'),
    ('M26 the noise PRNG is re-seeded per burst', SYN,
     '    voice_.start(program, &noise_);', '    noise_ = kNoiseSeed;\n    voice_.start(program, &noise_);'),
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
    lines = ['# A3-02 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
    survivors = 0
    for label, rel, old, new in MUTATIONS:
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        eol = '\r\n' if '\r\n' in text else '\n'
        o, n = old.replace('\n', eol), new.replace('\n', eol)
        count = text.count(o)
        if count != 1:
            lines.append('%s: ANCHOR NOT UNIQUE (%d) in %s' % (label, count, rel))
            survivors += 1
            continue
        try:
            open(path, 'wb').write(text.replace(o, n).encode('utf-8'))
            code, out = build()
            if code != 0:
                lines.append('%s [%s]: BUILD FAILED (counts as killed)' % (label, rel))
                lines += ['    ' + l for l in out.splitlines() if 'error' in l][:4]
                continue
            results = run_tests()
        finally:
            open(path, 'wb').write(original)
            os.utime(path, None)
        killed = any(code != 0 for code, _ in results.values())
        survivors += 0 if killed else 1
        lines.append('%s [%s]: %s' % (label, rel, 'KILLED' if killed else 'SURVIVED'))
        for name, (code, reds) in results.items():
            if code != 0:
                lines.append('    %-14s exit=%d  %d RED' % (name, code, len(reds)))
                lines += ['        ' + r for r in reds[:6]]
    code, _ = build()
    restored = run_tests()
    lines.append('')
    lines.append('restored build exit=%d; ' % code + ', '.join('%s exit=%d' % (k, v[0]) for k, v in restored.items()))
    lines.append('mutations: %d, killed: %d, survived: %d' % (len(MUTATIONS), len(MUTATIONS) - survivors, survivors))
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines[-3:]))
    sys.exit(1 if survivors or code or any(v[0] for v in restored.values()) else 0)


if __name__ == '__main__':
    main()
