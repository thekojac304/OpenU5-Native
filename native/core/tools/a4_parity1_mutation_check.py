r"""A4-PARITY1 (ALPHA4_UI.md section 9) -- mutation check of every fix: each mutant edits one
production anchor, rebuilds, runs the checks that pin it and must turn at least one RED.
A native mutant that does not build is INVALID, not killed.

    python tools/a4_parity1_mutation_check.py <build-dir> [N1,T3,...]

N* mutate native production (rebuilt; ctest on the parity / runtime / ELF-list tests).
T* mutate the TypeScript reference (vitest on the touched suites, the corpus generators
   with --check, and the live gameplay / quest parity against the unchanged native).
Anchors are written with \n and matched in each file's own line endings
(checkout-line-endings-are-mixed); every file is restored and touched after each mutant.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
BIN = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin'
NODE = 'C:/Program Files/nodejs/node.exe'
SRC = 'native/core/src/'
TS = 'game/src/core/'
NATIVE_TARGETS = ['magic_parity_tests', 'advanced_combat_parity_tests', 'combat_parity_tests', 'ui_session_tests',
                  'a4_parity1_runtime', 'gameplay_driver', 'quest_driver', 'a3_04b_perf']
NATIVE_TESTS = ['magic_parity', 'advanced_combat_parity', 'combat_parity', 'combat_negate_parity', 'ui_session',
                'a4_parity1_runtime', 'gameplay_parity', 'quest_parity', 'a3_04b_perf']

MUTANTS = {
    # ---- NEW-1: the palace gate reads possession ----
    'N1': ('the out-of-combat gate reads the worn flag again', SRC + 'magic.cpp',
           '((ctx.location == 18 && !g.quest.artifacts[1]) || ctx.location == 29);',
           '((ctx.location == 18 && !g.worn_crown) || ctx.location == 29);'),
    'N2': ('the combat gate reads the worn flag again', SRC + 'combat.cpp',
           '(c.game.position.map.location == 18 && !c.game.quest.artifacts[1])',
           '(c.game.position.map.location == 18 && !c.game.worn_crown)'),
    'N3': ('the palace absorbs whatever the party carries', SRC + 'magic.cpp',
           '((ctx.location == 18 && !g.quest.artifacts[1]) || ctx.location == 29);',
           '((ctx.location == 18) || ctx.location == 29);'),
    # ---- P1b: Use crown writes time spell 0x1c ----
    'N4': ('Use crown toggles the invented worn flag again', SRC + 'quest_world.cpp',
           'if(id==18 || id==36 || id==19){',
           'if(id==19){message("Crown");c.game.worn_crown=!c.game.worn_crown;message(c.game.worn_crown?'
           '"Thou dost don the Crown of Lord British...":"Removed!");return CommandStatus::Success;}\n'
           '    if(id==18 || id==36){'),
    'N5': ('the worn crown is not permanent (20 turns)', SRC + 'quest_world.cpp',
           'c.turn.spell_turns=255;message(id==18', 'c.turn.spell_turns=id==19?20:255;message(id==18'),
    'N6': ("the crown writes the amulet's value 0x0e", SRC + 'quest_world.cpp',
           r"id==19?'\x1c':'\x1d'", r"id==19?'\x0e':'\x1d'"),
    # ---- P1c: Negate / worn crown against enemy magic ----
    'N7': ('the magic-projectile gate removed (COMBAT 0x0185)', SRC + 'combat.cpp',
           'if ((a.enemy->abilities & 0x80) && negates_enemy_magic(c.turn.time_spell))',
           'if (false && (a.enemy->abilities & 0x80) && negates_enemy_magic(c.turn.time_spell))'),
    'N8': ("the projectile gate before the 50 % roll (the roll's draw skipped)", SRC + 'combat.cpp',
           'if (a.enemy->index != 26 && rand(0, 255) >= 128)',
           'if (((a.enemy->abilities & 0x80) && negates_enemy_magic(c.turn.time_spell)) || '
           '(a.enemy->index != 26 && rand(0, 255) >= 128))'),
    'N9': ('the teleport gate removed (COMBAT 0x0f27)', SRC + 'combat.cpp',
           '&& !negates_enemy_magic(c.turn.time_spell)) {', ') {'),
    'N10': ('the special-ability gate removed (COMSUBS 0x0112)', SRC + 'combat_magic.inc',
            'if (negates_enemy_magic(c.turn.time_spell))', 'if (false && negates_enemy_magic(c.turn.time_spell))'),
    'N11': ('only Negate gates, not the worn crown', SRC + 'combat.cpp',
            r"return time_spell == 'N' || time_spell == '\x1c';", "return time_spell == 'N';"),
    'N12': ('only the worn crown gates, not Negate', SRC + 'combat.cpp',
            r"return time_spell == 'N' || time_spell == '\x1c';", r"return time_spell == '\x1c';"),
    # ---- NEW-2: the dungeon's command keys ----
    'N13': ('dungeon M opens Cast again (the A-7 alias)', SRC + 'ui_session.cpp',
            'Mix and NewOrder underground.\n    case \'m\': { command_echo("Mix Reagents");command_echo("");UiIntent i; '
            'i.kind=UiIntentKind::OpenSpellSelection; i.request=UiRequestId::Custom;',
            'Mix and NewOrder underground.\n    case \'m\': { command_echo("Cast...");UiIntent i; '
            'i.kind=UiIntentKind::OpenSpellSelection; i.request=UiRequestId::Spell;'),
    'N14': ('dungeon N falls back to "What?"', SRC + 'ui_session.cpp',
            "    case 'n': { UiIntent i; i.kind=UiIntentKind::OpenPartySelection; i.request=UiRequestId::Party; "
            "dispatch(i); return true; }\n    // The keys the kernel",
            '    // The keys the kernel'),
    'N15': ("B refuses without the turn", SRC + 'ui_session.cpp',
            '              c.item=int16_t(DungeonAction::Tick); break;', '              return true;'),
    'N16': ('P refuses with a turn', SRC + 'ui_session.cpp',
            'append(UiTextChannel::Message, "Not here!"); return true; // 0x3378',
            'append(UiTextChannel::Message, "Not here!"); c.item=int16_t(DungeonAction::Tick); break; // 0x3378'),
    'N17': ("T's string loses its '!'", SRC + 'ui_session.cpp',
            'command_echo("Talk-Funny, no response!");', 'command_echo("Talk-Funny, no response");'),
    'N18': ('the core refuses Yell underground again ("What?")', SRC + 'commands.cpp',
            'if(!c.combat&&c.dungeon&&cmd.kind==CommandKind::Yell){',
            'if(false&&!c.combat&&c.dungeon&&cmd.kind==CommandKind::Yell){'),
    'N19': ("X's string capitalised ('what?' is DS 0x4368's lower case)", SRC + 'ui_session.cpp',
            'command_echo("X-it what?");', 'command_echo("X-it What?");'),
    'N20': ('F refuses without the turn', SRC + 'ui_session.cpp',
            'command_echo("Fire-What?"); c.item=int16_t(DungeonAction::Tick); break;',
            'command_echo("Fire-What?"); return true;'),
    'N21': ("E's string capitalised", SRC + 'ui_session.cpp',
            'command_echo("Enter what?");', 'command_echo("Enter What?");'),
    # ---- D-50: the well's stristr ----
    'N22': ('the wish matches case-sensitively again', SRC + 'look.cpp',
            'auto fold = [](char16_t c) { int b = c & 0x7f; return b > 0x60 ? (b & 0x5f) : b; };',
            'auto fold = [](char16_t c) { return int(c); };'),
    'N23': ("a plain substring search (0x6f1e's skip after a partial match dropped)", SRC + 'look.cpp',
            'start += matched + 1;', 'start += 1;'),
    # ---- NEW-3: the IRAM placement ----
    'N24': ('speaker_segment_frames left in flash', 'native/targets/tdeck/main/audio_iram.lf',
            '    sfx_synth:_ZN6openu522speaker_segment_framesERKNS_14SpeakerSegmentE (noflash)\n', ''),
    # ---- the TypeScript reference ----
    'T1': ('reference: the out-of-combat gate reads wornCrown', TS + 'magic/cast.ts',
           '!!state.lbArtifacts?.crown)', '!!state.wornCrown)'),
    'T2': ('reference: Use crown sets the worn flag, not 0x1c', TS + 'endgame/use-tools.ts',
           'ctx.state.timeSpell = TIME_SPELL_CROWN; // set_time_spell(0x1c, 0xff, efecto 9)',
           'ctx.state.wornCrown = true; // mutant'),
    'T3': ('reference: only Negate gates enemy magic', TS + 'combat/combat.ts',
           'return timeSpell === "N" || timeSpell === TIME_SPELL_CROWN;', 'return timeSpell === "N";'),
    'T4': ('reference: the magic-projectile gate removed', TS + 'combat/combat.ts',
           'if (def?.abilities.rangedMagic && negatesEnemyMagic(this.opts.state.timeSpell)) {', 'if (false) {'),
    'T5': ('reference: the special-ability gate removed', TS + 'combat/combat.ts',
           'if (negatesEnemyMagic(this.opts.state.timeSpell)) return null;', ''),
    'T6': ('reference: the teleport gate removed', TS + 'combat/combat.ts',
           '&& !negatesEnemyMagic(this.opts.state.timeSpell)) {', ') {'),
    'T7': ('reference: the wish matches case-sensitively', TS + 'world/wishingwell.ts',
           'WISH_HORSE_WORDS.some((w) => kernelStristr(wish, w) >= 0)', 'WISH_HORSE_WORDS.some((w) => wish.includes(w))'),
}

ENV = dict(os.environ, PATH=BIN + os.pathsep + os.environ['PATH'])


def touch(path):
    now = time.time()
    os.utime(path, (now, now))


def run_native(build):
    r = subprocess.run([BIN + '/ninja.exe', '-C', build] + NATIVE_TARGETS, capture_output=True, text=True, env=ENV)
    if r.returncode:
        return 'INVALID', r.stdout[-800:]
    pattern = '^(' + '|'.join(NATIVE_TESTS) + ')$'
    r = subprocess.run([BIN + '/ctest.exe', '-R', pattern], cwd=build, capture_output=True, text=True, env=ENV)
    failed = [l.strip() for l in r.stdout.splitlines() if '(Failed)' in l or 'Failed  ' in l]
    return ('KILLED' if r.returncode else 'SURVIVED'), ' | '.join(failed[:8])


def run_ts(build):
    failed = []
    r = subprocess.run(['npx.cmd', 'vitest', 'run', 'tests/cast-absorbed-gate.test.ts', 'tests/use-tools.test.ts',
                        'tests/shrines.test.ts', 'tests/combat-spells.test.ts'], cwd=os.path.join(ROOT, 'game'),
                       capture_output=True, text=True, errors='replace')
    if r.returncode:
        failed.append('vitest')
    for gen, args in [('generate-magic-fixtures.ts', ['--check']), ('generate-advanced-combat-fixtures.ts', ['--check']),
                      ('generate-combat-fixtures.ts', ['--check']), ('generate-combat-fixtures.ts', ['--negate', '--check'])]:
        r = subprocess.run([NODE, '--import', 'tsx', 'native/core/tools/' + gen] + args, cwd=ROOT,
                           capture_output=True, text=True, errors='replace')
        if r.returncode:
            failed.append(gen + ' ' + ' '.join(args))
    r = subprocess.run([BIN + '/ctest.exe', '-R', '^(gameplay_parity|quest_parity)$'], cwd=build,
                       capture_output=True, text=True, env=ENV)
    if r.returncode:
        failed += [l.strip() for l in r.stdout.splitlines() if '(Failed)' in l]
    return ('KILLED' if failed else 'SURVIVED'), ' | '.join(failed)


def main():
    build = os.path.abspath(sys.argv[1])
    only = sys.argv[2].split(',') if len(sys.argv) > 2 else list(MUTANTS)
    results = {}
    for mid in only:
        what, rel, old, new = MUTANTS[mid]
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        if '\r\n' in text:
            old, new = old.replace('\n', '\r\n'), new.replace('\n', '\r\n')
        assert text.count(old) == 1, (mid, text.count(old), old)
        try:
            open(path, 'wb').write(text.replace(old, new, 1).encode('utf-8'))
            touch(path)
            verdict, detail = (run_ts if mid.startswith('T') else run_native)(build)
        finally:
            open(path, 'wb').write(original)
            touch(path)
        results[mid] = verdict
        print(f'{mid:4} {verdict:8} {what}\n     {detail}', flush=True)
    v1, d1 = run_native(build)
    v2, d2 = run_ts(build)
    print(f'restored tree: native {"GREEN" if v1 == "SURVIVED" else v1} {d1}; reference {"GREEN" if v2 == "SURVIVED" else v2} {d2}')
    killed = sum(v == 'KILLED' for v in results.values())
    print(f'\nA4-PARITY1 mutations: {killed}/{len(results)} killed')
    return 0 if killed == len(results) and v1 == v2 == 'SURVIVED' else 1


if __name__ == '__main__':
    sys.exit(main())
