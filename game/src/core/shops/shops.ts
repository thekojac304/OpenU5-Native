/**
 * Sistema de TIENDAS de Ultima V — núcleo puro (sin DOM, sin UI).
 *
 * Precios, stock, servicios y transacciones de los 8 tipos de mercader.
 * REGLAS EXACTAS del binario (SHOPPES.OVL ×3, Task 3.6, re/notes/shops.md); ya
 * NO son el port de Redux. Hallazgo capital: TODAS las tiendas regatean por la
 * INTELIGENCIA del comprador (record+0x0E), no por Destreza ni Karma.
 *
 * Regateo (SHOPPES:0x02D8 y gemelos, __lmul 0x61B2 / __ldiv 0x6110):
 *   compra = base + ⌊base·(100 − 3·INT) / 100⌋      (dos pasos; trunc hacia 0)
 *   venta  = ⌊3·INT·base / 100⌋ + 1                  (INT del vendedor)
 * El punto neutro está en INT ≈ 33 (100/3). INT 0 ⇒ 2×base; INT 50 ⇒ ~½base.
 *
 * Convención de transacciones (igual que party.ts): mutan `GameState` in situ y
 * devuelven un `TxResult { ok, message }`; ante fallo NO mutan.
 */
import type { GameState } from "../state.js";
import { postPurchaseGoldDrain, shadowlordHereIndex } from "../world/blackthorn.js";
// Cadenas DERIVADAS de DATA.OVL (fileoff = DS + 0x10) con su emisor citado en el
// disasm. Las tres tiendas de abajo devolvían en su lugar una frase FABRICADA
// («Here thou art!», cero ocurrencias en el juego) — task #147.
import { SHOP_UI, TAVERN_ALIVE_WORD } from "../world/cmd-strings.js";
import {
  advanceClock,
  ringRegenSweep,
  type RandFn,
  type SkyRefreshCtx,
} from "../world/survival.js";
import { rosterLeaveCompact, rosterPickupInsert } from "../party.js";
import { applyResurrect } from "../magic/cast.js";
import {
  DRUNK_CUP_GATE,
  DRUNK_TIMER_TURNS,
  FRIGATE_PRICES,
  GUILD_GRANT,
  GUILD_PRICES,
  HORSE_PRICES,
  INN_CAPACITY,
  INN_RATE,
  INN_ROOM_X,
  INN_ROOM_Y,
  RATION_BASE,
  RATION_FOOD_PER_UNIT,
  RUMOR_KEYWORDS,
  RUMOR_PRICES,
  RUMOR_SUBJECTS,
  RUMOR_GOSSIP_MAP,
  RUMOR_PLACES,
  SHIP_DOCK_X,
  SHIP_DOCK_Y,
  SKIFF_PRICES,
  TAVERN_ROUND_PRICE,
  WINE_PRICES,
  shopTownIndex,
} from "./shop-tables.js";

// ---------------------------------------------------------------------------
// Núcleo de regateo por Inteligencia (compartido por TODAS las tiendas)
// ---------------------------------------------------------------------------

/**
 * Precio de COMPRA regateado por INT. Réplica bit a bit del binario
 * (SHOPPES:0x02D8-0x0318): `base + ⌊base·(100 − 3·INT)/100⌋`, con la división
 * aplicada SÓLO al término interno y truncada hacia cero (Borland __ldiv). El
 * término es negativo para INT > 33 (encarece por debajo, abarata por encima).
 * Sin clamp de mínimo (el binario no lo tiene).
 */
export function shopBuyPrice(base: number, intelligence: number): number {
  const term = Math.trunc((base * (100 - 3 * intelligence)) / 100);
  return base + term;
}

/**
 * Precio de VENTA regateado por INT. Binario SHOPPES:0x0EB9-0x0ED6:
 * `⌊3·INT·base/100⌋ + 1`. Simétrico a la compra alrededor de INT ≈ 33.
 */
export function shopSellPrice(base: number, intelligence: number): number {
  return Math.trunc((3 * intelligence * base) / 100) + 1;
}

/** Los 8 tipos de mercader (claves de ShoppeKeeperMap.json). */
export type ShopType =
  | "Blacksmith"
  | "Barkeeper"
  | "HorseSeller"
  | "Shipwright"
  | "MagicSeller"
  | "GuildMaster"
  | "Healer"
  | "InnKeeper";

/** Byte centinela `Equipment.Nothing` en las tablas de stock. */
export const EQUIPMENT_NOTHING = 0xff;

/** Topes del binario: inventario byte (0x63) y oro word (0x270F). */
const CAP_99 = 99;
const CAP_GOLD = 9999;

// ---------------------------------------------------------------------------
// Location: número de enum (SingleMapReference.Location) → nombre de
// ShoppeKeeperMap.json. Portado de References/Maps/SingleMapReference.cs:22-63.
// ---------------------------------------------------------------------------
/** Nº de location → clave `Location` de ShoppeKeeperMap.json. Exportada para que el
 *  derivador OFFLINE de anclas (e2e/espejo-tour/tools/derive-anchors.mjs, que es .mjs y no
 *  puede importar este .ts) pueda ser PINCHADO contra ella por unit test en vez de que su
 *  copia derive en silencio. */
export const LOCATION_NAMES: Record<number, string> = {
  0: "Britannia_Underworld",
  1: "Moonglow",
  2: "Britain",
  3: "Jhelom",
  4: "Yew",
  5: "Minoc",
  6: "Trinsic",
  7: "Skara_Brae",
  8: "New_Magincia",
  17: "Lord_Britishs_Castle",
  18: "Palace_of_Blackthorn",
  19: "West_Britanny",
  20: "North_Britanny",
  21: "East_Britanny",
  22: "Paws",
  23: "Cove",
  24: "Buccaneers_Den",
  26: "Bordermarch",
  30: "Lycaeum",
  31: "Empath_Abbey",
  32: "Serpents_Hold",
};

// ---------------------------------------------------------------------------
// Herrero (Blacksmith): stock y precios de armas/armadura
// ---------------------------------------------------------------------------

/**
 * Stock de equipo del herrero de la ciudad `townIndex` (0-8, en el orden de las
 * 9 ciudades con herrero: Britain, Jhelom, Yew, Minoc, Trinsic, LB's Castle,
 * Buccaneer's Den, Bordermarch, Serpent's Hold). `weaponsSold` es
 * `data.json:weaponsSoldByMerchants` (9×8). Filtra los huecos `Nothing` (0xFF).
 * Devuelve ids de `Equipment` (índices de equipmentBasePrices/equipmentQuantities).
 */
export function blacksmithStock(townIndex: number, weaponsSold: number[][]): number[] {
  const row = weaponsSold[townIndex];
  if (!row) return [];
  return row.filter((id) => id !== EQUIPMENT_NOTHING);
}

/**
 * Precio de compra de una pieza de equipo, regateado por INTELIGENCIA del
 * comprador (binario SHOPPES:0x0896 `[bx+0x55b6]` = record+0x0E = INT, NO DEX).
 * = `shopBuyPrice(base, INT)`.
 */
export function equipmentBuyPrice(basePrice: number, intelligence: number): number {
  return shopBuyPrice(basePrice, intelligence);
}

/**
 * Precio de venta de una pieza de equipo (binario SHOPPES:0x0E76): usa la INT
 * del vendedor y añade `+1`. = `shopSellPrice(base, INT)`. Si `base==0` el
 * herrero no compra el ítem (lo maneja `sellEquipment`).
 */
export function equipmentSellPrice(basePrice: number, intelligence: number): number {
  return shopSellPrice(basePrice, intelligence);
}

// ---------------------------------------------------------------------------
// Reactivos (MagicSeller): precio por-ciudad regateado por Inteligencia
// ---------------------------------------------------------------------------

/**
 * Precio de un reactivo. El binario NO usa Karma ni "pay-what-you-want": lee la
 * tabla por-ciudad `reagentBasePrices[town*8 + slot]` (5 towns × 8 reactivos,
 * DS 0x3A32) y aplica el mismo regateo por INT (SHOPPES:0x0574-0x05AB). El input
 * "how many" (≤12) se lee pero NO escala precio ni cantidad: cada compra concede
 * la cantidad FIJA `reagentQuantities[town*8+slot]` por este pago único.
 */
export function reagentPrice(basePrice: number, intelligence: number): number {
  return shopBuyPrice(basePrice, intelligence);
}

/**
 * Precio de un reactivo por (ciudad, slot) resuelto desde las tablas 5×8.
 * `magicTownIndex` = posición 0-based en SHOP_TOWNES.MagicSeller. Devuelve
 * `null` si el slot está vacío (base 0 = no vendido).
 */
export function reagentPriceAt(
  magicTownIndex: number,
  slot: number,
  intelligence: number,
  reagentBasePrices: number[],
): number | null {
  const base = reagentBasePrices[magicTownIndex * 8 + slot] ?? 0;
  if (base <= 0) return null;
  return shopBuyPrice(base, intelligence);
}

