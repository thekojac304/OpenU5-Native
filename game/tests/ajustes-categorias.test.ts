/**
 * ARQUITECTURA DE INFORMACIÓN del panel de ajustes (rediseño de ajustes) — la mitad PURA.
 *
 * QUÉ CIERRA, y las tres son promesas que se rompen EN SILENCIO:
 *
 *  1. ★★ NINGÚN AJUSTE SE PIERDE. El rediseño parte «Vídeo» en dos y reparte once
 *     secciones entre ocho categorías; el modo de fallo de un reparto es que una fila se
 *     quede sin destino y DESAPAREZCA de la interfaz — sigue existiendo, sigue
 *     persistiendo, y el jugador no puede tocarla. Un censo por RÓTULO (no por cuenta)
 *     enrojece nombrando la fila que falta o la que se coló.
 *
 *  2. TODO GRUPO DECLARADO EXISTE. `settingsNav` tiene una red de seguridad (una sección
 *     con `group` desconocido cae en la última categoría en vez de evaporarse), y esa red
 *     es justo lo que haría que un error de dedo pasara inadvertido. Aquí se exige la
 *     coherencia de verdad, en la puerta pura y por nombre.
 *
 *  3. LOS RÓTULOS DE CATEGORÍA CABEN EN EL RAÍL, en EN y en ES. Es la clase #248/F5: en la
 *     piel fiel el texto prominente se pixeliza a una celda (16 px) por carácter, así que
 *     un rótulo largo no se recorta con elipsis — desborda o envuelve. El raíl mide 170 px.
 *
 * Sin DOM: la mitad de render (categorías, drill, teclado, filtro) vive en
 * `tests/ajustes-panel-dom.test.ts`.
 */
import { afterEach, describe, expect, it } from "vitest";
import {
  buildShellCategories,
  buildShellSections,
  type ShellDeps,
} from "../src/ui/shell/sections.js";
import { DRILL_MAX_WIDTH, FOOTER_GROUP, SETTINGS_NAV_CSS } from "../src/ui/shell/settingsNav.js";
import postcss, { type Declaration, type Rule } from "postcss";
import { BASE_LANG, setLang } from "../src/i18n/index.js";
import type { DebugSection } from "../src/debug/types.js";

const noop = (): void => {};

/** Deps con TODAS las vías disponibles: el censo máximo del menú (táctil + DEV + web). */
function depsCompletas(overrides: Partial<ShellDeps> = {}): ShellDeps {
  const deps: ShellDeps = {
    layoutPartidoDisponible: () => true,
    layoutPartido: () => false,
    setLayoutPartido: noop,
    currentSkinId: () => "1988 (fiel)",
    availableSkins: () => [
      { id: "faithful", label: "1988 (fiel)" },
      { id: "shader", label: "Shader (xBR)" },
    ],
    selectSkin: noop,
    musicEnabled: () => false,
    setMusicEnabled: noop,
    musicStatus: () => ({ estado: "a", pista: "b", via: "c", portada: "d", error: "-", hilo: "-" }),
    musicVolume: () => 0.5,
    setMusicVolume: noop,
    speakerEnabled: () => true,
    setSpeakerEnabled: noop,
    openSaves: noop,
    openReplays: noop,
    openDebug: noop,
    companionAvailable: () => true,
    openPrivacidad: noop,
    languages: () => [{ code: "en", label: "English", seed: false }],
    currentLang: () => "en",
    selectLang: noop,
    padSideDisponible: () => true,
    swapPadSide: noop,
    enhancedControlsDisponible: () => true,
    enhancedControls: () => false,
    setEnhancedControls: noop,
    castingUi: () => "classic",
    setCastingUi: noop,
    close: noop,
  };
  return Object.assign(deps, overrides);
}

function rotulos(secciones: DebugSection[]): string[] {
  return secciones.flatMap((s) => (s.fields ?? []).map((f) => f.label));
}

/**
 * CENSO del menú con todas las vías abiertas, medido el 2026-09-11 sobre la rama del
 * rediseño. Es la lista literal de TODO lo que el jugador puede tocar: si el rediseño (o
 * cualquier cambio futuro) pierde una fila, aquí sale su nombre.
 *
 * Las dos secciones `custom` (Teclas y Comandos del original) no tienen `fields` y por eso
 * no aparecen; su presencia se asierta aparte, por id.
 */
