/**
 * A3-HF10 — la respuesta a "How much? " de (M)ix, derivada del binario de 1988
 * (re/notes/mix-hf10-command-parity.md §3-§4):
 *
 *   · CMDS.OVL 0x1a70 `mix_quantity(mask)`: 0 sale sin comprobar; cada reagente
 *     MARCADO se compara SIN SIGNO de 16 bits con la respuesta (0x1aa5 `jae`) y uno
 *     corto imprime "Insufficient reagents!" y RE-PREGUNTA (0x1ac6).
 *   · `cmd_mix` 0x1b71 (`jg`): n <= 0 aborta en silencio; 0x1b78: máscara vacía →
 *     "Nothing to mix!".
 *   · kernel 0x3b9e (getnum): '+'/'-' sólo como 1ª casilla; ESC borra y sigue
 *     leyendo; backspace-en-vacío se ignora; sólo Enter sale.
 *
 * El port nativo aplica la misma regla en `openu5::mix_quantity_short`
 * (native/core/src/magic.cpp) — test de runtime `a3_hf10_mix_parity_runtime`.
 */
import { describe, it, expect } from "vitest";
import { mixQuantityVerdict } from "../src/core/magic/mix.js";
import { PromptManager } from "../src/ui/prompt-manager.js";

const counts = (c: Record<number, number>) => (r: number) => c[r] ?? 0;

describe("mixQuantityVerdict (CMDS 0x1a70 → 0x1b71 → 0x1b78)", () => {
  it("0 aborta en silencio SIN comprobar reagentes (0x1a8e)", () => {
    expect(mixQuantityVerdict(0, [0], counts({}))).toBe("abort");
    expect(mixQuantityVerdict(0, [], counts({}))).toBe("abort");
  });

  it("un reagente MARCADO corto → insufficient (re-pregunta); los no marcados no cuentan", () => {
    expect(mixQuantityVerdict(3, [1, 3], counts({ 1: 2, 3: 8 }))).toBe("insufficient");
    expect(mixQuantityVerdict(2, [1, 3], counts({ 1: 2, 3: 8 }))).toBe("mix");
    expect(mixQuantityVerdict(3, [0], counts({ 0: 3, 1: 0 }))).toBe("mix");
  });

  it("un N negativo es 0xfffb sin signo: siempre corto con algo marcado", () => {
    expect(mixQuantityVerdict(-5, [0], counts({ 0: 99 }))).toBe("insufficient");
    expect(mixQuantityVerdict(-1, [0, 1], counts({ 0: 99, 1: 99 }))).toBe("insufficient");
  });

  it("máscara vacía: N>0 → nothing (\"Nothing to mix!\"), N<0 → abort en silencio", () => {
    expect(mixQuantityVerdict(2, [], counts({ 0: 5 }))).toBe("nothing");
    expect(mixQuantityVerdict(-2, [], counts({ 0: 5 }))).toBe("abort");
  });

  it("la respuesta de dos cifras se compara entera (no hay tope oculto)", () => {
    expect(mixQuantityVerdict(99, [0], counts({ 0: 99 }))).toBe("mix");
    expect(mixQuantityVerdict(12, [0], counts({ 0: 11 }))).toBe("insufficient");
  });
});

function key(k: string): KeyboardEvent {
  return { key: k, preventDefault: () => {} } as unknown as KeyboardEvent;
}

describe("PromptManager — number = getnum del kernel (0x3b9e)", () => {
  const make = () => {
    const echoes: string[] = [];
    const got: number[] = [];
    const pm = new PromptManager({ hud: { echoSetLast: (t) => echoes.push(t) } });
    pm.current = { type: "number", prefix: "How much? ", buffer: "", max: 2, submit: (n) => got.push(n) };
    return { pm, echoes, got };
  };

  it("ESC borra lo tecleado y SIGUE preguntando (0x3c0e); en vacío no hace nada", () => {
    const { pm, echoes, got } = make();
    pm.handleKey(key("4"));
    pm.handleKey(key("Escape"));
    expect(pm.current).not.toBe(null);
    expect(echoes[echoes.length - 1]).toBe("How much? ");
    pm.handleKey(key("Escape"));
    expect(pm.current).not.toBe(null);
    pm.handleKey(key("7"));
    pm.handleKey(key("Enter"));
    expect(got).toEqual([7]);
  });

  it("backspace borra una cifra; en vacío se ignora (0x3c00)", () => {
    const { pm, got } = make();
    pm.handleKey(key("4"));
    pm.handleKey(key("5"));
    pm.handleKey(key("Backspace"));
    pm.handleKey(key("Backspace"));
    pm.handleKey(key("Backspace"));
    expect(pm.current).not.toBe(null);
    pm.handleKey(key("Enter"));
    expect(got).toEqual([0]);
  });

  it("'+'/'-' sólo en la 1ª casilla y ocupan una (0x3be2)", () => {
    const { pm, got } = make();
    pm.handleKey(key("-"));
    pm.handleKey(key("5"));
    pm.handleKey(key("6")); // max=2: el signo ya ocupa una casilla
    pm.handleKey(key("Enter"));
    const b = make();
    b.pm.handleKey(key("+"));
    b.pm.handleKey(key("3"));
    b.pm.handleKey(key("Enter"));
    const c = make();
    c.pm.handleKey(key("3"));
    c.pm.handleKey(key("-")); // no es la 1ª casilla: ignorado
    c.pm.handleKey(key("Enter"));
    expect([...got, ...b.got, ...c.got]).toEqual([-5, 3, 3]);
  });
});
