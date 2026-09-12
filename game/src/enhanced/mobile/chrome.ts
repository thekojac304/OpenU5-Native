/**
 * CHAPA ENHANCED — el montador, y el único que toca el DOM del deck.
 *
 * DÓNDE VIVE, Y POR QUÉ AHÍ. La chapa se monta DENTRO de `.touch-controls`, el deck que
 * construye `ui/touch.ts`. No es comodidad: es lo que hace que Enhanced herede gratis —y
 * sin duplicar— cinco mecanismos que ya están medidos y con candado:
 *
 *   1. LA RESERVA. `syncReserve()` mide el border box de `.touch-controls` y publica
 *      `--u5-touch-reserve` (vertical) o `--u5-touch-reserve-x` (apaisado) como padding
 *      de `#app`, y dispara un `resize` para que la piel re-escale el canvas. Montando
 *      dentro, la chapa entra en esa medida sola y el bucle sigue siendo UNO.
 *   2. EL TECHO DEL SHELL. `syncTechoShell()` mide `.touch-util` para publicar
 *      `--u5-shell-techo` e impedir que el panel del shell caiga sobre la Esc que lo
 *      cierra (ficha #127). Por eso la barra Enhanced se ANIDA en `.touch-util` en vez de
 *      sustituirla: la medición sigue encontrando la franja de teclas fijas.
 *   3. EL IDIOMA EN CALIENTE. `TouchControls.relabel()` recorre `this.root` buscando
 *      `[data-ts-label|title|aria]`. Como la chapa es descendiente de ese root y rotula
 *      con el mismo `setTsLabel`, cambiar de idioma la re-traduce entera sin una sola
 *      suscripción propia.
 *   4. EL RÉGIMEN TÁCTIL. `aplicarRegimen()` pone el `display` inline del root y la clase
 *      `u5-touch` de `<html>`: al apagarse el régimen (2-en-1 con ratón enchufado) la
 *      chapa se apaga con el deck, y su CSS —que exige `.u5-touch`— deja de casar.
 *   5. LAS HOJAS. A–Z, 123 y Sí-No siguen siendo las del deck, con su auto-alzado por
 *      `expectInput`. La chapa no reimplementa entrada de texto, cantidades ni Y/N.
 *
 * QUÉ APAGA. Sólo lo que sustituye: barra de modo, hoja «move» (cruceta clásica + la
 * rejilla de 25 comandos) y los botones clásicos de la fila útil. Lo hace el CSS
 * (`css.ts`), no este módulo: nada se mueve de sitio en el DOM, así que no hay nada que
 * restaurar y `dispose()` es sólo quitar lo que se añadió.
 */
import {
  onDeckContext,
  currentDeckContext,
  type DeckContext,
} from "../../ui/touch.js";
import { installEnhancedCss, uninstallEnhancedCss } from "./css.js";
import { buildMovement, type MovementHandle } from "./movement.js";
import { buildActionBar, type ActionBarHandle } from "./actionbar.js";
import { buildDrawer, type DrawerHandle, type DrawerExtra } from "./drawer.js";
import { buildQuickBar, type QuickBarHandle } from "./quickbar.js";
import { attachForma, type FormaHandle } from "./forma.js";
import { applyPadPos, clearPadPos, loadPadPos, onPadPosChange } from "./padpos.js";

export interface EnhancedChromeHandle {
  /** Cierra el cajón y retira todo lo añadido (CSS, clase de raíz y DOM). */
  dispose(): void;
  /** Sondas del arnés: el cajón y su estado. */
  readonly drawer: DrawerHandle;
  readonly bar: ActionBarHandle;
  readonly quick: QuickBarHandle;
  readonly movement: MovementHandle;
  /** Forma viva (apilada / banda) — sonda del arnés. Ver `forma.ts`. */
  readonly forma: FormaHandle;
  /** La FILA de mandos (cruceta + acciones rápidas). Sonda del arnés. Ver `css.ts` §4b. */
  readonly fila: HTMLElement;
}

/**
 * Monta la chapa. Devuelve `null` si el deck aún no existe (escritorio, `?replay=` en
 * modo solo-UI original, o un montaje sin `TouchControls`): la chapa es una afordancia
 * del deck y sin deck no tiene dónde vivir ni qué medir.
 */
