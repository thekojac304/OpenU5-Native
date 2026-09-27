/**
 * BUS DE EVENTOS DE SONIDO (task #3) — el lado CORE.
 *
 * El core es puro y NO sabe cómo suena nada: emite EVENTOS LÓGICOS de sonido
 * (`SfxCue`) en los MISMOS puntos donde el binario original llama al PC-speaker
 * (xref exhaustivo en `re/notes/sfx-catalog.md`). Cada PIEL decide su timbre:
 * la fiel los sintetiza como el speaker de 1988 (`skin/fiel/speaker.ts`), una
 * piel moderna podría mapearlos a XMI/SFX (decisión #10 de la interview).
 *
 * Un `SfxCue` es sólo un ID de ACCIÓN (+ un parámetro escalar opcional cuando la
 * acción varía en runtime: dígito de instrumento, signo de la transacción de
 * tienda…). Los parámetros ACÚSTICOS exactos (inc/delay/count/start/step de cada
 * primitiva) viven en la piel, NO aquí: el core no conoce Hz ni duraciones.
 *
 * SEPARACIÓN / PARIDAD: emitir un cue es empujar un `GameEvent` de kind `"sfx"`
 * en el array que la acción ya devuelve — CERO cambios de lógica y CERO consumo
 * de `g_rng` (el ruido del speaker usa su PRNG LOCAL `[0x545c]`, portado en la
 * piel, que no toca el stream del juego). Regla A intacta: este módulo no importa
 * nada de presentación; es el contrato el que transporta el cue hacia la piel.
 */

/**
 * ID lógico de acción sonora. Uno por familia de emisión del catálogo (§6 de
 * `sfx-catalog.md`). La piel mapea cada ID a su(s) primitiva(s) con los params
 * exactos derivados del asm. Los timbres/Hz reales quedan para el catálogo AV
 * (task #4); aquí sólo importa QUÉ acción sonó, no cómo.
 */
