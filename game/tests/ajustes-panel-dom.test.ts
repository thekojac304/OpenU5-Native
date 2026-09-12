// @vitest-environment jsdom
/**
 * PANEL DE AJUSTES — la mitad de RENDER (rediseño de ajustes). Sujeto: el `DebugPanel` con
 * `opts.categories`, o sea el drawer SISTEMA tal y como lo monta `main.ts`, construido con
 * las secciones REALES de `buildShellSections` (no una maqueta: si el reparto de
 * categorías o un gate cambian, este test lo ve).
 *
 * QUÉ CUBRE Y POR QUÉ AQUÍ Y NO EN E2E: todo lo que es ESTRUCTURA y COMPORTAMIENTO del DOM
 * —categorías, navegación, semántica aria, teclado, filtro, pie fijo, persistencia de
 * callbacks— se puede medir sin navegador y corre en la puerta barata. Lo que jsdom NO
 * puede medir es el LAYOUT (píxeles, solapes, qué se ve): eso vive en
 * `e2e/shell-menu.spec.ts` (escritorio) y `e2e/mobile/*` (teléfono), y esos son los que
 * juzgan el modo raíl vs lista→detalle sobre cajas de verdad.
 *
 * 🔴 EL MODO SE FIJA A MANO (`setMode`) Y ESO ES DELIBERADO, no un atajo: en jsdom
 * `clientWidth` es 0 SIEMPRE, así que la medida real no existe y una prueba que dependiera
 * de ella mediría el vacío. El navegador expone `setMode()` justamente para esto (ver su
 * cabecera); el criterio de ANCHO —qué medida elige qué modo— lo ejercita el e2e.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { DebugPanel } from "../src/debug/panel.js";
import {
  buildShellCategories,
  buildShellSections,
  type ShellDeps,
} from "../src/ui/shell/sections.js";
import { BASE_LANG, setLang } from "../src/i18n/index.js";

const DRAWER = '[data-testid="u5-shell-drawer"]';

interface Arnes {
  panel: DebugPanel;
  root: HTMLElement;
  calls: string[];
  host: HTMLElement;
}

function monta(overrides: Partial<ShellDeps> = {}): Arnes {
  const calls: string[] = [];
  const noop = (): void => {};
  const deps: ShellDeps = {
    layoutPartidoDisponible: () => true,
    layoutPartido: () => false,
    setLayoutPartido: (v) => calls.push(`setLayoutPartido:${v}`),
    currentSkinId: () => "1988 (fiel)",
    availableSkins: () => [
      { id: "faithful", label: "1988 (fiel)" },
      { id: "shader", label: "Shader (xBR)" },
    ],
    selectSkin: (id) => calls.push(`selectSkin:${id}`),
    musicEnabled: () => false,
    setMusicEnabled: (v) => calls.push(`setMusicEnabled:${v}`),
    musicStatus: () => ({ estado: "a", pista: "b", via: "c", portada: "d", error: "-" }),
    musicVolume: () => 0.55,
    setMusicVolume: (v) => calls.push(`setMusicVolume:${v}`),
    speakerEnabled: () => true,
    setSpeakerEnabled: (v) => calls.push(`setSpeakerEnabled:${v}`),
    openSaves: () => calls.push("openSaves"),
    openReplays: () => calls.push("openReplays"),
    openDebug: () => calls.push("openDebug"),
    companionAvailable: () => true,
    openPrivacidad: () => calls.push("openPrivacidad"),
    languages: () => [
      { code: "en", label: "English", seed: false },
      { code: "es", label: "Español", seed: false },
    ],
    currentLang: () => "en",
    selectLang: (c) => calls.push(`selectLang:${c}`),
    padSideDisponible: () => true,
    swapPadSide: () => calls.push("swapPadSide"),
    enhancedControlsDisponible: () => true,
    enhancedControls: () => false,
    setEnhancedControls: (v) => calls.push(`setEnhancedControls:${v}`),
    close: () => calls.push("close"),
    ...overrides,
  } as ShellDeps;
  Object.assign(deps, overrides);

  const host = document.createElement("div");
  document.body.appendChild(host);
  const panel = new DebugPanel(host, () => buildShellSections(deps), noop, {
    title: () => "SYSTEM",
    testId: "u5-shell-drawer",
    closeButton: false,
    categories: buildShellCategories,
    navBack: () => "Back",
    navCategories: () => "Settings categories",
  });
  panel.open();
  const root = host.querySelector<HTMLElement>(DRAWER)!;
  // Modo ANCHO por defecto (escritorio). Ver la cabecera: jsdom no mide.
  panel.settingsNav!.setMode("rail");
  return { panel, root, calls, host };
}

const tabs = (root: HTMLElement): HTMLButtonElement[] =>
  Array.from(root.querySelectorAll<HTMLButtonElement>(".u5set-cat"));
const nav = (root: HTMLElement): HTMLElement => root.querySelector<HTMLElement>(".u5set")!;

/** Rótulos de TODAS las filas pintadas, estén en la categoría que estén. */
const rotulosPintados = (root: HTMLElement): string[] =>
  Array.from(root.querySelectorAll<HTMLElement>(".u5dbg-field")).map(
    (f) =>
      f.querySelector("label")?.textContent ??
      f.querySelector("button")?.textContent?.trim() ??
      "",
  );

