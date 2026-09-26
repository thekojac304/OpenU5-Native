# Batch 53B — stepping back onto the moongate you arrived through

Question from Phase 7E-C (Batch 53 image): Britannia 96,103, hour 21, the gate drawn one cell North;
stepping onto it transits (PASS). At the destination a gate is still drawn, but stepping back onto
it "does not transport the party back". Is the original's return trip missing on the device?

**Answer: there is no return trip in the original.** Every gate leads to the stone the *moons*
select, never to the gate you came from. During the same phase that is the stone the party just
arrived on, so stepping back onto the destination gate transits the party onto itself. The device
does exactly that (the gate fires; the party stays put); the only thing it lacks is the close/open
animation, which is D-48 (Alpha 3). **Outcome A.**

Evidence: `re/tools/batch53b_moongate_return.py` (reads `original/u5/ultima5`, asserts every
claim below; output committed as `native/core/batch53b-original-moongate.log`).

## 1. Where the gate is tested

| fact | bytes |
|---|---|
| MAINOUT.OVL is overlay 2 and loads at **0x81d0** | thunk `0x7a3a` → ovl 2, `ljmp 0x8ef2` = MAINOUT `0x0d22` (prologue), which the kernel dispatcher calls at `0x00c4` when `g_location == 0`. MAINOUT's own near calls resolve to kernel prologues only at 0x81d0 (`0x16e0`→`0x4402`, `0x05a0`→`0x5910`, `0x0aa2`→`0x39fc`); at the old 0x8304 none do. `re/tools/callers_banda.py` carried 0x8304 and so missed every MAINOUT caller — corrected in this batch. |
| the outdoor main loop tests the gate at the top of **every** iteration | MAINOUT `0x0a84` … `0x0b00 call 0x48a8` (unconditional) … `0x0b14 call 0x0598` (key read) … `0x0d1a jmp 0x0a8f` |
| no "just arrived" guard outdoors | MAINOUT never references `[0xa9bc]`. The only such flag is the TOWN loop's one-shot skip (`TOWN 0x1468`, set by the town loader `0x11f0`), i.e. one skipped check after a town load. |

## 2. What fires it and where it goes

- `kernel_moongate_enter` `0x48a8` reads the **map-buffer** cell under the party
  (`0x48bd call 0x4402`, `0x48c2 cmp byte [bx],0xDC`). Only the night render
  `kernel_moongate_render` `0x475a` writes 0xDC there — on every stone `0x4702` reports visible
  (this location, this floor, in the window) — and it runs inside the screen update `0x5910`
  (`0x594e`), which the key read calls first (MAINOUT `0x05a0`). Visibility and activity are
  therefore one predicate: a drawn gate is an active gate.
- Presentation, then `0x493c` turns the cell under the party into grass (5), then the midnight edge
  (`0x494d`, 00:00–00:09: no jump), then the destination: `0x4962` hour < 12 → `[0x5885]`
  Felucca, else `[0x5886]` Trammel, `- 0x30`, `0x4977 call 0x47f4`.
- `kernel_moongate_teleport` `0x47f4` copies that stone's location / x / y / floor
  (`0x483d`–`0x4856`). The origin's x/y are read only for the two tile pointers; neither routine
  calls `advance_clock` `0x4f7c` or the phase refresh `0x4a84`, and neither writes the latches.

So the destination is a function of the clock alone. At the destination, same phase ⇒ same stone
⇒ the party's own cell. The origin gate is reachable again only when the moons select its phase
(Felucca/Trammel bytes of DATA.OVL `0x1EEA`, latched at surface hour boundaries by `0x4a84`), or
by Vas Rel Por.

## 3. Standing on a gate (declared divergence, D-58)

Because the test is at the loop top and the paint happens during the key read, the original fires
again on the **next iteration** whenever the party is still on a drawn gate — after a Pass, a
Look, any non-moving command, or when night falls on a party standing on a buried stone. It never
fires twice in one iteration, and it never fires on arrival (the jump reloads the map buffer:
MAINOUT `0x0000` via `0x7b7e`, so the arrival cell is terrain until the next paint). The
TypeScript reference (`Game.move` → `checkMoongate`) and the native core
(`commands.cpp` `move()` → `CommandEffect::Moongate`) test only after a successful step. With the
phase unchanged the outcome is identical (the party is sent onto itself); it differs only when the
phase changes while the party stands on a gate (midnight, nightfall). Not the Phase 7E-C case; not
changed in 53B. A fix belongs in the reference first (the parity oracle pins the step-only
trigger), then native.

## 4. Device side finding (D-59)

The device never binds `CommandContext::sky` (`SkyRefresh`); the parity driver
(`quest_driver.cpp:51`) does. The moon-phase latch is therefore never refreshed on the device.
Device saves carry no valid latch (`-1`), so `active_gate_phase` always falls back to the day's
table bytes — correct for every same-day case including 7E-C, but it cannot reproduce the
original's "yesterday's phases until the next surface hour" window, and a save that did carry a
valid latch would freeze the gates. Queued; not changed in 53B.
