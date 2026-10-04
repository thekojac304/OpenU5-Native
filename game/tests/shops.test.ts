/**
 * Tests del sistema de tiendas contra los assets REALES (data.json extraído de
 * DATA.OVL, ShoppeKeeperMap.json) y el estado inicial (initial-state.json).
 * Verifican fórmulas de precio con valores concretos, stock del herrero,
 * precios de healer y transacciones sobre GameState.
 */
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { describe, it, expect } from "vitest";
import { createNewGame, type ExtractedInitialState, type GameState } from "../src/core/state.js";
import {
  blacksmithStock,
  shopBuyPrice,
  shopSellPrice,
  equipmentBuyPrice,
  equipmentSellPrice,
  reagentPrice,
  provisionPrice,
  horsePrice,
  buyHorse,
  shipPrice,
  healerPrices,
  innAt,
  innRest,
  innRestPrice,
  innLeave,
  innPickup,
  innPickupPrice,
  buyGuildItem,
  guildPrice,
  buyTavernRound,
  buyWine,
  buyRations,
  rumorPrice,
  rumorKeywordIndex,
  rumorGossip,
  payRumor,
  shipwrightPrice,
  buyShip,
  shoppeKeeperAt,
  buyEquipment,
  sellEquipment,
  buyReagent,
  healerHeal,
  postPurchaseDrain,
  EQUIPMENT_NOTHING,
  HEALING_TOWNES,
  type ShoppeKeeperMapEntry,
} from "../src/core/shops/shops.js";
import { RUMOR_FORM_INDEX } from "../src/core/shops/shop-tables.js";
import type { RandFn } from "../src/core/world/survival.js";

/** Lee un asset JSON quitando el BOM si lo hubiera (ShoppeKeeperMap.json lo tiene). */
function readJson<T>(url: string): T {
  const path = fileURLToPath(new URL(url, import.meta.url));
  return JSON.parse(readFileSync(path, "utf8").replace(/^﻿/, "")) as T;
}

interface DataOvl {
  equipmentBasePrices: number[];
  reagentBasePrices: number[];
  weaponsSoldByMerchants: number[][];
  healPrices: number[];
  curePrices: number[];
  resurrectPrices: number[];
  storeNames: string[];
  shoppeKeeperNames: string[];
}

const data = readJson<DataOvl>("../assets/data.json");
const shoppeMap = readJson<Record<string, ShoppeKeeperMapEntry>>(
  "../src/core/data/ShoppeKeeperMap.json",
);

function freshState(): GameState {
  const init = readJson<ExtractedInitialState>("../assets/initial-state.json");
  return createNewGame(init);
}

// Equipment enum ids relevantes.
const LONG_SWORD = 30;
const PLATE_MAIL = 14;

describe("shopBuyPrice / shopSellPrice (regateo EXACTO por Inteligencia)", () => {
  it("compra: base + ⌊base·(100−3·INT)/100⌋, dos pasos truncados hacia 0", () => {
    // INT 0 duplica; INT ~33 ≈ base; INT 50 ≈ mitad.
    expect(shopBuyPrice(100, 0)).toBe(200);
    expect(shopBuyPrice(70, 33)).toBe(70);
    expect(shopBuyPrice(100, 50)).toBe(50);
  });
  it("venta: ⌊3·INT·base/100⌋ + 1 (INT del vendedor)", () => {
    expect(shopSellPrice(70, 33)).toBe(70); // 6930/100=69 +1
    expect(shopSellPrice(70, 23)).toBe(49); // 4830/100=48 +1
    expect(shopSellPrice(100, 0)).toBe(1); // 0 +1
  });
});

describe("equipmentBuyPrice / equipmentSellPrice (por INTELIGENCIA, no DEX)", () => {
  const longSwordBase = data.equipmentBasePrices[LONG_SWORD]!;

  it("con INT 33 el precio es ~base (espada larga = 70)", () => {
    expect(longSwordBase).toBe(70);
    expect(equipmentBuyPrice(longSwordBase, 33)).toBe(70);
    expect(equipmentSellPrice(longSwordBase, 33)).toBe(70);
  });

  it("con INT alto (43) la compra baja (dos pasos truncados)", () => {
    expect(equipmentBuyPrice(longSwordBase, 43)).toBe(50); // 70 + trunc(70·-29/100)=70-20
    expect(equipmentBuyPrice(data.equipmentBasePrices[PLATE_MAIL]!, 43)).toBe(497); // 700-203
  });

  it("con INT bajo (23) la compra sube y la venta baja", () => {
    expect(equipmentBuyPrice(longSwordBase, 23)).toBe(91); // 70 + trunc(70·31/100)=70+21
    expect(equipmentSellPrice(longSwordBase, 23)).toBe(49); // trunc(4830/100)+1
  });
});

