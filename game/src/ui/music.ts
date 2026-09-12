/**
 * Música contextual con crossfade (QoL). Las pistas se SINTETIZAN EN VIVO con el
 * emulador de OPL (`ui/opl/`) a partir del MIDI y el banco de timbres del Ultima V
 * Upgrade Patch — no son audio pregrabado. El navegador exige un gesto del usuario
 * antes de sonar: la reproducción arranca con la primera interacción.
 *
 * ESTE FICHERO NO CAMBIÓ SU API AL MIGRAR DE OGG A OPL, y era el requisito: `main.ts`
 * lo cablea en once sitios (contextos, combate, mazmorra, F7, menú del shell) y ninguno
 * se ha tocado. Lo que cambió es lo de dentro — de `HTMLAudioElement` + `setInterval` a
 * un `AudioWorkletNode` por pista con el cruce agendado en el reloj de audio.
 */
import { OplMusicEngine, type MusicEngine } from "./opl/engine.js";

/**
 * Id de canción del driver del parche: el índice EXACTO de la tabla de punteros de
 * `mid.drv` 0x20, que su rutina de reproducción indexa con `bx = [0x120 + al*2]`.
 * Ver `extractor/src/audio/tracklist.ts` (mismo orden) y `re/notes/music-location-mapping.md` §2.
 */
export type SongId = 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15;

/** Ruta de la pista por id de canción. El índice ES el id (no reordenar). */
export const SONG_TRACK: readonly string[] = [
  "/assets/music/theme.mid", // 0x0 U5THEME  — Ultima V Theme
  "/assets/music/britannia.mid", // 0x1 BRITLAND — Britannic Lands
  "/assets/music/hornpipe.mid", // 0x2 HORNPIPE — Cap'n Johne's Hornpipe
  "/assets/music/engagement.mid", // 0x3 ENGGMNT  — Engagement and Melee
  "/assets/music/stones.mid", // 0x4 STONES   — Stones
  "/assets/music/greyson.mid", // 0x5 GREYSON  — Greyson's Tale
  "/assets/music/fanfare.mid", // 0x6 FANFARE  — Fanfare for the Virtuous
  "/assets/music/monarch.mid", // 0x7 MONARCH  — The Missing Monarch
  "/assets/music/tarantella.mid", // 0x8 TRNTLLA  — Villager Tarantella
  "/assets/music/halls.mid", // 0x9 HALLS    — Halls of Doom
  "/assets/music/worlds-below.mid", // 0xa WRLDBLW  — Worlds Below
  "/assets/music/blackthorn.mid", // 0xb BLCKTHRN — Lord Blackthorn
  "/assets/music/ladynan.mid", // 0xc LADYNAN  — Dream of Lady Nan
  "/assets/music/reunion.mid", // 0xd REUNION  — Joyous Reunion
  "/assets/music/rule-britannia.mid", // 0xe RULEBRIT — Rule Britannia
  "/assets/music/amiga.mid", // 0xf AMIGA    — Amiga Theme
];

/**
 * Contexto de música. Se parte en DOS FAMILIAS porque el driver del parche las trata
 * distinto, y la partición no es nuestra: es la suya.
 *
 *  · **Localización** (selector 0x00 del driver): la canción la DERIVA el propio driver de
 *    la posición, en cada sondeo de teclado. Estos contextos nombran un SITIO, y quien los
 *    elige es [[contextForLocation]] — nadie debería pedirlos a mano.
 *  · **Guionizadas** (selectores 0x09/0x0c/0x12/0x15/0x1b y las dos tablas de rango): la
 *    escena IMPONE una canción y el driver deja de mirar la posición (bandera `[0x11f]`).
 *    Estos contextos nombran la ESCENA, y donde la escena no tiene más nombre que su
 *    canción (las páginas de la intro, los cuadros del endgame) se nombran por el tramo:
 *    `intro-halls` = «el tramo de la intro que suena a Halls of Doom».
 */
