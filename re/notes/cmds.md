# Comandos sueltos — CMDS.OVL + SJOG.OVL (Task 3.9)

Reglas re-derivadas del asm con citas `offset: → regla` (formato de
`re/notes/combat.md`). Offsets = file offset dentro del `.OVL`.
CBASE(CMDS/SJOG) = **0xBF80**; `call 0xNNNN` con NNNN≥0x1D10 (o `0xffffXXXX`) =
externo kernel, `off = (NNNN + 0xBF80) & 0xFFFF`. `rand(lo,hi)` = kernel 0x2092,
inclusive ambos (`re/notes/rng.md`; `push lo; push hi`).

El clon vive en `game/src/core/world/commands.ts` (funciones puras) + wiring en
`game/src/core/game.ts`. Paridad: `re/tools/cmds_parity.py` (predict asm ↔
`__parity__/cmds-run.ts` con `OriginalRng`), escenarios `re/parity/cmds/` (15).
Verificación resumida en `re/verified/cmds.md`.

Convención de paridad = **modelo↔clon** (Python asm-derivado vs core TS del clon,
mismo stream y estado), como mazmorra (3.4). Bloqueo BSS: los cofres/objetos son
registros de 8 B que sólo existen con un mapa real cargado; headless no se
siembran, así que la paridad runtime contra DOSBox de los comandos de
cofre/objeto es divergencia de alcance (igual que el render de mazmorra).

---

## 1. Ignite torch (CMDS 0x0D98) — 1 rand SÓLO en mazmorra ✅

- `0d98: cmp [g_torches 0x57AE],0; jne` → si 0 → "None owned!" y `ret`, sin
  consumir nada.
- `0da8: dec [g_torches]` → gasta 1 antorcha.
- Fuera de mazmorra (`0dac: loc<=0x20 || 0db3: loc>=0x29`) → `0dd6: mov byte
  [g_torch_mins 0x58A7], 0xF0` = **SET 240**, sin RNG.
- Mazmorra (0x21≤loc≤0x28): `0dc5: rand(0,15)`; `0dc8: add 0x70` → 112..127;
  `0dd0: call 0x7f70(&g_torch_mins, 0x70+rand, 0xFF)`.

**Semántica de kernel 0x3EF0** (= `0x7f70`), resuelta estáticamente (pregunta
abierta del draft §10): es un *saturating-add* con cap, NO un set —
```
al = [ptr]; ax = al + amount; if ax >= cap: [ptr] = cap else [ptr] += amount
```
(ULTIMA.EXE 0x3ef0-0x3f10). Con antorcha apagada (torch_mins=0) el resultado es
112..127; con una ya encendida, suma. El clon `survival.ts::igniteTorch` hace
`min(cur+112+rand(0,15), 0xff)` en mazmorra y `=240` fuera → **idéntico byte a
byte** (min(x,255) ≡ el if/else del kernel). Ya era exacto; sólo se documenta.

## 2. New Order (CMDS 0x0DDC) — SIN RNG ✅

Swap de dos registros de roster (base 0x55A8, stride 0x20). `0e0c`: si el índice
elegido es 0 (Avatar) → nombre + " must lead!" y sale — **el Avatar queda
anclado en la posición 1**. `0e64`: swap de 32 B vía buffer temporal. `0ea8: mov
[g_unk_a9fa],1` (redraw); **no setea g_unk_24e6** → acción libre (no consume
turno). Clon: `newOrder(characters, idx1, idx2)` + `game.newOrder`.

## 3. Push (CMDS 0x161A) — SIN RNG ⚠️→formulado (tile-gating exacto, capas ausentes)

Predicado `is_pushable_tile` (0x14ba, transcrito RANGO a RANGO): tiles **0x5B;
0x90..0x93; 0xA5..0xA6; 0xA8..0xA9; 0xAD..0xAF; 0xB4..0xB7** (14bd-14fc; ¡ojo!
0xA7 y 0xAC quedan FUERA). Relleno **0x45** (cañón, `(src&0xFC)==0xB4`) / **0x44**
(suelo).

Decisión (16bf-178a):
1. `0x770e(fuente)==0` (sin objeto encima) AND `is_pushable(srcTile)` — si no →
   "Won't budge!".
2. Si `0x770e(dest)==0` **Y `destTile == fill` EXACTO** → deslizar (`0x1548`):
   `dest ← srcTile`, `fuente ← destTile(=fill)` (es un SWAP src↔dest).
3. Si no, **`partyTile == fill` EXACTO** → pull (`0x15B0`): `celda_party ←
   srcTile`, `fuente ← partyTile(=fill)`.