/** Cantidad de reactivo concedida por una compra (tabla fija, cap se aplica al sumar). */
export function reagentGrantQty(
  magicTownIndex: number,
  slot: number,
  reagentQuantities: number[],
): number {
  return reagentQuantities[magicTownIndex * 8 + slot] ?? 0;
}

// ---------------------------------------------------------------------------
// Provisiones / Caballos / Barcos: ajuste por Inteligencia (mismo regateo)
// ---------------------------------------------------------------------------

/**
 * Precio de una provisión regateado por INT. El binario usa el MISMO regateo que
 * el resto de tiendas (no la fórmula "−1.5%/punto truncado por punto" de Redux).
 * Las provisiones de comida/antorchas del binario viven en la taberna (raciones)
 * y el gremio (antorchas); esta función mantiene la firma para el panel de UI.
 */
export function provisionPrice(base: number, intelligence: number): number {
  return shopBuyPrice(base, intelligence);
}

/**
 * Precio de un caballo regateado por INT. base = `HORSE_PRICES[horseTownIdx]`
 * (100/130/160/190; binario DS 0x3C30). Equivale al `2×base` de Redux con
 * −1.5%/pt, pero aquí se codifica el precio-base a INT=0 real del binario.
 */
export function horsePrice(base: number, intelligence: number): number {
  return shopBuyPrice(base, intelligence);
}

/** Precio de un barco: mismo regateo por INT que el resto (shipwright §7B). */
export function shipPrice(base: number, intelligence: number): number {
  return shopBuyPrice(base, intelligence);
}

/**
 * Compra de un caballo (HorseSeller, SHOPPES.OVL:0x07BE). Núcleo de oro: precio =
 * `horsePrice(HORSE_PRICES[horseTownIdx], INT_comprador)` (base binaria a INT=0). Si
 * `gold < precio` → **"Thou couldst not afford to feed it!"** (DS 0x7a7e/0x7a9e), sin
 * cobrar. Si hay oro → resta y devuelve ok con **"Yes!"** (0x7a78). El binario SÓLO
 * llega aquí tras encontrar una casilla adyacente válida (tile ∈ {0x44,0x45,0x05}
 * libre; si no, "The stables are closed." ANTES del precio) y coloca la montura
 * (tile 0x10) en esa celda: eso lo maneja la capa de juego (`game.stableHorse`). El
 * caballo ∈ grupo SHOPPES.OVL → aplica la merma de la Falsedad (0x0951 call 0x19a).
 */
export function buyHorse(
  state: GameState,
  horseTownIdx: number,
  intelligence: number,
): TxResult {
  const price = horsePrice(HORSE_PRICES[horseTownIdx] ?? 0, intelligence);
  if (state.gold < price) {
    // CS 0x0934 + CS 0x093b: el binario parte la oración en DOS impresores —
    // DS 0x7a7e por el llano (0x75c0) y DS 0x7a9e por el expansor de `$` (0x26).
    // La partición es de la segmentación de DATA.OVL; es UN mensaje.
    return { ok: false, message: SHOP_UI.horseBroke }; // DS 0x7a7e + DS 0x7a9e
  }
  state.gold -= price;
  return { ok: true, message: SHOP_UI.horseYesExcl }; // DS 0x7a78 (CS 0x0924)
}

// ---------------------------------------------------------------------------
// Healer (curandero): heal / cure / resurrect
// ---------------------------------------------------------------------------

export type HealerService = "heal" | "cure" | "resurrect";

export interface HealerPriceTable {
  heal: number;
  cure: number;
  resurrect: number;
}

/**
 * Orden de las 7 ciudades con healer (números de enum Location), tal como
 * aparecen en `DATA.OVL:SHOPPE_KEEPER_TOWNES_HEALING` (offset 0x243a, 7 bytes):
 * Minoc, Trinsic, Skara Brae, East Britanny, Cove, Lycaeum, Empath Abbey.
 * Las tablas de precios (heal/cure/resurrect) se indexan por este orden.
 */
export const HEALING_TOWNES: number[] = [5, 6, 7, 21, 23, 30, 31];

/**
 * Precios de los tres servicios del healer en `location`, o `null` si esa
 * ubicación no tiene healer. Redux HealerServices.BuildServicesList: indexa las
 * tablas por la posición de la ubicación en SHOPPE_KEEPER_TOWNES_HEALING.
 * Sin dependencia de karma. `heal/cure/resurrectPrices` son las tablas de
 * data.json (healPrices, curePrices, resurrectPrices).
 */
export function healerPrices(
  location: number,
  healPrices: number[],
  curePrices: number[],
  resurrectPrices: number[],
  healingTownes: number[] = HEALING_TOWNES,
): HealerPriceTable | null {
  const idx = healingTownes.indexOf(location);
  if (idx < 0) return null;
  return {
    heal: healPrices[idx] ?? 0,
    cure: curePrices[idx] ?? 0,
    resurrect: resurrectPrices[idx] ?? 0,
  };
}

// ---------------------------------------------------------------------------
// Posada (Innkeeper): SHOPPES3.OVL — Rest / Leave / Pick up (reglas exactas)
// ---------------------------------------------------------------------------

export interface InnInfo {
  /** Posición 0-based en SHOP_TOWNES.InnKeeper (índice de las tablas de posada). */
  townIndex: number;
  /** Unidad de tarifa base (INN_RATE). Rest = rate·party; Leave/Pickup = rate·10. */
  rate: number;
  /** Capacidad de huéspedes hospedados (INN_CAPACITY). */
  capacity: number;
  /** Coordenada de la cama a la que teletransporta el descanso (INN_ROOM_X/Y). */
  roomX: number;
  roomY: number;
}

/** Datos de la posada en `location`, o `null` si no hay posada allí. */
export function innAt(location: number): InnInfo | null {
  const idx = shopTownIndex("InnKeeper", location);
  if (idx < 0) return null;
  return {
    townIndex: idx,
    rate: INN_RATE[idx]!,
    capacity: INN_CAPACITY[idx]!,
    roomX: INN_ROOM_X[idx]!,
    roomY: INN_ROOM_Y[idx]!,
  };
}

/** Precio de descansar una noche: `haggle(rate·partySize, INT)` (SHOPPES3:0x0095). */
export function innRestPrice(inn: InnInfo, partySize: number, intelligence: number): number {
  return shopBuyPrice(inn.rate * partySize, intelligence);
}

/** Tarifa MENSUAL de dejar/recoger un compañero: `haggle(rate·10, INT)` (SHOPPES3:0x037C). */
export function innMonthlyRate(inn: InnInfo, intelligence: number): number {
  return shopBuyPrice(inn.rate * 10, intelligence);
}

/** Precio de recoger un compañero: `tarifaMensual · max(1, meses)` (SHOPPES3:0x077F). */
export function innPickupPrice(inn: InnInfo, intelligence: number, months: number): number {
  return innMonthlyRate(inn, intelligence) * Math.max(1, months);
}

/** Restaura los MP de un miembro tras dormir según su clase (A/M→INT, B→INT/2). */
function restoreMpByClass(rec: {
  class: string;
  intelligence: number;
  currentMp: number;
}): void {
  if (rec.class === "A" || rec.class === "M") rec.currentMp = rec.intelligence;
  else if (rec.class === "B") rec.currentMp = Math.trunc(rec.intelligence / 2);
}

/** Índices de los personajes que están EN la party (partyStatus 0x00). */
function partyIndices(state: GameState): number[] {
  const out: number[] = [];
  state.characters.forEach((c, i) => {
    if (c.partyStatus === 0) out.push(i);
  });
  return out;
}

/** Huéspedes hospedados en `location` (partyStatus == location; +0x1F del record). */
export function innGuestCount(state: GameState, location: number): number {
  return state.characters.filter((c) => c.partyStatus === location).length;
}

export interface InnRestResult extends TxResult {
  /** Coordenada de la cama para teletransportar el party (sólo si ok). */
  roomX?: number;
  roomY?: number;
}

/**
 * Descanso en la posada (tecla R, SHOPPES3:0x0072). Cobra `haggle(rate·party,
 * INT_del_negociador)`; si no hay habitación libre o falta oro, no muta y falla.
 * Por cada miembro vivo: HP=maxHP, MP por clase, `S→G`, y **`P → D` (HP=0): el
 * descanso MATA a los envenenados**. Devuelve las coords de la cama para que la
 * capa de presentación teletransporte y avance el reloj a las 6:00.
 */
