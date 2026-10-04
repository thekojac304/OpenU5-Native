# BLCKTHRN.OVL — Guardias, cárcel y Blackthorn (Task 3.10)

Derivación EXACTA (asm-only, READ-ONLY) del subsistema de captura/interrogatorio de
Blackthorn, wipe de party ("refuge"), peaje/password de guardias y merma de la
Falsedad. Fuentes: `re/disasm/BLCKTHRN.OVL.asm`, `ULTIMA.EXE.asm`,
`TOWN/MAINOUT/DUNGEON/CMDS/TALK.OVL.asm`, `original/u5/ultima5/DATA.OVL`. Formato de
cita: `addr: instrucción → regla`. Portado en `game/src/core/world/blackthorn.ts`;
paridad en `re/tools/test_blackthorn_parity.py`; convención de fidelidad en
`re/verified/blackthorn.md`.

## 0. Rebase y anclas

- **BLCKTHRN.OVL load_seg = 0x0A29** (= COMBAT/NPC). Regla de rebase de call
  externo: `kernel_off = (NNNN + 0xA290) & 0xFFFF`. Calls locales aparecen con
  offset pequeño (`call 0xbe`, `call 0x2e`, `call 0`).
- **Tamaño 3184 B (0xC70)**. Prólogos `55 8bec` verificados en 9 offsets
  (0x0000/0x002e/0x00be/0x0278/0x02ea/0x03ae/0x054a/0x060e/0x0910) + padding de
  ceros 0x0c66..0x0c6f. Partición completa en `re/tools/blackthorn_catalog.py`
  (ledger: 100% cubierto, 0 gaps).
- **Strings** DS: `kernel_print_ds` (0x1850) con `mov ax,<DS_off>`; resuelven en
  DATA.OVL BASE con `fileoff = DS_off + 0x10`. Los de interrogatorio DS 0xB21E.. se
  cargan en runtime desde **MISCMSG.DAT** (`kernel 0x256E`); en DATA.OVL están a 0.

## 1. Mapa de funciones (9 funciones)

| off | firma | qué hace |
|-----|-------|----------|
| 0x0000 | `beep_delay(n)` | n× {frame/paint ; flash(2)} — pausa+beep cosmética |
| 0x002e | `dir_to_delta(code)` | code&0xFC → dirección; code&3 → dx/dy en g_cmb_scratch |
| 0x00be | `anim_vm(ptr)` | intérprete de bytecode de cutscene (jump-table cs:[bx−0x5bfa]) |
| 0x0278 | `print_question(subj,var)` | 1 de 4 variantes del interrogatorio + virtud [0x1F4E] |
| 0x02ea | `check_mantra(subj)` | lee 14 chars → substring-match toupper vs mantra [0x1F5E] |
| 0x03ae | `sacrifice_member(mode)` | alarma + saca el 2º miembro vivo; party_size-- |
| 0x054a | `interrogate(numLiving,subj)` | bucle de 4 preguntas escalando |
| 0x060e | `blackthorn_capture()` | **ENTRY (stub 0x7abe)**: escena completa de captura |
| 0x0910 | `party_refuge()` | **ENTRY (stub 0x7a5e)**: wipe de party → despertar en LB |

## 2. Disparo (dispatch overlay #8)

`stub 0x7abe → 0x060e` (capture), `stub 0x7a5e → 0x0910` (refuge).

### 2.1 Captura — TOWN.OVL:0x12ae
```
12b9: cmp byte [g_location], 0x12 ; jne …    → sólo en Palacio de Blackthorn (loc 18)
12c0: call K 0x39fc (party_conscious_state)
12c5: jge 0x12ca (ax>=0) else jmp 0x134b     → si NO todos muertos
12ca: call stub 0x7abe = blackthorn_capture()
```
**Regla del gate interno de 0x12ae**: loc **0x12** + party no completamente muerto
(party_conscious_state>=0) → captura DETERMINISTA (sin RNG). El clon:
`blackthornCaptureTriggers(state)`.

**PERO 0x12ae NO se llama por turno**: su ÚNICO caller es `npc_engine` (§2.1a). El
gate de arriba es la 2ª mitad; la 1ª (la PRECONDICIÓN real) es el ATAQUE de un
guardia — hay que estar ADYACENTE a un guardia del Palacio. F1.7-T2 aproximó el
trigger como "cada turno en loc 0x12"; F1.7-T5 lo sustituyó por el ataque real.