afterEach(() => {
  setLang(BASE_LANG, { persist: false });
  document.body.replaceChildren();
  document.getElementById("u5dbg-style")?.remove();
});

describe("panel de ajustes — estructura y categorías", () => {
  it("pinta el navegador con una pestaña por categoría poblada, en orden", () => {
    const { root } = monta();
    expect(root.querySelector(".u5set")).not.toBeNull();
    expect(tabs(root).map((t) => t.dataset.cat)).toEqual([
      "video",
      "audio",
      "controls",
      "game",
      "lang",
      "help",
      "more",
      "debug",
    ]);
  });

  it("★ TODA fila del menú se pinta: ninguna se pierde en el reparto por categorías", () => {
    const { root } = monta();
    const vivos = rotulosPintados(root);
    for (const rotulo of [
      "Save / Load (F5)",
      "Replays",
      "Active skin",
      "Skin: 1988 (fiel)",
      "Skin: Shader (xBR)",
      "4:3 period aspect (1988 skin)",
      "Fullscreen",
      "Language",
      "Split layout (portrait)",
      "Swap pad side",
      "Enhanced controls",
      "Music (F7)",
      "Music volume (%)",
      "Music: switch",
      "Music: track",
      "Music: output",
      "Music: title screen",
      "Music: last error",
      "PC speaker 1988 (F8)",
      "My games, records and moments (new tab)",
      "Legendary moments (new tab)",
      "Privacy & data",
      "Atlas & guide (new tab)",
      "Close this menu",
      "Open debug menu (` / F4)",
    ]) {
      expect(vivos, `la fila «${rotulo}» no se pintó en ninguna categoría`).toContain(rotulo);
    }
  });

  it("las dos referencias `custom` (Teclas y Comandos) se pintan dentro de Ayuda", () => {
    const { root } = monta();
    const ayuda = root.querySelector<HTMLElement>('.u5set-page[data-cat="help"]')!;
    for (const id of ["shell-keys", "shell-commands", "shell-help"]) {
      expect(
        ayuda.querySelector(`[data-section="${id}"]`),
        `${id} no está en la categoría Ayuda`,
      ).not.toBeNull();
    }
    // Y su contenido (las filas de tecla/comando) sigue ahí, no sólo la caja.
    expect(
      ayuda.querySelectorAll('[data-section="shell-commands"] .u5dbg-hint').length,
    ).toBeGreaterThan(20);
  });

  it("cada sección conserva su `data-section` (los localizadores estables no se mueven)", () => {
    const { root } = monta();
    const ids = Array.from(root.querySelectorAll<HTMLElement>(".u5dbg-section")).map(
      (s) => s.dataset.section,
    );
    expect(ids).toContain("shell-lang");
    expect(ids).toContain("shell-video");
    expect(ids).toContain("shell-controls");
    expect(ids).toContain("shell-close");
  });

  it("la pestaña declara qué secciones contiene (`data-owns`), y categoryOf lo confirma", () => {
    const { root, panel } = monta();
    const ayuda = tabs(root).find((t) => t.dataset.cat === "help")!;
    expect(ayuda.dataset.owns!.split(" ")).toEqual([
      "shell-keys",
      "shell-commands",
      "shell-help",
    ]);
    expect(panel.settingsNav!.categoryOf("shell-audio")).toBe("audio");
    expect(panel.settingsNav!.categoryOf("no-existe")).toBeNull();
  });

  it("una categoría sin secciones no se pinta (sin openDebug no hay pestaña Debug)", () => {
    const { root } = monta({ openDebug: undefined });
    expect(tabs(root).map((t) => t.dataset.cat)).not.toContain("debug");
  });

  it("en escritorio (sin deps táctiles) no hay pestaña Mandos y el resto sigue entero", () => {
    const { root } = monta({
      layoutPartidoDisponible: () => false,
      padSideDisponible: () => false,
      enhancedControlsDisponible: () => false,
    });
    expect(tabs(root).map((t) => t.dataset.cat)).not.toContain("controls");
    expect(rotulosPintados(root)).toContain("4:3 period aspect (1988 skin)");
    expect(rotulosPintados(root)).toContain("Close this menu");
  });
});

