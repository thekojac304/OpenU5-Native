# Auditoría video-M: yell-palabra, foso, MUERTE + resurrección LB + Vertigo (Task #49)

Auditoría port-vs-original de las 5 secuencias del `video-M-dungeon-trampa-resurreccion.mov`
(frames 1fps en `original/av-referencia/video-M-frames/`). Fuentes: disasm
`re/disasm/{CMDS,BLCKTHRN,DUNGEON}.OVL.asm`, strings `original/u5/ultima5/{DATA.OVL,KARMA.DAT}`,
port `game/src/core/`. Formato de cita: `addr: instrucción → regla`; strings DATA.OVL
`fileoff = DS_off + 0x10`. Ver también `re/notes/blackthorn.md §4` (refuge), `re/notes/dungeon.md §5c`
(foso), `re/notes/cmds.md §13` + `re/notes/shadowlord-ritual.md` (yell).

---

## Secuencia 1 — Yell palabra de poder (f001-f010)

**Vídeo**: en la entrada de mazmorra, `Y` → prompt `Yell what?:` → texto libre. Palabra
inválida (`$`) → `No effect!`. `FALLAX` → `A word of power is uttered` y el sello de la
mazmorra desaparece (se abre).

### Original — CMDS.OVL overworld yell `0x12c8` (dispatch: `Y` → kernel 0x3464 "Yell " → CMDS 0x1418; `location 0` → 0x12c8)
- Prompt: kernel imprime `"Yell "`; el handler lee palabra (`0x7b9c`, máx 0xf). Strings del
  handler (DATA.OVL): `0x4529 "what?\n:"`, entrada vacía `0x4531 "Nothing\n"`.
- `12d7`: tabla de 8 punteros a palabras en **DS 0x4502** (file 0x4512). Orden CONFIRMADO
  byte-a-byte:

  | idx | palabra | DS ptr | mazmorra (loc) |
  |----|---------|--------|----------------|
  | 0 | FALLAX | 0x449d | 33 Deceit |
  | 1 | VILIS | 0x44a4 | 34 Despise |
  | 2 | INOPIA | 0x44aa | 35 Destard |
  | 3 | MALUM | 0x44b1 | 36 Wrong |
  | 4 | AVIDUS | 0x44b7 | 37 Covetous |
  | 5 | INFAMA | 0x44be | 38 Shame |
  | 6 | IGNAVUS | 0x44c5 | 39 Hythloth |
  | 7 | VERAMOCOR | 0x44cd | 40 Doom |

- `12e7`: match input↔palabra vía kernel `0xffffaf9e` (**substring-toupper**, mismo helper que
  el mantra de Blackthorn — NO igualdad exacta). Devuelve idx o 0xffff.
- `12ea: cmp ax,0xffff; je …` → si el input casa CUALQUIER palabra de la tabla:
  `12f2: print 0x44d7` = **`"\nA word of power is uttered\n"`** (¡siempre que sea palabra válida,
  ANTES del check posicional!).
- Bucle `si=0..7` (`12df`-`13fc`): compara la celda party+dir contra las tablas de coordenada de
  entrada de mazmorra `[si+0x1eaa]` (X) / `[si+0x1ed2]` (Y). Si el party está adyacente a la
  entrada de la mazmorra `si` en la dirección gritada Y la palabra casó ese idx:
  - `13bd: xor byte [si+0x58d0], 0x80` → **togglea el bit de sello** de esa mazmorra.
  - `13d5: 0x8482` (ptr al tile de mapa) + `13de: al=tabla[0x4512+si] xor 0xdf; xor [tile],al` →
    **transforma el tile de entrada** (sello ↔ abierto).
  - `13e2: g_unk_24e6 |= 2` (consume turno).
- Sin ningún match posicional (`bp-6==0`, `1402`): `1408: print 0x44f4` = **`"\nNo effect!\n"`**
  (también cae aquí si el input no era palabra de la tabla).

