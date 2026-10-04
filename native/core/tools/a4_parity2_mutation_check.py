r"""A4-PARITY2 (ALPHA4_UI.md section 16) -- mutation check of every fix: each mutant edits one (or a few)
production anchors, rebuilds, runs the checks that pin the item and must turn at least one RED.
A native mutant that does not build is INVALID, not killed.

    python tools/a4_parity2_mutation_check.py <build-dir> [ITEM|ID[,ID...]]

ITEM is a group name (D89, D83, D85, ...); with no second argument every mutant of every group runs.
N* mutate native production (rebuilt; ctest on the group's tests), T* mutate the TypeScript reference
(vitest on the group's suites, the group's corpus generators with --check, and the live parity tests
against the unchanged native).

Anchors are written with \n and matched in each file's own line endings
(checkout-line-endings-are-mixed); every file is restored and touched after each mutant, and the restored
tree is re-run at the end (it must be GREEN, or the whole run fails).
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

# group -> what to run
GROUPS = {
    'D89': dict(
        native_targets=['a4_parity2_d89_naval_ouch', 'a4_enh2_rules', 'a4_enh1_rules', 'movement_flow_parity_tests',
                        'gameplay_driver', 'quest_driver', 'turn_parity_tests'],
        native_tests=['a4_parity2_d89_naval_ouch', 'a4_enh2_rules', 'a4_enh1_rules', 'movement_flow_parity', 'gameplay_parity',
                      'quest_parity', 'turn_parity'],
        vitest=['tests/a4-parity2-d89-naval-ouch.test.ts', 'tests/naval-live.test.ts', 'tests/cactus-ouch.test.ts',
                'tests/transport-exact.test.ts'],
        generators=[('generate-movement-flow-fixtures.ts', ['--check']), ('generate-turn-fixtures.ts', ['--check'])],
    ),
}

MUTANTS = {
    # ---------------------------------------------------------------- D-89 native
    'N1': ('D89', 'naval OUCH rolls once on the active member again', [(SRC + 'commands.cpp',
           '            party_random_damage(c.game, rand);\n            event(GameEventKind::PartyChanged); // K:2A52',
           '            { const auto d = rand(1, 8); auto i = c.game.party.active_character; if (i >= c.game.party.character_count) i = 0;\n'
           '              auto &hp = c.game.party.characters[i].current_hp; hp = uint16_t(hp > d ? hp - d : 0); }\n'
           '            event(GameEventKind::PartyChanged); // K:2A52')]),
    'N2': ('D89', 'naval OUCH does not tell the party panel', [(SRC + 'commands.cpp',
           '            event(GameEventKind::PartyChanged); // K:2A52 ends in the K:2900 party-panel redraw\n', '')]),
    'N3': ('D89', 'the helper draws BEFORE it skips a dead member', [(SRC + 'turn.cpp',
           "        if (i < g.party.character_count && g.party.characters[i].status == 'D') continue;\n"
           "        const int32_t d = rand(1,8);",
           "        const int32_t d = rand(1,8);\n"
           "        if (i < g.party.character_count && g.party.characters[i].status == 'D') continue;")]),
    'N4': ('D89', 'the helper stops at five members', [(SRC + 'turn.cpp',
           'for (int32_t i = 0; i < g.party.party_size && i < 6; ++i) {\n        if (i < g.party.character_count',
           'for (int32_t i = 0; i < g.party.party_size && i < 5; ++i) {\n        if (i < g.party.character_count')]),
    'N5': ('D89', 'God Mode gates the naval OUCH before the draws', [(SRC + 'commands.cpp',
           '            party_random_damage(c.game, rand);\n            event(GameEventKind::PartyChanged); // K:2A52',
           '            if (!party_damage_blocked(c.game)) party_random_damage(c.game, rand);\n            event(GameEventKind::PartyChanged); // K:2A52')]),
    'N6': ('D89', 'the cactus also beeps (the beep is the else branch)', [(SRC + 'commands.cpp',
           '        } else event(GameEventKind::Sfx, "move-blocked");\n    }\n    void naval_turn(',
           '        } event(GameEventKind::Sfx, "move-blocked");\n    }\n    void naval_turn(')]),
    'N7': ('D89', 'HP == damage no longer kills', [(SRC + 'turn.cpp',
           "    if (hp <= 0) { ch.status = 'D';", "    if (hp < 0) { ch.status = 'D';")]),
    'N8': ('D89', 'a dying active member stays selected', [(SRC + 'turn.cpp',
           ' if (g.party.active_character == i) g.party.active_character = 255; }', ' }')]),
    'N9': ('D89', 'the helper loops over the roster, not the party', [(SRC + 'turn.cpp',
           'for (int32_t i = 0; i < g.party.party_size && i < 6; ++i) {\n        if (i < g.party.character_count',
           'for (int32_t i = 0; i < g.party.character_count && i < 6; ++i) {\n        if (i < g.party.character_count')]),
    # ---------------------------------------------------------------- D-89 reference
    'T1': ('D89', 'reference: the naval OUCH rolls once on the active member again', [(TS + 'game.ts',
           '      partyRandomDamage(this.state, this.rand);\n      events.push({ kind: "party-changed" });\n',
           '      { const dmg = this.rand(1, 8); const active = this.state.characters[this.state.activeCharacter] ?? this.state.characters[0];\n'
           '        if (active) active.currentHp = Math.max(0, active.currentHp - dmg); }\n      events.push({ kind: "party-changed" });\n')]),
    'T2': ('D89', 'reference: the naval OUCH does not tell the party panel', [(TS + 'game.ts',
           '      partyRandomDamage(this.state, this.rand);\n      events.push({ kind: "party-changed" });\n',
           '      partyRandomDamage(this.state, this.rand);\n')]),
    'T3': ('D89', 'reference: the helper draws before it skips the dead', [(TS + 'world/survival.ts',
           '    if (state.characters[i]?.status === "D") continue;\n    applyDamage(state, i, rand(1, 8));',
           '    const d = rand(1, 8);\n    if (state.characters[i]?.status === "D") continue;\n    applyDamage(state, i, d);')]),
    'T4': ('D89', 'reference: the helper stops at five members', [(TS + 'world/survival.ts',
           'for (let i = 0; i < state.partySize && i < 6; i++) {\n    if (state.characters[i]?.status === "D") continue;',
           'for (let i = 0; i < state.partySize && i < 5; i++) {\n    if (state.characters[i]?.status === "D") continue;')]),
    'T5': ('D89', 'reference: HP == damage no longer kills', [(TS + 'world/survival.ts',
           '  ch.currentHp -= amount;\n  if (ch.currentHp <= 0) {', '  ch.currentHp -= amount;\n  if (ch.currentHp < 0) {')]),
}

ENV = dict(os.environ, PATH=BIN + os.pathsep + os.environ['PATH'])


def touch(path):
    now = time.time()
    os.utime(path, (now, now))


def run_native(build, group):
    g = GROUPS[group]
    r = subprocess.run([BIN + '/ninja.exe', '-C', build] + g['native_targets'], capture_output=True, text=True, env=ENV)
    if r.returncode:
        return 'INVALID', r.stdout[-800:]
    pattern = '^(' + '|'.join(g['native_tests']) + ')$'
    r = subprocess.run([BIN + '/ctest.exe', '-R', pattern], cwd=build, capture_output=True, text=True, env=ENV)
    failed = [l.strip() for l in r.stdout.splitlines() if '(Failed)' in l or 'Failed  ' in l]
    return ('KILLED' if r.returncode else 'SURVIVED'), ' | '.join(failed[:8])


def run_ts(build, group):
    g = GROUPS[group]
    failed = []
    r = subprocess.run(['npx.cmd', 'vitest', 'run'] + g['vitest'], cwd=os.path.join(ROOT, 'game'),
                       capture_output=True, text=True, errors='replace')
    if r.returncode:
        failed.append('vitest')
    for gen, args in g['generators']:
        r = subprocess.run([NODE, '--import', 'tsx', 'native/core/tools/' + gen] + args, cwd=ROOT,
                           capture_output=True, text=True, errors='replace')
        if r.returncode:
            failed.append(gen + ' ' + ' '.join(args))
    r = subprocess.run([BIN + '/ctest.exe', '-R', '^(gameplay_parity|quest_parity)$'], cwd=build,
                       capture_output=True, text=True, env=ENV)
    if r.returncode:
        failed += [l.strip() for l in r.stdout.splitlines() if '(Failed)' in l]
    return ('KILLED' if failed else 'SURVIVED'), ' | '.join(failed)


def apply(edits):
    saved = []
    for rel, old, new in edits:
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        o, n = (old.replace('\n', '\r\n'), new.replace('\n', '\r\n')) if '\r\n' in text else (old, new)
        assert text.count(o) == 1, (rel, text.count(o), old)
        saved.append((path, original))
        open(path, 'wb').write(text.replace(o, n, 1).encode('utf-8'))
        touch(path)
    return saved


def main():
    build = os.path.abspath(sys.argv[1])
    sel = sys.argv[2].split(',') if len(sys.argv) > 2 else None
    ids = [m for m, v in MUTANTS.items() if sel is None or m in sel or v[0] in sel]
    results = {}
    for mid in ids:
        group, what, edits = MUTANTS[mid]
        saved = []
        try:
            saved = apply(edits)
            verdict, detail = (run_ts if mid.startswith('T') else run_native)(build, group)
        finally:
            for path, original in saved:
                open(path, 'wb').write(original)
                touch(path)
        results[mid] = verdict
        print(f'{mid:4} {verdict:8} [{group}] {what}\n     {detail}', flush=True)
    clean = True
    for group in sorted({MUTANTS[m][0] for m in ids}):
        v1, d1 = run_native(build, group)
        v2, d2 = run_ts(build, group)
        print(f'restored tree [{group}]: native {"GREEN" if v1 == "SURVIVED" else v1} {d1}; reference {"GREEN" if v2 == "SURVIVED" else v2} {d2}')
        clean = clean and v1 == v2 == 'SURVIVED'
    killed = sum(v == 'KILLED' for v in results.values())
    print(f'\nA4-PARITY2 mutations: {killed}/{len(results)} killed')
    return 0 if killed == len(results) and clean else 1


if __name__ == '__main__':
    sys.exit(main())
