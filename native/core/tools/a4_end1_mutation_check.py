"""Alpha 4 A4-END1 -- mutation check of the ending.

    python tools/a4_end1_mutation_check.py <build-dir> [ids,comma,separated | --anchors]

As tools/a4_ui3_mutation_check.py: each mutant is one edit of production code
(anchors converted to the file's own line endings); the driver applies it,
touches the file, builds the named test targets, runs them, and restores +
touches the file whatever happens. KILLED = a named test exits non-zero,
INVALID = it does not build. `--anchors` only proves every anchor is unique.
The PATH must hold the host toolchain; `pages` needs node + the repo's tsx.

E0 is the pre-END1 device in one edit (the presenter off: Batch 53's one-shot
narration): the RED-first view of every A4-END1 guard that a missing ending
would leave standing. The rest are the brief's classes -- the trigger and the
text (order, once), the throne room (room, recolor, XOR, gate), the waits (40
frames, the step cadence, the holds, timed from the key, menus stop the
clock), the keys (one key one getkey, no type-ahead, Y/N only, case-folded,
Mic is a key and never an abort, no save), the dissolve (capture, order, rate,
bands, release), the pages and the scroll (position, opaque cells, drawn
once), the music (0x15 chain, 0x18 per page, 0x1b waits), the sounds
(footsteps, the two sweeps), the terminal screens (forever, Load, Return to
Title, un-win), and the TypeScript composer of the seven screens.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
NINJA = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/ninja.exe'
PACK = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
AUDIO = os.path.join(ROOT, 'native/assets/openu5-audio.bin')
ORIGINAL = os.path.join(ROOT, 'original/u5/ultima5')
SCENE = 'native/core/src/endgame_scene.cpp'
RUNTIME = 'native/targets/tdeck/main/alpha_runtime.cpp'
BOARD = 'native/targets/tdeck/main/tdeck_board.cpp'
RENDERER = 'native/targets/tdeck/main/native_renderer.cpp'
SESSION = 'native/core/src/ui_session.cpp'
QUEST = 'native/core/src/quest_world.cpp'
COMPOSER = 'native/tools/u5pack/alpha1-endgame.ts'
TESTS = {
    'scene': ('a4_end1_endgame_scene', [ORIGINAL]),
    'rt': ('a4_end1_ending_runtime', [PACK, AUDIO]),
    'b53': ('batch53_release_blockers', [PACK]),
    'b53a': ('batch53a_ending_terminal', [PACK]),
    'pages': (None, ['node', '--import', 'tsx', 'native/tools/u5pack/check-endgame-pages.ts',
                     '--real', ORIGINAL, '--pack', PACK]),
}

# (id, what, file, anchor, replacement, tests)
MUTANTS = [
    # --- the pre-END1 device -------------------------------------------------
    ('E0', 'the presenter off: Batch 53\'s one-shot narration (the pre-END1 device)', RUNTIME,
     'quest_.endgame_presenter=endgame_&&resources_.endgame_room&&resources_.endgame_pages&&endgame_records_[10];',
     'quest_.endgame_presenter=false;', ['rt', 'b53', 'b53a']),
    # --- trigger and text ----------------------------------------------------
    ('E1', 'the presenter AND the narration: the ending\'s text twice', QUEST,
     'auto status=c.quest_world&&c.quest_world->endgame_presenter?(rescue_lord_british(c.game,true),CommandStatus::Success):rescue_events(c,true,sink);',
     'auto status=rescue_events(c,true,sink);', ['rt', 'b53']),
    ('E2', 'the GameWon token is printed again (D-57)', SESSION,
     '        if(e.kind==GameEventKind::GameWon)enter_ending();\n        break;',
     '        if(e.text)append(UiTextChannel::Quest,e.text);\n        if(e.kind==GameEventKind::GameWon)enter_ending();\n        break;',
     ['rt']),
    ('E3', 'Lord British\'s speech starts at ENDMSG 0x05 (0x04 lost)', SCENE,
     '            member_ = 4;\n            pc_ = Pc::Speech;', '            member_ = 5;\n            pc_ = Pc::Speech;',
     ['scene', 'rt']),
    ('E4', 'the device line comes over the overlay at once (A-15)', RUNTIME,
     'if(!(endgame_&&endgame_->active()))ui_->append(openu5::UiTextChannel::System,"The quest is complete. Alt+M: System Menu");',
     'ui_->append(openu5::UiTextChannel::System,"The quest is complete. Alt+M: System Menu");', ['rt']),
    # --- the throne room -----------------------------------------------------
    ('V1', 'no recolor (fn36 never ran)', SCENE, '    s.endgame_recolor = true;', '    s.endgame_recolor = false;',
     ['scene', 'rt']),
    ('V2', 'a wrong recolor LUT entry (1 -> 1, not 5)', SCENE,
     'const uint8_t kEndgameRecolorLut[16] = {0x0, 0x5, 0x4, 0x4,', 'const uint8_t kEndgameRecolorLut[16] = {0x0, 0x1, 0x4, 0x4,',
     ['scene', 'rt']),
    ('V3', 'the renderer ignores the recolor', RENDERER,
     'auto lut_for=[&](uint16_t tile)->const uint8_t*{return snapshot.endgame_recolor&&endgame_recolored_tile(tile)?kEndgameRecolorLut:nullptr;};',
     'auto lut_for=[&](uint16_t tile)->const uint8_t*{(void)tile;return nullptr;};', ['rt']),
    ('V4', 'the revive\'s XOR is not drawn', RUNTIME, '(endgame_source&&endgame_->inverted())', '(false&&endgame_->inverted())',
     ['rt']),
    ('V5', 'the rising gate shows the bottom of 0xdc, not its top', RENDERER,
     'std::copy(gate,gate+rows*8,bitmap+(16-rows)*8);', 'std::copy(gate+(16-rows)*8,gate+128,bitmap+(16-rows)*8);', ['rt']),
    ('V6', 'Lord British starts one row up', SCENE, 'actors_[kLbSlot] = {kLbTile, 5, 8, true};',
     'actors_[kLbSlot] = {kLbTile, 5, 7, true};', ['scene', 'rt']),
    ('V7', 'a wrong lineup (members 1 and 2 swap sides)', SCENE, 'constexpr uint8_t kLineupCol[6] = {5, 4, 6, 3, 5, 7};',
     'constexpr uint8_t kLineupCol[6] = {5, 6, 4, 3, 5, 7};', ['scene']),
    ('V8', 'the longer-axis rule flips on a tie', SCENE, '    if (dcol < drow) a.row', '    if (dcol <= drow) a.row', ['scene']),
    ('V9', 'the bed pose is 0x1b', SCENE, 'if (under == 0xab) pose = 0x1a;', 'if (under == 0xab) pose = 0x1b;', ['scene']),
    ('V10', 'the gate rises 14 stages', SCENE, 'if (++gate_stage_ < 0x10) return frames(1);',
     'if (++gate_stage_ < 0x0f) return frames(1);', ['scene']),
    # --- the waits -----------------------------------------------------------
    ('W1', '32 frames before Lord British walks, not 40', SCENE, '            return frames(0x28);\n        case Pc::LbWalk:',
     '            return frames(0x20);\n        case Pc::LbWalk:', ['scene', 'rt']),
    ('W2', 'the revive shows the healed roster at the print, not after the sweep', SCENE,
     '            inverted_ = true;\n            if (sink.cue) sink.cue(sink.context, EndgameCue::ReviveSweep);',
     '            party.characters[member_].status = \'G\';\n            inverted_ = true;\n            if (sink.cue) sink.cue(sink.context, EndgameCue::ReviveSweep);',
     ['scene', 'rt']),
    ('W3', 'after a getkey the next wait is timed from when the getkey began', RUNTIME,
     '        if(endgame_->wait()!=openu5::EndgameWait::Frames&&endgame_->wait()!=openu5::EndgameWait::Hold)\n            endgame_mark_us_=endgame_clock_us_;\n',
     '', ['rt']),
    ('W4', 'the clock runs on under the System Menu (catch-up when it closes)', RUNTIME,
     '    if(endgame_&&endgame_->active()&&(system_menu_.active()||ui_->mode()==openu5::UiMode::DebugMenu))\n        endgame_last_us_=esp_timer_get_time();\n',
     '', ['rt']),
    # --- the keys ------------------------------------------------------------
    ('K1', 'the trackball ends a getkey', RUNTIME, 'else if(action.kind==openu5::UiActionKind::Back)key=u\'\\b\';',
     'else if(action.kind==openu5::UiActionKind::Back)key=u\'\\b\';else if(action.kind==openu5::UiActionKind::Direction)key=u\' \';',
     ['rt']),
    ('K2', 'a key hurries a timed wait (keys outside a getkey are not nothing)', SCENE,
     '    if (wait_ == EndgameWait::Key) { ready_ = true; return true; }',
     '    if (wait_ == EndgameWait::Key || wait_ == EndgameWait::Frames || wait_ == EndgameWait::Hold) { ready_ = true; return true; }',
     ['scene', 'rt']),
    ('K3', 'any key answers the box question (No)', SCENE,
     '    if (up != u\'Y\' && up != u\'N\') return false;\n    answer_ = uint8_t(up);',
     '    answer_ = uint8_t(up == u\'Y\' ? \'Y\' : \'N\');', ['scene', 'rt']),
    ('K4', 'no case folding: a lower-case y is read again', SCENE,
     'const char16_t up = ch >= u\'a\' && ch <= u\'z\' ? char16_t(ch - 0x20) : ch;', 'const char16_t up = ch;',
     ['scene', 'rt', 'b53']),
    ('K5', 'the Mic aborts the ending', RUNTIME, 'else if(action.kind==openu5::UiActionKind::Cancel)key=0x1b;',
     'else if(action.kind==openu5::UiActionKind::Cancel){stop_endgame();return true;}', ['rt']),
    ('K6', 'Alt+S saves the ended game', RUNTIME, '}else if(shortcut==DeviceShortcut::Save&&ui_->ending_active()){',
     '}else if(shortcut==DeviceShortcut::Save&&false){', ['rt', 'b53a']),
    ('K7', 'Yes without the box wins (0x08b9 ignores the box)', SCENE, 'if (answer_ == \'Y\' && game_->wooden_box) {',
     'if (answer_ == \'Y\') {', ['scene', 'rt']),
    # --- the dissolve --------------------------------------------------------
    ('D1', 'the fizzle is always skipped', RUNTIME, 'if(endgame_capture&&(e!=ESP_OK||!board.begin_endgame_capture())){',
     'if(endgame_capture){', ['rt']),
    ('D2', 'the capture frame is not a full repaint', BOARD,
     '    alpha_drawn_ = false; // the next show_alpha sends every pixel\n', '', ['rt']),
    ('D3', 'the fizzle runs at 32,000 px/s', RUNTIME, 'constexpr uint32_t kEndgameFizzlePixelsPerSecond = 25600;',
     'constexpr uint32_t kEndgameFizzlePixelsPerSecond = 32000;', ['rt']),
    ('D4', 'a wrong 16-bit tap (0xb000)', SCENE, '0x240, 0x500, 0xca0, 0x1b00, 0x3500, 0x6000, 0xb400};',
     '0x240, 0x500, 0xca0, 0x1b00, 0x3500, 0x6000, 0xb000};', ['scene', 'rt']),
    ('D5', 'the bands above and below the picture never fizzle', BOARD,
     '    while (bands.produced() < band_target && bands.next(x, y))', '    while (false && bands.produced() < band_target && bands.next(x, y))',
     ['rt']),
    ('D6', 'the copy of the panel is never released', BOARD,
     '    if (endgame_shadow_) heap_caps_free(endgame_shadow_);\n    endgame_shadow_ = nullptr;\n', '', ['rt']),
    # --- the pages and the scroll --------------------------------------------
    ('P1', 'the picture at y 0, not centred at y 20', BOARD, 'constexpr int kEndgameTop = 20;', 'constexpr int kEndgameTop = 0;',
     ['rt']),
    ('P2', 'the scroll\'s cells are transparent (only ink drawn)', BOARD,
     'pixel = ink != ((cell->flags & openu5::kEndgameCellInverse) != 0) ? palette[15] : palette[0];',
     'if (ink != ((cell->flags & openu5::kEndgameCellInverse) != 0)) pixel = palette[15];', ['rt']),
    ('P3', 'a page is redrawn every frame', RUNTIME, 'page,dirty_||force);', 'page,true);', ['rt']),
    ('P4', 'the datestamp centres one column right', SCENE, 'const int col = (avail - last) / 2;',
     'const int col = (avail - last + 1) / 2;', ['scene', 'rt']),
    ('P5', '"Twenty" for the ordinal stem (Twentyieth)', SCENE, 'accum("Twent");', 'accum("Twenty");', ['scene']),
    ('P6', 'the playtime borrows 30 days, not 28', SCENE, 'if (p.days < 0) { p.days += 0x1c; --p.months; }',
     'if (p.days < 0) { p.days += 0x1e; --p.months; }', ['scene']),
    # --- the music and the sounds --------------------------------------------
    ('M1', 'no Reunion -> Rule Britannia chain', RUNTIME,
     'if(endgame_music_==openu5::MusicContext::Reunion&&endgame_reunion_end_us_&&now>=endgame_reunion_end_us_){',
     'if(false){', ['rt']),
    ('M2', '0x1b cuts a Reunion still playing', RUNTIME,
     '}else if(self.endgame_music_!=openu5::MusicContext::Reunion||!self.endgame_reunion_end_us_||now>=self.endgame_reunion_end_us_){',
     '}else{', ['rt']),
    ('M3', 'the story music by page, not page + 1 (0x0aee passes BL = page + 1)', SCENE,
     'sink.music(sink.context, EndgameMusic::StoryScene, uint8_t(page_ + 1));',
     'sink.music(sink.context, EndgameMusic::StoryScene, uint8_t(page_));', ['scene', 'rt']),
    ('M4', 'no footsteps', SCENE, '                if (sink.cue) sink.cue(sink.context, EndgameCue::Footstep);', '',
     ['scene', 'rt']),
    ('M5', 'the revive plays the orb\'s sweep', RUNTIME,
     'self.audio_.play_sfx(openu5::SfxId::EndgameOrb,cue==openu5::EndgameCue::ReviveSweep?1:0);',
     'self.audio_.play_sfx(openu5::SfxId::EndgameOrb,0);', ['rt']),
    ('M6', 'no orb sweep', SCENE, '            if (sink.cue) sink.cue(sink.context, EndgameCue::OrbSweep);\n', '',
     ['scene', 'rt']),
    # --- the last screens ----------------------------------------------------
    ('F1', 'a key on the scroll ends the ending', SCENE, '    if (wait_ != EndgameWait::YesNo) return false;',
     '    if (wait_ == EndgameWait::Forever) { phase_ = EndgamePhase::Inactive; return true; }\n    if (wait_ != EndgameWait::YesNo) return false;',
     ['scene', 'rt']),
    ('F2', 'Load leaves the overlay running', RUNTIME,
     '    // synchronize_ending: a won game reloads into the bare Ending).\n    stop_endgame();',
     '    // synchronize_ending: a won game reloads into the bare Ending).', ['rt', 'b53a']),
    ('F3', 'Return to Title leaves the overlay running', RUNTIME,
     '        // A4-END1: the device\'s way out of ENDGAME.OVL (DOS needed a reset).\n        stop_endgame();',
     '        // A4-END1: the device\'s way out of ENDGAME.OVL (DOS needed a reset).', ['rt']),
    ('F4', 'an un-won game keeps the overlay (Developer Preset: Endgame)', RUNTIME, '    if(!won)stop_endgame();\n', '',
     ['b53a']),
    ('F5', 'no wander in the stranded room', SCENE, '            wander(kOrder[wander_index_]);', '            (void)kOrder;',
     ['scene', 'rt']),
    # --- the TypeScript composer of the seven screens ------------------------
    ('T1', 'the justification rounds instead of truncating (idiv)', COMPOSER, 'const q = Math.trunc(extra / spaces);',
     'const q = Math.round(extra / spaces);', ['pages']),
    ('T2', 'an 8-pixel leading', COMPOSER, '    penY += 9;                                                 // 0x023e',
     '    penY += 8;                                                 // 0x023e', ['pages']),
    ('T3', 'the art drawn before the headlines', COMPOSER,
     '  if (lay.headlines) {\n    const t = src.text;\n    if (i === 0) { blit(page, t[0]!, 216, 0); blit(page, t[4]!, 152, 28); }\n    else { blit(page, t[5]!, 224, 0); blit(page, t[0]!, 176, 0); }\n  }\n  blit(page, src.end[lay.file]![lay.sub]!, lay.artX, lay.artY);',
     '  blit(page, src.end[lay.file]![lay.sub]!, lay.artX, lay.artY);\n  if (lay.headlines) {\n    const t = src.text;\n    if (i === 0) { blit(page, t[0]!, 216, 0); blit(page, t[4]!, 152, 28); }\n    else { blit(page, t[5]!, 224, 0); blit(page, t[0]!, 176, 0); }\n  }',
     ['pages']),
    ('T4', 'the clip at y 200, not 0xc0', COMPOSER, '    if (y >= 0xc0) return; // 0x01c9', '    if (y >= 0xc8) return; // 0x01c9',
     ['pages']),
    ('T5', 'no hyphen at a soft-hyphen break', COMPOSER,
     '    if (at(lineEnd) === 0x5f) draw(0x2d, penX, penY);          // 0x021a-0x0236: the hyphen\n', '', ['pages']),
    ('T6', 'the scroll at (0,0)', COMPOSER, '  blit(page, src.scroll[0]!, 40, 0); // 0x020b-0x0217',
     '  blit(page, src.scroll[0]!, 0, 0); // 0x020b-0x0217', ['pages']),
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
    cmd = args if target is None else [os.path.join(build_dir, target + '.exe')] + args
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=900, cwd=ROOT)
    reds = []
    for line in r.stdout.splitlines():
        w = line.split()
        if w and w[0] == 'RED' and len(w) > 1:
            reds.append(w[1])
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
        print(f'A4-END1 mutants: {len(MUTANTS)} anchors, {bad} not unique')
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
            detail = '; '.join(f'{TESTS[k][0] or "check-endgame-pages"} exit={code} RED={",".join(r) or "-"}'
                               for k, code, r in results)
            print(f'{mid} {"KILLED" if dead else "SURVIVED"} -- {what} -- {detail}', flush=True)
            killed += dead
            survived += not dead
        finally:
            open(path, 'wb').write(original)
            touch(path)
    ok, _ = build(build_dir, [t for t, _ in TESTS.values()])
    green = ok and all(run(build_dir, k)[0] == 0 for k in TESTS)
    print(f'A4-END1 mutation check: {killed} killed, {survived} survived, {invalid} invalid; '
          f'restored build {"GREEN" if green else "RED"}')


if __name__ == '__main__':
    main()
