# Transporte y navegación (Task 3.7)

Re-derivación EXACTA del subsistema de transporte del jugador y de la náutica
desde el desensamblado de ULTIMA.EXE, MAINOUT.OVL (load_seg 0x81D0: CS =
fileoff + 0x81D0) y CMDS.OVL (load_seg 0xBF8: CS = fileoff + 0xBF80), más las
rutinas de viento/reparación del kernel. Direcciones = **offset dentro del
.OVL/EXE** (kernel: listing offset = code pool = fileoff − 0x800).

Port TypeScript: `game/src/core/world/wind.ts` (viento, deriva) y
`game/src/core/world/transport.ts` (Board/X-it/Fire/Yell, casco, HMS Cape,
naves NPC). Integración del wind-tick: `game/src/core/game.ts (tickTurn)`.
Paridad: `re/tools/transport_parity.py` + `re/parity/transport/*.json`.

Convención de llamadas: Borland C pascal (`rand_range(lo,hi)` = `push lo; push
hi; call 0x2092`, ambos inclusive, ver `re/notes/rng.md`).

## 0. Globals

| DS | nombre | rol |
|----|--------|-----|
| 0x587C | g_transport_tile | estado de transporte (byte): vehículo+facing+vela (§1) |
| 0x5892 | g_wind | 0=Calm,1=North,2=South,3=East,4=West |
| 0x5891 | g_wind_enabled (unk) | gate del wind-tick en el world-turn; Time-stop → 0 (§4) |
| 0x5883 | g_wind_drift_ctr | contador de cadencia de deriva; reset en set_wind y al derivar (§3) |
| 0x5955 | g_sail_dir | rumbo del barco (0=parado,1=O,2=E,3=N,4=S); el barco lo repite solo |
| 0x5956 | g_unk_5956 | flag de repintado/"parada" de navegación |
| 0x5953/0x5954 | g_ship_dock_x/y | muelle donde aparece la nave comprada (shipwright) |
| 0x6605 | g_ship_flags | 0x40/0x82/0xC0 = "hay nave que colocar" (shipwright→MAINOUT §6) |
| 0x57BB | g_hms_cape | HMS Cape: gate `> 0x7f` (bit alto) → mitad de coste naval (§5) |
| 0xA524 | g_cape_toggle | alterna 0↔1 con el Cape para dar world-turn en tramos pares (§5) |
| 0x5C5A | g_world_objects | tabla de objetos del overworld, stride 8: +0/+1 tile, +2 X, +3 Y, +4 floor, +5 casco, +7 skiffs/acumulador |
| 0x5C5F | g_hull | casco del vehículo activo (obj 0 → +5) |
| 0x5C61 | g_skiffs | esquifes a bordo de la fragata activa (obj 0 → +7) |
| 0x57B0 | g_carpets | alfombras en inventario |

## 1. Codificación de g_transport_tile (DS 0x587C)

`clase = [DS 0x54F4 + (transport_tile >> 2)]` (tabla de 64 B, DATA.OVL fileoff
0x5504; kernel_tile_passable 0x2C4C, ya derivado en Task 3.1). Rangos:

> ⚠ **TABLA CORREGIDA 2026-07-27** (carril `fix/transporte-verbo`). La versión anterior tenía
> **tres defectos**, los tres verificados contra el disasm y los bytes de `DATA.OVL`:
> **(a)** daba UNA tabla de verbos cuando hay **DOS rutinas** y difieren en el barco;
> **(b)** decía `base+turn_arg` para caballo y alfombra, y esas dos clases **sólo tienen DOS
> orientaciones de sprite** (E y O; N/S dejan el tile intacto); **(c)** ponía
> `rema ("Rowing!")` en la columna VERBO de la fragata arriada, mezclando el verbo de
> `transport_face` con un print de `ship_try_move` (0x020E) que es otra cosa.

**El verbo NO es una propiedad del vehículo: es una propiedad del (vehículo, CONTEXTO).**
Hay dos rutinas hermanas y sólo difieren en el BARCO:

| tile | vehículo | verbo OVERWORLD (`transport_face` MAINOUT **0x00DA**) | verbo PUEBLO (`town_transport_face` TOWN.OVL **0x057C**) | facing que aplica |
|---|---|---|---|---|
| 0x1C/0x1D | a pie | — (default 0x0106→0x0129) | — (default 0x05A0→0x05FC) | — |
| 0x10–0x13 | caballo | `"Ride "` DS 0x2946 (file 0x2956) @0x010A | `"Ride "` DS 0x2666 (file 0x2676) @0x05A2 | **sólo 2**: E→0x12, O→0x13; **N/S no tocan el tile** |
| 0x14–0x17 | alfombra mágica | `"Fly "` DS 0x294C (file 0x295C) @0x0130 | `"Fly "` DS 0x266C (file 0x267C) @0x05C4 | **sólo 2**: E→0x14, O→0x15 |
| 0x20–0x23 | fragata, velas IZADAS | `"Head "` DS 0x2956 (file 0x2966) @0x016A, **sólo si el facing CAMBIÓ**, y `return 1` (aborta el paso) | **SIN VERBO** — 0x0591 salta DIRECTO a 0x05ED | base+turn_arg (4) |
| 0x24–0x27 | fragata, velas ARRIADAS | **igual que 0x20**: `"Head "` (0x00FC `cmp 0x24` → je 0x16A) | **SIN VERBO** — 0x0596 → 0x05ED | base+turn_arg (4) |
| 0x28–0x2B | esquife (skiff) | `"Row "` DS 0x2951 (file 0x2961) @0x0152 | `"Row "` DS 0x2671 (file 0x2681) @0x05E6 | base+turn_arg (4) |
| 0x2C–0x2F | nave NPC de vela (pirata) — objeto del mundo, NO del jugador | — | — | §3b |

**El RUMBO lo imprime el LLAMADOR, y también difiere por contexto**: overworld
`outdoor_move` 0x0490 lo empuja en 0x0507 (DS 0x29DB `"North\n"`, file 0x29EB) **sólo si
`g_sail_dir == 0`** (gate en 0x0500); pueblo `town_move` 0x0600 lo empuja en 0x0662
(DS 0x2676 `"North\n"`, file 0x2686) **SIN gate ninguno**. Los verbos llevan **espacio final
y ningún `\n`**; los rumbos llevan `\n` ⇒ el original compone UNA línea (`"Fly " + "North\n"`).

Otra asimetría estructural: `transport_face` (overworld) **devuelve 0/1** y el 1 aborta el
paso (girar cuesta el turno); `town_transport_face` es **VOID** (`ret 2` sin valor) — en pueblo
girar **no** consume el paso.

Facing genérico (MAINOUT 0x00DA @0x015C/0x0163 · TOWN 0x05ED): `nuevo = (tile & 0xFC) + turn_arg`,
con **N→0, E→1, S→2, O→3** — **pero sólo para esquife y fragata**; caballo y alfombra usan las
asignaciones fijas de la tabla (ver columna «facing que aplica»). Códigos de dirección del kernel
getkey: **1=Oeste, 2=Este, 3=Norte, 4=Sur**. **No hay transporte-globo pilotable**: la clase 0x2C
solo aparece en el loop de objetos del mundo (§3b); el dispatch de facing del jugador (0x00DA)
NO tiene caso 0x2C.

## 2. Bucle exterior y despacho de rumbo (MAINOUT 0x0A84 / 0x0490)

`outdoor_move(dy,dx)` MAINOUT 0x0490:
- Si fragata velas izadas (0x20) y la tecla ≠ g_sail_dir → g_sail_dir = tecla;
  g_wind_drift_ctr = 0 (vira).
- switch dir 1..4 → fija (dx,dy) y llama `transport_face(turn_arg)` 0x00DA. Si
  `transport_face` devolvió ≠0 (giró o becalmado) → RETURN sin mover (girar
  consume el turno).
- si movió: 0x0354 wrap (mueve el party, coords byte) + 0x03E0 terreno lento.

**Girar un barco (transport_face 0x00DA / 0x01DC):** `nuevo = (tile&0xFC)+turn_arg`.
Si `nuevo != viejo` → imprime "Head <dir>" y **devuelve 1 (giró)**: el bucle NO
mueve. Si `nuevo == viejo`:
- `transport >= 0x24` (velas arriadas / skiff) → return 0 (rema, sin viento).
- velas izadas (0x20-0x23) y `g_wind==0` (Calm) → return 1 (**becalmada, no mueve**).
- velas izadas con viento → return 0 (mueve).

⇒ **Fragata con velas izadas solo avanza con viento; girar SIEMPRE cuesta un
turno.** El skiff y la fragata arriada reman siempre. Port: `shipFacingStep`.

`ship_try_move(dx,dy)` MAINOUT 0x01FE — colisión/atraque navegando (g_sail_dir≠0):
tile destino 3 → "BREAKING UP!"; 0x47 → "Docked!" + **transport += 4** (auto-arría
velas 0x20→0x24); otro → "COLLISION!" + daño de casco; g_sail_dir=0. Bloqueado sin
navegar (skiff/rema) → "Blocked!"; cactus 0x2F → "OUCH!" + rand(1,8) daño party.

> **A4-PARITY2 D-89 (2026-10-03):** el daño del cactus ES `party_random_damage` (K:2AA8, MAINOUT 0x0336): un `rand(1,8)` POR MIEMBRO vivo (sin tirada para un 'D'), no al personaje activo; los puertos (referencia y nativo) lo tiraban una sola vez al activo y se corrigieron. El naval no tiene rutina propia: la cola 0x0312-0x0347 es común a pie y vehículo (`re/notes/cactus-ouch-acta.md`).