export type MusicContext =
  // ── Familia 1: localización (sel 0x00) ───────────────────────────────────────────
  | "overworld"
  | "underworld"
  | "frigate"
  | "cities"
  | "lighthouse"
  | "hut"
  | "castle"
  | "blackthorn"
  | "village"
  | "keep"
  | "principle"
  | "dungeon"
  | "combat"
  | "victory"
  // ── Familia 2: escenas guionizadas (selectores propios) ──────────────────────────
  | "title"
  | "creation"
  | "shrine"
  | "camp"
  | "intro-stones"
  | "intro-halls"
  | "intro-greyson"
  | "endgame-stones"
  | "endgame-ladynan"
  | "reunion"
  | "finale"
  | "silence";

/**
 * Contexto → id de canción. `null` = SILENCIO (el `mov al,0xff` del driver, que su rutina
 * de reproducción trata como no-op y su selector 0x03 como parada).
 *
 * Las filas de la familia «localización» salen UNA A UNA del switch de `mid.drv` 0x016d;
 * las guionizadas, del selector que invoca cada escena. Tabla completa con el disasm al
 * lado en `re/notes/music-location-mapping.md` §1.2/§1.4.
 */
const CONTEXT_SONG: Record<MusicContext, SongId | null> = {
  // sel 0x00 — el switch por g_location
  overworld: 0x1, // loc 0, g_floor == 0
  underworld: 0xa, // loc 0, g_floor != 0
  frigate: 0x2, // (g_transport_tile & 0xf8) == 0x20, gana a la localización
  cities: 0x8, // loc 0x01..0x08
  lighthouse: 0xc, // loc 0x09..0x0c
  hut: 0x5, // loc 0x0d..0x10
  castle: 0x7, // loc 0x11
  blackthorn: 0xb, // loc 0x12
  village: 0x5, // loc 0x13..0x18
  keep: 0xc, // loc 0x19..0x1d
  principle: 0x6, // loc 0x1e..0x20 (Lycaeum · Empath Abbey · Serpent's Hold)
  dungeon: 0x9, // loc 0x21..0x28
  combat: 0x3, // loc 0xff, g_cmb_victory_flag == 0
  victory: 0x0, // loc 0xff, g_cmb_victory_flag != 0
  // Selectores guionizados
  title: 0x0, // sel 0x09 (menú de portada)
  creation: 0xf, // sel 0x0c (creación de personaje)
  shrine: 0x4, // sel 0x12 (santuarios)
  camp: 0x4, // sel 0x12 (acampada / hole-up)
  "intro-stones": 0x4, // sel 0x06, tabla 0x223: páginas 0..7
  "intro-halls": 0x9, // sel 0x06, tabla 0x223: páginas 8..0x0e
  "intro-greyson": 0x5, // sel 0x06, tabla 0x223: páginas 0x0f..0x15
  "endgame-stones": 0x4, // sel 0x18, tabla 0x24a: cuadros 0..3
  "endgame-ladynan": 0xc, // sel 0x18, tabla 0x24a: cuadros 4..7
  reunion: 0xd, // sel 0x15
  finale: 0xe, // sel 0x1b (Rule Britannia cierra el endgame)
  silence: null, // sel 0x03
};

/** Contextos de la familia «guionizada»: CONGELAN la música (bandera `[0x11f]` a 0). */
const SCRIPTED: ReadonlySet<MusicContext> = new Set<MusicContext>([
  "title",
  "creation",
  "shrine",
  "camp",
  "intro-stones",
  "intro-halls",
  "intro-greyson",
  "endgame-stones",
  "endgame-ladynan",
  "reunion",
  "finale",
  "silence",
]);

/** Posición del party, tal como la lee el switch del driver. */
export interface LocationMusicInput {
  /** `g_location` (DS:0x5893). */
  location: number;
  /** `g_floor` (DS:0x5895); 0xff = Underworld. */
  floor: number;
  /** `g_transport_tile` (DS:0x587C). Ausente = a pie (0x1c). */
  transportTile?: number;
  /** `g_cmb_victory_flag` (DS:0x58A3): el «VICTORY!» ya anunciado. */
  combatVictory?: boolean;
  /** ¿Estamos en el arnés de combate? (el driver lo ve como `g_location == 0xff`). */
  inCombat?: boolean;
}

