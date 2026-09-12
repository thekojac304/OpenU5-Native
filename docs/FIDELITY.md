# FIDELITY.md — registro de fidelidad al original

Discrepancias conocidas, decisiones tomadas y pendientes de verificar contra el
`ULTIMA.EXE` original en DOSBox.

**Convención de estado y dueño** (uniforme tras Task F.1; ninguna ⚠️ queda sin
dueño ni apunta a una task ya cerrada):
- **✅ RUNTIME-VERIFICADO / ✅(asm+runtime)** — cerrado: regla asm-derivada Y
  cotejada byte a byte contra DOSBox. Sin dueño pendiente.
- **✅(asm) / ✅** — regla asm-derivada y portada con tests del clon; sin ⚠️.
- **⚠️→formulado** — regla asm-derivada + cruce modelo↔clon; **dueño = Task F.2**
  (paridad runtime contra DOSBox). Las que el live verifique suben a ✅ con su
  evidencia.
- **⚠️** (a secas) — o bien **dueño = Task F.2** (cableado al bucle / escenario
  runtime), o bien **pregunta de oráculo** (un valor del original aún sin medir);
  cada entrada dice cuál.
- **❌** — desviación consciente (QoL aditivo); no se cierra.

Las Tasks 3.1–3.13 están CERRADAS: se citan sólo como PROCEDENCIA de una regla
(dónde se derivó), nunca como dueño de trabajo pendiente — ese es siempre F.2.

**Cierre F.2 (2026-07-11).** Tras la task final **ninguna ⚠️ de este documento
queda huérfana**: cada una está catalogada en **`re/deliberate-divergences.md`**
como divergencia deliberada de clase A–E (paridad de stream vs runtime-DOSBox /
cableado interactivo pendiente / pregunta de oráculo / frontera de alcance / trama)
con su **vía de cierre** anotada. El §8 de ese documento mapea cada ⚠️ de aquí a su
clase. La ruta crítica **gitana→endgame** (`re/tools/test_master_scenario.py`) cierra
la unificación del stream de 3.13 **a nivel MODELO↔CLON**: demuestra el orden de RNG
del binario con una sola semilla threadeada por los motores puros. La unificación del
stream **VIVO** del clon jugable (`game.ts`) quedó **CERRADA en la Fase 1.1** (PORT del
stream vivo): `game.ts` consume ahora un único `OriginalRng` ordenado que delega el
esqueleto del turno en `turn.ts`, con determinismo E2E por seed
(`game/e2e/determinism.spec.ts`); ver deliberate-divergences §2 (primer bullet). Córre
los arneses con `npm run re:parity:all`.

## Motor / movimiento

- ✅ **Movimiento bloqueado** — VERIFICADO en DOSBox (Task 3.1, re/verified/kernel-survival.md):
  en pueblo "Blocked!" consume 1 minuto; en el EXTERIOR un movimiento bloqueado NO consume
  tiempo ni turno del mundo (MAINOUT 0xC30). Reglas exactas portadas a world/movement.ts.
- ✅ **Terreno lento ("Slow progress!")** — VERIFICADO en DOSBox (Task 3.1): no es
  probabilístico; es coste FIJO de minutos (+2 tiles 4,6,7,8,30,31 con "Slow progress!";
  +4 tiles 9-15 con "Very slow!") sobre el coste base exterior de 2 min/paso, más 1/2
  turnos EXTRA del mundo (monstruos) según la clase (MAINOUT 0x448/0x468: call 0x1A60).
  Portado completo: minutos (terrainSpeedClass) y turnos extra (world-turns
  adicionales que `Game.move` corre vía `runContextTurn` antes del coste base).
  Evidencia: re/verified/kernel-survival.md.
  - ⚠️ Matiz de evidencia: el runtime DOSBox ejercitó la clase 2 ("Very slow!"); la clase 1
    ("Slow progress!") comparte el mismo código asm con otras constantes y queda cubierta
    por los tests del clon (sin escenario DOSBox propio).
- ✅ **Reloj/comida/hambre/antorchas** — VERIFICADO en DOSBox (Task 3.1): 1 min/acción en
  pueblo, 2 en exterior; comidas a las 6/12/18 (food -= vivos no D/S); "Starving!" con
  food==0 en cada cambio de hora (rand(1,8) por miembro, RNG byte-idéntico); antorchas en
  MINUTOS (Ignite=240 fijo fuera de mazmorra; 112+rand(0,15) dentro). world/survival.ts.
  - ⚠️ Divergencia consciente: turnsSinceStart del clon no satura (el original usa un u8
    saturante a 255, kernel 0x2B9F).
  - ⚠️ La reubicación de Shadowlords a medianoche (kernel 0x4FF5) y los ceros de fin de mes
    sobre globals sin identificar (0x5858-0x585A) se portarán con sus subsistemas.
- ⚠️ **Wrap del overworld** en ambos ejes (256×256). Evidencia asm fuerte (MAINOUT 0x354:
  coordenadas byte con wrap natural, Task 3.1) + test del clon; paridad runtime en DOSBox
  pendiente (llegar al borde es impracticable con el arnés actual).
- ❌ **Cámara con interpolación suave y viewport 21×15** — QoL de presentación. La lógica
  (line of sight, distancias) no depende del viewport.

## Puertas

- ✅ **Duración de puerta abierta: 4 turnos** (DoorManager) — DERIVADO del binario: el timer
  del original es `[0x5952]=4` (SJOG cmd_open 0x1406) y se decrementa 1 por turno de pueblo
  (TOWN 0x15f6); ver `game/src/core/world/doors.ts:40-46`. Lo ÚNICO Clase C que queda es la
  fase exacta turno-a-turno respecto al turno del propio (O)pen, no el valor 4. (El ⚠ «valor
  del original sin verificar» era RANCIO — auditoría ronda 2, 30-07.)
- ⚠️ **Reabrir una puerta YA abierta (question C, sin verificar el mensaje exacto)**: el clon
  refresca el timer y responde "Opened!" (el `open()` lee el tile REAL de la puerta vía
  base+`mapOverrides`, no el sustituto de suelo de `effectiveTile`). En el binario, abrir una
  puerta pone `tile=0x44` (suelo) en el mapa vivo y guarda la original en `g_unk_594f` con
  timer=4 (SJOG Open 0x1374; `re/notes/cmds.md:149-155`), así que reabrir leería suelo y caería
  a `open_chest_world` (NO refresca, NO "Opened!"). El clon diverge porque usa `DoorManager`
  (representación separada de la apertura) en vez de mutar el tile del mapa a 0x44; portar la
  semántica exacta de reabrir exigiría portar ese modelo (tracker único + auto-cierre previo).
  Cubierto por test de regresión en `game/tests/game.test.ts` (Game.open reabrir → "Opened!").
- ⚠️ **Puerta abierta se dibuja/pisa como BrickFloor (68)**, siguiendo a Redux. El original
  podría usar otro tile según el suelo del edificio.
- ⚠️ **NPCs atraviesan puertas regulares** (se las trata como transitables para su pathfinding,
  sin abrirlas visualmente). El original las abre/cierra a su paso.

## Diálogo

- ✅ **Matching de keywords**: prefijo, case-insensitive (VERIFICADO asm — TALK.OVL:0x0000
  `kw_match`, máscara &0x7F + normalización; re/notes/npc.md §10). Equivalente a `startsWith`.
- ✅ **JoinParty máx 6** (TALK.OVL:0x080A): rechazo si `party_size==6`. El clon decide el
  rechazo en el caller (`party.ts`) — resultado equivalente (texto distinto, ver NPCs).
- **Karma**: cap 99 verificado (opcode 0x89, kernel 0x7F70 = `min(x+1,0x63)`); floor 0 (opcode
  0x8A, kernel 0x7FB6) ⚠️ PENDIENTE (0x7FB6 es un thunk far, sin derivar aún). El clon clampa
  [0,99] en `effects.ts`.
- ⚠️→formulado **Intérprete = port de Redux** (no el intérprete de bytes 0x0F32 de TALK.OVL).
  Produce la misma conversación observable; el ORDEN exacto de efectos y el consumo de bytes
  no están cotejados byte a byte (re/notes/npc.md §9, re/verified/npc.md).
- ✅ **Gold-demand con chequeo de fondos** (TALK.OVL:0x05B5): cobra sólo si `gold >= n`, si
  no responde "Thou hast not enough!" y no cobra. Portado en `dialogue/effects.ts` (testeado).
  ⚠️ La CANTIDAD `n` aún proviene del extractor de Redux, no del encoding de 3 bytes del script.
  ⚠️ La rama post-pago (0x0603-0x064E: bonus de karma del mercader de monturas — tile
  `&0xFC==0x6C` y `turn_count>=100` → karma+1, y +2 si el pago te deja sin oro) NO está portada
  (quirk obscuro; requiere sprite+turn_count en el punto de aplicación del efecto).
