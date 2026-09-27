# Catálogo de SFX del PC-speaker por acción de juego (task #3 · insumo task #4)

SCOUT de la familia de sonido del PC-speaker de ULTIMA V. XREF exhaustivo de
TODOS los call-sites (kernel + 24 overlays) de las primitivas de speaker, con los
argumentos pulidos del asm y la acción de juego a la que corresponde cada emisión.
Es el **spec sonoro** de la task #3 (SFX Web Audio) y el guion de grabación de la
task #4 (catálogo AV).

Fuentes: la familia ya derivada en `kernel-sweep-2.md §144` (tono/sweep),
`kernel-sweep-3.md §6/§8.4/§tail` (ruido/beep/flash), `seg2.md` (aritmética far de
overlays), y este barrido. Método XREF de overlays: `dispatch_table.near_calls_to_kernel`
(todo el código vive en un CS de 64K; el destino real de un `call` near del overlay
es `(i+3+rel+load_seg*16) & 0xFFFF`). Capstone muestra los far como `call 0xffffXXXX`
o como un offset local fantasma — ambos resueltos aquí al kernel-root.

---

## 0. Regla de oro: el timbre exacto NO es derivable del estático → AV (task #4)

Las dos primitivas de pitch **bit-banguean el gate del speaker en un bucle software**
cuya velocidad depende de `[0x5356]` (reloj calibrado en runtime por `0x1158`), no de
un divisor PIT fijo. Por eso:

- **Los parámetros (índice de arranque, dirección/magnitud del barrido, nº de muestras
  = duración) SÍ son el spec fiel** y están todos aquí.
- **La frecuencia percibida en Hz y la envolvente NO se derivan del binario** (dependen
  de `[0x5454] = [0x5356]<0x64 ? 0 : [0x5356]/0x18`, calibrado por CPU): quedan para el
  catálogo AV, grabando el original en DOSBox con audio. Marcados `→AV`.

---

## 1. La familia de primitivas del PC-speaker (kernel-root)

| # | Entrada | Nombre | ret/args | Mecanismo |
|---|---------|--------|----------|-----------|
| 1 | `0x2192` | `pcspeaker_tone_sweep` | ret 0xa · 5 args | Tono barrido PWM por software |
| 2 | `0x223c` | `pcspeaker_noise_burst` | ret 6 · 3 args | Ruido en banda (PRNG local `[0x545c]`) |
| 3 | `0x22c0` | `pcspeaker_beep` **(nuevo)** | ret 4 · 2 args | Beep bloqueante = set_tone + delay + stop |
| 4 | `0x22e2` | `pcspeaker_set_tone` | ret 2 · 1 arg | Programa PIT ch2 + abre gate (`out 0x61 \|3`) |
| 5 | `0x230e` | `pcspeaker_stop` | — | Cierra gate (`out 0x61 &0xfc`) |
| 6 | `0x43ae` | `pcspeaker_glide` **(nuevo; corrige seg2)** | ret 8 · 4 args | Glissando lineal set_tone en bucle |
| — | `0x20c8` / `0x20fa` | `delay_via_timer` | | Retardos software (calibrados por `[0x5356]`) |

Flag global de sonido on/off: **`g_unk_a9ce`** (`[0xa9ce]`). Si es 0 todas las
primitivas hacen el bucle de temporización **sin abrir el gate** (silencio pero
conserva el timing y el consumo de RNG local). El port debe respetar un toggle
equivalente ("modo 1988 speaker" on/off de la task #3).

### 1.1 `pcspeaker_tone_sweep` `0x2192` — semántica exacta de los 5 args

Args por bp-offset (empujados en orden inverso, o sea `push`-first→last =
`inc, delay, count, start, step`):

| bp | nombre | rol en el bucle |
|----|--------|-----------------|
| `[bp+0xc]` | `inc` | incremento del acumulador de fase `dx` por muestra (≈ frecuencia base) |
| `[bp+0xa]` | `delay` | multiplicador del retardo por muestra (× `[0x5454]`) → periodo de muestreo |
| `[bp+8]`  | `count` | nº de muestras (**= duración**) |
| `[bp+6]`  | `start` | umbral inicial `bx` (arranque del barrido) |
| `[bp+4]`  | `step`  | rampa del umbral por muestra (**signo = dirección del sweep**: `+`→pitch baja, `-`→pitch sube) |

Núcleo (`0x21c4`/`0x21f1`): `dx += inc`; si `dx>bx` gate ON, si no gate OFF; `bx += step`;
retardo `[0x5454]·delay`. Es un generador PWM/Bresenham: el pitch audible es la tasa de
conmutación del gate, que deriva del PIT (divisor fijo `0x3c`) modulado por el software.
**Hz reales →AV**; forma (rampa/duración) derivada.

### 1.2 `pcspeaker_noise_burst` `0x223c` — semántica de los 3 args

`push`-first→last = `step, dur, band`:

| bp | nombre | rol |
|----|--------|-----|
| `[bp+8]` | `step` | acumulador; el burst termina cuando `[0x5456] >= dur` |
| `[bp+6]` | `dur`  | umbral de terminación (**duración** en unidades de `step`) |
| `[bp+4]` | `band` | ancho de banda de frecuencia del ruido (`[0x64, band]`) |

PRNG **LOCAL** de sonido en `[0x545c]` (`(([0x545c]+0x9248) ror 3) ^ 0x9248) + 0x11`):
NO consume `g_rng` — cero impacto en el orden-de-rands del juego (crítico para paridad).
El valor mapea a `[0x64, band]` y programa el PIT (`0x34de12 / freq → out 0x42`).
Timbre percibido (estática/daño/trueno) →AV; banda/duración derivadas.

### 1.3 `pcspeaker_glide` `0x43ae` — **corrige `seg2.md` ("interpolador DDA de movimiento")**

`seg2.md` clasificó `0x43ae` (14 far-sites) como *"interpolador lineal/DDA (stepper de
mov/anim por casillas)"*. **Es falso: es una primitiva de SONIDO.** El cuerpo llama
`0x22e2` (set_tone) en bucle y cierra con `0x230e` (stop):

```
[bp-2] = (endfreq - startfreq) * step / total     ; delta de freq por paso
si = startfreq
for di in 0, step, 2·step … until di >= total:
    set_tone(si); delay(step); si += [bp-2]
stop()
```

Args (`push`-first→last = `startfreq, endfreq, step, total`): `[bp+0xa]=startfreq`,
`[bp+8]=endfreq`, `[bp+6]=step/delay`, `[bp+4]=total`. Es un **glissando** de
`startfreq`→`endfreq`. `seg2.md §Clasificación` y su entrada `0x842e→0x43ae
kernel_fn_43ae` deben re-etiquetarse `pcspeaker_glide`. (El verdadero stepper de
movimiento por casillas es otra rutina; esta no lo es.)

### 1.4 `pcspeaker_beep` `0x22c0` — beep bloqueante (nuevo nombre)

`beep(freq=[bp+6], dur=[bp+4])`: si `g_unk_a9ce`, `set_tone(freq)`; `delay(dur, 1)`
(`0x20c8`); `stop()`. Beep síncrono de pitch fijo. Sólo lo usa el tick de FX de
tiles animados (§3.4, casos 1/4).

---

## 2. XREF exhaustivo — inventario de call-sites

**~109 call-sites** en total. Resueltos con `near_calls_to_kernel` (overlays) + grep
directo (kernel). Recuento por primitiva:

| Primitiva | Kernel | Overlays | Total |
|-----------|:------:|:--------:|:-----:|
| `tone_sweep 0x2192` | 6 | 38 | 44 |
| `noise_burst 0x223c` | 8 | 17 | 25 |
| `glide 0x43ae` | 1 | 26 | 27 |
| `set_tone 0x22e2` | 6 | 2 | 8 |
| `stop 0x230e` | 3 | 2 | 5 |
| `beep 0x22c0` | 2 | 0 | 2 |

Reparto por overlay (offsets de fichero de cada `call`):

| Overlay | tone_sweep | noise_burst | glide | set/stop |
|---------|-----------|-------------|-------|----------|
| TOWN | 0xe6d, 0x11e9 | 0xa75, 0x101e | — | 0xfbb / 0xfd3 |
| MAINOUT | — | 0x300 | 0x113b,0x11ec,0x12a6,0x13e9 | — |
| DUNGEON | — | 0x4b9,0x99e,0xa30,0x103c | 0x1483,0x1cfb | — |
| COMBAT | 0x958 | 0x7f1,0x1cbf | 0x1b2,0x33c,0x3b6,0x4f6 | — |
| COMSUBS | 0x1b1,0x2cb | 0x6de | 0x352,0x3d6,0xacb,0xc0b | — |
| CAST | 0xd85,0xe6e,0x15f0,0x1620,0x198f | 0x19e0,0x1fbc | 0x29a,0xeb2,0x11d3,0x1325,0x166d,0x1ba7 | 0x1c00 / 0x1f51 |
| CAST2 | 0x56,0x6d,0x94,0x560,0xaed,0xbe3,0xc05,0xc57,0xc79 | 0x1d | — | — |
| SHOPPES | 0x13d8,0x13ef,0x1417,0x142b,0x144f,0x1466 | — | — | — |
| BLCKTHRN | 0x3e3,0x405,0x83f,0xa34,0xb8d | — | — | — |
| OUTSUBS | 0x67b,0x698,0x896,0x8c1 | — | 0x492 | — |
| ENDGAME | 0x78f,0x987 | — | — | — |
| SJOG | 0x2218 | 0x237 | 0xc31,0x1a21,0x1c37,0x1f08 | — |
| CMDS | 0x11b2 | — | 0x9d5,0xc05,0x18ac | — |
| LOOKOBJ | — | 0x129 | — | — |
| TALK | — | — | 0x11a8 | — |
| ZSTATS | — | — | 0xe42 | — |
| FONT | — | 0x3ca, 0x88d | — | — |