export function innRest(state: GameState, buyerIdx: number, location: number): InnRestResult {
  const inn = innAt(location);
  if (!inn) return { ok: false, message: "There is no inn here." };
  if (inn.capacity <= innGuestCount(state, location)) {
    return { ok: false, message: "I have no room available!" };
  }
  const buyer = state.characters[buyerIdx];
  if (!buyer) return { ok: false, message: "No such person." };
  const price = innRestPrice(inn, state.partySize, buyer.intelligence);
  if (state.gold < price) {
    // Sin oro (SHOPPES3 0x0106 jge): DS 0x4de3 `\n\n"Highwaymen!\nCheap, at
    // that!\nOUT!" ` (print 0x3670) + DS 0x4e07 `screams\n$.\n` (expansor $
    // 0x9d8e = nombre del posadero; lo sustituye la capa de presentación con
    // expandShoppeTemplate, como el resto de atribuciones `says $.`).
    return { ok: false, message: '\n\n"Highwaymen!\nCheap, at that!\nOUT!" screams\n$.\n' };
  }
  state.gold -= price;
  for (const i of partyIndices(state)) {
    const rec = state.characters[i]!;
    if (rec.status === "D") continue;
    rec.currentHp = rec.maxHp;
    restoreMpByClass(rec);
    if (rec.status === "P") {
      rec.status = "D";
      rec.currentHp = 0;
    } else if (rec.status === "S") {
      rec.status = "G";
    }
  }
  return { ok: true, message: "Morning!", roomX: inn.roomX, roomY: inn.roomY };
}

/**
 * Tope de iteraciones del bucle nocturno. NO es del binario: allí el bucle sólo
 * sale por `g_hour == 6`, y con Time-stop ('T') el reloj no avanza y el original se
 * queda colgado. El clon corre en un navegador y no puede colgarse, así que corta.
 * La cota es holgada: el peor caso REAL es Quickness ('Q'), que parte el paso a 4
 * min ⇒ ≤ 1440/4 = 360 iteraciones por vuelta de día.
 */
const INN_NIGHT_MAX_STEPS = 1000;

/**
 * La NOCHE de la posada — bucle SHOPPES3 0x01b5-0x01f4, leído instrucción a
 * instrucción. Corre el RELOJ DEL JUEGO (`advance_clock`, kernel 0x4F7C), no
 * aritmética de calendario: por eso consume antorcha/luz por minuto, cruza la
 * medianoche con su re-sorteo de Shadowlords y mueve el stream de RNG.
 *
 *   01b5  si = 12
 *   01bd  PRE-bucle: `advance_clock(5)` ×12 = 60 min, SIN comprobar la hora ni
 *         tocar nada más; sale por `dec si / je 0x1ef` DIRECTO al check de hora.
 *   01ca  bucle NOCHE, por iteración:
 *           0x01ca `beep_ticks(1)`      (kernel 0x3ae6 — presentación: gated por
 *                                        el flag de sonido [0x58a4], sin RNG)
 *           0x01d1 `kernel_ring_regen`  (kernel 0x400c → `ringRegenSweep`)
 *           0x01d4 `draw_status_panel`  (kernel 0x2900 — presentación)
 *           0x01d7 `advance_clock(9)`
 *           0x01de si `g_hour` quedó en 0x14 (20) ó en 5 → `call 0x7a9a`
 *   01ef  repite mientras `g_hour != 6`.
 *
 * El paso de 9 minutos es lo que hace que el MINUTO de despertar sea variable: el
 * bucle sale al PRIMER aterrizaje dentro de la hora 6, que cae en el minuto 0..8
 * según con qué minuto se entrara. (El caso en que el pre-bucle ya deja la hora en
 * 6 sale sin dar ni un paso de 9, y entonces el minuto puede ser cualquiera.)
 *
 * `onHourTiles` es el destino del `call 0x7a9a`: SHOPPES3 está en la banda 4
 * (`near_call_base` 0xe1e0, overlay-load-layout.md §1), así que el crudo 0x98ba →
 * CS 0x7a9a, que cae en la banda de stubs [0x7a16,0x81c6) y su `ljmp` va a
 * TOWN.OVL 0x0170 = `town_time_tile_transform`, ya portado en
 * `world/townHourTiles.ts` (Game lo expone como `refreshHourTiles`).
 *
 * ⚠ DIVERGENCIA DE MODELO declarada, no corregida aquí: el binario llama a la
 * transformación en CADA iteración cuya hora resultante sea 20 ó 5 (≈7 veces por
 * hora), y su fase 1 es un `xor 0xdd` sobre el buffer vivo, o sea un TOGGLE que
 * depende de la PARIDAD del número de llamadas. El clon modela la capa como
 * OVERLAY recalculado desde el mapa estático, luego repetir la llamada es
 * idempotente. Es la misma divergencia overlay-vs-mutación que ya documenta
 * `townHourTiles.ts`; portar el bucle no la crea ni la agrava.
 */
export function innNightPass(
  state: GameState,
  rand: RandFn,
  onHourTiles: () => void = () => {},
  sky?: SkyRefreshCtx,
): void {
  // 0x01b5-0x01c7 — PRE-bucle: 12 × advance_clock(5). No es equivalente a un solo
  // advance_clock(60): con Quickness cada llamada se parte por separado (0x4f8d).
  for (let i = 0; i < 12; i++) advanceClock(state, 5, rand, sky);

  // 0x01ef → 0x01ca: check de hora ARRIBA, cuerpo abajo.
  for (let step = 0; state.time.hour !== 6 && step < INN_NIGHT_MAX_STEPS; step++) {
    // 0x01d1 — anillo de regeneración; sin portador consume 0 tiradas.
    ringRegenSweep(state.characters, state.partySize, rand, (i) => {
      const ch = state.characters[i]!;
      ch.currentHp = Math.min(ch.currentHp + 1, ch.maxHp);
    });
    advanceClock(state, 9, rand, sky); // 0x01d7
    const h = state.time.hour;
    if (h === 20 || h === 5) onHourTiles(); // 0x01de-0x01ec
  }
}

/**
 * Dejar un compañero hospedado (tecla L, SHOPPES3:0x02AE). NO cobra ahora (se
 * cobra al recoger). Rechaza si el party tiene 1 miembro o si `leaveIdx` es el
 * Avatar (índice 0). Marca `partyStatus = location`, `monthsAtInn = 0`, party−1.
 */
export function innLeave(state: GameState, leaveIdx: number, location: number): TxResult {
  if (state.partySize <= 1) {
    return { ok: false, message: "One must first be left behind!" };
  }
  if (leaveIdx === 0) {
    return { ok: false, message: "Thy friend will not leave thee!" };
  }
  const rec = state.characters[leaveIdx];
  if (!rec || rec.partyStatus !== 0) {
    return { ok: false, message: "That one is not in thy party." };
  }
  // El binario COMPACTA el roster entero y manda al hospedado al slot 15, además
  // de re-indexar el personaje activo (SHOPPES3 0x03dd-0x0472). Marcar sólo el
  // partyStatus dejaba huecos y los bucles de party (que iteran slots 0..N-1)
  // golpeaban a quien se quedaba en la posada — #124.
  rosterLeaveCompact(state, leaveIdx, location);
  return { ok: true, message: "It shall be done." };
}

export interface InnPickupResult extends TxResult {
  /**
   * ¿El huésped estaba envenenado y ha muerto esperando? Selecciona cuál de los DOS
   * fragmentos hermanos de la convergencia 0x088b se emitió (`message`), y el call-site
   * lo necesita porque la atribución `says $.` va en un TERCER mensaje (DS 0x5055) que
   * sólo él puede componer — ahí es donde vive el nombre del posadero. #145.
   */
  died?: boolean;
}

/**
 * Recoger un compañero hospedado (tecla P, SHOPPES3:0x04E6). Cobra
 * `haggle(rate·10, INT)·max(1, meses)`. Rechaza si el party está lleno (6) o si
 * falta oro. Si el compañero estaba envenenado, murió mientras esperaba (`P→D`);
 * restaura MP por clase. Reincorpora al party (partyStatus 0, party+1).
 *
 * ⚠ Las tres guardas de arriba las corre TAMBIÉN el call-site, porque el binario las
 * corre ANTES de abrir el registro de huéspedes (0x04f4 party==6, 0x0506 sin huéspedes,
 * 0x0793 sin oro) y sus mensajes llevan atribución o cierran la sesión. Aquí se quedan
 * como red del núcleo puro, no como el camino vivo — el precedente es `innRoomAvailable`,
 * que ya duplica en la consola el helper 0x002c. #145.
 */