export type SfxId =
  // ── Combate (noise_burst) ───────────────────────────────────────────────
  // El SONIDO DEL GOLPE es el flash de impacto `kernel 0x3564` (combat-ui-spec §3)
  // y va POR BANDO del objetivo (0x35ac `test [bx+2],0x80` = registro de actor
  // 0xba14 con flag de JUGADOR + blink del marcador HP del slot `[bx+3]` vía
  // 0x2a28): jugador → NB(40,3000,500) @0x35c9 (heavy); enemigo/no-combate →
  // NB(10,3000,2000) @0x35de (estándar). La lectura previa «heavy = golpe
  // pesado/letal» (§3.1) queda SUPERADA por esta derivación (carril audio-costuras).
  | "combat-hit" // golpe que acierta sobre ENEMIGO — NB (10,3000,2000) @0x35de
  | "combat-hit-heavy" // golpe que acierta sobre un PJ (bando jugador) — NB (40,3000,500) @0x35c9
  // `combat-damage` = kernel 0x2a52 `party_member_take_damage` (blink 0x2a28 +
  // NB(10,1600,2000) @0x2a68 + HP−= en 0x55b8). Sus callers derivados son los
  // caminos de DAÑO DE TRAMPA/HAZARD (no el melee del arena): ACID de cofre
  // (0x2fd0 caso 0 @0x3032), bucle de daño a toda la party 0x2aa8 @0x2ad3
  // (bomba/foso/campo eléctrico/fuego de mazmorra, DUNGEON far-calls 0x04f7/
  // 0x0aea/0x0dc3), y hazards kernel @0x2b40. Se emite UNO por miembro dañado.
  | "combat-damage" // un PJ recibe daño (kernel 0x2a52) — NB (10,1600,2000)
  | "combat-defeat" // desvanecer / derrota — NB (40,3000,500)
  // ── Magia ────────────────────────────────────────────────────────────────
  // 🔴 ATRIBUCIÓN REFUTADA, SIN ARREGLAR (carril cola-cast, cuerpo entero de
  // ULTIMA.EXE:0x4368 leído; re/notes/cola-cast-acta.md §6 y sfx-catalog §3.5
  // CORREGIDA). Esta secuencia TS×4 es `sfx_victory_fanfare`: sus DOS únicos
  // llamadores en el binario son COMBAT.OVL:0x0d02 (justo tras "\nVICTORY!\n" y
  // g_cmb_victory_flag=1) y CAST.OVL:0x1759 (cola de use_shard_at_flame, tras "The
  // doom of the Shadowlord X is wrought!"). NINGUNO es lanzar un conjuro; la rama de
  // derrota ni la toca. El «Llamado por CAST» de la nota vieja era cierto como
  // FICHERO y falso como sujeto. Lo que el despachador de conjuros sí emite en una de
  // sus ramas (CAST.OVL:0x0e6e) es tone_sweep(0x2648,1,0x6d60,0x3e8,2) = el cue
  // `spell-zap` de abajo. NO se cambia aquí qué suena: cambiarlo es decisión del lead
  // (presentación pura — 0x4368 no toca globales ni RNG, así que NO mueve stream).
  | "cast-spell" // hoy: emitido al castear (main.ts 2186/2948/3034). Ver 🔴 arriba.
  | "spell-zap" // efecto por-hechizo agudo (zap) — TS
  // ABANICO de hechizo de línea (In Zu/In Nox Hur/In Flam Hur/In Vas Grav Corp):
  // CAST.OVL 0x1f60 abre con noise_burst(step=0x320, dur=por-modo, band=0x2bc)
  // (0x1fb0-0x1fbc → kernel 0x223c) y el trazador 0x1bb0 retrigger a set_tone
  // (0x22e2) con rand(100,10000) POR PÍXEL pintado (0x1bf4-0x1c00) — el crackle.
  // `n` = modo 1..4 (dur: 1→0x3e80, 2→0x4b00, 3/4→0x5140; 0x1f91-0x1faa/0x2088).
  | "line-spray" // abanico de línea — NB + crackle (el rand del crackle NO es g_rng aquí)
  // JINGLE del pergamino de hechizo-de-tiempo (In Sanct/In An/An Tym): CAST2 0x0000
  // (vía setter 0x08f8, stub 0x80b2 desde el tail del lector 0x125c): NB de entrada
  // (step=0x320, dur=0x1f40+0x640·idx, band=0x2bc) + DOS tone_sweep 0x2192 espejo
  // (inc=[0x4af6+2i], delay=1, count=0x2710+0xfa0·idx, f0=[0x4b08]/[0x4b1a],
  // step=±[0x4b2c]) — subida y bajada. Durante el jingle el viewport queda INVERTIDO
  // (XOR blanco, ver skin fiel). `n` = idx del scroll (2=In Sanct, 3=In An, 7=An Tym).
  | "time-spell" // pergamino de tiempo — NB + sweep↑ + sweep↓ (tablas DS 0x4af6-0x4b2c)
  // WELL DONE del Altar (#295, CAST2 0x0c44-0x0c85): la MISMA forma de dos bucles espejo
  // que el ritual del shard de aquí abajo — `si` = 0x7d0 → 0x61a8 de 0x32 en 0x32 y vuelta,
  // 460+460 = 920 llamadas a `tone_sweep` con los cinco argumentos constantes salvo `si`:
  // (0xc1c, 1, 0x96, si, 0). Es el tramo que SOSTIENE la inversión del viewport: entre el
  // rect XOR de 0x0c41 y el `kernel_flash(10)` de 0x0d1a no hay nada más que estos barridos
  // (y el 0x3072 de 0x0c88, sin cablear — #300, consume RNG).
  | "shrine-well-done" // entrega de misión en santuario — dos barridos espejo (0xc1c,1,0x96,si,0)
  // ALAKAZAM de la DONACIÓN aceptada (#364, CAST2 0x0bd0-0x0c0f, rama 0x0b1d de
  // `shrine_visit`): TERCER miembro de la familia de dos bucles espejo del shard/WELL DONE
  // — mismas cotas (`si`=0x7d0→0x61a8→0x7d0, paso 0x32 del BUCLE, no del sweep) y los cinco
  // args del tono constantes salvo `si`: (0xa8c, 1, 0xc8, si, 0) @0x0bd3-0x0be2/0x0bf5-0x0c04,
  // 460+460 = 920 llamadas a `tone` (0x3fb2). `count`=0xc8 es el del SHARD (el WELL DONE usa
  // 0x96); `inc`=0xa8c es propio. SIN sacudida: esta rama no llama a 0x4e92 (salta a 0xd16).
  // Sostiene la inversión del viewport entre el rect XOR de 0x0bcd y el kernel_flash(10) de
  // 0xd16 — mismo régimen «suelto» que el WELL DONE (#295). Sólo suena en el ÉXITO (n≥1 y
  // oro suficiente): las salidas n=0 / sin-oro / mantra fallado no pasan por aquí.
  | "shrine-donation" // ALAKAZAM de la donación — dos barridos espejo (0xa8c,1,0xc8,si,0)
  // MELODÍA del ORDAINED (#364-b, CAST2 0x0adb-0x0b02, rama 0x0a81 de `shrine_visit`):
  // tras el «Return again when thy Quest is done!» (print 0x0ac3) el binario toca SIETE
  // notas — un bucle con UNA llamada a `tone` (0x3fb2 → kernel 0x2192) por iteración y
  // los CINCO argumentos leídos de CUATRO tablas paralelas de 7 words en DS (punteros
  // avanzando de 2 en 2, tope `cmp si,0x4c1e` @0x0afe): `inc`=[0x4be6] (`di` @0x0acb),
  // `delay`=1 constante (@0x0add), `count`=[0x4bf4] (@0x0ace), `start`=[0x4c02] (@0x0ad3),
  // `step`=[0x4c10] (@0x0ad8). Valores LEÍDOS de DATA.OVL (fileoff = DS+0x10) — tablas
  // verbatim en la piel. SIN inversión NI sacudida: la rama no llama a 0x2890/0x29a6/0x4e92
  // (sale por `jmp 0xd16` @0x0b04 = el kernel_flash(10) común del rito). Sólo suena en
  // ORDAINED: las demás ramas (WELL DONE, donación, fallo, vacía) no pasan por 0x0ac6.
  | "shrine-ordained" // melodía de 7 notas del ORDAINED — tablas DS 0x4be6/0x4bf4/0x4c02/0x4c10
  // ── Objetos / mundo ────────────────────────────────────────────────────────
  | "sceptre" // usar el Cetro — TS (0xfd2,1,65000,1,1)
  // MATERIALIZACIÓN de Blackthorn en la escena de captura (#324): tone_sweep único de
  // BLCKTHRN 0x082b-0x083f (pushes 0xaf0/1/0x32c8/0x64/5, notación PUSH del corpus =
  // (inc,delay,count,start,step)), justo antes del fizzle-in del trono (kernel 0x1068).
  | "blackthorn-materialize" // aparición de Blackthorn — TS (0xaf0,1,0x32c8,0x64,5)
  // RITUAL DEL SHARD (#201). Los DOS barridos de `use_shard_at_flame` son la MISMA rutina
  // que el Cetro (`tone_sweep`, CAST.OVL 0x6212 → ULTIMA.EXE 0x2192 con base 0xBF80), pero
  // NO son dos llamadas: son dos BUCLES de 460 llamadas cada uno, leídos de CAST:
  //  · ASCENDENTE  (0x15dd-0x15fc): `si` = 0x7d0(2000), `add si,0x32` hasta `cmp si,0x61a8`
  //  · DESCENDENTE (0x160d-0x162a): `si` = 0x61a8, `sub si,0x32` hasta `cmp si,0x7d0`
  // Los 5 argumentos son CONSTANTES en las 920 llamadas salvo `si`: (0xa50, 1, 0xc8, si, 0).
  // 🔴 El `±0x32` es el paso del BUCLE sobre `start`; el argumento `step` que se empuja es
  // CERO (`sub ax,ax` en 0x15ed/0x161d) — quien lea «paso ±50» como el 5º argumento estará
  // describiendo una llamada que el binario no hace.
  // 🔴 Y bajo el modelo de tono de esta casa las DOS mitades son INDISTINGUIBLES: el pitch
  // sale de `inc` (constante), y `start`/`step` sólo mueven el DUTY — `toneSweep` los
  // descarta explícitamente (`speaker.ts:112-142`, refutado por espectro de dos testigos).
  // Lo que sube y baja es el TIMBRE, no el tono; se emite como un solo cue de dos tramos.
  // La nota canónica lo llamaba «animación de elevar/bajar el shard»: es AUDIO (#201).
  | "shard-sweep" // barrido del ritual del shard — TS×2 tramos (sin `n`: ambos iguales)
  // FANFARRIA DE VICTORIA — `sfx_victory_fanfare` (CAST.OVL 0x83e8 → ULTIMA.EXE 0x4368),
  // 70 B, `ret` pelado, cero args, no toca globales ni RNG:
  // `toneSweep(4600,1,10800,300,6)×3 + toneSweep(6100,1,21600,300,3)` (cola-cast-acta §6).
  // Sus DOS únicos llamadores son COMBAT tras «VICTORY!» y la COLA de este ritual — que el
  // original comparta la fanfarria del combate con la caída de un Shadowlord es del binario.
  // 🔴 ESTE SONIDO YA ESTABA EN EL CATÁLOGO, CON EL NOMBRE DE OTRO: `cast-spell` lo emite
  // con estos MISMOS cuatro `toneSweep` (`speaker.ts:395-400`), y `sfx-catalog.md` §3.5 ya
  // declara esa atribución FALSA y SIN ARREGLAR por decisión del lead. Este id no duplica la
  // onda: `speaker.ts` la define una vez y los dos ids la comparten, para que no puedan
  // divergir mientras convivan. Aquí se añade el nombre VERDADERO; corregir qué emite
  // `cast-spell` sigue siendo la decisión pendiente de §3.5.
  // 🔴 «el único camino que sí es suyo» decía esta línea, y desde #212 son LOS DOS del
  // binario: el ritual (`use-tools.ts`, CAST 0x1759) y la VICTORIA del arena
  // (`sfxForCombatEvent`, COMBAT 0x0d02). El segundo faltaba y por eso ganar era MUDO.
  // 🔴 Y la forma de la onda NO es «tres notas y una final más grave», como decían esta
  // nota y `sfx-catalog.md` §3.5/§6: el pitch sale de `inc` (4600 ×3 → 6100), así que la
  // cuarta nota es más AGUDA y del doble de larga. Lo que se parte por la mitad es `step`
  // (6→3), que bajo el modelo de esta casa mueve el DUTY y no el tono (`speaker.ts:410`).
  // Son DOS pitches: exactamente los «DOS TONOS» del reporte del usuario.
  | "victory-fanfare" // fanfarria de victoria — TS×4 (0x4368): 3× ~1811 Hz + 1× ~2402 Hz
  | "moongate" // activar tile especial / moongate — TS (0x170c,1,30000,2000,2)
  // TERREMOTO (task #29): rumble grave que acompaña a la sacudida de pantalla del
  // clavicémbalo (muro→pasadizo, TOWN 0x0e9e-0x0ea6 `xor [0x67b9],0xb` + `call
  // 0xffffaea2`, rutina kernel COMPARTIDA @0xaea2, también MAINOUT 0x0a7d). Params
  // acústicos EXACTOS = kernel sin resolver (base-overlay pendiente) → la piel los
  // CALIBRA al testigo (pico ≈106 Hz, energía 0-500 Hz, pulsado ~8×). Presentación
  // pura: NUNCA por música de fondo. re/notes/quake-harpsichord.md.
  | "quake" // terremoto (sacudida de viewport) — rumble grave pulsado (⚠ Clase C)
  // CATARATA (#322): el barrido descendente del PC-speaker de la caída.
  // `outsubs_waterfall_fall` OUTSUBS 0x0458 empuja en 0x0482-0x0491 los cuatro
  // argumentos y llama en 0x0492 a kernel 0x43AE `pcspeaker_glide`:
  //   0x0482 push 0x9c4 (2500 = start) · 0x0486 push 0x320 (800 = end)
  //   0x048a push 1 (step)             · 0x048e push 0x12c (300 = total)
  // = glide(2500, 800, 1, 300), convención de ORDEN DE PUSH (audio-diff-
  // calibration.md:16/:22). NO es Clase C: los cuatro literales son inmediatos
  // leídos del cuerpo, y la primitiva `glide` ya está derivada y en uso por
  // cannon-fire/combat-escape/ring-vanishes/torch-borrowed. Sustituye al préstamo
  // del «quake» que el port arrastraba (declarado como aproximación en game.ts).
  | "waterfall-fall" // caída por catarata — GL (2500→800, 1, 300) @OUTSUBS 0x0492
  | "shadowlord-announce" // drone del heraldo de Shadowlord — TS (0x19c8,1,60000,2000,1)
  | "instrument-note" // tocar instrumento; `n` = dígito (0..9) → tabla de notas [0x2746]
  // JINGLE de transacción de tienda — RE-DERIVADO (carril audio-costuras): los 6
  // tone_sweep de SHOPPES (0x13d8/0x13ef/0x1417/0x142b/0x144f/0x1466) viven TODOS
  // en UNA rutina LINEAL 0x13b0→ret 0x1469 (3 pares subida/bajada, params completos
  // — el hedge «→AV» de §4.4 queda resuelto leyendo los push con reuso de ax). Sus
  // ÚNICOS callers son las ramas C/H/R del CURANDERO (0x1611/0x1684/0x16eb), tras
  // ejecutarse el servicio (pago OK vía 0x146a ret 0, gratis en location 5
  // «Receive now the Light!», o caridad Skara Brae). SHOPPES2/3 (taberna/astillero/
  // posada) tienen CERO llamadas de speaker y el resto de flujos de SHOPPES
  // (herrero Sold!/reactivos/gremio/establo) son MUDOS: el único sonido de
  // transacción del binario es este jingle del curandero. `n` queda sin uso.
  | "shop-transaction" // jingle del servicio del curandero (SHOPPES 0x13b0) — TS×6
  // ── Comando (F)ire — cañonazo de fragata ───────────────────────────────────
  // Broadside de la fragata (comando F). Owner CONFIRMADO por asm: dispatch 'F' =
  // CMDS 0x0aea → 0x0962 (broadside overworld) → glide(1000,200,5,300) en 0x9d5,
  // gateado tras el chequeo de perpendicularidad (str "Fire broadsides only!"
  // 0x42cd en la rama de fallo). Misma tupla en la rama de arena (BOOOM! 0x42f2 +
  // glide 0x0c05). El whoosh descendente del proyectil. sfx-catalog.md §4.9/§10.3.
  | "cannon-fire" // disparar broadside (F) — GL (1000→200, 5, 300)
  // ── Escape de combate + anillo consumido (glides confirmados por #48/#52) ──
  // Escape de combate: al huir del arena (SJOG 0x1c37 "Escape!" 0x8eae / CMDS 0x17ec
  // handler) suena GL(1200→2000,1,40). El port imprime "Escape!" byte-exacto en
  // playerEscape (huida del PJ, distinta del "X escapes!" del enemigo).
  | "combat-escape" // huida del PJ del arena de combate — GL (1200→2000, 1, 40)
  // ── Rechazo de comando en la ARENA (funnel SJOG 0x1f26) — ficha #161 ───────
  // COMBAT.OVL rechaza las teclas que su bucle no acepta por el funnel SJOG 0x1f26:
  // imprime el rótulo del comando + la cadena del código (1 «what?» DS 0x8f12 ·
  // 2 «-Not here» DS 0x8f1a · 3 «-Funny, no response!» DS 0x8f24) + '\n' (0x1f4b
  // push 0xa → 0x573a = kernel putchar 0x16ba — NO es una pausa) y REMATA SIEMPRE
  // (los 3 códigos y el default confluyen en 0x1f4b) con DOS beeps CONTIGUOS:
  //   0x1f52 push 0xdc; push 0x96; call 0x6340   ·   0x1f5d push 0x96 ×2; call 0x6340
  // Resolución cross-overlay con dispatch_table (base near-call SJOG = 0xBF80;
  // control positivo acreditado: SJOG 0x766c→0x35ec prompt_direction, citas-sesgo-
  // overlay.md): 0x6340 → kernel 0x22c0 `beep`. Convención de args adjudicada por
  // call-site acreditado (kernel 0x4299 push 0xbb8,3 = beep(3000,3) = el TIC del
  // reloj, speaker-audit.md): PRIMER push = freq ⇒ beep(0xdc=220 Hz, dur 0x96) +
  // beep(0x96=150 Hz, dur 0x96) — un «bip-bop» DESCENDENTE, no dos pitidos iguales
  // (el comentario que #161 heredaba en main.ts traía la pareja invertida:
  // «beep(0x96,0xdc)… dos pitidos de 150 Hz»). Sin silencio entre ambos: cada beep
  // = set_tone 0x22e2 (div 0x1234DE/freq ⇒ arg en Hz) + delay calibrado
  // 0x20c8(dur,1) + off 0x230e, y el segundo arranca al retornar el primero.
  // Emisores portados hoy: el rechazo de (Q)uit en la arena (tecla Q y F5,
  // main.ts). El resto del funnel (B/X/E/F/H/I/L/M/N/V/T) sigue sin portar —
  // censo completo en ui/touch.ts (bloque COMBAT_BUTTONS).
  | "combat-reject" // rechazo de comando en combate — beep(0xdc,0x96) + beep(0x96,0x96) @SJOG 0x1f52/0x1f5d
  // ABSORCIÓN del desenlace (cabo de #179, SJOG `absorb` 0x1ea4): tras el print de
  // «\n<nombre> is absorbed!\n» el binario emite UN SOLO tono — SJOG 0x1ef8-0x1f08
  // `push 0x4b0,0x7d0,1,0x28; call 0x842e` → ULTIMA.EXE 0x43AE `pcspeaker_glide`
  // (base near-call SJOG 0xBF80, dispatch_table; controles: 0x766c→0x35EC prompt_
  // direction · 0x6340→0x22C0 beep #161 · stub 0x7e66→SJOG 0x1EA4 el propio absorb)
  // = GL(1200→2000, paso 1, total 40), byte-idéntico a combat-escape/ring-vanishes.
  // 🔴 El «tono corto (0x573a, arg 0xa)» que el acta #179 §3 listaba como PRIMER
  // sonido queda REFUTADO: 0x573a→0x16BA es PUTCHAR (mismo call+arg que el funnel
  // 0x1f4b, adjudicado en #161 — «NO es una pausa») y el 0xa es el '\n' inicial del
  // mensaje. El absorb tiene UN tono, no dos. Careo: absorcion-179-acta.md §10.
  | "combat-absorbed" // miembro absorbido por el alma (SJOG 0x1f08) — GL (1200→2000, 1, 40)
  // "Ring vanishes!" (ZSTATS 0xe42, str 0x995e): el anillo se consume (1/16) al
  // readyear un anillo de ids 0x2a/0x2c. GL(1200→2000,1,40). El rand ya está en el
  // stream vivo (readyItem); el cue es puramente presentacional.
  | "ring-vanishes" // anillo consumido al readyear (R) — GL (1200→2000, 1, 40)
  // ── AMBIENTE por PROXIMIDAD (tick 0x4102 = `ambient_sfx_tick`) ──────────────
  // El original escanea el viewport visible cada repintado idle y emite el SFX del
  // tile animado MÁS CERCANO al party (una casilla → un sonido). Deriva completa +
  // IDs de tile confirmados (Redux + tile-anim-census, AV): re/notes/ambient-audio-
  // audit.md. Reemplazan los placeholders vagos `field-pulse`/`field-crackle` (nunca
  // cableados), que colapsaban 4 fuentes distintas en dos "fields" imaginarios.
  | "ambient-fountain" // Fountain 0xd8–0xdb, clase 3 — NB (10,30,25000) burbujeo
  | "ambient-waterfall" // Waterfall 0xd4–0xd7, clase 2 — NB (20,60,10000) cascada
  | "ambient-clock-tick" // Clock 0xfa/0xfb, clase 1, fase 0 — beep(3000,3) TIC
  | "ambient-clock-tock" // Clock 0xfa/0xfb, clase 1, fase 4 — beep(2000,3) TAC
  | "ambient-clock-chime" // Clock dando la hora ([0x5884]≠0) — TS(3116,1,2000,20000,-10)
  // ── Movimiento ─────────────────────────────────────────────────────────────
  // PASO EXITOSO a pie: SÍ suena en el original (kernel sfx_footstep 0x433e =
  // noise_burst(1,25,1000) + delay(0x14) + noise_burst(1,25,1500)). Confirmado por
  // TESTIGO DE RUNTIME (dosbox-x, 12/12 pasos disparan 0x223c; pasar-turno e idle
  // MUDOS) — REFUTA la conclusión previa "andar es mudo". re/notes/walk-sound-
  // verdict.md (task #51). El catálogo previo (§3.6) lo tenía como "ambiente
  // cercano", sin ligarlo al paso del jugador.
  | "move-step" // paso efectivo a pie — noise_burst ×2 (band 1000 y 1500)
  // Bump de pared a pie ("Blocked!"): beep(freq=0xa5,dur=0xc8). El bump por cactus
  // (0x2f) NO suena en el original, y ahora se sabe POR QUÉ: son ramas EXCLUYENTES
  // del mismo `if` (MAINOUT 0x0329 `cmp [bp-6],0x2f`) — el cactus se va por 0x032f
  // («OUCH!» + daño) y el beep vive en el `else` de 0x033c. El port YA modela el
  // OUCH a pie (#157), así que este cue queda acotado al choque NO-cactus.
  // Idéntico en overworld (MAINOUT 0x0344) y pueblo (TOWN 0x0849). sfx-catalog.md §10.
  | "move-blocked" // choque contra tile no transitable — beep (0xa5,0xc8)
  // ── Trampas / búsqueda ─────────────────────────────────────────────────────
  // ⚠ RE-ATRIBUIDO (#54 pieza 6, por lectura del cuerpo 0x01f2-0x02e6): este cue NO es
  // ningún «spring de trampa», y el id `search-fail` es un nombre heredado del error.
  // SJOG 0x1f2 es `search_remains_outcome` (así lo tiene el ledger frontier.json, que
  // estaba en lo cierto): el resultado de rebuscar en unos restos. Su NB(500,3000,40)
  // @0x0237 acompaña a **«Plague!»** (DS 0x8606) y a poner al buscador en estado 'P'
  // (0x0241 `mov byte [bx+0x55b3],0x50`, bx = charIdx<<5) — no hay trampa ni veneno.
  // Las trampas del (S)earch viven en la rutina HERMANA `search_trap_check` 0x2ea, con
  // sus propias cadenas (DS 0x864a «no trap!» · 0x8654 «a simple trap!» · 0x8664
  // «a complex trap!» · 0x8676 «a trap!»), y esa es MUDA. Sigue SIN EMISOR: el flujo de
  // restos no está portado (ver el bloque 6b declarado en el commit de #54).
  | "search-fail" // Plague! del rebusque de restos (SJOG search_remains_outcome 0x1f2 @0x237) — NB (500,3000,40); SIN emisor hoy
  // Trampa de cofre al SALTAR — RE-DERIVADO (carril audio-costuras): el despachador
  // kernel `0x2fd0` (= SJOG 0x7050 `chestTrap`: ACID/POISON/BOMB/GAS) ABRE con
  // NB(40,3000,500) @0x2fe3 INCONDICIONAL, antes del rand de tipo. ⚠ La frase que
  // seguía aquí («el mismo bang suena en el spring del search, SJOG
  // spawn_trap_effect 0x1f2: “A trap!” + NB @0x237 + envenena al buscador») queda
  // RETIRADA por lectura de cuerpo (#54 pieza 6): 0x1f2 es `search_remains_outcome`,
  // su NB acompaña a «Plague!» y no hay trampa en esa rutina. Además «A trap!» no
  // existe VERBATIM en DATA.OVL: el binario tiene «a trap!» (DS 0x8676, minúscula,
  // para componer tras «Thou dost find») y «A trap» (DS 0x875c, sin cierre).
  // La DETECCIÓN de trampa (trapCheck SJOG
  // 0x2ea «a simple/complex trap!») es MUDA en el binario — la emisión previa del
  // port en la detección queda retirada. Tras el bang, el DAÑO blipea por miembro
  // vía `combat-damage` (0x2a52; ACID=1, BOMB=por vivo; POISON/GAS silenciosos).
  | "dungeon-trap" // trampa de cofre DISPARADA (kernel 0x2fd0 @0x2fe3) — NB (40,3000,500)
  // Campo de energía de mazmorra (DUNGEON 0x0470): «Ouch!» + «Electric field!»
  // (DS 0x2ca8/0x2caf) + doble invert-flash (0x89b6/0x9f2a) + NB(1,500,20000)
  // @0x4b9, y después el daño a toda la party (far 0x2aa8 @0x04f7 → un
  // `combat-damage` por miembro vivo). El rebote no atraviesa.
  | "dungeon-zap" // choque con campo eléctrico (DUNGEON 0x4b9) — NB (1,500,20000)
  // Campos de SUEÑO/VENENO de mazmorra (DUNGEON 0x0948/0x09e6): en el contest de
  // DEX, POR CADA miembro que CAE (roll≥DEX, no muerto) el binario pone el status
  // ('S' @0x98b / 'P' @0xa21), parpadea su marcador (far 0x2a28 @0x98f) y suena
  // NB(1,50,3500) (@0x99e sueño / @0xa30 veneno). Un blip por afligido; los que
  // resisten son mudos. (La atribución previa del catálogo «gotas/eco» era un
  // placeholder — corregida por esta lectura.)
  | "field-afflict" // miembro dormido/envenenado por campo (DUNGEON 0x99e/0xa30) — NB (1,50,3500)
  // Fallo del cambio de planta mágico Uus/Des Por (DUNGEON 0x1c6a con mode=1):
  // la rama 0x1ce4 imprime «Failed!» (DS 0x6c7a) + glide(800→2000,1,50) @0x1cfb.
  // El Klimb (mode=0) nunca cae aquí (su cola de fallo 0xee5 es silenciosa).
  | "dungeon-fail" // «Failed!» de Uus/Des Por (DUNGEON 0x1ce4) — GL (800→2000,1,50)
  // Coger la ANTORCHA DE PARED con (G)et (SJOG cmd_get 0x18ce, rama 0xB0/0xB1):
  // "Borrowed!" (DS 0x8de8) + glide 0x842e(0x32,1,0x7d0,0x320) @0x1a21 =
  // GL(800→2000,1,50), el mismo "alarma/negativo" del catálogo (§6). Cableado al
  // cerrarse el hallazgo #52 (el "Borrowed!" del binario es la antorcha, no los
  // platos): el cue se emite en la rama sconce de game.get(), no en stealFood.
  | "torch-borrowed" // "Borrowed!" de antorcha de pared (SJOG 0x1a21) — GL (800→2000,1,50)
  // ROMPER EL ESPEJO con (A)ttack (#217, TOWN.OVL `town_attack_cmd` 0x0a69-0x0a80):
  // bucle `si = 0x7d0; noise_burst(0x28,0x78,si); si += 0x3e8; while si < 0x4e20` =
  // DIECIOCHO ráfagas con la BANDA creciendo 2000→19000 de mil en mil.
  // 🔴 NO es una rampa de 18 tonos, y confundirlo es la trampa de #137 en su forma
  // más pura: `si` es el TECHO DE BANDA del `noise_burst`, no una frecuencia. Cada
  // ráfaga sortea `dur/step` = 0x78/0x28 = 3 tonos en [0x64, si] con el PRNG LOCAL
  // (0x545c), así que son 54 tonos aleatorios cuya banda se ensancha: cristal
  // rompiéndose, no una sirena. Se CALCA con la primitiva fiel (no hace falta
  // Clase-C: `noiseBurst` ya modela el sorteo, el cierre por arriba y el PRNG).
  | "mirror-break" // romper el espejo (TOWN 0x0a69) — NB (40,120,band) ×18, band 2000→19000
  // ── La APARICIÓN del campamento (partitura §4.7a, OUTSUBS camp_results) ─────
  | "apparition-materialize" // "An apparition!" — TS (0xa3c,1,10000,2500,6)
  | "apparition-arpeggio" // arpegio de materialización (3 notas, tabla [0x3a26]) — TS×3
  | "apparition-heal-chime" // campanilla de cura por miembro — TS (0x157c,1,5000,200,13)
  | "apparition-chord" // acorde largo antes del discurso — TS (0x157c,1,60000,2500,1)
  // ── El TRUENO de la resurrección de Lord British (party-wipe / refuge) ─────
  // "There is a peal of thunder!" (DS 0x719e): BLCKTHRN.OVL `party_refuge` 0x0ac5 →
  // 0x0acc + 0x0acf `call 0x8de2` DOS VECES = dos truenos. 0x8de2 está por encima del
  // techo del disasm kernel (0x86ee) → los params acústicos exactos son Clase C,
  // calibrados al testigo (mismo convenio que `quake`/`intro-thunder`). El cue se emite
  // dos veces (una por peal). Presentación pura; NO consume g_rng.
  | "refuge-thunder" // peal de trueno de la escena de refuge — ⚠ Clase C (2× por escena)
  // Cinemática de la intro (demo "The Summoning/Arrival"), PARAMS byte-citados del scene
  // engine de FONT.OVL (scene_tick 0x02fc; cita de demo-stage3). Todo por SPEAKER.
  | "intro-thunder" // moongate rise/fall (modo 2): FONT 0x03c6 call 0x405c(0x14,0x3c,0x2710) = NB(20,60,10000)
  | "intro-chime" // moongate modo 3 (frame 0/4): FONT 0x03ef mov ax,0xbb8; push 3; call 0x40e0(tono) = beep(3000,3)
  | "intro-summon" // opcode SUMMON: FONT 0x088d call 0x405c(1,0x4b0,0xfa0) = NB(1,1200,4000)
  // ── PANTALLA DE TÍTULO (#220) — los emite el DRIVER DE VÍDEO, no el kernel ──────
  // El censo de #211 («el emisor vive en el kernel») quedó en CERO para INTRO/FLAMES/FONT
  // porque barría ULTIMA.EXE + los 24 overlays y NO los cuatro `*.DRV`. El emisor es
  // `EGA.DRV:0x27af`, un `noise_burst` propio del driver (mismo esqueleto y mismo PRNG que
  // el del kernel, estado aparte ⇒ no toca `g_rng`), al que se entra por `lcall [0x5350]`
  // con selector. Derivación completa: `re/notes/intro-sonidos-220.md`.
  // ⚠ Su banda se DIVIDE por 2 antes de mapear (`0x27d9 shr cx,1`), al revés que el kernel:
  // el parámetro acústico del port lleva ese `>>1` dentro (speaker.ts), no aquí.
  | "title-fizzle" // dissolve de pantalla (sel 0x66 CF=0 @0x269f): 1 ráfaga/2 px con banda ++ desde 0xf0
  | "title-crackle" // crepitar del subtítulo (sel 0x69 CF=1 @0x29c5): NB(1,0x19,0xbb8) por tick sorteado
  // ── La CANCIÓN de Iolo bardo (FASE 1 del camp) ─────────────────────────────────
  // Suena por SPEAKER (sweep 0x2192), NO por driver de música (el usuario no oye música en su
  // DOSBox y aun así suena). Melodía EXACTA del binario (DATA.OVL 0x6a58 = 53 índices + 0x6a44
  // = tabla de freq; motor de sonido kernel 0x42d2 modo 4), derivada por el oráculo DOSBox +
  // estático. Ver speaker.ts (BARD_MELODY/BARD_FREQ_TABLE) y camp-scene-kernel.md §6.
  | "bard-song"
  // ── ENDGAME (#20/#34) — los dos ÚNICOS sonidos del cierre ───────────────────────
  // El antiguo cue `endgame-beep` fue RETIRADO (adenda fanfarria-re 2026-07-22,
  // re/notes/fanfarria-endgame-espectral.md §3/§7): `call 0xffff9856` → ULTIMA.EXE
  // 0x3ae6 NO es un beep — es RUN-N-FRAMES (bucle dur × [tick de frame 0x5910 +
  // delay 0x20fa(1)]), y en la sala del endgame el ambiente del tick es MUDO ⇒
  // todos los «beeps» del censo GAP 8 son PAUSAS MUDAS (n unidades × ~55 ms/tick):
  // llegada verde n=0x28 (0x06f2), pumps de página n=0x28 (0x0830/0x0922), pings
  // de entrada tick(1)/tick(4) del moongate = pacing mudo. Los «15 tonos asc/desc»
  // del moongate eran 15 FRAMES de animación del reveal ([0x5887] 1..15), sin
  // sonido. El pacer modela esas pausas con RELOJ (delayUnits / cadencia de
  // frames), no con un cue. El cruce del puente con trolls (MAINOUT 0xffffb916 =
  // el mismo 0x3ae6) pacea igual: pausa muda por beat (`pauseUnits`).
  // Barrido del LANZAMIENTO del Orb (GAP 4 paso 2): ENDGAME 0x0973 `push 0x1450,1,
  // 0xc350(50000),0x2710(10000),1; call 0x7f02` — params EXACTOS del asm (freq sweep;
  // uno de los 2 únicos sweeps audibles de la fase final, con el «lives!» 0x078f).
  | "endgame-orb";

