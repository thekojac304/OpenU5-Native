/**
 * GUARDAS DE FUENTE del motor de música.
 *
 * ── POR QUÉ UNA GUARDA DE FUENTE Y NO UN TEST DE COMPORTAMIENTO ─────────────────────
 * El resto del carril OPL se prueba de verdad (banco, voces, chip y secuenciador son
 * puros: 92 tests). Pero `engine.ts` es la ÚNICA pieza que toca Web Audio —`AudioContext`,
 * `AudioWorkletNode`, `postMessage`— y nada de eso existe en vitest. El defecto que esta
 * guarda fija se le escapó ENTERO a la batería precisamente por eso: los tests del
 * reproductor usan un motor de mentira, así que el motor de verdad no lo miraba nadie.
 *
 * ── EL DEFECTO, MEDIDO EN NAVEGADOR EL 2026-09-11 ───────────────────────────────────
 * El banco de timbres se cachea en una promesa (`this.banco`) para no volver a pedirlo.
 * Se mandaba al worklet con LISTA DE TRANSFERENCIA — `postMessage(msg, [bank, midi])` —
 * y transferir un `ArrayBuffer` lo DESACOPLA del hilo principal. Resultado: la caché
 * quedaba apuntando a un búfer vacío y, a partir de la segunda pista, todo `play()`
 * moría con «ArrayBuffer at index 0 is already detached».
 *
 * 🔴 Y EL SÍNTOMA NO SEÑALABA A LA CAUSA: la PRIMERA pista sonaba perfectamente. Sólo
 * fallaba al cambiar de zona, y como el motor se tragaba el error en silencio, desde
 * fuera se leía como «la música no funciona» a secas. Dos defectos distintos —el de
 * aliasing y el `catch` mudo— sumados a uno solo imposible de atribuir.
 *
 * La guarda es tosca a propósito: no puede comprobar QUE suena, pero sí que nadie
 * reintroduce la transferencia. Si algún día el motor se hace inyectable (como
 * `skin/fiel/speaker.ts` con su `AudioContextFactory`), esto se sustituye por un test
 * de comportamiento y se borra sin pena.
 *
 * ── LA TERCERA ARQUITECTURA (2026-09-11, misma tarde) ───────────────────────────────
 * Tras cuatro rondas de «hitching» intermitente con el sintetizador DENTRO del callback
 * de audio (worklet en escritorio, ScriptProcessor en iOS), la síntesis se movió a un
 * Web Worker que produce trozos por adelantado, y el hilo de audio sólo reproduce
 * `AudioBufferSourceNode`s nativos. Las guardas de abajo fijan ESA propiedad: que nadie
 * vuelva a meter JS en el callback de audio, por buena que parezca la razón.
 */
import { describe, expect, it } from "vitest";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const AQUI = dirname(fileURLToPath(import.meta.url));
const fuente = (rel: string): string =>
  readFileSync(join(AQUI, "..", "src", "ui", rel), "utf8");