### 2.1a Trigger real: ataque de guardia — TOWN.OVL:0x1352 `npc_engine`
`npc_engine` (2º world-turn del bucle de pueblo, 0x1683 `call 0x1352`, arg
`[bp+4] = result−1`, gated por `[0x65bf]≠0 ∨ result==2`, 0x1671-0x167c) procesa el
NPC activo `[0x65bf]`:
```
1368: ax = g_npc_rt[idx].objIdx (rt+0xC) ; shl 3 ; add 0x5c5a → &g_world_objects[objIdx]  ([bp-4])
1376: call npc_target_for_attack (thunk 0xd740 → NPC.OVL 0x06E4)   ; (re)fija g_npc_attack_tile
1379: cmp byte [g_npc_attack_tile 0x65be], 0x61 ; jne 0x13b4       → ¿marcador 0x61?
  ── RAMA 0x61 (aiType 6/7 hostil, §2.1b): ──
138a: cmp word [g_npc_rt[idx].dialogNum 0x5f68], 0xfe ; jne 0x13a4
13a4: bx = [bp-4] ; cmp byte [bx], 0x70 ; je 0x13d6                → ¿objeto guardia? → captura
  ── RAMA !=0x61 (aiType 4/5 = LOS PALACE GUARDS, marcador 0x74): ──
13b4: cmp word [bp+4], 0 ; jne 0x13d6      → si result−1 ≠ 0 (result==2) → captura DIRECTA
13ba: (result==1) idx=[0x65bf] ; cmp [g_npc_rt[idx].dialogNum 0x5f68],0 ; je 0x13dc  (dialog 0 → nada)
13ce: call 0xfffff912 [⚠ 30-07: RESUELTO — NO es «0x1912 opaco»: near-call a kernel 0x7AE2 (PLINK) → TALK 0x031E talk_converse_dispatch, leído ENTERO en re/notes/talk-031e-resolucion.md] ; or ax,ax ; jne 0x13d6, → captura
13d6: call 0x12ae = captura                                        (re-gatea loc 0x12 + conscious)
```
**HALLAZGO CRÍTICO (fix review T5)**: los palace guards son **aiType 0/4** (npcs.json
loc 18 slots 8-15, dialog 255) → su fast-path fija `g_npc_attack_tile=0x74`, **NO 0x61**
→ toman la **rama 0x13b4**, no la 0x13a4 que cité de más en el 1er pase. La captura de
un guardia merchant depende ahí de `result==2` (0x13b4) **o** del handler ~~opaco
0x1912 (fuera de TOWN.OVL, no desensamblado)~~ [⚠ 30-07: resuelto = TALK 0x031E,
ver talk-031e-resolucion.md]. ~~La rama 0x61 (0x13a4, tile 0x70) sólo
aplica a guardias ya HOSTILES (aiType 6/7).~~ Ver §2.1c para la re-etiqueta.

> 🔴 **TACHADO-DOCUMENTADO (23-08, carril gargolas-hostiles — el softlock de las
> gárgolas del Palacio).** El listado de «RAMA 0x61» de arriba está **TRUNCADO**: se
> corta en `13a4 … je 0x13d6 → captura` y presenta ese `je` como el final de la rama.
> El binario sigue: con tile != 0x70, `13ac: mov [bp-2],1` y el **tail 0x13dc-0x1414**
> — actor >= 0x40 (0x13f4) → **print DS 0x2881 `"\nAttacked!\n"` (0x13fb) +
> `call 0x9bc town_attack_engine_commit`** = dead-bit + COMBATE (`enter_combat_vs_actor`
> ULTIMA.EXE 0x6150) + ranura FUERA; actor < 0x40 → `call 0xb0` a secas (0x140e). Y el
> retorno de `0x12ae` cae en el MISMO tail: rehusar el arresto ('N', 0x1346 ret 1) →
> combate contra ESE guardia. La frase tachada («sólo aplica a guardias») es la que el
> port copió como `return null` — el softlock. Derivación completa y careo:
> `gargolas-hostiles-palacio.md`.