## 3. Deriva del barco por viento (MAINOUT 0x0598/0x0619) — SIN RNG

`tick_and_getkey()` MAINOUT 0x0598 corre 1×/iteración del bucle exterior. Con
g_sail_dir==0 es getkey. Navegando aplica la deriva:

- Rumbo (g_sail_dir): O→(−1,0), E→(+1,0), N→(0,−1), S→(0,+1).
- Empuje del viento: tablas `wind_dx@0x29F5 = [00,00,00,FF,01]`,
  `wind_dy@0x29F9 = [01,01,FF,00,00]` (idx = g_wind). El viento empuja en la
  dirección **opuesta a su nombre**: N→(0,+1), S→(0,−1), E→(−1,0), O→(+1,0).
  Calm (idx 0) salta la deriva (0x05ED).

Algoritmo (0x0619-0x0655):
```
di = 1 + (wind_dx[wind] != rumbo_dx) + (wind_dy[wind] != rumbo_dy)
umbral = di % 3          ; a favor di=1→1; perpendicular di=2→2; en contra di=3→0
si umbral > g_wind_drift_ctr → NO deriva, ctr++ (0x06A7)
si no → ctr=0 y DERIVA (avanza un tile en el rumbo; el bucle lo trata como flecha)
```
Cadencia: a favor del viento avanza ~cada 2 ticks (umbral 1); perpendicular menos
(umbral 2); en contra pura avanza siempre (umbral 0). Port: `windDriftStep`.
**La deriva NO consume RNG** — solo el wind-tick (§4).

**Orden world_turn ↔ desplazamiento (Concern C de T2, RESUELTO).** El `world_turn`
(MAINOUT 0x1A60) se llama en `tick_and_getkey` @0x069D (rama WAIT 0x0670:
advance_clock + world_turn + ctr++). La rama de DERIVA (0x0651 ctr=0; 0x0655 jmp
0x6CF) **salta** 0x0670 y devuelve el rumbo como "tecla" SIN world_turn/advance_clock;
el desplazamiento lo aplica `outdoor_move` (0x0490) @0x0BB3 en el bucle exterior,
DESPUÉS de que `tick_and_getkey` retorne. ⇒ **NAVEGANDO (deriva), el world_turn corre
ANTES de que el barco se desplace**: los monstruos reaccionan a la casilla PRE-deriva.
El clon (`game.ts runNavalTurn`) **se alineó SOLO para la deriva**: corre
`outdoorWorldTurn` antes de aplicar el paso de deriva (`resolveNavalStep`).

⚠ **REMAR es distinto**: con `g_sail_dir==0` (skiff / fragata arriada) `tick_and_getkey`
es getkey PURO (no entra en la rama WAIT 0x0670); el paso va por `outdoor_move` como a
pie: `move_party` (0x0354) y LUEGO el world_turn (0x03E0 @0x0539) — **desplazamiento
PRIMERO, world_turn después**. El clon lo conserva así (rema aplica `resolveNavalStep`
ANTES de `runNavalTurn`, igual que el movimiento a pie verificado en F.2). NO se
reordena remar.

Divergencia residual medible SOLO en la deriva (no silenciada): el clon COLAPSA en 1
acción/tecla lo que el binario reparte entre ticks-WAIT (advance_clock+world_turn, sin
desplazar) y ticks-DERIVA (desplazan vía outdoor_move); la CADENCIA di%3 es idéntica,
solo cambia el reparto fino de minutos/fase. BP: 0x069D (world_turn) vs 0x0BB3 (paso).

### 3b. Naves NPC de vela (clase 0x2C) — MAINOUT 0x198C — ⚠️ la DECISIÓN es determinista, MOVERSE CUESTA 1 TIRADA (corregido por #218)

`object_wind_drift(objIdx)` se llama por cada objeto desde el world-turn 0x1A60.
Para objetos 0x2C-0x2F el viento decide si el objeto avanza:
```
si g_wind==0 (Calm) → NO mover
facing = obj_tile − 0x2C (0..3)
threshold = word[facing*8 + g_wind*2 + 0x2BF6]   ; 4 = nunca frena
acc = ++[obj+0x5C61]; si acc <= threshold → mover; si acc > threshold → acc=0, NO mover
```
Tabla (facing 0..3 × viento N/S/E/O): f0=2/3/4/4, f1=4/4/2/3, f2=3/2/4/4,
f3=4/4/3/2. Port: `npcShipMoves`. **NO es un transporte del jugador** (sin caso
0x2C en el facing dispatch, sin Board a 0x2C).