export function innPickup(
  state: GameState,
  buyerIdx: number,
  memberIdx: number,
  location: number,
): InnPickupResult {
  if (state.partySize >= 6) {
    return { ok: false, message: "One must first be left behind!" };
  }
  const inn = innAt(location);
  if (!inn) return { ok: false, message: "There is no inn here." };
  const buyer = state.characters[buyerIdx];
  const rec = state.characters[memberIdx];
  if (!buyer || !rec || rec.partyStatus !== location) {
    return { ok: false, message: "No one here is from thy party!" };
  }
  const months = Math.max(1, rec.monthsAtInn ?? 0);
  const price = innPickupPrice(inn, buyer.intelligence, months);
  if (state.gold < price) {
    // SHOPPES3 0x0799: el original imprime el registro 193 de SHOPPE.DAT
    // (`Unfortunately, thou dost not possess the necessary funds!\nGUARDS!"\nsays
    // $.`) — un registro del pool, con `$`, que el núcleo puro no puede leer.
    // Lo emite `innPickupMember` vía INN_PICKUP_BROKE_INDEX (#145). #36.
    return { ok: false, reason: "gold", message: "" };
  }
  state.gold -= price;
  rec.monthsAtInn = 0;
  // Inversa del leave: desplaza ARRIBA y lo inserta en el slot `partySize`
  // (SHOPPES3 0x07be-0x083a). Pone partyStatus=0 tras moverlo, como 0x084c.
  rosterPickupInsert(state, memberIdx);
  if (rec.status === "P") {
    rec.status = "D";
    rec.currentHp = 0;
    restoreMpByClass(rec);
    // 0x085f `mov ax,0x5005` → DS 0x5005 = DATA.OVL fileoff 0x5015 =
    // b'Thy friend has died, by the way."\n' VERBATIM, con la comilla de CIERRE. La de
    // apertura la trae el mensaje anterior, DS 0x4fe1 (`\n\n"That will be % gold,
    // please."\n\n"`), que acaba en `\n\n"` — posada multi-mensaje (#145/#155). #142.
    return { ok: true, message: 'Thy friend has died, by the way."\n', died: true };
  }
  restoreMpByClass(rec);
  // 0x0888 `mov ax,0x5028` → DS 0x5028 = `I hope thou hast found thy stay enjoyable,"\n`,
  // el hermano de 0x5005 en la MISMA convergencia (0x088b). El port devolvía aquí
  // `"Welcome back!"`, que no está en el binario (#145). La comilla de cierre es la que
  // abre `innPickupPay` (DS 0x4fe1), y la atribución `says $.` va en un tercer mensaje
  // que compone el call-site, donde vive el nombre del posadero.
  return { ok: true, message: 'I hope thou hast found thy stay enjoyable,"\n', died: false };
}

// ---------------------------------------------------------------------------
// Resolución de tienda/mercader por ubicación + tipo
// ---------------------------------------------------------------------------

/** Entrada de ShoppeKeeperMap.json. */
export interface ShoppeKeeperMapEntry {
  Location: string;
  ShoppeKeeperType: string;
}

export interface ShoppeKeeperInfo {
  /** Índice global de la tienda (0-45) en ShoppeKeeperMap / STORE_NAMES. */
  index: number;
  /** Nombre del negocio (STORE_NAMES[index]). */
  shopName: string;
  /** Nombre del mercader (SHOPPE_KEEPER_NAMES[index], con "Simplon" ya eliminado). */
  keeperName: string;
}

/**
 * Resuelve qué tienda/mercader hay en `location` para el tipo `type`, usando
 * ShoppeKeeperMap.json + las listas STORE_NAMES/SHOPPE_KEEPER_NAMES de data.json.
 * Replica el emparejamiento posicional de Redux ShoppeKeeperReferences (incluida
 * la eliminación del nombre basura "Simplon"). Devuelve `null` si no hay tal
 * mercader en esa ubicación.
 */
export function shoppeKeeperAt(
  location: number,
  type: ShopType,
  shoppeKeeperMap: Record<string, ShoppeKeeperMapEntry>,
  storeNames: string[],
  shoppeKeeperNames: string[],
): ShoppeKeeperInfo | null {
  const locName = LOCATION_NAMES[location];
  if (locName === undefined) return null;
  const keeperNames = shoppeKeeperNames.filter((n) => n !== "Simplon");
  for (const [key, entry] of Object.entries(shoppeKeeperMap)) {
    if (entry.Location === locName && entry.ShoppeKeeperType === type) {
      const index = Number(key);
      return {
        index,
        shopName: storeNames[index] ?? "",
        keeperName: keeperNames[index] ?? "",
      };
    }
  }
  return null;
}

// ---------------------------------------------------------------------------
// Transacciones puras sobre GameState (mutan y devuelven TxResult)
// ---------------------------------------------------------------------------

export interface TxResult {
  ok: boolean;
  message: string;
  /**
   * Motivo del fallo, para que el conductor de PRESENTACIÓN elija la rama fiel sin
   * re-implementar los checks (buy_one_item SHOPPES 0x09ac: `cap` = 0x0A5E `cmp
   * qty,0x63` — "Thou canst not carry any more!" + pausa; `gold` = 0x0A7B `cmp
   * g_gold,precio` — insulto rand(0,3) + "yells $." y la SESIÓN entera termina).
   * Sólo lo emiten las transacciones que lo necesitan (compra del herrero).
   */
  reason?: "cap" | "gold";
}

/** Ítems que el herrero NO compra (SHOPPES:0x0E7D): 0x1B y 0x1D. */
const UNSELLABLE_EQUIP = new Set([0x1b, 0x1d]);

/**
 * MUNICIÓN por lote (SHOPPES:0x0AB7-0x0AC1): al COMPRAR Arrows (0x1B) o Quarrels
 * (0x1D) el binario NO suma 1 — pone la cantidad DIRECTAMENTE a 0x63 (99): compras
 * el carcaj/las bolsas LLENAS («A quiver-full of fine Arrows…» / «We can fill thy
 * pouches with crossbow Quarrels…», shoppe.json 33/35). El resto de ítems va por
 * add_byte_capped(+1, cap 99) (kernel 0x9C60 [= CS 0x3ef0 → ULTIMA.EXE:0x3ef0] @0x0AD6).
 */
const AMMO_FILL_EQUIP = new Set([0x1b, 0x1d]);

/**
 * Compra una pieza de equipo: resta oro e incrementa su cantidad (cap 99). Si ya
 * tienes 99 (SHOPPES:0x0A5E `cmp 0x63`) el binario no compra. Munición 0x1B/0x1D:
 * la cantidad queda EN 99 (lote lleno, 0x0AC1), no +1.
 */
export function buyEquipment(state: GameState, equipId: number, price: number): TxResult {
  if ((state.equipmentQuantities[equipId] ?? 0) >= CAP_99) {
    // CS 0x0a65: DS 0x7b76 por el impresor llano + DS 0x7b9a `says $.\n\n` por el
    // expansor + getkey de pausa. Es la copia del EQUIPO: UN salto a cada lado, no
    // los DOS de la del reactivo (DS 0x792c). La atribución la pone el call-site.
    return { ok: false, reason: "cap", message: SHOP_UI.buyFull };
  }
  if (state.gold < price) {
    // CS 0x0a7b `cmp [g_gold],ax / jge 0xaaa`. El texto NO es del núcleo: el
    // original compone DS 0x7ba4 `\n"` + rand(0,3) sobre la tabla DS 0x3cae
    // (0x0a8f call 0x7e02) + DS 0x7ba8 `"\nyells $.\n`, o sea que necesita el
    // stream de rands Y el nombre del tendero, y ninguno de los dos vive aquí.
    // Lo emite el call-site (buyDealYes / pickBuy) por `reason`. #36.
    return { ok: false, reason: "gold", message: "" };
  }
  state.gold -= price;
  state.equipmentQuantities[equipId] = AMMO_FILL_EQUIP.has(equipId)
    ? CAP_99
    : Math.min(CAP_99, (state.equipmentQuantities[equipId] ?? 0) + 1);
  return { ok: true, message: SHOP_UI.buySold }; // DS 0x7bb4 (CS 0x0ad9)
}

/**
 * Vende una pieza de equipo: añade oro (cap 9999, `SHOPPES.OVL 0x0f3d,`
 * add_word_capped) y decrementa su cantidad (`0x0f4b,` sub_byte).
 *
 * ⚠ LOS DOS RECHAZOS DEL BINARIO NO VIVEN AQUÍ, y este docblock lo decía al revés
 * (#57). `sell_one_item` los resuelve ANTES de llegar a la parte que este núcleo
 * modela, y los dos son del call-site (`shop-console.ts` `pickSell`), porque los dos
 * emiten texto con `$` y uno de ellos decide si la sesión sigue:
 *   · munición 0x1B/0x1D (`0x0e7d,`) → DS 0x7d32 y **ret 1**: la sesión CIERRA.
 *   · precio base 0 (`0x0ea2,`)      → DS 0x7d8c y **ret 0**: la sesión SIGUE.
 *
 * 🔴 Y la guarda de stock de abajo NO es `0x0ea2,`: aquélla compara el PRECIO BASE
 * (`cmp word ptr [si + 0x3a82], 0`, tabla de WORDs indexada `item*2`), no la
 * cantidad. El binario no tiene guarda de stock en `sell_one_item` y no la necesita,
 * porque el listador ya filtra por cantidad (`0x0c61,`
 * `cmp byte ptr [si + 0x57c0], 0`). El `<= 0` que sigue es **defensa propia del port
 * para un call-site inalcanzable**, declarada como tal — no un calco.
 */
