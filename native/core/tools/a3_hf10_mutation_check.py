#!/usr/bin/env python
"""Alpha 3 A3-HF10 (D-6 / D-70) -- mutation validation of Mix command parity.

Each native mutation changes production anchors -- the device's Mix arms and
picker rows (native/targets/tdeck/main/alpha_runtime.cpp), the picker / getnum
key rules (native/core/src/ui_session.cpp), the 0x1a70 shortage rule
(native/core/src/magic.cpp) or the core Mix command (native/core/src/commands.cpp)
-- rebuilds a3_hf10_mix_parity_runtime and records which checks turn RED.
Each TypeScript mutation changes game/src/ui/prompt-manager.ts or
game/src/core/magic/mix.ts and runs the two HF10 vitest files. Every file is
restored byte for byte and touched (a restored file is older than the mutated
object: without the touch ninja keeps the mutant). A mutation that leaves
everything GREEN is a SURVIVOR and fails this run; one that does not build is
INVALID (not killed) and must be rewritten.

Usage: python native/core/tools/a3_hf10_mutation_check.py <build-dir> <log> [M1,T2,...]
(the optional third argument re-runs only the named mutations)
"""
import os
import re
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
BIN = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin'
CMAKE = BIN + '/cmake.exe'
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
ONLY = set(sys.argv[3].split(',')) if len(sys.argv) > 3 else None
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
TARGET = 'a3_hf10_mix_parity_runtime'
GAME = os.path.join(ROOT, 'game')
NPX = 'npx.cmd' if os.name == 'nt' else 'npx'
TS_TESTS = ['tests/mix-hf10-quantity.test.ts', 'tests/prompt-manager.test.ts']

RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
UI = 'native/core/src/ui_session.cpp'
MG = 'native/core/src/magic.cpp'
CM = 'native/core/src/commands.cpp'
PM = 'game/src/ui/prompt-manager.ts'
MX = 'game/src/core/magic/mix.ts'

CUSTOM = r'''{pending_mix_spell_=selections_[i.value.index].value;pending_mix_mask_=0;open_mix_reagents(0);}'''
ASK = r'''        if(i.value.yes)ui_->begin_number(openu5::UiRequestId::MixQuantity,"How much? ",-9,99,2);'''
TOGGLE = r'''pending_mix_mask_^=uint8_t(1u<<selections_[i.value.index].value);'''
HOURS = r'''c.hours=int16_t(i.value.number);'''
SHORT_CALL = r'''        if(openu5::mix_quantity_short(game_,pending_mix_mask_,i.value.number)){'''
CANCEL = r'''if(i.request==openu5::UiRequestId::MixReagents||i.request==openu5::UiRequestId::MixQuantity){pending_mix_spell_=-1;pending_mix_mask_=0;}'''
PICKER_MODE = r'''    ui_->begin_selection(openu5::UiMode::InventorySelection,openu5::UiRequestId::MixReagents,"Reagents:",{this,selection_count,selection_item},row);'''
LOAD_RESET = r'''    pending_mix_spell_=-1;pending_mix_mask_=0; // A3-HF10'''
PRECHECK = r'''            if(!owned){ui_->append(openu5::UiTextChannel::Message,"No reagents owned!");dirty_=true;return;}'''
UI_DOWN = r'''                if (down && selection_cursor_ + 1 < count) ++selection_cursor_;'''
UI_SPACE = r'''            } else if (count && (a.kind == UiActionKind::Confirm ||
                                 (a.kind == UiActionKind::Character && a.character == u' ')))'''
UI_ESC = r'''            if (a.kind == UiActionKind::Cancel || a.kind == UiActionKind::Back) {
                input_length_ = 0; input_[0] = 0;
                return true;
            }'''
UI_SIGN = r'''                if (!input_length_ && input_limit_) { input_[input_length_++] = a.character; input_[input_length_] = 0; }'''
UI_NEG = r'''                if (negative) n = -n;'''
UI_BACK = r'''            if (a.kind == UiActionKind::Cancel) cancel_modal();
            else if (count && a.kind == UiActionKind::Direction) {'''