describe("panel de ajustes — navegación", () => {
  it("arranca con la primera categoría seleccionada y sólo su página visible", () => {
    const { root, panel } = monta();
    expect(panel.settingsNav!.active()).toBe("video");
    const paginas = Array.from(root.querySelectorAll<HTMLElement>(".u5set-page"));
    expect(paginas.filter((p) => !p.hidden).map((p) => p.dataset.cat)).toEqual(["video"]);
  });

  it("un clic en una pestaña cambia la página y el título del panel", () => {
    const { root } = monta();
    tabs(root).find((t) => t.dataset.cat === "audio")!.click();
    expect(root.querySelector<HTMLElement>(".u5set-pane-title")!.textContent).toBe("Audio");
    const visibles = Array.from(root.querySelectorAll<HTMLElement>(".u5set-page"))
      .filter((p) => !p.hidden)
      .map((p) => p.dataset.cat);
    expect(visibles).toEqual(["audio"]);
  });

  // ── MODO ESTRECHO (teléfono): lista → detalle ─────────────────────────────────────
  it("drill: nace en el índice, entra al tocar una categoría y vuelve con «atrás»", () => {
    const { root, panel } = monta();
    const nv = panel.settingsNav!;
    nv.setMode("rail"); // punto de partida conocido
    nv.setMode("drill");
    // `setMode("drill")` no fuerza vista: la vista de arranque del navegador es el índice.
    nv.back();
    expect(nv.view()).toBe("index");

    tabs(root).find((t) => t.dataset.cat === "controls")!.click();
    expect(nv.view()).toBe("detail");
    expect(nv.active()).toBe("controls");

    root.querySelector<HTMLButtonElement>('[data-testid="u5-settings-back"]')!.click();
    expect(nv.view()).toBe("index");
    // Volver NO pierde la categoría elegida (reentrar debe llevar donde estabas).
    expect(nv.active()).toBe("controls");
  });

  it("drill: al entrar, el foco salta al botón «atrás» (el raíl desaparece bajo el foco)", () => {
    const { root, panel } = monta();
    panel.settingsNav!.setMode("drill");
    tabs(root).find((t) => t.dataset.cat === "audio")!.click();
    expect(document.activeElement).toBe(
      root.querySelector('[data-testid="u5-settings-back"]'),
    );
  });

  it("drill: «atrás» devuelve el foco a la pestaña de la que se salió", () => {
    const { root, panel } = monta();
    panel.settingsNav!.setMode("drill");
    const audio = tabs(root).find((t) => t.dataset.cat === "audio")!;
    audio.click();
    root.querySelector<HTMLButtonElement>('[data-testid="u5-settings-back"]')!.click();
    expect(document.activeElement).toBe(audio);
  });

  it("ensanchar desde drill deja el contenido a la vista (el índice ya no aplica)", () => {
    const { root, panel } = monta();
    const nv = panel.settingsNav!;
    nv.setMode("drill");
    nv.back();
    expect(nv.view()).toBe("index");
    nv.setMode("rail");
    expect(nv.view()).toBe("detail");
    expect(nav(root).dataset.mode).toBe("rail");
  });

  it("en modo raíl, seleccionar NO roba el foco (no hay navegación que anunciar)", () => {
    const { root, panel } = monta();
    const antes = document.activeElement;
    panel.settingsNav!.select("more");
    expect(document.activeElement).toBe(antes);
  });
});