export function sellEquipment(state: GameState, equipId: number, price: number): TxResult {
  if (UNSELLABLE_EQUIP.has(equipId)) {
    // `message` VACÍO a propósito (patrón de #36). «I cannot buy that!» era una
    // FABRICACIÓN: la frase real de este rechazo es DS 0x7d32
    // (`\n\n"We don't deal in used ammunition!"\ngrowls $.\n`) y la emite el call-site
    // —`shop-console.ts` `pickSell`, gate 0x0e7d— porque lleva `$` y decide el cierre
    // de sesión. Esta rama del núcleo es además INALCANZABLE desde la UI: `pickSell`
    // atrapa 0x1b/0x1d antes de llamar aquí.
    return { ok: false, message: "" };
  }
  if ((state.equipmentQuantities[equipId] ?? 0) <= 0) {
    // Ídem, y aquí el motivo es más fuerte: «Thou hast none to sell!» no sólo estaba
    // sin transcripción — **el binario no tiene esta guarda**, así que no hay frase
    // que transcribir. Es defensa propia del port sobre un call-site que el listador
    // (`0x0c61,`) ya hace imposible. Sin texto, para que no reviva como fiel.
    return { ok: false, message: "" };
  }
  state.equipmentQuantities[equipId] = state.equipmentQuantities[equipId]! - 1;
  state.gold = Math.min(CAP_GOLD, state.gold + price);
  // CS 0x0f2a: DS 0x7d76 por el expansor de `$`, y DESPUÉS el add_word_capped del oro
  // (CS 0x0f3d, tope 0x270f). El eco `Yes\n\n` de la tecla va HORNEADO en el literal
  // de DATA.OVL, no lo antepone el call-site (mismo patrón que guildYes/horseYesExcl).
  return { ok: true, message: SHOP_UI.sellDealYes }; // DS 0x7d76
}

/**
 * Compra `qty` unidades de un reactivo por `price` total. El binario rechaza ANTES
 * de cobrar si ya tienes 99 (SHOPPES:0x0546 `cmp 0x63`) y suma con cap 99 (0x9C60).
 *
 * MENSAJES (cuerpo leído entero, SHOPPES 0x0546-0x064b):
 *   · tope 99 (0x0550): DS 0x792c + getkey de pausa. NO es «Thou canst carry no
 *     more!» — esa oración no existe en DATA.OVL; el binario dice «canst NOT carry
 *     ANY more».
 *   · pago (0x061c): sub gold + merma 0x19a + add_byte_capped(…,0x63) y DS 0x7988
 *     por el expansor de `$` (0x26) + DS 0x79a2 por el impresor llano (0x75c0).
 * El `$` de reagentThanks es el NOMBRE DEL TENDERO y lo expande el call-site
 * (`expandShoppeTemplate`), igual que la vía con pool de shop-console.
 */
export function buyReagent(
  state: GameState,
  reagentIdx: number,
  qty: number,
  price: number,
): TxResult {
  if (qty <= 0) return { ok: false, message: "Buy how many?" };
  if ((state.reagentQuantities[reagentIdx] ?? 0) >= CAP_99) {
    return { ok: false, message: SHOP_UI.reagentFull }; // DS 0x792c (0x0550)
  }
  // CS 0x060a: sin oro el original imprime el registro 147 de SHOPPE.DAT
  // (`\n"Thou profaneth my shoppe with thy empty purse! OUT!"\nsnarls $.\n`,
  // 0x0610 `mov ax,0xb6e2` sobre el chunk precargado en 0xb21e desde fileoff
  // 0x1a67 ⇒ rebase −0x97b7 ⇒ fileoff 0x1f2b) y ret −1 (0x0617): registro del
  // pool + `$`, nada de esto lo tiene el núcleo. Lo emite el call-site. #36.
  if (state.gold < price) return { ok: false, reason: "gold", message: "" };
  state.gold -= price;
  state.reagentQuantities[reagentIdx] = Math.min(
    CAP_99,
    (state.reagentQuantities[reagentIdx] ?? 0) + qty,
  );
  return { ok: true, message: SHOP_UI.reagentThanks }; // DS 0x7988 (0x063d)
}

// PODADA (tarjeta #40): `buyProvisions` / `ProvisionKind`. NO hay «mercader de
// provisiones» en Ultima V, y lo prueba el CENSO de las dos globales que la función
// tocaba, no sólo la lectura de dos rutinas: los únicos escritores en contexto de
// tienda de `g_food` (DS 0x57a8) y de las antorchas (DS 0x57ae) son
// `SHOPPES2.OVL 0x0453,` (comida: add_capped(+25/unidad, tope 9999) vía 0x5d34) y
// `SHOPPES.OVL 0x03c2,` (antorchas: lote FIJO +5, cap 99). Los demás escritores no
// son tiendas: `TALK.OVL 0x0682,` (dar/recibir, ±1), `SJOG.OVL 0x152b,` (loot) y
// `BLCKTHRN.OVL 0x0c47,` (la cárcel, `mov word ptr [g_food], 0x3f`).
//
// ⇒ Las TRES vías reales suman LOTES FIJOS. Ninguna rutina del binario suma una
// cantidad LIBRE por un precio LIBRE, que es exactamente lo que la firma
// `(state, kind, qty, price)` calcaba: no calcaba nada. Sus dos mercancías están
// portadas y cableadas por su vía propia — `buyRations` (Barkeeper) y
// `buyGuildItem` item=2 (GuildMaster). Era el residuo del placeholder sintético
// «Food(1)+Torch» de la UI pre-mercaderes (`re/notes/content-audit.md`:85) y se
// quedó sin un solo consumidor en `game/src`.


// ---------------------------------------------------------------------------
// GuildMaster (SHOPPES.OVL:0x04A2) — llaves / gemas / antorchas
// ---------------------------------------------------------------------------

export type GuildItem = 0 | 1 | 2; // 0=keys, 1=gems, 2=torches

/** Precio de un ítem del gremio regateado por INT (SHOPPES:0x02BA). */
export function guildPrice(guildTownIdx: number, item: GuildItem, intelligence: number): number {
  const base = GUILD_PRICES[guildTownIdx]?.[item] ?? 0;
  return shopBuyPrice(base, intelligence);
}

/**
 * Compra un lote del gremio: paga `guildPrice` y concede el LOTE FIJO
 * (keys +3 / gems +4 / torches +5), cada uno con cap 99 (add_byte_capped 0x9C60).
 *
 * MENSAJE (cuerpo entero de `buy_one_guild`, SHOPPES 0x02ba-0x03d9): el pago
 * (0x0374) hace sub gold + merma 0x19a + repaint 0x8670, entra al switch del ítem
 * (0x0394/0x03b8/0x03c2 → add_byte_capped sobre DS 0x57ac/0x57ad/0x57ae con 3/4/5
 * y tope 0x63) y emite DS 0x78a0 por el expansor de `$` (0x26), luego `m'lady`
 * (DS 0x78c0) o `m'lord` (DS 0x78c8) por el género del negociador y `?\n\n`
 * (DS 0x78d0). Es una TERCERA frase: ni la del herbolario ni la del vino.
 *
 * ★ SIN GATE DE TOPE (derivado en #147, retirado en #153). El binario no comprueba
 * la cantidad en ningún punto de la cadena: en `buy_one_guild` (0x02ba-0x03d9) y en
 * sus dos callers (0x03f6 lista, 0x04a2 entrada) el único rechazo previo al cobro es
 * el de ORO (0x0361 `cmp [g_gold]`). Con 99 en mano el original COBRA y el
 * add_byte_capped satura, de modo que el jugador paga y no recibe nada. Aquí hubo un
 * `if (>= CAP_99) return {ok:false}` FABRICADO, con un mensaje que tampoco existe en
 * DATA.OVL; era mecánica ECONÓMICA inventada (protegía al jugador de un cobro que el
 * original sí hace).
 * Control de la ausencia, sobre el overlay ENTERO y no por ventana: en todo
 * SHOPPES.OVL hay exactamente DOS `cmp` con 0x63 — 0x0549 (reactivos, DS 0x5850) y
 * 0x0a5e (equipo, DS 0x57c0) — y ninguno cae en la cadena del gremio ni toca sus
 * globales DS 0x57ac/0x57ad/0x57ae.
 */