> ★ **CORRECCIÓN #218 — el «0 RNG» de esta sección contaba sólo el CUERPO.** Lo
> determinista es **la decisión de si el objeto avanza** (la cadencia por acumulador
> de arriba: eso sigue siendo exacto y no tira). Lo que no es cierto es que la
> sección entera sea sin RNG: **«mover» no es una escritura de coordenadas aquí, es
> una LLAMADA**, y el callee tira. Derivación en `re/notes/rand-218-acta.md` §3.2.
>
> El pseudocódigo dice `si acc <= threshold → mover`. Ese «mover» se compila así:
>
> | offset | instrucción | qué |
> |---|---|---|
> | 0x1a37 | `je 0x1a4e` | threshold==4 («nunca frena») → moverse |
> | 0x1a46 | `jbe 0x1a4e` | acc <= threshold → moverse |
> | 0x1a4e | `push [bp+4]` / `mov ax,1` / `push ax` | argumentos del mover de actores |
> | 0x1a55 | `call 0x17d4` | ★ **el mover**, intra-overlay |
>
> Y el mover de actores, MAINOUT 0x17d4, **gasta una `rand_range(0,1)` SIEMPRE**:
>
> | offset | instrucción | qué |
> |---|---|---|
> | 0x18ae | `sub ax,ax` / `push ax` / `mov ax,1` / `push ax` | argumentos 0 y 1 |
> | 0x18b5 | `call` → kernel 0x2092 | `rand_range(0,1)`: cara o cruz del EJE a intentar primero |
> | 0x18b8 | `cmp ax,1` / `jne 0x1930` | 1 → intenta X antes que Y; 0 → al revés |
>
> **Es incondicional**: los cuatro caminos que salen de los gates de signo
> (0x1833, 0x186f, 0x1891 y la caída de 0x18a3/0x18ee) confluyen en 0x18ae antes de
> cualquier otra cosa. No hay rama que se salte esa tirada.
>
> ⇒ **cada nave NPC de vela que se mueve consume 1 `rand_range(0,1)` por turno de
> mundo**, y además 0x17d4 termina llamando a MAINOUT 0x1578 — alcanzado en 0x192a y vía 0x197d —,
> que es la **compuerta de terreno de los actores de #104**: 1 tirada más en 13 de
> los 28 tiles. La cuenta correcta de esta sección no es 0, es **1 + (0 ó 1)** por
> nave que se mueve. Enlaza con #128 (esa compuerta no está portada).

**Casco inicial (obj+5).** Al spawnear la nave pirata (0x2C) sobre agua, el world_turn
escribe el casco con una **constante**: `mov byte [bx+0x5C5F],0x64` (spawn 0xFC4
@0x1050, tras `cmp [bp-2],0x2C`). ⇒ **casco = 0x64 = 100**, UNO MÁS que el `HULL_MAX=99`
del barco del jugador (una nave pirata capturada excede el tope del jugador). Port:
`PIRATE_SHIP_HULL=0x64`, sembrado en el pick del spawn (`game.ts` overworld picker →
`OverworldEnemy.hull`); lo consume Fire (broadside). Ya NO es undefined→99 (fallback).

## 4. Cambio periódico de viento — kernel 0x2F62 [RNG/paridad]

`maybe_change_wind()` se llama 1×/world-turn desde el housekeeping 0x5910
(`5944: call 0x2f62`), gate `5933: cmp [0x5891],0; je skip` (deshabilitado si 0;
Time-stop 'T' lo fuerza a 0 en 0x591D-0x5924).
```
2f70: r = rand(0,63)              ; SIEMPRE 1 tirada/tick
      si r != 0 → RETURN           ; 63/64 no cambia
2f89: w = rand(0,4)               ; solo si r==0 (1/64)
      si w != 0 → set_wind(w)
2f7a: si w == 0 (propone Calm): k = rand(0,255)
        si k >= 0xC0 (192) → set_wind(0)    ; 64/256 = 1/4 queda Calm
        si k <  0xC0       → re-tira rand(0,4) (sesgo anti-calma)
```
⇒ **Cada world-turn consume EXACTAMENTE 1 rand(0,63)** aunque no cambie —
imprescindible para la paridad del stream. En el hit 1/64, +1 rand(0,4) y, si
propone Calm, +1 rand(0,255) y re-tiradas hasta k≥192. Port: `maybeChangeWind`.

`set_wind(w)` kernel 0x2E96: escribe g_wind (0x5892) y **g_wind_drift_ctr=0**
(0x2EA5). Único escritor de g_wind en toda la imagen (lo llaman
maybe_change_wind y Rel Hur). No redibuja en mazmorra/underworld. Port: `setWind`.

### 4.1. Re-derivación del mislabel `0x2F62 = sprite_frame_randomizer` (Task #20)

