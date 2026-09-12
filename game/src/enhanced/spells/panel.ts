/**
 * LA LISTA DE HECHIZOS — la vista, y NADA MÁS.
 *
 * 🔴 ESTE MÓDULO NO LANZA HECHIZOS. No importa `cast.ts`, no toca `GameState`, no mueve el
 * RNG y no sabe qué pasa después de elegir. Su contrato entero es: pintar 48 filas, dejar
 * elegir una con dedo o con teclado, y llamar a `onPick(entry)` o a `onCancel()`. Quien
 * conecta eso con el juego es `main.ts`, y lo hace ESCRIBIENDO LAS INICIALES POR EL MISMO
 * getstring rúnico del original — ver el docblock de `pickSpellForCast` allí.
 *
 * Esa separación es la que hace barata la promesa «modo Moderno sólo sustituye el paso de
 * TECLEAR»: si esta capa no tiene acceso a las reglas, no puede saltárselas.
 *
 * ── LO QUE SE ENSEÑA, Y DE DÓNDE SALE ────────────────────────────────────────────────
 * Todo de `SpellEntry` (`catalog.ts`), que a su vez es `MagicDefinitions.json` + la tabla
 * DS:0x1C90 del binario. Y la CANTIDAD MEZCLADA, que sale de `spellQuantities` — el mismo
 * dato que la página de hechizos de Ztats ya lista, así que la lista no revela nada que el
 * jugador no tuviera a dos teclas.
 *
 * ── JERARQUÍA DE LA FILA (refinamiento UX, 12-09) ────────────────────────────────────
 * 🔴 LA FILA VIEJA ERA UN VOLCADO. Medida en un iPhone SE (375×667) con el panel vivo:
 * **93,8 px de alto medio** y **6 filas de 48 visibles sin scroll**, porque cada una traía
 * palabras + cantidad + efecto + «Circle N» + coste + la lista ENTERA de reactivos +
 * «Targets: …» + los motivos de indisponibilidad, todo en párrafos que envolvían a tres y
 * cuatro líneas. Elegir un hechizo es una decisión de DOS datos («¿lo tengo mezclado?» y
 * «¿me llega el maná?»); el resto es ficha de referencia y ahora vive en el DETALLE.
 *
 * El reparto, campo por campo, y el porqué de cada uno:
 *
 *   campo                 dónde          por qué
 *   ────────────────────  ─────────────  ─────────────────────────────────────────────
 *   palabras de poder     FILA (título)  es el NOMBRE del hechizo (ver `catalog.ts`)
 *   efecto                FILA (línea 2) lo que hace, en cuatro palabras
 *   cantidad mezclada     FILA (×N)      gate «None mixed!» (CAST:0x0ebb) — decide
 *   coste de maná         FILA (N MP)    gate «M.P. too low!» (0x0ede) — decide
 *   ventana / maná / niv. FILA (aviso)   sólo cuando MUERDE, y sólo el motivo corto
 *   círculo               DETALLE        DUPLICADO: la lista ya va agrupada por círculo
 *                                        y la cabecera pegajosa lo dice encima
 *   reactivos             DETALLE        los reactivos se gastan al MEZCLAR (comando M),
 *                                        no al lanzar: aquí no deciden nada
 *   modo de apuntado      DETALLE        informativo; quien apunta es el flujo fiel
 *   «None mixed»          — (colapsado)  `×0` ya lo dice, con menos tinta
 *
 * El DETALLE es UNA sola pieza de datos (`spellDetails`) con DOS presentaciones que elige
 * el CSS: en estrecho se despliega bajo la fila (botón ⓘ) y en ancho vive en el panel
 * lateral, que sigue siempre a la fila ACTIVA. Un solo constructor, cero divergencia.
 *
 * ── POR QUÉ EL ⓘ ES UN BOTÓN APARTE Y NO LA PROPIA FILA ──────────────────────────────
 * Porque «ver la ficha» y «lanzar el hechizo» no pueden compartir gesto: un despliegue que
 * lanzara por accidente cuesta el hechizo mezclado y el maná (dos de los gates del binario
 * COBRAN). Botones hermanos ⇒ un tap es elegir y el otro es mirar, con `aria-expanded`
 * propio y sin anidar `<button>` dentro de `<button>` (que además es HTML inválido).
 *
 * ── POR QUÉ NINGUNA FILA SE DESHABILITA ──────────────────────────────────────────────
 * El original deja teclear CUALQUIER hechizo y contesta después: «None mixed!»
 * (CAST:0x0ebb), «Not here!» (ventana temporal, 0x0e1a), «M.P. too low!» (0x0ede) o el
 * «Failed!» del gate de nivel (0x0f01) — y dos de esas respuestas CUESTAN el hechizo
 * mezclado y el maná. Bloquear la fila borraría ese castigo, que es gameplay. Así que las
 * filas que hoy no saldrían bien se ATENÚAN (informar) pero se pueden elegir igual
 * (no cambiar nada).
 */
