/**
 * Alpha 4 A4-SAVE3 (native/targets/tdeck/ALPHA4_UI.md section 5): the independent reader.
 *
 * The C++ side (a4_save3_pc_bridge_runtime --emit) imports the genuine DOS save through the
 * real runtime, changes it, saves, exports, and writes the files. This script reads those
 * files with the TypeScript reference's own SAVED.GAM parser (game/src/core/saveNative.ts
 * importNativeSave) -- not with the C++ codec or the bridge that wrote them -- and, for the
 * cells the reference keeps in its sidecar, with raw reads at the offsets the reference and
 * the RE notes document. SAVED.OOL has no reader in the reference ("No .OOL import exists in
 * the reference"), so its records are read here by the documented 8-byte layout.
 *
 * usage: node --import tsx check-pc-save.ts <a4_save3_pc_bridge_runtime> <pack> <fixture dir>
 */
import { readFileSync, mkdtempSync, rmSync } from 'node:fs';
import { join } from 'node:path';
import { tmpdir } from 'node:os';
import { spawnSync } from 'node:child_process';
import { importNativeSave } from '../../../game/src/core/saveNative.js';
import { emptySidecar } from '../../../game/src/core/u5gam.js';
import { setCargaFiel } from '../../../game/src/core/npc/carga-fiel.js';
import type { GameState } from '../../../game/src/core/state.js';

const [driver, pack, fixtures] = process.argv.slice(2);
if (!driver || !pack || !fixtures) {
  console.error('usage: check-pc-save.ts <a4_save3_pc_bridge_runtime> <pack> <fixture dir>');
  process.exit(2);
}
const out = mkdtempSync(join(tmpdir(), 'openu5-a4-save3-'));
const run = spawnSync(driver, [pack, fixtures, '--emit', out], { encoding: 'utf8', maxBuffer: 1 << 28 });
if (run.status !== 0) {
  console.error(run.stdout.split('\n').filter((l) => /^\s+RED /.test(l)).join('\n'));
  console.error(`the driver failed (exit ${run.status})`);
  process.exit(1);
}

let checks = 0,
  failures = 0;
function check(ok: boolean, id: string, what: string) {
  ++checks;
  if (!ok) ++failures;
  console.log(`  ${ok ? 'GREEN' : 'RED  '} ${id.padEnd(5)} ${what}`);
}
const bytes = (name: string) => new Uint8Array(readFileSync(join(out, name)));
const json = (name: string) => JSON.parse(readFileSync(join(out, name), 'utf8'));
setCargaFiel(false);
const read = (gam: Uint8Array): GameState => importNativeSave(gam, emptySidecar());

// Cells the reference keeps in its sidecar, at the offsets it documents (state.ts
// SAVE_OPTIONAL_DEFAULTS) and SJOG's found-once bitmap (re/notes/sjog.md: [0x585c + si>>3]).
const WIND = 0x2ec;
const SEARCH = 0x2b6;
const searchFound = (gam: Uint8Array) => {
  const found: number[] = [];
  for (let i = 0; i < 113; ++i) if (i !== 13 && i !== 14 && i !== 15 && gam[SEARCH + (i >> 3)]! & (1 << (i & 7))) found.push(i);
  return found;
};
// An object table (32 records x 8 B: tile, tile, x, y, floor, hull, 0, skiffs --
// re/notes/witness-o1-0x6b4.md), vehicles only, as sorted keys.
const vehicles = (table: Uint8Array, floor: number) => {
  const v: string[] = [];
  for (let i = 1; i < 32; ++i) {
    const t = table[i * 8]!;
    const frigate = (t & 0xf8) === 0x20,
      skiff = (t & 0xfc) === 0x28,
      horse = (t & 0xfe) === 0x10;
    if (!frigate && !skiff && !horse) continue;
    v.push(`${frigate ? 'frigate' : skiff ? 'skiff' : 'horse'}@${floor}:${table[i * 8 + 2]},${table[i * 8 + 3]}` +
      (frigate ? ` hull ${table[i * 8 + 5]} skiffs ${table[i * 8 + 7]}` : ''));
  }
  return v.sort();
};
const liveTable = (gam: Uint8Array) => gam.slice(0x6b4, 0x6b4 + 256);
const brit = (ool: Uint8Array) => ool.slice(0, 256);
const under = (ool: Uint8Array) => ool.slice(256, 512);