/**
 * EL SWITCH DEL DRIVER, PORTADO LÍNEA A LÍNEA — `mid.drv` 0x016d.
 *
 * 🔴 NO ES CRITERIO NUESTRO NI UNA TABLA INVENTADA: durante años esto fue «pueblos →
 * Stones» para veintiocho sitios distintos, con una nota que lo rotulaba honestamente como
 * criterio propio porque el mapeo «no cedió al análisis estático». Cedió el 2026-09-12, y
 * por donde no se estaba mirando: **la decisión no vive en `ULTIMA.EXE` sino DENTRO del
 * propio `mid.drv`**, que recibe el segmento de datos del juego en BX y lee `g_location`,
 * `g_floor`, `g_transport_tile` y `g_cmb_victory_flag` él mismo. Por eso no aparecía ni
 * tabla plana ni constantes `mov al,N` agrupadas en el ejecutable: no están ahí.
 *
 * El orden de las ramas ES el del binario y NO es intercambiable: el centinela de combate
 * gana a todo, y la FRAGATA gana a la localización (navegar tapa la música del sitio).
 *
 * Fuera de 0x00..0x28 y 0xff el driver carga 0xFF = callar (el modo demo 0x40 y el endgame
 * 0x42 caen aquí, y por eso sus escenas necesitan selector propio).
 *
 * @returns id de canción, o `null` para «callar».
 */
export function songForLocation(pos: LocationMusicInput): SongId | null {
  const loc = pos.location;
  // 0185: cmp bl,0xff — centinela de combate (lo escribe ULTIMA.EXE 0x5fb4 al armar la arena)
  if (loc === 0xff || pos.inCombat) return pos.combatVictory ? 0x0 : 0x3;
  // 019d: and bh,0xf8 / cmp bh,0x20 — familia FRAGATA (velas izadas 0x20-0x23, arriadas 0x24-0x27)
  if (((pos.transportTile ?? 0x1c) & 0xf8) === 0x20) return 0x2;
  // 01ae: cmp bl,0 — exterior, con g_floor de discriminador
  if (loc === 0) return pos.floor === 0 ? 0x1 : 0xa;
  if (loc <= 0x08) return 0x8; // 01c3 — las ocho Ciudades de la Virtud
  if (loc <= 0x0c) return 0xc; // 01cd — los cuatro faros
  if (loc <= 0x10) return 0x5; // 01d7 — las cuatro cabañas
  if (loc <= 0x11) return 0x7; // 01e1 — castillo de Lord British
  if (loc <= 0x12) return 0xb; // 01eb — palacio de Blackthorn
  if (loc <= 0x18) return 0x5; // 01f5 — los seis pueblos
  if (loc <= 0x1d) return 0xc; // 01ff — las cinco fortalezas
  if (loc <= 0x20) return 0x6; // 0209 — Liceo · Abadía · Serpent's Hold
  if (loc <= 0x28) return 0x9; // 0213 — las ocho mazmorras
  return null; // 021d — mov al,0xff
}

/**
 * El mismo switch, devuelto como CONTEXTO con nombre (para el diagnóstico y los tests).
 * Deriva del id, así que no puede desalinearse de [[songForLocation]].
 */
export function contextForLocation(pos: LocationMusicInput): MusicContext {
  const loc = pos.location;
  if (loc === 0xff || pos.inCombat) return pos.combatVictory ? "victory" : "combat";
  if (((pos.transportTile ?? 0x1c) & 0xf8) === 0x20) return "frigate";
  if (loc === 0) return pos.floor === 0 ? "overworld" : "underworld";
  if (loc <= 0x08) return "cities";
  if (loc <= 0x0c) return "lighthouse";
  if (loc <= 0x10) return "hut";
  if (loc <= 0x11) return "castle";
  if (loc <= 0x12) return "blackthorn";
  if (loc <= 0x18) return "village";
  if (loc <= 0x1d) return "keep";
  if (loc <= 0x20) return "principle";
  if (loc <= 0x28) return "dungeon";
  return "silence";
}