### 2.1b Targeting — NPC.OVL:0x06E4 `npc_target_for_attack` [asm]
`manhattan = dist_manhattan(party, npc)` (0x071d `call 0x6a0`; §3.1). DOS caminos:
```
0723: cmp ax,1 ; jne 0x75a        → si NO adyacente (manhattan!=1) → LOOP (b)
                                     FAST-PATH (a), manhattan==1, SIN RNG:
0728:   cmp aiType,3 ; jle 0x75a   → aiType 0..3 → loop
072e/34: aiType 4/5 → 0x73d        → g_npc_attack_tile = 0x74 (si dialogNum!=0); 074e [0x65bf]=idx
073a:   aiType 6/7 → 0x7be         → g_npc_attack_tile = 0x61; 074b→074e [0x65bf]=idx
075a: LOOP (b): construye ≤8 candidatos, para aiType 5/7 tira rand(0,0x3f) umbral
      0x10 (0x08a5/0x08d4) para decidir PERSECUCIÓN; MUEVE el NPC (0x0917-0x092a
      escribe npc.x/y + g_world_objects), NO fija g_npc_attack_tile ni [0x65bf].
```
**`[0x65bf]` (idx del NPC activo) tiene EXACTAMENTE 2 escrituras en NPC.OVL** (censo por
BYTES con control positivo, de FORMA y negativo — `tc-result-producer.md` §4, que confirma
esta cifra): `074e` (=idx, en el tail del fast-path) y `0dc6` (=0, reset del bucle
npc_tick_all). ⚠ El reset **NO es del bucle: está en el PRÓLOGO** de la rutina (0x0db4,
justo tras `push di/si`), junto al de `[0x65be]` en 0x0dc1 — por eso ambas globales tienen
vida de UN turno y no hay limitador de tasa posible. Y hay un consumidor de `[0x65be]`
FUERA de TOWN que esta acta no censaba: **CMDS.OVL 0x05c2** (`cmp byte [0x65be],0x61`),
dentro del acampar (stub 0x802e, DS 0xa170 `'Hole up- '`): un NPC HOSTIL adyacente
INTERRUMPE el descanso.
Cero escrituras en TOWN.OVL. ⇒ **un NPC sólo se vuelve "activo" (y habilita el gate
`[0x65bf]≠0` de npc_engine) por el fast-path de adyacencia manhattan==1.** El loop (b)
no lo toca. ADYACENCIA = precondición NECESARIA de la captura, asm-probada.

### 2.1c Decisión de fidelidad (fix review T5): adyacencia NECESARIA, suficiencia CLASE C
- **Fast-path vs loop (a/b)**: el loop (b) NO fija ni `g_npc_attack_tile` (0x65be, **TRES**
  escrituras — 0x0746, 0x07be y el reset **0x0dc1**; ⚠ 30-07: esta acta declaraba «2», y el
  censo POR BYTES de `tc-result-producer.md` §4 encontró la tercera, que es justamente la
  que prueba la NO-persistencia; las dos que ARMAN siguen siendo post-adyacencia) ni
  `[0x65bf]` (2 escrituras, §2.1b)
  → **no dispara la captura**; su `rand(0,0x3f)` es del stream de MOVIMIENTO del NPC,
  no del trigger → **Clase C de persecución** (`manager.ts:fleeStep`, ⚠️→formulado).
  **(b) NO es inevitable.** ✅ derivado.
- **ADYACENCIA NECESARIA** ✅ derivada: toda ruta a 0x12ae exige que el guardia sea el
  NPC activo `[0x65bf]`, que sólo se fija por adyacencia (§2.1b). Sin guardia pegado no
  hay captura.
- ~~**SUFICIENCIA = APROXIMACIÓN CLASE C de conducta observable**~~ **⚠ CERRADA en dos
  tiempos**: B-RUNTIME por el testigo DOSBox 2026-07-14 (oracle-blackthorn.md), y
  **DERIVADA 30-07** (talk-031e-resolucion.md): el ex-«0x1912 opaco» es TALK 0x031E
  `talk_converse_dispatch` (leído ENTERO); para el guardia (dlgNum 0xFF) corre el
  guard_demand, TALK `0x1e2`, y es LA ÚNICA RAMA que propaga ret≠0 al retorno del dispatch, `0x13d2`, → 0x12ae.
  Sin insignia: ret 1 SILENCIOSO (gate 0x2a4 antes del print) → captura ⇒ adyacencia
  BASTA en el caso sin-insignia, que es el del clon. Con insignia: reto de password EN
  LA INTERCEPCIÓN (tarjeta T-A). `result` sale del dispatcher de pueblo (0x159a); qué
  produce `result==2` (captura DIRECTA sin TALK) sigue sin aislar — tarjeta T-C.
  ⚠ Matiz que sobrevive: la sub-rama `result==2` de 0x13b4 salta a captura DIRECTA sin
  leer `[0x65bf]` — vía que no depende del NPC-activo.
- ~~**BP DOSBox para cerrar** (F.2)~~ [⚠ 30-07: el objetivo del BP («retorno de
  0x1912» / gate interno) se DISOLVIÓ con la resolución estática. Si se persigue la
  rareza de la demanda de tributo, el BP útil es el productor de `result` (0x159a) y
  el ciclo de `[0x65bf]` — ver talk-031e-resolucion.md §4.]

