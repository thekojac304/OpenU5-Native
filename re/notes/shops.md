# Tiendas exactas: SHOPPES.OVL + SHOPPES2.OVL + SHOPPES3.OVL (Task 3.6)

Re-derivación EXACTA de los 8 tipos de mercader desde el desensamblado.
Direcciones = **offset de fichero** dentro del .OVL indicado. Rebase de `call`
near a kernel (técnica de 3.4/3.5): SHOPPES.OVL `load_seg*16 = 0xA290`;
SHOPPES2/SHOPPES3.OVL `load_seg*16 = 0xE1E0`; `true_CS = (target + load_seg*16) &
0xFFFF`. `rand(lo,hi)` = kernel far-call 0x7E02 (inclusive). Tablas DATA.OVL:
`fileoff = DS + 0x10`. Helpers Borland: __lmul 0x61B2, __ldiv 0x6110 (trunc hacia
cero, con signo). Globals: g_gold DS 0x57AA (word), g_food 0x57A8, g_karma 0x5888.

Port: `game/src/core/shops/shops.ts` + `shop-tables.ts` (tablas con cita DS).
Paridad: `re/parity/shops/*.json` + `re/tools/shops_parity.py` +
`__parity__/shops-run.ts`. Fidelidad: `re/verified/shops.md`.

## 0. HALLAZGO CAPITAL — el ajuste de precio usa **INTELIGENCIA**

Todas las tiendas de SHOPPES.OVL ajustan por el stat en `[charIdx*0x20 + 0x55B6]`.
El record mide 0x20 B desde DS 0x55A8; `+0x0E` = **INT** (0x55A8+0x0E = 0x55B6).
El clon-Redux usaba DEX (equipo) y KARMA (reactivos) — **falso** para el binario.

| shop | cita | stat |
|---|---|---|
| blacksmith buy/sell | SHOPPES:0x0896 / 0x0EB9 `[bx+0x55b6]` | INT |
| reagents buy | SHOPPES:0x0587 | INT |
| guild buy | SHOPPES:0x02F4 | INT |
| transport buy | SHOPPES:0x087C (0x07be) | INT |
| taberna raciones / posada / shipwright | SHOPPES2:0x03B3 / SHOPPES3:0x00B0 / SHOPPES2:0x0959 | INT |

### 0.1 Fórmula de COMPRA — `buy_price_adjust` (SHOPPES:0x02D8-0x0318 y gemelos)
```
b118 = base
__lmul(base, 100 - 3*INT) ; __ldiv(_, 100)   ; = ⌊base·(100−3·INT)/100⌋ (trunc 0)
b118 += ese término
```
**buyPrice = base + ⌊base·(100 − 3·INT)/100⌋** (DOS pasos: la división trunca el
término interno, luego se suma `base`; NO es `⌊base·(200−3·INT)/100⌋`). Punto
neutro INT = 100/3 ≈ 33.33. INT 0 ⇒ 2×base; INT 50 ⇒ ~½base. Sin clamp de mínimo.
Confirmado el orden de operandos leyendo los pushes de 0x0574-0x05AB (reactivos).

### 0.2 Fórmula de VENTA — `sell_one_item` (SHOPPES:0x0E76)
```
al = INT ; mul base (16×16→32) ; dx:ax = 3*(INT*base) ; __ldiv(_,100) ; inc ax
```
**sellPrice = ⌊3·INT·base/100⌋ + 1** (INT del vendedor). Si
`equipmentBasePrices[item]==0` el herrero no compra el ítem (0x0EA2).

