#!/usr/bin/env python
"""Alpha 3 A3-01 -- mutation validation of the audio guards.

Each mutation changes ONE production line, rebuilds, runs the A3-01 tests and
records which checks turn RED; the file is then restored byte for byte and
touched (a restored file is older than the mutated object: without the touch
ninja keeps the mutant -- the Batch 26 lesson). A mutation that leaves every
test GREEN is a SURVIVOR and fails this run.

Usage: python native/core/tools/a3_01_mutation_check.py <build-dir> <log>
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
NODE = 'C:/Program Files/nodejs/node.exe'
CORE = os.path.join(ROOT, 'native', 'core')
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]

CONTRACT = [os.path.join(BUILD, 'a3_01_audio_contract.exe'), os.path.join(ROOT, 'game/src/core/sfx.ts'),
            os.path.join(ROOT, 'game/src/ui/music.ts'), CORE, os.path.join(CORE, 'fixtures/a3-01-audio-stock.bin'),
            os.path.join(ROOT, 'native/assets/openu5-audio.bin')]
RUNTIME = [os.path.join(BUILD, 'a3_01_audio_runtime.exe'), os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin'),
           os.path.join(ROOT, 'native/assets/openu5-audio.bin'), os.path.join(CORE, 'fixtures/a3-01-audio-stock.bin')]
CAPABILITY = [NODE, '--import', 'tsx', os.path.join(ROOT, 'native/tools/u5pack/check-audio-pack.ts'),
              '--real', os.path.join(ROOT, 'original/u5/ultima5'), '--fixture', os.path.join(CORE, 'fixtures/a3-01-audio-stock.bin')]
TESTS = {'contract': CONTRACT, 'runtime': RUNTIME, 'capability': CAPABILITY}

AUDIO = 'native/core/src/audio.cpp'
PACK = 'native/core/src/audio_pack.cpp'
MENU = 'native/core/src/system_menu.cpp'
FRONT = 'native/core/src/frontend.cpp'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
DET = 'native/tools/u5pack/audio-capability.ts'
WR = 'native/tools/u5pack/audio.ts'

MUTATIONS = [
    ('M1 music reaches the backend without the supported patch', AUDIO,
     'has_music() && music_volume_ > 0 ? song_for_context(context_) : MusicSong::None;',
     'music_volume_ > 0 ? song_for_context(context_) : MusicSong::None;'),
    ('M2 an SFX volume change also drives the music channel', AUDIO,
     '    if (backend_) backend_->set_gain(AudioChannel::Sfx, volume_to_gain_q15(sfx_volume_));\n}',
     '    if (backend_) backend_->set_gain(AudioChannel::Music, volume_to_gain_q15(sfx_volume_));\n}'),
    ('M3 SFX volume 0 is not mute', AUDIO,
     'if (!backend_ || sfx_volume_ == 0) {', 'if (!backend_) {'),
    ('M4 the same song restarts on every request', AUDIO,
     '    if (want == song_) return;\n', '    if (want == song_ && want == MusicSong::None) return;\n'),
    ('M5 a linear gain law', AUDIO,
     'return uint16_t(v * v * kUnityGainQ15 / (uint32_t(kVolumeMax) * kVolumeMax));',
     'return uint16_t(v * kUnityGainQ15 / kVolumeMax);'),
    ('M6 two reference cue strings swapped', AUDIO,
     '{"combat-hit", SfxOrigin::Original},             // NB(10,3000,2000) kernel 0x35de\n    {"combat-hit-heavy", SfxOrigin::Original},',
     '{"combat-hit-heavy", SfxOrigin::Original},             // NB(10,3000,2000) kernel 0x35de\n    {"combat-hit", SfxOrigin::Original},'),
    ('M7 a "supported" claim is trusted without its songs', PACK,
     '        if (!complete) return fail(AudioPackState::Inconsistent);', '        (void)complete;'),
    ('M8 the payload CRC is not checked', PACK,
     '    if (info.payload_crc32 != le32(d + 24)) return fail(AudioPackState::Corrupt);', ''),
    ('M9 a stale major version is accepted', PACK,
     'if (info.version_major != kAudioPackVersionMajor) return fail(AudioPackState::UnsupportedVersion);',
     'if (info.version_major == 0) return fail(AudioPackState::UnsupportedVersion);'),
    ('M10 the Music Volume row adjusts while music is unavailable', MENU,
     'case kMusicVolumeRow:if(music_availability_==MusicAvailability::Available)settings_.music_volume',
     'case kMusicVolumeRow:settings_.music_volume'),
    ('M11 Return to Title forgets the music availability', FRONT,
     '*this=FrontendSession{};music_availability_=music;', '*this=FrontendSession{};(void)music;'),
    ('M12 paced cues are not routed at their release', RT,
     '    self.present_audio(e);\n    self.ui_->consume(e);', '    self.ui_->consume(e);'),
    ('M13 cues are played at emission, before the pacer', RT,
     '    if(narrative_pacer_.active()?narrative_pacer_.scene()==openu5::NarrativeScene::Camp',
     '    present_audio(e);\n    if(narrative_pacer_.active()?narrative_pacer_.scene()==openu5::NarrativeScene::Camp'),
    ('M14 settings volumes never reach the audio service', RT,
     '    audio_.set_sfx_volume(settings_.sound_volume);\n    audio_.set_music_volume(settings_.music_volume);\n', ''),
    ('M15 configure_audio does not tell the System Menu', RT,
     '    system_menu_.set_music_availability(availability);\n', ''),
    ('M16 a load does not flush SFX', RT, '    audio_.flush_for_load();\n', ''),
    ('M17 the Developer binder drops the audio test', RT, 'services.audio_test=audio_test_tone;', ''),
    ('M18 the audio path draws from the game RNG', RT,
     '    audio_.play_sfx(id,e.note);\n}', '    audio_.play_sfx(id,e.note);game_.rng.next_raw16();\n}'),
    ('M19 any mid.drv is trusted', DET,
     '    ? known.find((k) => k.size === driverFile.bytes.length && k.sha256 === driverHash)',
     '    ? known[0]'),
    ('M20 foreign XMI files count as stock', DET,
     '    if (foreignXmi.length)\n', '    if (false)\n'),
    ('M21 an incomplete patch still ships its songs', WR,
     '  if (d.capability === "supported-music-patch") {', '  if (d.songs.some((s) => s.found)) {'),
]


def run(cmd, cwd):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def build():
    code, out = run([CMAKE, '--build', BUILD, '--target', 'a3_01_audio_contract', 'a3_01_audio_runtime'], ROOT)
    return code, out


def run_tests():
    results = {}
    for name, cmd in TESTS.items():
        code, out = run(cmd, ROOT)
        reds = [l for l in out.splitlines() if l.startswith('RED')]
        results[name] = (code, reds)
    return results


def main():
    env_path = os.path.dirname(CMAKE)
    os.environ['PATH'] = env_path + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-01 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
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
                lines.append('    %-10s exit=%d  %d RED' % (name, code, len(reds)))
                lines += ['        ' + r for r in reds[:6]]
    code, _ = build()
    restored = run_tests()
    lines.append('')
    lines.append('restored build exit=%d; ' % code + ', '.join(
        '%s exit=%d' % (k, v[0]) for k, v in restored.items()))
    lines.append('mutations: %d, killed: %d, survived: %d' % (len(MUTATIONS), len(MUTATIONS) - survivors, survivors))
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines[-3:]))
    sys.exit(1 if survivors or code or any(v[0] for v in restored.values()) else 0)


if __name__ == '__main__':
    main()
