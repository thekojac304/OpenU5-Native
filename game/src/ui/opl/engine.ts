/**
 * MOTOR DE MÚSICA OPL — el grafo de Web Audio y la carga de assets.
 *
 * Es la única pieza de todo el carril que toca el navegador. Por debajo hay tres capas
 * puras y testeables (banco, voces, chip) y un secuenciador; aquí sólo se monta el grafo,
 * se piden los ficheros y se cruza de una pista a otra.
 *
 * ═══ 🔴 CERO JAVASCRIPT EN EL HILO DE AUDIO ═════════════════════════════════════════
 *
 * La síntesis corre en un Web Worker (`render-worker.ts`) que produce trozos de audio
 * por ADELANTADO; aquí se encolan como `AudioBufferSourceNode`s nativos agendados en el
 * reloj de audio, uno pegado al siguiente (`start(t)` es exacto a la muestra, así que el
 * empalme es limpio). El hilo de audio no ejecuta JS: sólo mezcla búferes.
 *
 * Es la TERCERA arquitectura, y las dos anteriores están documentadas en el worker.
 * Resumen: un sintetizador dentro del callback de audio —worklet o ScriptProcessor—
 * está a merced del recolector, del planificador, de los escudos del navegador (Brave) y
 * de la etapa de salida, y nada de eso lo puede medir la página. Con trozos por
 * adelantado, un atasco tiene que durar MÁS que el colchón entero (varios segundos)
 * para oírse, y si aun así se oye, es demostrablemente el navegador y no este código.
 *
 * Y un Worker NO exige contexto seguro: el móvil por LAN (`http://192.168…`), que antes
 * caía a un ScriptProcessor en el hilo principal del juego, entra por la misma puerta.
 *
 * ═══ EL CROSSFADE LO HACE WEB AUDIO, NO UN setInterval ══════════════════════════════
 *
 * Cada pista tiene su `GainNode` y se rampan con `linearRampToValueAtTime`, que agenda
 * el reloj de AUDIO y no lo desvía nada de lo que pase en la página.
 *
 * ═══ QUÉ SE CARGA Y CUÁNDO ══════════════════════════════════════════════════════════
 *
 * `fat.opl` (3,6 KB) UNA vez y se reutiliza para todas las pistas; el `.mid` de cada
 * contexto (1-13 KB) la primera vez que suena, y luego de la caché del propio navegador.
 * Todo junto pesa ~87 KB — la vía OGG que esto sustituye eran 15 MB.
 */
import type { RenderWorkerIn, RenderWorkerOut } from "./render-worker.js";
// Vite empaqueta el worker como módulo aparte y devuelve su constructor. Requiere
// `worker: { format: "es" }` en vite.config.ts.
import RenderWorker from "./render-worker.js?worker";

/** Ruta del banco de timbres que emite el extractor. */
const BANK_URL = "/assets/music/fat.opl";
/** Duración del cruce entre contextos, en segundos. */
const FADE_SECONDS = 1.2;
/**
 * Duración de cada trozo renderizado, en segundos. 2 s se sintetizan en ~45 ms (el
 * synth va a 45× tiempo real, medido) y pesan 768 KB en estéreo a 48 kHz.
 */
const CHUNK_SECONDS = 2;
/**
 * Trozos que se mantienen encolados POR DELANTE de la reproducción. Con 3 × 2 s, un
 * atasco tiene que durar más de ~4 s para dejar la cola vacía.
 */
const LEAD_CHUNKS = 3;
/** Margen (s) entre «ahora» y el arranque del primer trozo, para que el agendado llegue. */
const START_MARGIN_S = 0.06;

/**
 * Lo que `MusicPlayer` necesita de un motor de música. Existe para poder INYECTAR un
 * doble en los tests: la lógica de perfiles y persistencia se comprueba sin Web Audio,
 * igual que `skin/fiel/speaker.ts` inyecta su `AudioContext`.
 */
export interface MusicEngine {
  /** Arranca `url` (un `.mid`), cruzando desde lo que estuviera sonando. */
  play(url: string, volume: number): void;
  /** Para del todo y suelta los nodos. */
  stop(): void;
  /** Cambia el volumen de lo que suene ahora. */
  setVolume(volume: number): void;
  /** Radiografía para el menú SISTEMA. */
  readonly status: MusicEngineStatus;
}