- ⚠️ **NPC hardcode 0xFD/FE/FF** (Lord British/guardias/password): guardias y password
  DERIVADOS en Task 3.10 (re/verified/blackthorn.md, TALK 0xFF 0x01e2); el cableado al
  diálogo y el hardcode de Lord British → F.2.
- ⚠️ **Ciclos de GotoLabel sin input se cortan a 16 saltos** — protección anti-cuelgue.
- ⚠️ Textos de fallback ("That I cannot help thee with.", "If you say so...") tomados de
  Redux; cotejar literalmente con DOSBox.

## Karma (re-derivado, walkthrough refutado)

- ✅ Acciones: diálogo ±1 (opcodes), robar comida/trigo −1, asesinato −10, liberar presos +2,
  donar santuario +1/100 oro. Fuente: código Redux (docs/formats/karma-shops-moongates.md §1.2).
  ⚠️→formulado La **donación de santuario está derivada del binario** (CAST2.OVL 0x0b47-0x0b91:
  `100*n` oro por `+n` karma, clamp 99; n==0 = no-op) por asm + cruce modelo↔clon — procedencia
  Redux RESUELTA; runtime → F.2. Ver §"Santuarios, mantras…" abajo.
- ⚠️ **Efecto: solo precio de reagentes**. Implementamos la fórmula de diseño
  `base × (1 + (100−karma)/100)` con división REAL (Redux tiene bug de división entera que
  la hace binaria). Verificar la curva del original en DOSBox.
- ⚠️ Rango 0-99 con clamp.

## Tiendas (Task 3.6 — reglas exactas de SHOPPES.OVL ×3)

Convención de etiquetado: ⚠️→formulado = asm-derivado + cruce modelo↔clon; paridad runtime
DOSBox pendiente. Citas en `re/notes/shops.md`; paridad `re/tools/test_shops_parity.py`.
- ⚠️→formulado **Regateo por INTELIGENCIA en TODAS las tiendas** (operando 0x55B6 = record+0x0E,
  §0): compra `base + ⌊base·(100−3·INT)/100⌋` (dos pasos, trunc hacia 0), venta
  `⌊3·INT·base/100⌋+1`. ⭐ Corrige el DEX (equipo) y KARMA (reactivos) de Redux, que eran falsos.
- ⚠️→formulado **Reactivos por-ciudad 5×8 + cantidad FIJA** (§2): precio de `reagentBasePrices`
  regateado por INT; grant = `reagentQuantities`; el input "how many" se ignora. No hay
  "pay-what-you-want/karma".
- ⚠️→formulado **Gremio** (§3): lote fijo keys+3/gems+4/torches+5 cap 99, precios por-town + INT.
- ⚠️→formulado **Transporte/naves** (§4/§7B): base binaria (caballo 100/130/160/190; fragata/
  esquife por astillero) + regateo; nave colocada en coords de muelle. `2×base_bin==base_clon`.
  **Shipwright ✅ CABLEADO en F1.5**: `shop.ts` rama 0x84 → `buyShip` → `game.spawnDockShip`
  coloca la nave-objeto en el muelle del overworld (ver §Transporte). HorseSeller/GuildMaster
  siguen sin UI viva (funciones puras) → fase posterior.
- ⚠️→formulado **Healer**: cure(P)/heal(HP<max&vivo)/resurrect(D) + precios planos por ciudad,
  sin stat. Cruzado modelo↔clon (escenario healer.json); sin runtime DOSBox.
- ⚠️→formulado **Posada** (§6): rest `haggle(rate·party)` cura vivos + MP por clase (A/M→INT,
  B→INT/2) + **MATA a los envenenados (P→D)** + reloj a 6:00 + capacidad; leave gratis; pickup
  `haggle(rate·10)·meses`. Reemplaza el coste fijo 4/40·6/60 de Redux.
- ⚠️→formulado **Taberna** (§7A): ronda por vivos, vinos con gate EXACTO en 3 servicios
  (comida+bebida) → **karma −1 + borrachera 25 turnos**, raciones +25 comida/ud regateadas,
  26 rumores con precio.
- ⚠️ **Merma post-compra 0x019A** (§0.3): `gold-=rand(1,64)` con suelo cuando
  `g_shadowlord_here_idx==0` = **Falsehood en la ciudad** (gate DECIDIDO: 0x5958 = índice del
  Shadowlord presente, TOWN.OVL). Estado de Shadowlords DERIVADO en Task 3.10
  (`shadowlordLocs`, re/verified/blackthorn.md) y port puro `postPurchaseGoldDrain`;
  el cableado a la compra → F.2. Consume 1 rand/compra.

## NPCs (Task 3.5 — reglas exactas de NPC.OVL)

Convención de etiquetado: ⚠️→formulado = asm-derivado + cruce modelo↔clon; paridad runtime
DOSBox pendiente (las que el live verifique de verdad suben con su evidencia).
- ⚠️→formulado **schedule_index** (NPC.OVL:0x12E0): argmin de `(hour-times[k])&0xFF`, empate al
  índice menor, quirk índice 3→posición 1. Portado byte a byte (re/notes/npc.md §0.3).
- ⚠️→formulado **Distancia = MANHATTAN** (0x06A0), no Chebyshev — radio de wander y cercanía.
- ⚠️→formulado **Wander** (0x0C50): skip ~50% (`rand(0,255)&8`) + dirección `rand(0,64)&3+1`
  (span 65 — `rand0(0x40)`=rand(0,64) inclusive, BLINDA, sin reintento) + radio Manhattan;
  **big-wander sin límite** (maxdist=0). Consumo de RNG exacto, cruzado en la paridad
  modelo↔clon (re/tools/test_npc_parity.py).
- ⚠️→formulado **Gate de planta** (0x1251): la IA/wander (y su RNG) sólo corre para NPC en la
  planta del jugador; los de otra planta se quedan quietos.
- ⚠️→formulado **AI types 0..7** dispatch (jump-table 0x0D00): 0=fixed, 1=wander(3),
  2=big-wander(∞), 3/6=run-away (`dist<4`), 4=merchant, 5/7=flee.
- ⚠️→formulado **NPC en planta distinta se TELETRANSPORTA** (el binario camina a una escalera,
  states 6/7); el escáner de ruta voraz (0x032C) se aproxima con A*; la huida exacta (0x06E4,
  ligada a hostilidad) se aproxima con un paso greedy.
- ✅ Texto del rechazo de join: **CERRADO en `58413c38`**. Esta entrada decía «el clon dice
  "Thy party is full."» — cadena FABRICADA, cero ocurrencias en los 125 ficheros de
  `original/u5/ultima5/`. Hoy `core/party.ts` emite las DOS cadenas del binario por
  separado, como los dos `print_string` consecutivos de `join_party`: `"Thou hast no room
  for me in thy party! ` (DS 0x9348) y `Seek me again if one of thy members doth leave\n
  thee.` (DS 0x9372), y **la conversación SIGUE** (`return 0`, TALK.OVL:0x081d), que era la
  mitad no textual de la divergencia.
- ⚠️ NPCs atraviesan puertas regulares sin abrirlas visualmente.

## Combate (para Fase 5)

- ✅(asm+runtime) **Fórmula de acierto** (Task 3.2, re/verified/combat.md): re-derivada
  EXACTA del binario (COMBAT:0x14D6 154d-1566, cita instrucción a instrucción):
  `acierta ⇔ rand30() >= (dexDefensor − statAtacante + 30)/2` — usa AMBAS destrezas
  (ni Redux ni xu4 tenían razón); armas contundentes (spellAttackRange[w−1]==8) usan STR.
  Runtime: `test_combat_trace_parity_live` VERDE (2026-07-10, 1 passed in 635.20s) —
  acierto + daño/defensa + veneno + trayectoria de HP calcados tirada a tirada contra
  el binario, en un escenario de melé sembrado (enemigos ya adyacentes, sin movimiento).
  ⚠️ El MOVIMIENTO de la IA queda FUERA de esta verificación runtime (divergencia de
  alcance deliberada, no laguna de modelado): su stream de rand no se captura con
  fiabilidad por el canal pty (~1/12 tiradas legibles), así que el escenario lo excluye
  sembrando a los enemigos ya en melé; la fórmula sigue asm-derivada con cita
  (re/notes/combat.md §8.2/§13) pero sin paridad runtime — ver re/verified/combat.md
  §15 para el detalle.
- ✅(asm) Daño, iniciativa (countdown 36−velocidad; velocidad de spawn dex±rand0(7)−4
  VERIFICADA EN VIVO), XP=maxHP/4+1 y turno de la IA: re/notes/combat.md.