describe("panel de ajustes — accesibilidad", () => {
  it("el raíl es un tablist con nombre, y cada pestaña controla su panel", () => {
    const { root } = monta();
    const rail = root.querySelector<HTMLElement>(".u5set-rail")!;
    expect(rail.getAttribute("role")).toBe("tablist");
    expect(rail.getAttribute("aria-orientation")).toBe("vertical");
    expect(rail.getAttribute("aria-label")).toBe("Settings categories");
    for (const t of tabs(root)) {
      expect(t.getAttribute("role")).toBe("tab");
      const page = root.querySelector<HTMLElement>(`#${t.getAttribute("aria-controls")}`);
      expect(page, `la pestaña ${t.dataset.cat} apunta a un panel inexistente`).not.toBeNull();
      expect(page!.getAttribute("role")).toBe("tabpanel");
      expect(page!.getAttribute("aria-labelledby")).toBe(t.id);
    }
  });

  it("`aria-selected` y el tabindex rotatorio siguen a la categoría activa", () => {
    const { root } = monta();
    const seleccion = (): string[] =>
      tabs(root)
        .filter((t) => t.getAttribute("aria-selected") === "true")
        .map((t) => t.dataset.cat!);
    expect(seleccion()).toEqual(["video"]);
    expect(tabs(root).filter((t) => t.tabIndex === 0).map((t) => t.dataset.cat)).toEqual([
      "video",
    ]);
    tabs(root).find((t) => t.dataset.cat === "more")!.click();
    expect(seleccion()).toEqual(["more"]);
    expect(tabs(root).filter((t) => t.tabIndex === 0).map((t) => t.dataset.cat)).toEqual([
      "more",
    ]);
  });

  it("teclado del raíl: flechas, Home y End mueven la selección en ciclo", () => {
    const { root, panel } = monta();
    const rail = root.querySelector<HTMLElement>(".u5set-rail")!;
    const pulsa = (key: string): void => {
      rail.dispatchEvent(new KeyboardEvent("keydown", { key, bubbles: true, cancelable: true }));
    };
    pulsa("ArrowDown");
    expect(panel.settingsNav!.active()).toBe("audio");
    pulsa("ArrowUp");
    expect(panel.settingsNav!.active()).toBe("video");
    pulsa("ArrowUp"); // ciclo hacia atrás desde la primera
    expect(panel.settingsNav!.active()).toBe("debug");
    pulsa("Home");
    expect(panel.settingsNav!.active()).toBe("video");
    pulsa("End");
    expect(panel.settingsNav!.active()).toBe("debug");
  });

  it("las flechas NO entran en la categoría (entrar es Enter/Espacio sobre la pestaña)", () => {
    const { root, panel } = monta();
    panel.settingsNav!.setMode("drill");
    panel.settingsNav!.back();
    root
      .querySelector<HTMLElement>(".u5set-rail")!
      .dispatchEvent(new KeyboardEvent("keydown", { key: "ArrowDown", bubbles: true }));
    expect(panel.settingsNav!.view()).toBe("index");
    expect(panel.settingsNav!.active()).toBe("audio");
  });

  it("cada control con rótulo queda ATADO a su `<label>` (nombre accesible y diana)", () => {
    const { root } = monta();
    const filas = Array.from(root.querySelectorAll<HTMLElement>(".u5set .u5dbg-field"));
    const sueltas: string[] = [];
    for (const fila of filas) {
      const label = fila.querySelector("label");
      if (!label) continue; // fila de botón: su nombre es el texto del propio botón
      const ctl = fila.querySelector<HTMLElement>("input,select");
      if (!ctl || !ctl.id || label.getAttribute("for") !== ctl.id) {
        sueltas.push(label.textContent ?? "?");
      }
    }
    expect(sueltas, `rótulos sin atar a su control: ${sueltas.join(", ")}`).toEqual([]);
    expect(filas.length, "control anti-vacuo: el panel pinta filas").toBeGreaterThan(10);
  });

  it("ningún botón del panel se queda sin nombre accesible", () => {
    const { root } = monta();
    const anonimos = Array.from(root.querySelectorAll<HTMLButtonElement>(".u5set button"))
      .filter((b) => !(b.getAttribute("aria-label") ?? b.textContent ?? "").trim())
      .map((b) => b.className);
    expect(anonimos).toEqual([]);
  });

  it("las secciones dentro de una categoría son encabezados, no acordeones mudos", () => {
    const { root } = monta();
    const head = root.querySelector<HTMLElement>(
      '[data-section="shell-commands"] .u5dbg-sec-head',
    )!;
    expect(head.getAttribute("role")).toBe("heading");
    expect(head.getAttribute("aria-level")).toBe("3");
  });
});

