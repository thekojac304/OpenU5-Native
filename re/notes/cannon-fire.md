# (F)ire y el sistema de CAÑONES — derivación (task #32)

El comando **(F)ire** tiene TRES ramas según `g_location` (DS:0x5893). El dispatcher del
kernel (0x3266, tabla en command-dispatch.md §6) ecoa `"Fire-"` (DS 0xa164) y salta al stub
CMDS:0x0AEA (0x8046). El overlay `cmd_fire` (CMDS.OVL.asm 0x0AEA) reparte:

```
0x0AF0  cmp g_location,0x20 ; jbe 0x0B08     ; loc 0 (exterior) + 0x01-0x20 (pueblos)
0x0AF7  cmp g_location,0x29 ; jae 0x0B08     ; loc >=0x29 (combate)
        ; entre 0x21 y 0x28 (MAZMORRA): cae aquí →
0x0AFE  ax=0x42E4 ; call 0x58D0(print) ; ret ; "What?\n"
0x0B08  cmp g_location,0 ; jne 0x0B16        ; loc!=0 → cañón a pie (0x0B16)
0x0B0F  call 0x0962 ; ret                    ; loc==0 → ANDANADA de fragata
```

- **Mazmorra (0x21-0x28)** → `"What?\n"` (DS 0x42E4), sin más.
- **Exterior (loc 0)** → `cmds_fire_broadside` 0x0962 (andanada de fragata).
- **Pueblo (0x01-0x20) / combate (≥0x29)** → `cmds_fire_cannon` 0x0B16 (cañón a pie).

Los 6 strings (punteros DS resueltos en DATA.OVL con `fileoff = DS+0x10`, byte-exactos):

| DS ptr | DATA.OVL | texto | uso |
|--------|----------|-------|-----|
| 0x42C6 | 0x42D6 | `What?\n` | andanada: no vas en fragata (0x0978) |
| 0x42CD | 0x42DD | `Fire broadsides only!\n` | andanada: disparo NO perpendicular (0x09B2) |
| 0x42E4 | 0x42F4 | `What?\n` | mazmorra (0x0AFE) |
| 0x42EB | 0x42FB | `What?\n` | pueblo: SIN cañón adyacente (0x0BDE) |
| 0x42F2 | 0x4302 | `BOOOM!\n` | dispara (ambas ramas, 0x0BEE / print previo a la bola) |
| 0x42FA | 0x430A | `Door destroyed!\n` | impacto en muro/puerta (0x0D1C) |

Sonido (ambas ramas, idéntico): `call 0x842E` con args `(300, 5, 200, 1000)` = el GLIDE
del cañón (start 1000 → end 200), ya catalogado como `cannon-fire` (sfx-catalog.md §4.9,
audio-diff-calibration.md). No es `noise_burst`.

---

## 1. Cañón a pie — pueblo/combate (CMDS 0x0B16) ← el bug del usuario

**Escaneo de los 4 vecinos ortogonales** del party en un buffer de mapa de stride 0x20/fila,
1 byte/tile, centro (party) = 0xABA7. Orden fijo, **el PRIMER cañón encontrado gana**:

| # | dir | addr buffer | offset vs centro | provisional (dx,dy) |
|---|-----|-------------|------------------|---------------------|
| 1 | N | 0xAB87 | −0x20 | (0,−1) |
| 2 | E | 0xABA8 | +1 | (+1,0) |
| 3 | S | 0xABC7 | +0x20 | (0,+1) |
| 4 | O | 0xABA6 | −1 | (−1,0) |

Predicado de cañón (0x0B2F/0x0B4A/0x0B60/0x0B7A): `(tile & 0xFC) == 0xB4` → tiles
**0xB4..0xB7**. Si ninguno cuadra → `"What?\n"` (0x0BDE → DS 0x42EB). El tile del cañón
casado se guarda en `[bp-0x1E]`.

**Dirección de disparo = orientación del tile del cañón** (`tile & 3`, switch 0x0BBC),
NO la posición relativa del vecino:

| tile | `&3` | switch | (dx,dy) | dir |
|------|------|--------|---------|-----|
| 0xB4 | 0 | 0x0BE4 | (0,−1) | Norte |
| 0xB5 | 1 | 0x0C8C | (+1,0) | Este |
| 0xB6 | 2 | 0x0C9A | (0,+1) | Sur |
| 0xB7 | 3 | 0x0CA8 | (−1,0) | Oeste |

Doble anclaje independiente: `dir_vector_to_facing` (CMDS 0x1504, el que usa **Push** al
rotar un cañón empujado, cmds.md §3) fija el tile a **base 0xB4 + {N:0, E:1, S:2, O:3}** —
idéntico. ⚠ **Los nombres de `TileData.json` (Ultima5Redux) rotulan 0xB5=CannonDown y
0xB6=CannonRight, INVIRTIENDO Este/Sur.** El motor manda: **0xB5 dispara al ESTE, 0xB6 al
SUR.** El clon (`cannonFireDir`) sigue el ASM, no los nombres.

