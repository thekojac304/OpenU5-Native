# Future T-Deck frontend roadmap

This document records post-audit goals only. None of these items is implemented by the device
integration audit.

## Original-style intro and main menu

Add an Ultima V-inspired title/intro presentation and an original-style main menu with New,
Continue, Load, and other actions supported by the authoritative save model. Keep boot/resource
errors visible and recoverable on the handheld.

## Settings

Create a persistent device settings screen for display brightness, future sound and music volume,
Movement Mode, trackball sensitivity/repeat tuning, practical text/UI sizing, touch controls,
developer options, and additional device preferences as they emerge. Settings must remain separate
from gameplay state and survive save replacement or recovery.

## Main-game UI redesign

Move toward the original Ultima V composition while preserving handheld readability. Treat the
transcript as a running log and render the current command/input at its bottom rather than spending
a separate strip only on typed input. Preserve deliberate wrapping and roughly the current 176×176
gameplay viewport unless measurements justify a conscious tradeoff. The redesign should make the
lower strip available without destabilizing the map/presentation contract.

## Future touch buttons

Reserve the possibility of two or three lower-area touch buttons. They must emit the same semantic
`UiAction` values as keyboard, trackball, and Mic input—never a separate gameplay command system.
Menu, Back/Cancel, and a context action are candidates; exact controls remain an interaction-design
decision after the layout work.

## Audio architecture

Do not add sound or music as part of this audit. Future audio should consume semantic game events,
route through independent sound/music buses, and expose persistent sound and music volumes. Core
gameplay must not depend on an audio backend being present.