4. Si no → "Won't budge\n" (**SIN '!'** — DS 0x4567 vía 0x1784; cadena DISTINTA
   de la 0x4559 del paso 1, no un typo: approved-strings.json, arena fix-121;
   corregido en #289 — esta línea decía «Won't budge!»). Éxito: el party avanza
   a la celda fuente; `1798: [g_unk_24e6]=1` (**turno**).
5. Clases 0x90/0xB4: tras mover, **rotan su facing** al tile correspondiente a la
   dirección de empuje (`dir_vector_to_facing` 0x1504; slide 1575-15a8, pull
   15dd-1613). En combate (loc>0x7f) usa la posición de `g_cmb_actor`.

Clon (`game.push`): reproduce el gating EXACTO de tile (destTile==fill / partyTile
==fill) y el swap. **Divergencias ⚠️** por el modelo del clon: (a) sin capa de
objetos → no evalúa `0x770e` (celda vacía de objeto se asume); ~~(b) **no rota el
facing** de cañón/mueble (0x90/0xB4)~~ **(b) CERRADA por #289: rota vía
`pushOrientedTile` — slide flip=0, pull flip=1 (0x1575-15a8 / 0x15dd-1613) — e
imprime "Pushed!\n"/"Pulled!\n" y las DOS cadenas de fallo 0x4559/0x4567**; (c) la
rama de combate `g_cmb_actor` vive en `Combat.playerPush` (arena propia, fix-121).
Por eso Push se marca ⚠️→formulado en `re/verified/cmds.md`.

## 4. Klimb-con-garfio exterior (CMDS 0x1C20) — RNG rico ✅ (MECÁNICA NUEVA)

- `1c28: cmp [g_grapple 0x57AF],0; jne` → sin Grapple → "With what?".
- `1c3a: cmp [g_transport_tile 0x587C],0x1c; je` → sólo a pie → si no "On foot!".
- `1c46: 0x766c` dirección; celda objetivo = party+dir; tile via 0x8482:
  - `1c84: cmp 0x0D` → tile 0x0D → "Impassable!".
  - `1c8e: cmp 0x0C` → tile ≠ 0x0C → "Not climbable!". **Único escalable = montaña
    0x0C** (en el clon tile 12 = SmallMountains; 0x0D=13=TallMountains impasable —
    calzan exacto).
- Por cada miembro `i` de g_party_size (`1c9a`..`1cf9`):
  - `1cb3: cmp status[0x55B3+i·0x20],'D'; je skip` → los **muertos NO tiran**.
  - `1cc0: rand(1,30)`; `1cca: cmp DEX[0x55B5+i·0x20], roll; jae skip` → **DEX ≥
    roll ⇒ escala sin daño**; DEX < roll ⇒ "Fell!" + `1cde: rand(1,5)` daño
    (`1ce2: 0x6ad2` apply_damage con clamp).
  - Orden EXACTO: dex(m0)[,dmg(m0)], dex(m1)[,dmg(m1)], … (crítico para stream).
- `1cfc: 0xffffbc46` mueve el party sobre la montaña. El clon: `klimbGrapple` +
  `game.klimbGrapple` (mueve la party a la celda de la montaña).

El clon previo respondía "What?" en el exterior y no tenía nada de esto.

## 5. Camp / Hole-up (CMDS 0x0552 + helper 0x0400) — RNG ⚠️→formulado

"For how many hours?" (dígito '0'..'9'); miembros 'G'→'S'; si el tile delante ≠0
→ interrumpe. Helper de curación **0x0400** (recorre el roster):

> ⚠️ **CORRECCIÓN de reloj (F1.8-T2, verificada byte a byte por review):** el
> "`advance_clock(10)` por hora" del draft es **impreciso**. Lo derivado: la escena
> fija `target_hour = (g_hour + hours) mod 24` (CMDS 0x0066-0x0079) ⇒ **pasan N
> horas de juego**, animadas en pasos de **5 min** por frame (`advance_clock(5)`,
> 0x0318), NO 10/hora. Y el helper 0x0400 corre **UNA SOLA VEZ POR ACAMPADA** (no
> por hora): el cooldown `g_unk_588c` recarga a 0x0E (0x0505) y bloquea repeticiones
> dentro de las ≤9 h. Consumo de rands por acampada = (#elegibles) heal + 1 gate.
- Elegibilidad por miembro (0x0444-0x0464): flag por-miembro==0, **`g_unk_588c <
  1`** (cooldown), `[bp+4] > 5`, status≠'D', y `si ≠ [bp+6]` (el de guardia).
- `046e: HP += rand(1,63)`; clamp a maxHP (0x0473). **No hay gate de HP-lleno**:
  el rand se tira igual y satura.
- **Restaura MP por clase** (0x0483-0x04e2, 0 rand): Avatar/Mage ('A'/'M') →
  `MP = INT` (0x55b7 = 0x55b6); Bard ('B') → `MP = INT/2` (shr 1); Fighter → nada.
- Tras el bucle (0x04e7): `test [bp+8],0x82`==0 → `04f4: rand(0,99)`; `<25` →
  **evento/emboscada** (fija g_month en 0x588d, llama 0xffffbfd6).
- `0505: g_unk_588c = 0x0E` (recarga el cooldown; se decrementa 1/hora en otro
  sitio).

⚠️ El gate `g_unk_588c` (cooldown de curación ligado al reloj) y la exclusión
`si≠[bp+6]` hacen el stream dependiente de estado horario que el núcleo puro no
reproduce headless — **igual que el flicker de render en mazmorra**. El clon
`campHoleUp(members, rand, guardIdx)` (F1.8-T2) modela la ESTRUCTURA: heal
rand(1,63)/miembro elegible + rand(0,99)<25 de emboscada, **UNA vez por acampada**
(el guardia no cura), con test conductual; NO entra en el set de paridad
seed-exacta. El wiring (`game.camp`, kernel 0x3C9A) avanza el reloj N horas
aparte. Los rechazos de contexto ("On land or ship!" 0xa30e sobre agua 1-3, "On
foot!" 0xa322 en vehículo) **cobran turno** (jmp 0x3cd8→0x3ee5 fija g_unk_24e6=1);
la cancelación del prompt de horas (0/Space) sale por 0x3eea **sin** turno. La
reparación de casco NO vive en CMDS (la portó 3.7, kernel 0x3C9A); Camp no la
duplica.

## 6. Jimmy (SJOG 0x0D4A) — RNG por tipo de cerradura ✅ (MECÁNICA NUEVA)

Precond: mazmorra → `jimmy_dungeon_room` 0x0C3E; `g_keys 0x57AC==0` → "No Keys!".
Dirección 0x766c. **La llave se gasta SÓLO al fallar; la cerradura mágica siempre
rompe.**

| tile | tipo | tirada | éxito | fallo |
|------|------|--------|-------|-------|
| 0xB9/0xBB | puerta | `rand(0,29)` @0dd4 | DEX > roll → tile−1 (0xB8/0xBA), "Unlocked!", **sin llave** | "Key broke!" + dec g_keys |
| 0x97/0x98 | mágica | — | (nunca) | SIEMPRE "Key broke!" + dec |
| 0x84/0x85 | cepo/prisionero | `rand(0,29)` @0e54 **SIEMPRE** (pueblo y mazmorra convergen en 0e42 tras seleccionar miembro; el `cmp loc,0x80` de 0e22 sólo salta el check de ocupante 0x770e) | DEX>roll: efecto por loc — **loc<0x7f** (pueblo) libera NPC (limpia [idx·16+0x5F68], estado [0x5D5E/5F/60+idx·16]=5, "I thank thee!" 0x8b36) + **karma += 2 cap 99** (0x7f70); **loc≥0x7f** → tile=0x44, "Unlocked", sin karma | DEX≤roll: "Key broke!" + dec (en AMBOS contextos) |
| cofre-objeto (0x0BAA) | `((tile&0x7F)−DEX+0x1E)>>1` = threshold; `rand(1,30)` | roll > threshold → "Success!", limpia bit 0x80, **sin llave** | roll ≤ threshold → "Key broke!" + dec |
| sala mazmorra (0x0C3E) | `(floor·2−DEX+0x1E)>>1` = threshold; `rand(1,30)` | roll > threshold → tile=0x40\|(tile&8), "Chest unlocked" | roll ≤ threshold → "Key broke!" + dec |

Clon: `jimmyLock({kind,tile,dex,floor,location}, rand)`; el llamador hace
`if (keyBroke) state.keys--` y, para `prisoner`, `if (freed) karma=min(99,karma+karmaDelta)`.
**Puertas 0xB9/0xBB CABLEADAS** en `game.jimmy(dir)` (son tiles de mapa + `state.keys`
existe; NO aplica el bloqueo BSS). Cepo y cofre-objeto necesitan la capa de
objetos/NPC → cableado en 3.13.

**Turno**: el binario sólo marca turno consumido en el ÉXITO (`0e10:
[g_unk_24e6]|=2`); el fallo (romper la llave) sale por 0xf81 **sin** tocarlo. El
clon `game.jimmy` hace `endTurn` sólo en el éxito para calcar esto.

**Quirks sin rand** (para el stream de 3.13): en `jimmy_chest_obj` (0x0BAA) un
objeto sin bit 0x80 (no cerrado) → "Key broke!" + dec **sin tirar** (0bc7→0c1d); en
`jimmy_dungeon_room` (0x0C3E) un cofre ya abierto (0x70) → "Already open!" sin
tirar. Ambos consumen 0 rand.

## 7. Open (SJOG 0x1374) — karma + trampa + botín ✅ (funciones puras)

Precond: mazmorra → `open_dungeon_room` 0x12D4. `0x7a4c(0x594f,0x5950,0x5951)`
**auto-cierra la puerta abierta previa** (tracker g_unk_594f + timer [0x5952]=4).
Dispatch tile: 0xAF "It's open!"; 0x99 "Too heavy!"; 0x97/0x98/0xB9/0xBB
"Locked!"; **0xB8/0xBA** abre (g_unk_594f=tile, timer=4, tile=0x44, "Opened!");
else → `open_chest_world` 0x112C.

`open_chest_world`: contents = [slot·8+0x5C5F]; quita el objeto.
- **Karma robo**: si `1 ≤ g_location ≤ 0x20` (pueblo): `g_karma>2 ? −2 : =0`
  (abrir cofre ajeno = robo).
- **Trampa**: contents bit 0x80 → "Trapped!" + `0x7050(member)` (§8).
- **Botín**: `loot_fixed` 0x1040 + `loot_random` 0x10B8 (§9); vacío → "Chest
  empty!".

Clon: `chestTrap` + `chestLoot` (puras, paridad de stream). El wiring de Open
sobre cofres-objeto queda para el bucle con objetos cargados (3.13); las reglas
de karma/trampa/botín están portadas y verificadas por stream.

## 8. Trampa de cofre — kernel 0x2FD0 (= SJOG `0x7050`) ✅

Firma `trap(member)`. `2fd7: 0x223c` sonido cosmético (sin rand de juego).
- **loc > 0x7f (banda ALTA)**: `2ff4: type = rand(0,1)` → sólo ACID/POISON.
- **loc ≤ 0x7f**: `3001: r = rand(0,7)`; `3006: type = [r + 0x559e]` con la tabla
  DS:0x559e = `00 00 00 01 01 02 02 03` → **ACID 3/8, POISON 2/8, BOMB 2/8, GAS
  1/8**.

⚠️ La banda alta **NO es «mazmorra»**: el original escribe `g_location` 0x21..0x28 en
mazmorra (#123), que cae por DEBAJO de la frontera ⇒ en mazmorra sale la tabla COMPLETA.
El único inmediato > 0x7f que se escribe en los 28 `.asm` es **0xFF**, y en los tres
sitios leídos va siempre acompañado de aparcar la localización real en otra global
(`DUNGEON 0x00a8`, y en 0x00d8 restaura desde `g_unk_5894`; `CAST2 0x0ea4`, que antes la
copia a `[0xbd15]`; `CMDS 0x0332`, que la repone tras usarla) ⇒ 0xFF = **localización
suspendida por una escena**, no un lugar. Medida sobre inmediatos: los ocho `mov [g_location], al/ah`
del censo quedan fuera de esa cota. La etiqueta definitiva es de **#184**.

| type | DS | texto | efecto | rutina |
|------|-----|-------|--------|--------|
| 0 ACID | 0x5581 | `ACID!\n` | daño al que abre = `max(1, rand(0,60) >> 1)` ∈ [1,30] | 0x3abe→0x2a52 |
| 1 POISON | 0x5588 | `POISON!\n` | envenena al que abre (status→'P'), sin daño | 0x2fa6 |
| 2 BOMB | 0x5591 | `BOMB!\n` | por cada miembro **VIVO**: `rand(1,8)` daño (roll sólo si vivo) | 0x2aa8 |
| 3 GAS | 0x5598 | `GAS!\n` | envenena a los 6 (0x2fa6 comprueba vivo+idx<size) | 0x305d |

Los cuatro textos volcados byte a byte de **DATA.OVL** con la regla de siempre
(`fileoff = DS + 0x10` → 0x5591/0x5598/0x55a1/0x55a8) y la tabla en 0x55ae. Que la
rutina viva en el kernel no mete sus cadenas en `ULTIMA.EXE`: `DS` es el segmento
compartido y lo que se carga ahí es DATA.OVL.

**TRES callers**, resueltos por banda (`re/tools/callers_por_banda.py 0x2fd0`; el grep
por texto ve 0 de 3):

| caller | qué lo dispara |
|---|---|
| `SJOG.OVL:0x1222` | cofre del mundo |
| `SJOG.OVL:0x1323` | cofre de mazmorra |
| `CMDS.OVL:0x1c04` | ★ Mix con los reagentes incorrectos (§12) |

Clon: `chestTrap(location, opener, members, rand)`, portado y fiel al kernel. Lo que NO
está cableado en el port es el caller de Mix; ver `re/notes/mix-trap-105-acta.md` §5.2.
La rutina sigue siendo la del kernel.

## 9. Botín de cofre — orden EXACTO de rand ✅

Tablas volcadas byte a byte (`task-3.9-trap-loot-tables.md`). El orden de rand es
crítico; la clave es que el chequeo `guard>contents` corta ANTES de tirar.

**`loot_fixed` (0x1040), filas si=7..0**: item/guard/maxqty = FIXED_*.
```
si guard[si] > contents: continue           # SIN tirada (1083)
roll = rand(1,30)                            # 1090
si guard[si] > roll: continue                # 1099
base = maxqty[si]==1 ? 1 : rand(1,maxqty[si])# 109d/1050
qty  = loot_place(item[si], base, contents)  # (§9a)
```
`FIXED_ITEM=[1,2,3,4,7,8,13,15]`, `FIXED_GUARD=[25,3,17,17,9,15,7,7]`,
`FIXED_MAXQTY=[10,90,8,8,2,2,2,2]`.

**`loot_random` (0x10B8), `floor(contents/2)+1` intentos**:
```
idx = rand(0,47)                             # 10db
si guard[idx] > contents: continue           # SIN 2ª tirada (10e6)
roll = rand(1,30)                            # 10f2
si guard[idx] > roll: continue               # 10fb
qty = loot_place(item[idx], idx, contents)   # equipo → else → qty=idx
```
(Ojo al conteo de iteraciones: el bucle 0x1117 corre con `ax` de count..0 →
`(contents>>1)+1` cuerpos.)

### 9a. `loot_place` (0x0F88) — la cantidad depende del ID (usado como "mode")
`0f8f`: switch sobre el ID del item:
- id 1 → `rand(1, contents)` (0fa4)
- id 2 (oro) → `rand(1, 3·contents)` (0fb6)
- id 3/4 → `base − 1` (0f9b: dec, sin tirada)
- else → `base` (0fc6, sin tirada)

Nota: para el oro (id 2) se tira **base=rand(1,90)** en loot_fixed y luego se
DESCARTA por el override `rand(1,3·contents)` — pero el rand se consume igual
(hay que reproducirlo). Clon `chestLoot` lo respeta.

### 9b. Botín de sala de mazmorra `get_dungeon_room` (0x179E), filas si=0..6
```
roll = rand(1, floor*4+4)                    # 182b
si guard[si] > roll: continue                # 183f (sin tirada de cantidad)
si si==5: qty=rand(0,7), tipo POCIÓN         # 1851
si si==6: qty=rand(0,7), tipo SCROLL         # 186a
si si==1: qty=rand(1, floor*8), ORO          # 187d
else:     qty=rand(1, maxamt[si]), item[si]  # 188c
```
`DUNGEON_GUARD=[2,4,5,10,20,25,25]`, `DUNGEON_MAXAMT=[31,0,3,3,3,7,7]`,
`DUNGEON_ITEM=[15,2,7,8,13,0xFF,0xFF]`. Clon: `dungeonChestLoot(floor, rand)`.

Mapa id→categoría (switch de GET 0x1458): 1=chest, 2=gold, 3=potion, 4=scroll,
5/6/9/10/11/12=equipment, 7=keys, 8=gems, 13=torches, 14=sandalwood, 15=food.

## 10. Get (SJOG 0x18CE) — cosecha/karma + switch de items ✅ (parcial)

Precond: mazmorra → `get_dungeon_room` 0x179E (§9b). Objetos ~~kind∈{0x10,0x19,0x1B}~~
(**CORREGIDO 17-08, ficha #354** — derivación 9b194451 / `get-alfombra-346.md` §2, cuerpo
0x1958-0x197D re-leído: la primera condición es `0x196a cmp cx,0x10 / jl` = **RANGO
kind<0x10**, no el valor 0x10 — la lectura vieja INVERTÍA dos casos: el caballo aparcado
0x10 parecería cogible y la caja de sándalo 0x0E no) **kind<0x10, o ==0x19, o ==0x1B,**
o (kind&0xFC)==0xB4 → `get_item_switch` 0x1458 (determinista). Si no, por
tile del mapa:
- **0x2D cosecha** → tile=0x2C, "Crops picked!", +1 comida (cap 9999); **karma:
  g_karma≠0 → dec**.
- **0x9A/0x9B/0x9C platos** → comer si alcanzable ("Mmmmm...!") / "Can't reach!".
  **[DERIVADO 2026-07-27, tarea #35 — el «alcanzable» es un GATE DE DIRECCIÓN, y los
  platos SÍ tocan karma.]** Leído en `re/disasm/SJOG.OVL.asm`; `[bp-0xa]`/`[bp-0xe]` son
  los deltas `g_cmb_scratch_x/y` de la dirección elegida:
  · **0x9A** (comida en la mitad ALTA) `@0x1a6a`: exige `dy == +1`; si no → mensaje
    DS 0x8e10 y **salida sin comer**. Si sí: tile ← **0x95**, y **`jmp 0x1a40`** = la
    COLA DE LA COSECHA.
  · **0x9B** (mitad BAJA) `@0x1a92`: exige `dy == −1`; si no → DS 0x8e30. Si sí: tile ←
    0x95 y **`jmp 0x1a58`**.
  · **0x9C** (las DOS mitades) `@0x1aca`: si `dx == ±1` → DS 0x8e44 **sin comer** (no
    se alcanza de lado); si no, `dy==+1` → tile ← 0x9B y `dy==−1` → tile ← 0x9A (te
    llevas la mitad de TU lado), y `jmp 0x1aae` → `jmp 0x1a58`.
  **Consecuencia:** las tres ramas de ÉXITO desembocan en la cola de la cosecha
  (`0x1a44-0x1a66`): **+1 comida con cap 9999** (`call 0x7f94` con 0x270f/1/0x57a8) y
  **`cmp [g_karma],0` / `jne` / `dec [g_karma]`** en 0x1a58-0x1a66 — o sea **karma −1 si
  karma≠0, EXACTAMENTE el mismo delta que la cosecha**. Robar comida de una mesa es tan
  malo como segar el trigo ajeno. Asimetría menor: sólo 0x9A pasa por `g_unk_a9fa = 1`
  (0x1a53); 0x9B/0x9C entran a la cola por debajo de esa línea.
  Esto CIERRA `content-audit.md` §ÁREAS SIN FUENTE LOCAL ítem 9 («deltas de karma de
  table-plate exactos»): no requerían derivación nueva, estaban en 0x18CE.
- **0xB0/0xB1 antorcha de pared** → tile=0x44, **g_torch_mins=0x64 (100)**,
  "Borrowed!".
- else "Nothing to get!".

**DIVERGENCIA del clon en los platos (medida 2026-07-27, tarea #35 — NO arreglada, radio
de sellos primero).** `game.ts:get` acierta lo que más importa: `stealFood` aplica **+1
comida y karma−1**, y para 0x9C elige `dir==="south" ? 0x9B : 0x9A`, que casa con el
binario (llegar desde arriba deja la mitad de abajo). Lo que le falta es **el gate de
dirección**: (1) en 0x9A/0x9B no comprueba `dy` — el binario sólo deja comer 0x9A con
`dy==+1` y 0x9B con `dy==−1`, el clon come desde cualquier lado; (2) en 0x9C no rechaza
`dx==±1` — el binario dice «no alcanzas», el clon come como si vinieras del norte.
Además el `food++` del clon no aplica el cap 9999 del binario (tampoco en la cosecha).
⚠ El comentario de `game.ts` (rama `stealFood`) declara los **deltas food/karma
«pendiente de oráculo DOSBox»**: ya NO lo están — salen de 0x18CE y COINCIDEN. Lo que
sigue abierto de esa nota son los **strings** de los platos (DS 0x8e04/0x8e10/0x8e24/
0x8e30/0x8e44/0x8e58, hoy sustituidos por un "Borrowed!" que pertenece a la antorcha —
Clase C #69) y el gate de dirección.

**Piedra lunar (kind 0x19, ficha #357)**: la segunda excepción escrita a mano despacha por
la cadena secundaria de `get_item_switch` (0x172D `cmp ax,0x19` → 0x1733 `jmp 0x148c`); la
rama 0x148C imprime DS 0x8C4E «A moonstone!\n» y hace `[bx+0x5840]=0xFF` (bx = quality =
nº de piedra; 0x5840 = tabla de location, 0xFF = mochila) antes de la cola común 0x177A.
Careo del clon en `get-alfombra-346.md` §7 (RESUELTO): grant portado desde antes
(`applySearchGrant` id 25), nombre portado por #357 (`lootItemName` case 25; antes caía al
default «An item!»). Único productor de kind 0x19: `search_moonstone` 0x03A8 — el canal de
override del banco alto no aplica.

El clon (`game.get`) ya cubre comida de mesa/cosecha (con karma−1), el cofre del
tesoro real y la **antorcha de pared** (carril get-torch: 0xB0/0xB1 → tile 0x44 +
`torchTurns=0x64` + "Borrowed!" + cue `torch-borrowed`; sin +torch, sin karma —
rama 0x19e8-0x1a27 salta directa al exit 0x1b2e). El switch completo de items de
cofre queda para el cableado con objetos cargados (3.13). Sin RNG en overworld.

## 11. Search (SJOG 0x095C) — ⚠️ **CONSUME RNG también en MUNDO** (corregido por #218)

> ★ **CORRECCIÓN #218 (dos defectos en la misma frase).** Esta sección decía
> «determinista, CERO rand ✅ (mundo)» y atribuía el revelado de puerta secreta al
> callee citado abajo. **Las dos cosas son falsas**, y la segunda producía la primera:
> el offset citado no es el revelador, es una rutina de TRAMPA cuya primera
> instrucción útil es una tirada. Derivación completa en `re/notes/rand-218-acta.md` §3.1.
>
> **(1) El revelado NO está en ningún callee: está DENTRO de este mismo cuerpo.** El
> discriminador de #150 (`g_floor >= 0x80`) y las dos escrituras de tile viven en el
> tramo, 0x0b40-0x0b63, de la propia rutina:
>
> | offset | instrucción | qué |
> |---|---|---|
> | 0x0b40 | `cmp byte [0x5895], 0x80` | discriminador de #150, sobre `g_floor` |
> | 0x0b45 | `jae 0x0b58` | ≥0x80 (sótano/Underworld) → rama de mazmorra |
> | 0x0b52 | `mov byte [bx], 0xb9` | rama MUNDO |
> | 0x0b63 | `mov byte [bx], 0xb8` | rama MAZMORRA |
>
> **(2) El callee que sí se llama en la rama de probe `==0x1F` es una TRAMPA, y tira.**
> El call-site es 0x0a63, tras `cmp ax,0x1f` en 0x0a50 y el mensaje DS 0x893e. El
> destino, resuelto por banda, es `SJOG.OVL:0x01f2`, y **su primera acción es
> `rand_range(0,7)`**:
>
> | offset | instrucción | qué |
> |---|---|---|
> | 0x01fd | `call` → kernel 0x2092 | `rand_range(0,7)`, empujado 0 y 7 |
> | 0x0202 | `jne 0x0207` / `jmp 0x028e` | resultado ≠ 0 (7/8) → sale; **la tirada ya está gastada** |
> | 0x021c | `call` → kernel 0x2092 | rama 1/8: `rand_range(0,0x1f)` |
> | 0x021f | `cmp ax, 0x13` | ==0x13 → envenena: `[bx+0x55b3] = 0x50` ('P'), DS 0x8606, tono 0x223c |
> | 0x0256 | `call` → kernel 0x2092 | si no, `rand_range(0,3)` de reparto entre 4 salidas |
>
> Seis llamadas a la primitiva en el cuerpo de esa rutina. La confusión es de dígitos:
> el **valor** de probe `0x1F` y el **offset** del callee `0x01f2` se parecen, y la
> frase antigua los fundió en uno.
>
> **Coste real en el mundo**, por tanto: la rama de probe `==0x1F` gasta **≥1** tirada
> siempre y más en 1 de cada 8 entradas. El resto de ramas de Search (flavor por tile,
> escaneo de objetos) sigue sin tirar. La etiqueta correcta ya no es «CERO rand», es
> «cero rand SALVO la rama de puerta secreta».

**Search es DIRECCIONAL**: `097e: 0x766c` pide dirección; la celda inspeccionada es
**party+dir** (0988-099d), no la del party. Escanea la tabla de objetos 0x5C62
(kind==1 en esa celda/floor) → "Thou dost find"; luego `0a3e: 0x7782(x,y,floor)`
probe; si ==0x1F → mensaje DS 0x893e "a hidden door!", **la trampa de 0x0a63 (con su
tirada, ver recuadro)** y el revelado en 0x0b40-0x0b63 (tile **0x4E → 0xB9 mundo /
0xB8 mazmorra**, según `g_floor≥0x80`); si no, flavor por tile
(0xa5/0x4F/0x2B/…: "Near the well"/"In the fireplace"/…, strings DS 0x8950-0x8a34).

Clon: `revealSecretDoor` + `game.search(dir)` reproduce el reveal en party+dir
(variante mundo 0xB9). ⚠️ **No portado**: flavor por mueble (strings 0x8950-0x8a34)
y el escaneo de objetos ocultos (capa de objetos). La búsqueda "sin dir" del clon
(árbol de Minoc / items de trama / searchObjects) es un añadido propio sobre la
celda actual, no del binario.

### `search_dungeon` (SJOG 0x0646) — ⚠️ PARCIAL (no portado al núcleo)
Search en corredor de mazmorra hace más que revelar: selecciona miembro, exige luz,
`threshold=(floor·2−DEX+0x1E)>>1`, tira `rand(1,30)` de detección de trampa con
mensajes por tramos (0710-075a), revela alijo 0x62 (07c6-07fa), 0x61→0x60 (0782), y
el crumble del muro especial (abajo). El clon NO porta este handler (la mazmorra usa
`DungeonState`); queda ⚠️→formulado.

### Reconciliación del muro secreto de mazmorra (diferida de 3.4 §8) — CERRADA
El clon usa el **nibble hi 0xD** del grid de corredor (`g_dng_map` 0x595A) como
paso secreto revelable por Search; se mantiene (representación NIBBLE del corredor
8×8) y **preserva la conectividad** — ésa es la conclusión que vale. El reveal
`0x4E→0xB9/0xB8` es de **mapa completo** (0xB9 mundo, 0xB8 sala de mazmorra), no del
grid nibble. **Matiz corregido (review)**: el nibble **0xC** (`g_dng_wall_variant`)
NO es sólo cosmético — con variant∉{1,2}, `search_dungeon` lo MUTA a `0xB0|(cell&8)`
("It crumbles away", 0868-089a). O sea, 0xC SÍ es atravesable por Search, pero por
crumble→0xB0, no por 0x4E→0xB9. Ese branch de crumble está sin portar (parte del
`search_dungeon` ⚠️).

## 12. Mix (CMDS 0x1AD8) — el ACIERTO no tira, pero el FALLO sí ✅ (corregido #105)

El éxito depende SÓLO de acertar la receta `[idx+0x1CC0]` (no hay tirada); los
reagentes se gastan siempre; 10 min.

⚠️ **El «0 rand» que decía esta sección era FALSO.** Se contaron los `rand` del CUERPO
de `cmd_mix` y no los de su callee. Si la máscara marcada NO casa con la requerida
(`0x1bd1 cmp ax,[bp-2]` / `jne 0x1bf6`), la rama de fallo **entra en la trampa de §8**:

| offset | instrucción | qué |
|---|---|---|
| 0x1bf6 | `mov ax,0xa` / `call 0x573a` | `putchar('\n')` (kernel 0x16ba) |
| 0x1bfd | `call 0x7a7c` | `party_conscious_state` (kernel 0x39fc): 1er miembro `'G'`/`'P'` → `g_cmb_scratch_x` |
| 0x1c00 | `push [g_cmb_scratch_x]` | el miembro que la come |
| 0x1c04 | `call 0x7050` | **`chest_trap_trigger`** (kernel 0x2fd0) — resuelto por banda, `near_call_base(CMDS.OVL)=0xbf80` |

⇒ **Mezclar con los reagentes equivocados EXPLOTA**, con el mismo despachador que el
cofre. Censo de tiradas real: **1..7**, no 0 y tampoco «1» — 1 para el tipo, más las de
§8 (ACID +1, BOMB +1 por miembro vivo, POISON/GAS +0).

`grep "call 0x2fd0"` da cero en los `.asm`: es notación relativa a la base del overlay
(familia #173). El censo correcto es `re/tools/callers_por_banda.py 0x2fd0`.

Derivación completa, careo con el port y el defecto que deja abierto:
`re/notes/mix-trap-105-acta.md`. El flujo entero (precheck, selector 0x18be, cantidad 0x1a70,
getnum 0x3b9e) está en `re/notes/mix-hf10-command-parity.md` (A3-HF10).

## 13. Yell-en-tierra (CMDS 0x1202/0x12c8) — word-of-power (Task 3.8)

Prompt de palabra (0x7b9c, máx 0xf chars) + búsqueda en tabla DS:0x4502. Efecto
(word-of-power) compartido con 3.8; fuera de este scope.

---

## Ledger de funciones (Task 3.9)

### CMDS.OVL (los NO-3.7 de este scope)
0x0D98 `cmd_ignite_torch` · 0x0DDC `cmd_new_order` · 0x161A `cmd_push`
(+ helpers 0x14BA `is_pushable_tile`, 0x1504 `dir_vector_to_facing`, 0x1548
`slide_object_fwd`, 0x15B0 `swap_object_party`) · 0x1C20 `cmd_klimb_grapple` ·
0x0552 `cmd_camp_holeup` + helper 0x0400 · (Mix 0x1AD8 + subs 0x18BE/0x1A70 =
magic-scope; Yell-tierra 0x1202/0x12c8 = 3.8; 0x17EC `combat_escape_check` =
3.2/3.12). Board/Fire/X-it/Camp-on-ship stub ya en 3.7.

### SJOG.OVL — bloque contiguo 0x0646–0x18CE (14 funcs, `task-3.13-...` ledger)
0x0646 `search_dungeon` · 0x095C `cmd_search` · 0x0BAA `jimmy_chest_obj` ·
0x0C3E `jimmy_dungeon_room` · 0x0D4A `cmd_jimmy` · 0x0F88 `loot_place` · 0x1040
`loot_fixed` · 0x10B8 `loot_random` · 0x112C `open_chest_world` · 0x12D4
`open_dungeon_room` · 0x1374 `cmd_open` · 0x1458 `get_item_switch` · 0x179E
`get_dungeon_room` · 0x18CE `cmd_get`. (Las otras 21 de SJOG son de 3.12.)

### Kernel
0x2FD0 `trap` (= SJOG 0x7050) + primitivas 0x3ABE (ACID) / 0x2A52 (apply_damage)
/ 0x2FA6 (poison) / 0x2AA8 (bomb) / 0x3EF0 (light-source saturating-add).

**Invariante de cobertura**: 202800 (sin cambio; sólo se anotan meanings).

## Globals tocados / resueltos
- `g_torch_mins` 0x58A7, `g_torches` 0x57AE, `g_keys` 0x57AC, `g_karma` 0x5888,
  `g_grapple` 0x57AF, `g_transport_tile` 0x587C, `g_food` 0x57A8, `g_carpets`
  0x57B0, `g_active_char` 0x587B.
- **Resueltos** (antes `pendiente`): `g_unk_24e6` = flag "turno consumido" (los
  handlers lo setean; el dispatcher cobra el turno); `g_unk_a9fa` = flag de
  redraw del panel; `g_unk_594f`(/5950/5951) = tracker de la puerta abierta para
  auto-cierre (+ timer [0x5952]=4); `g_unk_588c` = cooldown de curación de Camp
  (recarga 0x0E, decrementa 1/hora).


> **A4-PARITY2 D-87 (2026-10-03) — closed.** Re-disassembled: the crop `0x2D` and the plate `0x9A` (call `0x1A50`) and the plates `0x9B` / `0x9C` (call `0x1ABE`) all end in `counter_add(&food, 1, 9999)` (ULTIMA.EXE `0x3F14`: `s = int16(old+1)`, stores 9999 when `s >= 9999`, else `old+1`) — it clamps a value above the cap DOWN to 9999, never refuses, and nothing after the call reads its result (the tile, the text and the karma are unconditional). The arena `G` runs the same SJOG routine through thunk `0x7E06` (COMBAT.OVL base `0xA290`, not `0xBFEC`); there is no separate arena routine. Both ports now cap (reference `addWordCapped`, native `std::min(9999, food+1)`); `native/core/a4-parity2-findings/D87-FINAL.md`.