import { ts } from "../../i18n/shell.js";
import { installSpellPickerCss } from "./css.js";
import {
  groupByCircle,
  matchesQuery,
  targetLabel,
  type SpellEntry,
} from "./catalog.js";

/** Contexto vivo del lanzamiento, para atenuar lo que la ventana temporal no permite. */
export type CastPlace = "outdoor" | "town" | "dungeon" | "combat";

/** Lo que el panel necesita saber del lanzador YA elegido. Sólo para atenuar filas. */
export interface CasterInfo {
  name: string;
  mp: number;
  level: number;
}

export interface SpellPickerDeps {
  /** Las 48 filas, en orden canónico (`buildSpellCatalog`). */
  entries: readonly SpellEntry[];
  /** Cantidad mezclada viva de un hechizo (`GameState.spellQuantities[index]`). */
  quantity(index: number): number;
  /** Dónde se está lanzando — decide qué bit de la ventana temporal se exige. */
  place: CastPlace;
  /** Lanzador ya resuelto por el picker FIEL (`pickCaster`), o null si no se sabe. */
  caster: CasterInfo | null;
  /** Elegido: el llamador escribe sus iniciales por el getstring rúnico. */
  onPick(entry: SpellEntry): void;
  /** Cancelado (Esc, velo, botón): el llamador manda un ESC al getstring rúnico. */
  onCancel(): void;
  /** Host del panel. Por defecto `document.body`. */
  host?: HTMLElement;
  /**
   * ¿Enfocar el buscador al abrir? Por defecto NO en punteros gruesos: en un teléfono un
   * `focus()` sobre un `<input>` levanta el teclado del sistema encima de la lista, que es
   * exactamente lo que esta pantalla viene a evitar.
   */
  autofocusSearch?: boolean;
}

export interface SpellPickerHandle {
  /** Raíz (el velo). */
  root: HTMLElement;
  /** Cierra SIN elegir y sin llamar a `onCancel` (para el desmontaje del llamador). */
  dispose(): void;
  /** Filas actualmente visibles, en orden de pintado (para el arnés y los tests). */
  visible(): SpellEntry[];
  /** Índice (en `visible()`) de la fila activa del recorrido con teclado. */
  activeIndex(): number;
}

/** Panel vivo. Uno solo: es modal, como el getkey del binario. */
let abierto: SpellPickerHandle | null = null;

/** ¿Hay lista de hechizos abierta ahora mismo? La consulta `main.ts` (hoja del deck). */
export function spellPickerOpen(): boolean {
  return abierto !== null;
}

/** Cierra la lista viva sin elegir ni cancelar. No-op si no hay ninguna. */
export function closeSpellPicker(): void {
  abierto?.dispose();
}

/** ¿Permite la ventana temporal lanzar esta fila AQUÍ? (mismo bit que `requiredTimeBit`). */
function permitidoAqui(entry: SpellEntry, place: CastPlace): boolean {
  return entry.contexts[place];
}

/**
 * LOS CUATRO GATES DEL DISPATCHER, leídos como DATO (no como texto ya maquetado).
 *
 * Es el mismo cuarteto que `CAST:0x0e1a` / `0x0ebb` / `0x0ede` / `0x0f01` aplican DESPUÉS,
 * y aquí sólo sirve para INFORMAR: ninguna de las cuatro banderas bloquea nada (ver el
 * docblock del módulo). Se expone porque lo consumen las DOS presentaciones —el aviso
 * corto de la fila y la ficha larga del detalle— y una sola derivación es una sola verdad.
 */