/**
 * Salud de la COLA de trozos, medida en el hilo principal — que aquí SÍ puede verlo todo,
 * porque el agendado se hace aquí. `underruns` cuenta las veces que hizo falta un trozo
 * y el siguiente tenía que haber empezado ya: la cola se vació. Es la única forma en que
 * este motor puede producir un hueco, y queda contada.
 */
export interface MusicQueueStats {
  /** Trozos entregados por el worker desde el arranque de la pista. */
  readonly chunks: number;
  /** Trozos agendados por delante de «ahora» en este momento. */
  readonly ahead: number;
  /** Veces que la cola llegó vacía al reloj de audio (hueco audible). */
  readonly underruns: number;
}

/**
 * Radiografía del motor, para los renglones «Music:» del menú SISTEMA.
 *
 * NO es telemetría ni un lujo de depuración: la música depende de cosas que el jugador
 * no puede ver y que fallan de forma MUDA —assets extraídos o no, gesto de desbloqueo
 * dado o no, política de autoplay del navegador—. Sin esto, «no hay música» es
 * indistinguible entre media docena de causas, y en un TELÉFONO no hay consola.
 */
export interface MusicEngineStatus {
  /** Vía de síntesis: `worker` (trozos por adelantado) o `-` si aún no suena nada. */
  readonly via: "worker" | "-";
  /** Estado del `AudioContext` (`running`, `suspended`, o `-` si no hay). */
  readonly audio: string;
  /** ¿Hay una pista montada ahora mismo? */
  readonly sonando: boolean;
  /** Último fallo, si lo hubo. */
  readonly error: string | null;
  /** Salud de la cola de trozos; `null` hasta que suene algo. */
  readonly cola: MusicQueueStats | null;
}

/** Una pista viva: su ganancia (para el cruce) y cómo desmontarla. */
interface Voice {
  readonly gain: GainNode;
  detener(): void;
  /** Estadísticas de SU cola (sólo la voz viva alimenta el diagnóstico). */
  stats(): MusicQueueStats;
}

/** ¿Hay Web Audio siquiera? (headless y SSR: no). */
function audioDisponible(): boolean {
  return typeof AudioContext !== "undefined" && typeof Worker !== "undefined";
}

export class OplMusicEngine implements MusicEngine {
  private ctx: AudioContext | null = null;
  private banco: Promise<ArrayBuffer> | null = null;
  private actual: Voice | null = null;
  /**
   * Cada `play()` incrementa esto. Las cargas son asíncronas y el usuario puede cruzar
   * tres contextos antes de que llegue el primer fichero: al volver de un `await`, una
   * carga cuya generación ya no es la vigente se DESCARTA. Sin esto, entrar y salir de
   * un castillo deprisa deja dos pistas sonando a la vez, para siempre.
   */
  private generacion = 0;
  private volumen = 1;
  private via: MusicEngineStatus["via"] = "-";
  private ultimoError: string | null = null;

  get status(): MusicEngineStatus {
    return {
      via: this.via,
      audio: this.ctx ? this.ctx.state : "-",
      sonando: this.actual !== null,
      error: this.ultimoError,
      cola: this.actual ? this.actual.stats() : null,
    };
  }

  private contexto(): AudioContext | null {
    if (!audioDisponible()) return null;
    if (!this.ctx) {
      // Con el hilo de audio libre de JS, la latencia por defecto del navegador es la
      // que mejor conoce su plataforma; no hay nada que ganar pidiendo otra. (Se probó
      // un búfer numérico de 100 ms con el worklet: tartamudeo constante en Brave/Windows,
      // fallo en la etapa de salida, invisible desde la página. Lección aprendida.)
      this.ctx = new AudioContext();
      this.reintentaReanudar(this.ctx);
    }
    if (this.ctx.state === "suspended") void this.ctx.resume();
    return this.ctx;
  }

