# Mix command parity — A3-HF10 derivation (CMDS.OVL 0x1ad8 / 0x18be / 0x1a70, kernel 0x3b9e)

Derived 2026-09-28 from the shipped 1988 binaries (`original/u5/ultima5/CMDS.OVL`,
`ULTIMA.EXE`, `DATA.OVL`) with `re/tools/dis16.py`; CMDS near calls resolve through the
overlay base `0xBF80` (`call 0x66ec` → kernel `0x266c` getkey, `call 0x7c1e` → kernel
`0x3b9e` getnum, `call 0x7050` → kernel `0x2fd0` chest trap). DS strings read from
`DATA.OVL` at `DS + 0x10`. This note replaces the `re/notes/mix-flow-acta.md` citation in
`game/src/core/magic/mixReagentPicker.ts`, a file that is not in the tree.

## 1. `cmd_mix` (CMDS 0x1ad8)

| offset | what |
|---|---|
| 0x1ae0–0x1afa | sum all eight reagent bytes `[0x5850+r]`; zero → `"No reagents owned!\n"` (DS 0x8f98), return (0x1c1a) |
| 0x1b06–0x1b1b | `"For what spell?\n:"` (DS 0x8fac); spell getstring; -1 → `"\nNone!\n"` (DS 0x8fbe) |
| 0x1b1e–0x1b5a | footer: `\n` + CP437 `← , → , ↑ , ↓` + `" to move,\nRETURN selects.\nType M to mix:"` (DS 0x8fc6) |
| 0x1b5d | `mask = reagent_picker()` (0x18be); `< 0` (ESC) → epilogue, **no message** |
| 0x1b6b | `n = mix_quantity(mask)` (0x1a70); `n <= 0` (`jg` fails) → epilogue, **no message** |
| 0x1b78 | `mask == 0` → `"\nNothing to mix!\n"` (DS 0x9004) |
| 0x1b81 | `"Mixing...\n"` (DS 0x8ff0), then a 10-tick wait (0x617a delay / 0x7b66 run_n_frames by `[0x5893]`) |
| 0x1b9f–0x1bba | for each reagent `r` with `mask & (0x80>>r)`: `[0x5850+r] -= n` — **before** the recipe test, right or wrong |
| 0x1bc2–0x1bd4 | `spell >= 0 && recipe[0x1cc0+spell] == mask` → `"\nDone!\n"` (DS 0x8ffc); `[0x57f0+spell] += n`, capped at 0x63 (99) |
| 0x1bf6–0x1c04 | otherwise: `putchar('\n')`, first conscious member (kernel 0x39fc), chest trap (kernel 0x2fd0) — `re/notes/mix-trap-105-acta.md` |

Mask equality is exact: an extra reagent, a missing one, or an empty set are all "wrong".
There is no pre-check that the player OWNS the recipe; ownership only matters through the
quantity test of §3, which reads the MARKED reagents, not the recipe.

## 2. Reagent picker (CMDS 0x18be)

* Rows: the reagents with a non-zero count, ascending id (0x18ca–0x18dd), under the
  `"Reagents:"` header (DS 0x8f64); each row `" NN NAME"`, count in two digits with `'0'`
  fill (0x194c `print_number(q, 2, '0')`).
* Mask starts at 0 (0x198a `mov di,si`, `si = 0`), cursor on the first row.
* Keys (getkey 0x266c upper-cases `a`–`z` at kernel 0x2032, so `m` is `M`):
  * up/left (getkey 1 / 3): cursor up, **clamped** at the first row (0x19b2 `jle`);
  * down/right (2 / 4): cursor down, **clamped** at the last row (0x19d0 `jle`);
  * RETURN 0x0d **or** Space 0x20: toggle the row's bit, `di ^= 0x80>>id` (0x19ee–0x19f9),
    redraw the mark (0x0f filled / 0x20 blank between two 0xfd, column 3); the picker stays open;
  * `M` 0x4d: close, print `"\n\n"` (DS 0x8f6e), return the mask (0x19e0);
  * ESC 0x1b: close, `putchar('\n')`, return -1 (0x1a2e);
  * anything else, backspace included: ignored, getkey re-reads (0x1a50).
* Nothing is written to the inventory here.

## 3. Quantity (CMDS 0x1a70, `mix_quantity(mask)`)

```
1a78  ok = 1
1a7d  print "How much? "              ; DS 0x8f72
1a88  n = getnum(2)                    ; kernel 0x3b9e
1a8e  if n == 0: goto 1ac6             ; ok still 1 -> return 0
1a92  for r in 0..7 (bit 0x80>>r):
1a97    if (mask & bit) && byte[0x5850+r] < n (UNSIGNED 16-bit, 0x1aa5 jae):
1aa7        print "Insufficient reagents!\n\n"   ; DS 0x8f7e
1aae        ok = 0 ; break
1ac6  if !ok: goto 1a78                ; ask again
1acc  return n
```

* The test reads only the MARKED reagents; an empty mask never fails it.
* A negative `n` compares as `0xfff7..0xffff`, so with any reagent marked it is always
  "Insufficient reagents!" and a re-ask; with an empty mask it returns and `cmd_mix`
  aborts in silence (`n <= 0`).
* Nothing is written to the inventory here either: an insufficient answer mutates nothing.

## 4. getnum (kernel 0x3b9e, called with 2)

* Buffer of at most `max` (clamped to 5) characters; digits echo, extra digits are ignored.
* `-` or `+` is accepted as the FIRST character only (0x3be2–0x3bf4); it takes a buffer slot.
* Backspace 0x08 (or getkey 1) deletes one character; on an empty buffer it does nothing.
* ESC 0x1b erases the whole buffer and **keeps reading**; on an empty buffer it does nothing.
  There is no cancel: only RETURN leaves.
* RETURN: digits `1..len-1` are summed with the 1/10/100 table at DS 0x6a0a, then the first
  character: `-` negates, a digit adds, `+` adds nothing. So 2 slots give `0..99` or `-9..+9`.
* RETURN on an EMPTY buffer reads the never-written first byte `[bp-6]`: stack residue. The
  reference (`prompt-manager.ts`) and the native port take it as 0 (silent abort); this is a
  declared model, the residue is not reproduced.

## 5. Resulting contract (both ports)

1. Zero reagents owned → "No reagents owned!", nothing opens.
2. Spell chosen → picker, nothing marked; the player marks by hand; no auto-selection.
3. ESC at the picker → nothing printed, nothing mutated.
4. `M` → "How much? " — even with nothing marked.
5. Quantity: 0 / empty → silent abort; marked reagent short (or a negative number) →
   "Insufficient reagents!" and the question again; empty mask with n > 0 → "Nothing to mix!".
6. Otherwise "Mixing...", deduct `n` of each MARKED reagent, then: exact recipe → "Done!",
   spell count `+n` capped at 99 (reagents are spent in full even past the cap); wrong set →
   no charge, and the chest trap fires.
