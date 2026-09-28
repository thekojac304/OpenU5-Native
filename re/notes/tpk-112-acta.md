# ACTA #112 — el visor NEGRO tras el TPK de Stonegate

**VEREDICTO: (b) el negro es FIEL.** No hay divergencia. La hipótesis de la ventana
(«sin party viva no hay fuente de luz y el visor sólo dibuja la casilla propia») queda
**REFUTADA**: el camino de luz ni siquiera se ejecuta. El negro es la **escena de
refuge**, que el original pinta —dos veces— y que el port ya modelaba.

Contexto, para que nadie lo lea mal: el TPK del testigo **no es una sala de combate**.
Es la **trampilla** de Stonegate (loc 29 = 0x1D) en pueblo, `post_turn` TOWN 0x0F02
rama 0x0f96. La lava es el relleno del TPK, no el terreno de una arena.

---

## 1. Qué se ve, y qué descarta la propia captura

`humo-4-stonegate-despues.png` (scratchpad de la sesión, `humo-shots/`): viewport
**negro con el Avatar SOLO en el centro**, roster `0D` ×3, log `A TRAPDOOR!` →
`An unending darkness engulfs thee...` → `Thou hast found refuge.`

Esa imagen ya es el discriminador. Es **exactamente** lo que produce
`buildRefugeSceneFigures("void", …)`: una sola figura, el Avatar, en el centro. Y es
**exactamente** el frame f042 del vídeo del ORIGINAL que el port cita como autoridad
(«viewport negro + Avatar solo»).

## 2. El camino de luz NO se ejecuta (prueba de código, no de prosa)

`game/src/skin/coreview.ts:1133-1140` — la cadena es un `else if`:

```
if (egBake)            → bakeEndgameRoom(...)
else if (refugeFigures)→ bakeRefugeFigures(...)     ← durante el refuge cae AQUÍ
else if (campScene)    → bakeCampArena(...)
else if (mode!=="dungeon") → bakeMapWindow(...)     ← visMask / visRadius se calculan AQUÍ
```

Con la escena montada, `bakeMapWindow` **no se llama**: no hay máscara de visibilidad,
no hay radio, no se consulta el terreno. Un radio de luz 0 no puede ser la causa de algo
que se decide antes de calcular radio alguno.

## 3. Qué hace el ORIGINAL — se ennegrece DOS veces

Todos los near-calls resueltos con `re/tools/routine_census.resolve_near_call`
(bases: TOWN `0x81d0`, BLCKTHRN/DNGLOOK `0xa290`), **no** por aritmética de sesgos a ojo:

| call-site | target | ULTIMA.EXE | identidad (nota previa) |
|---|---|---|---|
| TOWN 0x0fa0 / BLCKTHRN 0x0969 | 0x88a0 / 0x67e0 | **0x0A70** | `set_color` (SEL 0x2d) — camp-apparition-scene.md:26, endgame-derivation.md:446 |
| TOWN 0x0fab / BLCKTHRN 0x096f | 0x88d6 / 0x6816 | **0x0AA6** | región del viewport — dnglook-raster-spec.md:67,284 |
| BLCKTHRN 0x0982 | 0x6cb6 | **0x0F46** | `fx_screen_wipe` (#71) |
| BLCKTHRN 0x0962/0x097c | 0x6992 | **0x0C22** | fijador de MODO (#97) |

**(i) TOWN 0x0fa0-0x0fb0 — ANTES de la lava.** Orden literal de la rama de Stonegate:

```
0fa0-0fb0  set_color(0) + región (8,8)-(0xB7,0xB7)   ← ★ ENNEGRECE EL VISOR (11×11)
0fb3-0fd3  cortina de sonido descendente (1000→251)
0fd6-0fe3  repne stosb, 0x400 bytes de 0x8F en DS 0x6608 ← la lava, pantalla YA negra
0fea-0ff4  tabla de objetos/actores a cero
0ff6-1037  roster entero a HP 0 / 'D'
```

⇒ **respuesta a la pregunta (a) de la tarjeta: el original NO enseña la lava ni un
frame.** La escribe en el búfer de mapa con el visor ya apagado.

**(ii) BLCKTHRN 0x0962-0x09b3 — el refuge lo vuelve a apagar.** `set_mode(1)`,
`set_color(0)`, región del viewport, `set_mode(0)`, `fx_screen_wipe` sobre la misma caja;
y a continuación (0x098f-0x09b3) rellena a `0xFFFF` un búfer de **11 bytes × 11 filas,
stride 0x20** — la ventana de casillas entera a «oculto». Tres mecanismos concurrentes,
el mismo resultado.

El port implementa el tercero (celdas ocultas + figuras horneadas). Mismo pixel.

## 4. Orden en el port: tampoco hay frame de lava

`stonegateLavaWipe` pinta la lava y empuja `map-changed`; `checkRefuge` (TOWN 0x1436)
corre **en el mismo turno** al cerrar `runContextTurn`, y `runRefugeScene` llama a
`step()` **síncronamente**, montando `void` antes de ceder al bucle de eventos. No se
pinta ningún frame intermedio. El port llega al mismo sitio por otra vía (original:
apaga → escribe invisible; port: escribe → monta la escena antes de pintar), y el
resultado observable es idéntico. Clase C.

## 5. Defecto de atribución encontrado de paso (NO corregido aquí)

`re/notes/lote-mecanica-pendiente.md:95` mete los dos calls del ennegrecimiento dentro
de la **«cortina de sonido»**:

```
0fa0-0fd3  cortina de sonido: call 0x88a0(0) · 0x88d6(0xb7,0,8,0) · bucle 1000->251
```

Son cosas distintas — TOWN `0x0fa0-0x0fb0` es **vídeo** (color + región), y TOWN
`0x0fb3-0x0fd3` es el **sonido**. Y los args están transcritos mal: los `push` son `8,8,0xb7,0xb7`, no
`(0xb7,0,8,0)`. Esa fusión es justo la razón por la que el ennegrecimiento no llegó al
comentario del port y la observación quedó sin explicar. **No toco la nota del lote #54**
(carril ajeno, posiblemente vivo): queda apuntado para su dueño.

## 6. Cola abierta — NO adjudicado, no lo lea nadie como veredicto

**¿Persiste la lava si vuelves a Stonegate?** En el port `setMapOverride` escribe en
`state.mapOverrides` (clave `29:0:x:y`), capa **persistida en el save**
(`saveNative.ts:564`) y que **nadie limpia** al entrar a un mapa ⇒ tras resucitar en el
castillo y volver a Stonegate, el port serviría 32×32 de lava (tile de daño). En el
original la escritura va al búfer vivo `DS:0x6608`, que el propio refuge machaca al
cargar el mapa del castillo (BLCKTHRN 0x091f).

**Lo que NO he derivado**, y por eso no lo adjudico: si el modelo de persistencia de
small maps del original guarda o no los cambios de TERRENO de pueblo (el `.GAM` sí lleva
la tabla de objetos; el terreno se re-lee de disco, pero eso hay que probarlo). Es una
pregunta de alcance mucho mayor que #112 — toca TODOS los consumidores de
`mapOverrides` (puertas forzadas, campos arados, cofres). Lo que lo cerraría: sonda de
oráculo — TPK en Stonegate → resucitar → volver a entrar → mirar el terreno.

## 7. Gates

Exits leídos **sin pipe**.

| gate | resultado |
|---|---|
| `vitest run tests/trapdoor-fall.test.ts` | **13 passed** · EXIT 0 |
| `tsc --noEmit` | EXIT 0 |
| mutación de control (ver abajo) | 2 rojos esperados, 10 previos verdes |

**Control positivo (los verdes se vieron SUSPENDER).** Un verde de un test que sólo
afirma lo que el port ya hace no vale nada hasta verlo caer. Mutando (a) el primer beat
para que no fije `scene:"void"` y (b) el corte `if (phase === "void")` de
`refugeScene.ts`, los **dos** asertos nuevos se pusieron rojos
(`expected undefined to be 'void'`; `expected […4 figuras] to have a length of 1`)
mientras los **10 tests previos del fichero seguían verdes** — la mutación era dirigida.
Mutaciones revertidas; `git diff` deja sólo el test y un comentario.

## 8. Ficheros tocados

- `game/tests/trapdoor-fall.test.ts` — bloque nuevo `#112` (3 asertos: el beat `void` del
  turno del TPK; el CONTROL de que la lava sí se escribió, que separa las dos causas
  candidatas; y que `void` hornea una sola figura). **DETECTORES del port**, no sellos de
  fidelidad: rojo = «el port cambió de camino». La fidelidad la sostiene §3, no el expect.
- `game/src/core/game.ts` — SOLO comentario de `stonegateLavaWipe`: la derivación del
  ennegrecimiento previo, en TOWN 0x0fa0-0x0fb0, que faltaba. Cero valores, cero comportamiento.

---

**Nota A3-HF9 (H-185).** «a negro ANTES de nada más» se precisa: lo primero de `party_refuge`
es el redraw `0x093f` (sólo bajo loc `0x21`) y `delay(10)` (`0x0942`/`0x0946`); la línea de la
oscuridad (`0x095f`) y el negro (`0x0962`) vienen DESPUÉS. El guión lleva ahora ese `delay(10)` como
su primer beat (sin escena) y el negro llega con la línea; `trapdoor-fall.test.ts` #112 lo fija así.
Si el redraw `0x093f` enseña o no la lava en el TPK de Stonegate no se ha adjudicado aquí.
