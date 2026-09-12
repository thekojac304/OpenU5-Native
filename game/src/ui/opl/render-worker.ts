/**
 * WORKER DE RENDER — sintetiza la música en trozos, FUERA de cualquier hilo con plazo.
 *
 * ═══ POR QUÉ UN WORKER Y NO EL HILO DE AUDIO ════════════════════════════════════════
 *
 * Las dos primeras arquitecturas metían el sintetizador DENTRO del callback de audio:
 * un `AudioWorkletProcessor` (hilo de audio, sólo en contexto seguro) y, de respaldo, un
 * `ScriptProcessorNode` (hilo principal). Las dos daban «hitching» intermitente, cada
 * una en su plataforma (Brave en PC, Safari en iOS), y cuatro rondas de arreglos reales
 * —relleno por lotes, 1,79 M asignaciones/s, el asignador de voces, el búfer de
 * latencia— lo redujeron sin quitarlo. La razón de fondo: un callback de audio tiene un
 * PLAZO DURO de unos milisegundos, y cualquier JS que corra dentro está a merced del
 * recolector, del planificador, de los escudos del navegador y de la etapa de salida —
 * cosas que ninguna medida hecha desde la página puede ver, y que sólo se oyen.
 *
 * Aquí el sintetizador corre en un Worker sin plazo alguno: produce trozos de audio
 * por ADELANTADO y el hilo principal los encola como `AudioBufferSourceNode`s nativos,
 * agendados en el reloj de audio. El hilo de audio no ejecuta ni una línea de JS. Un
 * Worker existe en TODOS los navegadores y NO exige contexto seguro, así que el móvil
 * por LAN entra por la misma puerta que el escritorio.
 *
 * El protocolo es de dos mensajes: el motor manda «start» (banco + MIDI + tasa + tamaño
 * de trozo) y luego «next» cada vez que quiere otro; el worker responde «chunk» con dos
 * `Float32Array` TRANSFERIDOS (aquí sí: se asignan frescos por trozo, no se cachean, y
 * asignar en un worker no molesta a nadie).
 */
import { parseMilesOplBank } from "./bank.js";
import { OplSongPlayer, parseSmf } from "./sequencer.js";

export type RenderWorkerIn =
  | {
      readonly type: "start";
      readonly bank: ArrayBuffer;
      readonly midi: ArrayBuffer;
      readonly sampleRate: number;
      /** Muestras por trozo, a `sampleRate`. */
      readonly chunkFrames: number;
    }
  | { readonly type: "next" };

export type RenderWorkerOut =
  | {
      readonly type: "chunk";
      readonly index: number;
      /** Sobre un `ArrayBuffer` PLANO (no compartido): `copyToChannel` lo exige. */
      readonly left: Float32Array<ArrayBuffer>;
      readonly right: Float32Array<ArrayBuffer>;
    }
  | { readonly type: "error"; readonly message: string };

let player: OplSongPlayer | null = null;
let chunkFrames = 0;
let index = 0;

function render(): void {
  if (!player) return;
  const left = new Float32Array(chunkFrames);
  const right = new Float32Array(chunkFrames);
  player.render(left, right, chunkFrames);
  const out: RenderWorkerOut = { type: "chunk", index: index++, left, right };
  // Transferencia: los dos búferes son de este trozo y nadie más los referencia.
  (self as unknown as Worker).postMessage(out, [left.buffer, right.buffer]);
}

self.onmessage = (ev: MessageEvent<RenderWorkerIn>): void => {
  const msg = ev.data;
  if (msg.type === "next") {
    render();
    return;
  }
  try {
    chunkFrames = msg.chunkFrames;
    index = 0;
    player = new OplSongPlayer({
      bank: parseMilesOplBank(new Uint8Array(msg.bank)),
      smf: parseSmf(new Uint8Array(msg.midi)),
      sampleRate: msg.sampleRate,
      loop: true,
    });
    render(); // el primer trozo sale sin esperar a un «next»
  } catch (e) {
    player = null;
    const out: RenderWorkerOut = {
      type: "error",
      message: e instanceof Error ? e.message : String(e),
    };
    (self as unknown as Worker).postMessage(out);
  }
};