**Vuelo del proyectil** (loop 0x0C1A): la bola PARTE de la celda del cañón (`startX/Y =
party + dirVecino`) y avanza en la dirección de orientación. Contador `range=5` con
**pre-decremento y test `>0`** (0x0C15/0x0C2C) → **4 celdas útiles** (cannonCell+1..+4). En
cada celda:

1. **Ocupante** (`call 0x7782(x,y,floor)` → tile, 0 = vacío). Si ≠0 (0x0CB8): la bola
   SOBREVUELA terreno/overlay pasable (tile <0x1C salvo 0x10/0x11, ó 0x78-0x7F) y en el
   resto **impacta un NPC/monstruo** (`[bp-0x18]=1`, guarda el índice).
2. **Sin ocupante**: lee el tile BASE (`call 0x8482(x,y)` → puntero) y es **muro/puerta
   sólido** si está en **0x97..0x99 ó 0xB8..0xBB** (0x0C75-0x0C87) → `[bp-2]=1`. (Lista
   LITERAL; NO el flag `RangeWeapon_Passable` de TileData — 0x99 Portcullis lo marca pasable
   pero el cañón lo revienta.)
3. Si no → la bola sigue.

**Resolución** (tras el loop, 0x0CEE):
- **Muro/puerta** (`[bp-2]`): `[bx]=0x44` (0x0D2E) → el tile se vuelve **0x44 BrickFloor**;
  imprime `"Door destroyed!\n"` (DS 0x42FA); `g_unk_24E6=1` (consume turno).
- **NPC** (`[bp-0x18]` y bp-8≠0, 0x0D3B): `call 0x7AF4` = kernel `0x3A74`
  / `set_actor_record`, con siete campos cero: borra el registro-objeto alcanzado;
  `g_unk_24E6|=2`; **KARMA −5 con clamp** (0x0D5A: `karma = karma>5 ? karma-5 : 0`); luego
  `0xffffBB9E/86/92(idx)` = TOWN `0x011e` busca el slot por índice-objeto,
  `0x0052` marca el bit elegible y `0x00b0` vacía la ranura viva.

**RNG:** el código visible de esta rama **NO llama a rand()**. La aleatoriedad (si la hay)
puede vivir en el world-turn que dispara `g_unk_24E6` DESPUÉS
de que el handler retorna.

## 2. Andanada de fragata — exterior (CMDS 0x0962)

Ya modelada en el port (`Game.fire(dir)`, transport.md §7C). Requiere `g_transport_tile`
∈ 0x20-0x27 (fragata; si no → `"What?"` DS 0x42C6), **getdir** (`call 0x766C`), disparo
**perpendicular a la quilla** (bit0 del transport; si no → `"Fire broadsides only!"` DS
0x42CD), alcance **3**, world-turn (viento) SÓLO en impacto ANTES del `rand(1,20)` de daño.

---

## 3. Estado del port + implementación (task #32)

**Bug del usuario** (a pie junto a un cañón en West Winds, F → `"Fire-"` + `"What?"`): el
port sólo tenía la rama de fragata (`Game.fire(dir)`) y la UI SIEMPRE pedía dirección; en
pueblo caía en el `"What?"` de no-fragata. Faltaba la rama 0x0B16 entera.

Implementado:
- `game/src/core/world/cannon.ts` — primitivas puras: `isCannonTile` (`&0xFC==0xB4`),
  `cannonFireDir` (`&3`→N/E/S/O), `isCannonSolid` (0x97-0x99 ∪ 0xB8-0xBB),
  `CANNON_RUBBLE_TILE=0x44`, `CANNON_NEIGHBOR_SCAN` (N,E,S,O).
- `Game.fireCannon()` (game.ts) — la rama a pie completa; `Game.fireWantsDirection()` — true
  SÓLO en loc 0 sobre fragata (la única que hace getdir).
- `main.ts` — el handler de `f` ecoa `"Fire-"` y bifurca: `fireWantsDirection()` →
  getdir→`fire(dir)` (andanada); resto → `fireCannon()` inmediato.
- Tests: `game/tests/cannon.test.ts` (15) — primitivas, orientación, destrucción de muro,
  alcance 4, no-cañón/mazmorra/exterior→"What?", karma-5+muerte de NPC, `fireWantsDirection`.

**Corrección Batch 33 (H-168):** el impacto no tira HP/daño. `0x7AF4` es el setter de
registro-objeto `0x3A74`; los argumentos cero borran el objeto alcanzado. Los thunks
posteriores hallan el slot, marcan su bit si el tipo es elegible y lo vacían inmediatamente.
La rama visible sigue sin tirar RNG. El world-turn disparado por `g_unk_24E6` permanece
fuera de esta adjudicación de ciclo de vida. Alcance restante mínimo; el caso del usuario (cañón a pie
que revienta muros/puertas) queda fiel y cubierto por tests.

---

## 4. El VUELO del proyectil (tasks #298 / #311 / #313)

El comando pide la animación del vuelo con `call 0xffffbc6a`, y lo hace **una vez por disparo,
NO por celda**: los tres call-sites están FUERA del bucle del rayo.