describe("engine.ts — el banco de timbres NO se transfiere", () => {
  it("🔴 ningún postMessage lleva lista de transferencia", () => {
    const src = fuente("opl/engine.ts");
    // `postMessage(x)` sí; `postMessage(x, [ ... ])` no.
    const conTransferencia = /postMessage\s*\([^)]*,\s*\[/.test(src);
    expect(
      conTransferencia,
      "transferir el banco lo desacopla y mata la 2ª pista en adelante",
    ).toBe(false);
  });

  it("el banco se cachea (por eso no puede transferirse)", () => {
    // Si alguien quita la caché, transferir volvería a ser seguro — y esta guarda,
    // innecesaria. Se comprueba para que las dos decisiones viajen juntas.
    expect(fuente("opl/engine.ts")).toMatch(/this\.banco\s*\?\?=/);
  });
});

describe("engine.ts — cero JavaScript en el hilo de audio", () => {
  /**
   * Un sintetizador dentro del callback de audio está a merced del recolector, del
   * planificador, de los escudos del navegador (Brave) y de la etapa de salida — y NADA
   * de eso lo puede medir la página: cuatro rondas de arreglos reales redujeron el fallo
   * sin quitarlo, y una medida «perfecta» desde dentro del worklet coexistió con un
   * tartamudeo constante en los oídos del usuario. Aquí el hilo de audio no ejecuta JS.
   */
  it("🔴 el motor NO usa AudioWorklet ni ScriptProcessorNode", () => {
    const src = fuente("opl/engine.ts");
    expect(src).not.toContain("audioWorklet");
    expect(src).not.toContain("AudioWorkletNode");
    expect(src).not.toContain("createScriptProcessor");
  });

  it("🔴 la síntesis va en un Web Worker importado con `?worker`", () => {
    const src = fuente("opl/engine.ts");
    expect(src).toMatch(/from "\.\/render-worker\.js\?worker"/);
    expect(src).toContain("new RenderWorker()");
  });

  it("los trozos se reproducen con AudioBufferSourceNode agendados en el reloj de audio", () => {
    const src = fuente("opl/engine.ts");
    expect(src).toContain("createBufferSource()");
    // `start(siguiente)` con `siguiente += chunkDur`: pegados a la muestra, no «cuando llega».
    expect(src).toMatch(/src\.start\(siguiente\)/);
    expect(src).toMatch(/siguiente \+= chunkDur/);
  });

  it("un Worker no exige contexto seguro: `audioDisponible` no mira el worklet", () => {
    const src = fuente("opl/engine.ts");
    const fn = /function audioDisponible\(\)[\s\S]*?\n}/.exec(src);
    expect(fn, "audioDisponible debe existir").not.toBeNull();
    expect(fn![0]).not.toContain("AudioWorklet");
    expect(fn![0]).toContain("Worker");
  });

  it("el worker sí transfiere sus trozos (son suyos), pero el motor no transfiere el banco", () => {
    // En el worker cada trozo se asigna fresco: transferirlo es correcto y gratis.
    expect(fuente("opl/render-worker.ts")).toMatch(/postMessage\(out, \[left\.buffer, right\.buffer\]\)/);
  });
});

describe("main.ts — la portada suena", () => {
  it("🔴 el reproductor se construye ANTES del `await intro.run()`", () => {
    const lineas = readFileSync(join(AQUI, "..", "src", "main.ts"), "utf8").split(/\r?\n/);
    // Por LÍNEAS DE CÓDIGO, no por `indexOf` sobre el fichero entero: `main.ts` menciona
    // `await intro.run()` en tres comentarios, y el primero está 300 líneas ANTES del
    // enunciado real. La primera versión de esta guarda comparaba contra ese comentario
    // y daba rojo con el código bien puesto — midiendo prosa en vez de programa.
    const esCodigo = (l: string): boolean => !/^\s*(\/\/|\*|\/\*)/.test(l);
    const construccion = lineas.findIndex(
      (l) => esCodigo(l) && l.includes("new MusicPlayer("),
    );
    const intro = lineas.findIndex((l) => esCodigo(l) && /=\s*await intro\.run\(\)/.test(l));
    expect(construccion, "no se encontró `new MusicPlayer(`").toBeGreaterThan(-1);
    expect(intro, "no se encontró el `await intro.run()` real").toBeGreaterThan(-1);
    // La intro BLOQUEA el arranque; detrás de ella, la portada no puede sonar.
    expect(construccion).toBeLessThan(intro);
  });

  it("🔴 el tema de portada se pide en la intro, no sólo al ganar la partida", () => {
    const src = readFileSync(join(AQUI, "..", "src", "main.ts"), "utf8");
    const veces = src.match(/music\.play\("title"\)/g) ?? [];
    // Había UNA sola llamada y estaba en el manejador de `game-won`: el tema del título
    // de Ultima V sólo sonaba si te terminabas el juego.
    expect(veces.length).toBeGreaterThanOrEqual(2);
  });
});

