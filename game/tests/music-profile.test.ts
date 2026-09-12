/**
 * Perfil de audio por piel (task #27, `re/notes/audio-profile-1988.md`).
 *
 * DERIVACIÓN: el DOS 1988 en un PC estándar NO tiene música de fondo (sólo PC-speaker)
 * ⇒ la piel fiel 1988 debe arrancar SIN música; la piel dev conserva la música "enhanced"
 * (XMI→OPL). F7 es un opt-in EXPLÍCITO que persiste y MANDA sobre el default del perfil.
 *
 * Estos tests son DISCRIMINANTES: fallan si alguien vuelve a un default fijo (p.ej. ON para
 * todas las pieles) o si el override de F7 deja de ganar al default del perfil.
 *
 * ⚠️ SE PRUEBA CONTRA UN MOTOR DE MENTIRA, no contra Web Audio. Hasta el 2026-09-11 estos
 * tests espiaban `HTMLAudioElement` (`new Audio(src)`, `.play()`, `.loop`) porque la música
 * era OGG pregrabado. Al pasar a síntesis OPL en un `AudioWorklet` eso dejó de existir, y
 * el sustituto NO es «probar el worklet»: lo que este fichero protege es la política de
 * perfiles y persistencia, que no tiene nada que ver con cómo suena. El motor se INYECTA
 * (`MusicEngine`), igual que `fiel/speaker.ts` inyecta su `AudioContext`. Cada aserto de la
 * versión anterior sigue aquí, mirando al doble en vez de al elemento de audio.
 */
import { describe, expect, it, beforeEach, afterEach, vi } from "vitest";
import {
  MusicPlayer,
  MUSIC_STORAGE_KEY,
  musicEnabled,
  hasExplicitMusicPref,
  songForLocation,
  introPageContext,
  endgameSceneContext,
  SONG_TRACK,
  type MusicContext,
} from "../src/ui/music.js";
import type { MusicEngine } from "../src/ui/opl/engine.js";

class LocalStorageStub {
  private store = new Map<string, string>();
  getItem(k: string): string | null {
    return this.store.has(k) ? this.store.get(k)! : null;
  }
  setItem(k: string, v: string): void {
    this.store.set(k, String(v));
  }
  removeItem(k: string): void {
    this.store.delete(k);
  }
  clear(): void {
    this.store.clear();
  }
  key(i: number): string | null {
    return [...this.store.keys()][i] ?? null;
  }
  get length(): number {
    return this.store.size;
  }
}

/** Doble de `MusicEngine`: apunta qué se pidió sonar, con qué volumen, y si se paró. */
class FakeEngine implements MusicEngine {
  readonly played: string[] = [];
  readonly volumes: number[] = [];
  stops = 0;
  play(url: string, volume: number): void {
    this.played.push(url);
    this.volumes.push(volume);
  }
  stop(): void {
    this.stops++;
  }
  setVolume(volume: number): void {
    this.volumes.push(volume);
  }
  get status(): { via: "-"; audio: string; sonando: boolean; error: null; cola: null } {
    return { via: "-", audio: "fake", sonando: this.sonando, error: null, cola: null };
  }
  /** ¿Suena algo ahora mismo? (hubo un play posterior al último stop). */
  get sonando(): boolean {
    return this.played.length > 0 && this.stops === 0;
  }
}

interface Env {
  engine: FakeEngine;
  /** Simula el primer gesto del usuario (desbloqueo de autoplay del navegador). */
  unlock(): void;
}

function installEnv(): Env & { fire(evento: string): void; tipos(): string[] } {
  const listeners: Record<string, Array<() => void>> = {};
  const win = {
    addEventListener: (t: string, cb: () => void): void => {
      (listeners[t] ??= []).push(cb);
    },
    removeEventListener: (t: string, cb: () => void): void => {
      listeners[t] = (listeners[t] ?? []).filter((f) => f !== cb);
    },
  };
  vi.stubGlobal("window", win);
  vi.stubGlobal("localStorage", new LocalStorageStub());
  const fire = (evento: string): void =>
    [...(listeners[evento] ?? [])].forEach((f) => f());
  return {
    engine: new FakeEngine(),
    unlock: () => fire("keydown"),
    fire,
    /** Tipos de evento con al menos un listener enganchado. */
    tipos: () => Object.keys(listeners).filter((t) => (listeners[t] ?? []).length > 0),
  };
}