/**
 * Cue de sonido: ID de acción + parámetro escalar opcional (`n`). Es un VALOR
 * inmutable que viaja en el array de eventos de la acción. La piel lo consume.
 */
export interface SfxCue {
  id: SfxId;
  /** Variante numérica de la acción (dígito de instrumento; signo de transacción). */
  n?: number;
}

/** Forma mínima de un evento de sonido tal como viaja en un `GameEvent`. */
export interface SfxGameEvent {
  kind: "sfx";
  sfx: SfxCue;
}

/**
 * Constructor de un evento de sonido para empujar en el array de una acción del
 * core: `events.push(sfxEvent("moongate"))`. No consume RNG ni toca estado.
 */
export function sfxEvent(id: SfxId, n?: number): SfxGameEvent {
  return { kind: "sfx", sfx: n === undefined ? { id } : { id, n } };
}

/** Forma mínima de un evento de combate del que derivar un cue (sin acoplar tipos). */
export interface CombatSfxInput {
  kind: string;
  hit?: boolean;
  /** true si el objetivo es un PJ de la party (recibe daño el jugador). */
  targetIsPlayer?: boolean;
  /** texto del evento `message` (huida "Escape!" / cofre "Trapped!"). */
  text?: string;
}

/**
 * Deriva el cue de sonido de un evento de combate ya emitido, sin duplicar la
 * lógica ni tocar `combat.ts` (que porta el RNG de combate y no debe cambiar).
 * El punto lógico del golpe ES el evento `attacked{hit}` / `died` del binario
 * (el sonido del impacto es el flash `kernel 0x3564`, POR BANDO del objetivo —
 * 0x35ac `test [bx+2],0x80` + blink 0x2a28; combat-ui-spec §3):
 *   - `attacked` con `hit` sobre un PJ → NB(40,3000,500) @0x35c9 (heavy).
 *   - `attacked` con `hit` sobre enemigo → NB(10,3000,2000) @0x35de.
 *   - `died` → desvanecer/derrota (0x2fd0 @0x2fe3... misma banda 40/3000/500).
 * Un fallo/miss no suena (el melee fallido de la IA es silencioso, 0x035b).
 * (La rama previa `lethal → heavy` queda RETIRADA: el heavy del binario no
 * distingue letalidad sino BANDO — re-derivación del carril audio-costuras.)
 */