console.log('[TS] the DOS fixture, as the reference reads it, against Native\'s import');
const dos = new Uint8Array(readFileSync(join(fixtures, 'SAVED.GAM')));
const ref = read(dos);
const imported = json('fixture-state.json');
const party = ref.characters.slice(0, ref.partySize).map((c) => c.name);
check(
  imported.gold === ref.gold && imported.food === ref.food && imported.karma === ref.karma && imported.partySize === ref.partySize &&
    JSON.stringify(imported.names) === JSON.stringify(party) && imported.location === ref.position.location &&
    imported.floor === ref.position.floor && imported.x === ref.position.x && imported.y === ref.position.y,
  'TS1',
  `Native's import of the fixture equals the reference's reading: ${party.join(', ')}; ${ref.gold} gold; location ${ref.position.location} (${ref.position.x},${ref.position.y})`,
);
check(
  imported.wind === dos[WIND] && JSON.stringify(imported.search) === JSON.stringify(searchFound(dos)) && imported.frigates.length === 0,
  'TS2',
  `the bridged cells: wind ${dos[WIND]} (0x2EC) and the found-once bitmap (${searchFound(dos).length} found) as the file holds them; no frigate parked`,
);

console.log('[TS] the exported round trip, read by the reference');
const gam = bytes('roundtrip-SAVED.GAM');
const ool = bytes('roundtrip-SAVED.OOL');
check(gam.length === 4192 && ool.length === 512, 'TS3', 'the export is a 4192-byte SAVED.GAM and a 512-byte SAVED.OOL');
const back = read(gam);
const state = json('roundtrip-state.json');
check(
  back.gold === 1234 && back.position.x === 16 && gam[WIND] === 2 && searchFound(gam).includes(20) && state.gold === 1234 && state.wind === 2,
  'TS4',
  'the changes made on Native are in the exported file: 1234 gold, x 16, wind South (0x2EC = 2), search item 20 found',
);
// Every field the reference reads is the fixture's, except the ones changed on purpose.
const strip = (s: GameState) => {
  const c = JSON.parse(JSON.stringify(s));
  delete c.gold;
  delete c.position.x;
  return c;
};
const a = strip(ref),
  b = strip(back);
const differing = Object.keys({ ...a, ...b }).filter((k) => JSON.stringify(a[k]) !== JSON.stringify(b[k]));
check(differing.length === 0, 'TS5', `apart from gold and x, the reference reads the exported save exactly as it reads the DOS original (differing: ${differing.join(', ') || 'none'})`);
const docked = vehicles(brit(ool), 0);
check(
  JSON.stringify(docked) === JSON.stringify(state.frigates.map((f: { floor: number; x: number; y: number; hull: number; skiffs: number }) =>
    `frigate@${f.floor}:${f.x},${f.y} hull ${f.hull} skiffs ${f.skiffs}`).sort()) && docked.length === 1,
  'TS6',
  `the frigate docked outside the castle is parked in SAVED.OOL's BRIT block: ${docked.join('; ')}`,
);

console.log('[TS] vehicles: a DOS layout in, the same layout out');
const vin = bytes('vehicles-in-SAVED.GAM'),
  vinOol = bytes('vehicles-in-SAVED.OOL');
const vout = bytes('vehicles-out-SAVED.GAM'),
  voutOol = bytes('vehicles-out-SAVED.OOL');
const inLive = vehicles(liveTable(vin), 0),
  outLive = vehicles(liveTable(vout), 0);
const inUnder = vehicles(under(vinOol), 255),
  outUnder = vehicles(under(voutOol), 255);
check(JSON.stringify(inLive) === JSON.stringify(outLive) && inLive.length === 3, 'TS7', `the live table's vehicles survive import and export: ${outLive.join('; ')}`);
check(JSON.stringify(inUnder) === JSON.stringify(outUnder) && inUnder.length === 2, 'TS8', `UNDER.OOL's parked vehicles survive, the new-game skiff once: ${outUnder.join('; ')}`);
const refIn = read(vin).worldObjects.filter((o) => o.location === 0).map((o) => `${o.kind}@${o.x},${o.y}`).sort();
const refOut = read(vout).worldObjects.filter((o) => o.location === 0).map((o) => `${o.kind}@${o.x},${o.y}`).sort();
check(JSON.stringify(refIn) === JSON.stringify(refOut), 'TS9', `and the reference's own reader sees the same vehicles in both: ${refOut.join('; ')}`);
const monsters = (g: Uint8Array) => read(g).overworldEnemies.map((e) => `def ${e.defIndex}@${e.x},${e.y}`).sort();
check(JSON.stringify(monsters(vin)) === JSON.stringify(monsters(vout)) && monsters(vin).length === 1, 'TS10',
  `the reference reads the same monster in and out: ${monsters(vout).join('; ')}`);

rmSync(out, { recursive: true, force: true });
console.log(`\ncheck-pc-save: ${checks - failures}/${checks} checks GREEN, ${failures} RED`);
process.exit(failures ? 1 : 0);