### Port — `game/src/core/quest/words.ts` + `game.ts::enterDungeon` (3989)
- Tabla `wordsOfPower` (data.json) en orden CORRECTO (FALLAX..VERAMOCOR ↔ loc 33-40).
- `tryEnterDungeon(state, loc, spokenWord)`: match por **igualdad `.toLowerCase()`** (no substring),
  persiste `questFlags["word-spoken:<loc>"]` para siempre.
- Se invoca en `enterDungeon(spokenWord)` — la palabra se pide **al entrar**, no como comando
  Yell direccional.

### Veredicto S1
- **Mecánica: EQUIVALENTE** (la palabra abre la mazmorra de forma permanente; el port persiste
  un flag, el original togglea bit 0x80 + tile). ✅
- **GAP-1a (strings fabricados)**: el port usa strings inventados, registrados como divergencia
  deliberada en `approved-strings.json`:
  - éxito `"The word of power resounds — the seal is broken!"` [C Clase-E] → REAL:
    `"\nA word of power is uttered\n"` (DS 0x44d7).
  - fallo `"The words fall to the ground, lifeless."` → REAL: `"\nNo effect!\n"` (DS 0x44f4, YA
    en el manifiesto como [C]).
  - `"The way lies open."` (fabricado, sin equivalente asm).
  - prompt real `"what?\n:"` / vacío `"Nothing\n"` no modelados.
  **FIX (Fase B)**: sustituir por byte-exactos. Bajo. Valor: la palabra que el usuario ve en el vídeo.
- **GAP-1b (menor, Clase C)**: match substring-toupper (original) vs exact-lowercase (port); y el
  original es DIRECCIONAL/POSICIONAL (adyacente a la entrada + dirección), el port pide la palabra
  al entrar. Divergencia de UX declarable; no cambia el resultado observable (mazmorra abierta).

---

## Secuencia 2 — Foso en mazmorra (f015-f025)

**Vídeo**: vista first-person L1 con foso en el suelo → caída (baja de nivel).

### Original — DUNGEON.OVL `0x0A4C` `dng_pit_fall` (ver `re/notes/dungeon.md §5c`)
```
si tile ∈ {0x61,0x69} y g_floor<8:
  "Pit Trap!" + "Falling..." ; and tile,0xF8 (limpia subtipo → 0x60)
  g_floor++ ; render ; "...splat!" + party_random_damage (rand(1,8) c/u, sólo VIVOS)
  si el nuevo tile es foso → ENCADENA
al parar: g_floor==8 → g_location=0 (Underworld) ; si sala → combate
```

### Port — `game/src/core/dungeon/dungeon.ts::pitFall` (451)
- `while curType==Trap && (curSub&7)==PitFall && f<N`: emite `"Pit Trap!"`, `"Falling..."`,
  limpia subtipo (`&0xF8`), `f++`, `"      ...splat!"`, daño rand(1,8) por miembro vivo, encadena.
- `A pit.` para 0x60 hoyo simple. Salida al Underworld cuando `f→8`.

### Veredicto S2
- **PORTADO Y FIEL, grado A** (strings byte-exactos, rand order, encadenado, salida a Underworld).
  Sin gaps de núcleo. ✅
- El **render** del foso como trapezoide oscuro en la vista first-person es asunto de la piel
  fiel (E1-S9/DNGLOOK), fuera de este scope de núcleo.

---

## Secuencias 3+4+5 — MUERTE + resurrección LB + Vertigo (f035-f078)

**Vídeo**: party a 0D → `An unending darkness engulfs thee...`, `Thou hast found refuge.`,
`No evil lives here, only peace and darkness.`, `But thy slumber is disturbed!` (pantalla negra,
Avatar solo). Aparición espectral (sprite azul + 2 figuras) + `Thou hast strayed far from the path
of the Avatar. Seek now to renew a life of Virtue, lest thy soul pass finally beyond my reach!` +
`Strange words are intoned.` → `Vertigo...` → despertar en el castillo de LB, día+1, party viva.

