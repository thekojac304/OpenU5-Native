/**
 * FILAS DEL SELECTOR COMPACTO DE MIEMBRO — derivadas, y PURAS.
 *
 * 🔴 NO ES UNA SEGUNDA LISTA DE PARTY. Cada campo sale de donde ya manda: `partySize` y
 * `characters[0..partySize-1]` de `GameState`, y los tres datos que se pintan son
 * EXACTAMENTE los tres que el roster del marco EGA ya enseña en su fila
 * (`skin/fiel/roster.ts`: nombre a 9 · HP a 4 · letra de estado). El selector no inventa
 * vocabulario nuevo: repite el que el jugador ya está mirando dos centímetros más arriba.
 *
 * ── LA TECLA ES EL CONTRATO ──────────────────────────────────────────────────────────
 * `key` es el byte que la fila SINTETIZA, y es el mismo que leería un teclado físico:
 * `selectPartyMemberKey` (kernel `select_party_member` 0x2d7a) acepta `'1'..'N'` como
 * selección DIRECTA del miembro `N-1`. Por eso `slot` es 1-based y `key` es su dígito:
 * tocar «Shamino» y teclear `2` son el mismo evento para todo lo que hay debajo — el
 * reductor, el prompt, el grabador de repeticiones y el orden del RNG.
 *
 * ── POR QUÉ NO SE OCULTA A NADIE (ni muertos, ni dormidos, ni hechizados) ────────────
 * Porque el picker del binario TAMPOCO los oculta, y quien valida es el LLAMADOR, no el
 * picker (docblock de `core/selectPartyMember.ts`: «NO valida el miembro […] eso es del
 * CALLER»). Dos consecuencias medibles si se filtrara aquí:
 *   · `In Mani Corp` (resucitar) tiene por objetivo, por definición, a un compañero
 *     MUERTO — esconderlo haría INALCANZABLE el hechizo desde el selector;
 *   · el bucle de re-pregunta de `resolve_command_char` (0x4a4e, «Disabled!») existe
 *     precisamente porque se puede elegir a alguien no apto; sin poder elegirlo, esa
 *     respuesta del original deja de ser alcanzable.
 * Lo que sí se hace es DECIR el estado (la misma letra del roster), que informa sin
 * cambiar nada. Es la misma regla que la lista de hechizos: marcar, nunca bloquear.
 *
 * «Sólo los miembros válidos» del encargo se cumple en el otro eje, que es el que dolía:
 * la rejilla genérica ofrecía `0`-`9` —diez teclas, siete de ellas sin destino— y aquí
 * hay exactamente `partySize` filas.
 */

/** Lo que el selector necesita de un miembro. Subconjunto de `CharacterState`. */
export interface PartyMemberLike {
  name: string;
  status: string;
  currentHp: number;
  maxHp: number;
}

/** Lo que el selector necesita del estado. Subconjunto de `GameState`. */
export interface PartyStateLike {
  partySize: number;
  characters: readonly PartyMemberLike[];
}

/** Una fila del selector. */
export interface PartyChoice {
  /** Índice 0-based en el roster = el que devuelve el picker (`onSelect`). */
  index: number;
  /** Ranura 1-based = el dígito que se teclea. */
  slot: number;
  /** La tecla SINTETIZADA. Siempre `String(slot)`; se expone para que el test la lea. */
  key: string;
  /** Nombre EFECTIVO (el del roster; «Avatar» cuando el record aún no tiene nombre). */
  name: string;
  /** Letra de estado tal cual la guarda el `.GAM` ('G','P','C','S','D'). */
  status: string;
  hp: number;
  maxHp: number;
  /**
   * ¿Está en pie? — el MISMO predicado que la flecha del roster (`roster.ts`: «alive =
   * status !== 'D' && status !== 'S'»). Sólo atenúa; no oculta ni deshabilita nada.
   */
  enPie: boolean;
}

/**
 * TOPE DE RANURAS QUE EL SELECTOR PUEDE SERVIR.
 *
 * No es un número de diseño: es el rango que el reductor acepta. `selectPartyMemberKey`
 * casa `/^[1-9]$/`, así que un miembro en la ranura 10 no tendría tecla que sintetizar —
 * y una fila que no puede producir su propia pulsación sería un botón mentiroso. El party
 * de Ultima V son 6 como mucho, así que el tope no muerde nunca; está para que, si algún
 * día mordiera, lo haga callando una fila en vez de fabricando una tecla inexistente.
 */
export const MAX_SLOTS = 9;

/** Nombre efectivo — copia local de la regla de `core/party.ts` para no arrastrar core. */
function nombre(raw: string | undefined): string {
  return raw?.trim() || "Avatar";
}

/** Construye las filas del selector para el estado vivo. Pura: sin DOM y sin i18n. */
export function partyChoices(state: PartyStateLike): PartyChoice[] {
  const out: PartyChoice[] = [];
  const n = Math.min(Math.max(0, state.partySize | 0), MAX_SLOTS);
  for (let i = 0; i < n; i++) {
    const c = state.characters[i];
    if (!c) continue; // roster incompleto (partida a medio cargar): se calla, no inventa
    const status = c.status ?? "";
    out.push({
      index: i,
      slot: i + 1,
      key: String(i + 1),
      name: nombre(c.name),
      status,
      hp: c.currentHp,
      maxHp: c.maxHp,
      enPie: status !== "D" && status !== "S",
    });
  }
  return out;
}