describe("reagentPrice (por INTELIGENCIA, tabla por-ciudad — no Karma)", () => {
  it("INT 0 duplica el precio base; INT ~33 lo deja igual", () => {
    expect(reagentPrice(20, 0)).toBe(40);
    expect(reagentPrice(20, 33)).toBe(20);
  });
  it("INT intermedio abarata (Ginseng Moonglow base 20)", () => {
    expect(reagentPrice(20, 20)).toBe(28); // 20 + trunc(20·40/100)=20+8
  });
});

describe("provisionPrice / horsePrice / shipPrice (regateo por Inteligencia)", () => {
  it("provisión usa el mismo regateo que el resto (base + haggle)", () => {
    expect(provisionPrice(10, 0)).toBe(20);
    expect(provisionPrice(10, 33)).toBe(10);
  });
  it("caballo: base binaria 100/130/160/190, ×2 a INT 0", () => {
    expect(horsePrice(100, 0)).toBe(200); // Trinsic base 100 → 200 a INT 0
    expect(horsePrice(100, 10)).toBe(170);
    expect(horsePrice(100, 20)).toBe(140);
  });
  it("barco (shipwright) usa el mismo regateo", () => {
    expect(shipPrice(600, 0)).toBe(1200); // fragata Jhelom base 600 → 1200 a INT 0
    expect(shipPrice(600, 10)).toBe(1020);
  });
});

describe("buyHorse (núcleo de oro: precio por INT, cobra o rechaza)", () => {
  it("cobra el precio regateado y devuelve 'Yes!'", () => {
    const s = freshState();
    s.gold = 500;
    // town0 Trinsic base 100 → 200 a INT 0.
    const r = buyHorse(s, 0, 0);
    expect(r).toEqual({ ok: true, message: "Yes!" });
    expect(s.gold).toBe(300);
  });
  // RE-SELLADO en #147 tanda 2: el aserto decía la frase aplanada del port. El binario
  // parte la oración en DOS impresores —DS 0x7a7e por el llano (CS 0x0934) y DS 0x7a9e
  // por el expansor de `$` (CS 0x093b)— y la partición es de la segmentación de
  // DATA.OVL, no dos mensajes. La mecánica (no cobrar) no se toca.
  it("sin oro: DS 0x7a7e + DS 0x7a9e sin cobrar", () => {
    const s = freshState();
    s.gold = 50; // precio 200 > 50
    const r = buyHorse(s, 0, 0);
    expect(r.ok).toBe(false);
    expect(r.message).toBe('\n\n"Thou couldst not afford to feed it!"\nyells $.\n');
    expect(s.gold).toBe(50);
  });
});

describe("guild (llaves/gemas/antorchas: lote fijo +3/+4/+5, cap 99)", () => {
  it("precio regateado por INT desde GUILD_PRICES", () => {
    // town0 = [190,255,12]; keys a INT 0 → 190·2 = 380.
    expect(guildPrice(0, 0, 0)).toBe(380);
    expect(guildPrice(0, 2, 0)).toBe(24); // antorchas base 12 → 24
  });
  it("compra concede el lote fijo y cobra", () => {
    const s = freshState();
    s.gold = 1000;
    const beforeGems = s.gems;
    const price = guildPrice(0, 1, 33); // gems base 255 → 255 + ⌊255·1/100⌋ = 257
    const r = buyGuildItem(s, 0, 1, 33);
    expect(r.ok).toBe(true);
    expect(s.gems).toBe(beforeGems + 4);
    expect(s.gold).toBe(1000 - price);
  });
  // ★ HISTORIA DE ESTE ASERTO (#147 → #153). Nació sellando «con 99 ya en mano no
  // compra» como si fuera fidelidad; #147 leyó el cuerpo entero y lo re-selló como
  // DETECTOR del baseline del port; #153 retira el gate y lo pone en su forma
  // DERIVADA. El aserto se invierte a propósito: ése era el diseño, no una rotura.
  //
  // DERIVACIÓN: `buy_one_guild` (SHOPPES CS 0x02ba-0x03d9) y sus dos callers
  // (CS 0x03f6 lista, CS 0x04a2 entrada) NO tienen gate de tope. El único rechazo
  // previo al cobro es el de ORO (CS 0x0361). Pagado (CS 0x0374), el switch del ítem
  // llama a add_byte_capped con tope 0x63 sobre DS 0x57ac/0x57ad/0x57ae — o sea que
  // con 99 el original COBRA y el cap se come el lote entero.
  // CONTROL COMPLETO, no ventana: en TODO SHOPPES.OVL hay exactamente DOS `cmp` con
  // 0x63 —CS 0x0549 (reactivos, DS 0x5850) y CS 0x0a5e (equipo, DS 0x57c0)— y
  // ninguno cae en la cadena del gremio ni toca sus globales.
  it("con 99 ya en mano COBRA igual y el tope se come el lote (no hay gate)", () => {
    const s = freshState();
    s.gold = 9999;
    s.keys = 99;
    const price = guildPrice(0, 0, 33);
    const r = buyGuildItem(s, 0, 0, 33);
    expect(r.ok).toBe(true);
    expect(s.gold).toBe(9999 - price); // cobra
    expect(s.keys).toBe(99); // y no concede nada: add_byte_capped satura
  });
});

