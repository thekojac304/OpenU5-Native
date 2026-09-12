/**
 * EndgamePacer — pacer de la secuencia final (#34, patrón RefugeScript).
 *
 * El core emite el GUIÓN completo (`EndgameScript`, evento {kind:"endgame"}) y aquí
 * se PRESENTA: beats de texto paceados por TECLA (kernel_print_ds + getkey 0x83dc),
 * fases de animación a reloj de pared (re-tinte, orb/moongate, disolución, pergamino)
 * y estados terminales (freeze de victoria / sala-prisión varada). `endgaming` traga
 * el input global; `endgameAwaitKey` avanza los beats de tecla. Cadencias Clase C
 * calibradas al testigo 2:34 (re/notes/endgame-witness-20260721.md).
 *
 * Extraído de boot() (auditoría MANT-1/ARQ-2). Deps inyectadas (patrón
 * shop-console.ts); ciclo de vida testeable por vitest con fake timers.
 */
import type { Game } from "../core/game.js";
import type { SfxCue } from "../core/sfx.js";
import type { EndgameScript } from "../core/endgame/sequence.js";
import { questScroll } from "../core/quest/endgame.js";
import { endgameDissolveOrder } from "../core/transition/endgameDissolve.js";
import {
  buildOrbMoongateTimeline,
  endgameLineup,
  planEndgame,
  EndgamePrisonWander,
  LB_SEATED,
  LB_WALK_TILE,
} from "../skin/endgameScene.js";
import type { EndgameSceneView } from "../skin/api.js";
import { combatPartyTile } from "../skin/coreview.js";
import { t } from "../i18n/index.js";

export interface EndgamePacerDeps {
  game: Game;
  view: {
    setEndgameScene(scene: EndgameSceneView | null): void;
    emitSfx(cue: SfxCue): void;
  };
  hud: { message(text: string): void; messageAppend(text: string): void };
  refreshAwaiting: () => void;
  cancelAutoWalk: () => void;
  /** Delay efectivo de un beat de escena (knob `?scenebeat`, auditoría Q4). */
  sceneMs: (ms: number) => number;
  /** Valor crudo del knob (`?scenebeat`), o null sin knob: la disolución mide su
   *  DURACIÓN a reloj de pared y también se recorta bajo el knob. */
  sceneBeatMs: number | null;
  /** Mapa de la sala del trono (endgame.json del extractor), o null sin asset. */
  endgameRoom: number[][] | null;
  /**
   * Aviso de cambio de FASE, para quien tenga que acompañarla. Hoy lo usa la música:
   * ENDGAME.OVL llama al driver TRES veces (la tabla de rango por cuadro @0x0aee, Joyous
   * Reunion @0x0aff y Rule Britannia @0x0b18), o sea que el cierre cambia de canción
   * SEGÚN AVANZA — sin este aviso el port sólo podría poner una y dejarla. Opcional: sin
   * él el pacer se comporta exactamente igual que antes.
   */
  onPhase?: (phase: EndgameSceneView["phase"]) => void;
}

/** ms por unidad de run-n-frames 0x3AE6 (1 tick INT 1Ch ≈ 54.9 ms): convierte
 *  los `delayUnits` del guión (pausas MUDAS de la adenda fanfarria-re) a reloj. */
export const EG_TICK_MS = 55;
/** Fallback del beat del re-tinte verde sin `delayUnits` (pump 0x28 ≈ 2.2 s). */
export const EG_GREEN_MS = 1500;
/** ms por frame del timeline orb/moongate (testigo 85-100 s ≈ 65 frames). */
export const EG_STEP_MS = 180;
/** Duración de la disolución de píxeles (testigo 101-104 s ≈ 3 s). */
export const EG_DISSOLVE_MS = 3000;
export const EG_DISSOLVE_TICK_MS = 90;
/** Estampado del pergamino (misma cadencia que la vía DOM, SCROLL_LINE_MS). */
export const EG_SCROLL_LINE_MS = 350;
/** Cola tras el pergamino: MUDA y terminal. La «fanfarria» del testigo quedó
 *  REFUTADA (adenda fanfarria-re §6: audio POST-FREEZE, artefacto del host; el
 *  binario cae al spin mudo 0x4f9 sin ninguna llamada tras el último print). */
