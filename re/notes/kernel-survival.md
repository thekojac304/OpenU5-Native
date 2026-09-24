# Kernel: reloj, turnos, hambre, luz y movimiento base (Task 3.1)

Re-derivación EXACTA desde el desensamblado de ULTIMA.EXE (offsets sobre los
34544 B de imagen post-cabecera, fileoff = pool + 0x800) y de los overlays
de nivel 1 (TOWN/MAINOUT, load_seg 0x81D: CS = fileoff + 0x81D0) y de
OUTSUBS.OVL (load_seg 0xA29: CS = fileoff + 0xA290). Verificación en vivo:
`re/parity/kernel/*.json` + `re/tools/test_parity.py` (oráculo DOSBox).

Convención de llamadas: Borland C pascal — el PRIMER push es el argumento
de la izquierda ([bp+mayor]); `rand_range(lo,hi)` = `push lo; push hi;
call 0x2092` (ambos inclusive, ver re/notes/rng.md).

## 1. `kernel_advance_clock(n)` — kernel 0x4F7C (arg: MINUTOS)

El reloj NO avanza "1 minuto por turno": avanza **n minutos por llamada**,
y n depende del contexto y del terreno (§5). Port: `game/src/core/world/survival.ts
(advanceClock)`.

```
4f84: cmp [bp+4], 0 → n==0: salta directo al recálculo de luz (§4)
4f8d: cmp byte [g_time_spell 0x587A], 'Q'   ; Quickness activo
4f94:   → n = n>>1 (sar); si queda 0 → n=1
4fa0: [0x5880 g_prev_hour] = g_hour          ; snapshot pre-avance
4fa6: cmp byte [0x587A], 'T'                 ; Time-stop: NO avanza nada
4fb0: g_minute += n
4fb4: counter_sub_u8(&[0x58A7 g_torch_mins], n)   ; antorcha: -n MINUTOS
4fbe: counter_sub_u8(&[0x58A6 g_light_spell_mins], n)
4fc8: si g_minute > 59: g_minute -= 60
4fd7:   counter_sub_u8(&[0x588C], 1)         ; countdown horario (u8, sin identificar)
4fe2:   g_hour++
4fe6:   si g_hour > 23: g_hour = 0
4ff5:     por cada Shadowlord i∈0..2 ([0x58C8+i]):        ; medianoche
5004:       si [0x58C8+i] < 0x80: nueva loc = rand_range(1,8),
              != g_location y != LOS TRES slots — incluido el PROPIO i,
              que aún lleva su valor viejo (0x5020 recorre si=0..2 y la
              escritura es posterior, 0x5044) ⇒ nunca se queda donde estaba.
              Reintento SIN COTA: 0x5037 or di,di / 0x5039 je 0x5004.
5051:   g_day++
5055:   si g_day > 28 (0x1C):                 ; nuevo mes
505c:     [0x585A]=[0x5859]=[0x5858]=0, [0x57B2]=0, [0x5959]=0
506a:     g_day = 1
5072:     por cada registro del roster (16, stride 0x20, campo +0x17
507a:       monthsAtInn en 0x55BF+i*0x20): si < 25 → ++   ; envejece posadas
508a:     g_month++
508e:     si g_month > 13 (0xD): g_month = 1; g_year++    ; 13 meses × 28 días
509e:   call 0x2900 (kernel_status_redraw)
```

- Rollovers en cascada EXACTOS: 60 min → hora, 24 h → día, 28 días → mes,
  13 meses → año. (El clon `advanceMinutes` ya era idéntico.)
- La antorcha y el hechizo de luz se consumen en **minutos de juego**, no en
  turnos: en pueblo (1 min/turno) una antorcha dura 240 turnos, en exterior
  (2 min/turno) 120.
