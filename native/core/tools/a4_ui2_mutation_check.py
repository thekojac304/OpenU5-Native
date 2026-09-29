"""Alpha 4 UI Batch 2 -- mutation check of the three A4-UI2 tests.

    python tools/a4_ui2_mutation_check.py <build-dir> [ids,comma,separated]

Each mutant is one edit of production code (anchors converted to the file's
own line endings). The driver applies it, touches the file, builds the named
test target(s), runs them, and restores + touches the file whatever happens
(see host-and-firmware-toolchain: a restored file older than the mutated
object would leave the mutant in the build). A mutant is KILLED when a named
test exits non-zero, INVALID when it does not build. The PATH must hold the
host toolchain (w64devkit) for the compiler's own subprocesses.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
NINJA = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/ninja.exe'
PACK = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
AUDIO = os.path.join(ROOT, 'native/assets/openu5-audio.bin')
BOARD = 'native/targets/tdeck/main/tdeck_board.cpp'
RUNTIME = 'native/targets/tdeck/main/alpha_runtime.cpp'
FRONTEND = 'native/core/src/frontend.cpp'
MENU = 'native/core/src/system_menu.cpp'
TESTS = {
    'title': ('a4_ui2_title_runtime', [PACK]),
    'music': ('a4_ui2_death_music_runtime', [PACK, AUDIO]),
    'menu': ('a4_ui2_save_menu_runtime', [PACK]),
    'storage': ('a3_04g_storage_runtime', [PACK]),
}

# (id, what, file, anchor, replacement, tests)
MUTANTS = [
    ('A1', 'credits in the compact 5x7 font', BOARD,
     'return draw_cells(x,y,width,8,x,text,color,SIZE_MAX,color,false,ChromeFont::Ibm);',
     'return draw_cells(x,y,width,8,x,text,color,SIZE_MAX,color,false,ChromeFont::Compact);', ['title']),
    ('A2', 'credits not centred (+8 px)', BOARD,
     'const int x=(kDisplayWidth-width)/2;account',
     'const int x=(kDisplayWidth-width)/2+8;account', ['title']),
    ('A3', '"Press a key" also left in the footer row', BOARD,
     'const char*footer=creation_art||title_credits?"":v.footer?v.footer:"";',
     'const char*footer=creation_art?"":v.footer?v.footer:"";', ['title']),
    ('A4', 'the block resent on every animation frame', BOARD,
     'bool changed=layout_changed||v.line_count!=frontend_cache_.line_count;',
     'bool changed=true;', ['title']),
    ('A5', 'the Title view keeps the generic kind', FRONTEND,
     'if(!intro_page_)v.kind=FrontendViewKind::TitleCredits;', '', ['title']),
    ('A6', 'the credit block at the old y=116', 'native/targets/tdeck/main/device_ui_views.h',
     'constexpr int kTitleCreditsY=130,', 'constexpr int kTitleCreditsY=116,', ['title']),
    ('B1', 'no re-derive when the Refuge begins', RUNTIME,
     'if(e.kind==openu5::GameEventKind::Refuge)sync_music();', '', ['music']),
    ('B2', 'no re-derive when the Refuge resolves', RUNTIME,
     '        // The scene ends in render(), so re-derive here: The Missing Monarch.\n        sync_music();\n',
     '        // The scene ends in render(), so re-derive here: The Missing Monarch.\n', ['music']),
    ('B3', 'the intake re-derives for TrollSneak, not the Refuge', RUNTIME,
     'if(e.kind==openu5::GameEventKind::Refuge)sync_music();',
     'if(e.kind==openu5::GameEventKind::TrollSneak)sync_music();', ['music']),
    ('C1', 'Latest by physical slot, not by age', FRONTEND,
     'l.latest=int8_t(s[1].sequence>s[0].sequence?1:0);', 'l.latest=0;', ['menu']),
    ('C2', 'System Menu Backup row loads the latest', MENU,
     'pending_.kind=SystemMenuIntentKind::LoadSlot;pending_.slot=l.backup;',
     'pending_.kind=SystemMenuIntentKind::LoadLatest;', ['menu']),
    ('C3', 'no footer notice after a save', RUNTIME,
     'system_menu_.set_notice(!ok?"Save failed. Your previous save is kept.":',
     '(void)(!ok?"Save failed. Your previous save is kept.":', ['menu']),
    ('C4', 'the shell body clear back at y=20', BOARD,
     'const int body_y=title_art?114:23;', 'const int body_y=title_art?114:20;', ['menu']),
    ('C5', 'rows not cut to 36 cells', FRONTEND,
     'int(std::min(kSaveRowChars,cap-1))', 'int(cap-1)', ['menu']),
    ('C6', 'a refused slot loses its sequence (memory card)',
     'native/targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp',
     'slots[i].sequence = g_slots[i].present ? g_slots[i].commit.sequence : 0;', 'slots[i].sequence = 0;', ['menu']),
    ('C7', 'a refused slot loses its sequence (the real alpha_save.cpp)', 'native/targets/tdeck/main/alpha_save.cpp',
     'slots[i].present=true;slots[i].sequence=commit.sequence;source[i]="refused";',
     'slots[i].present=true;source[i]="refused";', ['storage']),
    ('C8', 'title Load Game: Back goes to the main menu', FRONTEND,
     'enter(FrontendState::Continue,now);cursor_=1;}', 'enter(FrontendState::MainMenu,now);}', ['menu']),
    ('C9', 'no place in the summary', 'native/targets/tdeck/main/alpha_save_generation.cpp',
     'std::snprintf(out.place,sizeof(out.place),"%s",hud_location_caption(',
     'if(false)std::snprintf(out.place,sizeof(out.place),"%s",hud_location_caption(', ['menu']),
    ('C10', 'the Save row does not say what it overwrites', MENU,
     'cursor_==1?"Saves now; the previous save becomes the backup":', '', ['menu']),
    ('C11', 'no warning on Create New Character', FRONTEND,
     'cursor_==1&&any_valid_save(saves_)?', 'false&&any_valid_save(saves_)?', ['menu']),
    ('C12', 'a damaged or missing backup does nothing silently', MENU,
     'else set_notice(l.backup<0?"No backup save yet":"That backup is damaged and cannot load");', '', ['menu']),
    ('C13', 'no party size in the summary', 'native/targets/tdeck/main/alpha_save_generation.cpp',
     'out.party=openu5::party_members(s.game.party).count;', 'out.party=0;', ['menu']),
]


def touch(p):
    t = time.time()
    os.utime(p, (t, t))


def build(build_dir, targets):
    r = subprocess.run([NINJA, '-C', build_dir] + targets, capture_output=True, text=True)
    return r.returncode == 0, r.stdout[-1500:]


def run(build_dir, key):
    target, args = TESTS[key]
    r = subprocess.run([os.path.join(build_dir, target + '.exe')] + args, capture_output=True, text=True)
    reds = [l.split()[1] for l in r.stdout.splitlines() if l.strip().startswith('RED') and len(l.split()) > 1]
    return r.returncode, reds


def main():
    build_dir = os.path.abspath(sys.argv[1])
    only = set(sys.argv[2].split(',')) if len(sys.argv) > 2 else None
    killed = invalid = survived = 0
    for mid, what, rel, anchor, repl, keys in MUTANTS:
        if only and mid not in only:
            continue
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        eol = '\r\n' if '\r\n' in text else '\n'
        a, b = anchor.replace('\n', eol), repl.replace('\n', eol)
        if text.count(a) != 1:
            print(f'{mid} ANCHOR-MISSING ({text.count(a)} matches) -- {what}', flush=True)
            invalid += 1
            continue
        try:
            open(path, 'wb').write(text.replace(a, b).encode('utf-8'))
            touch(path)
            targets = [TESTS[k][0] for k in keys]
            ok, out = build(build_dir, targets)
            if not ok:
                print(f'{mid} INVALID (does not build) -- {what}\n{out}', flush=True)
                invalid += 1
                continue
            results = [(k,) + run(build_dir, k) for k in keys]
            dead = any(code != 0 for _, code, _ in results)
            detail = '; '.join(f'{TESTS[k][0]} exit={code} RED={",".join(r) or "-"}' for k, code, r in results)
            print(f'{mid} {"KILLED" if dead else "SURVIVED"} -- {what} -- {detail}', flush=True)
            killed += dead
            survived += not dead
        finally:
            open(path, 'wb').write(original)
            touch(path)
    ok, _ = build(build_dir, [t for t, _ in TESTS.values()])
    green = ok and all(run(build_dir, k)[0] == 0 for k in TESTS)
    print(f'A4-UI2 mutation check: {killed} killed, {survived} survived, {invalid} invalid; '
          f'restored build {"GREEN" if green else "RED"}')


if __name__ == '__main__':
    main()
