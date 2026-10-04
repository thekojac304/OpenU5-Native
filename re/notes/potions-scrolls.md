# Consumibles: POCIONES (8 colores) y PERGAMINOS (8 scrolls) — derivación del binario

Los dos sistemas que el censo docs-físicos (`docs/censo-docs-fisicos-port.md` §A1/A2)
marcó SIN PORTAR. Ambos lectores viven en **CAST.OVL** (base de carga 0xBF80) y se
alcanzan desde el (U)se → `cmd_use_item` (CAST 0x1792). Toda cita es
`re/disasm/CAST.OVL.asm` / `CAST2.OVL.asm` + offset; strings verificados contra
`original/u5/play/DATA.OVL` (`fileoff = DS + 0x10`, convención de `cast-input.md §5`).

## Despacho del (U)se — CAST 0x1792
```
1816: call 0x11de   ; LECTOR DE PERGAMINOS (scroll seleccionado 0-7)
182d: call 0x135a   ; BEBEDOR DE POCIONES  (color seleccionado 0-7)
```
El picker (`item_page_controller` ZSTATS 0x0f2e, modo 'U') devuelve el id de la tabla
extendida (0xB9EE): **scrolls 0-7, potions 8-0xf**, carpet 0x10, … El id se despacha
por la jump-table de CAST 0x185d.

## Convención de `g_location` (misma que `magic/cast.ts` CastContext)
`0` exterior · `1..0x20` pueblo · `0x21..0x7F` mazmorra · `>=0x80` combate.

---

## PERGAMINOS — lector CAST 0x11de
```
11e4: [bp-2] = 1
11e9: bx = arg (scroll 0-7)
11ec: dec byte[bx + 0x5820]     ; CONSUME el scroll (tabla DS 0x5820) — SIEMPRE, lo primero
11f0: print "Scroll\n\n" (DS 0x466a)
11fa: if arg>7 → exit
1205: jmp cs:[bx*2 - 0x2d40]     ; jump-table @file 0x1340
```
Jump-table (verificada leyendo el binario, `-0xBF80`): idx0→0x120a, 1→0x1222, 2→0x124a,
3→0x1264, 4→0x1278, 5→0x12b4, 6→0x12d8, 7→0x1300.

