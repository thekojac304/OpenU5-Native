/**
 * CampSleep — conductor de la secuencia de sueño de la acampada (H)ole up.
 *
 * En el original el descanso NO es instantáneo — se muestra "Zzzz...", el reloj
 * corre las N horas y luego se despierta. Este conductor lleva el bucle de sueño
 * del core paso a paso (1 h/tick) para HACER VISIBLE ese paso del tiempo;
 * `campSleep` es el timer y `camping` bloquea el input mientras la party duerme
 * (modal, como los paneles).
 *
 * Ritmo del sueño: 1 h de juego por tick. El original anima advance_clock(5)
 * por frame (~100 ms) durante el descanso → 8 h ≈ 10 s de reloj corriendo; el
 * port discretiza a 1 h/paso, así que se elige un tick que lea como "las horas
 * corren" sin alargar el turno (Clase C: cadencia no derivada seed-exacto; camp
 * está FUERA del set de paridad, deliberate-divergences.md:580).
 *
 * FASE 1 del easter egg de Iolo (witness-derived, CAMP_IOLO_MUSICA.mov): si el
 * vigía es BARDO, antes del bucle de horas TOCA el laúd ~7.5 s con el RELOJ
 * CONGELADO (medido: audio 6.5-14 s, el sol de la tira de cielo no se mueve
 * 8-13 s). PRESENTACIÓN PURA: no llama a game.* (ni campSleepStep ni RNG), sólo
 * pinta la escena en fase canción y espera — el estado final del camp es
 * idéntico con y sin canción (blindado por parity + el test discriminante).
 * Cadencia real-time = Clase C. Ver re/notes/camp-asm.md §Iolo.
 *
 * Extraído de boot() (auditoría MANT-1/ARQ-2): los timers campSleep/campSongTimer/
 * campFlashTimer eran closures inimportables; ahora el ciclo de vida completo
 * (dormir/canción/despertar/aparición/emboscada/cancelación) es testeable por
 * vitest con fake timers. Deps inyectadas (patrón shop-console.ts).
 */
import type { Game } from "../core/game.js";
import type { SfxCue } from "../core/sfx.js";
import { apparitionDurationMs } from "../skin/fiel/apparition.js";
import { cueDurationMs } from "../skin/fiel/speaker.js";

export interface CampSleepDeps {
  game: Game;
  view: {
    setCampScene(
      on: boolean,
      guardIdx?: number,
      guardCell?: { col: number; row: number } | null,
      songPhase?: boolean,
    ): void;
    emitSfx(cue: SfxCue): void;
  };
  hud: { message(text: string): void };
  applyEvents: (events: ReturnType<Game["move"]>) => void;
  refreshAwaiting: () => void;
  cancelAutoWalk: () => void;
  /** Delay efectivo de un beat de escena (knob `?scenebeat`, auditoría Q4). */
  sceneMs: (ms: number) => number;
  /**
   * Flag global de SONIDO — `g_unk_a9ce` del original (^S en MAINOUT 0x0b87, F8 en el
   * port). Es una TERCERA precondición del easter egg del bardo, no sólo del audio:
   * ver `bardEasterEggFires`. Thunk (no valor) porque el usuario lo alterna en caliente.
   */
  soundEnabled: () => boolean;
  /**
   * La acampada TERMINA (despertar, o emboscada que la corta). Hoy lo usa la música:
   * `kernel_camp_holeup` (ULTIMA.EXE 0x3e65) pone «Stones» con canción FIJA, corre la
   * escena y devuelve el mando a la localización al volver (su `call 0x0e1d`). Opcional.
   */
  onEnd?: () => void;
}

/** Cadencia del tick de 1 h de sueño (Clase C, ver cabecera). */
export const CAMP_HOUR_MS = 360;