- 'Q'/'T' en `g_time_spell` (DS 0x587A, mostrado como carácter en la línea
  de estado por 0x2900:0x29FF): Quickness reduce el coste a n>>1 —
  `sar` = **floor(n/2)** — con mínimo 1 (n=3 → 1, no 2); Time-stop
  congela el reloj. Su duración vive en 0x588E (§2).

### 1.1 ★ #102 — LAS SALIDAS TEMPRANAS: el binario tiene UNA, el clon TRES

El clon corta con `return` en tres sitios (`n==0`, `'T'`, `n<0`). El binario **no retorna en
ninguno**: sólo el cero se desvía, y a la COLA, no al `ret`. Adjudicado uno a uno.

| caso | binario | clon | veredicto |
|---|---|---|---|
| `n == 0` | `4f84 cmp / 4f88 jne / 4f8a jmp 0x50a1` → **la cola** | salta a la cola y sale | **FIEL** |
| `'T'` (0x54) | `4fa6 cmp / 4fab je 0x4fc8` → sigue por el rollover y cae en la cola | `return` | **DIVERGE, pero INERTE** ↓ |
| `n < 0` | **no se desvía**: `jne` sólo mira el cero ⇒ vía normal | `return` | **SIN RESOLVER** ↓ |

**`'T'` — divergencia inerte, y se prueba.** El binario salta a `0x4fc8`, evita la suma de
minutos (`0x4fb0`) y **las dos** restas saturantes (`0x4fbb`, `0x4fc5`), pero **alcanza la
cola** y con ella el refresco del latch lunar de `0x514a`. El clon no. No se observa jamás
porque la primera guarda de la cola es `0x514a mov al,[g_prev_hour] / 0x514d cmp [g_hour],al
/ 0x5151 je` — «sólo si CAMBIÓ la hora» —, y el snapshot `g_prev_hour = g_hour` de
`0x4fa0-0x4fa3` ocurre **ANTES** del test de `'T'`. Con `'T'` la hora ya no se toca, así que
el flanco es **falso por construcción** y el latch no puede dispararse. ⇒ El atajo del clon
es observacionalmente equivalente. Test: `survival.test.ts` «#102: con 'T' el latch lunar no
puede dispararse».

**`n < 0` — el comportamiento del binario está MEDIDO; la alcanzabilidad, NO.** Lo que hace:
`sar` sobre negativo nunca llega a 0 (así que el `inc` de mínimo-1 no entra); `0x4fad mov
al,[bp+4]` toma **el byte bajo** y `0x4fb0 add [g_minute], al` lo suma en 8 bits; y las dos
llamadas a `0x3f36` comparan con **`jbe`, o sea SIN SIGNO**, de modo que un negativo (enorme
como unsigned) cae por la rama de saturación y **pone antorcha y hechizo de luz a 0**.
🔴 **Lo que NO se ha medido es si alguien puede pasar un negativo**, y por tanto el `return`
del clon sigue siendo un corte DECLARADO, no un corte demostrado seguro.
**Límite de enumeración, explícito:** `grep "call 0x4f7c"` encuentra **2** llamadores, los
dos en ULTIMA.EXE y los dos con constante — `0x3cf8 mov ax,5` y `0x6353 sub ax,ax` (¡un
**0** literal, que acredita que la rama del cero es viva y deliberada en el original!). Los
llamadores en overlays **no salen con ese patrón**, porque usan el crudo de SU ranura: el
censo está INCOMPLETO y decirlo es parte del resultado.

### Recalculo de luz al final (SIEMPRE, incluso con n=0) → §4

## 2. `kernel_turn_housekeeping()` — kernel 0x2AE8 (fin de turno)

Llamada UNA vez por turno consumido desde los bucles de contexto
(TOWN:0x10D0, MAINOUT:0xCD3, DUNGEON:0xE22) y desde CMDS:0x671.
Port: `survival.ts (turnHousekeeping)`.

