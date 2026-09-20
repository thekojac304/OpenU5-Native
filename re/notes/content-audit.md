# Auditoría de Contenido Jugable — Guía/Manual/Claims vs Port (Tarea #49)

> **Qué es esto.** Minado de TODA la documentación local del repo (manual del Avatar,
> guía, `docs/research/claims-brutos.md`, corpus del Grand Tour) contra el port, para
> localizar mecánicas/secretos/items descritos por las fuentes que el port **no cubre**
> (flags = candidatos a gap) o cubre (para dar tranquilidad). Fase 1 = LOCAL. La salida
> alimenta la decisión del usuario sobre una fase 2 de research por internet.
>
> **Método.** 6 subagentes de dominio en paralelo, cada uno extrae afirmaciones jugables
> de las fuentes y las clasifica contra `game/src` + `re/notes/*` (grep obligatorio de las
> derivaciones ASM ya hechas antes de flagear). Jerarquía de autoridad: **asm > game data
> > cluebook > walkthroughs > internet**. Las ~80 claims NO verificadas de
> `claims-brutos.md` se trataron como HIPÓTESIS, no verdades.
>
> Fecha: 2026-07-16 · Rama: `re/content-audit` · Solo lectura del port.

---

## RESUMEN EJECUTIVO

**El port está sorprendentemente completo en el NÚCLEO (`game/src/core/*`).** Combate,
magia (definiciones/reactivos/costes/mezcla/cast-input), transporte, moongates, klimb,
search, shrines, gitana, karma, schedules de NPC, quest principal (artefactos + shards +
Shadowlords + ritual + Words of Power + rescate en Doom) están implementados y en gran
parte verificados byte-a-byte con arneses de paridad. La mayoría de los "gaps" reales NO
son de lógica ausente sino de **cableado a la UI / verbo de juego ausente**: funciones
fieles ya portadas que **ningún comando jugable alcanza**.

> **NATURALEZA DE ESTA MATRIZ — DETECTOR, no verificación ASM.** Esta auditoría cruza la
> DOCUMENTACIÓN (guía/manual/claims/corpus) contra el CÓDIGO del port para *detectar* dónde
> mirar; NO es una derivación byte-a-byte del binario. Un "CUBIERTA" aquí significa "el port
> tiene código que parece cubrirlo", no "verificado contra el ASM". La jerarquía de autoridad
> del proyecto sigue siendo **asm > game data > cluebook > walkthrough > internet**: cuando una
> fila afirme fidelidad de comportamiento, ciérrala con su derivación propia (ver el Apéndice
> ASM de #51 como ejemplo del rigor exigido: el testigo/la guía dicen QUÉ existe; el binario
> dice CÓMO). No leer ninguna celda de esta matriz como verdad derivada.

### Cifras (≈203 afirmaciones auditadas)

| Dominio | Total | CUBIERTA | FLAG | PENDIENTE |
|---|---:|---:|---:|---:|
| Magia & Words of Power | 33 | 22 | 8 | 3 |
| Combate & criaturas | 38 | 38 | 0 | 5 |
| Quest / Endgame / Shadowlords | 26 | 21 | 1 | 4 |
| NPCs / Diálogo / Shrines / Karma | 38 | 28 | 9 | 1 |
| Transporte / Mundo / Moongates | 45 | 41 | 2 | 2 |
| Economía / Tiendas / Items | 18 | 12 | 6 | 0 |
| **TOTAL** | **~203** | **~162** | **~26** | **~15** |

(Los FLAG de magia/combate/dungeon caen casi todos bajo tareas ya abiertas #42/#44/#47;
ver TOP-10. Los "PENDIENTE" son no-verificables sin derivación ASM o cinemáticas de
`MISCMAPS.DAT` = tarea #20.)

### TOP-10 FLAGS PRIORIZADOS (para decidir fase 2)

1. **[ALTA] GuildMaster (mercado negro) NO funcional en UI** — el ÚNICO gap que puede
   ahogar progresión. `main.ts:1031` mapea el tipo 0x86→GuildMaster y abre panel, pero
   `ui/shop.ts render()` no tiene rama GuildMaster → cae al placeholder "(llegará en fase
   posterior)". El núcleo `buyGuildItem`/`guildPrice` existe y es byte-fiel (solo en
   `__parity__`). El Guild es el **único vendedor de llaves/gemas/antorchas** → hoy solo
   se consiguen por loot aleatorio. Afecta economía de luz, ganzúa (Jimmy) y (V)iew map.
   Arreglo ≈15 líneas espejando la rama MagicSeller.
2. **[MEDIA] Caja de sándalo INOBTENIBLE en juego** → mata el final "bueno". El objeto
   está colocado (`npcs.json` loc 17 slot 31 tipo 14, junto al pasadizo del clavicémbalo)
   y la pared SÍ se abre (`harpsichord.ts` melodía `6789878765653`), pero
   `manager.ts:npcSlotObjectKind` clasifica el tipo 14 como `"prop"` inerte y
   `commands.ts:applyLootGrant` trata `"sandalwood"` como no-op → `specialItems.woodenBox`
   nunca se activa fuera de debug/save. La rama del final que ya está codificada
   (`lordbritish.ts:174 if woodenBox`) queda muerta. No bloquea la victoria base.