const CENSO_AJUSTES: readonly string[] = [
  // Partidas y paneles
  "Save / Load (F5)",
  "Replays",
  // Magia (interfaz de lanzamiento: clásica / lista de hechizos)
  "Casting interface",
  // Vídeo
  "Active skin",
  "Skin: 1988 (fiel)",
  "Skin: Shader (xBR)",
  "4:3 period aspect (1988 skin)",
  "Fullscreen",
  // Idioma
  "Language",
  // Mandos (las tres filas que vivían dentro de «Vídeo»)
  "Split layout (portrait)",
  "Swap pad side",
  "Enhanced controls",
  // Audio
  "Music (F7)",
  "Music volume (%)",
  "Music: switch",
  "Music: track",
  "Music: output",
  "Music: title screen",
  "Music: audio thread",
  "Music: last error",
  "PC speaker 1988 (F8)",
  // Más
  "My games, records and moments (new tab)",
  "Legendary moments (new tab)",
  // Privacidad
  "Privacy & data",
  // Ayuda
  "Atlas & guide (new tab)",
  // Salida del panel
  "Close this menu",
  // Debug (sólo DEV)
  "Open debug menu (` / F4)",
];

describe("ajustes — reparto por categorías", () => {
  afterEach(() => setLang(BASE_LANG, { persist: false }));

  it("★ ningún ajuste se pierde en el reparto: el censo de rótulos es el esperado", () => {
    const vivos = rotulos(buildShellSections(depsCompletas()));
    // Ordenados para que el aserto hable de PERTENENCIA y no de colocación: el orden de
    // las categorías es una decisión de diseño que puede cambiar; perder una fila, no.
    expect([...vivos].sort()).toEqual([...CENSO_AJUSTES].sort());
  });

  it("las dos referencias `custom` (Teclas y Comandos) siguen en el menú", () => {
    const secs = buildShellSections(depsCompletas());
    for (const id of ["shell-keys", "shell-commands"]) {
      const s = secs.find((x) => x.id === id);
      expect(s, `falta la sección ${id}`).toBeDefined();
      expect(typeof s!.custom, `${id} dejó de tener renderer propio`).toBe("function");
    }
  });

  it("toda sección declara un grupo, y ese grupo es una categoría real o el pie", () => {
    const ids = new Set(buildShellCategories().map((c) => c.id));
    ids.add(FOOTER_GROUP);
    const huerfanas = buildShellSections(depsCompletas())
      .filter((s) => !s.group || !ids.has(s.group))
      .map((s) => `${s.id} (group=${String(s.group)})`);
    expect(
      huerfanas,
      `secciones con grupo ausente o desconocido: ${huerfanas.join(", ")}. ` +
        `settingsNav las recogería en la ÚLTIMA categoría para no perderlas, que es una ` +
        `red de seguridad y no el sitio donde deben estar`,
    ).toEqual([]);
  });

  it("la salida del panel va al PIE, no a una categoría (alcanzable desde cualquiera)", () => {
    const cerrar = buildShellSections(depsCompletas()).find((s) => s.id === "shell-close");
    expect(cerrar?.group).toBe(FOOTER_GROUP);
  });

  it("«Mandos» separa las tres filas táctiles de «Vídeo» (y Vídeo conserva las suyas)", () => {
    const secs = buildShellSections(depsCompletas());
    const mandos = secs.find((s) => s.id === "shell-controls")!;
    expect(rotulos([mandos])).toEqual([
      "Split layout (portrait)",
      "Swap pad side",
      "Enhanced controls",
    ]);
    const video = secs.find((s) => s.id === "shell-video")!;
    expect(rotulos([video])).toEqual([
      "Active skin",
      "Skin: 1988 (fiel)",
      "Skin: Shader (xBR)",
      "4:3 period aspect (1988 skin)",
      "Fullscreen",
    ]);
  });

  // GATES INTACTOS — el rediseño no puede ensanchar ni estrechar ninguna disponibilidad.
  it("sin deps táctiles la sección Mandos NO se construye (y nada de Vídeo se cae con ella)", () => {
    const secs = buildShellSections(
      depsCompletas({
        layoutPartidoDisponible: () => false,
        padSideDisponible: () => false,
        enhancedControlsDisponible: () => false,
      }),
    );
    expect(secs.map((s) => s.id)).not.toContain("shell-controls");
    expect(rotulos(secs.filter((s) => s.id === "shell-video"))).toEqual([
      "Active skin",
      "Skin: 1988 (fiel)",
      "Skin: Shader (xBR)",
      "4:3 period aspect (1988 skin)",
      "Fullscreen",
    ]);
  });

  it("cada gate táctil se ofrece por separado (una sola dep viva ⇒ sección con una fila)", () => {
    const soloPad = buildShellSections(
      depsCompletas({
        layoutPartidoDisponible: () => false,
        enhancedControlsDisponible: () => false,
      }),
    );
    expect(rotulos(soloPad.filter((s) => s.id === "shell-controls"))).toEqual(["Swap pad side"]);

    const soloEnhanced = buildShellSections(
      depsCompletas({
        layoutPartidoDisponible: () => false,
        padSideDisponible: () => false,
      }),
    );
    expect(rotulos(soloEnhanced.filter((s) => s.id === "shell-controls"))).toEqual([
      "Enhanced controls",
    ]);
  });

  it("sin openDebug no hay sección ni categoría poblada de Debug", () => {
    const secs = buildShellSections(depsCompletas({ openDebug: undefined }));
    expect(secs.map((s) => s.id)).not.toContain("shell-debug");
    // La categoría sigue DECLARADA (la lista es fija); es el navegador quien no la pinta
    // al no encontrarle secciones. Se asierta aquí para que nadie la borre por «vacía».
    expect(buildShellCategories().map((c) => c.id)).toContain("debug");
  });

  it("los ids de categoría son únicos y ninguno choca con el grupo reservado del pie", () => {
    const ids = buildShellCategories().map((c) => c.id);
    expect(new Set(ids).size).toBe(ids.length);
    expect(ids).not.toContain(FOOTER_GROUP);
  });

  it("las categorías se traducen: en inglés identidad, en español la tabla del shell", () => {
    expect(buildShellCategories().map((c) => c.title)).toEqual([
      "Video",
      "Audio",
      "Controls",
      "Game",
      "Language",
      "Help",
      "More",
      "Debug (QA)",
    ]);
    setLang("es", { persist: false });
    expect(buildShellCategories().map((c) => c.title)).toEqual([
      "Vídeo",
      "Audio",
      "Mandos",
      "Juego",
      "Idioma",
      "Ayuda",
      "Más",
      "Debug (QA)",
    ]);
  });

  /**
   * ★ LA GUARDA DE ANCHO (#248/F5 aplicada al raíl). Con la piel fiel el drawer pixeliza
   * su texto prominente a UNA CELDA de 16 px por carácter y no recorta con elipsis, así
   * que un rótulo que no cabe envuelve o desborda. El raíl tiene 170 px con ~128 px
   * útiles; el tope de 10 caracteres es la cifra que deja «Debug (QA)» justo dentro y
   * rechaza cualquier «Convenience» o «Accessibility» que alguien añada mañana.
   *
   * Se mide en los DOS idiomas porque una traducción puede alargar lo que el inglés cabía.
   */
  it("ningún rótulo de categoría pasa de 10 caracteres, ni en EN ni en ES", () => {
    for (const lang of ["en", "es"]) {
      setLang(lang, { persist: false });
      const largos = buildShellCategories()
        .filter((c) => c.title.length > 10)
        .map((c) => `${lang}:${c.title} (${c.title.length})`);
      expect(largos, `rótulos que no caben en el raíl: ${largos.join(", ")}`).toEqual([]);
    }
  });

  /**
   * Control ANTI-VACUO del aserto de arriba: si `buildShellCategories()` devolviera lista
   * vacía (un refactor, un gate mal puesto), el filtro pasaría en verde sin medir nada.
   */
  it("control positivo: el menú declara al menos seis categorías", () => {
    expect(buildShellCategories().length).toBeGreaterThanOrEqual(6);
  });

  /**
   * #248 extendido al chrome NUEVO: el atlas IBM.CH sólo cubre 0x00-0x7F, y un carácter
   * fuera no se pinta pero SÍ reserva ancho (32 px de sangría fantasma en su día). Los
   * rótulos de categoría se sirven en las dos lenguas, así que el castellano entra con sus
   * acentos — lo que se prohíbe es el ADORNO, no la letra: el ámbito son los glifos
   * decorativos (flechas, engranajes, iconos), que viven muy por encima del Latin-1 que
   * cubre el atlas de extensión.
   */
  it("ningún rótulo de categoría lleva glifo decorativo fuera de los dos atlas", () => {
    for (const lang of ["en", "es"]) {
      setLang(lang, { persist: false });
      for (const c of buildShellCategories()) {
        const fuera = [...c.title].filter((ch) => ch.charCodeAt(0) > 0xff);
        expect(
          fuera,
          `${lang}:«${c.title}» lleva ${fuera
            .map((ch) => `U+${ch.charCodeAt(0).toString(16).toUpperCase()}`)
            .join(" ")} — el atlas no lo pinta pero le reserva ancho (#248)`,
        ).toEqual([]);
      }
    }
  });
});