- ✅(asm) **Passability del movimiento por CLASE del combatiente** (fix #43): la casilla
  pisable la decide COMBAT:0x0000 → kernel 0x2C4C (bitmap por sprite; re/notes/combat.md
  §10+§13:466-469), NO un único `walkable`. El clon usa: party a pie = `walkable`; enemigo
  acuático (isWater) = `waterEnemyPassable`; terrestre = `landEnemyPassable` (misma regla ya
  portada en world/enemies.ts y encounters.ts). Corrige el stall del soak Tramo 4: un acuático
  spawneado en agua (Bay) quedaba congelado y el combate no terminaba nunca. placeEnemies
  respeta además tipo-vs-tile al colocar.
- ⚠️ aproximado (fix #43) **Volador (`canFlyOverWater`)**: el clon lo trata como
  `landEnemyPassable ∪ waterEnemyPassable`. Esto NO es ni el asm ni la referencia: el binario
  usaría su propia fila del bitmap de clase (kernel 0x2C4C / tabla [0x54F4 + tile>>2]), SIN
  derivar para la clase voladora; y la única referencia que el proyecto cita como autoritativa,
  `Enemy.CanMoveToDumb` (docs/formats/combat-dungeons.md:278), usa `IsWaterTile` = check por
  NOMBRE (`Name.contains("water")`), no `IsWaterEnemyPassable` — divergen 9 tiles
  (Waterfall1-4, CornerWithWater1-4, WaterJugTable). Se deja land∪water como aproximación segura
  (no reintroduce el freeze; voladores comunes: Bat/Ghost/Gazer/Wisp/Dragon/MongBat) y se declara
  la medida pendiente en re/deliberate-divergences.md §3 (derivar la fila voladora del bitmap).
  NO se copia el name-matching de Redux a ciegas: es su heurística, no el binario.
- ⚠️ no cubierto (fix #43) **`isSand` (Sand Trap)**: no se comprueba; cae al bucket terrestre
  (`landEnemyPassable`). Inocuo hoy — el único enemigo isSand vive solo en el mapa Desert
  (combatmaps[4]), cuyos tiles tienen `landEnemyPassable` == `walkable`, así que no hay
  regresión. La referencia usa un check por nombre (`Contains("sand")`); pendiente igual que la
  fila voladora del bitmap. Misma nota de exclusión explícita que `canPassWalls` (ajeno a agua).
- ✅(asm) **Ataque a distancia / cursor de Aim** (fix #44, COMSUBS:0x0504 + 0x0C52/0x0A68):
  alcance del golpe = `ATTACK_RANGE_VALUES[w]` (DS 0x1664; 0 → melé); el proyectil vuela la
  línea Bresenham y para en la primera celda opaca (0x12DE / kernel 0x5D8E ≈ `rangeWeaponPassable`);
  un enemigo adyacente que te golpeó "interferes!" y anula el disparo (0x09FC). El MOTOR ya lo
  modelaba (`playerAttack` con `canReach` + raycast) y la UI de CLICK ya apuntaba a cualquier
  celda en alcance; sólo el TECLADO estaba capado a la casilla adyacente (arco inútil a 2+
  casillas). Reenganchado con `playerAttackDir` (recorre la línea cardinal hasta el alcance del
  arma y apunta al primer enemigo; si no hay, celda vacía → "Nothing!"). El cursor libre
  interactivo del binario (Aim, 0x0504) NO se replica pixel a pixel: el clon lo mapea a
  click-a-celda + A/flecha-en-línea (divergencia de UI declarada en re/notes/combat.md §14).
  Cierra el escenario del soak: el acuático del Bay, inmatable a pie, cae con arco desde la costa
  (unit `combat.test.ts` + e2e `combat-ranged.spec.ts`).
- ✅(asm) **Consumo de munición** (fix #51, COMSUBS:0x097C): PER-SHOT (el binario decrementa en
  0x0B3D, ANTES del hit en 0x0B51 → se gasta acierte o no). Bow 0x1A / magic bow 0x24 decrementan
  Arrows 0x1B y crossbow 0x1C decrementa Quarrels 0x1D del inventario compartido
  (`equipmentQuantities`); al llegar a 0 el desequipado barre al **PARTY ENTERO** y devuelve **N**
  al pack (09a2-09ab: el `call` va a SJOG.OVL:0x1b34, que recorre `si < g_party_size` sobre
  `unequip_item` y devuelve el conteo; el `add` de 09ab es de byte y **no lleva tope**). Las
  armas de arrojar {Dagger 0x10, Spear 0x15, Throwing Axe 0x16} a distancia > 1 gastan 1 unidad de
  sí mismas (09b8); la última se lanza y se PIERDE (09ce, sin devolver, y sólo del actor). Cableado
  en `attackWith` → `consumeAmmo` (sólo el jugador; la IA usa `strike` directo → paridad intacta)
  reusando `ammoItemFor`/`isThrownWeapon`/`unequipWeaponById` (arrojadiza) y `unequipItemById`
  (el barrido) de `equip.ts`. Tests: `combat.test.ts` (13, incluido un disparo que FALLA la tirada
  y aun así gasta la flecha) + e2e `combat-ranged.spec.ts` (las flechas bajan de 10 a 9).
  ⚠️ Bug PORTADO por contrato (underflow, #18): NO ocurre en el turno de un
  solo PJ — los arcos son de DOS MANOS (TYPE_TABLE 0x1A/0x1C/0x24 = 0x30), así que un PJ empuña como
  mucho UN arco y dispara una vez. Sigue siendo alcanzable entre PJs, pero por una vía estrecha:
  el ammo-gate 0x0d0c sólo bloquea EQUIPAR con 0, no DISPARAR, y **Bow 0x1A y Magic Bow 0x24
  comparten el pool de Arrows siendo ítems distintos**, así que el barrido del 0x1A no desarma
  al que lleva el 0x24. Si A vacía el pool con el Bow y B dispara después su Magic Bow, el binario
  hace `dec` sobre 0 → underflow a 255 (0x099c `dec`+`jne`, NO desequipa) → 255 flechas gratis.
  🔴 **CORREGIDO 2026-08-06 (#36)**: este párrafo decía «el clon CLAMPA a 0 (`Math.max`) y además
  desequipa el arco de B» — describía el clon ANTERIOR al fix de #18, que revirtió ese arreglo
  tácito; y la vía que citaba («B dispara SU PROPIO arco») no existe, porque el barrido de A
  también le habría quitado el suyo si fuera el mismo id. Las arrojadizas NO hacen underflow (su
  rama 0x9b8 comprueba `== 0` y desequipa ANTES de decrementar). Sling 0x11 (no consume en 0x097C) y
  Flaming Oil 0x13 (consumo en otro sitio, 0x0ACE) quedan fuera; detalle en
  re/deliberate-divergences.md §2 y derivación en re/notes/municion-36-acta.md.
- ⚠️ Resto de mecánicas (triple golpe, glass sword, robo de comida 3/4, daemon gate 1/8,
  división, botín rand30 vs treasure…): asm-derivadas con tests unitarios; paridad
  runtime pendiente (re/verified/combat.md).
- ✅(asm+cruce) Probabilidad de encuentro overworld: RE-DERIVADA en Task 3.13. NO es 1/16
  plano — es `rand(1,30) < spawn_threshold(bioma/planta/hora)` en el world_turn 0x1A60
  (spawn_threshold 0x0D8C: underworld 3, agua 0, pantano/montaña 2, else 1, +3 nocturno
  00-04). Terreno normal de DÍA no spawnea nunca. Portado en `world/loops/spawn.ts` y
  cableado en `game.ts` (`runContextTurn` → `outdoorTurn`) por el stream vivo compartido
  (re/verified/loops.md).
- ⚠️ Niveles: umbral de subida y HP por nivel viven en OUTSUBS:0x658/Lord British —
  no re-derivados aún → F.2. La GANANCIA de XP en combate ya es exacta: maxHP/4+1 SOLO al PJ que da
  el golpe mortal, aplicada a su contador del roster en el momento del golpe con cap
  9999 (COMBAT:0x1574 167a + 0x194A 1a2e-1a51); no hay reparto al party ni anuncio al
  cerrar el combate.

## Magia (Task 3.3 — CAST.OVL + CAST2.OVL)

- ⚠️→formulado (asm-derivado, paridad runtime pendiente) **Dispatcher exacto**
  (CAST:0x0dba, re/verified/magic.md): ventana temporal table-driven (tabla de 4 bits
  DS:0x1C90) → conocido → CONSUMO del hechizo mezclado ANTES del check de maná → coste =
  círculo → gate de nivel silencioso. Corrige 3 aproximaciones del clon (ventana por
  strings, consumo tras maná, sin gate). Verificado en el disasm instrucción a
  instrucción, pero SIN escenario de paridad runtime todavía (no cierra su ⚠️).
- ⚠️→formulado (asm-derivado, paridad runtime pendiente) **Fórmulas numéricas**
  re-derivadas y portadas con tests unitarios del clon: daño de ataque table-driven
  (attackValues[weaponId]: Grav Por rand(1,16), Vas Flam rand(1,30), Xen Corp 99), NO
  círculo·6; Mani = rand30() cap maxHP (no 25); Vas Mani = maxHP (no 50); luz 100/255
  MINUTOS (no turnos); estados P/Q/C/N/T en un global único {20,30,20,10,10}; resurrección
  exacta (HP=1, MP por clase, exp·karma/100 si karma<98, nivel=bitlength(exp/100)+1,
  maxHP=30·nivel). 30 tests de magia verdes, pero sin paridad runtime contra el binario
  (la fórmula no cierra su ⚠️ hasta el escenario DOSBox — gate de F.1).
- ⚠️ **33 efectos antes 'unsupported'** ahora con descriptor de efecto EXACTO (viento,
  comida, invocaciones, campos, blink, charm, polymorph, death vision, gate travel,
  mazmorra ±nivel, líneas AoE, terremoto…). Los que tocan mapa/combate se aplican al
  integrarlos con esos motores (Tasks 3.2/3.4/3.7); asm-derivados con cita, tests del clon.
- ⚠️ **Paridad runtime contra el binario**: arnés `re/tools/magic_parity.py` (clon↔RAM
  por globales legibles) con 10 escenarios verdes en el LADO CLON; los offsets de los
  globales están verificados en vivo (sonda 2026-07-10), pero el cast por inyección de
  teclas en DOSBox (test live opt-in) aún no dispara — protocolo del comando Cast
  pendiente de RE. Ver re/verified/magic.md.
- ⚠️ **In Quas Xen / In Quas Wis**: efecto exacto sin confirmar (confianza baja del scout).
- ❌ **Interfaz de lanzamiento «Lista de hechizos»** (ajuste *Sistema → Juego → Magia*,
  `u5.castingUI`, por defecto **Clásico**; `?casting=modern|classic` manda sobre lo
  guardado). QoL de ENTRADA, no de magia: sustituye ÚNICAMENTE el paso de teclear las
  iniciales rúnicas por una lista navegable de los 48 lanzables (palabras de poder,
  descripción del asset, círculo/coste, reactivos, modo de apuntado y cantidad mezclada).
  **Ni una regla del dispatcher cambia de sitio**: al elegir una fila, el panel arma el
  getstring rúnico FIEL (`ui/pickers.ts::pickSpellTyped`) y le ESCRIBE las iniciales por el
  mismo camino de teclado que el jugador (`ui/touch.ts::press` → keydown de `window` →
  `PromptManager`), así que `matchSpellByInitials` → `castSpell` corren idénticos, con las
  mismas filas de consola, el mismo consumo, los mismos prompts de objetivo y el mismo RNG
  — y la repetición graba las mismas teclas. Cancelar es el ESC del getstring («None!»).
  El modo Clásico queda intacto: con él, el sustituto ES `pickSpellTyped`, sin capa en
  medio. Capa en `game/src/enhanced/spells/`; candados en `game/tests/hechizos-*.test.ts`
  (careo clásico↔lista sobre consola, estado y RNG) y `game/e2e/hechizos-lista.spec.ts`.

## Mazmorras (Task 3.4 — DUNGEON.OVL + DNGLOOK.OVL)

- ⚠️→formulado (asm-derivado + cruce modelo↔clon; paridad runtime pendiente) **Trampas, campos,
  fosos y fuentes** (antes eran invención nuestra). Portadas con paridad de STREAM entre dos
  modelos (KernelRng Python ↔ OriginalRng del clon, 14 escenarios verdes) — NO es paridad runtime
  contra DOSBox: campos por low-nibble (0=sueño,1=veneno,2=fuego,3=energía), sueño/veneno = contest
  DEX (rand(1,30)≥DEX, tirada consumida también por muertos), fuego/bomba = rand(1,8) a cada
  miembro, energía (sólo 0x83 exacto) = rebote+daño, foso encadenado (1 nivel/foso, rand(1,8) c/u,
  sale por el fondo al Underworld), fuentes por TILE exacto sobre el PJ activo (cura/heal-total/
  veneno-incondicional/daño rand(0,7)), despertar dormidos rand(0,63)<4, bordes con WRAP toroidal.
  Se eliminó la "trampa de escalera" inventada. Ver re/verified/dungeon.md.
- ⚠️→formulado **Salida por Klimb**: planta 0 arriba → Britannia; planta más profunda abajo →
  Underworld (regla exit_dungeon 0x1D08). Klimb no daña.
- ⚠️→formulado (asm-derivado, paridad runtime pendiente): coste de tiempo por paso, RNG del
  render (flicker 2×rand/frame — EXCLUIDO del núcleo puro por alcance), monstruo errante
  (spawn/IA/emboscada), Attack cambia-nivel. El muro secreto de mazmorra quedó RECONCILIADO en
  Task 3.9: nibble hi 0xD del corredor = paso secreto revelable por Search (se mantiene); nibble
  0xC = cosmético/Look-only; el reveal 0x4E→0xB9/0xB8 es de mapa completo (ver re/verified/cmds.md).
- ⚠️ **Luz/antorchas**: el gate (sin antorcha NI hechizo → oscuridad total) es asm-exacto; la
  profundidad de raycast (4 celdas) es asm-derivada. Consumo de antorcha ya exacto de 3.1.
- ✅ Conectividad al Underworld verificada en los datos: bajan Wrong (planta 7, escalera en 7,7),
  Despise, Covetous y Shame. Deceit/Destard/Hythloth/Doom NO tienen escalera abajo en su última
  planta. Las salas de Deceit solo se entran por Klimb desde la planta superior (coincide con
  DungeonRoomAccess.csv).

## Transporte y navegación (Task 3.7 — MAINOUT + CMDS + kernel)

Subsistema nuevo (antes ausente). Port a nivel LIBRERÍA: `game/src/core/world/wind.ts`
+ `world/transport.ts`; el wind-tick sí está cableado al world-turn del overworld
(`game.ts` `runContextTurn` → `outdoorTurn`). **✅ CABLEADO en Fase 1.2** (Board/X-it/
Yell/Fire + rama naval de `move()` + `ship_try_move` + hundimiento del jugador): las
primitivas verificadas están conectadas al dispatch de `main.ts` y a `game.ts`
(`board`/`exitVehicle`/`yellSails`/`fire`/`navalMove`/`resolveNavalStep`). Journey E2E:
`game/e2e/naval.spec.ts`. Ver re/verified/transport.md y re/notes/transport.md §7.

**✅ Adquisición ORGÁNICA de la nave — CABLEADA en F1.5**: el Shipwright vivo
(`shop.ts` rama 0x84 → `buyShip` → `game.spawnDockShip(dockX,dockY,flags,0)`) coloca la
nave comprada como objeto del mundo (`worldObjects`, g_world_objects 0x5C5A) en el muelle
del OVERWORLD (loc 0, `SHIP_DOCK_X/Y`); Board la activa (slot0 = transport/hull/skiffs) y la
retira; X-it en tierra la re-atraca como objeto con su hull/skiffs vivos (persistente →
re-abordable, aviso DANGER si malparada). El hook fantasma `__u5test.boardTile`/`?ttile`
**fue RETIRADO** de `main.ts`. E2E del ciclo: `game/e2e/objects.spec.ts`. Clase C residual:
location del muelle (overworld vs pueblo) y coord del drop de X-it (deliberate-divergences §3).

- ✅ **RUNTIME-VERIFICADO (valor byte-a-byte en DOSBox)** **Cambio de viento**
  (kernel 0x2F62): 1×rand(0,63) por world-turn del overworld SIEMPRE (el único RNG
  del tick de viento); hit 1/64 → rand(0,4) y, si propone Calm, rand(0,255)≥192 la
  acepta (sesgo anti-calma). `test_wind_value_live` forzó semillas en el BP de
  0x2F62 y g_wind (0x5892) del turno siguiente casó EXACTO con el modelo para
  N/S/E/O/Calm. Encoding 0=Calm/1=N/2=S/3=E/4=O cerrado. Gate [0x5891]; Time-stop
  lo salta. ✅ El clon YA rueda el viento en pueblo (Task 3.13, cierra la divergencia); el binario lo tira 1×/llamada a 0x5910 (por tecla) — el "por tecla incl. inválidas" → F.2.
- ⚠️→formulado **Viento empuja opuesto a su nombre** (tablas 0x29F5/0x29F9) y
  **deriva del barco** por cadencia `di%3` (a favor 1/perpendicular 2/en contra 0),
  sin RNG. **Girar cuesta un turno**; fragata con velas izadas sin viento =
  becalmada; velas arriadas y skiff reman siempre. ✅ **CABLEADO (F1.2)** en la rama
  naval de `move()` (`navalMove`).
- ✅ **CABLEADO (F1.2)** **Board/X-it/Yell** (CMDS): g_transport_tile con facing+vela;
  Yell = Hoist/Furl (0x20↔0x24); estiba de skiff/alfombra; prioridades de
  desembarque (tierra > skiff > alfombra); "DANGER: SHIP BADLY DAMAGED!" (hull<10),
  "WARNING: NO SKIFFS ON BOARD!". Métodos `board`/`exitVehicle`/`yellSails` + teclas
  B/X/Y en `main.ts`.
- ✅ **CABLEADO (F1.2)** **Broadside** (CMDS 0x0962): solo perpendicular a la quilla;
  1×rand(1,20) por impacto; underflow del casco → hundido. Método `fire(dir)` + tecla
  F. **Reparación por camp** (kernel 0x3C9A): 25 min + rand(1,3) cap 99, bucle mientras
  hull<10 (`repairHull`, cableado del Camp completo → Fase posterior).
- ⚠️→formulado **HMS Cape** (MAINOUT 0x0670): 1 min/tramo y world-turn alterno con el
  Cape; 2 min cada tramo sin él. ✅ **CABLEADO (F1.2)** vía `navalStepCost` en
  `runNavalTurn`. **Naves NPC de vela** (0x2C) frenadas por viento (tabla 0x2BF6),
  deterministas; **casco inicial 0x64=100** (constante spawn 0x1050, `PIRATE_SHIP_HULL`),
  sembrado en el spawn del overworld.
- ✅ **CABLEADO (F1.2)** **Orden world_turn ↔ desplazamiento naval** (Concern C):
  NAVEGANDO (deriva), el binario corre el world_turn (0x069D) ANTES de que outdoor_move
  (0x0BB3) aplique el paso → los monstruos se mueven antes que el barco; `runNavalTurn`
  alineado SOLO para la deriva. **REMAR conserva su orden post-move** (getkey puro, va
  por outdoor_move como a pie: move_party 0x0354 y LUEGO world_turn 0x0539). El casco
  **NO se toca al hundir** (fiel; el `sub g_hull` es sólo de la rama de supervivencia).
  Residual medible: el reparto fino minutos/fase de la deriva entre ticks-WAIT y
  ticks-DERIVA (re/notes/transport.md §3, BP 0x069D vs 0x0BB3).
- ✅ **CABLEADO (F1.2)** **ship_try_move (MAINOUT 0x01FE)**: atraque ("Docked!"
  +auto-FURL, sin pisar el muelle 0x0306), "COLLISION!"/"BREAKING UP!" con daño de
  casco `rand(1,30)` y el cactus naval "OUCH!" `rand(1,8)`. Función pura `shipTryMove`
  + `resolveNavalStep` en la rama naval de `move()`. **Hundimiento del jugador**
  (damage_ship 0x10D6): `sinkPlayerShip` (skiff > alfombra > ahogo "DROWNING!!!")
  cableado en `resolveNavalStep` cuando el daño ≥ casco.
- ✅ **CABLEADO (F1.2)** **Broadside al impactar corre un world-turn ANTES del
  rand(1,20)** (0x0A5E): `fire()` corre `runContextTurn` (tick de viento rand(0,63))
  antes de `broadside` — orden del stream verificado en el disasm CMDS 0x0962.
- Preguntas abiertas restantes (re/notes/transport.md §10): semántica de la deriva,
  objetos 0x2C. (RESUELTO en review: polaridad 0x73E igual en los 3 callers; valor
  del viento verificado en runtime.)

## Santuarios, mantras, pozo de deseos y moongates (Task 3.8 — kernel + CAST2 + CMDS + LOOKOBJ)

Reglas EXACTAS del binario DOS (citas asm en re/notes/shrines.md), todas SIN RNG.
Port a nivel LIBRERÍA: `game/src/core/world/shrines.ts` + `world/wishingwell.ts`
(nuevos) + refino de `world/moongates.ts`. El edge de moongate sí está cableado
(`game.ts checkMoongate`); **el disparo AL PISAR de shrine_visit (tile 0x19) y del
peregrinaje al Codex (tile 0x11), el disparo AL MIRAR del pozo (tile 0xa1) y el flujo
de restauración (tile 0x1a) están CABLEADOS (F1.4, `game.ts checkShrineEntry`/`meditate`/
`submitShrineRestore`/`look`→`dropCoin`/`makeWish`).** El guardián del Shrine of the Codex
en (0xE9,0xEB) overworld se cableó en Task 3.13 (applyOutdoorSpecialTiles). La donación
(Flow 3 de F1.3) pasa de "listo sin trigger" a **jugable** (el modo `donation` de `meditate`
emite `shrine-donate-prompt`). Cobertura E2E de teclado en `game/e2e/shrines.spec.ts` (6
journeys: show-mantra, quest-complete, donación, restaurar, restaurar-cancelado, pozo+Board).
Huecos declarados Clase C (re/deliberate-divergences.md §F1.4): strings de meditación/quest/
lección/pozo (placeholders ⚠), comportamiento de ESC del meditate, minutos de meditación
(probable 0) y coord exacta del caballo del pozo. Ver re/verified/shrines.md y re/verified/loops.md.

- ✅ **RUNTIME-VERIFICADO (DOSBox) — Fases del día** (kernel_time_refresh 0x4a84 → DATA.OVL
  0x1EEA): `test_lunar_phase_feed_live` sembró g_day y forzó un cambio de hora, y
  g_felucca_phase/g_trammel_phase (0x5885/0x5886) salieron byte-idénticos a MOON_PHASES para
  los días 1/8/15/22. `moonPhasesForDay` correcto (sin off-by-2). Es el feed que selecciona el
  destino de la moongate → ancla runtime del subsistema.
- ⚠️→formulado **Moongates — semántica de destino** (kernel 0x47f4, leído del asm): los arrays
  0x5840/0x5848 son LOCATION/FLOOR de destino (0x483d/0x4852 → g_location/g_floor), no
  "buried"/"z" de Redux. Globals renombrados a g_moonstone_loc/g_moonstone_floor.
- ⚠️→formulado **Edge de medianoche** (kernel 0x494d): 00:00-00:09 la puerta cierra sin
  teleportar (portado en moongates.ts + game.ts). Antes ausente.
- ✅→cableado (F1.4) **Donación de santuario** (CAST2 0x0b47-0x0b91): 100·n oro por +n karma
  (clamp 99); n==0 = no-op (0x0b2d); no cobra si falta oro (re-pregunta en bucle). asm + cruce
  modelo↔clon; procedencia Redux resuelta. El modo `donation` de `meditate` emite
  `shrine-donate-prompt` (trigger en `game.ts`, tests `shrine-trigger.test.ts`/`shrines.spec.ts`).
- ✅→cableado (F1.4) **Máquina de estados del santuario** (CAST2 0x0966 + Codex 0x0d24): santuario
  fija la quest (0x0a88) SIN tocar el bit del Codex; el peregrinaje al **Shrine of the Codex**
  (tile 0x11 → 0x0d24) marca el bit visitado de la virtud de índice más bajo con quest (0x0d7d);
  volver al santuario completa la quest (0x0c18: +3 karma, +3 Humility, atributo del Avatar cap 30).
  Disparo AL PISAR vía `checkShrineEntry` (trigger en `game.ts`, tests `shrine-trigger.test.ts`/`shrines.spec.ts`).
- ✅→cableado (F1.4) **Restaurar santuario destruido** (CMDS 0x1202): virtud + mantra×3 tecleados
  + coord exacta → limpia el bit alto de g_shrine_destroyed[v] y repinta tile 0x19. Cadena de 4
  prompts de texto (selector.prompt) + `submitShrineRestore` (trigger en `game.ts`, tests
  `shrine-trigger.test.ts`/`shrines.spec.ts`).
- ✅→cableado (F1.4) **Pozo de deseos** (LOOKOBJ 0x0042): "Drop a coin?" → 1 oro → si el deseo
  contiene Corvette/Ferrari/Lamborghini/Lotus/Porsche/Horse (strings byte-exactas de DATA.OVL) Y
  estás en Paws (0x16)/Empath Abbey (0x1F) → spawnea caballo (tile 0x10). Fuera: sin efecto,
  moneda gastada. Disparo AL MIRAR (`look`→`dropCoin`→`makeWish`) + caballo montable por (B)oard
  (trigger en `game.ts`, tests `wishing-well.test.ts`/`shrines.spec.ts`).
- ⚠️ **PENDIENTE**: ceremonia final del Codex (0x0da2, todo texto/animación → endgame F1.10) y las
  descripciones de quest/lección (DATA 0x4b5e/0x4b6e word ptrs) NO están portadas — placeholders
  Clase C consolidados en re/deliberate-divergences.md §F1.4. El caballo del pozo se coloca por
  mapOverride (spawn físico via worldObjects → F1.5; algoritmo de coord de kernel_spawn_object
  0x97e4 sin derivar, Clase C). El teleport de moongate en vivo es best-effort (el chunk no se
  recarga con write_mem); la paridad es cruce modelo↔clon.

## Comandos sueltos (Task 3.9 — CMDS.OVL + SJOG.OVL)

Reglas exactas en `re/notes/cmds.md`; verificación en `re/verified/cmds.md`.
Paridad modelo↔clon (`re/tools/cmds_parity.py`, 17 escenarios) + `commands.test.ts`.
Bloqueo BSS: los cofres/objetos son registros de 8 B que no se cargan headless →
paridad de stream, no runtime (como el render de mazmorra).

- ✅ **Klimb-con-garfio** (CMDS 0x1C20, MECÁNICA NUEVA): Grapple + a pie + montaña
  0x0C; por miembro vivo `rand(1,30)` vs DEX (≥ escala; < → "Fell!" + `rand(1,5)`);
  los muertos no tiran. Antes el clon decía "What?" en el exterior. game.ts + commands.ts.
- ✅ **Escaleras de PUEBLO — transición AUTOMÁTICA al caminar** (TOWN 0x0810 → 0x052E,
  re/notes/town-klimb.md; fix #46). Pisar una escalera 0xC4-0xC7 cambia de planta según
  `orient = tile-0xC4` vs la dirección del paso (N0 E1 S2 W3): `orient==dir` → sube ("Up!"),
  `orient==dir^2` → baja ("Down!"), resto → cruza sin cambiar; la posición (x,y) NO cambia.
  ⚠ **Cierra un PUNTO CIEGO del catálogo**: el antiguo "Klimb ✅" cubría SÓLO el garfio
  exterior (CMDS 0x1C20) y la mazmorra (DUNGEON 0x1E10) — la variante de pueblo nunca se
  había portado y el clon metió un "K sube/baja escaleras" INVENTADO que atrapaba al jugador
  en el tejado del castillo de LB. Se RETIRÓ ese K-escaleras. El (K)limb de pueblo (TOWN
  0x0B82) sólo maneja escalas (0xC8 sube / 0xC9 baja), reja 0x86 (baja) y "encaramarse" a
  roca 0x4C / valla 0xCA·0xCB con K+dir; a caballo → "-On foot!"; resto → "Klimb-What?".
  game.ts (applyStairStep / klimbTown / klimbLadder) + main.ts (needs-direction).
- ✅ **Jimmy** (SJOG 0x0D4A, MECÁNICA NUEVA): puertas `rand(0,29)` (DEX>roll); cofres
  `rand(1,30)` vs threshold; cepo/prisionero 0x84/0x85 (libera NPC + karma+2 en pueblo);
  **llave se gasta SÓLO al fallar; cerradura mágica siempre rompe sin tirar**. Las
  **puertas 0xB9/0xBB están CABLEADAS** (`game.jimmy(dir)`, tiles de mapa + `state.keys`);
  cepo/cofre-objeto → F.2.
- ✅ **New Order** (CMDS 0x0DDC): swap de 2 miembros, Avatar anclado (" must lead!"),
  sin RNG ni turno.
- ✅ **Trampa de cofre** (kernel 0x2FD0) y **botín** (SJOG 0x1040/0x10B8/0x179E):
  tipos ACID/POISON/BOMB/GAS por tabla DS:0x559e; loot con orden EXACTO de rand;
  tablas leídas de DATA.OVL y ancladas con test (`re/notes/cmds.md §8-9`).
- ✅ **Ignite**: confirmada la semántica del kernel 0x3EF0 (saturating-add cap 0xFF);
  el clon ya era exacto (240 fuera / 112+rand(0,15) dentro).
- ✅ **Search puerta secreta**: DIRECCIONAL (party+dir), tile 0x4E → 0xB9 (mundo). Cierra
  la reconciliación de muro secreto de mazmorra diferida de 3.4 (nibble 0xD = corredor
  revelable; 0x4E→0xB9/0xB8 = mapa completo). Matiz: 0xC crumble→0xB0 vía search_dungeon
  (sin portar). **Fix #47 (PORT)**: el `game.search(dir)` del núcleo ya era direccional,
  pero el teclado (main.ts) invocaba `game.search()` SIN dirección → las puertas secretas
  eran IRREVELABLES jugando. Ahora la tecla `S` pide dirección vía `pendingDirCommand`
  (como Look/Open/Get; SJOG 0x095C `097e` = getdir 0x766c). Fix acompañante: `game.open()`
  leía el mapa BASE (ignoraba `mapOverrides`), así que una puerta secreta revelada+jimmy'd
  (sólo existe en la capa de override) daba "Nothing to open!" y el contenido tras el muro
  seguía inalcanzable; ahora lee `this.activeMap` (base+overrides) como jimmy/push/look.
  E2E: `commands.spec.ts` revela y cruza el corredor oculto de LB z1 (loc 17, tile 0x4E en 14,11).
- ✅ **Tecla Jimmy en el mundo — Fix #48 (PORT)**: `game.jimmy(dir)` ya existía y era exacto,
  pero NO tenía tecla: la `j` la ocupaba el diario QoL, así que una puerta secreta revelada
  (0xB9) sólo se destrababa por hook. **Decisión de fidelidad de teclas (las del original mandan;
  el QoL no las pisa)**: `J` = Jimmy en el binario (`kernel_cmd_dispatch` 0x32DA → SJOG `cmd_jimmy`
  0x0D4A), y es **DIRECCIONAL** (`0d5a` = getdir 0x766c, igual que Open/Search/Look), luego la tecla
  `j` ahora dispara `pendingDirCommand="jimmy"`. El **diario QoL se mudó a `F6`**: en Ultima V toda
  letra A–Z la despacha el kernel (incluso D/W, que imprimen "-What?"), así que ninguna letra está
  libre; F1–F10 (getkey 0x1D5E → 0xC9..0xD2) caen al default "What?" del dispatcher — no son comandos
  reales —, y F6 sigue la convención QoL de Tab=mapa y F5=guardar sin pisar nada del original.
  Jimmy no depende del Ready-RNG de F1.6: usa `this.rand` y cobra turno sólo en el éxito (0e10),
  ya calcado. E2E: `commands.spec.ts` cruza la puerta secreta SÓLO con teclado (`s`+dir revela →
  `j`+dir jimmy → `o`+dir abre → cruza); `panels.spec.ts` togglea el diario con `F6`.
- ⚠️→formulado **Push** (CMDS 0x161A): predicado de tiles y gating de tile exactos
  (0x5B;0x90-93;0xA5-A6;0xA8-A9;0xAD-AF;0xB4-B7; slide sii destTile==fill, pull sii
  partyTile==fill), pero sin capa de objetos (0x770e), sin rotación de facing 0x90/0xB4,
  ni rama de combate `g_cmb_actor`.
- ⚠️→formulado **Camp/Hole-up** (CMDS 0x0552): heal `rand(1,63)`/miembro/hora + MP por
  clase (A/M=INT, B=INT/2) + evento `rand(0,99)<25`; el gate de curación depende del
  cooldown horario `g_unk_588c`, no reproducible headless → modelo portado, excluido del
  set seed-exacto.
- ⚠️→formulado **search_dungeon** (SJOG 0x0646): detección de trampa/alijo/crumble en
  corredor de mazmorra; no portado (la mazmorra usa DungeonState).
- ✅ **Open/Get de cofres-objeto y switch de items** (SJOG 0x112C/0x1458) — **CABLEADO
  en F1.5** sobre la capa `worldObjects` (g_world_objects 0x5C5A). `open()` sobre un objeto
  `kind:"chest"` corre el orden EXACTO del binario (turno → karma-robo en pueblo −2/=0 →
  trampa si contents&0x80 vía `chestTrap` → botín `chestLoot`+`applyLootGrant` → elimina el
  objeto) con el RNG del **stream vivo** (`this.rand`). `get()` sobre `kind:"torch"` (rama
  sconce 0x148c) sube torches (cap 0x63) y elimina la antorcha. La capa persiste en el save
  (patrón #49) → cofre abierto sigue abierto. Tests: `chest-object.test.ts`,
  `torch-object.test.ts`, `world-objects.test.ts`, `e2e/objects.spec.ts`. **Residual (Clase
  C, deliberate-divergences §3)**: sembrado inicial de cofres de pueblo (O5, hoy por hook de
  test); jump-table completa de get_special_item (O3, sólo torch); mapeo id→array en
  `applyLootGrant` (O-loot); slot+6/+1 (O2); mapeo SAVED.GAM↔0x5C5A (O1). Get de cosecha/mesa
  (overworld, 0 rand) ya cableado desde 3.9. Ver `re/notes/objects.md`.

## Guardias, cárcel y Blackthorn (Task 3.10 — BLCKTHRN.OVL + kernel/TOWN/TALK)

Reglas exactas en `re/notes/blackthorn.md`; verificación en `re/verified/blackthorn.md`.
Port a nivel LIBRERÍA: `game/src/core/world/blackthorn.ts` (funciones puras + hooks;
el cableado al town-turn → F.2). Paridad modelo↔clon
(`re/tools/test_blackthorn_parity.py`, 29 escenarios) + `blackthorn.test.ts` (20).
BLCKTHRN.OVL **100% en el ledger** (9 funciones + padding). MECÁNICA NUEVA: el clon
no modelaba NADA de este subsistema.

- ✅ **Captura de Blackthorn** (TOWN 0x12ae → BLCKTHRN 0x060e): DETERMINISTA en el
  Palacio (loc 0x12) con el party no del todo muerto (`party_conscious_state>=0`).
  Interroga por el **primer santuario en pie** (`g_shrine_destroyed[i]==0`).
- ✅ **Interrogatorio** (0x054a): ceder el mantra = **karma −5 (suelo 0)** + santuario
  marcado caído (0xFF) + **compañero ejecutado** si numLiving>1 (else "rewarded with
  thy life"); negarse solo → "To the dungeon!"; negarse con party>1 → péndulo en la 4ª
  ronda. Matcher = **substring toupper** de 14 chars. Depósito final: **(10,7) loc 0x12,
  keys=0, a pie**.
- ✅ **Refuge / party-wipe** (0x0910, MECÁNICA NUEVA): party entero muerto
  (`party_conscious_state==−1`) **NO es game-over** — despiertas en el **castillo de
  Lord British (loc 0x11, planta 1, en (10,10))**, a pie, con **karma restaurado a suelo
  75**, light/torch a 0, comida a 63 si estaba a 0, **HP→maxHP por miembro** (0x0b98).
  El reloj se lleva a la **hora 6** (bucle advance_clock(9)); el minuto/día exactos
  dependen de la hora de entrada — el clon fija minuto 0 como canónico (⚠️).
- ✅ **Guardias TALK 0xFF** (0x01e2, resuelve el hueco de 3.5): el prompt "Dost thou pay?"
  **cobra al ACEPTAR ('Y')**, no al rehusar (helper 0x00ac: 'Y'→ret 0). loc 0x12 password
  "IMPE"; loc 5 (Minoc) 'Y' → `gold/=2`; otras 'Y' + `tributo<=oro` → 10 gp/miembro vivo.
  `ret` 1 (rehúsa/no-paga/password malo) = ruta de escalada del caller (→ F.2).
- ✅ **Merma de la Falsedad** (SHOPPES 0x019a, port de 3.6): con Shadowlord 0 (Falsehood)
  en la ciudad (`shadowlordLocs` vs `location`), cada compra sisa `gold -= rand(1,64)`
  con suelo 0. Función pura `postPurchaseGoldDrain`; cableado a tienda = 3.6 (→ F.2).
- ⚠️ **Byte de status del revive** (kernel 0xdc66) y **g_floor del depósito de captura**
  (0x08e7 deja 0xff, no re-fijado → riesgo para el cableado de captura → F.2 si se captura en
  planta alta); **gate del password** (`g_time_spell==0x1d` + Black Badge); **thunks de
  alerta del cañón** (0x7B06/0x7B12/0x7B1E, PLINK a overlay, cuerpos no desensamblados).
  Todos declarados; cableado en 3.9 (→ F.2). El compañero ejecutado persiste en el slot 15
  del roster (byte-exactitud del save → Task F).

## Creación de personaje — la gitana (Task 3.11 — FONT.OVL + DATA.OVL)

Reglas exactas del cuestionario de la gitana ("The Summoning"), re-derivadas de FONT.OVL
(la creación va empaquetada ahí, no en INTRO). Detalle y citas en `re/notes/gypsy.md`;
fidelidad en `re/verified/gypsy.md`. Port: `game/src/core/creation/gypsy.ts` + integración
en `core/state.ts:createNewGame`.

- ✅✅ **Semilla del bracket == 0** (RUNTIME-VERIFICADO en DOSBox: `g_rng_seed` leído en el
  título antes de elegir menú = 0x0000). Solo esto está medido en vivo.
- ✅ **Bracket determinista** (asm/estático): dado seed=0, el torneo es fijo; el eslabón
  "nada consume RNG entre el título y el primer `pick_virtue`" se apoya en el censo de rng.md
  (0 sitios srand/time_hash en INTRO/FONT — los reseeders; INTRO con 0 rand y el único rand de
  FONT es el propio pick_virtue 0x9a6), NO en medida en vivo del primer pick — ese cierre
  runtime (BP en pick_virtue 0x9a6 + leer seed) está en la lista de F.2. El clon replica la
  secuencia `rand_range(0,7)` con `OriginalRng(0)`.
- ✅ **Torneo de eliminación 4+2+1 = 7 preguntas** de las 8 virtudes. `pick_virtue` =
  rand(0,7) con rechazo usada/eliminada; la respuesta 'A'/'B' elige la virtud ganadora
  (menor/mayor índice); la perdedora se elimina.
- ✅ **Stats** = base INIT.GAM 15/15/15 + Σ(tablas STR/DEX/INT de DATA.OVL de las virtudes
  ganadoras). Finalize: `MP = INT`, `STR = max(15+ΣSTR, 20)` (suelo 20 solo STR). HP/maxHP/
  exp/level/clase/equipo NO cambian (quedan los de INIT.GAM: 60/60/150/2/'A').
- ✅ **Sexo** M/F escribe [reg+9]=0x0B/0x0C; no toca stats. **Nombre** al `name[9]` del reg 0,
  **máx 8 caracteres** (FONT 0x0bc8 `push 8` → rutina de input 0x3c58); **nombre vacío ABORTA**
  la creación sin escribir SAVED.GAM (FONT 0x0bcf `cmp [name],0` → `jmp 0xe40`).
- ✅ **Pantalla de creación cableada** (`game/src/ui/creation.ts` + título "Create New
  Character"): nombre → género → 7 preguntas con los textos REALES de QUESTION.DAT (28
  preguntas + 2 narraciones, `game/assets/questions.json`) → `applyGypsyCreation`. El
  emparejamiento par→pregunta usa `questionIndexForPair` (fórmula combinatoria verificada).
- ⚠️ **Transfer de Ultima IV** (INTRO.OVL, reescala de stats U4→U5): derivado y documentado,
  NO portado (el clon no importa personajes de U4).

## Endgame, Z-stats, Look y Search (Task 3.12 — ENDGAME/ZSTATS/LOOKOBJ/SJOG/FONT/FLAMES)

- ⚠️→formulado **Ready/equipar = ENCUMBRANCE** (ZSTATS try_equip_or_unequip 0x0c5c):
  slot por tabla de tipos DS 0x1a7e, y check de fuerza = suma de PESOS (DS 0x1aae) de
  los 6 slots + el nuevo vs **Str** (record+0x0c). El `reqStrength` por-item del clon era
  un misread (la misma tabla 0x1aae leída +16). + gate de munición (Bow/Crossbow),
  rechazo de Arrows/Quarrels (type 0x00), dos-manos exige ambas manos libres,
  "Ring vanishes!" 1/16 (ids 42/44), 9 clases "AMBFDTPRS". Portado en `core/equip.ts`,
  `party.ts` (`re/verified/zstats.md`).
- ⚠️→formulado **Endgame** (ENDGAME 0x0326/0x0407): playtime = (año−139/mes−4/día−5)
  con préstamo 28/13, e informe "N year[s], M month[s], D day[s]" con plural/separador
  exactos; rama "buena" gateada por la Sandalwood Box (g_wooden_box). Cierra el
  "endgame simplificado". Portado en `core/quest/endgame.ts`.
- ⚠️→formulado **Look — casos especiales** (LOOKOBJ look_dispatch 0x0502): carteles =
  las 5 caras reales {0x89,0x8A,0xA0,0xA4,0xF8} (antes mal); reloj de pie H:MM AM/PM
  (0xFA/FB), Flame por location (0xDE), entrada de mazmorra por banda X (0xDF), cielo
  día/noche (0x59). Portado en `core/game.ts` `look()`.
- ⚠️→formulado **Trap-detect** (SJOG search_trap_check 0x02ea): `rand(1,30) >=
  threshold` con Int (record+0x0e), tabla de mensajes exacta. Función pura `world/traps.ts`.
- ✅ **Tabla de 113 items ocultos de Search** reconciliada: el binario itera 113
  (`cmp si,0x71`); la 114ª de data.json es un centinela de ceros → `searchAt` capa a 113.
- ✅ **FLAMES.OVL** = sólo thunk PLINK (32 B), sin lógica. Ledger de los 6 ficheros 100%.
- ⚠️ **Mirar al sol DAÑA** al personaje activo (look_sky 0x0383, (active_char,1)+redraw):
  NO portado (el `look()` del clon es read-only, sin "quién mira") — cita en re/verified/lookobj.md.
- ⚠️ **Esfera de cristal (0x29) + View-a-gem**: renderer gráfico 32×32 + roll rand(1,30)
  vs Int; NO portado (necesita el "quién mira" + gem_view gráfico).
- ⚠️ **Display de Z-stats** (paginación, ♂/♀, 4 listas scroll) y **motor FONT** (justif.
  pixel-exacta, animador de escena): UI/presentación → Task F. El TEXTO del endgame sí es exacto.
- ⚠️ **"Readied."/"the stars." son de conveniencia**: el binario no imprime nada al
  equipar con éxito, y de noche pinta un campo de 80 estrellas + zodíaco, no una frase.
- ⚠️ **Cableado del RNG del Ready y de trapCheck a la UI/bucle**: `main.ts:631` no pasa
  `randRange` (ring-vanish nunca dispara en juego); trapCheck es función pura + hook → F.2.

## Bucles de contexto exterior/pueblo (Task 3.13)

Motores de turno TOWN 0x141E / MAINOUT 0x0A84 — el "corazón del stream". Derivación
`re/notes/loops.md`, verificado `re/verified/loops.md`, paridad modelo↔clon
`re/tools/test_loops_parity.py` (13 passed) + `tests/loops*.test.ts` (37 tests).

- ✅(asm+cruce) **Orden de RNG por turno**: exterior = viento rand(0,63) (getkey) →
  advance_clock(2) → tiles especiales → underworld_hazard → housekeeping → world_turn
  spawn rand(1,30); pueblo = viento por-tecla → [confusión rand(0,1)+rand(0,3)] →
  advance_clock(1) → post_turn (despertar rand(0,15) / tick de daño / pantano
  rand(0,29)) → housekeeping → guard_wander → 2º world-turn condicional. Motor puro
  en `world/loops/turn.ts`.
- ✅(asm+cruce) **Spawn exacto** (ver arriba, sección combate): reemplaza el 1/16.
- ✅(asm) **Viento de pueblo**: ahora rueda en pueblo (cierra transport.md §4), por
  turno consumido. El "por tecla incl. inválidas" → F.2.
- ✅(asm) **post_turn de pueblo (0x0F02)**: despertar 'S' rand(0,15)==0xF; **pantano
  DE PUEBLO rand(0,29) vs DEX** (RANGO DISTINTO del exterior rand(1,30) → función
  aparte `townSwampPoison`, F.2 no debe reutilizar `swampPoison`); tick de viento
  extra en tiles de daño 0x8C/0xBC/0x8F; confusión rand(0,1)+rand(0,3) en read_command.
- ✅(asm) **Peligros del exterior**: underworld_hazard 0x0A60 (rand(0,255)==0x69, 1/256),
  bridge_troll_ambush 0x1BE8 (gate 1/8 + tick de viento interno 0x5910 + rand(1,30) vs
  DEX; peaje **99−3·STR** [0x63] del **primer miembro consciente**, no del que falla),
  swamp_poison OUTSUBS 0x5FC, whirlpool 0x1248. Cableados en `game.ts`.
- ✅(asm) **Santuario AL PISAR (0x0C8A)**: guardián del Shrine of the Codex en
  (0xE9,0xEB) overworld — con Sacred Quest activa "Pass, Seeker!"; si no,
  "Passage denied!" + empuja 1 al sur. Cableado en `applyOutdoorSpecialTiles`. Sin RNG.
- ✅(asm) **guard_wander 0x0C78**: motor puro (rand(0,1) act/eje/signo, 1 ó 3 rands);
  cableado en vivo (tabla de objetos-guardia de small maps) → F.2.
- ✅ **Unificación del stream vivo** — **CERRADA en Fase 1.1** (PORT del stream vivo).
  `game.ts` ya consume un **único `OriginalRng` vivo** (`liveRng`): `move()` separa la
  geometría (`resolveStep`) del turno y **delega el esqueleto de RNG** en
  `outdoorTurn`/`townTurn` de `turn.ts` (orden EXACTO: viento → reloj → hazards →
  housekeeping → spawn). Eliminadas las tres fuentes separadas
  (`encounterRng = Math.random`, `Rng`-hash del picker de spawn, semilla derivada del
  combate). Determinismo E2E por seed en `game/e2e/determinism.spec.ts`. Detalle en
  `re/deliberate-divergences.md §2` (primer bullet).
- ✅ **Detalles — CERRADOS en Fase 1.1**: 2º world-turn del npc_engine ([0x65BF]),
  fases Quickness/montura del world_turn (montura = fase alterna), pick_spawn_coords
  exacto (2×rand(0,31) con re-roll por distancia, MAINOUT 0x0FC4/0x0F4E), re-roll de
  Shadowlords a medianoche (0x4FF5→0x5004) por el stream vivo en overworld/pueblo Y
  mazmorra/ignite (`advanceTurn`→`advanceClock` ahora forwardea el `rand`),
  combate/mazmorra sobre el stream (fork de `liveRng.getSeed()` + resync en `endCombat`;
  la mazmorra viva inyecta la instancia `liveRng`), y `guards.ts` (facing **X-only por
  el asm** 0x0D36; validación del DESTINO tras las 3 rands). **Único abierto**:
  `troll_toll` con prompt Y/N interactivo (hoy auto-pago) → **Fase 1.3** (capa de prompts).
- ⚠️ **Cableado del RNG del Ready y de trapCheck → Clase B**: el stream vivo unificado
  ya existe (Fase 1.1), pero Ready-RNG/trapCheck siguen sin cablearse a él — función
  pura + hook (equip/traps). Cierre pendiente en la integración de UI. Ver la nota de
  Zstats arriba.

## Trama (Fase 7)

- ⚠️ **Posiciones de los shards y artefactos**: no están en SEARCH_OBJECT; usamos ubicaciones
  jugables derivadas (Underworld junto a la salida de la mazmorra del Shadowlord; corona en
  Blackthorn, cetro en Stonegate, amuleto junto a Doom). Re-derivar las canónicas de ULTIMA.EXE.
- ⚠️ **Ritual de Shadowlords simplificado**: Use shard en la fortaleza de la Llama opuesta.
  El original exige invocar al Shadowlord (gritar su nombre) y atraerlo a la llama.
- ⚠️ **Shadowlords errantes por pueblos**: no implementado (aparición aleatoria sembrando miedo).

## Audio

- ❌ **Música**: el DOS original era mudo (`re/notes/audio-profile-1988.md`); usamos el parche
  XMI comunitario (Ultima V Upgrade Patch, de Voyager Dragon) como QoL. Sigue siendo un añadido
  NO fiel, pero desde 2026-09-11 su PROCEDENCIA es otra: ya no se pre-renderiza con un soundfont
  ajeno (GeneralUser GS), sino que se SINTETIZA EN VIVO con un emulador de OPL2/OPL3 propio
  (`game/src/ui/opl/`) alimentado por el banco de timbres del propio parche (`FAT.OPL`) — o sea,
  los datos del parche sobre el hardware al que apuntaba, en vez de nuestra interpretación de
  ellos. Y desde 2026-09-12 el MAPEO tampoco es criterio propio: qué canción suena en cada
  sitio está DERIVADO del driver del parche (`mid.drv` 0x016d, un switch sobre `g_location`,
  `g_floor`, `g_transport_tile` y `g_cmb_victory_flag` que el driver lee él mismo del segmento
  de datos del juego). Tabla completa, con el disasm al lado, en
  `re/notes/music-location-mapping.md`; el port la implementa en `game/src/ui/music.ts`
  (`songForLocation`). Las escenas guionizadas —portada, cinemática, creación de personaje,
  santuario, acampada, captura de Blackthorn, muerte y endgame— van por los selectores fijos
  del driver, cableados en `game/src/main.ts`. Lo que sigue sin derivar, y va rotulado en el
  código: qué beat del endgame del port equivale a cada cuadro del original (el guión del clon
  no lleva el contador que indexa la tabla de rango 0x24a), así que ahí se reproduce el ORDEN
  —tabla de cuadros → Joyous Reunion → Rule Britannia— y no los índices.

## Assets

- ❌ **Tiles HD**: upscaling xBR 4x de los TILES.16 originales (QoL visual conmutable; el
  atlas EGA 1:1 también se genera).
- ⚠️ Paleta EGA canónica (no la del conversor Raw16ToBMP de Redux, que tiene 2 colores raros
  y un doble-amarillo bug). Cotejar tonos contra captura DOSBox.