El barrido de RNG en render (`oracle-flash-rng.md`, kernel-render-sweep) etiquetó
`0x2F62` como **`sprite_frame_randomizer`** (VOID, rand(0,63), "sólo cosmético,
dibuja un sprite") y por eso `oracle-flash-rng.md §4 ⚠` marcó como sospechoso que
`test_wind_value_live` pasara forzando el seed en `0x2F62`. **El disasm demuestra
que ese label es ERRÓNEO y que el de `test_transport_parity` (`WIND_TICK=0x2F62 =
maybe_change_wind`) es el CORRECTO:**

- `0x2F62` rola rand(0,63) (0x2F70) y, en el hit 1/64, `rand(0,4)`/`rand(0,255)≥0xC0`
  (bucle 0x2F7A-0x2F97) → deja el valor 0..4 en `si` y llama `0x2E96(si)`.
- **`0x2E96` NO dibuja un sprite: en `0x2EA2` hace `mov byte [g_wind(0x5892)], al`**
  (escribe el viento) + `[0x5883]=0` (reset drift ctr) y **luego redibuja el
  INDICADOR de rumbo** (strings 0x555C/0x5562/0x5568/0x556E/0x5574 = Calm/N/S/E/O
  vía `call 0x1850`). Es `set_wind`, el único escritor de g_wind (§4).

⇒ `0x2F62` **SÍ cambia el viento**. La "aparición de un sprite/frame" que vio el
barrido es en realidad el redibujo del indicador de viento. La estructura
(rand(0,63) + rejection-sampling) es idéntica a como la modela
`transport_parity.wind_tick`.

**Por qué `test_wind_value_live` es VÁLIDO (no coincidencia):** arma el BP en la
ENTRADA de `0x2F62` (antes de su rand), fuerza `g_rng_seed` a una semilla cuyo
`wind_tick` predice un viento concreto (`_find_forced_seed` garantiza rand(0,63)==0
⇒ SIEMPRE cambia), reanuda; `0x2F62` corre con esa semilla, computa el viento y
**escribe g_wind vía 0x2E96**; en el hit siguiente se lee g_wind = el predicho. El
test funciona **precisamente porque `0x2F62` ES el consumidor real del viento** y su
escritura de g_wind es directa: si fuese un randomizador de sprite que no toca
g_wind, forzar el seed ahí NO produciría el viento predicho y el test fallaría.
Pasa ⇒ confirma el mecanismo. **No hay consumidor de viento oculto: el rand del
viento y su escritor son `0x2F62 → 0x2E96`.**

**Único caller de `0x2F62` = `0x5910` (0x5944), el tick de animación/housekeeping**
(§4). Corre en tiempo real (idle) además de por turno, así que el veredicto de
INDETERMINISMO de `oracle-flash-rng.md` (el seed global avanza por reloj de pared
en cualquier pantalla animada) **se sostiene igual**: cambie el label a
maybe_change_wind, sigue siendo un consumidor incondicional del seed en el timer de
render. Sólo se corrige la SEMÁNTICA (viento, no sprite), no la conclusión.

**Alcance del gate — el viento también rueda en PUEBLOS** (✅ CERRADO en Task 3.13):
el housekeeping 0x5910 (que llama a maybe_change_wind) se invoca desde TOWN.OVL en 7
sitios, y TOWN:0x121D pone `[0x58A4]=1`. Además `[0x5891]` se RE-ARMA dentro de
0x5910 (0x5A1D), así que no es "1×/turno" sino **1×/llamada a 0x5910**. El clon YA
NO gatea `location===0`: `game.ts tickTurn` rueda el viento también en pueblo (no en
mazmorra), por turno consumido — cerrada la divergencia. El matiz "1×/llamada a 0x5910
POR TECLA (incl. inválidas) + 2º world_turn del npc_engine" queda para F.2 (requiere
el hook a nivel getkey); ver re/notes/loops.md §2 y re/verified/loops.md.

**Rel Hur** (CAST2 0x040A, ya portado en Task 3.3 `magic/cast.ts`): remapea la
flecha pulsada → viento con el NOMBRE de esa dirección (ARROW_TO_WIND
`[0,4,3,1,2]`: N→1,S→2,E→3,O→4) y desemboca en set_wind (resetea el drift ctr).

## 5. HMS Cape — coste de tiempo naval (MAINOUT 0x0670)

```
0670: si g_hms_cape[0x57BB] > 0x7F:            ; bit alto = planos conseguidos
0677:   toggle [0xA524] entre 0 y 1; ax = 1    ; advance_clock(1)
0688: si no: ax = 2                            ; advance_clock(2)
068b: advance_clock(ax)
0694: cmp word[0xA524],0; je 0x69d → call 0x1A60   ; world-turn si el toggle POST-flip == 0
```
⇒ **Con Cape: 1 min/tramo y world-turn (monstruos) solo cuando el toggle
POST-flip == 0** — la mitad del tiempo y de los encuentros. **FASE**: con el
toggle inicial 0, el flip lo pone a 1 → el PRIMER tramo naval SALTA el world-turn
y lo corre en el segundo (0694 `je` = corre si ==0). Sin Cape: 2 min + world-turn
cada tramo. Port: `navalStepCost` devuelve `worldTurn: toggle===0`.
Escritores de g_hms_cape: CAST.OVL 0x1A86 `|=0x80`, SJOG.OVL 0x15D4 `=0xFF` (el
ledger "0/1" es impreciso: la comparación es `>0x7F`). Port: `navalStepCost`.