export class CampSleep {
  private campSleep: number | null = null;
  /** FASE 1 de Iolo: temporizador de la canción. */
  private campSongTimer: number | null = null;
  // Flash de la APARICIÓN al despertar: desmonta la escena al acabar el pulso. CON
  // handle (auditoría R6/MANT-2: era el único timer de la familia camp sin ciclo de
  // vida — re-acampar o cargar partida dentro de la ventana de ~20 s dejaba un timer
  // huérfano desmontando la escena nueva a destiempo).
  private campFlashTimer: number | null = null;
  private _camping = false;

  // La fase visual dura EXACTAMENTE lo que el cue "bard-song" (deriva de BARD_MELODY
  // + el ritmo Clase C ~142 ms/índice = ~7.5 s), así el audio del laúd nunca se
  // derrama en la caminata post-camp (antes: rests de 1860 ms hacían un cue de 27 s
  // que sonaba ~20 s dentro del movimiento — el "ráfagas de sonidos raros al moverse"
  // del reporte).
  private readonly campSongMs = Math.ceil(cueDurationMs({ id: "bard-song" }));

  constructor(private readonly deps: CampSleepDeps) {}

  /** ¿La party está durmiendo? (modal: el input se traga hasta despertar). */
  get camping(): boolean {
    return this._camping;
  }

  /** Cancela TODOS los timers de la familia camp (sleep + canción + flash). */
  cancel(): void {
    if (this.campSleep !== null) {
      clearInterval(this.campSleep);
      this.campSleep = null;
    }
    if (this.campSongTimer !== null) {
      clearTimeout(this.campSongTimer);
      this.campSongTimer = null;
    }
    if (this.campFlashTimer !== null) {
      clearTimeout(this.campFlashTimer);
      this.campFlashTimer = null;
    }
  }

  /**
   * Teardown completo (cargar partida mid-escena, auditoría R3): cancela los
   * timers, baja el flag modal y desmonta la escena de la vista.
   */
  reset(): void {
    this.cancel();
    this._camping = false;
    this.deps.view.setCampScene(false);
  }