/**
 * Tabla de rango de la INTRO — `mid.drv` 0x223 (selector 0x06), indexada por la página de
 * la cinemática que INTRO.OVL 0x0adc pasa en BL. Fuera de 0x15 el driver calla.
 */
export function introPageContext(page: number): MusicContext {
  if (page <= 0x07) return "intro-stones";
  if (page <= 0x0e) return "intro-halls";
  if (page <= 0x15) return "intro-greyson";
  return "silence";
}

/**
 * Tabla de rango del ENDGAME — `mid.drv` 0x24a (selector 0x18), indexada por el cuadro que
 * ENDGAME.OVL 0x0aee pasa en BL. Fuera de 7 el driver calla.
 */
export function endgameSceneContext(scene: number): MusicContext {
  if (scene <= 0x03) return "endgame-stones";
  if (scene <= 0x07) return "endgame-ladynan";
  return "silence";
}

/** Volumen por defecto (el VOLUME=0.55 histórico). */
const DEFAULT_VOLUME = 0.55;
/** Clave del volumen persistente de la música (string decimal 0..1). */
export const MUSIC_VOLUME_KEY = "u5.musicVolume";

/** Lee el volumen persistido (0..1); ausente/basura ⇒ default. Nunca lanza. */
export function musicVolume(
  store: Pick<Storage, "getItem"> | undefined = safeStorage(),
): number {
  try {
    const raw = store?.getItem(MUSIC_VOLUME_KEY);
    if (raw == null) return DEFAULT_VOLUME;
    const v = Number(raw);
    if (!Number.isFinite(v)) return DEFAULT_VOLUME;
    return Math.min(1, Math.max(0, v));
  } catch {
    return DEFAULT_VOLUME;
  }
}

/**
 * Clave del toggle persistente de la música (localStorage). PRESENTE ⇒ preferencia
 * EXPLÍCITA del usuario (F7), que MANDA sobre el default del perfil de piel. AUSENTE ⇒
 * decide el default del perfil (piel fiel 1988 ⇒ OFF, piel dev ⇒ ON; ver
 * `MusicPlayer.setProfileDefault` y `re/notes/audio-profile-1988.md`).
 */
export const MUSIC_STORAGE_KEY = "u5.music";

function safeStorage(): Storage | undefined {
  try {
    return typeof localStorage !== "undefined" ? localStorage : undefined;
  } catch {
    return undefined;
  }
}

/**
 * ¿Fijó el usuario una preferencia explícita de música con F7? Si la fijó, gana sobre el
 * default del perfil de piel; si no, manda el perfil. Nunca lanza.
 */
export function hasExplicitMusicPref(): boolean {
  try {
    return safeStorage()?.getItem(MUSIC_STORAGE_KEY) != null;
  } catch {
    return false;
  }
}

/**
 * ¿Está la música activada? Con preferencia explícita del usuario ("1"/"0") gana ésta;
 * si no hay ninguna, manda `profileDefault` (el default de la piel activa). Nunca lanza.
 */
export function musicEnabled(profileDefault: boolean): boolean {
  try {
    const v = safeStorage()?.getItem(MUSIC_STORAGE_KEY);
    if (v === "1") return true;
    if (v === "0") return false;
    return profileDefault;
  } catch {
    return profileDefault;
  }
}