describe("blacksmithStock", () => {
  it("el herrero de Britain no está vacío y no contiene 0xFF (Nothing)", () => {
    const stock = blacksmithStock(0, data.weaponsSoldByMerchants);
    expect(stock.length).toBeGreaterThan(0);
    expect(stock).not.toContain(EQUIPMENT_NOTHING);
    // Britain vende armas a distancia: incluye Bow (26) y MagicBow (36).
    expect(stock).toContain(26);
    expect(stock).toContain(36);
  });
  it("Minoc vende la espada larga (30)", () => {
    expect(blacksmithStock(3, data.weaponsSoldByMerchants)).toContain(LONG_SWORD);
  });
});

describe("healerPrices", () => {
  it("Minoc tiene los precios reales de heal/cure/resurrect", () => {
    expect(healerPrices(5, data.healPrices, data.curePrices, data.resurrectPrices)).toEqual({
      heal: 35,
      cure: 20,
      resurrect: 200,
    });
  });
  it("Britain no tiene healer (null)", () => {
    expect(healerPrices(2, data.healPrices, data.curePrices, data.resurrectPrices)).toBeNull();
  });
  it("la resurrección más cara es la de Empath Abbey (262)", () => {
    const resurrects = HEALING_TOWNES.map(
      (loc) => healerPrices(loc, data.healPrices, data.curePrices, data.resurrectPrices)!.resurrect,
    );
    expect(Math.max(...resurrects)).toBe(262);
    expect(healerPrices(31, data.healPrices, data.curePrices, data.resurrectPrices)!.resurrect).toBe(262);
  });
});

describe("inn (posada EXACTA: rate·party, mata envenenados, pickup×meses)", () => {
  it("innAt: Britain (rate 2, cap 3, cama 21,10); Moonglow no tiene posada", () => {
    expect(innAt(2)).toMatchObject({ townIndex: 0, rate: 2, capacity: 3, roomX: 21, roomY: 10 });
    expect(innAt(1)).toBeNull();
  });
  it("Rest cobra haggle(rate·party, INT), cura a los vivos y MATA a los envenenados", () => {
    const s = freshState();
    s.gold = 1000;
    s.partySize = 2;
    const a = s.characters[0]!;
    const b = s.characters[1]!;
    a.partyStatus = 0; b.partyStatus = 0;
    a.status = "G"; a.currentHp = 1; a.maxHp = 50; a.class = "A"; a.intelligence = 20;
    b.status = "P"; b.currentHp = 10; b.maxHp = 40; // envenenado
    const price = innRestPrice(innAt(2)!, 2, a.intelligence); // rate2·party2=4, INT20 → 4+trunc(4·40/100)=4+1=5
    const r = innRest(s, 0, 2);
    expect(r.ok).toBe(true);
    expect(s.gold).toBe(1000 - price);
    expect(a.currentHp).toBe(50); // curado
    expect(a.currentMp).toBe(20); // clase A → MP=INT
    expect(b.status).toBe("D"); // envenenado murió
    expect(b.currentHp).toBe(0);
  });
  it("Rest sin oro no muta (Highwaymen)", () => {
    const s = freshState();
    s.gold = 0;
    s.partySize = 1;
    s.characters[0]!.partyStatus = 0;
    const r = innRest(s, 0, 2);
    expect(r.ok).toBe(false);
    expect(s.gold).toBe(0);
    // Insulto BYTE-EXACTO (DATA.OVL 0x4DF3 + 0x4E17, SHOPPES3 0x0108-0x0113):
    // saltos de línea del binario + atribución `screams\n$.` con expansor $.
    expect(r.message).toBe('\n\n"Highwaymen!\nCheap, at that!\nOUT!" screams\n$.\n');
  });
  it("Leave no cobra y baja party; Pickup cobra rate·10·haggle·meses", () => {
    const s = freshState();
    s.gold = 1000;
    s.partySize = 2;
    s.characters[0]!.partyStatus = 0;
    const comp = s.characters[1]!;
    comp.partyStatus = 0;
    comp.status = "G";
    // Leave al miembro 1 (no Avatar): sin coste, party→1, marcado en la posada.
    const lv = innLeave(s, 1, 2);
    expect(lv.ok).toBe(true);
    expect(s.gold).toBe(1000); // no cobra
    expect(s.partySize).toBe(1);
    expect(comp.partyStatus).toBe(2);
    // #124 — el leave COMPACTA el roster: el hospedado NO se queda en su índice,
    // va al ÚLTIMO slot (SHOPPES3 0x0465 escribe en 0x5788 = record[15]). Por eso
    // el pickup se localiza por IDENTIDAD, no por el índice de antes.
    expect(s.characters.indexOf(comp)).toBe(15);
    expect(s.characters).toHaveLength(16); // la longitud del roster se conserva
    // Pickup tras 3 meses: rate2·10=20, INT15 → 20+trunc(20·55/100)=20+11=31; ×3=93.
    comp.monthsAtInn = 3;
    const price = innPickupPrice(innAt(2)!, s.characters[0]!.intelligence, 3);
    const pk = innPickup(s, 0, s.characters.indexOf(comp), 2);
    expect(pk.ok).toBe(true);
    expect(s.gold).toBe(1000 - price);
    expect(s.partySize).toBe(2);
    expect(comp.partyStatus).toBe(0);
    // …y el pickup lo re-inserta en el slot `partySize` de ANTES (aquí 1), que es
    // donde 0x0829 escribe el TEMP: la contigüidad queda restaurada.
    expect(s.characters.indexOf(comp)).toBe(1);
    expect(s.characters).toHaveLength(16);
  });
});

