/**
 * Bootstrap: carga assets, crea el juego y conecta render + input + HUD.
 */
import type { TalkScript } from "./core/dialogue/conversation.js";
import { TalkScriptRegistry } from "./core/dialogue/registry.js";
import { Game, type GameData, type GameEvent, type RefugeScript } from "./core/game.js";
import { mapDisplayName } from "./core/location-display.js";
import {
  campPromptStart,
  bedHours,
  type CampPromptStep, type CampPromptCtx,
} from "./core/campPrompt.js";
import {
  driveCampPrompt as runCampPromptStep,
  type CampPromptDriverDeps,
} from "./core/campPromptDriver.js";
import { traceSprayRays, sprayColorForMode } from "./skin/fiel/combat.js";
import { blocksSpellLine } from "./core/magic/areaSpellTables.js";
import { NpcManager, type NpcSlot } from "./core/npc/manager.js";
import { cargaFielActiva } from "./core/npc/carga-fiel.js";
import { joinByName, effectiveName } from "./core/party.js";
import { testHookInnLeave } from "./debug/debugApi.js";
import {
  autosave,
  loadGame,
  loadMostRecentSave,
  mostRecentSaveId,
  slotDeLaUltimaCarga,
} from "./core/persistence.js";
// La foto se escribe por `save-keys`, que es donde vive su clave (y su lector). Ver el
// porqué en la cabecera de esa función: un `setItem` con el prefijo aquí sería la segunda
// copia, y el día que cambiara daría cero fotos en silencio.
import { writeSaveShot } from "./core/save-keys.js";
import { importNativeSave, type SaveSidecar } from "./core/saveNative.js";
import { applyGypsyCreation, type ExtractedInitialState, type GameState, type WorldObject } from "./core/state.js";
import { DoorManager } from "./core/world/doors.js";
import { SHOP_CLOSED_MESSAGE, shopIsOpen } from "./core/world/shop-hours.js";
import type { SmallMapLocation, WorldData } from "./core/world/map.js";
import type { Direction } from "./core/world/movement.js";
import type { IntentSink } from "./skin/api.js";
import { sfxForCombatEvent } from "./core/sfx.js";
import { CoreViewImpl } from "./skin/coreview.js";
import { initDebugMenu } from "./debug/index.js";
import { DebugPanel } from "./debug/panel.js";
import { buildShellCategories, buildShellSections } from "./ui/shell/sections.js";
import { instalaConsentimiento } from "./web/arranque.js";
import { EV } from "./web/eventos.js";
import { companionAvailable, probeCompanion } from "./ui/shell/companion.js";
import { mountGearButton, gearTitle } from "./ui/shell/gear.js";
import { mountSkinSwitcher } from "./ui/shell/skinSwitcher.js";
import { mountShellToolbar } from "./ui/shell/shellToolbar.js";
import { ts } from "./i18n/shell.js";
import { applyShellTheme, shellThemeFor, setShell8x8 } from "./ui/shell/theme.js";
import { syncPixelFont, syncPixelFontSaves, syncPixelFontReplays } from "./ui/shell/pixelfont.js";
import { mountOriginalFrame } from "./ui/shell/originalFrame.js";
import { SpeakerAudio, BLOCKING_CUES, cueDurationMs, wellDoneInvertWindowMs, donationInvertWindowMs } from "./skin/fiel/speaker.js";
import { planTurnPhase } from "./skin/turn-phase.js"; // ★ #208 — la fase del prefijo cortado (flushEventPrefix)
import { FaithfulSkin } from "./skin/fiel/skin.js";
import { ShaderSkin } from "./skin/shader/skin.js";
import type { HostableSkin } from "./skin/hostable.js";
import {
  PortraitSkin,
  esPantallaTactil,
  layoutPartidoBandera,
  layoutPartidoDisponible,
  layoutPartidoInicial,
  reflowFlag,
} from "./skin/portrait/skin.js";
import {
  alternarLadoCursores,
  guardarLayoutPartido,
  ladoCursoresEsElVivo,
  layoutPartidoGuardado,
} from "./skin/portrait/deck-nativo.js";
import {
  installBotonesUi,
  installLayoutToggleButton,
  installWideDeck,
  uninstallBotonesUi,
  uninstallWideDeck,
} from "./skin/portrait/deck-ancho.js";
import { installPortraitDeckDom } from "./skin/portrait/deck-dom.js";
import {
  chapaEnhancedViva,
  enhancedControlsActivo,
  enhancedControlsDisponible,
  enhancedControlsGuardado,
  guardarEnhancedControls,
} from "./enhanced/mode.js";
// TAP CONTEXTUAL del mundo (chapa Enhanced): tocar un NPC adyacente y VISIBLE inicia el
// comando (T)alk hacia él sintetizando las mismas dos teclas que teclea una persona. La
// lógica vive ENTERA en `enhanced/context/` (fachada de sólo lectura + reglas puras);
// aquí sólo se cablea al sink de intents, que es el punto más estrecho del flujo de tap.
import { manejarTapContextual } from "./enhanced/context/actions.js";
// INTERFAZ DE LANZAMIENTO (Clásico / Lista de hechizos) — capa QoL ADITIVA: la lista
// sustituye el paso de TECLEAR las iniciales rúnicas y nada más. Toda la mecánica de magia
// sigue viviendo en `core/magic/` y se alcanza por el MISMO getstring del original.
import { castingUiGuardado, guardarCastingUi, type CastingUi } from "./enhanced/spells/mode.js";
import { buildSpellCatalog } from "./enhanced/spells/catalog.js";
import { makeCastSpellEntry } from "./enhanced/spells/entry.js";
import { spellPickerOpen } from "./enhanced/spells/panel.js";
import { closePartyChooser, syncPartyChooser } from "./enhanced/party/panel.js";
// INTERFAZ DE TIENDA (auditoría de entrada manual, fase A): el prompt `{type:"shop"}` es
// un getkey CRUDO y por eso nunca alzó hoja del deck. El régimen Modern le pone una
// superficie —una fila por opción que la fase VIVA ya expone— que sintetiza la MISMA
// tecla por `press()`. El interruptor es una preferencia pura, como `castingUi`.
import { closeShopPanel, syncShopPanel } from "./enhanced/shop/panel.js";
import {
  guardarShopUi,
  panelDeTiendaActivo,
  shopUiGuardado,
  type ShopUi,
} from "./enhanced/shop/mode.js";
import {
  mountEnhancedChrome,
  type EnhancedChromeHandle,
} from "./enhanced/mobile/chrome.js";
// ERGONOMÍA DE LOS MANDOS MÓVILES (carril 12-09): dónde cae la cruceta y qué hay en las
// cuatro acciones rápidas. Los dos son preferencias PURAS con su propia persistencia; aquí
// sólo se cablean al panel de ajustes, que es la raíz de composición de siempre.
import { loadPadPos, setPadPos, type PadPos } from "./enhanced/mobile/padpos.js";
import {
  loadQuickWorld,
  quickOptions,
  setQuickSlot,
} from "./enhanced/mobile/quickslots.js";
import { SkinManager, type SkinChoice } from "./skin/manager.js";
import { MusicPlayer, introPageContext, type LocationMusicInput } from "./ui/music.js";
import { SavePanel } from "./ui/savepanel.js";
import { showEndgameScroll } from "./ui/endgame-overlay.js";
import { PromptManager } from "./ui/prompt-manager.js";
import { TalkConsole } from "./ui/talk-console.js";
import { createPickers } from "./ui/pickers.js";
import { questScroll } from "./core/quest/endgame.js";
import type { EndgameText } from "./core/endgame/sequence.js";
// Conductores de escena/pacers extraídos de boot() (TRAMO 1 del refactor
// estructural, auditoría MANT-1/ARQ-2): cada uno posee su estado de ciclo de
// vida (timers/flags) con deps inyectadas y es testeable por vitest.
import { AutoWalk } from "./ui/autowalk.js";
import { snapAttackCell } from "./ui/attack-snap.js";
import { MoongateTransitGate } from "./ui/moongate-gate.js";
import { CombatPacer } from "./ui/combat-pacer.js";
import { CampSleep } from "./ui/camp-sleep.js";
import { BedSleep } from "./ui/bed-sleep.js";
import { RitualInvert } from "./ui/ritual-invert.js";
import { TrollSneak } from "./ui/troll-sneak.js";
import { ShrineScenePacer } from "./ui/shrine-scene.js";
import { BlackthornScenePacer } from "./ui/blackthorn-scene-pacer.js";
import { ShrineKeyPacer } from "./ui/shrine-key-pacer.js";
import { PoisonTick, POISON_BLIP_MS } from "./ui/poison-tick.js";
import { EndgamePacer } from "./ui/endgame-pacer.js";
import { isModalOpen, promptCursorOnLiveRow } from "./ui/awaiting-gate.js";
import { SelectorPanel } from "./ui/selector.js";
import { type ShopData } from "./ui/shop.js";
import { ShopConsole, type ShopArmsRowInput } from "./ui/shop-console.js";
import { initShopArmsPicker, shopArmsKey } from "./core/shops/shopArmsPicker.js";
import { initInnRegister, innRegisterKey } from "./core/shops/innRegisterPicker.js";
import { type QuestionData } from "./ui/creation.js";
import { FaithfulIntro, type FaithfulIntroText } from "./ui/faithful-intro.js";
import { buildSpellDefs, matchSpellByInitials, type SpellDef } from "./core/magic/spells.js";
// LA CEREMONIA `CAST2.OVL:0x0000` — el jingle-con-inversión-de-viewport común a las cuatro
// vías de magia del binario (40 sitios de llamada, censo en el módulo). El cue histórico se
// llama `time-spell` porque el port sólo lo conocía por el pergamino de tiempo; la rutina es
// la MISMA para el (C)ast, la poción y los pergaminos.
import {
  VAS_REL_POR_PHASE_CEREMONY_INDEX,
  VAS_REL_POR_SPELL_INDEX,
  castCeremonyIndexOrNull,
  potionCeremonyIndex,
  scrollCeremonyIndex,
} from "./core/magic/ceremony.js";
import {
  buildMixReagentRows,
  initMixReagentPicker,
  mixPickerFooterLines,
  mixReagentKey,
  reagentBit,
  type MixReagentPickerModel,
  type MixReagentRow,
} from "./core/magic/mixReagentPicker.js";
import { castSpell, applyMani, applyVasMani, applyCure, applyAwaken, applyResurrect, inWisPeerText, DEATH_VISION_FRAMES, type CastEffect } from "./core/magic/cast.js";
import { PAUSE_UNIT_MS } from "./skin/world-fx.js";
import { CombatRng, combatDistance } from "./core/combat/formulas.js";
import { OriginalRng } from "./core/rng-original.js";
import { isItemEquipped } from "./core/equip.js";
import {
  buildReadyRows,
  initReadyPicker,
  readyPickerKey,
  type ReadyPickerModel,
} from "./core/readyPicker.js";
import {
  buildUseRows,
  initUsePicker,
  usePickerKey,
  type UsePickerModel,
  type UsePickerRow,
} from "./core/usePicker.js";
import { rerollPotionColor, applyPotionEffect, applyPotionCombatSync } from "./core/usePotion.js";
import { readScroll, windForDirection } from "./core/useScroll.js";
import {
  TouchControls,
  setExpectedInput,
  refreshTouchDeck,
  swapPadSide,
  padSideOfrecible,
  padSideVivo,
  syncShellExpanded,
  type ExpectedInput,
} from "./ui/touch.js";
// #333 A1: el layout partido sigue al RÉGIMEN en caliente (2-en-1) — misma primitiva
// viva de #334 que ya consumen el deck y los FAB del shell.
import { onCambioRegimenTactil } from "./ui/regimen-tactil.js";
import magicDefsJson from "./core/data/MagicDefinitions.json";
import inventoryDetailsJson from "./core/data/InventoryDetails.json";
import shortEquipNamesJson from "./core/data/shortEquipNames.json";
import additionalEnemyFlagsJson from "./core/data/AdditionalEnemyFlags.json";
import { buildEnemyDefs, combatCastAbsorbed, combatCastEffect, COMBAT_ABSORBED_MESSAGE } from "./core/combat/index.js";
import { shoppeKeeperAt, type ShopType, type ShoppeKeeperMapEntry } from "./core/shops/shops.js";
import { CMD_STRINGS, ATTACK_ON_FOOT, BLACKTHORN_UI, COMBAT_STRINGS, DIR_WORDS, GATE_TRAVEL_PROMPT, MIX_UI, READY_UI, SHRINE_UI, TALK_UI, USE_UI, WELL_UI, YELL_UI, readyRejectLines } from "./core/world/cmd-strings.js";
import shoppeKeeperMapJson from "./core/data/ShoppeKeeperMap.json";
import { setLang, getLang, onLangChange, AVAILABLE_LANGS, tf, t } from "./i18n/index.js";
import { mountLanguageSwitcher } from "./ui/shell/languageSwitcher.js";
import { KeyRecorder } from "./replay/recorder.js";
import { ReplayPlayer } from "./replay/player.js";
import { getLog } from "./replay/store.js";
import { captureScreenshot } from "./ui/screenshot.js";
import { refrescarMiniatura } from "./ui/shot-refresh.js";
import { mountReplayUi, type ReplayUiHandle } from "./ui/replay-ui.js";
import { avisaSiLaExtraccionEsVieja } from "./web/extraccion.js";
import { instalaDsStrings, DS_STRINGS_ASSET, type DsStrings } from "./core/data/ds-strings.js";

async function fetchJson<T>(url: string): Promise<T> {
  const res = await fetch(url);
  if (!res.ok) {
    throw new Error(
      `No se pudo cargar ${url} (${res.status}). ¿Has ejecutado \`npm run extract\`?`,
    );
  }
  return (await res.json()) as T;
}

/** Preferencia de piel persistida por F9 (localStorage). Ver la decisión de arranque. */
const SKIN_STORAGE_KEY = "u5.skin";

/**
 * Etiquetas legibles de las pieles — fuente ÚNICA para el registro del SkinManager,
 * el menú SISTEMA y el switcher (en juego y en la intro). dev queda fuera de las
 * ofrecidas al usuario (task #79) pero conserva etiqueta para el display.
 */
const SKIN_LABELS: Record<string, string> = {
  faithful: "1988 (fiel)",
  shader: "Shader (xBR)",
};
/** Pieles user-facing ofrecidas por el switcher DURANTE la intro (dev jubilada). */
const INTRO_SKIN_CHOICES: { id: string; label: string }[] = [
  { id: "faithful", label: SKIN_LABELS.faithful! },
  { id: "shader", label: SKIN_LABELS.shader! },
];

/**
 * Deps del switcher de IDIOMA (i18n F1), compartidas por el FAB de la intro y el
 * de juego. Fuente única = el módulo i18n (AVAILABLE_LANGS/getLang/setLang). El
 * cambio PERSISTE (elección real del usuario, a diferencia del override `?lang`).
 */
const LANG_SWITCHER_DEPS = {
  choices: () => AVAILABLE_LANGS.map((l) => ({ code: l.code, label: l.name, seed: l.seed })),
  currentCode: () => getLang(),
  selectLang: (code: string) => setLang(code),
};

function readSkinPref(): string | null {
  try {
    return typeof localStorage !== "undefined" ? localStorage.getItem(SKIN_STORAGE_KEY) : null;
  } catch {
    return null;
  }
}

function persistSkinPref(id: string | null): void {
  try {
    if (id && typeof localStorage !== "undefined") localStorage.setItem(SKIN_STORAGE_KEY, id);
  } catch {
    /* almacenamiento no disponible (modo privado/cuota): la elección vive sólo en memoria */
  }
}

/** Flechas para moverse; las letras quedan libres para los comandos del original. */
const KEY_DIRECTIONS: Record<string, Direction> = {
  ArrowUp: "north",
  ArrowDown: "south",
  ArrowLeft: "west",
  ArrowRight: "east",
};

/**
 * Comandos direccionales cuyo `getdir` (kernel 0x35EC) ECOA la palabra de dirección
 * tras el guión ("Open-North"). Verificados como llamadores de 0x35EC en el asm
 * (SJOG/CMDS/MAINOUT/LOOKOBJ). `look` SÍ va aquí: el despachador de (L)ook (ULTIMA.EXE
 * `kernel_cmd_dispatch` 0x3310) imprime "Look" (DS 0xa1a8) y a continuación `putchar('-')`
 * (0x3332: `mov ax,0x2d; call 0x16ba`) ANTES de invocar LOOKOBJ:0x099C, cuyo gate de
 * dirección (getdir, aborta si 0) ecoa la palabra → "Look-North" (re-derivado del asm;
 * el guión NO está en la DS pero SÍ lo imprime el despachador, y el testigo DOS del
 * usuario lo confirma). El guión de `look` lo añade el call-site (no está en su DS). Se
 * excluyen `talk`/`fire` (flujo town/exterior o munición aparte).
 */
const DIR_ECHO_COMMANDS = new Set<string>([
  "open", "get", "search", "jimmy", "push", "klimb", "attack", "look",
]);

async function boot(): Promise<void> {
  const parent = document.getElementById("app")!;
  // CONSENTIMIENTO (carril 1) — lo PRIMERO del boot, para que el visitante pueda
  // decidir mientras cargan los assets. Superficie "play": aquí la grabación de
  // sesión está PROHIBIDA por construcción (superficies.ts) porque esta página es
  // un canvas con arte de EA. Sin permiso —el caso por defecto— no se carga ningún
  // SDK y `evento()` descarta en silencio.
  const consentimiento = instalaConsentimiento({ superficie: "play" });
  // PUERTA DE VERSIÓN DE LA EXTRACCIÓN (#293) — si quien juega trae una extracción de /byo
  // anterior a un asset que este build ya usa, se le DICE, nombrando lo que falta y con la
  // acción de re-extraer. Antes eso era silencio: el asset caía a la red, el soft-404 (#63)
  // devolvía la home y el `.catch()` de más abajo degradaba sin decir nada (el santuario sin
  // escena que reportó el usuario el 14-08). No se espera a que termine —es una lectura de
  // Cache Storage y no tiene por qué retrasar la carga de los assets—, y no puede tumbar el
  // boot: el aviso aparece cuando esté, encima de lo que haya.
  void avisaSiLaExtraccionEsVieja({
    win: window,
    doc: document,
    idioma: getLang() === "es" ? "es" : "en",
  }).catch((err) => console.warn("[boot] puerta de version de la extraccion no evaluada:", err));
  try {
    const [overworld, underworld, smallMapsRaw, init, data, npcData] =
      await Promise.all([
        fetchJson<number[][]>("/assets/maps/overworld.json"),
        fetchJson<number[][]>("/assets/maps/underworld.json"),
        fetchJson<SmallMapLocation[]>("/assets/maps/smallmaps.json"),
        fetchJson<ExtractedInitialState>("/assets/initial-state.json"),
        fetchJson<GameData & Record<string, unknown>>("/assets/data.json"),
        fetchJson<Record<number, NpcSlot[]>>("/assets/npcs.json"),
      ]);
    const [towneTlk, dwellingTlk, castleTlk, keepTlk] = await Promise.all([
      fetchJson<TalkScript[]>("/assets/talk/towne.json"),
      fetchJson<TalkScript[]>("/assets/talk/dwelling.json"),
      fetchJson<TalkScript[]>("/assets/talk/castle.json"),
      fetchJson<TalkScript[]>("/assets/talk/keep.json"),
    ]);
    const questions = await fetchJson<QuestionData>("/assets/questions.json");
    // Mensajes del búfer DS 0xB21E (KARMA/MISCMSG/ENDMSG.DAT) — FICHA β. Va con los
    // OBLIGATORIOS y no con los best-effort de abajo A PROPÓSITO: sin él el port no
    // tiene los discursos de resurrección, el interrogatorio de Blackthorn ni las
    // lecciones del Códice, y esas escenas reventarían LEJOS de aquí (en mitad de una
    // partida) en vez de en el arranque con el aviso de `npm run extract`.
    instalaDsStrings(await fetchJson<DsStrings>(DS_STRINGS_ASSET));
    // Guion de la cinemática fiel (The Summoning + The Story). Best-effort: si el
    // extractor no ha corrido, la intro fiel salta las páginas sin romper el boot.
    const story = (await fetch("/assets/story.json")
      .then((r) => (r.ok ? (r.json() as Promise<string[]>) : []))
      .catch((err) => {
        // R7: aviso de una línea — sin él, una regresión del extractor degradaba
        // en silencio (la intro salta páginas y sólo se ve en careo visual).
        console.warn("[boot] story.json no cargado (la intro salta las páginas):", err);
        return [];
      })) as string[];
    // Datos del ENDGAME (#34): diálogo ENDMSG.DAT + narración END.DAT + mapa de la sala
    // MISCMAPS.DAT[528:704] (endgame.json del extractor). Best-effort: sin él, el cierre
    // cae al fallback viejo (mensajes + pergamino DOM), sin romper el boot.
    const endgameData = (await fetch("/assets/endgame.json")
      .then((r) => (r.ok ? r.json() : null))
      .catch((err) => {
        console.warn("[boot] endgame.json no cargado (el cierre cae al fallback viejo):", err);
        return null;
      })) as {
      scene?: { tiles?: number[][] };
      narration?: { pages?: string[] };
      dialogue?: { records?: string[] };
    } | null;
    // ESCENA del santuario / Codex (#277): las dos rejillas 11×11 de MISCMAPS.DAT
    // (shrine-scene.json del extractor). Best-effort como el endgame: sin el asset el
    // rito corre SIN escena (sólo texto) — la degradación previa a #277, no una rotura.
    const shrineSceneData = (await fetch("/assets/shrine-scene.json")
      .then((r) => (r.ok ? r.json() : null))
      .catch((err) => {
        console.warn("[boot] shrine-scene.json no cargado (el rito corre sin escena):", err);
        return null;
      })) as {
      shrine?: { tiles?: number[][] };
      codex?: { tiles?: number[][] };
      capture?: { tiles?: number[][] };
    } | null;
    const shrineScenes =
      shrineSceneData?.shrine?.tiles && shrineSceneData?.codex?.tiles
        ? { shrine: shrineSceneData.shrine.tiles, codex: shrineSceneData.codex.tiles }
        : undefined;
    // #324: la SALA DEL TRONO de la captura (MISCMAPS.DAT[0:176], mismo asset). Clave
    // nueva del extractor: un asset viejo la trae ausente → la captura corre sin escena
    // (sólo texto, la degradación previa a #324), sin romper el boot.
    const captureScene = shrineSceneData?.capture?.tiles;
    const endgameText: EndgameText | undefined = endgameData?.dialogue?.records
      ? { dialogue: endgameData.dialogue.records, narration: endgameData.narration?.pages }
      : undefined;
    const endgameRoom: number[][] | null = endgameData?.scene?.tiles ?? null;
    // Pool de textos de SHOPPE.DAT (saludos 1-de-4 del mercader, carril saludos-shoppe).
    // Material de EA → viaja como asset extraído, jamás como literal tracked. Best-effort:
    // sin él, ShopConsole degrada al encabezado `${shop}` + `${keeper} says:` sin romper.
    const shoppeTexts = (await fetch("/assets/shoppe.json")
      .then((r) => (r.ok ? (r.json() as Promise<string[]>) : null))
      .catch((err) => {
        console.warn("[boot] shoppe.json no cargado (saludos de tienda degradados):", err);
        return null;
      })) as string[] | null;

    const world: WorldData = {
      overworld,
      underworld,
      smallMaps: new Map(smallMapsRaw.map((l) => [l.id, l])),
    };

    const [signs, combatMaps, dungeons, look2] = await Promise.all([
      fetchJson<{ location: number; floor: number; x: number; y: number; text: string }[]>(
        "/assets/signs.json",
      ),
      fetchJson<unknown[]>("/assets/maps/combatmaps.json"),
      fetchJson<unknown[]>("/assets/maps/dungeons.json"),
      fetchJson<string[]>("/assets/look2.json"),
    ]);

    const game = new Game(
      init,
      world,
      {
        locationsX: data.locationsX,
        locationsY: data.locationsY,
        locationNames: data.locationNames ?? [],
        moonPhases: data.moonPhases as number[] | undefined,
        searchObjects: data.searchObjects as never,
        signs,
        look2,
        wordsOfPower: data.wordsOfPower as string[],
        shrines: {
          virtues: data.virtues as string[],
          mantras: data.mantras as string[],
          shrineX: data.shrineX as number[],
          shrineY: data.shrineY as number[],
        },
        shrineScenes, // #277: rejillas de MISCMAPS para la escena del rito
        captureScene, // #324: la sala del trono de la captura (MISCMAPS[0:176])

        shardSpawns: data.shardSpawns as { x: number; y: number; z: number }[] | undefined,
      },
      undefined,
      {
        npcManager: new NpcManager(npcData),
        doors: new DoorManager(),
        talkScripts: new TalkScriptRegistry({
          towne: towneTlk,
          dwelling: dwellingTlk,
          castle: castleTlk,
          keep: keepTlk,
        }),
        combatResources: {
          combatMaps: combatMaps as never,
          enemyDefs: buildEnemyDefs(
            {
              enemyStats: data.enemyStats as number[][],
              enemyFlags: data.enemyFlags as number[][],
              enemyAttackRange: data.enemyAttackRange as number[],
              monsterNamesMixed: data.monsterNamesMixed as string[],
              monsterNamesUpper: data.monsterNamesUpper as string[],
            },
            additionalEnemyFlagsJson as never,
          ),
          attackValues: data.attackValues as number[],
          attackRangeValues: data.attackRangeValues as number[],
          defenseValues: data.defenseValues as number[],
        },
        dungeons: dungeons as never,
        // #34 (ruling wiring): el texto del endgame se INYECTA en la construcción de
        // Game (como combatResources); el desvío de absorción (#179, fireAbsorptionEndgame)
        // emite el guión completo con él.
        endgameText,
      },
    );

    // Deep-link de desarrollo: ?loc=0&x=84&y=108&floor=0 (solo modo dev)
    if (import.meta.env.DEV) {
      const params = new URLSearchParams(window.location.search);
      if (params.has("x") && params.has("y")) {
        const location = Number(params.get("loc") ?? 0);
        game.state.position = {
          location,
          floor: Number(params.get("floor") ?? 0),
          x: Number(params.get("x")),
          y: Number(params.get("y")),
        };
        if (location !== 0) {
          game.npcManager?.enterMap(location, game.state);
          // Entrada fresca (deep-link): siembra los objetos de interior del .NPC como en
          // checkLocationEntry (cofres del castillo, etc.) para depuración y E2E (task #3).
          game.hydrateInteriorObjects(location);
        }
      }
      if (params.has("hour")) game.state.time.hour = Number(params.get("hour"));
      // La posición y la hora del deep-link se fijan fuera de loadSmallMap: resincroniza
      // la capa horaria de reja/puente (TOWN 0x0170) para la planta/hora resultantes.
      game.refreshHourTiles();

      // Hooks de test (solo DEV): estado real + siembra del stream vivo (F.2).
      // `addWorldObject` empuja un objeto estacionario (cofre/antorcha/nave) en la capa
      // de objetos del mundo (g_world_objects 0x5C5A, F1.5): permite a los E2E sembrar
      // un cofre/antorcha/nave-en-el-muelle de forma determinista y abordarlo/abrirlo
      // ORGÁNICAMENTE con B/O/G. Reemplaza al viejo `boardTile` fantasma (F1.2), que
      // fingía estar en un transporte SIN objeto del mundo — ya innecesario: la nave se
      // adquiere de verdad (shipwright → spawnDockShip) o se siembra como objeto aquí.
      (window as unknown as Record<string, unknown>).__u5test = {
        game,
        state: () => game.state,
        reseed: (seed: number) => game.reseed(seed),
        addWorldObject: (obj: WorldObject) => {
          game.state.worldObjects ??= [];
          game.state.worldObjects.push(obj);
        },
        // Hook e2e read-write: une un companion por nombre LLAMANDO al core
        // (core/party.joinByName), no una réplica — así el arnés espejo-tour recluta el
        // join del LP sin depender del Talk direccional (frágil bajo deriva de posición).
        join: (name: string) => joinByName(game.state, name),
        // Hook e2e read-write GEMELO del join: DEJA A UN COMPAÑERO en la location actual
        // llamando al CORE (`core/shops.innLeave` = calco de SHOPPES3 0x02AE-0x047D, la
        // tecla L del posadero: partyStatus=location, monthsAtInn=0, party−1) — NO una
        // réplica. El arnés del ESPEJO lo necesita para los SWAPS de party del LP2 de AD
        // (Julia→Mariah en la Haunting Inn de Skara Brae, Iolo en Britain): sin soltar
        // hueco, el `join` del recluta siguiente rebota con el «no room for me» del
        // binario (`party.ts` PARTY_FULL, DS 0x9348+0x9372 — antes aquí ponía «Thy party
        // is full.», cadena fabricada que 58413c38 retiró) y el ledger de party
        // divergiría del LP el resto de la cadena.
        // El CUERPO vive en debug/debugApi.ts (`testHookInnLeave`) para que el test lo
        // ejerza — con la GUARDA `location !== 0` de la ficha E1 (espejo-auditor 21-08):
        // fuera de settlement la tecla L del posadero no existe y partyStatus=0 es el
        // sentinel «en party», así que ejecutar el leave ahí corrompía el roster
        // (partySize=5 con 6 activos, caso ad13-g21). Ver el docstring de la función.
        innLeave: (name: string) => testHookInnLeave(game.state, name),
      };
      if (params.has("seed")) game.reseed(Number(params.get("seed")));
    }

    // ── Journey Onward = recargar la partida guardada (equivalente de SAVED.GAM) ──
    // El menú de portada del original recarga SAVED.GAM verbatim en "Journey Onward"
    // (INTRO.OVL intro_main_controller 0x0986; re/notes/gypsy.md:11-13); "Create New
    // Character" (la gitana) lo (re)escribe. Nuestro SAVED.GAM = el save MÁS RECIENTE
    // (autosave rotatorio de (Q) o slot manual, por timestamp). Sin save → arranque
    // fresco (como el SAVED.GAM por defecto que el disco original trae). Se restaura
    // IN-PLACE sobre game.state (misma técnica que applyLoadedState) ANTES de crear la
    // vista, así el primer render ya pinta la partida cargada.
    const bootParams = new URLSearchParams(window.location.search);
    // `?fresh` fuerza partida nueva (guard de determinismo del tour/pixeldiff/e2e); el
    // deep-link DEV (?x&?y) posiciona a mano, así que también salta la restauración.
    //
    // `?save=<id>` elige UNA partida concreta en vez de la más reciente, y `?replay=<id>`
    // arranca en una repetición. Los dos entran desde la landing `/byo`, que lista lo que
    // hay en este navegador: sin ellos, «continuar» sólo podía significar «la última», y
    // una lista de cinco partidas con un único destino es una lista que miente.
    // ⚠️ `?replay` NO restaura ningún save: el reproductor deja el juego en el ANCLA del
    // registro (estado serializado + semilla), así que cargar antes otra partida sería
    // trabajo que se pisa a sí mismo — y un parpadeo de la partida equivocada.
    const savedGameId = bootParams.get("save");
    const replayId = bootParams.get("replay");
    // ── `?embed=1`: el juego dentro del popover de /byo ───────────────────────────
    // 🔴 REUSA la supresión que YA existe (`u5shell-fab-suppressed`, shellToolbar.ts:29:
    // engranaje + selector de piel + selector de idioma). Inventar un segundo mecanismo
    // de ocultación daría dos sitios que decidir mantener y uno que se olvidaría.
    // ⚠️ La BARRA DE TRANSPORTE de la repetición NO entra en esa clase, y tampoco se
    // queda fija encima del vídeo: tiene un régimen PROPIO (`embedded` de replay-ui.ts,
    // clase `u5-replay-bar--limpia`) porque lo que se pide de ella no es «ocultar» sino
    // «apartarse y volver». Feedback del usuario sobre el popover ya desplegado:
    // «replays deberían no mostrar botones, solo UI». Suprimirla del todo como a los FAB
    // dejaría al visitante sin pausa ni velocidad, y meterla en `u5shell-fab-suppressed`
    // habría cambiado el SENTIDO de esa clase (que es «tapa un popup, escóndete») para
    // todos sus demás usuarios. Sigue sin hacer falta API nueva hacia fuera: los
    // controles viven DENTRO del iframe, donde ya estaban probados; sacarlos al cromo
    // del popover obligaría a inventar un protocolo `postMessage` y a duplicar fuera el
    // estado del reproductor.
    //
    // 🔴 Y SALTA LA CINEMÁTICA POR SÍ SOLO (ver `skipIntro`, más abajo). ESTO NO ARREGLA
    // UN ROJO VIVO: el popover ya pasa `&nointro` en su `src` (popover-replay.ts:74) y
    // por tanto funciona. Lo que cierra es que `embed` DEPENDÍA de que su llamador se
    // acordara de un SEGUNDO parámetro, y está medido lo que pasa cuando no se acuerda:
    // abriendo `?replay=<id>&embed=1` a secas y esperando 25 s (quince veces el arranque
    // medido, 1,6 s) el estado se queda en `introPhase="attract"` — los logos de 1988 —,
    // la barra de transporte NO existe en el DOM y la posición es la de partida nueva
    // (x=15) en vez de la del ancla (x=26). Causa: `replayPlayer.load()` vive DETRÁS del
    // `await intro.run()`, y suprimir los FAB por CSS no toca esa espera; encima, sin FAB
    // tampoco hay con qué navegar el menú que aparece.
    // O sea: el modo embebido tenía una precondición no escrita. Ahora la cumple él.
    const embebido = bootParams.get("embed") === "1";
    if (embebido) {
      document.body.classList.add("u5shell-fab-suppressed");
    }
    // ── `?replay=<id>`: SÓLO LA UI DE 1988, y el alcance NO es la botonera ────────────
    // Reporte del usuario (09-08) sobre el popover ya desplegado: «replays sigue mostrando
    // todo el ui con botonera port. tiene que ser solo el ui original del juego». El
    // requisito es por EXTENSIÓN —cualquier capa que no sea del juego de 1988 sobra—, así
    // que el gate se pone en las CAPAS, no en la que se reportó.
    //
    // 🔴 `u5shell-fab-suppressed` (arriba) NO bastaba, y el censo en un iPhone emulado
    // (390×844) dice por qué: esa clase tapa los TRES FAB del shell y nada más. Lo que se
    // veía en la repetición eran otras DOS capas, de dos dueños distintos, que nadie había
    // clasificado como cromo:
    //   · el DECK TÁCTIL (`ui/touch.ts`, 270 px = 32 % del alto del móvil): cruceta, rejilla
    //     de comandos, fila de utilidades, ☰ y ⛶. Se monta por `(pointer: coarse)`, sin
    //     mirar si hay repetición;
    //   · el ENVOLTORIO PORTRAIT (`skin/portrait/`), que además de re-componer la pantalla
    //     de 1988 en dos bloques trae la piel de botones y el ▤.
    // Los FAB estaban suprimidos y las otras dos no: por eso el arreglo del embebido parecía
    // hecho y el usuario seguía viendo botones.
    //
    // ⚠ Y NINGÚN BOTÓN DEL DECK HACÍA NADA. Con una repetición cargada `replayActive()` es
    // cierto hasta que se descarga, y `handleGameKey` (más abajo) suelta toda tecla que no
    // venga del reproductor. O sea: la botonera no era sólo ruido visual, era una rejilla de
    // botones muertos encima de la partida que se está viendo.
    //
    // EL PREDICADO ES LA URL, NO LA PREFERENCIA: nada de esto escribe `u5.skin` ni
    // `u5.reflow`. Es un régimen de ESTA CARGA — la siguiente sin `?replay` vuelve a la piel
    // y al layout del usuario intactos, sin tener que restaurar nada.
    const soloUiOriginal = replayId !== null;
    const forceFresh = bootParams.has("fresh") || bootParams.has("x") || replayId !== null;
    const restoreMostRecentSave = (): boolean => {
      if (forceFresh) return false;
      // Un id que ya no existe (la partida se borró en otra pestaña) NO deja al jugador
      // en una partida nueva sin decir nada: se cae a la más reciente, que es lo que
      // hacía el arranque antes de que existiera el parámetro.
      let loaded: GameState | null = null;
      // La RANURA de la que sale el estado, para poder rehacer su miniatura si es vieja.
      // Se anota en las dos ramas porque el respaldo carga OTRA partida, no la pedida.
      let restauradoId: string | null = null;
      if (savedGameId) {
        try {
          loaded = loadGame(savedGameId);
          restauradoId = savedGameId;
        } catch {
          loaded = null;
        }
      }
      if (!loaded) {
        restauradoId = mostRecentSaveId();
        loaded = loadMostRecentSave();
      }
      if (!loaded) return false;
      Object.assign(game.state, loaded);
      if (game.state.position.location !== 0) {
        game.npcManager?.enterMap(game.state.position.location, game.state, true); // #108: rehidrata la maquina de caminata del save (npcWalk)
      }
      // Cargar = `town_load_map(fresh=0)` → `0x0408(0)` (ULTIMA.EXE 0x00f7 → TOWN 0x11F0): el
      // cargador pone el tracker de puerta abierta [0x594f] a 0 (0x041d) y relee la planta, así
      // que una puerta abierta al guardar vuelve CERRADA. Batch 24, H-162.
      game.doors?.reset();
      // Save restaurado in-place: resincroniza reja/puente por hora (TOWN 0x0170).
      game.refreshHourTiles();
      // Miniatura vieja (jpeg de antes del 08-08, o el teléfono entero de los layouts
      // táctiles): se rehace cuando el juego haya pintado. Adorno, sin `await` y sin poder
      // fallar hacia fuera — ver `ui/shot-refresh.ts`.
      if (restauradoId) void refrescarMiniatura(restauradoId, parent);
      return true;
    };

    // Idioma de arranque (capa i18n, F1). La preferencia persistida (localStorage
    // u5.lang) la lee el propio módulo al importarse; aquí sólo aplicamos el override
    // EFÍMERO de URL `?lang=es|en` (para e2e/dev, sin ensuciar la preferencia), igual
    // que `?skin`. Default sin nada = inglés (el suelo del calco). No toca el save
    // nativo: es pura preferencia de presentación (analisis.md §6).
    const urlLang = new URLSearchParams(window.location.search).get("lang");
    if (urlLang) setLang(urlLang, { persist: false });

    // Piel de arranque: la SMOOTH (shader = fiel + xBR) es el DEFAULT al cargar sin
    // parámetros (encargo del usuario 04-08: «siempre default skin smooth»). Sin
    // parámetro arranca igualmente en la CINEMÁTICA fiel 1988 (logos+título+menú+
    // Summoning+gitana, E1-S13): la cinemática la comparten las DOS pieles, así que el
    // defecto cambia lo que se SUAVIZA, no lo que se ve primero.
    //
    // PRECEDENCIA — y es una REGLA, no una constante:
    //   1. `?skin=faithful|shader` en la URL manda (es lo que pinchan todos los arneses).
    //   2. la preferencia PERSISTIDA por el switcher/F9 (localStorage u5.skin), en LOS
    //      DOS VALORES.
    //   3. si no hay nada, la SMOOTH.
    //
    // 🔴 EL PASO 2 DICE «LOS DOS VALORES» POR UN DEFECTO LATENTE QUE ESTE CAMBIO
    // DESPIERTA. Hasta hoy esta rama sólo honraba la preferencia cuando valía "shader";
    // una "faithful" se caía al `else`… que TAMBIÉN era "faithful", así que las dos vías
    // coincidían y la asimetría era invisible. Al pasar el `else` a smooth se despierta:
    // quien eligiera la piel FIEL con F9 la vería volver a smooth en cada recarga, con el
    // interruptor aparentemente roto. Es EL MISMO error, en el mismo sitio conceptual,
    // que el ▤ del layout tuvo el 28-07 — y la solución es la simétrica de aquélla
    // (`layoutPartidoInicial`: la preferencia manda en LOS DOS SENTIDOS).
    // La guarda viva de esta rama es el testigo que persiste `faithful` en
    // `faithful.spec.ts` / `intro-skin.spec.ts`: sin él, el defecto entra sin ponerse rojo.
    //
    // Jubilación fase 2: la piel dev se ELIMINA. `?skin=` sólo acepta faithful|shader;
    // una preferencia persistida "dev" (de un save viejo) cae al DEFECTO (hoy smooth),
    // sin romperse. Ambas pieles comparten la CINEMÁTICA fiel de arranque y el perfil de
    // audio (speaker, sin música enhanced).
    // VEREDICTO FINAL #23 del usuario: la «ventana 1988» (chrome EGA + fuente 8×8 IBM +
    // marco del UI original: banda ►título◄ + rizos de pergamino de esquina) es el DEFAULT
    // del shell en TODAS las pieles (fiel y shader), SIN flag. El antiguo `?shell8x8=2`
    // (variante C, banco de pruebas de esta ventana) ES ahora el default. El look VECTOR
    // redondeado queda como FALLBACK QA tras `?shellVector=1`.
    const shellVector =
      new URLSearchParams(window.location.search).get("shellVector") === "1";
    const shell8x8On = !shellVector; // EGA/8×8 en el drawer + panel de partidas
    const shellFrameC = !shellVector; // marco del UI original (banda + rizos)
    setShell8x8(shell8x8On);
    const urlSkin = new URLSearchParams(window.location.search).get("skin");
    const persistedSkin = readSkinPref();
    const initialSkin: "faithful" | "shader" =
      urlSkin === "faithful" || urlSkin === "shader"
        ? urlSkin
        : persistedSkin === "faithful" || persistedSkin === "shader"
          ? persistedSkin
          : "shader";
    // Piel a montar TRAS la intro. Arranca en `initialSkin`; el usuario puede cambiarla EN
    // VIVO durante la intro (F9 / switcher): la intro actualiza esta variable vía onChange.
    let bootSkin: "faithful" | "shader" = initialSkin;
    // PROTOTIPO re-flow VERTICAL (idea del usuario): bandera `?reflow=1` (decide por área),
    // `?reflow=force` (re-compone siempre, para medir), `?skin=portrait` (alias) o la
    // preferencia `u5.reflow`. Elige la VARIANTE (banda vs cuadrado); ya no decide sola si
    // se arranca partido — eso lo resuelve `layoutPartidoInicial` (ver abajo).
    // Ver `docs/portrait-reflow-estudio.md` y `skin/portrait/`.
    const reflowMode = reflowFlag(
      window.location.search,
      typeof localStorage !== "undefined" ? localStorage : undefined,
    );
    // LAYOUT PARTIDO POR DEFECTO EN MÓVIL (encargo del usuario 02-08). Las TRES entradas de
    // la decisión se miden AQUÍ, una vez, y las consume la función pura `layoutPartidoInicial`
    // en los cuatro sitios que antes tenían tres criterios distintos (`?? true` en el toggle
    // y en `selectSkin`, `?? reflowMode !== "off"` en el arranque). «Móvil» = puntero grueso,
    // MEDIDO — el mismo predicado con el que aparece el deck táctil, ver `esPantallaTactil`.
    const banderaPartido = layoutPartidoBandera(
      window.location.search,
      typeof localStorage !== "undefined" ? localStorage : undefined,
    );
    // 🔴 #336: el régimen táctil se LEE POR LLAMADA, no se cachea aquí. Era un `const` de
    // arranque —el cuarto decisor rancio de la ficha— y en un 2-en-1 que se pliega a
    // tableta a mitad de sesión el layout seguía decidiéndose con el puntero de hace una
    // hora, mientras el deck (migrado en #334) ya había cambiado: dos verdades a la vez.
    /** Lee la preferencia VIVA (puede cambiar durante la sesión) y resuelve la precedencia. */
    const quiereLayoutPartidoAhora = (): boolean =>
      layoutPartidoInicial(layoutPartidoGuardado(), banderaPartido, esPantallaTactil());
    // ?nointro (DEV): salta la CINEMÁTICA fiel y arranca directo en el mundo con la
    // piel fiel montada. Sólo para el arnés píxel-diff (task #26 fase 2): la captura
    // Playwright necesita el mundo renderizado de forma DETERMINISTA, sin navegar el
    // attract/menú. Equivale a elegir "Journey Onward" sin reproducir el cinemático.
    //
    // SIN gate de DEV desde el 26-07: la intro y el menú NO tienen controles táctiles
    // (ticket abierto), así que en un móvil el juego desplegado no se podía ni EMPEZAR —
    // la bandera era la única vía y en producción se ignoraba en silencio. El staging es
    // privado (basic-auth), la bandera no expone nada, y quien no la ponga ve el flujo
    // completo de siempre.
    //
    // 🔴 `?embed=1` LA SALTA TAMBIÉN, y hace REDUNDANTE (no inútil) el `&nointro` que el
    // popover de `/byo` ya pone en su `src`. Ver el porqué medido donde se lee el
    // parámetro, unas 130 líneas más arriba: `replayPlayer.load()` vive detrás del
    // `await intro.run()` de este bloque, así que un embebido sin `nointro` enseña los
    // logos de 1988 y no llega a cargar la repetición. Los dos parámetros dicen ahora lo
    // mismo desde dos sitios; el que manda es el que no se puede olvidar.
    const skipIntro = new URLSearchParams(window.location.search).has("nointro") || embebido;

    // PERFIL DE AUDIO por piel (task #27, `re/notes/audio-profile-1988.md`): la piel fiel
    // 1988 arranca SIN música de fondo — el DOS original en un PC estándar no tenía música,
    // sólo PC-speaker (la canción de Iolo, fanfarrias, etc. son SFX de speaker, y ésos SÍ
    // son fieles y siguen sonando). F7 permite encenderla en la fiel como opt-in EXPLÍCITO,
    // que persiste y gana al default del perfil.
    //
    // 🔴 SE CONSTRUYE AQUÍ, ANTES DE LA INTRO, Y ESO ES EL ARREGLO DE UN HUECO REAL. Vivía
    // detrás del `await intro.run()` de abajo, que BLOQUEA el arranque mientras dura la
    // portada: durante toda la cinemática y el menú de «Journey Onward» el reproductor no
    // existía todavía, así que la pantalla de título no podía sonar y el interruptor de
    // los ajustes no tenía a quién hablarle. Y el tema de portada (`theme.mid`) sólo se
    // pedía en UN sitio, el manejador de `game-won` — o sea, la música del título de
    // Ultima V sólo sonaba si te terminabas el juego. Reportado por el usuario, que tenía
    // toda la razón: la portada es lo PRIMERO que debería sonar.
    const music = new MusicPlayer(false);

    if (!skipIntro) {
      // Tema de portada durante la cinemática y el menú (`U5THEME.XMI` → `theme.mid`).
      // Con la música apagada (default de la piel fiel) esto sólo anota el contexto
      // pendiente y no suena; al encenderla con F7 arranca sola. Ver `MusicPlayer.play`.
      music.play("title");
      // Textos EXACTOS del binario (data.json): el menú y "Copyright…" del pool
      // introMenuU4Transfer (0x30f2), los prompts de creación del pool
      // textCreateCharCmdsCrt (0xa020). Se buscan por CONTENIDO (robusto a índices).
      const pools = (data.stringPools ?? []) as { name: string; strings: string[] }[];
      const poolOf = (name: string): string[] =>
        pools.find((p) => p.name === name)?.strings ?? [];
      const introPool = poolOf("introMenuU4Transfer");
      const createPool = poolOf("textCreateCharCmdsCrt");
      const find = (arr: string[], needle: string, fallback: string): string =>
        arr.find((s) => s.startsWith(needle)) ?? fallback;
      // Las 6 etiquetas del menú van consecutivas tras "Journey Onward".
      const journeyIdx = introPool.indexOf("Journey Onward");
      const menuOptions =
        journeyIdx >= 0
          ? introPool.slice(journeyIdx, journeyIdx + 6)
          : [
              "Journey Onward",
              "Create New Character",
              "Transfer from Ultima IV",
              "Ultima V Introduction",
              "Acknowledgements",
              "Return to the View",
            ];
      const introText: FaithfulIntroText = {
        menuOptions,
        selectPrompt: find(introPool, "Select", "Select: "),
        copyright: find(introPool, "Copyright", "Copyright 1988 Lord British"),
        namePrompt: find(createPool, "By what name", "By what name shalt thou be known?"),
        sexPrompt: find(createPool, "Art thou", "Art thou Male or Female? "),
        story,
        questions,
      };
      // Piel «shader»: la cinemática de intro pasa por el filtro xBR (#70b). La fiel
      // (y cualquier otra) la corre sin filtro (byte-idéntica al DOSBox, #64). El cambio
      // de piel funciona TAMBIÉN en la intro (task #79 ampliado): F9 la cicla y el
      // switcher directo la elige; `onChange` persiste y recuerda cuál montar al salir.
      const intro = new FaithfulIntro(parent, introText, {
        initial: initialSkin,
        onChange: (id) => {
          if (id === "faithful" || id === "shader") {
            bootSkin = id;
            persistSkinPref(id);
            introSwitcher?.refresh();
            // FASE 3: los FABs de la intro también se tematizan por piel (el drawer
            // no se muestra en intro, pero ◧/idioma sí). Sin gear (⚙) por #71.
            applyShellTheme(id);
          }
        },
      });
      // Tema inicial del shell para los FABs de la intro (piel de arranque).
      applyShellTheme(initialSkin);
      // Hook e2e SÓLO DEV, read-only: expone la fase de la cinemática de creación para
      // que el arnés Playwright conduzca el camino CANVAS de la gitana (que no tiene DOM
      // que observar) de forma determinista — poll de fase en vez de contar teclas. Se
      // adjunta al __u5test ya creado arriba (mismo bloque DEV). Cero efecto en estado.
      if (import.meta.env.DEV) {
        const hooks = (window as unknown as Record<string, Record<string, unknown>>).__u5test;
        if (hooks) hooks.introPhase = (): string => intro.currentPhase;
        // Opción resaltada del menú de portada (ficha #33): el resalte es vídeo inverso
        // pintado a canvas, así que sin este hook el arnés no puede ver si ▲/▼ movieron
        // el cursor. Read-only, DEV.
        if (hooks) hooks.introMenuSelected = (): number => intro.menuSelected;
      }
      // Switcher directo de piel sobre la intro (mismo componente que en juego). El
      // menú SISTEMA (⚙) NO se muestra en la intro (exclusión deliberada de #71); esto
      // es SÓLO la piel. Se desmonta al terminar la intro (el de juego se monta luego).
      const introSwitcher = mountSkinSwitcher(parent, {
        choices: () => INTRO_SKIN_CHOICES,
        currentId: () => intro.skinId,
        selectSkin: (id) => intro.selectSkin(id),
      });
      // Switcher de IDIOMA sobre la intro (decisión del usuario 2026-07-17): el idioma
      // afecta al menú/gitana, lo primero que se lee → debe poder elegirse ANTES del
      // menú. Cambio en caliente (persiste); el texto de la intro no se re-traduce
      // retroactivamente y en F1 es aún inglés (semilla). El ⚙ SISTEMA NO va en intro (#71).
      const introLangSwitcher = mountLanguageSwitcher(parent, LANG_SWITCHER_DEPS);
      // Cambiar de idioma en la intro repinta ambos FAB (el de piel actualiza su
      // tooltip/menú-título; el de idioma su etiqueta/lista). Desuscripción en finally.
      const offIntroLang = onLangChange(() => {
        introSwitcher.refresh();
        introLangSwitcher.refresh();
      });
      // ── MÚSICA DE LA CINEMÁTICA (INTRO.OVL 0x0ad0 / 0x0adc / 0x0aed) ───────────────
      // La portada NO es una canción sola: el parche engancha TRES selectores distintos en
      // la intro —0x09 «Ultima V Theme» para el menú, 0x06 (tabla de rango 0x223 por
      // PÁGINA de The Summoning) para la cinemática, y 0x0c «Amiga Theme» para la creación
      // de personaje, que su Readme anuncia como característica propia («Amiga music
      // during Character Creation — not in original!»). Sin esto `AMIGA.XMI` no sonaba en
      // ningún sitio del port.
      //
      // Se SIGUE POR SONDEO y no por callback, que es exactamente lo que hace el original:
      // el parche no tiene eventos de fase — cuelga su refresco del sondeo de teclado
      // (`poll_key_blink_cursor` 0x1b5b) y recalcula la canción en cada vuelta. Aquí el
      // sondeo es de 8/s, sólo vive mientras dura la intro, y `play` de-duplica por
      // canción: pedir la misma diez veces no la reinicia. Read-only sobre la intro (dos
      // getters), así que la cinemática sigue siendo byte-idéntica al DOSBox.
      const introMusicTimer = window.setInterval(() => {
        switch (intro.currentPhase) {
          case "story":
            music.play(introPageContext(intro.currentStoryPage));
            break;
          case "name":
          case "sex":
          case "cast":
          case "quiz":
            music.play("creation");
            break;
          default:
            music.play("title");
        }
      }, 125);
      let result;
      try {
        result = await intro.run();
      } finally {
        window.clearInterval(introMusicTimer);
        offIntroLang();
        introSwitcher.dispose();
        introLangSwitcher.dispose();
      }
      if (result.action === "create") {
        applyGypsyCreation(game.state, result.creation);
        consentimiento.analitica.evento(EV.PERSONAJE_CREADO);
      }
      // "Journey Onward" (J): recarga el save más reciente = cargar SAVED.GAM. La
      // intro sólo emite "journey" CON save (gate INTRO.OVL 0x0ec9: sin personaje
      // imprime "No active game…" y no sale del menú; y "Return to the View" ya no
      // arranca — relanza el demo, 0x100a). El fallback sin-save de esta línea es
      // por tanto inalcanzable desde el menú; queda como red de seguridad.
      else if (result.action === "journey") {
        if (restoreMostRecentSave()) consentimiento.analitica.evento(EV.VIAJE_REANUDADO);
      }
    }

    // ?nointro (DEV/pixeldiff): se saltó todo menú → auto-restaura el último save
    // (QoL coherente con esa piel). El arnés Playwright usa contextos frescos SIN
    // localStorage, así que aquí es no-op salvo que el usuario tenga un save; `?fresh`
    // lo desactiva para capturas deterministas (ya cubierto por `forceFresh`).
    if (skipIntro) restoreMostRecentSave();

    // La vista del core (snapshot 11×11 + consola + eventos de turno) es el
    // ÚNICO canal core→piel (E1-S1). `hud` queda como fachada sobre el modelo de
    // consola compartido para no tocar los ~60 call sites de main.ts. La piel
    // dev (PixiJS) se monta al final del boot vía SkinManager.
    const view = new CoreViewImpl(game);

    /**
     * LA CEREMONIA `CAST2.OVL:0x0000` — UNA primitiva para las cuatro vías de magia.
     *
     * El binario la llama desde 40 sitios repartidos en DOS overlays y ningún otro
     * (CAST.OVL 34, CAST2.OVL 6; censo con `dispatch_table.near_calls_to_kernel(*,0x8106)`,
     * derivación completa en `core/magic/ceremony.ts`). Todos empujan UN argumento gateado
     * a `< 9`, y con él escala la rutina entera: la ráfaga de ruido de entrada, los dos
     * `tone_sweep` espejo y —lo que aquí importa— la ventana en que el viewport queda
     * INVERTIDO por el `rect` XOR (8,8)-(183,183).
     *
     * El port ya tenía la pieza completa (`TimeSpellFlash` + `timeSpellFlashWindowMs` +
     * `invertViewportInterior`) y la disparaba SÓLO desde los pergaminos 2/3/7. El cue
     * conserva el nombre histórico `time-spell` porque el port sólo conocía la rutina por
     * ese camino; renombrarlo es cosmético y no se hace aquí.
     */
    const emitCeremony = (n: number | null): void => {
      if (n !== null) view.emitSfx({ id: "time-spell", n });
    };
    /**
     * (C)ast: el índice es el CÍRCULO (`CAST.OVL:0x0e0a-0x0e14`, el mismo `[bp-8]` que cobra
     * el maná), y SIETE de los 48 no hacen ceremonia — los tres arma-hechizo y los cuatro
     * abanicos de línea, que tienen sonido propio. Vas Rel Por tampoco la hace AQUÍ: la suya
     * cuelga del gate de fase (0x0d31), así que se emite allí y no dos veces.
     */
    const emitCastCeremony = (spellIndex: number): void => {
      if (spellIndex === VAS_REL_POR_SPELL_INDEX) return;
      emitCeremony(castCeremonyIndexOrNull(spellIndex));
    };

    const hud = {
      message: (text: string, rune?: boolean): void => view.pushConsole(text, "message", rune),
      // Fila MIXTA (#364-c): tramos {text,rune} cuando la fuente cambia a mitad de fila
      // (ALAKAZAM de la donación, «A scroll: <runa>!», habla rúnica de TALK).
      messageSegments: (segments: readonly { text: string; rune: boolean }[]): void =>
        view.pushConsoleSegments(segments, "message"),
      // Eco del comando del jugador ("Look-", "North"…): lleva bullet en la piel
      // fiel (F-G). Mismo texto del core (guarda de strings intacta), sólo metadata.
      echo: (text: string): void => view.pushConsole(text, "echo"),
      // Continúa la fila del eco en la misma línea ("Open-" + "North" → "Open-North",
      // getdir kernel 0x35EC). Sin bajar fila; el resultado cae en la siguiente.
      echoAppend: (text: string): void => view.echoAppend(text),
      // Continúa la fila de MENSAJE viva (prompts de Ready: "Player: "+nombre,
      // "Item: "+"Done"). Gemelo de echoAppend para líneas sin bullet.
      messageAppend: (text: string): void => view.messageAppend(text),
      // Reescribe la fila de eco viva (getstring de consola: Yell / palabra de poder).
      echoSetLast: (text: string): void => view.echoSetLast(text),
      // Abre la fila de CURSOR del getstring (el «:» del prompt, DS 0x4529 "what?\n:";
      // TALK_UI.cursor): eco SIN bullet (`cont`), donde `echoSetLast` reescribe el input.
      echoCursor: (text: string): void => view.echoCursor(text),
      refresh: (): void => view.notifyDirty(),
    };
    // Cambio de idioma en caliente (como el selector de piel): repinta. Los textos YA
    // impresos no se re-traducen (coherente con un log de scroll, analisis.md §6.2);
    // sólo los mensajes NUEVOS pasan por `t()`. La UI del selector la monta el carril
    // del shell cuando aterrice; aquí queda la API viva enganchada al repintado.
    onLangChange(() => view.notifyDirty());
    // 🔴 AQUÍ NO SE IMPRIME NADA, Y ES EL ARREGLO DE LA FICHA #116 — no un olvido.
    // Esta línea era `hud.message("Welcome to Britannia!")`, una bienvenida del clon que
    // el original NO tiene. Medido por DOS canales independientes:
    //   · OBSERVACIÓN — careo de consola de la #113 (`re/notes/consola-ancla-113.md` §4):
    //     con la geometría ya calcada, los frames 0-3 se quedaban en 0,27-0,77 (contra
    //     0,806 de media) por esta línea y sólo por ella. El frame 0 del original enseña
    //     «►Pass» —el ECO del primer comando— con la consola vacía por encima.
    //   · BINARIO — la rama de ÉXITO de "Journey Onward" (`INTRO.OVL:0x0f26-0x0f99`) es
    //     E/S de fichero y setup: ni un `call 0xffff9690` (el impresor del overlay, que la
    //     rama de FALLO contigua usa tres veces en 0x0ed4/0x0edb/0x0ee2 para «No active
    //     game.»). Y el init del sobremundo (`MAINOUT.OVL:0x0000-0x0079`) tampoco llama a
    //     su impresor (`0xffff9680`); el primero del fichero está ya en 0x010e, dentro del
    //     handler de transporte. ⇒ no es que el original imprima OTRA cosa: es que en el
    //     camino de arranque NO HAY IMPRESOR. La consola arranca vacía y su primera línea
    //     la escribe el jugador.
    // Que el pipeline core→piel esté vivo al arrancar lo sigue probando `easy-keys.spec.ts`
    // («Space imprime Pass»), con una cadena que el original SÍ emite (DS 0xa134).

    // El deck táctil NO se construye en una repetición (`soloUiOriginal`, ver arriba). No se
    // monta-y-esconde: montarlo pone `html.u5-touch`, que RESERVA la banda inferior y sube el
    // juego — un hueco negro donde estaban los botones seguiría sin ser la pantalla de 1988.
    // La API de módulo del deck (`setExpectedInput`, `refreshTouchDeck`) es `active?.…` sobre
    // una instancia que aquí queda a `null`: sin instancia son no-ops, no errores.
    if (!soloUiOriginal) new TouchControls(parent, game);
    // `music` se construyó ANTES de la intro (ver allí el porqué): a partir de aquí la
    // pista la manda la POSICIÓN, y este primer `updateMusic()` releva al tema de portada.
    //
    // 🔴 ES EL SELECTOR 0x00 DEL DRIVER, con sus CUATRO entradas — no sólo la localización.
    // `mid.drv` 0x016d lee `g_location`, `g_floor`, `g_transport_tile` y
    // `g_cmb_victory_flag`, y en ese orden: navegar en fragata tapa la música del sitio, y
    // el centinela de combate (0xFF) gana a todo. Pasarle sólo (location, floor) dejaba
    // mudas dos canciones enteras — el Hornpipe de la fragata y el tema tras «VICTORY!».
    // Derivación con el disasm al lado en `re/notes/music-location-mapping.md` §1.2.
    //
    // El original lo re-evalúa en CADA sondeo de teclado (el parche engancha su refresco
    // en `poll_key_blink_cursor` 0x1b5b), así que llamar a esto de más es fiel, no
    // derrochador: `playLocation` de-duplica por canción y no reinicia la pista viva.
    const musicPos = (): LocationMusicInput => ({
      location: game.state.position.location,
      floor: game.state.position.floor,
      transportTile: game.state.transportTile,
      inCombat: !!game.combat,
      combatVictory: game.combat?.victory === true,
    });
    const updateMusic = (): void => music.playLocation(musicPos());
    /**
     * Devuelve el mando a la localización tras una escena de canción FIJA — el selector
     * 0x0f del driver, que en el original cierra TODAS: acampada, rito, captura y muerte
     * llaman a `call 0x0e1d` justo al volver (`re/notes/music-location-mapping.md` §1.4).
     * Sin esto la escena se quedaría dueña de la música para siempre.
     */
    const resumeMusic = (): void => music.resumeLocation(musicPos());
    // Primer relevo: la PORTADA es una escena de canción fija (se quedó dueña de la música
    // al pedir «title»), así que aquí hay que devolver el mando, no sólo refrescar. Es el
    // mismo orden del arranque del parche: cargar driver → correr la intro → selector 0x0f
    // (ULTIMA.EXE 0x0e36, su tercer `call`).
    resumeMusic();

    // Menú DEBUG/QA (fuera del UI del juego): drawer lateral para fijar cualquier
    // cosa del estado (teleport, party, recursos, reloj, inventario). Se monta sólo
    // al abrirlo (tecla ` o F4, o ?debug=1). `notify` repinta la piel + música tras
    // cada mutación. Escribe DIRECTO en game.state (cero-rand); no toca mecánica.
    //
    // GATE (build, no piel): dev server SIEMPRE · producción SÓLO si se pide por URL.
    // En un build de producción SIN `?debug=1` el atajo ` / F4, el drawer y su listener
    // global NO existen — el QA no se filtra a usuarios finales en NINGUNA piel. En dev
    // sí está en todas las pieles (los QA lo necesitan con fiel/shader). La sección
    // "Debug (QA)" del menú SISTEMA y la fila de teclas del atajo van gated al MISMO
    // flag (ver más abajo: `openDebug`), así que las tres cosas aparecen y desaparecen
    // juntas y no hay forma de tener una sin las otras.
    // 🔴 PUERTA TEMPORAL A PETICIÓN DEL USUARIO (07-08): además del dev server, el
    // drawer se monta en un build de PRODUCCIÓN si la URL trae `?debug=1`. Sigue
    // AUSENTE por defecto —un visitante que no lo pida no ve la sección «Debug (QA)»,
    // ni la fila de teclas, ni tiene el listener del atajo—, así que la razón de la
    // puerta original (que el QA no se filtre a usuarios finales) se conserva: lo que
    // cambia es que ahora se puede pedir, no que venga puesto.
    // Para revertirlo basta borrar `|| debugPorUrl` y esta nota.
    const debugPorUrl =
      new URLSearchParams(window.location.search).get("debug") === "1";
    const debugMenu = import.meta.env.DEV || debugPorUrl
      ? initDebugMenu({
          game,
          parent,
          notify: () => {
            view.notifyDirty();
            updateMusic();
          },
        })
      : null;

    // Conductor del tap-to-walk (extraído a ui/autowalk.ts): posee el interval de
    // 140 ms; se cancela ante cualquier comando manual / combate (R1) / map-changed.
    // `applyEvents` se declara más abajo — la dep llega como closure (late-binding).
    const autoWalkCtl = new AutoWalk({ game, applyEvents: (e) => applyEvents(e) });
    const cancelAutoWalk = (): void => autoWalkCtl.cancel();

    // Cruce de MOONGATE en curso (secuencia scripted de la piel): en el binario
    // kernel_moongate_enter (0x48a8) es SÍNCRONA — disolución del jugador (0x1068) +
    // cierre 16→0 (0x4912-0x492b, delay(2) por etapa) — y NO lee input mientras corre.
    // Nuestra piel la pacea a reloj de pared, así que el input debe tragarse mientras
    // dure (modal como la acampada); sin esto, moverse durante el cruce ARRASTRABA la
    // puerta de llegada con el jugador (el overlay de cierre va centrado sobre el
    // party) — testigo del usuario 2026-07-22. El probe se ata al registrar las
    // pieles (moongateGate.bind, final del boot). Extraído a ui/moongate-gate.ts.
    const moongateGate = new MoongateTransitGate();

    // KNOB DE ESCENAS A RELOJ DE PARED (`?scenebeat=<ms>`, auditoría de calidad Q4):
    // fija TODOS los delays de las escenas MODALES (refuge, endgame, camp) al valor
    // dado — `?scenebeat=0` las hace efectivamente instantáneas. Es un knob de pura
    // PRESENTACIÓN (las escenas son modales que tragan input: no reordena teclas ni
    // toca core/RNG); los beats paceados POR TECLA (diálogo/historia del endgame) no
    // cambian — siguen esperando la tecla. A DIFERENCIA de `combeat`, NO se activa
    // solo por `navigator.webdriver`: el grand-tour sellado corre con las cadencias
    // reales (Clase C calibradas a testigo) y sus digests no deben moverse. Solo un
    // spec que lo pida por URL (p.ej. endgame.spec, que sin esto necesita 120 s de
    // timeout por la ventana de 90 s a reloj de pared) recorta el reloj de pared.
    const sceneBeatParam = new URLSearchParams(window.location.search).get("scenebeat");
    const SCENE_BEAT_MS =
      sceneBeatParam !== null ? Math.max(0, Number(sceneBeatParam) || 0) : null;
    /** Delay efectivo de un beat de escena: el del knob si está armado, si no el real. */
    const sceneMs = (ms: number): number => SCENE_BEAT_MS ?? ms;

    // Conductor de la secuencia de sueño de la acampada (H)ole up — extraído a
    // ui/camp-sleep.ts (timers campSleep/campSongTimer/campFlashTimer + flag modal
    // `camping`). Los prompts previos (hours/watch/guard) siguen en startCamp.
    const campSleepCtl = new CampSleep({
      game,
      view,
      hud,
      applyEvents: (e) => applyEvents(e),
      refreshAwaiting: () => refreshAwaiting(),
      cancelAutoWalk,
      sceneMs,
      // `g_unk_a9ce` (^S / F8): TERCERA precondición del easter egg del bardo, no sólo
      // del audio — con el sonido apagado el original no le cambia el sprite ni gasta
      // la pausa de 52 redibujos (CMDS.OVL 0x0123/0x0128, ficha #39). THUNK: `speaker`
      // se declara más abajo en este mismo boot() y el usuario lo alterna en caliente.
      soundEnabled: () => speaker.enabled,
      // Acampada terminada (despertar o emboscada) ⇒ el mando vuelve a la localización,
      // como el `call 0x0e1d` con que `kernel_camp_holeup` cierra. En la emboscada el
      // combate ya arrancó y `enterCombatMode` ya descongeló; esto es idempotente.
      onEnd: () => resumeMusic(),
    });

    // Conductor del sueño en CAMA de pueblo (#296) — hermano del de acampada. Antes esto
    // era `applyEvents(game.bedSleep(n))`: las N horas resueltas en UN frame, sin ver correr
    // el reloj y sin sitio donde vivir el apagón del viewport del original. Ver ui/bed-sleep.ts.
    const bedSleepCtl = new BedSleep({
      game,
      view,
      hud,
      applyEvents: (e) => applyEvents(e),
      refreshAwaiting: () => refreshAwaiting(),
      cancelAutoWalk,
      sceneMs,
    });

    // ★ #295 — INVERSIÓN del viewport del «WELL DONE» del Altar (CAST2 0x0c34-0x0c41). Mismo
    // patrón que la cortina de cama y por la misma razón: el core resuelve el rito en un
    // `applyEvents` atómico, así que el estado necesita un conductor que lo sostenga.
    const ritualInvertCtl = new RitualInvert({ view, sceneMs });

    // ── ★ #326 — REVELADO de la poción BLANCA (CAST2.OVL 0x046c) ─────────────────────
    // La MISMA rutina a la que Wis An Ylem hace TAIL (#319), así que la poción REUSA la
    // primitiva unificada `view.revealViewport` (rama -1 de 0x5d0a: flood saltado, búfer
    // 11×11 a 0xFF = rayos X con salas selladas; careo independiente de las dos ramas de
    // la confluencia, mismo veredicto). Duración = DEATH_VISION_FRAMES (si=0x14 en
    // 0x049d, la constante EN CRUDO de cast.ts) × PAUSE_UNIT_MS (el tick de int 1c
    // ≈ 55 ms de 0x20fa). Lo que la poción AÑADE sobre el hechizo: (a) es MUDA — CAST
    // 0x151b llama SIN el `push 6` del jingle — y (b) aquí el bucle modal del binario
    // (0x04a0-0x04b8 no lee teclado) SÍ se calca: `revealing` traga el input como
    // refuge/troll, bajo el knob `sceneMs`. Para el hechizo esa modalidad sigue
    // DECLARADA como divergencia (acta #319 §5 (a)) — unificarla es follow-up de su
    // carril, no de éste. Derivación: re/notes/efectos-visuales-326.md §3/§3-bis.
    let revealTimer: number | null = null;
    let revealing = false;
    const runMapReveal = (): void => {
      cancelAutoWalk();
      if (revealTimer !== null) clearTimeout(revealTimer);
      const ms = sceneMs(DEATH_VISION_FRAMES * PAUSE_UNIT_MS);
      revealing = true;
      view.revealViewport(ms);
      refreshAwaiting();
      revealTimer = window.setTimeout(() => {
        revealTimer = null;
        revealing = false; // la vista se re-censura sola al expirar (el 0x5910 final)
        refreshAwaiting();
      }, ms);
    };
    const cancelMapReveal = (): void => {
      if (revealTimer !== null) clearTimeout(revealTimer);
      revealTimer = null;
      revealing = false;
      view.revealViewport(0); // revealUntil = ahora ⇒ el siguiente horneado re-censura
    };

    // Escena de MUERTE + RESURRECCIÓN de Lord British (party-wipe / refuge, BLCKTHRN 0x0910).
    // Como la acampada, es un MODAL a reloj de pared: `refuging` bloquea el input mientras la
    // escena corre, `refugeTimer` pacea los beats del guión (`RefugeScript`), y al terminar
    // `game.resolveRefuge()` revive al party y lo despierta en el castillo. Ver `runRefugeScene`.
    let refugeTimer: number | null = null;
    let refuging = false;
    // Cadencia de la escena (Clase C, calibrada a video-M): ms por unidad cruda de `delay`
    // (0x7e6a); un piso de LECTURA en los beats con texto (para que la línea se lea) y uno
    // corto en las transiciones de figura/sonido (que en el original son casi inmediatas).
    const REFUGE_UNIT_MS = 70;
    const REFUGE_TEXT_MIN_MS = 900;
    const REFUGE_SCENE_MIN_MS = 260;
    const cancelRefuge = (): void => {
      if (refugeTimer !== null) {
        clearTimeout(refugeTimer);
        refugeTimer = null;
      }
    };

    // Reproduce el GUIÓN de la escena de muerte+resurrección (BLCKTHRN 0x0910) a reloj de
    // pared: monta el modal (input bloqueado), pacea los beats (fase visual → consola → sfx →
    // pausa) y al agotarlos llama a `game.resolveRefuge()` (revive + despierta en el castillo
    // de LB) y desmonta la escena. Espejo de `runCampSleep`. El guión lo emite el core puro
    // (`buildRefugeScript`), aquí sólo se PRESENTA con timing. Ver core/game.ts.
    const runRefugeScene = (script: RefugeScript): void => {
      cancelAutoWalk();
      cancelRefuge();
      refuging = true;
      // MUERTE DE LA PARTY = SILENCIO, y es una decisión EXPLÍCITA del autor del parche:
      // «I made sure that Death and the Blackthorn capture sequence were both handled
      // properly: no music should be playing during these sequences» (History.txt). En el
      // binario se ve igual — los TRES llamadores de `party_refuge` (TOWN.OVL 0x1862,
      // MAINOUT.OVL 0x0af0, DUNGEON.OVL 0x1014) paran la música ANTES de entrar y la
      // reactivan al volver (glue 0x0e26/0x0e7c). El `resumeMusic()` va en el beat final,
      // con la party ya revivida en el castillo.
      music.play("silence");
      refreshAwaiting();
      let i = 0;
      const step = (): void => {
        if (i >= script.beats.length) {
          cancelRefuge();
          refuging = false;
          view.setRefugeScene(null); // desmonta la escena → se revela el castillo
          applyEvents(game.resolveRefuge()); // revive + despertar (map/party-changed)
          resumeMusic(); // el `call 0x0e1d` de los tres llamadores, ya en el castillo
          refreshAwaiting();
          return;
        }
        const beat = script.beats[i++]!;
        if (beat.scene) view.setRefugeScene(beat.scene);
        if (beat.message) hud.message(beat.message);
        if (beat.sfx) view.emitSfx(beat.sfx);
        const floor = beat.message ? REFUGE_TEXT_MIN_MS : REFUGE_SCENE_MIN_MS;
        const ms = REFUGE_UNIT_MS * (beat.delayUnits ?? 0) + floor;
        refugeTimer = window.setTimeout(step, sceneMs(ms));
      };
      step();
    };

    // ── CRUCE DEL PUENTE con trolls (MAINOUT 0x1c0e-0x1ca6): pacer del guión ─────────
    // Extraído a ui/troll-sneak.ts (derivación 0x3AE6 = run-n-frames en su cabecera).
    // BAJO AUTOMATIZACIÓN (navigator.webdriver) la unidad es 0 → todo síncrono,
    // byte-idéntico al flujo previo (e2e/digests sin drift). `?trollbeat=<ms>`
    // fuerza la unidad (verificación visual automatizada: ?trollbeat=55).
    // TICKET #18 — UN SOLO KNOB para las escenas modales: el troll pasa a honrar
    // `?scenebeat` como las demás (refuge/camp/endgame). PRECEDENCIA, de más a menos
    // específico:  `?trollbeat` (override propio, para verificación visual con la
    // cadencia real) > `?scenebeat` (el knob común) > `navigator.webdriver` → 0
    // (comportamiento previo, digests sin drift) > 55 ms reales.
    // El orden importa: con el knob AUSENTE el valor es idéntico al de antes, así que
    // esta unificación NO mueve ningún sello.
    const trollBeatParam = new URLSearchParams(window.location.search).get("trollbeat");
    const TROLL_UNIT_MS =
      trollBeatParam !== null
        ? Math.max(0, Number(trollBeatParam) || 0)
        : SCENE_BEAT_MS !== null
          ? SCENE_BEAT_MS
          : navigator.webdriver
            ? 0
            : 55;
    const trollSneakCtl = new TrollSneak({
      unitMs: TROLL_UNIT_MS,
      hud,
      applyEvents: (e) => applyEvents(e),
      refreshAwaiting: () => refreshAwaiting(),
      cancelAutoWalk,
    });

    // ── ESCENA DEL SANTUARIO / CODEX (#277): paceador del guión de CAST2 0x0e76 ────────
    // La ESTRUCTURA es derivada (4 cues vacíos + colocación + 4 pasos, cada cue = 5
    // fotogramas de `beep_ticks`); la UNIDAD de tiempo es CALIBRADA contra el testigo de
    // vídeo, y son dos cosas distintas. `beep_ticks` (kernel 0x3ae6) es un bucle de
    // [viewport_redraw + delay(1 tick)]: el ASM fija el NÚMERO de fotogramas, pero el
    // coste del repintado no está en el ASM, así que «1 fotograma = 1 tick INT 1Ch ≈
    // 55 ms» era un SUPUESTO mío heredado del troll — y el testigo lo refuta por 2,22×.
    //
    // 🔴 CALIBRACIÓN, con su aritmética (medición de playlist-espejo-278 sobre Part 19,
    // santuario de Honor, 1080p): corte de pantalla t=94,6 · Avatar aparece t=97,1 ·
    // swap a arrodillado t=100,1. Entre corte y aparición van los 4 cues vacíos; entre
    // aparición y kneel, la colocación + los 4 pasos = 5 cues. ⇒ 9 cues en 5,5 s =
    // 0,611 s/cue = 122 ms por fotograma. Con 120 ms el guion PREDICE los dos hitos:
    // aparición 97,0 (medido 97,1) y kneel 100,0 (medido 100,1) — residuo 0,1 s sobre un
    // tramo de 5,5 s, con la estructura de cues SIN tocar. Es decir: el testigo confirma
    // el esqueleto derivado y sólo corrige la escala.
    const SHRINE_SCENE_UNIT_MS = 120;
    const shrineScenePacer = new ShrineScenePacer({
      // Bajo automatización 0 (drena síncrono, e2e y digests sin drift) y `?scenebeat`
      // sigue mandando cuando el usuario lo fija — misma precedencia que las demás escenas.
      unitMs: TROLL_UNIT_MS === 0 ? 0 : SCENE_BEAT_MS !== null ? SCENE_BEAT_MS : SHRINE_SCENE_UNIT_MS,
      setScene: (s) => view.setShrineScene(s),
      emitSfx: (cue) => view.emitSfx(cue),
      applyEvents: (e) => applyEvents(e),
      refreshAwaiting: () => refreshAwaiting(),
      cancelAutoWalk,
      // Fin del rito (mitad de salida agotada) ⇒ vuelve a mandar la localización, como el
      // `call 0x0e1d` con que MAINOUT.OVL 0x0968 cierra la visita al santuario.
      onSceneEnd: () => resumeMusic(),
    });

    // ── ★ #324 ESCENA DE LA CAPTURA de Blackthorn (BLCKTHRN 0x060e + anim_vm 0x00be) ──
    // Mismo patrón y misma unidad que el rito: los `frames` de los beats son unidades
    // de run-n-frames 0x3AE6 / ticks INT 1Ch ⇒ PAUSE_UNIT_MS (55). Bajo automatización
    // 0 (drena síncrono); `?scenebeat` manda si el usuario lo fija. La escena PERSISTE
    // entre segmentos (los prompts del interrogatorio corren con la sala a la vista).
    const blackthornScenePacer = new BlackthornScenePacer({
      unitMs: TROLL_UNIT_MS === 0 ? 0 : SCENE_BEAT_MS !== null ? SCENE_BEAT_MS : PAUSE_UNIT_MS,
      setScene: (s) => view.setBlackthornScene(s),
      emitSfx: (cue) => view.emitSfx(cue),
      applyEvents: (e) => applyEvents(e),
      refreshAwaiting: () => refreshAwaiting(),
      cancelAutoWalk,
      // Fin de la captura (depósito en la celda) ⇒ vuelve a mandar la localización, que
      // en ese punto ya es la celda del palacio (loc 0x12 ⇒ «Lord Blackthorn»).
      onSceneEnd: () => resumeMusic(),
    });

    // ── ★ #294 ESPERAS DE TECLA del rito (CAST2 `call 0x448c` → kernel 0x266c) ─────────
    // ONCE sitios derivados: DOS en la rama ORDAINED de `shrine_visit` (0x0a9b/0x0abc) y
    // NUEVE en el handler del Códice (0x0d2b…0x0e5b). A diferencia de la escena de arriba,
    // esto NO lleva unidad de tiempo — es espera de TECLA, no de reloj, y no se calibra.
    // `instant` comparte discriminante con las escenas paceadas (`TROLL_UNIT_MS === 0` =
    // automatización o knob a 0): ahí las esperas no existen y el turno drena de una, que
    // es exactamente el orden de eventos previo a esta ficha.
    const shrineKeyPacer = new ShrineKeyPacer({
      instant: TROLL_UNIT_MS === 0,
      applyEvents: (e) => applyEvents(e),
      refreshAwaiting: () => refreshAwaiting(),
    });

    // ── ★ #213 TICK DE VENENO al andar (kernel 0x2b0b → 0x2a52): paceador del guión ──
    // Por cada miembro 'P' y EN ORDEN DE SLOT: fila del roster en vídeo inverso
    // (`0x2a28`, la MISMA primitiva del picker y del impacto) + `noise_burst(10,1600,
    // 2000)` (`0x223c`) + des-inversión. El original los encadena porque su altavoz
    // BLOQUEA (regla de #206); aquí se encadenan espaciándolos a reloj de pared, sin
    // tocar el cerrojo `BLOCKING` de playSegs (que sigue acotado al ritual, #206/#208).
    // NO es modal: no traga input ni difiere el turno — pasa en CADA paso.
    // Bajo automatización (webdriver) el paso es 0 → cues de golpe, sin timers ni flash
    // (digests sin drift). `?scenebeat` lo comparte con las demás escenas.
    const POISON_UNIT_MS =
      SCENE_BEAT_MS !== null ? SCENE_BEAT_MS : navigator.webdriver ? 0 : POISON_BLIP_MS;
    const poisonTickCtl = new PoisonTick({
      blipMs: POISON_UNIT_MS,
      setDamageFlash: (idx) => view.setDamageFlash(idx),
      playDamageCue: () => view.emitSfx({ id: "combat-damage" }),
    });

    // ── ENDGAME (#34): pacer de la secuencia final (patrón RefugeScript) ─────────────
    // Extraído a ui/endgame-pacer.ts: el core emite el GUIÓN completo
    // (`EndgameScript`, evento {kind:"endgame"}) y el pacer lo PRESENTA (beats de
    // texto por TECLA, animaciones a reloj de pared, estados terminales); su flag
    // `active` traga el input global y `consumeKey` avanza los beats de tecla.
    const endgamePacer = new EndgamePacer({
      game,
      view,
      hud,
      refreshAwaiting: () => refreshAwaiting(),
      cancelAutoWalk,
      sceneMs,
      sceneBeatMs: SCENE_BEAT_MS,
      endgameRoom,
      // ── MÚSICA DEL CIERRE (ENDGAME.OVL 0x0aee / 0x0aff / 0x0b18) ──────────────────
      // El endgame es la ÚNICA escena del parche que cambia de canción tres veces, y la
      // última es la que cierra el juego: Rule Britannia. Sin esto, `RULEBRIT.XMI` no
      // sonaba NUNCA (estaba mal cableada como música del castillo de Lord British) y
      // `REUNION.XMI` tampoco.
      //
      // ⚠️ RÓTULO HONESTO — esto es DERIVADO EN FORMA, NO EN ÍNDICE. Lo derivado del
      // binario es el ORDEN (tabla de cuadros → Joyous Reunion → Rule Britannia) y que el
      // cierre RULEBRIT es INCONDICIONAL: ENDGAME.OVL 0x0b18 cae fuera del `je 0xb18`, así
      // que suena tanto en el final bueno como en el VARADO (cuyo bucle de deambular
      // 0x0b1f→0x0ac9 vuelve a pasar por él en cada vuelta, que es el modismo de «déjala
      // sonando» del driver). Lo que NO está derivado es qué beat de ESTE port equivale a
      // cada cuadro del original: el guión del clon no lleva el contador `[bp-6]` que
      // indexa la tabla 0x24a, así que el reparto por fase es una lectura del orden, no un
      // calco de índices. Va rotulado aquí y no disfrazado de derivación.
      onPhase: (phase) => {
        if (phase === "storyHouse" || phase === "storyDream") music.play("endgame-ladynan");
        else if (phase === "scroll") music.play("reunion");
        else if (phase === "terminalFreeze" || phase === "terminalPrison") music.play("finale");
        else music.play("endgame-stones");
      },
    });

    // Prompt "Klimb-U/D-" pendiente: la celda de mazmorra tiene escalera arriba Y
    // abajo (DUNGEON 0x1e9c). Mientras está activo, la próxima tecla elige la vía.
    let pendingDungeonKlimb = false;
    // Prompt «Dir-» del (S)earch 3D vivo (cadena F2-T7): guarda el ÍNDICE del miembro
    // ya elegido por el selector 0x8a08 mientras el getkey de dirección espera.
    let pendingDungeonSearch: number | null = null;
    // Prompt «Dir-» del (L)ook 3D (F3, misma cadena sobre DNGLOOK: selector @0x0007 →
    // gate de luz @0x0013 → dir @0x0028 `call 0xdcae`). El Look no usa el índice del
    // miembro (el selector solo ecoa el nombre) → basta un flag.
    let pendingDungeonLook = false;

    // ── FAMILIA Ctrl (<0x20) — jump tables de los 3 bucles de contexto:
    // MAINOUT 0x0bbe (handlers 0x0b34 K / 0x0b48 E / 0x0b6e V / 0x0b80 S),
    // TOWN 0x14b8 y DUNGEON 0x06c4 (0x06f2 K / 0x0710 E / 0x073a V / 0x0776 S).
    // Semántica idéntica en los tres; NINGUNA cobra turno (Ctrl-K/V saltan
    // directo al re-read de tecla, MAINOUT 0x0b7c jmp 0xaf8). Strings DATA.OVL
    // (fileoff=DS+0x10): "Exit to DOS? " 0x2b3e / "N\n" 0x2b4c / "1.16" 0x2b4f /
    // "Sound " 0x2b54 / "Off\n" 0x2b5b / "On\n" 0x2b60 (copias mazmorra 0x2d2d+).
    // Devuelve true si consumió la tecla. Corrige de paso el hijack de Ctrl+K
    // (antes disparaba Klimb: el branch 'k' no gateaba ev.ctrlKey).
    //
    // ⚠ SON CUATRO BUCLES, NO TRES, y el cuarto NO tiene la misma familia. El de
    // COMBATE (COMBAT.OVL 0x063e, getkey @0x0838) despacha los <0x20 con una cadena
    // de `cmp` propia — no con tabla — y de la familia SOLO lleva **Ctrl-S**:
    //   0x084a..0x0886: 0x4a→'J' · 0x41→'A' · 0x1b→ESC · 1..4→movimiento (0x0a14) ·
    //   **0x13→0x08b6 (Sound)** · TODO LO DEMÁS → 0x0ab7 = "What?\n" (DS 0x6ee6).
    // El discriminante duro de que Ctrl-K NO existe ahí es un censo, no una lectura:
    // `g_karma` (DS 0x5888) tiene **CERO referencias en COMBAT.OVL** — mientras que
    // MAINOUT/TOWN/DUNGEON lo leen en 0x0b34/0x14b8/0x06f2. Por eso `ctx` existe: en
    // combate esta función atiende Ctrl-S y NADA más. (Derivación: la tabla entera de
    // los cuatro bucles, la base de carga de overlays y el testigo de vídeo, en
    // `re/notes/teclas-control-karma-derivacion.md`.)
    //
    // ⚠ Los cuatro emiten por `hud.echo`, NO por `hud.message`: fila de COMANDO (con
    // su bullet «>»), no fila de mensaje. No es cosmética, es el ORDEN del bucle —
    // `tick_and_getkey` (MAINOUT 0x05c6-0x05d7) hace `emisor('\n')` y pinta el prompt
    // ANTES de `getkey_with_redraw` (0x06dc), y ningún handler de la familia emite
    // salto de línea delante de su texto ⇒ lo que imprimen cae SOBRE la fila del
    // prompt, detrás del «>». Medido en el material de EA, dos veces y con dígitos
    // distintos: las filas son `>84` (ep.17 @04:47) y `>92` (@25:18), nunca un `84`
    // suelto. Con `hud.message` el port abría fila nueva sin bullet (careo de capturas
    // port↔original, este carril).
    const handleCtrlKey = (ev: KeyboardEvent, ctx: "map" | "combat" = "map"): boolean => {
      if (!ev.ctrlKey || ev.metaKey || ev.altKey) return false;
      const k = ev.key.toLowerCase();
      if (k === "s" && ctx === "combat") {
        // Ctrl-S en COMBATE — COMBAT.OVL 0x0881 `cmp ax,0x13` → 0x08b6: mismo
        // "Sound " + estado NUEVO y mismo toggle `sbb ax,ax; neg ax` sobre a9ce
        // que MAINOUT 0x0b87-0x0ba4; cambian sólo las COPIAS del pool
        // ("Sound " DS 0x6de4, "Off\n" 0x6dec, "On\n" 0x6df2 — texto idéntico, y
        // por tanto la MISMA huella de i18n que las de MAINOUT).
        ev.preventDefault();
        const on = speaker.toggle();
        hud.echo(t("Sound ") + t(on ? "On\n" : "Off\n"));
        return true;
      }
      if (ctx === "combat") return false; // K/V/E no son de este bucle (ver cabecera)
      if (k === "k") {
        // Ctrl-K (0x0B): imprime el KARMA como decimal llano — kernel print_number
        // 0x1a3e con (pad=' ', width=1, g_karma DS:0x5888) + '\n'. SIN turno.
        ev.preventDefault();
        hud.echo(String(game.state.karma)); // fila de COMANDO: cae tras el prompt (ver cabecera)
        return true;
      }
      if (k === "s") {
        // Ctrl-S (0x13): "Sound " + estado NUEVO tras el toggle (a9ce==0 → imprime
        // "On" y a9ce=1; MAINOUT 0x0b87-0x0ba4). Cableado al speaker fiel del port
        // (mismo toggle que F8). t() por pieza (el compuesto no es key del corpus).
        ev.preventDefault();
        const on = speaker.toggle();
        hud.echo(t("Sound ") + t(on ? "On\n" : "Off\n"));
        return true;
      }
      if (k === "v") {
        // Ctrl-V (0x16): versión "1.16" + '\n' (el censo la catalogaba mal como
        // «volumen»; corregido en docs/censo-teclado-refcard.md).
        ev.preventDefault();
        hud.echo("1.16");
        return true;
      }
      if (k === "e") {
        // Ctrl-E (0x05): "Exit to DOS? " → getkey: 'Y' EXACTA = exit(0) SIN
        // guardar (0x0b59 call 0x86a8 + exit); resto imprime "N\n" (0x2b4c).
        // Equivalente web de exit-to-DOS: recargar la página = volver al boot/
        // intro SIN guardar (el autosave NO se toca aquí). BANCO menor: el prompt
        // del port sólo resuelve con Y/N (getkey del binario trata CUALQUIER
        // tecla ≠'Y' como N).
        ev.preventDefault();
        hud.echo("Exit to DOS? ");
        prompts.current = {
          type: "yesno",
          resolve: (yes) => {
            if (yes) window.location.reload();
            else hud.echoAppend("N"); // "N\n" INLINE, en la MISMA fila de comando
          },
        };
        refreshAwaiting();
        return true;
      }
      return false;
    };

    // (Q)uit & Save — compartido por overworld/pueblo y mazmorra (CAST2 0x10FE es
    // INCONDICIONAL de contexto: el kernel 0x338C lo llama sin gate de location y
    // devuelve 0 = sin turno). Flujo fiel por consola, ver comentario en el branch.
    const doQuitSave = (): void => {
      hud.echo(CMD_STRINGS.quit); // "Quit:"
      hud.message("Save game? "); // DATA.OVL 0x9668 (acaba en espacio, sin \n)
      prompts.current = {
        type: "yesno", // getYN 0x448c: SÓLO Y/N, re-lee cualquier otra
        resolve: (yes) => {
          if (!yes) {
            hud.messageAppend("No"); // "Save game? No" INLINE (0x9676 "No\n")
            return;
          }
          hud.messageAppend("Yes"); // "Save game? Yes" INLINE (0x967a "Yes\n…")
          hud.message("Saving..."); // 0x967e "Saving...\n"
          const saved = autosave(game.state, mapName(), captureScreenshot(parent));
          if (!saved.ok) hud.message("Save failed!"); // no es string del binario (QoL)
          else {
            consentimiento.analitica.evento(EV.JUEGO_GUARDADO);
            hud.message("Done."); // DATA.OVL 0x96bc
          }
        },
      };
      refreshAwaiting();
    };

    const handleDungeonKey = (ev: KeyboardEvent): void => {
      const key = ev.key;
      ev.preventDefault();
      // Resolución del prompt U/D (getkey 0xa49c): U/↑ sube, D/↓ baja, Space "Pass"
      // (0x6cc6, sin moverse pero cobra turno). Cualquier otra tecla se ignora y el
      // prompt sigue abierto, como el bucle de getkey del original (1eca→1ece→1eac).
      if (pendingDungeonKlimb) {
        const up = key === "ArrowUp" || key.toLowerCase() === "u";
        const down = key === "ArrowDown" || key.toLowerCase() === "d";
        const pass = key === " " || key === "Spacebar" || key === "Escape"; // Esc = añadido ergonómico
        if (up) {
          pendingDungeonKlimb = false;
          applyEvents(game.dungeonCommand("klimb", "up"));
        } else if (down) {
          pendingDungeonKlimb = false;
          applyEvents(game.dungeonCommand("klimb", "down"));
        } else if (pass) {
          pendingDungeonKlimb = false;
          applyEvents(game.dungeonCommand("klimb", "pass"));
        }
        return;
      }
      // Prompt "Dir-" del (S)earch 3D (SJOG 0x0672, cadena F2-T7 cableada): getkey
      // en bucle {1,2,3,4,Space} (códigos de FLECHA del kernel): ↑=Ahead (3),
      // ↓=Here (4), →=Right (2), ←=Left (1); Space="Pass" (DS 0x84ec) y aborta SIN
      // turno. Otras teclas re-leen (bucle getkey del original); ESC = añadido
      // ergonómico (≡ Space, como en Klimb-U/D). Numpad 8/2/4/6 = flechas (getkey
      // remap ULTIMA.EXE 0x26a4).
      if (pendingDungeonSearch !== null || pendingDungeonLook) {
        const np = ev.code.startsWith("Numpad");
        const target =
          ev.key === "ArrowUp" || (np && ev.key === "8") ? ("ahead" as const)
          : ev.key === "ArrowDown" || (np && ev.key === "2") ? ("here" as const)
          : ev.key === "ArrowRight" || (np && ev.key === "6") ? ("right" as const)
          : ev.key === "ArrowLeft" || (np && ev.key === "4") ? ("left" as const)
          : null;
        if (target) {
          const searcherIdx = pendingDungeonSearch;
          const isLook = pendingDungeonLook;
          pendingDungeonSearch = null;
          pendingDungeonLook = false;
          // Eco de la elección en la MISMA fila («Dir-Ahead», testigo P16):
          // strings DS 0x84f2/0x84fa/0x8500/0x8508 (con \n en el binario; aquí la
          // fila de eco cierra sola). t() en call-site: echoAppend es mecánico.
          const label =
            target === "ahead" ? "Ahead\n"
            : target === "here" ? "Here\n"
            : target === "right" ? "Right\n"
            : "Left\n";
          hud.echoAppend(t(label).replace(/\n$/, ""));
          if (isLook) {
            // (L)ook F3: describe la celda ELEGIDA; fuente MIRADA → prompt de
            // bebida (DNGLOOK 0x012f encadena la bebida de la fuente mirada).
            applyEvents(game.dungeonCommand("look", undefined, { target }));
            if (game.dungeonFountainAhead(target)) {
              hud.message("Will you drink?"); // DS 0x7700
              prompts.current = {
                type: "yesno", // getkey Y/N crudo (DNGLOOK 0x01ea; ESC no decodificado)
                resolve: (yes) => {
                  if (yes) {
                    hud.message("Yes.  Gulp!"); // DS 0x7718 (dos espacios)
                    applyEvents(game.dungeonDrinkAhead(target)); // sin 2º turno: lo cobró el Look
                  } else {
                    hud.message("No."); // DS 0x7712
                  }
                },
              };
              refreshAwaiting();
            }
          } else {
            applyEvents(game.dungeonCommand("search", undefined, { target, searcherIdx: searcherIdx! }));
          }
        } else if (ev.key === " " || ev.key === "Spacebar" || ev.key === "Escape") {
          pendingDungeonSearch = null;
          pendingDungeonLook = false;
          hud.echoAppend(t("Pass\n").replace(/\n$/, "")); // DS 0x84ec — aborta sin turno
        }
        return;
      }
      // Familia Ctrl — jump table local del bucle DUNGEON (0x06c4: 0x0B karma /
      // 0x05 Exit-to-DOS / 0x13 Sound / 0x16 versión), idéntica a MAINOUT/TOWN.
      if (handleCtrlKey(ev)) return;
      // Cualquier otro combo Ctrl/Meta NO es un comando del juego (en el binario
      // los <0x20 restantes caen al default de la jump table): no despachar la
      // letra pelada (evita que Ctrl+K disparara Klimb — hijack del informe).
      if (ev.ctrlKey || ev.metaKey) { keyRec.drop(); return; } // no es comando: el atajo es del navegador
      // Dígitos (DUNGEON 0x07bc-0x07d6): fila superior = Set Active Plr (kernel
      // 0xbeb0) con retorno FORZADO a 0 (@0x07d1) = SIN turno. NUMPAD con NumLock
      // (getkey remap ULTIMA.EXE 0x26a4): 8/2/4/6 = movimiento; 7/9/1/3 =
      // diagonales 0xD3-0xD6 (sólo aim/listas — banco QoL-diferido del censo §3,
      // aquí se ignoran).
      if (/^[0-9]$/.test(ev.key)) {
        const np = ev.code.startsWith("Numpad");
        if (np && "8246".includes(ev.key)) {
          const cmd =
            ev.key === "8" ? "forward"
            : ev.key === "2" ? "back"
            : ev.key === "4" ? "left"
            : "right";
          applyEvents(game.dungeonCommand(cmd));
          return;
        }
        if (np) return; // 7/9/1/3 y Numpad0/5: sin comando en el pasillo
        hud.echo(CMD_STRINGS.setActive); // "Set Active Plr:" (DS 0xa396)
        applyEvents(game.setActivePlayer(Number(ev.key)));
        return;
      }
      // Space = Pass (kernel 0x31F4 rama loc≠0): "Pass\n" + turno de mazmorra. El eco
      // lo pone el DESPACHADOR (0x3210 → 0x33ea, DS 0xa134), que es común a overworld
      // y mazmorra — de ahí que se emita aquí y no dentro de `ds.pass()` (el binario
      // imprime ANTES de saltar al handler del overlay).
      if (ev.key === " " || ev.key === "Spacebar") {
        hud.echo(CMD_STRINGS.pass); // ">Pass" (dispatch 0x31F4→0x33ea, DS 0xa134)
        applyEvents(game.dungeonCommand("pass"));
        return;
      }
      // (H)ole up & camp también en mazmorra (kernel 0x3C9A rama loc>=0x21 →
      // 0x5f86). campContext devuelve ok sin chequeo de terreno.
      if (key.toLowerCase() === "h") {
        startCamp();
        return;
      }
      // ── Despacho completo del pasillo (kernel_cmd_dispatch 0x3178 vía DUNGEON
      // 0x07a0: TODA tecla ≥0x20 despacha; ver ramas por comando abajo) ──
      const dk = key.toLowerCase();
      // (A)ttack de mazmorra — DUNGEON 0x1D4A: "Attack\n" + ataque a la celda
      // ENCARADA. ~~sin monstruo errante (no portado)~~ ← RANCIO, retirado #327: el errante
      // SÍ está portado (core/dungeon/wanderer.ts, 355 líneas) y esta rama lo consume — pero
      // no aquí: DELEGA en dungeonCommand("attack"), cuyo docblock (dungeon-cmds.ts, rama
      // cmd==="attack") dice «en AMBAS ramas (con errante encarado arranca el combate de
      // pasillo)». Este comentario describía una decisión que su fichero NO TOMA, y por eso
      // nadie lo careaba; de él nació la ficha #285 pidiendo portar lo que ya estaba.
      // Sin errante encarado → "What?\n". SIN turno (ret 0) en ambas.
      if (dk === "a") {
        applyEvents(game.dungeonCommand("attack"));
        return;
      }
      // (L)ook — DNGLOOK 0x0000. Eco del kernel "Look"+"...\n" (DS 0xa1a8+0xa1ae,
      // 0x3310 rama mazmorra). CADENA Player:/Dir- CABLEADA (F3, testigo P16
      // «>Look... / Player: Min / Dir-Ahead / You see:», mismo patrón F2-T7 del
      // Search sobre DNGLOOK):
      //   1. selector de miembro @0x0007 `call 0xffffa6f8` (= resolve_display_char;
      //      cancel → aborta sin turno @0x0010). El Look no usa el índice.
      //   2. gate de luz @0x0013 ANTES del Dir-: a oscuras describe darkness
      //      directo (sin prompt de dirección).
      //   3. prompt «Dir-» @0x0028 `call 0xdcae` → pendingDungeonLook (arriba);
      //      la celda MIRADA encadena el "Will you drink?" de la fuente (0x012f).
      if (dk === "l") {
        // 🔴 `t()` AQUÍ, no después: `pushConsole` es el choke de i18n y traduce la
        // cadena ENTERA, pero "Look..." no existe en el binario —`CMD_STRINGS.look` es
        // "Look" y los puntos los pone este call-site—, así que el diccionario fallaba
        // y el eco salía en inglés jugando en español. Se traduce la CONSTANTE (que sí
        // está en el corpus) y se compone después. Mismo patrón que `yell` (1742).
        hud.echo(t(CMD_STRINGS.look) + "...");
        pickCommandChar(() => {
          if ((game.state.torchTurns ?? 0) <= 0 && (game.state.lightSpellMins ?? 0) <= 0) {
            applyEvents(game.dungeonCommand("look")); // darkness (0x752e), sin Dir-
            return;
          }
          pendingDungeonLook = true;
          hud.echo("Dir-"); // DS 0x84e6
          refreshAwaiting();
        });
        return;
      }
      // (G)et — SJOG 0x18CE rama mazmorra. OJO: el kernel SALTA el eco "Get-"
      // cuando loc≥0x21 (0x3274 jae 0x3282); el "Get\n" lo imprime el overlay (core).
      if (dk === "g") {
        applyEvents(game.dungeonCommand("get"));
        return;
      }
      // (J)immy — SJOG 0x0D4A → jimmy_dungeon 0x0C3E (desarmar trampa de cofre).
      // Eco del kernel "Jimmy-" (DS 0xa198) + "\n" del overlay (DS 0x8a7a).
      if (dk === "j") {
        hud.echo(CMD_STRINGS.jimmy);
        applyEvents(game.dungeonCommand("jimmy"));
        return;
      }
      // (Y)ell — CMDS 0x1418 rama no-fragata: "Yell what?\n:" + getstring; en
      // mazmorra (0x14ac) cualquier palabra → "\nNo effect!\n" (DS 0x453a).
      // game.yell() emite yell-word-prompt y yellWord cae en esa rama por location.
      if (dk === "y") {
        applyEvents(game.yell());
        return;
      }
      // (R)eady / (M)ix / (U)se / (N)ew Order / (Q)uit — handlers INCONDICIONALES
      // de contexto del kernel (0x339a/0x3340/0x340c/0x334e/0x338c): mismos flujos
      // que overworld/pueblo.
      if (dk === "r") {
        doReady();
        return;
      }
      if (dk === "m") {
        doMix();
        return;
      }
      if (dk === "u") {
        openUsePicker();
        return;
      }
      if (dk === "n") {
        pickMember(tf("Swap — who?"), (idx1) => {
          pickMember(tf("Swap with — who?"), (idx2) => {
            applyEvents(game.newOrder(idx1, idx2));
          });
        });
        return;
      }
      if (dk === "q") {
        doQuitSave();
        return;
      }
      // ── ECOS DE ERROR byte-exactos del despacho en mazmorra ──
      // (T)alk: kernel 0x33c2 rama loc>0x20 → "Talk-Funny, no response!\n"
      // (DS 0xa22c, un ÚNICO string). Devuelve 1 = turno.
      if (dk === "t") {
        hud.message("Talk-Funny, no response!\n");
        applyEvents(game.dungeonSpellTurn()); // turno de mazmorra sin efecto (ret 1)
        return;
      }
      // (P)ush: kernel 0x336a rama 0x20<loc<0x29 → "Push\nNot here!\n" (DS 0xa1d4)
      // vía 0x31e5 = ret 0, SIN turno.
      if (dk === "p") {
        hud.message("Push\nNot here!\n");
        return;
      }
      // (E)nter: kernel 0x3254 rama loc≠0 → "Enter what?\n" (DS 0xa156) + ret 1.
      if (dk === "e") {
        hud.message("Enter what?\n");
        applyEvents(game.dungeonSpellTurn());
        return;
      }
      // (F)ire: kernel 0x3266 "Fire-" (DS 0xa164) + CMDS 0x0AEA rama mazmorra
      // (0x0af7) → "What?\n" (DS 0x42e4). Ret 1 = turno.
      if (dk === "f") {
        hud.echo(CMD_STRINGS.fire);
        hud.message("What?\n");
        applyEvents(game.dungeonSpellTurn());
        return;
      }
      // (X)-it: kernel 0x3456 "X-it " (DS 0xa280) + CMDS 0x0EB4 a pie (0x0f14) →
      // "what?\n" (DS 0x4368, minúscula). Ret 1 = turno.
      if (dk === "x") {
        hud.echo(CMD_STRINGS.xit);
        hud.message("what?\n");
        applyEvents(game.dungeonSpellTurn());
        return;
      }
      // (B)oard: kernel 0x3236 "Board " (DS 0xa13a) + CMDS 0x07F6 rama mazmorra
      // (0x080a) → "\nNot here!\n" (DS 0x4252). Ret 1 = turno.
      if (dk === "b") {
        hud.echo(CMD_STRINGS.board);
        hud.message("\nNot here!\n");
        applyEvents(game.dungeonSpellTurn());
        return;
      }
      // (V)iew a gem en mazmorra — dispatcher 0x341A rama g_location>=0x21 (DNGLOOK
      // 0x06a8): misma mecánica (gate/consumo/turno), la vista muestra la planta 8×8.
      //
      // EL ECO VA AQUÍ, y no es simetría de estilo: el bucle de mazmorra NO tiene
      // despachador propio. DUNGEON.OVL manda toda tecla ≥0x20 al MISMO
      // `kernel_cmd_dispatch` 0x3178 — `07a0: push word ptr [bp+4]` + `07a3: call
      // 0xffffafa8`, y con load_seg 0x081D la base de near-call es 0x81D0, así que
      // 0xafa8+0x81D0 = 0x13178 → 0x3178 (command-dispatch.md §5 ya lo listaba como el
      // único call de ese bucle a la tabla de comandos). Su caso V
      // imprime la cadena de DS 0xa258 en el código 0x341e — ANTES del gate de gemas
      // (0x3421) y ANTES del `cmp [g_location],0x21` (0x342c) que bifurca a DNGLOOK. No
      // hay estado en el que 1988 abra la vista de mazmorra sin haberla impreso.
      // Censo con su control: DUNGEON.OVL.asm no tiene ninguna ocurrencia de a258 ni
      // ningún compare contra 0x56/0x76, y sí tiene 83 compares contra literales de un
      // byte (nibbles de tile) — o sea que el grep no está ciego, es que no hay segunda
      // rama que pudiera callar el eco. Sin esta línea la mazmorra era el ÚNICO contexto
      // mudo (Pass/Look/Jimmy/Fire/Xit/Board de este mismo bucle sí ecoan), y sin gemas
      // el aviso de DS 0xa266 salía suelto, sin la pulsación que lo provocó.
      // Presentación pura: cero RNG, cero turno.
      if (key.toLowerCase() === "v") {
        hud.echo(CMD_STRINGS.view); // ">View a gem!" (DS 0xa258, 0x341e)
        applyEvents(game.view());
        return;
      }
      // (C)ast en mazmorra: In Lor (luz → profundidad 4), Uus/Des Por (±planta) y los
      // hechizos permitidos por la ventana temporal de mazmorra. El turno lo cobra el
      // aplicador (dungeonMagicChangeLevel / dungeonSpellTurn), no aquí.
      if (key.toLowerCase() === "c") {
        doDungeonCast();
        return;
      }
      const cmd =
        key === "ArrowUp" ? "forward"
        : key === "ArrowDown" ? "back"
        : key === "ArrowLeft" ? "left"
        : key === "ArrowRight" ? "right"
        : key === "Enter" || key === "." ? "turnAround"
        : key.toLowerCase() === "k" ? "klimb"
        : key.toLowerCase() === "s" ? "search"
        : key.toLowerCase() === "o" ? "open"
        : key.toLowerCase() === "d" ? "drink"
        : key.toLowerCase() === "i" ? "ignite"
        : null;
      if (!cmd) {
        // 'Z' SÍ es comando en mazmorra (Ztats, kernel 0x3472): lo captura el
        // keyHandler modal de la piel (ztatsKeyReducer, listener en captura); si
        // propaga hasta aquí no debe caer al default.
        if (dk === "z") return;
        // DEFAULT del dispatcher en el bucle DUNGEON (misma jump table 0x3178,
        // default 0x34D8): tecla no-comando → "What?\n" SIN turno; 'W' (Wear) →
        // "W-What?\n" (0x3450). ('D' aquí es el atajo QoL de drink, divergencia
        // declarada arriba.) F5-F10/Tab = QoL del shell, mudas también en mazmorra.
        const printable = ev.key.length === 1 && ev.key >= " ";
        if (printable || /^F[1-4]$/.test(ev.key)) {
          if (dk === "w") hud.message("W-What?\n"); // 0x3450
          else hud.message("What?\n"); // 0x34D8, ret 0 = sin turno
        }
        return;
      }
      if (cmd === "ignite") {
        // Regla exacta del original (CMDS.OVL 0x0D98; re/notes/kernel-survival.md
        // §3). game.ignite() cobra además el coste de turno estándar del
        // comando (también si falla con "None owned!").
        applyEvents(game.ignite());
        hud.refresh();
        return;
      }
      // (S)earch — eco del case de mazmorra del dispatcher: "Search..." (DS 0xa204,
      // kernel 0x3332 rama loc>=0x21) ANTES del handler (SJOG search_dungeon 0x0646).
      // CADENA F2-T7 CABLEADA (antes bancada; derivación en dungeon.ts::search):
      //   1. selector de miembro 0x8a08 = kernel 0x4988 — MISMO helper que el
      //      (S)earch 2D (pickCommandChar: activo→directo, ≤1 elegible→auto, si no
      //      «Player: »+select; cancel→«None!» y ABORTA @0x0654, sin turno).
      //   2. gate de luz (0x065a): a oscuras el handler imprime darkness y NO
      //      pregunta dirección (el turno lo cobra dungeonCommand, como antes).
      //   3. prompt «Dir-» (0x0672, DS 0x84e6) → pendingDungeonSearch (arriba).
      if (cmd === "search") {
        hud.echo("Search..."); // DS 0xa204 ("Search...\n")
        pickCommandChar((mIdx) => {
          if ((game.state.torchTurns ?? 0) <= 0 && (game.state.lightSpellMins ?? 0) <= 0) {
            applyEvents(game.dungeonCommand("search")); // darkness (0x0668), sin Dir-
            return;
          }
          pendingDungeonSearch = mIdx;
          hud.echo("Dir-"); // DS 0x84e6
          refreshAwaiting();
        });
        return;
      }
      // (K)limb con escalera arriba Y abajo: el original NO decide solo — abre el
      // prompt "Klimb-U/D-" (0x6cba) y espera la vía. Pulsar K aquí NO cuesta turno
      // (el getkey del asm es libre); el turno lo cobra la resolución U/D/Pass. Con una
      // sola vía (o ninguna), dungeonCommand("klimb") resuelve directo como siempre.
      if (cmd === "klimb" && game.dungeonKlimbNeedsChoice()) {
        pendingDungeonKlimb = true;
        hud.echo("Klimb-U/D-");
        return;
      }
      // Klimb single-vía (F2-T7c): el eco del dispatcher "Klimb-" (DS 0xa1a0)
      // precede al "Up!"/"Down!" del handler — el port emitía "Up!" seco
      // (espejo S6; P16 muestra «>Klimb-Down!»).
      if (cmd === "klimb") {
        hud.echo(CMD_STRINGS.klimb); // "Klimb-" (DS 0xa1a0)
      }
      // (D)rink sobre una fuente: el original bebe MIRANDO la fuente y pregunta
      // "Will you drink?" (DS 0x7700) con getkey Y/N crudo (DNGLOOK 0x013b: bucle
      // 0x01ea que sólo rompe con 'Y'/'N'; otras teclas se ignoran, ESC no
      // decodificado). 'N' → "No." (DS 0x7712), sin efecto ni turno. 'Y' → "Yes.
      // Gulp!" (DS 0x7718) y el efecto de drinkFountain (turno + Cured!/Healed!/
      // Poisoned!/Bad taste.). Fuera de fuente, el atajo QoL 'd' bebe directo.
      if (cmd === "drink" && game.dungeonFountainHere()) {
        hud.message("Will you drink?"); // DS 0x7700
        prompts.current = {
          type: "yesno", // getkey Y/N (ESC ignorado, como el bucle 0x01ea)
          resolve: (yes) => {
            if (yes) {
              hud.message("Yes.  Gulp!"); // DS 0x7718 (dos espacios)
              applyEvents(game.dungeonCommand("drink"));
            } else {
              hud.message("No."); // DS 0x7712
            }
          },
        };
        refreshAwaiting();
        return;
      }
      applyEvents(game.dungeonCommand(cmd));
    };

    // La cámara/avatar de combate ya no se manipulan aquí: cada piel deriva su
    // presentación de los eventos combat-started/combat-ended (view.notifyTurn).
    // Banner de turno de PJ (COMBAT.OVL 0x0701-0x07af): "<nombre>, armed with <lista>:".
    // La LISTA (careo-combate T5) enumera, EN ESTE ORDEN, casco (+0x19) / mano izq
    // (+0x1b) / mano dcha (+0x1c) — helper 0x05b6, que añade el nombre SOLO si el item
    // tiene ATTACK_VALUES[id] != 0 (tabla DS 0x15fc; gate 0x05c5) — separadas por ", "
    // (DS 0x6da0); si NINGUNA ataca, "bare hands" (DS 0x6db2). ", armed with " = DS
    // 0x6da4; cierre ":" = DS 0x6dbe. Testigos: "Iolo, armed with Main Gauche, Short
    // Sword:" (vídeo-J) / "Geoffrey, armed with Spiked Helm, Mace, Spiked Shield:" (n6).
    // Se emite una vez por turno: el 1er turno desde enterCombatMode; los siguientes
    // desde el evento "turn" que dispara advanceTurn. video-N f060-f078.
    const announceCombatTurn = (
      unit: { kind: string; charIdx?: number } | null | undefined,
    ): void => {
      if (!unit || unit.kind !== "player" || unit.charIdx == null) return;
      const rec = game.state.characters[unit.charIdx];
      if (!rec) return;
      const atk = game.combatResources?.attackValues ?? [];
      const ids = [rec.helmet, rec.weapon, rec.shield].filter(
        (id): id is number => id != null && id !== 0xff && (atk[id] ?? 0) > 0,
      );
      const weapon = ids.length
        ? ids.map((id) => EQUIP_NAMES[id] ?? "bare hands").join(", ")
        : "bare hands";
      // LÍNEA EN BLANCO antes del banner: combat_player_turn abre con putchar('\n')
      // — COMBAT @0x06f1 `push 0xa; call 0x742a` (kernel putchar 0x16ba) — ANTES de
      // imprimir nombre + ", armed with" (@0x06fe COMSUBS 0x0094 + @0x0717 DS 0x6da4).
      // El log original separa cada bloque de turno con UNA fila vacía.
      //
      // #108: esto era `hud.message("")` bajo una guarda de «no dupliques si la fila
      // anterior ya está vacía». Ambas piezas eran compensación del defecto de filas:
      // la cadena VACÍA no imprime nada en el original (0x186f sale antes de tocar la
      // ventana), así que el blanco lo tiene que poner un `\n` de verdad; y la guarda
      // existía porque "*** CONFLICT ***\n" metía un blanco espurio que ya no existe
      // (con la aritmética fiel esa cadena da UNA fila). El putchar del binario es
      // INCONDICIONAL —`1742: inc byte ptr [si+5]` sin guarda, y §7.3 trampa 3: los
      // blancos son reales, no se des-duplican—, así que aquí tampoco hay condición.
      hud.message("\n");
      hud.message(tf("{}, armed with {}:", effectiveName(rec.name), weapon));
    };

    const enterCombatMode = (): void => {
      // Hallazgo R1 (auditoría): el interval de autoWalk (140 ms) solo se cancelaba
      // en map-changed/Blocked! — un combate iniciado A MITAD de una auto-marcha
      // dejaba a la party derivando por el mapa subyacente (walk-echos espurios +
      // reloj/hazards avanzando) durante toda la pelea. El original no tiene nada
      // parecido: el main-loop de combate posee el input entero.
      cancelAutoWalk();
      // El combate NO es una escena guionizada: en el original es una LOCALIZACIÓN — el
      // arnés escribe 0xFF en `g_location` (ULTIMA.EXE 0x5fb4) y el driver bifurca por ese
      // centinela. Va por `updateMusic()` y no por un `play("combat")` suelto para que la
      // OTRA rama del centinela también llegue: con el flag de victoria ya puesto suena el
      // tema de Ultima V, y eso incluye el caso de entrar a una sala SIN enemigos (latch
      // silencioso de `Combat`, calco de COMBAT.OVL 0x0bb2), donde el original nunca llega
      // a sonar «Engagement and Melee».
      //
      // Y va por `resumeMusic` —no por `updateMusic`— porque el combate puede arrancar
      // DENTRO de una escena de canción fija: la EMBOSCADA de la acampada. El original
      // hace exactamente esto y en este mismo punto — el arnés de arena llama al selector
      // 0x0f (ULTIMA.EXE 0x6069, `call 0x0e49`) JUSTO antes de entrar en
      // `combat_main_loop`, descongelando el «Stones» del camp para que se oiga la pelea.
      resumeMusic();
      // El arranque de combate lo anuncia el CORE con la secuencia fiel (lote D):
      //   <pre-línea contextual> → <grupo, monsterNamesUpper> → "*** CONFLICT ***\n" (DS
      //   0xa438) → 1er turno. Pre-línea: enemy-init "Attacked!\n" (0x2882), camp
      //   "Ambushed!\n\n" (0x41e0), player-init ninguna (el eco "Attack-<dir>" ya la puso).
      // El "*** COMBAT! ***" del clon era cruft (distinto); el banner REAL es "*** CONFLICT
      // ***". El "{name} attacks!" era FABRICADO; ambos purgados/corregidos.
      // currentUnit fija el 1er combatiente (findNextActor, sin RNG); si es PJ,
      // su banner de turno precede al input, como en el original.
      // Si el 1er actor es un PJ POSEÍDO por la Sword of Chaos (charmed al resolver el
      // turno), la IA lo conduce ya (pumpCombat anuncia el turno que aterrice vía combatOut).
      if (game.combat?.currentUnit?.charmed) pumpCombat();
      else if (game.combat?.currentUnit?.kind === "enemy") {
        // INICIATIVA ENEMIGA (hotfix combate #1/#2): el main-loop del original
        // (COMBAT 0x0B94 @0x0c84-0x0c90) conduce los turnos de IA SOLO, sin esperar
        // input — el port dejaba la arena CONGELADA (recuadro sobre el enemigo)
        // hasta la primera tecla, que además resolvía la tanda entera en ráfaga
        // (el «no actúan» + «salto» del reporte en vivo). Con beat>0 se arranca la
        // tanda paceada aquí; con beat 0 (webdriver) se conserva el flujo previo
        // (tanda al primer keypress) para no desplazar la semántica de teclas de
        // los drivers e2e/tour (digests intactos).
        if (ENEMY_BEAT_MS > 0) pumpCombat();
      } else announceCombatTurn(game.combat?.currentUnit);
    };
    const exitCombatMode = (): void => {
      view.setCombatAim(false); // por si se salió con el prompt de aim abierto
      updateMusic();
    };

    // Señales de MODAL de UI para el gate del cursor de consola (F-G). Se declaran
    // AQUÍ (antes de applyEvents/refreshAwaiting) para que la derivación sea segura
    // aunque se invoque durante el boot (TDZ); sus valores reales se asignan más
    // abajo, en los sitios de diálogo/selector/prompt.
    // El PROMPT VIVO (getkey/getstring modal, 9 tipos) + su reductor de teclas
    // viven en ui/prompt-manager.ts (TRAMO 2); aquí sólo se arma/lee `current`.
    const prompts = new PromptManager({ hud });
    // Conversación por CONSOLA (piel fiel/shader) — extraída a ui/talk-console.ts
    // (TRAMO 2). Se instancia AQUÍ (antes de refreshAwaiting) por el mismo motivo
    // TDZ que el resto de señales modales; `refreshAwaiting` llega como closure.
    const talkConsole = new TalkConsole({
      game,
      hud,
      prompts,
      refreshAwaiting: () => refreshAwaiting(),
      // Pausas del guion TLK (KeyWait 0x8F / Pause 0x83, bug 2 talk-celda-paginacion):
      // mismo discriminante de automatización que ShrineKeyPacer (#294) — bajo webdriver
      // o knob a 0 el volcado drena síncrono y los digests/sellos no se mueven.
      instant: () => TROLL_UNIT_MS === 0,
    });
    let shopConsole: ShopConsole | null = null;
    let selectorPanel: SelectorPanel | null = null;
    // (V)iew-a-gem de MAZMORRA renderizada en el canvas de la piel fiel (E1-S9 2b):
    // no usa el panel DOM, así que su cierre (cualquier tecla) se rastrea aquí.
    let canvasGemActive = false;
    /** ¿Cerrar la vista aérea cobra el turno de (V)? Falso cuando la abrió la bola de
     *  cristal (#144): esa vía entra por `cmd_look`, no por el case V del despachador. */
    let canvasGemChargesTurn = true;
    // Vista de zodíaco del catalejo (canvas de la piel fiel/shader): se cierra con
    // cualquier tecla, como la de gema. Se rastrea aquí (no usa panel DOM).
    let zodiacActive = false;

    // PACER de la tanda enemiga + cola de teclas (careo-combate T11) — extraído a
    // ui/combat-pacer.ts (derivación completa en su cabecera). `?combeat=<ms>` fuerza
    // el beat; BAJO AUTOMATIZACIÓN (navigator.webdriver) es 0 → tanda síncrona,
    // byte-idéntica al flujo previo (digests intactos).
    const beatParam = new URLSearchParams(window.location.search).get("combeat");
    const ENEMY_BEAT_MS =
      beatParam !== null
        ? Math.max(0, Number(beatParam) || 0)
        : navigator.webdriver
          ? 0
          : 400;
    const combatPacer = new CombatPacer({
      beatMs: ENEMY_BEAT_MS,
      combat: () => game.combat,
      combatOut: (evs) => combatOut(evs),
      endCombat: () => applyEvents(game.endCombat()),
      hudRefresh: () => hud.refresh(),
      refreshAwaiting: () => refreshAwaiting(),
      // Re-entrega de una tecla drenada (el getkey del original la recoge del
      // buffer BIOS al volver al await — mismo orden, ninguna perdida).
      handleKey: (key) => handleCombatKey({ key, preventDefault: () => {} } as KeyboardEvent),
    });
    const pumpCombat = (): void => combatPacer.pump();

    // F-G: apaga el cursor de consola mientras hay un MODAL de UI bloqueante
    // abierto (prompt Y/N/dígito del original, diálogo NPC, o selector Cast/Ready/
    // …) o mientras corre la tanda enemiga paceada (el prompt ► del combate solo
    // vive en el await del PJ). Se llama tras cada tecla y al abrir/cerrar modales.
    // ★ #329 — el predicado vive en `ui/awaiting-gate.ts`, no aquí: dentro de `boot()` no
    // era alcanzable por ningún test (boot no se exporta — misma medición de #237), y la
    // ficha pide guarda + mutante del gate. El criterio y la derivación de cada bandera
    // están en ese fichero; aquí sólo se leen los estados vivos.
    const refreshAwaiting = (): void => {
      view.setAwaitingInput(
        !isModalOpen({
          promptOpen: prompts.current != null,
          talkActive: talkConsole.active,
          shopOpen: shopConsole != null,
          camping: campSleepCtl.camping,
          sleeping: bedSleepCtl.sleeping,
          refuging,
          endgamePacing: endgamePacer.active,
          trollSneaking: trollSneakCtl.active,
          shrineScenePacing: shrineScenePacer.active,
          blackthornScenePacing: blackthornScenePacer.active, // #324: segmento de la captura
          shrineKeyWaiting: shrineKeyPacer.active, // NO modal desde #329 (ver el fichero)
          combatPacing: combatPacer.pacing,
          selectorVisible: selectorPanel?.visible ?? false,
        }),
      );
      // Cursor ANIMADO al final de la fila viva del prompt («To phase: ▓») — el calco del
      // `poll_key_blink_cursor` (0x1b38) que el bucle getkey del kernel (0x266c) ejecuta
      // en CADA iteración (cabo #341 §7.2; derivación y testigo en
      // re/notes/getkey-cursor-derivacion.md). Antes sólo cubría text/number/rune y vivía
      // en la cola del keydown: aquí cubre TODA la clase y se refresca también cuando un
      // prompt se arma/cierra fuera de un keydown (esos caminos ya llaman refreshAwaiting).
      // `talkConsole.cursorWaiting` (bug 2): el KeyWait del TLK espera en el MISMO bucle
      // `getkey 0x266c` que la población de PROMPT_TYPES_CURSOR_ON_LIVE_ROW — cursor
      // animado mientras dura (0x267f `call 0x1b38`); el Pause cronometrado NO (su bucle
      // TALK 0x0f92 no pasa por 0x266c) y el getter ya lo excluye.
      view.setAwaitingGetstring(
        promptCursorOnLiveRow(prompts.current?.type ?? null) || talkConsole.cursorWaiting,
      );
    };

    // Móvil (Lote 2): declara al deck táctil qué input espera el motor para que alce la hoja
    // justa (getnum→123, getstring/rúnico→A-Z, getkey Y/N→Sí-No, getdir→cruceta). Se deriva
    // del MISMO estado que gestiona el gate del cursor (prompts.current) más los `pendingDir*`.
    // Los prompts de tecla cruda (party-select/ready/shop) no fuerzan hoja: el usuario elige.
    // No-op en escritorio. Se llama desde el listener de COLA de keydown (línea final), NO
    // desde refreshAwaiting: refreshAwaiting corre durante el boot ANTES de que
    // `pendingDirCommand` se inicialice (declarado más abajo) — tocarlo ahí es TDZ. En la
    // cola de keydown todo el estado ya existe (mismo motivo que setAwaitingDirection).
    const syncTouchExpect = (): void => {
      // Con la LISTA DE HECHIZOS abierta no se declara input esperado: el getstring rúnico
      // que hay debajo lo va a teclear el panel, no el jugador, así que alzar la hoja A–Z
      // levantaría un teclado encima de la pantalla que existe para no tener que teclear.
      // (El prompt sigue armado y modal; lo único que se calla es la hoja del deck.)
      if (spellPickerOpen()) {
        setExpectedInput(null);
        closePartyChooser();
        closeShopPanel();
        return;
      }
      const pp = prompts.current;
      // ── PANEL COMPACTO DE TIENDA (auditoría de entrada manual, fase A) ─────────────
      // El prompt de tienda es un getkey CRUDO: no es dígito, ni Y/N, ni A-Z, así que el
      // deck nunca le alzó hoja (y con razón — ver el comentario de arriba). La
      // consecuencia en un teléfono era que la conversación entera del mercader (saludo,
      // menú por letra, lista de ocho reactivos, `Deal?`, epílogo) se contestaba a ciegas.
      //
      // Lo que cambia es LA SUPERFICIE, no el prompt: `syncShopPanel` pinta una fila por
      // opción que `ShopConsole.snapshot()` YA expone para esa fase, y cada una sintetiza
      // su letra por el mismo `press()` que usan los botones del deck. El conductor, el
      // prompt, los precios y el stream de rand no se enteran. Y NO toca `expected`: el
      // prompt de tienda sigue sin forzar hoja, exactamente como en Clásico.
      //
      // El gate es el régimen `u5.shopUI` y NO la chapa Enhanced: acertar una letra de un
      // menú recién impreso cuesta lo mismo en escritorio, así que esto se ofrece en las
      // dos superficies (misma decisión que la lista de hechizos). `panelDeTiendaActivo()`
      // sólo se consulta con una tienda abierta — construye un `URLSearchParams` y toca
      // `localStorage`, y esto corre en la cola de CADA keydown.
      const tiendaArmada = pp?.type === "shop" && shopConsole !== null;
      syncShopPanel({
        activo: tiendaArmada,
        disponible: tiendaArmada && panelDeTiendaActivo(),
        snapshot: tiendaArmada ? (shopConsole?.snapshot() ?? null) : null,
      });
      // ── SELECTOR COMPACTO DE MIEMBRO (auditoría de mandos móviles, 12-09) ──────────
      // `party-select` sigue siendo un prompt de DÍGITO —sus teclas son '1'..'N'— y por eso
      // seguía alzando la hoja «123» genérica: la rejilla de diez teclas que existe para las
      // cantidades de Mix y las donaciones. Medido en un iPhone SE (375×667) con party de 3,
      // eso inflaba el deck de 274 a 535 px (el 80 % del viewport) para ofrecer siete teclas
      // sin destino y cero nombres.
      //
      // Lo que cambia es LA SUPERFICIE, no el prompt: `syncPartyChooser` pinta una fila por
      // miembro y cada una SINTETIZA su dígito por el mismo `press()` que usan los botones
      // del deck. El reductor, el prompt, el cursor del roster y el grabador de repeticiones
      // no se enteran. Si se hace cargo, el numpad no se alza; si no (escritorio, chapa
      // clásica, party vacío), se cae a la rama de siempre y el numpad vuelve.
      // `chapaEnhancedViva()` y no `enhancedControlsActivo()`: esto corre en la cola de
      // CADA keydown, y hay que creerle al DOM (la clase de la que cuelga el CSS de la
      // chapa) y no a la preferencia — durante una conmutación en caliente pueden
      // discrepar un instante, y quien pinta encima del deck no puede adelantarse a él.
      const conSelectorPj = syncPartyChooser({
        activo: pp?.type === "party-select",
        disponible: chapaEnhancedViva(),
        state: game.state,
      });
      const awaitingDir =
        pendingDirCommand != null ||
        pendingCastDoor != null ||
        pendingCastUnlock ||
        pendingCastBlink ||
        pendingUseSkullKey;
      const expected: ExpectedInput =
        // `party-select` = el picker de PJ del kernel 0x2d7a (Cast/Ready/Ztats/Search…).
        // Entra aquí por el reporte del usuario del 27-07 noche («al hacer Cast y llegar
        // a seleccionar quién, hay que abrir el teclado a mano»): sus teclas son los
        // DÍGITOS 1-N, que eligen miembro directo (selectPartyMemberKey), así que la hoja
        // justa es el numpad. Las otras vías del picker no se pierden — las flechas del
        // cursor y el ⏎/Esc de confirmación viven FUERA de las hojas (cruceta y fila útil,
        // ambas permanentes en el portrait del prototipo).
        // ★ 12-09: y sólo cuando el selector compacto NO se ha hecho cargo. Con él vivo,
        // alzar además el numpad sería ofrecer las dos superficies a la vez para la misma
        // pregunta — el doble camino (y el doble alto) que esta fase viene a quitar.
        (pp?.type === "digit" || pp?.type === "number" ||
          (pp?.type === "party-select" && !conSelectorPj))
          ? "digit"
          : pp?.type === "text" || pp?.type === "rune"
            ? "string"
            : pp?.type === "yesno" || pp?.type === "yesno-esc"
              ? "yesno"
              : awaitingDir
                ? "dir"
                : null;
      setExpectedInput(expected);
    };

    /**
     * getstring de CONSOLA para las preguntas de TEXTO del juego (santuario, pozo,
     * interrogatorio de Blackthorn, contraseña del guardia). En 1988 NO hay ventana ni
     * botones: las cinco preguntas imprimen su literal con `print_string` y leen la
     * respuesta con el MISMO `input_string` del kernel (ULTIMA.EXE 0x3b1c), ecoada en
     * línea sobre la consola del marco EGA. Es el camino que ya recorrían (Y)ell
     * (`yell-word-prompt`, abajo) y la charla (`ui/talk-console.ts`); estas cinco iban
     * por `SelectorPanel.prompt` — un modal DOM con input y botones Speak/Cancel, ajeno
     * al original (reporte del usuario 14-08, Shrine of Honesty en iPhone).
     *
     * Rendir el prompt aquí lo pone en la fuente de píxeles y, de paso, resuelve el
     * teclado táctil SIN maquinaria nueva: `prompts.current.type === "text"` hace que la
     * cola de keydown declare `ExpectedInput = "string"` (`syncTouchExpect`), el deck
     * alza la hoja A-Z y `expectedSheetListener` avisa SÍNCRONAMENTE al puente del
     * teclado del sistema (`skin/portrait/deck-nativo.ts`) DENTRO del gesto — que es la
     * única forma de que iOS despliegue el teclado. El `<input>` real sigue existiendo:
     * es el campo invisible de 1×1 px de ese puente, que traduce `beforeinput` a keydown
     * sintético. En escritorio el teclado físico entra por `PromptManager.handleKey` sin
     * intermediario.
     *
     * El literal se PARTE en dos donde el original pone el salto de fila que precede a la
     * lectura: `lines` es lo que imprime `print_string` y `prefix` lo que queda DELANTE
     * del texto tecleado en la fila de eco. Los tres repartos del corpus, y por qué no
     * son intercambiables:
     *   · `…meditate?\n\n:`  → lines con el `\n` (fila en blanco), prefix `":"`.
     *   · `\nMantra:`        → SIN lines: el `:` no está tras un salto, así que el eco va
     *                          en la MISMA fila y el prefijo entero es `"Mantra:"`.
     *   · `\nThy wish?\n`    → lines con la pregunta, prefix `""`: ese literal NO lleva
     *                          `:`, y ponérselo sería inventar un cursor que 1988 no pinta.
     */
    const askText = (
      lines: string,
      prefix: string,
      max: number,
      onText: (text: string) => void,
    ): void => {
      if (lines) hud.message(lines);
      hud.echoCursor(prefix); // fila del getstring (sin bullet, con cursor al final)
      prompts.current = {
        type: "text",
        prefix,
        buffer: "",
        max,
        resolve: onText,
        // ESC / vaciar+backspace: se resuelve con cadena VACÍA en vez de dejar el prompt
        // colgando. El original aborta la ceremonia con la entrada vacía (CAST2 0x09cc
        // `cmp byte [0xbd08],0`), y resolver mantiene UNA sola salida: la del core, que
        // es quien limpia su `pending`. El modal retirado NO llamaba al submit en su
        // botón Cancel y dejaba `pending.visit` sin limpiar.
        cancel: () => onText(""),
      };
      refreshAwaiting();
    };

    const applyEvents = (events: ReturnType<Game["move"]>): void => {
      // ★ #371 — LOS CUES DEL PREFIJO CONSUMIDO SE PUBLICAN ANTES DE CORTAR EL BUCLE.
      // El único enrutador de {kind:"sfx"} de un turno es el `view.notifyTurn(events)` del
      // FINAL de esta función (auto-enrutado de coreview.notifyTurn), y las CINCO ramas que
      // cortan con `return` (refuge, troll-sneak, shrine-scene, shrine-key-wait, endgame) lo
      // saltan: todo cue ANTERIOR al corte moría sin sonar. Medido en #371 (censo de
      // osciladores, modo humano, control positivo de pisadas en la misma corrida): TONO=0
      // en donación (#364), WELL DONE (#295/#345) y melodía del ORDAINED (#364-b) — el
      // ALAKAZAM y el XOR salían (los maneja su rama del bucle) y el barrido no (su enrutado
      // vivía después del return). Vigente desde #277: la salida de escena puso un evento
      // cortante en TODAS las ramas terminales del rito. Los tests de EMISIÓN (core) y de
      // CATÁLOGO (speaker) no podían verlo: el hueco está exactamente entre los dos.
      // Los eventos DIFERIDOS (rest) NO se flushean aquí: cuando el pacer los re-aplica, o
      // completan el bucle (y notifyTurn los enruta) o vuelven a cortar (y este helper los
      // flushea entonces) — nunca dos veces, porque el prefijo cortado jamás llega a
      // notifyTurn.
      // ★ #373 — EL CABO HERMANO DE #371, CERRADO: el corte también mataba los kinds
      // VISUALES del prefijo cuyo único enrutador era el `onTurn` de las pieles. CENSO de
      // los kinds del batch que las ramas terminales dejaban morir (derivado del par
      // productor/consumidor: qué emite el core en el batch × qué se consume SOLO en
      // notifyTurn/onTurn):
      //   · sfx             → reparado por #371 (emitSfx del prefijo, abajo).
      //   · quake           → MEDIDO muerto (los tres del Códice, CAST2 0x0dc0/0dd7/0dee:
      //                       rumble sonando post-#371 y pantalla quieta). Ficha #373.
      //   · cell-explosion  → misma clase, sin repro barato hoy (ningún batch actual la
      //   · cell-projectile   pone delante de un corte; un cañonazo cuyo housekeeping
      //                       acabe en refuge la produciría). Cubiertos POR CONSTRUCCIÓN:
      //                       el flush publica el prefijo entero, no un kind suelto.
      //   · moved / party-changed / map-changed → NO mueren: ningún consumidor por-evento
      //     en onTurn (la piel lee snapshot); su efecto es de lote (render/hud.refresh) y
      //     lo cubren los pacers de las escenas y el turno que completa.
      // `emitTurnFx` entrega el prefijo ENTERO a las pieles (onTurnFx → applyTurnFx =
      // la MISMA planTurnPhase del turno completo: los cues bloqueantes del prefijo
      // deciden CUÁNDO arranca cada visual, y las 3 ráfagas del Códice sacuden SOSTENIDO
      // como en el lote sin cortar). Sin bookkeeping de turno: `personTurnCount` («un
      // TURNO = un avance de frame») sigue moviéndose SOLO en onTurn. El argumento
      // sin-dobles es el MISMO de #371 y cubre a los dos canales a la vez.
      const flushEventPrefix = (upto: number): void => {
        // ★ #208 — el prefijo también lleva su fase: la MISMA `planTurnPhase` (sobre el
        // prefijo, que es el lote que las pieles verán en `onTurnFx`) decide el lead de
        // cada cue. Población hoy: todos los prefijos reales dan 0 (los pares
        // {quake, rumble} del Códice van alineados a su ventana) — se cablea igual para
        // que el corte de una rama terminal no pueda hacer perder la fase a un lote
        // futuro que sí la tenga, que es exactamente la clase de hueco de #371/#373.
        const prefix = events.slice(0, upto);
        const { sfxLeadMs } = planTurnPhase(prefix);
        for (let i = 0; i < upto; i++) {
          const pe = events[i]!;
          if (pe.kind === "sfx" && pe.sfx) view.emitSfx(pe.sfx, sfxLeadMs[i]);
        }
        view.emitTurnFx(prefix);
      };
      for (const e of events) {
        // Party-wipe: la escena de MUERTE + RESURRECCIÓN de Lord British (BLCKTHRN 0x0910).
        // El core NO ha mutado nada (roster sigue caído); aquí se MONTA el modal paceado y, al
        // terminar, `runRefugeScene` llama a `resolveRefuge` (revive + despertar en el castillo).
        // Es el evento TERMINAL del turno (checkRefuge es lo último): se corta el bucle.
        if (e.kind === "refuge" && e.refuge) {
          flushEventPrefix(events.indexOf(e)); // #371/#373: cues y fx ya recorridos se publican antes del corte
          runRefugeScene(e.refuge);
          return;
        }
        // ★ #213 — TICK DE VENENO: arranca el paceador y SIGUE el bucle (a diferencia
        // del troll, esto no es modal ni difiere nada: en el binario ocurre DENTRO del
        // housekeeping del turno, que ya terminó).
        if (e.kind === "poison-tick" && e.poisonTick) {
          poisonTickCtl.run(e.poisonTick);
          continue;
        }
        // Cruce del puente con trolls (MAINOUT 0x1c0e-0x1ca6): el guión se pacea a
        // reloj de pared y el RESTO del turno (troll-toll-prompt `Caught!...` o
        // nada) se difiere hasta agotar los beats — se corta el bucle aquí. Bajo
        // automatización (unidad 0) `trollSneakCtl.run` drena todo síncrono.
        if (e.kind === "troll-sneak" && e.trollSneak) {
          flushEventPrefix(events.indexOf(e)); // #371/#373: cues y fx ya recorridos se publican antes del corte
          trollSneakCtl.run(e.trollSneak, events.slice(events.indexOf(e) + 1));
          return;
        }
        // ESCENA DEL SANTUARIO / CODEX (#277, CAST2 0x0e76): mismo trato que el troll —
        // se pacea el guión (mapa propio + caminata + arrodillarse) y el RESTO del turno
        // se DIFIERE hasta agotar los beats, que es lo que hace el original al ser todo
        // síncrono dentro de la rutina. Así el «Upon what virtue…» sale con el Avatar YA
        // arrodillado ante el altar. Bajo automatización (unidad 0) drena síncrono.
        if (e.kind === "shrine-scene" && e.shrineScene) {
          const shrineScript = e.shrineScene;
          const shrineRest = events.slice(events.indexOf(e) + 1);
          // «Stones» con canción FIJA durante todo el rito: MAINOUT.OVL 0x0968 lo pone
          // (glue 0x0e50 = selector 0x12) ANTES de entrar en `shrine_visit` y congela la
          // música, de modo que la explanada no suena a sobremundo. Lo descongela
          // `onSceneEnd` del pacer. Se pide en las DOS mitades del guión (entrada y
          // salida): el de-duplicado va por canción, así que la segunda no la reinicia.
          music.play("shrine");
          // ★ #371 — el flush va ANTES del aparcamiento: los barridos de la donación y del
          // WELL DONE suenan DENTRO de la inversión (el binario los mete entre el rect XOR
          // y el kernel_flash), y la melodía del ORDAINED tras su último print. La escena
          // de salida (este evento) corre detrás, como en 1988.
          flushEventPrefix(events.indexOf(e));
          // ★ #330(B) — SERIALIZACIÓN con la inversión del WELL DONE. En el binario el
          // `kernel_flash(10)` de 0x0d1a restaura el viewport y sólo DESPUÉS vienen el `ret`
          // de `shrine_visit` y la caminata de salida del envoltorio (0x1075-0x10bd): nunca
          // se solapan. Aquí las dos son de reloj de pared y arrancaban en el MISMO
          // `applyEvents`, con la inversión (5.347,5 ms) durando más que la salida (2.251 ms)
          // ⇒ el negativo se veía ya de vuelta en el sobremundo. Aparcar la salida detrás de
          // la restauración reproduce el orden del binario por construcción.
          // Bajo automatización la ventana es 0 ⇒ `inverted` es false y esto no se toma:
          // mismo orden de eventos que antes en e2e y digests.
          if (ritualInvertCtl.inverted) {
            ritualInvertCtl.whenRestored(() => shrineScenePacer.run(shrineScript, shrineRest));
            return;
          }
          shrineScenePacer.run(shrineScript, shrineRest);
          return;
        }
        // ★ #324 — ESCENA DE LA CAPTURA de Blackthorn: mismo trato que el rito — se
        // pacea el segmento (apagón/sala/guiones del VM) y el RESTO del turno se
        // DIFIERE hasta agotar los beats (en el binario todo es síncrono dentro de
        // 0x060e). Las esperas de tecla de la captura reusan `shrine-key-wait` (abajo).
        // Bajo automatización (unidad 0) drena síncrono.
        if (e.kind === "blackthorn-scene" && e.blackthornScene) {
          flushEventPrefix(events.indexOf(e)); // #371/#373: cues y fx del prefijo se publican antes del corte
          // La CAPTURA es la otra secuencia que el parche deja MUDA a propósito (History.txt,
          // junto con la muerte): TOWN.OVL 0x12ca para la música (glue 0x0e72) antes de
          // llamar a `blackthorn_capture` y la reactiva al volver. El mando se devuelve en
          // el `onSceneEnd` del pacer, ya con la party depositada en la celda.
          music.play("silence");
          blackthornScenePacer.run(e.blackthornScene, events.slice(events.indexOf(e) + 1));
          return;
        }
        // ★ #294 — ESPERA DE TECLA del rito (CAST2 `call 0x448c`): el original PARA aquí
        // con el viewport redibujándose hasta que el jugador pulsa algo, así que el resto
        // del turno se APARCA y lo reanuda `consumeKey`. `indexOf` funciona porque cada
        // marcador es un objeto NUEVO (`keyWait()` en shrine-ceremonies.ts) — con uno
        // compartido, las nueve esperas del Códice cortarían todas por la primera.
        // Bajo automatización `wait` devuelve false y el bucle sigue: mismo orden de
        // eventos que antes de la ficha (e2e y digests intactos).
        if (e.kind === "shrine-key-wait") {
          if (shrineKeyPacer.wait(events.slice(events.indexOf(e) + 1))) {
            flushEventPrefix(events.indexOf(e)); // #371/#373: los quake del Códice (rumble Y sacudida) viajan troceados entre esperas
            return;
          }
          continue;
        }
        // ★ #295 — INVERSIÓN del viewport del WELL DONE. NO corta el bucle (a diferencia de
        // las escenas paceadas): el original sigue imprimiendo el «Strength +1» CON el
        // viewport ya invertido, así que el resto de eventos del turno tienen que seguir
        // corriendo. La ventana se cierra sola por temporizador = el `kernel_flash(10)`
        // de 0x0d1a; su duración se DERIVA de los dos barridos de altavoz de 0x0c44-0x0c85
        // MÁS la sacudida de 0x0c88 (#355: el trueno encadenado suena DENTRO del negativo).
        // ★ #364 — el ALAKAZAM de la donación comparte el régimen (rect XOR suelto 0x0bcd →
        // flash 0xd16) pero SU ventana sale de SUS barridos (count 0xc8, no 0x96): el campo
        // `ritual` del evento elige de qué cue derivarla. Ausente = WELL DONE (#295).
        if (e.kind === "ritual-invert") {
          ritualInvertCtl.run(
            e.ritual === "donation" ? donationInvertWindowMs() : wellDoneInvertWindowMs(),
          );
          continue;
        }
        // ENDGAME (#34): el core emitió el GUIÓN completo del cierre (con endgameText
        // inyectado). Es el evento TERMINAL de la partida: se monta el pacer y se corta
        // el bucle (nada después de él importa — el juego no vuelve).
        if (e.kind === "endgame" && e.endgame) {
          flushEventPrefix(events.indexOf(e)); // #371/#373: cues y fx ya recorridos se publican antes del corte
          endgamePacer.run(e.endgame);
          return;
        }
        // Cartel (L)ook: el DOS lo imprime EN EL FLUJO DE LA CONSOLA (no en un overlay) —
        // `pushSignBox` empuja la caja (marco RUNES.CH + cuerpo rúnico) como filas de log,
        // conservando el cuerpo latín en el `.text` de cada fila (historial + e2e). Sustituye
        // al `hud.message(e.text)` de un mensaje normal (el `text` de este evento es el mismo
        // cuerpo multilínea, que ya no hace falta empujar aparte). La shader hereda por nearest.
        if (e.kind === "message" && e.signLines && e.signLines.length > 0) {
          view.pushSignBox(e.signLines, e.signRaw);
        } else if (e.kind === "message" && e.text) {
          // Mensaje MIXTO (#364-c): los tramos {text,rune} llevan el cambio de fuente a
          // mitad de fila; `text` sigue siendo la concatenación (historial/e2e intactos).
          if (e.segments) hud.messageSegments(e.segments);
          else hud.message(e.text, e.rune);
        }
        // Eco de dirección por paso (#61): línea de consola con bullet ► (kind echo),
        // como el ">North" del original (MAINOUT 0x0500).
        if (e.kind === "walk-echo" && e.text) hud.echo(e.text);
        if (e.kind === "combat-started") enterCombatMode();
        if (e.kind === "combat-ended") exitCombatMode();
        if (e.kind === "dungeon-entered") {
          music.play("dungeon");
          // Sin pista de control del clon en la consola (espacio de juego) — retirada
          // en TODOS los skins por decisión del usuario. La ayuda vive fuera del render.
        }
        if (e.kind === "dungeon-exited") updateMusic();
        // (V)iew a gem: abre la vista aérea modal (game.view ya consumió la gema y
        // cobró el turno). Se cierra con cualquier tecla (rama de arriba del keydown).
        if (e.kind === "gem-view" && e.gemView) {
          view.setGemView(e.gemView);
          // #144: la vista abierta por la BOLA DE CRISTAL (LOOKOBJ 0x0a3b) no atraviesa
          // el case V del despachador, así que al cerrarla NO se cobra el turno de (V).
          canvasGemChargesTurn = e.gemFromCrystalBall !== true;
          // La vista de gema se pinta EN EL CANVAS del viewport (fiel y shader, que
          // envuelve a la fiel). Mazmorra 8×8 (E1-S9 2b) y overworld/pueblo 32×32 EGA por
          // categoría (#76, gem_view LOOKOBJ 0x10fc). (El panel DOM era de la piel dev.)
          canvasGemActive = true;
        }
        // (U)se Spyglass de noche: vista de zodíaco modal en el viewport. Cosmética; se
        // cierra con cualquier tecla (rama de arriba del keydown, junto a la de gema).
        if (e.kind === "zodiac-view" && e.zodiacView) {
          view.setZodiacView(e.zodiacView);
          zodiacActive = true;
        }
        if (e.kind === "game-won") {
          // Fork del endgame (#20 L3/L4): SÓLO la victoria (con la Sandalwood Box) abre
          // el pergamino de cierre (endgame_datestamp 0x0326). El final "varado" (sin la
          // caja) no lo muestra — su diálogo ("pull up a chair…") ya está en el log.
          // #34: si el turno trae el GUIÓN completo (evento endgame con script), el
          // pergamino lo estampa la FASE scroll del pacer — se suprime la vía DOM vieja
          // (que queda como fallback sin endgame.json).
          const scripted = events.some((ev2) => ev2.kind === "endgame" && ev2.endgame);
          if (e.ending !== "stranded" && !scripted) {
            showEndgameScroll(parent, questScroll(game.state));
          }
          // Con GUIÓN la música la lleva el pacer fase a fase (ver su `onPhase`); sin él
          // —fallback DOM, sin `endgame.json`— no hay fases que seguir, así que se pone
          // directamente el cierre. Antes sonaba aquí el tema de PORTADA, que en el
          // original no toca el endgame: su última llamada al driver es Rule Britannia
          // (ENDGAME.OVL 0x0b18), y es incondicional — también en el final varado.
          if (!scripted) music.play("finale");
        }
        // Prompt Y/N interactivo de salida de pueblo (F1.3 Flow 1). El resolve
        // re-entra en `game` y vuelve a llamar applyEvents (re-entrada segura,
        // igual que la palabra de poder). Los flujos 2 (troll-toll) y 3
        // (shrine-donate) siguen el mismo patrón más abajo.
        if (e.kind === "town-exit-prompt") {
          // "\nDost thou wish to leave? " (DATA.OVL 0x26a0, acaba en ESPACIO sin \n) →
          // la respuesta va INLINE tras la pregunta, como el peaje: TOWN 0x07be imprime
          // la PALABRA ("Yes\n\nExit to\n" 0x26bb / "No\n" 0x26e2) con print_string sin
          // \n previo. El eco lo pone el reductor (aquí); el CORE emite sólo la
          // CONSECUENCIA ("\nExit to\nBritannia!" en Y; nada en N salvo el turno).
          hud.message("Dost thou wish to leave? "); // DATA.OVL 0x26a0
          prompts.current = {
            type: "yesno-esc",
            resolve: (yes) => {
              hud.messageAppend(yes ? "Yes" : "No"); // inline (0x26bb "Yes\n…" / 0x26e2 "No\n")
              applyEvents(game.confirmTownExit(yes));
            },
          };
        }
        // Prompt Y/N del peaje de trolls (F1.3 Flow 2, MAINOUT 0x1B3E). Tipo
        // `yesno`: ESC IGNORADO (⚠ distinto de Flow 1). El resolve re-entra en
        // game.resolveTrollToll (pago→cola diferida / rechazo→combate). A diferencia
        // del getYN de palabra (Quit), aquí el original ecoa el CHAR CRUDO de la tecla
        // 'Y'/'N' + LF INLINE tras "Dost thou pay?" (MAINOUT 0x1b90-0x1b9d: putchar
        // [bp-2] + putchar 0xa). messageAppend NO traduce → el char es mecánico, correcto.
        if (e.kind === "troll-toll-prompt" && e.toll !== undefined) {
          // TICKET-001 del corpus yt (re/notes/yt-careo-tickets.md): el literal fiel
          // lleva el prefijo «Caught!» y DOBLE salto (DATA.OVL 0x6b3c: `Caught!\n\nThe trolls
          // demand a ` + ` gp toll!\n\nDost thou pay?`) — la forma aplanada era divergencia.
          hud.message(`Caught!\n\nThe trolls demand a ${e.toll} gp toll!\n\nDost thou pay?`); // 0x6b3c+0x6b4a
          prompts.current = {
            type: "yesno",
            tag: "troll-toll", // hook e2e guardPromptOpen (el arnés del espejo REHÚSA: el LP no pagó)
            resolve: (yes) => {
              hud.messageAppend(yes ? "Y" : "N"); // eco del char crudo (MAINOUT 0x1b90)
              applyEvents(game.resolveTrollToll(yes));
            },
          };
        }
        // Prompt Y/N de la demanda de un guardia de pueblo (F2-T4, TALK 0x01e2).
        // Tributo (DS 0x90cc + N + 0x90e0) o caridad de Minoc (0x22 + DS 0x90a2 +
        // 0x22); ambos cierran con el helper Y/N 0x00ac (DS 0x9052 `\n\nDost thou
        // pay?\n\n:`) que ECOA LA PALABRA (DS 0x9066 `Yes` / 0x906c `No!`) tras el
        // ':' antes de resolver. El testigo (arrest-ep3, OCR): «A guard demands a
        // 20 gp tribute to Blackthorn! / Dost thou pay? :Yes».
        if (e.kind === "guard-tribute-prompt") {
          if (e.charity) {
            hud.message('"Thou wilt give\nhalf thy gold to\ncharity!"\n\nDost thou pay?\n\n:'); // 0x90a2+0x9052
          } else {
            // COMPUESTO → tf() en call-site (el choke t() de pushConsole no puede
            // re-mapear el número al template; regla i18n-plantillas).
            hud.message(tf("A guard demands\na {} gp tribute\nto Blackthorn!\n\nDost thou pay?\n\n:", e.toll ?? 0)); // 0x90cc+0x90e0+0x9052
          }
          prompts.current = {
            type: "yesno",
            tag: "guard-tribute", // hook e2e guardPromptOpen (el tour paga)
            resolve: (yes) => {
              hud.messageAppend(yes ? "Yes" : "No!"); // eco palabra (TALK 0x00c7/0x00d8)
              applyEvents(game.resolveGuardTribute(yes));
            },
          };
        }
        // #301 — el NPC ARRANCA la conversación por quedarse ADYACENTE, sin (T)alk
        // (NPC.OVL 0x06e4 manhattan==1 + aiType 4/5 → npc_engine TOWN 0x13ce → TALK
        // 0x031E). Se abre la MISMA consola que el comando (T)alk porque en el binario
        // es literalmente la misma rutina: los dos caminos convergen en
        // `talk_converse_dispatch`. Por eso NO hay motor paralelo ni texto propio —
        // el saludo, la descripción «You see …», las preguntas con `You respond-` y
        // el cierre salen del .TLK igual que siempre.
        if (e.kind === "npc-initiates-talk" && e.initiatesTalk) {
          const target = game.talkScriptFor(e.initiatesTalk.npc);
          if (target) talkConsole.start(target);
        }
        // #304 — el mismo mecanismo con un TENDERO: `talk_converse_dispatch` manda la
        // familia 0x80..0xFC a 0x03e4 y, pasado el tramo, a `call 0xe6` = la tienda. El
        // core ya aplicó el gate horario (`shopIsOpen`) y la guarda del caballo; aquí
        // sólo queda lo que el binario hace DENTRO de 0xe6 antes de repartir por tipo:
        // `dlgNum − 0x81` contra la tabla de OCHO (0x017a `cmp ax,7 / ja 0x1da` ⇒ fuera
        // de los ocho no abre nada, y ése es el `undefined` de SHOP_TYPES). Se abre la
        // MISMA consola del comando (T)alk, no una paralela.
        if (e.kind === "npc-initiates-shop" && e.initiatesShop) {
          const npc = e.initiatesShop.npc;
          const shopType = SHOP_TYPES[npc.dialogNumber];
          if (shopType) {
            const info = shoppeKeeperAt(
              game.state.position.location,
              shopType,
              shoppeKeeperMapJson as unknown as Record<string, ShoppeKeeperMapEntry>,
              (data.storeNames as string[]) ?? [],
              (data.shoppeKeeperNames as string[]) ?? [],
            );
            startShopConsole(shopType, info);
          }
        }
        // Prompt Y/N del arresto (F2-T4, TOWN 0x12ae rama pueblo): DS 0x27e2 + 0x27fe.
        // El eco NO se añade aquí: va HORNEADO en las consecuencias del core (DS
        // 0x281b `Yes\n\n…unconscious!` / 0x285e `No\n\n"Then defend thyself…`).
        if (e.kind === "guard-arrest-prompt") {
          hud.message('\n"Thou art under arrest!"\n\n"Wilt thou come quietly?"\n\n:'); // 0x27e2+0x27fe
          prompts.current = {
            type: "yesno",
            tag: "guard-arrest", // hook e2e guardPromptOpen
            resolve: (yes) => {
              applyEvents(game.resolveGuardArrest(yes));
            },
          };
        }
        // Prompt de DÍGITO de la donación de santuario (F1.3 Flow 3, CAST2 0x0B1D).
        // Tipo `digit`: solo '0'-'9' (getkey crudo 0x448c, ESC/otras ignoradas). El
        // resolve re-entra en game.submitDonation; si el oro no alcanza, éste re-emite
        // `shrine-donate-prompt` → applyEvents re-arma este mismo prompt (el bucle del
        // original). El TRIGGER de meditación que lo lanza es F1.4.
        // Prompt derivado de MISCMSG.DAT 0x081f (buffer 0xb692, cargado por el loader
        // CAST2 0x0ef9 desde el offset de fichero 0x3ab). CAST2 0x0b63 lo imprime.
        if (e.kind === "shrine-donate-prompt") {
          hud.message("\n\nOffer how many hundredweights gold? "); // MISCMSG.DAT 0x081f
          prompts.current = {
            type: "digit",
            resolve: (n) => applyEvents(game.submitDonation(n)),
          };
        }
        // (Sin prompt "Meditate?": la ceremonia de santuario/Codex corre AL PISAR — sus
        // mensajes (virtud/mantra/lección) y el shrine-donate-prompt ya vienen en los
        // eventos del move. El yes/no del clon era fabricado, retirado en fiel/shrine-flow.)
        // Los DOS ritos de santuario piden virtud + mantra×3 por TEXTO, y los dos leen por
        // la CONSOLA (getstring del kernel), no por ventana — literales y cotas en
        // SHRINE_UI. Visita a santuario VIVO = interrogatorio (CAST2 0x09c1 getstring +
        // bucle Mantra ×3 0x0a0c); restauración del destruido = CMDS 0x1202 al pisar el
        // tile 0x1a. El juego ya conoce virtud/coord (pendingVisit/pendingRestore); aquí
        // sólo se capturan las cadenas tecleadas, y el resultado lo ramifica el core
        // (ordained/complete/donación/unfocused).
        if (e.kind === "shrine-visit-prompt") {
          // Cota del getstring: CAST2 0x09c5 `mov ax,0xc` sobre el buffer DS 0xbd08 ⇒ 12
          // caracteres, y el MISMO buffer se reusa en los tres mantras (0x0a17).
          const max = 0xc;
          let typedVirtue = "";
          const mantras: string[] = [];
          const askMantra = (): void => {
            if (mantras.length === 3) {
              applyEvents(game.submitShrineVisit(typedVirtue, mantras));
              return;
            }
            // "\nMantra:" — DATA.OVL DS 0x958e (fileoff 0x959e), impreso por CAST2 0x0a0c
            // en cada vuelta. El `:` NO va tras salto de fila ⇒ es PREFIJO del eco.
            askText("", SHRINE_UI.mantra, max, (m) => {
              mantras.push(m);
              if (m === "") applyEvents(game.submitShrineVisit(typedVirtue, mantras));
              else askMantra();
            });
          };
          // "Upon what virtue dost thou meditate?\n\n:" — MISCMSG.DAT 0x743 (buffer DS
          // 0xb5b6, cargado por CAST2 0x0ef9 desde el offset de fichero 0x3ab; CAST2
          // 0x09ba lo imprime).
          askText(`${SHRINE_UI.virtueVisit}\n`, TALK_UI.cursor, max, (vn) => {
            typedVirtue = vn;
            // Entrada VACÍA = salir del rito SIN recorrer el resto de preguntas, como el
            // binario (CAST2 0x09cc `cmp byte [0xbd08],0 / je` para la virtud; 0x0a1e/
            // 0x0a23 para cada mantra). Es además lo que hace ESC, que resuelve vacío.
            // ⚠ RESIDUO DECLARADO: el original ABORTA EN SILENCIO y el port imprime
            // «Thine thoughts are unfocused.» — la cadena vacía entra en el mismo camino
            // de fallo del core. Corregirlo pide una salida propia en shrine-ceremonies,
            // que es mecánica del rito y no de la entrada de texto (fuera de #268).
            if (vn === "") applyEvents(game.submitShrineVisit(vn, mantras));
            else askMantra();
          });
        }
        if (e.kind === "shrine-restore-prompt") {
          // Rito DISTINTO del de arriba: otra rutina, otro literal (partido en tres filas)
          // y otra cota — CMDS 0x121a/0x125a `mov ax,0xf` sobre el buffer de pila [bp-0x10].
          const max = SHRINE_UI.maxRestore;
          let typedVirtue = "";
          const mantras: string[] = [];
          const askMantra = (): void => {
            if (mantras.length === 3) {
              applyEvents(game.submitShrineRestore(typedVirtue, mantras));
              return;
            }
            askText("", SHRINE_UI.mantra, max, (m) => {
              mantras.push(m);
              if (m === "") applyEvents(game.submitShrineRestore(typedVirtue, mantras));
              else askMantra();
            });
          };
          askText(`${SHRINE_UI.virtueRestore}\n`, TALK_UI.cursor, max, (vn) => {
            typedVirtue = vn;
            if (vn === "") applyEvents(game.submitShrineRestore(vn, mantras)); // ver visita
            else askMantra();
          });
        }
        // Pozo — LOOK a un pozo imprime "a well.\n\nDrop a coin?" byte-exacto (LOOKOBJ
        // 0x0048, DATA.OVL DS 0x720c): la descripción del objeto + el prompt en un solo
        // print (el pozo se especial-casa antes del "Thou dost see" genérico). Tipo yesno-esc.
        if (e.kind === "well-drop-prompt") {
          hud.message("a well.\n\nDrop a coin?"); // [D] LOOKOBJ 0x0048 / DATA.OVL DS 0x720c
          prompts.current = {
            type: "yesno-esc",
            resolve: (yes) => applyEvents(game.dropCoin(yes)),
          };
        }
        // Pozo — "\nThy wish?\n" (F1.4). DATA.OVL DS 0x722c (fileoff 0x723c), impreso por
        // LOOKOBJ 0x007f; getstring en 0x0092 con máx 0xC (0x008e), buffer [bp-0xe].
        // ⚠ Este literal NO lleva `:`: el original abre la fila de lectura SIN cursor de
        // prompt, así que el eco va con prefijo vacío (poner ':' sería inventarlo).
        if (e.kind === "well-wish-prompt") {
          askText(WELL_UI.wish, "", 0xc, (wish) => applyEvents(game.makeWish(wish)));
        }
        // Fuente overworld/pueblo — mirar una fuente (0xD8-0xDB) imprime "a gurgling
        // fountain!" y pregunta "Who will drink?" con select_player (LOOKOBJ 0x0162):
        // el PJ elegido, según su estado, da "Incapacitated!" si muerto('D')/dormido('S')
        // o "Refreshing..." en otro caso. Cancelar (ESC) → "None!". Es PURO FLAVOR: el
        // handler dumpeado sólo imprime (NO cura; verificado en el asm), así que NO se
        // toca HP ni turno. El picker es el mismo roster fiel que Cast/Ready.
        if (e.kind === "fountain-drink-prompt") {
          hud.message("a gurgling fountain!"); // DS 0x729c
          pickMember(
            tf("Who will drink?"), // DS 0x72b4 (tf: prompt de pickMember, fuera del choke t())
            (mIdx) => {
              const s = game.state.characters[mIdx]?.status;
              // 'D' muerto / 'S' dormido → "Incapacitated!" (0x018f-0x019a); resto →
              // "Refreshing..." (0x01a0). Ramas explícitas para que la guarda de strings
              // las rastree (el extractor no resuelve el ternario dentro de hud.message).
              if (s === "D" || s === "S") hud.message("Incapacitated!"); // DS 0x72ce
              else hud.message("Refreshing..."); // DS 0x72e0
            },
            () => hud.message("None!"), // ESC → DS 0x72c6
          );
        }
        // BOLA DE CRISTAL (#144) — LOOKOBJ cmd_look 0x09ea: el caso 0x29 abre el
        // selector de PJ de comando (`call 0xffffa6f8` → kernel 0x4988, EL MISMO que
        // conduce el (S)earch) y sólo despues tira el dado (0x09f6). Por eso el picker
        // va aquí y no dentro del core: si devuelve -1 (nadie elegible o ESC) el binario
        // salta al epílogo en 0x09f0 SIN consumir tirada, que es exactamente lo que pasa
        // cuando `pickCommandChar` no invoca su callback.
        if (e.kind === "crystal-ball-prompt") {
          pickCommandChar((mIdx) => applyEvents(game.crystalBall(mIdx)));
        }
        // Interrogatorio de captura de Blackthorn (F1.7-T2). El resolve re-entra en
        // game.submitInterrogationResponse, que re-emite este evento para la siguiente
        // ronda (≤4) o cierra la escena.
        // Son DOS impresiones, y al port le faltaba la segunda: BLCKTHRN
        // 0x054a imprime la PREGUNTA (MISCMSG, llega en `e.text`) y `check_mantra` 0x02ea
        // imprime ADEMÁS "\n\nYour response?\n:" (DATA.OVL DS 0x6f7a, fileoff 0x6f8a) antes
        // del getstring de 0x0301 (máx 0xE en 0x02fd, buffer [bp-0x12]). El modal usaba la
        // pregunta como TÍTULO y la línea de respuesta no se pintaba en ningún sitio.
        if (e.kind === "blackthorn-interrogation-prompt") {
          // t() POR PIEZA (mismo patrón que el Yell de abajo): `e.text` ya llega
          // traducido (tf en la emisión) y el COMPUESTO pregunta+respuesta no es key
          // del corpus, así que el choke de pushConsole lo deja tal cual — sin este
          // t() la coletilla salía «Your response?» en inglés bajo lang=es (vídeo del
          // usuario, 24-08). En 'en' t() es identidad: byte-idéntico a antes.
          askText(`${e.text ?? ""}${t(BLACKTHORN_UI.response)}`, TALK_UI.cursor, 0xe, (m) =>
            applyEvents(game.submitInterrogationResponse(m)),
          );
        }
        // Password del guardia del Palacio (F1.7-T3, TALK 0x02a4). Texto libre
        // (patrón Words of Power); el resolve corre game.submitGuardPassword, que
        // con "IMPE" marca el pase y rompe el bucle de captura.
        // `e.text` (GUARD_PASSWORD_CHALLENGE) ya trae el reto ENTERO byte a byte, comillas
        // incluidas, hasta el "Your response?" de DS 0x9128 — TALK 0x02ae-0x02c3. Ese
        // literal termina en `\n` y NO en `\n:` (a diferencia del gemelo de BLCKTHRN), así
        // que la fila de lectura va sin cursor. getstring en 0x02d2, máx 0xE (0x02ce).
        if (e.kind === "blackthorn-guard-password-prompt") {
          askText(e.text ?? "Your response?", "", 0xe, (p) =>
            applyEvents(game.submitGuardPassword(p)),
          );
        }
        // (Y)ell fuera de fragata (CMDS 0x1418 → 0x1202): pide la palabra a gritar. En
        // la sala de una Llama, gritar FAULINEI/ASTAROTH/NOSFENTOR convoca al
        // Shadowlord (F1.10-T5). El original la lee INLINE por CONSOLA (getstring
        // 0x7b9c, máx 0xF chars), NO en un popup: el dispatcher ecoa "Yell " (DS 0xa286,
        // 0x3464) y el handler imprime "what?\n:" (DS 0x4529) ANTES de leer — dos filas:
        //   ►Yell what?      (eco del comando, con bullet)
        //   :VERAMOCOR▓      (fila del getstring, sin bullet, con cursor de la ola)
        // Testigo usuario (verdicts/yell-prompt) + death-resurrection-audit.md §S1.
        if (e.kind === "yell-word-prompt") {
          // Fila 1 — eco del comando. t() POR PIEZA ("Yell "→"Vocear ", "what?"→"¿qué?";
          // el compuesto no es key del corpus, mismo patrón que "Attack-Aim!"). Pasa por
          // pushConsole → t() (identidad para el compuesto ya traducido).
          hud.echo(t(CMD_STRINGS.yell) + t(YELL_UI.what)); // "Vocear ¿qué?" (con bullet)
          // Fila 2 — cursor del getstring ":" (sin bullet; `cont`). El input tecleado se
          // ecoa AQUÍ con echoSetLast (prefijo ":" ya "traducido" = identidad, estable —
          // no revierte al teclear, cf. doom-yell). El cursor de la ola cae al final
          // (awaitingGetstring, fijado en el keydown handler).
          hud.echoCursor(TALK_UI.cursor); // ":" → fila de eco viva del getstring
          prompts.current = {
            type: "text",
            prefix: TALK_UI.cursor,
            buffer: "",
            // 🔴 30, no 15. Decía `0xf` citando «getstring 0x7b9c: 15 chars», y el 0xF salía
            // de CMDS 0x121a/0x125a — que son los getstrings de `shrine_restore` (0x1202),
            // otra rutina. El del Yell es `1463: mov ax,0x1e` sobre [bp-0x20], leído en la
            // rama de tierra 0x1458. Truncaba la palabra del jugador a la mitad. Sin RNG:
            // el getstring es crudo y no toca el stream vivo (ficha #268).
            max: YELL_UI.max, // CMDS 0x1463 `mov ax,0x1e` → 30 chars
            resolve: (word) => applyEvents(game.yellWord(word)),
            // Vaciar+backspace / ESC = cancelar sin gritar (sin turno): el binario no
            // cobra si no se envía palabra. yellWord("") ya es no-op, pero evitamos
            // llamarlo para no emitir mensajes espurios.
          };
          refreshAwaiting();
        }
        // (K)limb sobre un tile no-escala: el original pide dirección (getdir
        // 0xB41C) para intentar encaramarse a una valla/roca. Reusa el flujo
        // direccional compartido (pendingDirCommand → game.klimb(dir)).
        if (e.kind === "needs-direction" && e.command === "klimb") {
          pendingDirCommand = "klimb";
          hud.echo(CMD_STRINGS.klimb);
        }
        if (e.kind === "map-changed") {
          cancelAutoWalk();
          updateMusic();
          // El autosave NUNCA debe romper el flujo (ni el arranque): un fallo de
          // cuota se loguea y el juego sigue. Ver hallazgo soak #35.
          const saved = autosave(game.state, mapName(), captureScreenshot(parent));
          if (!saved.ok) {
            console.warn(`Autosave omitido (${saved.reason}: almacenamiento lleno). El juego continúa.`);
          }
        }
      }
      // Turno atómico → eventos consumibles para las pieles (snap de cámara,
      // repintado…). El core ya avanzó; la piel sólo decide su presentación.
      view.notifyTurn(events);
      hud.refresh();
      // El turno pudo entrar/salir de combate o cerrar un modal: re-deriva el
      // gate del cursor de consola (F-G).
      refreshAwaiting();
    };

    // Hook de test (solo DEV): expone el bridge de eventos para que los E2E puedan
    // empujar por el pipeline REAL de presentación (p.ej. el endgame #20 L4, cuyo
    // trigger natural exige llegar a Doom-7). No altera runtime de producción.
    {
      const testHook = (window as unknown as Record<string, unknown>).__u5test as
        | Record<string, unknown>
        | undefined;
      if (testHook) testHook.applyEvents = (events: ReturnType<Game["move"]>) => applyEvents(events);
    }

    // Salida de combate: enruta mensajes al HUD y deriva los cues de sonido del
    // PC-speaker (task #3) de los eventos ya emitidos (golpe/daño/muerte), sin
    // tocar `combat.ts` (que porta el RNG de combate). El punto lógico del golpe
    // ES el evento `attacked{hit}`/`died` del binario. `combat-damage` cuando el
    // objetivo es un PJ de la party; si no, `combat-hit`; `died` → `combat-defeat`.
    type CombatEv = {
      kind: string;
      hit?: boolean;
      /** ★ #328: víctima arrastrada por un Corpser — arma la pausa 0x3AE6(4). */
      dragged?: boolean;
      text?: string;
      /** "message" MIXTO (#364-c): «A scroll: <runa>!» del (G)et en arena. */
      segments?: readonly { text: string; rune: boolean }[];
      actorId?: number;
      targetId?: number;
      x?: number;
      y?: number;
    };
    const routeCombatSfx = (events: readonly CombatEv[]): void => {
      for (const e of events) {
        const target = game.combat?.combatants.find((c) => c.id === e.targetId);
        const cue = sfxForCombatEvent({
          kind: e.kind,
          hit: e.hit,
          targetIsPlayer: target?.kind === "player",
          text: e.text,
        });
        if (!cue) continue;
        view.emitSfx(cue);
        // #212 — Cue cuyo emisor BLOQUEA el bucle del original: además de sonar, PARA el
        // juego mientras dura. Hoy el único que llega por aquí es `victory-fanfare`
        // (COMBAT.OVL:0x0d02, tras "\nVICTORY!\n"). La pertenencia se lee de `BLOCKING_CUES`
        // —la MISMA lista que usa el encadenado de audio de #206— y la duración del propio
        // catálogo, para que el sonido y la pausa no puedan medir cosas distintas.
        if (BLOCKING_CUES.has(cue.id)) combatPacer.armBlockingPause(cueDurationMs(cue));
      }
    };
    // Efectos EFÍMEROS de combate (E1-S12, spec §2/§3): derivados de los MISMOS
    // eventos ya emitidos, sin tocar combat.ts (que porta el RNG). Proyectil cuando
    // el golpe es a distancia (atacante y objetivo NO adyacentes); flash de impacto
    // en cada golpe/ muerte que aterriza. Cadencias/glifos = Clase C (#26).
    const routeCombatFx = (events: readonly CombatEv[]): void => {
      const combat = game.combat;
      if (!combat) return;
      for (const e of events) {
        // Terremoto de hechizo (In Vas Por Ylem): sacudida de la ventana + rumble,
        // igual que el quake del overworld (#29/#36). No lleva objetivo.
        if (e.kind === "quake") {
          view.emitCombatFx({ kind: "quake" });
          view.emitSfx({ id: "quake" });
          continue;
        }
        // ABANICO de hechizo de línea (In Zu/In Nox Hur/In Flam Hur/In Vas Grav Corp):
        // el core señala caster+dirección+modo; aquí se TRAZAN los 21 rayos (calco de
        // CAST.OVL 0x1c36, corte LOS por la tabla 0x6a14 sobre la rejilla VIVA de la
        // arena) y la piel los anima. + el cue del crackle (0x223c/0x22e2).
        if (e.kind === "lineSpray" && e.x != null && e.y != null) {
          const actor = combat.combatants.find((c) => c.id === e.actorId);
          if (actor) {
            const tiles = combat.mapTiles;
            const rays = traceSprayRays(
              { x: actor.x, y: actor.y },
              { x: e.x, y: e.y },
              (cx, cy) => blocksSpellLine(tiles[cy]?.[cx] ?? -1),
            );
            const mode = (e as { mode?: number }).mode ?? 4;
            view.emitCombatFx({ kind: "lineSpray", rays, color: sprayColorForMode(mode) });
            view.emitSfx({ id: "line-spray", n: mode });
          }
          continue;
        }
        // Tiro a distancia DESPERDICIADO (muro/celda vacía): anima el vuelo del
        // proyectil hasta el aterrizaje, sin objetivo ni daño (COMSUBS:0x12DE).
        if (e.kind === "projectile" && e.x != null && e.y != null) {
          const actor = combat.combatants.find((c) => c.id === e.actorId);
          if (actor) {
            view.emitCombatFx({
              kind: "projectile",
              from: { x: actor.x, y: actor.y },
              to: { x: e.x, y: e.y },
              hit: false,
            });
          }
          continue;
        }
        if (e.targetId == null) continue;
        const target = combat.combatants.find((c) => c.id === e.targetId);
        if (!target) continue;
        const kind = target.kind === "player" ? "party" : "enemy";
        if (e.kind === "attacked") {
          const actor = combat.combatants.find((c) => c.id === e.actorId);
          if (actor) {
            const cells = Math.max(
              Math.abs(actor.x - target.x),
              Math.abs(actor.y - target.y),
            );
            // A distancia el misil vuela acierte o falle (fallo = fuego amigo, §2).
            if (cells > 1) {
              view.emitCombatFx({
                kind: "projectile",
                from: { x: actor.x, y: actor.y },
                to: { x: target.x, y: target.y },
                hit: Boolean(e.hit),
              });
            }
          }
          if (e.hit) {
            view.emitCombatFx({ kind: "hitFlash", x: target.x, y: target.y, targetKind: kind });
          }
          // ★ #328 — ARRASTRE del Corpser: tras escribir render-tile 0 (la víctima ya
          // desapareció en este mismo snapshot), el binario corre 0x3AE6(4) — COMSUBS
          // 0x03F2 `push 4` / 0x03F6 `call 0x5906` → kernel 0x3AE6 (base near-call
          // COMSUBS 0xE1E0, resuelto con dispatch_table). 4 fotogramas de pausa MUDA
          // que separan la desaparición del resto de la tanda. La unidad ms es la
          // MISMA calibración compartida de las pausas 0x3ae6 (PAUSE_UNIT_MS = 55,
          // skin/world-fx.ts; Clase C: el ASM fija FOTOGRAMAS, no ms). SIN vaciar la
          // cola de teclas: 0x3AE6 no toca el búfer BIOS (eso es el 0x1b16 de la
          // fanfarria) — por eso flushKeys=false.
          if (e.dragged) combatPacer.armBlockingPause(4 * PAUSE_UNIT_MS, false);
        } else if (e.kind === "died") {
          view.emitCombatFx({ kind: "hitFlash", x: target.x, y: target.y, targetKind: kind });
        }
      }
    };
    const combatOut = (events: readonly CombatEv[]): void => {
      for (const e of events) {
        if (e.kind === "message" && e.text) {
          // #364-c: fila mixta (scroll del botín de arena) → tramos {text,rune}.
          if (e.segments) hud.messageSegments(e.segments);
          else hud.message(e.text);
        }
        // Resultados del golpe (careo-combate T1): los eventos `attacked`/`died` LLEVAN
        // el texto fiel del original ("Orc missed!"/"Orc grazed!"/"javier hit!"/"Orc
        // killed!" — COMSUBS 0x0312/0x00D2, DS 0x99aa/0x99f2/0x9a22/0x99fc) y antes se
        // DESCARTABAN (solo se imprimía kind "message"): el log de combate quedaba mudo.
        else if ((e.kind === "attacked" || e.kind === "died") && e.text) hud.message(e.text);
        // Eco de comando del combate (fila ► del prompt): ESC "Escape…" (CMDS 0x17ec).
        else if (e.kind === "echo" && e.text) hud.echo(t(e.text));
        // "turn" (advanceTurn) = nuevo turno: si toca a un PJ, su banner "armed with".
        else if (e.kind === "turn" && e.actorId !== undefined)
          announceCombatTurn(game.combat?.byId(e.actorId));
      }
      routeCombatSfx(events);
      routeCombatFx(events);
      // El latch de «VICTORY!» CAMBIA LA MÚSICA SIN SALIR DE LA ARENA, y por eso el
      // refresco va aquí y no en el teardown: el driver sigue en la rama del centinela
      // 0xFF y lo único que cambia bajo sus pies es `g_cmb_victory_flag` (0x58A3) — el
      // combate no termina con la victoria (la party puede seguir dentro, recogiendo). En
      // el original esto lo destapa el propio sondeo de teclado; aquí, cada tanda de
      // eventos. De-duplicado por canción ⇒ barato e idempotente, como `notifyDirty`.
      updateMusic();
      // Repinta las pieles tras la acción: un `moved` puro no empuja línea de
      // consola, así que sin esto la arena no se refrescaría hasta el siguiente
      // tick del reloj F-A. notifyDirty es barato e idempotente.
      view.notifyDirty();
    };


    /** Input en modo combate. */
    const COMBAT_DIRS: Record<string, { dx: number; dy: number; dir8: string }> = {
      ArrowUp: { dx: 0, dy: -1, dir8: "north" },
      ArrowDown: { dx: 0, dy: 1, dir8: "south" },
      ArrowLeft: { dx: -1, dy: 0, dir8: "west" },
      ArrowRight: { dx: 1, dy: 0, dir8: "east" },
    };
    // Cursor de Aim móvil (COMSUBS:0x0504, spec §7): mientras `combatAim` no es
    // null, las flechas MUEVEN la cruz por la arena (acotada a alcance + rejilla),
    // Enter/Space/A CONFIRMAN el disparo sobre la celda del cursor y Esc CANCELA.
    let combatAim: {
      cursor: { x: number; y: number };
      actor: { x: number; y: number };
      range: number;
    } | null = null;
    // Hechizo APUNTADO en curso (combatAttack/lineAoe): mientras no es null, el
    // cursor de Aim confirma con `playerCast(fx, celda)` en vez de `playerAttack`.
    // Lo arma el comando (C)ast del combate tras teclear el hechizo; ESC lo cancela.
    let pendingCombatCast: CastEffect | null = null;
    // (G)et/(O)pen/(J)immy/(S)earch/(P)ush de la arena están en getdir: tras la tecla, el
    // bucle espera una DIRECCIÓN (SJOG Get 0x18ce @0x18ea / Open 0x1374 @0x139f / Jimmy
    // 0x0d4a @0x0d78 / Search 0x095c @0x097e / Push CMDS 0x161a @0x1632 llaman a getdir
    // 0x766c y apuntan a actor_activo+dir). Mientras no es null, la flecha resuelve el
    // comando sobre la celda vecina.
    let pendingCombatDir: "get" | "open" | "jimmy" | "search" | "push" | null = null;
    const handleCombatKey = (ev: KeyboardEvent): void => {
      const combat = game.combat!;
      const key = ev.key;
      // ── FAMILIA Ctrl DEL BUCLE DE COMBATE — va lo PRIMERO, antes del
      // `preventDefault()`. El guard de Ctrl aterrizó en los otros dos bucles
      // (handleGameKey / handleDungeonKey) y este se quedó fuera: como aquí se
      // llamaba a `preventDefault()` incondicionalmente y las ramas de letra miran
      // `key.toLowerCase()`, en combate **doce** comandos quedaban secuestrados por
      // su combo — Ctrl+A (A)ttack, Ctrl+C (C)ast, Ctrl+R (R)eady (¡y sin recargar
      // la página!), Ctrl+U, Ctrl+K, Ctrl+Q, Ctrl+G, Ctrl+O, Ctrl+J, Ctrl+S,
      // Ctrl+P, Ctrl+Y —, varios de ellos consumiendo turno. Es exactamente el
      // hijack de Ctrl+K→Klimb que ya se arregló arriba, en el tercer bucle.
      //
      // De la familia, COMBAT.OVL sólo tiene Ctrl-S (0x0881→0x08b6). El resto de
      // códigos <0x20 caen en su default 0x0ab7 = "What?\n" (DS 0x6ee6); aquí NO se
      // emite ese "What?" y se cae al mismo silencio que los otros dos bucles, por
      // la razón ya escrita en ellos: un Ctrl+letra suelto es un atajo del NAVEGADOR
      // y robárselo para contestar «What?» cuesta más de lo que da. Divergencia
      // DECLARADA con su cita, no un hueco.
      //
      // ⚠ Y va antes del `combatPacer` a propósito: la cola del pacer (el búfer BIOS
      // del original) guarda `string`, no el evento, así que no puede llevar el
      // modificador — un Ctrl+S encolado se drenaría como una (S)earch. Lo único que
      // se mueve respecto al original es CUÁNDO sale la fila "Sound …" (antes de la
      // tanda enemiga en vez de después), no qué hace.
      if (ev.ctrlKey || ev.metaKey || ev.altKey) {
        if (handleCtrlKey(ev, "combat")) return;
        keyRec.drop(); // no es comando: el atajo es del navegador
        return;
      }
      ev.preventDefault();
      // Tanda enemiga paceada en vuelo (T11): la tecla se ENCOLA (buffer BIOS del
      // original) y se drena al terminar la tanda — no se procesa ni se pierde.
      if (combatPacer.pacing) {
        combatPacer.enqueue(key);
        return;
      }
      const cur = combat.currentUnit;
      // Un PJ POSEÍDO por la Sword of Chaos (charmed) NO recibe input: lo conduce la IA
      // (como un enemigo). Espejo de COMBAT.OVL 0x06b7 (g_active_char=0xff → call 0x3f4).
      if (!cur || cur.kind !== "player" || cur.charmed) {
        if (combatAim) {
          combatAim = null;
          view.setCombatAim(false);
        }
        pumpCombat();
        return;
      }
      // getdir del (G)et/(O)pen de arena: la flecha fija la dirección → resuelve el cofre en
      // la celda VECINA (actor_activo+dir). ESC cancela sin gastar turno (getdir 0x766c=0 →
      // SJOG salta al exit, 0x18f1/0x13a6). Va ANTES del combatAim/flee: son excluyentes.
      if (pendingCombatDir) {
        const cmd = pendingCombatDir;
        const d = COMBAT_DIRS[key];
        if (d) {
          pendingCombatDir = null;
          hud.echoAppend(t(DIR_WORDS[d.dir8 as "north" | "south" | "east" | "west"]));
          const dir8 = d.dir8 as never;
          combatOut(
            cmd === "get"
              ? combat.playerGet(dir8)
              : cmd === "open"
                ? combat.playerOpen(dir8)
                : cmd === "jimmy"
                  ? combat.playerJimmy(dir8)
                  : cmd === "search"
                    ? combat.playerSearch(dir8)
                    : combat.playerPush(dir8),
          );
          pumpCombat();
          return;
        }
        if (key === "Escape" || key === " ") {
          if (cmd === "get" || cmd === "open") {
            // Comportamiento ATERRIZADO del carril chest-reveal: ESC silencioso sin turno,
            // Space ignorado. ⚠ Diverge del kernel getdir (abajo) — declarado en
            // re/notes/combat-commands.md, no se toca aquí (movería digests del tour).
            if (key === "Escape") pendingCombatDir = null;
            return;
          }
          // J/S/P — getdir FIEL (kernel 0x35EC @0x363b-0x364e): ESC (0x1b) o Space (0x20)
          // ecoan "Pass\n" (DS 0xa2a0) sobre la fila del comando y devuelven 0; el funnel
          // descarta el retorno y la ACCIÓN SE CONSUME ([bp-4]=1 @0x083e).
          pendingCombatDir = null;
          hud.echoAppend(t("Pass"));
          combatOut(combat.playerDirCancel());
          pumpCombat();
          return;
        }
        return; // getdir sólo acepta direcciones
      }
      if (combatAim) {
        const d = COMBAT_DIRS[key];
        if (d) {
          // Flecha: mueve la cruz una celda si sigue en alcance (dist ≤ range,
          // COMSUBS 0x05f8) y dentro de la rejilla 0..10 (0x0600). NO dispara.
          const nx = combatAim.cursor.x + d.dx;
          const ny = combatAim.cursor.y + d.dy;
          const inGrid = nx >= 0 && ny >= 0 && nx < 11 && ny < 11;
          const inRange =
            combatDistance(nx - combatAim.actor.x, ny - combatAim.actor.y) <=
            combatAim.range;
          if (inGrid && inRange) {
            combatAim.cursor = { x: nx, y: ny };
            view.setCombatAim(true, combatAim.cursor);
          }
          return;
        }
        // CADENA DE ARMAS (COMSUBS 0x0D96→0x0D3C): con 2-3 ítems que atacan, el
        // binario abre el Aim del SIGUIENTE arma automáticamente tras resolver o
        // cancelar el anterior (el turno no vuelve al prompt hasta agotar la cola).
        // El port re-abre el cursor si tras la acción el turno SIGUE siendo del
        // mismo actor (cola pendiente). SOLO en vivo (beat>0): bajo webdriver los
        // drivers cuentan con re-abrir con 'a' (misma gating que el beat T11 —
        // cambiarles la semántica de teclas desplazaría los digests del tour).
        // `ev` = eventos de la acción: si NO trae "turn"/"end", la acción devolvió
        // SIN advanceTurn = quedan armas en la cola (playerAttack/Cancel solo
        // retornan temprano en ese caso) → se encadena el Aim del siguiente arma.
        const chainNextWeaponAim = (prevId: number, ev: ReturnType<typeof combat.playerAttackCancel>): void => {
          if (ENEMY_BEAT_MS <= 0) return;
          if (ev.some((e) => e.kind === "turn" || e.kind === "ended")) return;
          const cb = game.combat;
          const cur2 = cb?.currentUnit;
          if (!cb || !cur2 || cur2.id !== prevId || cur2.kind !== "player" || cur2.charmed) return;
          const geo = cb.aimGeometry();
          if (!geo) return;
          combatAim = { cursor: { ...geo.initial }, actor: geo.actor, range: geo.range };
          view.setCombatAim(true, combatAim.cursor);
          hud.echo(t(CMD_STRINGS.attack) + t(COMBAT_STRINGS.aim)); // "Attack-Aim! " del arma siguiente
        };
        const onSelf =
          combatAim.cursor.x === combatAim.actor.x &&
          combatAim.cursor.y === combatAim.actor.y;
        if (key === "Enter" || key.toLowerCase() === "a" || key === " ") {
          // Confirm = Enter / 'A' / Space (COMSUBS 0x0504 @0x0640-0x0664/0x06f2).
          // Sobre la PROPIA celda del actor: Enter/'A' se IGNORAN (el binario
          // @0x06a3-0x06b5 vuelve al bucle — no puedes dispararte); Space @0x068a
          // CANCELA (mismo camino que ESC: [bp-0xc]=1).
          // EXCEPCIÓN An Grav: su apuntador es OTRO (CAST2:0x306, no 0x0504) y ahí
          // Space CONFIRMA en cualquier celda, la propia incluida (0x362-0x365
          // `cmp ax,0x20 / je 0x3a8` acepta sin veto de self) — un campo E8/E9/EA es
          // transparente y el lanzador puede estar ENCIMA del que quiere disipar.
          const selfVeto = onSelf && pendingCombatCast?.kind !== "dispelField";
          if (selfVeto && key !== " ") return;
          if (selfVeto && key === " ") {
            combatAim = null;
            view.setCombatAim(false);
            if (pendingCombatCast) {
              pendingCombatCast = null; // cast apuntado: se pierde sin efecto
              return;
            }
            const prevId = combat.currentUnit?.id ?? -1;
            const evCancel = combat.playerAttackCancel(); // consume el golpe (0x0c52)
            combatOut(evCancel);
            pumpCombat();
            chainNextWeaponAim(prevId, evCancel);
            return;
          }
          // Confirma sobre la celda del cursor. Si hay un hechizo APUNTADO en curso
          // (combatAttack/lineAoe/dispelField), lo resuelve por `playerCast`; si no,
          // es un ataque normal (mismo camino de RNG/orden, paridad intacta).
          const { x, y } = combatAim.cursor;
          combatAim = null;
          view.setCombatAim(false);
          if (pendingCombatCast) {
            const fx = pendingCombatCast;
            pendingCombatCast = null;
            combatOut(combat.playerCast(fx, { x, y }));
            pumpCombat();
          } else {
            const prevId = combat.currentUnit?.id ?? -1;
            const evAtk = combat.playerAttack(x, y);
            combatOut(evAtk);
            pumpCombat();
            chainNextWeaponAim(prevId, evAtk);
          }
          return;
        }
        if (key === "Escape") {
          // ESC del Aim de ATAQUE (hotfix #4, DERIVADO): el binario CONSUME el
          // golpe — 0x0504 @0x06ea ret 0 → melé imprime "Nothing!" (0x0C52
          // @0x0cfa) / ranged sale en silencio (0x0A68 @0x0ab8) y el turno se
          // gasta al agotar la cola (dispatch 'A' deja [bp-4]=1). El «ESC sin
          // gastar turno» previo citaba 0x06ea sin seguir el retorno. Para un
          // CAST apuntado se mantiene el cierre sin turno (flujo CAST, fuera del
          // alcance del hotfix; el maná ya se perdió en el dispatcher).
          combatAim = null;
          view.setCombatAim(false);
          if (pendingCombatCast) {
            pendingCombatCast = null;
            return;
          }
          const prevId = combat.currentUnit?.id ?? -1;
          const evEsc = combat.playerAttackCancel();
          combatOut(evEsc);
          pumpCombat();
          chainNextWeaponAim(prevId, evEsc);
          return;
        }
        return; // en modo aim, ignora el resto de teclas
      }
      // (Esc) fuera del modo aim (COMBAT.OVL 0x0864→0x09dc, veredicto esc-flee-verdict.md):
      // con enemigos vivos NO hace NADA (la tecla llega al juego y el juego la ignora, sin
      // gastar turno — la huida a media pelea es CAMINAR fuera del borde); tras la victoria
      // retira a TODO el party de golpe y cierra la escena. La lógica vive en playerEscapeQuick.
      if (key === "Escape") {
        combatOut(combat.playerEscapeQuick());
        pumpCombat();
        return;
      }
      if (key.toLowerCase() === "a") {
        // Arranca el cursor sobre el enemigo más cercano en alcance (0x0539-0x0568).
        const geo = combat.aimGeometry();
        if (!geo) return;
        combatAim = {
          cursor: { ...geo.initial },
          actor: geo.actor,
          range: geo.range,
        };
        view.setCombatAim(true, combatAim.cursor);
        // Eco byte-exacto del comando: "Attack-" + "Aim! " (DATA.OVL); la consola
        // antepone el bullet ►/> → ">Attack-Aim!" del vídeo-O (f020/f045/f050).
        // t() por PIEZA: el choke t() del pushConsole vería el COMPUESTO "Attack-Aim! "
        // (no es key) y caería a inglés bajo 'es'; cada mitad SÍ es key (approved) → se
        // traduce aquí y el t() posterior deja el resultado intacto (doble pasada segura).
        hud.echo(t(CMD_STRINGS.attack) + t(COMBAT_STRINGS.aim));
        return;
      }
      // (C)ast en COMBATE (CAST.OVL): el PJ activo lanza un hechizo como su acción del
      // turno. Nombre TECLEADO por iniciales rúnicas (mismo getstring que el overworld);
      // `castSpell` corre el dispatcher sobre el RNG DEL COMBATE (`combat.rng`, g_rng_seed
      // compartido) y `playerCast` aplica el efecto sobre la arena. Los hechizos APUNTADOS
      // (combatAttack/lineAoe) entran en el cursor de Aim con el efecto pendiente.
      if (key.toLowerCase() === "c") {
        const casterIdx = cur.charIdx;
        const casterRec = casterIdx !== undefined ? game.state.characters[casterIdx] : undefined;
        if (!casterRec) return;
        // Eco del comando en la ARENA: lo imprime el overlay de combate con su copia PROPIA
        // de la cadena — `COMBAT.OVL 0x08f0: b8f66d mov ax, 0x6df6` → DS 0x6df6 = "Cast...\n"
        // (única referencia a ese offset en todo el corpus). NO es la DS 0xa142 del
        // dispatcher del kernel, que es la de overworld/pueblo/mazmorra (call-sites 2770 y
        // 2853): mismos bytes, tabla distinta. Cita que faltaba (hueco cedido por movil-cmds).
        hud.echo(CMD_STRINGS.cast);
        // Gate FIEL (COMBAT.OVL 0x08F0, ANTES del getstring rúnico): negate-magic (In An)
        // o combate iniciado en el Palacio de Blackthorn sin la Corona de LB puesta →
        // "Absorbed!\n" y turno consumido, sin pedir el nombre del hechizo. Por eso en el
        // binario teclear c,i,v,p,y no casteaba: la 'c' consumía el turno y las runas caían
        // como comandos sueltos. Task #65 (ver `combatCastAbsorbed`).
        if (combatCastAbsorbed(game.state.timeSpell, game.state.position.location, !!game.state.wornCrown)) {
          const cb0 = game.combat;
          if (!cb0) return;
          hud.message(COMBAT_ABSORBED_MESSAGE);
          combatOut(cb0.playerCast(null, null)); // consume el turno del lanzador
          pumpCombat();
          return;
        }
        pickSpellForCast(tf("Spell name: "), (initials) => {
          const cb = game.combat;
          if (!cb) return;
          if (initials === "") { hud.message("None!"); return; } // ESC/vacío: no gasta turno
          const idx = matchSpellByInitials(spellDefs, initials);
          if (idx < 0) { hud.message("No effect!"); return; } // nombre inválido: no gasta turno
          const def = spellDefs[idx]!;
          const r = castSpell(
            game.state,
            casterRec,
            def,
            { location: game.state.position.location, inCombat: true },
            cb.rng,
          );
          if (r.message) hud.message(r.message); // fallos con línea propia (Not here!/None mixed!/M.P. too low!)
          // TAIL común del Cast (CAST.OVL 0x11a6, result=0 → "Failed!" DS 0x4660): los gates de
          // maná/nivel consumen (r.consumed) y caen al tail — el de maná tras su "M.P. too low!".
          if (!r.ok && r.consumed) hud.message("Failed!");
          // CEREMONIA del (C)ast en ARENA — la MISMA `CAST2:0x0000` que el pergamino de
          // tiempo, con el CÍRCULO por índice (CAST.OVL 0x0e0a-0x0e14). Faltaba entera: el
          // careo denso mide separación 0,1 (SIN BIMODALIDAD) al castear, contra 182,5 del
          // An Tym en la misma sesión.
          if (r.ok) emitCastCeremony(def.index);
          const fxRaw = r.effect;
          // #91 P3 — en COMBATE los cuatro In*Grav son un ATAQUE con arma-hechizo, no un
          // camino aparte. `cast_field_wall` rama `g_location >= 0x80` (CAST.OVL
          // 0x00ec-0x0100) hace `g_cmb_weapon = [bx+0x4592]` · `push g_cmb_actor` ·
          // `push g_cmb_weapon` · `call 0xffffc14a`, que es EL MISMO destino y los MISMOS
          // dos argumentos que `cmb_set_weapon_then_attack` (0x0032-0x0044), el que ya
          // produce `combatAttack` para Grav Por / Vas Flam / Xen Corp. Por eso aquí se
          // traduce a `combatAttack` con su id de arma en vez de abrir una vía nueva.
          // Y NO siembra campo: ver la cadena cerrada en `SPELL_WEAPON_STATS`.
          const fx = combatCastEffect(fxRaw);
          // `illusion` (In Quas Xen) entra aquí porque el binario SÍ abre cursor: emite el
          // prompt "Creature: " y llama al apuntador COMSUBS:0x0504 (CAST.OVL 0x0b34-0x0b41,
          // ficha #340). Su ESC devuelve 0 = hechizo sin efecto.
          if (fx && (fx.kind === "combatAttack" || fx.kind === "lineAoe" || fx.kind === "illusion")) {
            // Apuntado: arranca el cursor de Aim; confirmar dispara playerCast(fx, celda).
            const geo = cb.aimGeometry();
            if (geo) {
              combatAim = { cursor: { ...geo.initial }, actor: geo.actor, range: geo.range };
              pendingCombatCast = fx;
              view.setCombatAim(true, combatAim.cursor);
              return;
            }
          }
          if (fx && fx.kind === "dispelField") {
            // An Grav en combate (CAST2:0x07bc rama g_location>=0x80): el apuntador
            // 0x306 arranca el cursor EN LA CELDA DEL LANZADOR y lo mueve tecla a
            // tecla SIN tope de alcance (no clampa contra un range) hasta confirmar.
            // Se reusa el cursor de Aim con range de tablero (10 = diagonal máxima
            // 11×11) e initial = la celda propia — no el last-target del aimGeometry
            // de armas. Confirmar dispara playerCast(dispelField, celda) →
            // castDispelField. ESC lo pierde sin efecto, como el resto de apuntados.
            const geo = cb.aimGeometry();
            if (geo) {
              // range 20 > √200 ≈ 14,1 = diagonal máxima del 11×11: sin clamp efectivo.
              combatAim = { cursor: { ...geo.actor }, actor: geo.actor, range: 20 };
              pendingCombatCast = fx;
              view.setCombatAim(true, combatAim.cursor);
              return;
            }
          }
          // Efectos de OBJETIVO-PJ en combate (lote 6): heal/cure/awaken/resurrect.
          // Picker FIEL (select_party_member, como el doCast de overworld) → aplica
          // con el RNG DEL COMBATE (cb.rng, Mani consume rand30 del stream compartido)
          // → sincroniza el HP del combatiente en la arena → CONSUME el turno. El
          // dispatcher (castSpell) no tira para estos efectos; sólo Mani tira.
          if (fx && (fx.kind === "healTarget" || fx.kind === "cure" || fx.kind === "awaken" || fx.kind === "resurrect")) {
            // Target-select fiel: «On who: » + nombre (CAST2 0x9e; el «sin prompt»
            // del lote cast-echo era falso negativo — ver pickCastTarget).
            pickCastTarget((mIdx) => {
              const m = game.state.characters[mIdx]!;
              let ok = false;
              if (fx.kind === "healTarget") ok = fx.mode === "full" ? applyVasMani(m) : applyMani(m, cb.rng) > 0;
              else if (fx.kind === "cure") ok = applyCure(m);
              else if (fx.kind === "awaken") ok = applyAwaken(m);
              else ok = applyResurrect(m, game.state.karma);
              // Resultado GENÉRICO del tail del Cast (CAST.OVL 0x11a6 → print 0x58d0):
              // result==1 → "Success!" (DS 0x4656); result==0 → "Failed!" (DS 0x4660). El
              // resurrect por CAST NO imprime "Resurrection!" — esa cadena (DS 0x46d2) es del
              // lector de pergaminos/anillos (0x11de), jamás alcanzable desde Cast; In Mani
              // Corp cae al MISMO tail → "Success!"/"Failed!". El prose con nombre era fabricado.
              hud.message(ok ? "Success!" : "Failed!");
              // Sincroniza el combatiente de la arena con el record (el motor lee
              // Combatant.hp/status): heal sube HP; resurrect revive al caído.
              const cmb = cb.combatants.find((c) => c.kind === "player" && c.charIdx === mIdx);
              if (cmb) {
                cmb.hp = m.currentHp;
                if (m.status !== "D" && cmb.status === "dead") cmb.status = "active";
                if (m.status === "S") cmb.sleeping = true;
                else if (cmb.sleeping && m.status === "G") cmb.sleeping = false;
              }
              combatOut(cb.playerCast(null, null)); // consume el turno del caster
              pumpCombat();
            });
            return;
          }
          // No apuntado (terremoto/global/estado) o gate fallado (fx null): aplica y
          // CONSUME el turno del caster.
          combatOut(cb.playerCast(fx ?? null, null));
          pumpCombat();
        });
        return;
      }
      if (key.toLowerCase() === "u") {
        // (U)se en la arena (COMBAT.OVL 0x0b20 → "Use item" DS 0x6e42 → combat_cmd 0x544
        // code 5). Gate bit 0x80 = miembro de party: el port ya exigió cur.kind==="player"
        // arriba. La poción la bebe el PJ activo del turno. Ver re/notes/combat-use-potions.md.
        if (cur.charIdx === undefined) return;
        hud.echo("Use item"); // "Use item\n\n" (DS 0x6e42)
        openCombatUsePicker(cur.charIdx);
        return;
      }
      if (key.toLowerCase() === "r") {
        // (R)eady en la arena (COMBAT.OVL 0x09a2 → combat_cmd 0x544 code 3 → picker de
        // equipo ZSTATS). Actúa sobre el PJ ACTIVO del turno (como (U)se); abre el mismo
        // picker de pergamino que el overworld pero con el bloqueo de armadura y consumo
        // de turno propios del combate (openCombatReadyPicker). El eco lo imprime el propio
        // overlay, NO el dispatcher del kernel, y con su copia PROPIA de la cadena:
        // `COMBAT.OVL 0x09a2: b82e6e mov ax, 0x6e2e` → DS 0x6e2e = "Ready...\n\n", mismos
        // bytes que la DS 0xa1f0 del dispatcher (que es la del overworld/pueblo/mazmorra,
        // ULTIMA.EXE 0x339a). La cita a 0xa1f0 que había aquí atribuía al offset equivocado.
        if (cur.charIdx === undefined) return;
        hud.echo(CMD_STRINGS.ready);
        openCombatReadyPicker(cur.charIdx);
        return;
      }
      if (key === " ") {
        combatOut(combat.playerPass());
        pumpCombat();
        return;
      }
      // Klimb de COMBATE (E3c): huida por escalera de una sala de mazmorra. Vive en
      // el handler de combate (aislado del Klimb de mazmorra de first-person y del
      // cancel de dirección del Aim, que ya retornó arriba). Sobre un tile de
      // escalera → "Klimb-Up!/Down!" + "Escape!"; si no, "Klimb-what?". video-N f065.
      if (key.toLowerCase() === "k") {
        combatOut(combat.playerKlimbEscape());
        pumpCombat();
        return;
      }
      // (N)úmero 0-9: SET ACTIVE PLAYER en combate (COMBAT:0x063E @0x09ec/0x09fe →
      // SJOG 0x1F7A). Semántica DERIVADA del binario (hotfix #3 — invierte la
      // derivación previa): '0' o selección VÁLIDA ⇒ el actor en curso CEDE el
      // turno sin acción (tail @0x0b79-0x0b83, sin housekeeping SJOG 0x2012) y, con
      // g_active_char fijado, los demás miembros auto-pasan cada ronda
      // (`skipsForActiveChar`); el elegido lleva la flecha `→` del roster (#69).
      // Selección INVÁLIDA ⇒ "Invalid!" y RE-PROMPT del MISMO actor con su banner
      // re-impreso (salto @0x0a41→0x06f1), sin gastar turno. '7'-'9' ⇒ "What?\n"
      // sin turno ni banner (default @0x0ab7). Ecos: "Set active plr:\n" (DS
      // 0x8f3a/0x6e66) en la fila del prompt + resultado en línea propia.
      if (key.length === 1 && key >= "0" && key <= "9") {
        const r = game.combatActivePlayer(Number(key));
        if (r.echo === null) {
          hud.message("What?"); // DS 0x6ee6, re-prompt sin banner ni turno
          return;
        }
        // trimEnd: el "\n" de la cadena DS baja de fila en el DOS; en la consola del
        // port una fila la abre el propio push — el \n literal crearía una fila
        // vacía espuria entre "Set active plr:" y el resultado (no está en el log
        // original: SJOG @0x1f87 imprime "Set active plr:\n" y el nombre sigue
        // INMEDIATAMENTE debajo).
        hud.echo(COMBAT_STRINGS.setActive.trimEnd());
        hud.message(r.echo); // nombre / "None!" / "Invalid!"
        if (r.yieldTurn) {
          combatOut(combat.playerYieldTurn());
          pumpCombat();
        } else if (r.reprompt) {
          announceCombatTurn(combat.currentUnit); // @0x0a41→0x06f1: banner otra vez
          hud.refresh();
        }
        return;
      }
      // (G)et / (O)pen del botín EN LA ARENA: DIRECCIONALES (getdir), como en el overworld —
      // resuelto por binario (Get→SJOG:0x18ce, Open→SJOG:0x1374, ambos `call 0x766c` getdir y
      // apuntan a g_party+dir; COMBAT.OVL 0x063e copia la celda del actor activo a g_party).
      // El cofre (tile 1, COMBAT:0x1574 172c) es IMPASABLE: NO se pisa, se abre desde AL LADO.
      // Eco "Get-"/"Open-" y, tras la flecha, la palabra de dirección (pendingCombatDir arriba).
      // re/notes/combat-commands.md. La victoria NO cierra el combate: la party saquea antes de salir.
      // OJO CON LA CITA: en COMBATE el nombre del comando NO lo imprime el despachador
      // del kernel (0x3178) — la arena no pasa por él — sino COMBAT.OVL con su PROPIA
      // copia de la tabla de nombres. Los bytes son los mismos, el offset NO:
      //   combate  DS 0x6e14 "Get-"  · DS 0x6e22 "Open-"   (bloque propio de COMBAT.OVL,
      //            contiguo: 0x6df6 "Cast...\n" · 0x6e14 "Get-" · 0x6e1a "Jimmy-" ·
      //            0x6e22 "Open-" · 0x6e28 "Push-" · 0x6e2e "Ready...\n\n" · 0x6e3a
      //            "Search-" · 0x6e42 "Use item\n\n" · 0x6e60 "Pass\n"…)
      //   kernel   DS 0xa16a "Get-"  · DS 0xa1ce "Open-"   (despachador, overworld/pueblo)
      // Verificado byte a byte contra DATA.OVL (`grep -aob` da DOS ocurrencias de cada
      // una). Citaban el offset del despachador estando dentro de handleCombatKey; el
      // hermano de este defecto lo cazó ready-flow en el eco de Ready (2152).
      // (Q)uit & Save EN LA ARENA — RECHAZO FIEL (ficha #154 F2, «igual que original»).
      // El binario NO guarda aquí y NO calla: imprime «Quit-Not here» y re-prompta al
      // mismo combatiente sin gastar turno (derivación completa y citas en
      // COMBAT_STRINGS.quitReject). Antes esta tecla se caía por el final del despachador
      // SIN ECO NINGUNO — un no-op mudo, que es la única de las tres respuestas posibles
      // que el original no da.
      //
      // LOS DOS TONOS DEL FUNNEL — PORTADOS (#161, cue `combat-reject`): el funnel remata
      // con `SJOG.OVL:0x1f52` beep(0xdc,0x96) + `0x1f5d` beep(0x96,0x96) = 220 Hz y
      // 150 Hz contiguos (dur 0x96 ambos). ⚠ Aquí decía «beep(0x96,0xdc) + beep(0x96,
      // 0x96) — dos pitidos de 150 Hz medidos»: la pareja del primero estaba INVERTIDA.
      // Re-derivado en #161 con la convención adjudicada por call-site acreditado (kernel
      // 0x4299 push 0xbb8,3 = beep(3000,3) = tic del reloj): el PRIMER push es la freq
      // ⇒ el primer beep es de 0xdc = 220 Hz, no de 150. Es un «bip-bop» descendente.
      // Derivación completa y controles en el docblock del SfxId (core/sfx.ts). El canal
      // es `view.emitSfx` directo, como todos los caminos de combate que no producen un
      // turno atómico (docblock de CoreViewImpl.emitSfx); el beep suena para TODO el
      // funnel en el binario (códigos 1/2/3 y default confluyen en 0x1f4b), así que el
      // día que se porten más rechazos de la arena reutilizan este mismo cue.
      //
      // ⚠ Y EL ALCANCE ES SÓLO LA ARENA. En MAZMORRA el original SÍ guarda: la Q llega
      // intacta al kernel (`DUNGEON.OVL:0x07a0 call → ULTIMA.EXE:0x3178` → handler 0x338c,
      // que NO lee `g_location` — su vecino 'S' en 0x33a8 sí lo hace, y ése es el control
      // que lo hace elocuente) y `CAST2.OVL:0x10fe` escribe SAVED.GAM sin gate de contexto.
      // El port ya lo hacía bien (`handleDungeonKey`, rama `dk === "q"` → `doQuitSave`).
      if (key.toLowerCase() === "q") {
        hud.message(COMBAT_STRINGS.quitReject); // COMBAT 0x0a78 → SJOG 0x1f26 código 2
        view.emitSfx({ id: "combat-reject" }); // los dos beeps del funnel (0x1f52/0x1f5d, #161)
        return; // ret 1 = re-prompt del mismo actor, sin turno
      }
      if (key.toLowerCase() === "g") {
        hud.echo(CMD_STRINGS.get); // "Get-" (COMBAT.OVL, DS 0x6e14 — NO el 0xa16a del kernel)
        pendingCombatDir = "get";
        return;
      }
      if (key.toLowerCase() === "o") {
        hud.echo(CMD_STRINGS.open); // "Open-" (COMBAT.OVL, DS 0x6e22 — NO el 0xa1ce del kernel)
        pendingCombatDir = "open";
        return;
      }
      // (J)immy/(S)earch/(P)ush/(Y)ell EN LA ARENA (ficha #121) — COMBAT.OVL @0x097a/
      // 0x09ac/0x0994/0x09c0. El eco del NOMBRE sale de la copia PROPIA del overlay
      // (DS 0x6e1a "Jimmy-" · 0x6e3a "Search-" · 0x6e28 "Push-" · 0x6e4e "Yell " — NO las
      // del despachador del kernel 0xa198/0xa1fc/0xa1e4/0xa286: mismos bytes, tabla
      // distinta; regla del gemelo byte-idéntico, re/notes/combat-commands.md). J/S van
      // por el funnel 0x0544 (codes 1/4 → SJOG:0x0d4a/0x095c) y P/Y llaman directo a
      // CMDS:0x161a/0x1418; J/S/P hacen getdir DENTRO de su rutina ⇒ mismo flujo
      // pendingCombatDir que (G)et/(O)pen. Derivación completa en Combat.playerJimmy/
      // playerSearch/playerPush/playerYell.
      if (key.toLowerCase() === "j") {
        hud.echo(CMD_STRINGS.jimmy);
        // SJOG 0x0d66 < 0x0d78: el gate de llaves va ANTES del getdir — sin llaves no se
        // pide dirección y el turno se consume igual (funnel 0x05b0 descarta el retorno).
        if (game.state.keys <= 0) {
          combatOut(combat.playerJimmy(null));
          pumpCombat();
          return;
        }
        pendingCombatDir = "jimmy";
        return;
      }
      if (key.toLowerCase() === "s") {
        hud.echo(CMD_STRINGS.search);
        pendingCombatDir = "search";
        return;
      }
      if (key.toLowerCase() === "p") {
        hud.echo(CMD_STRINGS.push);
        pendingCombatDir = "push";
        return;
      }
      // (Y)ell — CMDS:0x1418: "Yell " + "what?\n:" (DS 0x4529) + getstring(30) inline
      // (mismo flujo yell-word-prompt del overworld, 0x1463 `mov ax,0x1e`). Vacío/ESC →
      // "Nothing" y el turno SE CONSUME igual (retorno ignorado, jmp 0x7ba) — por eso
      // aquí `cancel` SÍ llama a playerYell(""), al revés que el yell del overworld.
      if (key.toLowerCase() === "y") {
        hud.echo(t(CMD_STRINGS.yell) + t(YELL_UI.what)); // t() por pieza (patrón Attack-Aim)
        hud.echoCursor(TALK_UI.cursor); // ":" → fila de eco viva del getstring
        prompts.current = {
          type: "text",
          prefix: TALK_UI.cursor,
          buffer: "",
          max: YELL_UI.max, // CMDS 0x1463 `mov ax,0x1e` → 30 chars
          resolve: (word) => {
            combatOut(combat.playerYell(word));
            pumpCombat();
          },
          cancel: () => {
            combatOut(combat.playerYell("")); // DS 0x4531 "Nothing\n" + turno
            pumpCombat();
          },
        };
        refreshAwaiting();
        return;
      }
      const d = COMBAT_DIRS[key];
      if (d) {
        // ECO DE DIRECCIÓN por paso de combate (careo-combate T2): SJOG 0x1C56
        // imprime la palabra ANTES de resolver el paso — "North\n"/"South\n"/
        // "East\n"/"West\n" (DS 0x8eb8/0x8ec0/0x8ec8/0x8ece, copias PROPIAS del
        // overlay, no las de getdir) — en TODO pulsado de flecha (éxito, Blocked!
        // y salida por el borde). Mismo patrón que el walk-echo del overworld.
        hud.echo(t(DIR_WORDS[d.dir8 as "north" | "south" | "east" | "west"]));
        combatOut(combat.playerMove(d.dir8 as never));
        pumpCombat();
      }
    };

    const SHOP_TYPES: Record<number, ShopType> = {
      0x81: "Blacksmith",
      0x82: "Barkeeper",
      0x83: "HorseSeller",
      0x84: "Shipwright",
      0x85: "MagicSeller",
      0x86: "GuildMaster",
      0x87: "Healer",
      0x88: "InnKeeper",
    };
    const shopData: ShopData = {
      equipmentBasePrices: data.equipmentBasePrices as number[],
      weaponsSoldByMerchants: data.weaponsSoldByMerchants as number[][],
      reagentBasePrices: data.reagentBasePrices as number[],
      reagentQuantities: data.reagentQuantities as number[],
      healPrices: data.healPrices as number[],
      curePrices: data.curePrices as number[],
      resurrectPrices: data.resurrectPrices as number[],
    };

    // ── Tienda por CONSOLA (piel fiel/shader) ────────────────────────────────────
    // ~~El binario NUNCA abre ventana~~ **ABSOLUTO FALSO, retirado #327**: lo desmiente su
    // propio árbol DOS veces — la ventana «Arms» del sell del herrero (shop-console.ts, ya
    // portada) y el GUEST REGISTER de la posada (#283, aterrizada 02b3a158). Un absoluto en
    // un comentario funciona como INSTRUCCIÓN DE NO MIRAR, y aquí lo que había que mirar era
    // justo el arquetipo de #278. Lo cierto y acotado: el mercader es una CONVERSACIÓN en la consola del
    // marco EGA (SHOPPES*.OVL, ret 2 del dispatcher de Talk). El ShopPanel DOM es QoL
    // sólo-dev (mismo patrón que talk-console y el fix de viewgem #76). El conductor
    // `ShopConsole` llama a EXACTAMENTE las mismas funciones del core que el panel DOM ⇒
    // las mutaciones de estado (y los sellos byte-idénticos del Grand Tour) se conservan.
    // censo-ui-flujos §3.
    const armShopKey = (): void => {
      prompts.current = { type: "shop", onKey: (k) => shopConsole?.key(k) };
      refreshAwaiting();
    };
    const endShopConsole = (): void => {
      shopConsole = null;
      prompts.current = null;
      // El panel Modern se desmonta AQUÍ y no sólo en la cola del keydown: la despedida
      // del mercader es el único momento en que la tienda se cierra, y dejarlo al
      // `syncShopPanel` del final de la tecla lo mantendría pintado un frame sobre un
      // mapa que ya no tiene tienda. Cerrar NO emite tecla (ver `dispose`).
      closeShopPanel();
      hud.refresh();
      refreshAwaiting();
    };
    const startShopConsole = (
      type: ShopType,
      info: ReturnType<typeof shoppeKeeperAt>,
    ): void => {
      shopConsole = new ShopConsole(type, {
        game,
        shopData,
        info,
        shoppeTexts,
        message: (text) => hud.message(text),
        refreshGold: () => hud.refresh(),
        // Jingle del servicio del curandero (SHOPPES 0x13b0, carril audio-costuras):
        // la consola de tienda vive fuera del stream de GameEvents → emisión directa
        // por el bus de la vista, como combate/casting. Presentación pura (0 RNG).
        sfx: (id) => view.emitSfx({ id }),
        armKey: armShopKey,
        // Keyword del rumor de taberna: getstring de consola inline (precedente Yell);
        // al resolver/cancelar vuelve al menú de la tienda (armShopKey).
        armText: (prefix, max, resolve) => {
          hud.echo(prefix); // fila de eco viva del getstring
          prompts.current = {
            type: "text",
            prefix,
            buffer: "",
            max,
            resolve,
            cancel: () => armShopKey(),
          };
          refreshAwaiting();
        },
        close: endShopConsole,
        // Picker de miembro del curandero (SHOPPES 0x137c → kernel select 0x8bfe):
        // cursor de roster SIN prompt propio (el `"Who needs my aid?" ` lo imprime
        // la consola de tienda). Cancel → `No one` + epílogo (lo maneja ShopConsole).
        pickMember: (onSelect, onCancel) => pickMember("", onSelect, onCancel),
        // T-004b: ventana «Arms» del flujo SELL del herrero (`list_wares` SHOPPES.OVL
        // 0x0c80) — REUTILIZA el overlay del picker de Ready (patrón openMixReagentPicker):
        // publica `ReadyPickerView` variante "shop" (fila ` N-Abbrev`, página de 5,
        // banda ▲/▼/↕) y conduce las teclas con el reductor puro `shopArmsKey` (↑/↓
        // mueven con scroll de página, Enter ELIGE la fila del cursor — la oferta +
        // Deal? Y/N de sell_one_item 0xe76 la conduce ShopConsole.pickSell —, Space/
        // ESC cancelan → Good-bye del flujo). Testigo clip #31 arms_sell_list.png.
        openArmsPicker: (
          rows: readonly ShopArmsRowInput[],
          onPick: (index: number) => void,
          onCancel: () => void,
        ): void => {
          let model = initShopArmsPicker();
          const toView = () => ({
            phase: "pick" as const,
            // Banner ►Arms◄ (testigo clip #31). La key «Arms» ya existe en es.json
            // (compartida con la página Arms de Ztats, «Armas»); el banner no pasa por
            // el choke de la piel → t() aquí ('en' = identidad byte-exacta).
            title: t("Arms"),
            variant: "shop" as const,
            rows: rows.map((r) => ({ name: r.name, qty: r.qty, equipped: false, glyph: 0 })),
            cursor: model.cursor,
            scroll: model.scroll,
          });
          const publish = (): void => view.setReadyPicker(toView());
          const close = (): void => {
            view.setReadyPicker(null);
            prompts.current = null;
            refreshAwaiting();
          };
          const onKey = (key: string): void => {
            const act = shopArmsKey(model, key, rows.length);
            if (act.kind === "move") {
              model = act.model;
              publish();
            } else if (act.kind === "pick") {
              close();
              onPick(act.index);
            } else if (act.kind === "close") {
              close();
              onCancel();
            }
          };
          publish();
          prompts.current = { type: "ready-picker", onKey };
          refreshAwaiting();
        },
        // #283 — ventana REGISTER de la posada. Mismo patrón que `openArmsPicker`
        // (modal del panel derecho, autoritativa sobre pendingPrompt), con SU reductor:
        // aquí el ESPACIO confirma, no cancela (SHOPPES3 0x06eb salta al mismo destino
        // que el Enter de 0x06e1), y ESC tiene su propio brazo con `No one`.
        openInnRegister: (
          guests: readonly string[],
          onPick: (index: number) => void,
          onCancel: () => void,
        ): void => {
          let model = initInnRegister();
          const publish = (): void =>
            view.setInnRegister({ guests, cursor: model.cursor });
          const close = (): void => {
            view.setInnRegister(null);
            prompts.current = null;
            refreshAwaiting();
          };
          const onKey = (key: string): void => {
            const act = innRegisterKey(model, key, guests.length);
            if (act.kind === "move") {
              model = act.model;
              publish();
            } else if (act.kind === "pick") {
              close();
              onPick(act.index);
            } else if (act.kind === "cancel") {
              close();
              onCancel();
            }
          };
          publish();
          prompts.current = { type: "ready-picker", onKey };
          refreshAwaiting();
        },
      });
      shopConsole.start();
    };

    const startTalk = (dir: Direction): void => {
      // Mercaderes: los bytes 0x81-0x88 en dialogNumber abren tienda
      const pos = game.state.position;
      if (pos.location !== 0 && game.npcManager) {
        const { dx, dy } = { north: {dx:0,dy:-1}, south: {dx:0,dy:1}, east: {dx:1,dy:0}, west: {dx:-1,dy:0} }[dir];
        const npc = game.npcManager.npcAt(pos.location, pos.floor, pos.x + dx, pos.y + dy);
        const shopType = npc ? SHOP_TYPES[npc.dialogNumber] : undefined;
        if (npc && shopType) {
          // #315 — el gate de TRAMO va ANTES que todo lo demás, y el orden es DERIVADO:
          // TALK 0x031E prueba la paridad del índice (0x03e7/0x03fa) y sólo entonces
          // llama a `0xe6`, que es donde vive la guarda del caballo (0x00ed). ⇒ a un
          // tendero fuera de tramo se le oye «Come see me… when it's open!», NO el
          // insulto del caballo, aunque llegues montado.
          if (!shopIsOpen(npc, game.state.time.hour)) {
            applyEvents([{ kind: "message", text: SHOP_CLOSED_MESSAGE }]);
            return;
          }
          // #170 — guarda de cabecera de `talk_to_npc` (TALK.OVL CS:0x00ed): el mercader
          // no atiende A CABALLO, salvo el HorseSeller (dialogNumber 0x83). Va AQUÍ, en la
          // rama de tienda, y no a la cabeza de startTalk: el despachador del binario
          // (CS:0x0396 `cmp [bp-2],0x80 / jge`) manda los dialogNumber < 0x80 al intérprete
          // TLK sin pasar por 0x00e6, así que la guarda NUNCA ve a un aldeano normal.
          const mounted = game.tryTalkMountedMerchant(dir);
          if (mounted) {
            applyEvents(mounted);
            return;
          }
          const info = shoppeKeeperAt(
            pos.location,
            shopType,
            shoppeKeeperMapJson as unknown as Record<string, ShoppeKeeperMapEntry>,
            (data.storeNames as string[]) ?? [],
            (data.shoppeKeeperNames as string[]) ?? [],
          );
          // Tienda por CONSOLA del marco EGA (menú por tecla), como el binario: el
          // mercader es una conversación, no una ventana. censo-ui-flujos §3.
          startShopConsole(shopType, info);
          return;
        }
      }
      // Guardias del Palacio de Blackthorn (TALK 0x01e2, dialogNum 0xFF): NO tienen
      // TLK script — los sirve el handler hardcodeado. En loc 0x12 retan por el
      // password (F1.7-T3, la vía de escape del bucle de captura). Interceptado ANTES
      // de talkTarget (que rechaza dialogNumber > 0x7f).
      const guard = game.tryTalkGuard(dir);
      if (guard) {
        applyEvents(guard);
        return;
      }
      // NPC poseído por un Shadowlord urbano (dialogNum 0xFD/0xFE, F1.10-T6): sin
      // TLK script; respuesta hardcodeada (huida / hostilidad). Antes de talkTarget.
      const possessed = game.tryTalkPossessed(dir);
      if (possessed) {
        applyEvents(possessed);
        return;
      }
      const target = game.talkTarget(dir);
      if (!target) {
        hud.message("Funny, no response!");
        return;
      }
      // Conversación por CONSOLA del marco EGA (getstring inline), como el binario
      // (censo-ui-flujos §2). Conducción entera en ui/talk-console.ts.
      talkConsole.start(target);
    };

    // Paneles Ztats (z) y Save (Esc/s)
    // 🔴 ÉSTE ES EL QUE ESCRIBE, y por eso el fix del nombre de sitio no podía quedarse en
    // `coreview.locationName()`: aquel alimenta la PIEL (banner, HUD), y este de aquí es el
    // que va DENTRO del guardado (`saveGame`/`autosave` → `SaveMeta.locationName`, que viaja
    // al sobre `.u5gam` y al nombre del fichero descargado) y el que compone la etiqueta por
    // defecto de una repetición. Son DOS CALCOS del mismo predicado con nombres distintos
    // —`locationName` allí, `mapName` aquí—, y un censo por el nombre de la función se deja
    // uno: el identificador de máquina que reportó el usuario salía de los dos.
    // Ahora los dos llaman a la MISMA función, que es lo que impide que vuelvan a separarse.
    // ⚠ Y desde #132 la que comparten es `mapDisplayName`, no sólo `locationDisplayName`:
    // lo que seguía escrito DOS veces era el respaldo (`|| smallMaps.name ?? "?"`), que es
    // justo por donde el identificador de máquina volvía al guardado. Un solo sitio, y con
    // nombre propio para que un test pueda apuntarle.
    const mapName = (): string =>
      mapDisplayName(
        game.state.position.location,
        game.state.position.floor,
        data.locationNames,
        () => world.smallMaps.get(game.state.position.location)?.name,
      );
    // Aplica un GameState cargado sobre el estado vivo (in-place: renderer/HUD
    // conservan la referencia a game.state) y re-renderiza. Compartido por el panel
    // de guardado y por el hook DEV del arnés píxel-diff (task #26 fase 2).
    const applyLoadedState = (loaded: GameState): void => {
      // ── Teardown de ESCENAS/TIMERS vivos ANTES de pisar el estado (auditoría R3) ──
      // Cargar partida en mitad de una escena a reloj de pared (camp/refuge/endgame/
      // troll-sneak) dejaba los timers viejos mutando el estado recién cargado
      // (campWake curaba/avanzaba reloj; resolveRefuge teleportaba a LB; en endgame
      // `endgaming` tragaba el teclado para siempre). Se cancela TODO, se bajan los
      // flags modales y se desmontan las vistas de escena; los prompts transitorios
      // de input también se limpian (pertenecían a la partida anterior).
      cancelAutoWalk();
      campSleepCtl.reset(); // cancel + flag modal abajo + desmonta la escena
      bedSleepCtl.reset(); // #296: cancel + flag + DESMONTA la cortina negra del sueño en cama
      ritualInvertCtl.reset(); // #295: cancel + DESMONTA la inversión del WELL DONE
      cancelMapReveal(); // ★ #326: apaga el revelado de la poción blanca y su timer
      cancelRefuge();
      endgamePacer.reset(); // cancel + flag + awaitKey + desmonta la escena
      trollSneakCtl.cancel();
      shrineScenePacer.cancel();
      blackthornScenePacer.cancel(); // #324: cancel + DESMONTA la escena de la captura
      shrineKeyPacer.cancel(); // ★ #294: olvida el turno de rito aparcado en una espera de tecla
      view.setShrineScene(null); // #277: desmonta la escena si el reinicio la pilla montada
      poisonTickCtl.cancel(); // ★ #213: apaga el flash de daño y su timer
      refuging = false;
      view.setRefugeScene(null);
      if (canvasGemActive) {
        canvasGemActive = false;
        view.setGemView(null);
      }
      if (zodiacActive) {
        zodiacActive = false;
        view.setZodiacView(null);
      }
      prompts.current = null;
      // …y con el prompt se va el panel Modern de tienda, que es una vista suya: cargar
      // partida en mitad de un mercader lo dejaría pintado sobre la partida nueva (misma
      // clase que las escenas y los timers que este teardown ya baja).
      closeShopPanel();
      pendingDirCommand = null;
      pendingDungeonKlimb = false;
      pendingDungeonSearch = null;
      pendingDungeonLook = false;
      pendingCastDoor = null;
      pendingCastUnlock = false;
      pendingCastBlink = false;
      pendingUseSkullKey = false;
      pendingScrollWind = null;
      Object.assign(game.state, loaded);
      if (game.state.position.location !== 0) {
        game.npcManager?.enterMap(game.state.position.location, game.state, true); // #108: rehidrata la maquina de caminata del save (npcWalk)
      }
      // Cargar = `town_load_map(fresh=0)` → `0x0408(0)` (ULTIMA.EXE 0x00f7 → TOWN 0x11F0): el
      // cargador pone el tracker de puerta abierta [0x594f] a 0 (0x041d) y relee la planta, así
      // que una puerta abierta al guardar vuelve CERRADA. Batch 24, H-162.
      game.doors?.reset();
      game.refreshHourTiles(); // reja/puente por hora (TOWN 0x0170) tras cargar
      view.notifyTurn([{ kind: "map-changed" }]); // pieles: snap de cámara
      updateMusic();
      hud.refresh();
      refreshAwaiting(); // el teardown pudo cerrar prompts → re-deriva el gate del cursor
    };
    const savePanel = new SavePanel(parent, {
      onLoad: (loaded: GameState) => {
        applyLoadedState(loaded);
        hud.message("Game loaded.");
        // La partida ya está pintada: si su miniatura persistida es de las viejas (jpeg
        // 200×125, o el teléfono entero de los layouts táctiles), se rehace ahora. Sin
        // `await`: la carga no espera a un adorno. Ver `ui/shot-refresh.ts` para por qué
        // el criterio NO es «¿es 320×200?» y por qué esto no puede entrar en bucle.
        //
        // El id NO viaja por el callback —`ui/savepanel.ts` tiene otro dueño en esta ola—
        // así que se pregunta a quien de verdad lo sabe: `persistence.ts` lo anota en
        // `loadGame` y lo BORRA en los caminos de importar, que es lo que evita re-escribir
        // la foto de una partida importada encima de la miniatura de otra.
        const id = slotDeLaUltimaCarga();
        if (id) void refrescarMiniatura(id, parent);
      },
      onMessage: (text: string) => hud.message(text),
      onAnalyticsEvent: (event: string) => consentimiento.analitica.evento(event),
      // Miniatura para la tarjeta de /byo. Lectura pura del canvas: cero tiradas de
      // RNG (medido, ver la cabecera de ui/screenshot.ts).
      snapshot: () => captureScreenshot(parent),
      // Maqueta #23: bajo `?shell8x8=1` el panel de partidas se pinta con la fuente 8×8.
      ...(shell8x8On
        ? { afterRender: (root: HTMLElement) => syncPixelFontSaves(root, true) }
        : {}),
    });
    // Hook DEV del arnés píxel-diff (task #26 fase 2) y del encadenado del Grand Tour
    // (F3): carga un SAVED.GAM NATIVO (bytes del original o del checkpoint del port,
    // 4192 B) en el estado vivo y re-renderiza.
    //
    // El `sidecar` OPCIONAL lleva el estado de juego SIN hueco en el .GAM (transporte,
    // questFlags, naval, Shadowlords, hechizos temporales — ver SaveSidecar). Dos
    // llamadores:
    //   · píxel-diff (#26): carga un .GAM del ORIGINAL sin sidecar → defaults de
    //     partida nueva (a pie, sin runtime), igual que importNativeSaveFiles sin
    //     sidecar. El .GAM del original no trae ese estado, así que no hay nada que
    //     hidratar.
    //   · Grand Tour (F3): encadena capítulos por el checkpoint del port
    //     (saves/chNN.{gam,sidecar.json}). Aquí SÍ pasa el sidecar para que ch02+
    //     hidrate su estado de juego; sin esto el hook lo descartaba en silencio
    //     (deuda #1 de la review de ch01, antes de que los capítulos con estado
    //     runtime lo necesiten).
    const NEW_GAME_SIDECAR: SaveSidecar = {
      version: 1,
      qol: { journal: [] },
      gameState: { transport: "foot", questFlags: {} },
    };
    if (import.meta.env.DEV) {
      const hooks = (window as unknown as Record<string, unknown>).__u5test as
        | Record<string, unknown>
        | undefined;
      if (hooks) {
        // PUERTA #D1 (`core/npc/carga-fiel.ts`): estado REAL de la puerta DENTRO de la
        // página. Read-only y sólo DEV. Existe porque «puse la env en la shell» y «la puerta
        // está abierta en el bundle» son dos afirmaciones distintas (dos procesos, dos
        // mecanismos): un careo de brazos que no lo compruebe puede medir un brazo con la
        // puerta cerrada creyéndola abierta, y leer «no hay diferencia» como «los NPC no
        // afectan aquí» — el veredicto invertido.
        hooks.cargaFielActiva = (): boolean => cargaFielActiva();
        hooks.loadNativeSave = (bytes: ArrayLike<number>, sidecar?: SaveSidecar): void => {
          const gam = bytes instanceof Uint8Array ? bytes : Uint8Array.from(bytes);
          applyLoadedState(importNativeSave(gam, sidecar ?? NEW_GAME_SIDECAR));
        };
        // Señal LÓGICA de "diálogo abierto" para el arnés del Grand Tour (task #8): la
        // charla va por CONSOLA (fiel/shader), así que la señal es `conversation != null`
        // (armada síncrona en el mismo keydown). Un fallo ("Funny, no response!") la deja
        // en falso. Independiente del render → el arnés detecta enganche/fallo sin techo
        // de reloj. Read-only. (La piel dev DOM se jubiló en la fase 2.)
        hooks.dialogueOpen = (): boolean => talkConsole.active;
        // ¿Esta piel conduce la conversación por CONSOLA? Siempre sí tras la jubilación de
        // la piel dev (ya no hay panel DOM). El arnés lo consulta para elegir cómo TECLEAR
        // la keyword y de dónde LEER la respuesta (getstring + consoleLines). Read-only.
        hooks.dialogueConsole = (): boolean => true;
        // Consola LÓGICA para el arnés del Grand Tour (task #16): la MISMA fuente
        // que pinta el HUD dev (`view.snapshot().console`) y la consola de la piel
        // fiel — el `.hud-log` DOM sólo existe en DevSkin (skin/dev.ts). Leerla por
        // hook hace `faceCommand`/`readSign` de nav.ts INDEPENDIENTES de piel. Los
        // helpers slicean el tail (últimas 2-4 líneas), presente en ambas pieles.
        // Read-only, sólo DEV: cero efecto en estado.
        hooks.consoleLines = (): string[] => view.snapshot().console.map((l) => l.text);
        // Como `consoleLines` pero con la metadata de PRESENTACIÓN (careo #341): `kind`
        // (echo=lleva bullet ► y abre grupo) y `cont` (eco SIN bullet que no abre grupo,
        // console.ts:115). El e2e del prompt de Vas Rel Por asserta contra el testigo de
        // vídeo que las filas de prompt son planas. Read-only, sólo DEV.
        hooks.consoleMeta = (): { text: string; kind: string; cont: boolean }[] =>
          view.snapshot().console.map((l) => ({ text: l.text, kind: l.kind, cont: l.cont === true }));
        // ¿Está publicado el cursor de la FILA VIVA del prompt (cabo #341 §7.2)? La
        // MISMA señal que hace a la piel pintar la ola tras «To phase: »/«Player: »
        // (snapshot().awaitingGetstring ← promptCursorOnLiveRow). El e2e del prompt la
        // asserta con el prompt abierto: es la guarda de CABLEADO de refreshAwaiting —
        // revertir el flag a la terna text/number/rune la enrojece. Read-only, sólo DEV.
        hooks.promptCursor = (): boolean => view.snapshot().awaitingGetstring;
        // HISTORIAL largo de consola (carril log-scroll): el ring de arriba se queda
        // en 12 (contrato del arnés/e2e intacto); esto expone el buffer de 500 que
        // alimenta el modo scrollback de la piel. Read-only, sólo DEV.
        hooks.consoleHistory = (): string[] =>
          (view.snapshot().consoleHistory ?? []).map((l) => l.text);
        // Estado del picker de READY (task #78) para el arnés e2e: fase + filas +
        // cursor, o null fuera de Ready. La MISMA vista que pinta la piel fiel
        // (`snap.readyPicker`), así el e2e conduce el overlay sin depender de píxeles.
        // Read-only, sólo DEV.
        hooks.readyPicker = (): unknown => view.snapshot().readyPicker ?? null;
        // ¿Hay un select de PJ sobre el roster armado? (party-select del kernel
        // 0x2d7a/0x4988: Ready/Ztats/Cast/Search/curandero). El arnés del tour lo
        // consulta para CONFIRMAR el cursor (Enter) donde el guion no fija miembro
        // (p.ej. (S)earch con party>1, C6). Read-only, sólo DEV.
        hooks.partySelectOpen = (): boolean => prompts.current?.type === "party-select";
        // ¿Hay un prompt de GUARDIA pendiente? (demanda de tributo TALK 0x01e2 /
        // arresto TOWN 0x12ae, F2-T4). El arnés del tour lo consulta en cada espera
        // de turno y lo resuelve PAGANDO ('y') — ruling del lead: el arnés se adapta
        // al mundo. Read-only, sólo DEV.
        hooks.guardPromptOpen = (): string | null =>
          prompts.current?.type === "yesno" ? (prompts.current.tag ?? null) : null;
        // ¿Hay una tienda abierta? (conductor de consola fiel/shader). El arnés lo consulta
        // para saber que el (T)alk a un mercader enganchó. Read-only.
        hooks.shopOpen = (): boolean => shopConsole != null;
        // TIPO del prompt VIVO del gestor de prompts (`prompts.current`), o null si no hay
        // ninguno: "text"/"number"/"rune" (getstring), "yesno", "digit", "party-select",
        // "ready-picker", "shop". El arnés del ESPEJO lo consulta ANTES de teclear una
        // keyword del LP: sin sumidero de entrada vivo, un `typed` se pulveriza sobre el
        // MAPA como ráfaga de comandos (la W de "YOUNGW" abre (W)ear, la C de "COMPASSION"
        // abre (C)ast…) y deja modos abiertos que se tragan el resto del guion — el
        // atasco medido en el estreno AD (episodios enteros con la consola repitiendo
        // «Thou must free one of thy hands first!»). Read-only, sólo DEV.
        hooks.promptType = (): string | null => prompts.current?.type ?? null;
        // Estado de la TIENDA POR CONSOLA (fiel/shader) para el arnés mode-aware: tipo +
        // fase + opciones {key,label} de la lista/menú actual, o null en piel dev (que usa
        // el panel DOM). El arnés lo lee para mapear un `itemLabel` a la tecla que hay que
        // pulsar (menú → lista → letra), igual que `dialogueConsole` para las keywords.
        // Read-only, sólo DEV: cero efecto en estado.
        hooks.shopConsole = (): unknown => shopConsole?.snapshot() ?? null;
        // Idioma activo (capa i18n, F1) para el e2e del selector: leer + fijar en
        // caliente sin persistir (efímero, no ensucia la preferencia). La consola ya
        // sale traducida por `t()` en `pushConsole`, así que `consoleLines()` refleja
        // el idioma. Read-only salvo `set`. Sólo DEV.
        hooks.lang = (): string => getLang();
        hooks.setLang = (l: string): void => setLang(l, { persist: false });
        // Empuja un string INGLÉS por el MISMO path de consola del juego (hud.message
        // → pushConsole → t()), para que el e2e de i18n verifique las 20 semillas
        // traducidas+re-wrapeadas de punta a punta (render real), no sólo por unidad.
        hooks.pushConsole = (s: string): void => hud.message(s);
        // Señal LÓGICA de "mundo montado" (task #16): sustituye la espera de arranque
        // por `.hud-clock` (DOM sólo-dev). Este bloque se alcanza tras el título DOM
        // (piel dev, post-click) o tras el montaje directo con `?nointro` (piel fiel),
        // así que su MERA existencia marca el fin del boot en AMBAS pieles.
        hooks.worldReady = (): boolean => true;
        // Sonda de VISTA para el arnés e2e (combat-view-bug): qué está pintando la piel
        // sin depender de píxeles. Durante un combate de SALA de mazmorra game.combat y
        // game.dungeonState coexisten; la vista debe ser la ARENA (dungeon:null), no el
        // pasillo 3D. La MISMA fuente que consume la piel (`view.snapshot()`). Read-only, DEV.
        hooks.viewMode = (): { mode: string; hasDungeon: boolean; hasCombat: boolean } => {
          const s = view.snapshot();
          return { mode: s.mode, hasDungeon: s.dungeon != null, hasCombat: s.combatView != null };
        };
        // Pacers de ESCENA vivos (rito de santuario / captura de Blackthorn) y su
        // keywait (#294, `getkey_with_redraw` CAST2 0x448c → kernel 0x266c). El arnés
        // de vídeo (graba-video.mjs) los consulta para pulsar Espacio SOLO cuando el
        // rito está aparcado esperando tecla y CORTAR cuando la escena drenó: los
        // guiones de N espacios FIJOS derramaban los sobrantes al despachador como
        // «>Pass» (auditoría de la colección de eventos, 22-08 — codice-ceremonia con
        // ~12 s de cola, blackthorn-captura con ~8 s). Read-only, sólo DEV.
        hooks.scenePacers = (): {
          shrineScene: boolean;
          shrineKeyWait: boolean;
          blackthornScene: boolean;
        } => ({
          shrineScene: shrineScenePacer.active,
          shrineKeyWait: shrineKeyPacer.active,
          blackthornScene: blackthornScenePacer.active,
        });
        // Fase VIVA de la escena del ENDGAME (#34) para el arnés e2e — la MISMA vista
        // que pinta la piel (`snap.endgameScene`), o null fuera del cierre. Read-only, DEV.
        hooks.endgamePhase = (): string | null => view.snapshot().endgameScene?.phase ?? null;
        // PÁGINA viva de END.DAT en los beats de historia (#176): `storyHouse` cubre las
        // páginas 0-1 y `storyDream` las 2-5, así que la FASE SOLA NO IDENTIFICA la
        // pantalla — el arnés visual del desenlace empareja cada página con su segundo
        // del testigo, y sin esto compararía la lámina de la página 2 contra la de la 5.
        // null fuera de un beat de historia. Read-only, DEV.
        hooks.endgameStoryPage = (): number | null => {
          const eg = view.snapshot().endgameScene;
          if (!eg || (eg.phase !== "storyHouse" && eg.phase !== "storyDream")) return null;
          return eg.storyPage ?? null;
        };
        // TEXTO VIVO del pergamino del cierre (fase `scroll`): las líneas que el pacer
        // publicó (`runScroll` → scene.scrollLines) y el revelado alcanzado. Cierra el
        // agujero de verificación del bug scroll-vacío 2026-07-23 (el e2e asertaba fase
        // y eventos, no el CONTENIDO estampado en canvas). Read-only, DEV.
        hooks.endgameScroll = (): { lines: string[]; reveal: number } | null => {
          const eg = view.snapshot().endgameScene;
          if (!eg?.scrollLines) return null;
          return { lines: eg.scrollLines.map((l) => l.text), reveal: eg.scrollReveal ?? 0 };
        };
        // Sonda ÚNICA del PACER de la tanda enemiga (auditoría de calidad Q2/G3): estado
        // vivo del beat de combate — ¿hay beat en vuelo? (`combatPacer !== null`), ¿cuántas
        // teclas hay ENCOLADAS esperando el drenaje?, ¿qué beat rige? (`?combeat` /
        // webdriver→0 / 400 default) y el gate del cursor (`awaitingInput`, que
        // refreshAwaiting apaga durante la tanda). Cubre la clase «solo-live» del
        // pacer: el spec e2e/combat-pacer.spec.ts la conduce con `?combeat` real.
        // (Reconciliación: subsume las sondas combatPacerActive/combatQueueLength de
        // calidad/lote-guardas G3 — `active`/`queued` son esos mismos valores.)
        // Read-only, sólo DEV: cero efecto en estado.
        hooks.combatPacer = (): {
          active: boolean;
          queued: number;
          beatMs: number;
          awaiting: boolean;
        } => ({
          active: combatPacer.pacing,
          queued: combatPacer.queued,
          beatMs: ENEMY_BEAT_MS,
          awaiting: view.snapshot().awaitingInput,
        });
        // Sonda del CURSOR DE AIM vivo (celda o null). Necesaria para conducir combate con
        // PACERS VIVOS (beat>0): `chainNextWeaponAim` re-abre el Aim del arma siguiente SIN
        // tecla del driver (fidelidad COMSUBS 0x0D96→0x0D3C), y un driver que sondea estado
        // no puede saber que sus flechas moverían el cursor en vez de al actor — medido en
        // ch16b (§8.1 tempo-cinematico.md): livelock de 62 rondas con el actor congelado.
        // Read-only, sólo DEV: cero efecto en estado.
        hooks.combatAim = (): { x: number; y: number } | null =>
          view.snapshot().combatView?.aim?.cell ?? null;
      }
    }
    // Plantilla base (4192 B) para el export SAVED.GAM nativo (task #27). Best-effort:
    // si falta /assets/init.gam, el panel deshabilita el export nativo con un aviso.
    void fetch("/assets/init.gam")
      .then((r) => (r.ok ? r.arrayBuffer() : null))
      .then((buf) => savePanel.setSaveTemplate(buf ? new Uint8Array(buf) : null))
      .catch(() => savePanel.setSaveTemplate(null));

    // Selector genérico para Mix/Cast/Ready
    const selector = new SelectorPanel(parent);
    selectorPanel = selector; // señal para refreshAwaiting (gate del cursor F-G)
    // El selector se cierra por su propio botón Cancel / submit (fuera del keydown
    // de window, que además intercepta); envolvemos `hide` para re-derivar el gate
    // del cursor (F-G) al cerrarse sin pasar por applyEvents.
    const selectorHide = selector.hide.bind(selector);
    selector.hide = (): void => {
      selectorHide();
      refreshAwaiting();
    };
    const spellDefs: SpellDef[] = buildSpellDefs(magicDefsJson as never);
    // RNG del kernel para las tiradas de magia (Mani, comida…). Semilla por
    // tiempo en la GUI; la paridad exacta contra el binario vive en re/tools.
    const castRng = new CombatRng(new OriginalRng(Date.now() & 0xffff));
    const EQUIP_NAMES: string[] = (
      (inventoryDetailsJson as { Armament: { ItemName: string }[] }).Armament ?? []
    )
      .slice(1)
      .map((a) => a.ItemName.replace(/([a-z])([A-Z])/g, "$1 $2"));

    // Pickers de PJ FIELES + getstring rúnico (select_party_member 0x2d7a /
    // resolve_command_char 0x4988 / On-who CAST2 0x9e / rúnico CAST2 0x00de) —
    // extraídos a ui/pickers.ts (TRAMO 2); derivación completa en sus cabeceras.
    const { pickMember, pickCaster, pickCommandChar, pickCastTarget, pickSpellTyped } =
      createPickers({
        game,
        hud,
        view,
        prompts,
        refreshAwaiting: () => refreshAwaiting(),
      });

    // Nombre abreviado del reagente para la lista del selector (data.json `reagents`
    // = DATA.OVL name-table 0x19d2, byte-idéntico: "Sulfur Ash".."Mandrake").
    const reagentNames = (data.reagents as string[] | undefined) ?? [];
    const reagentName = (id: number): string => reagentNames[id] ?? `Reagent ${id}`;

    // ── INTERFAZ DE LANZAMIENTO: «Clásico» / «Lista de hechizos» ─────────────────────
    // El catálogo son los MISMOS `spellDefs` de arriba (48 lanzables; Nox queda fuera por
    // incastable) y los MISMOS nombres de reactivo que ya usa el selector de (M)ix. Cero
    // datos nuevos y cero tablas nuevas: ver `enhanced/spells/catalog.ts`.
    const spellCatalog = buildSpellCatalog(spellDefs, reagentNames);
    /**
     * El sustituto de `pickSpellTyped` para los TRES (C)ast (exterior/pueblo, mazmorra,
     * arena). En modo Clásico ES `pickSpellTyped` —la misma función, sin envolver—; en
     * modo Moderno abre la lista y luego TECLEA las iniciales elegidas por ese mismo
     * getstring rúnico. Toda la derivación, en `enhanced/spells/entry.ts`.
     *
     * 🔴 (M)ix NO lo usa y no debe usarlo: mezclar es otro comando con otro prompt.
     */
    const pickSpellForCast = makeCastSpellEntry({
      pickSpellTyped,
      catalog: () => spellCatalog,
      quantity: (index) => game.state.spellQuantities[index] ?? 0,
      // El MISMO reparto que `requiredTimeBit` (CAST:0x0e1a): combate manda sobre todo, y
      // fuera de él decide `g_location` — con la mazmorra leída de `dungeonState`, que es
      // donde vive en el port (ver la nota de `doDungeonCast`).
      place: () => {
        if (game.combat) return "combat";
        if (game.dungeonState) return "dungeon";
        return game.state.position.location === 0 ? "outdoor" : "town";
      },
      // El lanzador ya lo resolvió `pickCaster` (o el actor del turno en la arena) ANTES de
      // llegar aquí: el activo es la mejor aproximación disponible y sólo sirve para
      // ATENUAR filas, nunca para bloquearlas.
      caster: () => {
        const st = game.state;
        const idx =
          game.combat?.currentUnit?.charIdx ??
          (st.activeCharacter !== 0xff && st.activeCharacter < st.partySize
            ? st.activeCharacter
            : 0);
        const c = st.characters[idx];
        return c ? { name: effectiveName(c.name), mp: c.currentMp, level: c.level } : null;
      },
    });

    // Vista del selector de reagentes para la piel: REUTILIZA el overlay de lista del
    // comando Ready (`ReadyPickerView`, fase `pick`) — el binario también comparte
    // `render_item_list` entre Ready y otras listas (readyPicker.ts). El marco NO es una
    // aproximación: 0x5ca2 (→ kernel 0x1c22, `set_text_window`) sólo FIJA el rectángulo de
    // ventana (24,1,38,9) — el MISMO `READY_PICKER_RECT` — y el borde pergamino es el
    // chrome de panel compartido; NO hay box propio que pintar (derivación con
    // sanity-check 0x58d0→print_string). La fila usa la variante "mix" de `readyRowCells`:
    // " NN <marca 0xfd> NAME", con relleno 0xf al marcar (0x1a19). FIEL-DERIVADO.
    const mixReagentView = (rows: readonly MixReagentRow[], model: MixReagentPickerModel) => ({
      phase: "pick" as const,
      title: t(MIX_UI.reagents), // DS 0x8f64, cabecera de la lista (@0x1924)
      variant: "mix" as const, // fila " NN <marca 0xfd> NAME" (CMDS.OVL 0x18be), no la de Ready
      rows: rows.map((r) => ({
        name: r.name,
        qty: r.qty,
        equipped: (model.selected & reagentBit(r.reagentId)) !== 0, // marcado (relleno 0xf)
        glyph: 0x0f, // (no usado por la variante mix; la marca la pinta readyRowCells)
      })),
      cursor: model.cursor,
      scroll: 0, // ≤8 reagentes: caben en las 7 filas sin scroll (rara vez 8 poseídos)
    });

    // Selector de reagentes con flechas (CMDS.OVL 0x18be): tras el nombre del hechizo,
    // el jugador MARCA a mano los reagentes (↑↓ mueven, RETURN/Space marcan, 'M' mezcla,
    // ESC cancela). Devuelve los índices SELECCIONADOS a `onMix`. Presentación por el
    // overlay de Ready (arriba); teclas por el reductor puro `mixReagentKey`.
    const openMixReagentPicker = (def: SpellDef, onMix: (selected: number[]) => void): void => {
      let model = initMixReagentPicker();
      const rows = buildMixReagentRows({
        qtyOf: (id) => game.state.reagentQuantities[id] ?? 0,
        nameOf: reagentName,
      });
      const selectedIds = (m: MixReagentPickerModel): number[] =>
        rows.filter((r) => (m.selected & reagentBit(r.reagentId)) !== 0).map((r) => r.reagentId);
      const publish = (): void => view.setReadyPicker(mixReagentView(rows, model));
      const close = (): void => {
        view.setReadyPicker(null);
        prompts.current = null;
        refreshAwaiting();
      };
      const onKey = (key: string): void => {
        const act = mixReagentKey(model, key, rows);
        if (act.kind === "move" || act.kind === "toggle") {
          model = act.model;
          publish();
        } else if (act.kind === "mix") {
          close();
          onMix(selectedIds(model));
        } else if (act.kind === "close") {
          close(); // ESC: cancela SIN mezclar ni mensaje (@0x1a2e, ret -1)
        }
      };
      // PIE DE INSTRUCCIONES a la CONSOLA, antes de abrir el panel (CMDS 0x1b1e-0x1b5a
      // va delante del `call 0x18be` de 0x1b5d): "←,→,↑,↓ to move," / "RETURN selects."
      // / "Type M to mix:". El port no lo emitía en absoluto — es el texto que el
      // usuario echó en falta, y el C13 que el acta espejo-part08 dejó sin adjudicar
      // («¿picker o consola?»): el binario dice CONSOLA, y su captura del original lo
      // confirma. Los glifos de flecha NO pasan por t() (son códigos CP437).
      for (const line of mixPickerFooterLines()) hud.message(t(line));
      publish();
      prompts.current = { type: "ready-picker", onKey };
      refreshAwaiting();
    };

    // Cantidad de Mix "How much? " (DS 0x8f72; CMDS.OVL 0x1a70): tras SELECCIONAR los
    // reagentes, el original lee un NÚMERO tecleado (getnum 0x7c1e, 2 dígitos → 99).
    // N<=0 aborta en silencio (0x1b71 `jle`). Si N supera algún reagente MARCADO →
    // "Insufficient reagents!" (DS 0x8f7e) y RE-PREGUNTA (bucle 0x1ac6). Selección VACÍA
    // → "Nothing to mix!" (DS 0x9004, 0x1b78). Con reagentes marcados: "Mixing..." (DS
    // 0x8ff0, 0x1b81) → consume N de cada MARCADO → si la máscara casa con la requerida
    // "Done!" (DS 0x8ffc) + carga; si NO, se gastan igual sin carga y SALTA LA TRAMPA
    // (0x1bf6-0x1c04).
    // 🔴 «Mix NO toca RNG» decía aquí hasta #105, y era FALSO: la rama de reagentes
    // equivocados entra en chest_trap_trigger y consume de 1 a 7 tiradas (1 el tipo,
    // +1 ACID, +1 por miembro vivo BOMB, +0 POISON/GAS). El error venía de contar los
    // `rand` del CUERPO de cmd_mix y no los de su callee — la misma figura corregida en
    // re/notes/cmds.md §12 y en las dos citas del ledger de CMDS 0x1ad8.
    const askMixQuantity = (def: SpellDef, selected: number[]): void => {
      const arm = (): void => {
        hud.echo(t(MIX_UI.howMuch)); // DS 0x8f72, fila de eco viva
        prompts.current = {
          type: "number",
          prefix: t(MIX_UI.howMuch),
          buffer: "",
          max: 2,
          submit: (n) => {
            if (n <= 0) return; // N<=0: aborta en silencio (0x1b71)
            if (selected.length === 0) { hud.message(t(MIX_UI.nothingToMix)); return; } // DS 0x9004 (0x1b78)
            const short = selected.some((r) => (game.state.reagentQuantities[r] ?? 0) < n);
            if (short) {
              hud.message(t(MIX_UI.insufficient)); // DS 0x8f7e
              arm(); // re-pregunta (0x1ac6)
              return;
            }
            hud.message(t(MIX_UI.mixing)); // DS 0x8ff0 (0x1b81)
            // La MECÁNICA (consumo + carga + TRAMPA de #105) vive en Game porque la rama
            // de reagentes equivocados tira dados con el stream vivo; aquí sólo se
            // presentan sus eventos: "Done!" (0x1bd6) o el bang + el tipo de trampa
            // ("ACID!"/"POISON!"/"BOMB!"/"GAS!", DS 0x5581-0x5598) + los blips de daño.
            applyEvents(game.mixReagents(def, selected, n));
          },
        };
        refreshAwaiting();
      };
      arm();
    };

    const doMix = (): void => {
      // Mix (CMDS 0x1AD8): "For what spell?" (DS 0x8fac) → nombre TECLEADO por iniciales
      // rúnicas (CMDS 0x1b0d, el MISMO getstring que Cast) → SELECTOR de reagentes con
      // flechas (openMixReagentPicker, 0x18be) → cantidad "How much?" (askMixQuantity,
      // 0x1a70) → game.mixReagents. ⚠ CON RNG en la rama de reagentes equivocados (la
      // trampa de #105; el «SIN RNG» que decía aquí era la 2ª instancia de la misma
      // afirmación falsa). Vacío/ESC en el nombre → "None!" (DS 0x8fbe).
      // Nombre no válido → "No effect!" (evita el índice -2 crudo del binario).
      // PRECHECK (CMDS 0x1ae0-0x1af8): si la suma de TODOS los reagentes es 0, aborta
      // ANTES de preguntar con "No reagents owned!" (DS 0x8f98).
      // ECO DEL DISPATCHER: "Mix Reagents\n\n" (DS 0xa1b4). El kernel lo imprime al
      // pulsar M ANTES de entrar en cmd_mix, así que va delante del precheck de
      // reagentes — la captura del usuario lo muestra como ">Mix Reagents". La cadena
      // estaba declarada en CMD_STRINGS.mix y NO la emitía nadie (mismo género que el
      // eco de "Use item", C12 del acta espejo-part08).
      hud.echo(CMD_STRINGS.mix);
      const totalReagents = game.state.reagentQuantities.reduce((a, b) => a + (b ?? 0), 0);
      if (totalReagents === 0) { hud.message(t(MIX_UI.noReagents)); return; } // DS 0x8f98
      // Prompt en DOS filas: "For what spell?" y debajo el cursor ":" del getstring —
      // el original es UN solo string con `\n` dentro (DS 0x8fac "For what spell?\n:",
      // CMDS 0x1b06). El port lo emitía en una sola fila ("For what spell? AN NOX").
      // Las DOS filas las arma `pickSpellTyped` (#107, común con Cast); aquí sólo viaja
      // la etiqueta, y por `MIX_UI.forWhatSpell` para que la cadena tenga su cita DS.
      pickSpellTyped(t(MIX_UI.forWhatSpell), (initials) => {
        if (initials === "") { hud.message(t(MIX_UI.none)); return; }
        const idx = matchSpellByInitials(spellDefs, initials);
        if (idx < 0) { hud.message("No effect!"); return; }
        const def = spellDefs[idx]!;
        openMixReagentPicker(def, (selected) => askMixQuantity(def, selected));
      });
    };

    /**
     * Vas Rel Por (idx 46) — `vas_rel_por_phase_gate`, `CAST.OVL:0x0cf0`-`0x0d4b` (ficha
     * #341, derivación completa en `re/notes/vas-rel-por-341-derivacion.md`). Teleporta a
     * la piedra lunar de la fase que TECLEAS, no a la de la hora.
     *
     * ```
     * 0cf6  if ((g_transport_tile & 0xf0) == 0x20) → ret 0     ; a bordo de un BARCO
     * 0cff  print "To phase: "                                 ; DS 0x45e7
     * 0d06  al = getkey()                                      ; kernel 0x266c
     * 0d0c  if (al >= 0x20) putchar(al)                        ; eco sólo si imprimible
     * 0d16  putchar('\n')                                      ; SIEMPRE
     * 0d1d  if (al < '1' || al > '8') → ret 0                  ; sin bucle de reintento
     * 0d29  al -= '1'                                          ; fase 0..7
     * 0d2d  jingle 8                                           ; CAST2:0x0000
     * 0d3a  res = moonstone_teleport(al)                       ; kernel 0x47f4
     * 0d3f  return res ? -1 : 0
     * ```
     *
     * 🔴 EL RETORNO TIENE **TRES** VALORES, y ahí estaba el pendiente que #319 §6 dejó
     * escrito («¿por qué el brazo pone `[bp-6] = 0`?»). La pregunta era del DESPACHADOR, no
     * de la rutina: `cmd_cast` `CAST.OVL:0x112e`-`0x113d` hace `res = vas_rel_por()`,
     * `[bp-0xa] = res`, y **sólo si `res != 0`** escribe `[bp-6] = 0`. La cola común
     * `0x11a6` imprime por `[bp-0xa]`: `== 1` → "Success!" · `== 0` → "Failed!" + beep ·
     * **cualquier otra cosa → NADA**. Como Vas Rel Por devuelve `-1` al acertar, el éxito
     * es MUDO; y `[bp-6]` (el valor que `cmd_cast` retorna, `0x11d6`) pasa de su 1 por
     * defecto a 0. O sea: el `[bp-6]=0` no es «el hechizo falló» sino «el hechizo TE MOVIÓ
     * DE SITIO» — el llamador no debe seguir tratando el turno como uno normal. El port no
     * modela ese retorno (no tiene la cola del despachador como valor), así que lo que se
     * calca es lo observable: éxito SILENCIOSO, fracaso "Failed!".
     *
     * El gate del barco (`0x0cf9 and al,0xf0` / `0x0cfb cmp al,0x20`) cubre la banda
     * `0x20`-`0x2F` = fragata (0x20-0x27) + esquife (0x28-0x2B) + nave NPC (0x2C-0x2F): a
     * pie (0x1C), a caballo (0x10) y en alfombra (0x14) SÍ se lanza. Y va ANTES del prompt:
     * a bordo el hechizo no llega ni a preguntar.
     */
    const castGateTravel = (): void => {
      // 0x0cf6-0x0cfd. `?? 0x1c` = a pie, el mismo default que usa el resto de main.ts.
      if (((game.state.transportTile ?? 0x1c) & 0xf0) === 0x20) {
        hud.message("Failed!"); // cola 0x11a6 con res=0 (DS 0x4660)
        return;
      }
      // El prompt lo imprime `print_string` (0x0cff → kernel 0x1850), NO el eco del
      // despachador: fila PLANA, sin bullet ► y sin abrir grupo (careo #341 contra el
      // único testigo de vídeo de los tres corpus — Lord Fenton lf29 t≈1658 / lf30
      // t≈374·518·535 / lf31 t≈1256, cinco casts: «:VAS REL POR» y «To phase: N»
      // consecutivos, sin ► y sin línea en blanco entre ellos). `echoCursor` es la fila
      // de eco viva marcada `cont` (la misma del «:» de Yell/Talk): `echoSetLast` ecoa
      // la tecla DETRÁS y la piel no le pone bullet (console.ts:115).
      // `t()` aquí porque `echoCursor` no pasa por el choke de pushConsole, y el MISMO
      // texto va de `prefix`: antes el prefix crudo (EN) pisaba la fila traducida al
      // ecoar la tecla en ES.
      const gatePrompt = t(GATE_TRAVEL_PROMPT); // DS 0x45e7, sin '\n': la tecla se ecoa DETRÁS
      hud.echoCursor(gatePrompt);
      prompts.current = {
        type: "getkey",
        prefix: gatePrompt,
        resolve: (key) => {
          refreshAwaiting();
          // 0x0d1d/0x0d23: la banda es '1'..'8' EXACTA y no hay reintento — cualquier otra
          // tecla (incluido ESC) cae al mismo `sub ax,ax` de 0x0d46.
          if (key.length !== 1 || key < "1" || key > "8") {
            hud.message("Failed!");
            return;
          }
          const phase = key.charCodeAt(0) - 0x31; // 0x0d29 `sub byte [bp-2], 0x31`
          // …y la CEREMONIA que ese jingle trae consigo. El literal 8 es el de `0x0d2d
          // mov ax, 8`, y el SITIO es éste y no el (C)ast: `CAST.OVL:0x0cf0` sólo la dispara
          // tras el gate `'1'..'8'` EXACTO (0x0d1d/0x0d23); las tres salidas tempranas caen
          // en `0x0d46 sub ax,ax` y se la saltan. Por eso `emitCastCeremony` excluye el 46.
          emitCeremony(VAS_REL_POR_PHASE_CEREMONY_INDEX);
          const events: GameEvent[] = [];
          if (!game.moonstoneTeleport(phase, events)) {
            hud.message("Failed!"); // 0x4804: esa piedra la llevas encima
            return;
          }
          applyEvents(events); // éxito: res=-1 ⇒ la cola 0x11a6 NO imprime nada
        },
      };
      refreshAwaiting();
    };

    const doCast = (): void => {
      hud.echo(CMD_STRINGS.cast); // "Cast...\n" (DS 0xa142): el dispatcher lo ecoa al pulsar C (como en combate 1213); faltaba fuera de combate
      // Caster-select del (C)ast fuera de combate (CAST:0x0dd5 → gate 0x8a08/0x4988,
      // antes de "Spell name:" en 0x0de2). Con jugador ACTIVO va DIRECTO al hechizo (sin
      // "Player:"); si no, auto-elige el único elegible o pregunta con ►Select:◄. El
      // "Cast & who?" previo era FABRICADO (ver `pickCaster`). Task #78-activo.
      pickCaster((charIdx) => {
        const caster = game.state.characters[charIdx]!;
        // "Spell name:" (DS 0x4603) → nombre TECLEADO por iniciales rúnicas (CAST
        // 0x0de9 `call 0xffffc10e` → CAST2 0x00de). NO se pre-filtra por "mezclados":
        // el original deja teclear CUALQUIER hechizo y luego castSpell aplica el gate
        // "None mixed!" (CAST 0x0ebb). Vacío/ESC → "None!" (DS 0x4611, ret -1); nombre
        // que no es hechizo → "No effect!" (DS 0x4618, ret -2).
        pickSpellForCast(tf("Spell name: "), (initials) => {
          if (initials === "") { hud.message("None!"); return; }
          const idx = matchSpellByInitials(spellDefs, initials);
          if (idx < 0) { hud.message("No effect!"); return; }
          const def = spellDefs[idx]!;
          const r = castSpell(game.state, caster, def, { location: game.state.position.location, inCombat: false }, castRng);
          if (r.message) hud.message(r.message); // éxito silencioso: NO empujar línea vacía (los fallos sí traen su mensaje)
          // TAIL común del Cast (CAST.OVL 0x11a6, result=0 → "Failed!" DS 0x4660): los gates de
          // maná/nivel consumen (r.consumed) y caen al tail — el de maná tras su "M.P. too low!".
          if (!r.ok && r.consumed) hud.message("Failed!");
          // Conjuro efectivamente lanzado: el barrido base de casting del speaker
          // (§3.5, 0x4368). Sólo suena si pasó los gates (r.ok) — con entrada tecleada
          // ya puede llegar un hechizo NO mezclado ("None mixed!"), que no castea.
          // CEREMONIA del (C)ast — la MISMA `CAST2:0x0000` del pergamino de tiempo, con el
          // CÍRCULO por índice (CAST.OVL 0x0e0a-0x0e14). Es el 85 % de las inversiones de
          // paleta del corpus de vídeo y el port no disparaba NINGUNA: careo denso propio
          // sobre VAS LOR y MANI da separación 0,1 (SIN BIMODALIDAD) contra 182,5 del An Tym.
          if (r.ok) emitCastCeremony(def.index);
          const fx = r.effect;
          if (!fx) return;
          // Los efectos globales (luz, estado temporal, viento, comida) ya
          // los aplicó castSpell sobre game.state. Aquí sólo resolvemos los
          // que requieren elegir un PJ objetivo (picker del original).
          if (fx.kind === "light") {
            // In Lor / Vas Lor: éxito SILENCIOSO (CAST 0x0f20, tail 0x11a6 flag 0xffff → no
            // imprime). "A light surrounds thee!" era FABRICADO (lote cast-echo).
          } else if (fx.kind === "healTarget") {
            // Target-select «On who: » (CAST2 0x9e); resultado GENÉRICO "Success!"/"Failed!".
            pickCastTarget((idx) => {
              const m = game.state.characters[idx]!;
              const healed = fx.mode === "full" ? applyVasMani(m) : applyMani(m, castRng) > 0;
              hud.message(healed ? "Success!" : "Failed!"); // DS 0x4656 / 0x4660
            });
          } else if (fx.kind === "cure") {
            pickCastTarget((idx) => {
              const m = game.state.characters[idx]!;
              hud.message(applyCure(m) ? "Success!" : "Failed!");
            });
          } else if (fx.kind === "awaken") {
            pickCastTarget((idx) => {
              const m = game.state.characters[idx]!;
              hud.message(applyAwaken(m) ? "Success!" : "Failed!");
            });
          } else if (fx.kind === "resurrect") {
            // In Mani Corp FUERA DE COMBATE. Faltaba entera: el hechizo se consumía y el
            // maná se cobraba (`castSpell` ya devuelve el efecto, `cast.ts` case 42) y
            // aquí no había rama ⇒ ni prompt ni resurrección (medido en vivo: 99→98
            // hechizos, 30→22 P.M., compañero seguido muerto).
            // El binario: entrada 42 de la jump table `CAST.OVL:0x1146` (word CS 0xd06c
            // − base 0xBF80) = handler `CAST.OVL:0x10ec`, tres instrucciones:
            //   10ec: e8bbb0  call 0xffffc1aa   ; = stub 0x812a → CAST2.OVL:0x009e
            //                                   ;   el MISMO «On who: » de heal/cure/awaken
            //   10ef: 50      push ax           ; ← índice elegido
            //   10f0: 2bc0    sub ax,ax / push  ; ← flag verboso = 0
            //   10f3: e880ae  call 0xffffbf76   ; = stub 0x7ef6 → CAST2.OVL:0x05e0
            //   10f6: 8946f6  mov [bp-0xa], ax  ; ← código de resultado
            //   10fc: e9a700  jmp 0x11a6        ; ← TAIL común: 1 → "Success!" (DS 0x4656)
            //                                   ;               0 → "Failed!"  (DS 0x4660)
            // El flag 0 es lo que separa esta vía de la del PERGAMINO (`CAST.OVL:0x12e6`,
            // que empuja **1**): con 1, `CAST2:0x05e0` @0x060a imprime DS 0x953c =
            // b'Not dead!\n' cuando el objetivo no está muerto; con 0 calla y sólo habla
            // el tail. Por eso aquí NO va "Not dead!" ni "Resurrection!" (DS 0x46d2, del
            // lector de pergaminos): sólo el Success!/Failed! genérico.
            // Ventana temporal: DS 0x1C90[42] = 0x0e = exterior|pueblo|mazmorra (SIN el
            // bit de combate) ⇒ ésta es precisamente la vía viva del hechizo.
            pickCastTarget((idx) => {
              const m = game.state.characters[idx]!;
              hud.message(applyResurrect(m, game.state.karma) ? "Success!" : "Failed!");
            });
          } else if (fx.kind === "gateTravel") {
            castGateTravel();
          } else if (fx.kind === "peer") {
            // In Wis (#319, acta §3 — CAST2.OVL:0x06ec): coordenadas de SEXTANTE del
            // grupo, Y antes que X, nibbles como letras 'A'..'P'. La fila entera va en
            // FUENTE 1 (rúnica, text_set_font 0x1c9e) y tras volver a IBM el binario
            // sólo imprime LF ⇒ el `rune` por fila de pushConsole basta (medicion-364c).
            // Ventana temporal 0x08 = sólo EXTERIOR (TIME_PERMITTED_BITS[9]), así que
            // position.x/y son g_party_x/y del mapa grande. Éxito sin "Success!": el
            // handler no toca el código de resultado (ret 0x0767) y la cola 0x11a6 calla.
            hud.message(inWisPeerText(game.state.position.x, game.state.position.y), true);
          } else if (fx.kind === "deathVision") {
            // Wis An Ylem (#319, acta §5 — CAST2.OVL:0x046c): VEINTE fotogramas de
            // ventana sin censura de visibilidad (vis_buffer_build con radio −1 =
            // flood saltado, búfer 11×11 todo 0xFF) y restaura. La mecánica y las
            // divergencias declaradas (modalidad del input, tick 0x4552 con RNG no
            // portado) viven en CoreViewImpl.revealViewport/visField.
            view.revealViewport(DEATH_VISION_FRAMES * PAUSE_UNIT_MS);
          } else if (fx.kind === "sealDoor") {
            // An Ex Por: SELLA una puerta (CAST.OVL 0x1020 → sub 0x846). El original
            // pide dirección (getdir) y magifica la puerta cerrada-normal enfrente
            // (0xB8→0x97). NO consume skull key. (In Ex Por #26 ya NO abre puertas —
            // es getdir + animación; las skull doors se abren con (U)se Skull Key.
            // Task #22.)
            pendingCastDoor = fx;
            hud.message("Seal -- which way?");
          } else if (fx.kind === "disarmOrOpen") {
            // An Sanct: getdir y luego `dec` sobre la puerta con cerrojo.
            pendingCastUnlock = true;
          } else if (fx.kind === "blink") {
            // In Por de EXTERIOR (#182 D2): getdir y teleporte por el rayo hasta la
            // ÚLTIMA hierba de la ventana de chunks. SILENCIOSO como el muro de campo:
            // la rama de 0x0680 no imprime prompt propio (el «Direction-» lo pone el
            // getdir de CAST2 0x0306, que aquí es el cursor de dirección) y ni el
            // acierto ni el fallo tienen mensaje.
            pendingCastBlink = true;
          }
        });
      });
    };

    // (C)ast DENTRO de la mazmorra 3D. En el original el dispatcher de mazmorra
    // (DUNGEON:0x06C4) reenvía las teclas que no maneja al kernel_cmd_dispatch
    // (0x3178), que incluye Cast; la ventana temporal (bit 0x02 mazmorra) filtra qué
    // hechizos valen aquí. `castSpell` corre el dispatcher exacto (mismos gates) y
    // devuelve el efecto; los específicos de mazmorra los aplica el motor:
    //  - light (In Lor/Vas Lor): fija `lightSpellMins` → la vista 3D pasa a 4 celdas
    //    de profundidad (dungeon/light.ts, DUNGEON:0x1AD6/0x1B0C).
    //  - Uus/Des Por: `dungeonMagicChangeLevel(∓1)` (DUNGEON:0x1C6A, mode=1).
    //  - curación/cura/despierta: pickers como el (C)ast de overworld.
    const doDungeonCast = (): void => {
      hud.echo(CMD_STRINGS.cast); // "Cast...\n" (DS 0xa142): eco del dispatcher al pulsar C (faltaba en mazmorra)
      // Mismo caster-select con gate del activo que el (C)ast de overworld (ver doCast /
      // pickCaster): jugador activo → directo; si no, auto-único o ►Select:◄.
      pickCaster((charIdx) => {
        const caster = game.state.characters[charIdx]!;
        pickSpellForCast(tf("Spell name: "), (initials) => {
          if (initials === "") { hud.message("None!"); return; }
          const idx = matchSpellByInitials(spellDefs, initials);
          if (idx < 0) { hud.message("No effect!"); return; }
          const def = spellDefs[idx]!;
          // La VENTANA TEMPORAL necesita la location de MAZMORRA (0x21..0x28): al entrar
          // a la mazmorra el port NO cambia game.state.position.location (sigue 0), la
          // mazmorra vive en game.dungeonState. En el binario g_location SÍ es 0x21..0x28
          // aquí, así que se pasa pos.dungeon (=33..40) para que el bit 0x02 (mazmorra)
          // habilite Uus/Des Por y compañía. Sin esto Uus/Des Por darían "Not here!".
          const dungeonLoc = game.dungeonState?.pos.dungeon ?? game.state.position.location;
          const r = castSpell(game.state, caster, def, { location: dungeonLoc, inCombat: false }, castRng);
          if (r.message) hud.message(r.message); // fallos con línea propia (Not here!/None mixed!/M.P. too low!)
          // TAIL común del Cast (CAST.OVL 0x11a6, result=0 → "Failed!" DS 0x4660): los gates de
          // maná/nivel consumen (r.consumed) y caen al tail — el de maná tras su "M.P. too low!".
          if (!r.ok && r.consumed) hud.message("Failed!");
          // CEREMONIA del (C)ast en MAZMORRA — misma rutina y mismo índice-círculo. Las dos
          // ramas de abajo ya citaban sus literales («el jingle 4 …», «el jingle 2 …») y decían
          // que los cubría la fanfarria: los cubre ESTO, y el índice derivado del círculo sale
          // igual al literal que ellas leyeron del asm — dos vías al mismo número.
          if (r.ok) emitCastCeremony(def.index);
          const fx = r.effect;
          if (!fx) return;
          if (fx.kind === "fieldWall") {
            // In Flam/Nox/Zu/Sanct Grav (#14/15/16/20) — CAST.OVL `cast_field_wall`
            // 0x004c, rama `g_location < 0x80`: siembra el campo en la celda de ENFRENTE
            // de la rejilla 8×8 de mazmorra, conservando el bit 3 (`00c6 & 8` + `00ce or`).
            // Aquí, y NO en el (C)ast de exterior/pueblo: la máscara DS:0x1C90 de los
            // cuatro vale 0x03 = mazmorra+combate, medida en el volcado estático Y en la
            // RAM viva (`re/notes/field-grav-gate-testigo-20260808.md`). NO hay getdir:
            // el binario usa la orientación ya fijada (`0071 g_dng_facing`).
            // Silencioso en éxito (res=0xFFFF ⇒ la cola 0x11a6 no imprime); sólo el
            // fallo habla, y ese "Failed!" lo emite `applyFieldWall`.
            applyEvents(game.applyDungeonFieldWall(fx.fieldTile));
          } else if (fx.kind === "dispelField") {
            // An Grav (#319, acta §4 — CAST2.OVL:0x07bc, rama g_location<0x80): disipa
            // el campo BAJO el grupo o, si no lo hay, el de ENFRENTE (deltas por facing
            // con `&7` en los dos ejes), conservando el bit iluminado (`and [bx],8`).
            // "Field destroyed!" en éxito (cola muda, res=0xFFFF) / "Failed!" en fallo.
            // El jingle 4 del binario (`push 4 → CAST2:0x0000`) lo cubre el
            // `emitCastCeremony` genérico de arriba: An Grav es hechizo 18 y su círculo es 4,
            // así que el índice derivado COINCIDE con el literal de la rama.
            // Sin dungeonSpellTurn: como su gemela fieldWall (la rutina no toca 24e6).
            applyEvents(game.applyAnGravDispel());
          } else if (fx.kind === "disarmOrOpen") {
            // An Sanct (#286 — CAST.OVL:0x02d2, rama 0x20<loc<0x80, cuerpo 0x02ee-0x0395):
            // abre el COFRE de la celda propia o, si ahí no hay, el de ENFRENTE (deltas
            // por facing con `&7` en los dos ejes) — `(tile&8)|0x70`, trampa y cerradura
            // fuera sin tirada. "Disarmed!" (DS 0x45a1, si bit 0) + "Chest opened!"
            // (DS 0x45ac) en éxito (cola muda, res=0xFFFF) / "Failed!" en fallo. El
            // jingle 2 del binario (`push 2 → CAST2:0x0000` @0x02ee, a la ENTRADA de la
            // rama) lo cubre el `emitCastCeremony` de arriba: An Sanct es hechizo 6, círculo
            // 2, y el índice derivado COINCIDE con el literal de la rama. Sin
            // dungeonSpellTurn: la rama no toca 24e6 (el `or ,2` es de la rama de puerta).
            // Antes de #286 este descriptor caía al `else` final (turno y nada más).
            applyEvents(game.applyAnSanctOpenChest());
          } else if (fx.kind === "dungeonAscend") {
            applyEvents(game.dungeonMagicChangeLevel(-1));
          } else if (fx.kind === "dungeonDescend") {
            applyEvents(game.dungeonMagicChangeLevel(1));
          } else if (fx.kind === "light") {
            // In Lor / Vas Lor: silencioso (tail 0x11a6). "A light surrounds thee!" fabricado.
            applyEvents(game.dungeonSpellTurn()); // la luz ya está en el estado; cobra el turno
          } else if (fx.kind === "healTarget") {
            applyEvents(game.dungeonSpellTurn());
            pickCastTarget((i) => {
              const m = game.state.characters[i]!;
              const healed = fx.mode === "full" ? applyVasMani(m) : applyMani(m, castRng) > 0;
              hud.message(healed ? "Success!" : "Failed!"); // DS 0x4656 / 0x4660
            });
          } else if (fx.kind === "cure") {
            applyEvents(game.dungeonSpellTurn());
            pickCastTarget((i) => {
              const m = game.state.characters[i]!;
              hud.message(applyCure(m) ? "Success!" : "Failed!");
            });
          } else if (fx.kind === "awaken") {
            applyEvents(game.dungeonSpellTurn());
            pickCastTarget((i) => {
              const m = game.state.characters[i]!;
              hud.message(applyAwaken(m) ? "Success!" : "Failed!");
            });
          } else if (fx.kind === "resurrect") {
            // In Mani Corp TAMBIÉN aquí: DS 0x1C90[42] = 0x0e lleva el bit 0x02 (mazmorra),
            // y el handler `CAST.OVL:0x10ec` es UNO SOLO — no tiene rama por `g_location`
            // (a diferencia de `cast_field_wall` 0x004c o `An Sanct` 0x02d2, que sí la
            // tienen). Antes caía al `else` de abajo: turno cobrado y NADA más.
            applyEvents(game.dungeonSpellTurn());
            pickCastTarget((i) => {
              const m = game.state.characters[i]!;
              hud.message(applyResurrect(m, game.state.karma) ? "Success!" : "Failed!");
            });
          } else {
            // Resto de efectos permitidos en mazmorra (muros de campo, etc.): el
            // descriptor no tiene aplicador de mazmorra en el port; sólo se cobra
            // el turno (el mensaje del hechizo ya se pintó).
            applyEvents(game.dungeonSpellTurn());
          }
        });
      });
    };

    // Nombre visible de un ítem de armamento en el PICKER: la tabla de nombres CORTOS
    // fiel del binario (DATA.OVL DS 0x1962, `shortEquipNames.json`), NO los nombres
    // largos de InventoryDetails. El original NUNCA trunca — usa formas abreviadas que
    // caben en las 10 celdas del campo ("Cloth", "Chain", "Flame Oil", "Long Sword").
    // El port antes pintaba los largos de InventoryDetails truncados a lo bruto ("Cloth
    // Armo", "Flaming Oi") = infidelidad de fuente. (El banner de combate "armed with"
    // sigue con EQUIP_NAMES: es otra superficie, otra tabla — fuera de este arreglo.)
    const SHORT_EQUIP_NAMES = shortEquipNamesJson.names;
    const readyItemName = (id: number): string => SHORT_EQUIP_NAMES[id] ?? `Item ${id}`;

    // (Re)construye la vista del picker de Ready (fase `pick`) para el PJ elegido: filas
    // = ítems poseídos O equipados, en orden de id (`find_next_owned` con charIdx incluye
    // los equipados de cuenta 0). El banner es el nombre del PJ (cmd_ready @0x12e8).
    const readyPickView = (charIdx: number, model: ReadyPickerModel) => {
      const char = game.state.characters[charIdx]!;
      const rows = buildReadyRows({
        qtyOf: (id) => game.state.equipmentQuantities[id] ?? 0,
        isEquipped: (id) => isItemEquipped(char, id),
        nameOf: readyItemName,
      });
      return {
        phase: "pick" as const,
        title: effectiveName(char.name),
        rows,
        cursor: Math.min(model.cursor, Math.max(0, rows.length - 1)),
        scroll: model.scroll,
      };
    };

    // Abre el OVERLAY de pergamino del picker (`item_page_controller` @0x0f2e, modo 'R')
    // para `charIdx`: imprime "Item: " (DS 0x9998) y arma un prompt `ready-picker` que
    // conduce el cursor + equipa in situ. La MECÁNICA de equipar (encumbrance, ammo,
    // ring 1/16) es de `game.readyItem` (Task 3.12); aquí sólo va la PRESENTACIÓN.
    const openReadyPicker = (charIdx: number): void => {
      let model = initReadyPicker();
      const rowsNow = () => readyPickView(charIdx, model).rows;
      // Sin ítems equipables → "Thou art empty-handed!" (DS 0x997e, cmd_ready @0x12c3):
      // NO se abre el picker.
      if (rowsNow().length === 0) {
        hud.message(READY_UI.empty);
        view.setReadyPicker(null);
        prompts.current = null;
        refreshAwaiting();
        return;
      }
      hud.message(READY_UI.item); // "Item: " — fila de mensaje viva (la ola cae al final)
      const publish = (): void => view.setReadyPicker(readyPickView(charIdx, model));
      const close = (withDone: boolean): void => {
        if (withDone) hud.messageAppend(READY_UI.done); // "Item: " → "Item: Done" (@0x123e)
        view.setReadyPicker(null);
        prompts.current = null;
        refreshAwaiting();
      };
      const onKey = (key: string): void => {
        const rows = rowsNow();
        const act = readyPickerKey(model, key, rows.length);
        if (act.kind === "move") {
          model = act.model;
          publish();
        } else if (act.kind === "equip") {
          const row = rows[act.index];
          if (!row) return;
          // Equipar/desequipar por el motor (enruta el "Ring vanishes!" 1/16 por el
          // stream vivo). Éxito = "" (F1.9): no imprime nada, sólo redibuja la fila.
          const r = game.readyItem(charIdx, row.equipId);
          if (r.vanished) {
            // Ring vanishes! (@0x0e01): el original imprime el mensaje y el controller
            // SALE (try_equip devuelve 1), SIN "Done" (no pasa por el path de ESC).
            if (r.message) hud.message(r.message);
            view.emitSfx({ id: "ring-vanishes" });
            close(false);
            return;
          }
          // Rechazo (fuerza/manos/munición/ocupado): el original NO lo imprime pelado —
          // pasa por la envoltura ZSTATS 0x0bee, que lo enmarca en líneas en blanco y
          // RE-IMPRIME el prompt "Item: " (el picker sigue abierto).
          for (const ln of readyRejectLines(r.message)) hud.message(ln);
          publish(); // el picker sigue abierto; la fila del cursor se redibuja
        } else if (act.kind === "close") {
          close(true);
        }
        // kind "none": el modal traga la tecla (getkey re-lee)
      };
      prompts.current = { type: "ready-picker", onKey };
      publish();
      refreshAwaiting();
    };

    const doReady = (): void => {
      // ECO DEL COMANDO — lo imprime el DISPATCHER DEL KERNEL, no el handler: ULTIMA.EXE
      // 0x339a `mov ax,0xa1f0; push; call 0x1850` y SÓLO DESPUÉS `call 0x7e4e`
      // (dispatch_table.py: R @339a 'Ready...' → ZSTATS.OVL:0x1296, stub 0x7e4e). Los «...»
      // son bytes de la cadena (DS 0xa1f0 = "Ready...\n\n"). El clon lo emitía SÓLO en
      // combate (COMBAT.OVL 0x09a2, su copia propia DS 0x6e2e = mismos bytes) y se lo
      // dejaba en overworld/pueblo/mazmorra — el usuario lo reportó con capturas (">Ready...").
      hud.echo(CMD_STRINGS.ready);
      // Fase 1: selección de jugador (`resolve_display_char` @0x0000). El banner del
      // panel pasa a ►Select:◄ (fase `select` del picker) y el prompt de consola es
      // "Player: " (DS 0x96b4); al elegir se le AÑADE el nombre ("Player: Elwood"). El
      // cursor va sobre el ROSTER en vídeo inverso (mismo `select_party_member` que
      // Camp/Cast). Party de 1 → el picker arranca con el único miembro bajo el cursor.
      view.setReadyPicker({ phase: "select", title: "Select:", rows: [], cursor: 0, scroll: 0 });
      pickMember(READY_UI.player, (charIdx) => {
        // Elegido: "Player: " + nombre en la MISMA fila (resolve_display_char @0x004a).
        hud.messageAppend(effectiveName(game.state.characters[charIdx]?.name));
        openReadyPicker(charIdx);
      }, () => {
        // ESC en la selección → "None!" (DS 0x96be, ret -1) y se cierra sin picker.
        // messageAppend NO pasa por el choke t() (continúa fila viva) → traducir aquí
        // ("None!" es key approved) para que bajo 'es' no filtre "Jugador: None!".
        hud.messageAppend(t(READY_UI.none));
        view.setReadyPicker(null);
        refreshAwaiting();
      });
    };

    /** Comandos de una letra que piden dirección ("Which way?"), como el original. */
    let pendingDirCommand: "open" | "look" | "talk" | "get" | "fire" | "search" | "jimmy" | "klimb" | "push" | "attack" | null = null;
    // Dirección pendiente del hechizo de sellado An Ex Por (sealDoor), que pide
    // dirección tras el picker de Cast (task #15/#22). Espejo de pendingDirCommand.
    let pendingCastDoor: { kind: "sealDoor" } | null = null;
    // In Por FUERA DE COMBATE (#182 D2): la rama de EXTERIOR de CAST.OVL 0x05DC pide
    // dirección (0x0680 → CAST2 0x0306 «Direction-») y luego teleporta por el rayo
    // (game.applyBlinkSpell). Espejo de pendingCastDoor.
    let pendingCastBlink = false;
    let pendingCastUnlock = false;
    // An Sanct (disarmOrOpen) FUERA DE COMBATE: la rama no-mazmorra de CAST.OVL 0x02d2
    // pide dirección (0x0398 → CAST2 0x0306) y QUITA EL CERROJO de la puerta apuntada
    // (0xB9→0xB8 / 0xBB→0xBA, `dec byte [bx]` @0x03c8) — NO la abre. Espejo de
    // pendingCastDoor. Antes de esto el efecto se producía y NADIE lo consumía fuera de
    // combate (único consumidor: combat.ts, y allí declarado no-op) ⇒ An Sanct no hacía
    // nada en exterior/pueblo/mazmorra.
    // (U)se Skull Key pide dirección tras seleccionar el item en el picker (getdir,
    // CAST.OVL 0x18dd). Espejo de pendingCastDoor. Task #22.
    let pendingUseSkullKey = false;
    // (U)se → pergamino Rel Hur: getdir tras "Wind change!" → set_wind si overworld
    // (loc<0x21). Espejo de pendingUseSkullKey. Ver re/notes/potions-scrolls.md (scroll 1).
    let pendingScrollWind: { location: number } | null = null;
    // Prompts Y/N/dígito del original (getkey crudo, F1.3): `prompts.current` se
    // declara arriba (gate del cursor F-G). Dirigido por GameEvents de prompt y
    // resuelto con métodos de re-entrada de `game`. Precedente: dungeon-word-prompt.

    /**
     * Eco fiel del handler de (U)se: el binario NO ecoa el NOMBRE del ítem elegido (a
     * diferencia de Ready) — el item_page_controller en modo 'U' (@0x1230) sólo cierra y
     * devuelve el id; cada handler imprime su PROPIA cabecera ("Scroll"/"Potion"/"Carpet"/
     * "Moonstone"…) SIN salto tras "Item: " (0x48b1), así que continúa esa fila. Esta
     * ayuda toma los eventos del verbo, APENDA la primera línea de mensaje (la cabecera) a
     * la fila "Item: " viva y aplica el RESTO como filas nuevas. Elimina el doble eco del
     * nombre (era "Item: Vas Lor"/"Scroll" y "Item: Carpet"/"Carpet"; ahora "Item: Scroll"
     * / "Item: Carpet"). Ver re/notes/use-merchants.md §Use.
     */
    const useEcho = (events: ReturnType<Game["move"]>): void => {
      const i = events.findIndex((e) => e.kind === "message" && !!e.text);
      if (i < 0) {
        applyEvents(events);
        return;
      }
      hud.messageAppend((events[i] as { text: string }).text); // cabecera continúa "Item: "
      applyEvents([...events.slice(0, i), ...events.slice(i + 1)]);
    };

    /**
     * Abre el OVERLAY de pergamino del picker de (U)se (`item_page_controller` @0x0f2e,
     * modo 'U'). REUSA la maquinaria de Ready (#78): el renderer de la piel fiel
     * (`skin/fiel/ready.ts`) y el canal `view.setReadyPicker` — el picker de Use es EL
     * MISMO pergamino, sólo cambia la tabla (extendida) y que ENTER USA+CIERRA en vez de
     * equipar in situ (@0x1230). No hay fase de selección de jugador: el binario llama al
     * controller con charIdx=0xff (ver re/notes/use-merchants.md §Use). El despacho de
     * cada verbo es `game.useXxx()` (Task #52/#22) / `useShard` (shards). Popup DOM =
     * sólo piel dev (patrón 58f76be); aquí va la presentación fiel.
     */
    const openUsePicker = (): void => {
      const rows: UsePickerRow[] = buildUseRows(game.state);
      // Vacío → "No usable items!" (DS 0x489f) SIN abrir el picker (find_next_owned
      // @0x05a4 devolvió 0xffff, CAST 0x17bd).
      if (rows.length === 0) {
        hud.message(USE_UI.noItems);
        return;
      }
      let model: UsePickerModel = initUsePicker();
      // Vista para el renderer de Ready: filas sin glifo de equipo (Use no equipa);
      // `qty>0` se pinta a 2 díg., `qty=0` es un flag booleano (Class C, ver usePicker.ts).
      const toView = (): unknown => ({
        phase: "pick" as const,
        // Banner del panel = "Items:" (DS 0x48b8), lo que empuja `cmd_use_item`
        // @0x17c8 a la rutina de cabecera. NO es "Use item" — ése es el eco del
        // DISPATCHER en la consola (CMD_STRINGS.use, DS 0xa24c), que ya se emitió y
        // sigue visible bajo el panel. Divergencia D2 del careo side-by-side.
        // t() como los demás banners que fija el shell (`t("Arms")` 0x3257,
        // `t(MIX_UI.reagents)` 0x3735): el canal `title` se blitea CRUDO en skin.ts
        // porque también transporta el NOMBRE del PJ en Ready, que no se traduce.
        title: t(USE_UI.banner),
        rows: rows.map((r) => ({ name: r.name, qty: r.qty, equipped: false, glyph: 0 })),
        cursor: model.cursor,
        scroll: model.scroll,
        variant: "use" as const,
      });
      hud.message(USE_UI.item); // "Item: " (DS 0x48b1) — fila viva; la ola cae al final.
      const publish = (): void => view.setReadyPicker(toView() as never);
      const close = (): void => {
        view.setReadyPicker(null);
        prompts.current = null;
        refreshAwaiting();
      };
      const onKey = (key: string): void => {
        const act = usePickerKey(model, key, rows.length);
        if (act.kind === "move") {
          model = act.model;
          publish();
        } else if (act.kind === "use") {
          const row = rows[act.index];
          if (!row) return;
          // ENTER (@0x1230, modo 'U'): el picker CIERRA y devuelve el id; NO ecoa el nombre
          // (a diferencia de Ready). Cada handler imprime su cabecera, que CONTINÚA "Item: ".
          const a = row.action;
          if (a.kind === "skullKey") {
            // "Skull Key\n" (DS 0x48fe) continúa "Item: "; luego getdir (CAST.OVL 0x18dd).
            hud.messageAppend("Skull Key");
            close();
            pendingUseSkullKey = true;
            hud.message("Use Skull Key -- which way?");
            return;
          }
          if (a.kind === "scroll") {
            // Leer pergamino (CAST.OVL 0x11de). Consumo ANTES de todo (0x11ec). Sin gate de
            // maná/reagente/clase: legible por cualquiera. Ver re/notes/potions-scrolls.md.
            const idx = a.index;
            if ((game.state.scrollQuantities[idx] ?? 0) > 0) game.state.scrollQuantities[idx]!--;
            close();
            hud.messageAppend("Scroll"); // "Scroll\n\n" (DS 0x466a) → "Item: Scroll"
            const res = readScroll(game.state, idx, game.state.position.location);
            for (const m of res.messages) hud.message(m);
            // CEREMONIA del pergamino (CAST2 0x0000, XOR fn21): la piel fiel pinta la
            // inversión. 🔴 La lista cableada «2/3/7» dejaba fuera DOS entradas de la jump
            // table de pergaminos (`0x1205`, tabla en 0x1340): Vas Lor (0, `0x1218 sub ax,ax`)
            // e In Quas Wis (4, `0x1290 mov ax,4`). Ahora sale de `scrollCeremonyIndex`, que
            // lleva la tabla entera con su derivación — y donde el índice NO es el nº de
            // pergamino habría que verlo ahí, no aquí.
            // La rama "No effect!" de An Tym (loc 0x1d/0x28) NO pasa por el setter; e
            // In Quas Wis exige `g_location <= 0x7f` (0x127f), que fuera de la arena se
            // cumple siempre en esta boca (la de arena es `applyCombatScroll`).
            if (!res.messages.includes("No effect!")) emitCeremony(scrollCeremonyIndex(idx));
            const fu = res.followup;
            if (fu.kind === "windDir") {
              // Rel Hur: getdir (silencioso, señalado por el cursor) → viento si overworld.
              pendingScrollWind = { location: game.state.position.location };
            } else if (fu.kind === "resurrect") {
              // In Mani Corp: elige PJ y resucita (sin Success/Failed; el eco es "Resurrection!").
              pickCastTarget((i) => {
                applyResurrect(game.state.characters[i]!, game.state.karma);
              });
            }
            // reveal (In Quas Wis) = cosmético Clase-C; summonDaemon inalcanzable vía (U)se.
            return;
          }
          if (a.kind === "potion") {
            // Beber poción (CAST.OVL 0x135a). Consumo ANTES del target-select (0x136a):
            // cancelar el picker de PJ gasta la poción igual. Reroll (1/16 fiasco / 1/16
            // otro color) DESPUÉS del target-select (0x13a1). Ver re/notes/potions-scrolls.md.
            const color = a.color;
            if ((game.state.potionQuantities[color] ?? 0) > 0) game.state.potionQuantities[color]!--;
            close();
            hud.messageAppend("Potion"); // "Potion\n" (DS 0x4706) → "Item: Potion"
            // Target-select del trago = el MISMO «On who: » (la función CAST 0x135a
            // es el caller 0x138e de CAST2 0x9e fuera de combate; en combate
            // auto-apunta al actor del turno — openCombatUsePicker ya lo calca).
            pickCastTarget((idx) => {
              // CEREMONIA de la poción (CAST.OVL `0x139b push word ptr [bp+4]` / `0x139e
              // call`): el índice es el COLOR. 🔴 Va DENTRO del callback y ANTES del reroll,
              // que es donde está en el binario: la ceremonia sigue al target-select y sólo
              // con objetivo válido (`0x1394 or ax,ax` / `0x1396 jge 0x139b` — cancelar gasta
              // la poción y no destella), y el reroll de color es POSTERIOR (0x13a1), así que
              // lo que suena es el color PEDIDO, no el que salga.
              emitCeremony(potionCeremonyIndex(color));
              const target = game.state.characters[idx]!;
              const eff = rerollPotionColor(color, castRng);
              // g_location EFECTIVO (#123): la poción Blanca sólo revela mapa con
              // `location < 0x21` (0x1514), y dentro de la mazmorra `position.location`
              // vale 0 en el clon ⇒ el gate se cumplía EN FALSO y revelaba el mapa de
              // superficie desde el interior. `game.effectiveLocation` da 0x21..0x28 allí.
              const out = applyPotionEffect(target, eff, castRng, game.effectiveLocation);
              if (out.message) hud.message(out.message);
              // ★ #326 — BLANCA con el gate bueno (loc<0x21): revelado del mapa
              // (CAST2 0x046c). Con ok=false (aviso fiel) no hay revelado, y la
              // poción es MUDA (sin jingle: el push 6 es de Wis An Ylem).
              if (out.effectiveColor === 7 && out.ok) runMapReveal();
            });
            return;
          }
          if (a.kind === "moonstone") {
            // Enterrar la gema lunar a los pies del party (CAST.OVL 0x153c): planta su
            // moongate personal si estamos en overworld sobre tile enterrable.
            useEcho(game.useMoonstone(a.phase));
            close();
            return;
          }
          if (a.kind === "shard") {
            useEcho(game.useShard(a.which));
          } else {
            switch (a.tool) {
              case "carpet": useEcho(game.useMagicCarpet()); break;
              case "amulet": useEcho(game.useAmulet()); break;
              case "crown": useEcho(game.useCrown()); break;
              case "sceptre": useEcho(game.useSceptre()); break;
              case "spyglass": useEcho(game.useSpyglass()); break;
              case "hmsCape": useEcho(game.useHmsCape()); break;
              case "sextant": useEcho(game.useSextant()); break;
              case "pocketWatch": useEcho(game.usePocketWatch()); break;
              case "blackBadge": useEcho(game.useBlackBadge()); break;
              case "woodenBox": useEcho(game.useWoodenBox()); break;
            }
          }
          close();
        } else if (act.kind === "close") {
          // ESC (@0x1244, mode 'U'): "None!" (DS 0x9976) en la misma fila → "Item: None!".
          hud.messageAppend(USE_UI.none);
          close();
        }
        // kind "none": el modal traga la tecla (getkey re-lee).
      };
      prompts.current = { type: "ready-picker", onKey };
      publish();
      refreshAwaiting();
    };

    // (U)se DENTRO de la arena de combate (COMBAT.OVL 0x0b20 → "Use item" → combat_cmd
    // 0x544 code 5 → item picker → bebedor CAST 0x135a rama combate). A diferencia del
    // overworld, la poción la bebe el PJ ACTIVO del turno (target = g_cmb_actor, sin
    // picker de objetivo), consume el turno, y Purple/Black surten su efecto REAL de
    // combate. Alcance: pociones (el deliverable); ver re/notes/combat-use-potions.md.
    const applyCombatPotion = (charIdx: number, color: number): void => {
      const cb = game.combat;
      if (!cb) return;
      const rec = game.state.characters[charIdx];
      if (!rec) return;
      const cur = cb.combatants.find((c) => c.kind === "player" && c.charIdx === charIdx);
      // Consumo ANTES de aplicar (CAST 0x136a); "Potion\n" (DS 0x4706).
      if ((game.state.potionQuantities[color] ?? 0) > 0) game.state.potionQuantities[color]!--;
      hud.messageAppend("Potion"); // "Potion\n" (DS 0x4706) → "Item: Potion" (sin eco del color)
      // CEREMONIA de la poción EN ARENA — el mismo `0x139e`, y aquí sin cancelación posible:
      // la rama alta de `0x1375 cmp g_location, 0x7f` no pasa por «On who:», toma
      // `g_cmb_actor` y cae directa al empuje del color.
      emitCeremony(potionCeremonyIndex(color));
      // Reroll (1/16 fiasco / 1/16 otro color) + efecto, con la RNG DEL COMBATE (cb.rng,
      // stream compartido) — punto exacto 0x13a1. location 0x80 = arena (rama combate).
      const eff = rerollPotionColor(color, cb.rng);
      const out = applyPotionEffect(rec, eff, cb.rng, 0x80);
      if (out.message) hud.message(out.message);
      // Sincroniza el Combatant con el record + efectos de combate (CAST 0x14a0/0x14dc):
      if (cur) applyPotionCombatSync(cur, rec.currentHp, rec.status, out.effectiveColor, out.ok);
      combatOut(cb.playerCast(null, null)); // (U)se consume el turno del bebedor
      pumpCombat();
    };

    // (U)se de PERGAMINO en la arena (CAST.OVL 0x11de rama combate, loc>=0x80). El picker
    // del DOS es COMPLETO (scrolls + pociones); el port sólo listaba pociones. Deriva
    // re/notes/potions-scrolls.md §PERGAMINOS + CAST2 0x04c2. Comportamiento por scroll en
    // combate: los GLOBALES (Vas Lor luz 240 / In Sanct 'P'/100 / In An 'N'/20 / An Tym 'T'/20)
    // se aplican sobre state (readScroll) y el motor los lee de state.timeSpell; In Quas Wis (4)
    // e In Mani Corp (6) → "Not here!"; Rel Hur (1) → getdir SIN efecto (loc>=0x21), aquí no-op
    // (Clase-C: se omite el prompt de dirección, sin consecuencia observable); Kal Xen Corp (5)
    // → SUMMON DAEMON aliado. El scroll se consume SIEMPRE lo primero (0x11ec) y el (U)se gasta
    // el turno. La poción/­scroll la usa el PJ ACTIVO del turno (target = g_cmb_actor).
    const applyCombatScroll = (idx: number): void => {
      const cb = game.combat;
      if (!cb) return;
      if ((game.state.scrollQuantities[idx] ?? 0) > 0) game.state.scrollQuantities[idx]!--;
      hud.messageAppend("Scroll"); // "Scroll\n\n" (DS 0x466a) → "Item: Scroll" (sin eco del nombre)
      const res = readScroll(game.state, idx, 0x80); // 0x80 = arena (rama combate del lector)
      for (const m of res.messages) hud.message(m);
      // Hechizo-de-tiempo (2/3/7): jingle + inversión del viewport (CAST2 0x0000 vía
      // setter 0x08f8) — también en arena (testigo doom-n6: An Tym en sala de Doom).
      // 🔴 In Quas Wis (4) NO entra aquí aunque la tabla le dé literal: su handler 0x1278
      // gatea `g_location <= 0x7f` (0x127f) ANTES de la ceremonia, y la arena es 0x80 —
      // imprime «Not here!» y se va. Vas Lor (0) sí, que no lleva gate de location.
      if (idx !== 4) emitCeremony(scrollCeremonyIndex(idx));
      if (res.followup.kind === "summonDaemon") {
        // Kal Xen Corp EN ARENA (CAST2 0x04c2, arg 1): SIEMPRE aliado, sin contest rand30
        // (a diferencia del Cast). playerCast siembra el daemon + consume el turno.
        combatOut(cb.playerCast({ kind: "summonDaemon", alwaysAlly: true }, null));
      } else {
        // Globales ya aplicados sobre state; gates "Not here!"; Rel Hur no-op → sólo el turno.
        combatOut(cb.playerCast(null, null));
      }
      pumpCombat();
    };

    /**
     * (U)se de un NO-consumible EN LA ARENA (g_location>=0x80). El item_page_controller de
     * combate abre la MISMA tabla extendida que el overworld (CAST 0x1792); cada handler
     * corre con la location de arena. Derivado item-a-item de re/disasm/CAST.OVL.asm:
     *   · FUNCIONAN (sin gate de location) → corre el verbo del port (location-independiente):
     *     amulet 0x1908 / crown 0x193e (toggle) · watch 0x1ad4 (hora) · badge 0x1b2e (toggle)
     *     · box 0x1b58 · plans 0x1a76 (gate por transporte, no location) · shards (ritual
     *     posicional: sin Llama adyacente en la arena → no-efecto, seguro).
     *   · FALLAN por su gate de location (el verbo del port leería la loc del OVERWORLD, que
     *     en combate no cambia → desplegaría/enterraría por error): se emite el eco DERIVADO
     *     con g_location=arena: carpet 0x18be "Not here!" · spyglass 0x1a3a "Not here!" ·
     *     sextant 0x1a96 "Only outdoors!" · moonstone 0x155f "cannot be buried here!".
     *   · ESPACIALES DE ARENA (Clase-C BANCADO, cita): sceptre 0x1966 barre campos 0x70-0x7f
     *     alrededor de la celda del COMBATIENTE (no del overworld) → aquí sólo el eco de
     *     blandido, sin el barrido; skull key 0x18c4 decrementa la llave y hace getdir pero
     *     loc>=0x80 SALTA el unlock (0x18ea) → dec + eco, sin efecto.
     * Cualquier selección CONSUME el turno (como potion/scroll); ESC="None!" no.
     */
    const msg = (text: string): { kind: "message"; text: string } => ({ kind: "message", text });
    const combatUseNonConsumable = (a: UsePickerRow["action"]): void => {
      if (a.kind === "moonstone") { useEcho([msg("Moonstone"), msg("cannot be buried here!")]); return; }
      if (a.kind === "skullKey") {
        // No-op-con-coste FIEL (0x18c4): decrementa la llave + "Skull Key"; en arena
        // (loc>=0x80) el getdir NO llega al unlock (0x18ea jb saltado) → el jugador PIERDE
        // la llave sin efecto. El getdir en sí no tiene consecuencia observable (cualquier
        // dirección salta el unlock) → se omite su prompt (Clase-C); el coste y el eco son fieles.
        if ((game.state.skullKeys ?? 0) > 0) game.state.skullKeys--; // 0x18c4 dec
        useEcho([msg("Skull Key")]);
        return;
      }
      // Shard en arena: ritual posicional sin Llama adyacente → "Gem Shard…Shard of X" +
      // "No effect!" (ritual.ts SHARD_HEADER + RITUAL_NO_EFFECT, 0x4794+0x47f0). El verbo
      // real lo produce; en combate no hay Shadowlord encima → destroyed=false (seguro).
      if (a.kind === "shard") { useEcho(game.useShard(a.which)); return; }
      if (a.kind !== "tool") return; // scroll/potion se despachan por sus handlers de combate
      switch (a.tool) {
        // Location-independientes: el verbo del port es correcto en la arena.
        case "amulet": useEcho(game.useAmulet()); break;
        case "crown": useEcho(game.useCrown()); break;
        case "pocketWatch": useEcho(game.usePocketWatch()); break;
        case "blackBadge": useEcho(game.useBlackBadge()); break;
        case "woodenBox": useEcho(game.useWoodenBox()); break;
        case "hmsCape": useEcho(game.useHmsCape()); break; // gate por transporte (no location)
        // Gated por location → eco de fallo derivado (g_location=arena).
        case "carpet": useEcho([msg("Carpet"), msg("Not here!")]); break; // 0x18be
        case "spyglass": useEcho([msg("Spyglass"), msg("Not here!")]); break; // 0x1a3a
        case "sextant": useEcho([msg("Sextant"), msg("Only outdoors!")]); break; // 0x1a96
        // Cetro EN ARENA (0x1966 → barrido 0x19a5): disuelve campos (tile&0xf0==0x70) en
        // el 3×3 del COMBATIENTE ACTIVO contra la rejilla viva; "No effect!" si 0. El
        // barrido de disolución del muro (sfx de blandido) es cosmético Clase-C.
        case "sceptre": {
          const dissolved = game.combat?.sceptreDissolveFields() ?? 0;
          const evs = [msg("Sceptre"), msg("Wielding the Sceptre of Lord British...")];
          if (dissolved === 0) evs.push(msg("No effect!")); // 0x4981 (0 disueltos → 0x19fb je)
          useEcho(evs);
          break;
        }
      }
    };

    const openCombatUsePicker = (charIdx: number): void => {
      const cb = game.combat;
      if (!cb) return;
      // Picker COMPLETO del DOS en arena: el item_page_controller modo 'U' abre la tabla
      // EXTENDIDA entera (CAST 0x1792), no sólo consumibles. Los no-consumibles fallan su
      // gate o corren su efecto según el ítem (ver combatUseNonConsumable).
      const rows = buildUseRows(game.state);
      if (rows.length === 0) {
        hud.message(USE_UI.noItems); // "No usable items!" (sin abrir el picker)
        return;
      }
      let model: UsePickerModel = initUsePicker();
      const toView = (): unknown => ({
        phase: "pick" as const,
        // Banner del panel = "Items:" (DS 0x48b8), lo que empuja `cmd_use_item`
        // @0x17c8 a la rutina de cabecera. NO es "Use item" — ése es el eco del
        // DISPATCHER en la consola (CMD_STRINGS.use, DS 0xa24c), que ya se emitió y
        // sigue visible bajo el panel. Divergencia D2 del careo side-by-side.
        // t() como los demás banners que fija el shell (`t("Arms")` 0x3257,
        // `t(MIX_UI.reagents)` 0x3735): el canal `title` se blitea CRUDO en skin.ts
        // porque también transporta el NOMBRE del PJ en Ready, que no se traduce.
        title: t(USE_UI.banner),
        rows: rows.map((r) => ({ name: r.name, qty: r.qty, equipped: false, glyph: 0 })),
        cursor: model.cursor,
        scroll: model.scroll,
        variant: "use" as const,
      });
      hud.message(USE_UI.item); // "Item: "
      const publish = (): void => view.setReadyPicker(toView() as never);
      const close = (): void => {
        view.setReadyPicker(null);
        prompts.current = null;
        refreshAwaiting();
      };
      const onKey = (key: string): void => {
        const act = usePickerKey(model, key, rows.length);
        if (act.kind === "move") {
          model = act.model;
          publish();
        } else if (act.kind === "use") {
          const row = rows[act.index];
          if (!row) return;
          if (row.action.kind === "potion") {
            // Sin eco del nombre (mode 'U' @0x1230 no lo imprime); la cabecera "Potion"
            // continúa "Item: " dentro de applyCombatPotion (consume el turno).
            const color = row.action.color;
            close();
            applyCombatPotion(charIdx, color);
          } else if (row.action.kind === "scroll") {
            const idx = row.action.index;
            close();
            applyCombatScroll(idx); // consume el turno
          } else {
            // No-consumible en la arena: eco fiel + CONSUME el turno (como potion/scroll).
            close();
            combatUseNonConsumable(row.action);
            combatOut(cb.playerCast(null, null));
            pumpCombat();
          }
        } else if (act.kind === "close") {
          // ESC: "None!" y NO gasta el turno (vuelve al prompt de combate).
          hud.messageAppend(USE_UI.none);
          close();
        }
      };
      prompts.current = { type: "ready-picker", onKey };
      publish();
      refreshAwaiting();
    };

    // (R)eady DENTRO de la arena de combate (COMBAT.OVL 0x09a2 → cmd-dispatcher 0x0544
    // code 3 → picker de equipo ZSTATS 0x0f2e/0x0c5c). Diferencias con el Ready de
    // overworld (`doReady`/`openReadyPicker`):
    //   · NO hay fase de selección de jugador: actúa sobre el combatiente ACTIVO del
    //     turno (g_cmb_actor, §5 de la derivación ASM), como el (U)se de arena.
    //   · El bloqueo de armadura está ACTIVO (`readyItem(...,true)`, gate 0x0c94: ids
    //     9-15 = armadura de cuerpo tipo 0x40 → "¡No podéis cambiar de armadura en plena
    //     batalla!"). El gate del binario es g_location>0x7f (CUALQUIER mapa de combate)
    //     con victory==0, no sólo mazmorra. Cascos/escudos/armas/anillos/amuletos SÍ.
    //   · El comando CONSUME el turno al cerrar (COMBAT deja [bp-4]=1 en el path de 'R',
    //     0x083e→0x0974 sin resetearlo), ocurra o no un cambio — a diferencia del Ready
    //     de overworld, que es acción libre. Reusa el MISMO picker de pergamino fiel.
    const openCombatReadyPicker = (charIdx: number): void => {
      const cb = game.combat;
      if (!cb) return;
      let model = initReadyPicker();
      const rowsNow = () => readyPickView(charIdx, model).rows;
      // Cierra el picker y CONSUME el turno del combatiente activo (una sola vez por
      // comando 'R', cierre por Done/ESC o por "Ring vanishes!").
      const closeAndEndTurn = (withDone: boolean): void => {
        if (withDone) hud.messageAppend(READY_UI.done); // "Item: " → "Item: Done"
        view.setReadyPicker(null);
        prompts.current = null;
        refreshAwaiting();
        combatOut(cb.playerReady());
        pumpCombat();
      };
      // Sin ítems equipables → "Thou art empty-handed!" SIN abrir el picker; el turno se
      // consume igual (la 'R' ya fue la acción del combatiente; COMBAT no la revierte).
      if (rowsNow().length === 0) {
        hud.message(READY_UI.empty);
        view.setReadyPicker(null);
        combatOut(cb.playerReady());
        pumpCombat();
        return;
      }
      hud.message(READY_UI.item); // "Item: " — fila de mensaje viva
      const publish = (): void => view.setReadyPicker(readyPickView(charIdx, model));
      const onKey = (key: string): void => {
        const rows = rowsNow();
        const act = readyPickerKey(model, key, rows.length);
        if (act.kind === "move") {
          model = act.model;
          publish();
        } else if (act.kind === "equip") {
          const row = rows[act.index];
          if (!row) return;
          // Equipa/desequipa con el bloqueo de armadura activo (inCombat=true). El motor
          // resincroniza el arma/defensa del combatiente activo (game.readyItem).
          const r = game.readyItem(charIdx, row.equipId, true);
          if (r.vanished) {
            // Ring vanishes! (0x0e01): imprime el mensaje, el controller SALE (sin "Done").
            if (r.message) hud.message(r.message);
            view.emitSfx({ id: "ring-vanishes" });
            closeAndEndTurn(false);
            return;
          }
          // Rechazo (armadura/fuerza/manos/munición/ocupado): mismo picker de ZSTATS que
          // el overworld ⇒ misma envoltura 0x0bee ("\n\n" + mensaje + "\n\nItem: ").
          for (const ln of readyRejectLines(r.message)) hud.message(ln);
          publish(); // el picker sigue abierto; la fila del cursor se redibuja
        } else if (act.kind === "close") {
          closeAndEndTurn(true);
        }
        // kind "none": el modal traga la tecla (getkey re-lee)
      };
      prompts.current = { type: "ready-picker", onKey };
      publish();
      refreshAwaiting();
    };

    // (H)ole up & camp — kernel_camp_holeup 0x3C9A. Orquesta los prompts crudos
    // (getkey) sobre `prompts.current`: header → contexto → "For how many hours?"
    // (1-9, 0/Space/ESC cancelan SIN turno, 0x3eea) → "Wilt thou set a watch?"
    // (sólo con ≥2 despiertos) → picker de guardia. Sirve a overworld y mazmorra
    // (game.campContext detecta el contexto). Copys verbatim de DATA.OVL.
    // La SECUENCIA DE SUEÑO (Zzzz → reloj 1 h/tick → despertar/aparición/emboscada,
    // + la canción de Iolo si el vigía es bardo) vive en ui/camp-sleep.ts
    // (campSleepCtl, instanciado arriba); aquí queda la orquestación de prompts.

    const startCamp = (): void => {
      cancelAutoWalk();
      const ctx = game.campContext();
      // Dormir en CAMA de pueblo (CMDS 0x0552, tile 0xAB): "Hole up- " + "For how many
      // hours? " (0x4209, SIN "(1-9)") → dormir SIN watch/guardia/emboscada. Su propia
      // cabecera "Hole up-" (no "Hole up & camp!"). '0'/Space/ESC → cancelar sin turno.
      if (ctx.ok && ctx.bed) {
        hud.message("Hole up- "); // 0xa170
        hud.message("For how many hours? "); // 0x4209
        prompts.current = {
          type: "digit",
          cancelKeys: true,
          resolve: (n, key) => {
            // ⚠ ASIMETRÍA MEDIDA con el camp del kernel: aquí los tests de cancelación
            // (0x057a espacio / 0x0583 '0', ambos `jmp 0x6e8`) van ANTES del eco
            // (0x058c-0x059d), así que cancelar NO ecoa. En el camp es al revés. Son dos
            // rutinas distintas; `bedHours` guarda esa diferencia.
            const r = bedHours(n, key);
            if (r.echo) hud.messageAppend(r.echo);
            if (r.hours === 0) return; // '0'/Space → 0x6e8, sin turno y sin eco
            // #296: PACEADO, no atómico. `game.bedSleep(r.hours)` sigue existiendo (es la
            // versión de paridad/tests) pero resolvía las N horas en un frame — el reloj no
            // se veía correr y el apagón del viewport no tenía dónde durar.
            bedSleepCtl.run(r.hours);
          },
        };
        return;
      }
      // Pueblo/castillo/dwelling: el original NO imprime "Hole up & camp!" — el despachador
      // de 'H' (kernel 0x3288) desvía a la rama pueblo que imprime su propia cabecera
      // "Hole up- Only in bed!" (0xa170+0xa17a) y cobra turno. Por eso el gate va ANTES del
      // header de camp (a diferencia de los rechazos de overworld On foot!/On land or ship!,
      // que 0x3C9A SÍ imprime tras "Hole up & camp!").
      if (!ctx.ok && ctx.inTown) {
        applyEvents(game.campReject(ctx.message)); // "Hole up- Only in bed!" + turno
        return;
      }
      hud.message("Hole up & camp!\n\n"); // 0xa2c2 + 0xa306
      if (!ctx.ok) {
        applyEvents(game.campReject(ctx.message)); // rechazo → mensaje + turno (0x3ee5)
        return;
      }
      if (ctx.ship) {
        applyEvents(game.campRepairShip()); // fragata → reparar casco (0x3cf8)
        return;
      }
      // El diálogo hours→watch→guard lo modela la máquina PURA `campPrompt` (blindada por
      // camp-prompt.test.ts); aquí sólo cableamos cada fase a `prompts.current`/`hud`/
      // `runCampSleep`. El filtrado de teclas fiel (dígito/Space, Y/N) vive en prompts.current.
      driveCampPrompt(campPromptStart());
    };

    // Ctx del estado de juego para la máquina de prompts (nº despiertos + validez/nombre
    // del guardia). El guardia válido = miembro en party con status 'G' (0x3ec6 cmp 0x47).
    const campPromptCtx = (): CampPromptCtx => ({
      watchCount: game.campWatchCount(),
      partySize: game.state.partySize,
      isValidGuard: (idx) =>
        idx >= 0 && idx < game.state.partySize && game.state.characters[idx]?.status === "G",
    });

    // Sinks del driver de camp (extraído a core/campPromptDriver.ts para testear el flujo
    // Y→picker→elegir→dormir sin DOM). El driver es AUTORITATIVO sobre `prompts.current` y el
    // cursor del picker (los re-arma en cada paso); aquí sólo cableamos hud/view/sleep.
    const campDriverDeps: CampPromptDriverDeps = {
      print: (t) => hud.message(t),
      // ECO INLINE (dígito de horas y «Yes»/«No» del watch): CONTINÚA la fila del prompt,
      // que acaba en espacio sin `\n` — igual que el char crudo del peaje de trolls (:1587).
      // ⚠ Pasa por `t()` A PROPÓSITO: «Yes»/«No» SÍ están en el corpus (es.json → «Sí»/«No»)
      // y `messageAppend` NO cruza el choke de `pushConsole`, así que sin esto el castellano
      // diría «Yes». Al dígito no le afecta: `t()` devuelve la entrada si no es clave, y las
      // ÚNICAS claves de un carácter del corpus son `s` y `"` (verificado sobre es.json) —
      // nunca un dígito ni el espacio. El nombre del parámetro NO puede ser `t`: taparía al
      // `t()` importado.
      printInline: (text) => hud.messageAppend(t(text)),
      setSelectCursor: (idx) => view.setSelectCursor(idx),
      setPrompt: (p) => { prompts.current = p; },
      ctx: campPromptCtx,
      startSleep: (hours, guardIdx) => campSleepCtl.run(hours, guardIdx),
    };
    const driveCampPrompt = (step: CampPromptStep): void =>
      runCampPromptStep(step, campDriverDeps);

    // Cambio de piel en caliente — MISMO camino para F9, el botón del menú SISTEMA
    // y el switcher directo (task #79): persiste la elección y reaplica el default
    // de música del perfil destino (salvo preferencia F7 explícita, que manda).
    // dev ⇒ música enhanced; fiel y shader ⇒ speaker (sin música). Task #27/#70.
    /**
     * La piel que el JUGADOR está viendo, como id user-facing. Con el envoltorio del
     * portrait montado, `skins.currentId` es `"portrait"` — que no es una piel elegible
     * (se registra `userFacing:false`) sino un LAYOUT que aloja a la fiel. Todo lo que
     * razona sobre «qué piel hay puesta» tiene que preguntar por esto, no por el id
     * crudo: si no, se persiste una preferencia que el arranque descartará y se cicla
     * desde un punto que no está en el ciclo (diagnóstico smooth×portrait, §2.3).
     *
     * LOTE C: ya no es siempre «faithful». El envoltorio aloja la piel que el jugador
     * eligió (fiel o shader), así que la visible es la ALOJADA — preguntársela a él es lo
     * que mantiene honesto todo lo demás (persistencia, ciclo, tema del shell).
     */
    const pielVisibleId = (): string =>
      portraitSkin && skins.currentId === portraitSkin.id ?
        portraitSkin.hostedId
      : (skins.currentId ?? "faithful");

    /**
     * Pieles ofrecidas al usuario. Hasta el LOTE C esta función ANOTABA cada piel que no
     * fuera la fiel con «— sale del layout partido» (lote B): era verdad y por eso se
     * escribió. Ya NO lo es —el envoltorio aloja cualquiera de las dos—, así que la
     * anotación se retira: mantenerla sería la clase de prosa que promete una consecuencia
     * que el código dejó de tener. Las etiquetas salen intactas en los dos layouts.
     */
    const skinChoices = (): SkinChoice[] => skins.userFacingSkins;

    const afterSkinChange = (): void => {
      // Se persiste la piel VISIBLE, nunca el id del envoltorio (ver `pielVisibleId`).
      persistSkinPref(pielVisibleId());
      music.setProfileDefault(false); // fiel/shader: enhanced OFF por defecto (la piel dev, que lo tenía ON, se jubiló)
      // FASE 3: re-tematiza el shell (overlay DOM) según la piel activa, en caliente.
      // Por la piel VISIBLE, no por `currentId`: con el envoltorio montado el id crudo es
      // "portrait", y desde el lote C eso ya no implica «fiel» — el shell tiene que seguir
      // a la piel ALOJADA o el drawer se queda tematizado de la que no es.
      applyShellTheme(pielVisibleId());
      // Fuente 8×8 real en fiel: pixeliza (o revierte) el texto prominente del drawer.
      syncPixelFont(shellPanel?.rootEl, shellThemeFor(pielVisibleId()) === "faithful");
    };
    // Fallo de un swap en caliente (R4): SkinManager ya hizo ROLLBACK a la piel
    // anterior; aquí sólo se loguea y se re-sincroniza el shell con la piel viva
    // (sin .catch la promesa quedaba rechazada sin manejar y afterSkinChange no corría).
    const skinSwapFailed = (err: unknown): void => {
      console.warn("[skins] swap en caliente falló (rollback a la piel anterior):", err);
      afterSkinChange();
    };
    // F9 / botón "Cambiar piel": cicla las pieles USER-FACING (fiel↔shader; dev queda
    // fuera del ciclo tras #79) vía SkinManager.toggle().
    // `afterSkinChange` SÓLO si hubo cambio de verdad. Con el antiguo `Promise<void>` el
    // `then` corría también en la salida temprana del manager, así que un F9 que no hacía
    // nada (ciclo aún sin registrar, ver SkinManager.toggle) acababa PERSISTIENDO como
    // preferencia la piel que ya estaba montada — una piel que el jugador no eligió.
    const swapSkin = (): void => {
      // ★ CON EL ENVOLTORIO DEL PORTRAIT MONTADO, `toggle()` MIENTE (diagnóstico
      // smooth×portrait, PUNTO 1 — y es el ÚNICO camino de piel visible en móvil, porque
      // el FAB ◧ se oculta en táctil). `toggle()` busca la piel actual en el ciclo
      // user-facing; el envoltorio no está en él (`userFacing:false`), así que el
      // `indexOf` da -1 y salta SIEMPRE a la primera del ciclo: la 1ª pulsación no
      // cambiaba de piel, DESMONTABA EL LAYOUT — en silencio y sin escribir la
      // preferencia, que es exactamente lo que `selectSkin` existe para no hacer.
      // Ahora esta vía calcula el siguiente desde la piel VISIBLE y pasa por
      // `selectSkin`, o sea por la guarda declarada: si la piel destino no cabe en el
      // layout partido, se apaga la preferencia y el ▤ queda honesto.
      if (portraitSkin) {
        const ids = skins.userFacingSkins.map((c) => c.id);
        const i = ids.indexOf(pielVisibleId());
        const siguiente = ids[(i + 1) % ids.length];
        if (siguiente) selectSkin(siguiente);
        return;
      }
      void skins
        .toggle()
        .then((swapped) => {
          if (swapped) afterSkinChange();
        })
        .catch(skinSwapFailed);
    };
    /**
     * Conmuta layout PARTIDO ⇔ ORIGINAL. La preferencia ya la ha escrito el botón; aquí
     * sólo se aplica: como el partido es una piel ENVOLTORIO, cambiarlo es cambiar de piel.
     * Se recarga preservando el estado por URL cuando no hay envoltorio registrado (caso
     * «arranqué en original y ahora lo quiero partido»): montar un envoltorio que no se
     * registró al boot exigiría re-hacer el registro, y una recarga es honesta y barata.
     */
    const toggleLayoutPartido = (): void => {
      const quiere = quiereLayoutPartidoAhora();
      if (!quiere) {
        // Partido → original: se salta a la piel que el envoltorio ALOJABA, no a la fiel
        // por decreto. Antes del lote C daba igual (sólo cabía la fiel dentro); ahora
        // hardcodearla convertiría el botón de LAYOUT en un botón que además cambia de
        // piel — justo el daño colateral que el PUNTO 2 del diagnóstico documentó.
        selectSkin(pielVisibleId());
        return;
      }
      if (portraitSkin) {
        // …y al volver al partido se aloja la piel VIVA, para que el viaje de ida y vuelta
        // conserve la elección del jugador en los dos sentidos.
        void portraitSkin.setHosted(pielAlojable(skins.currentId ?? "faithful"));
        selectSkin(portraitSkin.id);
      } else window.location.reload();
    };

    /**
     * Switcher directo: salta a una piel CONCRETA por id (un click = cambio).
     *
     * EL DEFECTO QUE ARREGLA (reporte del usuario 27-07: «si a través del menú shell cambio
     * de piel, sale el menú anterior y layout anterior»): el layout partido es una piel
     * ENVOLTORIO registrada `userFacing:false`, así que elegir «fiel» en el switcher
     * desmontaba el envoltorio y con él se iban el layout, la botonera nueva y los
     * separadores — sin avisar. Ahora, con el layout partido elegido, pedir la piel que el
     * envoltorio ALOJA mantiene el envoltorio: se respeta la elección persistida.
     *
     * ★ LOTE C (encargo del usuario 28-07: «que el smooth pueda estar en layout partido;
     * deben ser posibles ambas pieles»). Donde había una GUARDA hay ahora una CAPACIDAD.
     * El límite que este bloque declaraba —«el envoltorio sólo puede alojar la FIEL, así
     * que elegir shader sale del layout partido»— dejó de existir: `PortraitSkin` recibe
     * la piel alojada por inyección y `ShaderSkin` publica los cinco miembros que el
     * composer consume (ver `skin/hostable.ts`). Elegir una piel estando en el partido ya
     * NO abandona el layout: cambia la FUENTE dentro del envoltorio, en caliente, sin
     * tocar el deck ni la preferencia de layout.
     */
    const selectSkin = (id: string): void => {
      const partido = quiereLayoutPartidoAhora() && portraitSkin !== null;
      if (partido && portraitSkin && esAlojable(id)) {
        if (skins.currentId === portraitSkin.id) {
          // Ya estamos dentro del envoltorio: intercambio EN CALIENTE de la piel alojada.
          if (portraitSkin.hostedId === id) return; // no-op benigno (clic en la activa)
          void portraitSkin
            .setHosted(pielAlojable(id))
            .then(afterSkinChange)
            .catch(skinSwapFailed);
          return;
        }
        // El partido está elegido pero no montado (venimos del clásico): se monta el
        // envoltorio YA con la piel pedida dentro, en un solo paso y sin parpadeo.
        void portraitSkin.setHosted(pielAlojable(id));
        void skins
          .swap(portraitSkin.id)
          .then((swapped) => {
            if (swapped) afterSkinChange();
          })
          .catch(skinSwapFailed);
        return;
      }
      // Piel NO alojable (ninguna hoy; la puerta queda abierta para una futura) estando en
      // el partido: se sale del layout, y como antes NO en silencio — se apaga la
      // preferencia para que el ▤ siga reflejando el estado real.
      if (partido && id !== portraitSkin!.id) guardarLayoutPartido(false);
      void skins
        .swap(id)
        .then((swapped) => {
          if (swapped) afterSkinChange();
        })
        .catch(skinSwapFailed);
    };
    // Drawer del menú SISTEMA (shell del UI externo). Se instancia tras registrar
    // las pieles (final del boot); las teclas lo consultan con optional chaining.
    let shellPanel: DebugPanel | null = null;

    /**
     * ── PRECEDENCIA ENTRE SUPERFICIES DEL SHELL (ficha #154 F7) ──────────────────────────
     *
     * DEFECTO: F10 abría el drawer SISTEMA **encima** del panel de partidas abierto. El
     * drawer va en z-index 99999 y el panel en 60, así que el resultado era dos superficies
     * del shell apiladas con la de abajo tapada y viva — se podía escribir en un campo que
     * no se veía.
     *
     * DECISIÓN, y el criterio es COHERENCIA con lo que ya había, no gusto: el shell de este
     * juego mantiene **una superficie a la vez**, y esa regla ya estaba escrita dos veces —
     * en la escalera de Escape (que cierra la de más arriba y consume la tecla, cada rama
     * con su `return`) y en el dismiss por clic-fuera (`closeOpenShellPopups`, que las cierra
     * todas). F10 era el único que la rompía. Así que F10 pasa a comportarse **exactamente
     * como Escape**: con algo del shell abierto lo CIERRA y no abre nada; con todo cerrado
     * abre el drawer. Un segundo F10 lo abre.
     *
     * Se elige esto y no «cierra el panel Y abre el drawer de un golpe» porque hace que las
     * dos teclas sean indistinguibles para quien las usa: el jugador no tiene que saber si
     * pulsó Escape o F10 para predecir qué va a pasar. Y el coste del camino largo es una
     * pulsación de más en el único caso donde el usuario ya tenía otra cosa abierta.
     */
    const shellSurfaceOpen = (): boolean =>
      savePanel.visible || selector.visible || (shellPanel?.isOpen ?? false);
    /** Cierra la superficie del shell que esté arriba. Devuelve si cerró algo. */
    const closeTopShellSurface = (): boolean => {
      if (savePanel.visible) { savePanel.hide(); return true; }
      if (selector.visible) { selector.hide(); return true; }
      if (shellPanel?.isOpen) { shellPanel.close(); syncShellExpanded(false); return true; }
      return false;
    };
    /** Alterna el drawer SISTEMA publicando su estado en el ☰ del deck (accesibilidad). */
    const toggleShellDrawer = (): void => {
      shellPanel?.toggle();
      syncShellExpanded(shellPanel?.isOpen ?? false);
    };

    // ── REGISTRO DE TECLAS (carril 2 del lanzamiento) ────────────────────────────
    // El grabador se engancha AQUÍ, en el reductor, y no al `keydown` del DOM: lo que
    // se graba es lo que el juego CONSUME. Las teclas que el reductor se traga (escena
    // modal viva, F5..F10 del shell, panel DOM abierto) se sueltan con `keyRec.drop()`
    // en su propio `return` — si se grabaran, la repetición las ejecutaría (porque a
    // otra velocidad la escena que las tragaba ya habría terminado) y divergiría.
    // Todo local: nada de esto envía nada a ningún sitio.
    const keyRec = new KeyRecorder();
    // El panel y el reproductor se montan al final del boot (necesitan `parent` y la
    // piel ya elegida); estas referencias los adelantan a las guardas de aquí arriba.
    let replayActiveRef: () => boolean = () => false;
    let replayInjectingRef: () => boolean = () => false;
    /**
     * ¿Puede el juego consumir una tecla AHORA? Es el complemento exacto de las guardas
     * de cabecera de `handleGameKey` que se tragan el input a reloj de pared, más los
     * paneles DOM que capturan el teclado. El reproductor lo consulta antes de cada
     * tecla: sin esta espera, reproducir a 4× perdería las teclas que caen dentro de
     * una acampada o de un cruce de moongate.
     */
    const inputReady = (): boolean =>
      !campSleepCtl.camping &&
      !bedSleepCtl.sleeping &&
      !refuging &&
      !revealing &&
      !trollSneakCtl.active &&
      !moongateGate.transiting &&
      !savePanel.visible &&
      !selector.visible;

    const handleGameKey = (ev: KeyboardEvent): void => {
      // La tecla queda EN VUELO: se graba en el `commit` del listener de cola, salvo
      // que alguna rama de abajo la suelte por haberla ignorado.
      keyRec.note(ev);
      // Repetición en curso: SÓLO manda el reproductor. Una tecla del usuario en mitad
      // de una repetición no es «interactuar»: es corromper la partida que se está
      // reconstruyendo, y a partir de ahí lo que se ve ya no es la partida de nadie.
      if (replayActiveRef() && !replayInjectingRef()) {
        keyRec.drop();
        ev.preventDefault();
        return;
      }
      // La party está durmiendo (secuencia de acampada): el input se traga hasta
      // que despierta. El original no acepta comandos mientras corre el reloj del
      // hold-up; el sueño sólo lo interrumpe una emboscada (que el core dispara).
      if (campSleepCtl.camping) {
        keyRec.drop();
        ev.preventDefault();
        return;
      }
      // Y lo mismo durmiendo en CAMA (#296): el bucle de CMDS 0x0634 no lee teclado —
      // sólo espera el tick, avanza el reloj y comprueba el gate de la casilla. La única
      // interrupción es «Thrown out of bed!», que dispara el core, no el jugador.
      if (bedSleepCtl.sleeping) {
        keyRec.drop();
        ev.preventDefault();
        return;
      }
      // Party-wipe: la escena de muerte+resurrección (BLCKTHRN 0x0910) corre como modal a
      // reloj de pared; el input se traga hasta que despiertas en el castillo (igual que la
      // acampada). La secuencia se auto-avanza (no la interrumpe ninguna tecla). Ver runRefugeScene.
      if (refuging) {
        keyRec.drop();
        ev.preventDefault();
        return;
      }
      // ★ #326 — Revelado de la poción blanca: el bucle de CAST2 0x04a0-0x04b8 no
      // lee teclado (20 fotogramas a reloj de int 1c); mientras se pacea, el input
      // se traga, como refuge/troll.
      if (revealing) {
        keyRec.drop();
        ev.preventDefault();
        return;
      }
      // Cruce del puente con trolls: la secuencia sneaks corre síncrona en el
      // original (los beep-delays 0x3AE6 no leen teclado); mientras la piel la
      // pacea, el input se traga — el prompt Y/N del peaje llega DESPUÉS (diferido).
      if (trollSneakCtl.active) {
        keyRec.drop();
        ev.preventDefault();
        return;
      }
      // Cruce de moongate: la secuencia scripted (disolución + cierre + eco de llegada)
      // es SÍNCRONA en el original (kernel_moongate_enter 0x48a8: bucles 0x1068/0x4912
      // con delay(2), sin leer input). Mientras la piel la corre, el input se traga —
      // moverse a mitad arrastraba la puerta de llegada con el jugador (bug testigo).
      if (moongateGate.transiting) {
        keyRec.drop();
        ev.preventDefault();
        return;
      }
      // ENDGAME (#34): el cierre es MODAL hasta el final de los tiempos — los beats de
      // TEXTO (diálogo del trono / páginas de historia) avanzan con CUALQUIER tecla
      // (getkey 0x83dc); las fases de animación y los estados terminales se la tragan
      // (freeze de victoria = bucle infinito 0x04f9; sala-prisión = idle 0x0ae7).
      if (endgamePacer.active) {
        ev.preventDefault();
        endgamePacer.consumeKey();
        return;
      }
      // ★ #294 — RITO APARCADO en una espera de tecla (CAST2 `call 0x448c` → kernel
      // `getkey_with_redraw` 0x266c): avanza con CUALQUIER tecla, que es lo que devuelve
      // esa rutina — el filtro Y/N que a veces la envuelve lo pone el LLAMADOR (0x0b6a lo
      // hace para los dígitos de la donación) y el rito no lo pone. La tecla NO se descarta
      // del grabador (`keyRec.drop()`): la consume el juego y una repetición tiene que
      // volver a darla, igual que en los beats de texto del endgame.
      if (shrineKeyPacer.active) {
        ev.preventDefault();
        shrineKeyPacer.consumeKey();
        return;
      }
      // ── Bug 2 (talk-celda-paginacion) — VOLCADO DE DIÁLOGO aparcado en una pausa del
      // guion TLK (KeyWait 0x8F → TALK 0x1010 `call 0x66ec` = getkey 0x266c; Pause 0x83 →
      // bucle 0x0f92 saltable por tecla): avanza con CUALQUIER tecla, que es lo que ambas
      // esperas leen — el KeyWait descarta el retorno del getkey y el Pause corta su bucle
      // al primer sondeo positivo. La tecla NO se descarta del grabador (como el rito y
      // los beats del endgame: la consume el juego y una repetición debe volver a darla).
      // Va ANTES del enrutado de prompts: mientras el volcado está aparcado no hay prompt
      // armado, y sin esta rama la tecla caería al despachador de comandos del mapa.
      if (talkConsole.keyWaiting) {
        ev.preventDefault();
        talkConsole.consumeKey();
        return;
      }
      // Hot-swap de pieles: F9 cambia de piel EN CALIENTE, en mitad de la partida,
      // mismo estado vivo, sin recargar. Va ANTES de todo. Cicla las pieles
      // USER-FACING (fiel↔shader; dev jubilada del ciclo, task #79). El menú SISTEMA
      // y el switcher directo comparten este camino (swapSkin/selectSkin).
      if (ev.key === "F9") {
        keyRec.drop(); // QoL del shell: no es estado de juego
        ev.preventDefault();
        swapSkin();
        return;
      }
      // F10: menú SISTEMA (shell QoL — partidas, vídeo, audio, teclas). Como F5..F9,
      // F10 no es un comando del original (getkey mapea F1..F10 → "What?"): no pisa nada.
      // Con OTRA superficie del shell abierta, F10 la cierra en vez de apilarse encima
      // (ficha #154 F7; el porqué y el criterio, en `closeTopShellSurface`).
      if (ev.key === "F10") {
        keyRec.drop(); // QoL del shell
        ev.preventDefault();
        if (savePanel.visible || selector.visible) closeTopShellSurface();
        else toggleShellDrawer();
        return;
      }
      // F5: panel de PARTIDAS. Vive AQUÍ, con las demás F de shell, y no doscientas líneas
      // más abajo — donde estaba, detrás de los early-returns de combate y mazmorra.
      //
      // ── POR QUÉ SUBE, Y ES LA DECISIÓN «IGUAL QUE ORIGINAL» DE LA FICHA #154 F2 ────────
      // Detrás de aquellos returns, F5 en mazmorra o en combate era un NO-OP MUDO: ni panel
      // ni mensaje ni nada. Ninguno de los dos contextos se comporta así en 1988, y son
      // DISTINTOS entre sí — que es justo lo que un único silencio no podía expresar:
      //
      //  · MAZMORRA → el original GUARDA. La Q llega intacta al kernel (DUNGEON.OVL:0x07a0
      //    → ULTIMA.EXE:0x3178 → handler 0x338c) y `CAST2.OVL:0x10fe` escribe SAVED.GAM sin
      //    mirar `g_location` ni `g_floor` en todo su cuerpo (0x10fe-0x11b9). El control que
      //    lo hace elocuente es su VECINO: el handler 'S' de 0x33a8 sí abre con
      //    `cmp byte [g_location],0x21`. El de 'Q' no tiene nada equivalente. ⇒ el panel se
      //    ABRE, que además es lo que la tecla Q del propio port ya hacía ahí
      //    (`handleDungeonKey`, rama `dk === "q"`): eran tecla y botón discrepando.
      //  · COMBATE → el original RECHAZA con texto: «Quit-Not here» sin turno (tabla de
      //    saltos COMBAT.OVL:0x0af8 índice 6 → 0x0a78 → SJOG.OVL:0x1f26 código 2). ⇒ se
      //    emite ESE mensaje, el mismo que ahora emite la Q en `handleCombatKey`.
      //
      // ⚠ Que F5 no sea una tecla de 1988 no lo exime: lo que se deriva no es la TECLA sino
      // la respuesta del juego a «guardar aquí», y de eso el binario sí tiene veredicto. El
      // principio que ya estaba escrito en `openSaves` (sections.ts) — «tecla == botón en
      // todo contexto» — se conserva entero; lo que cambia es que ahora los dos hacen lo
      // DERIVADO en vez de los dos no hacer nada.
      //
      // ⚠ CABO DECLARADO, NO MEDIDO POR MÍ: `re/notes/dungeon-rooms-audit.md:173` dice que
      // una SALA de mazmorra corre el bucle de teclas de COMBAT.OVL. Si es cierto, dentro de
      // una sala la respuesta fiel sería la de COMBATE y no la de mazmorra. No he re-derivado
      // la entrada a sala byte a byte, así que aquí manda `game.combat` (que es lo que el
      // port sabe) y el caso queda anotado para quien lo mida.
      if (ev.key === "F5") {
        keyRec.drop(); // QoL del shell (abre el panel de partidas)
        ev.preventDefault();
        if (game.combat) {
          hud.message(COMBAT_STRINGS.quitReject); // COMBAT 0x0a78 → SJOG 0x1f26 código 2
          view.emitSfx({ id: "combat-reject" }); // los dos beeps del funnel (0x1f52/0x1f5d, #161)
          return;
        }
        cancelAutoWalk();
        savePanel.show(game.state, mapName());
        return;
      }
      // F8: toggle del speaker fiel (task #3). F8 no es un comando del original
      // (getkey mapea F1..F10 a 0xC9..0xD2 → default "What?"), así que no pisa nada
      // — misma convención que F9=swap de piel.
      if (ev.key === "F8") {
        keyRec.drop(); // QoL del shell
        ev.preventDefault();
        const on = speaker.toggle();
        hud.message(on ? "Speaker on." : "Speaker off.");
        return;
      }
      // F7: toggle de la MÚSICA contextual "enhanced" (parche XMI→OGG). Como F8/F9, F7 no
      // es un comando del original (getkey mapea F1..F10 a 0xC9..0xD2 → default "What?"),
      // así que no pisa nada. Funciona en AMBAS pieles y su elección es EXPLÍCITA: persiste
      // (localStorage u5.music) y MANDA sobre el default del perfil de piel (fiel ⇒ sin
      // música / dev ⇒ enhanced; task #27). Así el usuario puede encender la música en la
      // piel fiel como opt-in. El mensaje es QoL (no es un string del binario).
      if (ev.key === "F7") {
        keyRec.drop(); // QoL del shell
        ev.preventDefault();
        const on = music.toggle();
        hud.message(on ? "Music on." : "Music off.");
        return;
      }
      // Vista aérea de (V)iew a gem: modal que se cierra con CUALQUIER tecla (el
      // original hace polling de tecla y vuelve, gem_view 0x11b6). La gema ya la
      // consumió game.view() al abrir; el TURNO se cobra AL CERRAR (game.afterGemView),
      // que es el orden del binario: el bucle de contexto corre tras retornar gem_view
      // (epílogo 0x31ee). Así una emboscada/spawn del turno arranca DESPUÉS del cierre.
      if (canvasGemActive) {
        ev.preventDefault();
        // 🔴 UN AUTO-REPEAT NO CIERRA — bug del ORIGINAL, arreglado y declarado
        // (docs/bugs-del-original.md §1.11, directriz del usuario del 05-08).
        //
        // El defecto que mata, MEDIDO sobre el caso que reportó el usuario: mantener la V
        // pulsada mandaba `keydown` #1 (abre, ecoa, `dec [g_gems]`) y el primer repeat del
        // SO caía aquí y CERRABA — sin ecoar, porque esta rama retorna antes del eco. Con
        // dos keydowns el jugador veía UN solo «>View a gem!», el viewport con el terreno
        // y una gema menos: «no pasa nada» con la gema comida. Los ecos cuentan aberturas
        // (⌈N/2⌉) y la PARIDAD de N decide si queda abierta; por eso un eco único NO
        // descartaba el auto-repeat, lo confirmaba.
        //
        // 1988 hace LO MISMO —por eso es bug del original y no del port—: `gem_view`
        // sondea con `int 16h AH=1` + `int 21h AH=6` (lectura DESTRUCTIVA) y sale a la
        // primera tecla del buffer; la BIOS mete ahí la repetición typematic con el mismo
        // scancode, indistinguible de una pulsación nueva, y el juego nunca cambia la
        // cadencia (censo: cero `int 16h AH=03h` en los 28 ficheros del corpus).
        //
        // ⚠ LA GUARDA VA AQUÍ Y NO EN EL DESPACHADOR ENTERO, y la razón es DERIVADA, no
        // de gusto: en el original se ANDA manteniendo la flecha —la repetición typematic
        // ES el paso continuo—, y este mismo repo la reproduce a mano en el táctil
        // (`ui/hold-repeat.ts`, HOLD_START_MS/HOLD_REPEAT_MS, escrito para que la cruceta
        // ande al mantener). Un `if (ev.repeat) return` al principio del keydown mataría
        // el andar-manteniendo del teclado físico: cambiaría un defecto por una regresión
        // más gorda, y encima menos fiel. Lo que se corrige es sólo lo que no tiene
        // defensa: que la repetición de la MISMA pulsación que abrió el modal lo cierre.
        if (ev.repeat) return;
        canvasGemActive = false;
        view.setGemView(null);
        // #144: si la vista la abrió la bola de cristal, cerrarla no cobra el turno de
        // (V) — `gem_view` la llamó `cmd_look` (0x0a3b), no el epílogo 0x31ee del case V.
        if (canvasGemChargesTurn) applyEvents(game.afterGemView());
        canvasGemChargesTurn = true;
        return;
      }
      // Vista de zodíaco del catalejo: modal cosmético que se cierra con CUALQUIER tecla
      // (look_sky hace polling y vuelve, 0x4ee). No cobra turno propio (el (U)se ya lo
      // gestionó); sólo cierra la vista y repinta el mapa.
      if (zodiacActive) {
        ev.preventDefault();
        // Misma guarda que la gema, por la misma razón y con una diferencia de COSTE que
        // conviene no borrar: el zodíaco comparte la forma (modal que cualquier tecla
        // cierra, rama pegada a la de arriba) pero NO cobra turno ni gasta consumible —
        // el (U)se ya lo gestionó. Su parpadeo era cosmético; el de la gema costaba una
        // gema. Se arregla igual por coherencia (el mismo gesto no puede comportarse de
        // dos maneras en dos modales hermanos), no porque hubiera nada que recuperar.
        if (ev.repeat) return;
        zodiacActive = false;
        view.setZodiacView(null);
        return;
      }
      // Prompts Y/N/dígito del original (getkey crudo). Precede a TODO (incl. el
      // bloque Escape y el guard de paneles). El REDUCTOR modal (los 9 tipos:
      // yesno/yesno-esc/digit/text/number/rune/party-select/ready-picker/shop)
      // vive en ui/prompt-manager.ts; true = tecla consumida por el prompt vivo.
      if (prompts.handleKey(ev)) return;
      if (ev.key === "Escape") {
        // ESC cierra el popup/drawer del shell que esté ABIERTO (el foco está en el
        // shell: cerrarlo no roba nada al juego). Cada rama CONSUME el ESC (return).
        // Con NADA del shell abierto, ESC NO abre el shell y NO se consume: cae al bucle
        // del juego, donde ESC es tecla real (cancelar apuntado / huida en combate,
        // COMBAT.OVL 0x06ea). Antes ESC abría el menú SISTEMA y robaba el keydown al
        // juego — retirado (veredicto usuario: ESC es del juego, no del shell).
        // Las cuatro ramas cierran algo del SHELL: el juego no ve la tecla ⇒ no se graba.
        if (savePanel.visible) { keyRec.drop(); savePanel.hide(); return; }
        if (selector.visible) { keyRec.drop(); selector.hide(); return; }
        if (debugMenu?.panel.isOpen) { keyRec.drop(); debugMenu.close(); return; }
        if (shellPanel?.isOpen) { keyRec.drop(); shellPanel.close(); syncShellExpanded(false); return; }
        // nada del shell abierto → no consumir; sigue al bucle del juego.
      }
      if (savePanel.visible || selector.visible) { keyRec.drop(); return; } // los paneles DOM capturan el teclado (la charla/tienda fiel van por prompts.current)
      if (game.combat) {
        handleCombatKey(ev);
        return;
      }
      if (game.dungeonState) {
        handleDungeonKey(ev);
        return;
      }
      // Familia Ctrl de MAINOUT/TOWN (jump tables 0x0bbe/0x14b8): Ctrl-K karma,
      // Ctrl-S sound, Ctrl-V versión, Ctrl-E exit-to-DOS. Va ANTES de las ramas
      // de letra (hoy Ctrl+K disparaba Klimb — hijack del informe de cobertura).
      if (handleCtrlKey(ev)) return;
      // Resto de combos Ctrl/Meta: no son comandos del juego (default de la jump
      // table <0x20) — no despachar la letra pelada ni robar el atajo al navegador.
      if (ev.ctrlKey || ev.metaKey) { keyRec.drop(); return; } // no es comando: el atajo es del navegador
      if (ev.key.toLowerCase() === "z") {
        ev.preventDefault();
        // Ztats: la piel fiel/shader pinta la ficha en su propio overlay (skin/fiel: su
        // keyHandler reacciona a Z) — E1-S11. (El panel DOM de Ztats era de la piel dev,
        // jubilada.)
        return;
      }
      // (F5 vivía aquí, DETRÁS de los early-returns de combate y mazmorra, y por eso era
      //  un no-op mudo en los dos. Subido con las demás F de shell — ficha #154 F2/F7.)
      // NUMPAD con NumLock (teclas-05): el getkey del binario (wrapper ULTIMA.EXE
      // 0x26a4, tabla 0x26fe) remapea los dígitos SHIFTED del numpad a direcciones:
      // 8/2/4/6 → ↑/↓/←/→ (códigos 3/4/1/2) y 7/9/1/3 → 0xD3-0xD6 (diagonales,
      // activas sólo en aim/Ztats/tiendas — QoL-diferido del censo §3, aquí se
      // ignoran). El port trataba TODO dígito como Set Active Player (divergencia
      // activa del informe); la fila superior sigue siendo Set Active Plr.
      if (/^[0-9]$/.test(ev.key) && ev.code.startsWith("Numpad")) {
        ev.preventDefault();
        const nd =
          ev.key === "8" ? "north"
          : ev.key === "2" ? "south"
          : ev.key === "4" ? "west"
          : ev.key === "6" ? "east"
          : null;
        if (nd) {
          cancelAutoWalk();
          applyEvents(game.move(nd));
        }
        return; // 7/9/1/3 y 0/5: sin comando aquí (diagonales = banco QoL-diferido)
      }
      // Clavicémbalo (TOWN 0x1580): en small map, si la party está SENTADA al
      // clavicémbalo (tile al sur == 0x8D), los dígitos '0'-'9' tocan una nota SIN
      // consumir turno y NO caen al dispatch normal. El gate de ztats ya está
      // cubierto por el `return` de arriba (ztatsPanel.visible). Ver
      // re/notes/interactions-piano-fire-audit.md §1.2. (task #54 G2)
      if (ev.key.length === 1 && ev.key >= "0" && ev.key <= "9" && game.harpsichordSeated()) {
        ev.preventDefault();
        applyEvents(game.playHarpsichordNote(Number(ev.key)));
        return;
      }
      // ── BORRACHERA: intercepto del dispatch (town_read_command 0x0DD0-0x0E27) ──
      // En pueblo con [0x5957]≠0 el prólogo (viento + gate de confusión) rueda POR
      // TECLA LEÍDA y ANTES del dispatch; con "Hic!" el código devuelto SUSTITUYE al
      // comando → el turno es un TUMBO y la tecla pulsada NO corre (banco zona-caliente
      // del carril cobertura-medias, ahora cableado). Va aquí — tras los modales, el
      // clavicémbalo y los numpad-moves; antes de set-active y de TODO el dispatch de
      // comandos. EXCLUYE: flechas (move() rueda su propio prólogo), teclas de un
      // getdir/prompt vivo (el binario no rueda en los getkey anidados) y las QoL del
      // shell (F5-F10/Tab, ya retornadas; 'z' va al modal de la piel — divergencia
      // declarada de presentación). Sobrio/overworld: no-op de coste cero.
      if (
        !pendingDirCommand &&
        !pendingCastDoor &&
        !pendingCastUnlock &&
        !pendingCastBlink &&
        !pendingUseSkullKey &&
        !pendingScrollWind &&
        KEY_DIRECTIONS[ev.key] === undefined &&
        ((ev.key.length === 1 && ev.key >= " ") || /^F[1-4]$/.test(ev.key))
      ) {
        const stagger = game.commandDrunkIntercept();
        if (stagger) {
          ev.preventDefault();
          cancelAutoWalk();
          applyEvents(stagger);
          return;
        }
      }
      // ★ #185 — CABECERA del bucle de pueblo (TOWN 0x1436-0x1452): con la party ENTERA
      // DORMIDA el original NO lee tecla. Imprime «Zzzzzz...» y consume el turno solo, y
      // por eso va AQUÍ, antes de despachar ningún comando de mapa: cualquier tecla que
      // llegue en ese estado se resuelve como el turno automático, no como su comando.
      // Es la guarda HERMANA de la del CIERRE de #181 (que gatea con −1, el refuge); los
      // dos caminos conviven porque `ax == 1` deja intacta la bandera [bp-0xa] de 0x1431.
      {
        const sleeping = game.townAutoSleepTurn();
        if (sleeping) {
          ev.preventDefault();
          cancelAutoWalk();
          applyEvents(sleeping);
          return;
        }
      }
      // Dígitos '0'-'9' SIN clavicémbalo y SIN ztats/modal (el gate de ztats de
      // arriba ya hizo return): SET ACTIVE PLAYER (kernel 0x4080, vía MAINOUT 0xc06 /
      // TOWN 0xe34). Eco "Set Active Plr:" SIEMPRE (0x4095), luego el core añade el
      // nombre / "None!" / "Invalid!". Coexiste con el select 1-6 del ztats abierto
      // (gestionado por su panel) y con la nota del clavicémbalo (arriba). QA #69.
      if (ev.key.length === 1 && ev.key >= "0" && ev.key <= "9") {
        ev.preventDefault();
        cancelAutoWalk();
        hud.echo(CMD_STRINGS.setActive); // "Set Active Plr:" (DS 0xa396)
        applyEvents(game.setActivePlayer(Number(ev.key)));
        return;
      }
      const dir = KEY_DIRECTIONS[ev.key];
      const key = ev.key.toLowerCase();
      if (pendingCastDoor) {
        ev.preventDefault();
        const fx = pendingCastDoor;
        pendingCastDoor = null;
        if (dir) applyEvents(game.applyDoorSpell(fx, dir));
        else hud.message("Cancelled.");
        return;
      }
      if (pendingCastUnlock) {
        ev.preventDefault();
        pendingCastUnlock = false;
        if (dir) applyEvents(game.applyUnlockSpell(dir));
        else hud.message("Cancelled.");
        return;
      }
      if (pendingCastBlink) {
        // getdir del In Por de exterior. Cancelar (sin dir) sale por 0x0687 `mov ax,0xffff`
        // SIN mover ni cobrar turno: applyBlinkSpell(null) devuelve cero eventos.
        ev.preventDefault();
        pendingCastBlink = false;
        applyEvents(game.applyBlinkSpell(dir ?? null));
        return;
      }
      if (pendingUseSkullKey) {
        // getdir del (U)se Skull Key (CAST.OVL 0x18dd). La llave YA se decrementó al
        // seleccionar el item (0x18c4 va ANTES del getdir): game.useSkullKey consume
        // aunque `dir` sea null (cancelado). Task #22.
        ev.preventDefault();
        pendingUseSkullKey = false;
        applyEvents(game.useSkullKey(dir ?? null));
        return;
      }
      if (pendingScrollWind) {
        // getdir del pergamino Rel Hur (CAST2 0x0306): la dirección fija el viento, pero
        // SÓLO en overworld (loc<0x21; gate 0x122f). Cancelar (sin dir) = no cambia el viento.
        ev.preventDefault();
        const loc = pendingScrollWind.location;
        pendingScrollWind = null;
        if (dir && loc < 0x21) {
          game.state.wind = windForDirection(dir);
          game.state.windDriftCtr = 0; // set_wind resetea el contador de deriva (kernel 0x2EA5)
        }
        return;
      }
      if (pendingDirCommand) {
        ev.preventDefault();
        if (dir) {
          const cmd = pendingDirCommand;
          pendingDirCommand = null;
          // ECO DE LA DIRECCIÓN (getdir kernel 0x35EC): los comandos direccionales
          // que pasan por getdir imprimen la palabra de dirección tras el guión, en
          // la MISMA fila ("Open-North"), ANTES del resultado. Set = los que resuelven
          // a kernel 0x35EC (Open/Get/Search/Jimmy/Push/Klimb/Attack, todos con eco
          // "Cmd-"). Look ("Look" SIN guión) y Talk/Fire (flujo distinto) se excluyen.
          // Puro cosmético: no toca el turno ni lo que se cobra (QA usuario).
          // t() en el call-site: la palabra la imprime getdir (display), NO es input
          // tecleado — echoAppend sigue mecánico por doctrina (coreview §3). Bajo 'en'
          // t es identidad; bajo 'es' evita el compuesto mixto «Atacar-South» (QA usuario).
          if (DIR_ECHO_COMMANDS.has(cmd)) hud.echoAppend(t(DIR_WORDS[dir]));
          if (cmd === "open") applyEvents(game.open(dir));
          if (cmd === "look") applyEvents(game.look(dir));
          if (cmd === "talk") startTalk(dir);
          if (cmd === "get") applyEvents(game.get(dir));
          if (cmd === "fire") applyEvents(game.fire(dir));
          // (S)earch: player-select del comando (SJOG 0x09a0 → kernel 0x4988) ANTES
          // del resultado — «Player: Min» (LP P08 E11); el elegido percibe la trampa.
          if (cmd === "search") pickCommandChar((mIdx) => applyEvents(game.search(dir, mIdx)));
          if (cmd === "jimmy") applyEvents(game.jimmy(dir));
          if (cmd === "klimb") applyEvents(game.klimb(dir));
          if (cmd === "push") applyEvents(game.push(dir));
          if (cmd === "attack") applyEvents(game.attack(dir));
        } else {
          const cmd = pendingDirCommand;
          pendingDirCommand = null;
          // Cancelar el getdir del Klimb de pueblo COBRA 1 turno (TOWN town_klimb
          // 0x0C3E: al volver getdir=0 marca [bp-2]=1 → el bucle avanza el reloj).
          // Es la ÚNICA rama con cobro-en-cancel VALIDADA byte a byte; los otros 7
          // comandos direccionales quedan sin medir (Clase C, deliberate-divergences.md
          // §3) → conservan el "Cancelled." sin turno del flujo compartido.
          if (cmd === "klimb") applyEvents(game.klimbCancel());
          else hud.message("Cancelled.");
        }
        return;
      }
      if (key === "t") {
        ev.preventDefault();
        cancelAutoWalk();
        pendingDirCommand = "talk";
        hud.echo(CMD_STRINGS.talk);
        return;
      }
      if (key === "o") {
        ev.preventDefault();
        cancelAutoWalk();
        pendingDirCommand = "open";
        hud.echo(CMD_STRINGS.open);
        return;
      }
      if (key === "l") {
        ev.preventDefault();
        cancelAutoWalk();
        pendingDirCommand = "look";
        // "Look" (DS 0xa1a8) + guión del despachador (0x3332 `putchar 0x2d`), ANTES del
        // getdir → la palabra de dirección se añade luego (DIR_ECHO_COMMANDS) = "Look-North".
        //
        // 🔴 DEFECTO VIVO CORREGIDO (visto por el usuario, 04-08): esto era
        // `hud.echo(CMD_STRINGS.look + "-")` y el eco salía «Look-North» en una partida
        // en castellano. `pushConsole` traduce la cadena COMPUESTA, y "Look-" NO EXISTE
        // en el binario: el guión lo imprime el despachador aparte, así que la DS lleva
        // "Look" a secas — al revés que sus hermanos ("Open-", "Search-"…), que sí lo
        // traen dentro. El diccionario fallaba y caía al inglés.
        // Y no se arregla añadiendo "Look-" a es.json: la guarda anti-fabricación de
        // `i18n-manifest` lo rechaza con razón —una key que no existe en el original—.
        // Se traduce la CONSTANTE y se compone después.
        hud.echo(t(CMD_STRINGS.look) + "-");
        return;
      }
      if (key === "g") {
        ev.preventDefault();
        cancelAutoWalk();
        pendingDirCommand = "get";
        hud.echo(CMD_STRINGS.get);
        return;
      }
      if (key === "k") {
        ev.preventDefault();
        cancelAutoWalk();
        applyEvents(game.klimb());
        return;
      }
      if (key === "s") {
        // (S)earch pide dirección, como en el original (SJOG 0x095C: 097e llama a
        // getdir 0x766c; la celda inspeccionada es party+dir, 0988-099d). Sin
        // dirección, las puertas secretas (0x4E) eran irrevelables desde teclado.
        ev.preventDefault();
        cancelAutoWalk();
        pendingDirCommand = "search";
        hud.echo(CMD_STRINGS.search);
        return;
      }
      if (key === "j") {
        // (J)immy es DIRECCIONAL en el binario (kernel 0x32DA → SJOG cmd_jimmy
        // 0x0D4A: 0d5a llama a getdir 0x766c, igual que Open/Search/Look). Destraba
        // la puerta/cerradura de la celda party+dir. game.jimmy() ya calca la regla
        // de DEX vs rand y sólo cobra turno en el éxito.
        ev.preventDefault();
        cancelAutoWalk();
        pendingDirCommand = "jimmy";
        hud.echo(CMD_STRINGS.jimmy);
        return;
      }
      if (key === "m") {
        ev.preventDefault();
        cancelAutoWalk();
        doMix();
        return;
      }
      if (key === "c") {
        ev.preventDefault();
        cancelAutoWalk();
        doCast();
        return;
      }
      if (key === "r") {
        ev.preventDefault();
        cancelAutoWalk();
        doReady();
        return;
      }
      if (key === "u") {
        // (U)se item — dispatcher CAST.OVL 0x1792: abre el picker de items usables
        // (extended item table 0xB9EE) → según el item, rama del jump-table 0x185d.
        // Skull Key (0x18c4) desmagifica la puerta enfrente; shards = mecánica del
        // clon (Shadowlords). El picker sólo lista items con count≥1 (ZSTATS
        // find_next/prev_owned 0x05a4/0x056c saltan qty 0) → la Skull Key aparece
        // sólo con skullKeys≥1: por eso useSkullKey nunca puede hacer underflow. Task #22.
        ev.preventDefault();
        cancelAutoWalk();
        // PRESENTACIÓN: (U)se usa el OVERLAY de pergamino fiel (`item_page_controller`
        // @0x0f2e mode 'U', reusa la maquinaria de Ready #78). Lista los items usables que
        // POSEES en el orden del jump-table de (U)se (CAST.OVL 0x185d: Skull Key, artefactos
        // de LB, shards, herramientas de endgame) y despacha a game.useXxx(). Ver
        // re/notes/use-merchants.md §Use. (El popup DOM listaba lo mismo pero era de la piel
        // dev, jubilada — veredicto usuario #8.)
        // Eco del dispatcher "Use item\n\n" (DS 0xa24c) — C12 del espejo P08 (E03):
        // el overworld no lo emitía (en combate ya salía, main.ts openCombatUsePicker).
        hud.echo(CMD_STRINGS.use);
        openUsePicker();
        return;
      }
      // (E)nter — kernel dispatch 0x3254. Estar ENCIMA de una localización
      // (pueblo/castillo/keep/mazmorra) y pulsar E entra; el original NO auto-entra
      // al pisar (game.enter deriva MAINOUT cmd_enter 0x08de: label por tile + carga).
      if (key === "e") {
        ev.preventDefault();
        cancelAutoWalk();
        applyEvents(game.enter());
        return;
      }
      if (key === "b") {
        ev.preventDefault();
        cancelAutoWalk();
        applyEvents(game.board());
        return;
      }
      if (key === "q") {
        // (Q)uit & Save — dispatcher kernel 0x338C → CAST2 cmd_quit_save 0x10FE. El
        // original no es multi-save: guarda LA partida. Flujo fiel POR CONSOLA (no
        // popup): eco "Quit:" (DS 0xa1ea) → "\nSave game? " (DATA.OVL 0x9668, acaba en
        // ESPACIO sin `\n`) → getYN 0x448c (SÓLO Y/N, re-lee cualquier otra). La
        // respuesta va INLINE tras la pregunta (getYN 0xa49c no ecoa; el caller imprime
        // la palabra sin `\n` previo — CAST2 0x111f/0x1126): N → "No\n" (0x9676) sin
        // guardar · Y → "Yes\n" (0x967a) + "Saving...\n" (0x967e) + escribe el save +
        // "Done.\n" (0x96bc, CAST2 0x11af→0x11b2). El port persiste con `autosave`
        // (localStorage) = su equivalente de SAVED.GAM; el panel multi-save (F5) es QoL
        // del skin nuevo y se conserva aparte.
        ev.preventDefault();
        cancelAutoWalk();
        doQuitSave(); // flujo compartido con mazmorra (CAST2 0x10FE es incondicional)
        return;
      }
      // (X)-it — dispatcher kernel 0x3456 → CMDS cmd_xit 0x0EB4. El dispatcher ECOA
      // "X-it " (DS 0xa280, sin `\n`) ANTES de correr el overlay, igual que el resto
      // de comandos; el port se lo saltaba y a pie sólo se veía el "what?" suelto
      // (msg 0x4368, minúscula) — parecía el "What?" del default. Con el eco queda
      // "X-it" + "what?" a pie y "X-it" + resultado montado/embarcado. exitVehicle ya
      // deriva CMDS 0x0EB4 (rama por g_transport_tile&0xFC).
      if (key === "x") {
        ev.preventDefault();
        cancelAutoWalk();
        hud.echo(CMD_STRINGS.xit); // "X-it " (eco del dispatcher 0x3456; faltaba)
        applyEvents(game.exitVehicle());
        return;
      }
      if (key === "y") {
        // (Y)ell — CMDS 0x1418. En fragata iza/arría velas; fuera, pide palabra
        // (yell-word-prompt) → convocatoria de Shadowlord en salas de Llama. F1.10-T5.
        ev.preventDefault();
        cancelAutoWalk();
        applyEvents(game.yell());
        return;
      }
      if (key === "f") {
        ev.preventDefault();
        cancelAutoWalk();
        hud.echo(CMD_STRINGS.fire); // "Fire-" (dispatch kernel, DS 0xa164)
        // ANDANADA de fragata (loc 0 sobre fragata) → getdir → game.fire(dir). El resto
        // (cañón a pie en pueblo/combate, mazmorra, exterior a pie) se resuelve al
        // instante SIN pedir dirección: es la rama 0x0B16/0x0AF7/0x0978 de cmd_fire.
        if (game.fireWantsDirection()) {
          pendingDirCommand = "fire";
        } else {
          applyEvents(game.fireCannon());
        }
        return;
      }
      // (A)ttack — dispatcher kernel 0x3216. El overlay destino imprime "Attack-"
      // (MAINOUT 0x29fe / TOWN 0x26e0) y ANTES de pedir dirección aplica el gate de
      // vehículo/terreno (`game.attackContext`, MAINOUT 0x70d / TOWN 0xa08): sobre
      // agua en skiff/alfombra (overworld) o en cualquier no-a-pie (pueblo) →
      // "On foot!" SIN getdir. Si pasa, pide dirección (getdir 0xB41C). En OVERWORLD
      // golpear un enemigo errante adyacente inicia combate (startCombat, fork del
      // stream); nada → "Nothing to attack!". En PUEBLO el combate/karma/guardias no
      // está modelado → Clase C (game.attack lo documenta). NO cobra turno estándar
      // ([bp-2]=0, sin g_unk_24e6); cancelar → "Cancelled." del flujo compartido. En
      // combate la 'a' la maneja handleCombatKey (rama game.combat de arriba).
      if (key === "a") {
        ev.preventDefault();
        cancelAutoWalk();
        hud.echo(CMD_STRINGS.attack); // "Attack-" (el overlay lo imprime ANTES del gate)
        const ctx = game.attackContext();
        if (!ctx.ok) {
          hud.message(ctx.message ?? ATTACK_ON_FOOT); // rechazo por vehículo → sin getdir
          return;
        }
        pendingDirCommand = "attack";
        return;
      }
      // (P)ush — CMDS 0x161A. Direccional (como Open/Get): "Push-" y luego la
      // flecha (el original imprime "Push" y llama a getdir 0x766c, sin prompt
      // "which way"). game.push(dir) desliza/intercambia el objeto y cobra turno
      // SÓLO en éxito (0x1798 setea g_unk_24e6; el fallo "Won't budge!" no).
      if (key === "p") {
        ev.preventDefault();
        cancelAutoWalk();
        pendingDirCommand = "push";
        hud.echo(CMD_STRINGS.push);
        return;
      }
      // (N)ew Order — CMDS 0x0DDC. ACCIÓN LIBRE (no cobra turno): swap de dos
      // miembros de la marcha, con el Avatar anclado en cabeza (game.newOrder
      // devuelve "<nombre> must lead!" si se elige el idx 0). El original prompta
      // "Swap <n1> with <n2>" seleccionando dos miembros (0x6f0e ×2); aquí se
      // reusa el picker de miembros (mismo que Cast/Ready).
      if (key === "n") {
        ev.preventDefault();
        cancelAutoWalk();
        hud.echo(CMD_STRINGS.newOrder); // ">New Order" (dispatch 0x334E, DS 0xa1c4)
        // tf en los prompts (patrón "Cast & who?" @2265: el choke t() no ve el título
        // de pickMember) — fuga ES cazada por el soak (carril sell-chatter).
        pickMember(tf("Swap — who?"), (idx1) => {
          pickMember(tf("Swap with — who?"), (idx2) => {
            applyEvents(game.newOrder(idx1, idx2));
          });
        });
        return;
      }
      // Ignite torch fuera de mazmorra — CMDS 0x0D98 (gap de la matriz: I ya estaba
      // en el bucle de mazmorra, main.ts handleDungeonKey). game.ignite() enciende
      // (g_torch_mins=240 fijo en exterior/pueblo) y cobra el turno estándar también
      // al fallar ("None owned!"), como el dispatcher que devuelve "consumido".
      if (key === "i") {
        ev.preventDefault();
        cancelAutoWalk();
        hud.echo(CMD_STRINGS.ignite); // eco del comando ">Ignite torch!" (dispatch 0x32CC, DS 0xa188)
        applyEvents(game.ignite());
        return;
      }
      // (H)ole up & camp — kernel 0x3C9A (dispatcher 0x3288). Acampada a la
      // intemperie: cura la party, pasa N horas y puede disparar la aparición/
      // emboscada. startCamp orquesta los prompts crudos.
      if (key === "h") {
        ev.preventDefault();
        startCamp();
        return;
      }
      // (V)iew a gem — dispatcher kernel 0x341A. Imprime "View a gem!", gatea por
      // gemas ("You have none!" si 0), consume 1 gema y abre la vista aérea del
      // entorno (overworld/pueblo). Cobra 1 turno (incluso sin gemas). game.view()
      // emite el evento "gem-view" que applyEvents traduce a viewGemPanel.show().
      if (key === "v") {
        ev.preventDefault();
        cancelAutoWalk();
        hud.echo(CMD_STRINGS.view); // eco del comando ">View a gem!" (dispatch 0x341A, DS 0xa258)
        applyEvents(game.view());
        return;
      }
      // Pass — Space (kernel 0x31F4): pasa el turno sin moverse ("Pass"), cobrando
      // el coste estándar del contexto. En combate el Space lo maneja handleCombatKey
      // (rama game.combat de arriba); aquí es overworld/pueblo.
      // El ECO es del DESPACHADOR, igual que Ignite/View/Open: 0x31F4 descarta la rama
      // de vela y cae en 0x3210 `mov ax,0xa134` → 0x33ea `call print_string` (0x1850),
      // el MISMO emisor que los demás casos. DS 0xa134 = "Pass\n" (DATA.OVL verificado
      // byte a byte). Antes lo empujaba el core como `{kind:"message"}` con texto "Pass"
      // SIN el `\n`: sin bullet ► y sin la línea en blanco que separa una pulsación de
      // la siguiente — el log apilaba «Aguardáis» pegados (QA usuario móvil 28-07).
      if (ev.key === " " || ev.key === "Spacebar") {
        ev.preventDefault();
        cancelAutoWalk();
        hud.echo(CMD_STRINGS.pass); // ">Pass" (dispatch 0x31F4→0x33ea, DS 0xa134)
        applyEvents(game.pass());
        return;
      }
      if (!dir) {
        // DEFAULT del dispatcher (kernel_cmd_dispatch 0x34D8; auditoría de cobertura
        // «teclas-06-what-echo-invalidas»): toda tecla no-comando imprime "What?\n"
        // SIN turno (ret 0). 'D' (Descend) y 'W' (Wear) NO existen como comandos en
        // esta versión y tienen su print propio (0x324E "D-What?\n" / 0x3450
        // "W-What?\n" — se hace con Klimb/Ready). F1..F10 caen al mismo default en
        // el binario (getkey 0x1D5E → 0xC9-0xD2); aquí F5-F10/Tab son QoL declaradas
        // del shell (ya retornaron arriba) — solo F1-F4 ecoan. Los combos Ctrl/Meta
        // también retornaron antes (QoL declarada: no robar atajos del navegador).
        const printable = ev.key.length === 1 && ev.key >= " ";
        if (printable || /^F[1-4]$/.test(ev.key)) {
          ev.preventDefault();
          if (key === "d") hud.message("D-What?\n"); // 0x324E
          else if (key === "w") hud.message("W-What?\n"); // 0x3450
          else hud.message("What?\n"); // 0x34D8, ret 0 = sin turno
        }
        return;
      }
      ev.preventDefault();
      cancelAutoWalk();
      applyEvents(game.move(dir));
    };

    // Cada tecla puede abrir/cerrar un modal (selector, prompt) sin pasar por
    // applyEvents; re-deriva el gate del cursor (F-G) tras procesarla.
    window.addEventListener("keydown", (ev) => {
      handleGameKey(ev);
      // Cierra la tecla EN VUELO con el turno RESULTANTE. Si alguna rama la soltó
      // (`keyRec.drop()`), esto es un no-op. El turno es a la vez el índice de salto de
      // la repetición y su SUMA DE CONTROL: al reproducir, si el turno no coincide, la
      // repetición ha divergido y lo dice en vez de seguir pintando otra partida.
      keyRec.commit(game.state.turnsSinceStart);
      refreshAwaiting();
      // Publica si el bucle espera una DIRECCIÓN (getdir vivo): la piel fiel pone
      // entonces el cursor de la ola JUNTO al comando ("Look-ζ") en vez de en una
      // línea de prompt aparte. Se computa aquí (no en refreshAwaiting) porque
      // `pendingDirCommand` se declara más abajo y esto corre tras el boot (sin TDZ).
      view.setAwaitingDirection(
        pendingDirCommand != null || pendingCastDoor != null || pendingCastUnlock || pendingUseSkullKey,
      );
      // El cursor de la fila viva del prompt (setAwaitingGetstring) se publica desde
      // refreshAwaiting (arriba, ya llamado): cubre TODA la clase de prompts que esperan
      // por el getkey 0x266c, no sólo los getstrings (cabo #341 §7.2).
      syncTouchExpect(); // móvil: alza la hoja del deck para el input esperado (tras el boot).
      // …y re-deriva el CONTEXTO del deck (mundo/mazmorra/combate): la tecla que acaba
      // de procesarse es la que entra a una mazmorra o abre un combate. Antes esto lo
      // descubría un polling de 400 ms; ahora es evento (auditoría móvil, TANDA C).
      refreshTouchDeck();
    });

    // Capa INTENCIÓN→COMANDO (E1-S1). Las pieles (y la botonera táctil) producen
    // intents; este sink los traduce a comandos del dispatcher original. Es la
    // costura #8/#9 de la interview: el input compone con cualquier piel (el
    // tap-to-walk funciona igual en dev que en la fiel, porque llega en coords de
    // MAPA, no de pantalla). Vive FUERA de las pieles.
    const intents: IntentSink = {
      dispatch(intent) {
        if (intent.type === "key") {
          // Nivel 0: tecla del original (mismo camino que la botonera táctil).
          window.dispatchEvent(new KeyboardEvent("keydown", { key: intent.key, bubbles: true }));
          return;
        }
        if (intent.type === "console") {
          // Salida de consola que una piel produce por sí misma (Ztats de la piel
          // fiel): mismo `pushConsole` que el resto del juego → output real, en
          // cualquier piel (RUTA CORE, api.ts Intent "console").
          view.pushConsole(intent.text, intent.kind ?? "message");
          return;
        }
        // ── TAP CONTEXTUAL (chapa Enhanced) — la costura, y es DELIBERADAMENTE ESTRECHA ──
        // Va ANTES de `cancelAutoWalk()` a propósito: si resuelve, esta rama no debe haber
        // tocado NADA del camino de siempre — la cancelación de la auto-marcha la hace el
        // propio `handleGameKey` al procesar la 't' (rama `if (key === "t")`), igual que
        // con un teclado físico. Tocarla aquí sería cancelar dos veces por un camino y una
        // por el otro, que es justo la clase de divergencia que este carril prohíbe.
        //
        // Las TRES conjunciones son el contrato entero:
        //   · `chapaEnhancedViva()` — se le pregunta al DOM, no a la preferencia (mismo
        //     criterio que el resto del fichero): con la chapa apagada —o en escritorio,
        //     donde ni se ofrece— esto es un `classList.contains` y ni se construye el
        //     snapshot. El modo Clásico no paga ni cambia.
        //   · `resolverTapContextual` — su propio gate de modales sale de `awaitingInput`,
        //     que ES `!isModalOpen(...)` (ui/awaiting-gate.ts). No hay lista paralela.
        //   · `shellSurfaceOpen()` — las superficies DOM del shell capturan el teclado
        //     antes del bucle (`keyRec.drop()`) y NO entran en `isModalOpen`; con una
        //     abierta el tap tiene que seguir haciendo lo de siempre.
        // Con `true` las dos `press()` (`t` + flecha, sobre `document.body`) YA salieron y
        // desde ahí el flujo es el del teclado, tecla a tecla: eco «Talk-», getdir vivo,
        // grabador, turno. Con `false` no se ha tocado NADA y sigue el camino de siempre.
        if (
          manejarTapContextual(
            {
              enhanced: chapaEnhancedViva,
              mundo: () => {
                const snap = view.snapshot();
                const pos = game.state.position;
                return {
                  ventana: snap,
                  modo: snap.mode,
                  esperandoInput: snap.awaitingInput,
                  esperandoDireccion: snap.awaitingDirection,
                  shellAbierto: shellSurfaceOpen(),
                  // EL MISMO predicado que elige el radio de `snapAttackCell` en el tap de
                  // COMBATE (más abajo en este mismo sink): la marca que pone el deck.
                  tactil: document.documentElement.classList.contains("u5-touch"),
                  // Estado CRUDO, y por eso la fachada sólo lo consulta DESPUÉS de aprobar
                  // la visibilidad de la celda (world.ts §anti-filtrado).
                  npcEn: (x, y) =>
                    game.npcManager?.npcAt(pos.location, pos.floor, x, y) != null,
                };
              },
            },
            intent.x,
            intent.y,
          )
        ) {
          return;
        }
        // tap-tile: QoL click-para-caminar (A*) / atacar en combate.
        cancelAutoWalk();
        if (game.combat) {
          const cur = game.combat.currentUnit;
          if (cur?.kind === "player") {
            // TOLERANCIA DEL TAP (auditoría móvil 2026-07-25): la casilla mide 19,5 px
            // CSS en un teléfono vertical, así que 10 px de error apuntan a la celda
            // vecina y el turno se quema con un «Nothing!». `snapAttackCell` corrige la
            // puntería SÓLO cuando es inequívoca (un único enemigo a un paso) — es
            // presentación, no mecánica: el core resuelve el golpe igual, con el mismo
            // comando, y con 0 o ≥2 candidatos la celda pasa TAL CUAL.
            // SÓLO EN TÁCTIL (`html.u5-touch`, la marca que pone el deck): en escritorio
            // el ratón es exacto y el clic en suelo vacío junto a un enemigo debe seguir
            // dando el «Nothing!» del binario. Radio 0 = sin ajuste.
            const aim = snapAttackCell(
              intent,
              game.combat.combatants.filter(
                (c) => c.kind === "enemy" && c.status !== "dead" && c.status !== "fled" && !c.invisible,
              ),
              document.documentElement.classList.contains("u5-touch") ? 1 : 0,
            );
            combatOut(game.combat.playerAttack(aim.x, aim.y));
            pumpCombat();
            refreshTouchDeck(); // el combate pudo terminar: rejilla del deck al día
          }
          return;
        }
        autoWalkCtl.walkTo(intent); // A* + interval de 140 ms (ui/autowalk.ts)
        // El paso puede disparar un encuentro (o entrar a una mazmorra): el deck
        // re-deriva su contexto por EVENTO, no por polling (ui/touch.ts).
        refreshTouchDeck();
      },
    };

    // Registro de pieles y montaje inicial (E1-S1). El core (game/view) no sabe
    // cuál está montada. E1-S8 registrará la piel fiel; F9 alternará en caliente.
    const skins = new SkinManager(parent, view, intents);
    // Jubilación fase 2: la piel dev ELIMINADA. Sólo fiel y shader.
    const fielSkin = new FaithfulSkin({ sceneBeatMs: SCENE_BEAT_MS }); // ticket #18: knob único
    skins.register(fielSkin, { label: SKIN_LABELS.faithful });
    // Piel «shader» (task #70): la fiel + xBR en el mundo. F9 la cicla (fiel↔shader);
    // ?skin=shader y la persistencia u5.skin la seleccionan de arranque (ver más arriba).
    const shaderSkin = new ShaderSkin();
    skins.register(shaderSkin, { label: SKIN_LABELS.shader });
    /**
     * Pieles que el envoltorio del layout partido puede ALOJAR (LOTE C). Las DOS, que es
     * literalmente el encargo. El mapeo id→instancia vive aquí y no en `PortraitSkin`
     * porque el registro de pieles es cosa de la composición raíz; el envoltorio sólo
     * sabe de `HostableSkin`.
     *
     * Se reusan las MISMAS instancias que registra el manager, no copias: el
     * `SkinManager` desmonta antes de montar (`manager.ts` §swap), y el envoltorio hace
     * lo propio con la alojada, así que nunca hay dos montajes vivos de la misma piel.
     * Con instancias aparte se pagarían dos veces los atlas HD y el estado de la piel no
     * sobreviviría al viaje partido ⇄ clásico.
     */
    const ALOJABLES: Record<string, HostableSkin> = {
      faithful: fielSkin,
      shader: shaderSkin,
    };
    const esAlojable = (id: string): boolean => id in ALOJABLES;
    const pielAlojable = (id: string): HostableSkin => ALOJABLES[id] ?? fielSkin;
    // PROTOTIPO re-flow vertical: se registra SÓLO con la bandera puesta y como
    // `userFacing:false` — fuera del ciclo F9 y del switcher (los conteos de pieles de la
    // suite e2e siguen viendo exactamente 2). Sin bandera no se instancia siquiera.
    // LAYOUT PARTIDO como PREFERENCIA, no como bandera de URL (petición del usuario 27-07:
    // «un botón en la barra de botones que switchee entre layout original y partido»). La
    // bandera sigue valiendo para forzarlo/medirlo; si no hay bandera, manda lo guardado.
    // ★ 02-08: ofrecerlo también en TÁCTIL sin bandera ni preferencia — es lo que hace que
    // el envoltorio (y con él el botón ▤, que es la ÚNICA vía de volver) exista en un móvil
    // recién instalado. Deliberadamente MÁS ANCHO que el arranque: con la preferencia en
    // «clásico» el envoltorio se instancia igual, para poder re-encenderlo. Ver
    // `layoutPartidoDisponible`.
    // ★ 16-08 (#333 pieza A1): y SOLO en régimen táctil — `layoutPartidoDisponible` gatea
    // ahora por el puntero, así que en un boot de escritorio el envoltorio NI SE INSTANCIA
    // aunque haya preferencia guardada o `?reflow=cuadrado` en la URL (decisión del
    // usuario: en escritorio el partido ni se ofrece ni se restaura). Un boot de
    // escritorio que pase a táctil (2-en-1) recupera el partido con la casilla del shell
    // (`setLayoutPartido` → recarga, la vía ya diseñada para «no hay envoltorio»).
    // ★ Y en una REPETICIÓN no se instancia, por ancho que sea el criterio: `soloUiOriginal`
    // manda sobre los tres (ver el bloque de `?replay=`). Va como conjunción AQUÍ y no dentro
    // de `layoutPartidoDisponible` a propósito — esa función es la regla de PREFERENCIA de
    // layout y sus tres entradas son las tres cosas que el usuario ha dicho sobre el layout;
    // una repetición no dice nada sobre el layout del usuario, dice que esta carga no es
    // suya. Meterlo dentro habría hecho que la preferencia y el régimen de presentación se
    // leyeran por el mismo grifo.
    const quiereLayoutPartido =
      !soloUiOriginal &&
      layoutPartidoDisponible(layoutPartidoGuardado(), banderaPartido, esPantallaTactil());
    // `toggleLayoutPartido` se define ABAJO (necesita `skins`); el envoltorio sólo recibe
    // una lambda que lo llama, así que el orden de declaración no importa.
    const portraitSkin =
      !quiereLayoutPartido ? null
      : new PortraitSkin(reflowMode === "off" ? "cuadrado" : reflowMode, () => toggleLayoutPartido());
    /**
     * ── LA CHAPA MÓVIL VIVA: clásica ⇄ Enhanced, SIN RECARGAR ────────────────────────
     *
     * 🔴 AQUÍ HUBO UNA RECARGA Y ERA LA DECISIÓN EQUIVOCADA. La escribí citando el
     * precedente de `toggleLayoutPartido` («recarga cuando no hay envoltorio que
     * conmutar») y midiendo el coste en el sitio erróneo: conté lo que costaba PROGRAMAR
     * el ir y volver, no lo que le costaba al JUGADOR. Reporte del usuario (13-09):
     * «enabling enhanced controls resets the game». Una recarga a mitad de partida tira
     * la sesión, y un ajuste de MANDOS no puede costar la partida. El precedente que cité
     * tampoco aplicaba: aquél recarga porque le falta un objeto que no existe (el
     * envoltorio del layout partido); aquí no faltaba nada, sólo había que escribir la
     * vuelta.
     *
     * ★ UNA SOLA RUTA, y es lo que hace esto seguro. El miedo escrito en `ui/touch.ts`
     * («armar/desarmar obliga a duplicar media `dispose()` — dos rutas de limpieza que
     * deben coincidir acaban divergiendo») es REAL, y por eso aquí no hay una ruta de
     * arranque y otra de conmutación: el arranque llama a ESTA MISMA función. Si la
     * vuelta se rompe, se rompe también el arranque — el camino que pisa todo el mundo.
     *
     * Lo que se intercambia son las DOS hojas de mandos, que no pueden convivir (la
     * clásica pone cuatro `display:grid !important` sobre `.touch-controls`):
     *   · clásica  → `installBotonesUi` + `installPortraitDeckDom` + el ▤ + el CSS del
     *                deck ancho cuando el layout partido está montado;
     *   · Enhanced → `mountEnhancedChrome`, que trae su propio `<style>` y su clase.
     *
     * El puente al teclado del SISTEMA (`deck-nativo.ts`) NO entra en el intercambio, a
     * propósito: lo instala la piel al montarse y es INERTE bajo Enhanced (su botón ⌨ y
     * su interceptor viven en `.touch-util`, que la chapa oculta), así que sobrevive al
     * viaje de ida y vuelta sin reconstruirlo. Menos que desmontar es menos que pueda
     * divergir.
     */
    let chapaEnhanced: EnhancedChromeHandle | null = null;
    let soltarDeckDom: (() => void) | null = null;
    let soltarBotonLayout: (() => void) | null = null;

    const quitarChapaClasica = (): void => {
      soltarBotonLayout?.();
      soltarBotonLayout = null;
      // Devuelve Enter/Esc/Espacio de las celdas de la cruceta a la fila útil (el propio
      // instalador recuerda padre y hermano de cada botón y restaura en orden inverso).
      soltarDeckDom?.();
      soltarDeckDom = null;
      uninstallWideDeck();
      uninstallBotonesUi();
    };

    const ponerChapaClasica = (): void => {
      installBotonesUi();
      // El CSS del deck ANCHO sólo tiene sentido con el envoltorio del layout partido
      // MONTADO: es su sub-variante. Se consulta la piel VIVA y no la preferencia, por el
      // mismo motivo que el ▤ (una preferencia rancia no puede decidir geometría).
      if (portraitSkin && skins.currentId === portraitSkin.id) installWideDeck("bloques");
      if (esPantallaTactil()) soltarDeckDom = installPortraitDeckDom();
      if (portraitSkin) {
        soltarBotonLayout = installLayoutToggleButton(() => {
          // POR EL LAYOUT VIVO, NO POR LA PREFERENCIA (diagnóstico smooth×portrait, PUNTO
          // 2). Negar la preferencia funciona sólo mientras nadie la desincronice; cuando
          // eso pasaba, la 1ª pulsación del ▤ era un no-op de layout que además MATABA la
          // piel elegida (medido: shader+clásico → ▤ → fiel, mismo layout). Leyendo lo que
          // hay MONTADO, el toggle no puede gastar un toque aunque la preferencia venga
          // rancia — y de paso la re-sincroniza.
          guardarLayoutPartido(skins.currentId !== portraitSkin.id);
          toggleLayoutPartido();
        });
      }
    };

    const aplicarChapaMovil = (enhanced: boolean): void => {
      if (soloUiOriginal) return; // repetición: no hay deck al que ponerle chapa
      if (enhanced) {
        quitarChapaClasica();
        // EXTRAS de la pestaña «System» del cajón. Hoy uno: el conmutador de LAYOUT.
        // 🔴 REPARA UNA AFORDANCIA QUE ESTA CHAPA SE LLEVÓ POR DELANTE. El botón ▤ vive
        // en la fila útil CLÁSICA, que Enhanced oculta, así que encender la chapa dejaba
        // el layout partido sin vía de un toque — y eso importa MÁS con Enhanced que sin
        // ella: medido a 390×844, con la chapa puesta el re-flow da un mapa de 390×565 y
        // el clásico uno de 390×244 (limitado por el ANCHO: el alto que la chapa libera
        // no se lo puede quedar). O sea que quien acabara en clásico veía el juego
        // pequeño entre dos franjas negras y sin botón con el que arreglarlo — que es
        // exactamente lo que el usuario reportó el 13-09.
        // La casilla del drawer del shell seguía existiendo; lo que faltaba era el atajo.
        chapaEnhanced ??= mountEnhancedChrome(
          portraitSkin
            ? [
                {
                  label: "Layout",
                  title: "Switch between the split and the original portrait layout",
                  run: () => {
                    // POR EL LAYOUT VIVO, no por la preferencia: mismo criterio que el ▤.
                    guardarLayoutPartido(skins.currentId !== portraitSkin.id);
                    toggleLayoutPartido();
                  },
                },
              ]
            : [],
        );
      } else {
        chapaEnhanced?.dispose();
        chapaEnhanced = null;
        ponerChapaClasica();
      }
      // La chapa nueva mide OTRA cosa: hay que re-publicar la reserva para que el canvas
      // se re-escale al hueco correcto. `refreshTouchDeck` no vale (sólo re-deriva el
      // contexto); el `resize` es la señal que `syncReserve` ya escucha.
      window.dispatchEvent(new Event("resize"));
    };

    if (portraitSkin) {
      skins.register(portraitSkin, {
        userFacing: false,
        label: "Vertical re-flow (prototipo)",
      });
    }
    // EL ARRANQUE VA POR LA MISMA PUERTA QUE EL CONMUTADOR (ver el docblock de arriba).
    aplicarChapaMovil(enhancedControlsActivo());
    // ★ #333 A1 × #336: el layout partido SIGUE AL RÉGIMEN en caliente (2-en-1). Sin
    // esto, la pieza B abría un agujero nuevo: al enchufar un ratón (régimen → escritorio)
    // el deck se apaga DE VERDAD —ya no lo resucita ningún `!important`— y el partido se
    // quedaba montado con la mitad de abajo muerta. Con envoltorio montado se
    // re-sincroniza por la MISMA vía que el ▤ (`toggleLayoutPartido`), sin tocar la
    // preferencia guardada: esto es una decisión de RÉGIMEN, no una elección del jugador,
    // y al volver a táctil la preferencia intacta restaura lo que hubiera.
    // Sin envoltorio (boot de escritorio) no se hace nada automático: montarlo exigiría
    // recargar la página (la vía diseñada de `toggleLayoutPartido` sin envoltorio) y una
    // recarga no pedida a mitad de partida no se compra sola — la vuelta es la casilla del
    // shell, ofrecida por régimen VIVO (ver `layoutPartidoDisponible` en las deps del
    // drawer, más abajo).
    if (portraitSkin) {
      onCambioRegimenTactil(() => {
        const activo = skins.currentId === portraitSkin.id;
        if (activo !== quiereLayoutPartidoAhora()) toggleLayoutPartido();
      });
    }
    // Gate modal del cruce de moongate (ver handleGameKey): pregunta a las DOS pieles;
    // sólo la MONTADA puede estar en transit (unmount limpia el estado). El autoWalk
    // del tap-to-walk también se corta al arrancar el cruce (mismo modal).
    moongateGate.bind(
      () => fielSkin.transiting || shaderSkin.transiting || (portraitSkin?.transiting ?? false),
    );
    view.subscribe({ onMoongateTransit: () => cancelAutoWalk() });

    // SPEAKER FIEL (task #3): el "modo 1988" es el sonido del PC-speaker. Como el
    // IntentSink, el sink de audio vive FUERA de la piel (composición root): escucha
    // los cues del core. Toggle persistente (default ON); F8 alterna.
    const speaker = new SpeakerAudio();
    view.subscribe({
      onSfx: (cue, leadMs) => {
        // El PC-speaker fiel suena en CUALQUIER piel sin audio propio (hoy dev+fiel).
        // Gatearlo a "faithful" dejaba MUDA la piel dev → el usuario no oía NADA en el
        // port (diag audio 2026-07-15). La decisión #10 (fiel=speaker/moderna=XMI) se
        // re-aplicará cuando exista una piel "moderna" con su propio sink de audio.
        // ★ #208 — `leadMs` es la espera de FASE del lote (planTurnPhase): así la
        // fanfarria del ritual suena TRAS la sacudida y la explosión (CAST 0x1759),
        // no encima. 0 en todos los caminos sin lote.
        speaker.play(cue, leadMs);
      },
    });

    // MENÚ SISTEMA (shell del UI externo): drawer con Partidas/Vídeo/Audio/Teclas
    // (+Debug en DEV). Reutiliza el motor del panel debug; DOM hermano del canvas
    // (cero impacto en el render fiel / paridad pixel). Entradas: Escape (con todo
    // cerrado), F10 y el botón ⚙.
    // La sonda del atlas se dispara AQUÍ (fire-and-forget, nunca lanza): el drawer
    // se construye perezosamente al abrirlo, así que para entonces ya respondió y
    // la sección Ayuda aparece sólo donde /companion está desplegado.
    // ── REPETICIONES: reproductor + panel ────────────────────────────────────────
    // El reproductor entrega las teclas por el MISMO camino que el teclado real (un
    // `KeyboardEvent` de ventana, como la botonera táctil): si usara una puerta trasera
    // dejaría de reproducir el juego y pasaría a reproducir la puerta trasera.
    let replayInjecting = false;
    const replayPlayer = new ReplayPlayer({
      restore: (anchor) => {
        // `applyLoadedState` ya desmonta escenas, timers y prompts vivos (la misma
        // limpieza que cargar una partida). Combate y mazmorra NO viven en el
        // GameState: son estado de proceso, así que se bajan a mano — rebobinar a una
        // tecla anterior puede pillarnos dentro de un combate que aún no existía.
        applyLoadedState(JSON.parse(anchor.state) as GameState);
        game.combat = null;
        game.dungeonState = null;
        game.reseed(anchor.seed); // el g_rng_seed no viaja en el save: sin esto, diverge
        view.notifyTurn([{ kind: "map-changed" }]);
        hud.refresh();
      },
      ready: inputReady,
      deliver: (e) => {
        replayInjecting = true;
        try {
          window.dispatchEvent(
            new KeyboardEvent("keydown", {
              key: e.key,
              code: e.numpad ? `Numpad${e.key}` : undefined,
              ctrlKey: e.ctrl ?? false,
              metaKey: e.meta ?? false,
              altKey: e.alt ?? false,
              bubbles: true,
            }),
          );
        } finally {
          replayInjecting = false;
        }
      },
      turn: () => game.state.turnsSinceStart,
      onChange: (s) => replayUi?.onPlayerStatus(s),
    });
    /** ¿Hay una repetición viva? Mientras la haya, el teclado REAL no toca la partida. */
    const replayActive = (): boolean => {
      const s = replayPlayer.status().state;
      return s !== "idle";
    };
    const replayUi: ReplayUiHandle = mountReplayUi(parent, {
      recorder: keyRec,
      player: replayPlayer,
      // El ancla ES un save: estado serializado + la semilla viva del RNG. Por eso
      // grabar se ofrece donde se puede guardar (fuera de combate y mazmorra): el
      // combate y la mazmorra son estado de PROCESO y no caben en el GameState, así
      // que un ancla tomada ahí no se podría restaurar.
      anchor: () => ({ state: JSON.stringify(game.state), seed: game.liveSeed() }),
      defaultLabel: () => `${mapName()} · ${ts("turn")} ${game.state.turnsSinceStart}`,
      message: (text) => hud.message(`${text}\n`),
      embedded: embebido,
      // F3: fuente de píxeles tras cada repintado.
      //
      // 🔴 EL HOOK SE INSTALA SIEMPRE Y DECIDE AL LLAMARSE, no al montarse — y esto se
      // corrigió DESPUÉS de medirlo: la primera versión copiaba el patrón del `afterRender`
      // del SavePanel (`...(condición ? {hook} : {})`), y la sonda dio el nombre de fila SIN
      // pixelizar con la piel fiel puesta. La condición se evalúa en el CONSTRUCTOR, y este
      // panel se monta ANTES del `skins.swap()` del final del arranque: en ese instante la
      // piel todavía no es la fiel, así que el hook no llegaba a instalarse nunca.
      // Y aunque el orden fuera otro, un gate congelado en el montaje no puede seguir a un
      // F9 EN CALIENTE — que es precisamente lo que este juego ofrece. `syncPixelFontReplays`
      // ya sabe revertir (recibe el booleano y hace pixelize/depixelize), así que dejarle a
      // él la decisión es más barato y además correcto en los dos sentidos del cambio.
      afterRender: (root: HTMLElement) =>
        syncPixelFontReplays(root, shellFrameC && shellThemeFor(skins.currentId) === "faithful"),
      // ✕ = SALIR DE LA REPETICIÓN, y sólo existe en la llegada por URL (`?replay=<id>`
      // sin `embed`, el enlace de la lista de partidas). Recargar SIN el parámetro es lo
      // que restituye la carga normal entera —piel, layout y deck del usuario, leídos de
      // sus preferencias— en vez de re-montar tres capas a mano y arriesgarse a que una
      // se quede fuera. No hay partida propia que perder: `forceFresh` incluye `replayId`,
      // así que esta carga nunca restauró ningún save.
      onExit:
        replayId === null ? undefined : (
          () => {
            const url = new URL(window.location.href);
            url.searchParams.delete("replay");
            window.location.replace(url.toString());
          }
        ),
    });
    replayActiveRef = replayActive;
    replayInjectingRef = () => replayInjecting;
    // ── LLEGADA CON `?replay=<id>`: la repetición que se pulsó en `/byo` ────────────
    // NAVEGANDO (pestaña propia) se carga y se deja PAUSADA en su primer fotograma.
    // Quien llega así acaba de cambiar de página y no está mirando todavía; arrancar
    // solo gastaría los primeros segundos de la partida contra una pantalla que aún
    // no se ve. La barra aparece con el ▶ listo, que dice a la vez qué es esto y qué
    // hacer. Un id inexistente NO deja el juego a medias: se avisa y se sigue con la
    // partida normal.
    //
    // EMBEBIDO (`?embed=1`, el popover de /byo) ARRANCA SOLO, y la asimetría es la
    // misma razón al revés: ahí el visitante NO ha cambiado de página — acaba de
    // pulsar «ver repetición» y está mirando el hueco donde debe aparecer. Un ▶ que
    // hay que buscar sobre un vídeo que además se quiere LIMPIO (la barra apartada,
    // ver arriba) sería pedirle que descubra un control invisible para que empiece lo
    // que ya pidió.
    if (replayId) {
      void getLog(replayId)
        .then((log) => {
          if (!log) {
            hud.message(`${ts("That recording is no longer on this device.")}\n`);
            return;
          }
          replayPlayer.load(log);
          if (embebido) replayPlayer.play();
        })
        .catch(() => hud.message(`${ts("Could not read the local recordings.")}\n`));
    }
    // Hooks e2e SÓLO DEV del carril de repeticiones: dan al arnés el MISMO grabador y el
    // MISMO reproductor que usa el usuario (no una réplica), más una HUELLA del estado
    // vivo. La huella es lo que hace que la ida y vuelta se compare por TRAYECTORIA y no
    // sólo por estado final: dos partidas distintas pueden reconverger (medido: alterar
    // una tecla divergía 15 pasos y volvía a coincidir en posición, turno Y semilla),
    // así que comparar sólo el final es un VERDE FALSO.
    if (import.meta.env.DEV) {
      const hooks = (window as unknown as Record<string, Record<string, unknown>>).__u5test;
      if (hooks) {
        hooks.replay = {
          startRec: () => keyRec.start({ state: JSON.stringify(game.state), seed: game.liveSeed() }),
          stopRec: (label = "e2e") => keyRec.stop(label),
          recLength: () => keyRec.length,
          fingerprint: () => `${game.liveSeed()}|${JSON.stringify(game.state)}`,
          load: (log: Parameters<typeof replayPlayer.load>[0]) => replayPlayer.load(log),
          step: () => replayPlayer.step(),
          play: () => replayPlayer.play(),
          pause: () => replayPlayer.pause(),
          seek: (n: number) => replayPlayer.seek(n),
          setSpeed: (x: number) => replayPlayer.setSpeed(x),
          status: () => replayPlayer.status(),
          dispose: () => replayPlayer.dispose(),
          openPanel: () => replayUi.open(),
        };
      }
    }

    void probeCompanion();
    shellPanel = new DebugPanel(
      parent,
      () =>
        buildShellSections({
          // Layout partido: la vía del SHELL. Es UNA de las dos, no la única —
          // 🔴 este comentario decía «la ÚNICA que puede DEVOLVERLO (el botón ▤ vive
          // dentro del layout partido y desaparece al apagarlo)» y describía el estado
          // ANTERIOR al 27-07, contradiciendo a su vecino de la línea 4533, que cuenta
          // cómo se sacó el ▤ FUERA del envoltorio precisamente para que sobreviva al
          // cambio de layout. Peligroso por dónde estaba: es la respuesta que alguien
          // viene a buscar aquí, y era la alarmante y falsa.
          // Hoy hay DOS vías de vuelta desde el clásico, y el envoltorio se instancia
          // aunque la preferencia diga «clásico» (`layoutPartidoDisponible`, más ancha
          // que el arranque a propósito): el ▤ y esta sección del shell.
          // ★ #333 A1 (16-08): por RÉGIMEN VIVO, no por `portraitSkin !== null` (una foto
          // del boot). Las dos direcciones importan: en escritorio la casilla desaparece
          // (el partido ni se ofrece — el drawer se reconstruye al abrirse, así que un
          // 2-en-1 la re-evalúa gratis), y en un boot de ESCRITORIO que pasó a táctil
          // aparece AUNQUE no haya envoltorio: `setLayoutPartido` sin envoltorio recarga
          // la página (vía diseñada), que es la única vuelta al partido en ese estado.
          layoutPartidoDisponible: () =>
            !soloUiOriginal &&
            layoutPartidoDisponible(layoutPartidoGuardado(), banderaPartido, esPantallaTactil()),
          layoutPartido: () => skins.currentId === portraitSkin?.id,
          setLayoutPartido: (on: boolean) => {
            guardarLayoutPartido(on);
            toggleLayoutPartido();
          },
          currentSkinId: () => skins.currentLabel,
          availableSkins: skinChoices, // anotadas con la consecuencia (ver skinChoices)
          selectSkin,
          // ── HERENCIA DEL POPOVER ☰ (ficha #154) ────────────────────────────────────
          // El ☰ del deck ya no abre un menú intermedio: emite F10 y cae aquí. Sus dos
          // entradas sin casa previa (🌐 idioma y ⇄ lado del pad) se sirven ahora como
          // filas del drawer. El idioma sale de las MISMAS deps que el FAB 🌐 de
          // escritorio (`LANG_SWITCHER_DEPS`, arriba): una sola fuente, dos superficies.
          languages: LANG_SWITCHER_DEPS.choices,
          currentLang: LANG_SWITCHER_DEPS.currentCode,
          selectLang: LANG_SWITCHER_DEPS.selectLang,
          // ── MANDOS ENHANCED (fase 1 de la auditoría móvil) ────────────────────────
          // EN CALIENTE: la partida NO se pierde al cambiar de mandos. La ida y la
          // vuelta las sirve `aplicarChapaMovil`, la MISMA función que usa el arranque
          // (ver su docblock, y por qué la recarga que había aquí era un error mío).
          enhancedControlsDisponible,
          enhancedControls: enhancedControlsGuardado,
          setEnhancedControls: (on: boolean) => {
            guardarEnhancedControls(on);
            aplicarChapaMovil(on);
          },
          // ── INTERFAZ DE LANZAMIENTO (Clásico / Lista de hechizos) ─────────────────
          // EN CALIENTE y sin re-montar nada: `pickSpellForCast` consulta el régimen en
          // CADA (C)ast, así que el cambio vale para el hechizo siguiente. No hay DOM
          // instalado que reconstruir (a diferencia de los mandos Enhanced), y por eso
          // esta fila no avisa de recarga: no la hay.
          castingUi: castingUiGuardado,
          setCastingUi: (ui: CastingUi) => guardarCastingUi(ui),
          // ── INTERFAZ DE TIENDA (Clásico / Moderno) ────────────────────────────────
          // EN CALIENTE y sin re-montar nada, por lo mismo que la fila de arriba:
          // `syncShopPanel` consulta el régimen en la cola de cada tecla, así que el
          // cambio vale para la fase de tienda siguiente — incluso con el mercader ya
          // abierto. No hay DOM instalado que reconstruir, y por eso esta fila tampoco
          // avisa de recarga: no la hay.
          shopUi: shopUiGuardado,
          setShopUi: (ui: ShopUi) => guardarShopUi(ui),
          padSideDisponible: padSideOfrecible,
          // ── POSICIÓN DE LA CRUCETA (izquierda · centro · derecha) ─────────────────
          // El gate es la CHAPA VIVA y no la preferencia guardada: `data-u5e-pad` sólo lo
          // consume el CSS de Enhanced, así que sin chapa montada el selector no movería
          // nada — y `chapaEnhancedViva()` es el mismo predicado que el resto del fichero
          // usa para «¿hay chapa?». Con él en `false`, la fila no se pinta y el ⇄ clásico
          // ocupa su sitio (ver la dep en `shell/sections.ts`).
          padPosDisponible: () => chapaEnhancedViva() && padSideOfrecible(),
          padPos: loadPadPos,
          setPadPos: (pos: PadPos) => {
            setPadPos(pos);
            // 🔴 …Y EL LADO CLÁSICO VA DETRÁS, que es lo que hace que esta fila SUSTITUYA al
            // ⇄ en vez de competir con él: en APAISADO el deck no es una banda inferior sino
            // una columna pegada a un borde, y ese borde lo sigue diciendo `data-pad-side`.
            // Sin esta línea, elegir «derecha» en vertical dejaba el raíl apaisado a la
            // izquierda — dos mandos contradiciéndose, que es el defecto #183 otra vez.
            // «Centro» NO toca el lado: no hay columna centrada, así que el raíl se queda
            // donde estuviera (el razonamiento, en `enhanced/mobile/padpos.ts`).
            if (pos !== "center" && padSideVivo() !== pos) swapPadSide();
          },
          // ── LAS CUATRO ACCIONES RÁPIDAS (sólo el juego del MUNDO) ────────────────
          // Mismo gate. Las OPCIONES salen del censo `WORLD_BUTTONS` tal cual: este
          // fichero no escribe ni un rótulo ni una tecla de comando.
          quickSlotsDisponible: () => chapaEnhancedViva(),
          quickSlotKeys: loadQuickWorld,
          quickSlotOptions: () => quickOptions().map((d) => ({ label: d.label, key: d.key })),
          setQuickSlot,
          // ── #183 (REPORTE DEL USUARIO 11-08: «Swap buttons: el cursor no funciona») ──
          // El ⇄ del drawer alternaba SIEMPRE `data-pad-side`, y en el layout PARTIDO
          // VERTICAL —el defecto en táctil desde el 02-08— ese atributo no tiene ningún
          // consumidor: sus dos reglas (`index.html`) ponen `row-reverse` sobre
          // `.touch-main`, que ahí es `display:contents`, y sobre `.touch-util`, que ahí
          // es una COLUMNA (invertir una columna con `row-reverse` no hace nada). MEDIDO
          // a 390×844: pulsarlo cambia el atributo de `left` a `right` y la cruceta, los
          // comandos, la columna útil y el relleno de `#app` quedan EN EL MISMO PÍXEL.
          //
          // Y el mecanismo que sí mueve el cursor en ese layout —`data-cursores-lado`,
          // el de la columna de accesos— existe y funciona (forzado a mano: la cruz salta
          // de x=242 a x=12 y las tres columnas se intercambian), pero SU botón está
          // `display:none` desde el 27-07 (`deck-ancho.ts`), cuando la spec del usuario
          // dejó esa columna en cuatro ítems. O sea: los dos controles quedaron CRUZADOS
          // —el alcanzable es inerte y el efectivo está oculto—, que es exactamente lo
          // que el usuario describe.
          //
          // El arreglo no duplica el estado: NO se escriben los dos atributos (dos fuentes
          // para «de qué lado va el deck» pueden discrepar en cuanto se toque una sola).
          // Se despacha al que el layout VIVO consume, y la pregunta la contesta el propio
          // módulo del layout, no una condición cableada aquí.
          swapPadSide: () => {
            if (ladoCursoresEsElVivo()) alternarLadoCursores();
            else swapPadSide();
          },
          musicEnabled: () => music.enabled,
          setMusicEnabled: (on) => music.setEnabled(on),
          musicStatus: () => music.diagnostico,
          musicVolume: () => music.volumeLevel,
          setMusicVolume: (v) => music.setVolume(v),
          speakerEnabled: () => speaker.enabled,
          setSpeakerEnabled: (on) => speaker.setEnabled(on),
          // Guardar: el panel QoL se auto-guarda en combate/mazmorra
          // (`!dungeonState && !combat`), igual que su tecla (F5 queda tras el
          // early-return de combate/mazmorra en handleGameKey). Así tecla==botón en
          // todo contexto. Esto es COHERENCIA INTERNA del port, no fidelidad: el
          // guardado libre es QoL declarada NO fiel (`core/persistence.ts:2` — U5
          // tenía un único SAVED.GAM), así que restringirlo no lo acerca al binario;
          // sólo evita que el botón haga algo que su tecla no hace.
          openSaves: () => {
            if (!game.dungeonState && !game.combat) {
              cancelAutoWalk();
              savePanel.show(game.state, mapName());
            }
          },
          // Repeticiones: MISMA restricción que guardar, y por la misma razón — el
          // ancla de una grabación ES un save, y ni el combate ni la mazmorra caben
          // en el GameState.
          openReplays: () => {
            if (!game.dungeonState && !game.combat) {
              cancelAutoWalk();
              replayUi.open();
            }
          },
          // Sólo DEV: la sección "Debug (QA)" y la fila de teclas del atajo se pintan
          // sólo si `openDebug` existe (mismo gate que el drawer QA, `debugMenu`).
          openDebug: debugMenu ? () => debugMenu.open() : undefined,
          companionAvailable,
          // Revocación (UE): la vía del shell es la ÚNICA que tiene el jugador para
          // retirar un permiso sin volver a la portada.
          openPrivacidad: () => {
            shellPanel?.close();
            syncShellExpanded(false);
            consentimiento.panel.abre();
          },
          close: () => { shellPanel?.close(); syncShellExpanded(false); },
        }),
      () => { shellPanel?.close(); syncShellExpanded(false); },
      // Chrome del drawer en el idioma activo (funciones ⇒ se re-resuelven al invalidar).
      {
        title: () => ts("SYSTEM"),
        badge: () => ts("MENU"),
        searchPlaceholder: () => ts("Filter fields…"),
        testId: "u5-shell-drawer",
        // PANEL DE AJUSTES CON CATEGORÍAS (rediseño de ajustes): el cuerpo deja de ser un
        // acordeón de once secciones apiladas y pasa a raíl + contenido (escritorio) /
        // lista → detalle (teléfono). Las tres funciones van como funciones —no como
        // valores— por la MISMA razón que el título y el badge de arriba: `invalidate()`
        // las re-resuelve al cambiar de idioma, así que los rótulos del navegador siguen
        // al idioma vivo igual que los de las filas.
        categories: buildShellCategories,
        navBack: () => ts("Back"),
        navCategories: () => ts("Settings categories"),
        // #263 (directriz del usuario 14-08): sin el `esc` de la esquina. La salida
        // rotulada pasa a la fila «Close menu» del propio drawer, con el mismo testid
        // (ver `DebugPanelOpts.closeButton` y la sección `shell-close`).
        closeButton: false,
        // Tras (re)construir (open / cambio de idioma): pixeliza el texto si la piel es fiel.
        afterBuild: () =>
          syncPixelFont(shellPanel?.rootEl, shellThemeFor(skins.currentId) === "faithful"),
      },
    );
    // CLÚSTER DE FAB DEL SHELL (#11, veredicto usuario 2026-07-19): los tres FAB (🌐 idioma ·
    // ◧ piel · ⚙ sistema) como overlay DISCRETO en la esquina inferior-derecha del viewport,
    // FUERA del chrome del juego (revierte la «Fusión B» que los fundía en el rótulo del panel).
    // Se suprimen mientras haya un popup del juego abierto para no taparlo (ver shellToolbar.ts).
    const shellToolbar = mountShellToolbar(parent, {
      gearToggle: toggleShellDrawer,
      skinDeps: {
        choices: skinChoices, // anotadas con la consecuencia (ver skinChoices)
        currentId: () => skins.currentId,
        selectSkin,
      },
      langDeps: LANG_SWITCHER_DEPS,
    });
    // Al cambiar de idioma en caliente: la barra repinta los FAB de idioma/piel y el tooltip
    // del ⚙; además invalida el drawer SISTEMA (sus labels se generan en `buildShellSections`,
    // que corre una vez → invalidate fuerza rebuild).
    onLangChange(() => {
      shellToolbar.refresh();
      shellPanel?.invalidate();
    });

    // VEREDICTO #23 (default): viste el panel de partidas y el drawer SISTEMA con el marco
    // del UI original (banda ►título◄ + marco de pergamino con rizos), sobre el chrome EGA/
    // 8×8. Activo salvo `?shellVector=1`. Se refresca el título al cambiar idioma.
    if (shellFrameC) {
      const saveFrame = mountOriginalFrame(savePanel.rootEl, () => ts("Journeys"));
      const drawerFrame = shellPanel
        ? mountOriginalFrame(shellPanel.rootEl, () => ts("SYSTEM"))
        : null;
      // #162 — EL MARCO YA SÍ SE MONTA EN EL PANEL DE REPETICIONES, y la causa NO era la
      // que el repliegue de F3 dejó escrita («el overlay del marco se ancla MAL dentro de
      // esa tarjeta»). El anclaje era CORRECTO: medido el 11-08 con la tarjeta abierta, el
      // cuerpo negro sale en 536×211 sobre una tarjeta de 560×245 —exactamente
      // `w−2·BORDER` × `h−BAND_H−BORDER`— y SIGUE cuadrando al forzar que la tarjeta crezca
      // (560×532 → cuerpo 536×498) y al encogerla otra vez. La geometría nunca falló.
      // Lo que fallaba era que el marco **no se pintaba**: el overlay usa `z-index:-1`, que
      // en un huésped SIN contexto de apilado cae detrás del fondo OPACO del propio
      // huésped. El drawer SISTEMA es `position:fixed; z-index:99999` (contexto ⇒ se ve);
      // la tarjeta era `position:relative; z-index:auto` (no ⇒ azul liso). Ésa es la
      // respuesta a la pregunta que F3 dejó abierta. El arreglo es una línea en
      // `ui/shell/originalFrame.ts` (`.u5of-host{isolation:isolate}`) y va allí, no aquí,
      // porque el requisito lo impone el MARCO y no cada panel.
      // ⚠ Y por qué no se localizó antes: las CIFRAS daban bien: la caja del cuerpo estaba
      // al píxel. Sólo la CAPTURA enseña que no hay nada pintado.
      const replayFrame = mountOriginalFrame(replayUi.cardEl, () => ts("Replays"));
      onLangChange(() => {
        saveFrame.refresh();
        drawerFrame?.refresh();
        replayFrame.refresh();
      });
    }

    // #23b — DISMISS POR FOCO-FUERA: un clic/tap FUERA de un drawer/popup del shell
    // ABIERTO lo cierra (convención de modal). Listener en CAPTURA a nivel window: si hay
    // un popup del shell abierto y el pointerdown cae fuera de él (y fuera de los FAB/menús
    // del shell, que se autogestionan), lo cierra y CORTA el evento con stopPropagation en
    // captura — así el clic exterior SÓLO cierra y NO llega al tap-walk del canvas
    // (skin/fiel escucha `pointerdown` en el canvas). El clic tras cerrar es uno nuevo: no
    // camina de rebote. Los switchers ◧/🌐 ya tienen su propio cierre por clic-fuera.
    const SHELL_POPUP_SEL =
      '.save-panel,[data-testid="u5-shell-drawer"]';
    const SHELL_SELF_MANAGED_SEL =
      ".u5shell-gear,.u5skinsw,.u5langsw,.u5skinsw-menu,.u5langsw-menu";
    const anyShellPopupOpen = (): boolean =>
      (shellPanel?.isOpen ?? false) ||
      savePanel.visible;
    const closeOpenShellPopups = (): void => {
      if (shellPanel?.isOpen) { shellPanel.close(); syncShellExpanded(false); }
      if (savePanel.visible) savePanel.hide();
    };
    window.addEventListener(
      "pointerdown",
      (ev) => {
        if (!anyShellPopupOpen()) return;
        const t = ev.target as Element | null;
        if (t && (t.closest(SHELL_POPUP_SEL) || t.closest(SHELL_SELF_MANAGED_SEL))) return;
        ev.stopPropagation();
        closeOpenShellPopups();
      },
      true,
    );

    // ── HOOK e2e READ-ONLY: SUMIDEROS DE ENTRADA VIVOS (sólo DEV) ──
    // `handleGameKey` (arriba) tiene una escalera de early-returns que se TRAGA el teclado:
    // pacers a reloj de pared (acampada, refugio de party-wipe, sneak de trolls, cruce de
    // moongate, endgame), vistas modales que se cierran con cualquier tecla (gema/zodíaco),
    // el reductor de prompts, los paneles DOM y los bucles de combate/mazmorra. Un arnés
    // que quiera pulsar una tecla de SHELL (el F5 del export de checkpoint del espejo) no
    // puede distinguir "no ha llegado" de "la ha comido X" sin esto — y ADIVINARLO costó
    // una rotura de cadena (ad21 del relevo-3b: el diag culpó al drawer y el bloqueador
    // real era `refuging`, que NINGUNA ráfaga de teclas puede despejar porque el pacer
    // corre a reloj de pared y hay que ESPERARLO). Se declara aquí (final del boot) para
    // que todo lo que lee esté ya construido. Read-only puro: cero efecto en estado.
    if (import.meta.env.DEV) {
      const hooks = (window as unknown as Record<string, unknown>).__u5test as
        | Record<string, unknown>
        | undefined;
      if (hooks) {
        hooks.inputSinks = (): Record<string, unknown> => ({
          // Pacers a RELOJ DE PARED: no se despejan con teclas, hay que esperarlos.
          camping: campSleepCtl.camping,
          bedSleeping: bedSleepCtl.sleeping,
          refuging,
          trollSneak: trollSneakCtl.active,
          moongate: moongateGate.transiting,
          endgame: endgamePacer.active,
          // Modales que consume CUALQUIER tecla.
          gemView: canvasGemActive,
          zodiac: zodiacActive,
          // Reductor de prompts + prompts direccionales de mazmorra.
          prompt: prompts.current?.type ?? null,
          dungeonKlimbPrompt: pendingDungeonKlimb,
          dungeonDirPrompt: pendingDungeonSearch !== null || pendingDungeonLook,
          // Paneles DOM que capturan el teclado (Escape los cierra).
          save: savePanel.visible,
          selector: selector.visible,
          // Drawers del shell: NO hacen early-return, pero `DebugPanel` hace
          // stopPropagation de todo keydown originado DENTRO de su root — así que con el
          // foco dentro de un drawer (incluso CERRADO) las teclas no llegan a window.
          drawerOpen: (debugMenu?.panel.isOpen ?? false) || (shellPanel?.isOpen ?? false),
          focusInDrawer: Boolean(
            (document.activeElement as Element | null)?.closest(".u5dbg-drawer"),
          ),
          // Bucles que se quedan el teclado entero.
          combat: Boolean(game.combat),
          dungeon: Boolean(game.dungeonState),
        });
        // Nº monotónico de prompts ARMADOS (#374). NO va dentro de `inputSinks` (aquél
        // responde «¿quién se come mis teclas?»; esto responde «¿es OTRO prompt el que
        // las come?»): en las cadenas de getstring del rito de santuario el siguiente se
        // re-arma SÍNCRONO dentro del keydown del Enter —fiel a CAST2 0x0a1b→0x0a0c, sin
        // beat—, así que `inputSinks().prompt` nunca deja de ser "text" entre eslabones y
        // el arnés (`submitPrompt` de e2e/helpers) distingue la resolución por seq.
        // Read-only puro: cero efecto en estado.
        hooks.promptSeq = (): number => prompts.seq;
        // ¿Hay un efecto AV transitorio pintándose AHORA? (#207). Lo consume el GRABADOR de
        // partidas (`game/tools/partida-render.mjs`), que hasta #207 avanzaba un presupuesto
        // FIJO de 6 subfotogramas por paso de replay y por eso cortaba toda animación más
        // larga que eso — la coreografía del Shard salía decapitada en el vídeo publicado.
        // 🔴 NO es un `inputSinks` más y por eso no va dentro de aquél: `inputSinks` responde
        // «¿quién se come mis teclas?» y esto responde «¿ha terminado de pintarse la escena?».
        // Meterlo ahí habría hecho que un arnés de teclas esperase por una explosión.
        // El predicado NO se replica aquí: es el MISMO getter que gobierna el repintado del
        // bucle rAF de la piel (`skin/fiel/skin.ts`, `get transientFxActive`). Una réplica se
        // habría quedado corta el día que alguien añada una capa de fx, y el grabador volvería
        // a cortar — sin síntoma, que es exactamente como #207 llegó hasta la publicación.
        //
        // 🔴 SE LE PREGUNTA AL **MANAGER**, NO A `fielSkin`, y esto es un defecto MEDIDO que
        // esta línea tuvo durante todo un sello verde. `ShaderSkin` instancia SU PROPIA
        // `FaithfulSkin` interna, así que con la shader activa —que es el régimen del VÍDEO,
        // el canvas compuesto— los fx viven en ESA instancia y `fielSkin` no recibe nada:
        // el predicado devolvía `false` para siempre y el grabador seguía cortando los
        // efectos con el arreglo puesto, 17 tests en verde y la batería sellada. Lo cazó un
        // control positivo de 40 s (empujar `quake` por `applyEvents` y ver si sube), no los
        // tests. El sujeto correcto es LA PIEL ACTIVA, y sólo el manager sabe cuál es.
        // `null` = no hay piel montada o la activa no declara la propiedad; el grabador
        // ABORTA con él en vez de leerlo como «no hay animación».
        hooks.fxActive = (): boolean | null => skins.transientFxActive;
      }
    }

    // Piel inicial: la FIEL es el default (primera clase); `?skin=dev` la fuerza a dev.
    // `bootSkin` parte de `initialSkin` pero refleja el cambio de piel que el usuario
    // haya hecho DURANTE la intro (F9 / switcher, task #79 ampliado).
    // Con la bandera del prototipo puesta se monta el ENVOLTORIO (que aloja a la fiel
    // dentro): la piel de juego sigue siendo la fiel, sólo se re-compone su canvas.
    // ARRANQUE: manda la PREFERENCIA guardada sobre la bandera de URL cuando existe.
    // Sin esto, el apagado no sobrevivía a la recarga en el deploy —cuya URL lleva
    // `reflow=cuadrado`—: el jugador apagaba el layout, recargaba y volvía el partido, o
    // sea que el toggle parecía roto. La bandera sigue mandando la PRIMERA vez (nadie ha
    // elegido aún) y para forzar/medir; en cuanto el jugador toca el botón o la casilla, su
    // elección gana.
    // ★ 02-08: y si NO hay ni preferencia ni bandera, el defecto de fábrica ya no es
    // «clásico» sino «partido en táctil» (encargo del usuario). Toda la precedencia vive en
    // `layoutPartidoInicial`; aquí sólo se consume.
    const arrancarPartido = quiereLayoutPartidoAhora();
    const conEnvoltorio = Boolean(portraitSkin) && arrancarPartido;
    // …y con el envoltorio ganando, la preferencia de piel se RESPETA (diagnóstico
    // smooth×portrait, PUNTO 3 — cerrado del todo por el LOTE C).
    //
    // Historia de este par de líneas, porque importa: el envoltorio sólo podía alojar la
    // fiel, así que un `u5.skin=shader` guardado se ignoraba EN SILENCIO (el localStorage
    // decía «shader» y el jugador veía 1988). El lote A no podía arreglarlo, sólo dejar de
    // mentir, y normalizaba la preferencia a «faithful» — pisando la elección del jugador.
    // Ahora el envoltorio aloja las dos, así que no hay nada que normalizar: se monta el
    // layout partido CON la piel elegida dentro, y el localStorage y la pantalla dicen lo
    // mismo sin que nadie tenga que ceder.
    if (conEnvoltorio) await portraitSkin!.setHosted(pielAlojable(bootSkin));
    await skins.swap(conEnvoltorio ? portraitSkin!.id : bootSkin);
    // Último escalón del embudo (portada → /byo → carpeta → extracción → AQUÍ): el
    // juego ha arrancado de verdad, con assets y piel montada. Se descarta solo si
    // no hay consentimiento.
    consentimiento.analitica.evento(EV.JUEGO_ARRANCADO);
    // ── MODO CAPTURA `?shot=<id>` (momentos legendarios) ─────────────────────────────
    // Un momento se siembra sin foto porque nadie lo ha jugado, y la única forma
    // EA-limpia de tener una es generarla en el navegador del visitante desde SU
    // extracción. En vez de extraer el renderizador (que vive dentro de `mount` junto al
    // teclado y el rAF, `skin/fiel/skin.ts:2155-2180,2399,2651`), /byo abre este mismo
    // juego en un iframe oculto con `?save=<id>&embed=1&nointro&shot=<id>`: el arranque
    // YA restaura el save antes del primer render —a propósito, `main.ts:430-436`— así
    // que aquí abajo la pantalla ES el momento.
    //
    // 🔴 INERTE SIN EL PARÁMETRO. Toda la rama cuelga de `shotParam`, y sin él no se
    // captura, no se escribe y no se emite nada: cero cambio de comportamiento en
    // cualquier arranque normal (condición del permiso para tocar este fichero, que está
    // bajo embargo; aserto en `re/tools/test_byo_momentos.py`).
    //
    // 🔴 Y NO DUPLICA EL FORMATO: llama a `captureScreenshot`, el mismo de (Q)uit&Save.
    // El carril del zoom está pasando la miniatura a PNG nativo 320×200 en
    // `ui/screenshot.ts`; heredarlo es automático porque aquí no hay ninguna constante
    // que se quede rancia.
    const shotParam = bootParams.get("shot");
    if (shotParam) {
      // 🔴 DOS `requestAnimationFrame` ANTES DE CAPTURAR, y esto está MEDIDO: sin la espera
      // la foto salía NEGRA (913 B de JPEG, un rectángulo liso que yo mismo di por bueno
      // hasta abrir el fichero). La causa NO es que el juego no pinte: la piel fiel deja su
      // backbuffer de 320×200 listo dentro de `mount`, pero `captureScreenshot` elegía el
      // canvas MAYOR y ése es el de la piel shader (960×600), que compone por rAF. Justo al
      // salir de `skins.swap` ese canvas existe y todavía está en blanco. Medido en un iframe
      // fuera de pantalla: a t+1 s tiene 57 colores distintos. Dos rAF = «después del
      // siguiente pintado», que es exactamente la condición que faltaba. Que baste se
      // comprueba en la sonda, que asevera que la imagen NO es lisa.
      //
      // 🔴 LA CAUSA CITADA ARRIBA ESTÁ EN PASADO DESDE #153, Y LA ESPERA SE QUEDA IGUAL. Hoy
      // `captureScreenshot` prefiere el búfer DECLARADO por la piel fiel, que es justo el que
      // ya estaba listo — así que el mecanismo exacto del negro de aquel día no puede volver
      // a darse por esta vía. La espera NO sobra: la guarda de contenido rechazaría un búfer
      // en blanco y la captura caería al canvas del shader, que sigue componiendo por rAF; y
      // el estado del juego que se quiere fotografiar tampoco está pintado antes del frame.
      // Se deja el párrafo en pasado en vez de borrarlo porque explica POR QUÉ hay una espera
      // que, leída hoy, parecería gratuita — y una espera sin razón escrita es una espera que
      // alguien quita.
      await new Promise<void>((r) => requestAnimationFrame(() => requestAnimationFrame(() => r())));
      const shot = captureScreenshot(parent);
      // `writeSaveShot` exige que la ranura exista (save-keys.ts) — sin eso, un id
      // equivocado dejaría una miniatura huérfana. El resultado viaja al padre TAL CUAL:
      // /byo decide, y un fallo aquí no rompe nada (el momento queda sin foto y la
      // tarjeta ya sabe caer al minimapa o al nombre del lugar).
      const ok = shot !== null && writeSaveShot(shotParam, shot);
      // Origen EXPLÍCITO y no `"*"`: este mensaje sólo le interesa a la página que abrió
      // el iframe, que es del mismo origen por construcción (el ensamblado deja
      // /byo.html y /play.html en la raíz del sitio).
      window.parent?.postMessage({ tipo: "u5:shot", id: shotParam, ok }, window.location.origin);
    }
    // FASE 3: tema inicial del shell según la piel de arranque (swap no llama
    // afterSkinChange). Por la VISIBLE: con el envoltorio montado el id crudo es
    // "portrait" y desde el lote C eso ya no implica «fiel».
    applyShellTheme(pielVisibleId());
  } catch (err) {
    const div = document.createElement("div");
    div.id = "error";
    div.textContent = String(err instanceof Error ? err.message : err);
    parent.appendChild(div);
    throw err;
  }
}

void boot();