describe("🔴 el camino de audio no puede ASIGNAR memoria", () => {
  /**
   * Regla de oro de `AudioWorkletProcessor`: en `process()` no se asigna. El recolector
   * no falla por objetos efímeros, pero SÍ para el hilo de vez en cuando, y una pausa en
   * el hilo de audio es un CHASQUIDO.
   *
   * El defecto medido el 2026-09-11: `waveAtten()` devolvía `{ att, neg }` y se llama dos
   * veces por canal y por muestra — 2 × 18 × 49716 = **1.789.776 objetos por segundo**.
   * Más los `subarray()` por bloque. Tras empaquetar el resultado en un entero y pasar
   * offsets en vez de vistas: de 1,79 M objetos/s a 50 KB/s.
   *
   * 🔴 NO SE VE EN UNA PRUEBA OFFLINE: sin plazo no hay chasquido, y la señal renderizada
   * sale perfecta (medido: 9 transitorios en 40 s, ninguno en un evento MIDI). Por eso la
   * guarda mira la FORMA DEL CÓDIGO y no la señal.
   */
  it("waveAtten devuelve un entero empaquetado, no un objeto", () => {
    const src = fuente("opl/chip.ts");
    expect(src).toMatch(/function waveAtten\([^)]*\): number/);
    expect(src, "un `return { att... }` vuelve a asignar por muestra").not.toMatch(
      /return \{\s*att:/,
    );
  });

  it("el chip escribe con OFFSET, sin crear vistas por bloque", () => {
    expect(fuente("opl/chip.ts")).toMatch(/generate\([^)]*offset/);
  });

  it("el secuenciador pasa OFFSETS, no vistas, en las llamadas por bloque", () => {
    // Se miran las DOS llamadas que ocurren por bloque, no el fichero entero: `asegura()`
    // sí usa `subarray`, pero sólo al agrandar el búfer (una vez), no en cada bloque.
    const src = fuente("opl/sequencer.ts");
    const generate = /this\.chip\.generate\([^;]*;/.exec(src);
    const fill = /this\.fillChip\([^;]*;/.exec(src);
    expect(generate, "debe existir la llamada a chip.generate").not.toBeNull();
    expect(fill, "debe existir la llamada a fillChip").not.toBeNull();
    expect(generate![0], "un subarray por bloque es una asignación por bloque").not.toContain(
      "subarray",
    );
    expect(fill![0], "un subarray por bloque es una asignación por bloque").not.toContain(
      "subarray",
    );
  });
});

describe("engine.ts — los fallos se cuentan", () => {
  it("🔴 el catch del arranque no puede quedarse mudo", () => {
    const src = fuente("opl/engine.ts");
    // Un `catch {}` o un `catch (e) {}` sin traza es exactamente lo que convirtió un
    // fallo de un minuto en una sesión de depuración entera.
    expect(src).toMatch(/catch\s*\([a-zA-Z]+\)\s*\{[\s\S]*?console\.warn/);
  });
});

describe("music.ts — el desbloqueo de autoplay no depende sólo de keydown", () => {
  it("🔴 los gestos COMPLETADOS están entre los de desbloqueo (regla de WebKit)", () => {
    // Safari no concede la activación de audio en `touchstart`/`pointerdown`: la concede
    // cuando el gesto TERMINA. Desbloquear sólo al empezar el toque deja el AudioContext
    // `suspended`, sin error y sin sonido. Medido en un iPhone: música dentro de la
    // partida (muchos toques) y nunca en la portada (un solo toque).
    const src = fuente("music.ts");
    const m = /const UNLOCK_EVENTS = \[([\s\S]*?)\] as const/.exec(src);
    expect(m, "UNLOCK_EVENTS debe existir").not.toBeNull();
    for (const ev of ["pointerup", "touchend", "click"]) {
      expect(m![1], `falta ${ev}`).toContain(ev);
    }
  });

  it("🔴 el motor reintenta `resume()` en gestos posteriores", () => {
    // Si el primer intento cae en un gesto que WebKit no acepta, sin reintento el
    // contexto se queda suspendido PARA SIEMPRE, mudo y sin error.
    expect(fuente("opl/engine.ts")).toContain("reintentaReanudar");
  });

  it("🔴 `keyup` está entre los eventos de desbloqueo", () => {
    // La intro fiel captura TODOS los keydown en window (faithful-intro.ts:814 +
    // stopImmediatePropagation), así que un desbloqueo que sólo mire keydown no llega
    // nunca para quien navega el menú con el teclado — que es como se navega.
    const src = fuente("music.ts");
    const m = /const UNLOCK_EVENTS = \[([^\]]*)\]/.exec(src);
    expect(m, "UNLOCK_EVENTS debe existir").not.toBeNull();
    expect(m![1]).toContain("keyup");
  });
});