3. **[MEDIA] Verbo (U)se para herramientas de endgame ausente** — el dispatcher de (U)se
   (`main.ts:1950`, `game.ts`) solo maneja Skull Key + Shard. Quedan sin activar por juego
   legítimo: **HMS Cape** (doble velocidad de fragata; el EFECTO `navalStepCost` sí es
   fiel), Spyglass, Sextant, Pocket Watch, Black Badge, y la propia caja de sándalo. Save
   byte 0x215 / registro debug los activa, pero no hay ruta jugable. Ligado a #20/#43.
4. **[MEDIA] Casteo de HECHIZOS EN COMBATE no aplicado** — todos los efectos ofensivos
   (Grav Por, Vas Flam, Xen Corp), muros de campo, línea-AoE, invocaciones, charm/miedo
   /polimorfia devuelven descriptores EXACTOS desde `cast.ts` pero `combat.ts` nunca
   consume un `CastEffect`. Un jugador en combate no puede lanzar un hechizo con efecto.
   **Ya rastreado como tarea #44** (+ #42 saving-throw/confusión/time-stop).
5. **[MEDIA] Hechizos de MAZMORRA no cableados** — Uus Por/Des Por (±planta) e In Lor (luz
   en la vista 3D) devuelven descriptores pero no mueven al party ni iluminan.
   **Ya rastreado como tarea #47 (pending).**
6. **[MEDIA] Barkeeper/taberna = stub sintético** — el núcleo fiel (rondas por vivos, vino
   con gate 3→karma−1+borrachera, raciones +25 comida, 26 rumores) existe pero NO cableado;
   la UI (`shop.ts:244-270`) vende "Food(1)"+"Torch" vía `buyProvisions`, no-fiel y
   autodeclarado (comentario 246-251). Comida SÍ comprable → no hay dead-end de inanición,
   pero rondas/vino/rumores/raciones son inalcanzables.
7. **[MEDIA] HorseSeller sin compra** — solo existe `horsePrice()`; no hay `buyHorse` en el
   núcleo ni rama UI. El caballo se obtiene por Wishing Well pero no se compra en establo.