/**
 * CONTRATO DE LA HOJA del navegador. jsdom no hace layout, así que lo que aquí se puede
 * medir de verdad no son píxeles sino las REGLAS: que existan, sobre qué selector caen y
 * qué declaran. Es la misma partición (y el mismo instrumento, postcss) que usa la guarda
 * #251, y el mismo criterio que el test de foco visible del deck táctil: se vigila la
 * regla porque es la que la auditoría encontró ausente.
 *
 * Lo que NO se mide aquí —que la caja pintada mida de verdad 44 px, que nada se solape—
 * es e2e sobre un navegador de verdad.
 */
const HOJA = postcss.parse(SETTINGS_NAV_CSS);

/** Reglas cuyo selector casa un predicado. */
function reglas(pred: (sel: string) => boolean): Rule[] {
  const out: Rule[] = [];
  HOJA.walkRules((r) => {
    if (r.selectors.some(pred)) out.push(r);
  });
  return out;
}

/** Declaraciones `prop` de las reglas que casan el predicado. */
function decls(pred: (sel: string) => boolean, prop: string): string[] {
  const out: string[] = [];
  for (const r of reglas(pred)) {
    r.walkDecls(prop, (d: Declaration) => {
      out.push(d.value);
    });
  }
  return out;
}

describe("ajustes — contrato de la hoja del navegador", () => {
  it("control anti-vacuo: la hoja compila y trae reglas", () => {
    expect(reglas(() => true).length).toBeGreaterThanOrEqual(20);
  });

  /**
   * ★ DIANAS DE TOQUE. Las cuatro superficies que un dedo tiene que acertar —la entrada de
   * categoría, la fila de ajuste, el botón de acción y la casilla— declaran suelo de 44 px.
   * La casilla es la que más lo necesita: en la piel fiel se pinta como el glifo textual
   * `[X]`, que sin suelo mide dos caracteres de ancho.
   */
  it("las cuatro dianas declaran 44 px de suelo", () => {
    for (const [nombre, sel] of [
      ["entrada de categoría", ".u5set-cat"],
      ["fila de ajuste", ".u5dbg-field"],
      ["botón de acción", ".u5dbg-field button"],
      ["casilla", "input[type=checkbox]"],
    ] as const) {
      const alturas = decls((s) => s.includes(sel), "min-height");
      expect(alturas, `${nombre}: sin suelo de altura declarado`).toContain("44px");
    }
    // La casilla además necesita ANCHO: su glifo es más estrecho que alto.
    expect(decls((s) => s.includes("input[type=checkbox]"), "min-width")).toContain("44px");
    // Y el botón de vuelta del modo estrecho, que es el control de navegación del teléfono.
    expect(decls((s) => s.includes(".u5set-back"), "min-height")).toContain("44px");
  });

  /**
   * ★ RÓTULOS SIN RECORTAR, que es la otra mitad de #248/F5. El acordeón daba a
   * `.u5dbg-field label` un `white-space:nowrap` + `text-overflow:ellipsis` dimensionado
   * para una línea; con rótulos largos («4:3 period aspect (1988 skin)», «Volumen música
   * (%)») eso se come justo la parte que dice de qué ajuste se trata, y en ES los rótulos
   * son sistemáticamente más largos que en EN. Dentro del panel de ajustes la fila ENVUELVE.
   */
  it("dentro del panel el rótulo de fila envuelve en vez de recortarse", () => {
    const sel = (s: string): boolean => s.includes(".u5set") && s.includes("label");
    expect(decls(sel, "white-space"), "el rótulo sigue en una línea").toContain("normal");
    expect(decls(sel, "text-overflow")).toContain("clip");
    expect(decls(sel, "overflow")).toContain("visible");
    // Y la fila permite la segunda línea (si no, envolver no serviría de nada).
    expect(decls((s) => s.includes(".u5set") && s.includes(".u5dbg-field"), "flex-wrap")).toContain(
      "wrap",
    );
  });

  it("el foco es VISIBLE en los tres controles de navegación", () => {
    for (const sel of [".u5set-cat", ".u5set-back"]) {
      const r = reglas((s) => s.startsWith(sel) && s.includes(":focus-visible"));
      expect(r.length, `${sel}: sin regla de foco visible`).toBeGreaterThan(0);
      const outline = r.flatMap((x) => {
        const o: string[] = [];
        x.walkDecls("outline", (d) => {
          o.push(d.value);
        });
        return o;
      });
      expect(outline.join(" "), `${sel}: la regla de foco no declara trazo propio`).toMatch(/\d+px/);
    }
  });

  /**
   * ★ LOS DOS MODOS EXISTEN EN LA HOJA. El JS sólo estampa `data-mode`/`data-view`; quien
   * de verdad pliega el panel es el CSS. Sin estas cuatro reglas el modo estrecho sería un
   * atributo que no hace nada — y el modo de fallo es mudo (el panel se vería en dos
   * columnas de 30 px en un teléfono, sin error en consola).
   */
  it("el modo estrecho declara sus cuatro reglas de plegado", () => {
    const drill = (s: string): boolean => s.includes('[data-mode="drill"]');
    expect(reglas(drill).length, "no hay reglas de modo estrecho").toBeGreaterThanOrEqual(4);
    // El índice esconde el contenido; el detalle esconde la lista. Las DOS direcciones.
    expect(
      reglas((s) => drill(s) && s.includes('[data-view="index"]') && s.includes(".u5set-pane"))
        .length,
      "en el índice, el contenido debe estar oculto",
    ).toBe(1);
    expect(
      reglas((s) => drill(s) && s.includes('[data-view="detail"]') && s.includes(".u5set-rail"))
        .length,
      "en el detalle, la lista de categorías debe estar oculta",
    ).toBe(1);
    /**
     * Y el botón de vuelta SÓLO existe en modo estrecho (en raíl no hay a dónde volver).
     *
     * ★ EL SELECTOR QUE OCULTA LLEVA ANCESTRO A PROPÓSITO, y esto es una guarda, no una
     * transcripción: en la piel fiel el botón se pixeliza y `pixelize()` le pone la clase
     * `.u5px`, cuya regla `display:inline` tiene la MISMA especificidad que un
     * `.u5set-back` pelado y va DESPUÉS en la hoja. Con el selector pelado el botón
     * «atrás» aparecía en escritorio (visto en captura). Si alguien lo simplifica, esto
     * enrojece.
     */
    const ocultaVuelta = reglas((s) => s.endsWith(".u5set-back") && !drill(s)).filter((r) => {
      let tiene = false;
      r.walkDecls("display", () => {
        tiene = true;
      });
      return tiene;
    });
    expect(ocultaVuelta.length, "no hay regla que oculte el botón de vuelta").toBe(1);
    expect(
      ocultaVuelta[0]!.selector,
      "el ocultado del botón de vuelta necesita ancestro para ganarle a `.u5px{display:inline}`",
    ).toMatch(/\S+\s+\.u5set-back$/);
    expect(decls((s) => s.endsWith(".u5set-back") && !drill(s), "display")).toContain("none");
    expect(decls((s) => drill(s) && s.includes(".u5set-back"), "display")).toContain("inline-flex");
  });

  it("el umbral de plegado es un ancho de panel, no de viewport, y es razonable", () => {
    // Un teléfono de 390 px deja ~300 px de panel útil: tiene que caer del lado estrecho.
    expect(DRILL_MAX_WIDTH).toBeGreaterThan(300);
    // …y un panel de escritorio (760 px menos marco) del ancho.
    expect(DRILL_MAX_WIDTH).toBeLessThan(680);
  });

  it("el raíl es pegajoso: el scroll lo sirve `.u5dbg-body`, no un contenedor nuevo", () => {
    // Invariante de la ficha #147: el cuerpo del drawer sigue siendo EL scroller. Si el
    // raíl dejara de ser sticky y el panel tuviera scroll propio, ese invariante móvil
    // pasaría a medir un nodo que ya no scrollea.
    expect(decls((s) => s === ".u5set-rail", "position")).toContain("sticky");
    expect(decls(() => true, "overflow-y"), "el navegador no debe abrir su propio scroll").toEqual(
      [],
    );
  });
});
