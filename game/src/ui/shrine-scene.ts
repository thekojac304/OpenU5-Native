/**
 * ShrineScenePacer — reproduce la ESCENA del santuario / cámara del Codex (#277) a reloj
 * de pared: monta la rejilla 11×11 de MISCMAPS.DAT en el viewport y mueve al Avatar beat a
 * beat (aparición → caminata por el sendero → arrodillarse; y a la vuelta, levantarse →
 * desandar → desaparecer).
 *
 * Es el mismo patrón —y la misma primitiva de tiempo— que `TrollSneak`: en el original la
 * pausa de cada paso la da `spectacle_cue` (CAST2 0x0e64) = `beep_ticks(1)` + pisada +
 * `beep_ticks(4)`, y `beep_ticks` (kernel 0x3ae6) es RUN-N-FRAMES (repintado + un tick
 * INT 1Ch por iteración ≈ 54,9 ms). ⇒ `frames × unitMs`, con la MISMA unidad que el troll.
 *
 * El RESTO de eventos del turno se DIFIERE hasta agotar los beats (`rest`), como en el
 * troll: en el binario todo esto es síncrono dentro de la rutina, así que nada legítimo
 * puede colarse en medio — y en particular el prompt «Upon what virtue…» sale con el
 * Avatar YA arrodillado, que es lo que enseñan las capturas del usuario.
 * BAJO AUTOMATIZACIÓN (navigator.webdriver) la unidad es 0 → drena todo SÍNCRONO, con lo
 * que e2e y digests no se mueven: el orden de eventos es idéntico al previo a #277.
 *
 * La escena la DESMONTA el propio guión de salida (su último beat deja el Avatar en
 * `null`), más el `setScene(null)` del final: sin él, la explanada se quedaría pegada en
 * el viewport tras volver al sobremundo.
 */
import type { Game } from "../core/game.js";
import type { ShrineSceneScript } from "../core/world/shrine-scene.js";
import type { ShrineSceneView } from "../skin/api.js";
import type { SfxCue } from "../core/sfx.js";

export interface ShrineScenePacerDeps {
  /** ms por FOTOGRAMA de `beep_ticks` (0 = drain síncrono bajo automatización). */
  unitMs: number;
  /** Monta/desmonta la escena en la vista (`CoreView.setShrineScene`). */
  setScene: (scene: ShrineSceneView | null) => void;
  /** Bus de sonido de la vista (la pisada de cada `spectacle_cue`). */
  emitSfx: (cue: SfxCue) => void;
  applyEvents: (events: ReturnType<Game["move"]>) => void;
  refreshAwaiting: () => void;
  cancelAutoWalk: () => void;
  /**
   * La escena se DESMONTA (mitad de salida agotada, explanada vacía). Hoy lo usa la
   * música: el rito es una de las escenas que en el original se tocan con el selector de
   * canción FIJA —MAINOUT.OVL 0x0968 pone «Stones» y congela— y devuelve el mando a la
   * localización justo al volver (su `call 0x0e1d`). Opcional.
   */
  onSceneEnd?: () => void;
}

export class ShrineScenePacer {
  private timer: number | null = null;
  private _active = false;

  constructor(private readonly deps: ShrineScenePacerDeps) {}

  /** ¿Escena en curso? (modal: el input se traga mientras corre, como el troll). */
  get active(): boolean {
    return this._active;
  }

  cancel(): void {
    if (this.timer !== null) {
      clearTimeout(this.timer);
      this.timer = null;
    }
    this._active = false;
  }

  /**
   * Reproduce una mitad del guión (entrada o salida) y, al agotar los beats, reanuda el
   * RESTO del turno diferido. Con unidad 0 (automatización) drena todo síncrono.
   */
  run(script: ShrineSceneScript, rest: ReturnType<Game["move"]>): void {
    const { unitMs, setScene, emitSfx, applyEvents, refreshAwaiting, cancelAutoWalk } =
      this.deps;
    cancelAutoWalk();
    this.cancel();
    this._active = true;
    refreshAwaiting();
    let i = 0;
    const step = (): void => {
      this.timer = null;
      while (i < script.beats.length) {
        const beat = script.beats[i++]!;
        setScene({
          tiles: script.tiles,
          avatar: beat.avatar
            ? { col: beat.avatar.x, row: beat.avatar.y, tile: beat.avatar.tile }
            : null,
        });
        // `sfx_footstep` (kernel 0x433e) dentro de cada `spectacle_cue`. Los beats de
        // cambio-de-tile (arrodillarse/levantarse) NO son cue: no suenan (footstep=false).
        if (beat.footstep) emitSfx({ id: "move-step" });
        if (unitMs > 0) {
          this.timer = window.setTimeout(step, beat.frames * unitMs);
          return;
        }
      }
      // Fin de esta mitad. La de SALIDA acaba con la explanada vacía; desmontar aquí
      // devuelve el viewport al sobremundo (el original restaura g_location en 0x10e7).
      const last = script.beats[script.beats.length - 1];
      if (!last || last.avatar === null) {
        setScene(null);
        this.deps.onSceneEnd?.();
      }
      this._active = false;
      applyEvents(rest); // reanuda el turno diferido (kneel + prompt, o nada)
      refreshAwaiting();
    };
    step();
  }
}
