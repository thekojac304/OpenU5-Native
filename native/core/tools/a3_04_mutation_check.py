#!/usr/bin/env python
"""Alpha 3 A3-04 -- mutation validation of the music decoder/synth and the
AlphaRuntime music-context wiring.

Each mutation changes ONE production line, rebuilds the two A3-04 test
targets and runs them; the file is then restored byte for byte and touched
(a restored file is older than the mutated object: without the touch ninja
keeps the mutant). A mutation that leaves every check GREEN is a SURVIVOR
and fails this run.

Usage: python native/core/tools/a3_04_mutation_check.py <build-dir> <log>
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
    'synth': [os.path.join(BUILD, 'a3_04_music_synth.exe'), CORE, AUDIO_PACK],
    'runtime': [os.path.join(BUILD, 'a3_04_music_runtime.exe'), RES, AUDIO_PACK, STOCK],
}
TARGETS = ['a3_04_music_synth', 'a3_04_music_runtime']

SYN = 'native/core/src/music_synth.cpp'
AUD = 'native/core/src/audio.cpp'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'

MUTATIONS = [
    ('M1 an absent program falls silent instead of the melodic-0 fallback', SYN,
     '    const OplTimbre *fallback = get(kMilesBankMelodic, 0);\n    return fallback ? *fallback : timbres_[0];',
     '    return timbres_[0];'),
    ('M2 velocity 0 is treated as a real note-on (the corpus never sends 0x8n)', SYN,
     '    if (velocity == 0) {\n        note_off(channel, note);\n        return;\n    }',
     '    if (false) {\n        note_off(channel, note);\n        return;\n    }'),
    ('M3 the sustain pedal never actually holds a note (CC64 always reads as released)', SYN,
     '    case 64: {\n        const bool pressed = value >= 64;',
     '    case 64: {\n        const bool pressed = false;'),
    ('M4 end_tick is the FIRST event, not the true maximum', SYN,
     '    out.end_tick = out.events.empty() ? 0 : out.events.back().tick;',
     '    out.end_tick = out.events.empty() ? 0 : out.events.front().tick;'),
    ('M5 a looping track never restarts (the loop branch is dead code)', SYN,
     '        } else if (loop_) {', '        } else if (false) {'),
    ('M6 the capacity jitter bug returns (exact read_pos_, not the stable upper bound)', SYN,
     '    const size_t needed = size_t(std::ceil(1.0 + ratio_ * double(frames - 1))) + 2;',
     '    const size_t needed = size_t(std::floor(read_pos_ + ratio_ * double(frames - 1))) + 2;'),
    ('M7 OPL2 and OPL3 channel counts are swapped', SYN,
     '    : kind_(kind), channel_count_(kind == OplChipKind::Opl3 ? 18 : 9) {}',
     '    : kind_(kind), channel_count_(kind == OplChipKind::Opl3 ? 9 : 18) {}'),
    ('M8 an unrecognised XMI status byte is accepted instead of rejected', SYN,
     '        } else {\n            return false; // unknown status byte: reject, do not guess\n        }',
     '        } else {\n            continue; // unknown status byte: reject, do not guess\n        }'),
    ('M9 intro page 7 falls in the wrong range (off-by-one)', AUD,
     'if (page <= 0x07) return MusicContext::IntroStones;', 'if (page < 0x07) return MusicContext::IntroStones;'),
    ('M10 the endgame scene table boundary is off by one', AUD,
     'if (scene <= 0x03) return MusicContext::EndgameStones;', 'if (scene < 0x03) return MusicContext::EndgameStones;'),
    ('M11 the frigate sentinel never reaches the selector (transport_tile always 0)', RT,
     'pos.transport_tile=uint8_t(turn_.transport_tile);', 'pos.transport_tile=0;'),
    ('M12 the Camp freeze is never checked (location always wins)', RT,
     '    if(camp_scene_active_||(narrative_pacer_.mounted()&&\n       (narrative_pacer_.scene()==openu5::NarrativeScene::Camp||narrative_pacer_.scene()==openu5::NarrativeScene::TrollSneak))){\n        audio_.play_music(openu5::MusicContext::Camp);return;}',
     '    if(false){\n        audio_.play_music(openu5::MusicContext::Camp);return;}'),
    ('M13 the victory flag is dropped from the combat/victory branch (source-scan guard)', RT,
     '    pos.combat_victory=combat_.victory;', ''),
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
    lines = ['# A3-04 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
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