/**
 * Eventos que cuentan como «gesto del usuario» para desbloquear el audio.
 *
 * 🔴 `keyup` NO ES REDUNDANTE CON `keydown`, Y ES EL QUE SALVA EL CASO NORMAL. La intro
 * fiel se adueña del teclado mientras dura la portada: `ui/faithful-intro.ts:814` engancha
 * su manejador en `window` EN FASE DE CAPTURA y llama a `stopImmediatePropagation()`
 * («el menú de portada captura TODO el teclado»). Como se registra ANTES que esta clase,
 * gana el orden y NINGÚN `keydown` llega hasta aquí.
 *
 * Y el menú de portada de Ultima V se maneja con el TECLADO (se pulsa `J` para Journey
 * Onward), así que quien no toque el ratón jamás produce un gesto que desbloquee: la
 * música se queda muda PARA SIEMPRE y el interruptor de los ajustes no hace nada
 * visible, porque `play()` sale por el gate de `unlocked` sin decir una palabra.
 *
 * MEDIDO en el navegador el 2026-09-11, con dos pulsaciones reales en la portada:
 * `keydown` 0 · `keyup` 2 · `pointerdown` 0. La intro sólo intercepta `keydown`, así
 * que `keyup` pasa limpio y cierra el agujero sin tocar la intro.
 *
 * 🔴 Y HACEN FALTA LOS GESTOS *COMPLETADOS* (`pointerup`, `touchend`, `click`), no sólo
 * los de inicio. WebKit no concede la activación de audio en `touchstart`/`pointerdown`:
 * la concede cuando el gesto TERMINA. Desbloquear sólo al empezar el toque tiene un modo
 * de fallo precioso — el `AudioContext` se crea, no lanza ningún error, y se queda
 * `suspended`: silencio absoluto sin una sola pista de por qué. En el iPhone del usuario
 * eso daba música dentro de la partida (donde hay toques de sobra y alguno cuaja) y NUNCA
 * en la portada, donde el único toque es el de «Tap to continue».
 */
const UNLOCK_EVENTS = [
  // Gestos COMPLETADOS. Van primero porque son los únicos que WebKit acepta como
  // activación para audio (ver abajo).
  "pointerup",
  "touchend",
  "click",
  "keyup",
  // Gestos de INICIO: valen en Chrome/Firefox y adelantan el desbloqueo.
  "pointerdown",
  "touchstart",
  "keydown",
] as const;

/**
 * Diagnóstico de la música, partido en trozos CORTOS.
 *
 * El control del menú SISTEMA mide ~170 px y no se puede desplazar (va deshabilitado),
 * así que una línea larga se corta y lo que se pierde es justo lo que diagnostica.
 * Cada campo cabe por sí solo y responde una pregunta distinta.
 */
export interface MusicDiagnostico {
  /** Interruptor y gesto de desbloqueo: `on unlock:Y`. */
  readonly estado: string;
  /** Pista pedida y si hay voz montada: `town voice:Y`. */
  readonly pista: string;
  /** Vía de síntesis y estado del AudioContext: `worker running`. */
  readonly via: string;
  /** Qué pasó con el tema de PORTADA: `go` | `lock` | `off` | `-`. */
  readonly portada: string;
  /** Último fallo, recortado. */
  readonly error: string;
  /**
   * Salud de la COLA de trozos: `ahead:2.7 under:0 n:41`. `ahead` = trozos (de 2 s)
   * agendados por delante de «ahora»; `under` = veces que la cola llegó vacía al reloj
   * de audio (un hueco audible, y la ÚNICA forma en que este motor puede producir uno);
   * `n` = trozos entregados. Si se oye un fallo y `under` sigue a 0, el fallo no lo
   * produjo este código: está en la salida de audio del navegador.
   */
  readonly hilo: string;
}