  /**
   * 🔴 NO QUITES LA GUARDA `soundEnabled()` DE ESTA FUNCIÓN. ES FIEL. (ficha #94)
   *
   * Quien llegue aquí con #94 en la mano viene con la regla general en la cabeza, y la
   * regla general dice lo CONTRARIO de lo que hace este sitio. Las dos cosas son ciertas:
   *
   *  · REGLA GENERAL (#94, `ULTIMA.EXE:0x22c0 pcspeaker_beep`): la bandera de sonido
   *    condiciona SÓLO el tono. `cmp [g_unk_a9ce],0 / je` salta únicamente el `call 0x22e2
   *    set_tone`; el `delay_via_timer` (0x22d7) y el apagado del gate (0x22da) son
   *    INCONDICIONALES. ⇒ envolver un beep entero en `if (sonido)` haría correr al clon
   *    MÁS RÁPIDO EN MUDO. Por eso el port NO gatea ninguna espera con el sonido…
   *  · …EXCEPTO AQUÍ, y esta excepción está MEDIDA: **el binario también gatea, pero en
   *    el LLAMADOR, no dentro de `pcspeaker_beep`**. `CMDS.OVL:0x0123 cmp byte ptr
   *    [g_unk_a9ce],0 / 0x0128 je 0x130` es la TERCERA precondición del easter egg (ver
   *    abajo); al no escribirse la ranura `[bp-4]`, el `0x014f cmp [bp-4],-1 / je 0x1b0`
   *    salta el sprite del laúd, la melodía **y la pausa de 52 redibujos**. ⇒ con el
   *    sonido apagado, el bardo del ORIGINAL tampoco cuesta tiempo.
   *
   * ⇒ borrar `soundEnabled()` de aquí «por fidelidad a #94» METE una divergencia: dejaría
   * 7,5 s de escena congelada en silencio que el original no tiene. Es el sitio contrario
   * al que #94 describe, y es el único de todo `game/src` donde una ESPERA cuelga del
   * sonido (censo: `soundEnabled` = 4 ocurrencias / 2 ficheros / UN consumidor — éste;
   * `cueDurationMs` = 1 consumidor — `campSongMs`, usado sólo en esta rama).
   *
   * ¿Se dispara el easter egg del bardo? Las TRES precondiciones del original, en su
   * orden (CMDS.OVL, bucle de roster 0x00be-0x0149):
   *
   *   0x0116  cmp byte ptr [bx], 0x42     ; clase 'B' = BARD
   *   0x0119  jne 0x130
   *   0x011e  cmp [bp-0x20], [bp+6]       ; …y ese miembro ES el vigía
   *   0x0121  jne 0x130
   *   0x0123  cmp byte ptr [g_unk_a9ce],0 ; …y el SONIDO está ON (^S)
   *   0x0128  je  0x130                   ; ← si falla cualquiera: NO guarda la ranura
   *   0x012a  mov [bp-4], [bp-0x24]       ;   (sólo si pasa las tres)
   *
   * Y la ranura es la que abre TODO el bloque: `[bp-4]` nace a -1 (0x0010) y
   * `0x014f cmp [bp-4],-1 / 0x0153 je 0x1b0` salta por encima del sprite del laúd
   * (0x017c-0x0181), de la melodía (0x0183) Y de la pausa de 52 redibujos
   * (0x0188 → 0x3AE6(52)). ⇒ **con el sonido apagado el bardo NI toca NI cambia de
   * sprite NI cuesta tiempo**: se duerme como los demás y "Zzzzzz..." sale en el acto.
   *
   * Era el residuo 3 de `camp-bard-anim.md §7`: el port sólo silenciaba el cue
   * (`speaker.play` retorna si está apagado) y dejaba 7.5 s de escena congelada EN
   * SILENCIO. Suprimir la fase NO mueve el RNG — es presentación pura (no llama a
   * `game.*`), lo que ya blindan el test discriminante y la paridad.
   */
  private bardEasterEggFires(guardIdx: number): boolean {
    const { game, soundEnabled } = this.deps;
    return (
      guardIdx >= 0 &&
      guardIdx < game.state.partySize &&
      game.state.characters[guardIdx]?.class === "B" &&
      // 🔴 FIEL — NO BORRAR (CMDS.OVL:0x0123, la 3ª precondición). No es la guarda de
      // audio de #94: aquí el binario gatea en el LLAMADOR y con ello se salta también
      // la PAUSA de 52 redibujos. Ver la cabecera antes de tocar esta línea.
      soundEnabled()
    );
  }