MG_UNSIGNED = r'''        if ((mask & (1 << i)) && uint32_t(std::max<int32_t>(g.reagent_quantities[i], 0)) < wanted) return true;'''
CM_CHECK = r'''        for(int i=0;i<8;++i)if((cmd.reagent_mask&(1<<i))&&c.game.reagent_quantities[i]<cmd.hours){say("Insufficient reagents!");return result;}'''
CM_DEDUCT = r'''if(cmd.reagent_mask&(1<<i))c.game.reagent_quantities[i]-=cmd.hours;'''
CM_RECIPE = r'''if(cmd.item<48&&def->reagents==cmd.reagent_mask){'''
CM_CHARGE = r'''c.game.spell_quantities[cmd.item]+cmd.hours);'''

NATIVE = [
    ('M1', 'skip the reagent picker: the spell goes straight to "How much?" with nothing marked',
     [(RT, CUSTOM, r'''{pending_mix_spell_=selections_[i.value.index].value;pending_mix_mask_=0;ui_->begin_number(openu5::UiRequestId::MixQuantity,"How much? ",-9,99,2);}''')]),
    ('M2', "auto-select the recipe: the picker opens with the spell's own reagents marked",
     [(RT, CUSTOM, r'''{pending_mix_spell_=selections_[i.value.index].value;{const auto*d=openu5::spell_definition(openu5::SpellId(pending_mix_spell_));pending_mix_mask_=d?d->reagents:0;}open_mix_reagents(0);}''')]),
    ('M3', 'accept a wrong recipe: any overlap with the recipe counts as exact',
     [(CM, CM_RECIPE, r'''if(cmd.item<48&&(def->reagents&cmd.reagent_mask)){''')]),
    ('M4', 'reject the correct recipe: every set is "wrong"',
     [(CM, CM_RECIPE, r'''if(cmd.item<48&&def->reagents==(cmd.reagent_mask^1)){''')]),
    ('M5', 'deduct the wrong reagent: a mark sets the NEXT reagent\'s bit',
     [(RT, TOGGLE, r'''pending_mix_mask_^=uint8_t(1u<<((selections_[i.value.index].value+1)&7));''')]),
    ('M6', 'deduct one copy regardless of the quantity',
     [(CM, CM_DEDUCT, r'''if(cmd.reagent_mask&(1<<i))c.game.reagent_quantities[i]-=1;''')]),
    ('M7', 'charge one spell regardless of the quantity',
     [(CM, CM_CHARGE, r'''c.game.spell_quantities[cmd.item]+1);''')]),
    ('M8', 'omit the quantity prompt: M mixes one at once',
     [(RT, ASK, r'''        if(i.value.yes){c.kind=openu5::CommandKind::Mix;c.item=pending_mix_spell_;c.hours=1;c.reagent_mask=pending_mix_mask_;pending_mix_spell_=-1;pending_mix_mask_=0;command(c);}''')]),
    ('M9', 'hard-code the quantity to 1 (the answer is read and ignored)',
     [(RT, HOURS, r'''c.hours=int16_t(i.value.number>0?1:i.value.number);''')]),
    ('M10', 'allow more than the marked reagents hold: no re-ask, no core refusal',
     [(RT, SHORT_CALL, r'''        if(false&&openu5::mix_quantity_short(game_,pending_mix_mask_,i.value.number)){'''),
      (CM, CM_CHECK, r'''        for(int i=0;i<8;++i)if((cmd.reagent_mask&(1<<i))&&c.game.reagent_quantities[i]<cmd.hours&&false){say("Insufficient reagents!");return result;}''')]),
    ('M11', 'mutate the inventory before the answer: a mark spends one of the reagent',
     [(RT, TOGGLE, TOGGLE + r'''--game_.reagent_quantities[selections_[i.value.index].value];''')]),
    ('M12', 'cancel still mutates: ESC at the picker mixes one with the marks',
     [(RT, CANCEL, r'''if(i.request==openu5::UiRequestId::MixReagents&&pending_mix_spell_>=0){openu5::Command m;m.kind=openu5::CommandKind::Mix;m.item=pending_mix_spell_;m.hours=1;m.reagent_mask=pending_mix_mask_;command(m);}''' + CANCEL)]),
    ('M13', 'keys leak into gameplay: the picker is not modal (Explore keeps the keys)',
     [(RT, PICKER_MODE, PICKER_MODE.replace('UiMode::InventorySelection', 'UiMode::Exploration'))]),
    ('M14', 'a successful load keeps the pending Mix',
     [(RT, LOAD_RESET, '    // A3-HF10 mutant M14: no reset')]),
    ('M15', 'the picker cursor wraps (the generic selection rule) instead of clamping',
     [(UI, UI_DOWN, r'''                if (down) selection_cursor_ = (selection_cursor_ + 1) % count;''')]),
    ('M16', 'Space does not toggle (RETURN only)',
     [(UI, UI_SPACE, r'''            } else if (count && (a.kind == UiActionKind::Confirm))''')]),
    ('M17', 'ESC cancels "How much?" (the generic numeric rule) instead of erasing',
     [(UI, UI_ESC, r'''            if (a.kind == UiActionKind::Cancel || a.kind == UiActionKind::Back) {
                cancel_modal();
                return true;
            }''')]),
    ('M18', 'no leading sign: + and - are dropped',
     [(UI, UI_SIGN, r'''                (void)input_limit_;''')]),
    ('M19', 'the sign is read and ignored: "-5" is 5',
     [(UI, UI_NEG, r'''                (void)negative;''')]),
    ('M20', 'the shortage test is signed: "-5" is never short',
     [(MG, MG_UNSIGNED, r'''        if ((mask & (1 << i)) && g.reagent_quantities[i] < quantity + int32_t(wanted & 0u)) return true;''')]),
    ('M21', 'the shortage test reads the UNMARKED reagents too',
     [(MG, MG_UNSIGNED, r'''        if ((mask & 0xff) && uint32_t(std::max<int32_t>(g.reagent_quantities[i], 0)) < wanted) return true;''')]),
    ('M22', 'no "No reagents owned!" precheck: the list opens with nothing to mark',
     [(RT, PRECHECK, r'''            (void)owned;''')]),
    ('M23', 'backspace cancels the picker (the generic selection rule)',
     [(UI, UI_BACK, r'''            if (a.kind == UiActionKind::Cancel || a.kind == UiActionKind::Back) cancel_modal();
            else if (count && a.kind == UiActionKind::Direction) {''')]),
]

