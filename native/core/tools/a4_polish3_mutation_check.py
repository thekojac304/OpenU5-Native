"""Alpha 4 A4-POLISH3 mutation check (ALPHA4_UI.md section 13).

    python tools/a4_polish3_mutation_check.py <build dir> [ids]

Each mutant edits ONE production site, rebuilds the two A4-POLISH3 targets and
expects at least one RED check. The source is restored (and touched, so ninja
never keeps a mutated object) after every mutant. tdeck_input.cpp is device-only
(not built on the host); its policy lives in keyboard_backlight.h, mutated here.
"""
import os
import subprocess
import sys
import time

CORE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TDECK = os.path.join(CORE, '..', 'targets', 'tdeck', 'main')
NINJA = r'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/ninja.exe'
PACK = os.path.join(CORE, '..', 'assets', 'openu5-alpha1-resources.bin')

FS = os.path.join(CORE, 'src', 'frontend_settings.cpp')
FE = os.path.join(CORE, 'src', 'frontend.cpp')
SM = os.path.join(CORE, 'src', 'system_menu.cpp')
FH = os.path.join(CORE, 'include', 'openu5', 'frontend.h')
KB = os.path.join(TDECK, 'keyboard_backlight.h')
RT = os.path.join(TDECK, 'alpha_runtime.cpp')

MUTANTS = [
    ('M1', FS, 'j["keyboardBacklight"]=J(int(s.keyboard_backlight));', '', 'the level is never written to settings.json'),
    ('M2', FS, 's.keyboard_backlight=uint8_t(light);', '', 'a stored level is ignored on load'),
    ('M3', FS, 'light=kKeyboardBacklightDefault;\ns.keyboard_backlight', 'return false;\ns.keyboard_backlight',
     'a bad level rejects the whole document'),
    ('M4', FS, 'int light=kKeyboardBacklightDefault;', 'int light=2;', 'an older file (no key) migrates to Medium, not Off'),
    ('M5', KB, 'desired == applied) return false;', 'false) return false;', 'an unchanged level is written again'),
    ('M6', KB, 'return desired != failed || attempts < kMaxAttempts;', 'return true;', 'a failing write is retried forever'),
    ('M7', KB, 'void keyboard_reinitialized() {\n        applied = kNone;', 'void keyboard_reinitialized() {\n',
     'a recovered keyboard is not re-lit'),
    ('M8', KB, '{0, 32, 127, 191, 255}', '{0, 32, 128, 191, 255}', 'Medium is not the sketch\'s 127'),
    ('M9', KB, 'frame[0] = kKeyboardBacklightCommand;', 'frame[0] = 0x02;', 'the frame uses the Alt+B command'),
    ('M10', SM, 'case kKeyboardLightRow:settings_.keyboard_backlight=step_keyboard_backlight(settings_.keyboard_backlight,d);break;',
     '', 'the System Menu row does not edit'),
    ('M11', FE, 'const uint8_t setting_count=7U;', 'const uint8_t setting_count=6U;', 'the title cursor cannot reach the row'),
    ('M12', FE, 'keyboard_light_available_=keyboard;', '(void)keyboard;','Return to Title forgets that no keyboard answered'),
    ('M13', FH, 'return uint8_t((level % kKeyboardBacklightLevels + delta + kKeyboardBacklightLevels) % kKeyboardBacklightLevels);',
     'return uint8_t(level + delta < 0 ? 0 : level + delta > 4 ? 4 : level + delta);', 'the row clamps instead of cycling'),
    ('M14', RT, 'audio_.set_music_volume(settings_.music_volume);\n    sync_keyboard_light();',
     'audio_.set_music_volume(settings_.music_volume);', 'a Settings change is not sent'),
    ('M15', RT, 'if(!keyboard_light_.set||settings_.keyboard_backlight==keyboard_light_sent_)return;',
     'if(!keyboard_light_.set)return;', 'every Settings key is sent again (no dedupe)'),
    ('M16', RT, '    frontend_.set_keyboard_light_available(sink.available);\n    sync_keyboard_light();',
     '    frontend_.set_keyboard_light_available(sink.available);', 'the saved level is not applied at boot'),
    ('M17', RT, 'system_menu_.set_keyboard_light_available(sink.available);', '', 'the menu never hears that no keyboard answered'),
]


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace')


def reds(build):
    out = ''
    for exe, args in (('a4_polish3_keyboard_light.exe', []), ('a4_polish3_keyboard_light_runtime.exe', [PACK])):
        r = run([os.path.join(build, exe)] + args)
        out += r.stdout
    return [l.split()[1] for l in out.splitlines() if l.startswith('RED ')]


def main():
    build = os.path.abspath(sys.argv[1])
    only = set(sys.argv[2].split(',')) if len(sys.argv) > 2 else None
    killed = invalid = survived = 0
    for mid, path, old, new, what in MUTANTS:
        if only and mid not in only:
            continue
        raw = open(path, 'rb').read()
        text = raw.decode('utf-8')
        eol = '\r\n' if '\r\n' in text else '\n'
        o, n = old.replace('\n', eol), new.replace('\n', eol)
        if text.count(o) != 1:
            print(f'{mid} ANCHOR {text.count(o)} matches in {os.path.basename(path)}')
            invalid += 1
            continue
        try:
            open(path, 'wb').write(text.replace(o, n).encode('utf-8'))
            b = run([NINJA, '-C', build, 'a4_polish3_keyboard_light', 'a4_polish3_keyboard_light_runtime'])
            if b.returncode != 0:
                print(f'{mid} INVALID (does not build): {what}')
                invalid += 1
                continue
            red = reds(build)
            if red:
                killed += 1
                print(f'{mid} KILLED by {",".join(red)}: {what}')
            else:
                survived += 1
                print(f'{mid} SURVIVED: {what}')
        finally:
            open(path, 'wb').write(raw)
            now = time.time()
            os.utime(path, (now, now))
    b = run([NINJA, '-C', build, 'a4_polish3_keyboard_light', 'a4_polish3_keyboard_light_runtime'])
    restored = b.returncode == 0 and not reds(build)
    print(f'\nA4-POLISH3 mutations: {killed} killed, {survived} survived, {invalid} invalid; restored tree GREEN: {restored}')
    return 0 if survived == 0 and invalid == 0 and restored else 1


if __name__ == '__main__':
    sys.exit(main())