export function buyGuildItem(
  state: GameState,
  guildTownIdx: number,
  item: GuildItem,
  intelligence: number,
): TxResult {
  const price = guildPrice(guildTownIdx, item, intelligence);
  const field = (["keys", "gems", "torches"] as const)[item];
  // CS 0x0361: sin oro el original imprime el registro 163 de SHOPPE.DAT
  // (`\n"Death comes in many ways to those who cheat the Guild!"\n warns $.\n`,
  // 0x0367 `mov ax,0x21e6` + print_shoppe 0x17a con el fileoff CRUDO) y ret −1
  // (0x036e). Registro del pool + `$`: lo emite el call-site. #36.
  if (state.gold < price) return { ok: false, reason: "gold", message: "" };
  state.gold -= price;
  state[field] = Math.min(CAP_99, (state[field] ?? 0) + GUILD_GRANT[item]);
  return { ok: true, message: SHOP_UI.guildSold }; // DS 0x78a0 (0x03a6)
}

// ---------------------------------------------------------------------------
// Barkeeper / taberna (SHOPPES2.OVL:0x066C)
// ---------------------------------------------------------------------------

/** Miembros vivos del party (status != 'D'); la ronda excluye a los muertos. */
function livingPartyCount(state: GameState): number {
  return partyIndices(state).filter((i) => state.characters[i]!.status !== "D").length;
}

/** Coste de una ronda de comida: `precio_por_cabeza · vivos` (SHOPPES2:0x01D2). */
export function tavernRoundPrice(barTownIdx: number, living: number): number {
  return (TAVERN_ROUND_PRICE[barTownIdx] ?? 0) * living;
}

/** Tile de MESA que admite plato — 0x0170 / 0x01a4 `cmp byte ptr [bx], 0x95`. */
const TAVERN_TABLE_TILE = 0x95;
/** Plato servido en la mesa del NORTE — 0x0185 `mov byte ptr [bx], 0x9b`. */
const TAVERN_PLATE_NORTH = 0x9b;
/** Plato servido en la mesa del SUR — 0x01b9 `mov byte ptr [bx], 0x9a`. */
const TAVERN_PLATE_SOUTH = 0x9a;

/**
 * Palabra del nº de vivos del anuncio de precio — `print_alive_count_word`
 * (SHOPPES2 0x006a). La transcripción del binario es `TAVERN_ALIVE_WORD` (2..6).
 *
 * 🔧 ARREGLO DECLARADO (ficha #17, directriz vigente: los bugs CLAROS del original se
 * ARREGLAN y se REGISTRAN). El binario **no tiene entrada para el 1**: cae al `ret` de
 * 0x00aa sin imprimir, y la frase sale con un HUECO — «gold for the  of ye,» con doble
 * espacio — siempre que el Avatar viaje solo. Aquí devolvemos «one». Es la ÚNICA
 * divergencia deliberada de esta línea; todo lo demás es transcripción.
 *
 * ⚠ El caso `0` (party ENTERA muerta, la rama `g_alive_a == 0` de 0x014a) se deja como
 * el binario —cadena vacía— A PROPÓSITO: no es el bug que la directriz adjudica, no hay
 * palabra que el original ponga ahí, e inventarla sería fabricar. Queda declarado como
 * caso degenerado sin adjudicar, no como descuido. `>6` es inalcanzable (el roster son
 * seis ranuras).
 */
export function tavernAliveWord(living: number): string {
  if (living === 1) return "one"; // 🔧 el hueco de EA, arreglado y registrado (#17)
  return TAVERN_ALIVE_WORD[living] ?? ""; // 2..6 del binario; 0 y >6 → nada, como 0x00aa
}

export interface TavernRoundResult extends TxResult {
  /**
   * PIEZAS del anuncio de precio (SHOPPES2 0x00dc-0x0111). Viaja APARTE de `message`
   * porque el binario lo imprime ANTES del gate de oro (0x0114): sale en las DOS
   * ramas, la que sirve y la que te echa. Se entregan PIEZAS y no una frase montada
   * porque el conductor traduce con `t()` **pieza a pieza** — el compuesto no es key
   * del corpus, así que montarlo aquí lo dejaría en inglés bajo `es` (mismo idioma que
   * el afford-only de la taberna). Antes de la ficha #17 el clon no emitía esta línea
   * EN ABSOLUTO: no es que copiara el hueco de EA, es que le faltaba la frase entera.
   */
  priceLine: {
    /** Precio TOTAL (`[g_unk_b118]`), impreso con `print_number` en 0x00ea. */
    cost: number;
    /** Palabra del nº de vivos (0x0100 → 0x006a) — `""` donde el binario no imprime. */
    aliveWord: string;
    /** `true` ⇒ «sir»; `false` ⇒ «milady». Ranura 0 (Avatar), `== 0x0b` en 0x00c3. */
    sir: boolean;
  };
  /**
   * Celda y tile del PLATO a escribir, o `undefined` si no hay mesa adyacente.
   * `dy` es el desplazamiento respecto del party (−1 norte, +1 sur). Lo escribe el
   * conductor con `setVolatileTerrain`: el binario va por PUNTERO (`get_tile_ptr`,
   * CS 0x4402), el mismo canal volátil que la puerta destrabada y la lava (#119/#121).
   */
  plate?: { dy: number; tile: number };
  /**
   * ¿Esta ronda incrementa `g_cups_served` (0xBD20)? Es el destino del
   * `je` de `g_alive_a == 0` (0x014f → 0x01c4), y se llega por DOS caminos:
   *  · COMIDA con la party ENTERA MUERTA (raro), y
   *  · CUALQUIER ronda de la casa (`houseRound`), que pone `g_alive_a` a 0 a
   *    propósito (0x0372) — o sea **con la party viva, por construcción**.
   *
   * 🔴 Esto CORRIGE lo que decía aquí («sólo con la party entera muerta; con
   * cualquier miembro vivo es false»): era una lectura impecable del llamador de
   * la comida y una afirmación FALSA sobre el binario, porque el otro llamador
   * —la bebida— entra en la misma rama en toda ronda de ale/ron/stout. El error
   * no estaba en la conducta sino en la premisa escrita, que es justo el material
   * que reutiliza el siguiente lector (asm-shoppes §42.1).
   */
  countsAsService: boolean;
}

/**
 * Ronda de comida de la taberna — SHOPPES2 0x0140-0x01C8. #192 (D4)
 *
 * ★★ EL GATE ES `g_alive_a`, Y `g_alive_a` ES EL NÚMERO DE VIVOS. Lo fija
 * `SHOPPES2:0x0000` (`ret 2`, arg = precio por cabeza), que recorre el roster
 * (`0x55b3`, stride 0x20) y por cada miembro con estado ≠ `'D'` (0x0030
 * `cmp byte ptr [si],0x44` / `je 0x3e`) incrementa `g_alive_a`/`g_alive_b` y suma el
 * precio a `g_shop_accum`. El ledger ya los tenía nombrados así en `globals.json`.
 *
 * Con eso, `014a: cmp word ptr [g_alive_a], 0` / `014f: je 0x1c4` reparte:
 *  · **≥1 vivo** (todo juego normal) → COMIDA + PLATO, y **NO** toca el contador;
 *  · **party entera muerta** → `01c4: inc word ptr [g_cups_served]`, y nada más.
 *
 * Eso CONFIRMA la conclusión de la seña de #174 («en el original nunca incrementa»)
 * y REFUTA su mecanismo («un contador gateado al revés»): no hay contador mal puesto,
 * hay un selector por número de vivos. El port hacía las dos cosas al revés — no daba
 * la comida y sí incrementaba el contador.
 *
 * LA COMIDA: `0151-015d push 0x57a8(&g_food) / push [g_alive_a] / push 0x270f /
 * call 0x5d34` → CS 0x3F14 `add_word_capped` ⇒ **+1 por miembro VIVO, tope 9999**
 * (resuelto con `overlay_near_call_base('SHOPPES2.OVL') = 0xE1E0`; controles positivos
 * en el test).
 *
 * EL PLATO, con ORDEN: primero la mesa del NORTE (0x0160-0x0173, `y-1`) y **sólo si
 * ahí no hay mesa** la del SUR (0x0194-0x01a7, `y+1`); tiles distintos (0x9b / 0x9a).
 * Se devuelve para que lo escriba el conductor — este módulo no toca el mapa.
 *
 * El COSTE ya era fiel (precio × vivos) y el gate de oro (0x0114-0x011b) va ANTES de
 * todo, así que sin oro no se cobra ni se sirve nada.
 */