export class MusicPlayer {
  private readonly engine: MusicEngine;
  private currentContext: MusicContext | null = null;
  /**
   * Canción VIVA (id del driver), el espejo de `[0x11e]`. El de-duplicado va por AQUÍ y no
   * por contexto, y eso es lo que hace el driver: su `0x267` compara `al` contra `[0x11e]`,
   * o sea el ID, no el sitio. Importa porque ahora hay contextos DISTINTOS que comparten
   * canción —pueblo y cabaña son los dos «Greyson's Tale», faro y fortaleza «Lady Nan»—, y
   * con dedupe por contexto cruzar de una cabaña a un pueblo REINICIARÍA la misma pista.
   * `undefined` = nada montado todavía; `null` = silencio deliberado.
   */
  private currentSong: SongId | null | undefined = undefined;
  /**
   * Espejo de la bandera `[0x11f]` del driver: con la música CONGELADA, los refrescos por
   * localización (selector 0x00) no tocan nada y la escena conserva su canción. La ponen
   * los contextos guionizados y la quita [[resumeLocation]] (el selector 0x0f del driver).
   */
  private frozen = false;
  private unlocked = false;
  /** Retira los listeners de desbloqueo; se llama una sola vez, al desbloquear. */
  private desengancha: (() => void) | undefined;
  private pendingContext: MusicContext | null = null;
  private _enabled: boolean;
  /** Volumen vivo 0..1 (persistido en u5.musicVolume). */
  private volume = musicVolume();
  /**
   * Bitácora corta de los últimos contextos pedidos y en qué acabó cada uno.
   *
   * 🔴 SIN ESTO EL DIAGNÓSTICO LLEGA TARDE, y es el defecto que tenía la primera versión
   * del renglón «Music status»: el menú SISTEMA sólo existe DENTRO de la partida, así que
   * cuando por fin se puede leer, lo que pasó en la PORTADA ya no está en el estado vivo —
   * justo el tramo que se quiere diagnosticar. La bitácora sobrevive al cambio de pista.
   *
   *   ctx:go   → se mandó al motor (a partir de aquí manda `err:`)
   *   ctx:off  → el interruptor estaba apagado
   *   ctx:lock → aún no había gesto del usuario (autoplay bloqueado)
   */
  private readonly historial: string[] = [];

  /**
   * Desenlace del tema de PORTADA. Campo propio y no una búsqueda en `historial` porque
   * la bitácora es un anillo de 5 y las entradas de `title` se pierden en cuanto el
   * jugador cambia de zona un par de veces.
   *
   * 🔴 «PEGAJOSO EN `go`», y ése era el defecto de la primera versión: mostraba el PRIMER
   * desenlace, que en el flujo normal es SIEMPRE `lock` —`play("title")` corre en el
   * arranque, mucho antes de que el jugador toque nada— así que decía `lock` tanto cuando
   * la portada sonaba como cuando no. Un diagnóstico que da el mismo valor en los dos
   * casos no diagnostica: sólo tranquiliza. Ahora `go` gana y significa, sin ambigüedad,
   * que el tema de portada LLEGÓ al motor.
   */
  private portadaOutcome = "-";

  private anota(context: MusicContext, salida: "go" | "off" | "lock"): void {
    this.historial.push(`${context}:${salida}`);
    if (this.historial.length > 5) this.historial.shift();
    if (context === "title" && this.portadaOutcome !== "go") this.portadaOutcome = salida;
  }

  /**
   * @param profileDefault default de música del perfil de piel INICIAL: piel fiel 1988 ⇒
   *   `false` (sin música: el DOS original en PC estándar no tenía música de fondo, sólo
   *   PC-speaker — `re/notes/audio-profile-1988.md`); piel dev ⇒ `true` (música "enhanced"
   *   XMI→OPL). Sólo aplica si el usuario no fijó una preferencia explícita con F7.
   * @param engine motor de audio. Se INYECTA para poder probar perfiles y persistencia
   *   sin Web Audio (mismo patrón que el `AudioContext` inyectable de `fiel/speaker.ts`).
   */
  constructor(profileDefault = false, engine: MusicEngine = new OplMusicEngine()) {
    this.engine = engine;
    this._enabled = musicEnabled(profileDefault);
    const unlock = (): void => this.unlock();
    for (const ev of UNLOCK_EVENTS) window.addEventListener(ev, unlock);
    this.desengancha = (): void => {
      for (const ev of UNLOCK_EVENTS) window.removeEventListener(ev, unlock);
    };
  }

