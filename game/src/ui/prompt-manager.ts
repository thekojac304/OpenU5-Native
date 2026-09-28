/**
 * PromptManager — el getkey/getstring/getnum MODAL de la consola (los 9 tipos
 * de `pendingPrompt`), extraído de boot() (TRAMO 2 del refactor estructural,
 * auditoría MANT-1/ARQ-2: el estado del prompt vivo + su reductor de teclas
 * eran un closure inimportable).
 *
 * Posee el PROMPT VIVO (`current`, único y modal como el getkey del binario) y
 * el REDUCTOR de keydown (`handleKey`): para yesno-esc, ESC=N; para yesno/digit,
 * ESC se ignora; tecla no válida = re-lee (getkey loop del binario). El prompt
 * no altera el stream vivo del juego.
 *
 * Los productores (flows de eventos, pickers, tienda, camp) siguen ARMANDO el
 * prompt (`prompts.current = {…}`) y los conductores autoritativos (party-select/
 * ready-picker/shop) lo re-arman/cierran desde su `onKey` — misma semántica que
 * el `let pendingPrompt` original. Deps inyectadas (patrón shop-console.ts).
 */
import { runeSyllableForInitial } from "../core/magic/spells.js";

export type PendingPrompt =
  | { type: "yesno-esc"; resolve: (yes: boolean) => void } // Flow 1: ESC=N
  // Flow 2: ESC ignorado. `tag` opcional identifica el DUEÑO del prompt para el
  // arnés e2e (hook guardPromptOpen — el tour paga el tributo de guardia).
  //
  // ★ El `tag` es PURA IDENTIDAD: nadie del port lo lee — sólo el hook read-only
  // `__u5test.guardPromptOpen()`. Sin él, dos prompts de mundo que el arnés debe
  // decidir de forma OPUESTA (pagar el tributo / rehusar el peaje) son
  // indistinguibles, y un arnés que no puede distinguirlos no puede contestar
  // NINGUNO: el prompt se queda vivo y se traga el resto del segmento. Eso costó
  // 161 avisos y −46 bloques en `ad05-g25` (acta `yesno-sin-tag`), atribuidos
  // durante una ventana entera a un Quit&Save que NUNCA se disparó.
  | { type: "yesno"; resolve: (yes: boolean) => void; tag?: "guard-tribute" | "guard-arrest" | "troll-toll" }
  // Flow 3: '0'-'9', resto ignorado. `cancelKeys` (Camp, kernel 0x3dd6/0x3ddf):
  // Space y ESC resuelven 0 (que el flujo de Camp trata como cancelar).
  // `resolve` recibe TAMBIÉN el carácter crudo que resolvió: el eco del original es del
  // CARÁCTER (kernel 0x3dc6 `putchar [bp-2]`), y con `cancelKeys` los tres caminos de
  // cancelación (Espacio, '0', ESC) colapsan al mismo n=0 pero ecoan distinto. `key` es ""
  // cuando la tecla no existe en el original (ESC) ⇒ nada que ecoar. Quien no ecoe puede
  // ignorar el 2º parámetro. Esta capa NO imprime el eco: sólo dice qué se pulsó (ponerlo
  // aquí lo metería en TODOS los prompts de dígito del juego).
  | { type: "digit"; resolve: (n: number, key: string) => void; cancelKeys?: boolean }
  // Picker `select_party_member` (0x2d7a): reenvía la tecla CRUDA (flechas/1-N/Enter/
  // Space/0/ESC) al reductor del picker, que gestiona cursor/selección/cancelación.
  | { type: "party-select"; onKey: (key: string) => void }
  // `getstring` de consola (Yell / palabra de poder): buffer de texto ecoado en
  // vivo tras el prompt; Enter resuelve, ESC/vaciar+backspace cancela. `prefix` es
  // el eco fijo que precede al buffer ("Yell "); `buffer` lo acumula el reductor.
  //
  // `escKernel` — régimen FIEL del getstring del kernel (ULTIMA.EXE 0x3b1c, el
  // 0x7b9c que empujan TALK 0x0a37/Yell; resuelto vía dispatch_table, bolsa-39
  // §«0x7b9c+0xBF80=0x3b1c»), leído instrucción a instrucción:
  //   · la ÚNICA salida es CR — 0x3b7c `cmp si,0xd` / 0x3b7f `jne 0x3b30` (loop);
  //   · ESC (0x3b4e `cmp si,0x1b`) NO termina ni envía: con buffer no vacío BORRA
  //     lo tecleado (0x3b57 `push di; call 0x1fa0` repinta, 0x3b5b `sub di,di`
  //     vacía) y con buffer vacío es no-op (0x3b53/0x3b55) — el bucle SIGUE;
  //   · Backspace con buffer vacío se ignora (0x3b3f `or di,di` / 0x3b41 `je`).
  // Lo arma la CONVERSACIÓN (talk-console): en 1988 el «What is thy name?» del
  // trono de Blackthorn te RETIENE (label 4 re-pregunta sin contador) y ESC no es
  // válvula — el ESC-cancela del port ahí era divergencia (reporte usuario 24-08).
  // Los getstrings con vacío DERIVADO propio (Yell dungeon «Vacío/ESC→Nothing»
  // CMDS 0x1418, ceremonia CAST2 0x09cc, rumor de taberna) NO lo ponen y conservan
  // su `cancel` declarado.
  | { type: "text"; prefix: string; buffer: string; max: number; resolve: (text: string) => void; cancel?: () => void; escKernel?: boolean }
  // getnum de consola (cantidad de Mix "How much?"; CMDS 0x7c1e → kernel 0x3b9e,
  // arg 2): dígitos ecoados en vivo hasta `max` caracteres; un '+'/'-' SÓLO como
  // primer carácter (0x3be2, ocupa una casilla). Enter envía (vacío → 0). ESC
  // BORRA el buffer y sigue leyendo (0x3c0e) y backspace-en-vacío se ignora: el
  // getnum NO tiene cancelación, sólo Enter sale (A3-HF10,
  // re/notes/mix-hf10-command-parity.md §4). Réplica del de `mix_quantity` (0x1a70).
  | { type: "number"; prefix: string; buffer: string; max: number; submit: (n: number) => void }
  // getstring RÚNICO de consola (Cast/Mix): se teclea la INICIAL de cada sílaba
  // (A-Z, salvo J/O que no tienen runa) y se ecoa la palabra rúnica completa en
  // MAYÚSCULAS ("I"→"IN", "L"→"LOR"); máx 4 sílabas. Enter/Space envía, Backspace
  // borra la última, ESC envía VACÍO (cancela). Réplica de CAST2.OVL 0x00de (ver
  // re/notes/cast-input.md). `submit` recibe las iniciales acumuladas ("" = vacío).
  | { type: "rune"; prefix: string; initials: string; syllables: string[]; max: number; submit: (initials: string) => void }
  // Picker de ítems del comando READY (overlay de pergamino, `item_page_controller`
  // @0x0f2e). Reenvía la tecla CRUDA a `readyPickerKey`; el handler (openReadyPicker)
  // mueve el cursor, equipa in situ y cierra. El overlay lo pinta la piel desde
  // `snap.readyPicker`; este prompt sólo conduce las teclas.
  | { type: "ready-picker"; onKey: (key: string) => void }
  // `getkey` PELADO (kernel `ULTIMA.EXE:0x266c`): UNA tecla, sin bucle de reintento y sin
  // filtro de validez — el que usa Vas Rel Por en `CAST.OVL:0x0d06` para leer la fase.
  // 🔴 NO es un `digit` con otro nombre: `digit` RE-LEE la tecla no válida (getkey loop del
  // menú de Camp) y aquí una tecla no válida TERMINA el hechizo con "Failed!" (`0x0d1d`
  // `cmp …,0x31` / `0x0d23 cmp …,0x38` → `0x0d46 sub ax,ax`). Confundirlos deja al jugador
  // atrapado en un prompt que el original ya habría cerrado.
  // El ECO lo hace ESTA capa porque el binario lo hace dentro del mismo tramo y sólo para
  // caracteres imprimibles: `0x0d0c cmp al,0x20` / `jb` salta el `putchar` (0x0d13) — o sea
  // ESC (0x1b) no se ecoa — y el `\n` de `0x0d16` va SIEMPRE, imprimible o no.
  | { type: "getkey"; prefix: string; resolve: (key: string) => void }
  // Tienda por CONSOLA (piel fiel/shader): reenvía la tecla CRUDA al conductor
  // `ShopConsole` (menú por tecla del mercader). AUTORITATIVO sobre el prompt
  // como party-select/ready-picker: el conductor re-arma (deps.armKey), pide texto
  // (deps.armText, keyword de rumor) o cierra (deps.close). censo-ui-flujos §3.
  | { type: "shop"; onKey: (key: string) => void };