### 0.3 `post_purchase_gold_rand` (SHOPPES:0x019A) — RESUELTO estáticamente
```
019a: cmp byte [g_shadowlord_here_idx], 0 ; jne ret   ; 0x5958
      call 0x9CC4(&gold, rand(1,64))
```
`0x9CC4` rebasa a kernel **0x3F54 = sub_word_floored(&dst, amt)** (`if *dst<=amt:
*dst=0 else *dst-=amt`). ⇒ **tras CADA pago, si `g_shadowlord_here_idx==0`,
`gold -= rand(1,64)` (suelo 0)** — una MERMA aleatoria de oro, no bonificación.
**Gate DECIDIDO** (era la incógnita): `0x5958` es el índice del Shadowlord presente
en la ciudad, no un flag genérico. TOWN.OVL:0x02B6-0x0306 pone `[0x5958]=0xFF` y
recorre `si=0..2` comparando `[si+0x58C8]` (`g_shadowlord_locs`, kernel-survival.md
:305) con `g_location`; al primer match `[0x5958]=si`. También lo lee
TALK.OVL:0x1187. ⇒ la merma dispara con `==0` = **Falsehood (Shadowlord 0) está en
la ciudad** — los mercaderes te sisan cuando reina la Falsedad. **No portado**: el
efecto EXISTE pero requiere el estado de posición de los Shadowlords (Task 3.10);
consume 1 rand del kernel por compra → relevante para paridad de RNG de mundo.

## 1. Blacksmith (armas/armadura) — SHOPPES.OVL:0x12B2
Menú B(uy)/S(ell). Stock = `weaponsSoldByMerchants[town][slot]` (DS 0x3AE2, 9×8,
0xFF=fin); `town = g_unk_b114` (índice 0..8 en SHOP_TOWNES.Blacksmith). Compra:
base `equipmentBasePrices[item]` (DS 0x3A82, 48 words) → §0.1; **rechaza a 99**
(0x0A5E `cmp [si+0x57c0],0x63`); grant `g_equip_qty[0x57C0+item] += 1` cap 99
(add_byte_capped 0x9C60). Venta: §0.2, gold cap 9999 (0x0F3D add_word_capped 0x9C84),
`sub_byte` del inventario. **El herrero rechaza los ítems 0x1B y 0x1D** (0x0E7D
`cmp 0x1b/0x1d` → "cannot buy that") y los de precio-base 0 (0x0EA2).

## 2. MagicSeller / reactivos — SHOPPES.OVL:0x075E → 0x0502
base `reagentBasePrices[town*8+slot]` (DS 0x3A32, **5 towns × 8**, byte) → §0.1;
grant `g_reagent_qty[0x5850+slot] += reagentQuantities[town*8+slot]` cap 99. El
input "how many" (0x7C82) se lee, avisa si >12 (0x05D9), pero **NO escala precio
ni cantidad** (se descarta; el grant es la cantidad FIJA de la tabla, RESUELTO
leyendo 0x05D6-0x0637). `town = g_unk_b114` (SHOP_TOWNES.MagicSeller, 5 towns). El
clon-Redux usaba karma + "pay-what-you-want" + input libre — **falso**.

## 3. GuildMaster (llaves/gemas/antorchas) — SHOPPES.OVL:0x04A2 → 0x02BA
Menú A/B/C → item 0/1/2. base `guildPrices[town*8 + item*2]` (DS 0x3BEA, 4 words/
town) → §0.1. Grant por LOTE FIJO: **keys +3 / gems +4 / torches +5**, cap 99
(0x0381). 3 gremios usados (SHOP_TOWNES.GuildMaster); la tabla tiene 4 filas y una
4ª columna que 0x02BA no lee.

## 4. HorseSeller (caballo) — SHOPPES.OVL:0x07BE
base `transportPrices[g_unk_b114*2 + 0x3C30]` = {100,130,160,190} → §0.1. Requiere
casilla adyacente válida {0x44,0x45,0x05} libre; al pagar coloca un objeto tile
0x10 en el mundo. **Equivalencia con el clon**: `2×base_bin == base_clon` — el
regateo del binario a INT=0 da `2×base`, igual que la codificación de Redux
(base 100 → 200 a INT 0 = Trinsic). El clon ahora usa la base binaria + §0.1.

