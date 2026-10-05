> *Scope: this method document comes from the OpenU5 browser/reference implementation and its reverse-engineering corpus, which Ultima V Native also draws on. Test commands here refer to OpenU5.*

# Methodology — how the original engine was re-derived

This document describes how every gameplay rule in this port was recovered from
the original `ULTIMA.EXE` (DOS, 1988) and how we prove the port implements it.
It is the answer to the only question that matters for a project that claims
byte-exactness: *"how do you know?"*

> **The Golden Rule:** no gameplay change is accepted without provenance.
> Every rule in the engine core cites the original assembly (overlay + offset)
> or a parity scenario that proves it. If the port contradicts the derived
> rule, the port is wrong — never the other way around.

## 1. The coverage ledger — 202,800 / 202,800 bytes

The original engine is `ULTIMA.EXE` (34,544 bytes) plus 24 code overlays and
`DATA.OVL` — **202,800 bytes total**. The coverage ledger
(`re/ledger/*.json`, driven by `re/tools/ledger.py`) assigns every single byte
to exactly one segment classified as `code` / `data` / `inert`, each with a
name and a note explaining what it is:

- **835 segments** across 26 files: 75.04% code, 24.89% data, 0.07% inert
  (linker padding, each block annotated with its size).
- Overlays are partitioned by function prologue (`55 8B EC`); data tables are
  identified by the overlays' access patterns; `DATA.OVL` (48,464 bytes of
  strings and numeric tables) is catalogued 100% in chunks.
- The invariant checker rejects overlapping or missing ranges:
  `python3 re/tools/ledger.py` must report `202800 / 202800  100.0%`.

Honesty note: the ledger is a **coverage** instrument, not a claim that every
kernel function is understood. Kernel functions no ported subsystem depends on
keep a provisional `kernel_fn_<addr>` name with a note saying exactly that.
Every function a subsystem *does* depend on is named, derived, and cited.

The disassembly itself is **not** in the repository — you regenerate it locally
from your own copy with `npm run re:disasm`. What is published is the tooling
and the derived knowledge (notes with short cited excerpts, the ledger JSON,
the parity harnesses).

## 2. Assembly citation discipline

Every rule ported into `game/src/core/` carries a citation of the form
`OVERLAY:0xOFFSET` (e.g. `COMBAT:0x14D6`) next to the code that implements it,
and a longer derivation note under `re/notes/<subsystem>.md`. Two examples of
what this discipline caught:

- The **to-hit formula** is
  `hit ⇔ rand30() >= (defenderDex − attackerStat + 30) / 2` — it uses *both*
  combatants' stats, and blunt weapons roll STR instead of DEX. Neither of the
  well-known reimplementations had this right; the binary did
  (`COMBAT:0x14D6`).
- The **RNG is not a textbook LCG**: it is an add/rotate/xor routine
  (`((seed + 0x9248) ror 3) ^ 0x9248 + 0x11`), reimplemented bit-exactly as
  `OriginalRng`. Getting this wrong desynchronizes *everything* downstream —
  combat rolls, wind, encounters, the gypsy character creation.

When a source like an existing remake disagrees with the assembly, the
assembly wins, and the divergence is recorded (several inherited rules from
community reimplementations were refuted this way: shop haggling uses INT, not
DEX/karma; overworld encounter chance is biome/time-driven, not a flat 1/16).

## 3. The DOSBox-X oracle — runtime verification against the real binary

Static derivation can be wrong. The strongest evidence tier in this project is
**runtime verification**: a harness (`re/tools/`) boots the original game in a
headless DOSBox-X with the debugger enabled, and then

1. **seeds RAM** (RNG seed, party state, world state) to force a deterministic
   scenario,
2. sets **code breakpoints** at the derived function addresses,
3. steps the real binary and reads back globals byte-for-byte,
4. runs the same scenario through the port's pure core and compares every
   value — not just the outcome, but the *order and count of RNG draws*.

Subsystems verified live this way include: the combat trace (to-hit + damage +
poison + HP trajectory, roll by roll), the RNG orbit itself, per-turn wind
values (`kernel 0x2F62`), the lunar-phase feed that drives moongates, gypsy
character creation from seed 0, and the movement/clock/hunger/torch rules.

The live tests are opt-in (they need your own game copy and `dosbox-x`); the
derived rules they anchor are covered by pure tests that run everywhere.

## 4. The parity harness — model ↔ port, one seed, one stream

Between "derived from asm" and "verified live" sits the **parity suite**
(`npm run re:parity:all`, ~2 minutes, **239 tests**): for each subsystem there
is an independent Python model written directly from the assembly, and the
harness runs identical scenarios through the Python model and the TypeScript
core, requiring identical outputs *and identical RNG consumption*.

The capstone is the **master scenario** (gypsy → endgame): character creation,
the outdoor loop, a troll ambush, the town loop and the endgame report chained
through a **single threaded RNG seed**, asserting seed and draw-trace equality
at every phase boundary. It proves the *order* of the binary's RNG stream is
modeled correctly end-to-end.

## 5. What is deliberately not identical

Fidelity claims are only credible if the non-identical parts are documented
with the same rigor. `re/deliberate-divergences.md` catalogues every conscious
divergence in classes (stream-parity-only vs runtime-verified, pending
interactive wiring, open oracle questions, scope boundaries, additive QoL),
each with *what*, *why*, and *how it would be closed*. Examples: the combat
AI's movement RNG is excluded from the live combat trace (its draws can't be
captured reliably over the pty channel), and an ammo-underflow bug in the
original (255 free arrows via a shared ammo pool) is deliberately *not*
reproduced — the port clamps at 0, and says so.

## 6. Test tiers

| Tier | Command | Needs |
|---|---|---|
| 1 — pure | typecheck + data-independent unit tests | nothing (public CI) |
| 2 — assets | `npm test`, `npm run e2e`, `npm run re:parity:all` | your own game files |
| 3 — live oracle | `U5RE_LIVE=1` parity runs | game files + dosbox-x |

Current counts (see `re/COVERAGE.md` for the closing report): 484 game unit
tests, 107 extractor tests, 293 RE-suite tests plus 11 opt-in live tests, and
the 239-test parity suite.

## 7. Why this is legal to publish

The repository contains the port's own code and the *derived knowledge* — no
game data, no art, no text, no maps, and no bulk disassembly listings. The
tooling regenerates the disassembly from **your** legally-owned copy, the
extractor builds the assets from **your** copy, and short cited assembly
excerpts in derivation notes follow the established practice of the
decompilation/preservation scene. See the README's Legal section.
