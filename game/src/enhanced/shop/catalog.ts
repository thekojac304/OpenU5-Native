/**
 * CATÁLOGO DEL PANEL DE TIENDA — la DERIVACIÓN pura, y nada más.
 *
 * 🔴 ESTE MÓDULO NO TIENE UNA SOLA TABLA DE MERCANCÍA, NI DE SERVICIOS, NI DE PRECIOS.
 * Su única entrada es `ShopConsole.snapshot()` — la instantánea que el conductor de la
 * conversación ya publicaba para el arnés del Grand Tour (`__u5test.shopConsole`) — y su
 * salida es esa MISMA lista de opciones, clasificada por FORMA para que la vista sepa si
 * pintar dos botones, una rejilla o una lista con scroll. Si una fase no expone una
 * opción, aquí no aparece: no hay de dónde sacarla.
 *
 * POR QUÉ HACE FALTA CLASIFICAR (y por qué la clasificación NO es una tabla de fases).
 * `ShopConsoleOption` es `{key,label,idx?}` para las treinta y pico fases del conductor,
 * y las treinta no se tocan igual: «Yes/No» son dos botones grandes, ocho reactivos son
 * una lista, y una pausa «pulsa una tecla» no tiene NINGUNA opción y aun así necesita un
 * botón. La forma se deriva de la propia instantánea con predicados ESTRUCTURALES:
 *
 *   · `continue` — el sufijo `-pause` del nombre de fase, que el conductor ya usa para
 *     las tres pausas del binario que DESCARTAN la tecla (`blacksmith-pause` getkey
 *     0x83dc · `buy-full-pause` 0x0a73 · `reagent-full-pause` 0x557). Las tres traen la
 *     lista de opciones VACÍA, que es lo que hace falta comprobar además del sufijo.
 *   · `yesno`   — exactamente dos opciones y sus teclas son `y`/`n`. Los dieciséis
 *     portones Y/N del conductor caen aquí sin nombrarse uno a uno.
 *   · `members` — alguna opción trae `idx`, el índice de personaje que el conductor pone
 *     SÓLO en sus listas de miembros (`renderHealMemberList`, la lista degradada del
 *     `Pick up` de la posada). La etiqueta YA es el nombre del compañero.
 *   · `list`    — el sufijo `-list` (compra del herrero, reactivos, gremio, carta de
 *     vinos): filas de mercancía, con el precio que el conductor ya compuso.
 *   · `menu`    — todo lo demás: los menús cortos por letra (Buy/Sell, Cure/Heal/
 *     Resurrect, Rest/Leave/Pick up, Frigate/Skiff, la carta de la taberna).
 *
 * Ninguno de los cinco nombra un ítem, un servicio ni un precio: nombran la FORMA de lo
 * que el conductor ya decidió exponer. Añadir una fase al conductor no obliga a tocar
 * este fichero.
 *
 * LA TECLA DE CANCELAR, que es el único sitio donde hay conocimiento del despachador.
 * `ShopConsole.key()` corre un gate GLOBAL de salida (`raw === " " || "Escape"` →
 * `leave()`) DESPUÉS de las fases que el binario re-lee. O sea: en un menú o una lista,
 * Escape despide al mercader — la misma salida que la tecla física. Dos excepciones, las
 * dos del conductor y las dos citadas:
 *   · las fases Y/N y las pausas RE-LEEN Escape (su getkey lo descarta), así que no se
 *     pinta botón de salida: sería un botón muerto. Las respuestas ya están en pantalla.
 *   · `healer-need` re-lee Escape (0x1568) pero SÍ sale con Espacio (`Nothing`, DS
 *     0x81c0), así que su botón emite Espacio. Es la tecla que el jugador pulsaría.
 */
import type { ShopConsoleOption, ShopConsoleSnapshot } from "../../ui/shop-console.js";

/** Forma de la fase: lo que la vista necesita saber para elegir disposición. */
export type ShopSheetKind = "continue" | "yesno" | "members" | "list" | "menu";

/** Una fila del panel. `key` es LA TECLA que el jugador teclearía; nada más se inventa. */
export interface ShopRow {
  /** Tecla cruda que sintetiza esta fila. Sale tal cual de `ShopConsoleOption.key`. */
  key: string;
  /** Rótulo, tal cual lo compuso el conductor (ya pasado por `t()` donde tocaba). */
  label: string;
  /** Índice de personaje, SÓLO en las listas de miembros. Copiado de `option.idx`. */
  member?: number;
  /** ¿Es un NOMBRE PROPIO o mercancía ya traducida? Entonces la vista NO lo re-traduce. */
  crudo: boolean;
}

