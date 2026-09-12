/**
 * La fila «Replays» del menú SISTEMA. Vive en su propio fichero (y no dentro de
 * `shell-sections.test.ts`) para no meter una arista de conflicto en un fichero que
 * tocan otros carriles.
 *
 * Lo que se ata: la fila es OPCIONAL por construcción — sin la dep `openReplays` NO
 * se pinta. Ése es el gate que deja al repo público y a la demo BYO sin la función si
 * algún día se decide, sin tocar el menú.
 */
import { describe, expect, it, afterEach } from "vitest";
import { buildShellSections, type ShellDeps } from "../src/ui/shell/sections.js";
import { setLang, BASE_LANG } from "../src/i18n/index.js";
import type { ButtonField, DebugSection } from "../src/debug/types.js";

function deps(overrides: Partial<ShellDeps> = {}): ShellDeps {
  return {
    currentSkinId: () => "1988 (fiel)",
    availableSkins: () => [{ id: "faithful", label: "1988 (fiel)" }],
    selectSkin: () => {},
    musicEnabled: () => false,
    setMusicEnabled: () => {},
    musicStatus: () => ({ estado: "s", pista: "s", via: "s", portada: "s", error: "-", hilo: "-" }),
    musicVolume: () => 0.5,
    setMusicVolume: () => {},
    speakerEnabled: () => true,
    setSpeakerEnabled: () => {},
    openSaves: () => {},
    close: () => {},
    ...overrides,
  };
}

function labels(sections: DebugSection[], id: string): string[] {
  return (sections.find((s) => s.id === id)?.fields ?? []).map((f) => f.label ?? "");
}

describe("menú SISTEMA — fila de Repeticiones", () => {
  afterEach(() => setLang(BASE_LANG, { persist: false }));

  it("sin la dep `openReplays` la fila NO se pinta", () => {
    expect(labels(buildShellSections(deps()), "shell-panels")).toEqual(["Save / Load (F5)"]);
  });

  it("con la dep, la fila aparece junto a la de partidas y llama a su handler", () => {
    const calls: string[] = [];
    const sections = buildShellSections(
      deps({
        openReplays: () => calls.push("openReplays"),
        close: () => calls.push("close"),
      }),
    );
    expect(labels(sections, "shell-panels")).toEqual(["Save / Load (F5)", "Replays"]);
    const row = sections
      .find((s) => s.id === "shell-panels")!
      .fields!.find((f) => f.label === "Replays") as ButtonField;
    row.run();
    // Cierra el drawer ANTES de abrir su panel — misma convención que el resto de
    // lanzadores del shell (si no, quedan dos capas de UI encima del juego).
    expect(calls).toEqual(["close", "openReplays"]);
  });

  it("la fila se traduce con la capa del shell (es)", () => {
    setLang("es", { persist: false });
    expect(labels(buildShellSections(deps({ openReplays: () => {} })), "shell-panels")).toContain(
      "Repeticiones",
    );
  });
});