```
2b0b: por cada miembro i < g_party_size (status en 0x55B3+i*0x20 = registro+0x0B):
        'D' (muerto) o 'S' (dormido) → no cuenta, sin efectos
        'P' (envenenado) → kernel_apply_damage(i, 1)      ; 1 HP por turno
        'G'/'C'/'P' → cuenta++  ([bp-2] = comensales vivos)
2b5d: si g_hour != [0x5880 g_prev_hour]:                  ; cambió la hora
2b66:   si g_food == 0: print "Starving!\n" (DS 0x54C8)
2b74:     + kernel_party_random_damage (0x2AA8):
            por cada miembro i<party_size no-'D': apply_damage(i, rand_range(1,8))
2b7a:   si no, si g_hour ∈ {6, 12, 18} (0x06/0x0C/0x12):  ; comidas
2b8f:     counter_sub_i16(&g_food, comensales)            ; clamp a 0
2b99:   [0x5880] = g_hour
2b9f: counter_add_u8(&[0x588B g_turn_count], 1, 0xFF)     ; ¡u8 SATURANTE!
2bae: si 0 < [0x588E g_time_spell_turns] < 0xFF: --;
2bc2:   si llega a 0 → [0x587A g_time_spell] = 0 + status_redraw
2bca: call 0x400C: anillo de regeneración                  ; §2.1
```

- El hambre golpea en CADA cambio de hora mientras `food == 0` (no solo a
  las horas de comida): daño rand(1,8) por miembro vivo.
- Hallazgo del run de paridad: el binario consume RNG en cantidades no
  modeladas ANTES de los rolls de hambre (idle/animaciones — sembrar
  g_rng_seed y comparar la primera órbita diverge), así que la paridad
  byte a byte se ancla capturando g_rng_seed EN VIVO en el turno
  (`capture_seed` en parity.py) y sembrando el clon con esa semilla.
  Como el canal pty puede perder pausas (re/notes/oracle.md), la captura
  arma DOS breakpoints: la entrada de kernel_party_random_damage (semilla
  del roll 0) y la de rand_range 0x2092 (una oportunidad por roll,
  filtrada por la dirección de retorno en pila ∈ [0x2AA8,0x2AE8)); si se
  observa el roll k, se deshacen k transiciones (la transición del RNG es
  biyectiva: rng_unstep).
- La comida se descuenta a las 6:00, 12:00 y 18:00 — `comensales` = miembros
  con status distinto de 'D' y 'S' (los envenenados comen; muertos y
  dormidos no).
- **Hallazgo**: 0x588B (roster+0x2E5) es un contador de turnos de **8 bits
  saturante a 255** (counter_add_u8 con max 0xFF), no u16 como suponía el
  mapeo inicial; 0x588C (+0x2E6) es un byte independiente decrementado 1
  por HORA en advance_clock (semántica pendiente).

### 2.1 `kernel_ring_regen()` — kernel 0x400C

Por cada miembro no muerto con arma/slot [0x55C5+i*0x20] == 0x2C (44 =
Ring of Regeneration): si rand_range(0,7) == 7 → counter_add_i16(&HP, 1,
maxHP). 1/8 de probabilidad de +1 HP por turno.

### 2.2 `kernel_apply_damage(idx, n)` — kernel 0x2A52

Flash de pantalla + sonido; `HP[idx] -= n` (word 0x55B8+idx*0x20 =
registro+0x10); si HP <= 0 → HP=0, status='D' (0x44), y si era el personaje
activo → g_active_char = 0xFF. Redibuja status.

## 3. Antorchas — Ignite (CMDS.OVL fileoff 0x0D98)

```
0d98: si g_torches (0x57AE) == 0 → "None owned!\n" (DS 0x430B), fin
0da8: g_torches--
0dac: si g_location ∈ [0x21, 0x28] (mazmorra):
0dba:   counter_add_u8(&g_torch_mins, rand_range(0,15) + 0x70, 0xFF)
          ; suma 112..127 minutos, tope 255
0dd6: si no (pueblo/exterior/underworld): g_torch_mins = 0xF0 (240) FIJO
```