TS_ESC = r'''      } else if (ev.key === "Escape") {
        if (p.buffer.length > 0) {
          p.buffer = "";
          hud.echoSetLast(p.prefix);
        }
      } else if (ev.key === "Backspace") {
        if (p.buffer.length > 0) {'''
TS_SIGN = r'''        (/^[0-9]$/.test(ev.key) || ((ev.key === "+" || ev.key === "-") && p.buffer.length === 0))'''
TS_WANTED = r'''  const wanted = n & 0xffff;'''
TS_UNMARKED = r'''  if (selected.some((r) => Math.max(0, qtyOf(r)) < wanted)) return "insufficient";'''
TS_ZERO = r'''  if (n === 0) return "abort";'''
TS = [
    ('T1', 'TS getnum: ESC cancels (the pre-HF10 prompt)',
     [(PM, TS_ESC, r'''      } else if (ev.key === "Escape") {
        this._current = null;
      } else if (ev.key === "Backspace") {
        if (p.buffer.length > 0) {''')]),
    ('T2', 'TS getnum: no leading sign', [(PM, TS_SIGN, r'''        /^[0-9]$/.test(ev.key)''')]),
    ('T3', 'TS verdict: signed comparison', [(MX, TS_WANTED, r'''  const wanted = n;''')]),
    ('T4', 'TS verdict: unmarked reagents are read',
     [(MX, TS_UNMARKED, r'''  if ([0, 1, 2, 3, 4, 5, 6, 7].some((r) => Math.max(0, qtyOf(r)) < wanted)) return "insufficient";''')]),
    ('T5', 'TS verdict: 0 is tested like any answer (empty mask 0 -> "nothing")',
     [(MX, TS_ZERO, r'''  if (n === 0 && selected.length > 0) return "abort";''')]),
]


