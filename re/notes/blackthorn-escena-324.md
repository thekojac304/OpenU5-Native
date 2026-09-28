# La ESCENA de la captura de Blackthorn (#324) — anim_vm y sus cinco guiones

Derivación estática COMPLETA de la mitad visual de `blackthorn_capture`
(BLCKTHRN.OVL 0x060e) que la ficha #324 declaraba sin portar: «7 beats volcados de
golpe, escalada de sprites y VM de cutscene». Fuentes: `re/disasm/BLCKTHRN.OVL.asm`
(rebase kernel `(NNNN + 0xA290) & 0xFFFF`, §0 de blackthorn.md), DATA.OVL
(bytecodes y tablas, fileoff = DS + 0x10), MISCMAPS.DAT (registro 0). Port en
`game/src/core/world/blackthorn-scene.ts` (guiones puros) +
`blackthorn-capture.ts` (orquestación) + `ui/blackthorn-scene-pacer.ts`
(presentación); mapa como asset del extractor (`shrine-scene.json` clave
`capture`, seek 0 — parser `extractor/src/parsers/shrine-scene.ts`).

## 1. anim_vm (0x00be) — el intérprete de cutscene

Jump-table `jmp word cs:[bx-0x5bfa]` = fichero 0x0176 (cs offset = fileoff +
0xA290), 9 entradas + los casos especiales del despachador 0x0243-0x0267:

| op | operandos | efecto |
|---|---|---|
| 0 | — | fin |
| 1 | — | el siguiente move es DUAL (2º byte slot/dir, 0x00d0) |
| 2 | n | repeat = n (0x00d8) |
| 3 / 4 | — | pisada ON / OFF (di, 0x00e8/0x00ee) — **no aparecen en estos guiones: di=1 siempre** |
| 5 | n | pausa run-n-frames `0x3AE6(n)` (0x00f4) |
| 6 | t,x,y | plot tile en el buffer de sala: `[0xAD14 + y*32 + x] = t` (0x0106) |
| 7 | — | tick/redraw kernel 0x5910 (0x013c) |
| 8 | — | beep_delay(repeat) (0x0142) |
| ≥0x10 | (dual: 2º byte) | PASO ×repeat: base&0xFC→slot (0x10→6, 0x14→7, 0x18→8, 0x1C→0, 0x20→1; `dir_to_delta` 0x002e), &3→dir (0=N dec y · 1=E inc x · 2=S inc y · 3=W dec x); por paso, si di: beep_delay(1) |
| 9 | n | borra el objeto n: tiles +0/+1 = 0 (0x0150) — **x/y QUEDAN** |

`beep_delay(n)` (0x0000) = n × { `sfx_footstep` kernel 0x433E + `0x3AE6(2)` } —
NO es un beep: son PISADAS (sfx-catalog §: `0x433e = noise_burst×2`).

Objetos: `g_world_objects` 0x5C5A, 8 B/slot, +2=x +3=y (adjudicado contra el
escritor NPC.OVL 0x0916-0x0926: scratch_x→+2, scratch_y→+3). Mapa de sala
ROW-major `[y*32+x]` — adjudicado por triple cruce: el compañero del guion 0x36DA
TERMINA en (5,7) y el plot 0x82 cae en la celda 229 = 7·32+5; los grilletes 0x85
del mapa caen EXACTOS en los 6 asientos de la tabla 0x1F0A leída con x=0x1F42; y
la escalada escribe la celda 293 = 9·32+5 = el reloj de arena que 0x36DA planta
en (5,9).

## 2. La secuencia completa de 0x060e (con offsets)

1. `0x0652` print venda → **pausa(2) + apagón** (`0x0676 set_color(0)` +
   `0x0689 fill_rect(8,8,183,183)`) + 5 arrastres `{delay_ticks(5) 0x20FA +
   pisada}` (0x069B) → print drag (0x06B0) → 18 arrastres más (0x06B9; con
   `g_unk_a9ce`=0 —sonido OFF— serían 3; el port modela ON).
2. Montaje (0x06E5-0x07C9): limpia 32 slots, `g_location=0xFF`, carga
   MISCMAPS[0:0xB0] y MISCMSG, expande el mapa, sienta a la party
   (tabla 0x1F0A fila numLiving → códigos → x=0x1F42, y=0x1F48; tile por clase:
   strchr 0x4D76 en "AMBFDTPRS" DS 0x701A → tabla 0x1ADE: A→0x4C, M→0x40,
   B→0x44, F→0x48, resto 0x4C; +0x100 en atlas). Pausa(0x10) → print chained →
   pausa(0x32) → print Footsteps! → beep_delay(8).