## 5. Healer — SHOPPES.OVL:0x14F8
'C' cure (status 'P'→'G'), 'H' heal (HP<max & vivo → HP=maxHP), 'R' resurrect
(status 'D' → kernel 0xDC66 [= CS 0x7ef6 → CAST2.OVL:0x05e0 resurrect_apply] + HP=maxHP). Precios PLANOS por ciudad, **sin ajuste
de stat**: `healPrices` (DS 0x3D86), `curePrices` (0x3D8E), `resurrectPrices`
(0x3D96 words). `g_unk_b114` = índice en SHOP_TOWNES.Healer (7). ✅ Coincide con el
clon (semántica + precios planos). Minoc (loc 5) tiene diálogo distinto (cosmético).

## 6. InnKeeper (posada) — SHOPPES3.OVL:0x08B4
**6 posadas** (SHOP_TOWNES.InnKeeper), **SIN RNG**. Mismo regateo §0.1 con INT del
negociador. Campos del record: `+0x0A` clase (A/M mago pleno, B medio-mago),
`+0x0B` status, `+0x0E` INT, `+0x0F` MP, `+0x10` HP(word), `+0x12` maxHP,
`+0x17`=0x55BF meses hospedado, `+0x1F`=0x55C7 tag de posada (0=viaja; =location
mientras hospedado). En el clon: `class`, `status`, `intelligence`, `currentMp`,
`currentHp`/`maxHp`, `monthsAtInn`, `partyStatus`.

- **Rest (R, 0x0072)**: capacidad `INN_CAPACITY[inn]` (DS 0x4DC4) vs huéspedes aquí
  (records con `+0x1F==location`) → "no room". `base = INN_RATE[inn]·party_size`
  (DS 0x4D7E = {2,3,2,3,2,3}); `precio = §0.1`. Si `gold<precio` → "Highwaymen!…
  OUT!". Teleporta a `(INN_ROOM_X, INN_ROOM_Y)` (**tablas** DS 0x4E7A/0x4E80 de 6 bytes
  indexadas por posada = innBedsX/Y del clon; consumidor SHOPPES3 0x0164
  `mov al,[bx+0x4e7a]`→`g_party_x` y 0x016b `mov al,[bx+0x4e80]`→`g_party_y`; valores
  byte-exactos {21,15,25,20,27,7} / {10,7,9,1,6,26}).
  Avanza el reloj hasta `g_hour==6`. Por miembro vivo: HP=maxHP; MP por
  clase (A/M → INT, B → INT/2); `S→G`; **`P → D` (HP=0): el descanso MATA a los
  envenenados** (0x0200-0x0282).
- **Leave (L, 0x02AE)**: `party==1` → "One must first be left behind!"; idx 0
  (Avatar) → "will not leave thee!". **NO cobra**: copia al register, `+0x17=0`,
  `+0x1F=location`, `party--`. La tarifa (`§0.1(rate·10)`) sólo se MUESTRA.
- **Pick up (P, 0x04E6)**: `party==6` → rechazo. `meses = max(1, +0x55BF)`;
  `precio = §0.1(rate·10)·meses`. Si `gold<precio` aborta. Reinserta, `party++`,
  `+0x1F=0`. Si estaba `P` → `D` ("died, by the way"); restaura MP por clase.

El clon-Redux (coste fijo 4/40, 6/60) no modelaba regateo, ×meses, muerte por
descanso ni MP por clase — todo portado exacto.

## 7. SHOPPES2.OVL — Barkeeper (taberna) + Shipwright
Dispatcher table-driven por `bd16=[b114+0x4D4C]` (mapa loc→shoptype
`00 00 00 02 03 01 00 02 00 00`). Taberna = bd16 1. Scratch: 0xBD16 shop-type,
0xBD18 lore-lock, 0xBD1A/1C vivos, 0xBD20 copas, 0x6605 flags de nave.

### 7A. Taberna (0x066C) — SHOP_TOWNES.Barkeeper (9 towns)
- **Ronda** (0x01D2→0x00DC): `precio_cabeza[b114*2+0x4C36]` = {3,4,5,3,2,5,3,4,5};
  coste = **vivos** × precio (excluye 'D'). Si `gold<coste` → "CAN'T PAY? Beat
  it!" sin cobrar; si no gold-=coste y sirve tiles de comida.