describe("tavern (ronda por vivos, vino con gate de 3 copas, raciones +25)", () => {
  it("ronda cobra precio·vivos y excluye a los muertos", () => {
    const s = freshState();
    s.gold = 100;
    s.characters.forEach((c) => (c.partyStatus = 0xff)); // aísla el party
    s.partySize = 2;
    s.characters[0]!.partyStatus = 0; s.characters[0]!.status = "G";
    s.characters[1]!.partyStatus = 0; s.characters[1]!.status = "D"; // muerto: no cuenta
    // Barkeeper town0 = Moonglow, precio por cabeza 3. Sólo 1 vivo → 3.
    const r = buyTavernRound(s, 0);
    expect(r.ok).toBe(true);
    expect(s.gold).toBe(97);
  });
  // ★ #325 — EL GATE DE BORRACHERA SE MUDÓ. Estaba aquí, o sea al COMPRAR, y en el
  // binario está en `SHOPPES2 0x020a`: al ELEGIR la bebida, ANTES del discriminante
  // `'W'` de 0x027c. Vive hoy en `ShopConsole.drinkDrunkGate`, con sus CINCO cadenas
  // y el Y/N que este núcleo no podía emitir (no tiene canal de teclado).
  //
  // 🔴 EL TEST QUE ESTABA AQUÍ NO SE «ACTUALIZÓ»: SE LE MOVIÓ LA COBERTURA, Y ÉSTA ES
  // LA CUENTA. El viejo («gate EXACTO en 3 servicios: karma −1 y borrachera 25; en 4
  // NO re-castiga») guardaba TRES propiedades. Dónde vive cada una hoy:
  //
  //   (a) con el contador en 3 → timer 25 y karma −1
  //       → tavern-wine-list.test.ts «'N': eco `No!`, timer 25 y karma −1 — y CONTINÚA
  //         a la carta». Verificado corriéndolo por nombre: 1 passed.
  //   (b) `== 3` y no `>= 3`: con 4 no re-castiga
  //       → tavern-wine-list.test.ts «con el contador en 4 NO vuelve a avisar: es
  //         `== 3`, no `>= 3`». Verificado por nombre: 1 passed.
  //   (c) la compra sigue saliendo y cobra los 18 de la Rose
  //       → el negativo de abajo (gold 1000 → 982) + tavern-wine-list.test.ts «elegir
  //         'a'… y cobro de 18» y el careo de la tabla DS 0x4c48 de las seis.
  //
  // Ninguna de las tres se borró, y el sitio nuevo prueba MÁS que el viejo: el viejo no
  // podía ver las cinco emisiones ni el Y/N. Lo que se añade AQUÍ es el negativo que
  // fija la mudanza — «¿qué quedaría si la hago pasar?»: sin él, un merge que devolviera
  // el gate a `buyWine` dejaría el castigo DOBLE y esta suite no diría nada.
  // (El otro toque a este fichero, «buyWine: el pago imprime DS 0x9c40», NO es una
  // guarda movida: sólo perdió dos argumentos de la llamada; su aserto está intacto.)
  it("#325: el pago de la copa NO aplica ya karma ni borrachera (el gate se mudó al 0x020a)", () => {
    const s = freshState();
    s.gold = 1000;
    s.karma = 50;
    const r = buyWine(s, 0); // Rose 18
    expect(r.ok).toBe(true);
    expect(s.gold).toBe(982);
    expect(s.karma).toBe(50); // intacto: castigar es cosa del gate, no del pago
    expect(s.drunkTurns).toBeUndefined();
  });
  it("raciones: +25 comida por unidad mientras haya oro", () => {
    const s = freshState();
    s.gold = 100;
    s.food = 0;
    // town0 ración base 10, INT 33 → ~10/unidad. Compra 3.
    const r = buyRations(s, 0, 33, 3);
    expect(r.ok).toBe(true);
    expect(r.bought).toBe(3);
    expect(s.food).toBe(75);
  });
  it("rumor: precio por keyword (SHOPPES2 0x05ab)", () => {
    expect(rumorPrice("honesty")).toBe(50);
    expect(rumorPrice("crow")).toBe(200); // 'crown'
    expect(rumorPrice("zzz")).toBeNull();
  });
  // ── #320: el matcher del binario y el chisme que publica ──
  // Esperados EN CRUDO transcritos de DATA.OVL (tablas DS 0x4C74/0x4CA8/0x4CDC/
  // 0x4CF6), NO derivados de las constantes del sujeto.
  it("rumor: la clave es SUBCADENA de lo tecleado, con frontera de palabra ANTERIOR", () => {
    // Casa al principio del búfer (si == 0, salto directo a 0x0585).
    expect(rumorKeywordIndex("honesty")).toBe(0);
    expect(rumorKeywordIndex("hone")).toBe(0);
    // Casa tras un ESPACIO (byte[0xBCF7+si] == 0x20, 0x057e).
    expect(rumorKeywordIndex("the crown")).toBe(15);
    // NO casa pegada a otra letra: 'hone' dentro de 'dishonesty' va tras una 's'.
    expect(rumorKeywordIndex("dishonesty")).toBe(-1);
    // Tope de 15 caracteres del input_string (0x053f): la clave que cae más allá
    // del corte no existe para el binario.
    // ★ El testigo lleva un ESPACIO antes de la clave a propósito: con la clave
    // pegada a una letra el aserto pasaba con el tope puesto Y quitado (la
    // frontera ya la rechazaba), o sea que no medía el tope — mutante M3 vacuo.
    // Aquí la frontera CONCEDE y lo único que puede rechazar es el corte a 15.
    expect("aaaaaaaaaaaaaa hythloth".indexOf("hyth")).toBe(15); // control: cae JUSTO fuera
    expect(rumorKeywordIndex("aaaaaaaaaaaaaa hythloth")).toBe(-1);
    expect(rumorKeywordIndex("")).toBe(-1);
  });
  it("rumor: sujeto y lugar por la DOBLE indirección del mapa de cotilleo", () => {
    // idx 0 'hone' → sujeto [0x4CA8+0] y lugar [0x4CF6 + 2*mapa[0]] = plaza 0.
    expect(rumorGossip(0)).toEqual({ subject: "Malik", place: "Moonglow" });
    // idx 17 'amul' → mapa[17] = 9, que NO es el índice de clave: si alguien
    // indexara las plazas con 17 se saldría de las 13 y leería otra tabla.
    expect(rumorGossip(17)).toEqual({ subject: "Simon and Tessa", place: "a hidden mountain keep" });
    expect(rumorGossip(25)).toEqual({ subject: "Jotham", place: "a lighthouse south of Britain" });
  });
  it("rumor: payRumor cobra sólo con oro y NO cobra sin él", () => {
    const s = freshState();
    s.gold = 500;
    const r = payRumor(s, 15); // 'crow' → 200
    expect(r.ok).toBe(true);
    expect(s.gold).toBe(300);
    expect(r.subject).toBe("Terrance");
    expect(r.place).toBe("Britain");
    const pobre = freshState();
    pobre.gold = 199;
    expect(payRumor(pobre, 15).ok).toBe(false);
    expect(pobre.gold).toBe(199); // la rama sin oro NO descuenta (0x05f0)
  });
  it("rumor: las CUATRO formulaciones del sorteo, en el orden de DS 0x4D44", () => {
    expect(RUMOR_FORM_INDEX.length).toBe(4);
    expect([...RUMOR_FORM_INDEX]).toEqual([85, 86, 87, 88]);
  });
});

