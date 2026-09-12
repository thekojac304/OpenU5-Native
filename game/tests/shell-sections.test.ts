/**
 * Secciones del menú SISTEMA (shell): estructura, cableado de deps y conversión
 * de unidades del volumen. Sin DOM (la sección Teclas es `custom` y no se
 * renderiza aquí); el drawer vivo lo cubre e2e/shell-menu.spec.ts.
 */
import { describe, expect, it, afterEach } from "vitest";
import { buildShellSections, type ShellDeps } from "../src/ui/shell/sections.js";
import { setLang, BASE_LANG } from "../src/i18n/index.js";
import type {
  ButtonField,
  CheckboxField,
  DebugField,
  DebugSection,
  NumberField,
} from "../src/debug/types.js";

type Stub = ShellDeps & { calls: string[] };

function stubDeps(overrides: Partial<ShellDeps> = {}): Stub {
  const calls: string[] = [];
  const deps: Stub = {
    calls,
    currentSkinId: () => "1988 (fiel)",
    availableSkins: () => [
      { id: "faithful", label: "1988 (fiel)" },
      { id: "shader", label: "Shader (xBR)" },
    ],
    selectSkin: (id: string) => calls.push(`selectSkin:${id}`),
    musicEnabled: () => false,
    setMusicEnabled: (on: boolean) => calls.push(`setMusicEnabled:${on}`),
    musicStatus: () => ({ estado: "s", pista: "s", via: "s", portada: "s", error: "-", hilo: "-" }),
    musicVolume: () => 0.55,
    setMusicVolume: (v: number) => calls.push(`setMusicVolume:${v}`),
    speakerEnabled: () => true,
    setSpeakerEnabled: (on: boolean) => calls.push(`setSpeakerEnabled:${on}`),
    openSaves: () => calls.push("openSaves"),
    close: () => calls.push("close"),
  };
  return Object.assign(deps, overrides);
}

function findField(sections: DebugSection[], label: string): DebugField {
  for (const s of sections)
    for (const f of s.fields ?? []) if (f.label === label) return f;
  throw new Error(`campo no encontrado: ${label}`);
}