export function sfxForCombatEvent(e: CombatSfxInput): SfxCue | null {
  if (e.kind === "died") return { id: "combat-defeat" };
  if (e.kind === "attacked" && e.hit) {
    if (e.targetIsPlayer) return { id: "combat-hit-heavy" };
    return { id: "combat-hit" };
  }
  // Huida del PJ del arena (SJOG 0x1c37 "Escape!" + glide 1200→2000). El mensaje
  // "Escape!" es byte-exacto y ÚNICO del jugador (el enemigo huye con "X escapes!").
  if (e.kind === "message" && e.text === "Escape!") return { id: "combat-escape" };
  // Cofre ATRAPADO abierto en el arena (SJOG open 0x112C → kernel 0x2fd0): el
  // despachador de trampa abre con NB(40,3000,500) @0x2fe3. El port emite "Trapped!"
  // (str 0x8b7e) como línea PROPIA (el tipo va en un message aparte, formato fiel
  // de dos líneas); la clave FIJA en EN sigue disparando el cue.
  if (e.kind === "message" && e.text?.startsWith("Trapped!")) return { id: "dungeon-trap" };
  // VICTORIA del arena (#212, reporte del usuario 13-08: «suena con DOS TONOS»).
  // COMBAT.OVL `combat_main_loop`, leído entero (base 0xA290):
  //   0x0cf6  mov ax,0x6f00 ; push ; call 0x75c0   → imprime DS 0x6f00 "\nVICTORY!\n"
  //   0x0cfd  mov byte [g_cmb_victory_flag],1
  //   0x0d02  call 0xffffa0d8  → (0x0d05+0x93d3−0x10000+0xA290)&0xFFFF = ULTIMA.EXE:0x4368
  //                              = `sfx_victory_fanfare`  ⇐ EL CUE QUE FALTABA
  //   0x0d05  call 0x7886      → (0x0d08+0x6b7e+0xA290)&0xFFFF = ULTIMA.EXE:0x1b16
  //                              = vaciado del búfer de teclado (ver abajo)
  // 🔴 NO ERA UN CUE NUEVO: es CABLEADO QUE FALTABA. `victory-fanfare` ya existía en el
  // catálogo desde #201 (la cola del ritual del shard, CAST.OVL:0x1759 → el MISMO 0x4368),
  // y `sfx-catalog.md` §3.5 ya nombraba a COMBAT.OVL:0x0d02 como su otro llamador — pero
  // el único emisor del port era `use-tools.ts:118`. Al ganar una batalla no sonaba nada.
  // La rama de DERROTA (0x0cda, DS 0x6eee "\nBATTLE IS LOST!") NO llama a 0x4368: por eso
  // el predicado es el texto de la victoria y no «se acabó el combate».
  // Presentación PURA, cero RNG: 0x4368 sólo encadena cuatro `tone_sweep` (0x2192), cuya
  // familia usa el PRNG LOCAL del ruido —y el sweep ni eso: su cuerpo (0x2192-0x223b) no
  // tiene una sola instrucción `call`. `0x1b16` sólo escribe 0x40:0x1A/0x1C. NO mueve stream.
  if (e.kind === "message" && e.text === "VICTORY!") return { id: "victory-fanfare" };
  // ABSORCIÓN del desenlace (cabo de #179): el glide suena justo DETRÁS del print
  // (SJOG 0x1ef5 «is absorbed!» → 0x1f08 glide, contiguos en el mismo beat, sin
  // ventana visual entre medias) ⇒ derivarlo del message es la posición fiel y no
  // hace falta fase (#208). El texto del port es tf("{} is absorbed!", nombre)
  // (combat.ts maybeAbsorb, DS 0x8f02 = « is absorbed!\n») — clave FIJA en EN,
  // como «Escape!»/«Trapped!». El sufijo con espacio no casa con ningún otro
  // mensaje del corpus (era huérfana hasta #179; único emisor: maybeAbsorb).
  if (e.kind === "message" && e.text?.endsWith(" is absorbed!")) return { id: "combat-absorbed" };
  // Antorcha de pared descolgada EN LA ARENA (#375): el fallback por tile de cmd_get
  // (SJOG 0x19e8-0x1a27) también corre en combate, y el glide GL(800→2000,1,50)
  // (0x842e @0x1a21) es INCONDICIONAL — el único gate de location de la rama es el
  // repintado 0x9eca (guard 0x19fb), no el sonido. Mismo cue que game.get() emite en
  // overworld (sfxEvent directo); aquí se deriva del texto, clave FIJA en EN como
  // "Escape!"/"Trapped!". "Borrowed!" es ÚNICO de la antorcha desde el cierre del
  // Clase-C #69 (los platos dicen "Mmmmm...!").
  if (e.kind === "message" && e.text === "Borrowed!") return { id: "torch-borrowed" };
  return null;
}

