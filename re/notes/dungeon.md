# Mazmorra — DUNGEON.OVL + DNGLOOK.OVL (Task 3.4)

Reglas re-derivadas del asm con citas instrucción→regla (formato de
`re/notes/combat.md`). Direcciones = offset de fichero dentro del .OVL indicado.
El clon vive en `game/src/core/dungeon/{dungeon,light,index}.ts`; el runner de
paridad en `game/src/core/__parity__/dungeon-run.ts`; arnés en
`re/tools/dungeon_parity.py`; escenarios en `re/parity/dungeon/*.json`.

**Rebase de calls near** (PLINK86, CS único de 64K, ver `command-dispatch.md`):
`true_CS = (listing_target + load_seg*16) & 0xFFFF`. DUNGEON load_seg*16 =
`0x81D0`, DNGLOOK = `0xA290`. `rand(lo,hi)` = kernel 0x2092 (inclusive,
`re/notes/rng.md`). `party_random_damage` = kernel 0x2AA8 = `apply_damage(i,
rand(1,8))` a cada miembro vivo. `apply_damage` = kernel 0x2A52.

Estado de verificación: **asm-derivado + cruce modelo↔clon** (KernelRng Python vs
`OriginalRng` TS calcan el stream y el estado final en 12 escenarios). Paridad
runtime contra DOSBox: **divergencia de alcance documentada** (ver
`re/verified/dungeon.md`), como el movimiento de IA en 3.2.

---

## 0. Modelo de datos

### 0.1 Mapa de nivel — `g_dng_map` @ DS:0x595A (512 B, en SAVED.GAM)
8 plantas de 8×8, 512 B contiguos:
```
tile(x,y,floor) = [DS:0x595A + (floor<<6) + (y<<3) + x]   ; x,y ∈ 0..7
```
Cita (movimiento, DUNGEON:0x05c6): `bl=g_floor; shl bx,6; al=[bx+si+0x595a]` con
`si=(y<<3)+x`. Repetida idénticamente en ≥12 sitios (0x074c, 0x09c9, 0x0a93,
0x0e4d, 0x10f6, 0x1c1b, DNGLOOK 0x0056…). Nibble ALTO = tipo; nibble bajo =
subtipo. Está en la ventana SAVED.GAM (roster_off + 0x3B4) → se guarda.

### 0.2 Diccionario de tiles (por nibble ALTO, del comando Look DNGLOOK:0x0000)

| hi | tipo (clon `CellType`) | "You see:" | notas |
|----|------------------------|------------|-------|
| 0x0 | Nothing (pasillo) | "a passage." | transitable |
| 0x1 | LadderUp | "an up ladder." | Klimb up |
| 0x2 | LadderDown | "a down ladder." | Klimb down |
| 0x3 | LadderUpDown | "a ladder." | Klimb up/down |
| 0x4 | Chest | "a wooden chest." | Get/Open → 0x7 (SJOG, Task 3.9) |
| 0x5 | Fountain | "a fountain." | low nibble = efecto (§6) |
| 0x6 | Trap/Pit | "a pit." | 0x60 hoyo, 0x61/0x69 caída, 0x62/0x6A bomba (§5) |
| 0x7 | OpenChest | "an open chest." | |
| 0x8 | MagicField | sleep/poison/fire/energy (§5) | low nibble & 7 = tipo; bit 0x8 = iluminado |
| 0x9 | Marker | "nothing of note." | tile "consumido"/oculto |
| 0xA | Room | "a heavy door." | entra a combate de sala (§7) |
| 0xB | Wall | "a wall." | bloquea |
| 0xC | SpecialWall | estalactita(1)/derrumbe(2)/… | secreto; `g_dng_wall_variant` (§8) |
| 0xD | Wall/SecretDoor | "a wall." | bloquea; el clon usa 0xD para el paso secreto revelable (⚠ nibble, §8) |
| 0xE | NormalDoor | "a heavy door." | transitable |
| 0xF | Room | "a heavy door." | entra a combate de sala (§7) |

Nota Look: `0x61` se muestra como `0x00` ("a passage") — el foso oculto no se
delata (`DNGLOOK:0x0070 cmp bp-a,0x61; jne; mov bp-a,0`).

### 0.3 Orientación y vectores
`g_dng_facing` @ DS:0x6603: **0=N, 1=E, 2=S, 3=O** (0x01D2 imprime
North/East/South/West según 0..3). `dx[0..3]@0x24D6 = [0,+1,0,-1]`,
`dy[0..3]@0x24DE = [-1,0,+1,0]` (mismas tablas que overworld/combat/CAST).
`g_dng_anim_dir` @ DS:0x6602 (0..5) = dir de última animación; a facing con
`DS:0x2C76=[0,1,2,3,0,2]` (0x000f). `g_dng_wall_variant` @ DS:0x6604 (1/2/3).
(Los tres renombrados en `globals.json` desde `g_unk_66xx`.)

---