## 6. Colocación de la nave comprada (MAINOUT 0x0D22, gate g_ship_flags 0x6605)

Tras comprar en el shipwright ([0x6605]=0x82 fragata / <0x80 skiff): aloca un
objeto en el muelle (g_ship_dock_x/y), `obj+5 = 99` (**casco máximo**, 0x0D7B),
**`obj+7 = [0x6605] & 0x3F`** (0x0D45: `mov al,[0x6605]; and al,0x3f; mov [bx+7],al`
→ **fragata 0x82 nace con 2 skiffs a bordo**), tile = `(0x6605>0x7F) ? 0x25 : 0x29`
(fragata velas arriadas S / skiff S), [0x6605]=0. Port: `spawnPurchasedShip(flags)`
fija hull=99 y skiffs=flags&0x3f (antes dejaba 0 → falso "NO SKIFFS ON BOARD!").

## 7. Board / X-it / Fire / Yell — CMDS.OVL

Casco y skiffs viven en el registro del objeto (tabla 0x5C5A, stride 8): `hull =
[obj<<3+0x5C5F]`, `skiffs = [obj<<3+0x5C61]`. Vehículo activo (obj 0) →
g_hull/g_skiffs. Ninguno consume RNG salvo el broadside.

### 7A. Board (B) CMDS 0x07F6
- Mazmorra (0x20<loc<0x29) → "Not here!".
- **Abordar caballo/alfombra/skiff EXIGE ir a pie** (helper 0x6EE: 1 sii
  transport∈{0x1c,0x1d}, si no "On foot"):
  - Caballo (0x10/0x11): con dueño en pueblo → "Nay!" (0x0856, **ANTES** del gate a
    pie 0x0862); si no, gate a pie → "horse", transport=tile+2.
  - Alfombra (0x1b): gate a pie → "carpet", transport=0x14.
  - Skiff (&0xFC==0x28): gate a pie → "skiff", transport=tile+2.
- Fragata (&0xFC==0x24): gate 0x70C acepta **solo desde alfombra 0x14/0x15**
  (no 0x16/0x17), a pie 0x1c/0x1d o skiff 0x28-0x2b (si no "On foot"). "Ship";
  **DANGER (hull<10, 0x08E5) y WARNING (skiffs==0, 0x0920) son ramas
  INDEPENDIENTES — pueden salir AMBAS**. Estiba entre las dos: fromTile&0xFE==0x14
  → g_carpets++ (0x090C); fromTile&0xFC==0x28 → skiffs++ (0x0919). transport=tile
  (0x24-0x27). Port: `board` (devuelve `warnings: string[]`).

### 7B. X-it (X) CMDS 0x0EB4 — switch(transport&0xFC)
- Caballo 0x10: "horse!"; suelta tile-2; transport=0x1c.
- Alfombra 0x14: requiere tierra (pred 0x73E); "carpet!"; suelta 0x1b; si no "No land nearby!".
- Fragata velas izadas 0x20: "Under sail!" (arriar antes).
- Fragata arriada 0x24: prioridad (1) tierra → a pie; (2) skiffs>0 → bota skiff
  (transport+=4, skiffs−−); (3) carpets>0 → carpets−−, transport=0x14; (4) "No skiffs
  on board!".