  /**
   * Da por hecho el gesto del usuario y reanuda el contexto pendiente. Es PÚBLICO para
   * que quien se adueñe del teclado pueda desbloquear a mano — la intro fiel ya hace
   * exactamente eso con el PC-speaker (`ensureDemoSpeaker().unlock()`), y por la misma
   * razón. Idempotente.
   */
  unlock(): void {
    if (this.unlocked) return;
    this.unlocked = true;
    this.desengancha?.();
    this.desengancha = undefined;
    if (this.pendingContext) this.play(this.pendingContext);
  }

  get enabled(): boolean {
    return this._enabled;
  }

  /** Volumen vivo (0..1) para el shell. */
  get volumeLevel(): number {
    return this.volume;
  }

  /**
   * Fija el volumen (0..1), lo persiste y lo aplica a la pista viva. QoL — no
   * toca estado ni RNG.
   */
  setVolume(v: number): void {
    this.volume = Math.min(1, Math.max(0, v));
    try {
      safeStorage()?.setItem(MUSIC_VOLUME_KEY, String(this.volume));
    } catch {
      /* almacenamiento no disponible: el volumen vive sólo en memoria */
    }
    this.engine.setVolume(this.volume);
  }

  /**
   * Enciende/apaga la música SIN persistir (uso interno + default de perfil). Al apagar,
   * detiene la pista viva pero RECUERDA el contexto (`pendingContext`) para reanudarlo.
   */
  private applyEnabled(on: boolean): void {
    this._enabled = on;
    if (!on) {
      this.engine.stop();
      // Conserva `pendingContext` = último contexto pedido; olvida el "vivo" para
      // que un play() del MISMO contexto al reencender no sea un no-op (ver play()).
      this.pendingContext = this.currentContext ?? this.pendingContext;
      this.currentContext = null;
      this.currentSong = undefined; // el dedupe va por canción: olvidarla también
    } else if (this.pendingContext) {
      this.play(this.pendingContext);
    }
  }

  /**
   * Enciende/apaga la música por acción EXPLÍCITA del usuario (F7). Persiste la elección
   * (localStorage u5.music), que desde aquí MANDA sobre el default del perfil de piel.
   * QoL — no es una tecla del original; el toggle no toca estado ni RNG del juego.
   */
  setEnabled(on: boolean): void {
    try {
      safeStorage()?.setItem(MUSIC_STORAGE_KEY, on ? "1" : "0");
    } catch {
      /* almacenamiento no disponible: el toggle vive sólo en memoria */
    }
    this.applyEnabled(on);
  }

  /**
   * Fija el default de música del PERFIL de piel activo, aplicado al cambiar de piel (F9).
   * Si el usuario fijó su preferencia con F7 (`hasExplicitMusicPref`), ésta MANDA y esto
   * es un no-op. QoL — cero estado/RNG.
   */
  setProfileDefault(defaultOn: boolean): void {
    if (hasExplicitMusicPref()) return; // el override manual del usuario gana
    this.applyEnabled(defaultOn);
  }

  toggle(): boolean {
    this.setEnabled(!this._enabled);
    return this._enabled;
  }

  /**
   * Estado de la música en UNA línea, para el renglón «Music status» del menú SISTEMA.
   *
   * Existe porque en un teléfono NO HAY CONSOLA: cuando la música no suena, el jugador
   * (y quien le ayude) no tiene forma de distinguir «apagada» de «sin gesto de
   * desbloqueo» de «el asset no está» de «el navegador bloqueó el audio». Cada tramo
   * responde una de esas preguntas, en orden de causa:
   *   on/off      · el interruptor (F7 / ajustes)
   *   unlock:Y/N  · ¿hubo ya un gesto del usuario? (política de autoplay)
   *   ctx:<nombre>· qué pista se ha pedido
   *   via:        · worker (trozos sintetizados por adelantado, fuera del hilo de audio)
   *   audio:      · estado del AudioContext
   *   voice:Y/N   · ¿hay pista montada?
   *   err:        · el último fallo, si lo hubo
   */
  get diagnostico(): MusicDiagnostico {
    const s = this.engine.status;
    // Cada trozo tiene que caber en el control del menú, que son ~20 caracteres: la
    // primera versión metía los siete campos en UNA línea y el móvil la cortaba en
    // «on unlock:Y ctx:town», escondiendo justo la mitad que diagnostica. Y un `input`
    // deshabilitado no se puede desplazar ni seleccionar, así que lo cortado era
    // irrecuperable. Trozos cortos > una línea completa que no se puede leer.
    const portada = this.portadaOutcome;
    return {
      estado: `${this._enabled ? "on" : "off"} unlock:${this.unlocked ? "Y" : "N"}`,
      pista: `${this.currentContext ?? this.pendingContext ?? "-"} voice:${s.sonando ? "Y" : "N"}`,
      via: `${s.via} ${s.audio}`,
      portada,
      error: s.error ? s.error.slice(0, 24) : "-",
      hilo: s.cola
        ? `ahead:${s.cola.ahead.toFixed(1)} under:${s.cola.underruns} n:${s.cola.chunks}`
        : "-",
    };
  }