| idx | scroll (clue book pg 50) | eco DS | efecto derivado | gate |
|-----|--------------------------|--------|------------------|------|
| 0 | Vas Lor | 0x4673 "Light!\n" | `g_light_spell_mins = 0xF0` (240) vía CAST2 0x08ea; anim(0) | ninguno |
| 1 | Rel Hur | 0x467b "Wind change!\n" | getdir (CAST2 0x0306) → `set_wind` (CAST2 0x040a) **sólo si location<0x21** | overworld |
| 2 | In Sanct | 0x4689 "Protection!\n" | `g_time_spell='P'(0x50)`, `turns=0x64` (100) vía CAST2 0x08f8; anim(2) | ninguno |
| 3 | In An | 0x4696 "Negate magic!\n" | `g_time_spell='N'(0x4e)`, `turns=0x14` (20); anim(3) | ninguno |
| 4 | In Quas Wis | 0x46a5 "View!\n" | reveal: overworld(CAST2 ov#9) si loc<0x21, town(ov#10) si 0x21-0x7f; anim(4) | `loc>0x7f`(combate)→"Not here!\n" (0x46ac) |
| 5 | Kal Xen Corp | 0x46b7 "Summon Daemon!\n" | summon daemon (monstruo 0x26) CAST2 0x04c2 | `loc<=0x7f`(no combate)→"Not here!\n" (0x46c7) |
| 6 | In Mani Corp | 0x46d2 "Resurrection!\n" | selChar + `applyResurrect` (CAST2 0x05e0) + sfx | `loc>=0x80`(combate)→"Not here!\n" (0x46e1) |

> **Fila 6, corrección (2026-10-04, A4-PARITY2 §16.12, `MANI-FINAL.md`).** El lector empuja `resurrect_apply` con flag **1**
> (CAST.OVL 0x12ea). Con un objetivo cuyo status no es 'D' (compara el BYTE contra 0x44: G, P, S y cualquier otro) y flag != 0
> la rutina imprime «Not dead!» (DS 0x953c, CAST2.OVL 0x060a) y devuelve 0; el lector devuelve 0 y el epílogo de (U)se
> (CAST.OVL 0x1b8a-0x1b94) imprime «Failed!» (DS 0x4a7b) y el glide. Transcripción: `Scroll` / `Resurrection!` / `On who: <Nombre>` /
> `Not dead!` / `Failed!`; el pergamino se gasta primero (0x11ec), sin RNG. Un 'D' revive en silencio y un picker cancelado
> (`None!`) no añade nada. El HECHIZO (flag 0) es silencioso y su cola imprime sólo «Failed!». En el arena: `Resurrection!`,
> `Not here!`, sin «Failed!». Los dos puertos descartaban el booleano; ahora lo usan (`main.ts`, `world_magic.cpp` caso 6).
| 7 | An Tym | 0x46f8 "Negate time!\n" | `g_time_spell='T'(0x54)`, `turns=0x14` (20); anim(7) | `loc∈{0x1d,0x28}`→"No effect!\n" (0x46ec)+beep |

**Magnitudes DISTINTAS del Cast** (el scroll no es "castear sin maná"): luz 240 vs
255 (Cast Vas Lor), protección 100 turnos vs 20 (Cast In Sanct), negate 20 vs 10,
time-stop 20 vs 10. Los scrolls son MÁS potentes. Estados globales `g_time_spell` =
mismos códigos que `magic/tables.ts` TIME_STATUS ('P'/'N'/'T').

`0x50`='P' aquí es el **estado global de protección** (party-wide `state.timeSpell`),
NO el `status='P'` (envenenado) de personaje — variables distintas, sin colisión.

CAST2 0x08f8 (`set_time_spell`): args (animId@bp+4, turns@bp+6, status@bp+8), orden de
push right-to-left. CAST2 0x08ea (`set_light`): arg (mins@bp+4).

Legible por CUALQUIERA: **sin gate de maná/reagente/nivel/capacidad mágica** (el lector
no toca `currentMp` ni comprueba clase). El "Resurrection!" del scroll (0x46d2) es el
que la memoria del proyecto atribuye a 0x11de (NO al Cast) — `magic/cast.ts:313`.

---

## POCIONES — bebedor CAST 0x135a
```
1362: [bp-6] = 1
1367: bx = arg (color 0-7)
136a: dec byte[bx + 0x5828]     ; CONSUME la poción (tabla DS 0x5828) — SIEMPRE, lo primero
136e: print "Potion\n" (DS 0x4706)
1375: if loc<=0x7f → selChar (CAST2 0x009e); else target = g_cmb_actor (combate)
1394: if target<0 → exit (la poción YA se consumió)
139b: anim(color_original)                                   ; CAST2 0x0000
; --- RNG (0x2092 randRange inclusivo, vía 0x6112) ---
13a8: r = rand(0,0x0f)
13b2:   if r==0  → color = 4              (fuerza ORANGE=dormir — el "fiasco 1/15" del clue book)
13c0:   elif r==1 → color = rand(0,7)     (efecto de OTRO color — "1/15 otro color")
13db: jmp cs:[color*2 - 0x2b60]           ; jump-table @file 0x1520
```
Jump-table (verificada): idx0→0x13e0, 1→0x142a, 2→0x1448, 3→0x1460, 4→0x1478,
5→0x14a0, 6→0x14dc, 7→0x1514.

Nombres de color (name-table DATA.OVL DS 0x067c): Blue, Yellow, Red, Green, Orange,
Purple, Black, White (== `ui/ztats.ts` POTION_NAMES).

Registro de PJ: `char*32 + 0x55b3`, **byte 0 = status** ('G'0x47 sano · 'P'0x50 veneno ·
'S'0x53 dormido · 'D' muerto).

| color | eco DS (éxito) | efecto | gate/condición |
|-------|----------------|--------|----------------|
| 0 Blue | — (sólo "Potion\n") | despierta status 'S'→'G' (`applyAwaken`) | requiere 'S'; éxito SIN eco extra |
| 1 Yellow | 0x470e "Healed!\n" | cura parcial `applyMani` (rand30, cap maxHP) CAST2 0x03c2 | eco sólo si curó>0 |
| 2 Red | 0x4717 "Poison cured!\n" | 'P'→'G' (`applyCure`) | requiere 'P' |
| 3 Green | 0x4726 "POISONED!\n" | 'G'→'P' (`applyPoison`, ENVENENA) | requiere 'G' |
| 4 Orange | 0x4731 "Slept!\n" | 'G'→'S' (`applySleep`, DUERME) [combate: ULTIMA.EXE 0x68AE, desde CAST.OVL 0x1497] | requiere 'G' |
| 5 Purple | 0x4739 "Poof!\n" | SÓLO COMBATE: transforma tile del actor a 0x90 (gas) | `loc<=0x7f`→"\nNo noticeable effect now!\n" (0x474c) |
| 6 Black | 0x4740 "Invisible!\n" | SÓLO COMBATE: flag 0x10 + sprite 0x1d | `loc<=0x7f`→"\nNo noticeable effect now!\n" |
| 7 White | — (reveal cosmético) | reveal/x-ray (CAST2 0x046c, radio 0x20, 20 frames) | `loc>=0x21`→"\nNo noticeable effect now!\n" |

**Aleatoriedad (1/16 cada rama, el clue book aproxima 1/15):** rand(0,15)==0 →
efecto de color 4 (ORANGE=dormir, el "fiasco"); ==1 → efecto de color aleatorio 0-7.
La poción CONSUMIDA es siempre el color ORIGINAL; el color rerolleado sólo decide el
efecto. El clue book llama "venenoso" al fiasco; el binario manda: es DORMIR.

**Green (3) es inherentemente veneno** (no "se degrada": envenena por diseño).

Consumo ANTES del target-select: cancelar el picker de PJ gasta la poción igual.

### Clase-C / diferido
- **Purple/Black efecto REAL** exige el modelo de tile/sprite de combate (transform 0x90,
  flag invisible 0x10). Vía (U)se el juego nunca está en combate (loc<=0x7f), así que por
  (U)se SIEMPRE dan "No noticeable effect now!" — FIEL. El efecto de combate = integración
  del menú de combate (fuera de este carril).
- **White reveal** (CAST2 0x046c) es una animación cosmética de mapa (sin cambio de estado
  persistente); se deja como no-op con eco vacío (fiel: el binario no imprime línea de efecto).
- **Rel Hur scroll / In Quas Wis view / Kal Xen Corp summon**: los efectos que tocan mapa
  overworld/combate reusan appliers existentes donde los hay; reveal/summon quedan Clase-C
  con eco fiel + gate correcto.
