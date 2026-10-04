# D-86 FINAL (reconciler): the dungeon digit key naming an invalid member

Read-only reconciliation of D86-A.md and D86-B.md. Everything in sections 1-2 was re-disassembled in this
session (re/tools/dis16.py, my own census scripts in `findings/d86f/`: `basecheck.py`, `census.py`). Nothing in the
repository was edited, built or run. Notation: `OVL:0xXXXX` = file offset in the overlay (the notes' CS:IP),
`K:0xXXXX` = ULTIMA.EXE image offset (dis16 `--exe`), `DS 0xXXXX` = DATA.OVL offset (file offset = DS + 0x10).

---------------------------------------------------------------------------------------------------

## 1 Verdict

**What the 1988 binary does.** In a dungeon every key `0x30..0x39` goes to DUNGEON.OVL `0x06c4` (arm `0x07bc-0x07d6`),
which calls the shared kernel set-active routine K:0x4080 and then **overwrites its return value with 0**
(`0x07ce mov [bp-2],ax` immediately followed by `0x07d1 mov word ptr [bp-2],0`). The kernel returns 1 only for the
"Invalid!" case; the dungeon discards it. Return 0 makes the dungeon loop skip exactly one thing: the call to the
per-turn block `0x0c76` (sleeper-wake `rand(0,0x3f)` rolls, wanderer/ambush step, tile/trap effects, status redraw,
K:0x2ae8 housekeeping). The digit path itself draws **no RNG** and charges **no housekeeping**. For `'0'`, a valid
member, an invalid index, a dead member and an asleep member the dungeon outcome is the same: no world turn.
Overworld (MAINOUT `0x0c0c`) and town (TOWN `0x0ef3`) do NOT discard the 1: there an invalid digit costs a turn.
The dungeon discard is the only "forced 0" of the three.

**Nuance neither the ledger nor the notes state (both investigators found it, I re-proved it).** The 1-minute
clock tick `advance_clock(1)` at `0x0f2f` is **not gated on the dispatcher result**. It runs once per loop
iteration, before the next key, for every key and every return value (T none, Q every second iteration). So "passes
no turn" is true of the world turn `0x0c76` but a dungeon digit still costs the loop minute, exactly like every other
no-turn dungeon key (Z, Look, Attack, ...). This is a separate, key-independent divergence of both ports; it is NOT
what D-86 describes and it should not be folded into the D-86 fix (see section 7).

**Who is wrong.**
* **Native: wrong, large.** `commands.cpp:781` ends the invalid-member branch with `if(!g.position.map.location)r.turn();`.
  A dungeon session leaves `position.map.location == 0`, so a full OUTDOOR turn runs on the surface tile under the
  entrance (wind roll, 2 minutes, hazard roll, housekeeping, `turns_since_start++`, doors, actors, spawn gate +
  `outdoor_tick`, Refuge, Waterfall).
* **TypeScript reference: wrong, small.** `game.ts:2187` has the same gate (`position.location === 0`, true underground
  because `enterDungeon` leaves `state.position` on the surface tile). `runContextTurn` is dungeon-aware
  (`effectiveLocation`), so it reaches only the dungeon tail (`game.ts:2651-2653`): `tickDoorsAndNpcs()` +
  `checkRefuge()`. No clock, no RNG, no housekeeping. Residue: one door-timer tick + `state.openDoors` re-serialise, and a
  spurious refuge check. Invisible to every existing fixture.
* **Notes / docs that are wrong or stale:**
  * `re/notes/dungeon.md` section 1, tail table row `0x0FD0` "sólo se cobra si el despachador devolvió 0", and the identical
    docblock `game/src/core/world/survival.ts:72`: `[bp-0xe]` is the kernel `0x39fc` party-state, written at `0x0fab`
    (`call 0xffffb82c ; mov [bp-0xe],ax`), BEFORE the key is read; it is not the dispatcher's result.
  * `re/notes/dungeon.md` line 70 "NO gasta turno": true only of `0x0c76`/housekeeping, not of the clock.
  * `game.ts:2166-2170` "en pueblo/mazmorra se imprime sin turno (no derivado)": now derived. Dungeon = world turn skipped (+ loop
    minute); town = a real 1-minute town turn (section 2.6), so "town prints without a turn" is itself a divergence (not D-86).
  * `dungeon-cmds.ts:313-315` ("(A)ttack ... no corre reloj"): same class, the loop-top minute is charged after Attack in the binary.
  * ledger D-86 / ALPHA4_UI.md 2957, 3337, 3349: correct that native runs a stray overworld turn; "passes none" should read "passes no world turn".
  * **Tooling:** `re/tools/callers_banda.py` line 18 (and the task brief's base table) give `DUNGEON.OVL: 0xe1e0`. The correct
    base is **0x81D0**. Shipped tool output for `callers_banda.py 0x4080` lists only TOWN `0x0ef3` and MAINOUT `0x0c0c` and **omits
    DUNGEON `0x07cb`** (I ran it); for `0x3178` it omits `DUNGEON 0x07a3`. Any by-band census of a kernel routine through
    the stock table silently drops every DUNGEON caller.

Confidence: high on all dungeon-side binary facts (re-disassembled, bytes dumped). Static only; no DOS run exists.

---------------------------------------------------------------------------------------------------

## 2 Binary facts

### 2.0 Tooling controls (positive controls and the overlay base)

* Kernel image = ULTIMA.EXE after the MZ header (`hdr 0x800`, image `0x86f0` bytes).
* **DUNGEON.OVL base is 0x81D0.** Evidence, all reproduced by me:
  * `findings/d86f/basecheck.py`: of the 199 negative-rel `E8` sites in DUNGEON.OVL, with base `0x81d0` **120 resolve to a
    `55 8b ec` kernel prologue**, 21 to the thunk table (`0x7a00-0x7e00`), 58 inside the overlay (>= `0x81d0`); with `0xe1e0`,
    `0xbfec`, `0xbf80`, `0xa290`, `0x8304`: **0** prologue hits each.
  * Kernel thunk `K:0x7a16` = `lcall loader / id 3 / ljmp 0:0x8ffe` (`K:0x7a1d: ea fe 8f 00 00`); `0x8ffe - 0x81d0 = 0x0e2e` = the
    prologue `55 8b ec 83 ec 0e` of the dungeon loop (`OVL:0x0e2e`). Its only caller is `K:0x010e call 0x7a16`.
  * Control: the by-band census with base `0x81d0` for `0x3178` gives DUNGEON `0x07a3`, MAINOUT `0x0c00`, TOWN `0x158f`, the set
    `re/notes/dungeon-dispatch-gates.md` section 1 quotes. For `0x4f7c` it finds DUNGEON `0x0f2f`, MAINOUT `0x0c3d`, TOWN `0x15d4`.
  * `thunks.py --bases` prints 0x8304 for id 3 (it takes the minimum thunk target); it is not the load base.
* Call arithmetic: a near `call rel16` at overlay offset `o` targets `(o + 3 + rel16 + 0x81D0) & 0xFFFF`.
  `OVL:0x07cb call 0xffffbeb0` -> `(0xbeb0 + 0x81d0) & 0xffff` = **K:0x4080**. `OVL:0x0f2f call 0xffffcdac` -> **K:0x4f7c**.
  `OVL:0x0caf call 0xffff9ec2` -> **K:0x2092** (the LCG: `K:0x209b add ax,0x9248 / ror ax,1 x3 / xor ax,0x9248 / add ax,0x11`,
  state word `[0x5420]`). `0x0e1f call 0xffffa730` -> K:0x2900 (redraw), `0x0e22 call 0xffffa918` -> K:0x2ae8 (housekeeping).
* DATA strings (DS + 0x10 verified byte for byte): `0xa396 "Set Active Plr:\n"`, `0xa3a8 "None!\n"`, `0xa3b0 "Invalid!\n"`.

### 2.1 DUNGEON.OVL key dispatcher `0x06c4` (`ret 2`; only caller `0x0f42`)

```
06c4 push bp / mov bp,sp / sub sp,4 / push si
06cb mov word ptr [bp-2],1            ; default result 1 ("a turn passed")
06d0 mov ax,[bp+4]                    ; the key (16-bit word)
06d3 cmp ax,0xb / je 0x6f2 / jle 0x6dd / jmp 0x7a8
06dd cmp ax,1 / jge 0x6e5 / jmp 0x7bc ; key < 1 -> digit gate
06e5 cmp ax,4 / jle 0x744 ; cmp ax,5 / je 0x710 ; jmp 0x7bc
07a0 push [bp+4] / call 0xffffafa8 (K:0x3178) / jmp 0x771     ; everything non-digit: kernel dispatcher, result = its return
07a8 cmp ax,0xd/je 0x744 ; 0x13/je 0x776 ; 0x16/je 0x73a ; 0x2e/je 0x744
07bc cmp word ptr [bp+4],0x30 / jb 0x7a0
07c2 cmp word ptr [bp+4],0x39 / ja 0x7a0          ; UNSIGNED compares: digit keys are exactly 0x30..0x39
07c8 push word ptr [bp+4]                          ; ff 76 04
07cb call 0xffffbeb0                               ; e8 e2 b6   -> K:0x4080
07ce mov word ptr [bp-2],ax                        ; 89 46 fe   kernel result stored ...
07d1 mov word ptr [bp-2],0                         ; c7 46 fe 00 00   ... and overwritten with 0
07d6 jmp 0x70a                                     ; e9 31 ff   070a: mov ax,[bp-2] ; jmp 0x7da -> ret 2
```
Raw bytes `0x07c8..0x07d8`: `ff 76 04 e8 e2 b6 89 46 fe c7 46 fe 00 00 e9 31 ff`. The result is a 16-bit word, 0 for every digit.
Key codes 1..5 (arrows, `0x06e5/0x06ea`) are not digits; `0x03d6` adds `0xfa` to keys 1..4 when `[0x538a]==0`, so they never reach the
digit gate either way. ENTER/'.'/^S/^V are handled before `0x07bc`.

### 2.2 Kernel `K:0x4080` (set active player; `ret 2`)

```
4087 mov [bp-2],1                    ; return default 1
408c mov ax,[bp+4] ; sub ax,0x31 ; mov [bp-4],ax      ; idx = key - '1' (16-bit)
4095 push 0xa396 ; call 0x1850                        ; prints "Set Active Plr:\n" ALWAYS
409c cmp word ptr [bp+4],0x30 ; jne 0x40b8
40a2 push 0xa3a8 ; call 0x1850                        ; "None!\n"
40a9 mov byte ptr [0x587b],0xff                       ; g_active_char = none
40ae call 0x2900                                      ; status-panel redraw
40b1 mov word ptr [bp-2],0 ; jmp 0x40f7               ; return 0
40b8 mov al,[0x585b] ; sub ah,ah ; cmp [bp-4],ax ; jae 0x40f0     ; idx >= party_size  (unsigned word vs zero-extended byte)
40c2 mov si,[bp-4] ; shl si,5                                     ; record stride 0x20, base 0x55a8
40c9 cmp byte ptr [si+0x55b3],0x44 ; je 0x40f0                    ; 'D'
40d0 cmp byte ptr [si+0x55b3],0x53 ; je 0x40f0                    ; 'S'
40d7 mov al,[bp-4] ; mov [0x587b],al                              ; g_active_char = idx
40dd lea ax,[si+0x55a8] ; push ax ; call 0x1850                   ; prints the record's name (rec+0)
40e6 push 0xa ; call 0x16ba                                       ; '\n'
40ed jmp 0x40ae                                                   ; redraw, return 0
40f0 push 0xa3b0 ; call 0x1850                                    ; "Invalid!\n"   (return stays 1, NO redraw, NO write)
40f7 mov ax,[bp-2] ... ret 2
```
Outcome table (dungeon):

| key | member state | text printed | `[0x587b]` | redraw `0x2900` | kernel ret | **dungeon ret** |
|---|---|---|---|---|---|---|
| `'0'` | any | `Set Active Plr:\n` `None!\n` | `0xff` | yes | 0 | **0** |
| `'1'..'9'`, idx >= `[0x585b]` | none (also an inn companion beyond party size) | `Set Active Plr:\n` `Invalid!\n` | unchanged | no | 1 | **0** |
| digit, idx < size, status `'D'` | dead | same Invalid | unchanged | no | 1 | **0** |
| digit, idx < size, status `'S'` | asleep | same Invalid | unchanged | no | 1 | **0** |
| digit, idx < size, other status (`G`, `P`, ...) | valid | `Set Active Plr:\n<name>\n` | `idx` | yes | 0 | **0** |

`'7'..'9'` are not special: idx 6..8 are `>= [0x585b]` for any possible party. Size 0 makes every digit but `'0'` invalid. Only the byte
`[0x585b]` bounds the index (no check against 6, no check against the roster count). RNG: the static call closure of K:0x4080
(printing, putchar, `0x2900` and their subtrees) contains neither `0x2092` nor `0x4f7c`; the display driver is reached only through
`lcall [0x5350]` (indirect, not followed). RNG draws: 0.

### 2.3 Dungeon main loop `OVL:0x0e2e` (the consumer of the result)

```
0f1e cmp byte [0x587a],0x51 ; jne 0xf34 ; xor di,1 ; je 0xf36 ; mov ax,1        ; 'Q': tick every second pass
0f2e push ax
0f2f call 0xffffcdac                      ; K:0x4f7c advance_clock(arg)   <- THE ONLY advance_clock IN DUNGEON.OVL
0f32 jmp 0xf36
0f34 sub di,di                            ; 'T': no tick, di = 0
0f36 call 0x03d6                          ; blocking getkey (prints '\n' + prompt, loops until key != 0; closure has no rand)
0f39 mov [bp-8],ax ; cmp ax,0xffff ; jle 0xf47
0f41 push ax ; 0f42 call 0x06c4 ; 0f45 mov si,ax          ; si = dispatcher result
0f47 cmp byte [0x5893],0x21 ; jae 0xf53                   ; else si=0, [bp-0xc]=0 (left the dungeon)
0f53..0f78 read the cell under the party -> [bp-2]/[bp-6]
0f7b or si,si ; je 0xf87                  ; si == 0  -> world turn SKIPPED
0f7f mov al,[bp-2] ; push ax ; push di ; 0f84 call 0xc76   ; world turn (the ONLY caller of 0x0c76)
0f87 cmp byte [0x5893],0x21 ; jae 0xf93 ; else si=0,[bp-0xc]=0
0f93 cmp [bp-0xc],0 ; je 0xff2                            ; loop head
0f99 cmp byte [0x5893],0x20 ; jbe 0xff2                   ; location <= 0x20: leave
0fa0 mov [bp-0xc],1 ; 0fa5 mov si,1                       ; si OVERWRITTEN here: the dispatcher result is not consulted again
0fa8 call 0xffffb82c ; 0fab mov [bp-0xe],ax               ; K:0x39fc party state (see below)
0fae cmp ax,si ; jne 0xfc3 ; (==1: putchar('\n'), call 0x4c2a, print DS 0x2ddd "Zzzzzz...\n")
0fc3 cmp [bp-0xe],0 ; jge 0xfd0 ; mov [bp-0xc],0 ; sub si,si
0fd0 cmp [bp-0xe],0 ; jne 0xf93                           ; party state != 0: no key, no clock, back to the head
0fd6 cmp byte [0x587a],0x54 ; jne 0xfe0 ; jmp 0xf1e       ; 'T' -> 0xf1e (-> 0xf34 no tick)
0fe0 cmp byte [0x587a],0x51 ; jne 0xfea ; jmp 0xf1e       ; 'Q'
0fea mov di,1 ; 0fed mov ax,di ; 0fef jmp 0xf2e           ; bytes e9 3c ff: 0xff2 - 0xc4 = 0xf2e  -> advance_clock(1)
```
* `K:0x39fc` (read in full `0x39fc-0x3a6d`): loops `[0x585b]` records; status `'G'` or `'P'` -> returns 0 immediately; counts `'S'`;
  returns 1 if only sleepers, `0xffff` if none. So `[bp-0xe]` = 0 in all normal play.
* `si` (the dispatcher result) gates exactly one thing: the call to `0x0c76`. The clock call `0x0f2f` sits before the key read and is
  reached after every command, whatever `si` was. The key's own world turn (`0x0c76`, which ends with `K:0x2900` and K:0x2ae8) is
  what a digit skips.
* `K:0x4f7c advance_clock(n)` read in full: `n == 0` returns; `[0x587a]=='Q'` -> `n >>= 1` (min 1); `[0x5880] = [0x587f]` (previous
  hour) on every call; `'T'` skips the add; `[0x5881] += n` (minute), light/torch counters `[0x58a7]`,`[0x58a6]` decremented through
  K:0x3f36; minute `> 0x3b` -> `-0x3c`, hour `[0x587f]++`; hour `> 0x17` -> day rollover whose Shadowlord relocation calls
  `rand(1,8)` (`K:0x5004-0x500c`, conditional on `[bx+0x58c8] < 0x80`). RNG draws of an ordinary minute: 0.
* Entry: `0x0f15 ... jmp 0xf93` so the first iteration also pays one minute before the first key; leaving the dungeon (location <= 0x20)
  skips the minute that would have followed the last key.

### 2.4 The world turn `OVL:0x0c76` (skipped for every digit; context only)

Per roster member `[0x585b]` with status `'S'` (`0x0ca3 cmp byte [si],0x53`): `rand(0,0x3f)` (`0x0caa push 0 / 0x0cab push 0x3f /
0x0caf call 0x2092`), `< 4` -> status `'G'`; `if di != 0` the wanderer step; the tile dispatch; common tail `0x0e1f call 0xa730`
(K:0x2900 redraw) and `0x0e22 call 0xa918` (K:0x2ae8 housekeeping). None of it runs for a digit.

### 2.5 Caller census (my own, with controls)

`findings/d86f/census.py`: every `E8`/`E9` in ULTIMA.EXE (base 0) and in every `*.OVL` under every candidate overlay base, over-reporting,
never missing.

| target | callers | relevant |
|---|---|---|
| K:0x4080 | **`DUNGEON.OVL 0x07cb`, `MAINOUT.OVL 0x0c0c`, `TOWN.OVL 0x0ef3`**; none in ULTIMA.EXE; no word `0x4080` in EXE/DUNGEON/MAINOUT/TOWN (no indirect use) | all three; DUNGEON is the item |
| K:0x3178 (control) | DUNGEON `0x07a3`, MAINOUT `0x0c00`, TOWN `0x158f` (+ false positives under non-native bases) | matches the notes |
| K:0x4f7c | DUNGEON `0x0f2f` (only one); EXE `0x3cfc 0x6356`; MAINOUT `0x0006 0x0066 0x0462 0x068c 0x0a56 0x0c3d`; TOWN `0x0511 0x1328 0x15d4`; CAST2 `0x10f1`; SHOPPES3 `0x01c1 0x01db` (+ others under their own bases) | DUNGEON `0x0f2f` only |
| DUNGEON `0x06c4` | `0x0f42` only | yes |
| DUNGEON `0x0c76` | `0x0f84` only | skipped for a digit |
| DUNGEON `0x0e2e` | thunk `K:0x7a16` <- `K:0x010e` | context |

### 2.6 Contrast (verified here, so the fix stays dungeon-only)

* **MAINOUT** `0x0c06-0x0c3d`: `call 0xffffbeb0 ; 0x0c0f mov [bp-8],ax` ... `0x0c20 cmp byte [0x5893],0 ; je 0xc30` (location 0 only) ...
  `0x0c30 cmp [bp-8],0 ; jne 0xc39 ; jmp 0xd14` -> `0x0c39 push 2 ; call 0xffffcdac` = `advance_clock(2)`. Invalid digit above ground
  = a full overworld turn (both ports model it; the `gameplay_parity` location-0 rows pin it).
* **TOWN** `0x0ef0-0x0ef6` returns K:0x4080's value; loop `0x159a mov [bp-0xa],ax ; 0x15b6 cmp [bp-0xa],0 ; jne 0x15bf` ->
  `0x15bf call 0xffffb82c ; inc ax ; jne 0x15c8` -> `0x15d0 push 1 ; 0x15d4 call advance_clock`. An invalid digit in a TOWN passes a
  1-minute town turn in the binary; both ports give none. Adjacent divergence, NOT D-86 (candidate new ledger row; pinned by the location-1
  rows and `game.test.ts`, see section 5).

---------------------------------------------------------------------------------------------------

## 3 Reference (TypeScript) exact change needed

* `game/src/core/game.ts` `setActivePlayer`, lines 2174-2200. The defective statement is **line 2187**:
  `if (this.state.position.location === 0) { events.push(...this.runContextTurn({ consumed: true })); }` (inside the invalid branch
  `2184-2191`; `"Invalid!"` is pushed at 2185).
* Change: `if (this.state.position.location === 0 && !this.dungeonState) {`. (`this.effectiveLocation === 0` is equivalent; `dungeonState`
  is the explicit discriminator, `game.ts:858`, used by `Ignite`-style guards at 1386/2984.) Nothing else in the function changes.
  `digit == 0` (2176-2180) and the valid branch (2192-2199) already match the binary.
* Update the stale docblock `game.ts:2166-2170` (dungeon is now derived: world turn skipped; town still "no derivado" or, if the owner takes
  the town item, derived as a 1-minute town turn). `game/src/main.ts:1612-1616` already says "retorno FORZADO a 0 (@0x07d1) = SIN turno" and
  `main.ts:1617-1631` (dungeon digits, `hud.echo(CMD_STRINGS.setActive); applyEvents(game.setActivePlayer(Number(ev.key)))`) needs no change.
* TS effect: today the residue is a surface `doors?.tick()` + `openDoors` re-serialise, `tickNpcs` (no-op at location 0) and `checkRefuge()`
  (returns `[]` unless the whole party is unconscious). After the fix none run. No state field the harness projects changes, so no TS
  expected value moves.
* Not part of D-86 (do not do it by accident): the loop-top clock minute for dungeon digits (see 7), the town turn.

## 4 Native exact change needed

* `native/core/src/commands.cpp`, `execute`, arm `774-784` (`!c.combat && (NewOrder || SetActivePlayer)`), defective statement **line 781**:
  `...{r.message("Invalid!");if(!g.position.map.location)r.turn();}`
* Change: `if(!c.dungeon&&!g.position.map.location)r.turn();`. `c.dungeon` is `CommandContext::dungeon` (`commands.h:138`), the discriminator
  `Ignite`/`ViewGem` already use at **`commands.cpp:785-789`** (`if(c.dungeon){advance_turn(...)}else r.turn();`). It is set by
  `dungeon_orchestration.cpp:144` (`c.dungeon = true` after `dungeon_load`) and cleared by `exit_dungeon` (`:75`); on the device
  `context_.dungeon = dungeon_.active` (`alpha_runtime.cpp:1143, 1709`).
* Why this is routing only: `execute` never routes SetActivePlayer to `execute_dungeon_command` (arm `664` takes only EnterDungeon /
  DungeonCommand / dungeon Cast / UseItem) nor to the arena (arm `673-675` needs `c.combat`), so the command lands in arm 774 in a dungeon.
  `ui_session.cpp:1201-1204` (`handle_dungeon`: `command_echo("Set Active Plr:"); c.kind=SetActivePlayer; c.member=int16_t(k-'0')`) and
  `ui_input_adapter.cpp` only deliver the digit and print the echo; neither ticks. `UiSession`'s own comment `1198-1200` ("return forced
  to 0 = no turn") is already correct. Device path: `alpha_runtime.cpp:1081 dispatch_world_command` -> `commands.cpp:1316` -> `execute`.
* How the stray tick arrives (source-derived, not run): a dungeon session never writes `game.position.map` (`dungeon_load`,
  `dungeon.cpp:271-293`, only writes the `DungeonState`); `exit_dungeon` sets `{0, under?255:0}` at `dungeon_orchestration.cpp:76`. So
  `position.map.location == 0` for the whole session and `Runner::turn` (`commands.cpp:199-252`, branch `202`) runs `outdoor_turn`
  (`turn.cpp:156-195`) on the surface tile: `maybe_change_wind` `rand(0,63)` (unless time spell `'T'`), `advance_clock(...,2,...)`,
  lava branch if the surface tile is 143, `if(position.map.floor!=0 && rand(0,255)==105)` hazard (underworld-origin dungeon), `housekeeping_into`
  (poison, hour-boundary food, `++turns_since_start`, ring draws); then `effect(WaterfallUnder)`, `effect(Doors)`, `actors(false)`,
  `world(under)` (`roll_spawn_gate` + `outdoor_tick`: can spawn or advance overworld enemies and start an outdoor fight), `effect(Refuge)`,
  `effect(Waterfall)` (`commands.cpp:93-`: if the surface position is `(0x36,0x8a)` it can even set `floor = 255`, "Falling into underworld!!").
  `result.turns` stays 0 on this arm (return at 783 precedes the `turns` assignment at 1312), so telemetry hides it.
* After the fix: Invalid prints `"Invalid!"` and mutates nothing (no clock, no RNG, no housekeeping), in a dungeon only. Overworld
  (`location==0`, `!c.dungeon`, also floor 255) keeps its turn; town (`location != 0`) keeps none; combat is a different arm.
* Native-only extra guards (unobservable on legal data, leave alone): `775` `character_count>kRosterCapacity || party_size<0 || party_size>kMaxParty`
  returns InvalidContext; `781` `i>=character_count`. The valid branch trims whitespace and falls back to `"Avatar"` (name, not D-86).

---------------------------------------------------------------------------------------------------

## 5 Parity fixtures and corpora that pin it

Searched: `native/core/tests`, `native/core/fixtures`, `native/core/CMakeLists.txt`, `native/core/tools`, `native/targets/tdeck/host_tests`,
`game/src/core/__parity__`, `game/tests`. No fixture exercises a dungeon digit with an invalid member.

**Must stay byte-identical (guards, and they stay green after the fix):**
* `gameplay_parity` (`CMakeLists.txt:3006`; `native/core/tools/check-gameplay.ts:130`, driver `native/core/tests/gameplay_driver.cpp:88`
  kind `active`): `for location of [0,1] x status [G,S,P,D] x name ['','  Iolo  ','Shamino'] x digit 0..7` = 192 sequences, no `dungeon` key
  (`gameplay_driver.cpp:65-66` sets `ctx.dungeon=true` only when `q.has("dungeon")`). Location-0 rows pin the overworld invalid-digit turn
  (correct per MAINOUT `0x0c39`); location-1 rows pin the town "no turn" (WRONG per the binary, section 2.6, but untouched by D-86).
  So the discriminator must be `c.dungeon`, not a blanket removal of `r.turn()`, and not a location-1 change.
* `native/core/tests/batch15_sacrifice_test.cpp:298-330` C1/C2 (default context, location 0, valid and invalid digit through `execute_command`).
* `native/core/tests/batch21a3_combat_active_player_test.cpp` (arena arm, different routine and strings), `batch3_group_a_test.cpp:105-144`
  and `host_tests/input_test.cpp` (routing only), `host_tests/dungeon_input_test.cpp` D-IN-2h (intent only).
* `game/tests/game.test.ts:449-497` (`location: 1` town: "Invalid no corre world_turn"; digits 0/valid/out of range/dead).
* `host_tests/batch21a_dungeon_room_test.cpp:462-485` RED-6 (`f.key('2')` in the corridor via the real UiSession: a VALID digit sets
  `active_character == 1`; the following auto-pass check is the arena). Unaffected.
* `fixtures/commands.txt`/`commands-coverage.json`, `dungeon*.txt`, `game/src/core/__parity__/*`: contain no set-active row.

**What must change:** nothing existing. Only new rows/tests (section 6). The ledger text at ALPHA2_PRESERVATION_LEDGER.md:231 and ALPHA4_UI.md 2957/3337
should be re-worded (see section 1) and D-86 closed; the `gameplay_parity` sequence count in the ledger (`ALPHA2_PRESERVATION_LEDGER.md:69`, 5,058) changes by the
number of rows added.

---------------------------------------------------------------------------------------------------

## 6 Test plan

### 6.1 RED-first rows (the repo's existing oracle shape)

Append to `native/core/tools/check-gameplay.ts` **after the last `add(...)` (line 139)**, not next to line 130, so no existing sequence index (the `Gameplay mismatch i`
number and `GAMEPLAY_START`) shifts. Builder is the one at lines 43 and 126/139:

```
for(const under of [false,true])for(const status of ['G','P','S','D'])for(const digit of [0,1,2,3,4,7,9])
 add(7,68,s=>{s.position.location=0;if(under)s.position.floor=255;s.characters.push({...s.characters[0],name:'Shamino',status});s.partySize=2;},false,'flee',
     {dungeon:{location:33,cells:Array(512).fill(0)},actions:[{kind:'active',member:digit}]});
```
plus rows with `timeSpell` `'T'` and `'Q'`, a row with `partySize` 6 and digits 7..9, and a row with `characters.length > partySize` (inn companion, digit
`partySize+1`). The TS side (`g.dungeonState` set, `position.location 0`) is state-neutral today; the native side advances time (+2 min), `turnsSinceStart`, the live seed
(wind roll, spawn gate, plus a hazard draw when floor 255), may alter food/poison/enemies. **RED = the native/TS comparison fails on `time`, `turnsSinceStart`, seed
for every INVALID row (S, D, out-of-range digits), passes for `'0'` and valid members.** After line 781 gets `!c.dungeon` the whole set passes with no TS edit,
because the TS residue is not projected.

### 6.2 TS-only RED (the reference residue the harness cannot see)

`game/tests/game.test.ts` next to the 449-497 block: a Game with `position.location 0`, `dungeonState` set, doors loaded with a pending timer (or
`vi.spyOn(game as any, "runContextTurn")`), `partySize 2`, call `setActivePlayer(5)`: expect `["Invalid!"]`, `runContextTurn` not called / door timer unchanged / no refuge
event even with an all-unconscious party. RED today (door tick + refuge check run), GREEN with `&& !this.dungeonState`.

### 6.3 Boundary cases (binary-derived expected results, dungeon context, party size N)

| input | state | expected |
|---|---|---|
| `'0'` | any | `None!`, active `255`, no turn (already true) |
| `'1'`, N=1 | G | name, active 0 |
| `'2'`, N=1, roster has a living slot 1 | beyond party size | `Invalid!` |
| `'6'`, N=6 | G or P | name, active 5 |
| `'7'`,`'8'`,`'9'`, N=6 | any | `Invalid!` |
| digit within size | `'S'` | `Invalid!`, active unchanged |
| digit within size | `'D'` | `Invalid!`, active unchanged |
| digit within size | `'P'` | valid (status P is not refused) |
| `'1'`, N=0 | - | `Invalid!` |
| invalid digit, time spell `'T'` and `'Q'` | any | no turn either way (the clock rule is separate) |
| invalid digit, overworld (`location 0`, `!dungeon`, floor 0 and 255) | any | **still a turn** (regression guard, existing 96 invalid rows) |
| invalid digit, town (`location 1`) | any | unchanged by D-86 (no turn in both ports) |
Asserted per case: messages (the UiSession echo is separate), `active_character`, `time`, `turns_since_start`, live RNG seed, food, HP, poison, overworld enemy count,
dungeon `pos`, `position`.

### 6.4 Mutations a good test must kill

1. Delete `&&!c.dungeon` (or the TS `&& !this.dungeonState`): the dungeon rows must go RED (native: time/turns/seed; TS: spy/door test).
2. Replace the discriminator with a blanket removal of `r.turn()` from the invalid branch: location-0 rows (overworld) must go RED.
3. Replace `!g.position.map.location` by `!c.dungeon` alone: the town (location 1) rows must go RED (the town would gain a turn).
4. `!c.dungeon` -> `!c.dungeon_context` (null in the harness' non-dungeon rows, set in dungeon rows): must stay equivalent only if the context is always set with `dungeon`; the
   dungeon row set kills any version that checks the wrong flag.
5. Flip the `'S'`/`'D'` refusal or the `'P'`-valid rule: S/D/P status rows must go RED.
A RED-first run must show the native failure BEFORE the one-line fix and green after, with the location-0/1 rows green throughout.

---------------------------------------------------------------------------------------------------

## 7 Residual unknowns (exact)

1. **Static only.** No DOSBox/original run exists for "invalid digit in a dungeon, compare `[0x5881]`/`[0x587f]` (minute/hour) before and after". The bytes are unambiguous
   (`0x0f87 -> 0x0f93 -> 0x0fd0 -> 0x0fea -> 0x0f2e/0x0f2f`), but a keystroke trace is the only dynamic witness for the loop-top minute and for `0x4080`'s redraw/text order.
2. **Decision (not a binary question): scope.** D-86 as written (the stray overworld turn) is closed by the one-line change in 3 and 4. The binary-exact model additionally charges
   `advance_clock(1)` alone (no housekeeping, no world turn) for every dungeon key that returns 0 (digits, Z, Look, Attack, turning...), `'T'` none, `'Q'` every second key, and
   charges it AFTER the key (loop top) rather than before as `dungeon_orchestration.cpp:191-193` (`advance_turn` before the action, Attack excepted) and
   `dungeon-cmds.ts:326` do. That is a class change touching many dungeon fixtures (`DungeonCommand` charges clock+housekeeping for all but Attack today), must be done reference-first, and
   needs its own ledger row; doing it for digits alone would leave an inconsistent island. Native `advance_turn` bundles housekeeping the binary gates on `si != 0`, so a clock-only
   helper (`advance_clock(g, c.turn, 1, &r.rand, c.sky)`) would be needed.
3. **Town invalid digit** costs a 1-minute town turn in the binary (`TOWN 0x15b6-0x15d4`); both ports and `game.test.ts` + the location-1 `gameplay_parity` rows say no turn. Needs a separate row
   (suggest D-90); not touched here.
4. **`0x0fd0 jne 0xf93`**: when K:0x39fc returns 1 (whole party asleep) the loop re-enters `0xf93` with no key read and no clock; it prints "Zzzzzz...\n" via `0x4c2a` each pass. Read literally it
   never exits unless something changes the roster; `0x4c2a` was not chased. It does not touch the digit path.
5. The display driver under `0x16ba`/`0x1850` (`lcall [0x5350]`) was not followed; it cannot be proven statically not to read `[0x5420]`, but no call site suggests it does. The idle key-wait redraw in `0x03d6`/`0x111e`
   draws `rand(0,100)` per animation frame (time-driven; `re/notes/dungeon.md` section 3 declares it out of scope), so "RNG draws per digit" is 0 for the handler, not for wall-clock time spent waiting.
6. Native stray-effect counts (>=2 draws, +2 min) are source-derived, not run; the RED rows will show the exact numbers. The waterfall branch (`commands.cpp:93-`) only matters if a dungeon entrance sits at `(0x36,0x8a)`; not checked.
7. Tooling: `re/tools/callers_banda.py` base for DUNGEON.OVL (`0xe1e0`) should be `0x81d0`; the other table bases were not re-audited by me beyond what the census needed (B's prologue scoring suggests several other overlays
   also disagree with the table). Not changed (read-only).
8. Items where A and B differed, settled by me: (a) base-check statistics (A 36/36 non-thunk, B 166/190): both are correct for their counting; mine is 120/199 prologue + 21 thunk + 58 in-overlay, 0 for 0xE1E0. (b) TS gate spelling
   (`effectiveLocation === 0` vs `!dungeonState`): equivalent; I recommend `!dungeonState`. (c) A wrote `survival.ts:77`; the offending table row is at `survival.ts:72`. (d) B calls RED-6 a "valid dungeon digit" test: it is, in the corridor
   via the real UiSession (`f.key('2')`), before the room arena starts. (e) Option A vs Option B (A's naming): both reports reach the same recommendation; I adopt Option A for D-86 and carve the clock class out.