beforeEach(() => {
  installEnv();
});
afterEach(() => {
  vi.unstubAllGlobals();
});

describe("musicEnabled() — el perfil decide salvo preferencia explícita", () => {
  it("sin preferencia guardada, devuelve el default del perfil", () => {
    expect(musicEnabled(false)).toBe(false); // fiel
    expect(musicEnabled(true)).toBe(true); // dev
    expect(hasExplicitMusicPref()).toBe(false);
  });

  it("con preferencia explícita, ésta gana al default del perfil", () => {
    localStorage.setItem(MUSIC_STORAGE_KEY, "1");
    expect(musicEnabled(false)).toBe(true); // fiel default OFF, pero el usuario forzó ON
    expect(hasExplicitMusicPref()).toBe(true);
    localStorage.setItem(MUSIC_STORAGE_KEY, "0");
    expect(musicEnabled(true)).toBe(false); // dev default ON, pero el usuario forzó OFF
  });
});

describe("piel fiel 1988 — arranca SIN música", () => {
  it("no arranca ninguna pista al pedir contexto (default OFF)", () => {
    const env = installEnv();
    const m = new MusicPlayer(false, env.engine);
    env.unlock();
    m.play("overworld");
    expect(m.enabled).toBe(false);
    expect(env.engine.played).toHaveLength(0);
  });

  it("F7 (opt-in explícito) SÍ arranca música y persiste la preferencia", () => {
    const env = installEnv();
    const m = new MusicPlayer(false, env.engine);
    env.unlock();
    const on = m.toggle();
    expect(on).toBe(true);
    expect(localStorage.getItem(MUSIC_STORAGE_KEY)).toBe("1");
    m.play("village");
    expect(env.engine.played).toEqual(["/assets/music/greyson.mid"]);
  });
});

describe("piel dev — arranca CON música", () => {
  it("arranca la pista del contexto (default ON)", () => {
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    env.unlock();
    m.play("overworld");
    expect(m.enabled).toBe(true);
    expect(env.engine.played).toEqual(["/assets/music/britannia.mid"]);
  });

  it("🔴 nada suena antes del primer gesto del usuario (autoplay bloqueado)", () => {
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    m.play("overworld"); // sin unlock
    expect(env.engine.played).toHaveLength(0);
    env.unlock(); // el gesto reanuda el contexto recordado
    expect(env.engine.played).toEqual(["/assets/music/britannia.mid"]);
  });
});

describe("setProfileDefault() — cambio de piel (F9) reaplica el default", () => {
  it("dev→fiel detiene la música; fiel→dev la reanuda (sin preferencia explícita)", () => {
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine); // arranca en dev
    env.unlock();
    m.play("overworld");
    expect(env.engine.sonando).toBe(true);

    m.setProfileDefault(false); // swap a fiel ⇒ sin música
    expect(m.enabled).toBe(false);
    expect(env.engine.stops).toBe(1);

    m.setProfileDefault(true); // swap de vuelta a dev ⇒ reanuda el contexto recordado
    expect(m.enabled).toBe(true);
    expect(env.engine.played).toEqual([
      "/assets/music/britannia.mid",
      "/assets/music/britannia.mid",
    ]);
  });

  it("la preferencia explícita del usuario (F7) GANA al default del perfil en el swap", () => {
    const env = installEnv();
    localStorage.setItem(MUSIC_STORAGE_KEY, "1"); // el usuario forzó música ON
    const m = new MusicPlayer(false, env.engine); // aunque arranque en fiel
    expect(m.enabled).toBe(true);
    m.setProfileDefault(false); // swap a fiel: NO debe apagar (override manual manda)
    expect(m.enabled).toBe(true);
    env.unlock();
    m.play("overworld");
    expect(env.engine.sonando).toBe(true);
  });
});

