"""Alpha 4 UI Batch 1 (B): mutate the restyle and require a4_ui1_chrome_runtime to go RED.

Each mutant is one anchored edit of production code; a mutant that does not
build is INVALID (not killed). The source is restored (and touched) after each.

    python a4_ui1_mutation_check.py [build-dir] [MB1,MB5,...]
"""
import os, subprocess, sys, time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CORE = os.path.join(ROOT, 'native', 'core')
BUILD = os.path.join(CORE, sys.argv[1] if len(sys.argv) > 1 else 'build-a4-ui1')
ONLY = set(sys.argv[2].split(',')) if len(sys.argv) > 2 else None
NINJA = r'C:\Dev\TamaPoke\.build-tools\w64devkit\bin\ninja.exe'
BOARD = 'native/targets/tdeck/main/tdeck_board.cpp'
MUTANTS = [
    ('MB1', 'rows paint over the right rule again (135 px)', BOARD,
     'draw_cells(openu5::kHudRightX,y,openu5::kHudRightW-1,8,', 'draw_cells(openu5::kHudRightX,y,openu5::kHudRightW,8,'),
    ('MB2', 'the -> ignores a sleeping member', BOARD,
     "const bool arrow=index==game.party.active_character&&status!='D'&&status!='S';",
     "const bool arrow=index==game.party.active_character&&status!='D';"),
    ('MB3', 'the roster in the compact font', BOARD,
     'draw_panel_row(row,4+int(row)*8,line,color,invert,force,ChromeFont::Ibm)',
     'draw_panel_row(row,4+int(row)*8,line,color,invert,force,ChromeFont::Compact)'),
    ('MB4', 'the date day-month-year', BOARD,
     '"%ld-%ld-%ld",long(game.time.month),long(game.time.day)', '"%ld-%ld-%ld",long(game.time.day),long(game.time.month)'),
    ('MB5', 'the digital clock dropped', BOARD,
     'std::snprintf(right,sizeof(right),"%02ld:%02ld",long(game.time.hour),long(game.time.minute));',
     'std::snprintf(right,sizeof(right),"%s","");'),
    ('MB6', 'MOVE clips the caption to 18', BOARD, '"%-17.17s MOVE"', '"%-18.18s MOVE"'),
    ('MB7', 'no > < ends on the notch', BOARD,
     'pixel=(kBracketWhite[band_row]&bit)?kChromeRule:(kBracketBlue[band_row]&bit)?kChromeBand:i>0?kBlack:kChromeBand;',
     'pixel=kChromeBand;'),
    ('MB8', 'the wind caption keeps the old prefix', BOARD,
     'const char *caption=bands_active?dungeon_bands->direction:hud.wind_visible?hud.wind:"";',
     'const char *caption=bands_active?dungeon_bands->direction:hud.wind_visible?" Wind: ":"";'),
    ('MB9', 'menus lose the reverse video', BOARD,
     'line,kWhite,menu_metrics,selected,1),kTag,"frontend sized line"', 'line,kWhite,menu_metrics,false,1),kTag,"frontend sized line"'),
    ('MB10', 'no frontend shell', BOARD,
     'ESP_RETURN_ON_ERROR(draw_frontend_shell(v.title?v.title:""),kTag,"frontend shell");',
     '(void)0;'),
    ('MB11', 'the footer stays green', BOARD,
     'draw_text_box(8,228,304,9,footer,kChromeDim)', 'draw_text_box(8,228,304,9,footer,kGreen)'),
    ('MB12', 'the touch reserve black again', BOARD,
     'fill_rect(0,0,kDisplayWidth,kDisplayHeight,kChromeBand),kTag,"initialize Alpha 2.0 game screen"',
     'fill_rect(0,0,kDisplayWidth,kDisplayHeight,kBlack),kTag,"initialize Alpha 2.0 game screen"'),
    ('MB13', 'no play-window rule', BOARD,
     'ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudViewportX-1,top,1,bottom-top+1,kChromeRule),kTag,"play window rule left");',
     '(void)top;(void)bottom;'),
    ('MB14', 'the chrome font never handed over', 'native/targets/tdeck/main/alpha_runtime.cpp',
     'board.set_chrome_font(resources_.ibm_font);', 'board.set_chrome_font(nullptr);'),
    ('MB15', 'the retained menu row not inverted', BOARD,
     'i<v.line_count&&int(i)==v.selected_line,1),kTag,"update retained frontend row"',
     'false,1),kTag,"update retained frontend row"'),
]
env = dict(os.environ, PATH=r'C:\Dev\TamaPoke\.build-tools\w64devkit\bin;' + os.environ['PATH'])


def run_test():
    return subprocess.run([os.path.join(BUILD, 'a4_ui1_chrome_runtime.exe'),
                           os.path.join(ROOT, 'native', 'assets', 'openu5-alpha1-resources.bin'),
                           os.path.join(ROOT, 'original', 'u5', 'ultima5', 'IBM.CH')], capture_output=True, text=True)


results = []
for mid, what, rel, old, new in MUTANTS:
    if ONLY and mid not in ONLY:
        continue
    path = os.path.join(ROOT, rel)
    original = open(path, 'rb').read()
    text = original.decode('utf8')
    try:
        if text.count(old) != 1:
            results.append((mid, what, f'ANCHOR x{text.count(old)}'))
            continue
        open(path, 'wb').write(text.replace(old, new).encode('utf8'))
        build = subprocess.run([NINJA, '-C', BUILD, 'a4_ui1_chrome_runtime'], env=env, capture_output=True, text=True)
        if build.returncode:
            results.append((mid, what, 'INVALID (does not build)'))
            continue
        reds = [l.split()[1] for l in run_test().stdout.splitlines() if l.startswith('RED ') and not l.startswith('RED G1')]
        results.append((mid, what, 'KILLED by ' + ','.join(reds) if reds else 'SURVIVED'))
    finally:
        open(path, 'wb').write(original)
        os.utime(path, None)
        time.sleep(1)
        os.utime(path, None)
subprocess.run([NINJA, '-C', BUILD, 'a4_ui1_chrome_runtime'], env=env, capture_output=True)
restored = [l for l in run_test().stdout.splitlines() if l.startswith('RED ') and not l.startswith('RED G1')]
for mid, what, verdict in results:
    print(f'{mid} {what}: {verdict}')
killed = sum(1 for r in results if r[2].startswith('KILLED'))
print(f'{killed}/{len(results)} killed; restored tree: {"clean" if not restored else "RED " + str(restored)}')