describe("panel de ajustes — filas, descripciones y pie", () => {
  it("la descripción de un ajuste se PINTA (antes sólo existía como `title` de botón)", () => {
    const { root } = monta();
    const fila = Array.from(root.querySelectorAll<HTMLElement>(".u5dbg-field")).find((f) =>
      f.querySelector("label")?.textContent?.startsWith("Enhanced controls"),
    )!;
    const hint = fila.querySelector<HTMLElement>(".u5dbg-hint");
    expect(hint, "la casilla de mandos Enhanced no muestra su descripción").not.toBeNull();
    expect(hint!.textContent).toContain("All original commands stay available");
  });

  it("la salida del panel vive en el PIE, fuera de toda página de categoría", () => {
    const { root } = monta();
    const cerrar = root.querySelector<HTMLElement>('[data-testid="u5-shell-drawer-close"]')!;
    expect(cerrar.closest(".u5set-footer"), "la salida no está en el pie").not.toBeNull();
    expect(cerrar.closest(".u5set-page"), "la salida quedó dentro de una categoría").toBeNull();
  });

  it("la salida sigue cerrando (no es un botón decorativo), esté donde esté la navegación", () => {
    const { root, calls, panel } = monta();
    panel.settingsNav!.select("more");
    root.querySelector<HTMLButtonElement>('[data-testid="u5-shell-drawer-close"]')!.click();
    expect(calls).toContain("close");
  });

  it("los `testId` de fila se emiten también en select/number/text (residuo saldado)", () => {
    const { root } = monta();
    expect(root.querySelector('[data-testid="u5-shell-swap-pad-side"]')).not.toBeNull();
    expect(root.querySelector('[data-testid="u5-shell-enhanced-controls"]')).not.toBeNull();
  });
});

describe("panel de ajustes — persistencia y efectos", () => {
  it("los callbacks de cada widget siguen cableados a sus deps", () => {
    const { root, calls } = monta();
    const fila = (rotulo: string): HTMLElement =>
      Array.from(root.querySelectorAll<HTMLElement>(".u5dbg-field")).find(
        (f) => f.querySelector("label")?.textContent === rotulo,
      )!;

    const musica = fila("Music (F7)").querySelector<HTMLInputElement>("input")!;
    musica.checked = true;
    musica.dispatchEvent(new Event("change"));
    expect(calls).toContain("setMusicEnabled:true");

    const vol = fila("Music volume (%)").querySelector<HTMLInputElement>("input")!;
    expect(vol.value, "el control publica el valor VIVO (55 % de 0.55)").toBe("55");
    vol.value = "30";
    vol.dispatchEvent(new Event("change"));
    expect(calls).toContain("setMusicVolume:0.3");

    const idioma = fila("Language").querySelector<HTMLSelectElement>("select")!;
    idioma.value = "es";
    idioma.dispatchEvent(new Event("change"));
    expect(calls).toContain("selectLang:es");

    root.querySelector<HTMLButtonElement>('[data-testid="u5-shell-swap-pad-side"]')!.click();
    expect(calls).toContain("swapPadSide");
  });

  it("los lanzadores siguen cerrando el shell ANTES de abrir su panel", () => {
    const { root, calls } = monta();
    const save = Array.from(root.querySelectorAll<HTMLButtonElement>("button")).find(
      (b) => b.textContent?.trim() === "Save / Load (F5)",
    )!;
    save.click();
    expect(calls).toEqual(["close", "openSaves"]);
  });

  it("un `refresh` re-lee los valores sin reconstruir el DOM (no se pierde la categoría)", () => {
    const { root, panel } = monta();
    panel.settingsNav!.select("audio");
    const antes = root.querySelector(".u5set");
    panel.refresh();
    expect(root.querySelector(".u5set")).toBe(antes);
    expect(panel.settingsNav!.active()).toBe("audio");
  });
});