### Original — BLCKTHRN.OVL `0x0910` `party_refuge` (disparo: party entero muerto, kernel 0x39fc==−1)
Orden EXACTO de prints (`call 0x75c0` = kernel_print_ds; DATA.OVL fileoff=DS+0x10):

| # | addr | DS | string |
|---|------|-----|--------|
| 1 | 0x095f | 0x70e2 | `\nAn unending darkness engulfs thee...` |
| 2 | 0x09d2 | 0x7108 | `\n\nThou hast found refuge.` |
| 3 | 0x09e0 | 0x7122 | `\n\nNo evil lives here, only peace and darkness.` |
| 4 | 0x09ee | 0x7152 | `\n\nBut thy slumber is disturbed!` |
| 5 | 0x0a4b | 0x7172 | `\n\nSomeone shouts\n\n"FORTIS FORTUNA\nAVENTARI"` |
| 6 | 0x0ac5 | 0x719e | `\n\nThere is a peal of thunder!\n` |
| **7** | **0x0b03-0x0b3e** | **KARMA.DAT** | **discurso de LB indexado por karma/20 (ver abajo)** |
| 8 | 0x0b41 | 0x71cc | `\n\nStrange words are intoned.` |
|   | 0x0b54-0x0bb1 | — | revive por miembro: currentHp:=maxHp (0x0b98), status via 0xdc66(i,0xff) |
| 9 | 0x0bba | 0x71ea | `\n\nVertigo...\n` |

Entre #4 y #5 y tras #6 corren animaciones de aparición (blits 0x6dd8 de sprites 0x5e/0x5f/0x74,
`g_char_anim_states`) — L3/UI, no texto.

**#7 discurso de resurrección (KARMA.DAT)** — `0x0b03`:
```
0b03: al = g_karma ; 0b08: cl=0x14 ; 0b0a: div cl → index = g_karma / 20   (en [bp-2])
0b18: "KARMA.DAT" (DS 0x71c2) ; 0b1c: buffer DS 0xb21e ; 0b20: push 0x7d0 (max)
0b24: bx=index ; shl bx,1 ; push [bx+0x1a74]   → offset de byte del record en el fichero
0b2d: call 0x82de  → carga+imprime el record de KARMA.DAT
```
La tabla `DS 0x1a74` (file 0x1a84) = offsets de byte de cada record: `[0,132,269,410,546,0x8080]`
(el 6º es basura → record 5 inalcanzable; karma cap 99 ⇒ index 0..4). **El index se calcula con el
karma AL MORIR** (el restore a 75 es posterior, 0x0bfd). Records de KARMA.DAT (6, byte-exactos):

```
[0] "«Thou hast strayed far …» (131 B, sha1 5fdde8b6 — recortado; verifica contra tu copia)"
[1] "«Thy soul seeks direction …» (136 B, sha1 68c2d5cb — recortado; verifica contra tu copia)"
[2] "«It is within thee …» (140 B, sha1 7e052ff5 — recortado; verifica contra tu copia)"
[3] "«Thou showest well the …» (135 B, sha1 60c70cc0 — recortado; verifica contra tu copia)"
[4] "«Well armed art thou …» (124 B, sha1 223f979b — recortado; verifica contra tu copia)"
[5] "«Well armed art thou …» (89 B, sha1 6b56ba72 — recortado; verifica contra tu copia)"  (inalcanzable: karma cap 99)
```
(Nota: dobles espacios internos son del fichero, byte-exactos.) El vídeo muestra el record **[0]**
(muerte con karma bajo) → confirma `index = karma/20`.

**Estado final** (0x0bfd-0x0c4d, ya portado): karma piso 75, loc 0x11, floor 1, (10,10), a pie,
time_spell=0, reloj a las 6:00, light/torch 0, comida 63 si estaba a 0.