- **Vinos** (0x01F4): `[·+0x4C48]` = {Rose 18, Claret 192, Sauterne 79, Muscatel
  30, Moselle 275, Chablis 98} (1 copa/compra). **Gate EXACTO en 3 servicios**
  (0x020A `cmp [bd20],3 ; jne`): dispara SÓLO cuando el contador vale EXACTAMENTE 3
  (con 4+ no re-avisa ni re-castiga). El contador `g_cups_served` (0xBD20) lo
  incrementan TANTO las rondas de comida (0x01C4, `serve_round_and_pay`) COMO las
  copas de vino (0x0364), no sólo las copas. Al insistir ('N'), `[0x5957]=0x19`
  (borrachera 25 turnos) + `call 0x5D56(&karma,1)`. **0x5D56 RESUELTO** = kernel
  0x3F36 = `sub_byte_floored(&karma,1)` ⇒ **RESTA 1 al karma** (suelo 0). Si
  `precio>gold` → "afford only".
- **Raciones** (0x0380): `base[b114*2+0x4C54]` = {10,15,20,25,30,25,20,25,30};
  **regateo §0.1 por INT del miembro**; loop: mientras gold≥precio, gold-=precio y
  **+25 comida** (cap 9999)/unidad. Si `g_food<3` añade `rand(0,1)+1` (RNG, rama
  de comida baja — no portada al core puro).
- **Rumores** (0x0508): 1/visita (gate bd18==0). Prefix-match contra **26
  keywords** @0x4C74; precio `[kw*2+0x4D10]` (honesty 50 … undead 100). Lugar por
  `barKeepGossipMap[kw+0x4CDC]`; formulación por `rand(0,3)`.
- **RNG taberna** (wrapper 0x3EB2): 0x03FB `rand(0,6)` (frase raciones), 0x0493
  `rand(0,1)+1` (comida baja), 0x063D `rand(0,3)` (rumor).

### 7B. Shipwright (0x0ABC → 0x08A8) — F=Frigate, S=Skiff
'F'(0x46) base `[b114*2+0x4D66]` = {600,753,650,700}; 'S'(0x53) base `[·+0x4D6E]` =
{200,175,125,100}; regateo §0.1 por INT. Al comprar, `commit` (0x080E) coloca la
nave en el muelle: `[0x5953]=[b114+0x4D76]` X={39,151,79,138}, `[0x5954]=
[b114+0x4D7A]` Y={221,21,109,159}. Path de reemplazo a 10000g (`[0x6605]&0xC0` ya
activo) — flag "ya tienes fragata"; expuesto como `SHIP_REPLACEMENT_PRICE`, no
cableado. `2×base_bin == base_clon` (misma equivalencia que caballos).

## 8. shoppeKeeperTownes — DS 0x23CA, 8 listas stride 0x10
`g_unk_b114` = índice 0-based de la ciudad DENTRO de la lista de su tipo (lo fija
el kernel/TALK al entrar). Listas en `shop-tables.ts:SHOP_TOWNES` (validadas:
Blacksmith=9 ✅, Shipwright=frigate/skiff towns ✅, Healer=HEALING_TOWNES ✅,
InnKeeper=6 ✅).

## 9. Divergencias con el clon corregidas (todas portadas)
1. Stat de ajuste **INT** en todas las tiendas (era DEX/KARMA). ⭐
2. Buy `base + ⌊base·(100−3·INT)/100⌋` (dos pasos, trunc), Sell `⌊3·INT·base/100⌋+1`.
3. Reactivos: tabla por-ciudad 5×8 + INT + cantidad FIJA `reagentQuantities`;
   input "how many" ignorado. (Era karma + input libre.)
4. Guild: lote fijo +3/+4/+5 cap 99, `guildPrices` por-town + INT.
5. Transporte: base binaria 100/130/160/190 (caballo), F/S por astillero.
6. Posada: rest = §0.1(rate·party); leave gratis; pickup = §0.1(rate·10)·meses;
   rest MATA envenenados, MP por clase, reloj a 6:00, capacidad.