### 2.2 kernel 0x39fc `party_conscious_state()`
```
3a26: cl=status[i] ; cmp 'G' je RET0 ; cmp 'P' → ax=0 (consciente)
3a44: cmp 'S' ; inc dx (dormidos)
3a5e: cmp dx,0 ; ax=1 (sólo dormidos) / ax=0xffff (todos muertos)
```
Devuelve **0** (≥1 consciente 'G'/'P'), **1** (sólo 'S'), **−1** (todos muertos). Gate
de la captura (>=0) y del refuge (==−1). Clon: `partyConsciousState`.

### 2.3 Refuge — TOWN:0x1436 / MAINOUT:0x0ac2 / DUNGEON:0x1014
```
TOWN 1456: cmp [bp-8],-1 ; 145c: call stub 0x7a5e = party_refuge()
```
`party_refuge()` se dispara con party entero muerto (0x39fc==−1) en pueblo/overworld/
mazmorra. **No es game-over** — despiertas (§4).

## 3. Captura + interrogatorio (loc 0x12)

### 3.1 `blackthorn_capture()` — 0x060e
```
0616: cuenta vivos → numLiving (loop 'D')
064a: g_transport_tile = 0x1c (a pie)
0652: print 0x6fbc "Thou art subdued and blindfolded!"  ; ANTES del bucle → también en depósito
0659: si=0..7: si g_shrine_destroyed[si]==0 → subj=si, break  (0x0665 si>=8 → jmp 0x8e7, sin trono)
06b0: print 0x6fe0 "Strong guards drag thee away!"
0718: carga MISCMSG.DAT → DS:0xb21e ; escena del trono (MISCMAPS/sprites)
07dc: print 0x7024 "Thou hast been chained and manacled!"
07ea: print 0x704c "Footsteps!"
0883: print 0x705a "Blackthorn says:\n\n\"Ah, " ; 088a: print nombre PJ0 (DS 0x55a8) ; 0891: print 0x7074 "!...honour to meet thee at last! "
089b: print 0x70a4 "GUARD! Release this good" ; género (08b1:0x70c0 " lady " si +9==0xc, 08b5:0x70c8 "man " si ==0xb) ; 08bc: print 0x70ce "at once!\""
08ca: print MISCMSG rec11 (DS 0xb54a) "\n\n\"Wait!\"...answering a question.\" "
08d0: call interrogate(numLiving, subj)      ; 0x54a
   (prints DATA.OVL: fileoff = DS_off + 0x10; sprites 0xbe/pausas 0x83dc = L3/UI)
08e7: g_floor=0xff (= PLANTA −1, el sótano — ver regla del depósito) ; redraw ; fade
08f6: g_party_x=10 ; g_party_y=7 ; g_keys=0 ; g_location=0x12
```
**Reglas** (verificadas en el disasm 0x08f6-0x0905):
- Interroga por el **PRIMER santuario con byte `g_shrine_destroyed[i]==0`** (todavía
  en pie y no cedido). Si los 8 cayeron → sin interrogatorio, sólo deposita.
- Depósito final: **(x=10, y=7, planta 0xff = SÓTANO) en loc 0x12, g_keys=0, a pie
  (0x1c)** — la celda del calabozo del Palacio.
  ~~0x08e7 escribe `g_floor=0xff` (centinela de redibujado antes del fade) y el
  depósito (0x08f6-0x0905) **NO re-fija g_floor** → queda 0xff. El clon **no toca
  floor** aquí; ⚠️ el cableado de 3.13 debe decidir la planta de aterrizaje si la
  captura se dispara desde planta alta (no hay evidencia asm de normalización —
  riesgo declarado, no una tesis derivada).~~ **Clase C CERRADA (24-08,
  carril blackthorn-deposito)**: la lectura «centinela» era errada — 0xff ES la
  planta de aterrizaje. Evidencia: (a) **0xff es el valor ordinario del sótano**:
  KLIMB (TOWN.OVL 0x052e) hace `0548: inc byte [g_floor]` / `0566: dec byte
  [g_floor]` sobre la MISMA variable DS 0x5895 — bajar la escalera desde la planta
  0 deja exactamente 0xff, y así se visita cualquier sótano (el save persiste el
  byte tal cual; los .NPC usan z=0xFF para el sótano, cf. npc-object-actors.md);
  (b) **el depósito gemelo elige planta explícita**: el arresto de pueblo escribe
  `1332: sub al,al ; 1337: mov [g_floor],al` = 0 para la celda de Yew — cada
  depósito escribe su destino completo, y el de la captura escribe 0xff;
  (c) **control por tiles** (asset smallmaps.json, loc 18, plantas [-1,0,1,2,3]):
  (10,7) SÓLO es pisable en z=−1 (tile 68, interior de la celda con su
  puerta-rastrillo 187 en (10,9)); en z=0/1/2 es muro 79 y en z=3 es 81 —
  depositar en la planta 0 encastra al party dentro del muro sur del almacén de
  barriles (rastrillo 184 en (11,7)), que es EXACTAMENTE lo que grabó el usuario
  en móvil el 24-08 («¡Bloqueado!» en las cuatro direcciones). Nada entre 0x08e7
  y el `ret` 0x090f re-toca g_floor ⇒ el aterrizaje es SIEMPRE el sótano, se
  dispare la captura desde la planta que sea. Port: `blackthornCaptureDeposit`
  (blackthorn.ts) fija `floor = −1`; testigos en blackthorn.test.ts («deposita en
  (10,7) planta -1») y capture-live.test.ts (mapa real + planta alta).