describe("shipwright (F/S regateado, coords de muelle)", () => {
  it("fragata Jhelom (town0) base 600 → 1200 a INT 0", () => {
    expect(shipwrightPrice(0, "frigate", 0)).toBe(1200);
    expect(shipwrightPrice(0, "skiff", 0)).toBe(400); // skiff base 200 → 400
  });
  it("buyShip cobra y devuelve las coords de muelle", () => {
    const s = freshState();
    s.gold = 2000;
    const r = buyShip(s, 0, "frigate", 33);
    expect(r.ok).toBe(true);
    expect(r.dockX).toBe(39);
    expect(r.dockY).toBe(221);
    expect(s.gold).toBeLessThan(2000);
  });
});

describe("shoppeKeeperAt", () => {
  it("resuelve el herrero de Britain (índice 0)", () => {
    expect(shoppeKeeperAt(2, "Blacksmith", shoppeMap, data.storeNames, data.shoppeKeeperNames)).toEqual({
      index: 0,
      shopName: "Iolo's Bows",
      keeperName: "Gwenneth",
    });
  });
  it("resuelve el healer de Minoc (índice 33, tras eliminar 'Simplon')", () => {
    const info = shoppeKeeperAt(5, "Healer", shoppeMap, data.storeNames, data.shoppeKeeperNames);
    expect(info?.index).toBe(33);
    expect(info?.keeperName).not.toBe("Simplon");
    expect(info?.shopName.length).toBeGreaterThan(0);
  });
  it("devuelve null si no hay ese mercader en la ubicación", () => {
    expect(shoppeKeeperAt(2, "Shipwright", shoppeMap, data.storeNames, data.shoppeKeeperNames)).toBeNull();
  });
});

