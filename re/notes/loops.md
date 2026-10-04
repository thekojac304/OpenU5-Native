# Bucles de contexto exterior/pueblo — TOWN.OVL + MAINOUT (Task 3.13)

Re-derivación EXACTA de los dos motores de turno de nivel 1 (los "corazones del
stream"): el bucle de pueblo `town_main_loop` (TOWN.OVL 0x141E) y el bucle
exterior `outdoor_main_loop` (MAINOUT.OVL 0x0A84), más las funciones puras que
consumen sus rand. Direcciones = offset dentro del .OVL; `load_seg 0x81D` →
CS = fileoff + 0x81D0; near-call a kernel 0xNNNN aparece como
`call 0x(NNNN+81D0)&FFFF`. Convención pascal: primer push = arg izquierdo;
`rand(lo,hi)` = kernel 0x2092, ambos inclusive (`re/notes/rng.md`).

Port TypeScript: `game/src/core/world/loops/{spawn,hazards,guards,turn}.ts` +
integración en `game/src/core/game.ts` (tickTurn/move/applyOutdoorSpecialTiles).
Paridad modelo↔clon: `re/tools/test_loops_parity.py` (`loops-run.ts` ↔
`loops_parity.py`), escenarios en `re/parity/loops/`.

## 1. Orden de RNG por turno — BUCLE EXTERIOR (MAINOUT 0x0A84)

```
0b14 tick_and_getkey 0x0598 → 0x5910:  VIENTO rand(0,63) SIEMPRE (incl. blocked/idle)
     dispatch: flechas→outdoor_move 0x0490 · dígitos→0x4080 · ASCII→0x3178
0c30 handler==0 (BLOCKED) → SIN reloj, SIN world-turn → loop
0c39 advance_clock(2)                    coste base
0c56 puente 0x6A/0x6B → bridge_troll_ambush 0x1BE8
0c64 pantano (tile 4) a pie → swamp_poison OUTSUBS 0x5FC (rand(1,30)/miembro)
0cd0 underworld_hazard 0x0A60            (floor≠0: rand(0,255)==0x69 → daño)
0cd3 turn_housekeeping 0x2AE8            (hambre rand(1,8) + anillo rand(0,7))
0d11 world_turn 0x1A60:  SPAWN rand(1,30) SIEMPRE + move-loop de monstruos
```

**El viento va PRIMERO (en el getkey), antes del housekeeping y del spawn.** El
viento es 1×/iteración; el spawn es 1×/world-turn. El terreno lento (0x03E0)
corre 1-2 world-turns EXTRA (→ 2/3×rand(1,30)) pero el viento sigue siendo 1×.

### 1.1 `spawn_threshold` — MAINOUT 0x0D8C [asm verificado]
```
floor > 0x7F (underworld)          → 3
tile ∈ [0x20,0x26] (agua/océano)   → base 0
tile == 4 (pantano) ó ∈ [9,0xF]    → base 2
else                                → base 1
hour >= 0x20 (rama muerta) ó < 5    → base += 3   ; bonus nocturno SÓLO 00:00-04:59
```
El `cmp g_hour,0x20; jae` es rama muerta (hour∈0..23): el atardecer (20:00-23:59)
NO recibe el bonus — quirk del binario.

### 1.2 Gate de spawn — MAINOUT 0x1A9F-0x1AB5 [asm verificado]
```
1aa7: rand(1,30) → si (roll)
1aac: spawn_threshold → ax
1aaf: cmp ax, si; jle 0x1ab6      ; threshold <= roll → NO spawn
1ab3:   spawn_monster 0x0FC4       ; threshold > roll → spawn
```
Consecuencia EXACTA (no obvia): terreno normal de día (threshold 1) **nunca**
spawnea (roll≥1); agua (0) tampoco. Sólo pantano/montaña (2), underworld (3) o la
noche (+3). El `spawn_monster` en sí (weighted_pick rand(0,255), tile_to_monster
rand(0,64)/(0,7)/(0,3), pick_coords 0x0F4E: rand(0,31)×2 con re-roll por DISTANCIA
—>6 en ambos ejes de la party, SIN check de pasabilidad ni tope) es RNG aparte que
sólo rueda cuando el gate dispara. **Reemplaza el 1/16 inventado del clon.**

`world_turn` además tiene, ANTES del spawn roll, tres gates sin rand: Time-stop
'T'→return 0; Quickness 'Q'→fase alterna [0x2C55]; montado 0x12/0x14→fase
alterna [0x2C57] (la montura mueve 2×/world-turn). El clon gatea sólo Time-stop;
las fases de Quickness/montura → F.2.

### 1.3 `underworld_hazard` — MAINOUT 0x0A60 [asm verificado]
`cmp g_floor,0; je ret` → sólo underworld. `rand(0,255); cmp ax,0x69; jne ret` →
el 0x69 es el VALOR de la rand (1/256), **NO un tile**. Al acertar: apply_damage
+ party_random_damage (rand(1,8)/miembro). Consume 1×rand(0,255)/turno en el
underworld (parte del stream). El string vive en DATA.OVL DS:0x2B1D (sin resolver).

### 1.4 `bridge_troll_ambush` 0x1BE8 / `troll_toll` 0x1B3E [asm verificado]
```
1bf7: rand(0,7); si !=0 → return          ; gate 1/8
1c01: si g_transport_tile != 0x1C → return ; sólo a pie
1c0b: call 0x5910 KERNEL (tick de viento/anim, NO el world_turn 0x1A60)  ; ENTRE gate y DEX
      por cada miembro no-'D'/'S':
1c76:   rand(1,30); si DEX >= roll → pasa; si DEX < roll → troll_toll y BREAK
```
El tick interno (0x1C0B) es el **kernel 0x5910** (viento + npc_tick), NO el
world_turn 0x1A60 de monstruos — corre ENTRE el gate y las tiradas de DEX.
`troll_toll` (0x1B3E): `toll = 0x63 − 3·STR` = **99 − 3·STR** (0x63 = 99, no 100);
el STR es el del **PRIMER MIEMBRO CONSCIENTE** ('G'/'P', via party_conscious_state
0x39FC → g_cmb_scratch_x, 0x1B4B/0x1B56), NO el del que falla la tirada. Prompt Y/N;
si Y y oro≥toll → cobra; spawn de un troll huyendo con el oro. Sin RNG propio.

### 1.5 `swamp_poison` — OUTSUBS 0x5FC (kernel-survival.md §6)
Por cada miembro no-'D'/'P': `rand(1,30) > DEX` → status='P' "Poisoned!". El daño
(1 HP/turno) lo aplica el housekeeping. `whirlpool` (0x1248): reubica al underworld
en (0x22,0x12), sin RNG de destino.

## 2. Orden de RNG por turno — BUCLE DE PUEBLO (TOWN 0x141E)

```
town_read_command(1) 0x0DC4 → world_turn 0x5910 @0x0DD0:  VIENTO rand(0,63)
     POR TECLA LEÍDA (incl. teclas que NO consumen turno) + npc_tick
     dispatch: flechas→town_move 0x0600 · dígitos→combo_lock · ASCII→0x3178
     result==0 → NO consume turno (sin reloj, sin housekeeping)
     result==3 → re-lee tecla (otro world_turn #1)
advance_clock(1) 0x15D4
post_turn 0x0F02 + turn_housekeeping 0x10D0   (hambre/anillo)
guard_wander 0x0C78 @0x165F                    (RNG propio de TOWN, si hay guardias)
npc_engine 0x1352 → world_turn #2 @0x1376      (si [0x65BF]≠0 o result==2): otro VIENTO
```

**El viento de pueblo rueda POR TECLA** (no por turno consumido) — cierra la
divergencia de `transport.md` §4 (el clon sólo lo rodaba en overworld). El clon lo
aproxima por turno consumido; el "por tecla incl. inválidas" y el 2º world-turn del
npc_engine son el hook de F.2 (requieren el flag [0x65BF] y el getkey-level).

### 2.0 `post_turn` — TOWN 0x0F02 [asm verificado]
Efectos por turno consumido (antes del housekeeping 0x10D0), en orden:
- **Despertar** — BUCLE sobre TODO el roster (0x0F18 si=0x55b3 … 0x0F35 add si,0x20;
  inc di; cmp di,party_size): por CADA miembro dormido 'S', `rand(0,15)` @0x0F2A;
  ==0xF (1/16) → despierta ('S'→'G'). Una tirada por cada 'S', no sólo el miembro 0.
- **Tile de daño** 0x8C (Blackthorn exec) / 0xBC / 0x8F: corre un tick de viento
  KERNEL 0x5910 extra (@0x0F8A/0x10BA) + `party_random_damage`.
  > **A4-PARITY2 D-85 (2026-10-03) — relabel and completion.** Disassembled again: tile **0x8C is the TRAPDOOR** (`cmp ax,0x8c` @0x0F63), not a
  > "damage tile"; the prologue @0x0F8A `call K:0x5910` + @0x0F8D `call K:0x2AA8` (`party_random_damage`) runs for EVERY trapdoor, before the
  > `cmp [g_location],0x1d` test at 0x0F96, and the Burning twin (tiles 0xBC / 0x8F, @0x10BA/0x10C4) has the order wind, message, damage. The ports
  > omit the prologue (recorded as D-90, not fixed). The location-29 kill loop @0x0FF9-0x103A is bounded by the BYTE [0x585b] (party size), not the
  > roster: indices 0..party_size-1, HP := 0 and status := 'D', no status filter, no cap (fixed by D-85). `native/core/a4-parity2-findings/D85-FINAL.md`.
- **Pantano de pueblo** (tile 4) a pie (0x1050/0x1056): por miembro no-'D'/'P'
  `rand(0,29)` @0x108D; si `> DEX` → 'P'. ⚠️ **RANGO DISTINTO del exterior**
  (`rand(1,30)`): span 30 empezando en 0, no en 1 → función aparte `townSwampPoison`,
  **F.2 NO debe reutilizar `swampPoison`**.
- Confusión (en `town_read_command` 0x0DC4, antes del dispatch): si `[0x5957]≠0`,
  `rand(0,1)` @0x0E00; si ==1 remapea a comando aleatorio → `rand(0,3)` @0x0E1C
  (tabla 0x2742) + dec del contador de confusión.

### 2.1 `guard_wander` — TOWN 0x0C78 [asm verificado]
Por cada objeto-guardia (tile & 0xFE == 0x10) en la planta del jugador:
```
0cbd: rand(0,1); actúa si ==0 (~50%). !=0 → 1 rand, no mueve.
      [4 checks de vecinos tile 0xA2/0x43 — SIN RNG; alguno bloquea → 1 rand, no mueve]
0d19: rand(0,1) eje (0=Y, 1=X)
0d27: rand(0,1) signo (sign = 2·r − 1 = ±1); mueve; tile 0x10/0x11 según signo
```
⇒ 1 rand si no actúa o si un vecino bloquea; 3 rands si actúa y se mueve. Es el
ÚNICO RNG propio de TOWN vivo por turno. El clon no rastrea objetos-guardia en
small maps → motor puro probado, cableado en vivo → F.2.

## 3. Divergencias con el clon (estado tras 3.13)

1. ✅ **Spawn exacto**: `rollSpawnGate` (rand(1,30) vs spawn_threshold) por el
   stream compartido reemplaza el 1/16 inventado. Terreno normal de día ya no
   spawnea. **Placement pick_spawn_coords (2×rand(0,31), re-roll por DISTANCIA >6
   en ambos ejes, 0x0F4E) CERRADO en F.2 T2** por el stream vivo; condición de
   re-roll derivada del asm, ancla offset −16 anclada-por-resumen, pasabilidad
   FUERA del bucle (re/verified/loops.md §Placement). Los world-turns EXTRA de terreno lento
   (0x448/0x468) ruedan cada uno su gate rand(1,30) tras el viento (afterWind).
2. ✅ **Viento de pueblo**: rueda ahora en pueblo (por turno consumido). El
   "por tecla incl. inválidas" → F.2.
3. ✅ **Tiles especiales exterior**: pantano→veneno, puente→trolls, underworld
   hazard, cableados en `game.ts`.
4. ✅ **Orden viento↔housekeeping en vivo** (CERRADO F.2 T1): `game.ts` delega el
   esqueleto en `outdoorTurn`/`townTurn` (viento primero) con un único `liveRng`.
   El paso BLOQUEADO exterior rueda SÓLO el viento (0xC30→0xD14, F.2 T2).
5. ⚠️/✅ **guard_wander / 2º world-turn / fases Quickness-montura**: el **2º
   world-turn del npc_engine** ([0x65BF], TOWN 0x1671) y las **fases Quickness
   ([0x2C55]) / montura ([0x2C57])** del world_turn (0x1A6D) CERRADOS en F.2 T2
   (`npcEngineSecondTurn`, `outdoorWorldTurnRuns`); el criterio del 2º world-turn
   aproxima `[0x65bf]≠0` por "hay NPCs en la planta". **guard_wander** en vivo
   sigue → F.2 (requiere la tabla de objetos-guardia de small maps).
6. ⚠️ **troll_toll Y/N**: el clon auto-paga si oro≥toll (sin prompt interactivo);
   el RNG (los rand(1,30) de DEX) es exacto; el prompt es capa de UI.

## 4. Mapa de funciones para el ledger

Ver `task-3.13-derivation-draft.md` §4 (TOWN, 32 funcs) y §5 (MAINOUT resto, 26
funcs). Sitios rand_range: TOWN 12 (todos condicionales), MAINOUT 20. Con esto
TOWN.OVL (6256 B) y MAINOUT.OVL (7344 B) quedan mapeados al 100% (3.7 cubrió las
6 funciones de transporte; 3.13 el bucle + monstruos + peligros).