- El saludo usa el género del PJ0 (`g_party_records+9`: 0x0c="lady", 0x0b="man") —
  cosmético, no portado como estado.

### 3.2 `interrogate(numLiving, subj)` — 0x054a
```
0552: warned=0 ; si=0..3
0563: print_question(subj, si)
0568: match = check_mantra(subj)
056c: SI match:
  0570: g_shrine_destroyed[subj]=0xff ; 0575: karma_sub5() (K 0x3F36 &g_karma,5)
  0580: si numLiving>1: sacrifice_member(0)  else: print "rewarded with thy life"
  0595: return
059e: SI NO match:
  si numLiving<2: print "To the dungeon!" ; return
  si warned!=0: advance_clock(2) ; switch si {0→tile 0xEA,1→0xEB,2→0xE8, 3→sacrifice_member(1)}
  else: warned=1 (1er fallo, sólo amenaza)
05fc: inc si ; si<4 loop
```
**Reglas**:
- Acertar el mantra en CUALQUIER ronda = **traición**: `g_shrine_destroyed[subj]=0xff`,
  **karma −5 con suelo 0** (kernel 0x3F36: `if karma<=5:0 else karma-5`) y, si
  numLiving>1, **un compañero ejecutado** ("merciful death"); si sólo el Avatar,
  Blackthorn lo perdona ("rewarded with thy life"), sin sacrificio.
- Avatar solo fallando → "To the dungeon with thee!" y termina, **sin sacrificio**.
- party>1 fallando: 1ª ronda = aviso; siguientes escalan (advance_clock(2)) y en la
  **4ª ronda (si=3) el péndulo ejecuta un compañero** ("treachery"). Negarse NO toca
  karma. Los tiles 0xEA/0xEB/0xE8 (@0xAE39) son cosméticos de la escena.
- El bucle CONTINÚA tras un fallo: se puede fallar y luego acertar en una ronda
  posterior → traición.

### 3.3 `sacrifice_member(mode)` — 0x03ae
```
03bc: print (mode? "pendulum blade falls!" : "merciful death!")
03d0: sirena rising/falling (K 0x2192) — cosmético
0438: selecciona el 2º miembro VIVO (loop status!='D', cx==2 → slot)
046c: copia su record (32 B) a DS:0x5788 (= g_party_records slot 15) ; compacta el
      roster ; 0x04c2-0x04d4: byte final del slot 15 = 0x7f ; dec g_party_size
```
Quita el **primer compañero (2º miembro vivo)**; el Avatar (slot 0) nunca. Sin RNG.
Clon: `sacrificeFirstCompanion`. ⚠️ **Layout de save**: 0x5788 = slot 15 de
`g_party_records` NO es un scratch efímero — el ejecutado PERSISTE ahí con el byte final
0x7f en la ventana del save (el nombre se imprime desde ese slot). ~~El clon lo elimina
del array; la paridad byte-a-byte del roster (slot 15 parqueado) es de Task F.~~
**CERRADO (24-08, carril save-residuos)**: el clon ya compacta los 16 records (splice)
Y aparca el ejecutado en `characters[15]` con `partyStatus=0x7f`; `exportNativeSave` lo
serializa. Testigos: `save-native-post-sacrificio.test.ts` · `blackthorn.test.ts` ·
`capture-live.test.ts`.

### 3.4 `check_mantra(subj)` — 0x02ea
Lee 14 chars (K 0x3B1C) y hace **substring-match case-insensitive** (K 0x216C strlen,
K 0x2032 toupper) contra el mantra `[0x1F5E+subj*2]`. "AHM", "ahm ", "the ahm" cuentan
como correcto para "Ahm". Clon: `mantraMatches`.