export function mountEnhancedChrome(
  extras: readonly DrawerExtra[] = [],
): EnhancedChromeHandle | null {
  if (typeof document === "undefined") return null;
  const deck = document.querySelector<HTMLElement>(".touch-controls");
  const util = deck?.querySelector<HTMLElement>(".touch-util");
  if (!deck || !util) return null;

  installEnhancedCss();

  const movement = buildMovement();
  const bar = buildActionBar();
  const quick = buildQuickBar();
  const drawer = buildDrawer(bar.commandsBtn, undefined, extras);

  // ORDEN EN EL FLUJO (vertical, de arriba abajo): hojas del deck · fila de mandos
  // (cruceta + acciones rápidas) · fila útil con la barra de sistema. La barra queda ABAJO
  // del todo a propósito, y son dos razones:
  //   · `syncTechoShell()` sólo publica el techo cuando la franja de teclas fijas está
  //     PEGADA al borde inferior (≤24 px) — si la barra flotara sobre la cruceta, el
  //     techo se apagaría y el panel del shell podría tapar la Esc otra vez;
  //   · deja la última fila del pulgar a la barra y no a las flechas, que así no quedan
  //     pegadas al indicador de inicio de iOS (zona de gestos del sistema).
  util.appendChild(bar.el);
  // ── LA FILA DE MANDOS: cruceta + acciones rápidas EN LA MISMA CAJA ──────────────────
  // Antes eran dos hermanos apilados (fila rápida encima, cruceta debajo), y eso es lo que
  // dejaba los ~200 px a un lado de la cruceta en blanco con la cruz pegada a un borde —
  // el desperdicio que el usuario reportó (§4 del encargo). Metiéndolos en UNA caja, el
  // reparto pasa a ser un `flex-direction` que el CSS elige según la posición de la cruz:
  //   · izquierda → `row`          [cruceta][2×2 de acciones]
  //   · derecha   → `row-reverse`  [2×2 de acciones][cruceta]
  //   · centro    → `column-reverse` acciones en una fila de 4, cruceta centrada debajo
  // El ORDEN DE DOM es SIEMPRE cruceta → acciones, y es a propósito: el orden de foco y el
  // de lectura de TalkBack/VoiceOver quedan fijos (mover primero, actuar después) valga lo
  // que valga la preferencia visual, que es la regla de siempre para `row-reverse`.
  const fila = document.createElement("div");
  fila.className = "u5e-fila";
  fila.appendChild(movement.el);
  fila.appendChild(quick.el);
  // La fila va ENCIMA de `.touch-util` (o sea, de la barra de sistema): deja la última
  // franja del pulgar a la barra y no a las flechas, que así no quedan pegadas al indicador
  // de inicio de iOS. Y `syncTechoShell()` sigue midiendo `.touch-util` pegada abajo, que
  // es la condición de que el techo del shell se publique (ficha #127).
  deck.insertBefore(fila, util);
  // LA POSICIÓN DE LA CRUZ, publicada en `<html>` (`data-u5e-pad`). Se siembra de la
  // preferencia y se re-aplica cuando el ajuste la cambia: ni un `reload`, ni un re-montaje.
  applyPadPos(loadPadPos());
  const offPad = onPadPosChange((pos) => applyPadPos(pos));
  // El cajón, el ÚLTIMO: es `position:absolute`, así que el orden de DOM sólo decide el
  // orden de PINTADO (queda por encima) y no aporta ni un píxel al border box del deck —
  // que es justo lo que la invariante de reserva necesita.
  deck.appendChild(drawer.el);

  // FORMA — apilada o banda, según lo que el layout partido deje libre. Se engancha
  // DESPUÉS de montar las piezas: su primer `sync()` mide el hueco, y el hueco no depende
  // de lo que la chapa ocupe (ver el diagrama de `forma.ts`), pero el ATRIBUTO tiene que
  // caer sobre un deck que ya tenga sus hijos o la primera capa pintada sería la apilada.
  const forma = attachForma(deck);

  const apply = (ctx: DeckContext): void => {
    bar.syncContext(ctx);
    quick.syncContext(ctx);
    drawer.render(ctx);
  };
  apply(currentDeckContext() ?? "world");

  // Contexto por EVENTO, no por sondeo: `TouchControls.refresh()` avisa cuando el modo
  // cambia de verdad, y a él lo dispara `main.ts` en cada tecla y cada tap.
  const offCtx = onDeckContext((ctx) => {
    // Un cambio de contexto (entrar en combate, caer en una mazmorra) reconstruye la
    // lista: el cajón se cierra para que nadie pulse a ciegas un botón que acaba de
    // cambiar de sitio bajo el dedo.
    drawer.close();
    apply(ctx);
  });

  return {
    drawer,
    bar,
    quick,
    movement,
    forma,
    fila,
    dispose(): void {
      offCtx();
      offPad();
      clearPadPos();
      fila.remove();
      forma.dispose();
      drawer.dispose();
      bar.dispose();
      quick.dispose();
      movement.dispose();
      uninstallEnhancedCss();
    },
  };
}
