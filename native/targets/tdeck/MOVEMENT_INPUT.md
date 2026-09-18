# T-Deck movement input

This layer changes only how physical T-Deck controls become existing semantic
UI actions. It does not change movement, combat, turns, passability, commands,
or any other native/core rule.

## Mic key

The Mic key is recognized by the physically verified raw matrix position `(0,6)`, independently of its
vendor `$` character label and the active keyboard layer.

- Press and release in under 1.1 seconds: emit one semantic `Cancel` action on
  release. This is the Escape/Back action in gameplay and the developer menu.
- Hold for 1.1 seconds: toggle Movement Mode immediately. The later release is
  consumed, so it cannot also emit Cancel.
- Repeated matrix snapshots cannot toggle twice because only raw edges begin a
  gesture and each hold has a handled latch.
- If an I2C read fails, the raw keyboard layer emits a resynchronization event.
  The adapter abandons any in-progress Mic gesture; an orphaned release can
  neither toggle nor cancel.

## Movement Mode and context safety

While Movement Mode is enabled and active, `W/A/S/D` emit the same semantic
North/West/South/East actions as the trackball. A cyan `MOVE` indicator appears
beside the current UI mode.

Movement Mode is active only in exploration and dungeon modes. It is suspended,
not forgotten, in text entry, numeric entry, dialogue, shops, selections,
special screens, the developer menu, and combat. Consequently literal keyboard
input cannot become movement in those contexts; returning to exploration or a
dungeon resumes the user's enabled setting.

Combat deliberately retains the existing controls: trackball directions are
combat movement/target directions and letter keys keep their normal combat
command meanings. WASD is not remapped during combat.

## Trackball filtering

The GPIO reader still emits one event per falling switch edge. The semantic
controller keeps a separate timestamp for each direction and rejects only a
same-direction edge within 35 ms. This replaces the 65 ms window, allowing a
new deliberate edge at 35 ms (about 28 detents/second) while short 1-2 ms bounce
edges remain suppressed. Opposite and perpendicular directions never suppress
one another.