### 3.5 Datos: virtudes y mantras (DATA.OVL)
`0x1F4E` subj-ptrs, `0x1F5E` mantra-ptrs (8 words). Mantras (DATA.OVL 0x0BE0, leídos
por `blackthorn_parity.load_mantras`): `Ahm, Mu, Ra, Beh, Cah, Summ, Om, Lum`
(virtudes 0..7: Honesty, Compassion, Valour, Justice, Sacrifice, Honor, Spirituality,
Humility).

## 4. Refuge / party-wipe — 0x0910

Secuencia: carga BRIT.DAT (mapa refugio), `g_transport_tile=0x1e` (transitorio),
narra "unending darkness…"/"Thou hast found refuge."/"But thy slumber is disturbed!",
limpia sprites, corre scripts de despertar. **Estado final EXACTO** (0x0bfd-0x0c4d,
leído del disasm):
```
0bfd: si g_karma < 0x4b(75): g_karma = 0x4b        → karma restaurado a SUELO 75
0c09: g_location = 0x11 (Lord British's Castle)
0c0e: g_floor = 1
0c13: g_transport_tile = 0x1c (a pie)
0c18: g_party_x = g_party_y = 0x0a (10,10)
0c20: g_time_spell_turns = 0 ; g_time_spell = 0
0c2a: bucle advance_clock(9) hasta g_hour==6        → reloj a las 6:00
0c38: g_light_spell_mins = 0 ; g_torch_mins = 0
0c40: si g_food==0: g_food = 0x3f (63)
0c4d: si 0 < loc_previa < 0x21: advance_clock/effect final
```
**Reglas** (portadas en `partyRefuge`):
- **NO es game-over**: despiertas en el **castillo de Lord British (loc 0x11), planta
  1, en (10,10)**, a pie, con el party REVIVIDO.
- **karma restaurado a un suelo de 75** (hallazgo estático; el clon usa
  `max(karma,75)`).
- time_spell limpio; light/torch a 0; comida a 63 si estaba a 0.
- **Reloj**: el bucle `advance_clock(9)` hasta `g_hour==6` (0x0c2a-0x0c36) deja la hora
  en 6 pero el **minuto es ≠0 dependiente de la hora de entrada** (y puede avanzar
  días, con housekeeping por cada tick). El clon fija (hora=6, minuto=0) como observable
  canónico — ⚠️ el minuto/día exactos requieren simular advance_clock (Task 3.13).
- **Revive por miembro** (0x0b54-0x0bb1): **HP a full es asm-directo** — 0x0b98:
  `ax=word[rec+0x12]` (maxHP) → 0x0b9d `word[rec+0x10]=ax` (currHP), **incondicional
  para TODOS los miembros** (el __ldiv de 0x0b6f es `36400/(7+i)` = frecuencia del
  sonido, NO HP). El byte de STATUS lo fija `kernel 0xdc66 [= CS 0x7ef6 → CAST2.OVL:0x05e0 resurrect_apply](i, 0xff)` (0x0b90-0x0b95) —
  su valor exacto queda ⚠️; el clon pone `status='G'`, `currentHp=maxHp`.
- **Saludo de LB** (0x0b03-0x0b2d): `g_karma/20` indexa la tabla de saludos 0x1a74 (LB
  te recibe según tu karma) — cosmético, no portado como estado.

## 5. Peaje / tributo / password de guardias — TALK.OVL:0x01e2 (dialogNum 0xFF)

Gated por `g_location`. Compartido con Task 3.5 (converse 0xFF). **Semántica del
prompt yes/no** (helper TALK 0x00ac, verificada): imprime DS:0x9052 "Dost thou pay?",
lee tecla toupper; **'Y' (0x59) → `sub ax,ax` = ret 0**; 'N' (0x4e) → ret 1. Es decir:
**se PAGA al aceptar ('Y')**. El valor de retorno del handler (0x318):
**0 = guardia satisfecho** (pagaste / password correcto), **1 = ruta de ESCALADA**
(rehúsas / no puedes pagar / gate falla / password erróneo) que el caller de 3.13 usa
para reaccionar (hostilidad/guardias).
- **loc 0x12 (Palacio)** (0x02a4): gate `cmp g_time_spell,0x1d ; je … else return 1`;
  luego `strcmp(input, DS:0x4A9A)` (`DS:0x4A9A = "IMPE\0"`, call 0 en 0x02e0). Match
  (ax!=0) → "Pass, friend!" (0x913a) ret 0; no → ret 1. (El gate `g_time_spell==0x1d`
  y el requisito del Black Badge quedan como ⚠️; el clon modela la comparación.)
