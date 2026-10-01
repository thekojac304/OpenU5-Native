"""Alpha 4 A4-SAVE2 -- mutation check of the manual save slots.

    python tools/a4_save2_mutation_check.py <build-dir> [ids,comma,separated]

Each mutant is one edit of production code (anchors converted to the file's
own line endings). The driver applies it, touches the file, builds the named
test target(s), runs them, and restores + touches the file whatever happens
(a restored file older than the mutated object would leave the mutant in the
build). A mutant is KILLED when a named test exits non-zero, INVALID when it
does not build. The PATH must hold the host toolchain (w64devkit) for the
compiler's own subprocesses.

The classes the A4-SAVE2 brief names: wrong-slot selection (S1-S4, S19, S23),
a destroyed fallback generation (S5, S7), migration choosing the wrong source
(S17, S18), overwrite-No modifying the slot (S12-S14), an invalid generation
accepted (S8, S11), plus Continue order, the list's cost and New Journey.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
NINJA = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/ninja.exe'
PACK = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
SAVE = 'native/targets/tdeck/main/alpha_save.cpp'
GEN = 'native/targets/tdeck/main/alpha_save_generation.cpp'
RUNTIME = 'native/targets/tdeck/main/alpha_runtime.cpp'
FRONTEND = 'native/core/src/frontend.cpp'
MENU = 'native/core/src/system_menu.cpp'
TESTS = {
    'slots': ('a4_save2_slots_runtime', [PACK]),
    'save1': ('a4_save1_recovery_runtime', [PACK]),
    'menu': ('a4_ui2_save_menu_runtime', [PACK]),
    'storage': ('a3_04g_storage_runtime', [PACK]),
    # A4-UI3: the real card behind the System Menu (S25's case moved there).
    'ux': ('a4_ui3_save_ux_runtime', [PACK, os.path.join(ROOT, 'original/u5/ultima5')]),
    'frontend': ('frontend_tests', [os.path.join(ROOT, 'game/assets/init.gam'), os.path.join(ROOT, 'game/assets/init.ool'),
                              os.path.join(ROOT, 'original/u5/ultima5/MISCMAPS.DAT')]),
}

# (id, what, file, anchor, replacement, tests)
MUTANTS = [
    ('S1', 'wrong slot: every slot writes the Slot 1 (legacy) names', SAVE,
     'if(slot==0)std::snprintf(out,sizeof(out),"%s/alpha1-g%d.%s%s"',
     'if(slot>=0)std::snprintf(out,sizeof(out),"%s/alpha1-g%d.%s%s"', ['slots']),
    ('S2', 'wrong slot: Slots 2/3 named one off (s3/s4)', SAVE,
     '"%s/alpha1-s%d-g%d.%s%s",AlphaSaveService::kDirectory,slot+1,',
     '"%s/alpha1-s%d-g%d.%s%s",AlphaSaveService::kDirectory,slot+2,', ['slots']),
    ('S3', 'wrong slot: save ignores the slot asked for (the newest slot instead)', SAVE,
     'int logical=requested_slot;', 'int logical=-1;', ['slots']),
    ('S4', 'no card-wide sequence (a pair counts from its own newest)', SAVE,
     'choose_save_target(present,s.commits[logical],newest_ok,card_newest);',
     'choose_save_target(present,s.commits[logical],newest_ok);', ['slots']),
    ('S5', 'destroyed fallback: a refused newest keeps itself and overwrites the older', GEN,
     'else{t.slot=newest;t.keep=present[1-newest]?1-newest:-1;}',
     'else{t.slot=1-newest;t.keep=newest;}', ['slots', 'save1']),
    ('S6', 'Continue takes the OLDEST slot first', SAVE,
     'while(at>0&&newest[order[at-1]]<newest[k])', 'while(at>0&&newest[order[at-1]]>newest[k])', ['slots']),
    ('S7', 'destroyed fallback: a slot load never falls back to its older generation', GEN,
     'for(int pass=0;pass<2;++pass){const int selected=openu5::save::select_generation(list,2);',
     'for(int pass=0;pass<1;++pass){const int selected=openu5::save::select_generation(list,2);', ['slots']),
    ('S8', 'invalid generation accepted: the list never runs the gate', SAVE,
     'const bool valid=read&&verify_candidate(v,s.stage);\r\n        if(valid){summary=',
     'const bool valid=read&&(verify_candidate(v,s.stage)||true);\r\n        if(valid){summary=', ['slots']),
    ('S9', 'the list never looks at the generation before a refused newest', SAVE,
     'if(present[older]&&probe(k,older,e.shown))', 'if(false&&probe(k,older,e.shown))', ['slots', 'menu']),
    ('S10', 'the list stages the older generation too (the slow-SD cost)', SAVE,
     'const int first=probe(k,newest,e.shown);',
     'if(present[1-newest]){openu5::FrontendSaveSlot ignored;probe(k,1-newest,ignored);}const int first=probe(k,newest,e.shown);',
     ['slots']),
    ('S11', 'invalid generation accepted: a semantically refused generation passes verify', GEN,
     '    return stage_generation(v,s);\r\n}', '    stage_generation(v,s);return true;\r\n}', ['slots']),
    ('S12', 'overwrite question starts on Yes', MENU,
     'else{confirm_slot_=int8_t(cursor_);page_=Page::Confirm;cursor_=0;}',
     'else{confirm_slot_=int8_t(cursor_);page_=Page::Confirm;cursor_=1;}', ['slots', 'menu']),
    ('S13', 'overwrite No saves anyway', MENU,
     'if(cursor_==1){pending_.kind=SystemMenuIntentKind::Save;pending_.slot=confirm_slot_;',
     'if(true){pending_.kind=SystemMenuIntentKind::Save;pending_.slot=confirm_slot_;', ['slots', 'menu']),
    ('S14', 'an occupied slot is saved over without asking', MENU,
     'else if(st==SaveSlotStatus::Empty){pending_.kind=SystemMenuIntentKind::Save;',
     'else if(true){pending_.kind=SystemMenuIntentKind::Save;', ['slots', 'menu']),
    ('S15', 'an empty slot is offered for loading', MENU,
     'if(page_==Page::Load){if(slot_loadable(catalog_,cursor_))', 'if(page_==Page::Load){if(true)', ['menu']),
    ('S16', 'the title Load page offers an empty slot', FRONTEND,
     'if(slot_loadable(catalog_,cursor_)){pending_.kind=FrontendIntentKind::LoadSlot;',
     'if(true){pending_.kind=FrontendIntentKind::LoadSlot;', ['menu', 'frontend']),
    ('S17', 'migration: the pre-SAVE2 pair is not Slot 1 (alpha1-s1-g*)', SAVE,
     'if(slot==0)std::snprintf(out,sizeof(out),"%s/alpha1-g%d.%s%s"',
     'if(slot==0)std::snprintf(out,sizeof(out),"%s/alpha1-s1-g%d.%s%s"', ['slots', 'save1']),
    ('S18', 'migration: a save naming no slot on a blank card goes to Slot 3', SAVE,
     'logical=continue_order(s,present_all,order)>0?order[0]:0;',
     'logical=continue_order(s,present_all,order)>0?order[0]:2;', ['slots', 'save1']),
    ('S19', 'Alt+S ignores the journey slot', RUNTIME,
     'resources_.initial_ool_size,ms,false,save_.last_slot());',
     'resources_.initial_ool_size,ms,false,-1);', ['slots']),
    ('S20', 'Alt+L reloads Continue, not the journey slot', RUNTIME,
     'bool ok=journey>=0?save_.load_slot(journey,', 'bool ok=false?save_.load_slot(journey,', ['slots']),
    ('S21', 'New Journey ignores the empty slot (always Slot 1)', FRONTEND,
     'case 1: new_journey_slot_=int8_t(first_empty_slot(catalog_));',
     'case 1: new_journey_slot_=int8_t(first_empty_slot(catalog_)>=0?0:-1);', ['slots', 'menu']),
    ('S22', 'New Journey "Replace Slot N?" starts on Yes', FRONTEND,
     'else if(a.kind==UiActionKind::Confirm){new_journey_slot_=int8_t(cursor_);slot_confirm_=true;cursor_=0;}',
     'else if(a.kind==UiActionKind::Confirm){new_journey_slot_=int8_t(cursor_);slot_confirm_=true;cursor_=1;}',
     ['slots', 'menu']),
    # A4-UI3 re-anchored S23 and S25 on the same code: the System Menu save now
    # returns to the game on success, and only a failed save refreshes the menu.
    ('S23', 'the System Menu save drops the chosen slot', RUNTIME,
     'resources_.initial_ool_size,ms,false,intent.slot);\n        // Alpha 4 A4-UI3',
     'resources_.initial_ool_size,ms,false,-1);\n        // Alpha 4 A4-UI3', ['slots', 'menu']),
    ('S24', 'the save list keys every slot by Slot 1\'s entry', SAVE,
     'const auto&k=commit[s][i];', 'const auto&k=commit[0][i];', ['slots']),
    ('S25', 'the menu is not given the card a failed save left', RUNTIME,
     'save_.inspect_catalog(catalog);system_menu_.set_save_catalog(catalog,save_.last_slot());\n            std::snprintf(notice',
     'save_.inspect_catalog(catalog);(void)catalog;\n            std::snprintf(notice', ['menu', 'storage', 'ux']),
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
        a = anchor.replace('\r\n', '\n').replace('\n', eol)
        b = repl.replace('\r\n', '\n').replace('\n', eol)
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
    print(f'A4-SAVE2 mutation check: {killed} killed, {survived} survived, {invalid} invalid; '
          f'restored build {"GREEN" if green else "RED"}')


if __name__ == '__main__':
    main()