## 1. Bucle de mazmorra — DUNGEON:0x0E2E (stub 0x7A16)
Dispatcher de teclas (DUNGEON:0x06C4): flechas → 0x0502 move(dir); 0x0D/0x2E →
advance; '0'..'9' → selección de PJ (kernel 0x4080, NO gasta turno); resto →
kernel_cmd_dispatch 0x3178. Comandos que reentran: Attack→0x1D4A, Klimb→0x1E10,
Look→DNGLOOK:0x0000, View gem→DNGLOOK:0x06A8, Uus/Des Por (CAST)→0x1C6A.

**Coste de tiempo** (#159, DERIVADO — corrige lo que decía esta línea): el paso
normal cuesta **1 minuto**, igual que TOWN. La lectura anterior («NO hay
`advance_clock(n)` por paso normal; el reloj sólo avanza vía eventos») nació de
censar los `call` del overlay sin seguir el `jmp`: DUNGEON tiene UN solo
`advance_clock`, el de 0x0F2F, y está físicamente dentro del bloque de Quickness
— pero el camino SIN hechizo temporal SALTA A ESE MISMO `push`/`call` en vez de
tener uno propio. Cola del bucle:

| offset | mnemónico | lectura |
|---|---|---|
| `0x0FD0` | `cmp [bp-0xe],0` / `jne 0xf93` | sólo se cobra si el despachador devolvió 0 |
| `0x0FD6` | `cmp [g_time_spell],0x54` / `je 0xf1e` | 'T' An Tym |
| `0x0FE0` | `cmp [g_time_spell],0x51` / `je 0xf1e` | 'Q' Rel Tym |
| `0x0FEA` | `mov di,1` | sin hechizo: di = 1 |
| `0x0FED` | `mov ax,di` | ax = 1 = el argumento |
| `0x0FEF` | `jmp 0xf2e` | **entra en el bloque de 'Q', en su `push ax`** |
| `0x0F2E` | `push ax` | |
| `0x0F2F` | `call 0x4F7C` | `advance_clock(1)` |

Salto verificado byte a byte: `e9 3c ff` en 0x0FEF ⇒ 0x0FF2 + (−196) = 0x0F2E. El
destino del `call` es advance_clock por CONTROL POSITIVO, no por su etiqueta (que
en overlay es file-relativa): TOWN 0x15D4 y MAINOUT 0x0C3D — los dos
`advance_clock` ya bautizados — rinden el mismo `call 0xffffcdac` byte a byte.

Los otros dos regímenes: 'Q' (Rel Tym) cobra 1 minuto **cada dos** turnos
(0x0F25 `xor di,1` / 0x0F29 `je 0xf36`); 'T' (An Tym) cae en 0x0F34 `sub di,di` y
NO llama. El housekeeping va después (0x0F84 → 0x0C76), como en MAINOUT.

El clon cobraba 2 (pasaba el coste sin fijar y caía en la rama de EXTERIOR, porque
deja `position.location` en 0 bajo tierra); corregido en los CINCO call-sites de
mazmorra. Ver `survival.ts::MINUTES_PER_ACTION_DUNGEON`.

---

## 2. Movimiento — DUNGEON:0x0502 `move(key,dir)` [clon `step`]
Interpretación RELATIVA al facing: dir 3 (↑)=Advance (+1), 4 (↓)=Back up (−1),
2 (→)=Turn right, 1 (←)=Turn left, else Turn around. Girar puro no gasta paso.
Destino con **WRAP toroidal por eje** (NO clamp/bloqueo): `0x057a or ax,ax; jge
0x583; mov [bp-6],7` (x=−1 → 7) y `0x0583 cmp [bp-6],7; jle; mov [bp-6],0` (x=8 →
0); idéntico para el eje y. Salir por un borde te lleva al lado opuesto de la
misma fila/columna. Clasificación tras el wrap (0x05FF):
- tile == 0x83 (energía) → mueve al destino y dispara electric field (0x0470),
  que **rebota** de vuelta → posición neta = origen (§5a). El clon lo modela como
  "no entra + daño".
- hi ∈ {0xB,0xC,0xD} → **"Blocked!"**.
- retroceso sobre hi ∈ {0xA,0xF} (sala a la espalda) → "Blocked!".
- resto pasable → turno del monstruo (0x1020) + mover party + on_enter.

---

## 3. Render y LUZ — DUNGEON:0x1A90 / 0x1BE0 / 0x111E
**Gate de luz (0x1AD6)**: `si g_light_spell_mins==0 Y g_torch_mins==0 → jmp
0x1BBE` (salta TODO el raycast → **oscuridad total**). Con luz, raytraza 4 celdas
de profundidad (0x150A/0x1682/0x1952). Antorcha en mazmorra: `g_torch_mins +=
rand(0,15)+0x70` (112..127 min, ya exacto de 3.1, `kernel-survival.md`).
**Consumo RNG (0x111E)**: salvo Time-stop (g_time_spell==0x54), cada frame de
vista tira **2×rand(0,100)** (0x1181/0x119A, umbral 0x32) para el flicker del
tile lejano. El clon (núcleo puro, sin render por-frame) **NO** modela el flicker
→ divergencia de alcance del stream declarada (igual que idle en survival §2).

---

## 4. Escaleras y salida (floor 0 = nivel 1 arriba, floor 7 = nivel 8 abajo)

### 4.1 `change_level(dir,mode)` — DUNGEON:0x1C6A
`dir>0` baja, `≤0` sube. Tope (floor 7 abajo / floor 0 arriba) → return 1 (Klimb
debe SALIR). Si `dng_landing_ok`==0 → "Failed!" (no cambia). Si OK → g_floor+=dir.

### 4.2 `dng_landing_ok(newfloor,mode)` — DUNGEON:0x1C0C
`mode!=0` y **nibble ALTO ≠ 0** → 0 (bloquea). Con mode==0 (Klimb normal)
siempre OK.
> **Corrección (barrido #39, lectura del cuerpo 0x1C0C):** el `or ax,ax; jne`
> en 0x1C42 precede a los cuatro `cmp 0xE0/0xD0/0xC0/0xB0`, así que en mode≠0 la
> celda se bloquea ante CUALQUIER nibble-alto ≠ 0 (aterrizaje sólo en pasadizo
> vacío hi==0) — los cuatro cmp son código muerto. No es la lista {0xB,0xC,0xD,0xE}.
> Corrobora el texto del ítem Uus/Des Por ("empty corridor to empty corridor",
> InventoryDetails.json:636). Ver [[inferible-sweep-f2]].

### 4.3 `exit_dungeon()` — DUNGEON:0x1D08
`party_x=[g_location+0x1E89]`, `party_y=[g_location+0x1EB1]` (tablas de salida).
Si g_floor != 0 → **Underworld** (g_floor=0xFF, "Exit to Underworld!"). Si
g_floor == 0 → **Britannia** (g_floor=0, "Exit to Britannia!"). g_location=0.
**Regla clave**: salir por la planta superior → superficie; por cualquier otra →
Underworld.

### 4.4 `Klimb` — DUNGEON:0x1E10
UP disponible si tilehi ∈ {0x1,0x3} ó (bit8 && g_grapple≠0). DOWN si tilehi ∈
{0x2,0x3,0x6} (los hoyos permiten bajar). Al tope → 0x1D08 (SALIR). El binario
**NO** tiene "trampa de escalera": subir/bajar no daña (corrige la invención del
clon previo). `g_grapple` @ DS:0x57AF permite up sobre celda ILUMINADA (bit 0x08)
sin escalera (**PORTADO**, tarea #38: `dungeon.ts klimb`, deriv. completa +
strings de fallo 0x6cd6/0x6cea en `klimb-grapple.md §4/§4.1`). Uus Por
(CAST:0x0FD2) / Des Por (CAST:0x0FFC) llaman a 0x1C6A(∓1), **NO** a 0x1D08 exit
(corrige el draft 3.3).

---

## 5. Trampas y campos — despachador on_enter DUNGEON:0x0C76
Secuencia exacta:
1. **Despertar dormidos** (0x0C94): por cada PJ con status 'S', `rand(0,63)`; si
   `< 4` → 'G'. ⇒ **1/16 por turno por miembro dormido**.
2. mover monstruo (§9, no portado).
3. clasificar el tile bajo la party:

| tile | efecto | rutina | regla exacta |
|------|--------|--------|--------------|
| 0x80/0x88 | campo de sueño | 0x0948 | por miembro: `rand(1,30) ≥ DEX` → 'S'; **limpia** (`&=8` → suelo) |
| 0x81/0x89 | campo de veneno | 0x09E6 | por miembro: `rand(1,30) ≥ DEX` → 'P'; **no limpia** |
| 0x82/0x8A | muro de fuego | 0x0DBC | "Fire!!" + `party_random_damage` (rand(1,8) c/u) |
| 0x83 | campo de energía | 0x0470 | "Electric field!" + rebote + rand(1,8) c/u (§5a); bloquea |
| 0x61/0x69 | foso de caída | 0x0A4C | caída encadenada (§5c) |
| 0x62/0x6A | bomba | 0x0DC8 | "Bomb Trap!" + rand(1,8) c/u + **limpia** (`&=8`) |
| 0x60/0x68 | hoyo simple | — | "a pit.", sin efecto |
| hi 0xA/0xF | sala | 0x0000 | combate de sala (§7) |

DEX = `[0x55B5 + i*0x20]`. El contest usa **≥** (`aflige si rand(1,30) ≥ DEX`),
un tile más peligroso que el veneno de pantano (que usa `>` estricto,
`kernel-survival.md §6`). La tirada `rand(1,30)` está al **TOPE del bucle**
(0x0970 sueño / 0x0a09 veneno) → **la consume CADA miembro, incluidos los
muertos**; el check 'D' (0x0986/0x0a1c) sólo salta la ESCRITURA del status, no la
tirada. (`party_random_damage` en cambio tira sólo por miembro VIVO, 0x2AA8.)
Orden REAL de campos por low nibble: **0=sueño(In Zu), 1=veneno(In Nox),
2=fuego(In Flam), 3=energía(In Sanct)** — coincide con los muros de CAST (draft
3.3 §2d). Sueño/veneno/fuego reconocen la variante iluminada (+0x08: 0x88/0x89/
0x8A); **la energía NO** — sólo dispara el tile EXACTO 0x83 (0x05d7), no 0x8B.

### 5a. Campo de energía — DUNGEON:0x0470
"Ouch!"+"Electric field!", **rebota al party un paso** (net = origen) +
`party_random_damage`. El clon: no entra, daño a cada miembro.

### 5c. Foso de caída — DUNGEON:0x0A4C
```
si tile ∈ {0x61,0x69} y g_floor<8:
   "Pit Trap!" + "Falling..."; and tile_actual,0xF8 (limpia subtipo → 0x60)
   g_floor++; render; "...splat!" + party_random_damage (rand(1,8) c/u)
   si el nuevo tile también es foso → SIGUE cayendo (encadena)
al parar: si g_floor==8 → g_location=0 (sale al Underworld);
          si el tile es sala → combate.
```
**1 nivel por foso, rand(1,8) por caída a cada miembro, encadena.**

---

## 6. Fuentes — beber (DNGLOOK:0x0000, rama 0x50)
Al mirar una fuente → "Will you drink?"; 'Y' → efecto por nibble bajo sobre el
**PJ seleccionado/activo** (kernel 0x4988):

| tile | efecto | regla |
|------|--------|-------|
| 0x50 | Curar veneno | "Cured!"; status='G' |
| 0x51 | Curación total | "Healed!"; HP=maxHP |
| 0x52 | Envenenar | "Poisoned!"; status='P' |
| 0x53 (resto) | Daño | "Bad taste."; `apply_damage(slot, rand(0,7))` |

El switch es **exact-match del tile COMPLETO** (0x50/0x51/0x52, else→daño), NO
`sub&3`: un tile 0x59 (fuente heal con bit iluminado) cae en el `else` → "Bad
taste" (daño), no cura. El veneno (0x52) escribe 'P' **incondicional** (0x025e),
sin comprobar el estado previo. Correcciones al clon previo (divergencias
#2,#3,#4): Heal = HP total (no rand); BadTaste = rand(0,7) (incluye 0); efecto por
tile exacto; objetivo = PJ activo/seleccionado.

---

## 7. Salas → combate — DUNGEON:0x0000
Tiles hi 0xA/0xF: "Entering room...", `room = tile & 0x0F`,
`offset = dungIdx*0x1600 + room*0x160` en DUNGEON.CBT (kernel 0x256E lee 0x160 B).
Al despejar: `and tile, 0xAF`. El clon: `roomCombatMapIndex(dungeon, tile&0xF)`
(orden que salta Despise). Combate en sí = Task 3.3.

---

## 8. Muro especial 0xC0 [parcial]
Look 0xC0 según `g_dng_wall_variant & 0x0F`: 1→"a dripping stalactite.",
2→"a caved in passage.", si no → `rand(0,255)`; ==0xFF → "an unfortunate software
pirate." (easter egg), resto → "a less fortunate adventurer.". Bloquea el paso.
**RECONCILIADO (Task 3.9, `re/notes/cmds.md §11`)**: el reveal de Search vive en
SJOG (0x095C/0x0646). El nibble hi **0xD** del corredor 8×8 = paso secreto
revelable (se mantiene en el clon, representación NIBBLE — preserva la
conectividad). El reveal `0x4E→0xB9 (mundo) / 0xB8 (sala de mazmorra)` es de
**mapa completo**, no del grid nibble. **Matiz (review)**: el nibble **0xC**
(`g_dng_wall_variant`) NO es sólo cosmético — `search_dungeon` (0x0646), con
variant∉{1,2}, lo MUTA a `0xB0|(cell&8)` ("It crumbles away", 0868-089a): 0xC SÍ es
atravesable por Search, por crumble→0xB0. Ese branch (parte de `search_dungeon`)
está **sin portar** (⚠️→formulado); la conclusión de conectividad se mantiene. ⚠
de 3.4 §8 cerrada; el detalle de crumble sigue en el ledger de 3.9.

---

## 9. Monstruo errante [PORTADO — carril monster-3d; derivación completa en `dungeon-wanderer.md`]
- **Spawn** (0x0252): 8 intentos `off=rand(0,63)`; permitido si hi<0x6 ó hi==0x7,
  no sobre el party. Tipos 0x16/0x18: `rand(0,99)>0x30` → desactiva.
- **Mover** (0x07E2): `rand(0,3)` dir; si destino==party, `rand(0,7)==1` → ataca.
- **Emboscada** (0x0B7E): "Attacked from the <dir>" + entra al ataque.
PORTADO calcado (core/dungeon/wanderer.ts + dungeon.ts; combate de pasillo procedural
vía DNGLOOK 0x0D3E/0x117E). Derivación completa y tablas: `re/notes/dungeon-wanderer.md`.

**Pendiente (on_enter desde el bucle)**: el binario llama a on_enter (0x0C76) y
al despertar de dormidos desde el bucle principal tras CUALQUIER acción que
consuma turno (DUNGEON:0x0F84, con gate `si≠0`), no sólo al pisar una celda. El
clon sólo corre wake/on_enter en el movimiento (`step`/`klimb`), así que un
comando que gasta turno sin moverse (p.ej. atacar al aire) no rola el despertar.
Divergencia de alcance a portar cuando se integre el bucle completo.

---

## 10. Attack en mazmorra — DUNGEON:0x1D4A [asm-derivado]
Objetivo = `party + dx/dy[facing]`. Si == celda del monstruo → golpe (kernel
0x5F86). Casos especiales por tipo (g_unk_58A0): ==5 → g_floor--; ==6 → g_floor++
(⚠ semántica a verificar). Sin monstruo → "What?".

---

## 11. Look / View gem (DNGLOOK)
- **Look** 0x0000: sin luz → "darkness."; con luz describe el tile (§0.2);
  fuentes → drink (§6); muro 0xC0 → variante (§8, consume `rand(0,255)`).
- **View gem** 0x06A8: dibuja el mapa 8×8 del nivel; consume 1 gema (g_gems
  DS:0x57AD). Sólo render.

---

## 12. Divergencias con el clon previo — resueltas (checklist de 11)
1. Campo de energía: `rand(1,8)` a CADA miembro + rebote; sólo tile EXACTO 0x83
   (antes: hurtFirst rand(5,15) y sub&7==3 aceptaba 0x8B). ✅
2. Fuente Heal: HP=maxHP (antes rand(3,10)). ✅
3. Fuente BadTaste: rand(0,7) (antes rand(1,10)). ✅
4. Fuentes por **tile exacto** sobre PJ activo; veneno 'P' incondicional (antes
   enum + `sub&3` + all-party + `==G`). ✅
5. Trampa de escalera ELIMINADA (no existe en el binario). ✅
6. Campos sueño/veneno: orden 0=sueño,1=veneno,2=fuego,3=energía + contest DEX
   (antes orden cambiado + daño plano). ✅
7. Fuego/bomba = `party_random_damage`; bomba limpia el tile. ✅
8. Foso encadenado 1 nivel/foso, rand(1,8) c/u, sale por el fondo. ✅
9. Salida Klimb: floor 0 up→Britannia; floor 7 down→Underworld. ✅ (ya presente)
10. Despertar dormidos `rand(0,63)<4` por turno. ✅
11. RNG del render (2×rand/frame + rand(0,255) de Look-0xC0): EXCLUIDO del núcleo
    puro con justificación (divergencia de alcance de stream). ✅ documentado

### Fixes de review (re-derivación independiente)
- **Bordes = WRAP toroidal** (§2, 0x057a/0x0583), no clamp/bloqueo: el clon
  bloqueaba donde el binario te lleva al lado opuesto. ✅ (escenario `border-wrap`)
- **Contest DEX tira también por los muertos** (§5, 0x0970/0x0a09): la tirada al
  tope del bucle la consume cada miembro; sólo la escritura del status salta a los
  'D'. ✅ (escenario `contest-dead-consumes-roll`)

---

## 13. Mapa de funciones (ledger)
### DUNGEON.OVL
0x0000 dng_enter_room · 0x0134 dng_after_room · 0x01D2 dng_draw_panel · 0x0252
dng_spawn_wanderer · 0x03D6 dng_getkey · 0x0470 dng_electric_field · 0x0502
dng_move · 0x06C4 dng_dispatch_key · 0x07E2 dng_move_wanderer · 0x0948
dng_field_sleep · 0x09E6 dng_field_poison · 0x0A4C dng_pit_fall · 0x0B7E
dng_ambush · 0x0C76 dng_on_enter_cell · 0x0DBC dng_field_fire · 0x0DC8
dng_bomb · 0x0E2E dng_main_loop · 0x10DC dng_get_tile · 0x111E
dng_calc_forward_tile · 0x1A90 dng_render_corridor · 0x1BE0 dng_redraw · 0x1C0C
dng_landing_ok · 0x1C6A dng_change_level · 0x1D08 dng_exit · 0x1D4A dng_attack ·
0x1E10 dng_klimb · 0x1020 dng_monster_turn.
### DNGLOOK.OVL
0x0000 dnglook_look · 0x06A8 dnglook_view_gem · 0x0284/0x0340/0x0844… render
helpers.

**Globals nuevos** (`globals.json`): `g_dng_map@0x595A` (512 B),
`g_dng_facing@0x6603`, `g_dng_anim_dir@0x6602`, `g_dng_wall_variant@0x6604`.

---

## 14. Bitmap de SALAS despejadas — `g_dng_room_cleared` @ DS:0x58E0 (task #41, PORTADO)

El original recuerda qué SALAS de mazmorra ya despejaste para no re-pelearlas. **No es
"celda visitada" per-celda** (el rótulo previo en top21-triage/inferible-sweep era
impreciso): es **1 bit por (mazmorra, nº de sala)**. Vive en la ventana SAVED.GAM
(**save offset 0x33A**; base roster 0x55A6 ⇒ 0x58E0−0x55A6=0x33A), 14 bytes = 7
mazmorras × 16 salas. Contiguo tras `g_shrine_destroyed` (0x332,8B) y antes de
`g_dng_map` (0x3B4).

### 14.1 Índice de bit — DNGLOOK 0x0844 (set) / 0x08d4 (test)
`dungIdx = g_location − 0x21` y **`if (dungIdx ≥ 1) dungIdx--`**; bit = `(dungIdx<<4) +
roomNumber` (LSB-first: `al=1; cl=idx&7; shl al,cl; or [bx+0x58e0],al`, 0x08bf-0x08c9).
⚠ **Quirk**: la fórmula COLAPSA Deceit (0x21) y Despise (0x22) al MISMO `dungIdx` 0.
Replicado tal cual para calco byte.

**Derivación VERIFICADA contra el disasm** (reconciliación 2026-07-27, tarea #22 — leída
de `re/disasm/DNGLOOK.OVL.asm`, no de memoria): `0891 sub ax,0x21` · `0897 cmp ax,1` /
`089a jl` / `089c dec` (⇒ el `--` sólo con `idx ≥ 1`) · `08a2 shl ax,4` + `08a6 add
[bp+4]` · byte = `bitIndex>>3` por el idiom `cdq/xor/sub/sar 3` (0x08ac-0x08ba) · set
LSB-first en 0x08bf-0x08c9. El test 0x08d4 abre con el MISMO prólogo (0x08df-0x08ea).
Matiz de precisión: el `& 0xf` de `roomNumber` **no está en el cuerpo de 0x0844** — 0x08a6
suma el argumento crudo; quien enmascara es el LLAMADOR (DUNGEON 0x0024, `and ax,0xf`).
El port lo enmascara además dentro de `dungeonClearedBitIndex`, que es equivalente
mientras el llamador pase un nibble.

#### 14.1.1 ★ RECONCILIACIÓN: el binario tiene UN colapso, no dos
La redacción anterior decía que este colapso era «distinto del colapso de
`roomCombatMapIndex` (que es Despise≡Destard)», dando a entender que en el ORIGINAL
convivían dos indexados. **Es falso, y así se corrige:**

- **El original usa la MISMA fórmula en los dos sitios.** `dng_enter_room`
  (DUNGEON.OVL **0x003a-0x005b**) calcula el offset en `DUNGEON.CBT` con *exactamente*
  `loc−0x21` + `if (idx ≥ 1) idx--` (`003d sub ax,0x21` · `0043 cmp ax,1` / `0046 jl` /
  `0048 dec`) y luego `imul 0x1600` (mazmorra) + `imul 0x160` (sala). O sea: el colapso
  **Deceit≡Despise gobierna TAMBIÉN el mapa de combate de la sala**, no sólo el bitmap.
- **`Despise≡Destard` NO existe en el binario: lo introduce el PORT.** Sale de
  `dungeonOrderSkippingDespise` (`game/src/core/dungeon/dungeon.ts:1516`, `order = d−33;
  if (d > 34) order--`), que es **procedencia REDUX SIN DERIVAR** — una re-derivación
  «saltar Despise» que nadie ha respaldado con el binario. La fórmula derivada MANDA.
- **Las dos coinciden en las 7 mazmorras que tienen salas** (Deceit 0, Destard 1, Wrong 2,
  Covetous 3, Shame 4, Hythloth 5, Doom 6) y **divergen SÓLO en Despise**: derivada → 0
  (bloque de Deceit), Redux → 1 (bloque de Destard).
- **Inerte, y verificado por los datos, no por memoria:** Despise tiene **0 celdas de
  sala** — recuento sobre `game/assets/maps/dungeons.json`, los 8 pisos × 512 celdas de
  cada mazmorra: `type 0xF` = Deceit 16 · **Despise 0** · Destard 16 · Wrong 36 ·
  Covetous 82 · Shame 16 · Hythloth 16 · Doom 16 (`0xA` = 0 en todas). Sin celda de sala
  no hay evento `combat-room`, así que ningún camino vivo pasa `34` por la fórmula.

**Consumidores** (censo del 27-07): la fórmula Redux la consume el port en
`dungeon.ts:1186` y `dungeon.ts:1287` (construcción del evento `combat-room`) y
`dungeon-cmds.ts:284-285` (`startDungeonRoomCombat`). La derivada la consumen
`dungeonRoomCleared` / `dungeonMarkRoomCleared`. **Divergencia LATENTE declarada, NO
arreglada** (radio de sellos primero): si algún día una celda de sala aparece en Despise
—o alguien llama a `roomCombatMapIndex(34, r)` desde un deep-link o un test— el port
cargaría el bloque de Destard donde el original carga el de Deceit.

⚠ Ojo con la cita: el JSDoc de `roomCombatMapIndex` invoca «DUNGEON:0x0000, offset =
dungIdx*0x1600 + room*0x160». La ARITMÉTICA del offset es correcta (0x1600/0x160
verificados en 0x004b-0x0059), pero la función **no implementa el `dungIdx` de 0x0000**:
cita una rutina que no sigue.

#### 14.1.2 ★ Guarda a la cabeza de 0x0844 — **DERIVADA** (tarea #33, 2026-07-27)

Antes de tocar el bitmap, 0x0844 hace una búsqueda que puede **salir sin marcar nada**
(0x0850-0x0887): cuenta en `DS:0x3840`, tabla de bytes en `DS:0x383a`, y clave
`((g_location & 0xF) << 4) + roomNumber`. Si la clave está en la tabla, salta a 0x08cd —
el epílogo — **sin ejecutar el `or [bx+0x58e0],al`**.

**LA TABLA, EN FRÍO.** No la escribe nadie **porque es DATO ESTÁTICO de DGROUP**: las dos
únicas referencias en todo el corpus de desensamblado (28 `.asm`, overlays + residente +
drivers) son las DOS LECTURAS de esta rutina.

> **El «nadie la escribe» está comprobado por DOS vías, y la segunda casi se me escapa**
> (regla del repo: *un cero silencioso no es refutación; dos ceros sí son evidencia*).
> (1) Barrido por **hex y por columna de encoding** — el listado concatena los bytes
> (`0872: 8a843a38`), así que cualquier instrucción que codifique `3a 38` / `40 38`
> consecutivos aparece: **2 hits, las 2 lecturas**. (2) ⚠ El desensamblador **SÍ emite
> `símbolo+offset`, pero en DECIMAL** (`[g_party_records+511]`, `[g_char_anim_states+10]`),
> de modo que un grep de `g_xxx+0x…` da **cero en falso**. Resueltos los **157** símbolos de
> `globals.json` y sumado el offset decimal de TODA referencia `[g_sym+N]` de los 28 `.asm`:
> **0 hits en 0x383a/0x3840 y 0 en toda la ventana 0x3820-0x3860.** Sólo con las dos vías, y
> con la evidencia POSITIVA de abajo (la imagen estática ya contiene la tabla coherente),
> el «es dato estático» se sostiene.

Sale de `DATA.OVL`, que se carga en DGROUP con delta **+0x10**
(`DS:X ↔ DATA.OVL fileoff X+0x10`):

```
DATA.OVL 0x384a  (= DS:0x383a)  50 5b 41 46 4b 4c    ← la tabla, 6 bytes
DATA.OVL 0x3850  (= DS:0x3840)  06                   ← la cuenta, contigua
```

La cuenta **6** y los **6 bytes** que la preceden encajan exactamente: tabla y contador
son un solo objeto de datos. **Mapeo VALIDADO de forma independiente**: por el mismo delta,
`DS:0x54d4` da `70 0c 00 28 01 f3 00 bd 72 3f ff ff ff cf ff ff`, **byte a byte igual** al
volcado del bitmap de pasabilidad ya publicado en `lote-D-witnesses-relevo.md:14`.

**LAS 6 CLAVES, DECODIFICADAS.** Con `loc&0xF` (Deceit 0x21→1 … Doom 0x28→8):

| clave | `loc&0xF` | mazmorra | sala | combatmap |
|---|---|---|---|---|
| `0x41` | 4 | **Wrong** (loc 36) | 1 | cm49 |
| `0x46` | 4 | **Wrong** | 6 | cm54 |
| `0x4b` | 4 | **Wrong** | 11 | cm59 |
| `0x4c` | 4 | **Wrong** | 12 | cm60 |
| `0x50` | 5 | **Covetous** (loc 37) | 0 | **cm64** |
| `0x5b` | 5 | **Covetous** | 11 | cm75 |

**SEMÁNTICA.** El llamador (`dng_enter_room` DUNGEON 0x00de) sólo llega aquí **después** de
que COMBAT 0xB94 devuelva 0, o sea con la sala YA peleada ⇒ la guarda no dice «invencible»,
dice **«no lo apuntes»**. Y el reparto es fino: `00f5` hace `g_dng_map[…] &= 0xAF` (0xF→0xA)
**inmediatamente después y SIN guarda**, luego dentro de la visita la sala sí queda
consumida; lo que se salta es el **bit PERSISTENTE** `0x58e0`. Como 0x093a sólo re-aplica el
`&0xAF` al recargar el mapa fresco de `DUNGEON.DAT` **para los tiles cuyo bit está puesto**,
el efecto neto es:

> ★ **Las 6 salas SE RE-ARMAN entre visitas a la mazmorra.** Se pelean, se limpian para esa
> visita, y vuelven a estar armadas la próxima vez que se carga el mapa. Son las únicas 6
> re-jugables por diseño de las 112 del juego.

Detalle de compilador que confirma la forma: `[bp-4]` (el índice del bucle) se escribe en
las dos salidas —0x087c encontrado, 0x0889 no encontrado— y **no se lee jamás**; `ret 2` no
devuelve nada útil. Es el `for (i=0; i<n; i++) if (t[i]==key) return;` clásico, con la `i`
muerta.

**POR QUÉ ESAS 6: NO derivable de los datos, y lo he intentado.** Es una lista de
excepciones **escrita a mano**, no computada:
- No es «las salas con la familia 0xEC»: 0xEC aparece SÓLO en Wrong y Covetous (lo que sí
  explica que las 6 claves caigan justo en esas dos mazmorras), pero **Wrong r11/r12 y
  Covetous r11 tienen CERO bytes 0xEC**, y Wrong r2/r3/r4/r5 tienen 3-6 y no están exentas.
- No es un tile especial: la intersección de bytes presentes en las 6 no contiene nada
  distintivo (el más raro, `0x4f`, sale igualmente en el 39 % de las 106 no exentas).
- Tampoco es «las inganables»: **Covetous r0 y r11 el port las mide DEADEND, pero Wrong r1
  la mide VICTORY** (ch23) ⇒ la correlación con el outcome NO existe.

**CONSECUENCIA PARA EL PORT: hueco REAL, pequeño y declarado.**
`dungeonMarkRoomCleared` (`dungeon.ts:1558`) marca **siempre** — no hay guarda. ⇒ el port
**pierde las 6 salas re-jugables**: las sella para siempre en la primera victoria, mientras
el original las devuelve. Es divergencia de comportamiento observable (re-farmeo de XP/botín
y el tile de la sala al reentrar), no de presentación. **NO se cablea aquí**: es cambio de
mecánica con radio de sellos (cm49/cm54/cm59/cm60/**cm64**/cm75 tocan los carriles de Wrong
y Covetous) ⇒ decisión del lead.

⚠ Y confirma lo que la §14.1.2 vieja intuía: **dentro de la MISMA rutina conviven dos
indexados** — la clave de la guarda usa `loc & 0xF` y **NO colapsa** (Deceit→0x1n,
Despise→0x2n), mientras el bitmap de abajo usa el `dungIdx` colapsado de §14.1. No es un
error: son dos espacios distintos, y sólo el segundo colapsa.

### 14.2 Escritura y aplicación
- **`dng_enter_room` (0x0000)**: tras COMBAT.OVL 0xB94 (stub 0x7c32) devolver 0 (sala
  peleada), en **0x00de** llama DNGLOOK **0x0844** `mark_cleared(roomNumber)` [stub
  0x7c62 → DNGLOOK 0x0844, resuelto con `dispatch_table.stubs()`], y en **0x00f5** hace
  `g_dng_map[floor][y][x] &= 0xAF` → 0xFn→0xAn EN VIVO (persistido en el .GAM).
- **`0x093a`** (al cargar el mapa fresco de DUNGEON.DAT): recorre los 512 bytes; por cada
  tile hi==0xF con el bit puesto → `&0xAF` (0xF→0xA). Es el equivalente de "recordar" las
  salas despejadas entre entradas a la mazmorra.

### 14.3 CORRECCIÓN a `top21-triage §2` / `inferible-sweep-f1 §1`
Afirmaban que "0xA no dispara combate". **Falso** por lectura directa: en `on_enter`
(0x0d17-0x0d40) y en el clasificador de movimiento (0x05ff), tanto 0xF como 0xA
AVANZAN→`dng_enter_room`; el único gate ahí es `kernel 0x39fc` (¿party G/P/S puede
pelear?). La no-repetición la decide **COMBAT 0xB94** (una sala ya despejada no coloca
monstruos). Para el clon MODAL, ambas lecturas convergen: **una sala despejada NO lanza
combate**. Cita de resolución de stubs: `re/notes/overlay-load-layout.md §3`.

### 14.4 Port (clon)
`game/src/core/dungeon/dungeon.ts`: `dungeonClearedBitIndex/dungeonRoomCleared/
dungeonMarkRoomCleared` + `DungeonState.applyClearedRooms` (0x093a) y
`markCurrentRoomCleared` (0x00de+0x00f5). `game.ts`: `applyClearedRooms` al entrar,
`markCurrentRoomCleared` en victoria de sala (`endCombat`). Persistencia nativa @0x33A en
`saveNative.ts` (write+read, round-trip). `onEnterCell`/`pitFall` suprimen `combat-room`
para salas despejadas. La gema 8×8 y la vista ya pintan 0xA como sala (glyph puerta 0x73,
`dnglook-raster-spec.md §4`) — reflejo heredado, sin cambio de píxel.


> **A4-PARITY2 D-86 (2026-10-03) — corrections.** (1) `[bp-0xe]` in the dungeon loop (`0x0fab`) is the kernel party-state `K:0x39fc` result, written BEFORE the key is read; it is not the dispatcher's result, so the table row "`0x0FD0` sólo se cobra si el despachador devolvió 0" is wrong, and so is the same docblock in `game/src/core/world/survival.ts`. (2) The dispatcher result `si` gates exactly one thing: the call to the per-turn block `0x0c76` (`0x0f7b or si,si / je 0x0f87`). The 1-minute clock call `advance_clock(1)` at `0x0f2f` is NOT gated on it: it runs once per loop iteration before the key read, for every key and every return value (`T`: none, `Q`: every second iteration) — recorded as D-91. (3) Every digit key `0x30..0x39` goes to `0x06c4` arm `0x07bc` which calls `K:0x4080` and forces the result to 0 (`0x07ce` / `0x07d1`): an invalid digit underground passes no world turn (fixed by D-86); the overworld (MAINOUT `0x0c39`) and the town (TOWN `0x15d4`) do charge one. `native/core/a4-parity2-findings/D86-FINAL.md`.