// ── AMBIENTE por PROXIMIDAD (`ambient_sfx_tick` 0x4102) ───────────────────────
//
// Derivación completa en re/notes/ambient-audio-audit.md. El original, en el bucle
// de espera de tecla (idle), repinta el viewport y llama a 0x4102, que:
//   1. Barre la ventana visible y elige por DISTANCIA EUCLÍDEA el tile animado MÁS
//      CERCANO al centro (party; o 5,5 en combate) de 3 clases audibles.
//   2. Emite el SFX de esa clase, con un contador de fase [0x6a34] (0..7) que
//      selecciona el tic/tac del reloj (0x4327 `inc`, 0x432b `cmp ,7`). Ese MISMO
//      byte es el slot 0 de la tabla de freqs contigua, que vale 0x0000 y NUNCA se
//      indexa porque la nota 0 salta el sweep (0x42df `cmp al,bh` + `je`): las dos
//      lecturas conviven, no se pisan. `noise_burst` usa PRNG LOCAL → cero g_rng.
// La clase 4 es el BARDO TOCANDO, no la estantería/Codex. Su byte sale de la capa de
// SPRITES (0x4200, puntero [bp-0x1c], stride 0x10 → [0xac64]), no de la de terreno
// ([bp-0x1a], stride 0x20 → [0xab02], que el gate exige ==0 = «esta celda la reclamó
// un actor»). Al venir del banco ALTO, el `and al,0xfc; cmp al,0x5c` de 0x4207 casa
// los tiles 0x15C–0x15F = BardPlaying1..4. En el banco de TERRENO 0x5c–0x5f son
// BookcaseLeft/BookcaseRight/CodexAngel*: tiles reales, pero NO los que este gate ve.
// Sigue SIN PORTARSE, y la razón no cambia: necesita el buffer overlay [0xac64] y las
// tablas de datos →AV (freqs DS:0x6a36–0x6a47, melodía DS:0x6a48, 53 notas).
// Adjudicación completa: re/notes/sfx-bardo-adjudicacion.md.

