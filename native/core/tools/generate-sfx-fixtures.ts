// Alpha 3 A3-02 -- the reference speaker catalogue, as segments, for the cues
// the native synthesizer renders. a3_02_sfx_synth compares its own programs
// against these rows (kind, pitch, duration, draw count); `--check` is the
// drift test that keeps the file equal to what speaker.ts says today.
//
//   node --import tsx native/core/tools/generate-sfx-fixtures.ts [--check]
//
// Row: <cue> <n> <segment> <kind> <f0 Hz> <f1 Hz> <ms> <steps>
//   steps = glide staircase length, or the number of noise draws; 0 otherwise.
// Noise pitches are not written: they come from the local PRNG, which the
// reference seeds arbitrarily (0x1234) and the port from the DS image.
import {readFileSync, writeFileSync} from 'node:fs';
import {renderCue} from '../../../game/src/skin/fiel/speaker.js';
import type {SfxId} from '../../../game/src/core/sfx.js';

const path = 'native/core/fixtures/a3-02-sfx-reference.txt';
const cues: [SfxId, number[]][] = [
  ['move-blocked', [0]], ['move-step', [0]], ['torch-borrowed', [0]], ['dungeon-fail', [0]],
  ['ring-vanishes', [0]], ['cannon-fire', [0]], ['waterfall-fall', [0]], ['dungeon-trap', [0]],
  ['dungeon-zap', [0]], ['field-afflict', [0]], ['mirror-break', [0]],
  ['combat-hit', [0]], ['combat-hit-heavy', [0]], ['combat-damage', [0]], ['combat-defeat', [0]],
  ['time-spell', [0, 1, 2, 3, 4, 5, 6, 7, 8]],
  ['instrument-note', [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]],
  ['apparition-materialize', [0]], ['apparition-arpeggio', [0]], ['apparition-heal-chime', [0]],
  ['apparition-chord', [0]], ['blackthorn-materialize', [0]],
];
const f = (x: number) => x.toFixed(4);
const rows: string[] = [];
for (const [id, ns] of cues)
  for (const n of ns)
    renderCue({id, n}).forEach((s, i) => {
      if (s.kind === 'tone') rows.push([id, n, i, s.steps ? 'glide' : 'tone', f(s.f0), f(s.f1), f(s.ms), s.steps?.length ?? 0].join(' '));
      else if (s.kind === 'noise') rows.push([id, n, i, 'noise', 0, 0, f(s.ms), s.freqs.length].join(' '));
      else rows.push([id, n, i, 'silence', 0, 0, f(s.ms), 0].join(' '));
    });
const output = rows.join('\n') + '\n';
if (process.argv.includes('--check')) {
  if (readFileSync(path, 'utf8') !== output) throw new Error('SFX reference drift');
} else writeFileSync(path, output);