**INTRO.OVL emite CERO llamadas de speaker.** Los efectos de la intro/gitana/endgame
(trueno, cartas del tarot, fanfarrias) salen por el **motor cinemático compartido de
FONT.OVL** (`font.md`: "el overlay lo comparten INTRO y ENDGAME") + las rutinas de
ENDGAME. Insumo para task #13 (RE de INTRO) y task #4 (grabación de la intro).

---

## 3. Emisores del KERNEL (leídos directamente en `ULTIMA.EXE.asm`)

### 3.1 Combate — golpe / daño (la fuente principal de `noise_burst`)

| Rutina | Call-site | Acción | Params `(step,dur,band)` | Forma |
|--------|-----------|--------|--------------------------|-------|
| `0x2a52` `combat_actor_take_damage` | `0x2a68` | Actor recibe daño (parpadea marcador `0x2a28` + resta HP en `[si+0x55b8]`) | `(10, 1600, 2000)` | ruido medio corto |
| `0x3564` (resolución de impacto) | `0x35c9` | Impacto en objetivo **vivo** en combate (bit `0x80`) | `(40, 3000, 500)` | ruido grave/pesado (golpe fuerte) |
| `0x3564` | `0x35de` | Impacto, rama genérica | `(10, 3000, 2000)` | ruido medio |
| `0x350a` (impacto en casilla) | `0x355a` | Efecto de golpe sobre coord party-relativa (dibuja `0x10e0` + tick `0x5910`) | `(10, 3000, 2000)` | ruido medio |
| `0x2fd0` | `0x2fe3` | Efecto grave (desvanecer/derrota; comprueba combate + rand) | `(40, 3000, 500)` | ruido grave largo |

`band=500` = golpe/muerte pesado; `band=2000` = golpe estándar. Distintos timbres →AV.

### 3.2 Cetro (sceptre) — `0x6221` (tone_sweep)

Handler de uso del cetro (`g_sceptre`, char `0xfc`): imprime str `0xa406` y emite
`tone_sweep(inc=0xfd2, delay=1, count=0xfde8, start=1, step=1)` → tono largo distintivo
de "poder del cetro". Luego `g_sceptre=0`.

### 3.3 Tile especial / moongate — `0x48e5` (tone_sweep)

Si el tile bajo el party (`0x4402`) `== 0xdc`: `tone_sweep(inc=0x170c, delay=1,
count=30000, start=2000, step=2)` (barrido ascendente largo), y pone `g_transport_tile
= 0x16`. Sonido de activación mágica del tile especial (moongate/whirlpool).

### 3.4 Tick de FX de tiles animados — `0x4102` (el más parecido a un "dispatcher")

Recorre el viewport 11×11 alrededor del party (o centro 5,5 en combate `g_location≥0x80`)
y por cada tipo de tile animado dispara un SFX distinto. Selector de sub-efecto en
`[bp-0xc]` (1..4); fase global en `[0x6a34]` (0..7) y `[0x6a08]` (0..0x34):

| Caso | Call-site | Sonido |
|------|-----------|--------|
| tipo 1, fase 0/4, `[0x5884]≠0` | `0x428b` tone_sweep | `(inc=3116, delay=1, count=2000, start=20000, step=-10)` pulso ascendente |
| tipo 1, fase 0 | `0x42a1` beep | `beep(freq=3000, dur=3)` |
| tipo 1, fase 4 | `0x429c` beep | `beep(freq=2000, dur=3)` |
| tipo 2 | `0x42be` noise_burst | `(step=20, dur=60, band=10000)` crepitación aguda |
| tipo 3 | `0x42c4` noise_burst | `(step=10, dur=30, band=25000)` crepitación muy aguda |
| tipo 4 | `0x42fb` tone_sweep | start indexado por tabla `[0x6a48][0x6a08]`; resto `(inc·,1,2000,·,-10)` |

**Por qué «aguda» y «muy aguda» es DERIVADO y no impresión** (adjudicado en #254, que llegó a estas
dos filas acusándolas de describir el modelo del port y no el del binario): `noise_burst` sortea un
valor en `[0x64, band]` y lo pasa por `div 0x1234DE` (`0x227b-0x2281`), cuyo cociente sale por el
puerto 0x42 en dos mitades. `0x1234DE` = **1.193.182 Hz = el reloj del 8253**, así que ese cociente
es el **contador** y la frecuencia emitida es `reloj/contador` = **el valor sorteado**. Banda 25000
⇒ sorteo hasta 25 kHz ⇒ muy aguda, en efecto. La lectura contraria («el valor es el divisor, luego
la frecuencia es `0x1234DE/valor` ⇒ 48 Hz ⇒ GRAVE») confunde el contador con la frecuencia; queda
tachada en `intro-sonidos-220.md §6`, con su guarda ejecutable.

**Tabla de datos sonora**: `[0x6a48]` (indexada por `[0x6a08]`, fase 0..0x34) alimenta el
`start` del caso 4. Es el equivalente sonoro más cercano a la "tabla de emisores": no es
un dispatcher id→params, sino una **tabla de fases de un efecto de campo animado** (campos
de fuego/veneno/energía/sueño y anim de terreno). El tipo concreto por tile ID →AV.

### 3.5 FANFARRIA DE VICTORIA — `0x4368` `sfx_victory_fanfare` (tone_sweep ×4)

3× `tone_sweep(inc=4600, delay=1, count=10800, start=300, step=6)` seguido de
`tone_sweep(inc=6100, delay=1, count=21600, start=300, step=3)`: tres notas iguales y
una final **más AGUDA** y del doble de larga.