export function buyTavernRound(
  state: GameState,
  barTownIdx: number,
  adjacent?: { north: number; south: number },
  houseRound = false,
): TavernRoundResult {
  const living = livingPartyCount(state);
  // PRECIO POR CABEZA — la COMIDA usa la tabla por ciudad (0x01d2); la RONDA DE LA
  // CASA de la bebida factura a 1 pieza (0x026f `mov ax, 1` justo antes del
  // `call 0` que acumula), sea cual sea el pueblo.
  const cost = houseRound ? living : tavernRoundPrice(barTownIdx, living);
  // ★ LOS DOS CONTADORES DE VIVOS, que divergen en VIDA y no en valor (§36.1/§41).
  // `g_alive_b` (0xBD1C) sobrevive y es el que lee la palabra de la frase; `g_alive_a`
  // (0xBD1A) es el de trabajo y la BEBIDA lo pone a 0 A PROPÓSITO antes de llamar a la
  // cuenta (0x0372 `mov word ptr [0xbd1a], 0` / `call 0xdc`). Ese 0 es una SEÑAL —«esto
  // no es una comida»—, no un estado de mundo: por eso una ronda de ale con la party
  // viva sí dice «the four of ye» (lee g_alive_b) y a la vez no da comida ni plato.
  const aliveA = houseRound ? 0 : living;
  // ANUNCIO DE PRECIO (0x00dc-0x0111), ANTES del gate de oro de 0x0114 ⇒ se emite
  // también cuando no puedes pagar. El género sale de la ranura 0 (`bx = 0<<5` en
  // 0x00c3), o sea SIEMPRE el Avatar, y la polaridad es `== 0x0b → sir` (CUALQUIER
  // otro byte → milady, no sólo el 0x0c femenino).
  const priceLine = {
    cost, // 0x00ea print_number(precio TOTAL, width 1, pad ' ')
    aliveWord: tavernAliveWord(living), // 0x0100 call 0x006a (+ el arreglo del 1, #17)
    sir: state.characters[0]?.gender === 0x0b, // 0x00c3 `cmp byte [·+0x55b1],0xb`
  };
  if (state.gold < cost) {
    return { ok: false, message: "CAN'T PAY? Beat it!", priceLine, countsAsService: false };
  }
  state.gold -= cost; // 0x0143 `sub word ptr [g_gold], ax`

  // 0x014a `cmp word ptr [g_alive_a], 0` / 0x014f `je 0x1c4`. Se llega aquí con 0 por
  // DOS caminos: la party entera muerta (comida) y TODA ronda de la casa (bebida).
  if (aliveA === 0) {
    return { ok: true, message: SHOP_UI.roundEnjoy, priceLine, countsAsService: true };
  }

  state.food = Math.min(CAP_GOLD, state.food + aliveA); // 0x015d add_word_capped

  // 0x0160-0x01a7: NORTE primero, SUR sólo si el norte no es mesa.
  let plate: { dy: number; tile: number } | undefined;
  if (adjacent?.north === TAVERN_TABLE_TILE) {
    plate = { dy: -1, tile: TAVERN_PLATE_NORTH };
  } else if (adjacent?.south === TAVERN_TABLE_TILE) {
    plate = { dy: 1, tile: TAVERN_PLATE_SOUTH };
  }

  // CS 0x01c8 empuja DS 0x9b16 al impresor 0x3670. Entrada DISTINTA de la del vino
  // (DS 0x9c40): ésta cierra con `\n\n`.
  return { ok: true, message: SHOP_UI.roundEnjoy, priceLine, plate, countsAsService: false }; // DS 0x9b16
}

/** Precio de una copa de la carta de vinos (SHOPPES2:0x01F4). */
export function winePrice(wineIdx: number): number {
  return WINE_PRICES[wineIdx] ?? 0;
}

/**
 * El pago de la copa no tiene campos propios: el gate de borrachera (y con él los
 * turnos de 25) vive en el call-site desde #325, porque en el binario está en
 * `0x020a`, ANTES del discriminante `'W'`.
 */
export type WineResult = TxResult;

/**
 * Compra una copa (1 por compra) — SÓLO el tramo de pago, `SHOPPES2 0x032a-0x036f`.
 *
 * ★ EL GATE DE BORRACHERA YA NO VIVE AQUÍ (#325). Estaba en esta función, o sea al
 * COMPRAR, y en el binario está en `0x020a`: al ELEGIR la bebida, ANTES del
 * discriminante `'W'` de `0x027c`. La diferencia no era decorativa —
 *
 *  · el binario aplica el castigo aunque luego te RETIRES de la carta con Espacio,
 *    y el clon no lo aplicaba (esa rama ni siquiera existía hasta #325);
 *  · el aviso son CINCO emisiones y un Y/N que aquí no se podían emitir, porque
 *    esto es el núcleo y no tiene canal de teclado. Se emitían CERO: el clon
 *    castigaba en silencio;
 *  · y estaba DUPLICADO — `tavernHouseRound` llevaba su propia copia inline, porque
 *    en el binario el gate es UNO y cubre las dos ramas de la bebida.
 *
 * Hoy el gate es `ShopConsole.drinkDrunkGate` (un solo sitio, con sus cinco cadenas
 * y el Y/N interactivo), y el contador `served` sigue siendo el nº de COPAS previas
 * (#192: el `inc` de `0x01C4` es el destino del `je` de `g_alive_a == 0`, la rama de
 * party entera muerta, no la de comida).
 */
export function buyWine(state: GameState, wineIdx: number): WineResult {
  const price = winePrice(wineIdx);
  // 0x032a `cmp word ptr [bx*2 + DS 0x4c48], ax` / `jle 0x352` — compra si precio ≤ oro.
  // El texto del rechazo (DS 0x9c20, ret 1 = ECHADO) lo pone el call-site, que es quien
  // tiene el nombre del tendero para la atribución `yells `.
  if (state.gold < price) return { ok: false, message: "", reason: "gold" };
  state.gold -= price;
  // Servida la copa (SHOPPES2 0x0368): DS 0x9c40 al impresor 0x3670, justo tras
  // `sub [g_gold]` (0x035d), la merma (0x9dfa) y el `inc [g_cups_served]` (0x0364).
  return { ok: true, message: SHOP_UI.wineEnjoy };
}

/** Precio por unidad de ración, regateado por INT (SHOPPES2:0x0380). */
export function rationPrice(barTownIdx: number, intelligence: number): number {
  return shopBuyPrice(RATION_BASE[barTownIdx] ?? 0, intelligence);
}

/**
 * Compra hasta `qty` unidades de raciones: mientras haya oro (≥ precio unitario)
 * resta el precio y añade +25 de comida por unidad (cap 9999). Devuelve cuántas
 * unidades se compraron realmente. NO modela el bonus RNG de comida baja
 * (rand(0,1)+1 cuando g_food<3), que consume RNG (ver harness de paridad).
 */
export function buyRations(
  state: GameState,
  barTownIdx: number,
  intelligence: number,
  qty: number,
): TxResult & { bought: number; full: boolean } {
  const price = rationPrice(barTownIdx, intelligence);
  let bought = 0;
  let full = false;
  while (bought < qty && state.gold >= price) {
    state.gold -= price;
    state.food = Math.min(CAP_GOLD, state.food + RATION_FOOD_PER_UNIT);
    bought += 1;
    // 0x462: si la comida TOCA el tope exacto (9999), el bucle PARA (esa unidad
    // ya está pagada) — no se sigue cobrando con la comida capada.
    if (state.food === CAP_GOLD) {
      full = true;
      break;
    }
  }
  if (bought === 0) return { ok: false, message: "Hrumph.", bought, full };
  return { ok: true, message: "Anything else?", bought, full };
}

/**
 * Índice de la clave de rumor que casa con lo tecleado, o −1 (SHOPPES2 0x0568-0x05a4,
 * cuerpo leído). El binario recorre las 26 entradas (`cmp di,0x1a` @0x059f) llamando a
 * `stristr(buffer 0xBCF8, clave)` y se queda con la PRIMERA que casa; el bucle NO busca
 * la mejor, así que el orden de la tabla decide los empates.
 *
 * 🔴 La clave es la AGUJA y lo tecleado el PAJAR — al revés que el `startsWith` que el
 * clon tenía. Y la frontera de palabra se comprueba sobre el carácter **ANTERIOR** al
 * match, no el siguiente: `cmp byte ptr [si - 0x4309], 0x20` (@0x057e) es
 * `byte[0xBCF7 + si]` = `buffer[si-1]`, con `si` = desplazamiento del match dentro del
 * búfer (`si == 0` salta directo a la rama de acierto @0x0585, `si < 0` = no casa
 * @0x057c). ⇒ acierta si la clave abre lo tecleado o va tras un espacio. Misma regla que
 * el keyword de conversación (#28); `frontier.json` la describe como «el carácter
 * SIGUIENTE sea espacio», y eso es una errata: con las claves de 4 letras ese predicado
 * haría fallar «honesty», que es el caso central.
 *
 * Cota de entrada: 15 caracteres (`input_string` sobre `DS 0xBCF8` @0x0543).
 */
export function rumorKeywordIndex(typed: string): number {
  const hay = typed.trim().slice(0, RUMOR_INPUT_MAX).toLowerCase();
  if (!hay) return -1;
  for (let i = 0; i < RUMOR_KEYWORDS.length; i++) {
    const at = hay.indexOf(RUMOR_KEYWORDS[i]!);
    if (at < 0) continue;
    if (at === 0 || hay[at - 1] === " ") return i;
  }
  return -1;
}

/** Tope del `input_string` de la clave — 15 caracteres (SHOPPES2 0x053f). */
export const RUMOR_INPUT_MAX = 15;