  /** La línea entera, para la consola del navegador (ahí sí cabe). */
  get diagnosticoLargo(): string {
    const d = this.diagnostico;
    return `${d.estado} ctx:${d.pista} via:${d.via} title:${d.portada} err:${d.error} | ${this.historial.join(" ") || "-"}`;
  }

  /**
   * Contexto según posición — el switch del driver, ya derivado. Delega en
   * [[contextForLocation]]; se conserva como método por los llamadores de siempre.
   */
  contextFor(location: number, floor: number, transportTile?: number): MusicContext {
    return contextForLocation({ location, floor, transportTile });
  }

  /**
   * Refresco por LOCALIZACIÓN — el selector 0x00 del driver, que el original dispara en
   * CADA sondeo de teclado. Con la música congelada por una escena guionizada no hace
   * nada, igual que el driver con su bandera `[0x11f]` a cero.
   */
  playLocation(pos: LocationMusicInput): void {
    if (this.frozen) return;
    this.play(contextForLocation(pos));
  }

  /** ¿Hay una escena guionizada dueña de la música? (espejo de `[0x11f] == 0`). */
  get isFrozen(): boolean {
    return this.frozen;
  }

  /**
   * Devuelve el mando a la localización — el selector 0x0f del driver, que toda escena
   * guionizada del parche invoca al terminar (glue 0x0e1d de `ULTIMA.EXE`). Con `pos` se
   * aplica el refresco en el acto, que es lo que hace el siguiente sondeo de teclado.
   */
  resumeLocation(pos?: LocationMusicInput): void {
    this.frozen = false;
    if (pos) this.playLocation(pos);
  }

  play(context: MusicContext): void {
    // La escena guionizada se adueña de la música ANTES de cualquier corte por dedupe o
    // por el interruptor: si no, apagar y encender la música dentro de una escena la
    // devolvería al modo localización a mitad de cinemática.
    if (SCRIPTED.has(context)) this.frozen = true;
    const song = CONTEXT_SONG[context];
    // De-duplicado por CANCIÓN (ver `currentSong`): el sitio puede cambiar sin que cambie
    // la pista, y en ese caso el driver la deja sonar en vez de reiniciarla.
    if (this.currentSong !== undefined && song === this.currentSong) {
      this.currentContext = context;
      this.pendingContext = context;
      return;
    }
    this.pendingContext = context;
    // Música apagada (F7): recuerda el contexto pedido pero no suena; se reanudará
    // en setEnabled(true). Igual que el gate de `unlocked`, no altera nada del juego.
    if (!this._enabled) {
      this.anota(context, "off");
      return;
    }
    if (!this.unlocked) {
      this.anota(context, "lock");
      return;
    }
    this.currentContext = context;
    this.currentSong = song;
    this.anota(context, "go");
    // `silence` es el `mov al,0xff` del driver: parar, no reproducir nada.
    if (song === null) this.engine.stop();
    else this.engine.play(SONG_TRACK[song]!, this.volume);
  }
}