3. Guardias slots 6/7 (`write_object_record(idx, f5..f0)` 0x3A74: f3=y=0xA,
   f2=x=4/6, tile 0x70→0x170) → **guion 0x3702** (`08 01 10 14 02 03 01 13 15
   05 08 00`): N dual, W/E ×3 dual → (1,9)/(9,9), pausa(8).
4. Materialización de Blackthorn: `tone_sweep 0x2192(0xAF0,1,0x32C8,0x64,5)`
   (0x082B, notación PUSH) + slot 8 = tile 0x16 (0x116 HolyFloorSymbol, 0x0842)
   + `fx_tile_fizzle_in 0x1068(5,5,0x178)` (0x0857) + slot 8 = 0x78 (0x178
   Blackthorn1, 0x0863) + pausa(8).
5. Saludo (0x087F, nombre DS 0x55A8) → **getkey 0x266C** (0x0894) → «GUARD!
   Release…» (0x0897, género +9: 0x0C lady / 0x0B man) → **guion 0x370E**
   (`05 0b 13 02 04 10 11 00`): el guardia A (1,9)→W→N×4→E→(1,5), hacia el
   Avatar — y «Wait!» (rec11, 0x08C6) → **getkey** (0x08CD) → interrogate.
6. Interrogatorio (0x054A): primer fallo → rec7 → **guion 0x36DA** (0x051C
   `call 0x523`): el guardia A marcha al compañero (slot 1) de (7,5) a la MESA
   (5,7), plot 0x82 (TortureTableWithBody1), objeto borrado, A vuelve a (1,5); el
   guardia B (9,9) W×3, planta el reloj LLENO plot 0xE9@(5,9), E×3 → rec8+nombre
   +' die!" ' → **getkey** (0x053F) → '\n\n'. Fallos con warned: advance_clock(2)
   (#288) + status redraw 0x2900 + switch ronda: **si==1→0xEB · si==2→0xE8**
   (0x05DA/0x05E2) sobre la celda 293. **El caso si==0→0xEA (0x05D2) es
   INALCANZABLE** (warned se arma en la ronda 0) — código muerto, documentado y
   no portado. si==3 → sacrifice_member(1).
7. `sacrifice_member` (0x03AE): print rec4/rec5 según modo → pausa(10) → SIRENA:
   dos bucles espejo de `tone_sweep(0xA50,1,0xC8,si,0)`, si=2000→25000→2000 paso
   0x32 (0x03D0-0x0411) — **la MISMA familia y constantes que el ritual del
   shard** (CAST 0x15DD, cue `shard-sweep` reusado) → `explosion_fx_at_cell`
   0x3522 sobre x/y del slot 1 (0x0414; como op 9 retiene x/y, la explosión cae
   en la mesa (5,7) si hubo aviso o en el asiento (7,5) si no) → objeto slot 1
   apagado → celda 229 = 0x80 (TortureChair1, 0x0429) → roster (slot 15 0x5788 +
   compacta) → modo 1: '\n\n'+nombre+' is sliced in half! ' → **getkey**
   (0x04F6) → rec6 treachery → putchar('\n').
8. Cierres: traición/mazmorra → **getkey + guion 0x369E** (0x0510): la puerta
   OESTE (0,4) se abre (plot 0x44), el guardia A escolta al AVATAR (slot 0)
   fuera —(3,5)→(0,4)→(0,2), el corredor de celdas NO— la puerta se cierra
   (plot 0xBB), y Blackthorn + ambos guardias desfilan por la puerta SUR
   (4-6,10) con beep_delay(6) de pisadas finales. Péndulo → NO pasa por 0x0510;
   `0x08D9 cmp [0x5C9A],0` (slot 8 vivo) → **guion 0x3716**: Blackthorn E,S×5 y
   fuera. Depósito 0x08E7: pantalla restaurada, (10,7), keys=0, loc 0x12.

## 3. Qué se portó y cómo

- Guiones como DATA derivada (ops espejo del bytecode, bytes verbatim en los
  docblocks) + un runner puro; beats con snapshot de figuras + parches de mapa +
  frames (unidad 0x3AE6 = PAUSE_UNIT_MS 55, la calibración compartida).
- Presentación: `BlackthornScenePacer` (patrón ShrineScenePacer #277; escena
  PERSISTENTE entre prompts), bake en coreview con capa de motion propia
  (patrón #363/#366: el shader compone las figuras con transparencia), esperas
  de tecla con el marcador `shrine-key-wait` (#294, mismo kernel 0x266C).
- Audio: pisadas = cue `move-step` (mismo 0x433E); sirena = cue `shard-sweep`
  (misma familia/constantes); materialización = cue nuevo
  `blackthorn-materialize` (0xAF0,1,0x32C8,0x64,5). Explosión = evento
  `cell-explosion` (#201) con dx/dy = celda − 5 (el pintor suma al centro 11×11).
- Sin asset (clave `capture` ausente): degradación al chorro previo (sólo
  texto), el discriminante que deja intactos los arneses puros.

## 4. Divergencias DECLARADAS (Clase C / blancos)

- **Textura del fizzle-in** (0x1068 LFSR) y del explosion_fx (0x3522): Blackthorn
  aparece al corte y la explosión usa el pintor de #201 — mecanismo visual
  aproximado, celdas y orden exactos.
- **'\n\n' post-tecla**: 0x6FB4 (tras el getkey del aviso) y el putchar('\n') de
  0x0500 van fundidos en las plantillas del port textual previo (corpus i18n
  intacto) — divergencia de SOLO blancos respecto al instante de impresión.
- **Sonido OFF**: los 3 arrastres de la variante `g_unk_a9ce`=0 no se modelan
  (el port siempre suena) — 18 arrastres fijos.
- **Cadencia del guion**: frames×55 ms calibración compartida; el binario corre
  a ticks INT 1Ch reales — sin testigo propio de esta escena (oráculo del mini
  ROTO, medido por otro carril).

## 5. Verificación

- 17 asertos puros con esperados EN CRUDO (posiciones derivadas a mano del
  bytecode) + 7 de orquestación (`game/tests/blackthorn-scene.test.ts`);
  mutantes: guard-start (3 rojos), keywait retirado (1), tile de arena (2),
  dir N invertida (4) — todos muertos, suite verde tras restaurar.
- Careo mapa↔tablas en `shrine-scene.test.ts` (#324): grilletes 0x85 en los 6
  asientos, 0xB9 (5,4), 0x80 (5,7), 0xE8 (5,9), 0xBB (0,4)/(10,4), hueco sur.
- Visual MIRADO en las DOS pieles (venda, trono con party/guardias/Blackthorn,
  mesa con cuerpo + reloj lleno, péndulo con silla vacía y roster compactado,
  depósito) — capturas del 19-08, vite propio 5297, `?scenebeat`.
- e2e `blackthorn.spec.ts` 2/2 verde (drena síncrono bajo webdriver).

## 6. A3-HF8 (2026-09-28, H-186 / D-43) — la explosión del sacrificio, releída del binario

Leído para este lote con `re/tools/dis16.py` (BLCKTHRN near calls por base `0xA290`:
`0x9292 → 0x3522`, `0x7f02 → 0x2192`, `0x9856 → 0x3ae6`, `0x75c0 → 0x1850`).

- `0x0414`–`0x041e`: `push [0x5c64]` / `push [0x5c65]` → `call 0x3522` (`explosion_fx_at_cell`,
  llamada ÚNICA, 1 burst), tras la sirena (`0x03d0`–`0x0411`) y ANTES de `0x0421`
  (`[0x5c63]`/`[0x5c62]` = 0: el slot 1 se apaga) y `0x0429` (`[0xadf9]` = `0x80`, la mesa vacía).
- ULTIMA.EXE `0x3522`: `cmp [0x5893],0x80 / jae` — la conversión mundo→ventana (−party+5)
  sólo si la localización es < `0x80`. La captura pone `[0x5893] = 0xff` en `0x06fc`, así que
  la celda es la coordenada de escena tal cual: la mesa (5,7) tras el aviso, el asiento si no.
  Luego `0x354b` `blit_tile(x, y, 0)` (tile 0 = `Explosion`, opaco, el mismo `0x10e0` que el
  marcador de golpe de combate), `0x355a` `noise_burst(0x7d0, 0xbb8, 0xa)` (bloquea
  ceil(3000/10)·10·1,5 = 4.500 muestras = **174 ms**, idéntico al `0x35de` del golpe a un enemigo),
  `0x355d` `viewport_redraw` (`0x5910`, sin delay propio): el tile desaparece. Sin sacudida, sin XOR.
- Nadie lee tecla entre `0x03c9` y `0x04f6` (péndulo) / el retorno (traición).
- El ritual del shard llama a la MISMA rutina siete veces seguidas (CAST `0x16e1`–`0x16fa`,
  `call 0x75a2` por base `0xBF80`): siete ciclos de tile + 174 ms de ruido + redibujo. El puerto
  (`world_fx.h`) lo pinta como 60 ms encendido / 60 apagado y mudo — queda en cola aparte (H-209 / D-67),
  no se tocó en A3-HF8.
- El puerto TS (`blackthorn-capture.ts`) emite `cell-explosion` DESPUÉS del segmento del sacrificio
  (el apagado incluido); el binario la dispara antes. El nativo sigue al binario (beat del guion,
  `ALPHA3_AUDIO.md` §34); el TS no se cambió (ningún fixture fija esa capa).