export const EG_SCROLL_TAIL_MS = 2500;
/** Pasada del wander de la sala-prisión (bucle idle infinito 0x0ae7). */
export const EG_WANDER_MS = 400;

export class EndgamePacer {
  private _active = false;
  private timer: number | null = null;
  private awaitKey: (() => void) | null = null;

  constructor(private readonly deps: EndgamePacerDeps) {}

  /** ¿Cierre en curso? (modal hasta el final de los tiempos). */
  get active(): boolean {
    return this._active;
  }

  cancelTimer(): void {
    if (this.timer !== null) {
      clearTimeout(this.timer);
      this.timer = null;
    }
  }

  /**
   * Teardown completo (cargar partida mid-escena, auditoría R3): cancela el
   * timer, baja el flag modal, olvida el await de tecla y desmonta la escena.
   */
  reset(): void {
    this.cancelTimer();
    this._active = false;
    this.awaitKey = null;
    this.deps.view.setEndgameScene(null);
  }

  /**
   * Tecla durante el cierre: los beats de TEXTO (diálogo del trono / páginas de
   * historia) avanzan con CUALQUIER tecla (getkey 0x83dc); las fases de animación
   * y los estados terminales se la tragan (freeze de victoria = bucle infinito
   * 0x04f9; sala-prisión = idle 0x0ae7).
   */
  consumeKey(): void {
    const advance = this.awaitKey;
    if (advance) {
      this.awaitKey = null;
      advance();
    }
  }