### Port — `game.ts::checkRefuge` (3051) + `REFUGE_NARRATION` (3019) + `world/blackthorn.ts::partyRefuge` (417)
- Disparo: `partyConsciousState==−1` en 3 sitios asm (overworld/pueblo/mazmorra) + compensación en
  `endCombat` (combate modal desacoplado). ✅
- `REFUGE_NARRATION`: las 9 líneas fijas #1-#6, #8, #9 — **byte-exactas y en orden**. ✅
- `partyRefuge`: revive (currentHp:=maxHp, status 'G'), karma≥75, loc/floor/pos/reloj/comida —
  **exacto**. ✅

### Veredicto S3/4/5
- **PORTADO Y FIEL en su mayor parte, grado A.** La mutación de estado y 8 de las 9 líneas están
  byte-exactas.
- **GAP-3 (el único real, ALTO valor)**: el **discurso de resurrección de KARMA.DAT (#7)** está
  OMITIDO. El comentario del port lo llama "cosmético... no se porta" — pero el vídeo lo muestra
  como la LÍNEA CENTRAL de la escena de resurrección (la voz espectral de Lord British). Precedente
  del proyecto: los strings de MISCMSG.DAT (interrogatorio) SÍ están hardcodeados byte-exactos en
  `game.ts` aunque el .DAT no se commitea → KARMA.DAT entra igual (6 records [D] byte-exactos).
  **FIX (Fase B)**: insertar el record `karma/20` (0..4) entre #6 `"There is a peal of thunder!"` y
  #8 `"Strange words are intoned."`. Requiere que `checkRefuge` calcule el index con el karma
  ANTES del restore (partyRefuge lo sube a 75) → leer `state.karma` antes de llamar a `partyRefuge`.

---

## Resumen de gaps (orden de valor)

| # | secuencia | gap | grado | fix |
|---|-----------|-----|-------|-----|
| 3 | resurrección | discurso KARMA.DAT (record karma/20) OMITIDO | ALTO | añadir 6 records [D] + index; insertar en refuge |
| 1a | yell | strings de éxito/fallo/prompt fabricados | BAJO | swap a byte-exactos DS 0x44d7/0x44f4/0x4529/0x4531 |
| 1b | yell | substring-vs-exact + direccional-vs-enter | Clase C | declarar; no bloquea |
| 2 | foso | — (portado grado A) | — | render first-person = piel fiel |
| 3b | refuge | byte de status del revive (~~kernel 0xdc66 [= CS 0x7ef6 → CAST2.OVL:0x05e0 resurrect_apply]~~) | ✅ **CERRADO SIN ORÁCULO** (barrido de citas 2026-07-25) | `0xdc66` no era una dirección de kernel sino el destino near-call CRUDO desde BLCKTHRN/SHOPPES (banda 2, base 0xa290): `(0xa290+0xdc66)&0xFFFF = 0x7EF6` → stub → **CAST2.OVL:0x05e0 `resurrect_apply`**. Cuerpo leído: rechaza índice <0; si el status +0x0b **no** es 'D' (0x44) y el 2º arg ≠0 imprime DS 0x953c «Not dead!» y devuelve 0; si es 'D' pone status **0x47 'G'** y **hp (+0x10) = 1** (NO maxHP), y luego reparte MP por clase (+0x0a 'A'/'B'/…). El `HP=maxHP` que cita shops.md §5 lo hace el **llamante**, no esta rutina: el healer lo escribe él mismo en SHOPPES 0x16ff (`[si+0x55ba] → [si+0x55b8]`) justo después de la llamada. |

---

## FASE B — IMPLEMENTADO (branch `fiel/death-resurrection`)

- **GAP-3 (discurso KARMA.DAT)**: portado. `game.ts::REFUGE_KARMA_MESSAGES` (5 records byte-exactos
  [D], rec0..rec4; el rec5 es inalcanzable con karma cap 99) + `refugeKarmaSpeech(deathKarma)`
  (`index = min(floor(karma/20), 4)`). `checkRefuge` lee `state.karma` ANTES de `partyRefuge`
  (que sube a 75) e inserta el discurso en `REFUGE_SPEECH_INSERT_AT=7` (entre "peal of thunder"
  y "Strange words are intoned"). Tests: `refuge-live.test.ts` (record 0 con karma 10; índice usa
  karma al morir con karma 90 → rec4).
- **GAP-1a (strings yell)**: portado. `quest/words.ts::tryEnterDungeon` → éxito
  `"\nA word of power is uttered\n"` (DS 0x44d7), fallo `"\nNo effect!\n"` (DS 0x44f4). Los
  fabricados ("The word of power resounds…", "The words fall to the ground, lifeless.") RETIRADOS
  del manifiesto (eran huérfanos tras el swap); "No effect!" recategorizado [C]→[D]. Rama defensiva
  re-entry/no-sello conserva "The way lies open." [C port-ism]. Test: `quest.test.ts`.
- **Nota de manifiesto**: los strings de la narración del refuge (incl. KARMA) NO se registran en
  `approved-strings.json` — el extractor forward no ve arrays estáticos consumidos vía `Game.`
  (mismo sink que `REFUGE_NARRATION`, ya sin registrar); añadirlos rompería la guarda de
  huérfanos (verificado). Provenance = citas `[D]` en el código + esta nota (cobertura inversa).
- Gate: tsc limpio · 1246 tests verdes (+2) · guarda anti-fabricación verde.
- **GAP-1b / 3b** siguen abiertos como divergencias declaradas (no bloquean): yell direccional vs
  enter-time (Clase C) y byte de status del revive (oráculo).

---

## TANDA 2 — GAP-1b RESUELTO: yell direccional real (CMDS 0x12c8)

El "pedir la palabra al entrar" era un INVENTO del port; se retira y se porta el flujo REAL.

- **`quest/words.ts::yellWordOfPower(wordsOfPower, spokenWord, adjacentDungeonLocations)`** (puro):
  substring-toupper contra la tabla de palabras (mismo helper que el mantra); palabra válida →
  `"\nA word of power is uttered\n"` (DS 0x44d7); si la mazmorra de la palabra está ADYACENTE →
  abre + turno; válida sin match adyacente → `uttered` + `"\nNo effect!\n"` (DS 0x44f4, 0x1408);
  inválida → sólo `No effect!`.
- **`game.ts::yellWord`**: en location 0 (overworld) reúne las mazmorras (33..40) en las 4 celdas
  adyacentes vía `locationAt` (tablas locationsX/Y = DS 0x1eaa/0x1ed2) y despacha a
  `yellWordOfPower`; al abrir persiste `questFlags["word-spoken:<loc>"]` (= bit 0x80 de [0x58d0+i])
  y cobra turno (`runContextTurn`, 0x13e2). location 1..0x20 sigue en la convocatoria de Shadowlord
  (0x1030). El sello persiste como estado de mundo en el save (questFlags).
- **`game.ts::enter`**: la entrada sellada se PRESENTA como tile derrumbe **0xDF** (impasable) en
  `activeMap.tileAt` (compose OUTSUBS 0x98/0x0); `enter` ya NO comprueba el sello ni imprime cadena
  (el string fabricado "A Word of Power seals this dungeon..." se RETIRA junto a su flujo). Llegar al
  branch de mazmorra ⇒ tile de cueva ⇒ sello abierto ⇒ `enterDungeon(id)`. Cerrado por el carril
  DOOM-SEAL-WIRE (2026-07-20).
- Tests: `quest.test.ts` (helper puro) + `yell-dungeon-seal.test.ts` (integración: yell abre → E
  entra; invariante "nunca inentrable"; + presentación 0xDF sellada/cueva abierta para Deceit y Doom).

**Mapeo de estado del sello** (respuesta a "¿dónde lo guarda el original?"): el original usa DOS
representaciones — (1) **bit 0x80 de [0x58d0+i]** (flag por-mazmorra, persistente = save) que togglea
`xor [0x58d0+i],0x80` (0x13bd); (2) **transformación del tile de entrada** en el mapa
(`xor [tile], al`, 0x13de). El port unifica el flag persistente en `questFlags["word-spoken:<loc>"]` y
DERIVA la transformación del tile: `activeMap.tileAt` repinta la cueva de entrada como derrumbe 0xDF
mientras el flag no esté puesto (compose OUTSUBS 0x98/0x0, `0149: mov byte [bx],0xdf`).

### Residuales de (c) — witness DOSBox (declarados, no bloquean)
- ~~**Tile-transform exacto** (sello→abierto)~~ ✅ **WIREADO** (carril DOOM-SEAL-WIRE, 2026-07-20): el
  original presenta el sello como tile derrumbe **0xDF** (byte 0x58d0[i]=0x00→0xDF; bit 0x80→cueva);
  el port lo repinta en `activeMap.tileAt`, floor-independiente. Ver deliberate-divergences §3.
- **Adyacente vs encima**: el port abre si la entrada está en una de las 4 celdas adyacentes
  (derivado de `party+dir`). Confirmar con witness que el original no acepta también "encima".
- **Doble impresión** "uttered"+"No effect" al gritar palabra válida lejos de su mazmorra: derivado
  del fall-through (12f2 imprime; 1408 imprime si bp-6==0). Confirmar orden/exactitud con witness.

---

## A3-HF9 (H-185 / D-42) — la CADENCIA del refuge y su GETKEY, leídas del binario

`party_refuge` (BLCKTHRN `0x0910`, near calls por la base `0xA290`; listado en
`native/core/batch51-original-scene-disasm.log`). Lo que el original BLOQUEA tras cada cosa
que muestra — el sustituto Clase C (70 ms/unidad + suelos 900/260 ms de `runRefugeScene`)
no tiene instrucción detrás:

| addr | muestra | bloquea después | device |
|---|---|---|---|
| `0x093f` / `0x0946` | redraw del visor (sólo loc < `0x21`) | `delay(10)` **ANTES** de la primera línea | 550 ms [A], mundo visible |
| `0x095f` / `0x0962`-`0x098c` | "An unending darkness…"; visor a negro | `RECT_DISSOLVE` (sin timer) | 55 ms [C→D] |
| `0x09d6` / `0x09dd` | "Thou hast found refuge." | `delay(14)` | 770 ms [A] |
| `0x09e4` / `0x09eb` | "No evil lives here…" | `delay(28)` | 1.540 ms [A] |
| `0x09f2` / `0x0a0d`-`0x0a49` | "But thy slumber is disturbed!" | 6 `tone_sweep`, a2 = DS `0x372c` = 260.000 muestras | 10.075 ms [B] |
| `0x0a4f` / `0x0a56` | "Someone shouts … AVENTARI" (UN print) | `delay(6)` | 330 ms [A] |
| `0x0a7c` / `0x0a90` | FIZZLE_IN `0x5e` (2,7) | fizzle (sin timer) + `delay(4)` | 55 + 220 ms |
| `0x0aae` / `0x0ac2` | FIZZLE_IN `0x5f` (8,7) | fizzle + `delay(4)` | 55 + 220 ms |
| `0x0ac9` / `0x0acc` / `0x0acf` | "There is a peal of thunder!" | `screen_shake_fx` (`0x3072`) ×2 | 2 × 936 ms [C] |
| `0x0af5` | FIZZLE_IN `0x174` (5,2) | fizzle | 55 ms |
| `0x0b03`-`0x0b3b` / **`0x0b3e`** | `"` + record KARMA.DAT `karma/20` + `"` | **GETKEY** `0x83dc` → kernel `0x266c`: sin timeout, cualquier tecla, descartada | espera de tecla |
| `0x0b45` / `0x0b4c` / `0x0b54`-`0x0bb1` | "Strange words are intoned." | `delay(4)` + un `tone_sweep` de `0x7530` por miembro (y su HP/roster) | 220 + N × 1.162 ms |
| `0x0bba` / `0x0bc1` | "Vertigo..." | `delay(4)` | 220 ms |
| `0x0bc4`-`0x0bfa` | visor a negro + `0x11c` en (5,5) | `RECT_DISSOLVE` | 55 ms |
| `0x0bfd`-… | — | suelo de karma 75, castillo, reloj… | `resolve_refuge` |

- **Orden de estado.** El índice del discurso se lee con el karma AL MORIR (`0x0b03`), ANTES del
  getkey; el revive por miembro va DESPUÉS (`0x0b54`); el suelo 75 al FINAL (`0x0bfd`). Nada muta
  antes del getkey.
- **Entrada.** Entre `0x0910` y `0x0b3e` nada lee el teclado ni lo vacía (`0x1b16`): en 1988 una
  tecla pulsada durante los bucles quedaría en el buffer de la BIOS y satisfaría el getkey al
  instante (typeahead). El port la TRAGA (divergencia declarada, como A3-HF6…HF8).
- **Referencia TS.** `buildRefugeScript` ponía el `delay(10)` DESPUÉS de la oscuridad y tres pausas
  sin instrucción (2 tras el 2º trueno, 2 tras la aparición, 8 tras el discurso) y ningún getkey.
  Corregido en A3-HF9 en la capa que fija `quest_parity` (`delayUnits` + `waitKey`); la piel web
  conserva su reloj Clase C, pero ahora espera la tecla del discurso.
- **Queda abierto (H-211 / D-68).** El contenido de la escena, no su cadencia: el Avatar sólo se
  coloca en `0x09f5`-`0x0a07` (tras "But thy slumber"), y el negro final `0x0bc4` deja al Avatar
  SOLO (el port mantiene las cuatro figuras en `vertigo`). Ver `ALPHA3_AUDIO.md` §35.


> **A4-PARITY2 D-83 / D-84 (2026-10-03) — superseding note.** The shared resurrection routine is CAST2.OVL `0x05e0` (`resurrect_apply`, kernel stub `0x7ef6`; the "kernel `0xdc66`" of older notes is the raw near-call operand seen from the `0xa290` band). It has exactly four callers: In Mani Corp spell (CAST `0x10f3`, mode 0), the scroll (CAST `0x12ee`, mode 1), the healer (SHOPPES `0x16f5`, mode `0xff`) and the Refuge (BLCKTHRN `0x0b95`, mode `0xff`). For a member whose status byte is `'D'` it sets status `'G'` and HP 1, MP by class (A/M = INT, B = INT>>1, others untouched), cuts experience to `exp*karma/100` (truncated) when karma < 98, and for EVERY karma recomputes level (`1 + bitlength(exp/100)`) and max HP (`30*level`); the healer (`0x16f8-0x1703`) and the Refuge (`0x0b98-0x0b9d`) then copy the NEW max HP over HP. The Refuge runs it with the karma the party DIED with (the floor of 75 comes after the loop, `0x0bfd`). Earlier statements here that the Refuge's/healer's revive byte is "open", "canonical" or that the healer "coincides with the clone" were wrong about the clone: both ports skipped the routine (fixed in A4-PARITY2: reference `shops.ts` / `blackthorn.ts`, native `healer_heal` / `resolve_refuge` over one shared `resurrect_apply`). `native/core/a4-parity2-findings/D83D84-FINAL.md`.