export interface PromptManagerDeps {
  /** Fila de eco viva del getstring/getnum (reescritura in-place del buffer). */
  hud: { echoSetLast(text: string): void };
}

export class PromptManager {
  private _current: PendingPrompt | null = null;
  private _seq = 0;

  constructor(private readonly deps: PromptManagerDeps) {}

  get current(): PendingPrompt | null {
    return this._current;
  }

  /**
   * Nº monotónico de prompts ARMADOS (cada `current = {…}` no-null lo avanza; cerrar con
   * null no). Existe para el instrumento (#374): en las cadenas de getstring del rito de
   * santuario el siguiente prompt se re-arma SÍNCRONO dentro del mismo keydown del Enter
   * —fiel al binario: CAST2 0x0a1b (getstring) → 0x0a56 → 0x0a0c encadena sin beat— así
   * que «current deja de ser text» no es observable entre eslabones; «hay un prompt
   * NUEVO» (seq avanzó) sí lo es. Cero efecto en estado: sólo se lee.
   */
  get seq(): number {
    return this._seq;
  }

  /** Arma (o cierra con null) el prompt vivo — gemelo del `pendingPrompt = …`. */
  set current(p: PendingPrompt | null) {
    if (p !== null) this._seq++;
    this._current = p;
  }

  /**
   * Reduce un keydown contra el prompt vivo. Devuelve true si había prompt (la
   * tecla queda CONSUMIDA — el getkey modal del binario se traga toda tecla,
   * válida o no). Sin prompt: false y la tecla sigue su curso.
   */
  handleKey(ev: KeyboardEvent): boolean {
    const p = this._current;
    if (!p) return false;
    ev.preventDefault();
    const { hud } = this.deps;
    if (p.type === "text") {
      // getstring de consola (Yell): teclea la palabra, se ecoa en vivo tras el
      // prefijo; Enter la envía, Backspace borra (y con buffer vacío cancela),
      // ESC cancela. Sólo caracteres imprimibles hasta `max` (0xF en el original).
      if (ev.key === "Enter") {
        this._current = null;
        p.resolve(p.buffer);
      } else if (ev.key === "Escape") {
        if (p.escKernel) {
          // Kernel 0x3b4e-0x3b5d: ESC BORRA la línea tecleada y el getstring SIGUE
          // leyendo — jamás envía ni cancela (la única salida es CR, 0x3b7f).
          p.buffer = "";
          hud.echoSetLast(p.prefix);
        } else {
          this._current = null;
          hud.echoSetLast(p.prefix); // deja el eco sin cursor colgando
          p.cancel?.();
        }
      } else if (ev.key === "Backspace") {
        if (p.buffer.length === 0) {
          if (!p.escKernel) {
            this._current = null;
            p.cancel?.();
          }
          // escKernel: BS en vacío se ignora (kernel 0x3b3f `or di,di` / 0x3b41 `je`).
        } else {
          p.buffer = p.buffer.slice(0, -1);
          hud.echoSetLast(p.prefix + p.buffer);
        }
      } else if (ev.key.length === 1 && ev.key >= " " && p.buffer.length < p.max) {
        p.buffer += ev.key;
        hud.echoSetLast(p.prefix + p.buffer);
      }
      return true;
    }
    if (p.type === "number") {
      // getnum del kernel (0x3b9e; cantidad de Mix vía CMDS 0x7c1e): dígitos
      // ecoados en vivo hasta `max`; '+'/'-' sólo en la 1ª casilla (0x3be2). Enter
      // envía: vacío (o un signo solo) → 0, que Mix trata como abortar (0x1b71).
      // ESC borra lo tecleado y SIGUE leyendo (0x3c0e; en vacío no hace nada) y
      // backspace-en-vacío se ignora (0x3c00): no hay tecla de cancelar (A3-HF10).
      if (ev.key === "Enter") {
        this._current = null;
        const digits = p.buffer.replace(/^[+-]/, "");
        const n = digits === "" ? 0 : Number(digits);
        p.submit(p.buffer.startsWith("-") ? -n : n);
      } else if (ev.key === "Escape") {
        if (p.buffer.length > 0) {
          p.buffer = "";
          hud.echoSetLast(p.prefix);
        }
      } else if (ev.key === "Backspace") {
        if (p.buffer.length > 0) {
          p.buffer = p.buffer.slice(0, -1);
          hud.echoSetLast(p.prefix + p.buffer);
        }
      } else if (
        p.buffer.length < p.max &&
        (/^[0-9]$/.test(ev.key) || ((ev.key === "+" || ev.key === "-") && p.buffer.length === 0))
      ) {
        p.buffer += ev.key;
        hud.echoSetLast(p.prefix + p.buffer);
      }
      return true;
    }
    if (p.type === "rune") {
      // getstring rúnico de Cast/Mix (CAST2.OVL 0x00de): teclea la INICIAL, se
      // ecoa la palabra rúnica en MAYÚSCULAS. Enter/Space envía (0x0d/0x20 →
      // done), Backspace borra la última sílaba (0x08), ESC vacía y envía
      // (0x1b → di:=0, done). Letras A-Z con runa y < max sílabas se aceptan;
      // 'J'/'O' y no-letras se ignoran (rechazo 0x00fa/0x0101 del binario).
      if (ev.key === "Enter" || ev.key === " " || ev.key === "Spacebar") {
        this._current = null;
        p.submit(p.initials);
      } else if (ev.key === "Escape") {
        this._current = null;
        p.submit(""); // ESC = di:=0 → retorno -1 ("None!")
      } else if (ev.key === "Backspace") {
        if (p.initials.length > 0) {
          p.initials = p.initials.slice(0, -1);
          p.syllables.pop();
          hud.echoSetLast(p.prefix + p.syllables.join(" "));
        }
      } else if (ev.key.length === 1 && p.initials.length < p.max) {
        const syl = runeSyllableForInitial(ev.key);
        if (syl) {
          p.initials += ev.key.toUpperCase();
          p.syllables.push(syl);
          hud.echoSetLast(p.prefix + p.syllables.join(" "));
        }
      }
      return true;
    }
    if (p.type === "getkey") {
      // Las teclas MODIFICADORAS sueltas (Shift/Control/Alt/Meta) no existen como byte en
      // el `getkey` de 1988 — el BIOS no las devuelve —, así que aquí tampoco resuelven:
      // no es tolerancia inventada, es que no hay tecla que leer. Todo lo demás sí.
      if (["Shift", "Control", "Alt", "Meta", "CapsLock"].includes(ev.key)) return true;
      this._current = null;
      // Eco del carácter (0x0d13) sólo si es imprimible (`cmp al,0x20 / jb`), y `\n`
      // SIEMPRE (0x0d16). `echoSetLast` reescribe la fila del prompt; el salto de línea lo
      // materializa el llamador al empujar su siguiente mensaje.
      const printable = ev.key.length === 1 && ev.key >= " ";
      hud.echoSetLast(p.prefix + (printable ? ev.key : ""));
      p.resolve(ev.key);
      return true;
    }
    if (p.type === "digit") {
      if (/^[0-9]$/.test(ev.key)) {
        this._current = null;
        p.resolve(Number(ev.key), ev.key);
      } else if (
        p.cancelKeys &&
        (ev.key === " " || ev.key === "Spacebar" || ev.key === "Escape")
      ) {
        // Camp: Space (0x3dc2 rompe el bucle de lectura → 0x3dd6 cancel) resuelve
        // 0, que el flujo de Camp trata como cancelar (0 ≡ Space). ⚠ ESC=cancelar
        // es QoL de input NO derivada (el getkey 0x3dbc sólo rompe con Space o
        // dígito; ESC no está decodificado) — inocua (equivale a '0').
        // El CRUDO los separa para el eco: Space es un carácter que el original ecoa
        // (" "), ESC no existe allí ⇒ "" = no ecoar nada inventado.
        this._current = null;
        p.resolve(0, ev.key === "Escape" ? "" : " ");
      }
      return true; // resto ignorado (getkey loop)
    }
    if (p.type === "party-select") {
      // Reenvía la tecla CRUDA al picker; `driveCampPrompt` (dentro de onKey) es
      // AUTORITATIVO sobre el prompt: mover → re-arma party-select; elegir/cancelar
      // → cierra (lo pone a null y arranca el sueño). No lo tocamos aquí.
      p.onKey(ev.key);
      return true;
    }
    if (p.type === "ready-picker") {
      // Overlay del picker de Ready: reenvía la tecla cruda al reductor
      // (`readyPickerKey`). El handler (openReadyPicker) es AUTORITATIVO sobre
      // el prompt: mover repinta, equipar redibuja, ESC/vanish cierra (lo pone
      // a null). No lo tocamos aquí.
      p.onKey(ev.key);
      return true;
    }
    if (p.type === "shop") {
      // Tienda por consola (piel fiel/shader): reenvía la tecla CRUDA al conductor
      // `ShopConsole`, AUTORITATIVO sobre el prompt (re-arma / pide texto /
      // cierra). Space/Escape los interpreta el conductor como "salir" (despedida).
      p.onKey(ev.key);
      return true;
    }
    const k = ev.key.toLowerCase();
    if (k === "y") {
      this._current = null;
      p.resolve(true);
    } else if (k === "n") {
      this._current = null;
      p.resolve(false);
    } else if (ev.key === "Escape" && p.type === "yesno-esc") {
      this._current = null;
      p.resolve(false);
    }
    // yesno: ESC ignorado (Flow 2). Cualquier otra tecla → ignorada.
    return true;
  }
}