/**
 * Clase de ambiente por proximidad de un tile de mapa CRUDO (0x4102 §1a). 0 = ninguna.
 *   1 = Clock (0xfa,0xfb)     — tic/tac (y campanada al dar la hora)
 *   2 = Waterfall (0xd4–0xd7) — rumor de cascada
 *   3 = Fountain (0xd8–0xdb)  — burbujeo
 * Tests idénticos al asm: `(t&0xfe)==0xfa` (0x41d0) · `(t&0xfc)==0xd4` (0x41df) ·
 * `(t&0xfc)==0xd8` (0x41ed). Sólo tiles 0..255 (mapa crudo); entidades (tile+256) e
 * "off-map" (<0) no cuentan.
 */
export function ambientTileClass(tile: number): 0 | 1 | 2 | 3 {
  if (tile < 0 || tile > 0xff) return 0;
  if ((tile & 0xfe) === 0xfa) return 1; // Clock 0xfa,0xfb
  if ((tile & 0xfc) === 0xd4) return 2; // Waterfall 0xd4–0xd7
  if ((tile & 0xfc) === 0xd8) return 3; // Fountain 0xd8–0xdb
  return 0;
}

/**
 * HORA en formato 12h del contador de campanadas `[0x5884]` (kernel 0x5164-0x5183,
 * dentro de `advance_clock 0x4f7c`, alcanzado SÓLO cuando la llamada movió la hora —
 * 0x514a `je 0x5186`, A3-HF2): `g_hour==0 → 12`; `>12 → hora−12`; resto → hora. El
 * consumidor (CoreView `observeClock`) re-arma su contador con esto al cambiar la hora
 * y lo decrementa en fase 0/4 (epílogo global de 0x4102, `0x430e-0x4323` `dec [0x5884]`).
 */