describe("🔴 desbloqueo de autoplay — la intro fiel se come los keydown", () => {
  /**
   * REGRESIÓN de un defecto REAL, medido en navegador el 2026-09-11: en la portada,
   * `ui/faithful-intro.ts:814` engancha su manejador de teclado en `window` EN CAPTURA y
   * llama a `stopImmediatePropagation()`, así que ni un `keydown` llega a `MusicPlayer`.
   * Sonda con dos pulsaciones reales: keydown 0 · keyup 2 · pointerdown 0.
   *
   * Y el menú de portada de Ultima V se navega con el teclado (`J` = Journey Onward), así
   * que quien no toque el ratón no desbloqueaba NUNCA: música muda para siempre y el
   * interruptor de los ajustes sin efecto visible. Si alguien vuelve a dejar el desbloqueo
   * sólo en `keydown`/`pointerdown`, este test se pone rojo.
   */
  it("desbloquea con keyup, que es el que SÍ sobrevive a la intro", () => {
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    m.play("overworld"); // el juego pide contexto; aún no hay gesto
    expect(env.engine.played).toHaveLength(0);
    env.fire("keyup"); // única señal de teclado que la intro no intercepta
    expect(env.engine.played).toEqual(["/assets/music/britannia.mid"]);
  });

  it("engancha el desbloqueo a keyup además de keydown/pointerdown", () => {
    const env = installEnv();
    new MusicPlayer(true, env.engine);
    expect(env.tipos()).toContain("keyup");
    expect(env.tipos()).toContain("pointerdown");
  });

  it("unlock() es público e idempotente (la intro puede llamarlo a mano)", () => {
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    m.play("village");
    m.unlock();
    m.unlock();
    m.unlock();
    expect(env.engine.played).toEqual(["/assets/music/greyson.mid"]);
  });

  it("un gesto cualquiera vale: pointerdown también desbloquea", () => {
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    m.play("overworld");
    env.fire("pointerdown");
    expect(env.engine.played).toEqual(["/assets/music/britannia.mid"]);
  });
});

describe("diagnóstico legible sin consola (renglón «Music status»)", () => {
  /**
   * En un teléfono no hay consola, y «no suena» tiene media docena de causas. La línea
   * tiene que distinguirlas, y la BITÁCORA tiene que sobrevivir al cambio de pista: el
   * menú SISTEMA sólo existe dentro de la partida, así que lo que pasó en la PORTADA hay
   * que poder leerlo DESPUÉS.
   */
  it("distingue apagada de bloqueada por autoplay", () => {
    const env = installEnv();
    const apagada = new MusicPlayer(false, env.engine);
    apagada.play("title");
    expect(apagada.diagnostico.estado).toContain("off");
    expect(apagada.diagnostico.portada).toBe("off");

    const env2 = installEnv();
    const bloqueada = new MusicPlayer(true, env2.engine);
    bloqueada.play("title");
    expect(bloqueada.diagnostico.estado).toContain("unlock:N");
    expect(bloqueada.diagnostico.portada).toBe("lock");
  });

  it("🔴 si el gesto llega YA en la partida, la portada nunca sonó (`lock`)", () => {
    // Es el caso del usuario en el iPhone: ningún gesto válido durante la portada, así
    // que `play("title")` se queda en el gate y el primer `go` es ya de otra pista.
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    m.play("title"); // portada: sin gesto ⇒ lock
    m.play("village"); // el juego arranca y pide otra pista, todavía sin gesto
    env.fire("pointerup"); // el jugador toca YA dentro de la partida
    expect(m.diagnostico.portada).toBe("lock");
    expect(m.diagnostico.pista).toContain("village");
  });

  it("🔴 cada trozo CABE en el control del menú (~20 caracteres)", () => {
    // Un renglón que no cabe se corta sin aviso y se lleva justo lo que diagnostica.
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    m.play("blackthorn");
    env.fire("pointerdown");
    const d = m.diagnostico;
    for (const [campo, valor] of Object.entries(d)) {
      expect(valor.length, `${campo} = "${valor}" no cabe`).toBeLessThanOrEqual(24);
    }
  });

  it("🔴 la bitácora conserva lo que pasó en la PORTADA al llegar a la partida", () => {
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    m.play("title"); // portada: sin gesto todavía ⇒ bloqueada
    env.fire("pointerdown"); // el jugador toca ⇒ se reanuda «title»
    m.play("village"); // ya dentro de la partida
    // 🔴 `portada` es PEGAJOSO EN `go`: una vez que el tema de portada llegó al motor,
    // el campo lo dice aunque después suene otra pista. La primera versión mostraba el
    // PRIMER desenlace, que en el flujo normal es SIEMPRE `lock` (play("title") corre en
    // el arranque, antes de que nadie toque nada) — o sea, decía lo mismo cuando la
    // portada sonaba y cuando no. Ese campo no distinguía nada.
    expect(m.diagnostico.portada).toBe("go");
    expect(m.diagnostico.pista).toContain("village");
    expect(m.diagnostico.estado).toContain("unlock:Y");
    // La línea larga (consola) sí lleva la bitácora entera.
    expect(m.diagnosticoLargo).toContain("title:lock");
    expect(m.diagnosticoLargo).toContain("title:go");
    expect(m.diagnosticoLargo).toContain("village:go");
  });
});

