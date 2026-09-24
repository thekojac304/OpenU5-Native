# Spec SDD — la emboscada del bucle de sueño (task #8)

**Rutina:** `camp()` = CMDS.OVL 0x0000-0x054e (`ret 6`). El **bucle de sueño**
0x01ee-0x030c y su **rama de emboscada** 0x0239-0x030c. Único hueco funcional
conocido del port tras cerrar FASE 1.

**Estado:** rama de emboscada **derivada al 100 %** (asm + tabla de enemigos volcada
byte a byte de DATA.OVL + identidad de los kernel-calls). Convierte #8 en cableado
puro. Lo que queda Clase C es **presentación** (escena/arena/SFX de la emboscada) y
el sub-evento de **guardia** (watch), no la mecánica del disparo.

**No re-derivado aquí (insumo, ya cerrado):** el flujo del camp portado (`Game.camp`,
`campHoleUp`, aparición 25 % — #7), la re-traza de la función completa
(`.superpowers/sdd/port-t7-review.md` ⚖️1), el oráculo del gate/aparición
(`re/notes/oracle-camp-event.md`), la fila ya abierta en
`re/deliberate-divergences.md:580`.

> **Batch 36 correction (H-171, original CMDS/EXE bytes).** The old name
> "housekeeping" for `0x2900` and `0x20fa` was wrong: these are status-panel
> redraw and one timer-tick delay. The true turn housekeeping entry is
> `0x2ae8`, absent from Camp. `0x5910` is viewport redraw, which calls the
> gated wind routine. CMDS `0x001c/0x001f` clears Q/T at Camp entry. The
> first redraw/ring of a new hour precedes that hour’s encounter roll.
> The target-hour check can end a Camp begun at nonzero minutes before twelve
> steps in its first hour (queued H-173). See the raw original bytes in
> `native/core/batch36-original-camp-bytes.log`.

---

## 0. Aritmética de direcciones (para leer los call-targets)

Todo el código vive en un segmento; una instrucción CMDS a fileoff F vive en
`load_seg:(0xBF80 + F)` (`CBASE=0xBF80`). El disasm muestra el fileoff local, así que
un **near-call** interno o externo aparece con un target "envuelto". El CS-offset
**real** = `(0xBF80 + target_mostrado) & 0xFFFF` (regla de `oracle-camp-event.md`,
verificada: `call 0x6112` → `(0xBF80+0x6112)&0xFFFF = 0x2092` = `rand`).

Identidad de cada call del bucle (todos resuelven a KERNEL residente):

| call en el asm | CS-offset real | rutina |
|---|---|---|
| `call 0x6112` | **0x2092** | `rand(lo,hi)` inclusivo (mismo stream que el gate 25 %) |
| `call 0x58d0` | 0x1850 | print string (DS ptr en AX) |
| `call 0x5910` (0x0204) | 0x5910... → ver nota¹ | viewport redraw (animation and gated wind RNG) |
| `call 0x808c` (0x0207) | **0x400c** | `kernel_ring_regen` (rand(0,7) por miembro con el ANILLO equipado 0x2c=44=Ring of Regeneration → +1 HP; NO un "status", oráculo 2026-07-18 — cableado en camp, 12×/hora) |
| `call 0x6980` (0x020a,0x02be) | 0x2900 | status-panel redraw; no food/poison processing |
| `call 0x60d6` (0x022b) | **0x2056** | `rng_time_hash()` — hash de la hora DOS (`int 21h AH=2Ch`), rng.md |
| `call 0x60fe` (0x022b) | **0x207e** | `rng_srand(ax)` — **SOBRESCRIBE `g_rng_seed`** con el reloj de pared |

> ⚠ **CORREGIDO por #186** (`rng-186-acta.md §5`). Estas dos filas decían «escena/alarma
> de emboscada (presentación)». La traducción del offset era correcta; la **identidad de la
> rutina de destino, no**: 0x2056/0x207E son las re-siembras del RNG que `rng.md` nombra
> desde la Task 1.3. No es presentación: es el punto donde **la paridad de stream deja de
> existir** (el `rand(0,7)` de 0x0239 que elige el enemigo ya sale del reloj). El residual
> NO es Clase C de AV.
| `call 0xffffa880` (0x02a2) | 0x6800 | **wake-check** de un miembro (`rand0(255)<0x10`, combat.md:257/402) |
| `call 0xffffbcbe` (0x0058,0x02ef) | **0x7C3E** | montaje de escena/combate con **arena de terreno** (arg0: 0=escena,1=combate) |
| `call 0xffffac42` (0x02fa) | **0x6BC2** | combat-init sembrado con `enemyType` (ruta sin flag&2) |
| `call 0xffffae02` (0x038e) | 0x6D82 | ¿tile transitable? (sub-evento de guardia) |
| `call 0xffffbdf6` (0x03a6) | 0x7D76 | avanza/coloca el monstruo que se acerca (guardia) |
| `call 0x617a` (0x031b) | 0x20fa | one INT 1Ch timer-tick delay |
| `call 0x8ffc` (0x0318) | advance_clock(5) | avance de reloj de 5 min |

¹ `0x5910` es el único caller de `maybe_change_wind` 0x2F62 (`rand(0,63)`, 1/64 cambia
viento) — `re/deliberate-divergences.md:1126`. Es decir **el tick de sueño consume
rand por el viento en cada paso de 5 min**.

---

## 1. Estructura del bucle de sueño (0x01ee-0x030c) — diagrama con offsets

Prólogo relevante (0x0000-0x009f):
- `bp+4` = **hours** (1..9), `bp+6` = **guardIdx** (miembro de guardia; normalizado a
  0..5 o −1 en 0x007d-0x008e), `bp+8` = **flags** de contexto.
- `bp-0x1c := 0xffff` (0x0015) — **enemyType de la emboscada, init −1**. Es el testigo
  que decide el epílogo (§3).
- `bp-0x28 := (g_hour + hours) mod 24` (0x0066-0x0079) — **target_hour**.
- `bp-0x1e := g_hour` (0x000d) — hora observada la vuelta previa.
- `flag&2` (0x002d): si puesto, monta la **escena de terreno** `0x7C3E(0, mapTile)`
  (campfire dibujado sobre el tile); si no, `0xa9b6` (=0x6936 party-anim/flash).

Bucle (cada vuelta = 1 paso de 5 min):

```
0x01ee  al = g_hour
0x01f3  if (g_hour == target_hour) → jmp 0x2fd        ; SALE del bucle (fin normal)
0x01ff  if (g_hour >= 24) g_hour -= 24                ; rollover
0x0204  call 0x5910    ; redraw, gated wind rand(0,63)     ── RAND (1/64 viento)
0x0207  call 0x400c   ; ring_regen: rand(0,7)/miembro con anillo  ── RAND (si aplica)
0x020a  call 0x2900    ; status-panel redraw, no survival tick
0x0212  if (g_hour == bp-0x1e)  → jmp 0x30c           ; la hora NO cambió → sin roll
        ; --- LA HORA CAMBIÓ (se cruzó un límite de hora) ---
0x021d  rand(0,63)                                    ── RAND: ROLL DE EMBOSCADA
0x0224  if (roll != 0)          → jmp 0x30c           ; miss (63/64) → sigue durmiendo
        ; --- roll==0 (1/64): EMBOSCADA (§2) ---
0x022b  … montaje + rand(0,7) tabla 0x1734 + combat-setup + early-return (§2/§3)
------------------------------------------------------------------------------
0x030c  bp-0x1e = g_hour                              ; recuerda la hora
0x0314  advance_clock(5)                              ; +5 min
0x031b  call 0x20fa    ; delay(1), no survival tick
0x0322  if (location < 0x21): call 0x4a84 sky/time status display (0x329-0x332)
0x0337  if (guardIdx == -1)     → jmp 0x1ee           ; sin guardia → siguiente paso
        ; --- HAY GUARDIA: sub-evento de watch (§4) ---
0x0340  rand(0,3)                                     ── RAND: roll de watch
0x034a  if (roll != 2)          → jmp 0x1ee           ; 3/4 → nada
0x0352  … monstruo se acerca un paso (rand(0,3) dirección) …    ── RAND
0x03d5  jmp 0x1ee
```

**Cadencia del roll de emboscada:** se tira **una vez por cada límite de hora
cruzado**. Para `hours=N` la última vuelta coincide con `target_hour` y sale por
0x01f3 **antes** del check ⇒ **N−1 rolls** (dormir 8 h = 7 rolls; P(emboscada) =
1 − (63/64)⁷ ≈ **10.5 %**). Confirmado por la re-traza de la review #7.

---

## 2. La rama de emboscada, instrucción a instrucción (0x022b-0x02fc)

```
022b: call 0x2056            ; ★★ rng_time_hash()  — reloj DOS (int 21h AH=2Ch)
022e: push ax
022f: call 0x207e            ; ★★ srand(ax) — RE-SIEMBRA g_rng_seed; paridad IMPOSIBLE desde aquí
0232: push 0 (lo) / 0235: push 7 (hi)
0239: call rand → rand(0,7)  ; índice de la tabla de enemigos     ── RAND
023c: bx = ax
023e: al = [bx + 0x1734]     ; enemyType = TABLA_0x1734[rand(0,7)]  (§2.1)
0244: bp-0x1c = enemyType    ; ← el testigo (deja de ser −1)
0247: print "Ambushed!"      ; DS 0x41e0 (fileoff 0x41f0, byte-exacto)
024e: if (guardIdx > -1):    ; SÓLO si hay guardia → estampa estado del party (§2.2)
0254-02bc:  loop j=0..5  sobre los 6 slots de party  → roster[k].status = 'G'|'P'
02be: call 0x2900            ; status-panel redraw
02c1: if (flags & 2):        ; ── selección de la vía de combate (§2.3)
02c7:    mapTile = mapa[party_y*..+party_x + 0x595a]
02ef:    call 0x7C3E(1, mapTile)     ; combate con arena de TERRENO
         else:
02f4:    call 0x6BC2(enemyType, flags) ; combate sembrado con el tipo (overworld)
02fd: (cae al epílogo §3)
```

### 2.1 La tabla de enemigos 0x1734 (VOLCADA de DATA.OVL)

`rand(0,7)` indexa 8 bytes en **DS 0x1734 = fileoff 0x1744** (regla `fileoff = DS+0x10`,
anclada por "Ambushed!" DS 0x41e0→fileoff 0x41f0). Cae dentro de `table_unk_1714`
(`re/notes/dataovl-tables.md`; Redux no la mapea → esta es la primera identificación).

Bytes: `29 14 15 18 16 19 24 14`. ⚠️ **CORRECCIÓN (resolución estática #8,
`camp-ambush-resolution.md §1`):** el mapping de nombres de abajo era INCORRECTO — usaba
el espacio `monsterNamePtrs1866` (DS 0x1866), un índice DISTINTO del `defIndex`. Por
disasm (`0x6BC2`=render_animated_tile → `kernel_spawn_actor` 0x6506, que hace
`[bp+0xc]<<3+0x13c1` para el sprite y guarda el byte como el campo TIPO del actor) el
byte ES el **`defIndex`** (índice de TIPO en el espacio de 48 de `monsterNamesUpper`/
`enemyDefs`), no un puntero de `monsterNamePtrs1866`. Nombres REALES (`enemyDefs[i].name`,
verificado):

| idx (rand 0..7) | tipo | nombre | prob |
|---|---|---|---|
| 0 | 0x29 (41) | **Troll** | 1/8 |
| 1 | 0x14 (20) | **Giant Rat** | 2/8 |
| 2 | 0x15 (21) | **Bat** | 1/8 |
| 3 | 0x18 (24) | **Slime** | 1/8 |
| 4 | 0x16 (22) | **Giant Spider** | 1/8 |
| 5 | 0x19 (25) | **Gremlin** | 1/8 |
| 6 | 0x24 (36) | **Headless** | 1/8 |
| 7 | 0x14 (20) | **Giant Rat** (repetido) | — |

**Giant Rat sale doble** (idx 1 y 7) ⇒ 25 %. idx 0 = 0x29 = `TROLL_DEF_INDEX` (41): el
idx0 de la emboscada es el MISMO troll del peaje del puente — la emboscada del camp y el
peaje son el mismo mecanismo de spawn+combate.

Estos 8 tipos son directamente el `OverworldEnemy.defIndex` del port
(`game/src/core/world/enemies.ts:18` = "Índice en la tabla de 48 enemigos").

### 2.2 Estado del party al entrar (loop 0x0254-0x02bc)

Sólo corre **si hay guardia** (`guardIdx > -1`). Recorre los 6 slots de combate del
party (registros DS:0xBA14, campo +3 = slot de roster; combat.md:25), y por cada
miembro `k` válido (`k < g_party_size`): llama `kernel 0x6800(j)` (wake-check
`rand0(255)<0x10`) y estampa `roster[k].status` = **'G'** (0x47, despierto/listo) o
**'P'** (0x50) según el histograma `bp-0x14[]` que el prólogo (0x00a2-0x0149) construyó
contando estados 'G'/'P'/'S' del party. Interpretación: el party **entra dormido y se
va despertando** para el combate; el de guardia (`guardIdx`) ya está despierto. La
semántica exacta de `bp-0x14[]`/`0x6800` (quién despierta este tick) es un **detalle de
combat-state Clase C** — no cambia el disparo ni el tipo de enemigo. Si `guardIdx==−1`
(caso típico del port, sin guardia) el loop **se salta entero** (0x0252 `jle 0x2be`).

### 2.3 La arena y las dos vías de combate

- **`flags & 2` puesto** → `0x7C3E(1, mapTile)` con el **tile de terreno** bajo el party
  (arena por bioma; misma rutina que dibujó la escena de camp en el prólogo).
- **`flags & 2` claro** → `0x6BC2(enemyType, flags)` sembrado con el tipo.

El **camp de overworld a pie** (el caso que ve el usuario, el que porta #7) tiene
`flags = 0x0004` (observado en RAM viva, `oracle-camp-event.md §D`) ⇒ **`flag&2 = 0` ⇒
vía `0x6BC2`**. La vía `0x7C3E`/terreno es para el otro contexto (p.ej. mazmorra).

⚠️ **Arena real = campfire, no el bioma plano.** El port ya enumera
`CombatMapIndex.CampFire` (`combat/encounters.ts`), que es la arena dedicada de
"ambushed while camping" de U5 (party alrededor de la hoguera). La ruta `0x6BC2`
probablemente **fuerza ese mapa**; `startCombat` del port, en cambio, elige la arena por
`combatMapForTile(tile)` (bioma). **Recomendación de cableado:** rutar la emboscada de
camp a `CombatMapIndex.CampFire`, no a la arena de bioma. Confirmar la selección exacta
de `0x6BC2` es un paso de implementación (o Clase C de presentación, solapa con #28).

---

## 3. El DESPUÉS (epílogo 0x02fd-0x0309)

```
02fd: cmp bp-0x1c, -1          ; ¿hubo emboscada? (enemyType != -1)
0301: jg 0x306
0303: jmp 0x3ea                ; NO hubo → helper 0x0400 (heal + gate 25 % aparición)
0306: ax = 1 ; jmp 0x549       ; SÍ hubo → RETORNO TEMPRANO (ret 6, ax=1)
```

Consecuencias de la emboscada (todas confirmadas):
1. **Retorno temprano `ax=1`** salta el helper 0x0400 → **sin curación parcial, sin gate
   del 25 %, sin aparición, sin level-up**. La acampada se corta en seco.
2. **Las horas restantes NO se duermen.** El reloj queda a la **hora de la
   interrupción** (sólo se avanzó hasta el tick que cruzó la hora del roll ganador; el
   `advance_clock(5)` del paso ganador ni siquiera corre — la rama salta a combate antes
   de 0x030c). El party NO descansa la noche.
3. **`ax=1`** es la señal al dispatcher de camp (kernel 0x3C9A) de "combate iniciado".
   El combate corre (COMBAT.OVL); al terminar se vuelve al **overworld** a la hora de la
   interrupción (NO se re-entra a camp: `camp()` ya retornó).

---

## 4. El sub-evento de guardia / watch (0x0340-0x03e8) — Clase C

Sólo si hay guardia. Por cada paso de sueño: `rand(0,3)==2` (1/4) → un monstruo cercano
(registro `bp-0x18`) **se acerca un paso** hacia el campamento; `rand(0,3)` elige
dirección (dec y / inc y / inc x / dec x), `0x6D82` comprueba tránsito, `0x7D76` lo
recoloca. Es la mecánica de "el centinela ve algo moverse en la oscuridad". **No
desemboca en la emboscada** (son eventos independientes); es cosmético del turno de
guardia. Queda **Clase C** (el port no tiene guardia de camp modelada; presentación).

---

## 5. Estado HOY del clon (grep) — sin rastro de emboscada de camp ✅

`grep -rniE "ambush|emboscada|Ambushed" game/src`:
- `world/loops/hazards.ts` = **bridge_troll_ambush** (MAINOUT 0x1BE8), otro mecanismo.
- `game.ts`/`commands.ts`/`main.ts` = comentarios que documentan que "Ambushed!" se
  **retiró** en #7 y que el bucle de sueño **no está modelado** (docstring de `camp()`
  1341-1347). Sin string "Ambushed!" en el manifiesto (guarda de strings verde).

Confirmado: **cero código de emboscada de camp**. `camp()` hace `advanceClock(60)×hours`
y salta directo a `campHoleUp` (heal+gate). El hueco es real y limpio.

---

## 6. Plan de cableado para el implementer

### Qué tocar
`Game.camp()` (`game/src/core/game.ts:1365`). Hoy: `for h<hours: advanceClock(60)` →
`campHoleUp`. Cablear el roll por hora **dentro del loop**, antes del heal:

```
for (h = 0; h < hours; h++) {
  advanceClock(60)                       // cruza la hora (incluye re-sort de Shadowlords)
  if (h < hours - 1) {                   // N-1 rolls: la última hora = target, no rola
    if (rand(0,63) === 0) {              // 0x021d  (1/64 por hora)
      const type = AMBUSH_TABLE[rand(0,7)] // 0x0239 → tabla 0x1734 (§2.1)
      // early-return: SIN campHoleUp / aparición
      return [ msg("Ambushed!\n\n"),      // DS 0x41e0 (re-alta en el manifiesto)
               ...startCampAmbush(type) ] // fork a combate (§2.3)
    }
  }
}
const res = campHoleUp(...)  // sólo si NINGÚN roll acertó
```

`startCampAmbush(type)` = el patrón del troll/errante ya portado: construir un enemigo
sintético `{ defIndex: type, ... }` y llamar **`startCombat`** (`game.ts:3666`, fork del
stream vivo en `liveRng.getSeed()`). Diferencias con el encuentro overworld:
- El **tipo** viene de `AMBUSH_TABLE` (8 entradas, §2.1), no del picker de spawn.
- La **arena** debería ser `CombatMapIndex.CampFire`, no `combatMapForTile(tile)` (§2.3)
  → probablemente un parámetro nuevo/override en `startCombat` o una variante.
- No hay `overworldEnemies.removeEnemy` (el enemigo no existía en el mapa).

Constante nueva: `AMBUSH_TABLE = [0x29,0x14,0x15,0x18,0x16,0x19,0x24,0x14]` — **anclarla a
los bytes de DATA.OVL** (leer `fileoff 0x1744`, como hacen las tablas de loot/trampa en
`cmds_parity.py` `_load_data_ovl_tables`), no un volcado a mano (romper circularidad,
patrón CRITICAL-1 de la review de #7).

### Tests
1. **Roll por hora (puro):** con `rand` scripted, verificar N−1 rolls por acampada de
   N h; `scripted([...,0])` en la posición de un cruce → emboscada; enemigo = tabla en el
   índice del `rand(0,7)` siguiente. (Estilo `commands.test.ts`.)
2. **Early-return:** cuando dispara, `camp()` **no** llama `campHoleUp` ni emite
   "Party rested!"/aparición; el reloj queda a la hora del cruce (< N·60). (Estilo
   `game.test.ts`.)
3. **e2e (Playwright):** acampar con un seed que garantice el 0 en un cruce → aparece
   "Ambushed!" y arranca combate con el tipo esperado. (Suite `game/tests`.)
4. **Ancla de tabla:** test que compara `AMBUSH_TABLE` con los bytes reales de DATA.OVL
   (como `test_tables_match_data_ovl`).

⚠️ **NO paridad seed-exacta de `camp()`**: sigue **excluido** del set (cero `camp*.json`
en `re/parity/cmds/`; `re/verified/cmds.md`). El bucle de sueño consume por tick el
viento (0x5910/rand 1/64), ring_regen (0x400c) y los redibujos/delays que `advanceClock` **no**
reproduce paso a paso → el stream del `camp()` completo diverge por diseño
(`deliberate-divergences.md:580`). El port modela el **resultado observable** (emboscada
~1/64 por hora + tipo correcto), no el stream interno. Tests = conductuales, no de seed.

### Clase C (NO fabricar)
- **Escena/arena/SFX de la emboscada** (arena campfire; el "party
  dormido despertando"): presentación → **catálogo AV / task #28** (pantalla de combate
  fiel). El vídeo del usuario **no** captura una emboscada (evento ~10 %/noche).
- ★ **NO es Clase C: `0x2056`/`0x207e` (0x022b) son la RE-SIEMBRA con el reloj DOS.**
  Al acertar el 1/64, el binario hace `srand(rng_time_hash())` ANTES de tirar el
  `rand(0,7)` del tipo de enemigo ⇒ desde ahí la paridad de stream **no se incumple, deja
  de existir**. El port conserva su determinismo (precedente D6, #180) y hace bien; lo que
  falta es DECLARARLO en `camp.ts:campSleepStep`. Ver `rng-186-acta.md §5` (tarjeta T4).
- **Sub-evento de guardia** (§4): sin guardia modelada en el port → cosmético.
- **`bp-0x14[]`/0x6800** (quién despierta exactamente, §2.2): detalle de combat-state.
- **`flags & 2`** y la selección `0x6BC2` vs `0x7C3E`: confirmar en implementación; el
  camp de overworld es la vía `0x6BC2` (flag=0x0004).

---

## 7. Estimación del cableado

**Bajo–medio.** La mecánica del disparo es un cambio localizado en `Game.camp()`
(~15-25 líneas) + `AMBUSH_TABLE` anclada + un `startCampAmbush` helper que reusa
`startCombat`. El único punto no trivial es la **arena campfire** (¿override de mapa en
`startCombat`?) y volver a dar de alta "Ambushed!" en el manifiesto de strings. Los
tests conductuales (roll por hora, early-return, ancla de tabla, e2e) son directos. La
escena visual completa **no** es de esta tarea (Clase C → #28). Sin bloqueos: el sistema
de combate del port (`Combat`, `startCombat`) ya existe y los encuentros errantes ya lo
usan con el fork del stream vivo.
