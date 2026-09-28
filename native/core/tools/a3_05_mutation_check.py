#!/usr/bin/env python
"""Alpha 3 A3-05 -- mutation validation of the session mutes and the kill cue
(ALPHA3_AUDIO.md section 29).

Each mutation changes ONE production anchor -- AudioService (openu5/audio.h,
src/audio.cpp), the Settings sessions (system_menu.cpp, frontend.cpp), the
kill routing (sfx_synth.cpp), the input adapter or the runtime -- rebuilds the
two host targets, a3_05_audio_mute (the pure halves) and a3_05_audio_controls
(the REAL runtime by raw keys), and records which checks turn RED. The file
is restored byte for byte and touched (a restored file is older than the
mutated object: without the touch ninja keeps the mutant). A mutation that
leaves every check GREEN is a SURVIVOR and fails this run; one that does not
build is INVALID (not killed). The volume curve is unchanged in A3-05 and is
deliberately not mutated here.

Usage: python native/core/tools/a3_05_mutation_check.py <build-dir> <log> [U1,U2,...]
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
AUDIO = os.path.join(ROOT, 'native/assets/openu5-audio.bin')
STOCK = os.path.join(ROOT, 'native/core/fixtures/a3-01-audio-stock.bin')
TARGETS = ['a3_05_audio_mute', 'a3_05_audio_controls']
ARGS = {'a3_05_audio_mute': [], 'a3_05_audio_controls': [RES, AUDIO, STOCK]}

AH = 'native/core/include/openu5/audio.h'
AC = 'native/core/src/audio.cpp'
SM = 'native/core/src/system_menu.cpp'
FE = 'native/core/src/frontend.cpp'
SY = 'native/core/src/sfx_synth.cpp'
AD = 'native/targets/tdeck/main/ui_input_adapter.cpp'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'

MUTATIONS = [
    # --- the mute never destroys the configured volume ---
    ('U1 the SFX mute zeroes the configured volume', AC,
     '    sfx_muted_ = muted;\n    if (backend_)',
     '    sfx_muted_ = muted;\n    if (muted) sfx_volume_ = 0;\n    if (backend_)'),
    ('U2 the music mute is written to the settings', RT,
     'else if(music){audio_.set_music_muted(!audio_.music_muted());',
     'else if(music){audio_.set_music_muted(!audio_.music_muted());if(audio_.music_muted())settings_.music_volume=0;'),
    # --- the right bus ---
    ('U3 the shortcuts swap buses', AD,
     "shortcut = lower == 'm' ? DeviceShortcut::MusicMute : DeviceShortcut::SfxMute;",
     "shortcut = lower == 'm' ? DeviceShortcut::SfxMute : DeviceShortcut::MusicMute;"),
    ('U8 an SFX edit unmutes music too', RT,
     'if(edits&openu5::kMusicVolumeEdited)audio_.set_music_muted(false);',
     'if(edits)audio_.set_music_muted(false);'),
    # --- the restore level ---
    ('U4 the SFX unmute restores 100 %', AH,
     'return sfx_muted_ ? 0 : volume_to_gain_q15(sfx_volume_); }',
     'return sfx_muted_ ? 0 : kUnityGainQ15; }'),
    ('U5 a restarted song plays at 100 %', AC,
     'if (backend_->start_music(want, volume_to_gain_q15(music_volume_))) {',
     'if (backend_->start_music(want, stats_.music_started ? kUnityGainQ15 : volume_to_gain_q15(music_volume_))) {'),
    # --- the mute actually silences ---
    ('U9 the music mute leaves the song playing', AC,
     'has_music() && music_volume_ > 0 && !music_muted_ ?',
     'has_music() && music_volume_ > 0 ?'),
    ('U10 a muted SFX request is still submitted', AC,
     'if (!backend_ || sfx_volume_ == 0 || sfx_muted_) {',
     'if (!backend_ || sfx_volume_ == 0) {'),
    # --- where it works ---
    ('U6 the toggle is ignored while the System Menu is open', RT,
     'if(shortcut==DeviceShortcut::MusicMute||shortcut==DeviceShortcut::SfxMute)\n        return toggle_audio_mute(',
     'if((shortcut==DeviceShortcut::MusicMute||shortcut==DeviceShortcut::SfxMute)&&!system_menu_.active())\n        return toggle_audio_mute('),
    ('U15 Shift is ignored (Alt+Shift+M is Alt+M again)', AD,
     "if (raw.modifiers.shift && (lower == 'm' || lower == 's')) {",
     "if (false && raw.modifiers.shift && (lower == 'm' || lower == 's')) {"),
    ('U16 every Alt+Shift chord is a mute', AD,
     "if (raw.modifiers.shift && (lower == 'm' || lower == 's')) {",
     "if (raw.modifiers.shift) {"),
    ('U11 stock files pretend music can be muted', RT,
     'if(music&&!audio_.has_music())',
     'if(false&&music&&!audio_.has_music())'),
    # --- Settings ---
    ('U7 a System Menu edit does not unmute', RT,
     'apply_volume_edits(system_menu_.take_volume_edits());',
     '(void)system_menu_.take_volume_edits();'),
    ('U18 a title Settings edit does not unmute', RT,
     'apply_volume_edits(frontend_.take_volume_edits());',
     '(void)frontend_.take_volume_edits();'),
    ('U19 the menus are never told the mutes', RT,
     '    system_menu_.set_audio_mutes(audio_.sfx_muted(),audio_.music_muted());',
     '    (void)0;'),
    ('U12 the System Menu SFX row ignores the mute', SM,
     'format_sfx_volume_row(d[4],96,settings_.sound_volume,sfx_muted_);',
     'format_sfx_volume_row(d[4],96,settings_.sound_volume,false);'),
    ('U13 the title SFX row ignores the mute', FE,
     'format_sfx_volume_row(dynamic[4],96,settings_.sound_volume,sfx_muted_);',
     'format_sfx_volume_row(dynamic[4],96,settings_.sound_volume,false);'),
    ('U20 a Music Volume edit is not reported', SM,
     'volume_edits_|=kMusicVolumeEdited;',
     ''),
    # --- feedback ---
    ('U17 no transcript line', RT,
     'if(!frontend_.active()&&ui_)ui_->append(openu5::UiTextChannel::System,line);',
     'if(false&&!frontend_.active()&&ui_)ui_->append(openu5::UiTextChannel::System,line);'),
    # --- the kill cue ---
    ('U14 the second kill burst restored', SY,
     'if (died) return SfxId::None;',
     'if (died) return SfxId::CombatDefeat;'),
]


def run(cmd):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def build():
    return run([CMAKE, '--build', BUILD, '--target'] + TARGETS)


def test():
    results = []
    for t in TARGETS:
        code, out = run([os.path.join(BUILD, t + '.exe')] + ARGS[t])
        reds = [l for l in out.splitlines() if l.startswith('RED')]
        results.append((t, code, reds))
    return results


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-05 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
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
            lines.append('    %s exit=%d  %d RED' % (t, code, len(reds)))
            lines += ['        ' + r[:200] for r in reds]
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