def env():
    e = dict(os.environ)
    e['PATH'] = BIN + ';' + e.get('PATH', '')
    return e


def apply(edits):
    saved = {}
    for path, old, new in edits:
        full = os.path.join(ROOT, path)
        if path not in saved:
            saved[path] = open(full, 'rb').read()
        text = open(full, 'rb').read().decode('utf-8')
        eol = '\r\n' if '\r\n' in text else '\n'
        o, n = old.replace('\n', eol), new.replace('\n', eol)
        assert text.count(o) == 1, (path, old[:70])
        with open(full, 'wb') as h:
            h.write(text.replace(o, n).encode('utf-8'))
    return saved


def restore(saved):
    now = time.time()
    for path, text in saved.items():
        full = os.path.join(ROOT, path)
        with open(full, 'wb') as h:
            h.write(text)
        os.utime(full, (now, now))


def native_run():
    b = subprocess.run([CMAKE, '--build', BUILD, '--target', TARGET], env=env(), capture_output=True, text=True,
                       errors='replace')
    if b.returncode:
        return None, (b.stdout + b.stderr)[-1500:]
    r = subprocess.run([os.path.join(BUILD, TARGET + '.exe'), RES], capture_output=True, text=True, errors='replace',
                       timeout=600)
    return [l[4:].split(' ')[0] for l in r.stdout.splitlines() if l.startswith('RED ')], ''


def ts_run():
    e = dict(os.environ, NO_COLOR='1', FORCE_COLOR='0')
    p = subprocess.run([NPX, 'vitest', 'run'] + TS_TESTS, cwd=GAME, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', env=e)
    text = p.stdout + p.stderr
    fails = sorted({re.sub(r'[^\x20-\x7e]', '-', l.split('>')[-1]).strip()[:60] for l in text.splitlines()
                    if l.lstrip().startswith('FAIL') and '>' in l})
    return fails if p.returncode else []


def main():
    out = ['# A3-HF10 mutation check: native mutants rebuild and run ' + TARGET +
           '; TypeScript mutants run vitest ' + ' '.join(TS_TESTS)]
    killed = survived = invalid = 0
    for mid, desc, edits in NATIVE + TS:
        if ONLY and mid not in ONLY:
            continue
        saved = apply(edits)
        try:
            if mid.startswith('M'):
                reds, err = native_run()
            else:
                reds, err = ts_run(), ''
        finally:
            restore(saved)
        if reds is None:
            invalid += 1
            out.append(f'{mid} INVALID (does not build) -- {desc}')
            out += ['    ' + l for l in err.splitlines()[-6:]]
        elif reds:
            killed += 1
            out.append(f'{mid} KILLED by {len(reds)} -- {desc}')
            out.append('    ' + ' '.join(reds[:16]) + (' ...' if len(reds) > 16 else ''))
        else:
            survived += 1
            out.append(f'{mid} SURVIVED -- {desc}')
        print(out[-2] if reds else out[-1], flush=True)
    reds, err = native_run()
    ts = ts_run()
    out.append(f'# restored: {TARGET} {"BUILD FAILED" if reds is None else str(len(reds)) + " RED"}; '
               f'vitest {len(ts)} failing')
    out.append(f'# {killed} killed, {survived} survived, {invalid} invalid')
    with open(LOG, 'w', encoding='utf-8', newline='\n') as h:
        h.write('\n'.join(out) + '\n')
    print('\n'.join(out[-2:]))
    return 0 if not survived and not invalid and reds == [] and not ts else 1


if __name__ == '__main__':
    sys.exit(main())
