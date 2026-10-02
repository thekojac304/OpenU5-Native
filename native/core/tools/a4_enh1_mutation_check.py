"""Alpha 4 A4-ENH1 -- mutation check of the enhancement pass (ALPHA4_UI.md section 10).

    python tools/a4_enh1_mutation_check.py <build-dir> [ids,comma,separated] [--anchors]

As tools/a4_ui3_mutation_check.py: each mutant is one edit of production code
(anchors converted to the file's own line endings); the driver applies it,
touches the file, builds the named test targets, runs them, and restores +
touches the file whatever happens. KILLED = a named test exits non-zero,
INVALID = it does not build. --anchors only checks every anchor matches once.
The PATH must hold the host toolchain.

Every mutant is a defect class the brief names: the click producing a key or a
double toggle, the trackball's tiny-roll / gap / idle / reversal rules, the
speed level not persisting, the Developer row coming back or Alt+D failing,
God Mode leaking damage, the cheat guards, the save key leaking into an
Original save or missing from the sidecar whitelist, and every difficulty hook
-- including a non-identity Original row, which must turn the pre-A4-ENH1
preservation goldens RED.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
NINJA = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/ninja.exe'
PACK = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
GAM = os.path.join(ROOT, 'game/assets/init.gam')
OOL = os.path.join(ROOT, 'game/assets/init.ool')
MISC = os.path.join(ROOT, 'original/u5/ultima5/MISCMAPS.DAT')
ADAPTER = 'native/targets/tdeck/main/ui_input_adapter.cpp'
CONTROLLER = 'native/targets/tdeck/main/input_controller.cpp'
RUNTIME = 'native/targets/tdeck/main/alpha_runtime.cpp'
SETTINGS = 'native/core/src/frontend_settings.cpp'
FRONTEND = 'native/core/src/frontend.cpp'
MENU = 'native/core/src/system_menu.cpp'
ENH = 'native/core/src/enhanced.cpp'
TURN = 'native/core/src/turn.cpp'
COMBAT = 'native/core/src/combat.cpp'
COMMANDS = 'native/core/src/commands.cpp'
SAVE = 'native/core/src/save_core.cpp'
PERSIST = 'native/core/src/persistence.cpp'
TESTS = {
    'input': ('input_regression', []),
    'runtime': ('a4_enh1_runtime', [PACK]),
    'rules': ('a4_enh1_rules', [GAM]),
    'gold': ('a4_enh1_preservation', []),
    'gold_rt': ('a4_enh1_preservation_runtime', [PACK]),
    'frontend': ('frontend_tests', [GAM, OOL, MISC]),
}

# (id, what, file, anchor, replacement, tests)
MUTANTS = [
    # --- the trackball click -------------------------------------------------
    ('K1', 'the click is not handled (falls through as an event of no meaning)', ADAPTER,
     'if (raw.kind == RawInputKind::TrackballClick) {', 'if (false) {', ['input', 'runtime']),
    ('K2', 'contact bounce re-toggles (no bounce / repeat window, press-down latch ignored)', CONTROLLER,
     'const bool bounce = down_ || (released_once_ && now_us - last_release_us_ < kBounceUs) ||\n                        (accepted_once_ && now_us - last_accept_us_ < kRepeatUs);',
     'const bool bounce = false;', ['input', 'runtime']),
    ('K3', 'the toggle is not handled before the screens route it (the System Menu takes a Confirm)', RUNTIME,
     '    if(shortcut==DeviceShortcut::MovementModeToggled)\n        return announce_movement_mode(raw.kind==RawInputKind::TrackballClick?"trackball-click":"mic-hold");',
     '', ['runtime']),
    ('K4', 'no feedback line', RUNTIME,
     'if(!frontend_.active()&&ui_)ui_->append(openu5::UiTextChannel::System,on?"WASD Mode: ON":"WASD Mode: OFF");',
     '', ['runtime']),
    ('K5', 'no click guard: the ball rocking under the finger moves the party', CONTROLLER,
     '    click_guard_until_us_ = raw.timestamp_us + kClickGuardUs;', '    click_guard_until_us_ = raw.timestamp_us;',
     ['input']),
    # --- the trackball speed -------------------------------------------------
    ('T1', 'every pulse is a step again (no accumulation)', CONTROLLER,
     '    if (acc_[axis] * sign < int32_t(tune.pulses_per_step)) {', '    if (acc_[axis] * sign < 1) {',
     ['input', 'runtime']),
    ('T2', 'no step gap: roll is banked and plays out', CONTROLLER,
     '    if (tune.step_gap_ms && step_seen_ && now - last_step_us_ < int64_t(tune.step_gap_ms) * 1000) {',
     '    if (false) {', ['input', 'runtime']),
    ('T3', 'no idle reset: tiny rolls add up across pauses', CONTROLLER,
     '    if (acc_[axis] && axis_seen_[axis] && now - last_axis_us_[axis] >= kIdleResetUs) {',
     '    if (false) {', ['input', 'runtime']),
    ('T4', 'no reversal reset: the old way must unwind first', CONTROLLER,
     '    if (acc_[axis] * sign < 0) {\n        acc_[axis] = 0; // the other way', '    if (false) {\n        acc_[axis] = 0; // the other way',
     ['input']),
    ('T5', 'the speed key is never read back (always the default)', SETTINGS,
     'if(j.has("trackballSpeed")&&!integer("trackballSpeed",kTrackballSpeedMin,kTrackballSpeedMax,speed))speed=kTrackballSpeedDefault;',
     'speed=kTrackballSpeedDefault;', ['frontend', 'runtime']),
    ('T6', 'the default level counts 2 pulses a step, not 3', CONTROLLER,
     '{4, 130, 4}, {3, 100, 4},', '{4, 130, 4}, {2, 100, 4},', ['input', 'runtime']),
    # --- the Developer menu --------------------------------------------------
    ('D1', 'the System Menu root shows a Developer row again', MENU,
     'for(auto s:{"Resume","Save Game","Load Game","Settings","Difficulty","Cheats","Return to Title"})v.lines[v.line_count++]=s;',
     'for(auto s:{"Resume","Save Game","Load Game","Settings","Difficulty","Cheats","Developer","Return to Title"})v.lines[v.line_count++]=s;',
     ['runtime', 'frontend']),
    ('D2', 'Alt+D on the title does nothing', FRONTEND,
     '    if(!developer_build_)return false;\n    if(state_!=FrontendState::Title',
     '    return false;\n    if(state_!=FrontendState::Title', ['runtime', 'frontend']),
    # --- the cheats ----------------------------------------------------------
    ('C1', 'God Mode does not cover apply_damage (poison, starvation, hazards, traps)', TURN,
     '    if (party_damage_blocked(g)) return; // A4-ENH1 God Mode (the draws that sized it already happened)', '',
     ['rules', 'runtime']),
    ('C2', 'God Mode does not cover combat', COMBAT,
     '        } else if (party_damage_blocked(g))\n            d = 0;', '        } else if (false)\n            d = 0;',
     ['rules']),
    ('C3', 'God Mode does not cover the chest trap', 'native/core/src/loot.cpp',
     '        if (party_damage_blocked(g)) return; // A4-ENH1 God Mode', '', ['rules']),
    ('C4', 'Heal is applied in combat (the arena then overwrites it)', ENH,
     '        if (in_combat) {', '        if (in_combat && false) {', ['rules', 'runtime']),
    ('C5', 'gold passes the 9999 cap', ENH,
     '        const int32_t after = std::min<int32_t>(kGoldCap, before + add);', '        const int32_t after = before + add;',
     ['rules']),
    ('C6', 'Max Gold can lower gold a preset left above the cap', ENH,
     '        if (before >= kGoldCap) {', '        if (false) {', ['rules']),
    ('C7', 'no cheats-used bit is recorded', ENH,
     '    g.enhanced.cheats_used |= cheat_bit(kind);', '', ['rules', 'runtime']),
    ('C8', 'the "enhanced" key is not on the sidecar whitelist (lost at every save)', PERSIST,
     '    "enhanced"};', '    };', ['rules', 'runtime']),
    ('C9', 'an Original journey with no cheat writes the key anyway', SAVE,
     '    if (enhanced_is_default(g.enhanced))\n        s.erase("enhanced");\n    else {',
     '    {', ['gold', 'rules', 'runtime']),
    ('C10', 'a load keeps no God Mode', SAVE,
     '        g.enhanced.god_mode = e["godMode"].kind == J::Bool && e["godMode"].truth();', '', ['rules', 'runtime']),
    ('C11', 'the Cheats page Enter applies nothing', MENU,
     'else if(a.kind==UiActionKind::Confirm){pending_.kind=SystemMenuIntentKind::Cheat;',
     'else if(false){pending_.kind=SystemMenuIntentKind::Cheat;', ['runtime']),
    # --- the difficulty ------------------------------------------------------
    ('R1', 'Original is not the identity: XP 101 %', ENH,
     '    {100, 100, 100, 1, 100, 100}, // Original', '    {100, 100, 101, 1, 100, 100}, // Original',
     ['gold', 'rules']),
    ('R2', 'Original is not the identity: poison every 2nd turn', ENH,
     '    {100, 100, 100, 1, 100, 100}, // Original', '    {100, 100, 100, 2, 100, 100}, // Original',
     ['gold', 'gold_rt', 'rules']),
    ('R3', 'Original is not the identity: enemies hit for 99 %', ENH,
     '    {100, 100, 100, 1, 100, 100}, // Original', '    {99, 100, 100, 1, 100, 100}, // Original',
     ['gold', 'rules']),
    ('R4', 'Original is not the identity: 99 % of the meals', ENH,
     '    {100, 100, 100, 1, 100, 100}, // Original', '    {100, 100, 100, 1, 99, 100}, // Original',
     ['gold', 'rules']),
    ('R5', 'Original is not the identity: 99 % of the spawns', ENH,
     '    {100, 100, 100, 1, 100, 100}, // Original', '    {100, 100, 100, 1, 100, 99}, // Original',
     ['gold_rt', 'rules']),
    ('R6', 'the XP hook is not applied', COMBAT,
     '                int xp = rules_xp_award(g, (d.hp >> 2) + 1);', '                int xp = (d.hp >> 2) + 1;',
     ['rules']),
    ('R7', 'the XP is scaled twice (once more on the tally)', COMBAT,
     '                s.xp[a.member] += xp;', '                s.xp[a.member] += xp;\n                r.exp = uint16_t(std::min<int32_t>(9999, int(r.exp) + rules_xp_award(g, xp) - xp));',
     ['rules']),
    ('R8', 'the incoming-damage hook is not applied', COMBAT,
     '        else if (!party_side(a))\n            d = rules_incoming_damage(g, d);', '        else if (false)\n            d = rules_incoming_damage(g, d);',
     ['rules']),
    ('R9', 'the poison hook is not applied', TURN,
     "        if (status == 'P' && rules_poison_due(g)) {", "        if (status == 'P') {", ['rules', 'runtime']),
    ('R10', 'the hunger hook is not applied', TURN,
     '        else if ((g.time.hour == 6 || g.time.hour == 12 || g.time.hour == 18) && rules_meal_due(g)) // A4-ENH1 hunger',
     '        else if ((g.time.hour == 6 || g.time.hour == 12 || g.time.hour == 18)) // A4-ENH1 hunger', ['rules']),
    ('R11', 'the encounter hook is not applied in the live world() turn', COMMANDS,
     'spawn.spawn && rules_encounter_allowed(c.game)', 'spawn.spawn', ['runtime']),
    ('R12', 'the meal thinning never skips a meal', ENH,
     '    return (meal + 1) * p / 100 > meal * p / 100;', '    return (meal + 1) * p / 100 >= meal * p / 100;',
     ['rules']),
    # (A "never scale a hit to 0" mutant is equivalent with today's table: no
    # preset is below 50 %, so (1 x pct + 50) / 100 >= 1 already. The clamp is
    # defensive, for future tuning; section 10 notes it.)
    ('R14', 'the difficulty is not part of "is default" (an Easy journey saves no key)', ENH,
     '    return e.difficulty == Difficulty::Original && !e.god_mode && !e.cheats_used;',
     '    return !e.god_mode && !e.cheats_used;', ['rules', 'runtime']),
    ('R15', 'a load ignores the saved difficulty', SAVE,
     '                g.enhanced.difficulty = Difficulty(i);', '                (void)i;', ['rules', 'runtime']),
    ('R16', 'the Difficulty page Enter applies nothing', MENU,
     'else if(a.kind==UiActionKind::Confirm){pending_.kind=SystemMenuIntentKind::SetDifficulty;',
     'else if(false){pending_.kind=SystemMenuIntentKind::SetDifficulty;', ['runtime']),
]


def touch(p):
    t = time.time()
    os.utime(p, (t, t))


def build(build_dir, targets):
    r = subprocess.run([NINJA, '-C', build_dir] + targets, capture_output=True, text=True)
    return r.returncode == 0, r.stdout[-1500:]


def run(build_dir, key):
    target, args = TESTS[key]
    r = subprocess.run([os.path.join(build_dir, target + '.exe')] + args, capture_output=True, text=True,
                       timeout=900, cwd=ROOT)
    reds = []
    for l in r.stdout.splitlines():
        w = l.split()
        if w and w[0] == 'RED' and len(w) > 1:
            reds.append(w[1])
    return r.returncode, reds


def main():
    build_dir = os.path.abspath(sys.argv[1])
    args = [a for a in sys.argv[2:] if not a.startswith('--')]
    only = set(args[0].split(',')) if args else None
    anchors_only = '--anchors' in sys.argv
    killed = invalid = survived = 0
    for mid, what, rel, anchor, repl, keys in MUTANTS:
        if only and mid not in only:
            continue
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        found = None
        for eol in ('\r\n', '\n'):
            a = anchor.replace('\r\n', '\n').replace('\n', eol)
            if text.count(a) == 1:
                found = (a, repl.replace('\r\n', '\n').replace('\n', eol))
                break
        if not found or found[0] == found[1]:
            print(f'{mid} ANCHOR-MISSING or no-op -- {what}', flush=True)
            invalid += 1
            continue
        if anchors_only:
            print(f'{mid} anchor ok -- {what}', flush=True)
            continue
        try:
            open(path, 'wb').write(text.replace(found[0], found[1]).encode('utf-8'))
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
    if anchors_only:
        return
    ok, _ = build(build_dir, [t for t, _ in TESTS.values()])
    green = ok and all(run(build_dir, k)[0] == 0 for k in TESTS)
    print(f'A4-ENH1 mutation check: {killed} killed, {survived} survived, {invalid} invalid; '
          f'restored build {"GREEN" if green else "RED"}')


if __name__ == '__main__':
    main()
