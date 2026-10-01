"""Alpha 4 A4-UI4/PRES1 -- mutation check of the frontend and presentation batch.

    python tools/a4_ui4_mutation_check.py <build-dir> [ids,comma,separated | --anchors]

As tools/a4_end1_mutation_check.py: each mutant is one edit of production code
(anchors converted to the file's own line endings); the driver applies it,
touches the file, builds the named test targets, runs them, and restores +
touches the file whatever happens. KILLED = a named test exits non-zero,
INVALID = it does not build. `--anchors` only proves every anchor is unique.
The PATH must hold the host toolchain (ninja, g++, ctest); `qp` needs node.

The classes are the batch's own: the frontend (the hotkey footer, the boot
label, Small text, the Settings wording), the gameplay screen (Direction?, the
underworld caption, the zodiac square, the caret), the console (the ended line,
getdir's word on the row, the DS echoes, Klimb, the arena's Get-, Mix and its
hold), the scene beats (the ritual's bursts and their sound, the quake's
pulses, the Refuge's stage), the moongates (the counter, the grass under a
partial gate, the glow, the transit, its timing, its hold on keys) and D-73
(the native parity path out of step with the reference). Section 8.20 adds the
console package (the blank row, the bullet's cell, the prompt row, the inline
cursor, bottom anchoring, the bullet's two tones, the wave glyph, the busy
hold, answers, the still-frame animation, the scrolled view, the device's
binder, the arena's single Cast) and the reverse-video lists.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
BIN = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/'
NINJA = BIN + 'ninja.exe'
PACK = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
AUDIO = os.path.join(ROOT, 'native/assets/openu5-audio.bin')
SESSION = 'native/core/src/ui_session.cpp'
FRONTEND = 'native/core/src/frontend.cpp'
SYSMENU = 'native/core/src/system_menu.cpp'
WORLDFX = 'native/core/include/openu5/world_fx.h'
NARRATIVE = 'native/core/src/narrative_scene.cpp'
PRESENT = 'native/core/src/presentation.cpp'
QUEST = 'native/core/src/quest_world.cpp'
RUNTIME = 'native/targets/tdeck/main/alpha_runtime.cpp'
BOARD = 'native/targets/tdeck/main/tdeck_board.cpp'
RENDERER = 'native/targets/tdeck/main/native_renderer.cpp'
CAPTION = 'native/targets/tdeck/main/location_names.h'
TESTS = {
    'pres': ('a4_ui4_presentation_runtime', [PACK, AUDIO]),
    'moon': ('a4_ui4_moongate_runtime', [PACK, AUDIO]),
    'b53a': ('batch53a_ending_terminal', [PACK]),
    'end1': ('a4_end1_ending_runtime', [PACK, AUDIO]),
    'hf9': ('a3_hf9_refuge_cadence_runtime', [PACK, AUDIO]),
    'b7b': ('batch7b_tests', []),
    'b25': ('batch25_shard_ritual', [PACK]),
    'qp': ('quest_driver', None),  # run through ctest: node check-quests.ts against the reference
    'con': ('a4_ui4_console_runtime', [PACK, AUDIO]),
}

# (id, what, file, anchor, replacement, tests)
MUTANTS = [
    # --- the frontend ----------------------------------------------------------
    ('F1', 'the footer names J C T U A R only', FRONTEND,
     'menu_count()>8?"Select: arrows / Enter / J C T U A R S P D":"Select: arrows / Enter / J C T U A R S P";',
     '"Select: arrows / Enter / J C T U A R";', ['pres']),
    ('F2', 'the boot splash keeps its stale literal', BOARD,
     'release_label_?release_label_:"OpenU5",kWhite,2,2),\n                        kTag,"draw coherent boot version");',
     '"Alpha 2.0",kWhite,2,2),\n                        kTag,"draw coherent boot version");', ['pres']),
    ('F3', 'Small drops the last column again (col*5/4)', BOARD,
     'const int c0=metrics.glyph_width<5?within_x+(within_x>2):within_x*5/metrics.glyph_width;',
     'const int c0=within_x*5/metrics.glyph_width;', ['pres']),
    ('F3b', 'Small drops the bottom row again (row*7/6)', BOARD,
     'const int r0=metrics.glyph_height<7?glyph_y+(glyph_y>3):glyph_y*7/metrics.glyph_height;',
     'const int r0=glyph_y*7/metrics.glyph_height;', ['pres']),
    ('F4', 'the System Menu Settings footer says "Mic returns" again', SYSMENU,
     'v.footer=why?why:"Left/right changes; Mic saves";}', 'v.footer=why?why:"Left/right changes; Mic returns";}', ['pres']),
    ('F4b', 'the System Menu row is "Developer / Debug" again', SYSMENU,
     'v.lines[v.line_count++]="Developer";', 'v.lines[v.line_count++]="Developer / Debug";', ['pres']),
    # --- the gameplay screen ---------------------------------------------------
    ('H1', 'a world getdir shows the aim readout', RUNTIME,
     'ui_->target_command_kind()!=openu5::CommandKind::Fire){std::snprintf(text,sizeof(text),"Direction?");return text;}',
     'false){std::snprintf(text,sizeof(text),"Direction?");return text;}', ['pres']),
    ('H2', 'the underworld caption tests floor < 0 only', CAPTION,
     '(surface_floor < 0 || surface_floor == 255 ? "Underworld" : "Britannia")',
     '(surface_floor < 0 ? "Underworld" : "Britannia")', ['pres']),
    ('H3', 'the zodiac view keeps the strips', RUNTIME,
     'gem_view_active_||zodiac_view_active_||camp_source,camp_source);', 'gem_view_active_||camp_source,camp_source);', ['pres']),
    ('H4', 'no caret glyph', BOARD,
     "    case '^': return {0x04, 0x02, 0x01, 0x02, 0x04}; // A4-UI4 (D-52): the Z-stats ready mark\n", '', ['pres']),
    # --- the console ------------------------------------------------------------
    ('E1', 'the arena\'s "ended" line printed again (D-72, duplicate lines)', SESSION,
     '    if (event.kind == CombatEventKind::Ended) return;\n', '', ['pres', 'b53a', 'end1']),
    ('E2', 'getdir\'s word on its own row', SESSION,
     'if (getdir_prints_word(cmd.kind)) append_continuation(UiTextChannel::CommandEcho, getdir_word(a.direction));',
     'if (getdir_prints_word(cmd.kind)) command_echo(getdir_word(a.direction));', ['pres', 'b25']),
    ('E3', 'getdir\'s word in lower case', SESSION,
     '    case Direction::North: return "North";', '    case Direction::North: return "north";', ['pres', 'b25']),
    ('E4', 'Klimb echoes its token again', SESSION,
     'if (e.text && std::strcmp(e.text,"klimb")==0){Command c;c.kind=CommandKind::Klimb;begin_target(UiRequestId::Direction,"Direction?",c,-1,-1);}\n        else direction_request(CommandKind::Pass,e.text?e.text:"Direction");',
     'direction_request(e.text && std::strcmp(e.text,"klimb")==0?CommandKind::Klimb:CommandKind::Pass,e.text?e.text:"Direction");',
     ['pres']),
    ('E5', 'Cast echoes "Cast" (no dots)', SESSION,
     "    case 'c': { command_echo(\"Cast...\");UiIntent i; i.kind=UiIntentKind::OpenSpellSelection; i.request=UiRequestId::Spell; dispatch(i); return true; }\n    case 'e':",
     "    case 'c': { command_echo(\"Cast\");UiIntent i; i.kind=UiIntentKind::OpenSpellSelection; i.request=UiRequestId::Spell; dispatch(i); return true; }\n    case 'e':",
     ['pres']),
    ('E6', 'Ready without its blank row', SESSION,
     "    case 'r': { command_echo(\"Ready...\");command_echo(\"\");UiIntent i; i.kind=UiIntentKind::OpenEquipmentSelection; i.request=UiRequestId::Equipment; dispatch(i); return true; }\n    case 's':",
     "    case 'r': { command_echo(\"Ready...\");UiIntent i; i.kind=UiIntentKind::OpenEquipmentSelection; i.request=UiRequestId::Equipment; dispatch(i); return true; }\n    case 's':",
     ['pres']),
    ('E7', 'the arena Get echoes "Get" (no dash)', SESSION,
     'c.kind=CommandKind::CombatGet; command_echo("Get-");', 'c.kind=CommandKind::CombatGet; command_echo("Get");', ['pres']),
    ('E8', 'Mix echoes "Mix"', SESSION,
     'command_echo("Mix Reagents");command_echo("");', 'command_echo("Mix");', ['pres']),
    ('E9', 'no 10-tick hold after "Mixing..."', RUNTIME,
     'std::strcmp(e.text,"Mixing...")==0)return openu5::delay_ticks_ms(10);', 'std::strcmp(e.text,"Mixing...")==0)return 0;', ['pres']),
    # --- the scene beats --------------------------------------------------------
    ('S1', 'the ritual bursts back to 60 ms', WORLDFX,
     'constexpr uint32_t kWorldFxExplosionBurstMs = tone_sweep_ms(kBlackthornBurstSamples);',
     'constexpr uint32_t kWorldFxExplosionBurstMs = 60;', ['pres', 'b7b']),
    ('S1b', 'the ritual bursts are silent', RUNTIME,
     '        audio_.play_sfx(openu5::SfxId::CombatHit);\n        dirty_=true;dirty_reason_="cell-explosion-burst";',
     '        dirty_=true;dirty_reason_="cell-explosion-burst";', ['pres']),
    ('S2', 'a quake pulse is no frame', RUNTIME,
     '    else if(quake_offset_px!=quake_drawn_px_){dirty_=true;dirty_reason_="quake-pulse";}\n', '', ['pres', 'hf9']),
    ('S3', 'the Refuge Avatar from the darkness line', NARRATIVE,
     '    if (phase == RefugePhase::Void && !avatar_placed) return written;',
     '    if (phase == RefugePhase::Void && !avatar_placed && false) return written;', ['pres', 'hf9', 'b7b']),
    ('S3b', 'the slumber beat never places the Avatar', NARRATIVE,
     '            if (step.places_avatar) avatar_placed_ = true;\n', '', ['pres']),
    ('S3c', 'the last stage keeps the ghosts and the apparition', NARRATIVE,
     '    if (phase == RefugePhase::Void || phase == RefugePhase::Vertigo) return written;',
     '    if (phase == RefugePhase::Void) return written;', ['pres', 'b7b']),
    # --- the moongates ------------------------------------------------------------
    ('M1', 'the counter jumps (no stage per tick)', RUNTIME,
     '        if(moongate_settle_)moongate_stage_=night?uint8_t(openu5::kMoongateStages):0;',
     '        moongate_settle_=true;if(moongate_settle_)moongate_stage_=night?uint8_t(openu5::kMoongateStages):0;', ['moon']),
    ('M2', 'the partial gate over the ending\'s floor 0x44 in the world', RENDERER,
     'partial_gate(i%kViewportTiles,i/kViewportTiles,snapshot.moongate_rows,int16_t(kMoongateGroundTile));',
     'partial_gate(i%kViewportTiles,i/kViewportTiles,snapshot.moongate_rows,int16_t(0x44));', ['moon']),
    ('M3', 'a standing gate does not glow', PRESENT,
     'visibility(c,map,center,reveal_all,s.visible,moongate_stage>0&&!map.id.location);',
     'visibility(c,map,center,reveal_all,s.visible,false);', ['moon']),
    ('M4', 'no transit: the destination at once (the pre-A4-UI4 device)', RUNTIME,
     '    moongate_transit_=true;moongate_transit_us_=esp_timer_get_time();moongate_origin_=game_.position;',
     '    moongate_transit_=false;moongate_transit_us_=esp_timer_get_time();moongate_origin_=game_.position;', ['moon']),
    ('M5', 'the closing at one tick a stage', RUNTIME,
     'constexpr uint32_t kMoongateTransitMs=kMoongateCloseMs+15*openu5::delay_ticks_ms(2);',
     'constexpr uint32_t kMoongateTransitMs=kMoongateCloseMs+15*openu5::delay_ticks_ms(1);', ['moon']),
    ('M6', 'the transit lets keys through', RUNTIME,
     '    if(moongate_transit_active()&&shortcut==DeviceShortcut::None){',
     '    if(false&&moongate_transit_active()&&shortcut==DeviceShortcut::None){',
     ['moon']),
    ('M7', 'the transit without the sweep\'s hold', RUNTIME,
     'constexpr uint32_t kMoongateFizzleMs=openu5::run_n_frames_ms(1)+openu5::tone_sweep_ms(0x7530);',
     'constexpr uint32_t kMoongateFizzleMs=openu5::run_n_frames_ms(1);', ['moon']),
    # --- D-73 -----------------------------------------------------------------------
    # --- section 8.20: the console package and the lists --------------------
    ('C1', 'no blank row before a command (getkey\'s LF)', SESSION,
     '            blank.sequence = block->sequence;\n            emit(blank);',
     '            blank.sequence = block->sequence;\n            if (false) emit(blank);', ['con']),
    ('C2', 'a command row wraps at full width (no bullet cell)', SESSION,
     'if (line_length + spaces + word_length > (bullet ? columns - 1 : columns))',
     'if (line_length + spaces + word_length > columns)', ['con']),
    ('C3', 'no live prompt row', SESSION,
     '    if (console && session.console_cursor() == UiConsoleCursor::NewCommand) {',
     '    if (false && console && session.console_cursor() == UiConsoleCursor::NewCommand) {', ['con']),
    ('C4', 'getdir has no inline cursor', SESSION,
     '    if (mode_ == UiMode::TargetSelection && request_ == UiRequestId::Direction)\n        return UiConsoleCursor::Inline;\n',
     '', ['con']),
    ('C5', 'the console fills from the top again', BOARD,
     'const size_t top=ui.console_layout()&&count<transcript_rows?transcript_rows-count:0;',
     'const size_t top=0;', ['con']),
    ('C6', 'the bullet in one tone (no white edge)', BOARD,
     'return (kBulletWhite[sy] & bit) ? 2 : (kBulletBlue[sy] & bit) ? 1 : 0;',
     'return ((kBulletWhite[sy] | kBulletBlue[sy]) & bit) ? 1 : 0;', ['con']),
    ('C7', 'the wave starts one glyph late (0x06)', BOARD,
     'const uint8_t *wave=ibm_font_+size_t(0x05+console_phase_)*8;',
     'const uint8_t *wave=ibm_font_+size_t(0x06+console_phase_)*8;', ['con']),
    ('C8', 'the prompt and cursor ignore a paced hold', RUNTIME,
     '    if(dialogue_pacer_.holding()||dialogue_pacer_.queued()!=0)return false;\n', '', ['con']),
    ('C9', 'an answer echoes as a command (bullet, blank row)', SESSION,
     '                else answer_echo(openu5::direction_name(a.direction));',
     '                else command_echo(openu5::direction_name(a.direction));', ['con']),
    ('C10', 'the wave never moves on a still frame', BOARD,
     '    if(!console_cursor_.visible||!alpha_drawn_||frontend_drawn_||debug_drawn_||console_cursor_.phase==console_phase_)\n        return ESP_OK;',
     '    if(console_cursor_.visible||!console_cursor_.visible)\n        return ESP_OK;', ['con']),
    ('C11', 'a scrolled-back view keeps the cursor', BOARD,
     'if(count&&ibm_font_&&ui.scroll_offset_lines()==0&&ui.console_cursor()',
     'if(count&&ibm_font_&&ui.console_cursor()', ['con']),
    ('C12', 'the device never turns the console layout on', RUNTIME,
     '    ui_->set_console_layout(true);\n}', '    ui_->set_console_layout(false);\n}', ['con']),
    ('C13', 'the arena\'s Cast key echoes again (Cast... twice)', SESSION,
     "    case 'c': { UiIntent i; i.kind=UiIntentKind::OpenSpellSelection;",
     "    case 'c': { command_echo(\"Cast...\");UiIntent i; i.kind=UiIntentKind::OpenSpellSelection;", ['con']),
    ('P1', 'the picker selects in plain video', BOARD,
     'kListMetrics,i<selection->row_count&&i==selection->selected_row,2)', 'kListMetrics,false,2)', ['con']),
    ('P2', 'the picker bar is 12 px (stale rows behind it)', BOARD,
     'draw_text_box_metrics(184,44+int(i)*14,134,14,line', 'draw_text_box_metrics(184,44+int(i)*14,134,12,line', ['con']),
    ('P3', 'a shop selects in plain video', BOARD,
     'kListMetrics,current&&i==shop->selected_row,2)', 'kListMetrics,false,2)', ['con']),
    ('R1', 'the native parity path keeps the reference\'s old narration', QUEST,
     '    if(absorption)return CommandStatus::Success;\n', '', ['qp', 'end1']),
]


def touch(p):
    t = time.time()
    os.utime(p, (t, t))


def build(build_dir, targets):
    targets = [t for t in targets if t]
    if not targets:
        return True, ''
    r = subprocess.run([NINJA, '-C', build_dir] + targets, capture_output=True, text=True)
    return r.returncode == 0, r.stdout[-1500:]


def run(build_dir, key):
    target, args = TESTS[key]
    if args is None:
        r = subprocess.run([BIN + 'ctest.exe', '--test-dir', build_dir, '-R', '^quest_parity$'], capture_output=True,
                           text=True, timeout=900, cwd=ROOT)
        return r.returncode, ([] if r.returncode == 0 else ['quest_parity'])
    r = subprocess.run([os.path.join(build_dir, target + '.exe')] + args, capture_output=True, text=True, timeout=900,
                       cwd=ROOT)
    reds = []
    for line in r.stdout.splitlines():
        w = line.split()
        if w and w[0] == 'RED' and len(w) > 1:
            reds.append(w[1])
        elif 'check' in line and 'failed' in line:
            reds.append(line.strip()[:40])
    return r.returncode, reds


def anchored(rel, anchor, repl):
    path = os.path.join(ROOT, rel)
    text = open(path, 'rb').read().decode('utf-8')
    eol = '\r\n' if '\r\n' in text else '\n'
    return path, text, anchor.replace('\r\n', '\n').replace('\n', eol), repl.replace('\r\n', '\n').replace('\n', eol)


def main():
    build_dir = os.path.abspath(sys.argv[1])
    arg = sys.argv[2] if len(sys.argv) > 2 else ''
    if arg == '--anchors':
        bad = 0
        for mid, what, rel, anchor, repl, keys in MUTANTS:
            _, text, a, _ = anchored(rel, anchor, repl)
            if text.count(a) != 1:
                print(f'{mid} ANCHOR {text.count(a)} matches in {rel} -- {what}')
                bad += 1
        print(f'A4-UI4 mutants: {len(MUTANTS)} anchors, {bad} not unique')
        return
    only = set(arg.split(',')) if arg else None
    killed = invalid = survived = 0
    for mid, what, rel, anchor, repl, keys in MUTANTS:
        if only and mid not in only:
            continue
        path, text, a, b = anchored(rel, anchor, repl)
        original = open(path, 'rb').read()
        if text.count(a) != 1:
            print(f'{mid} ANCHOR-MISSING ({text.count(a)} matches) -- {what}', flush=True)
            invalid += 1
            continue
        try:
            open(path, 'wb').write(text.replace(a, b).encode('utf-8'))
            touch(path)
            ok, out = build(build_dir, [TESTS[k][0] for k in keys])
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
    print(f'A4-UI4 mutation check: {killed} killed, {survived} survived, {invalid} invalid; '
          f'restored build {"GREEN" if green else "RED"}')


if __name__ == '__main__':
    main()
