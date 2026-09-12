/**
 * BlackthornScenePacer — presenta los SEGMENTOS de la escena de captura (#324,
 * BLCKTHRN 0x060e + anim_vm 0x00be) a reloj de pared, con el MISMO patrón que
 * `ShrineScenePacer`: el resto del turno se DIFIERE hasta agotar los beats (en el
 * binario todo es síncrono dentro de la rutina) y bajo automatización (unidad 0)
 * drena síncrono — e2e y digests ven el mismo orden de eventos.
 *
 * A diferencia del rito, la escena PERSISTE entre segmentos (los prompts del
 * interrogatorio corren con la sala a la vista): el pacer conserva la rejilla
 * (mutable, los parches del VM la van pisando) y las figuras entre `run()`s, y sólo
 * desmonta con `dismount` (el depósito 0x08e7) o `cancel()`.
 *
 * Unidad de tiempo: `frames` son unidades de run-n-frames 0x3AE6 / ticks INT 1Ch —
 * la calibración compartida PAUSE_UNIT_MS (main.ts la inyecta, como en las demás).
 */
import type { Game } from "../core/game.js";
import type { CaptureSceneScript } from "../core/world/blackthorn-scene.js";
import type { BlackthornSceneView } from "../skin/api.js";
import type { SfxCue } from "../core/sfx.js";

export interface BlackthornScenePacerDeps {
  /** ms por unidad 0x3AE6 (0 = drain síncrono bajo automatización). */
  unitMs: number;
  /** Monta/actualiza/desmonta la escena en la vista (`CoreView.setBlackthornScene`). */
  setScene: (scene: BlackthornSceneView | null) => void;
  /** Bus de sonido (pisadas 0x433e y barridos 0x2192 de los beats). */
  emitSfx: (cue: SfxCue) => void;
  applyEvents: (events: ReturnType<Game["move"]>) => void;
  refreshAwaiting: () => void;
  cancelAutoWalk: () => void;
  /**
   * La escena se DESMONTA (depósito en la celda, 0x08e7). Hoy lo usa la música: la captura
   * es una de las dos secuencias que el parche deja MUDAS a propósito (History.txt), y
   * TOWN.OVL 0x12ca devuelve el mando a la localización al volver (`call 0x0e1d`).
   * Opcional.
   */
  onSceneEnd?: () => void;
}

export class BlackthornScenePacer {
  private timer: number | null = null;
  private _active = false;
  /** Rejilla VIVA de la sala (copia mutable; los parches del VM la pisan). */
  private grid: number[][] | null = null;
  private figures: BlackthornSceneView["figures"] = [];
  private mounted: "blackout" | "throne" | null = null;

  constructor(private readonly deps: BlackthornScenePacerDeps) {}

  /** ¿Segmento en curso? (modal: el input se traga mientras corre, como el rito). */
  get active(): boolean {
    return this._active;
  }

  /** ¿Escena montada? (persiste entre segmentos, mientras dura el interrogatorio). */
  get sceneMounted(): boolean {
    return this.mounted !== null;
  }

  cancel(): void {
    if (this.timer !== null) {
      clearTimeout(this.timer);
      this.timer = null;
    }
    this._active = false;
    this.grid = null;
    this.figures = [];
    this.mounted = null;
    this.deps.setScene(null);
  }

  private view(): BlackthornSceneView {
    return {
      phase: this.mounted === "throne" ? "throne" : "blackout",
      tiles: this.grid,
      figures: this.figures,
    };
  }

  /**
   * Presenta un segmento y, al agotar los beats, reanuda el RESTO del turno diferido.
   * Con unidad 0 drena todo síncrono (sin timers).
   */
  run(script: CaptureSceneScript, rest: ReturnType<Game["move"]>): void {
    const { unitMs, setScene, emitSfx, applyEvents, refreshAwaiting, cancelAutoWalk } =
      this.deps;
    cancelAutoWalk();
    if (this.timer !== null) {
      clearTimeout(this.timer);
      this.timer = null;
    }
    this._active = true;
    refreshAwaiting();
    // El apagón de la venda (0x0676 set_color(0) + 0x0689 fill_rect del viewport).
    if (this.mounted === null && script.blackout) {
      this.mounted = "blackout";
      setScene(this.view());
    }
    let i = 0;
    const step = (): void => {
      this.timer = null;
      while (i < script.beats.length) {
        const beat = script.beats[i++]!;
        if (beat.mount && script.tiles) {
          this.grid = script.tiles.map((row) => [...row]);
          this.mounted = "throne";
        }
        if (beat.patch && this.grid) {
          const row = this.grid[beat.patch.y];
          if (row) row[beat.patch.x] = beat.patch.tile;
        }
        if (beat.figures) {
          this.figures = beat.figures.map((f) => ({ col: f.x, row: f.y, tile: f.tile }));
        }
        setScene(this.view());
        if (beat.footstep) emitSfx({ id: "move-step" }); // sfx_footstep 0x433e
        if (beat.sfx) emitSfx({ id: beat.sfx });
        if (unitMs > 0 && beat.frames > 0) {
          this.timer = window.setTimeout(step, beat.frames * unitMs);
          return;
        }
      }
      if (script.dismount) {
        // El depósito (0x08e7) restaura la pantalla: fuera escena.
        this.grid = null;
        this.figures = [];
        this.mounted = null;
        setScene(null);
        this.deps.onSceneEnd?.();
      }
      this._active = false;
      applyEvents(rest); // reanuda el turno diferido (mensajes, esperas, prompt…)
      refreshAwaiting();
    };
    step();
  }
}