🔴 **CORREGIDA (#212, 13-08).** Esta línea decía «una final más grave (mitad de incremento)»
y las DOS mitades de esa frase eran falsas, por el mismo error: leer `step` como si fuera
`inc`. Lo que se parte por la mitad es `step` (6→3); `inc` **SUBE** (4600→6100). Y el pitch
sale de `inc` — es la propia §1.1 de este fichero («el pitch audible es la tasa de conmutación
del gate», `dx += inc`), mientras `start`/`step` mueven el UMBRAL, o sea el DUTY. Con la
calibración del port (`SPEAKER_SAMPLE_RATE_HZ`) son ~1811 Hz ×3 y ~2402 Hz la última.
⇒ **la fanfarria tiene DOS pitches, no cuatro**, y eso es exactamente lo que el usuario
reportó el 13-08 al ganar una batalla: «suena con DOS TONOS». Guardado por aserto en
`game/tests/victory-fanfare-212.test.ts` (mutante: invertir la desigualdad lo pone rojo).

✅ **RE-CONFIRMADA contra el asm crudo (carril careo-fanfarria, 22-08)**, adjudicando la
contradicción con `cola-cast-acta.md` §6 (que decía «más grave»; corregida allí con
tachado): en `re/disasm/ULTIMA.EXE.asm`, `0x4377` empuja PRIMERO `0x11f8`=4600 (último
`push 6`) y `0x4391` empuja primero `0x17d4`=6100 (último `push 3`); el primer empuje
aterriza en `[bp+0xc]`, que es lo que `tone_sweep 0x2192` SUMA a `dx` cada muestra
(`0x21f1/0x21c4 mov ax,[bp+0xc] / add dx,ax`), y el gate conmuta con `0x21f8 cmp dx,bx`
(`0x2204 or al,3 / out 0x61`) a la tasa de vueltas de `dx` mod 65536 ⇒ pitch ∝ `inc`:
**4600→6100 = más AGUDA**, ratio ≈1,326. El 6→3 es `[bp+4]` (`0x220a add bx,[bp+4]`),
el paso del umbral = duty. El error del acta era mapear los empujes a las ranuras sin
invertir el orden.

🔴 **CORREGIDA (carril `cola-cast`, cuerpo entero leído; ver `cola-cast-acta.md` §6).**
La versión previa de esta sección decía «Sonido base de "lanzar conjuro" … Llamado por
CAST», heredado de `seg2` ("scheduler de anim de hechizo"). **Es falso.** Los ÚNICOS
dos llamadores de `0x4368` en todo el corpus, resueltos con la regla de banda por
`verify_cites.callers_of_cs` y contrastados a mano, son:

- `COMBAT.OVL:0x0d02` (`combat_main_loop`), justo tras imprimir DS `0x6f00` =
  `"\nVICTORY!\n"` y poner `g_cmb_victory_flag = 1`. La rama de DERROTA
  (`0x0cda`, `"\nBATTLE IS LOST!"`) NO lo llama.
- `CAST.OVL:0x1759`, en la cola de `use_shard_at_flame`, tras
  `"The doom of the Shadowlord <Faulinei|Astaroth|Nosfentor> is wrought!"`.

«Llamado por CAST» era cierto como FICHERO y falso como SUJETO: la rutina de CAST que
lo llama es el ritual del shard, no el despachador de conjuros. El sonido que sí emite
una rama del despachador (`CAST.OVL:0x0e5a-0x0e6e`) es un `tone_sweep` con
`push 2, 0x3e8, 0x6d60, 1, 0x2648` — el mismo que la §4.2 cataloga como «zap largo
agudo». ⚠️ **El port heredó la atribución vieja**: `game/src/core/sfx.ts:47`,
`game/src/skin/fiel/speaker.ts:395` y los tres `emitSfx({id:"cast-spell"})` de
`main.ts` tocan esta fanfarria al lanzar un conjuro. Divergencia de presentación pura
(`0x4368` no lee ni escribe globales y no consume RNG): **no mueve stream**, pero está SIN
ARREGLAR — decisión del lead.

🔴 **De esa frase se retira una MITAD (#212, 13-08): «y no la tocan al ganar».** Ya no es
cierto — desde #212 la victoria del arena SÍ emite la fanfarria, por el llamador real del
binario (COMBAT.OVL:0x0d02, predicado sobre el mensaje «VICTORY!» en `sfxForCombatEvent`),
y además PARA el juego mientras suena (`ui/combat-pacer.ts`). Lo que sigue pendiente es sólo
la otra mitad: que `cast-spell` la toque al castear. Esta divergencia tenía DOS partes —un
sitio donde sonaba de más y otro donde faltaba— y sólo se ha cerrado la segunda; quien lea
la frase entera como vigente creerá que ganar sigue siendo mudo. (Los números de línea de
`main.ts` que citaba esta entrada —2186 · 2948 · 3034— llevaban rancios desde antes: hoy son
otros. Se retiran en vez de re-fijarlos: una cita de línea en un fichero de 4.000 caduca sola.)

### 3.6 PISADA del jugador (footstep) — `0x433e` (noise_burst ×2) — cue `move-step`

`sfx_footstep 0x433e`: `noise_burst(1,25,1000)` + `delay(0x14)` +
`noise_burst(1,25,1500)` = doble burst de ruido grave, el "tk-tk" del PASO a pie.
**Corregido por la re-auditoría #51**: la versión previa lo etiquetaba "ambiente
cercano (llamado por SJOG)"; el testigo de runtime demuestra que se dispara en CADA
paso efectivo del jugador (12/12; ver §10 y `walk-sound-verdict.md`). El caller
exacto (¿SJOG?) no se pinneó, pero es irrelevante: es la pisada.

### 3.7 Flash de borde con beep aleatorio — `0x3072` `fx_flash_border` (ya en `kernel-sweep-3 §8.4`) ⚠ RNG

Barrido de borde EGA + `set_tone` (`0x30b8..0x3150`) con pitch `rand_range(0x13,0x96)`
(`0x2092`, **consume `g_rng`**), cerrado por `stop` (`0x316e`). Efecto AV de
"destello + chirrido" de eventos. Ojo paridad: este SÍ toca `g_rng` (ver task #20).

---

## 4. Emisores de OVERLAYS (params pulidos; algunos `start` computados)

### 4.1 TOWN.OVL — instrumentos, heraldo Shadowlord, fuentes

- **`0xe40` "tocar instrumento"** (`0xe6d` tone_sweep): entrada = tecla-dígito (`[bp+4]-0x30`),
  indexa **tabla de notas `[0x2746]`** → `tone_sweep(inc=nota, delay=1, count=4000,
  start=20000, step=-4)`. Una nota por dígito (arpa/laúd del bardo). La tabla `0x2746` es
  la escala de notas → derívala entera para el modo música.
- **`0x11b8` `shadowlord_announce`** (`0x11e9` tone_sweep): imprime heraldo (str `0x27b8`,
  **nombre del Shadowlord** desde tabla `[bx*2+0x27dc]`, str `0x27c4`) y emite el
  **drone del anuncio**: `tone_sweep(inc=0x19c8, delay=1, count=60000, start=2000, step=1)`
  → tono muy largo y lentamente ascendente (ominoso). **Corrige `shadowlord-urban.md:64`**,
  que lo citó como `0x11d5 (1,0xea60,0x7d0,1)`: el `call` real está en `0x11e9` y el
  5-tupla completo es `(inc=0x19c8,1,count=0xea60,start=0x7d0,step=1)` (faltaba el `inc`).
- **`0xa75` / `0x101e` noise_burst** `(step=40, dur≈3000/1600, band=500)`: golpe grave de
  evento en pueblo (fuente/portazo/derrumbe).
- **`0xfa0` glide manual** (`0xfbb` set_tone en bucle `0x3e8→0xfa`, `0xfd3` stop):
  glissando descendente 1000→250 (índice) hecho a mano con set_tone. Evento sonoro TOWN
  específico (descenso/apagado).

### 4.2 CAST.OVL / CAST2.OVL — efectos de hechizo (la mayor familia de tone_sweep)

Cada conjuro con efecto sonoro empuja su 5-tupla propia. Muestras representativas
(`inc,delay,count,start,step`):

| Overlay@off | Params | Forma |
|-------------|--------|-------|
| CAST @0xd85, @0xe6e | `(0x2648,1,28000,1000,2)` | zap largo agudo |
| CAST @0x15f0, @0x1620 | `(0xa50,1,200,0,si)` | blip corto (count = `si` runtime) |
| CAST @0x198f | `(0x1450,1,50000,1000,1)` | drone largo |
| CAST2 @0x94 | `(0x1180,1,-536,300,1)` | tono |
| CAST2 @0x560 | `(0xac8,1,12000,500,5)` | barrido |
| CAST2 @0xbe3/0xc05 | `(0xa8c,1,200,0,si)` | blip |
| CAST2 @0xc57/0xc79 | `(0xc1c,1,150,0,si)` | blip corto |
| CAST2 @0x56/0x6d/0xaed | args desde tablas `[si+0x4af6/0x4b08/0x4b2c…]` | por-hechizo (tabla de datos) |

CAST también usa **glide** (`0x29a,0xeb2,0x11d3,0x1325,0x166d,0x1ba7`; p.ej. `0x29a` =
`glide(start=750,end=2000,step=40,total=0x28)`) y **noise_burst** (`0x19e0`=`(10,3000,2000)`,
`0x1fbc`=`(step,dur,band=800)`), y un set_tone/stop propio (`0x1c00`/`0x1f51`) con freq
computada por `0x6112`. CAST2 `0x1d` usa noise via `0x405c` `(step=700,dur=1600,band=700)`.
**Tablas por-hechizo**: CAST2 `[0x4af6/0x4b08/0x4b1a/0x4b2c]` y BLCKTHRN `[bx]/[si]` →
son las "tablas de parámetros por conjuro"; conviene derivarlas junto al catálogo de magia.

### 4.3 COMBAT.OVL / COMSUBS.OVL — swing, impacto, whoosh

- COMBAT `0x958` tone_sweep `(0x… ,1,28000,1000,2)` (swing/lanzamiento).
- COMBAT `0x7f1` noise_burst `(40,3000,500)` golpe pesado; `0x1cbf` `(1,7000,600)` retumbo largo.
- COMBAT/COMSUBS **glide** ×8 (`0x1b2…`,`0x352…`): `(750→400,step,150)` y `(0x4b0…,7d0,7d0,28)`
  = whooshes de ataque/movimiento en la rejilla de combate.
- COMSUBS `0x1b1`=`(0xc1c,1,30000,1000,2)`, `0x2cb`=`(0xac8,1,5000,1000,15)`, `0x6de` noise
  `(700,1600,700)`.

### 4.4 SHOPPES.OVL — transacciones (6 tone_sweep)

`0x13d8,0x13ef,0x1417,0x142b,0x144f,0x1466`. Pares tono-ascendente/descendente
(campanilla de compra/venta, "no puedes pagar", regateo). Nota: varios `start`/`step`
reutilizan `ax` entre `push`es adyacentes → los valores exactos de las cabezas de tupla
se confirman mejor en AV, pero la forma (par corto agudo, `step` `+`/`-` = éxito/fallo)
es clara. Representativos: `@0x144f (0x8fc,1,·,·,2)` asc, `@0x1466 (0x8fc,1,·,0x8ca0,-2)` desc.

### 4.5 BLCKTHRN.OVL — palacio de Blackthorn (torturas/quiz)

`0x3e3/0x405` `tone_sweep(0xa50,1,200,0/si,0)`; `0x83f` `(0xaf0,1,13000,100,5)`;
`0xa34` args desde vars `[di]/[bx]/[si]` (tabla); `0xb8d` `(0x8e30,1,30000,2000,2)`.
Sonidos de las escenas del palacio (respuestas correctas/incorrectas, tortura).

### 4.6 DUNGEON.OVL — mazmorras (trampas/efectos)

Sólo noise_burst: `0x4b9` `(1,500,20000)`; `0x99e/0xa30` `(1,50,3500)` (gotas/eco);
`0x103c` `(1,[0xa9fb],20000)` (dur variable). + glide `0x1483,0x1cfb`. Trampas, caídas,
teleports de dungeon. LOOKOBJ `0x129` noise `(10,3000,2000)` = "look/search" reveal.

### 4.7 OUTSUBS.OVL — **APARICIÓN de Lord British (`camp_results`, 0x658–0x99b)** ⭐ re-atribuido

Los 4 tone_sweep de OUTSUBS `0x67b/0x698/0x896/0x8c1` **NO son "terreno/transporte"**: caen
dentro de `outsubs_camp_results` (entrada `0x658`), la **APARICIÓN del level-up de acampada**
(el evento gateado al 25 %; ver `oracle-camp-event.md §B/§C`, ledger #7/#27). Colocados por
posición relativa a los prints ya derivados:

| Beat de la escena | Ubicación | Sonido `(inc,delay,count,start,step)` | Rol |
|-------------------|-----------|----------------------------------------|-----|
| **"An apparition!"** (print str `0x7750` en `0x0660`) | `0x67b` | `(0xa3c, 1, 10000, 2500, 6)` | materialización — barrido único |
| Figura aparece (sprite tile `0x16`) | `0x698` ×3 en bucle | `(inc=tabla[0x3a26], 1, 5000, 200, 13)` | **arpegio de 3 notas** (tabla de pitches `0x3a26`) |
| Level-up (`0x070e`, HP=nivel·30) + cura total (`0x0820`) + `status:='G'` (`0x0828`) | — | *(sin sonido propio)* | el level-up/cura son silenciosos |
| Chime de cura por miembro (print str `0x7760`) | `0x896` | `(0x157c, 1, 5000, 200, 13)` | campanilla de curación |
| Fanfarria antes del discurso | `0x8c1` | `(0x157c, 1, **60000**, 2500, 1)` | **acorde largo sostenido** (count=60000) que precede al |
| Discurso por karma (`0x090e–0x099b`) | — | *(texto)* | … la arenga de Lord British |

**Tabla de datos sonora nueva: `[0x3a26]`** (3 words) = los pitches del arpegio de
materialización de la aparición. Derívala entera para el modo fiel. Los timbres/Hz →AV.

### 4.7b OUTSUBS.OVL / MAINOUT.OVL — mundo exterior (lo que SÍ es overworld)

OUTSUBS **glide `0x492`** = `glide(2500, 800, 1, 300)`, en una rutina **distinta**
(entrada `0x458`, fuera de `camp_results`).

★ ATRIBUCIÓN PRECISADA (30-07, tarjeta #69): esa rutina `0x0458` **es la CATARATA**, no un
efecto genérico de «transporte/hundimiento». Lo primero que hace es imprimir la cadena de
la caída — `0460 mov ax, 0x39b5` = DS 0x39b5 `"F-A-L-L-S!!!\n"` — y el glide de `0x0492`
va inmediatamente después, con el transporte oculto en `0x049d`. Sonido: barrido
**descendente** 2500→800 en 300 pasos de 1 (pendiente −5 por paso, calculada en el propio
cuerpo del glide: `43bf sub ax,[bp+0xa]` · `43c2 imul [bp-8]` · `43cc idiv cx`).

⚠ Y el offset que el port citaba para este sonido —`0xa11e`— **no es ningún identificador
de sonido**: es el destino del near-call tal como lo imprime el disasm, en espacio de
overlay (`base OUTSUBS 0xa290 + 0xa11e ≡ 0x43AE`). Corregido en `game/src/core/game.ts`
(`checkWaterfall`). Género «offset desnudo» de la tarjeta #69: el número existía como DS
—fragmento del toggle de teclado, ULTIMA.EXE 0x31e2— y por eso la cita equivocada
sobrevivió a los barridos anteriores.

Unidad del argumento NO derivada (cabo): `pcspeaker_set_tone` `0x22e2` programa el PIT con
`0x34DE12 / arg`, así que «2500» y «800» son valores del argumento, proporcionales a la
frecuencia pero no hercios. Lo que la derivación sí fija es el SENTIDO: descendente.
MAINOUT noise `0x300 (step=300,dur=2000,band=100)` + glide ×4 (`0x113b/0x11ec/0x12a6/0x13e9`).
Efectos de terreno/transporte/viento del overworld. Correlación tile→sonido →AV.

### 4.8 SJOG.OVL — search/open + rejilla de combate

`0x237` noise `(40,3000,500)` = trampa de cofre / fallo de search (grave); `0x2218`
tone_sweep en el bloque de combate `(0xc1c,1,30000,1000,2)`; glide `0xc31`, `0x1a21/0x1c37/0x1f08`
(combate).

⚠ CORRECCIÓN (2026-07-15, RE de atribución): `0xc31` NO es "search/open feedback".
Es el JIMMY de **cofre-objeto** (rutina SJOG `0xbaa`), rama de FALLO "Key broke!":
`and ax,0x7f` (=`tile&0x7F`) + `rand(1,0x1e)` vs threshold `((tile&0x7F)−DEX+0x1E)>>1`
→ `jbe` fallo → print str `0x8a6e` + `glide(0x320→0x7d0, 1, 0x32)` = 800→2000 asc +
`dec [g_keys]`. Casa EXACTO con `commands.ts jimmyLock` case `chestObject` (rand≤thr=FALLO).
Ver §10.2.

### 4.9 CMDS.OVL — comandos generales

`0x11b2` tone_sweep `(0x28a0,1,30000,2000,2)` + glide `0x9d5/0xc05` `(1000→200,·)` y
`0x18ac`. Feedback sonoro de comandos (p.ej. `Z`-stats, jimmy, mezclar reagentes).

### 4.10 FONT.OVL — motor cinemático (INTRO + ENDGAME + gitana)

`0x3ca` noise `(20,60,10000)` (= la crepitación del caso-2 de tiles: trueno/relámpago
de la intro), `0x88d` noise `(step,1200,4000)`. Vehículo de los SFX dramáticos de la
intro/endgame/gitana. **Toda la intro suena aquí, no en INTRO.OVL.**

### 4.11 ENDGAME.OVL / ZSTATS.OVL / TALK.OVL

ENDGAME `0x78f (0x2260,1,40000,1000,1)`, `0x987 (0x1450,1,50000,10000,1)` = fanfarrias/
drones del final. ZSTATS `0xe42` glide (feedback del panel Z-stats). TALK `0x11a8` glide
(bip de conversación/keyword).

---

## 5. ¿Hay un dispatcher de sonido id→params? — NO (y qué hay en su lugar)

No existe una tabla `sound_id → (freq,dur)` central ni una rutina `play_sfx(id)`. **Cada
acción emite su SFX inline** con inmediatos en el propio call-site. Lo más cercano a una
"tabla de emisores" sonora:

1. **`[0x6a48]`** (tick de FX de tiles `0x4102`, §3.4): fases 0..0x34 del efecto de campo
   animado → `start` del sweep. Derivarla entera para los campos animados.
2. **`[0x2746]`** (TOWN, §4.1): escala de **notas de instrumento** por tecla-dígito.
   Es la tabla musical; imprescindible para el modo "tocar instrumento".
3. **`[0x27dc]`** (TOWN, §4.1): nombres de Shadowlord (texto, no sonido) que acompañan
   el drone del anuncio.
4. **Tablas por-hechizo** en CAST2 (`[0x4af6/0x4b08/0x4b1a/0x4b2c]`) y BLCKTHRN (`[bx]/[si]`):
   parámetros de sweep por conjuro/escena. Derivar con el catálogo de magia.

---

## 6. Tabla MAESTRA acción → rutina → params → localización (spec task #3)

`TS`=tone_sweep `(inc,delay,count,start,step)` · `NB`=noise_burst `(step,dur,band)` ·
`GL`=glide `(start,end,step,total)` · `BP`=beep `(freq,dur)`. Hz reales →AV.

| Acción de juego | Rutina | Prim | Params derivados | Ubicación |
|-----------------|--------|------|------------------|-----------|
| Golpe estándar en combate | `combat hit` | NB | `(10,3000,2000)` | kernel `0x35de`/`0x355a` |
| Golpe pesado / a objetivo vivo | `combat hit` | NB | `(40,3000,500)` | kernel `0x35c9`, COMBAT `0x7f1` |
| Actor recibe daño (resta HP) | `0x2a52` | NB | `(10,1600,2000)` | kernel `0x2a68` |
| Desvanecer / derrota | `0x2fd0` | NB | `(40,3000,500)` | kernel `0x2fe3` |
| **VICTORIA en combate · shard deshecho** (🔴 antes «Lanzar conjuro (base)», corregido §3.5) | `0x4368` | TS×4 | `(4600,1,10800,300,6)`×3 + `(6100,1,21600,300,3)` | kernel `0x438b/0x43a5`; llamadores COMBAT `0x0d02` · CAST `0x1759` |
| Efecto por-hechizo (zap/blip) | CAST/CAST2 | TS/GL/NB | tuplas §4.2 (+ tablas `0x4af6…`) | CAST·CAST2 |
| Whoosh de ataque/movimiento (combate) | glide | GL | `(750,400,·,150)` / `(0x4b0,…)` | COMBAT/COMSUBS `0x1b2…` |
| Usar el Cetro | `0x6221` | TS | `(0xfd2,1,65000,1,1)` | kernel `0x6221` |
| Activar tile especial / moongate | `0x48e5` | TS | `(0x170c,1,30000,2000,2)` | kernel `0x48e5` |
| Campo animado — pulso | tick `0x4102` t1 | TS/BP | `(3116,1,2000,20000,-10)` / `beep(3000,3)`/`(2000,3)` | kernel `0x428b/0x42a1/0x429c` |
| Campo animado — crepitación | tick `0x4102` t2/t3 | NB | `(20,60,10000)` / `(10,30,25000)` | kernel `0x42be/0x42c4` |
| Retumbo ambiental cercano | `0x433e` | NB×2 | `(1,25,1000)`+`(1,25,1500)` | kernel `0x434a/0x4364` |
| Destello de borde + chirrido ⚠RNG | `0x3072` | set_tone | pitch `rand(0x13,0x96)` | kernel `0x30b8…0x316e` |
| Anuncio de Shadowlord (drone) | `0x11b8` TOWN | TS | `(0x19c8,1,60000,2000,1)` | TOWN `0x11e9` |
| Tocar instrumento (nota por dígito) | `0xe40` TOWN | TS | `(nota[0x2746],1,4000,20000,-4)` | TOWN `0xe6d` |
| Compra/venta/regateo en tienda | SHOPPES | TS | pares asc/desc §4.4 | SHOPPES `0x13d8…0x1466` |
| Trampa de cofre / fallo search | SJOG/LOOKOBJ | NB | `(40,3000,500)` / `(10,3000,2000)` | SJOG `0x237`, LOOKOBJ `0x129` |
| Trampa/eco/teleport de mazmorra | DUNGEON | NB/GL | `(1,50,3500)` / `(1,500,20000)` | DUNGEON `0x4b9…` |
| **Aparición del camp — "An apparition!"** | `camp_results` | TS | `(0xa3c,1,10000,2500,6)` | OUTSUBS `0x67b` |
| **Aparición — arpegio de materialización** (3 notas, tabla `0x3a26`) | `camp_results` | TS×3 | `(tabla[0x3a26],1,5000,200,13)` | OUTSUBS `0x698` |
| **Aparición — chime de cura** | `camp_results` | TS | `(0x157c,1,5000,200,13)` | OUTSUBS `0x896` |
| **Aparición — acorde largo antes del discurso** | `camp_results` | TS | `(0x157c,1,60000,2500,1)` | OUTSUBS `0x8c1` |
| Terreno/transporte/viento exterior | OUTSUBS `0x458`/MAINOUT | GL/NB | §4.7b | OUTSUBS `0x492`·MAINOUT |
| Escena palacio Blackthorn | BLCKTHRN | TS | §4.5 | BLCKTHRN `0x3e3…0xb8d` |
| Trueno/relámpago de la intro | FONT | NB | `(20,60,10000)` | FONT `0x3ca` |
| Fanfarria/drone del endgame | ENDGAME | TS | `(0x2260,1,40000,1000,1)` … | ENDGAME `0x78f/0x987` |
| Feedback comando / Z-stats / talk | CMDS/ZSTATS/TALK | GL | §4.9/4.11 | glide sites |

---

## 7. Pendiente para el catálogo AV (task #4 — grabar DOSBox con audio)

1. **Hz y envolvente reales** de cada acción (dependen de `[0x5356]`/`[0x5454]` en runtime).
2. **Timbre del ruido** (`noise_burst`) por banda: 500 vs 2000 vs 10000 vs 25000.
3. **Mapa tile-ID → tipo de FX (1..4)** del tick `0x4102` (qué tile suena a qué).
4. **Escala completa** de la tabla de notas `[0x2746]` (grabar tocando cada dígito).
5. **Efectos de la INTRO/gitana/endgame** vía FONT.OVL (INTRO.OVL no emite speaker).
6. Confirmar las **cabezas de tupla reutilizadas** en SHOPPES (§4.4) donde `ax` se
   reusa entre `push`es.

## 8. Correcciones a notas previas (registrar en el ledger)

- **`seg2.md`**: `0x43ae` (14 far-sites, "interpolador DDA de movimiento") es en realidad
  **`pcspeaker_glide`** (glissando set_tone; 27 call-sites totales, no 14 — el resto cae
  fuera de la ventana fantasma). Re-etiquetar entrada `0x842e→0x43ae`.
- **`shadowlord-urban.md:64`**: el `call` del drone está en `0x11e9` (no `0x11d5`) y el
  5-tupla completo es `tone_sweep(0x19c8,1,0xea60,0x7d0,1)` (faltaba el `inc=0x19c8`).
- **Nuevos nombres**: `0x22c0 = pcspeaker_beep`, `0x43ae = pcspeaker_glide`.

---

## 9. Estado de implementación del port (task #3)

El sintetizador (`game/src/skin/fiel/speaker.ts`) implementa **las 6 primitivas** y el
**catálogo completo (19 `SfxId`)** con los params de §6. El core emite el cue LÓGICO
(`game/src/core/sfx.ts`); la piel fiel lo sintetiza (onda cuadrada, sin samples).
Conversión param→Hz con **una** constante de calibración `SPEAKER_SAMPLE_RATE_HZ`
(→AV/#4): `pitch = (inc/65536)·SR`, `dur = count·delay·1000/SR`. El PRNG local del
ruido `[0x545c]` está portado (no toca `g_rng`).

### 9.1 Acciones CABLEADAS (emiten hoy) — actualizado por el carril audio-costuras (2026-07-22)

| Cue | Punto de emisión (port) | Vía |
|-----|-------------------------|-----|
| `combat-hit` / `combat-hit-heavy` / `combat-defeat` | derivadas de los eventos `attacked{hit}`/`died` de combate (main.ts, SIN tocar combat.ts/RNG); heavy = objetivo PJ (0x3564 por bando, §9.2) | `emitSfx` |
| `combat-damage` | daño de party por trampa/hazard (0x2a52): `partyRandomDamage` de mazmorra (eléctrico/foso/bomba/fuego, far 0x2aa8) + blips de trampa de cofre (`TrapResult.damageBlips`) | evento turno |
| `cast-spell` | `doCast` tras `castSpell` (main.ts) | `emitSfx` |
| `victory-fanfare` ⭐#212 | VICTORIA del arena: `sfxForCombatEvent` sobre el mensaje «VICTORY!» (COMBAT.OVL 0x0cf6 → fanfarria 0x0d02). **Además PARA el juego** lo que dura el cue (`CombatPacer.armBlockingPause`) y **TIRA las teclas** pulsadas mientras suena — 0x0d05 → `ULTIMA.EXE:0x1b16` vacía el búfer BIOS (`0x40:0x1A`/`0x1C` = 0x1E). El otro llamador, la cola del ritual del shard (CAST 0x1759), ya estaba cableado en #201 | `emitSfx` |
| `spell-zap` | `Game.useShard` al destruir un Shadowlord | evento turno |
| `moongate` | `Game.checkMoongate` al cruzar la puerta | evento turno |
| `move-step` ⭐#51 | `Game.move` tras `{kind:"moved"}` (paso a pie EXITOSO; footstep 0x433e) | evento turno |
| `search-fail` | `Game.search`, rama `result.entry===null` ("Nothing of note.") | evento turno |
| `dungeon-trap` | DISPARO de la trampa de cofre (kernel 0x2fd0 @0x2fe3): `openChestObject` (mundo), `openChest` de mazmorra, y arena vía mensaje «Trapped!» en `sfxForCombatEvent` | evento turno / `emitSfx` |
| `dungeon-zap` / `field-afflict` / `dungeon-fail` | mazmorra: choque con campo eléctrico (0x4b9) / afligido por campo sueño-veneno (0x99e/0xa30, por miembro) / «Failed!» de Uus-Des Por (0x1cfb) — eventos `sfx` de `dungeon.ts` traducidos en `dungeonCommand` | evento turno |
| `shop-transaction` | jingle del curandero (SHOPPES 0x13b0): `ShopConsole` (pago OK / gratis loc 5 / caridad) vía dep `sfx` | `emitSfx` |
| `shadowlord-announce` | `applyUrbanShadowlord` tras cada anuncio «An air of …» (TOWN 0x11e9; ×3 en Stonegate) | evento turno |
| `instrument-note` | `playHarpsichordNote` (TOWN 0xe34, tabla `[0x2746]`) | evento turno |
| `sceptre` | `use-tools.ts` al usar el Cetro | evento turno |
| `ambient-clock-chime` | `CoreView.ambientSfx` con contador `[0x5884]` (re-armado SÓLO al cambiar la hora — 0x514a, A3-HF2/A3-HF2.1 —, dec fase 0/4) — §5.1 ambient-audio-audit + corrección A3-HF2; validado en hardware (H-198) | bus ambiente |
| `apparition-materialize/-arpeggio/-heal-chime/-chord` | `Game.camp`, partitura §4.7a en orden | evento turno |
| `move-blocked` | `Game.move`, bump de pared "Blocked!" (§10) | evento turno |
| `cannon-fire` | `Game.fire()` (F broadside) tras el gate de perpendicularidad — glide `0x9d5` (§11, task #48) | evento turno |
| `combat-escape` | `sfxForCombatEvent` sobre el mensaje "Escape!" de `playerEscape` (§11.4, task #52) | `emitSfx` |
| `ring-vanishes` | handler de (R)eady en main.ts cuando `EquipResult.vanished` (§11.4, task #52) | `emitSfx` |

### 9.2 Acciones PENDIENTES — SALDADA por el carril audio-costuras (2026-07-22)

Todas las filas de la tabla anterior quedaron resueltas (cosidas o adjudicadas):

| Cue | Resolución (con cita) |
|-----|-----------------------|
| `combat-hit-heavy` | **COSIDO + RE-DERIVADO.** El heavy NO es «pesado/letal»: `0x3564` elige el ruido POR BANDO del objetivo (`0x35ac test [bx+2],0x80` = flag jugador del registro 0xba14, + blink HP `0x2a28` del slot `[bx+3]`): PJ → NB(40,3000,500) @0x35c9; enemigo → NB(10,3000,2000) @0x35de (= combat-ui-spec §3 «según bando»). `sfxForCombatEvent` mapea ahora `targetIsPlayer → combat-hit-heavy`; la rama `lethal` (sin base asm) retirada. `combat-damage` (0x2a52 @0x2a68) re-atribuido a los caminos de daño de trampa/hazard (callers reales: 0x2ad3 bucle-party 0x2aa8, 0x3032 ACID de 0x2fd0, 0x2b40). |
| `sceptre` | YA COSIDO (carril use-items: `use-tools.ts` emite el cue). La fila era STALE. |
| `shadowlord-announce` | **COSIDO.** El heraldo TOWN 0x11b8 SÍ está portado (`applyUrbanShadowlord`, shadowlord-urban.ts): el anuncio «An air of …» ahora emite el drone TS(0x19c8,1,60000,2000,1) @0x11e9 — que además ES el «tono de entrada a pueblo ~2223 Hz» del corpus AV (§10.2): coincide pitch (~2.6 kHz nominal, SR±13 %), duración (60000/SR ≈ 2.3 s vs ~2.5 s medidos) y frame del testigo («Enter towne JHELOM / An air of falsehood…»). En Stonegate suena 1× por SL anunciado. |
| `instrument-note` | YA COSIDO (`playHarpsichordNote` TOWN 0xe34; tabla `[0x2746]` volcada en speaker.ts y verificada contra DATA.OVL fileoff 0x2756). La fila era STALE. |
| `shop-transaction` | **COSIDO + RE-DERIVADO.** Los 6 tone_sweep de SHOPPES (§4.4) viven TODOS en UNA rutina LINEAL `0x13b0→ret 0x1469` (3 pares espejo; params COMPLETOS leídos de los push — el «→AV» de §4.4 queda resuelto: (0x100e,1,0x57e4,0x1388,±1) / (0x11b2,1,0x9c40,{1,0x9c40},±1) / (0x8fc,1,0x4650,{1,0x8ca0},±2)). Sus ÚNICOS callers: ramas C/H/R del CURANDERO (0x1611/0x1684/0x16eb) al EJECUTARSE el servicio (pago 0x146a ret 0 / gratis loc 5 / caridad Skara). SHOPPES2/3 = CERO speaker; Sold!/reactivos/gremio/establo/posada = MUDOS fiel. Emisión: `ShopConsole` dep `sfx` → `view.emitSfx`. |
| ~~`field-pulse` / `field-crackle`~~ | **RESUELTO (task #60).** No eran "campos animados": el tick 0x4102 = `ambient_sfx_tick`, ambiente por PROXIMIDAD del tile animado más cercano. Sustituidos por cues honestos `ambient-fountain` (Fountain 0xd8–db, NB 10,30,25000), `ambient-waterfall` (0xd4–d7, NB 20,60,10000), `ambient-clock-tick`/`-tock`/`-chime` (Clock 0xfa/fb). Emitidos desde la PIEL (reloj rAF = el repintado idle del original) vía `CoreView.ambientSfx`. **Campanada `[0x5884]` COSIDA** (modelo §5.1 ambient-audio-audit corregido en A3-HF2: re-armado sólo al cambiar la hora (0x514a) + dec fase 0/4 en CoreView; piel TS alineada en A3-HF2.1). Clase 4 (Codex 0x5c–5f) BANCADA — ver §9.3. Deriva: `re/notes/ambient-audio-audit.md`. |

**`dungeon-trap` RE-DERIVADO (audio-costuras)**: el bang canónico es el DISPARO de la
trampa — kernel `0x2fd0` (= SJOG 0x7050 `chestTrap`) abre con NB(40,3000,500) @0x2fe3
INCONDICIONAL. ⚠ **RE-ATRIBUIDO (#54 pieza 6a, 28-07)**: la frase que seguía aquí
—«mismo NB en el spring del search (SJOG `spawn_trap_effect` 0x1f2 @0x237, que además
ENVENENA al buscador)»— es FALSA y queda RETIRADA. Leído el cuerpo entero 0x01f2-0x02e6,
**0x1f2 es `search_remains_outcome`** (rebuscar en restos; así lo tenía ya
`re/ledger/frontier.json`) y su NB(500,3000,40) @0x0237 acompaña a **«Plague!»**
(DS 0x8606) + estado 'P' al buscador (`mov byte [bx+0x55b3],0x50`) — ni trampa ni veneno.
Las trampas del (S)earch son de la rutina HERMANA `search_trap_check` **0x2ea**, con sus
propias cadenas (DS 0x864a «no trap!» · 0x8654 «a simple trap!» · 0x8664 «a complex
trap!» · 0x8676 «a trap!»). Y «A trap!» **no existe VERBATIM en DATA.OVL**. CONTROL que
lo cierra: son dos ramas del MISMO despachador 0x0a08, cada una con su propia copia de
«Thou dost find» (0x0a13→DS 0x892c→0x2ea · 0x0a55→DS 0x893e→0x1f2).
Consecuencia práctica: cablear el flujo de restos NO reabre `dungeon-trap` — pide un cue
de PLAGA, que no existe (ticket #74). La emisión
previa en la DETECCIÓN (`trapCheck` 0x2ea, muda en el binario) fue RETIRADA — esa
conclusión SIGUE SIENDO VÁLIDA y no se toca; sólo se corrige su justificación. Cosidos
además, con cita DUNGEON.OVL (las atribuciones «gotas/eco/teleport» de §4.6 estaban
sueltas y quedan corregidas):
- `0x4b9` NB(1,500,20000) = **campo ELÉCTRICO** (0x0470: «Ouch!»+«Electric field!» DS
  0x2ca8/0x2caf + doble invert 0x89b6 + zap) → cue `dungeon-zap`.
- `0x99e`/`0xa30` NB(1,50,3500) = **blip por miembro AFLIGIDO** en los campos de
  sueño/veneno (loops 0x0948/0x09e6: status 'S'/'P' + blink 0x2a28 + NB, sólo si cae)
  → cue `field-afflict` (uno por afligido, emitido por `contestDex`).
- daño de party (bomba/foso/eléctrico/fuego): far `0x2aa8` (@0x04f7/0x0aea/0x0dc3) →
  un `combat-damage` (0x2a52 @0x2a68) POR MIEMBRO VIVO (`partyRandomDamage`); en la
  trampa de cofre, ACID=1 blip / BOMB=por vivo / POISON-GAS mudos (`TrapResult.damageBlips`).
- `0x1cfb` glide(800→2000,1,50) = **«Failed!» de Uus/Des Por** (0x1c6a rama 0x1ce4)
  → cue `dungeon-fail`. (El Klimb mode=0 nunca cae ahí; su fallo 0xee5 es mudo.)

### 9.3 Bancos honestos que quedan (audio-costuras, 2026-07-22)

- **Clase 4 Codex/estantería (0x42d2 modo ambiente, cond. 0x41f8-0x420b)** — las dos
  tablas YA ESTÁN DERIVADAS: `[0x6a48]` (fileoff 0x6a58) y `[0x6a34]` (fileoff 0x6a44)
  SON `BARD_MELODY`/`BARD_FREQ_TABLE` de speaker.ts (¡el zumbido de la cámara del
  Códice toca la canción del bardo por fases, TS(freq,1,2000,20000,-10) por tick con
  índice≠0!). Lo que FALTA es la CONDICIÓN de dos capas (`buf[0xab02+off]==0` base
  vacía Y `(buf[0xac64+off]&0xfc)==0x5c` overlay bookcase/angel): el port compone las
  capas distinto y aproximarla a «tile crudo 0x5c-0x5f» haría zumbar TODA librería de
  pueblo (¿fiel? sin testigo no se sabe — testigo trivial: DOSBox junto a una
  estantería del Lyceum). NO cableado para no fabricar.
- **Rebusque en RESTOS (S)earch** — ⚠ ENTRADA CORREGIDA (#54 pieza 6a): decía «Spring de
  trampa … “A trap!” + veneno al buscador» y NO es eso. `search_remains_outcome`
  SJOG 0x1f2: `rand(0,7)`≠0 → coloca objeto + `rand(0,0x1f)`==0x13 → **«Plague!»**
  (DS 0x8606) + NB(500,3000,40) @0x0237 + estado **'P'**; si ≠0x13, `rand(0,3)` y uno de
  «nothing!»/«worms!»/«guts!»/«a bloody pulp!». El FLUJO no está portado (y consume rand
  del stream vivo) → es hueco de MECÁNICA, no de audio; **ticket #74**, y necesita un cue
  de PLAGA (no el bang de trampa). Las trampas del Search son la rutina hermana 0x2ea.
- **DUNGEON glide 0x1483** (rutina 0x145c, arg==5: glide(3200→3500,1,0x14−8·arg)) —
  reveal de celda sin identificación firme del contexto; sin coser.
- **DUNGEON NB 0x103c** (tick 0x1020: NB(1,[0xa9fb],20000) con dur DECRECIENTE,
  armado [0xa9fb]=0xf en 0x694) = el RING del teleport/Blink de mazmorra (strings
  «Not in doorway!»/«Blocked!» DS 0x2cef/0x2d23); el teleport de mazmorra no está
  portado como tal → sin punto de emisión hoy.
- **Calibración SR del sweep**: el testigo 2223 Hz del drone 0x19c8 implica
  SR≈22.1 kHz (≈ el U=1.10 del oráculo), mientras el ancla WAV de task #72 da
  25.8 kHz (U=0.93). Dato para el pase AV #4 — aquí NO se toca `DELAY_UNIT_MS`.

---

## 10. MOVIMIENTO A PIE — el paso EXITOSO SÍ suena (footstep 0x433e) + el BUMP de pared (cue `move-blocked`)

> **⚠ CORREGIDO por la re-auditoría #51 (testigo de runtime dosbox-x).** La versión
> anterior de esta sección afirmaba "el paso EXITOSO es MUDO". **REFUTADO.** El paso
> a pie SÍ suena: cada paso efectivo dispara el wrapper kernel `sfx_footstep 0x433e`
> = `noise_burst(1,25,1000)` + `delay(0x14)` + `noise_burst(1,25,1500)` (= el cue
> `move-step`; los params ya estaban en §3.6, sólo faltaba ligarlos al paso). El
> barrido estático de abajo miró los handlers de COMMIT del paso (MAINOUT 0x0354 /
> TOWN 0x0810), que en efecto no tienen `out` — pero la llamada a 0x433e vive en
> OTRO punto del flujo de paso. Testigo: 12/12 pasos → `noise_burst 0x223c`; idle y
> pasar-turno mudos; captura de pila ret=0x434D/0x4367. Fuente: `walk-sound-verdict.md`.

**Contexto original (los handlers de COMMIT del paso no tienen `out` — cierto, pero
NO implica que el paso sea mudo; el footstep 0x433e se dispara aparte):**

- **Overworld — `move_party` MAINOUT `0x0354`** (add `g_party_x/y`, recentra chunk,
  redibuja): 0 call-sites de speaker EN ESTE HANDLER. El despacho de rumbo
  `outdoor_move` (`0x0490`) tampoco suena al aceptar el paso en su cuerpo (sólo
  `0x03e0` terreno-lento imprime "Slow progress!"/"Very slow!" — texto).
- **Pueblo — bloque de paso TOWN `0x0810`→`0x052e`**: ídem en el handler de commit.

**Además del footstep, al chocar suena el BUMP de pared** (tile no transitable, print
"Blocked!"). SFX distinto del paso, e **idéntico** en ambos contextos:

| Contexto | Call-site | Primitiva | Params | Nota |
|----------|-----------|-----------|--------|------|
| Overworld (MAINOUT) | `0x0344` `call 0xffffa0f0` | `beep` `0x22c0` | `beep(freq=0xa5, dur=0xc8)` | tras "Blocked!" (str `0x29ae`), rama NO-cactus |
| Pueblo (TOWN) | `0x0849` `call 0xffffa0f0` | `beep` `0x22c0` | `beep(freq=0xa5, dur=0xc8)` | tras "Blocked!" (str `0x26d6`) |

Resolución de la dirección far: los 3 overlays de nivel 1 (TOWN/MAINOUT/DUNGEON)
cargan en la **misma** base `0x81d0` (se pisan). Ancla: MAINOUT `0x0300` (catalogado
noise_burst) → ghost `0xa06c`; `kernel = (ghost + 0x81d0) & 0xFFFF` ⇒ `0xa06c→0x223c`
(noise_burst ✓). Aplicado al bump: `(0xa0f0 + 0x81d0) & 0xFFFF = 0x22c0 = pcspeaker_beep`
(confirmado leyendo el cuerpo de `0x22c0`: `set_tone(freq)` + `delay(dur,1)` + `stop`,
gateado por `g_unk_a9ce`, la flag global on/off — §1). Respeta pues el toggle de sonido.

**Matiz de fidelidad (→AV):** el bump por **cactus** `0x2f` NO suena en el original —
pero **SÓLO EN EXTERIOR**, y esta línea lo tenía mal en dos sitios (corregido en #224):

- ⛔ **La atribución «MAINOUT `0x329`/TOWN» era FALSA.** El `cmp …,0x2f` existe
  únicamente en MAINOUT `0x0329`. La cola gemela de pueblo es `town_move` TOWN
  `0x0600`, y su bloqueo, TOWN `0x083a-0x084c`, son tres instrucciones — `print DS 0x26d6
  "Blocked!\n"` + `call 0xffffa0f0` beep + la cola común `call 0xffff9946`: **no hay
  test de cactus en toda la rutina** (ni la salida silenciosa `0xEC` de MAINOUT
  `0x0317`). En pueblo el cactus da «Blocked!» + BEEP como cualquier pared.
  Esa barra `/` es la que sembró la premisa equivocada de la tarjeta #224.
- ⛔ **«El port no modela el "OUCH!" a pie» quedó RANCIO con #157**, que lo cableó. Y al
  compartir `resolveStep` entre capas, el port pasó a disparar OUCH! + `rand(1,8)`
  también en pueblo, donde el binario sólo pita: divergencia por EXCESO, arreglada en
  #224 apagando `onCactus` en la rama de pueblo del productor.

Alcance medido del caso (#224): el tile `0x2f` sale **16 veces en los 32 small maps y
las 16 en SinVraal's Hut** (id 15 = posición 14 de `smallmaps.json`), y las 16 tienen
vecino ortogonal pisable ALCANZABLE por BFS desde la entrada estándar (15,30) ⇒ era
disparable en las 16. Ese mapa **no tiene ni un tile navegable**, así que la variante
naval×cactus×pueblo es INALCANZABLE y se declara sin cablear.

**Portado:** cue lógico `move-blocked` (`core/sfx.ts`), síntesis `beep(0xa5,0xc8)` en
`speaker.ts`. Emitido en DOS sitios desde #224: `Game.move()` cuando `step.blocked &&
step.message==="Blocked!"` y no es cactus-de-exterior, y `Game.resolveNavalStep` cuando
`shipTryMove` devuelve outcome `"blocked"` (mismo `call 0xffffa0f0` del binario — la cola
`0x0312-0x0347` la comparten pie, montado y barco). **Corregido #51:** el "tick de paso"
SÍ existe en el binario (`sfx_footstep 0x433e`, cue `move-step`, §3.6/§10); el bump es un
evento aparte.

### 10.1 Pendientes de movimiento (no cableados)
- **Colisión NAVAL** (barco contra tile bloqueante navegando): noise_burst MAINOUT
  `0x0300` `(step=300,dur=2000,band=100)` + `damage_ship 0x109e`. Punto de emisión:
  `Game.navalMove`/`shipTryMove`. Cue candidato futuro (p.ej. `ship-collision`).

### 10.2 Segunda derivación (2026-07-15) — ⛔ REFUTADA por la re-auditoría #51 (testigo de runtime)

> **⛔ ESTA SECCIÓN SE EQUIVOCÓ.** Concluyó "el paso EXITOSO sigue MUDO; el sonido que
> el usuario oye NO es un tono de paso del juego" — FALSO. El testigo de runtime
> (dosbox-x, BPs en las primitivas de altavoz) demuestra que CADA paso a pie dispara
> `sfx_footstep 0x433e` = `noise_burst`×2. Ver `walk-sound-verdict.md` (task #51).
>
> **Por qué falló el barrido ASM de abajo:** miró `advance_clock`, `move_party`,
> `getkey` y los handlers de commit — todos correctos en que NO tienen `out` — pero la
> pisada sale de `0x433e`, invocada en OTRO punto del flujo de paso (no en esos).
>
> **Por qué falló el análisis de VÍDEO (el error más instructivo):** midió los
> transientes del paso como "pitch ALEATORIO e impuro (371/209/204/312/215/263 Hz,
> flatness 0.03-0.08)" y los descartó como "ruido de teclado mecánico, no un tono
> determinista del juego". PERO **eso ES exactamente la firma de `noise_burst`**: la
> primitiva 0x223c programa frecuencias ALEATORIAS del PRNG local en la banda
> `[100, band]` (band=1000 y 1500) → pitch impuro y variable por diseño, flatness alta.
> El razonamiento "un beep de paso sería el MISMO pitch cada vez" asumió un TONO puro;
> la pisada real es RUIDO. Los Hz medidos caen dentro de `[100,1000]`/`[100,1500]`. La
> evidencia que se usó para DESCARTAR el sonido de paso era, en realidad, la PRUEBA de
> que es el noise_burst de la pisada.
>
> Lo de abajo se conserva como registro del error (el bump, la entrada a pueblo y la
> música regional siguen siendo eventos reales y distintos del paso).

El usuario (ground truth de SU DOSBox) reportó que andar a pie SÍ suena y "distinto
según por dónde se pise". Re-derivación EXHAUSTIVA de todo el ciclo de turno + análisis
de audio de los vídeos de referencia. **Veredicto [REFUTADO]: el paso EXITOSO sigue MUDO en el
binario; el sonido que el usuario oye NO es un tono de paso del juego.**

**ASM — barrido del ciclo de turno completo (más allá de los handlers de move):**
- `advance_clock` kernel **`0x4f7c`** (lo llama TODA acción que cobra tiempo): cuerpo
  entero leído — sólo `call 0x3f36` (mul/div de tiempo) y `call 0x2092` (RNG, re-sorteo
  de Shadowlords a medianoche). **CERO** primitivas de speaker. El avance del reloj por
  paso NO suena.
- `move_party` MAINOUT `0x0354` y bloque de paso TOWN `0x052e`: 0 call-sites de speaker
  en la rama de éxito (ya en §10; re-confirmado).
- **`kernel_getkey` `0x1d5e`** (la lectura de tecla de cada comando): tras leer la tecla
  llama `0x1b24` sólo si `g_kbd_buffer_on`. `0x1b24` NO es un click: escribe el head/tail
  del buffer de teclado del BIOS (`0040:001a`/`0040:001c` = `0x1e`) → es un **FLUSH del
  buffer**, no una emisión de speaker. El driver NO hace click de tecla.
- Enumerados TODOS los `call 0xffff{a0f0,9fc2,a06c,a112,a13e,c1de}` (beep/sweep/noise/
  set_tone/stop/glide) de MAINOUT/TOWN/DUNGEON: ninguno cae en la aceptación de paso.

**Vídeo (`original/av-referencia/video-C`, andar por overworld, audio AAC):** los
transientes que coinciden con cada tecla de rumbo tienen **pitch ALEATORIO e impuro**
(371/209/204/312/215/263 Hz medidos, flatness 0.03–0.08, fracción de fundamental 0.07–0.24)
→ NO es un tono determinista del juego (un beep de paso del PC-speaker sería el **mismo**
pitch cada vez). Firma de **ruido de teclado mecánico** captado por la grabación, no del
PC-speaker. Contraste: el único tono LIMPIO del tramo (2223 Hz, flatness 0.002, sostenido
~2.5 s) coincide en el frame con "Enter towne JHELOM / An air of falsehood doth surround
thee" = **evento de entrada a pueblo** (o 1ª nota del tema de pueblo), NO un paso.

**Conciliación de "suena distinto según por dónde se pise":** son EVENTOS distintos, no un
tono por-tile: (a) el **bump** de pared `beep(0xa5,0xc8)=165 Hz` (§10, portado, verificado
sonando en navegador el 2026-07-15), (b) la **entrada a pueblo** / tono shadowlord, y (c) la
**música regional** (temas distintos por zona, ya cubierta por `ui/music.ts`/F7). El
"terreno lento" (`0x03e0` "Slow progress!"/"Very slow!") sigue siendo texto sin speaker.

**Cue PENDIENTE de derivar (→AV, task #4):** tono de **entrada a pueblo** ~2223 Hz sostenido
(¿evento propio o 1ª nota del tema de pueblo?). Falta localizar el emisor asm exacto; no se
porta hasta derivarlo. Fuera del alcance del movimiento.

### 10.3 "Feedback de comando" — RE de atribución (2026-07-15): las etiquetas del catálogo estaban MAL; NO cablear todavía

El usuario reportó "ni puertas". Al ir a cablear el "feedback de comando" de §4.8/§4.9
descubrí que las atribuciones eran especulativas ("p.ej.") y varias FALSAS. Hallazgos con
cita (por si sirve de aviso a quien retome — **no se cableó nada, sería fabricar**):

- **SJOG `0xc31` = JIMMY de cofre-objeto, rama "Key broke!"** (glide 800→2000/1/50). NO es
  "search/open" (ver §4.8). PERO el core solo llama `jimmyLock({kind:"chestObject"})` desde
  el arnés de paridad (`__parity__/cmds-run.ts`); **no hay comando VIVO** de jugador que abra
  un cofre-objeto con ganzúa en el port → sin punto de emisión. No cableable hoy.
- **JIMMY de PUERTA (el comando vivo `game.jimmy()`, SJOG `0xd4a`) es MUDO** en el binario:
  su región 0xd4a–0xe80 no llama a ninguna primitiva de speaker (glide `0x842e` solo se
  invoca en 0xc31 + 3 sitios de combate 0x1a21/0x1c37/0x1f08). El dungeon-chest (`0xc3e`)
  tampoco suena. → "ni puertas": forzar puertas NO suena en el original. Nada que portar.
- **CMDS `0x18ac`** NO es jimmy (jimmy vive en SJOG): rutina con bucle de 32 structs de 8
  bytes + `glide(0x4b0→0x7d0,1,0x28)`=1200→2000 + set `g_unk_a9fa`. Comando dueño sin trazar.
- **CMDS `0x9d5`** = `glide(0x3e8→0xc8,5,0x12c)`=1000→200 desc (largo, "power-down"); lee
  `g_party_x/y`. Comando dueño sin trazar.
- **ZSTATS `0xe42`, TALK `0x11a8`**: glides de nivel-overlay, sin derivar la sub-acción exacta.

**Estado:** quedan como **derivados-SIN-atribuir**. Cablearlos exige (a) RE de atribución
por-sitio (trazar el llamador hasta el dispatch; resolver strings DS `0x8a64/0x8a6e`; o un
BP-oráculo en el glide mientras el juego hace el comando), y (b) en el caso del cofre-objeto,
cablear primero el comando ausente. Es una tarea propia, no un cableado. Con el gate de audio
ya arreglado (§10.2 / commit des-gate), el usuario oye TODO lo YA portado; este layer de
feedback de comando es pulido de fidelidad aparte.

**Regla del mandato aplicada:** no existe emisor de paso en el asm y el vídeo no muestra
tono de juego por paso → NO se fabrica ningún "tick de paso" ni "sonido por terreno".

---

## 11. RE de ATRIBUCIÓN de los glides de comando (task #48, 2026-07-15) — RESUELVE §10.3

Traza estática por-sitio de cada glide "sin dueño" de §10.3, hasta la rutina de comando
(vía `command-dispatch.md §6` y los stubs de `dispatch_table.stubs()`), + resolución de
los strings impresos justo antes de cada glide (DATA.OVL `fileoff = DSoff+0x10`). **Los
dueños ahora están CONFIRMADOS por cita asm+string.** Se cableó SOLO el que tiene comando
VIVO y costura limpia en el port (Fire); el resto queda con dueño confirmado pero **sin
cablear**, con la razón por sitio. NADA por analogía.

### 11.1 Tabla final dueño → cue

`GL(start→end,step,total)`. Las tres formas de glide se **reutilizan** genéricamente entre
contextos (no hay un cue por-sitio en el binario):
- **GL(1000→200,5,300)** desc = "whoosh/power-down" del **cañonazo**.
- **GL(1200→2000,1,40)** [0x4b0→0x7d0] asc = "confirmar/éxito" (escape, absorción, equipar).
- **GL(800→2000,1,50)** [0x320→0x7d0] asc agudo = "alarma/negativo" (Key broke!, robo, Borrowed!).

| Sitio (fileoff) | Rutina / dispatch | Comando | Glide | Cita (string / asm) | Cableado |
|-----------------|-------------------|---------|-------|---------------------|----------|
| CMDS `0x9d5` | sub `0x0962` (broadside), llamado en `0x0b0f` desde Fire `0x0aea` | **(F)ire** overworld | GL(1000→200,5,300) | glide tras el gate de perpendicularidad; rama fallo imprime "Fire broadsides only!" `0x42cd` | ✅ `cannon-fire` |
| CMDS `0x0c05` | Fire `0x0aea`, rama de arena de combate | **(F)ire** cañón en arena | GL(1000→200,5,300) | "BOOOM!" `0x42f2` justo antes del glide | mismo cue; la rama de arena no es comando vivo distinto en el port (`Game.fire()` modela el broadside overworld = `0x0962`) |
| CMDS `0x18ac` | escape handler `0x17ec` (stub `0x7d8e`) | **Escape** de combate (éxito) | GL(1200→2000,1,40) | "Escape" `0x4574`, "-Not yet!" `0x4587`; scan de 32 structs de actor buscando `g_cmb_victory_flag` | ✅ `combat-escape` (#52) |
| SJOG `0x1c37` | rutina `0x1bb2` (rejilla de combate) | **Escape** de combate (por miembro) | GL(1200→2000,1,40) | "Escape!" `0x8eae`, "All must use the same exit!" `0x8e88` | ✅ `combat-escape` (#52) |
| ZSTATS `0xe42` | `try_equip_or_unequip` `0x0c5c` (comando **(R)eady**) | **"Ring vanishes!"** (anillo consumido 1/16) | GL(1200→2000,1,40) | print str `0x995e`="Ring vanishes!"; gate `rand(0,15)==0` (`0x0e11`); set member+`0x1f`=0xff | ✅ `ring-vanishes` (#52) |
| SJOG `0x1f08` | rutina `0x1ea4` (stub `0x7e66`) | actor **absorbido** en combate | GL(1200→2000,1,40) | "\<actor\> is absorbed!" `0x8f02` | NO — sin mensaje/mecánica de absorción en el port (grep vacío) |
| SJOG `0x1a21` | rutina que fija `g_torch_mins=0x64` | coger **ANTORCHA DE PARED** ("Borrowed!") | GL(800→2000,1,50) | "Borrowed!" `0x8de8` + set `g_torch_mins=0x64` | ✅ `torch-borrowed` (carril get-torch): la rama sconce 0xB0/0xB1 de `game.get()` ya está calcada (tile→0x44, `torchTurns=0x64`, "Borrowed!") y emite el cue con GL(800→2000,1,50). El "Borrowed!" de los platos (stealFood) sigue siendo la divergencia Clase C #69 y NO lleva cue |
| TALK `0x11a8` | rama de robo de **(T)alk** | robo DETECTADO al conversar | GL(800→2000,1,50) | "Something was stolen!" `0x94dc` justo antes del glide | NO — el port NO emite "Something was stolen!" (shadowlord-urban.ts: "NO posee NPCs"); sin costura |

Strings DS del mandato resueltos: `0x8a64`="Success!\n", `0x8a6e`="Key broke!\n" (confirma
§4.8 SJOG `0xc31` jimmy de cofre-objeto). El jimmy de PUERTA (`0xd4a`) sigue MUDO (§10.3).

**Hallazgo #52 — "Borrowed!" es ANTORCHA, no comida**: SJOG `0x1a21` fija `g_torch_mins=0x64`
antes del glide → el "Borrowed!" del binario es **coger una antorcha de pared**. El port ya lo
documenta al revés (game.ts:2952: lo imprime en el robo de PLATOS 0x9A/0x9B/0x9C, marcado Clase C
#69 pendiente de oráculo). Por eso NO se cabló: sería atar el glide de la antorcha a un evento
divergente. Queda para cuando se cierre el #69 (deltas food/karma de platos por oráculo).

### 11.2 Correcciones a §10.3

- **CMDS `0x9d5` YA trazado**: es la **(F)ire** (broadside), NO un "power-down sin dueño".
  El sub `0x0962` (que el port ya cita en `game.ts:2526` / `transport.ts:476` como broadside)
  contiene el glide; Fire lo llama en `0x0b0f`. Corrobora la RE previa del port.
- **CMDS `0x18ac` YA trazado**: es el **Escape** de combate (rutina `0x17ec`, imprime
  "Escape"/"-Not yet!" y barre `g_cmb_victory_flag`), NO "dueño sin trazar".
- **ZSTATS `0xe42` / TALK `0x11a8` YA atribuidos** a Ready-equip y a Talk-robo (arriba).

### 11.3 Cableado del port — tanda #48 (Fire)

Cue lógico nuevo **`cannon-fire`** (`core/sfx.ts`) = `GL(1000→200,5,300)` (`skin/fiel/speaker.ts`),
emitido en `Game.fire()` **tras** superar `broadsidePerpendicular` (posición exacta del glide
`0x9d5`, que suena tras el gate y ANTES de resolver el objetivo del ray). Una emisión por
comando F. Tests: `naval-live.test.ts` (perpendicular emite `cannon-fire`; paralelo rechazado NO).

### 11.4 Cableado del port — tanda #52 (Escape + Ring vanishes)

Dueños ya confirmados en #48 → cablear no es analogía, es port pendiente. Emisiones **puramente
presentacionales** (cero rands nuevos; el gate de paridad completo lo prueba):

- **`combat-escape`** = `GL(1200→2000,1,40)`: derivado en `sfxForCombatEvent` (`core/sfx.ts`) del
  evento `message` con texto byte-exacto **"Escape!"** (huida del PJ; el enemigo huye con "X
  escapes!" con nombre → no colisiona). `main.ts routeCombatSfx` pasa `e.text`. Cubre SJOG `0x1c37`
  y CMDS `0x17ec` (misma partitura). Test en `sfx-bus.test.ts`.
- **`ring-vanishes`** = `GL(1200→2000,1,40)`: emitido en el handler de **(R)eady** (`main.ts`,
  `game.readyItem`) cuando `EquipResult.vanished` (ZSTATS `0xe42` "Ring vanishes!" 0x995e, 1/16).
  El rand `0x0e11` ya lo tira `readyItem` por el stream vivo → cue presentacional.

**NO cableados** (dueño confirmado, sin costura fiel): `is absorbed!` (sin mecánica en el port),
`Borrowed!` (owner = antorcha; el "Borrowed!" del port es un robo-de-plato divergente Clase C #69),
robo en Talk (el port no emite "Something was stolen!"). Documentados con motivo arriba; se
retoman cuando el port modele esos eventos (o cierre el #69).