/**
 * Precio de un rumor por keyword (`[idx*2 + DS 0x4D10]` → `g_shop_accum`, 0x05ab),
 * o `null` si no casa ninguna. 1 rumor por visita en el binario.
 */
export function rumorPrice(keyword: string): number | null {
  const idx = rumorKeywordIndex(keyword);
  return idx < 0 ? null : RUMOR_PRICES[idx]!;
}

/** Sujeto y lugar del chisme de una clave ya casada (0x061d/0x0633). */
export function rumorGossip(idx: number): { subject: string; place: string } | null {
  const subject = RUMOR_SUBJECTS[idx];
  const slot = RUMOR_GOSSIP_MAP[idx];
  if (subject === undefined || slot === undefined) return null;
  const place = RUMOR_PLACES[slot];
  return place === undefined ? null : { subject, place };
}

/**
 * Cobro del rumor tras el «Fair 'nuff?» → Y (0x060e-0x0611): descuenta el oro y
 * devuelve el chisme a publicar. Sin oro (`g_shop_accum > g_gold`, 0x05f0) NO cobra
 * y el llamador emite «"Sorry, » + honorífico + el registro 91.
 *
 * 🔴 Ya NO cobra al acertar la clave: en el binario el precio se ANUNCIA y el pago
 * espera a la confirmación, y el `N` sale sin pagar (y aun así cuenta como servicio).
 */
export function payRumor(state: GameState, idx: number): TxResult & { subject?: string; place?: string } {
  const price = RUMOR_PRICES[idx];
  const gossip = rumorGossip(idx);
  if (price === undefined || !gossip) return { ok: false, message: "" };
  if (state.gold < price) return { ok: false, message: "" };
  state.gold -= price;
  return { ok: true, message: "", subject: gossip.subject, place: gossip.place };
}

// ---------------------------------------------------------------------------
// Shipwright (SHOPPES2.OVL:0x08A8) — F=Frigate, S=Skiff
// ---------------------------------------------------------------------------

export type ShipKind = "frigate" | "skiff";

/** Precio de una nave regateado por INT del comprador (SHOPPES2:0x0940/0x09D4). */
export function shipwrightPrice(
  shipTownIdx: number,
  kind: ShipKind,
  intelligence: number,
): number {
  const base = (kind === "frigate" ? FRIGATE_PRICES : SKIFF_PRICES)[shipTownIdx] ?? 0;
  return shopBuyPrice(base, intelligence);
}

export interface ShipBuyResult extends TxResult {
  /** Coords del muelle donde aparece la nave (SHIP_DOCK_X/Y), sólo si ok. */
  dockX?: number;
  dockY?: number;
}

/**
 * Compra una nave: paga `shipwrightPrice` y devuelve las coords de muelle donde
 * el binario coloca la nave (SHOPPES2:0x080E). No modela el path de reemplazo a
 * 10000g (flag 0x6605) — ver SHIP_REPLACEMENT_PRICE y re/notes/shops.md.
 */
export function buyShip(
  state: GameState,
  shipTownIdx: number,
  kind: ShipKind,
  intelligence: number,
): ShipBuyResult {
  const price = shipwrightPrice(shipTownIdx, kind, intelligence);
  // Helper de cobro SHOPPES2 0x7e2: sin oro el original imprime el registro 122
  // (`What? Cheat me, will ye? OUT!`, ptr 0x198e) + DS 0x9fc2 `yells $.` y marca
  // [0xbd22] ⇒ salida SIN despedida. `shipTakeYes` ya lo emite entero por
  // SHIP_CHEAT_INDEX, así que este mensaje no lo leía NADIE. #36.
  if (state.gold < price) return { ok: false, reason: "gold", message: "" };
  state.gold -= price;
  return {
    ok: true,
    message: "She awaits thee at the dock!",
    dockX: SHIP_DOCK_X[shipTownIdx],
    dockY: SHIP_DOCK_Y[shipTownIdx],
  };
}

/**
 * Aplica un servicio del healer al personaje `charIdx` cobrando `price`.
 * - `heal`: sólo si HP < HP máximo → restaura HP al máximo.
 * - `cure`: sólo si status 'P' (envenenado) → 'G'.
 * - `resurrect`: sólo si status 'D' (muerto) → la rutina COMPARTIDA `resurrect_apply`
 *   (SHOPPES 0x16f5 → CAST2.OVL 0x05e0, modo 0xff: status 'G', MP por clase, recorte de
 *   experiencia con karma < 98, nivel y HP máximo recalculados) y DESPUÉS HP := el máximo
 *   NUEVO (0x16f8-0x1703). A4-PARITY2 D-83: antes ponía 'G' con 1 HP sin la rutina.
 * Falla (sin cobrar) si el personaje no necesita el remedio o falta oro.
 * Portado de Healer.DoesPlayerNeedRemedy + PlayerCharacterRecord.Cure/HealAll.
 */
export function healerHeal(
  state: GameState,
  charIdx: number,
  service: HealerService,
  price: number,
): TxResult {
  const rec = state.characters[charIdx];
  if (!rec) return { ok: false, message: "No such person." };

  const needs =
    service === "heal"
      ? rec.currentHp < rec.maxHp
      : service === "cure"
        ? rec.status === "P"
        : rec.status === "D";
  if (!needs) {
    return { ok: false, message: "Thou hast no need of this art!" };
  }
  if (state.gold < price) {
    // CS 0x14bc. Ojo al orden del original: ANTES del texto corre la CARIDAD
    // (0x14c2 `cmp ax,0x64 / jg` + 0x14c7 `cmp [g_location],7 / je`) — con precio
    // ≤ 100 en Skara Brae el servicio se hace GRATIS. Sólo si no aplica imprime el
    // registro 173 (0x14ce `mov ax,0x23ab` + print_shoppe) y devuelve declinado
    // SIN echar (a diferencia de armería/reactivos/gremio). Las dos cosas —el
    // gate de ciudad y el registro del pool— son del call-site. #36.
    return { ok: false, reason: "gold", message: "" };
  }

  state.gold -= price;
  if (service === "heal") {
    rec.currentHp = rec.maxHp;
  } else if (service === "cure") {
    rec.status = "G";
  } else {
    // SHOPPES 0x16f5 `call 0xdc66` (= CS 0x7ef6 -> CAST2.OVL 0x05e0), mode 0xff: la misma
    // rutina que In Mani Corp (que se queda en HP 1). Sin RNG. Orden del binario: 'D' (0x16b5),
    // pago, deriva de la Falsedad, rutina, HP := max recalculado.
    applyResurrect(rec, state.karma);
    rec.currentHp = rec.maxHp; // 0x16f8-0x1703: `word [rec+0x10] := word [rec+0x12]`
  }
  return { ok: true, message: "It is done." };
}

// ---------------------------------------------------------------------------
// Merma de la Falsedad tras un pago — SHOPPES.OVL:0x019a
// ---------------------------------------------------------------------------

/**
 * `post_purchase_gold_rand` (SHOPPES.OVL:0x019a): el binario lo llama tras CADA
 * pago del grupo SHOPPES.OVL (cada `sub [g_gold],ax` seguido de `call 0x19a`; 5
 * sitios: gremio 0x037b, reactivos 0x0623, transporte 0x0951, herrero-compra
 * 0x0ab1, curandero 0x14ed). NO en la venta (entrada de oro) ni en SHOPPES2
 * (taberna/astillero) / SHOPPES3 (posada), cuyos pagos van seguidos de kernel
 * 0x9dfa, no de la merma.
 *
 * El cuerpo (0x019a-0x01b4) es GATE PRIMERO: `cmp [g_shadowlord_here_idx],0 ; jne
 * ret` (0x019a-0x019f) — sólo si el Shadowlord presente en la ciudad es la Falsedad
 * (índice 0) se ROLL `rand(1,64)` (0x01a1-0x01ad, push 1/push 0x40) y se aplica
 * `gold -= roll` con suelo 0 (0x01b1 = kernel 0x3f54 sub_word_floored). ⇒ el rand
 * se consume del stream vivo SÓLO con la Falsedad presente (0 rands en otro caso).
 * NO imprime mensaje: el binario sisa en silencio.
 *
 * `rand` es la fuente del stream vivo (el caller de tienda la inyecta). Devuelve el
 * oro efectivamente mermado (0 sin Falsedad, sin haber consumido rand).
 */
export function postPurchaseDrain(state: GameState, rand: RandFn): number {
  // 0x019f jne ret: gate ANTES del rand. Lee el FLAG FÍSICO `g_unk_5958` (lo que compara
  // SHOPPES 0x019a), no la tabla lógica — ver `shadowlordHereIndex`.
  if (shadowlordHereIndex(state) !== 0) return 0;
  return postPurchaseGoldDrain(state, rand(1, 64)); // rand(1,64) → gold -= roll (suelo 0)
}