describe("transacciones", () => {
  it("buyEquipment con oro justo resta oro e incrementa la cantidad", () => {
    const s = freshState();
    s.gold = 70;
    const before = s.equipmentQuantities[LONG_SWORD] ?? 0;
    const r = buyEquipment(s, LONG_SWORD, 70);
    expect(r.ok).toBe(true);
    expect(s.gold).toBe(0);
    expect(s.equipmentQuantities[LONG_SWORD]).toBe(before + 1);
  });

  it("buyEquipment con oro insuficiente falla sin mutar", () => {
    const s = freshState();
    s.gold = 69;
    const before = s.equipmentQuantities[LONG_SWORD] ?? 0;
    const r = buyEquipment(s, LONG_SWORD, 70);
    expect(r.ok).toBe(false);
    expect(s.gold).toBe(69);
    expect(s.equipmentQuantities[LONG_SWORD] ?? 0).toBe(before);
  });

  it("sellEquipment sin stock falla; con stock añade oro (cap 9999)", () => {
    const s = freshState();
    s.equipmentQuantities[LONG_SWORD] = 0;
    expect(sellEquipment(s, LONG_SWORD, 50).ok).toBe(false);
    s.equipmentQuantities[LONG_SWORD] = 1;
    s.gold = 0;
    const r = sellEquipment(s, LONG_SWORD, 50);
    expect(r.ok).toBe(true);
    expect(s.gold).toBe(50);
    expect(s.equipmentQuantities[LONG_SWORD]).toBe(0);
    // Cap de oro a 9999 al vender.
    s.equipmentQuantities[LONG_SWORD] = 1;
    s.gold = 9990;
    sellEquipment(s, LONG_SWORD, 50);
    expect(s.gold).toBe(9999);
  });

  it("caps del binario: equipo/reactivo rechazan a 99; herrero no compra 0x1B/0x1D", () => {
    const s = freshState();
    s.gold = 9999;
    s.equipmentQuantities[LONG_SWORD] = 99;
    expect(buyEquipment(s, LONG_SWORD, 10).ok).toBe(false); // ya al tope
    s.reagentQuantities[0] = 99;
    expect(buyReagent(s, 0, 5, 10).ok).toBe(false); // reactivo al tope
    expect(s.gold).toBe(9999); // no cobró en ninguno
    // El herrero rechaza los ítems no vendibles 0x1B (27) y 0x1D (29).
    s.equipmentQuantities[0x1b] = 3;
    expect(sellEquipment(s, 0x1b, 50).ok).toBe(false);
    expect(sellEquipment(s, 0x1d, 50).ok).toBe(false);
  });

  it("buyReagent suma cantidad al reactivo y resta oro", () => {
    const s = freshState();
    s.gold = 100;
    const before = s.reagentQuantities[0] ?? 0;
    const r = buyReagent(s, 0, 5, 60);
    expect(r.ok).toBe(true);
    expect(s.gold).toBe(40);
    expect(s.reagentQuantities[0]).toBe(before + 5);
  });

  it("healerHeal: heal restaura HP, cobra y falla si no hay necesidad", () => {
    const s = freshState();
    s.gold = 100;
    const c = s.characters[0]!;
    c.maxHp = 200;
    c.currentHp = 50;
    c.status = "G";
    const r = healerHeal(s, 0, "heal", 35);
    expect(r.ok).toBe(true);
    expect(c.currentHp).toBe(200);
    expect(s.gold).toBe(65);
    // ya al máximo: no hay necesidad, no cobra
    const r2 = healerHeal(s, 0, "heal", 35);
    expect(r2.ok).toBe(false);
    expect(s.gold).toBe(65);
  });

  it("healerHeal: cure quita el veneno; resurrect corre resurrect_apply y deja HP = el máximo NUEVO (A4-PARITY2 D-83)", () => {
    const s = freshState();
    s.gold = 500;
    const c = s.characters[0]!;
    c.status = "P";
    expect(healerHeal(s, 0, "cure", 20).ok).toBe(true);
    expect(c.status).toBe("G");

    c.status = "D";
    c.currentHp = 0;
    const r = healerHeal(s, 0, "resurrect", 200);
    expect(r.ok).toBe(true);
    // SHOPPES 0x16f5 -> CAST2.OVL 0x05e0 (mode 0xff), then 0x16f8-0x1703 HP := max HP. The INIT.GAM
    // Avatar (class A, INT 15, XP 150, level 2 / max 60, karma 75): XP 150*75/100 = 112 (truncated),
    // level 1 + bitlength(112/100 = 1) = 2, max 60, MP = INT = 15, HP = 60 (was: status G, HP 1).
    expect(c.status).toBe("G");
    expect(c.exp).toBe(112);
    expect(c.level).toBe(2);
    expect(c.maxHp).toBe(60);
    expect(c.currentHp).toBe(60);
    expect(c.currentMp).toBe(15);
    expect(s.gold).toBe(280); // 500 - 20 (cure) - 200 (resurrect)
  });

  it("healerHeal falla sin oro suficiente", () => {
    const s = freshState();
    s.gold = 10;
    const c = s.characters[0]!;
    c.status = "D";
    const r = healerHeal(s, 0, "resurrect", 200);
    expect(r.ok).toBe(false);
    expect(c.status).toBe("D");
    expect(s.gold).toBe(10);
  });
});