- **loc 5 (Minoc)** (0x01f3-0x022d): tras "half thy gold to charity!" el prompt "Dost
  thou pay?"; **'Y' → `g_gold /= 2`** (idiv 2 signed, 0x021f-0x0225) ret 0; 'N' → ret 1
  con el oro **intacto**.
- **otras loc** (0x0230): tributo `Σ 0xa por miembro con status!='D'` (10 gp/miembro
  vivo, 0x025a `cmp [si],'D'`); **'Y' y `tributo<=oro` → `g_gold -= tributo`** ret 0; 'N'
  (0x028d) → ret 1 sin cobro; no puede pagar (0x0292 `cmp tribute,gold ; ja`) → ret 1.
Clon: `guardDemand(state, response, agree)` con `ret` (0/1). **CORRECCIÓN de review**:
el port inicial invertía el yes/no (cobraba al rehusar) — arreglado.

## 6. Merma de la Falsedad — SHOPPES.OVL:0x019a (port de 3.6 pendiente en 3.10)
```
019a: cmp byte [g_shadowlord_here_idx(0x5958)], 0 ; jne ret
      call K 0x3F54 sub_word_floored(&g_gold, rand(1,64))
```
`g_shadowlord_here_idx` lo fija TOWN 0x02b6-0x0306: recorre `g_shadowlord_locs[0..2]`
(0x58C8) comparando con `g_location`; primer match → índice, 0xFF=ninguno. **Con la
Falsedad (Shadowlord 0) presente en la ciudad, cada compra sisa `gold -= rand(1,64)`
con suelo 0** — consume 1 rand del stream de mundo por compra. Clon:
`shadowlordPresentIndex` + `postPurchaseGoldDrain(state, roll)` (el `roll` lo provee el
caller de tienda; el cableado al bucle de tienda es de 3.6/3.13).

## 7. Propagación de ALERTA de cañón — CMDS.OVL:0x0d31 (helpers 0x7B06/0x7B12/0x7B1E)
```
0d5a: si g_karma>5: karma-=5 ; else karma=0      → −5 karma por matar con cañón
0d70: owner = K 0x7B1E([obj])                     → dueño del objeto (−1=ninguno)
0d79: si owner!=-1: K 0x7B06(owner) ; K 0x7B12(owner)  → marca hostil + llama guardias
```
`0x7B1E/0x7B06/0x7B12` son **thunks PLINK** (`lcall 0x72e,0x2ec` + `ljmp 0:target` →
0x82EE/0x8222/0x8280) que cargan y saltan a código overlay-resident (fuera del listado
kernel de 64K); sus cuerpos exactos requieren el oráculo o desensamblar el overlay
destino. **NO se añaden al ledger de ULTIMA.EXE** para no romper el invariante 202800
con segmentos especulativos (los cuerpos reales viven en overlays). La cadena
observable (karma −5 + hostilidad del dueño) queda documentada; su cableado es de 3.9.

## 8. Divergencias con el clon y estado de fidelidad

1. **Captura/interrogatorio: NUEVO** (el clon no lo tenía). Reglas exactas (karma −5,
   marca de santuario, sacrificio, depósito (10,7)/keys=0) — ✅ asm-directo.
2. **Refuge: NUEVO**. Wake (0x11,1,10,10), karma≥75, comida 63, **HP→maxHP** — ✅
   asm-directo. Sólo el **byte de status** del revive (kernel 0xdc66 [= CS 0x7ef6 → CAST2.OVL:0x05e0 resurrect_apply]) y el **minuto/día
   exactos** del reloj quedan ⚠️.
3. **Guardias TALK 0xFF: NUEVO**. Password/half-gold/tributo — ✅ asm-directo; el gate
   `g_time_spell==0x1d` y el requisito del Badge — ⚠️ (oráculo).
4. **Merma de la Falsedad** — ✅ asm-directo (función pura; cableado a tienda pendiente
   de 3.6/3.13).