describe("panel de ajustes — filtro e idioma", () => {
  it("el filtro oculta las categorías sin coincidencias y salta a una que las tenga", () => {
    const { root, panel } = monta();
    const buscador = root.querySelector<HTMLInputElement>(".u5dbg-search input")!;
    buscador.value = "music";
    buscador.dispatchEvent(new Event("input"));
    const visibles = tabs(root).filter((t) => !t.hidden).map((t) => t.dataset.cat);
    expect(visibles).toEqual(["audio"]);
    expect(
      panel.settingsNav!.active(),
      "con la categoría abierta sin resultados, el panel quedaría en blanco",
    ).toBe("audio");

    buscador.value = "";
    buscador.dispatchEvent(new Event("input"));
    expect(tabs(root).filter((t) => t.hidden)).toEqual([]);
  });

  it("cambio de idioma en caliente: `invalidate` + reapertura re-rotula categorías y filas", () => {
    const { panel, host } = monta();
    setLang("es", { persist: false });
    panel.invalidate();
    panel.open();
    const root = host.querySelector<HTMLElement>(DRAWER)!;
    expect(tabs(root).map((t) => t.textContent)).toEqual([
      "Vídeo",
      "Audio",
      "Mandos",
      "Juego",
      "Idioma",
      "Ayuda",
      "Más",
      "Debug (QA)",
    ]);
    expect(rotulosPintados(root)).toContain("Guardar / Cargar (F5)");
    expect(rotulosPintados(root)).toContain("Volumen música (%)");
  });

  it("`invalidate` suelta el observador del navegador anterior (sin fugas por idioma)", () => {
    const disconnect = vi.fn();
    const original = globalThis.ResizeObserver;
    globalThis.ResizeObserver = class {
      observe(): void {}
      unobserve(): void {}
      disconnect(): void {
        disconnect();
      }
    } as unknown as typeof ResizeObserver;
    try {
      const { panel } = monta();
      panel.invalidate();
      expect(disconnect).toHaveBeenCalled();
    } finally {
      globalThis.ResizeObserver = original;
    }
  });
});

describe("drawer QA de debug — el acordeón clásico no se toca", () => {
  /**
   * El motor lo comparten los DOS drawers. Sin `categories` el cuerpo tiene que seguir
   * siendo exactamente lo que era: acordeón plegable y SIN descripciones a la vista (sus
   * e2e localizan filas con `hasText`, y sacar el texto de ayuda de 500 campos puede
   * convertir un localizador de una fila en uno de tres — ver el comentario de
   * `visibleHints` en panel.ts).
   */
  it("sin `categories` no hay navegador, y las secciones se pliegan como siempre", () => {
    const host = document.createElement("div");
    document.body.appendChild(host);
    const panel = new DebugPanel(
      host,
      () => [
        {
          id: "qa",
          title: "QA",
          fields: [
            { widget: "checkbox", label: "Spyglass", hint: "pista larga", get: () => false, set: () => {} },
          ],
        },
      ],
      () => {},
    );
    panel.open();
    const root = host.querySelector<HTMLElement>('[data-testid="u5-debug-drawer"]')!;
    expect(root.querySelector(".u5set")).toBeNull();
    expect(panel.settingsNav).toBeNull();
    expect(root.querySelector(".u5dbg-field .u5dbg-hint")).toBeNull();

    const head = root.querySelector<HTMLElement>(".u5dbg-sec-head")!;
    expect(head.getAttribute("role")).toBe("button");
    expect(head.getAttribute("aria-expanded")).toBe("true");
    head.click();
    expect(root.querySelector(".u5dbg-section")!.classList.contains("collapsed")).toBe(true);
    expect(head.getAttribute("aria-expanded")).toBe("false");
  });

  it("la cabecera del acordeón también se pliega con teclado (Enter y Espacio)", () => {
    const host = document.createElement("div");
    document.body.appendChild(host);
    const panel = new DebugPanel(
      host,
      () => [{ id: "qa", title: "QA", fields: [] }],
      () => {},
    );
    panel.open();
    const root = host.querySelector<HTMLElement>('[data-testid="u5-debug-drawer"]')!;
    const head = root.querySelector<HTMLElement>(".u5dbg-sec-head")!;
    expect(head.tabIndex).toBe(0);
    head.dispatchEvent(new KeyboardEvent("keydown", { key: "Enter", bubbles: true }));
    expect(root.querySelector(".u5dbg-section")!.classList.contains("collapsed")).toBe(true);
    head.dispatchEvent(new KeyboardEvent("keydown", { key: " ", bubbles: true }));
    expect(root.querySelector(".u5dbg-section")!.classList.contains("collapsed")).toBe(false);
  });
});