describe("contextos", () => {
  it("pedir el MISMO contexto dos veces no relanza la pista", () => {
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    env.unlock();
    m.play("village");
    m.play("village");
    expect(env.engine.played).toHaveLength(1);
  });

  it("🔴 el de-duplicado va por CANCIÓN, no por contexto", () => {
    /**
     * Es el `cmp al,[0x11e]` del driver (`mid.drv` 0x267): compara el ID, no el sitio.
     * Importa porque hay contextos distintos que COMPARTEN canción — cabaña y pueblo son
     * los dos «Greyson's Tale», faro y fortaleza «Lady Nan» —, y de-duplicar por contexto
     * haría que salir de la cabaña de Iolo a Paws REINICIARA la misma pista.
     */
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    env.unlock();
    m.play("hut");
    m.play("village");
    expect(env.engine.played).toEqual(["/assets/music/greyson.mid"]);
    m.play("lighthouse");
    m.play("keep");
    expect(env.engine.played).toEqual([
      "/assets/music/greyson.mid",
      "/assets/music/ladynan.mid",
    ]);
  });

  /**
   * 🔴 LA TABLA DEL DRIVER, FILA A FILA — `mid.drv` 0x016d.
   *
   * Ninguna de estas filas es criterio nuestro. Durante mucho tiempo TODA localización no
   * especial caía en «Stones» (veintiocho sitios con la misma canción) porque el mapeo se
   * daba por no derivable: se había buscado en `ULTIMA.EXE` —tabla plana, constantes
   * `mov al,N`, llamadas desde los overlays— y no estaba. No estaba porque **la decisión
   * vive DENTRO de `mid.drv`**, que recibe el segmento de datos del juego en BX y lee
   * `g_location` él mismo. Derivación completa: `re/notes/music-location-mapping.md`.
   */
  it("🔴 songForLocation reproduce el switch del driver en TODO su dominio", () => {
    const song = (location: number, floor = 0): number | null =>
      songForLocation({ location, floor });
    expect(song(0, 0)).toBe(0x1); // Britannia  → Britannic Lands
    expect(song(0, 0xff)).toBe(0xa); // Underworld → Worlds Below
    for (let l = 0x01; l <= 0x08; l++) expect(song(l), `loc ${l}`).toBe(0x8); // 8 ciudades
    for (let l = 0x09; l <= 0x0c; l++) expect(song(l), `loc ${l}`).toBe(0xc); // 4 faros
    for (let l = 0x0d; l <= 0x10; l++) expect(song(l), `loc ${l}`).toBe(0x5); // 4 cabañas
    expect(song(0x11)).toBe(0x7); // castillo de Lord British → The Missing Monarch
    expect(song(0x12)).toBe(0xb); // palacio de Blackthorn
    for (let l = 0x13; l <= 0x18; l++) expect(song(l), `loc ${l}`).toBe(0x5); // 6 pueblos
    for (let l = 0x19; l <= 0x1d; l++) expect(song(l), `loc ${l}`).toBe(0xc); // 5 fortalezas
    for (let l = 0x1e; l <= 0x20; l++) expect(song(l), `loc ${l}`).toBe(0x6); // Liceo/Abadía/Serpent
    for (let l = 0x21; l <= 0x28; l++) expect(song(l), `loc ${l}`).toBe(0x9); // 8 mazmorras
    // Fuera del dominio el driver carga 0xFF = callar (demo 0x40, endgame 0x42…).
    expect(song(0x29)).toBeNull();
    expect(song(0x40)).toBeNull();
    expect(song(0x42)).toBeNull();
  });

  it("🔴 la FRAGATA tapa la música del sitio (y sólo la fragata)", () => {
    // 019d: `and bh,0xf8 / cmp bh,0x20` — la familia 0x20..0x27 (velas izadas Y arriadas).
    // El esquife (0x28+), el caballo (0x12/0x13) y la alfombra (0x14/0x15) NO tienen rama.
    for (let tile = 0x20; tile <= 0x27; tile++) {
      expect(
        songForLocation({ location: 0, floor: 0, transportTile: tile }),
        `tile ${tile}`,
      ).toBe(0x2);
    }
    for (const tile of [0x1c, 0x12, 0x14, 0x28, 0x2b]) {
      expect(
        songForLocation({ location: 0, floor: 0, transportTile: tile }),
        `tile ${tile}`,
      ).toBe(0x1);
    }
    // Gana incluso dentro de una localización (atracado en el muelle de una ciudad).
    expect(songForLocation({ location: 2, floor: 0, transportTile: 0x24 })).toBe(0x2);
  });

  it("🔴 el centinela de combate gana a todo, y el flag de victoria lo cambia", () => {
    // 0185: `cmp bl,0xff` es la PRIMERA comparación del switch; 018a mira g_cmb_victory_flag.
    expect(songForLocation({ location: 0xff, floor: 0 })).toBe(0x3); // Engagement and Melee
    expect(songForLocation({ location: 0xff, floor: 0, combatVictory: true })).toBe(0x0);
    // A bordo y en combate: manda el combate (el centinela se compara antes que el tile).
    expect(
      songForLocation({ location: 0, floor: 0, transportTile: 0x24, inCombat: true }),
    ).toBe(0x3);
  });

  it("contextFor mapea posición → contexto", () => {
    const m = new MusicPlayer(false, new FakeEngine());
    expect(m.contextFor(0, 0)).toBe("overworld");
    expect(m.contextFor(0, 0xff)).toBe("underworld");
    expect(m.contextFor(11, 0)).toBe("lighthouse"); // Greyhaven
    expect(m.contextFor(13, 0)).toBe("hut"); // cabaña de Iolo
    expect(m.contextFor(17, 0)).toBe("castle");
    expect(m.contextFor(18, 0)).toBe("blackthorn");
    expect(m.contextFor(22, 0)).toBe("village"); // Paws
    expect(m.contextFor(29, 0)).toBe("keep"); // Stonegate
    expect(m.contextFor(30, 0)).toBe("principle"); // The Lycaeum
    expect(m.contextFor(33, 0)).toBe("dungeon");
    expect(m.contextFor(2, 0, 0x24)).toBe("frigate"); // Britain, pero a bordo
  });

  it("🔴 las OCHO Ciudades de la Virtud suenan a Villager Tarantella", () => {
    /**
     * DOBLEMENTE fundado: el Readme del parche dice «Villager Terantella is played in all
     * eight Cities of Virtue in the C128/AppleII versions», y el switch del driver lo
     * confirma byte a byte (01c3: `cmp bl,8 / ja / mov al,8`). Es la única fila del mapeo
     * que tenía fuente ANTES de leer `mid.drv`.
     */
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    env.unlock();
    for (let loc = 1; loc <= 8; loc++) {
      expect(m.contextFor(loc, 0), `location ${loc}`).toBe("cities");
    }
    m.play("cities");
    expect(env.engine.played).toEqual(["/assets/music/tarantella.mid"]);
  });

  it("🔴 «Stones» NO es la música genérica de población", () => {
    // Tenía selector PROPIO en el driver (0x12) y eso ya hacía sospechar; el switch lo
    // confirma: `mov al,4` NO aparece en NINGUNA rama de localización. Stones es de las
    // escenas —santuario, acampada, intro, endgame— y de ningún sitio del mapa.
    for (let loc = 0; loc <= 0x28; loc++) {
      expect(songForLocation({ location: loc, floor: 0 }), `loc ${loc}`).not.toBe(0x4);
    }
  });

  it("🔴 las tablas de rango de intro y endgame (selectores 0x06 y 0x18)", () => {
    // mid.drv 0x223: 0..7 Stones · 8..0x0e Halls · 0x0f..0x15 Greyson · resto callar.
    expect(introPageContext(0)).toBe("intro-stones");
    expect(introPageContext(7)).toBe("intro-stones");
    expect(introPageContext(8)).toBe("intro-halls");
    expect(introPageContext(0x0e)).toBe("intro-halls");
    expect(introPageContext(0x0f)).toBe("intro-greyson");
    expect(introPageContext(0x15)).toBe("intro-greyson");
    expect(introPageContext(0x16)).toBe("silence");
    // mid.drv 0x24a: 0..3 Stones · 4..7 Lady Nan · resto callar.
    expect(endgameSceneContext(0)).toBe("endgame-stones");
    expect(endgameSceneContext(3)).toBe("endgame-stones");
    expect(endgameSceneContext(4)).toBe("endgame-ladynan");
    expect(endgameSceneContext(7)).toBe("endgame-ladynan");
    expect(endgameSceneContext(8)).toBe("silence");
  });

  it("🔴 una escena guionizada CONGELA la música; resumeLocation devuelve el mando", () => {
    /**
     * Es la bandera `[0x11f]` del driver. Sin ella, el refresco por localización —que en
     * el original corre en CADA sondeo de teclado— pisaría la canción de la escena a
     * mitad de cinemática.
     */
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    env.unlock();
    m.play("shrine");
    expect(m.isFrozen).toBe(true);
    m.playLocation({ location: 0, floor: 0 }); // el sobremundo NO puede pisar el rito
    expect(env.engine.played).toEqual(["/assets/music/stones.mid"]);
    m.resumeLocation({ location: 0, floor: 0 });
    expect(m.isFrozen).toBe(false);
    expect(env.engine.played).toEqual([
      "/assets/music/stones.mid",
      "/assets/music/britannia.mid",
    ]);
  });

  it("«silence» PARA la pista (el `mov al,0xff` del driver)", () => {
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    env.unlock();
    m.play("overworld");
    const stopsAntes = env.engine.stops;
    m.play("silence");
    expect(env.engine.stops).toBe(stopsAntes + 1);
    expect(env.engine.played).toHaveLength(1); // no se pidió ninguna pista nueva
  });

  it("🔴 LAS DIECISÉIS canciones del parche suenan en algún sitio", () => {
    /**
     * El cierre del arreglo, y lo que el usuario pidió: «que cada canción suene en el
     * momento/sitio que le toca». Antes sonaban DIEZ de dieciséis —Monarch, Fanfare, Lady
     * Nan, Greyson, Hornpipe, Amiga, Rule Britannia y Reunion no tenían NINGÚN camino que
     * las pidiera— y dos estaban cableadas AL SITIO EQUIVOCADO (Rule Britannia como música
     * del castillo, Hornpipe como música de taberna).
     *
     * Cubre las dieciséis por el MISMO camino que el juego: los contextos de localización
     * que deriva el switch, más los de escena que `main.ts` cablea.
     */
    const env = installEnv();
    const m = new MusicPlayer(true, env.engine);
    env.unlock();
    const contextos: MusicContext[] = [
      "overworld",
      "underworld",
      "frigate",
      "cities",
      "lighthouse",
      "hut",
      "castle",
      "blackthorn",
      "village",
      "keep",
      "principle",
      "dungeon",
      "combat",
      "victory",
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
    ];
    for (const c of contextos) m.play(c);
    expect(new Set(env.engine.played)).toEqual(new Set(SONG_TRACK)); // las 16, ni una menos
    expect(env.engine.played.every((u) => u.endsWith(".mid"))).toBe(true);
  });

  it("🔴 cada contexto resuelve a una pista real (ninguno cae en un 404)", () => {
    // La vía OGG tenía 4 de 9 pistas en disco: entrar en un castillo MATABA la música.
    expect(SONG_TRACK).toHaveLength(16);
    expect(new Set(SONG_TRACK).size).toBe(16);
    expect(
      SONG_TRACK.every((u) => u.startsWith("/assets/music/") && u.endsWith(".mid")),
    ).toBe(true);
  });
});