5. **Trigger de captura por ataque de guardia (F1.7-T5): NUEVO** (§2.1a/b/c). El clon
   sustituyó el gate per-turno de T2 por la adyacencia manhattan==1 a un guardia type
   0x70 (`blackthornGuardCaptureTriggers`). Grado MIXTO: **adyacencia NECESARIA ✅
   derivada** (idx `[0x65bf]` sólo se fija por el fast-path 0x06E4, 2 escrituras); la
   ~~suficiencia = APROXIMACIÓN Clase C~~ **suficiencia B-RUNTIME (testigo 14-07) y
   DERIVADA 30-07** (talk-031e-resolucion.md): la rama 0x13b4 con `result==1` llama a
   TALK 0x031E (ex-«0x1912 opaco») → guard_demand, `0x1e2`, → sin insignia ret 1 → 0x12ae;
   el clon (adyacencia sola) calca el caso sin-insignia.
   El loop de persecución (b) rand(0,0x3f) es Clase C aparte (movimiento, no dispara
   captura). ⚠ 30-07: el gate Black Badge (`g_time_spell==0x1d`, TALK 0x2a4) SÍ está
   en la vía de captura — con insignia el binario reta el password en la intercepción
   (tarjeta T-A); el pase persistente del clon sigue siendo modelo declarado
   (tarjeta T-B).
6. **"guardWander" ≠ palace guards** (aclaración del scout): el `guard_wander` de TOWN
   0x0C78 (loops.md §2.1, port en `world/loops/guards.ts`) es el vaivén de **OBJETOS de
   tile bajo 0x10/0x11** (`tile & 0xFE == 0x10`), un sistema DISTINTO. Los guardias de
   captura del Palacio son **NPCs type 0x70** con aiType 0 (fixed)/4 (merchant) y wander
   por el **dispatch NPC estándar** (`NpcManager`, npc.md §4.0), YA vivo — no hay rutina
   de wander propia que portar para ellos; el scout-blackthorn pieza B conflacionó ambos.
   ⚠️ **Corrección**: el Palacio SÍ tiene objetos con tile bajo 0x10/0x11 — los
   **caballos del establo** (npcs.json loc 18 slots 2-4, type 16/17 = sprite, `0x110`/`0x111`,
   byte bajo 0x10/0x11); mi claim previo "no hay objetos 0x10 en el Palacio" era FALSO.
   Pero `guard_wander` (0x0C78) sigue **sin cablear game-wide** (divergencia declarada,
   loops.md §3.5); no afecta a la captura, que va por los NPC type 0x70.

## 9. Preguntas abiertas (oráculo)
- **Byte de status** del revive por miembro del refuge (kernel 0xdc66 [= CS 0x7ef6 → CAST2.OVL:0x05e0 resurrect_apply](i,0xff), 0x0b90).
  El HP ya es asm-directo (currHp:=maxHp, 0x0b98).
- Gate real del password del Palacio (`g_time_spell==0x1d`) y si exige el Black Badge.
- Cuerpos exactos de los thunks de alerta 0x7B06/0x7B12/0x7B1E (owner-lookup + hostil).
- Minuto/día exactos tras el bucle advance_clock(9) del refuge (depende de la hora de
  entrada; el clon fija hora=6, minuto=0 como observable canónico).
- ~~Persistencia del slot 15 (0x5788) del compañero ejecutado en la ventana del save
  (byte-exactitud del roster) — Task F.~~ **CERRADA (24-08, carril save-residuos)** —
  ver §3.3.


> **A4-PARITY2 D-83 / D-84 (2026-10-03) — superseding note.** The shared resurrection routine is CAST2.OVL `0x05e0` (`resurrect_apply`, kernel stub `0x7ef6`; the "kernel `0xdc66`" of older notes is the raw near-call operand seen from the `0xa290` band). It has exactly four callers: In Mani Corp spell (CAST `0x10f3`, mode 0), the scroll (CAST `0x12ee`, mode 1), the healer (SHOPPES `0x16f5`, mode `0xff`) and the Refuge (BLCKTHRN `0x0b95`, mode `0xff`). For a member whose status byte is `'D'` it sets status `'G'` and HP 1, MP by class (A/M = INT, B = INT>>1, others untouched), cuts experience to `exp*karma/100` (truncated) when karma < 98, and for EVERY karma recomputes level (`1 + bitlength(exp/100)`) and max HP (`30*level`); the healer (`0x16f8-0x1703`) and the Refuge (`0x0b98-0x0b9d`) then copy the NEW max HP over HP. The Refuge runs it with the karma the party DIED with (the floor of 75 comes after the loop, `0x0bfd`). Earlier statements here that the Refuge's/healer's revive byte is "open", "canonical" or that the healer "coincides with the clone" were wrong about the clone: both ports skipped the routine (fixed in A4-PARITY2: reference `shops.ts` / `blackthorn.ts`, native `healer_heal` / `resolve_refuge` over one shared `resurrect_apply`). `native/core/a4-parity2-findings/D83D84-FINAL.md`.