export interface SpellAvailability {
  /** ¿La ventana temporal (DS:0x1C90) permite lanzarlo en este sitio? */
  aqui: boolean;
  /** ¿Hay al menos uno mezclado? */
  mezclado: boolean;
  /** ¿Le llega el maná al lanzador? (true si no se sabe quién lanza) */
  mana: boolean;
  /** ¿Tiene nivel suficiente? (true si no se sabe quién lanza) */
  nivel: boolean;
}

/** Deriva los cuatro gates de una fila. PURA: ni DOM ni i18n. */
export function spellAvailability(
  entry: SpellEntry,
  qty: number,
  place: CastPlace,
  caster: CasterInfo | null,
): SpellAvailability {
  return {
    aqui: permitidoAqui(entry, place),
    mezclado: qty > 0,
    mana: caster === null || caster.mp >= entry.mpCost,
    nivel: caster === null || caster.level >= entry.minLevel,
  };
}

/**
 * AVISO CORTO de la fila — la clave i18n del PRIMER gate que muerde, o `null` si la fila
 * saldría bien.
 *
 * 🔴 «None mixed» NO ESTÁ AQUÍ, y es deliberado: la fila ya pinta `×0`, que dice lo mismo
 * con un tercio de la tinta y sin empujar la fila a una línea más. El motivo largo sigue
 * estando en el DETALLE, donde la ficha se lee entera.
 *
 * ORDEN: sitio → nivel → maná. Es el orden en que el dispatcher los evalúa (`0x0e1a`
 * antes que `0x0f01` antes que `0x0ede`), así que el aviso nombra el gate que de verdad
 * cortaría primero y no uno posterior que el jugador nunca llegaría a ver.
 */
export function avisoCorto(av: SpellAvailability): string | null {
  if (!av.aqui) return "Not here";
  if (!av.nivel) return "Level too low";
  if (!av.mana) return "M.P. too low";
  return null;
}

/** Una fila de la FICHA: rótulo (traducible) + valor (ya compuesto). */
export interface SpellDetailRow {
  label: string;
  value: string;
}

/**
 * LA FICHA de un hechizo — lo que la fila por defecto ya NO enseña, en un solo sitio.
 *
 * Un único constructor para las dos presentaciones (despliegue en estrecho, panel lateral
 * en ancho): si divergieran, el mismo hechizo contaría dos historias según el ancho de la
 * ventana. Devuelve rótulos YA traducidos porque quien la llama sólo la pinta.
 */
export function spellDetails(
  entry: SpellEntry,
  av: SpellAvailability,
  caster: CasterInfo | null,
): SpellDetailRow[] {
  const out: SpellDetailRow[] = [
    { label: ts("Circle"), value: String(entry.circle) },
    { label: ts("MP"), value: String(entry.mpCost) },
    {
      label: ts("Reagents"),
      value:
        entry.reagentNames.length > 0 ? entry.reagentNames.join(", ") : ts("none"),
    },
  ];
  const tgt = targetLabel(entry.targetType);
  if (tgt) out.push({ label: ts("Targets"), value: ts(tgt) });
  // Los motivos, TODOS los que aplican y con su frase entera: aquí hay sitio, y el
  // jugador que abre la ficha es justo el que quiere saber por qué la fila está apagada.
  const motivos: string[] = [];
  if (!av.aqui) motivos.push(ts("Not here"));
  if (!av.mezclado) motivos.push(ts("None mixed"));
  if (!av.nivel) motivos.push(ts("Level too low"));
  if (caster !== null && !av.mana) motivos.push(ts("M.P. too low"));
  if (motivos.length > 0) out.push({ label: ts("Unavailable"), value: motivos.join(" · ") });
  return out;
}

/**
 * Abre la lista. Devuelve el asa; si ya había una abierta la cierra antes (no puede haber
 * dos getstrings vivos, así que tampoco dos listas).
 */