  /**
   * Reintenta `resume()` en cada gesto COMPLETADO hasta que el contexto corra.
   *
   * 🔴 NO ES REDUNDANTE CON EL `resume()` DE ARRIBA, y el caso que lo obliga es WebKit.
   * Safari no concede la activación de audio en `touchstart`/`pointerdown` —la concede
   * cuando el gesto TERMINA—, así que un contexto creado al EMPEZAR el toque nace
   * `suspended`, no lanza ningún error y no vuelve a intentarlo nunca: silencio perfecto
   * y mudo. Le pasó al usuario en un iPhone: dentro de la partida había toques de sobra y
   * alguno acababa cuajando, pero en la PORTADA el único toque es «Tap to continue» y la
   * música no arrancaba jamás.
   *
   * Los listeners se retiran solos en cuanto el contexto corre: no queda nada colgado.
   */
  private reintentaReanudar(ctx: AudioContext): void {
    if (typeof window === "undefined") return;
    const eventos = ["pointerup", "touchend", "click", "keyup"] as const;
    const quita = (): void => {
      for (const e of eventos) window.removeEventListener(e, intenta);
    };
    const intenta = (): void => {
      if (ctx.state === "running") {
        quita();
        return;
      }
      void ctx
        .resume()
        .then(() => {
          if (ctx.state === "running") quita();
        })
        .catch(() => {
          /* el navegador aún no lo concede: se reintenta en el siguiente gesto */
        });
    };
    for (const e of eventos) window.addEventListener(e, intenta);
  }

  private cargaBanco(): Promise<ArrayBuffer> {
    this.banco ??= fetch(BANK_URL).then((r) => {
      if (!r.ok) throw new Error(`no se pudo cargar ${BANK_URL} (HTTP ${r.status})`);
      return r.arrayBuffer();
    });
    return this.banco;
  }

  play(url: string, volume: number): void {
    this.volumen = volume;
    void this.arranca(url, ++this.generacion);
  }

  private async arranca(url: string, gen: number): Promise<void> {
    const ctx = this.contexto();
    if (!ctx) return;
    try {
      const [bank, midi] = await Promise.all([
        this.cargaBanco(),
        fetch(url).then((r) => {
          if (!r.ok) throw new Error(`no se pudo cargar ${url} (HTTP ${r.status})`);
          return r.arrayBuffer();
        }),
      ]);
      if (gen !== this.generacion) return; // nos adelantó otro cambio de contexto
      this.monta(ctx, bank, midi);
      this.via = "worker";
      this.ultimoError = null;
    } catch (e) {
      // Sin música se juega igual: un fallo de red o un asset ausente NO puede tumbar
      // el arranque del juego. El silencio es la degradación correcta.
      //
      // 🔴 PERO SILENCIOSO NO: un `catch` vacío costó una sesión entera de depuración el
      // 2026-09-11. «No hay música» tiene media docena de causas posibles y sin una línea
      // en consola son indistinguibles desde fuera. `warn` y no `error`: no es un fallo
      // del juego.
      this.ultimoError = e instanceof Error ? e.message : String(e);
      // eslint-disable-next-line no-console
      console.warn(`[u5/música] no se pudo arrancar ${url}: ${this.ultimoError}`);
    }
  }

  private monta(ctx: AudioContext, bank: ArrayBuffer, midi: ArrayBuffer): void {
    const gain = ctx.createGain();
    gain.gain.value = 0;
    gain.connect(ctx.destination);
    const voz = this.vozWorker(ctx, gain, bank, midi);

    const ahora = ctx.currentTime;
    gain.gain.setValueAtTime(0, ahora);
    gain.gain.linearRampToValueAtTime(this.volumen, ahora + FADE_SECONDS);
    this.desvanece(this.actual, ctx);
    this.actual = voz;
  }

