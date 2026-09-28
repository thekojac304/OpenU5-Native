# El rito del santuario: cadencia por TECLA y el NEGATIVO del viewport (#294 · #295 · #275)

Derivación de la cola del rito de santuario y del Códice en `CAST2.OVL`, nacida de tres
reportes del usuario del 14-08 sobre el deploy #32: (a) «al acabar los mantras sale todo el
texto de una — ¿en el original hay parones?», refinado por él mismo a «¿no hay que darle al
Enter tanto en shrines como en Codex?»; (b) el efecto en NEGATIVO al volver del Códice; y
(c) la entrada vacía, que en el original aborta callando.

## 0. Resolución de banda (la que hace legible todo lo demás)

Base de `CAST2` = `0xE1E0` (REGLA A de `quake-harpsichord.md §6`). Confirmada por DOS hits
independientes contra el corpus antes de usarla: `0x5906`→`0x3AE6` coincide con
`screenflash-sites.md §1`, y `0x2890`/`0x29a6`→`0x0A70`/`0x0B86` coinciden con el par
set_color/rect. Tabla de los destinos que aparecen abajo:

| operando en CAST2 | kernel | qué es (cuerpo leído) |
|---|---|---|
| `0x3670` | `0x1850` | `print_string` |
| `0x34da` | `0x16ba` | `putchar` |
| `0x448c` | `0x266c` | ~~**get-key BLOQUEANTE con redibujo**~~ → **bloquea al LLAMADOR; la pantalla SIGUE ANIMADA** (ver ★ abajo) |
| `0x5906` | `0x3AE6` | `kernel_flash(n)` = n × (redibujo + delay 1 tick) |
| `0x3fb2` | `0x2192` | tono por altavoz (puertos 0x42/0x61) |

