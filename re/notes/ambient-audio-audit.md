# Audio AMBIENTAL por PROXIMIDAD — la fuente que suena (task #60)

Reporte del usuario: *"las FUENTES no suenan en el port y en el original sí"*. Sospecha
del usuario: familia entera de ambientes por cercanía (fuente, ¿reloj, molino, fragua?).

**Veredicto: EXISTE una familia ambiental por proximidad y es UNA sola rutina — el
tick de FX de tiles animados `0x4102`** (ya catalogado en `sfx-catalog.md §3.4` pero
SIN portar, §9.2 "field-pulse/field-crackle sin punto de emisión"). Escanea el
viewport visible, elige el tile animado de cierta clase MÁS CERCANO al party, y emite
su SFX. Los IDs de tile confirman la semántica exacta (Redux `TileData.json` +
`tile-anim-census.md`, éste AV-validado con el vídeo L8 de la fuente). No hay "molino"
en U5; la fragua/fuelle ANIMA pero es MUDA (no cae en ninguna clase). El clásico
"sonido de la fuente al pasar cerca" = clase 3.

---

## 1. El mecanismo — `0x4102` (`ULTIMA.EXE.asm`, líneas 7123–7339)

`ambient_sfx_tick 0x4102` (yo lo bauticé así; el catálogo lo llamaba "tick de FX de
tiles animados"). Cuerpo íntegro leído. Dos fases:

### 1a. SELECCIÓN — el tile animado MÁS CERCANO gana (esto ES la "proximidad")

- Centro = `(g_party_x, g_party_y)` en overworld/pueblo; `(5,5)` si `g_location>=0x80`
  (combate; el centro de la rejilla). `0x4114`.
- Barre el **viewport visible** (ventana ~11×11 alrededor del centro; `[bp-0xa]` de
  `cx-5` a `cx+5`, `si` de `cy-5` a `cy+5`).
- Por cada casilla calcula la **distancia euclídea al cuadrado** `(x-cx)²+(y-cy)²`
  (`0x418b`/`0x41ad`). El "mejor" se guarda en `[bp-2]` (init `0x33`=51; el peor rincón
  del 11×11 dista 50 < 51 ⇒ toda la ventana cuenta). Sólo se considera un tile si su
  distancia `< [bp-2]` (`0x41b8 jge`), y al aceptarlo se actualiza `[bp-2]`.
- Lee el tile con `get_tile_ptr 0x4402(x,y)` (`0x41c1`) y lo **clasifica** en `di=1..4`:

  | Clase | Test asm | Tiles | Objeto (Redux) |
  |-------|----------|-------|----------------|
  | **1** | `(tile & 0xfe) == 0xfa` (`0x41d0`) | `0xfa,0xfb` | **Clock (reloj)** |
  | **2** | `(tile & 0xfc) == 0xd4` (`0x41df`) | `0xd4–0xd7` | **Waterfall (cascada)** |
  | **3** | `(tile & 0xfc) == 0xd8` (`0x41ed`) | `0xd8–0xdb` | **Fountain (fuente)** ⭐ |
  | **4** | `buf[0xab02]==0 && (buf[0xac64] & 0xfc)==0x5c` (`0x41f8`) | overlay `0x5c–0x5f` | **Bookcase/Codex** |

  ⇒ **Bellows `0xfc,0xfd` NO cae en clase 1** (`0xfc&0xfe=0xfc≠0xfa`): la fragua/fuelle
  anima pero es **MUDA**. Torture `0x80–83`, SnakeSign `0xec–ef`, Moongate `0xdc`:
  animan, no suenan (no hay clase para ellas).

- Resultado: `[bp-0xc]` = la clase del **único tile de menor distancia**. Una casilla,
  un sonido por tick. Empates → gana el último en orden de barrido.

### 1b. EMISIÓN — `switch(clase)` en `0x4247`, con un contador de fase `[0x6a34]` (0..7)

`[0x6a34]` es un contador propio del ambiente (0..7), se **incrementa cada llamada** a
`0x4102` (`0x4327`, wrap en 7). `[0x5884]` = **la hora en formato 12h** (nº de campanadas;
puesto desde `g_hour` en `0x5164–0x5183`: hora 0→12, `>12`→hora-12, resto→hora). `TS`=
tone_sweep · `NB`=noise_burst · `BP`=beep (params exactos = los inmediatos empujados):

| Clase | Objeto | Condición | Primitiva | Params | Call-site |
|-------|--------|-----------|-----------|--------|-----------|
| 1 | Clock | `[0x5884]≠0` y fase∈{0,4} | `TS` | `(0xc2c,1,0x7d0,0x4e20,0xfff6)` = (3116,1,2000,20000,-10) | `0x428b` |
| 1 | Clock | `[0x5884]==0`, fase 0 | `BP` | `beep(0xbb8,3)` = (3000,3) — **TIC** | `0x42a1` |
| 1 | Clock | `[0x5884]==0`, fase 4 | `BP` | `beep(0x7d0,3)` = (2000,3) — **TAC** | `0x429c` |
| 2 | Waterfall | (sin gate de fase) | `NB` | `(0x14,0x3c,0x2710)` = (20,60,10000) | `0x42be` |
| 3 | **Fountain** | (sin gate de fase) | `NB` | `(0xa,0x1e,0x61a8)` = **(10,30,25000)** | `0x42c4` |
| 4 | Codex/bookcase | `tabla[0x6a48][0x6a08]≠0` | `TS` | `(tabla[0x6a34+...],1,0x7d0,0x4e20,0xfff6)` | `0x42fb` |

Y en `0x430e`: si `[0x5884]≠0` y fase∈{0,4}, `dec [0x5884]` → **el reloj DA la hora**:
mientras quedan campanadas usa el `TS` (0x428b) en vez del tic/tac, y descuenta una por
tic. Agotadas (`[0x5884]==0`) vuelve al tic-tac beep. Es el reloj de pie dando las horas.

**Semántica por clase, en una frase:**
- **Clase 3 (Fountain)** = burbujeo: `NB(10,30,25000)` en CADA tick que la fuente sea el
  animado más cercano. Sin gate de fase ⇒ suena continuo. **Es lo que el usuario echa en
  falta.**
- **Clase 2 (Waterfall)** = rumor de cascada: `NB(20,60,10000)`, también continuo.
- **Clase 1 (Clock)** = tic-tac: `beep(3000,3)` en fase 0, `beep(2000,3)` en fase 4 (2 de
  cada 8 ticks) + campanadas al dar la hora.
- **Clase 4 (Codex/bookcase)** = zumbido místico con tabla de pitch `[0x6a48]`; sólo en la
  cámara del Códice / estanterías animadas. Muy nicho.

## 2. La cadencia — el bucle de ESPERA de tecla (idle), no el turno

`0x4102` lo llama SÓLO `viewport_redraw 0x5910` (`0x5a1a`), gateado por `[0x5891]`
(latch "ya hecho este ciclo"; se rearma a 0 al tope de `0x5910` si `g_time_spell==0x54`
= tiempo fluyendo normal; un Time-Stop lo congela). `0x5910` a su vez lo llama el
**bucle de espera de tecla** `0x1070` (`0x10d0`), que sondea el teclado y **cada 8
iteraciones** (`test di,7; jne`) repinta+ambienta, hasta que hay tecla o `di==0x100`.

⇒ **El ambiente suena mientras el jugador está QUIETO esperando input** (no es un
tic-por-turno: pasar turno/andar procesan y salen del bucle). El contador `[0x6a34]`
0..7 avanza una vez por repintado. Es la clásica "capa de ambiente" que late de fondo
en el mapa animado. Sin RNG del juego: `noise_burst` usa su PRNG LOCAL `[0x545c]`
(§1.2 catálogo) ⇒ **cero impacto de paridad** (no toca `g_rng`).

## 3. Correcciones al catálogo previo (`sfx-catalog.md`)

- §3.4 / §9.2 colapsaban las 4 clases en dos cues vagos **`field-pulse`/`field-crackle`**
  y las llamaban "campos animados / fields". **Son ambientes por PROXIMIDAD de objetos
  concretos**, no fields: clase 1=reloj, 2=cascada, 3=fuente, 4=Codex. Los params que el
  catálogo ya tenía cuadran (field-pulse = clase-1 TS de campanada; field-crackle = clase-2
  NB de cascada), pero faltaban la clase-3 (fuente `NB(10,30,25000)`) y los beeps tic-tac.
- El nombre honesto de `0x4102` es **`ambient_sfx_tick`** (elige-cercano + emite), no un
  "dispatcher de campos".

## 4. Puerto (game/src) — commit(s) `fiel/ambient-audio`

Cues nuevos (reemplazan los placeholders `field-pulse`/`field-crackle`, nunca cableados):
`ambient-fountain` NB(10,30,25000) · `ambient-waterfall` NB(20,60,10000) ·
`ambient-clock-tick` beep(3000,3) · `ambient-clock-tock` beep(2000,3) ·
`ambient-clock-chime` TS(3116,1,2000,20000,-10). Emisión en la PIEL (no en el core: el
original ambienta desde el bucle de repintado idle, que en el port es el reloj rAF de la
piel, no un turno): `FaithfulSkin.tickAmbient()` → `CoreView.ambientSfx(phase)`, que lee
los tiles **CRUDOS** del viewport 11×11 (`map.tileAt`, como el asm `get_tile_ptr 0x4402`,
NO el `snapshot.window` censurado por luz — clases 1-3 no se gatean por visibilidad, así
la fuente burbujea también de noche), elige el más cercano (`ambientCueForTiles`, orden de
barrido x-externo/y-interno = el asm, empates a menor x) y lo emite por el bus `onSfx`.

**CABLEADO hoy: fuente + cascada + reloj tic/tac.** Ver §5 para lo que queda (clase 4 y
la campanada del reloj), con el spec EXACTO para cerrarlo sin re-derivar.

## 5. Cabos precisos (derivados; pendientes de cablear — no fabricar)

### 5.1 Reloj — la CAMPANADA de la hora (`[0x5884]`) — derivado, pide testigo de oráculo

El reloj tiene DOS sonidos según `[0x5884]`: TS(3116,1,2000,20000,-10) "campanada"
(`0x428b`) si `[0x5884]≠0` y fase∈{0,4}, o beep tic/tac (`0x42a1`/`0x429c`) si `[0x5884]==0`.
Lo que HOY cableo es el tic/tac (`[0x5884]==0`). La campanada NO, porque su cadencia está
ENTRELAZADA y necesita testigo para no fabricar el patrón audible:

- **`[0x5884]` = la HORA en formato 12h** (`0x5164–0x5183`: `g_hour==0→0xc`; `>0xc→hora-12`;
  resto→hora). Puesto por la rutina de luz/tiempo dentro de `advance_clock 0x4f7c`, que
  corre en **CADA acción que cobra tiempo** (≈cada turno). ⇒ `[0x5884]` se **re-arma a la
  hora cada turno**, y como nunca es 0 tras eso (medianoche→12), la rama por defecto es la
  CAMPANADA, no el tic/tac.
- **Decremento GLOBAL**: `0x430e–0x4323` hace `dec [0x5884]` en fase 0/4 (si ≠0) en el
  epílogo de 0x4102, **independientemente de la clase**. ⇒ durante el idle, `[0x5884]` baja
  de `hora` a 0 en `hora` golpes de fase-0/4; mientras >0 el reloj DA la hora (TS ×`hora`),
  al llegar a 0 pasa a tic/tac.
- **Consecuencia (a confirmar con oráculo)**: cerca de un reloj, tras cada turno el reloj
  "da la hora" (`hora` campanadas) y luego tictaquea si el idle dura. Es un patrón raro
  (campanea por turno, no por hora) → **antes de cablearlo, BP en `0x428b` vs `0x42a1` con
  el party junto a un reloj**, pasando turnos a distintas horas, para verificar el patrón
  real (¿campanea cada turno? ¿sólo al cambiar de hora?). Modelo portable ya listo si se
  confirma: contador `[0x5884]` en el ticker, re-armado a la hora en `onTurn`, decrementado
  en fase 0/4. Cue `ambient-clock-chime` ya definido y sintetizado; sólo falta la costura +
  el testigo.

  **COSTURA COSIDA (carril audio-costuras, 2026-07-22)** con el modelo derivado tal cual:
  `CoreView.clockChimeCounter` se re-arma a `chimeHour12(hour)` en `notifyTurn` (=
  advance_clock por acción-con-tiempo) y se decrementa en el epílogo de `ambientSfx`
  cuando `fase&7 ∈ {0,4}` (global, independiente de la clase — 0x430e); la selección pura
  (`ambientCueForClass` caso 1) emite `ambient-clock-chime` mientras el contador >0 en
  fase 0/4 (gate 0x4262/0x4269 → 0x428b) y tic/tac con contador 0. El PATRÓN AUDIBLE
  resultante (dar la hora tras cada turno junto a un reloj) sigue ⚠ Clase C calibrable:
  el testigo BP 0x428b-vs-0x42a1 queda en la cola del usuario; si lo refuta, el knob es
  el re-armado (p. ej. sólo al CAMBIAR de hora), no la selección.

  **CORRECCIÓN (A3-HF2, 2026-09-27) — el modelo «re-arma cada turno» es FALSO, refutado por los bytes.**
  `advance_clock` 0x4f7c guarda la hora en `[0x5880]` ANTES de sumar (0x4fa0 `mov al,[0x587f]; mov [0x5880],al`)
  y en 0x514a-0x5151 (`mov al,[0x5880]; cmp [0x587f],al; je 0x5186`) SALTA el bloque 0x5164-0x5183 si la hora
  no cambió. ⇒ `[0x5884]` se re-arma **sólo al cambiar de hora** (el knob que esta nota ya nombraba). Censo de
  DS:0x5884 en ULTIMA.EXE + todos los .OVL (`re/tools/a3_hf2_ds_census.py`): un único sitio escritor (0x516b/0x5183).
  El port nativo lo corrigió en A3-HF2 (`ALPHA3_AUDIO.md` §24; `native/core/a3-hf2-derivation.log`). La piel TS
  (`skin/coreview.ts` `notifyTurn`) sigue con el modelo viejo: sólo presentación, sin fixture que la fije.

### 5.2 Clase 4 — Codex/estantería (`0x5c–0x5f`) — spec exacto, sin cablear

Condición (`0x41f8–0x420b`): `buf[0xab02+off]==0` (capa BASE vacía) **Y**
`(buf[0xac64+off] & 0xfc)==0x5c` (capa OVERLAY = BookcaseLeft/Right, CodexAngelLeft/Right).
Emisión (`0x42d2`): `v = tabla_A[0x6a48][ [0x6a08] ]` (0x6a08 = fase 0..0x34); si `v≠0`,
`tone_sweep(inc = word tabla_B[0x6a34 + 2·v], delay=1, count=2000, start=20000, step=-10)`.
Falta para cerrarlo: (a) **modelar las dos capas** `0xab02` (base) y `0xac64` (overlay) que
el port compone distinto; (b) **derivar las dos tablas de datos** `[0x6a48]` (0x35 bytes,
índices) y `[0x6a34]` (words = pitches) → **→AV** (datos, no ley). Es el zumbido de la
cámara del Códice; muy nicho.

## 6. Testigo de oráculo
**Fuente/cascada/reloj-tictac: NO hizo falta** — triple confirmación convergente: (a)
lectura estática decisiva de `0x4102`; (b) IDs de tile confirmados por Redux
(`Fountain1-4=0xd8–db`, `Waterfall1-4`, `Clock1-2`, `Bellows1-2`) y por
`tile-anim-census.md` (AV-validado, vídeo L8 de la fuente); (c) **el propio usuario
confirma que la fuente suena en su DOSBox** (ground truth). La cadencia exacta en ms del
repintado idle es →AV (calibración, no mecanismo).
**Campanada del reloj (§5.1): SÍ pide testigo** antes de cablear (patrón `[0x5884]` raro).