7. Taberna: ronda por vivos, vinos con gate 3 copas (karma −1 + borrachera 25),
   raciones +25/ud regateadas, 26 rumores con precio.
8. Shipwright: F/S regateado + coords de muelle.

## 10. Preguntas de oráculo — estado
RESUELTAS estáticamente (disasm, sin dosbox): (a) karma gate-borrachera = **−1 con
suelo** (0x5D56 = kernel 0x3F36 sub_byte_floored); (b) 0x019A = **gold −= rand(1,64)
con suelo** cuando `g_shadowlord_here_idx==0` = **Falsehood en la ciudad** (0x9CC4 =
kernel 0x3F54 sub_word_floored; gate DECIDIDO, ver §0.3); (c) redondeo = **trunc
hacia 0 en dos pasos** (orden de operandos confirmado); (d) reactivos: **input
ignorado, grant fijo** (0x05D6-0x0637); (e) gate del vino = **==3 exacto sobre el
contador de servicios comida+bebida** (0x020A). ABIERTAS (no bloquean): charIdx
concreto que pasa el dispatcher a cada entry (asumido activo/líder);
muerte-por-descanso confirmada en runtime (canal BSS, ver verified).

## Globals nuevos (a globals.json)
- `g_shoppe_id` DS 0xB114 (word) — índice de ciudad dentro del tipo (`g_unk_b114`).
- `g_shoppe_id2` DS 0xB116 (word) — 2º índice (greeting).
- `g_shop_accum` DS 0xB118 (word) — acumulador de precio/coste.
- `g_shop_qty` DS 0xB11A (word) — cantidad concedida (reactivos).
- `g_shop2_type` DS 0xBD16, `g_lore_lock` 0xBD18, `g_alive_a/b` 0xBD1A/1C,
  `g_cups_served` 0xBD20 (servicios de comida+bebida, gate ==3),
  `g_ship_pay_reject` 0xBD22, `g_ship_bought` 0xBD24 (SHOPPES2 scratch).
- `g_ship_flags` DS 0x6605 (byte) — bits de compra de nave (0x40/0x80/0x82).
- `g_drunk_timer` DS 0x5957 (byte) — turnos de borrachera (=0x19 al gate).
- `g_shadowlord_here_idx` DS 0x5958 (byte) — índice del Shadowlord presente en la
  ciudad (0..2, 0xFF=ninguno; lo fija TOWN.OVL comparando `g_shadowlord_locs`);
  gate de la merma 0x019A (==0 = Falsehood).
- `g_shadowlord_locs` DS 0x58C8 (3 bytes) — localización de cada Shadowlord.


> **A4-PARITY2 D-83 / D-84 (2026-10-03) — superseding note.** The shared resurrection routine is CAST2.OVL `0x05e0` (`resurrect_apply`, kernel stub `0x7ef6`; the "kernel `0xdc66`" of older notes is the raw near-call operand seen from the `0xa290` band). It has exactly four callers: In Mani Corp spell (CAST `0x10f3`, mode 0), the scroll (CAST `0x12ee`, mode 1), the healer (SHOPPES `0x16f5`, mode `0xff`) and the Refuge (BLCKTHRN `0x0b95`, mode `0xff`). For a member whose status byte is `'D'` it sets status `'G'` and HP 1, MP by class (A/M = INT, B = INT>>1, others untouched), cuts experience to `exp*karma/100` (truncated) when karma < 98, and for EVERY karma recomputes level (`1 + bitlength(exp/100)`) and max HP (`30*level`); the healer (`0x16f8-0x1703`) and the Refuge (`0x0b98-0x0b9d`) then copy the NEW max HP over HP. The Refuge runs it with the karma the party DIED with (the floor of 75 comes after the loop, `0x0bfd`). Earlier statements here that the Refuge's/healer's revive byte is "open", "canonical" or that the healer "coincides with the clone" were wrong about the clone: both ports skipped the routine (fixed in A4-PARITY2: reference `shops.ts` / `blackthorn.ts`, native `healer_heal` / `resolve_refuge` over one shared `resurrect_apply`). `native/core/a4-parity2-findings/D83D84-FINAL.md`.