★ **PRECISIÓN DE REDACCIÓN (#329, 15-08 — tachado-no-borrado en la fila de `0x448c`).** El
rótulo «BLOQUEANTE» era cierto a medias y la mitad que callaba es justo la que decide un
fix: bloquea al LLAMADOR (no retorna hasta que hay tecla), pero **NO deja la pantalla
quieta**. `0x266c` es un bucle de SONDEO — `0x267f` cima · `0x2683` poll `0x2032` · sin
tecla `0x269a call 0x5910` (el REDIBUJO, gateado a location fuera de `0x21..0x7f`) ·
`0x269f` vuelta a la cima. Quien leyera sólo «bloqueante» concluiría que el modal
CONGELADO del port es fiel, y es al revés: por eso el usuario reportó el rito como un
cuelgue en el deploy #34, y por eso #329 enciende el cursor en esa espera. Misma familia
que la corrección de §7 sobre esta rutina: el nombre de una espera decide qué se porta.
| `0x2890` | `0x0A70` | `set_color(c)` — enmascara a `0xF` (EGA) o `0x3` (CGA) |
| `0x29a6` | `0x0B86` | rect(x1,y1,x2,y2) con **`stc`** (carry = modo XOR) |
| `0x4e92` | `0x3072` | primitiva de sacudida/bandas — **consume RNG** |

## 1. #294 — la cadencia post-mantra es ESPERA DE TECLA, no un temporizador

De las tres hipótesis de la ficha gana la **(a)**. `0x266c` (destino de `call 0x448c`) es un
get-key **bloqueante**: bucle `0x267f-0x269f` sobre el poll `0x2032` del que no se sale hasta
que la tecla es distinta de 0, llamando a `0x5910` (redibujo del viewport) mientras espera, y
con una tabla de traducción de numpad para `0x31-0x39`. Devuelve una tecla **arbitraria**.

🔴 **Corrección de corpus.** `agregado-23-acta.md:197` la nombra «`getYN 0x448c` sólo lee
Y/N». Eso es falso **de la rutina**: el filtro Y/N lo pone el LLAMADOR. El control que lo
zanja está en el propio fichero — `0x0b6a` envuelve la misma llamada en
`cmp ax,0x30 / cmp ax,0x39` **en bucle** para quedarse sólo con dígitos, gesto que no tendría
sentido si la rutina ya filtrase. `citas-sesgo-overlay.md:23-25` la nombra bien
(`getkey_with_redraw`). Quien lea la primera concluirá que ahí no puede haber una espera de
tecla genérica, y por tanto que el reporte del usuario es imposible.

**Dónde parten el texto, exactamente.** En `shrine_visit` (`0x0966-0x0d23`), rama de ENTREGA
de misión, DOS key-wait en `0x0a9b` y `0x0abc` que dejan TRES bloques:

```
str 0xb5ff                                    ; "The Altar speaks and a Quest is ordained! "
0x0a9b  call 0x448c                           ; ← TECLA
str 0xb62c + str tabla[0x4b5e+v]+0xb21e + str 0x9598
0x0abc  call 0x448c                           ; ← TECLA
str 0xb669                                    ; "Return again when thy Quest is done!"
```

**El Códice lleva NUEVE.** El handler `0x0d24` (alcanzado por `call 0xd24` desde `0x1072`,
la rama Códice del envoltorio `0x0e76`) gatea con tecla en `0x0d2b`, `0x0d35`, `0x0d3f`,
`0x0d9f`, `0x0df8`, `0x0e16`, `0x0e2d`, `0x0e44` y `0x0e5b` — incluidas las cuatro páginas de
la profecía rúnica, cada una con su `set_font(1)` / `print_string` / `set_font(0)` / TECLA. Y
~~el envoltorio `0x0e76` lleva la suya en `0x110b`~~ (**corregido en §7**: `0x110b` no es del
envoltorio). Es decir: el «tanto en shrines como en Codex» del usuario es literal, y en el
Códice el efecto es MÁS marcado que en el santuario.

⚠️ **Alcance más estrecho de lo que sugiere la ficha en un punto:** la rama de QUEST COMPLETA
(«WELL DONE!», `0x0c18-0x0d1a`) **no lleva ningún `0x448c`**. Ahí el original no espera nada:
imprime, barre el altavoz y sale.

★ **Consecuencia de método:** una espera de tecla NO tiene constante de reloj de pared, así
que esta ficha se implementa **sin calibrar nada** contra vídeo — no hereda los 120 ms/
fotograma de la escena del santuario ni los necesita. El testigo de vídeo sólo vale aquí como
segundo canal (el tell de la ficha: intervalos IRREGULARES entre tomas distintas).

## 2. #295 — el negativo es un XOR de índice de color sobre el rect del viewport

El idiom, dos llamadas seguidas:

```
push [g_unk_13b0] ; call 0x2890        ; set_color(c), enmascarado a 0xF (EGA) / 0x3 (CGA)
push 8 ; push 8 ; push 0xb7 ; push 0xb7 ; call 0x29a6   ; rect(8,8,0xb7,0xb7) con stc
```

`0x0B86` mete `stc` antes del `lcall` al driver. `EGA.DRV fn21 @0x1180` **testea ese carry en
su primera instrucción** (`jae 0x119a`), y la rama de carry programa el registro **3 del
Graphics Controller** (índice 3 = Data Rotate / Function Select) con **`0x18`**, cuyos bits
4-3 = `11` = **operación lógica XOR**; al salir lo restaura a 0 (replace) junto con map-mask
`0xF` y bit-mask `0xFF`. El rectángulo `(8,8)-(0xb7,0xb7)` son **176×176 px = el viewport
exacto**, de modo que los paneles quedan intactos sin lógica adicional.

**Careo visual contra la captura del usuario** (`negativo-retorno-codex-295.jpeg`, santuario
de HONOR): hierba de fondo negra→**BLANCA**, moteado verde→**MAGENTA CLARO**, rocas siguen
oscuras, paneles laterales normales. Eso es `c → c XOR 15` sobre índices EGA de 4 bits. NO es
una paleta invertida ni un melt: el mecanismo derivado y la foto coinciden pieza por pieza.

**DOS regímenes de duración** — la diferencia importa al portarlo:

- **PAREADO** (`0x0031` … `0x007a`): invierte, suena, y **des-invierte con un segundo XOR**
  (XOR es su propia inversa). Destello corto; la duración la fija el tono de en medio.
- **SUELTO** (`0x0c41`, rama WELL DONE): invierte y **no des-invierte**. Se restaura sola
  cuando `kernel_flash(10)` (`0x0d1a`) redibuja el viewport diez veces. Por eso el usuario ve
  un negativo **sostenido**: entre la inversión y el redibujo van los dos barridos de altavoz
  (`0x0c44-0x0c85`, `si` de 2000→25000→2000 en pasos de 50 = 920 llamadas de tono) y el
  `0x3072`. Su fotograma cae en ese tramo.

**Censo del idiom:** 7 sitios en CAST2, TODOS con el mismo rect del viewport — `0x0031`,
`0x007a`, `0x0bc3` (donación), `0x0c37` (WELL DONE), y `0x0db3`/`0x0dca`/`0x0de1`, que son la
**ceremonia final del Códice** (las tres inversiones alternan `g_unk_13ae`/`g_unk_13b0` y
**bracketan** las tres llamadas a `0x3072` que el port ya emite como `{kind:"quake"}`,
`shrine-ceremonies.ts:355`). ~~Más 3 sitios en SHOPPES3.~~

🔴 **Corrección del carril fix-299 (2026-08-19), tachado-no-borrado:** los 3 sitios de
SHOPPES3 que este censo contó (`0x065a`/`0x0695`/`0x06bd`, tras el `set_color([g_unk_13b0])`
de `0x0646`) son rects de la ventana **REGISTER de la posada** (el tramo `0x052a-0x06c7` de
la ficha #283) con OTRA geometría (`0xc6..0x131`, franjas de 7 px) — **no son el destello del
curandero**, y la ficha #299 heredó de aquí «los 3 sitios de SHOPPES3 son los candidatos». El
destello real vive en **SHOPPES.OVL** (`healer_light_flash_fx` `0x13b0-0x1469`, TRES rect XOR
del viewport con máscaras 4/15/15 + tres pares de tone_sweep; llamadores `0x1611`/`0x1684`/
`0x16eb` = Cure/Heal/Resurrect), y este censo no podía verlo: SHOPPES.OVL llama al idiom por
stubs PROPIOS (`0x67e0` set_color / `0x68f6` rect), así que el grep por los literales de CAST2
(`0x2890`/`0x29a6`) casa con SHOPPES3 —que reusa esos números— y no con SHOPPES. El género es
el fichado: *el patrón del instrumento para en la frontera de la numeración de stubs*.
Derivación completa y port: rama fix/299-destellos-healer (`game/tests/healer-flash.test.ts`).

⇒ **Lo que le falta al port es el BRACKET, no la sacudida:** las sacudidas ya están cableadas;
lo que no está es la inversión que las envuelve.

🔴 **RNG — la separación que decide si hace falta ventana.** La inversión en sí (set_color +
XOR rect) es **CERO RNG**: son llamadas al driver. Quien consume es `0x3072`, que en sus dos
bucles de bandas (`si` de 8 a 0xb3 en pasos de 3) llama a `rand_range` (`0x2092`) + `set_tone`
**por banda** — familia de `screen_shake_fx` (#249). ⇒ la inversión se puede cablear sin
ventana; `0x3072` ya está portado como quake.

## 3. #275 — la entrada VACÍA sale del rito EN SILENCIO

`cmp byte ptr [0xbd08], 0` en `0x09cc` (virtud) y `0x0a1e` (cada uno de los tres mantras):
en los dos casos salta a `0x0d1d`, que cae **después** del `call 0x5906` de `0x0d1a`. La
salida por entrada vacía no imprime absolutamente nada, ni siquiera el flash final. La cadena
«Thine thoughts are unfocused.» (`0xb5de`) vive en OTRA rama, la `0x0a62` del mantra
EQUIVOCADO, inalcanzable con el buffer vacío. El port hacía caer la cadena vacía en el mismo
camino de fallo del core y por eso hablaba donde el original calla. **Arreglado y guardado**
(3 asertos en `game/tests/shrine-scene.test.ts`, dentro de la puerta vitest, con control
positivo; mutante M1 = desactivar el discriminante mata los 2 de silencio y deja vivo el
control).

## 4. Dos ⚠ del corpus que esta lectura cierra

- `screenflash-sites.md §2` marca los cuatro sitios del handler `0x0966` como
  «⚠ SIN CONFIRMAR — candidato gate-travel/recompensa-de-shrine». Quedan **CONFIRMADOS como
  SANTUARIO**: `0x0966` lee y escribe `g_shrine_visited_bitmap` (`0x58ce`, en `0x0a74` y
  `0x0d7d`) y `g_shrine_quest_bitmap` (`0x58cc`, en `0x0a88`/`0x0c1f`), y fija `g_karma=0x63`.
  El handler `0x0d24`, marcado «⚠ narración», es el **CÓDICE**.
- La errata de #281 queda corroborada desde el otro lado: en `0x0ed2` el `cmp …, 0x11` compara
  contra `[bp-4]`, que viene de `byte ptr [call 0x6222(party_x, party_y)]` = **el tile bajo la
  party**. `0x11` es un TILE, no una location.

## 5. Lo que NO está medido aquí (para que nadie lo dé por medido)

- La **duración en fotogramas** del negativo y si el testigo de vídeo la confirma: pedida a
  `espejo-barrido-2`, sin respuesta al escribir esto. La derivación dice «sostenido hasta el
  redibujo»; el número de fotogramas concreto NO está medido.
- La **irregularidad entre tomas** que sería el segundo canal de #294: pedida, sin medir.
- El cardinal exacto de tiradas de `0x3072` (conté los dos bucles de bandas, ~58 iteraciones
  cada uno, pero **no** verifiqué si hay anidamiento interno; #249 da 1856 por invocación para
  `screen_shake_fx`, y esa cifra y la mía no se han reconciliado). No usar ninguna de las dos
  como cifra firme sin rehacer la cuenta.

## 6. ADENDA rito-cableado-3 (14-08) — lo CABLEADO, lo declarado, y una cifra que §5 dejaba abierta

**Cableado en este árbol (rama `rito-cableado-3`):** el régimen **SUELTO** de §2 — el WELL DONE.
Estado de snapshot `ritualInvert` (patrón `bedBlackout` de #296) montado por el conductor
`ui/ritual-invert.ts`, emitido por el core en el orden del binario (`0x0c29` texto → `0x0c34`
set_color → `0x0c41` rect XOR → `0x0c44` barridos → atributos), y pintado por las DOS pieles
antes de la cortina de cama.

**La cifra que §5 daba por NO medida** («la duración en fotogramas del negativo») ya no
necesita al testigo de vídeo para el port: la ventana es exactamente los dos barridos de
`0x0c44-0x0c85`, y sus cinco argumentos están en el ASM — `toneSweep(0xc1c, 1, 0x96, si, 0)`
× 460 × 2, mismas cotas que el ritual del shard. Sale de `wellDoneInvertWindowMs()`, no de un
cronómetro. Lo que el vídeo acredita sigue siendo el HECHO (negativo sostenido), no la cifra.

**El régimen PAREADO de §2 ya estaba portado** y esta nota no lo decía: es el `TimeSpellFlash`
de `skin/fiel/invert-flash.ts`, disparado por el cue `time-spell` con la ventana de
`timeSpellFlashWindowMs`. O sea, de los dos regímenes que §2 distingue, al port le faltaba
UNO — el suelto.

**Lo que sigue SIN cablear, con su razón (no por olvido):**
- El **bracket del Códice** (`0x0db3`/`0x0dca`/`0x0de1`): DOBLE cerradura — el valor de
  arranque de `g_unk_13ae` (#305, oráculo) **y** el modelo de render (#317). Que llegue #305
  no lo desbloquea solo.
- El `call 0x4e92` de **`0x0c88`** (kernel 0x3072 dentro del WELL DONE): consume RNG ⇒ es #300.
  La ventana del port queda por eso algo más corta que la de 1988.
- Las **esperas de tecla** de §1 (#294): derivación intacta, cableado pendiente.

🔴 **Corrección a §2 que este carril midió (censo de #317):** «Eso es `c → c XOR 15` sobre
índices EGA» describe bien el BINARIO, pero el port lo aproxima con `difference` blanco, que
NO es la misma operación: con máscara 15 divergen los índices **6 y 9** por el brown-fix
`EGA[6]=#AA5500`. La aproximación es buena porque el 6 casi no sale en el viewport del rito —
no porque las operaciones coincidan. Clase C declarada en `frame.ts` y en `skin/api.ts`.

## 7. ADENDA rito-cableado-5 (14-08) — las esperas CABLEADAS, y dos correcciones al §1

**Cableado en este árbol (rama `rito-cableado-5`):** las once esperas de §1, como evento
`{kind:"shrine-key-wait"}` que el core emite en los sitios derivados y que `ui/shrine-key-
pacer.ts` presenta APARCANDO el resto del turno hasta la tecla. Sin temporizador: §1 ya
decía que una espera de tecla no tiene constante de reloj, y el cableado lo respeta —no
hereda la unidad de 120 ms de la escena de #277 ni pide testigo de vídeo. Bajo
automatización no aparca nada, así que el orden de eventos de e2e/digests es el de antes.

🔴 **CORRECCIÓN 1 — el `0x110b` NO es del envoltorio `0x0e76`, y por tanto el rito tiene
ONCE esperas, no doce.** §1 lo apuntaba como «la suya» del envoltorio. El envoltorio abarca
`[0x0e76, 0x10fe)` —lo dice `cast2-shrines-acta.md:82`— y su `ret` está en `0x10fc`; en
`0x10fe` empieza OTRA rutina, con su propio `push bp / mov bp,sp`. Esa rutina es el **Quit &
Save**: imprime `DS 0x9658` y mete la MISMA llamada `0x448c` en un bucle
`cmp al,0x59`/`cmp al,0x4e` (`save-window-writer.md:24-27`). O sea que el sitio que la ficha
contaba como espera del rito es, justamente, el **segundo control** de que el filtro Y/N lo
pone el LLAMADOR (el primero, ya citado en §1, es el bucle de dígitos de `0x0b6a`). El
censo del overlay son 15 llamadas: 2 + 9 del rito, y 4 ajenas (`0x00f0`, `0x0347`, `0x0b6a`,
`0x110b`).

🔴 **CORRECCIÓN 2 — el handler imprime sus dos líneas ANTES de buscar la virtud, y el port
lo tenía al revés.** El barrido del quest-bitmap arranca en `0x0d42`, o sea DESPUÉS de
`0x0d2b`/`0x0d35`/`0x0d3f` y de los prints de `0x0d2e` (`0xb703`) y `0x0d38` (`0xb733`). El
port tenía esas dos líneas detrás del `return` de la rama sin quest, así que la trampa de
dev salía sola: «HOW DID YOU GET HERE?» sin «The book is open…» ni «Upon the hallowed
page…». En el original la trampa llega DEBAJO de las dos, con sus tres esperas delante.
No es una corrección cosmética que se pudiera aplazar: cablear las esperas sin arreglar el
orden habría dejado en esa rama **tres teclas sin nada que separar**.

**Clase C declarada del ordained:** entre el print de `0x0a8c` y la primera espera, el
binario pone al Avatar DE PIE (`0x0a93`-`0x0a98` escriben el tile `0x1c` en
`g_char_anim_states[0..1]`) — se levanta MIENTRAS habla el altar. En el port ese gesto vive
en el guion de SALIDA de #277, que corre al final de la rama. Cambia CUÁNDO se ve el tile
dentro de la misma escena; ni estado ni RNG.

**Guarda:** `game/tests/shrine-key-wait.test.ts` (16 asertos), con el esperado EN CRUDO como
lista de tokens del turno entero —para que quitar CUALQUIERA de las once desplace el array y
la eche de menos por su sitio, no por el total— más los negativos con control positivo
(WELL DONE, mantra equivocado, entrada vacía, donación) y el ciclo de vida del paceador.

## 8. ADENDA A3-HF6 (27-09-2026, port nativo T-Deck) — las once esperas, re-derivadas y cableadas en el dispositivo

**Re-derivación independiente** (`re/tools/dis16.py` sobre `original/u5/ultima5/CAST2.OVL` y
`ULTIMA.EXE`): con base `0xE1E0` hay **15** `E8` que caen en `0x266c` — `0x00f0`, `0x0347`,
`0x0a9b`, `0x0abc`, `0x0b6a`, `0x0d2b`, `0x0d35`, `0x0d3f`, `0x0d9f`, `0x0df8`, `0x0e16`,
`0x0e2d`, `0x0e44`, `0x0e5b`, `0x110b` — exactamente el censo de §7 (2 + 9 del rito, 4
ajenas). En el tramo `0x0966-0x0e76` no hay NINGUNA llamada a `0x1b16` (vaciado del buffer
de teclado) ni a `0x20fa` (`delay`): cada espera es un `call 0x448c` pelado entre dos
`print`. Consecuencia: una tecla por espera; una tecla en el buffer (typeahead) satisface
sólo la SIGUIENTE espera, nunca varias.

🔴 **Precisión sobre §0.** En el bucle
de `0x266c`, `0x2683 call 0x2032` NO es el sondeo: `0x2032` es un `toupper` de 9
instrucciones (`cmp 0x61`/`cmp 0x7a`/`sub 0x20`). El sondeo es `0x267f call 0x1b38`
(`poll_key_blink_cursor`: parpadeo del cursor, `int 16h` + `int 21h ah=6` vía `0x0e0e` →
`0x1d5e`, y `delay(1)` si no hay tecla), cuyo resultado va a `0x2032` por la pila. La
conclusión de §0 no cambia: bloquea al llamador, la pantalla sigue animada.

⚠️ **Herramientas.** `re/tools/thunks.py --bases` y `callers_banda.py` dan a CAST2 la base
`0xC29E`; con ella `0x448c` cae en `0x072a`, a mitad de instrucción. La base buena es la de
§0 (`0xE1E0`). Mismo tipo de error que A3-HF5 encontró para TALK (`0xBF80`, no `0xA290`).
Las herramientas NO se cambian aquí.

**Estado nativo.** El core (`native/core/src/shrine.cpp`) emite `GameEventKind::ShrineKeyWait`
en las once; hasta A3-HF6 el dispositivo sólo las consumía dentro de la escena de captura de
Blackthorn. Desde A3-HF6 cada marcador es una espera de tecla del `DialoguePacer` de A3-HF5
(`paced_event_pause`), idéntica al KeyWait `0x8F` de TALK — `native/targets/tdeck/ALPHA3_AUDIO.md`
§32, H-183 / D-40. La rama WELL DONE sigue sin espera (§1 ⚠️).