- Skiff 0x28: **EXIGE tierra** (0x0F72: 0x73E==0 → "No land nearby!" 0x43AC aborta)
  y LUEGO rechaza el agua no desembarcable (0x0F85: tile bajo &0xFE==0x6A → "Not
  here!"); si pasa → "skiff!", transport=0x1c.
Port: `exitTransport`. **El predicado 0x73E tiene la MISMA polaridad en los 3
callers** (≠0 = hay tierra): alfombra 0x0F20 `jne`→desembarca, skiff 0x0F72
`jne`→procede, fragata 0x0FA3 `je`→(sin tierra) bota skiff. (Corrige el borrador,
que suponía polaridad opuesta.)

### 7C. Fire (F) broadside CMDS 0x0962
Requiere fragata (0x20-0x27). **Solo perpendicular a la quilla** (bit0 de transport
= eje N/S vs E/O); paralelo → "Fire broadsides only!". Alcance 3; primer objeto:
**dmg = rand(1,20)** (0x0A74, ÚNICA tirada de daño); casco −= dmg; underflow (>0x7F)
→ hundido. ⚠ **RNG de integración**: al impactar, el broadside corre ANTES un
world-turn (0x0A5E `call 0x5910`), que consume su rand(0,63) del tick de viento
ANTES del rand(1,20). La primitiva `broadside` modela solo el rand(1,20); el
cableado (game.ts Fire) debe correr el world-turn primero para el stream correcto.
Cañón de pueblo (0x0B16, sin RNG): alcance 5, karma −5 al matar. Port: `broadside`.

### 7D. Yell (Y) = Hoist/Furl CMDS 0x1418
Fragata (0x20-0x27) fuera del underworld (loc<0x80): velas izadas 0x20 → "FURL!"
(+4 → arriadas); arriadas 0x24 → "HOIST!" (−4 → izadas). Si no → "what?". Ciclo:
Board→arriadas (rema); HOIST→izadas (navega por viento); FURL→arriadas; atracar en
0x47 → auto-FURL. Port: `yell`.

### 7E. Hundimiento del JUGADOR — damage_ship 0x109E/0x10D6-0x1166 (MAINOUT)

`ship_try_move` (0x01FE) llama a `damage_ship` (0x109E) en la rama COLLISION/BREAKING
UP **navegando** (0x0303). Secuencia (derivada del disasm MAINOUT 0x1689-0x1773 y
:283-323; strings verificados contra DATA.OVL):

- **GATE + DAÑO (0x109E)**: solo fragata (`transport&0xF8==0x20`); `dmg = rand(1,30)`
  INCLUSIVE (1 tirada SIEMPRE); hunde sii `dmg >= hull` (`jae`, unsigned). Si
  sobrevive, `g_hull -= dmg`. **El casco NO se clampa al hundir** (`sub` incondicional;
  queda basura hasta el próximo Board porque se reemplaza el tile del vehículo).
- **HUNDIMIENTO (0x10D6)**: imprime **"Ship sunk!"** (DS 0x6ada) SIEMPRE, luego por
  PRIORIDAD:
  - `skiffs>0` → tile `0x28+(old&3)` (**skiff, FACING PRESERVADO, skiffs NO
    decrementa**, 0x10F5) + **"Abandon ship!"** (DS 0x6ae6).
  - `carpets>0` → `carpets−−`; tile `0x14+rand(0,1)` (alfombra, facing N/E ALEATORIO
    ← **+1 tirada del stream**, 0x1108) + **"Abandon ship!"**.
  - si no → **AHOGO (0x1120)**: `tile=0` (a pie EN el agua), **"DROWNING!!!"** (DS
    0x6af6), bucle de animación opaco. **"Abandon ship!" NUNCA en ahogo.**
  Prioridad **skiff > alfombra**. `g_unk_a9fa=1` (redraw) en las conversiones.
- **AHOGO**: NO party-wipe, NO daño de HP, NO teleport EN ESTA FUNCIÓN (los helpers
  del bucle son opacos → preguntas de oráculo, §10 / deliberate-divergences §3).
- **`g_sail_dir=0` lo pone el LLAMADOR** (0x0306), no `damage_ship`.
- **BREAKING-UP vs COLLISION**: misma secuencia; solo cambia el string previo
  ("BREAKING UP!" tile 3 / "COLLISION!" otro bloqueante) + sonido crash; ambos caen al
  MISMO call 0x109E.

Strings DS: 0x6ada "Ship sunk!\n" · 0x6ae6 "Abandon ship!\n" · 0x6af6 "DROWNING!!!\n" ·
0x298b "BREAKING UP!\n" · 0x2999 "COLLISION!\n".
Globals: g_transport_tile 0x587C · g_hull 0x5C5F (obj0+5) · g_skiffs 0x5C61 (obj0+7) ·
g_carpets 0x57B0 · flag redraw 0xA9FA.
RNG total: 1×rand(1,30) SIEMPRE; +1×rand(0,1) SOLO en conversión a alfombra.

Port: `sinkPlayerShip(transportTile, skiffs, carpets, rand)` (pura, aditiva) devuelve
`{messages, transportTile, mode, carpets, drowned}`; cableada en `game.ts`
`resolveNavalStep` cuando `shipTryMove().sunk`. El casco **NO se toca al hundir** (el
`sub g_hull` sólo corre en la rama de SUPERVIVENCIA; al hundir se reemplaza el tile y
`g_hull` queda intacto): `game.ts` deja `shipHull` sin modificar (fiel). **Divergencia
del clon** (única, aplazada): el ahogo se modela solo como cambio de tile+mensaje; el
party-wipe/HP/teleport vive en helpers opacos → oráculo §10 / deliberate-divergences §3.

## 8. Reparación de casco en Camp — kernel 0x3C9A (path transport&0xFC==0x24)

```
3cf8: advance_clock(5) ×5 → 25 min de juego
3d13: rand(1,3) → al ; [obj+5] += al ; cap 0x63 (99) ; repite mientras hull < 0x0A (10)
```
**RNG: exactamente 1 rand(1,3) si el casco está sano (≥10); más tiradas solo
mientras hull<10.** Port: `repairHull`. (El handler 0x3C9A completo es el
Camp/hole-up, alcance Task 3.9; aquí solo el path de reparación de barco.)

## 9. Divergencias con el clon (portadas)

1. **Viento**: el clon no modelaba g_wind/deriva/cambio. Añadido `wind.ts` +
   wind-tick en el world-turn del overworld (game.ts `tickTurn`, gate loc==0 y
   ≠Time-stop): 1 rand(0,63)/turno.
2. **Estado de transporte**: `transportTile` (byte con facing y vela) además del
   `TransportMode` grueso; `sailDir`, `windDriftCtr`, `shipHull`, `shipSkiffs`,
   `hmsCapeToggle` nuevos en `state.ts`.
3. **Yell = Hoist/Furl**, prioridades de X-it, Board con estiba de skiff/alfombra,
   broadside perpendicular (1 rand(1,20)), reparación por camp, coste naval con
   HMS Cape, naves NPC moduladas por viento — todo en `transport.ts`.
4. **Rel Hur** ahora resetea el drift ctr (set_wind es el único escritor de g_wind).

## 10. Preguntas abiertas (a oráculo)

- **Deriva 0x0598**: semántica exacta del avance (rumbo vs empuje) y doble conteo
  de minutos; el formulado sigue las tablas + `di=1+mismatches; ctr>=di%3`.
- **[0x5891]**: re-armado en 0x5910:0x5A1D; corre también en pueblos (§4). El clon
  lo gatea a overworld hasta Task 3.13.
- **Objetos 0x2C**: identidad (naves piratas a la deriva es lo más probable).
- **g_hms_cape**: gate `>0x7F` confirmado por asm; el ledger "0/1" es impreciso.
- **Ahogo del jugador (damage_ship 0x1120)** — 3 helpers opacos del bucle de animación
  del ahogo, con BP en el target del call (Clase C, ver deliberate-divergences §3):
  1. `0xffffc1de` (4 args): animación del ahogo — ¿consume RNG?
  2. bucle `0xffffb352 / 0xffffa8d8 / 0xffffb82c → 0xFFFF`: ¿toca HP/party/RNG
     (party-wipe)?
  3. `0xffffd740`: ¿refresh de pantalla sin RNG?

(RESUELTO en review: el predicado 0x73E tiene la MISMA polaridad en sus callers
—**CUATRO**, no 3: `CMDS:0x0f20 · 0x0f4c · 0x0f72 · 0x0fa3`, los cuatro INTRA-overlay,
medido con `re/tools/callers_por_banda.py` en `99c07474`; conteo corto, no fallo de banda —
—§7B—; el valor del viento se verificó byte-a-byte en runtime —re/verified—.)

---

## CORRECCIÓN §7E/§2 (#169, árbol de `7c4aa56c`) — `ship_try_move` NO es «la rutina del barco»

El fix de #157 (`037587d6`, `re/notes/cactus-ouch-acta.md`) aterrizó, pero **la etiqueta
que causó el defecto sigue viva en esta nota y en el ledger**: MAINOUT 0x01FE aparece aquí
encabezada como «colisión/atraque **navegando**» y en `re/ledger/frontier-manual.json`
lleva el nombre `ship_try_move`. Las dos cosas describen una de sus ramas como si fuera
su dominio.

**Es UNA sola rutina para todos los modos.** Su discriminador a-pie/vehículo es
`cmp [g_transport_tile],0x20 / jb` en 0x0312, y la cola de BLOQUEO 0x0312-0x0347 **la
comparten pie y barco**: 0x0322 imprime «Blocked!», 0x0329 pregunta si el tile destino es
cactus, y de ahí salen dos ramas EXCLUYENTES del mismo `if`: en CS 0x0336, «OUCH!» más
daño a la party; en CS 0x033c, el beep del choque contra pared de siempre.

Ése es justo el mecanismo que escondió la mecánica durante meses: el port modeló 0x01FE
como `shipTryMove` —sólo naval— **citando este mismo bloque**, así que la lógica del
cactus estaba portada pero archivada bajo «barco» y la vía a pie no la alcanzaba. No era
mecánica ausente: era mecánica MAL ENRUTADA.

**El renombre del ledger NO se hace aquí, y con motivo declarado**: `ship_try_move` tiene
hoy ocurrencias vivas en **20 ficheros** (medido con `git grep -c` en el árbol del ancla),
incluidos `game/src/core/world/transport.ts`, `game/src/core/game.ts`, cuatro ficheros de
tests y `re/notes/routine-census.json`. Un renombre a medias dejaría dos nombres vivos —
que es exactamente la enfermedad que el anexo de #169 está curando para CS 0x4988. Queda
como tarjeta propia. Hasta entonces: **el nombre es una etiqueta heredada de ALCANCE, no
una descripción del dominio de la rutina.**