describe("postPurchaseDrain — merma de la Falsedad tras un pago (SHOPPES 0x019a)", () => {
  // rand espía: registra (lo,hi) de cada llamada y devuelve un valor fijo, para
  // verificar tanto el consumo de RNG (orden/conteo) como la merma resultante.
  function spyRand(value: number): { fn: RandFn; calls: Array<[number, number]> } {
    const calls: Array<[number, number]> = [];
    return {
      calls,
      fn: (lo, hi) => {
        calls.push([lo, hi]);
        return value;
      },
    };
  }

  it("con la Falsedad (Shadowlord 0) presente: tira 1 rand(1,64) y merma el oro", () => {
    const s = freshState();
    s.gold = 100;
    s.position.location = 6; // Trinsic
    s.shadowlordLocs = [6, 3, 7]; // índice 0 (Falsedad) = aquí
    const { fn, calls } = spyRand(40);
    expect(postPurchaseDrain(s, fn)).toBe(40);
    expect(calls).toEqual([[1, 64]]); // EXACTAMENTE 1 rand, rango inclusivo 1..64 (push 1/0x40)
    expect(s.gold).toBe(60);
  });

  it("gate PRIMERO (0x019f jne ret): sin la Falsedad NO se tira rand ni se merma", () => {
    // Otro Shadowlord presente (índice 1 = Odio) — no la Falsedad.
    const other = freshState();
    other.gold = 100;
    other.position.location = 6;
    other.shadowlordLocs = [2, 6, 7];
    const a = spyRand(40);
    expect(postPurchaseDrain(other, a.fn)).toBe(0);
    expect(a.calls).toEqual([]); // 0 rands: el cmp/jne corta ANTES del rand
    expect(other.gold).toBe(100);

    // Ningún Shadowlord en la ciudad.
    const none = freshState();
    none.gold = 100;
    none.position.location = 6;
    none.shadowlordLocs = [1, 2, 3];
    const b = spyRand(40);
    expect(postPurchaseDrain(none, b.fn)).toBe(0);
    expect(b.calls).toEqual([]);
    expect(none.gold).toBe(100);
  });

  it("suelo 0 (sub_word_floored 0x3f54): si el roll supera el oro, queda en 0", () => {
    const s = freshState();
    s.gold = 30;
    s.position.location = 6;
    s.shadowlordLocs = [6, 3, 7];
    const { fn } = spyRand(64);
    expect(postPurchaseDrain(s, fn)).toBe(30);
    expect(s.gold).toBe(0);
  });
});

