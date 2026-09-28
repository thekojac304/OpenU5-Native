#!/usr/bin/env python
"""Alpha 3 A3-HF4 -- mutation validation of the successful-load cleanup
(ALPHA3_AUDIO.md section 30).

Each mutation changes ONE production anchor -- AlphaRuntime::
reset_transient_after_load() and its call, the two load routes' failure
branches (alpha_runtime.cpp), or UiSession::reset_after_load() (ui_session.cpp)
-- rebuilds the host targets below and records which checks turn RED. The file
is restored byte for byte and touched (a restored file is older than the
mutated object: without the touch ninja keeps the mutant). A mutation that
leaves every target GREEN is a SURVIVOR and fails this run; one that does not
build is INVALID (not killed).

Targets: a3_hf4_load_transient_runtime (the new checks) and the existing load
tests a mutation of the loaded game's own state should trip -- batch24
(reload parity), batch26 (dungeon save), batch27 (Alt+L), batch53a (ending).

Usage: python native/core/tools/a3_hf4_mutation_check.py <build-dir> <log> [H1,H2,...]
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
TARGETS = ['a3_hf4_load_transient_runtime', 'batch24_reload_parity', 'batch26_dungeon_save', 'batch27_alt_load',
           'batch53a_ending_terminal']

RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
UI = 'native/core/src/ui_session.cpp'
CALL = 'ui_->reset_after_load(resolve_synchronized_base_mode(openu5::UiMode::Exploration,context_.combat,dungeon_.active));'
MODE_TAIL = '    mode_ = world;\n}\n\nUiMode UiSession::world_return_mode'

MUTATIONS = [
    # --- the defect itself and the load/reset boundary ---
    ('H1 the UI is not reset on load (HEAD behaviour)', RT, '    ' + CALL + '\n', ''),
    ('H2 the whole cleanup is never called', RT,
     '    reset_transient_after_load();\n    // Batch 53A. Leave the Ending first', '    // Batch 53A. Leave the Ending first'),
    ('H3 a failed Alt+L clears too', RT,
     'bool ok=save_.load(context_,outdoor_,terrain_,actors_,retained_,ms);if(ok){',
     'bool ok=save_.load(context_,outdoor_,terrain_,actors_,retained_,ms);reset_transient_after_load();if(ok){'),
    ('H4 a failed menu load clears too', RT,
     '&&!ok)ui_->append(openu5::UiTextChannel::System,"No valid save");',
     '&&!ok){reset_transient_after_load();ui_->append(openu5::UiTextChannel::System,"No valid save");}'),
    ('H5 a load always lands in Explore (a dungeon save included)', RT, CALL,
     'ui_->reset_after_load(openu5::UiMode::Exploration);'),
    # --- UiSession: which modal survives ---
    ('H6 a target selector survives', UI, MODE_TAIL,
     '    if (mode_ != UiMode::TargetSelection) mode_ = world;\n}\n\nUiMode UiSession::world_return_mode'),
    ('H7 a yes/no survives', UI, MODE_TAIL,
     '    if (mode_ != UiMode::YesNo) mode_ = world;\n}\n\nUiMode UiSession::world_return_mode'),
    ('H8 a picker (Mix / Ready / Use) survives', UI, MODE_TAIL,
     '    if (!is_selection(mode_)) mode_ = world;\n}\n\nUiMode UiSession::world_return_mode'),
    ('H9 the old prompt text is kept', UI,
     'input_[0]=0;input_length_=0;prompt_[0]=0;pending_command_={};target_render_marker_=false;\n    cancel_means_no_',
     'input_[0]=0;input_length_=0;pending_command_={};target_render_marker_=false;\n    cancel_means_no_'),
    ('H10 the old request id is kept', UI,
     '    request_=UiRequestId::None;selection_={};selection_cursor_=0;\n    input_[0]=0;input_length_=0;prompt_[0]=0;pending_command_={};target_render_marker_=false;\n    cancel_means_no_',
     '    selection_={};selection_cursor_=0;\n    input_[0]=0;input_length_=0;prompt_[0]=0;pending_command_={};target_render_marker_=false;\n    cancel_means_no_'),
    ('H11 the UI shop phase stays open', UI,
     'shop_phase_=ShopPhase::Closed;shop_cursor_=0;shop_offer_count_=0;', 'shop_cursor_=0;shop_offer_count_=0;'),
    ('H12 a session base mode (Shop/Dialogue) survives', UI,
     '    base_mode_ = return_mode_ = pre_combat_mode_ = world;\n    shop_return_mode_',
     '    return_mode_ = pre_combat_mode_ = world;\n    shop_return_mode_'),
    ('H13 the Developer menu is closed by a load', UI,
     '    if (mode_ == UiMode::DebugMenu) { debug_return_mode_ = world; return; }\n', ''),
    ('H14 the Developer menu returns to the old picker', UI,
     '{ debug_return_mode_ = world; return; }', '{ return; }'),
    ('H15 the transcript is cleared (persistent presentation lost)', UI,
     'cancel_means_no_=escape_clears_=false;camp_hours_=0;',
     'cancel_means_no_=escape_clears_=false;camp_hours_=0;transcript_count_=0;transcript_head_=0;'),
    # --- the core halves of the old game's questions and sessions ---
    ('H16 awaiting_exit survives', RT,
     'commands_.awaiting_exit=false;commands_.awaiting_troll=false;', 'commands_.awaiting_troll=false;'),
    ('H17 the troll toll survives', RT,
     'commands_.awaiting_exit=false;commands_.awaiting_troll=false;', 'commands_.awaiting_exit=false;'),
    ('H18 the Blackthorn guard demand survives', RT,
     'blackthorn_={};dialogue_={};shop_={};shrine_={};', 'dialogue_={};shop_={};shrine_={};'),
    ('H19 the conversation session survives', RT,
     'blackthorn_={};dialogue_={};shop_={};shrine_={};', 'blackthorn_={};shop_={};shrine_={};'),
    ('H20 the shop session survives', RT,
     'blackthorn_={};dialogue_={};shop_={};shrine_={};', 'blackthorn_={};dialogue_={};shrine_={};'),
    # --- device views, timers, parked work ---
    ('H21 the gem view survives (and still owes its turn)', RT,
     'gem_view_active_=gem_view_charges_turn_=false;zodiac_view_active_=false;', 'zodiac_view_active_=false;'),
    ('H22 the zodiac view survives', RT,
     'gem_view_active_=gem_view_charges_turn_=false;zodiac_view_active_=false;',
     'gem_view_active_=gem_view_charges_turn_=false;'),
    ('H23 (Z)-stats survives', RT,
     'zodiac_view_active_=false;\n    zstats_open_=false;zstats_page_=0;', 'zodiac_view_active_=false;\n    zstats_page_=0;'),
    ('H24 the map reveal survives', RT,
     'map_reveal_end_us_=0;magic_invert_start_us_=magic_invert_end_us_=0;',
     'magic_invert_start_us_=magic_invert_end_us_=0;'),
    ('H25 the magic-ceremony inversion survives', RT,
     'magic_invert_start_us_=magic_invert_end_us_=0;quake_start_us_=0;', 'quake_start_us_=0;'),
    # (Clearing only the start time is self-healing: the next frame sees a quake
    # that began at t=0 and ends it. The mutant keeps the whole quake.)
    ('H26 the quake survives', RT, 'quake_start_us_=0;quake_pulses_=0;', ''),
    ('H27 a parked Use item survives', RT,
     'pending_combat_spell_=pending_ready_member_=pending_use_item_=pending_order_from_=-1;',
     'pending_combat_spell_=pending_ready_member_=pending_order_from_=-1;'),
    ('H28 the old picker rows stay armed', RT,
     'selection_count_=0;selection_request_=openu5::UiRequestId::None;', 'selection_count_=0;'),
    ('H29 a queued NPC approach survives', RT,
     'pending_npc_initiation_=PendingNpcInitiation::None;pending_npc_slot_', 'pending_npc_slot_'),
]


def run(cmd):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def build():
    return run([CMAKE, '--build', BUILD, '--target'] + TARGETS)


def test():
    results = []
    for t in TARGETS:
        code, out = run([os.path.join(BUILD, t + '.exe'), RES])
        reds = [l for l in out.splitlines() if l.startswith(('RED', 'FAIL', '[FAIL'))]
        results.append((t, code, reds))
    return results


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-HF4 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
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
            if code or reds:
                lines.append('    %s exit=%d  %d RED' % (t, code, len(reds)))
                lines += ['        ' + r[:200] for r in reds[:12]]
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