/** La hoja a pintar. `null` = esta fase no tiene nada que ofrecer con el dedo. */
export interface ShopSheet {
  kind: ShopSheetKind;
  /** Fase viva, tal cual la publica el snapshot (para el arnés y los tests). */
  phase: string;
  /** Tipo de mercader, tal cual. */
  type: string;
  rows: ShopRow[];
  /** Tecla de salida, o `null` si en esta fase el binario la re-lee (ver el docblock). */
  cancelKey: string | null;
}

/** Fase de PAUSA: el conductor las nombra con sufijo `-pause` y no exponen opciones. */
function esPausa(snap: ShopConsoleSnapshot): boolean {
  return snap.phase.endsWith("-pause") && snap.options.length === 0;
}

/** Portón Y/N: dos opciones y sus teclas son exactamente `y` y `n`. */
function esYesNo(opts: readonly ShopConsoleOption[]): boolean {
  return opts.length === 2 && opts[0]?.key === "y" && opts[1]?.key === "n";
}

/**
 * La tecla que el jugador pulsaría para salir de ESTA fase, o `null` si el binario la
 * re-lee y un botón sería un botón muerto. Derivación completa en el docblock de arriba.
 */
function teclaDeSalida(kind: ShopSheetKind, phase: string): string | null {
  if (kind === "yesno" || kind === "continue") return null;
  // `healer-need` (0x1550-0x1568): el getkey re-lee Escape; Espacio ecoa `Nothing`.
  if (phase === "healer-need") return " ";
  return "Escape";
}

/**
 * Deriva la hoja de la instantánea VIVA. Devuelve `null` cuando no hay nada que ofrecer:
 * sin opciones y sin pausa (la guarda defensiva del gremio fuera de tramo, o la fase
 * `menu` en que la ventana GUEST REGISTER es la dueña del prompt), un panel con un
 * «Salir» solitario sería peor que ninguno — y el teclado sigue armado debajo.
 */
export function shopSheet(snap: ShopConsoleSnapshot | null | undefined): ShopSheet | null {
  if (!snap) return null;
  if (esPausa(snap)) {
    return {
      kind: "continue",
      phase: snap.phase,
      type: snap.type,
      // La ÚNICA fila de una pausa. Espacio y no Enter: las tres pausas del binario
      // descartan la tecla, y el gate global de salida —que sí mira el Espacio— corre
      // DESPUÉS de las tres ramas, así que aquí Espacio significa «continúa», nunca
      // «vete». Es además lo que se pulsa ante un «press any key».
      rows: [{ key: " ", label: "Continue", crudo: false }],
      cancelKey: null,
    };
  }
  if (snap.options.length === 0) return null;
  const kind: ShopSheetKind = esYesNo(snap.options)
    ? "yesno"
    : snap.options.some((o) => typeof o.idx === "number")
      ? "members"
      : snap.phase.endsWith("-list")
        ? "list"
        : "menu";
  // `crudo`: los rótulos de mercancía («Long Sword — 42 gp») y los NOMBRES PROPIOS de los
  // compañeros ya vienen resueltos del conductor —los primeros pasados por `t()`, los
  // segundos porque un nombre propio no se traduce—, así que la vista no vuelve a
  // tocarlos. Los de menú y Y/N son cromo corto autorado («Buy», «Yes») y sí van por
  // `ts()`, como todo el cromo del shell.
  const crudo = kind === "list" || kind === "members";
  return {
    kind,
    phase: snap.phase,
    type: snap.type,
    rows: snap.options.map((o) => ({
      key: o.key,
      label: o.label,
      ...(typeof o.idx === "number" ? { member: o.idx } : {}),
      crudo,
    })),
    cancelKey: teclaDeSalida(kind, snap.phase),
  };
}

/**
 * FIRMA de la hoja — la señal de «esto ya no es lo mismo que había pintado».
 *
 * La usa la conciliación para decidir si RE-PINTA. El panel de tienda, a diferencia del
 * selector de miembro, cambia DENTRO de una misma sesión de prompt (cada tecla puede
 * llevar de `menu` a `buy-list` y de ahí a `buy-deal`), así que «idempotente» aquí no
 * puede ser «no repintar nunca»: es «no repintar si la hoja dice exactamente lo mismo».
 */
export function shopSheetFirma(sheet: ShopSheet | null): string {
  if (!sheet) return "";
  return [
    sheet.kind,
    sheet.phase,
    sheet.type,
    sheet.cancelKey ?? "",
    ...sheet.rows.map((r) => `${r.key}${r.label}${r.member ?? ""}`),
  ].join("");
}
