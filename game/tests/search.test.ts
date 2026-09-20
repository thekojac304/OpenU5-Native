/**
 * Tests de búsqueda de objetos ocultos con assets REALES (data.json searchObjects).
 */
import { readFileSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { beforeAll, describe, expect, it } from "vitest";
import { searchAt, applySearchGrant, type SearchData, type SearchObject } from "../src/core/world/search.js";
import {
  createNewGame,
  deserialize,
  serialize,
  type ExtractedInitialState,
  type GameState,
} from "../src/core/state.js";
import { advanceMinutes } from "../src/core/time.js";

const ASSETS = resolve(dirname(fileURLToPath(import.meta.url)), "../assets");
const readJson = <T>(p: string): T =>
  JSON.parse(readFileSync(`${ASSETS}/${p}`, "utf-8")) as T;

let data: SearchData;
let init: ExtractedInitialState;

beforeAll(() => {
  data = readJson<{ searchObjects: SearchObject[] }>("data.json");
  init = readJson("initial-state.json");
});

const newGame = (): GameState => createNewGame(init);

describe("searchAt (tabla SEARCH_OBJECT de DATA.OVL)", () => {
  it("la tabla tiene 114 entradas", () => {
    expect(data.searchObjects.length).toBe(0x72);
  });

  it("encuentra un objeto real y devuelve su tile (id + 0x100) + la entrada, SIN mensaje de acierto", () => {
    const state = newGame();
    // Entrada real de la tabla: id 31, location 4 (Yew), floor 0, x13, y2.
    // Tile devuelto = 31 + 0x100 = 287. El mensaje de ACIERTO lo compone el caller
    // (print_object_name 0x12a); searchAt sólo devuelve la entrada para colocar el objeto.
    const r = searchAt(state, data, 4, 0, 13, 2);
    expect(r.found).toBe(287);
    expect(r.entry).toMatchObject({ id: 31, location: 4, floor: 0, x: 13, y: 2 });
    expect(r.message).toBe("");
  });

  it("al repetir la búsqueda en la misma casilla no queda nada", () => {
    const state = newGame();
    const first = searchAt(state, data, 4, 0, 13, 2);
    expect(first.found).toBe(287);
    const second = searchAt(state, data, 4, 0, 13, 2);
    expect(second.found).toBeNull();
    expect(second.message).toBe("Nothing of note.");
  });

  it("una casilla sin objeto devuelve Nothing of note", () => {
    const state = newGame();
    // Coordenada absurda que no aparece en la tabla.
    const r = searchAt(state, data, 4, 0, 31, 31);
    expect(r.found).toBeNull();
    expect(r.message).toBe("Nothing of note.");
  });

  it("Underworld (location 0, floor 255): el floor NO se decodifica -- 255 ES el floor real del Underworld, no un byte de sótano", () => {
    // searchObjects[0] vive en el mapa MUNDO (location 0): su floor autorado
    // 255 ES el floor real en tiempo de ejecución para el Underworld (igual
    // que game.ts/world.cpp lo comparan tal cual), no el byte crudo de sótano
    // de DOS que decodeAuthoredFloor traduce a -1 sólo para location !== 0.
    // Si se decodificara aquí también, este objeto dejaría de hallarse.
    expect(data.searchObjects[0]).toMatchObject({ id: 11, location: 0, floor: 255, x: 233, y: 233 });
    const state = newGame();
    const r = searchAt(state, data, 0, 255, 233, 233);
    expect(r.found).toBe(0x10b); // id 11 + 0x100
  });

  it("el centinela de ceros (índice 113) NO se busca: el binario itera sólo 113", () => {
    // La entrada 114 de la tabla es loc/floor/x/y = 0 (relleno). El binario para
    // en si=0x70 (113 entradas), así que buscar en (loc 0, planta 0, 0, 0) no revela
    // nada. Sin el cap, findIndex casaría con el centinela.
    const sentinel = data.searchObjects[113];
    expect(sentinel).toMatchObject({ location: 0, floor: 0, x: 0, y: 0 });
    const state = newGame();
    const r = searchAt(state, data, 0, 0, 0, 0);
    expect(r.found).toBeNull();
    expect(r.message).toBe("Nothing of note.");
  });

  it("la persistencia usa questFlags con clave search:<índice>", () => {
    const state = newGame();
    const index = data.searchObjects.findIndex(
      (o) => o.location === 4 && o.floor === 0 && o.x === 13 && o.y === 2,
    );
    expect(index).toBeGreaterThanOrEqual(0);
    expect(state.questFlags[`search:${index}`]).toBeUndefined();
    searchAt(state, data, 4, 0, 13, 2);
    expect(state.questFlags[`search:${index}`]).toBe(true);
  });
});

describe("grant de ItemKey — get_special_item 0x1458 rama keys (SJOG 0x1568)", () => {
  // DOS PASOS (#22): searchAt COLOCA el objeto (no concede); applySearchGrant es lo que el
  // (G)et aplica al recogerlo. La rama keys decodifica el byte `quality`: bit alto (>0x7f) →
  // SKULL keys (g_skull_keys 0x57b1) con `quality & 0x7f`; si no → keys normales (g_keys
  // 0x57ac) con `quality`. Cap 0x63 (add_capped 0x7f70). Aquí se testea el par searchAt +
  // applySearchGrant (el mismo par que el flujo real Search→Get, cubierto e2e en
  // search-get.test.ts). Cita: re/disasm/SJOG.OVL.asm 0x1568, objects.md O3, cmds.md §10.

  it("searchAt NO concede nada: la skull key del árbol de Minoc sólo cuenta al (G)et", () => {
    const state = newGame();
    const skullBefore = state.skullKeys;
    const r = searchAt(state, data, 5, 0, 2, 2);
    expect(r.found).toBe(0x107); // ItemKey colocado, VISIBLE
    expect(state.skullKeys).toBe(skullBefore); // sin grant hasta el (G)et
  });

  it("el árbol de Minoc (searchObjects[14], loc5/f0/2,2, quality 0x85) da 5 SKULL keys al recoger", () => {
    // 0x85 > 0x7f → skull keys; 0x85 & 0x7f = 5. Es el 'árbol de calaveras' del
    // recuerdo, ahora DERIVADO de la tabla en vez de la implementación de memoria.
    expect(data.searchObjects[14]).toMatchObject({ id: 7, quality: 133, location: 5, floor: 0, x: 2, y: 2 });
    const state = newGame();
    const skullBefore = state.skullKeys;
    const keysBefore = state.keys;
    const r = searchAt(state, data, 5, 0, 2, 2);
    expect(r.found).toBe(0x107); // ItemKey
    const count = applySearchGrant(state, r.entry!); // el (G)et
    expect(count).toBe(5);
    expect(state.skullKeys).toBe(skullBefore + 5);
    expect(state.keys).toBe(keysBefore); // el bit alto NO toca las keys normales
  });

  it("un ItemKey con quality ≤ 0x7f (searchObjects[13], quality 9) da 9 keys normales al recoger", () => {
    // El floor autorado (255) es el byte crudo de DATA.OVL para el sótano de un
    // mapa pequeño; en tiempo de ejecución esa misma celda vive en floor -1
    // (smallmaps.json loc18, blackthornCaptureDeposit) -- decodeAuthoredFloor
    // convierte uno en otro. Se busca con -1, el floor real de la celda.
    expect(data.searchObjects[13]).toMatchObject({ id: 7, quality: 9, location: 18, floor: 255, x: 8, y: 6 });
    const state = newGame();
    // Índice 0x0d es re-hallable sólo con 0 llaves normales (gate SJOG 0x055d);
    // init.keys > 0, así que hay que vaciarlas para que el gate deje hallarlo.
    state.keys = 0;
    const skullBefore = state.skullKeys;
    const r = searchAt(state, data, 18, -1, 8, 6);
    expect(r.found).toBe(0x107);
    const count = applySearchGrant(state, r.entry!); // el (G)et
    expect(count).toBe(9);
    expect(state.keys).toBe(9);
    expect(state.skullKeys).toBe(skullBefore); // sin bit alto → NO skull keys
  });

  it("re-buscar la misma casilla no vuelve a hallar (bitmap/gate diario) tras coger", () => {
    const state = newGame();
    const first = searchAt(state, data, 5, 0, 2, 2);
    expect(first.found).toBe(0x107);
    applySearchGrant(state, first.entry!); // el (G)et
    const skullAfterFirst = state.skullKeys;
    const second = searchAt(state, data, 5, 0, 2, 2);
    expect(second.found).toBeNull(); // gate diario cerrado el mismo día
    expect(state.skullKeys).toBe(skullAfterFirst); // sin doble grant
  });

  it("el grant respeta el cap 0x63 (add_capped 0x7f70)", () => {
    const state = newGame();
    state.skullKeys = 0x62; // 98
    const r = searchAt(state, data, 5, 0, 2, 2);
    applySearchGrant(state, r.entry!); // +5 → clamp a 0x63
    expect(state.skullKeys).toBe(0x63);
  });
});

describe("grant de EQUIPO — get_special_item rama 0x1670 (#287)", () => {
  // Dispatch verificado contra el binario: ids 9..0xc directo (SJOG 0x1467/0x146c) y
  // los ids 5 y 6 por jump-table (words 5º/6º de file-off 0x171E = 0xd5f0 − 0xBF80 =
  // 0x1670). quality = CÓDIGO de equipo 0..47 (índice de g_equip_qty 0x57C0): munición
  // (0x1B Arrows / 0x1D Quarrels) +5 vía add_capped (0x168b); el resto +1 (inc 0x1693,
  // clamp 0x64→0x63). Esperados EN CRUDO (código y cantidad escritos a mano, no
  // derivados del sujeto).

  it("id5 quality 39 (Glass Sword) concede +1 en equipmentQuantities[39] y devuelve 1", () => {
    const state = newGame();
    expect(state.equipmentQuantities[39]).toBe(0);
    const count = applySearchGrant(state, { id: 5, quality: 39 });
    expect(count).toBe(1);
    expect(state.equipmentQuantities[39]).toBe(1);
  });

  it("los seis ids de la rama (5/6/9/10/11/12) despachan a equipo — mismo 0x1670", () => {
    for (const id of [5, 6, 9, 10, 11, 12]) {
      const state = newGame();
      expect(state.equipmentQuantities[21]).toBe(0); // 21 = un código real de la tabla (loc4)
      expect(applySearchGrant(state, { id, quality: 21 })).toBe(1);
      expect(state.equipmentQuantities[21]).toBe(1);
    }
  });

  it("munición: quality 0x1B (Arrows) y 0x1D (Quarrels) conceden +5 (0x1670/0x1676 → add_capped 5)", () => {
    // La TABLA search no trae munición (qualities reales 9..47 sin 27/29), pero la rama
    // es la MISMA rutina que el botín de cofre y el asm la trae: se testea en crudo.
    const state = newGame();
    expect(applySearchGrant(state, { id: 5, quality: 0x1b })).toBe(5);
    expect(state.equipmentQuantities[0x1b]).toBe(5);
    expect(applySearchGrant(state, { id: 5, quality: 0x1d })).toBe(5);
    expect(state.equipmentQuantities[0x1d]).toBe(5);
  });

  it("cap 0x63: +1 sobre 99 se queda en 99 (clamp 0x64→0x63) y +5 de munición no lo rebasa", () => {
    const state = newGame();
    state.equipmentQuantities[39] = 0x63;
    applySearchGrant(state, { id: 5, quality: 39 });
    expect(state.equipmentQuantities[39]).toBe(0x63);
    state.equipmentQuantities[0x1b] = 0x61; // 97 + 5 → clamp
    applySearchGrant(state, { id: 5, quality: 0x1b });
    expect(state.equipmentQuantities[0x1b]).toBe(0x63);
  });

  it("NEGATIVO: un id fuera de la rama (3, potion) no toca equipmentQuantities", () => {
    const state = newGame();
    const before = [...state.equipmentQuantities];
    // id 3 despacha a 0x163c (potion), no a 0x1670: applySearchGrant devuelve quality
    // tal cual (hueco O3 restante) y el array de equipo queda intacto.
    expect(applySearchGrant(state, { id: 3, quality: 4 })).toBe(4);
    expect(state.equipmentQuantities).toEqual(before);
  });

  it("NEGATIVO: un código fuera del array (48 slots) no escribe y devuelve 0", () => {
    const state = newGame();
    const before = [...state.equipmentQuantities];
    expect(applySearchGrant(state, { id: 5, quality: 48 })).toBe(0);
    expect(state.equipmentQuantities).toEqual(before);
  });
});

describe("gates de re-hallazgo (search_fixed_hidden_items, SJOG 0x0558-0x05b4)", () => {
  // Los índices 0x0d/0x0e/0x0f NO usan el bitmask permanente 0x585c (SJOG salta el
  // `or` en 0x0610-0x0618). Cada uno lleva su propio gate: 0x0e diario (g_day !=
  // [0x57b2]), 0x0d/0x0f por inventario (g_keys==0 / g_equip_qty+39==0).

  it("árbol de Minoc (0x0e): hallado hoy → no re-hallable el mismo día; al avanzar el día vuelve a dar 5 skull keys", () => {
    const state = newGame();
    const day0 = state.time.day;
    const r1 = searchAt(state, data, 5, 0, 2, 2);
    expect(r1.found).toBe(0x107);
    applySearchGrant(state, r1.entry!); // el (G)et
    expect(state.skullKeys).toBe(5);
    expect(state.skullTreeFoundDay).toBe(day0); // espejo de [0x57b2]=g_day, fijado al COLOCAR

    // Mismo día: el gate diario lo bloquea, sin doble grant.
    const r2 = searchAt(state, data, 5, 0, 2, 2);
    expect(r2.found).toBeNull();
    expect(state.skullKeys).toBe(5);

    // Avanzar el reloj REAL un día completo (advanceMinutes = el mecanismo de
    // juego, no seteo a mano de skullTreeFoundDay). Cruza medianoche → day++.
    state.time = advanceMinutes(state.time, 24 * 60);
    expect(state.time.day).not.toBe(day0);
    const r3 = searchAt(state, data, 5, 0, 2, 2);
    expect(r3.found).toBe(0x107);
    applySearchGrant(state, r3.entry!); // el (G)et
    expect(state.skullKeys).toBe(10); // +5 de nuevo, otro día
    expect(state.skullTreeFoundDay).toBe(state.time.day);
  });

  it("Buccaneer's Den (0x0d): sólo hallable con 0 llaves; da 9 al recoger y es repetible al re-vaciar", () => {
    // Se busca en floor -1 (el sótano real de la localización 18), no el 255
    // autorado -- ver decodeAuthoredFloor.
    const state = newGame();
    expect(state.keys).toBeGreaterThan(0); // init.keys = 2 → gate cerrado
    expect(searchAt(state, data, 18, -1, 8, 6).found).toBeNull();

    state.keys = 0; // gate abierto
    const r1 = searchAt(state, data, 18, -1, 8, 6);
    expect(r1.found).toBe(0x107);
    applySearchGrant(state, r1.entry!); // el (G)et → +9 → gate se auto-cierra
    expect(state.keys).toBe(9);

    // Con 9 llaves ya no aparece (NO once-only: es el inventario quien decide).
    expect(searchAt(state, data, 18, -1, 8, 6).found).toBeNull();
    expect(state.keys).toBe(9);

    // Re-vaciar → vuelve a estar hallable (repetible).
    state.keys = 0;
    const r3 = searchAt(state, data, 18, -1, 8, 6);
    expect(r3.found).toBe(0x107);
    applySearchGrant(state, r3.entry!);
    expect(state.keys).toBe(9);
  });

  it("overworld (0x0f, 64,80): gate por Glass Sword (equip 39) y el grant lo AUTO-CIERRA (#287)", () => {
    // Esperados EN CRUDO: searchObjects[15] = {id:5, quality:39, loc 0, f0, 64,80};
    // 39 = Glass Sword (g_equip_qty+39 = 0x57e7, gate SJOG 0x0587).
    expect(data.searchObjects[15]).toMatchObject({ id: 5, quality: 39, location: 0, floor: 0, x: 64, y: 80 });
    const state = newGame();
    expect(state.equipmentQuantities[39]).toBe(0); // sin Glass Sword → hallable
    const r1 = searchAt(state, data, 0, 0, 64, 80);
    expect(r1.found).toBe(0x105); // id5 + 0x100 (ItemWeapon)
    const count = applySearchGrant(state, r1.entry!); // el (G)et → rama 0x1670
    expect(count).toBe(1); // no-munición: inc @0x1693, no +5
    expect(state.equipmentQuantities[39]).toBe(1); // +1 Glass Sword

    // El propio grant cierra el gate (0x0587 ya no ve 0): repetible sólo al perderla.
    expect(searchAt(state, data, 0, 0, 64, 80).found).toBeNull();
    state.equipmentQuantities[39] = 0; // se rompió/vendió → vuelve a estar hallable
    expect(searchAt(state, data, 0, 0, 64, 80).found).toBe(0x105);
  });

  it("gate 0x770e (find_object_at_xy) de 0x0d/0x0f: con ocupante en la casilla NO se halla", () => {
    // SJOG 0x058e-0x059f: el 0x0f sólo halla si 0x770e(x,y,floor) devuelve 0. El predicado
    // es el `occupiedAt` inyectado (en Game = objectOrNpcAt, worldObjects + NPCs).
    const state = newGame();
    const seen: Array<[number, number, number]> = [];
    const occupied = (x: number, y: number, f: number): boolean => {
      seen.push([x, y, f]);
      return true; // ocupante presente
    };
    expect(searchAt(state, data, 0, 0, 64, 80, occupied).found).toBeNull(); // 0x0f
    state.keys = 0;
    expect(searchAt(state, data, 18, -1, 8, 6, occupied).found).toBeNull(); // 0x0d
    // El predicado se consultó con las coordenadas de la ENTRADA (x, y, floor
    // DECODIFICADO: -1, no el 255 autorado -- ver decodeAuthoredFloor).
    expect(seen).toContainEqual([64, 80, 0]);
    expect(seen).toContainEqual([8, 6, -1]);

    // Control positivo: casilla libre → los dos vuelven a hallarse.
    expect(searchAt(state, data, 0, 0, 64, 80, () => false).found).toBe(0x105);
    expect(searchAt(state, data, 18, -1, 8, 6, () => false).found).toBe(0x107);
  });

  it("el gate 0x770e NO aplica a índices normales ni al árbol 0x0e (sólo 0x0d/0x0f llaman)", () => {
    // El binario sólo emite `call 0x770e` en 0x056d (0x0d) y 0x059a (0x0f); el camino del
    // bitmask (0x5f6) y el gate diario (0x0574) no lo consultan.
    const state = newGame();
    const occupied = (): boolean => true;
    expect(searchAt(state, data, 4, 0, 13, 2, occupied).found).toBe(287); // índice normal
    const s2 = newGame();
    expect(searchAt(s2, data, 5, 0, 2, 2, occupied).found).toBe(0x107); // 0x0e árbol de Minoc
  });

  it("los índices normales siguen once-only (regresión del bitmask permanente)", () => {
    const state = newGame();
    // (4,0,13,2) = índice normal; hallado una vez y nunca más, sin importar día.
    expect(searchAt(state, data, 4, 0, 13, 2).found).toBe(287);
    expect(searchAt(state, data, 4, 0, 13, 2).found).toBeNull();
    state.time = advanceMinutes(state.time, 24 * 60); // otro día no lo repone
    expect(searchAt(state, data, 4, 0, 13, 2).found).toBeNull();
  });

  it("migración: un save viejo con search:13/14/15 (once-only) limpia esos flags al cargar", () => {
    const state = newGame();
    state.questFlags["search:13"] = true;
    state.questFlags["search:14"] = true;
    state.questFlags["search:15"] = true;
    state.questFlags["search:42"] = true; // un índice normal se conserva
    const loaded = deserialize(serialize(state));
    expect(loaded.questFlags["search:13"]).toBeUndefined();
    expect(loaded.questFlags["search:14"]).toBeUndefined();
    expect(loaded.questFlags["search:15"]).toBeUndefined();
    expect(loaded.questFlags["search:42"]).toBe(true);
    // Y el árbol vuelve a ser hallable (el once-only ya no lo bloquea).
    expect(searchAt(loaded, data, 5, 0, 2, 2).found).toBe(0x107);
  });
});
