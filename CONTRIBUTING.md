# Contributing

> **Scope.** This repository holds three kinds of material: **Ultima V Native** (the C++ T-Deck / native implementation, under `native/`), the separate **OpenU5** browser/reference implementation (`game/`, `demo-byo/`, `extractor/`), and **shared reverse-engineering material** (`re/`, `docs/`). Say which one your issue or PR concerns. The setup, test tiers and layer rules below describe the OpenU5 browser workflow; for Native, see [`native/core/README.md`](native/core/README.md) and [`native/targets/tdeck/README.md`](native/targets/tdeck/README.md). The provenance rule (no gameplay change without an assembly citation or parity evidence) applies to both.

Thanks for your interest! This project has one non-negotiable rule that shapes
everything else:

> **The Golden Rule: no gameplay change is accepted without provenance.**
> Every rule in the engine core cites the original assembly (overlay + offset)
> or a parity scenario that proves it. If your PR changes gameplay behaviour,
> it must come with an asm citation from the disassembly or a DOSBox-verified
> parity scenario. "It feels wrong" or "other remakes do X" is not evidence —
> this project exists precisely because other remakes approximated.

## Ways to contribute (easiest first)

1. **Bug reports with a save.** The game can export saves (F5 → Export). A report
   with an exported save + the RNG seed + steps is reproducible in minutes and
   is the single most valuable contribution. Use the issue template.
2. **Playtesting against the original.** If you own the GOG version and can run
   it in DOSBox: play the same situation in both and report any divergence,
   however small ("the guard turned left here, the port turns right").
3. **Docs and translations.** Much of the RE documentation started in Spanish
   and is being translated — help is welcome.
4. **Code.** See below.

## Code contributions

### Setup
You need your own copy of the game data (see README). Then:
```bash
npm install && npm run extract && npm run dev
```

### Test tiers
- **Tier 1 (no game data needed):** `npm run test:pure -w game` —
  data-independent unit tests (plus `npx tsc --noEmit` in `game/` for the
  typecheck). This is what public CI runs, on Node 22.
- **Tier 2 (needs your game data):** `npm test` (unit against extracted assets),
  `npm run e2e` (Playwright), `python3 re/tools/parity_all.py` (parity harness).
- **Tier 3 (needs dosbox-x):** live oracle runs against the real binary.
  Documented in docs/methodology.md; not required for most PRs.

Run everything you can before opening a PR; state in the PR body which tiers ran.

### Layer rules
- `game/src/core/` is **pure and deterministic**: no DOM, no PixiJS, no
  `Math.random`, no wall-clock time. CI enforces this.
- Rendering lives in `game/src/render/`, UI chrome in `game/src/ui/`.
- **UI PRs must not touch `core/`** and must keep the entire E2E suite green
  without modifying a single assert — the suite is the gameplay lock.

### Gameplay PRs — the provenance checklist
- [ ] Asm citation (overlay + offset) in a code comment next to the rule, OR a
      parity scenario JSON under `re/parity/`.
- [ ] Existing parity tests still green.
- [ ] If the change contradicts a rule documented in `re/notes/`, the PR must
      update the note with the new derivation — reviewed extra carefully.

## What we will not merge
- Game data, art, music, maps or text from any Ultima game, in any encoding.
- Bulk disassembly listings (short cited excerpts in notes are fine).
- Gameplay "fixes" that make the game nicer but diverge from the binary. Those
  belong in a clearly-marked optional layer, discussed in an issue first.

## Conduct
Standard [Contributor Covenant]. Be kind; this is a love letter to a 1988 game.