// ---------------------------------------------------------------------------
// #147 — FABRICACIÓN «Here thou art!»: las 3 tiendas devolvían una frase que NO
// EXISTE en el juego (cero ocurrencias en DATA.OVL, en original/u5/ultima5/* y en
// game/assets; lo único parecido es «Here thou art... » del TLK, cadena DISTINTA
// y de otro flujo). Estos asertos fijan la cadena DERIVADA de cada flujo, leída
// del cuerpo entero en el disasm y verificada byte a byte contra DATA.OVL
// (convención del repo: fileoff = DS + 0x10).
// ---------------------------------------------------------------------------
describe("#147 mensaje de éxito DERIVADO de las 3 tiendas (no fabricado)", () => {
  it("buyWine: el pago imprime DS 0x9c40 (SHOPPES2 0x0368, tras `sub [g_gold]` 0x035d)", () => {
    const s = freshState();
    s.gold = 1000;
    const r = buyWine(s, 0);
    expect(r.ok).toBe(true);
    expect(r.message).toBe('\nEnjoy!"');
  });

  it("buyReagent: el pago imprime DS 0x7988 por el expansor de $ (SHOPPES 0x63d)", () => {
    const s = freshState();
    s.gold = 100;
    const r = buyReagent(s, 0, 5, 60);
    expect(r.ok).toBe(true);
    // `$` = nombre del tendero; lo expande el call-site (expandShoppeTemplate),
    // igual que la vía CON pool de shop-console (reagentDealYes).
    expect(r.message).toBe('\n"I thank thee!"\nsays $.\n');
  });

  it("buyReagent al tope 99: DS 0x792c, NO «Thou canst carry no more!» (SHOPPES 0x550)", () => {
    const s = freshState();
    s.gold = 9999;
    s.reagentQuantities[0] = 99;
    const r = buyReagent(s, 0, 5, 10);
    expect(r.ok).toBe(false);
    // El binario dice «canst NOT carry ANY more» — otra oración que la del port.
    expect(r.message).toBe('\n\n"Thou canst not carry any more!"\n\n');
  });

  it("buyGuildItem: el pago imprime DS 0x78a0 («Sold!»), NI la del herbolario NI la del vino (SHOPPES 0x3a6)", () => {
    const s = freshState();
    s.gold = 1000;
    const r = buyGuildItem(s, 0, 1, 33);
    expect(r.ok).toBe(true);
    // El sufijo `m'lady`/`m'lord` (DS 0x78c0/0x78c8) + `?\n\n` (DS 0x78d0) lo
    // compone el call-site por género del negociador, no el core.
    expect(r.message).toBe('\n"Sold!"\nsays $.\n\n"What else, \n');
  });
});

// ---------------------------------------------------------------------------
// #147 TANDA 2 — el resto de pares de la tabla del acta de main: shops.ts emitía
// prosa propia teniendo la cadena DERIVADA al lado, en cmd-strings.ts. Cada aserto
// se corresponde con UN cuerpo leído entero en el disasm y verificado byte a byte
// contra DATA.OVL (convención del repo: fileoff = DS + 0x10).
// ---------------------------------------------------------------------------
describe("#147 tanda 2: los pares restantes de shops.ts contra cmd-strings.ts", () => {
  it("buyEquipment paga: CS 0x0ad9 empuja DS 0x7bb4 al expansor", () => {
    const s = freshState();
    s.gold = 1000;
    const r = buyEquipment(s, LONG_SWORD, 50);
    expect(r.ok).toBe(true);
    expect(r.message).toBe("\nSold!\n");
  });

  it("buyEquipment al tope 99: CS 0x0a65 empuja DS 0x7b76 (UN salto de linea a cada lado)", () => {
    const s = freshState();
    s.gold = 9999;
    s.equipmentQuantities[LONG_SWORD] = 99;
    const r = buyEquipment(s, LONG_SWORD, 10);
    expect(r.ok).toBe(false);
    // ★ Es la copia del EQUIPO (DS 0x7b76), NO la del reactivo (DS 0x792c, que lleva
    // DOS saltos a cada lado). Confundirlas es el error que el acta pre-registro.
    expect(r.message).toBe('\n"Thou canst not carry any more!"\n');
  });

  it("sellEquipment: CS 0x0f2a empuja DS 0x7d76 al expansor (el eco de Y va HORNEADO en el literal)", () => {
    const s = freshState();
    s.equipmentQuantities[LONG_SWORD] = 3;
    s.gold = 0;
    const r = sellEquipment(s, LONG_SWORD, 50);
    expect(r.ok).toBe(true);
    expect(r.message).toBe('Yes\n\n"Done!"\nsays $.');
  });

  // buyHorse sin oro (DS 0x7a7e + DS 0x7a9e): su aserto ya existía en el bloque de
  // buyHorse, arriba, y se RE-SELLA allí con la historia dentro en vez de duplicarlo.

  it("buyTavernRound servida: CS 0x01c8 empuja DS 0x9b16, la copia con cola de DOS saltos", () => {
    const s = freshState();
    s.gold = 1000;
    const r = buyTavernRound(s, 0);
    expect(r.ok).toBe(true);
    // Hermana de la del vino (DS 0x9c40 `\nEnjoy!"`), pero NO la misma: esta cierra
    // con `\n\n`. Son dos entradas distintas de DATA.OVL.
    expect(r.message).toBe('\nEnjoy!"\n\n');
  });
});
