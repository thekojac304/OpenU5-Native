# Ultima V Native Roadmap

This is the roadmap for **Ultima V Native**, the C++ native implementation. It is not the roadmap of the separate OpenU5 browser/reference implementation, which is kept in [`OPENU5_BROWSER_ROADMAP.md`](OPENU5_BROWSER_ROADMAP.md). No dates are promised.

## Current release

- **v0.4.0**, a pre-1.0 public release for the LilyGO T-Deck Plus
- complete and playable from character creation through the ending
- preservation-focused **Original** mode plus optional **Enhanced** features
- see [`RELEASE_NOTES_v0.4.0.md`](RELEASE_NOTES_v0.4.0.md)

## Next major goal: Windows

- a native Windows target
- reuse of the native core where practical
- a fully faithful Original mode, and the optional Enhanced mode
- desktop input and UI adaptation

## Preservation and parity work

- the documented non-blocking gaps (the D-90 to D-103 ledger rows and older minor rows)
- continued adjudication against the original DOS binary

## Internal naming cleanup

- a compatibility-aware migration of the remaining `openu5_*` technical identifiers, planned in [`docs/INTERNAL_RENAME_PLAN.md`](docs/INTERNAL_RENAME_PLAN.md)

## Future portability

- possibly other native platforms after Windows; no commitments

## What will not change

- no game data, art, music, text or maps are ever distributed
- Original mode stays faithful to the 1988 game; enhancements stay optional
