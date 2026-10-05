# OpenU5 Browser Port Roadmap

> **This document belongs to the OpenU5 browser/reference implementation included in this repository. It is not the roadmap for Ultima V Native** (see [`ROADMAP.md`](ROADMAP.md)).
> References below to shipped browser features, `openu5.org`, mobile/touch UI, skins, localization and browser milestones describe OpenU5 only.

# Roadmap

This is an honest map of where the project is. Items are not vague wishes: each
open line traces to an internal work queue with a defined method, and each
"done" line is locked by an automated suite.

## Done (suite-locked)

- **Rule migration at 100%** — the coverage ledger justifies **202,800 / 202,800
  bytes** of `ULTIMA.EXE` + its 24 overlays (code / data / inert, each with a
  note), and every gameplay rule in the engine cites its assembly.
- **Full game playable start to finish** — intro, character creation (gypsy),
  Britannia, towns/castles/keeps, NPC schedules and dialogue, shops, combat,
  the eight dungeons + Underworld in first-person 3D, dungeon rooms, Blackthorn's
  palace, shrines, moongates, naval, camping, death & resurrection, and the
  endgame sequence at Dungeon Doom.
- **Parity harness** — hundreds of scenarios run the same situation through an
  independent Python model (derived from the assembly) and the TypeScript engine;
  both must produce identical RNG streams and state. Plus runtime verification
  against the real binary in headless DOSBox-X for the seedable subsystems.
- **Byte-exact original RNG** — the add/rotate/xor generator at `ULTIMA.EXE
  0x2092`, live on a single unified stream through the whole game.
- **1988 presentation layer** — EGA-faithful skin (chrome, runic glyphs, CP437
  text windows), plus an optional shader skin. Gameplay code is untouched by
  skins: the E2E suite is the lock.
- **Spanish localization** — full UI + game text i18n layer (English remains
  byte-exact to the original).

## The "Grand Tour" gate — green

The gate before any announcement was **grand-tour-green**: an automated purist
playthrough of ALL content with a 100% coverage manifest, every dungeon room
sealed with a verdict, run twice byte-identically. That census is closed:
**112/112 dungeon rooms sealed, 0 queued** (70 VICTORY, 38 faithful dead ends,
4 terminal traps — each sealed by a derived mechanic or an assembly citation,
never fabricated), and the tour's main line is played end to end — including
the endgame at Doom — with real keystrokes, twice, byte-identically.

## Honest open items (deliberate, catalogued)

These are not bugs; each is documented in
[`deliberate-divergences.md`](re/deliberate-divergences.md) with its class and
its path to closure:

- **Class A** — rules verified by model↔engine stream parity but not yet
  re-verified live against DOSBox (the oracle can't seed every structure
  headlessly yet).
- **Class B** — verified pure functions awaiting interactive wiring, mostly
  presentation endgame pieces (pixel-dissolve painter timing, final fanfare
  audio) pending cold analysis of their routines.
- **Class C** — conservative additions declared by the port where the binary's
  mechanism is opaque (each one flagged in code and docs).
- Field-object encoding in combat maps (`0xE8–0xEB` gravity fields) is modelled
  as inert pending derivation.
- Minor audio seams (shop transaction cue, dungeon trap cue, town-entry beep)
  derived but not yet wired.

## After 1.0

- ~~Browser "bring your own files" demo~~ **shipped** — live at
  [openu5.org](https://openu5.org): drop your GOG installer / `ultima5/` folder,
  extraction runs client-side, play instantly.
- English migration of the remaining RE notes (the deep-dive corpus started in
  Spanish; `rng`, `combat`, `bugs-of-the-original` and `deliberate-divergences`
  are translated first).
- HD/remaster skin exploration (strictly on top of the faithful layer).
- ~~Mobile touch deck~~ **shipped** (portrait deck with the original's keyboard
  commands).
- Contributions of format corrections back to the Ultima Codex wiki.

## What will never be on this roadmap

- Shipping game data, art, music, text or maps — the engine stays clean forever.
- Gameplay "improvements" that diverge from the binary in the default layer.
- Monetization of any kind.