export function chimeHour12(hour: number): number {
  const h = hour & 0xff;
  if (h === 0) return 12;
  if (h > 12) return h - 12;
  return h;
}

/**
 * Cue del ambiente para una VENTANA de tiles crudos (0x4102). `tiles` es la rejilla
 * `wide×wide` centrada en el party (party en `wide>>1, wide>>1`). `phase` = contador
 * [0x6a34] (0..7) del ambiente, que avanza una vez por tick. `chimeCounter` = el
 * contador de campanadas `[0x5884]` (≠0 ⇒ el reloj DA LA HORA en vez de tic/tac;
 * ver §5.1 de ambient-audio-audit.md). Devuelve el cue del tile animado MÁS
 * CERCANO (o `null` si no hay ninguno, o si es el reloj fuera de sus fases 0/4).
 * PURA: la piel la llama cada tick de anim y enruta el cue a `emitSfx`.
 */
export function ambientCueForTiles(
  tiles: ArrayLike<number>,
  wide: number,
  phase: number,
  chimeCounter = 0,
): SfxCue | null {
  const cx = wide >> 1;
  const cy = wide >> 1;
  // [bp-2] arranca en 0x33 (51): el rincón más lejano del 11×11 dista 50 < 51, así
  // que toda la ventana entra y gana el más cercano (0x41b8 `jge` = sólo si < best).
  let best = 0x33;
  let cls: 0 | 1 | 2 | 3 = 0;
  // Orden de barrido IDÉNTICO al asm (0x4102): x EXTERNO (cx-5..cx+5), y INTERNO
  // (0x4233 inc del x-outer; 0x421c inc del y-inner). En un EMPATE de distancia gana
  // el PRIMERO en ese orden (`>=` descarta los iguales que vienen después): menor x
  // y, a igual x, menor y. Observable con dos animados equidistantes de columnas
  // distintas → el asm elige el de la columna más a la izquierda.
  for (let col = 0; col < wide; col++) {
    for (let row = 0; row < wide; row++) {
      const dx = col - cx;
      const dy = row - cy;
      const d2 = dx * dx + dy * dy;
      if (d2 >= best) continue; // el asm computa la distancia y descarta si no es menor
      const k = ambientTileClass(tiles[row * wide + col] ?? -1);
      if (k === 0) continue;
      best = d2;
      cls = k;
    }
  }
  return ambientCueForClass(cls, phase, chimeCounter);
}

