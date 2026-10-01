"""Alpha 4 A4-UI3 -- mutation check of the save/load UX.

    python tools/a4_ui3_mutation_check.py <build-dir> [ids,comma,separated]

As tools/a4_save2_mutation_check.py: each mutant is one edit of production
code (anchors converted to the file's own line endings); the driver applies
it, touches the file, builds the named test targets, runs them, and restores +
touches the file whatever happens. KILLED = a named test exits non-zero,
INVALID = it does not build. The PATH must hold the host toolchain.

The classes the A4-UI3 brief names: a wrong current-slot marker (U1, U2), the
destructive prompt defaulting Yes (U3), an empty Load accepted (U4), manual
Load cross-falling (U5), a recovered slot shown as healthy (U6), the wrong
slot named in the overwrite prompt (U7, U8), Back / the Mic confirming the
destructive action (U9, U10), metadata from the wrong slot (U11); plus the
Continue presentation, the save/load feedback, the two-row panel and DAMAGED.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
NINJA = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/ninja.exe'
PACK = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
ORIGINAL = os.path.join(ROOT, 'original/u5/ultima5')
SAVE = 'native/targets/tdeck/main/alpha_save.cpp'
RUNTIME = 'native/targets/tdeck/main/alpha_runtime.cpp'
BOARD = 'native/targets/tdeck/main/tdeck_board.cpp'
FRONTEND = 'native/core/src/frontend.cpp'
MENU = 'native/core/src/system_menu.cpp'
TESTS = {
    'rows': ('a4_ui3_slot_rows', []),
    'ux': ('a4_ui3_save_ux_runtime', [PACK, ORIGINAL]),
    'menu': ('a4_ui2_save_menu_runtime', [PACK]),
}

# (id, what, file, anchor, replacement, tests)
MUTANTS = [
    ('U1', 'wrong current-slot marker: CURRENT follows the cursor, not the journey', MENU,
     'list_slots(v,d,catalog_,cursor_,journey_slot_,kCurrentSlotTag);',
     'list_slots(v,d,catalog_,cursor_,cursor_,kCurrentSlotTag);', ['rows', 'ux']),
    ('U2', 'wrong title marker: LATEST always on Slot 1, not Continue\'s slot', FRONTEND,
     '        list_slots(v,dynamic,catalog_,cursor_,continue_slot(catalog_),kLatestSlotTag);\n        format_slot_detail(dynamic[6],96,catalog_,cursor_,false);',
     '        list_slots(v,dynamic,catalog_,cursor_,0,kLatestSlotTag);\n        format_slot_detail(dynamic[6],96,catalog_,cursor_,false);', ['rows', 'ux']),
    ('U3', 'the overwrite question defaults to Yes', MENU,
     'else{confirm_slot_=int8_t(cursor_);page_=Page::Confirm;cursor_=0;}',
     'else{confirm_slot_=int8_t(cursor_);page_=Page::Confirm;cursor_=1;}', ['rows', 'ux']),
    ('U4', 'an empty slot is accepted for Load', MENU,
     'if(page_==Page::Load){if(slot_loadable(catalog_,cursor_)){',
     'if(page_==Page::Load){if(catalog_.slots[cursor_].status!=SaveSlotStatus::Damaged){', ['rows', 'ux']),
    ('U5', 'manual Load cross-falls to Continue when its slot fails', RUNTIME,
     'else if(intent.kind==openu5::SystemMenuIntentKind::LoadSlot)ok=save_.load_slot(intent.slot,context_,outdoor_,terrain_,actors_,retained_,ms);',
     'else if(intent.kind==openu5::SystemMenuIntentKind::LoadSlot)ok=save_.load_slot(intent.slot,context_,outdoor_,terrain_,actors_,retained_,ms)||save_.load(context_,outdoor_,terrain_,actors_,retained_,ms);',
     ['ux']),
    ('U6', 'a recovered slot is shown as healthy (no RECOVERED tag)', FRONTEND,
     'const bool mark=slot==marked&&tag,rec=e.status==SaveSlotStatus::Recovered;',
     'const bool mark=slot==marked&&tag,rec=false;', ['rows', 'ux']),
    ('U7', 'the overwrite title names the journey\'s slot, not the one chosen', MENU,
     'std::snprintf(d[0],96,"Overwrite Slot %d?",confirm_slot_+1);',
     'std::snprintf(d[0],96,"Overwrite Slot %d?",journey_slot_+1);', ['rows', 'ux']),
    ('U8', 'the overwrite question shows another slot\'s save', MENU,
     'format_slot_identity(d[1],96,catalog_,confirm_slot_);',
     'format_slot_identity(d[1],96,catalog_,journey_slot_);', ['rows', 'ux']),
    ('U9', 'Back on the overwrite question saves', MENU,
     'if(page_==Page::Confirm){page_=Page::Save;cursor_=uint8_t(confirm_slot_);return true;}',
     'if(page_==Page::Confirm){pending_.kind=SystemMenuIntentKind::Save;pending_.slot=confirm_slot_;page_=Page::Root;cursor_=1;return true;}',
     ['rows', 'ux']),
    ('U10', 'the Mic (Cancel) on the question with Yes highlighted saves', MENU,
     'if(page_==Page::Confirm){page_=Page::Save;cursor_=uint8_t(confirm_slot_);return true;}',
     'if(page_==Page::Confirm){if(a.kind==UiActionKind::Cancel&&cursor_==1){pending_.kind=SystemMenuIntentKind::Save;pending_.slot=confirm_slot_;page_=Page::Root;cursor_=1;return true;}page_=Page::Save;cursor_=uint8_t(confirm_slot_);return true;}',
     ['rows', 'ux']),
    ('U11', 'metadata from the wrong slot: each row shows another slot\'s save', FRONTEND,
     'const auto&e=c.slots[slot];const bool held=slot_loadable(c,slot);',
     'const auto&e=c.slots[kSaveSlotCount-1-slot];const bool held=slot_loadable(c,kSaveSlotCount-1-slot);', ['rows', 'ux']),
    ('U12', 'a damaged slot is shown as EMPTY', FRONTEND,
     'const char*what=e.status==SaveSlotStatus::Empty?"EMPTY":!held?"DAMAGED":',
     'const char*what=e.status==SaveSlotStatus::Empty?"EMPTY":!held?"EMPTY":', ['rows', 'ux']),
    ('U13', 'an empty row shows leftover metadata', FRONTEND,
     'if(e.status!=SaveSlotStatus::Empty)std::snprintf(detail,cap,"%.*s",n,text);',
     'std::snprintf(detail,cap,"%.*s",n,text);', ['rows', 'ux']),
    ('U14', 'Journey Onward names the wrong slot number', FRONTEND,
     '"Latest: Slot %d, %.40s",c+1,', '"Latest: Slot %d, %.40s",c,', ['rows', 'ux']),
    ('U15', 'a successful save leaves the menu open (no return to the game)', RUNTIME,
     'ui_->append(openu5::UiTextChannel::System,notice);system_menu_.close();}',
     'ui_->append(openu5::UiTextChannel::System,notice);}', ['ux', 'menu']),
    ('U16', 'a recovered load is not announced', RUNTIME,
     '    if(!save_.last_load_recovered())return;', '    if(true)return;', ['ux', 'menu']),
    ('U17', 'every slot load claims a recovery', SAVE,
     'last_slot_=slot;last_load_recovered_=older;', 'last_slot_=slot;last_load_recovered_=true;', ['ux']),
    ('U18', 'Continue never reports passing over its newest slot or generation', SAVE,
     'last_load_recovered_=older||i>0;', 'last_load_recovered_=false;', ['ux']),
    ('U19', 'the panel selects one row of the two', BOARD,
     'const auto&v=paired?expanded:in;const int span=paired?2:1;',
     'const auto&v=paired?expanded:in;const int span=1;', ['ux']),
    ('U20', 'the retained redraw forgets the old selection\'s second row', BOARD,
     'int(i)<was+frontend_cache_.selected_span;', 'int(i)<was+1;', ['ux']),
    ('U21', 'the place row is drawn white (no grey detail)', BOARD,
     'return paired&&(i&1)&&!selected_row(i)?kChromeDim:kWhite;', 'return paired&&false&&!selected_row(i)?kChromeDim:kWhite;', ['ux']),
    ('U22', 'a failed save claims the slot kept its save when nothing was there', RUNTIME,
     'openu5::slot_loadable(catalog,intent.slot)?"Save failed. Slot %d keeps its last save":"Save failed. Nothing was saved in Slot %d"',
     'true?"Save failed. Slot %d keeps its last save":"Save failed. Nothing was saved in Slot %d"', ['ux']),
    ('U23', 'a failed load leaves the page listing the slot as it was', RUNTIME,
     'openu5::FrontendSaveCatalog catalog{};save_.inspect_catalog(catalog);system_menu_.set_save_catalog(catalog,save_.last_slot());\n        char notice[64];',
     'char notice[64];', ['ux']),
    ('U24', 'the PC import picker does not mark Continue\'s slot', FRONTEND,
     '            list_slots(v,dynamic,catalog_,cursor_,continue_slot(catalog_),kLatestSlotTag);\n            format_slot_detail(dynamic[6],96,catalog_,cursor_,true);',
     '            list_slots(v,dynamic,catalog_,cursor_,-1,kLatestSlotTag);\n            format_slot_detail(dynamic[6],96,catalog_,cursor_,true);', ['rows', 'ux']),
    ('U25', 'a failed save leaves the open menu listing the card as it was', RUNTIME,
     'save_.inspect_catalog(catalog);system_menu_.set_save_catalog(catalog,save_.last_slot());\n            std::snprintf(notice',
     'save_.inspect_catalog(catalog);(void)catalog;\n            std::snprintf(notice', ['ux']),
]


def touch(p):
    t = time.time()
    os.utime(p, (t, t))


def build(build_dir, targets):
    r = subprocess.run([NINJA, '-C', build_dir] + targets, capture_output=True, text=True)
    return r.returncode == 0, r.stdout[-1500:]


def run(build_dir, key):
    target, args = TESTS[key]
    r = subprocess.run([os.path.join(build_dir, target + '.exe')] + args, capture_output=True, text=True, timeout=600)
    reds = []
    for l in r.stdout.splitlines():
        w = l.split()
        if w and w[0] == 'RED' and len(w) > 1:
            reds.append(w[1])
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
    print(f'A4-UI3 mutation check: {killed} killed, {survived} survived, {invalid} invalid; '
          f'restored build {"GREEN" if green else "RED"}')


if __name__ == '__main__':
    main()