## 4. Luz — recálculo en advance_clock (kernel 0x50A1–0x5145)

`g_light_level` (DS 0x58A5), rango efectivo 2..50; 0x33+ = centinela "no
recalcular" (lo usan las mazmorras).

```
50a1: si [0x58A5] >= 0x33 → no tocar
50b3: si g_location == 0x19 O g_floor > 0x7F (underworld) O hour < 5 O hour > 19:
50cf:   luz = 2                                  ; noche (20:00–4:59)
50d6: si hour == 5:  luz = ramp[g_minute / 10]   ; amanecer
50f4: si hour == 19: luz = ramp[(59 - g_minute) / 10]  ; atardecer (espejo)
5110: si no: luz = 0x32 (50)                     ; pleno día
        ramp = [2, 5, 10, 20, 34, 49]            ; DS 0x6A80 (DATA.OVL 0x6A90)
5115: si g_light_spell_mins (0x58A6) != 0 y luz < 0x12 → luz = 0x12 (18)
5128: si g_torch_mins (0x58A7) != 0 y luz < 0x0A → luz = 0x0A (10)
513b: si cambió → flag redibujado [0x24E6] = 1
```

Después (0x514A): si cambió la hora y no estamos en mazmorra → call 0x4A84
(sky/time status display, not an NPC schedule pass); 0x5884 = hora en
formato 12h para el display. Batch 36 re-read the complete 0x4a84 body
from the original EXE.

## 5. Movimiento y coste de tiempo por contexto

### 5.1 Bucle TOWN (TOWN.OVL) — 1 minuto por acción

Bucle principal TOWN ~0x142C–0x1686: toda acción aceptada (incl. movimiento
BLOQUEADO y respuesta 'N' al prompt de salida) → `advance_clock(1)`
(TOWN:0x15D4, `mov ax,1`). Handler de movimiento TOWN:0x600:

- Mapas 32×32. Aviso de borde: y<1 (N), y>30 (S), x>30 (E), x<1 (W) marca
  "salida" (flag [bp-6]); si el paso es transitable pregunta
  "Dost thou wish to leave?" (DS 0x2690): 'Y' → sale al overworld en las
  coords de la location (tablas DS 0x1E89/0x1EB1) **sin consumir minuto**
  (el bucle no llama a advance_clock al salir); 'N' → consume 1 minuto.
- Colisión: objeto en destino via kernel_object_at 0x368E (tabla de objetos
  0x5C62, 31×8 bytes) con excepciones aborda-bles, y transitabilidad:
  `kernel_tile_passable(transport, tile)` 0x2C4C — switch por clase de
  TRANSPORTE (clase = [DS 0x54F4 + transport>>2], tabla de 64 B en
  DATA.OVL fileoff 0x5504; jump table inline 0x2D60). A pie (clase 0) →
  `kernel_pass_on_foot` 0x2BD4: bitmap de 256 bits sobre tiles en DS
  0x54D4 (bit a 1 = bloqueado) + excepción: tiles 0x90-0x93 bloqueados
  salvo transportes 0x1C/0x1D y 0x40-0x4F.
- Bloqueado → print "Blocked!\n" (DS 0x26D6) + beep; el bucle SÍ cobra el
  minuto (verificado en vivo: escenario town-blocked-move).
- Los ÚNICOS advance_clock de TOWN.OVL: 0x15D4 (bucle, n=1), 0x1328
  (cárcel/espera, n=20 en bucle hasta la hora objetivo) y 0x511 (n=0 =
  recálculo de luz, en el init/carga del mapa del pueblo).

### 5.2 Bucle MAINOUT (overworld/underworld) — 2 minutos + terreno

Bucle principal MAINOUT 0xA84–0xD20:

```
0c30: si el handler devolvió 0 (acción no consumida, p.ej. BLOQUEADO):
        NO se avanza el reloj, NO hay housekeeping ni turno del mundo
0c39: advance_clock(2)          ; coste base de toda acción exterior
0c56: tile 0x6A/0x6B (puente): posible emboscada de trolls (MAINOUT 0x1BE8)
0c64: tile 4 (swamp) a pie (g_transport_tile 0x587C == 0x1C):
        OUTSUBS:0x5FC veneno (§6)
0c7e: tile 0x8F: "Burning!" (OUTSUBS:0x5EE)
0c8a: (0xE9,0xEB) piso 0: shrine quest check
0cd0: … 0xCD3: kernel_turn_housekeeping
0cd6: tile 1 + transporte de agua: "Rough seas!" (remolino)
0d05: tile & 0xFC == 0xD4: cascada → OUTSUBS:0x458 (caída al underworld)
0d11: MAINOUT:0x1A60 — turno del mundo (monstruos)
```

Handler de movimiento MAINOUT:0x490 → chequeo MAINOUT:0x1FE (misma
estructura que TOWN:0x600 sobre el mapa de chunks; "Blocked!\n" DS 0x29AE;
tile 0x2F cactus → "OUCH!\n" DS 0x29B8 + kernel_party_random_damage
0x2AA8 = rand(1,8) por miembro). Si movió → MAINOUT:0x354 (§5.3) y
MAINOUT:0x3E0 (terreno lento):

```
03e0: tile bajo el party (chunk map):
        clase 0 (normal): tile < 4, tile == 5, tile >= 0x10 (salvo 0x1E/0x1F)
        clase 1 (lento):  tile ∈ {4, 6, 7, 8, 0x1E, 0x1F}
        clase 2 (muy lento): tile ∈ [9, 0xF]
0448: clase 1: 1 turno extra del mundo (0x1A60) + advance_clock(2)
        → coste total del paso = 4 minutos; print "Slow progress!\n"
          (DS 0x29BF) salvo que el turno extra del mundo devolviera evento
0468: clase 2: 2 turnos extra del mundo + advance_clock(4)
        → coste total 6 minutos; print "Very slow!\n" (DS 0x29CF)
```

**El "terreno lento" NO es probabilístico**: es coste fijo de minutos
(2/4/6) + turnos extra de los monstruos; el mensaje es informativo (se
suprime solo si el mundo produjo su propio evento). El pantano (tile 4)
además envenena (§6). tile 5 (¿camino/hierba clara?) es explícitamente
normal.

Matiz de ORDEN (importa para g_prev_hour): el advance_clock del terreno
lento (0x461, dentro del handler) es una llamada SEPARADA que corre ANTES
del advance_clock(2) base del bucle (0xC39). Como cada llamada
re-snapshotea g_prev_hour (0x4FA0), un cruce de hora dentro del tramo de
terreno lento queda INVISIBLE para el housekeeping (p.ej. paso muy lento a
las 5:58 → el original NO aplica la comida de las 6:00). El clon replica
las dos llamadas, no una fusionada (movement.ts).

Navegación (fuera de alcance, Task 3.7): cada tramo de barco cuesta 2 min,
o 1 min con HMS Cape (MAINOUT 0x670–0x68C, g_hms_cape > 0x7F).

### 5.3 Wrap del overworld — MAINOUT:0x354

```
0354: mov al, [bp+6] (dx); add byte ptr [g_party_x], al
0361: mov al, [bp+4] (dy); add byte ptr [g_party_y], al
```

Las coordenadas son BYTES: la aritmética 8-bit wrapa 256×256 de forma
natural en ambos ejes (0 − 1 = 255, 255 + 1 = 0). El resto de la función
gestiona la ventana de chunks (origen 0x589B/0x589C, scroll vía
OUTSUBS:0x2C8 y carga de filas OUTSUBS:0x1B4 desde BRIT.DAT/UNDER.DAT),
también con máscaras de 8/5 bits coherentes con el wrap.

### 5.4 Pass (Space) — kernel 0x31F4