8. **[MEDIA] Keyword-matching prefijo vs substring** (tarea #50 abierta) — el diálogo usa
   `startsWith`; el `kw_match` real podría ser substring (`stristr`). Si difiere, algunas
   palabras clave de quest tecleadas por el jugador no resolverían a la respuesta correcta.
   <!-- [PROSA-AUTOFIEL t3: ESTA ENTRADA ESTÁ OBSOLETA — el hueco está CERRADO. `stristr`
   quedó DERIVADO en ULTIMA.EXE 0x6f1e (`kernel-sweep-3.md:315`, `inferible-sweep-f3.md:47`)
   con gate de frontera TALK.OVL 0b5b-0b64, y el port YA lo implementa (`conversation.ts:189`
   `stristrIndex`; `talk-corpus-witness.md:12` = «matching stristr (NO startsWith)»). La
   afirmación «el diálogo usa startsWith» describe un estado del port que ya no existe. No se
   reescribe el texto histórico; queda anotado. Ver también :284. -->
   Impacto directo en progresión de diálogo. **No hay fuente local que lo resuelva → candidato
   claro a research/derivación.**
9. **[MEDIA] Inn Leave/Pick-up + páginas de inventario en Ztats** — núcleo `innLeave`/
   `innPickup` codificado pero no cableado (no se puede estabular party en posada). Ztats
   (`ui/ztats.ts:66-73`) solo muestra Food/Gold/Keys/Gems/Torches/Karma; las cantidades de
   reactivos/pociones/pergaminos/equipo existen en estado pero no se muestran.
10. **[MEDIA] Lord British: ¿cura completa al hablar? + monto exacto de extorsión** — no se
    halló ruta de cura-al-hablar con LB (su conversación es un script CASTLE.TLK normal);
    verificar si debe existir el special 0xFF. Y el monto de oro exigido por NPC viene del
    extractor Redux, no del opcode de 3 bytes del TLK (el chequeo de fondos sí es correcto).
    Ambos **sin fuente local que los fije** → candidatos a research.

**Nota tranquilizadora:** ninguna de las mecánicas de progresión principal está rota. El
único FLAG que puede *bloquear* economía es el #1 (Guild). El #2 (caja) solo degrada el
final a la variante sin escena buena. El resto son cableado UI o tareas ya abiertas.

---

## MATRIZ COMPLETA POR DOMINIO

### 1 · MAGIA & WORDS OF POWER
Núcleo ASM-derivado y con paridad (`re/tools/magic_parity.py`, `__parity__/magic-run.ts`).

- **CUBIERTA (22):** 48 hechizos + Nox nulo en orden canónico (`magic/spells.ts:63`);
  **48/48 recetas de reactivos** diffeadas exactas contra `MagicDefinitions.json`; coste
  PM=círculo (`cast.ts:289`); maná máx A/M=INT, Bardo=INT/2, Guerrero=0; efectos de un
  objetivo (Mani/Vas Mani cura, An Nox/An Zu, In Mani Corp resurrección, An Ex Por sella
  puerta, Rel Hur viento, In Xen Mani comida); estados temporales (In Sanct/Rel Tym/Quas
  An Wis/In An/An Tym); mezcla (`mix.ts`, consume-al-fallar); cast-input tecleado
  inicial→runas order-independent sin J/O (`spells.ts:141`, tarea #26); los **8 Words of
  Power** dungeon (FALLAX…VERAMOCOR, `quest/words.ts:5-8`, en corpus wop-*), texto "A word
  of power is uttered" byte-exacto.
- **FLAG (8, todos "aplicación de efecto"):**
  - [MEDIA] Ataques directos en combate (Grav Por/Vas Flam/Xen Corp) — descriptor exacto,
    no consumido por `combat.ts`. → tarea #44.
  - [MEDIA] Muros de campo (In Flam/Nox/Zu/Sanct Grav) — no aplicados al mapa/combate. #44.
  - [MEDIA] Línea-AoE (In Zu, In Nox Hur, In Flam Hur, In Vas Grav Corp). #44.
  - [MEDIA] Invocaciones (Kal Xen, In Bet Xen, Kal Xen Corp). #44.
  - [MEDIA] Charm/miedo/polimorfia/invisibilidad/mass-charm — descriptores existen; `combat`
    tiene flag `charmed` pero cast→charm no cableado. #44.
  - [MEDIA] Uus Por/Des Por/In Lor de mazmorra — descriptores exactos, no cableados. #47.
  - [BAJA] In Vas Por Ylem terremoto: mensaje existe, daño por-enemigo es combat-side. #44.
  - [BAJA→MEDIA] Vas Rel Por (gate travel) / In Quas Xen (clon, semántica "⚠ pendiente"
    en `cast.ts:216`).
- **PENDIENTE (3):** 🔴 **In Ex Por — RETIRADA (2026-08-07). Decía: «el port es MÁS fiel que el
  manual aquí». Es al revés: EL MANUAL TENÍA RAZÓN y el port omite la mecánica.** In Ex Por
  **sí abre cerraduras mágicas** en el original: el brazo del hechizo 26 (`CAST:0x1026`) llama,
  vía stub `0x80ee`, a `CAST2.OVL:0x0768`, que escribe `0x97→0xB8` / `0x98→0xBA` en el mapa vivo
  — el mismo worker que usa la Skull Key desde `CAST:0x18dd`. **Un worker, DOS llamadores.**
  Alcanzabilidad positiva (máscara `DS:0x1c90` ⇒ `0x05` = pueblo+combate). ⇒ el port
  (`cast.ts case 26 → castAnimOnly`) **diverge del original**, y la divergencia estaba declarada
  al revés. Se arregla APARTE (toca `game/src`; medir antes si mueve stream).
  Derivación y controles: `inexpor-dos-llamadores-acta.md`.
  Mantras de shrine (otro dominio). "~50 hechizos/8 círculos" (claim, corrobora).
  **ACTUALIZACIÓN (Batch 8B, 2026-09-20):** el lado **nativo** (`native/core`) ya
  implementa la mecánica — `world_magic.cpp` gana una rama `MagicEffect::Unlock`
  hermana de `Seal`/`Disarm`, reutilizando la transformación 0x97/0x98 → 0xB8/0xBA
  que ya usaba la Skull Key, con el mismo guard de pre-vuelo extendido al ítem 26.
  Regresión de comportamiento en `native/core/tests/batch5_test.cpp` (casos C1v/C1w/
  C1x/C1y/C1z), host suite completo sin regresiones (68/70, únicos fallos los ya
  conocidos R-21 `gameplay_parity` #2034 y el crash de entorno de `quest_parity`),
  firmware ESP-IDF limpio. `cast.ts case 26 → castAnimOnly` (lado TypeScript,
  `game/src`) **sigue pendiente**, sin tocar por este batch — es el "APARTE" que
  este mismo párrafo ya preveía, ahora acotado a un solo lado.

### 2 · COMBATE & CRIATURAS
100% cubierta en lo aseverado por fuentes; núcleo byte-fiel (COMBAT.OVL/COMSUBS.OVL).

- **CUBIERTA (38):** controles (mover/Attack a+dir/click/Pass/huir por mismo borde/Ready);
  iniciativa (countdown reload 36−speed, empate slot bajo), tirada de golpe rand30 vs
  (def−atk+30)/2 (STR para contundente, DEX resto), daño arma rand(1,atk)−armadura, XP
  maxHP/4+1 solo al matador cap 9999, clasificación de heridas + huida a 1/4 HP,
  VICTORY/BATTLE IS LOST, distancia floor(sqrt); **48 equipos** (Glass Sword 99 auto-hit +
  rotura, contundentes con STR, a distancia con rango/munición/desequipar-sin-munición,
  interferencia adyacente, dispersión de fallo, defensa de 6 slots, gate de fuerza,
  jeweled 0 / puños 1); **48 enemigos** con especiales (veneno melee+rango 3/4, robar
  comida, mirada→dormir del Gazer, dividir slimes, teletransporte, poseer/charm por INT,
  invisibilidad, invocar daemon, no-muertos ½ daño / inmortales 0, mimic/reaper estáticos,
  IA objetivo-más-cercano); **16 arenas** por tile de terreno, spawn ponderado por
  terreno×planta, formación por dirección de entrada, emboscada de acampada (#8), cañón (#32);
  Shadowlord matable en combate normal / glass sword.
- **PENDIENTE (5), ninguno rompe progresión:**
  - ~~[MEDIA] Castear hechizos DURANTE combate (tecla C en arena) — diferido "Task 3.3/#44";
    `combat.ts:829,895` difieren aplicación de g_5890. (Solapa con dominio Magia FLAG.)~~
    [HISTÓRICO 2026-07-25: superado — Cast-en-combate aterrizado; `Combat.playerCast`/
    `castCombatAttack` consumen `CastEffect` en core/combat/combat.ts (Task 3.3 cerrada).]
  - [BAJA] Campos de terreno de arena / ~~triggers .CBT~~ / daño por tile (no aseverado por
    fuentes). [HISTÓRICO 2026-07-25: los triggers .CBT SÍ están implementados —
    `Combat.fireTriggers` (COMBAT 0x111A, SJOG 0x1d3c, one-shot). Los campos 0xE8-0xEB y
    el daño por tile SIGUEN pendientes (combat.ts:15, :2021) — esa parte no es histórica.]
  - [BAJA] Contenido exacto de cofres en (G)et de arena (oro aproximado por rating).
  - [HISTÓRICO 2026-07-29: Rel Tym (quickness Q) en combate — PORTADO por #203
    (`re/notes/reltym-203-acta.md`): `rand(0,1)` por turno de enemigo y turno perdido
    entero con 0. La referencia de esta línea era a la tarjeta **#42, que nunca existió**
    (la lista salta del #41 al #43) — hueco huérfano de tarjeta desde entonces.]
  - [BAJA] `canFlyOverWater` en combate = divergencia deliberada documentada (evita freeze).

### 3 · QUEST / ENDGAME / SHADOWLORDS / ARTEFACTOS

- **CUBIERTA (21):** **4 artefactos con ubicación confirmada por datos** — Corona en el
  TEJADO del palacio de Blackthorn (loc 18, tipo 181 floor 3), Cetro en Stonegate (loc 29
  tipo 182), Amuleto en Underworld bajo Destard (`underworld-seed.ts` tile 0xB7), y la Caja
  (colocada pero ver FLAG). Los 3 Shards (0xB4, gated por vivo+no-tomado). Trío
  Shadowlord↔Virtud↔Llama correcto (Faulinei/Truth/Lycaeum, Astaroth/Love/Empath,
  Nosfentor/Courage/Serpent's Hold — **el port corrige un swap que trae la GUIA.md:229-231**).
  Ritual completo: Yell nombre en la Llama opuesta + (U)se Shard (`castShardIntoFlame`,
  gates de posición/summon/tileAbove); invasión urbana de SL + drenaje de oro de Falsehood;
  8 Words↔dungeons 33-40; VERAMOCOR→Doom→floor 7→rescate LB (gate 3 SL muertos +
  amuleto+corona+cetro); informe de tiempo de juego + pergamino de victoria byte-exactos;
  captura/interrogatorio/refugio de Blackthorn (password IMPE con bug de truncado a 4).
- **FLAG (1):** [MEDIA] **Caja de sándalo inobtenible** (raíz doble: adquisición §A + rama
  de final bueno §D). Ver TOP-2.
- **PENDIENTE (4):**
  - [BAJA] Sistema astronomía/cometa que predice qué ciudad ataca un SL (claim NO verificada;
    el tracking de ubicación de SL SÍ se modela, falta solo la UI de pista del astrónomo).
  - [BAJA] Matar Shadowlord en combate normal como vía alterna (el port solo destruye por
    ritual; claim NO verificada).
  - [BAJA] Diálogo de qué NPC del Consejo enseña cada Word (el Yell funciona sin gate = fiel;
    el contenido .TLK "aprende word X de NPC Y" no verificado en esta pasada).
  - [—] Cinemáticas de trono con `MISCMAPS.DAT` = **tarea #20** (fuera de alcance, no
    re-flageado).
  - **Discrepancias de DOC (no bugs del port):** GUIA.md:229-231 intercambia
    Astaroth/Nosfentor (port correcto); GUIA.md:227-228 dice "Search" para shards pero el
    port usa (G)et sobre el tile (mecánica real, corrección de fidelidad).

### 4 · NPCs / DIÁLOGO / SHRINES / VIRTUDES / KARMA / GITANA

- **CUBIERTA (28):** 8 mantras byte-idénticos a U4 (`data.json:mantras`), 8 virtudes +
  coords de shrine, meditación muestra mantra + fija quest, completar quest +3 karma +1
  atributo (flags byte-derivados), peregrinaje al Codex + ceremonia final, donación +1/100
  oro, restaurar shrine destruido (teclear virtud+mantra×3+coord); **gitana**: torneo de
  eliminación 8-virtudes (4+2+1=7 preguntas, seed 0, byte-exacto, cableado a creación);
  **karma**: robar comida −1, cofre de pueblo −2, atacar NPC −5 (asm; manual dice −10 =
  impreciso), respuestas mean/lie −1, liberar preso +2, borrachera −1, clamp 0..99;
  **schedules**: 3 posiciones/4 horas por NPC (`scheduleIndex` con quirk 3→1), IA jump-table
  0..7, distancia Manhattan, posesión urbana de SL (bug slot-#4); **diálogo**: TLK
  TOWNE/DWELLING/CASTLE/KEEP (135 NPCs), intérprete (secciones/labels/AskName/join/karma),
  guard toll + Blackthorn IMPE + Minoc gold-halving; **13 compañeros reclutables** (todos
  como registros en `initial-state.json`, `joinByName` + JoinParty, MAX 6); Saduj = trampa.
- **FLAG (9):**
  - [MEDIA] Keyword-matching prefijo vs substring (tarea #50). Ver TOP-8.
  - [MEDIA] Lord British cura-al-hablar posiblemente no modelada. Ver TOP-10.
  - [MEDIA] Monto de extorsión de oro desde extractor Redux, no opcode 3-byte TLK. TOP-10.
  - [MEDIA] Special NPCs 0xFD/FE/FF parcial (guards/Blackthorn/Minoc sí; frontera Task 3.10).
  - [BAJA] Cambio de planta de NPC = TELETRANSPORTE en vez de caminar a escaleras
    (`manager.ts:226`, divergencia documentada, solo trayectoria).
  - [BAJA] Pathfinding A* en vez de scanner greedy `npc_scan` (trayectoria, documentado).
  - [BAJA] Selección exacta de objetivo en flee/run-away aproximada (Task 3.9/3.10).
  - [BAJA] Bonus de karma post-pago de mercader de montura (quirk 0x0603) no portado.
  - [BAJA] XP se canjea en ACAMPADA no al hablar con LB (manual simplifica; port = asm).
- **PENDIENTE (1):** cobertura de TEST (no gap del port) — 44/44 hechos npc-dialogue del
  corpus sin cruzar, ~105 de 135 NPCs del manifest sin talk-test (Grand Tour en curso).

### 5 · TRANSPORTE / MUNDO / MOONGATES / KLIMB / SEARCH

- **CUBIERTA (41):** **Moongates con ciclo lunar de DOS lunas correcto y data-driven** —
  solo de noche (20:00-4:59), Felucca (madrugada) vs Trammel (noche) elige destino, tabla
  de 28 días leída de DATA.OVL 0x1EEA (más fiel que el manual), borde de medianoche sin
  teletransporte, enterrar/desenterrar moonstone; **fragata** (abordaje, velas por viento,
  becalmado "in irons", hoist/furl por Yell, cañones broadside perpendicular rand(1,20),
  colisión/rotura rand(1,30), hundimiento skiff>carpet>drown, reparación en acampada,
  compra en astillero = fragata con 2 skiffs); **HMS Cape doble velocidad** (efecto
  `navalStepCost` fiel — pero ver FLAG del verbo Use); **caballo/carpet/skiff** (abordaje/
  salida con "No land nearby!", carpet vuela sobre agua data-gated); barcos-NPC pirata por
  viento (no abordables = fiel); **viento** (cambia rand(0,63)/turno, empuja opuesto, Rel
  Hur); tiempo 1min/turno town, 2min/paso outdoor + terreno lento; wrap 256×256; whirlpool→
  Underworld; troll de puente; **Klimb** (montañas con grapple + DEX, town ladders/grate,
  dungeon techo-iluminado + prompt U/D); **Search** (2 pasos, secret doors 0x4E→0xB9);
  **Open** (autoclose 4 turnos, "Locked!"); **Jimmy** con llaves + DEX; Skull Key
  desmagifica (0x97→0xB8, NO In Ex Por, #22); detección de trampas por INT.
- **FLAG (2):**
  - [MEDIA] HMS Cape Plan no activable por juego (verbo Use, ver TOP-3; comparte gap con
    Spyglass/Sextant/Pocket Watch/Badge/Box).
  - [BAJA] Ahogamiento = solo cosmético (tile+mensaje "DROWNING!!!", sin party-wipe/HP/
    teleport; divergencia Class-C documentada, helpers de anim opacos, pregunta abierta de
    oráculo en `transport.md §10`).
- **PENDIENTE (2, ya rastreados):** movimiento de actores por clase de tile (#37 in_progress);
  puente levadizo/rastrillo por hora TOWN 0x0170 (#48 pending; tiles existen y son pasables,
  no bloquea viaje).

### 6 · ECONOMÍA / TIENDAS / ITEMS / COMIDA / INVENTARIO
Núcleo byte-fiel (SHOPPES.OVL/2/3); los gaps son de CABLEADO UI (`ui/shop.ts`).

- **CUBIERTA (12):** precio compra por INT `base+⌊base·(100−3·INT)/100⌋`, venta
  `⌊3·INT·base/100⌋+1`; herrero (compra Y venta, stock por pueblo); mago-reactivos (tabla
  5×8 por pueblo, qty fija); healer (heal/cure/**resurrección** con coste, wired); comida
  se consume a 6/12/18h, food==0→"Starving!"+rand(1,8) daño; antorcha/luz en minutos
  (Ignite 240); caps (oro/comida 9999, llaves/gemas/antorchas 99); gema consumida por
  (V)iew, llave por (J)immy; Ready/equip 6 slots con peso-vs-STR; astillero; posada Rest
  (heal vivos, veneno P→D, reloj→06:00); drenaje post-compra de Falsehood; offsets de save
  byte-exactos.
- **FLAG (6):**
  - [ALTA] **GuildMaster no funcional en UI** (llaves/gemas/antorchas incomprables). TOP-1.
  - [MEDIA] **HorseSeller sin buyHorse** (ni núcleo ni UI). TOP-7.
  - [MEDIA] **Barkeeper = stub sintético** (rondas/vino/raciones/rumores no cableados). TOP-6.
  - [MEDIA] Inn Leave/Pick-up no cableado (no estabular party). TOP-9.
  - [MEDIA] Ztats sin páginas de inventario (reactivos/pociones/pergaminos/equipo no
    mostrados). TOP-9.
  - [BAJA] Uso/quaff de pociones sin efecto (pociones son loot-only en U5, fuera de economía
    estricta).
- **PENDIENTE (0):** toda mecánica está cableada o tiene núcleo byte-derivado esperando UI.

---

## ÁREAS SIN FUENTE LOCAL (candidatas al research de fase 2)

Donde ni la guía ni las claims locales dicen lo suficiente para resolver — el port hace una
elección (a veces "⚠ pendiente" en el propio código) que NO se puede confirmar/refutar sin
derivación ASM nueva o testimonio externo. Priorizadas por impacto:

1. **Algoritmo exacto de keyword-matching de diálogo** (prefijo `startsWith` vs substring
   `stristr`) — tarea #50. Impacto en progresión de quest. Ninguna fuente local lo fija.
   <!-- [PROSA-AUTOFIEL t3: OBSOLETA — «ninguna fuente local lo fija» es falso desde que el
   barrido de kernel derivó `0x6f1e = stristr` (`kernel-sweep-3.md:315`). El ítem ya NO
   pertenece a esta lista de «requieren derivación ASM nueva o testimonio externo». Ver :90. -->
2. **Monto exacto de oro exigido por NPC** (codificación de 3 bytes del opcode TLK vs valor
   del extractor Redux). Ninguna fuente local.
3. **Lord British al hablar: ¿cura completa al party?** El comportamiento canónico de U5 no
   está en la guía local; verificar si falta el special 0xFF.
4. **Mecánica exacta de ahogamiento** (party-wipe/HP/teleport) — helpers de animación opacos,
   pregunta abierta de oráculo en `transport.md §10`.
5. **Sistema de astronomía/cometa** que predice el ataque de Shadowlord — claim de internet
   NO verificada; sin derivación local. (Bajo impacto.)
6. **Shadowlord matable en combate normal como vía alterna al ritual** — claim NO verificada.
   <!-- [PROSA-AUTOFIEL t4 / tarea #35 (2026-07-27): ADJUDICADA — **REFUTADA POR EL
   BINARIO**, ya no es «no verificada». El estado que registra que un Shadowlord está
   destruido es `g_shadowlord_locs` (DS 0x58C8, 3 bytes, uno por SL): el ritual escribe
   0xFF ahí (`CAST.OVL 0x170b: mov byte [bx+0x58c8], 0xff`) y todos los lectores tratan
   >=0x80 como «fuera del mundo». CENSO COMPLETO de ACCESOS a esos 3 bytes en todo
   `re/disasm` (hecho por hex Y por símbolo, ver la trampa de abajo): escritores SÓLO DOS
   — el ritual (CAST 0x170b) y el ERRANTE DE MEDIANOCHE (`ULTIMA.EXE 0x5044`), y este
   segundo (a) está guardado en 0x4ffd por `cmp byte [bx+0x58c8], 0x80 / jae` ⇒ NUNCA
   toca un SL ya destruido, y (b) sólo escribe un id de pueblo 1..8 sacado de
   `randRange(1,8)` en 0x500c ⇒ NUNCA escribe >=0x80. Lectores: CMDS 0x1078, OUTSUBS
   0x05ae, LOOKOBJ 0x04bf, TOWN 0x02cc/0x127f, ULTIMA.EXE 0x4ffd/0x5020, MAINOUT
   0x07de/0x07e3/0x07eb. **`COMBAT.OVL` no accede a 0x58C8 NI UNA VEZ** (verificado por
   las dos vías). ⇒ Ganar un combate contra un Shadowlord NO PUEDE destruirlo: el combate
   no toca el único estado que lo registraría. El ritual es la única vía, y el port acierta
   (sólo `endgame/use-tools.ts:59` pone `shadowlord-dead`).
   ⚠ TRAMPA DE MÉTODO, que casi me come: `grep 5bca` sobre el disasm da CERO aunque el
   acceso existe — el desensamblador lo imprime como `g_npc_dead_bitmap+112`. Igual pasa
   con 0x58c8: el grep por hex se dejaba 3 accesos de MAINOUT que sólo aparecen como
   `g_shadowlord_locs`. **Todo censo de accesos a un global debe hacerse por HEX Y POR
   SÍMBOLO**; con una sola vía el cero es silencioso.
   Y NO confundir con dos preguntas VECINAS que son OTRAS: (i) `shadowlord-ritual.md:152`
   (aliasing del doom-bit 0x5bca dentro del bitmap de NPCs muertos de Windemere) sigue
   ABIERTA y va de efectos colaterales del formato de save, no de matabilidad; (ii) la
   emboscada de la entrada de Doom (`game.ts` + MAINOUT 0x7d8-0x812) sí está derivada e
   implementada, y demuestra que un SL PELEA — pero pelear no es destruir. -->
7. **Efectos de quaff de pociones** (loot-only; ninguna fuente describe efectos por color).
   <!-- [PROSA-AUTOFIEL t4 (2026-07-27): CERRADA — «ninguna fuente describe efectos por
   color» es FALSO desde el 18-07. `re/notes/potions-scrolls.md` deriva el bebedor entero
   (CAST 0x135a): consumo en 0x136a, selector de PJ/actor de combate en 0x1375, jump-table
   por color verificada contra el binario (idx0→0x13e0 … idx7→0x1514) y hasta el «fiasco»
   RNG de 0x13a8 (r==0 → fuerza ORANGE/dormir, r==1 → efecto de otro color). Los nombres
   de color salen de la name-table DATA.OVL DS 0x067c. El ítem NO pertenece a «requieren
   derivación ASM nueva o testimonio externo». -->
8. **Contenido .TLK de "aprende Word X del NPC Y"** — el corpus tiene los pares teacher↔word
   pero el diálogo real que los entrega no está verificado.
9. **Deltas de karma de "table-plate" exactos** (0x9A/9B/9C) — divergencia Class-C anotada.
10. **Quirk de bonus de karma del mercader de montura** (0x0603) — no portado; verificar si
    merece.
    <!-- [PROSA-AUTOFIEL t4 (2026-07-27): MAL COLOCADA — este ítem no tiene ningún hueco de
    FUENTE. `re/notes/npc.md §9.1` (0x0603-0x064E) ya trae la derivación completa: tile del
    NPC (&0xFC)==0x6C y g_turn_count>=0x64 → turn_count=0 y karma=min(karma+1,99) (kernel
    0x7F70), +2 más si el pago te dejó a 0 de oro. La misma nota lo declara «quirk obscuro
    declarado como divergencia, no cableado». Lo que queda abierto es una DECISIÓN de port
    («¿merece portarlo?»), no una derivación: no pertenece a «ÁREAS SIN FUENTE LOCAL». -->

---

## SOLAPES CON TAREAS YA ABIERTAS (no duplicar)

- #42/#44 — pack de combate/magia + integración de hechizos en combate (cubre FLAGs de
  magia ofensiva/AoE/summon/charm y castear-en-combate).
- #47 — hechizos de mazmorra (In Lor/Uus Por/Des Por) — cubre FLAG de dungeon.
- #37 — movimiento de actores por clase de tile — cubre FLAG de persecución pirata.
- #48 — tiles de pueblo por hora (puente/rastrillo) — cubre FLAG G1 de transporte.
- #20 — motor de tile-cinematics de endgame (trono) — cubre PENDIENTE de cinemáticas.
- #43 — backlog bajo impacto (activación por proximidad de objetos) — solapa con el verbo Use.
- #50 — keyword-matching — cubre el FLAG de diálogo.

## GAPS NUEVOS (no cubiertos por ninguna tarea abierta que yo haya visto)

1. **GuildMaster UI** (ALTA) — rama `render()` ausente en `ui/shop.ts`.
2. **Caja de sándalo obtenible** (MEDIA) — grant tras el pasadizo del clavicémbalo.
3. **Verbo (U)se de herramientas de endgame** (MEDIA) — HMS Cape/Spyglass/Sextant/Watch/Badge.
4. **HorseSeller buyHorse + UI** (MEDIA).
5. **Barkeeper fiel + Inn Leave/Pickup + páginas de Ztats** (MEDIA) — cableado de núcleo ya
   escrito.
6. **Lord British cura-al-hablar** (MEDIA, si aplica) — verificar contra ASM.

---

## APÉNDICE ASM — #51: el (G)et de la caja de sándalo (tipo 14), derivado del binario

> El fix de #51 (caja obtenible) se implementó primero ESPEJANDO el patrón corona/cetro. A
> exigencia del usuario ("el testigo/la guía dicen QUE existe; el binario dice CÓMO") se deriva
> aquí la cadena real del binario y se confirma que el clon la calca. Autoridad: **asm**.

### Cadena citada (SJOG.OVL, DATA.OVL)

1. **Comando (G)et** — `SJOG.OVL 0x18ce`. Calcula la celda destino `(party_x+dx, party_y+dy)`
   y ESCANEA la tabla de world-objects del mapa (arrays stride 8: tipo en `DS:0x5c62`, x en
   `0x5c64`, y en `0x5c65`, z/floor en `0x5c66`). Empareja por x (`0x1941`), y (`0x194b`) y —
   en interiores (loc ≤ 0x7f) — floor (`0x1960`).
2. **Aceptación por TYPE** — `0x1964: mov al,[si]` carga el **type** del slot; `0x196a: cmp
   cx,0x10 / jl 0x197f` acepta directamente **todos los tipos 0..15** (además de 0x19/0x1b y la
   familia `& 0xFC == 0xB4` de shards/corona/cetro/amuleto). El type queda en `[bp-6]`.
3. **Despacho** — `0x1998: push [bp-6]` (el TYPE) `… 0x199f: call 0x1458` =
   `apply_item_grant(type, …)`. `apply_item_grant` (`0x1458`) lee el id en `[bp+8]`: `>0xc →
   0x172e`, donde `0x1740: cmp ax,0xe / je 0x14f0` enruta el **type 14** a la rama de la caja.
4. **Rama de la caja** — `SJOG.OVL 0x14f0`:
   - `0x14f0: push 0x8c76 / call 0x58d0` → imprime el string en **DS 0x8C76**.
   - `0x14f7: mov byte [g_wooden_box (DS 0x57bf)], 0xff` → fija el flag.
   - `0x14fc: or byte [DS 0x5b9d (g_npc_dead_bitmap+67)], 0x80` → marca el slot como muerto
     (no re-nace al re-entrar).
   - Sale por el exit común del Get (`jmp 0x1b2e`), que **consume el turno**.
5. **String** — `DATA.OVL` fileoff `0x8C86` (= DS 0x8C76 + 0x10) =
   `41 20 73 61 6e 64 61 6c 77 6f 6f 64 20 62 6f 78 21 0a` = **"A sandalwood box!\n"** (CON
   salto de línea, igual que corona/cetro).
6. **Flag → save/endgame** — `g_wooden_box` (DS 0x57bf) lo leen `ENDGAME.OVL 0x08c2` (gate de la
   escena buena del trono, `Y ∧ g_wooden_box`) y `ZSTATS.OVL 0x0a2d`. En el .gam persiste en
   el byte `0x219` (`saveNative.ts`).

### Confirmación: el clon calca (mismo gate, flag, string, turno)

| Aspecto | Binario (derivado) | Clon | ¿Calca? |
|---|---|---|---|
| **Gate** | NINGUNO — la rama 0x14f0 no comprueba melodía ni pasadizo; la caja es un slot-objeto estático, sólo la tapa el muro (spatial gate) | Sin gate lógico: `hydrateInteriorObjects` siembra la caja siempre; la reachability la da el pasadizo del clavicémbalo (`harpsichordPassageOpen`) | ✅ |
| **Dispatch** | por TYPE del slot (14) vía `apply_item_grant` — la MISMA rutina que corona (0xB5)/cetro (0xB6) | `plotItemForNpcType(14)→"wooden-box"` → `grantPlotItem` (misma vía que corona/cetro) | ✅ (equivalente observable; el "plot kind" es abstracción del port) |
| **Flag** | `g_wooden_box=0xff` (DS 0x57bf; .gam 0x219) | `specialItems.woodenBox=true` (.gam 0x219) | ✅ |
| **String** | DS 0x8C76 = "A sandalwood box!\n" | `SANDALWOOD_MSG = "A sandalwood box!\n"` | ✅ (**corregido**: el fix inicial omitía el `\n`; la derivación ASM lo cazó) |
| **Turno** | exit común del Get (0x1b2e) consume turno | plot-get → `runContextTurn({consumed:true})` | ✅ |
| **No-renacimiento** | marca el bit de slot muerto (`g_npc_dead_bitmap+67 \|= 0x80`) | retira el worldObject + gate `!woodenBox` en `hydrateInteriorObjects` | ✅ (equivalente observable; misma clase de divergencia que corona/cetro) |

**Correcciones aplicadas por esta derivación** (commit de #51-ASM): el string pasa de "A
sandalwood box!" a **"A sandalwood box!\n"** (byte-exacto DATA.OVL 0x8C86). El resto del fix ya
calcaba. NO hay precondición de "13 notas tocadas" en el Get (la melodía sólo abre el pasadizo,
`TOWN 0x0E34`); el objeto es tipo-14-genérico despachado por `apply_item_grant`, no un tile
especial. `commands.ts:lootItemName(14)` conserva "A sandalwood box!" sin `\n` porque es el path
de botín-suelo, que la caja NUNCA usa (no aparece en botín de cofre; `objects.md §O-loot`).
