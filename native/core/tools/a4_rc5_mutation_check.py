r"""Alpha 4 RC5 hotfix (ALPHA4_UI.md section 17) -- mutation check of the two RC4 hardware failures.

    python tools/a4_rc5_mutation_check.py <build-dir> [ID[,ID...]]

Each mutant edits production anchors, rebuilds the tests that pin the item and must turn at least one RED.
A mutant that does not build is INVALID, not killed. Anchors are matched in each file's own line endings
(checkout-line-endings-are-mixed); every file is restored and touched after each mutant, and the restored tree
is re-run at the end (it must be GREEN, or the whole run fails).

  H*  the healer's R (ui_session.cpp key map -> shop_orchestration.cpp -> shops.cpp)
  M*  the (U)se > In Mani Corp scroll route (alpha_runtime.cpp pickers -> world_magic.cpp)
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
BIN = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin'
SRC = 'native/core/src/'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'

TARGETS = ['a4_rc5_hotfix_runtime', 'a4_parity2_mani_not_dead', 'a4_parity2_d83_d84_resurrect', 'shop_parity_tests', 'shop_flow_tests',
           'ui_session_tests', 'ui_mode_regression', 'alpha_runtime_integration_regression', 'batch7b_tests']
TESTS = ['a4_rc5_hotfix_runtime', 'a4_parity2_mani_not_dead', 'a4_parity2_d83_d84_resurrect', 'shop_parity', 'shop_flow',
         'ui_session', 'ui_mode_regression', 'alpha_runtime_integration_regression', 'batch7b']

KEY = "case 'r': s.action=shop_type_==ShopType::Barkeeper?ShopAction::Rations:shop_type_==ShopType::Healer?ShopAction::Resurrect:ShopAction::Rest; break;"
MUTANTS = {
    # ---------------------------------------------------------------- the healer's R
    'H1': ('R is Rest again (RC4)', [(SRC + 'ui_session.cpp', KEY,
           "case 'r': s.action=shop_type_==ShopType::Barkeeper?ShopAction::Rations:ShopAction::Rest; break;")]),
    'H2': ('the healer R is Cure', [(SRC + 'ui_session.cpp', 'shop_type_==ShopType::Healer?ShopAction::Resurrect:', 'shop_type_==ShopType::Healer?ShopAction::Cure:')]),
    'H3': ('the healer R is Heal', [(SRC + 'ui_session.cpp', 'shop_type_==ShopType::Healer?ShopAction::Resurrect:', 'shop_type_==ShopType::Healer?ShopAction::Heal:')]),
    'H4': ('R is Resurrect in EVERY shop (the innkeeper loses Rest)', [(SRC + 'ui_session.cpp',
           'shop_type_==ShopType::Healer?ShopAction::Resurrect:ShopAction::Rest;', 'ShopAction::Resurrect;')]),
    'H5': ('the Barkeeper loses Rations', [(SRC + 'ui_session.cpp', 'shop_type_==ShopType::Barkeeper?ShopAction::Rations:shop_type_==ShopType::Healer',
           'shop_type_==ShopType::Healer')]),
    'H6': ('the service maps Resurrect to Cure', [(SRC + 'shop_orchestration.cpp', ': HealerService::Resurrect;\n        if (s.legacy_healer) {',
           ': HealerService::Cure;\n        if (s.legacy_healer) {')]),
    'H7': ('the healer deal skips resurrect_apply (the XP cut, level and max HP)', [(SRC + 'shops.cpp', '        resurrect_apply(c, g.karma);\n', '        c.status = \'G\';\n')]),
    'H8': ('the healer raises with a fixed karma 99 (no XP cut)', [(SRC + 'shops.cpp', 'resurrect_apply(c, g.karma);', 'resurrect_apply(c, 99);')]),
    'H9': ('the healer leaves HP at 1', [(SRC + 'shops.cpp', '        resurrect_apply(c, g.karma);\n        c.current_hp = c.max_hp;', '        resurrect_apply(c, g.karma);')]),
    'H10': ('the healer raises a LIVING member too', [(SRC + 'shop_orchestration.cpp', ": ch.status == 'D';", ': true;')]),
    'H11': ('the raise is free', [(SRC + 'shop_orchestration.cpp', 'if (paid) {\n                drain(r.ok);', 'if (false) {\n                drain(r.ok);')]),
    # ---------------------------------------------------------------- the In Mani Corp scroll route
    'M1': ('only "Failed!" (RC3)', [(SRC + 'world_magic.cpp', 'say("Not dead!");say("Failed!");', 'say("Failed!");')]),
    'M2': ('only "Not dead!"', [(SRC + 'world_magic.cpp', 'say("Not dead!");say("Failed!");', 'say("Not dead!");')]),
    'M3': ('the lines swapped', [(SRC + 'world_magic.cpp', 'say("Not dead!");say("Failed!");', 'say("Failed!");say("Not dead!");')]),
    'M4': ('the lines print on a DEAD target', [(SRC + 'world_magic.cpp', 'if(!apply_target_spell(*p,MagicEffect::Resurrect,g.karma,rand)){say("Not dead!")',
           'if(apply_target_spell(*p,MagicEffect::Resurrect,g.karma,rand)){say("Not dead!")')]),
    'M5': ('the scroll is not consumed', [(SRC + 'world_magic.cpp', 'if(g.scroll_quantities[cmd.item]>0){--g.scroll_quantities[cmd.item];}say("Scroll");',
           'say("Scroll");')]),
    'M6': ('the device skips the "Use on whom?" pick for the scroll (dispatches with no member)', [(RT,
           'if(!context_.combat&&(c.item==6||(c.item>=8&&c.item<16))){', 'if(!context_.combat&&((c.item>=8&&c.item<16))){')]),
    'M7': ('the device sends the pick to member 0 always', [(RT,
           'c.kind=openu5::CommandKind::UseItem;c.item=pending_use_item_;c.member=int16_t(i.value.index);',
           'c.kind=openu5::CommandKind::UseItem;c.item=pending_use_item_;c.member=0;')]),
    'M8': ('the SPELL form also prints "Not dead!" (spell and scroll must stay distinct)', [(SRC + 'world_magic.cpp',
           'if(auto *p=target())say(apply_target_spell(*p,fx,g.karma,rand)?"Success!":"Failed!");return {};}',
           'if(auto *p=target()){if(fx==MagicEffect::Resurrect&&p->status!=68)say("Not dead!");say(apply_target_spell(*p,fx,g.karma,rand)?"Success!":"Failed!");}return {};}')]),
}

ENV = dict(os.environ, PATH=BIN + os.pathsep + os.environ['PATH'])


def touch(path):
    now = time.time()
    os.utime(path, (now, now))


def run_native(build):
    r = subprocess.run([BIN + '/ninja.exe', '-C', build] + TARGETS, capture_output=True, text=True, env=ENV)
    if r.returncode:
        return 'INVALID', r.stdout[-800:]
    pattern = '^(' + '|'.join(TESTS) + ')$'
    r = subprocess.run([BIN + '/ctest.exe', '-R', pattern], cwd=build, capture_output=True, text=True, env=ENV)
    failed = [l.strip() for l in r.stdout.splitlines() if '(Failed)' in l or 'Failed  ' in l]
    return ('KILLED' if r.returncode else 'SURVIVED'), ' | '.join(failed[:8])


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
    ids = [m for m in MUTANTS if sel is None or m in sel]
    results = {}
    for mid in ids:
        what, edits = MUTANTS[mid]
        saved = []
        try:
            saved = apply(edits)
            verdict, detail = run_native(build)
        finally:
            for path, original in saved:
                open(path, 'wb').write(original)
                touch(path)
        results[mid] = verdict
        print(f'{mid:4} {verdict:8} {what}\n     {detail}', flush=True)
    v, d = run_native(build)
    print(f'restored tree: {"GREEN" if v == "SURVIVED" else v} {d}')
    killed = sum(x == 'KILLED' for x in results.values())
    print(f'\nA4-RC5 mutations: {killed}/{len(results)} killed' + ''.join(f'\n  {m}: {x}' for m, x in results.items() if x != 'KILLED'))
    return 0 if killed == len(results) and v == 'SURVIVED' else 1


if __name__ == '__main__':
    sys.exit(main())