Imprime "Pass\n" (DS 0xA134) — o "Sheets in irons!\n" si navegando con la
vela suelta — y devuelve 1: el bucle de contexto cobra el coste estándar
(1 min pueblo, 2 min exterior). En mazmorra el bucle DUNGEON tiene su
propio coste (Task 3.4).

### 5.5 Camp/Hole up (kernel 0x3C9A) y posada

- Reparar barco (transport 0x24..): 5 × advance_clock(5) + casco
  +rand(1,3) hasta 99 — Task 3.7.
- Dormir en el campamento: OUTSUBS:0x658 (level-ups: nivel =
  bit_alto(exp/100)+1, maxHP = 30 × nivel, sueño de karma) — Task 3.9/3.12.
- Cárcel/posada TOWN:0x1324: bucles de `advance_clock(20)` hasta la hora
  objetivo (así el reloj salta 20 en 20 minutos, con comidas/hambre
  aplicándose en cada cambio de hora).

## 6. Veneno de pantano — OUTSUBS.OVL:0x5FC

Al TERMINAR un paso sobre tile 4 a pie (llamado desde MAINOUT:0xC71):

```
por cada miembro i < party_size con status != 'D' y != 'P':
    si rand_range(1, 30) > DEX_i (byte 0x55B5+i*0x20 = registro+0x0D):
        status_i = 'P'; print "Poisoned!\n" (DS 0x3A1B)
```

El daño del veneno (1 HP/turno) lo aplica turn_housekeeping (§2).

## 7. OUTSUBS.OVL — catálogo honesto (2464 B, 12 funciones)

NO es "el subsistema del kernel": es el overlay de **soporte del
overworld** (nivel 2). Funciones (fileoff → nombre en coverage.json):

| fileoff | stub CS | nombre | evidencia |
|---------|---------|--------|-----------|
| 0x0000 | (interno) | outsubs_chunk_flag_check_a | tabla DS 0x3866[8] vs estado [0x58D0+i]==1; usada por 0x98 |
| 0x004A | (interno) | outsubs_chunk_flag_check_b | tabla DS 0x386E[8] vs [0x58D8+i]>0x7F (g_shrine_destroyed) |
| 0x0098 | (interno) | outsubs_chunk_load | lee chunk de 256 B (fichero 'B'RIT/UNDER via kernel 0x256E) a la caché 0x6608; sustituye tiles 0x16-0x18→0xDF y 0x19→0x1A según flags |
| 0x01B4 | 0x7B8A | outsubs_chunk_scroll_load | recarga filas/columnas de chunks al cruzar frontera (origen 0x589B/0x589C) |
| 0x02C8 | 0x7BD2 | outsubs_chunk_cache_shift | desplaza las páginas de 256 B de la caché al hacer scroll |
| 0x0368 | 0x7A22 | outsubs_world_filename | devuelve "BRIT.OOL"/"UNDER.OOL" (DS 0x3989/0x3992) según g_floor |
| 0x0388 | 0x7BBA | outsubs_enter_location | matching coords vs tablas DS 0x1E8A/0x1EB2 (32 locations); entra: g_location=i+1, floor=0, pos=(15,30) |
| 0x0458 | 0x7B42 | outsubs_waterfall_fall | "F-A-L-L-S!!!" / "Falling into underworld!!"; daño por miembro; en (0x36,0x8A) → g_floor=0xFF y recarga UNDER |
| 0x0566 | 0x7B96 | outsubs_spawn_status_objects | siembra objetos 0x5D3A/0x5D42 (amuleto LB, shadowlords) al entrar en MAINOUT |
| 0x05EE | 0x7B5A | outsubs_burning_tile | "Burning!" (tile 0x8F) + daño |
| 0x05FC | 0x7B4E | outsubs_swamp_poison | §6 |
| 0x0658 | 0x7F56 | outsubs_camp_results | level-ups + sueño de karma al acampar (§5.5) |