  /**
   * Una pista = un Worker que sintetiza trozos + una cola de `AudioBufferSourceNode`s
   * pegados en el reloj de audio.
   *
   * 🔴 EL AGENDADO ES POR RELOJ DE AUDIO, NO POR «CUANDO LLEGA». Cada trozo empieza
   * EXACTAMENTE donde acaba el anterior (`siguiente += duración`), calculado en segundos
   * del contexto: así el empalme entre trozos es exacto a la muestra y no depende de
   * cuándo el hilo principal se dignó a procesar el mensaje. Si un trozo llega cuando su
   * hora ya pasó, la cola se vació: se cuenta como underrun y se resincroniza.
   */
  private vozWorker(
    ctx: AudioContext,
    gain: GainNode,
    bank: ArrayBuffer,
    midi: ArrayBuffer,
  ): Voice {
    const chunkFrames = Math.round(CHUNK_SECONDS * ctx.sampleRate);
    const chunkDur = chunkFrames / ctx.sampleRate;
    const worker: Worker = new RenderWorker();
    let siguiente = 0; // instante (reloj de audio) en que empieza el próximo trozo
    let chunks = 0;
    let underruns = 0;
    let vivo = true;
    const fuentes = new Set<AudioBufferSourceNode>();

    const pide = (): void => {
      const msg: RenderWorkerIn = { type: "next" };
      worker.postMessage(msg);
    };

    worker.onmessage = (ev: MessageEvent<RenderWorkerOut>): void => {
      if (!vivo) return;
      const msg = ev.data;
      if (msg.type === "error") {
        this.ultimoError = msg.message;
        // eslint-disable-next-line no-console
        console.warn(`[u5/música] el worker no pudo sintetizar: ${msg.message}`);
        return;
      }
      const buffer = ctx.createBuffer(2, chunkFrames, ctx.sampleRate);
      buffer.copyToChannel(msg.left, 0);
      buffer.copyToChannel(msg.right, 1);

      const ahora = ctx.currentTime;
      if (chunks === 0) {
        siguiente = ahora + START_MARGIN_S;
      } else if (siguiente < ahora) {
        // La cola llegó vacía al reloj: hueco audible. Se anota y se resincroniza.
        underruns++;
        siguiente = ahora + START_MARGIN_S;
      }
      const src = ctx.createBufferSource();
      src.buffer = buffer;
      src.connect(gain);
      src.onended = () => {
        fuentes.delete(src);
        src.disconnect();
      };
      src.start(siguiente);
      fuentes.add(src);
      siguiente += chunkDur;
      chunks++;

      // Mantener LEAD_CHUNKS trozos por delante: pedir mientras falten.
      const porDelante = (siguiente - ctx.currentTime) / chunkDur;
      if (porDelante < LEAD_CHUNKS) pide();
    };
    worker.onerror = (ev): void => {
      this.ultimoError = ev.message;
      // eslint-disable-next-line no-console
      console.warn(`[u5/música] fallo en el worker: ${ev.message}`);
    };

    // Vigilante: aunque el worker esté al día, la cola se consume sola; cada medio trozo
    // se comprueba cuánto queda por delante y se pide lo que falte.
    const vigilante = globalThis.setInterval(() => {
      if (!vivo) return;
      const porDelante = (siguiente - ctx.currentTime) / chunkDur;
      if (porDelante < LEAD_CHUNKS) pide();
    }, (CHUNK_SECONDS * 1000) / 2);

    const start: RenderWorkerIn = {
      type: "start",
      bank,
      midi,
      sampleRate: ctx.sampleRate,
      chunkFrames,
    };
    // 🔴 SIN transferir `bank`: está CACHEADO en `this.banco` para todas las pistas, y
    // transferirlo lo desacopla para siempre (medido: la segunda pista moría con
    // «ArrayBuffer … already detached»). El clon estructurado copia 3,6 KB y ya.
    worker.postMessage(start);

    return {
      gain,
      stats: () => ({
        chunks,
        ahead: Math.max(0, (siguiente - ctx.currentTime) / chunkDur),
        underruns,
      }),
      detener: () => {
        vivo = false;
        globalThis.clearInterval(vigilante);
        worker.terminate();
        for (const s of fuentes) {
          try {
            s.stop();
          } catch {
            /* ya parada */
          }
          s.disconnect();
        }
        fuentes.clear();
        gain.disconnect();
      },
    };
  }

  /** Baja una voz a cero y la desmonta cuando el cruce ha terminado. */
  private desvanece(voz: Voice | null, ctx: AudioContext): void {
    if (!voz) return;
    const ahora = ctx.currentTime;
    voz.gain.gain.cancelScheduledValues(ahora);
    voz.gain.gain.setValueAtTime(voz.gain.gain.value, ahora);
    voz.gain.gain.linearRampToValueAtTime(0, ahora + FADE_SECONDS);
    globalThis.setTimeout(() => voz.detener(), FADE_SECONDS * 1000 + 100);
  }

  stop(): void {
    this.generacion++; // invalida cualquier carga en vuelo
    const ctx = this.ctx;
    if (ctx) this.desvanece(this.actual, ctx);
    this.actual = null;
  }

  setVolume(volume: number): void {
    this.volumen = volume;
    const ctx = this.ctx;
    if (!ctx || !this.actual) return;
    const ahora = ctx.currentTime;
    this.actual.gain.gain.cancelScheduledValues(ahora);
    this.actual.gain.gain.setValueAtTime(this.actual.gain.gain.value, ahora);
    // Rampa corta: un salto de volumen en un nodo de ganancia hace «clic».
    this.actual.gain.gain.linearRampToValueAtTime(volume, ahora + 0.05);
  }
}
