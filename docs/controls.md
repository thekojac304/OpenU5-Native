> *Scope: these controls describe the OpenU5 browser implementation. For Ultima V Native on the T-Deck, see the release notes.*

# Controls

The port implements the full keyboard of the DOS original (as specified by the
1988 Player Reference Card), plus a few additive quality-of-life keys that
never collide with an original command. Anything that differs from the
original is listed at the bottom.

## Command keys (identical to the DOS original)

| Key | Command | | Key | Command |
|---|---|---|---|---|
| **A** | Attack | | **N** | New Order (swap party members) |
| **B** | Board (horse/ship/skiff/carpet) | | **O** | Open |
| **C** | Cast a spell | | **P** | Push (furniture) |
| **E** | Enter (town, dungeon, shrine…) | | **Q** | Quit & Save |
| **F** | Fire (ship broadside / cannon) | | **R** | Ready (equip weapons & armour) |
| **G** | Get | | **S** | Search |
| **H** | Hole up & camp | | **T** | Talk |
| **I** | Ignite a torch | | **U** | Use an item |
| **J** | Jimmy a lock | | **V** | View (requires a gem) |
| **K** | Klimb | | **X** | X-it (dismount / leave vehicle) |
| **L** | Look | | **Y** | Yell (hoist/furl sails; words of power) |
| **M** | Mix reagents | | **Z** | Z-Stats (party status) |

Directional commands (Attack, Open, Look, Talk, Get, Jimmy, Search, Push…)
prompt for a direction — answer with an arrow key, exactly like the original.

## Movement

- **Arrow keys** — move / turn. No diagonal movement (faithful to the original).
- **Space** — pass a turn; also aborts a pending directional prompt.
- In **dungeons**, movement is relative to your facing (faithful to the
  binary's model): **↑** advance, **↓** retreat, **←/→** turn,
  **Enter** or **.** turn around 180°.

## Combat

- **Arrow keys** — move the active combatant.
- **A** — attack: an aiming cursor appears for ranged weapons; move it with
  the arrows and confirm with **A**/**Space**.
- **C** — cast; **Z** — stats; **Space** — pass.
- **1–6** set the active player, **0** returns to party-wide control
  (same as the original's "Set Active Plr").

## Menus and lists (Z-Stats, Ready, Use, shops…)

- **Arrows** move, **Space/Enter** select, **Escape** backs out.
- **Home / End / PgUp / PgDn** jump within long lists — same keys the
  reference card documents for the original.
- A **number** picks a party member directly in member prompts.

## Additive keys (not in the original — QoL only)

These occupy keys the DOS dispatcher left unused (every letter A–Z belongs to
the original; function keys fall through to "What?"), so nothing original is
shadowed:

| Key | Function |
|---|---|
| **F5** | Save / export save |
| **F7** | Contextual music on/off (the DOS version had no music) |
| **F8** | PC-speaker sound on/off |
| **F10 / Escape** | System menu (when no panel is open) |
| **D** (in dungeons) | Drink from the fountain you are standing on — a shortcut for the original's Look-at-fountain flow; the prompt and effects are the faithful ones |

## Original keys that do not apply in a browser

The DOS original had four system toggles that either collide with browser
shortcuts or have no equivalent in a browser's event model: **Ctrl-S** (sound
— use F8), **Ctrl-V** (music volume — use F7 / system menu), **Ctrl-T** (CPU
speed throttle — the port is frame-timed), **Ctrl-B** (BIOS keyboard buffer —
browsers deliver key events one by one). Their useful functions are covered by
the QoL keys above.

A full key-by-key audit of the port against the Player Reference Card lives in
the repository history; the summary is: all 24 command letters, movement,
aiming, list navigation and active-player keys are implemented; zero gameplay
gaps.