## 8. Diccionario de globals tocado (evidencia = este doc)

| DS | nombre | notas |
|----|--------|-------|
| 0x5880 | g_prev_hour | snapshot para detección de cambio de hora (advance_clock y housekeeping) |
| 0x587A | g_time_spell | carácter de efecto temporal en status: 'Q'uickness, 'T'ime-stop, 0 = ninguno |
| 0x588E | g_time_spell_turns | turnos restantes del efecto (0xFF = permanente) |
| 0x587C | g_transport_tile | tile/estado de transporte: 0x1C pie, 0x10-0x17 caballo/carpet, 0x20-0x2F barco, 0x24-0x27 skiff… (censo parcial) |
| 0x58A5 | g_light_level | radio de luz 2..50; ≥0x33 = control manual (mazmorra) |
| 0x58A6 | g_light_spell_mins | minutos restantes del hechizo de luz (min. luz 18) |
| 0x58A7 | g_torch_mins | MINUTOS de antorcha (antes "turnos"); min. luz 10 |
| 0x588B | g_turn_count | u8 saturante a 0xFF (corrige el u16 del mapeo inicial) |
| 0x588C | g_unk_588c | u8, −1 por hora (counter_sub en 0x4FD7); semántica pendiente |
| 0x5955 | g_sail_dir | última dirección de navegación (0 = parado); MAINOUT |
| 0x5956 | g_unk_5956 | flag de repintado/parada de navegación; MAINOUT |
| 0x58C8 | g_shadowlord_locs | 3 bytes: town (1..8) de cada Shadowlord; re-roll a medianoche |

### 8.1 `0x58C8` nombrado POR SUS CONSUMIDORES (#101, seis en cinco overlays)

El nombre no se hereda ni se infiere del re-sorteo: se cierra por uso. Dos bastan.

| dónde | qué hace | qué prueba |
|---|---|---|
| TOWN.OVL:0x02CC | `cmp byte [si+0x58C8], dl` con `dl = g_location` | el contenido **es** una location |
| TOWN.OVL:0x127F | con `g_location == 0x1D` (Stonegate) barre `si=2..0` y llama 0x11B8 si `< 0x80` | los tres, orden **2→1→0** |
| CAST.OVL:0x170B | `mov byte [bx+0x58C8], 0xFF` junto a `[bx+0x57B6]=0` y el OR sobre `g_npc_dead_bitmap+112` | `0xFF` = **destruido** |
| CMDS.OVL:0x1078 | `cmp [bx+0x58C8], 0xFF / jne` como puerta | exige «no destruido» (índice 0..3, 3 = ninguno) |
| LOOKOBJ.OVL:0x04BF | casa el slot contra una location+1 | lectura para mostrar |
| OUTSUBS.OVL:0x05AE | pinta tile `0xB4` sólo si `< 0x80` | `>= 0x80` = fuera del mapa |

Semántica cerrada: `< 0x80` en el mapa · `>= 0x80` fuera · `0xFF` destruido · `1..8` las
ocho ciudades. El clon ya tenía derivadas las dos de TOWN.OVL (`world/shadowlord-urban.ts`,
incluido el orden 2→1→0 de Stonegate).

### 8.2 🔴 #101-bis — el clon comparaba contra «los otros dos» (DIVERGENCIA, ARREGLADA)

**Y esta nota era el sembrador**: §1 decía «!= g_location y != *las otras dos*», y eso es
literalmente lo que el port implementó (`locs[(i+1)%3]`, `locs[(i+2)%3]`). Corregido arriba.

Medido con la función real del clon contra transcripción literal del asm, **mismo flujo de
rand**, 60.000 medianoches:

| magnitud | valor |
|---|---|
| difieren en nº de TIRADAS | 18.898 / 60.000 = **31,5 %** |
| difieren en DESTINO | 19.395 / 60.000 = **32,3 %** |
| control «se queda en su misma ciudad» | clon **12.170** · binario **0** |