  /**
   * Conduce el bucle de sueño del core (campSleepStep/campWake) PASO A PASO para
   * hacer visible el paso del tiempo: "Zzzz..." → el reloj avanza 1 h por tick →
   * "Party rested!" (o la aparición). El input queda bloqueado (`camping`) como en
   * un modal. Una emboscada (campSleepStep → combate) corta la secuencia. Los
   * rands salen en el MISMO orden que `game.camp()` atómico (mismos métodos).
   */
  run(hours: number, guardIdx: number): void {
    const { game, view, hud, applyEvents, refreshAwaiting, cancelAutoWalk, sceneMs } = this.deps;
    cancelAutoWalk();
    this.cancel();
    hud.message("Zzzzzz...\n\n"); // DATA.OVL DS 0x41d4
    this._camping = true;
    // Monta la escena de acampada: el coreview sustituye la ventana por la arena
    // CampFire + el party en formación (gateado a overworld a pie). El `hour` mueve al
    // guardia por su ronda: arranca en 0 y sube con cada tick de sueño (abajo).
    // La celda del vigia arranca en SU puesto de formacion y la avanza campSleepStep una
    // vez por hora (paseo aleatorio 0x0337-0x03e8). NO se serializa en el save: el binario
    // la reconstruye al montar la escena, y un campo nuevo seria superficie de compat.
    let guardCell = game.campGuardStartCell(guardIdx);
    view.setCampScene(true, guardIdx, guardCell);
    refreshAwaiting();
    let h = 0;
    const wake = (): void => {
      this.cancel();
      this._camping = false;
      this.deps.onEnd?.();
      const events = game.campWake(guardIdx); // cura parcial / aparición / "Party rested!"
      // Si cruza la APARICIÓN (25%), su flash de inversión de paleta debe caer SOBRE la
      // escena (el original la invierte con el party en formación). Mantenemos la escena
      // montada durante el flash y la desmontamos al terminar (un pulso por miembro vivo,
      // = las campanillas de cura). Sin aparición: desmonte inmediato.
      const apparition = events.some(
        (e) => e.kind === "sfx" && e.sfx?.id === "apparition-materialize",
      );
      applyEvents(events);
      if (apparition) {
        const living = game.state.characters
          .slice(0, game.state.partySize)
          .filter((c) => c.status !== "D").length;
        this.campFlashTimer = window.setTimeout(() => {
          this.campFlashTimer = null;
          view.setCampScene(false);
        }, sceneMs(apparitionDurationMs(living)));
      } else {
        view.setCampScene(false);
      }
      refreshAwaiting();
    };
    // El bucle de horas (reloj corriendo). Lo arranca directamente el camp normal, o al
    // TERMINAR la canción del bardo (fase 1). PURO respecto del core: idéntica secuencia de
    // campSleepStep con y sin canción — la fase 1 no toca game.*.
    const startHoursLoop = (): void => {
      this.campSleep = window.setInterval(() => {
        if (h >= hours) {
          wake();
          return;
        }
        const step = game.campSleepStep(h, hours, guardCell, guardIdx);
        h++;
        guardCell = step.guardCell;
        if (!step.ambush) view.setCampScene(true, guardIdx, guardCell); // ronda del vigia
        applyEvents(step.events); // avanza el reloj (visible al refrescar el HUD) + msgs
        if (step.ambush) {
          // La emboscada corta el sueño: el combate ya arrancó dentro de
          // campSleepStep (applyEvents lo montó vía combat-started). NO se
          // despierta ni se cura (epílogo 0x0306, ax=1). El coreview ya deja de
          // emitir la escena al entrar en combate (gate de modo), pero bajamos el
          // flag para no dejarla montada al salir de la arena.
          this.cancel();
          this._camping = false;
          this.deps.onEnd?.();
          view.setCampScene(false);
          refreshAwaiting();
        }
      }, sceneMs(CAMP_HOUR_MS));
    };
    // FASE 1 (canción de Iolo): sólo si el vigía es bardo. Toca ~7.5 s con el reloj
    // congelado (songPhase=true, sin avanzar h ni campSleepStep) y LUEGO vela normal. Es
    // presentación pura: si se cancela (unmount) el temporizador se limpia en cancel().
    if (this.bardEasterEggFires(guardIdx)) {
      view.setCampScene(true, guardIdx, guardCell, /*songPhase*/ true);
      // La canción del laúd de Iolo suena por el CANAL DE SFX/SPEAKER (emitSfx), como el
      // original en PC estándar: el usuario NUNCA oye música de fondo en su DOSBox y aun así
      // la canción suena → el original la emite por tonos de PC-speaker (misma vía que la
      // aparición/arpegio/campanillas), NO por el driver de música. NO se toca la música
      // ambiente (nada de crossfade). El id "bard-song" lleva la secuencia de notas (hoy
      // witness-derived; el nivel-2 del binario la sustituirá por la tabla exacta). Ver
      // re/notes/camp-scene-kernel.md §5.
      view.emitSfx({ id: "bard-song" });
      this.campSongTimer = window.setTimeout(() => {
        this.campSongTimer = null;
        view.setCampScene(true, guardIdx, guardCell, /*songPhase*/ false); // fin canción → vigilia
        startHoursLoop();
      }, sceneMs(this.campSongMs));
    } else {
      startHoursLoop();
    }
  }
}