export function openSpellPicker(deps: SpellPickerDeps): SpellPickerHandle {
  abierto?.dispose();
  installSpellPickerCss();

  const host = deps.host ?? document.body;
  const previo = document.activeElement as HTMLElement | null;

  const scrim = document.createElement("div");
  scrim.className = "u5sp-scrim";
  scrim.dataset.testid = "u5-spell-picker";

  const panel = document.createElement("div");
  panel.className = "u5sp";
  panel.setAttribute("role", "dialog");
  panel.setAttribute("aria-modal", "true");
  panel.setAttribute("aria-label", ts("Choose a spell"));
  scrim.appendChild(panel);

  // ── Cabecera ──────────────────────────────────────────────────────────────────────
  // DOS FILAS FIJAS y no una: en un teléfono el título + el lanzador + el buscador + el
  // cancelar no caben en una línea sin que el buscador quede en 90 px. La primera es
  // IDENTIDAD (quién lanza y con cuánto maná) y la segunda es ACCIÓN (buscar / cancelar),
  // que es la que el pulgar necesita fija mientras recorre 48 filas.
  const head = document.createElement("div");
  head.className = "u5sp-head";
  const linea1 = document.createElement("div");
  linea1.className = "u5sp-headline";
  const title = document.createElement("span");
  title.className = "u5sp-title";
  title.textContent = ts("Choose a spell");
  linea1.appendChild(title);
  if (deps.caster) {
    const who = document.createElement("span");
    who.className = "u5sp-caster";
    // Nombre PROPIO (no se traduce) + maná vivo. Es lo que ya se ve en el marco del juego,
    // y es el número contra el que se lee el «N MP» de cada fila.
    who.textContent = `${deps.caster.name} — ${deps.caster.mp} ${ts("MP")}`;
    linea1.appendChild(who);
  }
  head.appendChild(linea1);

  const linea2 = document.createElement("div");
  linea2.className = "u5sp-headrow";
  const search = document.createElement("input");
  search.className = "u5sp-search";
  search.type = "text";
  // Pistas de conducta por `setAttribute`, NUNCA por propiedad IDL (ficha #219): la
  // propiedad es MUDA en el motor que no la lleve en el prototipo, y el atributo la
  // deposita igual en todos. Lo vigila `idl-conducta-censo-219.test.ts`.
  search.setAttribute("autocomplete", "off");
  search.setAttribute("spellcheck", "false");
  search.setAttribute("autocapitalize", "off");
  search.setAttribute("autocorrect", "off");
  search.placeholder = ts("Search spells…");
  search.setAttribute("aria-label", ts("Search spells…"));
  search.dataset.testid = "u5-spell-picker-search";
  linea2.appendChild(search);
  const closeBtn = document.createElement("button");
  closeBtn.type = "button";
  closeBtn.className = "u5sp-close";
  closeBtn.textContent = ts("Cancel");
  closeBtn.dataset.testid = "u5-spell-picker-cancel";
  linea2.appendChild(closeBtn);
  head.appendChild(linea2);
  panel.appendChild(head);

  // ── Cuerpo: lista + (en ancho) panel lateral de ficha ─────────────────────────────
  const body = document.createElement("div");
  body.className = "u5sp-body";
  panel.appendChild(body);

  const list = document.createElement("div");
  list.className = "u5sp-list";
  body.appendChild(list);

  // El LATERAL existe en el DOM siempre; quien decide si se ve es el CSS (`min-width`).
  // Montarlo condicionalmente en JS obligaría a observar el viewport desde aquí —otro
  // observador de layout en un módulo que no tiene ninguno— y a re-montar en cada
  // rotación. Con `display:none` el lector de pantalla tampoco lo anuncia.
  const aside = document.createElement("div");
  aside.className = "u5sp-aside";
  aside.dataset.testid = "u5-spell-picker-aside";
  body.appendChild(aside);

  const foot = document.createElement("div");
  foot.className = "u5sp-foot";
  foot.textContent = ts("Arrows move · Enter casts · Esc cancels");
  panel.appendChild(foot);

  /** Filas visibles tras el filtro, en orden de pintado. Paralelo a `botones`. */
  let visibles: SpellEntry[] = [];
  let botones: HTMLButtonElement[] = [];
  let activo = 0;

  /** Pinta una ficha (lista de `label: value`) dentro de un contenedor ya vacío. */
  const pintaFicha = (destino: HTMLElement, entry: SpellEntry): void => {
    destino.textContent = "";
    const av = spellAvailability(entry, deps.quantity(entry.index), deps.place, deps.caster);
    // Título SÓLO en el lateral: en el despliegue la fila de encima ya dice de quién es.
    if (destino === aside) {
      const h = document.createElement("div");
      h.className = "u5sp-det-title";
      h.textContent = entry.words;
      destino.appendChild(h);
      const e = document.createElement("div");
      e.className = "u5sp-det-effect";
      e.textContent = ts(entry.effect);
      destino.appendChild(e);
    }
    for (const row of spellDetails(entry, av, deps.caster)) {
      const d = document.createElement("div");
      d.className = "u5sp-det-row";
      const k = document.createElement("span");
      k.className = "u5sp-det-k";
      k.textContent = row.label;
      const v = document.createElement("span");
      v.className = "u5sp-det-v";
      v.textContent = row.value;
      d.append(k, v);
      destino.appendChild(d);
    }
  };

  const marcaActivo = (): void => {
    botones.forEach((b, i) => {
      if (i === activo) b.dataset.active = "1";
      else delete b.dataset.active;
    });
    const vivo = botones[activo];
    vivo?.scrollIntoView?.({ block: "nearest" });
    // El LATERAL sigue a la fila activa: en ancho, recorrer con flechas es leer fichas.
    const sel = visibles[activo];
    if (sel) pintaFicha(aside, sel);
    else aside.textContent = "";
    // Si el FOCO ya estaba en una fila, sigue a la fila activa: teclado y lector de
    // pantalla tienen que contar la misma historia. Si el foco está en el buscador, NO se
    // le quita (se sigue escribiendo mientras las flechas recorren la lista).
    const act = document.activeElement;
    if (vivo && act instanceof HTMLElement && act.classList.contains("u5sp-row")) vivo.focus();
  };

  const pinta = (query: string): void => {
    list.textContent = "";
    visibles = [];
    botones = [];
    const filtradas = deps.entries.filter((e) => matchesQuery(e, query));
    if (filtradas.length === 0) {
      const vacio = document.createElement("div");
      vacio.className = "u5sp-empty";
      vacio.textContent = ts("No spell matches.");
      list.appendChild(vacio);
      activo = 0;
      aside.textContent = "";
      return;
    }
    for (const grupo of groupByCircle(filtradas)) {
      const cab = document.createElement("div");
      cab.className = "u5sp-group";
      cab.textContent = `${ts("Circle")} ${grupo.circle}`;
      list.appendChild(cab);
      const rejilla = document.createElement("div");
      rejilla.className = "u5sp-rows";
      list.appendChild(rejilla);
      for (const entry of grupo.spells) {
        const qty = deps.quantity(entry.index);
        const av = spellAvailability(entry, qty, deps.place, deps.caster);
        const aviso = avisoCorto(av);

        // ENVOLTORIO: la celda de la rejilla. Dentro van los DOS botones hermanos
        // (elegir / ficha) y el despliegue — anidar el ⓘ dentro de la fila sería HTML
        // inválido y, peor, un solo objetivo táctil para dos acciones opuestas.
        const item = document.createElement("div");
        item.className = "u5sp-item";
        item.dataset.spellItem = String(entry.index);

        const btn = document.createElement("button");
        btn.type = "button";
        btn.className = "u5sp-row";
        btn.dataset.spell = String(entry.index);
        btn.dataset.initials = entry.initials;
        if (!av.mezclado || !av.aqui || !av.mana || !av.nivel) btn.dataset.dim = "1";

        const words = document.createElement("span");
        words.className = "u5sp-words";
        // Las palabras de poder NO se traducen: son el nombre del hechizo (ver catalog.ts).
        words.textContent = entry.words;
        btn.appendChild(words);

        const q = document.createElement("span");
        q.className = "u5sp-qty";
        q.textContent = `×${qty}`;
        q.setAttribute("aria-label", `${qty} ${ts("mixed")}`);
        if (qty <= 0) q.dataset.zero = "1";
        btn.appendChild(q);

        const eff = document.createElement("span");
        eff.className = "u5sp-effect";
        // `effect` es `SimpleDescription` del asset (base inglesa) → pasa por ts() como
        // todo el cromo autorado; sin fila en la tabla degrada al inglés, que es el
        // contrato de `ts()`.
        eff.textContent = ts(entry.effect);
        btn.appendChild(eff);

        const cost = document.createElement("span");
        cost.className = "u5sp-cost";
        cost.textContent = `${entry.mpCost} ${ts("MP")}`;
        btn.appendChild(cost);

        // El aviso ocupa una TERCERA línea, y por eso sólo se monta cuando muerde: las
        // filas lanzables —la mayoría— se quedan en dos líneas y el alto medio no sube.
        if (aviso) {
          const w = document.createElement("span");
          w.className = "u5sp-warn";
          w.textContent = ts(aviso);
          btn.appendChild(w);
        }
        item.appendChild(btn);

        const idx = visibles.length;

        const ficha = document.createElement("div");
        ficha.className = "u5sp-det";
        ficha.id = `u5sp-det-${entry.index}`;
        ficha.hidden = true;

        const info = document.createElement("button");
        info.type = "button";
        info.className = "u5sp-info";
        info.textContent = "ⓘ";
        info.dataset.info = String(entry.index);
        info.setAttribute("aria-expanded", "false");
        info.setAttribute("aria-controls", ficha.id);
        info.setAttribute("aria-label", `${ts("Details")}: ${entry.words}`);
        info.title = ts("Details");
        item.appendChild(info);
        item.appendChild(ficha);

        btn.addEventListener("click", () => {
          activo = idx;
          elige(entry);
        });
        // ⓘ: activa la fila y CONMUTA su ficha. Nunca elige — ver el docblock del módulo.
        info.addEventListener("click", (ev) => {
          ev.stopPropagation();
          activo = idx;
          const abrir = ficha.hidden;
          if (abrir) pintaFicha(ficha, entry);
          ficha.hidden = !abrir;
          info.setAttribute("aria-expanded", abrir ? "true" : "false");
          marcaActivo();
        });
        // Tab también mueve la fila activa: si no, `Enter` lanzaría la fila marcada y no
        // la que tiene el foco — dos cursores distintos en el mismo panel. Vale para los
        // dos botones del item: el ⓘ es tan «dónde estoy» como la propia fila.
        const sigueFoco = (): void => {
          if (activo === idx) return;
          activo = idx;
          marcaActivo();
        };
        btn.addEventListener("focus", sigueFoco);
        info.addEventListener("focus", sigueFoco);
        rejilla.appendChild(item);
        visibles.push(entry);
        botones.push(btn);
      }
    }
    if (activo >= visibles.length) activo = 0;
    marcaActivo();
  };

  /** Desmonta el DOM y suelta el listener. NO avisa a nadie: los avisos son de arriba. */
  const cierra = (): void => {
    if (abierto?.root !== scrim) return;
    window.removeEventListener("keydown", onKey, true);
    scrim.remove();
    abierto = null;
    // Devuelve el foco a donde estaba (el canvas del juego, normalmente): sin esto, tras
    // cerrar el panel el foco queda en `<body>` y el siguiente Tab arranca del principio.
    try {
      previo?.focus?.();
    } catch {
      /* el nodo previo pudo desaparecer; no es motivo para romper el cierre. */
    }
  };

  const elige = (entry: SpellEntry): void => {
    cierra();
    deps.onPick(entry); // ← escribe las iniciales por el getstring rúnico (main.ts)
  };
  const cancela = (): void => {
    cierra();
    deps.onCancel(); // ← manda un ESC al getstring rúnico (main.ts) ⇒ «None!»
  };

  /**
   * TECLADO — listener de CAPTURA sobre `window`, y se traga TODO mientras el panel vive.
   *
   * 🔴 TRAGARSE TODA TECLA NO ES PEREZA: el getstring rúnico del original ya está armado
   * debajo (o lo estará en cuanto se elija), y el bucle del juego sigue escuchando en
   * `window`. Una tecla que se colara llegaría al juego con un modal delante — la misma
   * clase de defecto que `prompt-manager` resuelve consumiendo toda tecla, válida o no.
   *
   * Y por eso las iniciales SINTÉTICAS se escriben DESPUÉS de `cierra()`, nunca antes:
   * con este listener vivo se las tragaría él.
   */
  function onKey(ev: KeyboardEvent): void {
    const enBuscador = ev.target === search;
    // Con el foco en «Cancelar», Enter/Espacio son DE ESE BOTÓN. Interceptarlos aquí
    // lanzaría el hechizo activo desde el control que existe para no lanzar ninguno. El
    // ⓘ va en la MISMA lista y por la misma razón invertida: es el control que existe
    // para MIRAR sin lanzar, así que su Enter tiene que llegarle a él.
    const enCancelar = ev.target === closeBtn;
    const enInfo =
      ev.target instanceof HTMLElement && ev.target.classList.contains("u5sp-info");
    const mueve = (delta: number): void => {
      if (visibles.length === 0) return;
      activo = (activo + delta + visibles.length) % visibles.length;
      marcaActivo();
    };
    switch (ev.key) {
      case "Escape":
        ev.preventDefault();
        ev.stopPropagation();
        cancela();
        return;
      case "ArrowDown":
        ev.preventDefault();
        mueve(1);
        break;
      case "ArrowUp":
        ev.preventDefault();
        mueve(-1);
        break;
      // ← y → recorren la MISMA secuencia lineal que ↑/↓. La lista es de UNA columna en
      // todos los anchos desde el refinamiento del 12-09, así que las cuatro flechas
      // coinciden; se conservan las cuatro porque el mapa de teclas no debe cambiar con
      // el viewport (y porque en una lista larga «derecha» como «siguiente» no estorba).
      case "ArrowRight":
        ev.preventDefault();
        mueve(1);
        break;
      case "ArrowLeft":
        ev.preventDefault();
        mueve(-1);
        break;
      case "Home":
        ev.preventDefault();
        activo = 0;
        marcaActivo();
        break;
      case "End":
        ev.preventDefault();
        activo = Math.max(0, visibles.length - 1);
        marcaActivo();
        break;
      case "Enter": {
        if (enCancelar || enInfo) break;
        ev.preventDefault();
        ev.stopPropagation();
        const sel = visibles[activo];
        if (sel) elige(sel);
        return;
      }
      case " ":
      case "Spacebar": {
        // El espacio ESCRIBE si se está buscando (hay hechizos de dos y tres palabras) y
        // ELIGE si no. Es la misma regla que el getstring rúnico, donde Espacio envía.
        if (enBuscador || enCancelar || enInfo) break;
        ev.preventDefault();
        ev.stopPropagation();
        const sel = visibles[activo];
        if (sel) elige(sel);
        return;
      }
      default:
        break;
    }
    // Tab sigue funcionando (el panel es DOM real y se recorre con lector de pantalla);
    // todo lo demás queda CONSUMIDO para que el juego de debajo no lo vea.
    ev.stopPropagation();
  }

  search.addEventListener("input", () => pinta(search.value));
  closeBtn.addEventListener("click", cancela);
  scrim.addEventListener("click", (ev) => {
    if (ev.target === scrim) cancela(); // tocar FUERA del panel cancela, como el drawer
  });
  window.addEventListener("keydown", onKey, true);

  host.appendChild(scrim);
  pinta("");

  const gruesoPuntero =
    typeof window.matchMedia === "function" && window.matchMedia("(pointer: coarse)").matches;
  if (deps.autofocusSearch ?? !gruesoPuntero) search.focus();
  else botones[0]?.focus();

  abierto = {
    root: scrim,
    dispose: cierra,
    visible: () => [...visibles],
    activeIndex: () => activo,
  };
  return abierto;
}