describe("buildShellSections — menú SISTEMA", () => {
  // El shell se autora en inglés (base i18n); el idioma de módulo por defecto es 'en'.
  // Cada test restaura 'en' para no filtrar estado entre casos.
  afterEach(() => setLang(BASE_LANG, { persist: false }));

  it("secciones base (sin debug) y sección debug sólo con openDebug", () => {
    // Sin `companionAvailable` NO hay sección Ayuda: es el estado de la demo
    // pública / repo público, donde /companion no está desplegado.
    // El censo creció DOS veces con diseño deliberado y este aserto no se movió (estuvo
    // EXENTO en la puerta #221 hasta el 17-08): `shell-mas` es la puerta a la web de la
    // ficha #154 (8f3047dd — acción, entre los paneles y las referencias) y `shell-close`
    // es la salida ROTULADA del drawer que #263 puso al retirar el `esc` de la esquina
    // (e3bf247c — el requisito de accesibilidad no podía perderse con el botón).
    // ★ SIN «shell-controls» Y ESO ES LA MEDIDA, no un olvido: el rediseño de ajustes
    // sacó de «Vídeo» las tres filas táctiles (layout partido, lado del pad, mandos
    // Enhanced) a una sección propia, y las tres van con GATE DE DISPONIBILIDAD. Este stub
    // no cablea ninguna de esas deps —es el censo de ESCRITORIO—, así que la sección se
    // queda sin campos y no se construye. El caso táctil (con la sección) lo mide
    // `tests/ajustes-categorias.test.ts`, que arma las deps completas.
    expect(buildShellSections(stubDeps()).map((s) => s.id)).toEqual([
      "shell-panels",
      "shell-video",
      "shell-audio",
      "shell-mas",
      "shell-keys",
      "shell-commands",
      "shell-close",
    ]);
    const withDebug = buildShellSections(stubDeps({ openDebug: () => {} }));
    expect(withDebug.map((s) => s.id)).toContain("shell-debug");
  });

  // GATE del atlas (auditoría de cierre 2026-07-27): el enlace a /companion era
  // INCONDICIONAL y en los destinos públicos —que no llevan el atlas— abría una
  // pestaña 404. La fila existe si y sólo si la sonda dice que está desplegado.
  it("Ayuda/atlas: presente con companionAvailable true, ausente con false u omitido", () => {
    const conAtlas = buildShellSections(stubDeps({ companionAvailable: () => true }));
    expect(conAtlas.map((s) => s.id)).toContain("shell-help");
    expect(findField(conAtlas, "Atlas & guide (new tab)")).toBeDefined();

    for (const deps of [
      stubDeps({ companionAvailable: () => false }),
      stubDeps(),
    ]) {
      const secs = buildShellSections(deps);
      expect(secs.map((s) => s.id)).not.toContain("shell-help");
      expect(() => findField(secs, "Atlas & guide (new tab)")).toThrow();
    }
  });

  it("los lanzadores de panel cierran el shell ANTES de abrir el panel", () => {
    const deps = stubDeps();
    const btn = findField(buildShellSections(deps), "Save / Load (F5)") as ButtonField;
    btn.run();
    expect(deps.calls).toEqual(["close", "openSaves"]);
  });

  it("con deps táctiles aparece «shell-controls» entre Vídeo y Audio", () => {
    // La otra mitad del aserto de arriba: el gate abre en las dos direcciones.
    const secs = buildShellSections(
      stubDeps({ enhancedControlsDisponible: () => true, enhancedControls: () => false, setEnhancedControls: () => {} }),
    );
    expect(secs.map((s) => s.id)).toContain("shell-controls");
    const mandos = secs.find((s) => s.id === "shell-controls")!;
    expect((mandos.fields ?? []).map((f) => f.label)).toEqual(["Enhanced controls"]);
  });

  it("Video lista una piel user-facing por botón y cablea selectSkin (sin dev)", () => {
    const deps = stubDeps();
    const secs = buildShellSections(deps);
    const video = secs.find((s) => s.id === "shell-video")!;
    const skinBtns = (video.fields ?? []).filter(
      (f): f is ButtonField => f.widget === "button" && f.label.startsWith("Skin: "),
    );
    expect(skinBtns.map((b) => b.label)).toEqual(["Skin: 1988 (fiel)", "Skin: Shader (xBR)"]);
    skinBtns[1]!.run();
    expect(deps.calls).toContain("selectSkin:shader");
    // La piel dev NO se ofrece en el menú (task #79).
    expect(skinBtns.some((b) => b.label.toLowerCase().includes("dev"))).toBe(false);
  });

  it("volumen: expone % redondeado y escribe 0..1", () => {
    const deps = stubDeps();
    const vol = findField(buildShellSections(deps), "Music volume (%)") as NumberField;
    expect(vol.get()).toBe(55);
    vol.set(30);
    expect(deps.calls).toContain("setMusicVolume:0.3");
  });

  it("checkboxes de audio cablean a sus deps", () => {
    const deps = stubDeps();
    const secs = buildShellSections(deps);
    const musica = findField(secs, "Music (F7)") as CheckboxField;
    expect(musica.get()).toBe(false);
    musica.set(true);
    expect(deps.calls).toContain("setMusicEnabled:true");
    const spk = findField(secs, "PC speaker 1988 (F8)") as CheckboxField;
    expect(spk.get()).toBe(true);
  });

  it("lang=es traduce labels de sección/botón (nombre de piel intacto)", () => {
    setLang("es", { persist: false });
    const deps = stubDeps();
    const secs = buildShellSections(deps);
    expect(secs.find((s) => s.id === "shell-video")!.title).toBe("Vídeo");
    const save = findField(secs, "Guardar / Cargar (F5)") as ButtonField;
    expect(save.widget).toBe("button");
    // El prefijo se traduce pero la etiqueta de piel (nombre propio) NO.
    const video = secs.find((s) => s.id === "shell-video")!;
    const skinBtns = (video.fields ?? []).filter(
      (f): f is ButtonField => f.widget === "button" && f.label.startsWith("Piel: "),
    );
    expect(skinBtns.map((b) => b.label)).toEqual(["Piel: 1988 (fiel)", "Piel: Shader (xBR)"]);
  });
});