El cero del binario es **estructural, no estadístico**: `si=0..2` incluye el slot que se
reescribe y la escritura es posterior. Ejemplo reproducible: inicio `[7,255,3]`, party=1 →
clon 3 tiradas `[7,255,8]` · binario 4 tiradas `[8,255,7]`.

**ALCANCE — y por qué los arneses son CIEGOS a esto.** `state.shadowlordLocs` es `[]` por
defecto (`core/state.ts`), y con `[]` el bucle hace `continue` y consume **cero** tiradas;
sólo se puebla al cargar partida (`saveNative.ts`). Los arneses de paridad no lo fijan ⇒
**no pueden ver la divergencia por construcción**, y su verde se lee como ausencia de
defecto. Muerde en **partida cargada**, no en los sellos. Una divergencia invisible a los
arneses no es una divergencia pequeña: es una que nadie iba a encontrar.

### 8.3 🔴 El reintento SIN COTA es literalmente sin cota — semilla 20

`0x5039 je 0x5004` re-tira sin contador (el bucle **exterior** sí está acotado a 3,
`0x504B cmp [bp-4],3`). Con un generador estancado eso es un bucle infinito, y hay un
estado que lo produce: **`g_rng_seed == 20` es punto fijo** del generador de `rand_range`
(0x2092, ver `rng.md`):

```
20 + 0x9248 = 0x925C → ror3 = 0x924B → ^0x9248 = 0x0003 → +0x11 = 20
```

Medido sobre el espacio ENTERO de 16 bits en el clon: es el **único** punto fijo, su
**única preimagen es él mismo** (no se cae dentro dando pasos) y el ciclo del stream vivo
(`OriginalRng(0)`) tiene **47.343** estados y **no lo contiene**. ⇒ sólo se entra por un
`srand(20)` explícito — y el juego re-siembra del reloj de pared al acampar (`camp`,
int 21h AH=2Ch). Alcanzabilidad estrecha, consecuencia total: se congela el RNG del juego
**entero**, no sólo este bucle.

**NO se le pone cota al bucle**: el binario no la tiene y ponerla sería divergencia. Se
declara y se prueba (`game/tests/survival.test.ts`). Mismo grado que el defecto de
`rand_range` sin guarda `max<min`: mecanismo medido, alcanzabilidad estrecha.
Descubierto **por accidente**, al colgarse un test de propiedad que barría semillas.

## 9. Qué queda fuera (honesto)

- ~~**DUNGEON.OVL**: su bucle llama advance_clock(1) (DUNGEON:0xF2F) solo en
  el toggle de Quickness; el coste por paso de mazmorra se deriva en
  Task 3.4.~~ **RESUELTO y RECTIFICADO (#159)**: el coste por paso de mazmorra
  es **1 minuto**, y sale de ESE MISMO `advance_clock` de 0x0F2F. No hay dos
  llamadas: el camino sin hechizo temporal hace `mov di,1` / `mov ax,di` en
  0x0FEA-0x0FED y **salta dentro** del bloque de Quickness (`jmp 0xf2e` en
  0x0FEF, bytes `e9 3c ff` ⇒ 0x0FF2−196 = 0x0F2E) para reutilizar su `push
  ax` / `call`. La frase «solo en el toggle de Quickness» era el artefacto de
  contar `call` sin seguir el `jmp`. Detalle completo y tabla en
  `re/notes/dungeon.md §1`.
- Navegación/viento, remolino, trolls del puente, cascadas, camp completo,
  posada: anotados arriba con sus offsets, se portan en 3.7/3.9.
- 0x588C (countdown horario) y el detalle de los flags 0x58D0 (sustitución
  de tiles 0x16-0x18 en chunks): sin identificar — no se bautizan.
- El contador de turnos del clon (`turnsSinceStart`) se mantiene sin
  saturar (el original satura a 255); divergencia consciente documentada
  aquí y en survival.ts.