  run(script: EndgameScript): void {
    const { game, view, hud, refreshAwaiting, cancelAutoWalk, sceneMs, sceneBeatMs, endgameRoom } =
      this.deps;
    cancelAutoWalk();
    this.cancelTimer();
    this._active = true;
    refreshAwaiting();
    const roster = game.state.characters.slice(0, game.state.partySize);
    const partyTiles = roster.map((c) => combatPartyTile(c.class));
    // Estado VIVO de la escena; cada push publica una copia (repinta la piel).
    const scene: EndgameSceneView = {
      phase: "greenScene",
      room: endgameRoom,
      actors: endgameLineup(LB_WALK_TILE, partyTiles),
    };
    const pushScene = (): void => view.setEndgameScene({ ...scene });

    // Pergamino en líneas {text,rune}: questScroll + las 2 líneas RÚNICAS derivadas
    // (bytes de la fuente cinemática, re/notes/endgame.md §Pergamino) en lugar del
    // «THE QUEST OF THE AVATAR IS FOREVER» latino (el original lo estampa en runas).
    const buildScrollLines = (): { text: string; rune?: boolean; below?: boolean }[] => {
      const out: { text: string; rune?: boolean; below?: boolean }[] = [];
      const scroll = questScroll(game.state);
      scroll.forEach((line, idx) => {
        // El informe (última entrada, formatQuestReport) va FUERA del pergamino, en
        // blanco bajo el arte (testigo w140) → `below`.
        const below = idx === scroll.length - 1 ? true : undefined;
        if (line === t("THE QUEST OF THE AVATAR IS FOREVER")) {
          out.push({ text: "[E@QUE_@OF@[E@AVATAR", rune: true });
          out.push({ text: "IS@FOREVER", rune: true });
        } else {
          for (const l of line.split("\n")) out.push(below ? { text: l, below } : { text: l });
        }
      });
      return out;
    };

    const steps = planEndgame(script);
    let i = 0;

    // Fase orb/moongate (GAP 4): timeline derivado frame a frame + cues del censo.
    const runOrbGate = (done: () => void): void => {
      const frames = buildOrbMoongateTimeline(LB_WALK_TILE, partyTiles);
      let f = 0;
      const tick = (): void => {
        if (f >= frames.length) {
          done();
          return;
        }
        const fr = frames[f++]!;
        scene.actors = fr.actors;
        scene.moongate = fr.moongate;
        scene.orb = fr.orb;
        if (fr.sfx) view.emitSfx(fr.sfx);
        pushScene();
        this.timer = window.setTimeout(tick, sceneMs(EG_STEP_MS));
      };
      tick();
    };

    // Disolución (GAP 5): orden fn34 derivado (endgameDissolveOrder); progreso a reloj.
    // fn34 (sel 0x66, rama CLC) es la pintora real; el modelo fn32 quedó retirado —
    // ver endgameDissolve.ts:11-12.
    const runDissolve = (done: () => void): void => {
      scene.dissolveOrder ??= endgameDissolveOrder(320 * 200);
      scene.dissolve = 0;
      const t0 = performance.now();
      // La disolución mide su progreso a reloj de pared: bajo `?scenebeat` la
      // DURACIÓN también se recorta (con beat 0 completa en el primer tick).
      const dissolveMs = sceneBeatMs !== null ? Math.max(1, sceneBeatMs) : EG_DISSOLVE_MS;
      const tick = (): void => {
        const p = Math.min(1, (performance.now() - t0) / dissolveMs);
        scene.dissolve = p;
        pushScene();
        if (p >= 1) {
          done();
          return;
        }
        this.timer = window.setTimeout(tick, sceneMs(EG_DISSOLVE_TICK_MS));
      };
      tick();
    };

    // Pergamino (GAP 7): estampado línea a línea + cola muda (fanfarria Clase B).
    const runScroll = (done: () => void): void => {
      const lines = buildScrollLines();
      scene.scrollLines = lines;
      scene.scrollReveal = 0;
      pushScene();
      const tick = (): void => {
        scene.scrollReveal = (scene.scrollReveal ?? 0) + 1;
        pushScene();
        if ((scene.scrollReveal ?? 0) >= lines.length) {
          this.timer = window.setTimeout(done, sceneMs(EG_SCROLL_TAIL_MS));
          return;
        }
        this.timer = window.setTimeout(tick, sceneMs(EG_SCROLL_LINE_MS));
      };
      this.timer = window.setTimeout(tick, sceneMs(EG_SCROLL_LINE_MS));
    };

    // Sala-prisión varada (GAP 3b): LB SENTADO + party deambulando PARA SIEMPRE
    // (bucle idle infinito 0x0ac9/0x0ae7 con wander_sprite; input muerto).
    const runPrison = (): void => {
      const party = endgameLineup(LB_WALK_TILE, partyTiles).slice(1);
      const wander = new EndgamePrisonWander(LB_SEATED, party, endgameRoom ?? []);
      const tick = (): void => {
        scene.actors = wander.step();
        pushScene();
        this.timer = window.setTimeout(tick, sceneMs(EG_WANDER_MS));
      };
      tick();
    };

    const step = (): void => {
      if (i >= steps.length) return; // el guión SIEMPRE acaba en un beat terminal
      const { beat } = steps[i++]!;
      scene.phase = beat.phase;
      this.deps.onPhase?.(beat.phase);
      if (beat.sfx) view.emitSfx(beat.sfx);
      switch (beat.phase) {
        case "greenScene":
          // Pump de LLEGADA (0x06f2): pausa MUDA de `delayUnits` ticks (0x28 ≈ 2.2 s;
          // adenda fanfarria-re §3 — el antiguo par de beeps era una misattribución).
          pushScene();
          this.timer = window.setTimeout(
            step,
            sceneMs(beat.delayUnits ? beat.delayUnits * EG_TICK_MS : EG_GREEN_MS),
          );
          break;
        case "dialogue":
          // Página byte-exacta de ENDMSG.DAT por el choke i18n de la consola; la
          // auto-respuesta Yes/No (DATA.OVL 0x84b4/0x84ba) va inline localizada.
          if (beat.message) hud.message(beat.message);
          if (beat.reply) hud.messageAppend(t(beat.reply));
          pushScene();
          this.awaitKey = step; // getkey 0x83dc: pacea por tecla
          break;
        case "orbMoongate":
          runOrbGate(step);
          break;
        case "dissolve":
          runDissolve(step);
          break;
        case "storyHouse":
        case "storyDream":
          scene.storyPage = beat.page ?? 0;
          scene.storyText = beat.message ? t(beat.message) : "";
          pushScene();
          this.awaitKey = step; // páginas de historia paceadas por tecla
          break;
        case "scroll":
          runScroll(step);
          break;
        case "terminalFreeze":
          // GAP 9: «The End» — frame congelado (el pergamino estampado), SIN input
          // (bucle infinito 0x04f9). `endgaming` queda armado para siempre.
          pushScene();
          break;
        case "terminalPrison":
          runPrison();
          break;
      }
    };
    step();
  }
}