| call-site | superficie | rama | destino que empuja |
|-----------|-----------|------|--------------------|
| 0x0A45 | andanada | impacto | celda del objeto (`obj − party + 5`, 0x0A2B/0x0A3F) |
| 0x0AD2 | andanada | fallo | final del rayo (`3·paso + 5`, 0x0AB4-0x0ACF) |
| 0x0CFE | cañón a pie | convergencia | la celda donde paró el rayo (`[bp-0xC]`/`[bp-0x10]`) |

Los cinco argumentos son `(origX, origY, dstX, dstY, 1)` en coordenadas de la ventana 11×11
(pila en ese orden; en la convención C del binario el `1` es el primer parámetro). El sonido
—`call 0x842E`, glide 1000→200— es el MISMO en las dos superficies (0x09D5 y 0x0C05).

🔴 **Las dos superficies comparten el stub y el sonido, pero NO el origen ni el alcance.**
Es la confusión que hay que no heredar:

| | andanada (0x0962) | cañón a pie (0x0B16) |
|---|---|---|
| ORIGEN de la bala | `push 5 / push 5` (0x0A48) = centro de la ventana = **el barco** | `[bp-6]`/`[bp-0xA]`, fijados en 0x0BAD/0x0BB9 y **nunca re-escritos** = **la celda del cañón** |
| ALCANCE | **3** — `[bp-0xC]` 0,1,2 con post-incremento y `cmp 3` (0x0AA5) | **4** — `[bp-0x16]`=5 (0x0C15) con **pre-decremento** y test `>0` (0x0C2C) |

El «5» del cañón a pie **no es el alcance**: con pre-decremento y corte en 0 avanza 4 veces.
La ficha #311 dio «alcance 3» para las dos al generalizar el número de la andanada; el disasm
y el port (`Game.fireCannon`, desde #32) siempre dijeron 4. Las dos constantes viven JUNTAS y
citadas en `game/src/core/world/cannon.ts` para que la diferencia no se vuelva a unificar
«por simetría».

**RNG:** cero. La única tirada del comando entero es el `rand(1,20)` del daño de la andanada
(0x0A74 → `call 0x6112`, con `push 1` y `push 0x14`; el ledger lo escribe `(max,min)` = ficha
#76). Censo de control: en TODO `CMDS.OVL.asm` hay 10 `call 0x6112` y el único del tramo
0x0962-0x0D1C es ése — el tramo del cañón a pie (0x0B7E-0x0D1C) no tiene NINGUNO.

🔴 **MURO DE LECTURA (ficha #311):** `0xffffbc6a` + base COMSUBS 0xBF80 = `0x7BEA` =
`lcall 0x72E:0x2EC`, far-call cuyo segmento se reubica en carga. Los **fotogramas** del vuelo
(cuántos, con qué retardo, si hay estela o humo, y **si consume RNG**) **no son derivables del
disasm** — ya Clase-C en `serpent-ranged-derivation.md` §4. Lo cableado en el port es la
ESTRUCTURA de esta tabla (derivada) con la **cadencia PRESTADA** del proyectil de combate,
declarada como préstamo en `game/src/skin/world-fx.ts`. Si el oráculo de #311 mide que ese
stub tira RNG, el cableado de hoy pasa a divergente y hay que declararlo.

## 5. Adjudicación del defecto de #313 (16-08, corridas limpias)

El síntoma que abrió #313 («cero fillRect 4×4 en las cuatro combinaciones») quedó
**REFUTADO sobre el árbol fusionado** (rama `fix/canon-298` + main `1f8879e5`): con el
escenario ESTABLE —calentar con un turno que NO mueve al grupo ANTES de sembrar el cañón,
control del emisor por parche de instancia EN LA MISMA corrida, contador de `pageerror` a
cero— el pasillo entero `push → gate → dot → píxel` está vivo en las CUATRO combinaciones
(a pie/andanada × fiel/shader). Medido en navegador: a pie fiel `fx {1,0→5,0}`, 14
fotogramas de punto 4×4 blanco avanzando al este en fila constante; andanada fiel
`fx {0,0→3,0}`, 12; en shader el punto se recompone en device-px vía `paintCannonball`
mientras la fiel oculta pinta su 4×4, sin doble pintado (la aritmética cierra exacta:
16 dots fiel + 10 recomposiciones shader = 26 pasos de pintor, 0 duplicados).

Los ceros históricos tenían DOS causas de arnés, no de port: (a) esperar por
`window.__u5test.game` no acredita que la piel esté suscrita (carrera pre-fusión), y
(b) calentar con `ArrowUp` movía al grupo y desmontaba la siembra — `fireCannon()` sin
cañón adyacente no falla: emite `["message"]` y su cero se lee como «no se pinta».

El testigo durable del camino (evento → pintor por el canal VIVO, tecla F incluida) es
`game/e2e/cannon-projectile.spec.ts`: 4 tests con aserto de PÍXEL y control del emisor,
verificados contra el mutante «consumidor nunca» (4/4 rojos con él, 4/4 verdes sin él).