/** Cue de una clase de ambiente con la fase dada (switch de 0x4247 + gate de fase). */
function ambientCueForClass(cls: 0 | 1 | 2 | 3, phase: number, chimeCounter: number): SfxCue | null {
  switch (cls) {
    case 3:
      return { id: "ambient-fountain" }; // 0x42c4 NB(10,30,25000), sin gate de fase
    case 2:
      return { id: "ambient-waterfall" }; // 0x42be NB(20,60,10000), sin gate de fase
    case 1: {
      // Reloj (fases 0/4; resto MUDO): con `[0x5884]≠0` DA LA HORA — campanada
      // TS(3116,1,2000,20000,-10) @0x428b — y con 0 tictaquea (tic 0x42a1 fase 0 /
      // tac 0x429c fase 4). El re-armado ([0x5884]=hora12 en advance_clock 0x5164,
      // sólo al cambiar la hora: 0x514a) y el decremento global en fase 0/4
      // (0x430e-0x4323) los lleva el CONSUMIDOR (CoreView). Derivación §5.1
      // ambient-audio-audit.md con su corrección A3-HF2: da la hora al cruzarla, no
      // tras cada turno (validado en hardware, H-198).
      const p = phase & 7;
      if (p !== 0 && p !== 4) return null;
      if (chimeCounter > 0) return { id: "ambient-clock-chime" }; // 0x4262 [0x5884]≠0 + 0x4269 fase 0/4 → 0x428b
      return { id: p === 0 ? "ambient-clock-tick" : "ambient-clock-tock" };
    }
    default:
      return null;
  }
}
