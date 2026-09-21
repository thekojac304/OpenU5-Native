> English migration of `docs/bugs-del-original.md` (the Spanish note in the private repo remains the source of truth; translated 2026-08-05).

# Bugs of the original — canonical register

> Ultima V: Warriors of Destiny (Origin/EA, DOS, 1988) has defects. Porting it faithfully
> forces a decision, one by one, about which ones to reproduce and which ones to fix. This
> document is that decision, written down, with the binary citation next to it.
>
> Created on 2026-08-05 by the `bugs-original` lane from the census over 665 notes in
> `re/notes/`, the ledger of sealed routines and the repository history. It implements the
> user's directive of 2026-08-05 and the corresponding amendment to
> [`FIDELITY-CONTRACT.md`](FIDELITY-CONTRACT.md) §Policy.

## How to read this register

**Not everything odd is a bug.** To enter here as a bug, there has to be a demonstrable
oversight — a dead branch, a hole where a piece of data belongs, a promise with no effect,
broken arithmetic — and, whenever one exists, the symmetry argument: *the sibling routine
that does handle the same case correctly*. When that argument is missing, the entry drops
to "quirk" or stays out. §5 collects what we investigated and **discarded**, which in a
document like this matters as much as what goes in.

**Every claim travels with its grade**, and the grade is not decoration:

| Grade | Meaning |
|---|---|
| **MEASURED** | Read instruction by instruction in the disassembly, by whoever signs the entry. |
| **DERIVED** | Deduced from the code with explicit reasoning, without seeing the game run. |
| **INFERRED** | A reading of intent. Never supports an entry on its own. |
| **WITNESSED** | Additionally observed in the binary running under DOSBox. |

**Where it is said, and where it is deliberately not.** Every row of §1, §2 and §4 declares
its grade in the body. §3 (errata) and §5 (discarded candidates) **do not carry one, and that
is not a hole**: a quotation mark EA left open has no grade of evidence, and neither does a
refuted candidate — their nature already says everything there is to say about them. A bug
derived from the code is a bug; it simply is not the same as one seen happening, and the
document does not pretend otherwise.

**Grades today: 20 MEASURED · 9 DERIVED · 0 INFERRED · 1 WITNESSED · 1 undetermined**, over 31
rows carrying a grade (§1 + §2 + §4). The figure is not written by hand: the `/differences`
generator derives it from the file itself, and `re/tools/test_grados_registro.py` matches it
against this sentence in both mirrors. The sentence itself is rewritten by
`python3 re/tools/registro_cifras.py --write`: after a merge or a new row it is regenerated —
never recomputed by hand.

🔴 **"Almost everything here is DERIVED without an oracle witness. Each row says so." used to
live here, and it was false on both counts** (withdrawn 2026-08-10). It was not said in each
row: **seven** rows declared no grade at all —§1.8, §2.6 and five of the eight bullets of §4—
and §3 and §5 never declare one, by nature. And the dominant grade is not DERIVED: it is
**MEASURED**, by more than double. The sentence had, since the register's first day, asserted
of the whole population what was true of a part, and its "each row says so" half described a
rule the document did not keep. The seven silent rows were closed on 2026-08-10 by proposing
the grade **from the evidence each one already cited** —never by resemblance to its
neighbours— and the one whose evidence sustains none is declared **undetermined**, which is
not the same as saying nothing.

## The rule that decides, and why it is not "severity"

The cut-off criterion is **not** how severe the bug is. It is this, in this order:

1. **Does it touch the RNG stream?** If it does, **it is always reproduced**. This port's
   verification rests on reproducing the original's sequence of random numbers; fixing a
   bug that moves the stream breaks the very instrument we check everything else with.
   These bugs are declared here and they stay.
2. **If it does not**, and it is a clear oversight, **it gets fixed** and recorded.
3. **Typographical errata are preserved.** Closing a quotation mark EA left open is not
   fixing a bug: it is rewriting the original. They are part of what is being preserved.

---

## 1. Bugs OpenU5 fixes (or is going to fix)

These do not touch the stream. They are clear oversights. The real status of each one is
in its row — several are **pending**, and the register does not hide it.

### 1.1 The shipyard calls a female Avatar "sir" ✅ FIXED

**The original:** in the shipyard, the dialogue addresses the Avatar as "sir" even if she
is a woman. The branch that would say "milady" exists (`DS 0x9FCC`) but **it is dead**.

**Why it is a bug:** three sites in the binary read the same gender byte (offset +9 of the
32-byte roster record, `DS 0x55B1`) and compare it against **three different constants**:

| Site | Compares against | Correct? |
|---|---|---|
| `SHOPPES.OVL:0x0b0c` | `0x0C` | yes |
| `SHOPPES2.OVL:0x00c3` | `0x0B` | yes |
| `SHOPPES2.OVL:0x085a` | `0x46` = `'F'` | **no** |

Character creation never writes `0x46` — the census of the 16 `INIT.GAM` records returns
zero. Two sibling routines use the right constant and one uses an ASCII letter that field
never contains: that is an oversight, not a criterion.

**OpenU5:** branches on `gender === 0x0c` and says "milady". *Port:* `game/src/ui/shop-console.ts:1599` and `:2116` (verified 2026-08-05).

**Grade:** DERIVED (`re/notes/asm-shoppes-acta.md` §5). No oracle witness.

### 1.2 The tavern bill leaves a hole if the Avatar travels alone ✅ FIXED

**The original:** when you pay for food, the tavern keeper says how many of you there are.
The routine that writes the number (`SHOPPES2.OVL:0x006a`, `print_alive_count_word`)
switches on `two`…`six`. **There is no singular case**: with a single living member it
falls through to the `ret` without printing anything, and the sentence comes out with a
gap — `" gold for the ␣ of ye,"`.

**Why it is a bug:** the healer (`SHOPPES.OVL:0x137c`) handles the same edge case — a
single member — and handles it well, returning 0 without asking. Two routines from the
same territory, the same boundary, and only one of them considers it.

**OpenU5: FIXED — it emits the whole sentence, and in the singular case it says "one".**
*Port:* `game/src/ui/shop-console.ts:2471` (`emitTavernPriceLine`) composes it, and its **two**
callers emit it: `:2432` (the food round) and `:2514` (the drink's round of the house) — the
same two paths that enter `SHOPPES2.OVL:0x00dc` in the binary. The count word comes from
`game/src/core/shops/shops.ts:871` (`tavernAliveWord`), which returns "one" where the original
falls through to the `ret` without printing: that is the **single deliberate departure** on
this line, and everything else is transcription. Landed 2026-08-06 (`f3889145`); the second
caller, 2026-08-08. Card #17.

🔴 **Correction to this very row (2026-08-08), and the staleness window was HOURS.** Until
today this row published an "Extension 2026-08-06" block with three claims "re-verified
against the tree". All three have ceased to be true, and **two of them ceased that same
night**: the extension was written on 06-08 and the fix landed on 06-08 at 23:57. They are
retired by name, which is how a published claim gets retired:
- "today it **does not emit that sentence at all**" — **false**; see the citation above;
- "both halves … **orphaned**: no code references them" — **false**: they live in
  `game/src/core/world/cmd-strings.ts:536-537` and have an emitter. (The corpus entries it
  cited, `game/src/i18n/es.json:4875` and `:4880`, are still where it said.)
- "`tavernRoundPrice` is **imported** in `game/src/ui/shop-console.ts:69` and never used
  there" — true until this commit, which **removes that dead import**.

What **still stands** from that block is the part about the original, not the clone: the
sentence **does appear in the corpus of the ORIGINAL** captured by the mirror, so the clone's
omission would not have been neutral. Grade for that part: MEASURED via OCR corpus, with the
noise visible in the quotations.

**Corpus census (2026-08-06), with its denominator:** **6 occurrences across 3 files** under
`game/e2e/espejo-tour/routes-ad/` — `ad02`, `ad08`, `ad19` — with **two distinct count
words**, both carrying OCR noise: "gold for the **si** of ye, sir." (4×, presumably `six`)
and "pj!d for the **(mree** of ye" (2×, presumably `three`). Two different party sizes, each
printing its word **in exactly the slot** `print_alive_count_word` writes to.

🔴 **And what the census does NOT establish, which is the half that matters: captures of the
SINGULAR case number ZERO.** None of the 6 shows the gap. That does **not refute** the defect
— what was read in the disassembly still stands — but it settles that **the mirror corpus
cannot be its witness**: the tour never takes a party of one live member into a tavern, so
the case that would produce the hole **is not sampled**. What the corpus does establish is
the sentence's SHAPE and that the original prints it. The gap remains **without an oracle
witness**, and the one it needs is a tavern purchase with `g_alive_b = 1`.

⚠ **And it is still not mounted — said here so it does not read as an oversight.** When this
row was closed (2026-08-08) mounting it was weighed and **declined**: it requires a dedicated
oracle instance and a savegame walked down to a single living member, and what it would
establish — that the original prints the gap — is already **derived from the disassembly with
the whole body read** (below). The witness would raise the grade of that half-line; it would
not change the verdict or the fix. It stands as a **declared** pending, not a silent debt:
what this row lacks is a capture of the singular case, and it lacks it knowingly.

*(Method note, in case it helps whoever picks this up: the first census returned **4 across 2
files**, and it was wrong. The pattern required the prefix "gold", which OCR had mangled in
`ad02` — "pj!d" — so **the pattern carried an assumption about the surrounding text**, and in
a noisy corpus that assumption does not fail loudly: it UNDERCOUNTS silently. The good figure
comes from searching on the stable fragment, "of ye".)*

**Grade:** **MEASURED** — full body read in `re/disasm/SHOPPES2.OVL.asm` (2026-08-05). The
switch is five `cmp`s against 2, 3, 4, 5 and 6 (`0x006d`-`0x0084`); **any other value, 1
included, falls into the `jmp 0x00aa` at `0x0086` and from there to the `ret` at
`0x00aa`**, never reaching any of the five calls to the printer. This confirms the
derivation in `re/notes/asm-shoppes-acta.md` §26.1 and §27, whose citation was correct. No
oracle witness.
*(A vocabulary note, for whoever follows the citation: §26.1 declares its own grade with the
words "derived from the code, not observed in the running original". That "derived" is NOT
the DERIVED grade of the table above — it is exactly what this table calls MEASURED without
WITNESS: read in the disassembly, not watched running. The record and this row state the
same thing in different vocabularies; there is no grade contradiction.)*

### 1.3 Sleeping in a bed wakes you an hour late ✅ FIXED

**The original:** `CMDS.OVL` computes the target hour and normalises it incorrectly when
crossing midnight. Read in full:

```
059e: mov al, [g_hour]        ; current hour
05a1: sub ah, ah
05a3: add ax, si              ; + hours requested (si arrives as the digit's ASCII)
05a5: sub ax, 0x30            ; ASCII → number
05a8: mov [bp-6], ax          ; target = hour + hours
05ab: cmp ax, 0x17            ; past 23?
05ae: jle 0x5b4               ; no → no adjustment
05b0: sub word ptr [bp-6], 0x17   ; ★ yes → SUBTRACTS 23, NOT 24
```

With a target of 24 or more the adjustment falls one hour short: sleeping 2 h from 22:00
gives a target of 24 → 24 − 23 = **1:00**, when midnight is 0:00. **You wake an hour
late**, whenever the sleep crosses midnight.

**Why it is a bug:** on a 24-hour clock, normalising by subtracting 23 has no design
reading. It is the arithmetic error in its simplest form.

**OpenU5:** sleeps the number of hours requested. *Port:* `game/src/core/world/camp.ts:227`
(`bedSleep`, verified 2026-08-05). Card #22.

**One nuance we do NOT put here:** the same loop exits when `hour == target`, meaning the
original sleeps **until the hour boundary**, not N complete hours (starting at 10:37 and
sleeping 3 h wakes you at 13:00). That one may well be design, and it will be decided when
faithful sleep is ported — it is not sold as a bug.

**Grade:** **WITNESSED** (see below) on top of a MEASURED instruction-by-instruction read of `re/disasm/CMDS.OVL.asm`
(2026-08-05). It also corrects the citation inherited from the notes
(`re/notes/cama-241-acta.md` §6, `re/notes/sueno-cama-249.md` §1-2), which placed the
subtraction at `0x05ab`: that is the `cmp`, and the subtraction is at `0x05b0`. The notes
also omitted the `sub ax,0x30`, which is what reveals the argument arrives as ASCII.

**★ EXECUTION WITNESS OBTAINED, with a negative control.** Prediction registered *before*
running: seeding 22:00 and sleeping 3 h, the buggy target is `25 − 23 = 2` (waking at
**02:00**) and the correct one `25 − 24 = 1` (**01:00**). Two runs of the same instrument,
against the real binary under DOSBox:

| Route | Wake-up hour | |
|---|---|---|
| **Sleeping in a BED** (Iolo's Hut, cell 12,14 — the map's only bed) | **02:00** | ★ the bug, measured |
| Camping outdoors | 01:00 | negative control |

The bed wakes you **an hour late**, exactly as the subtraction of 23 predicts. And camping,
which uses a different sleep path, gives the correct 01:00 — which turns that run into a
**negative control**: the same harness is able to produce the right answer, so the 02:00 is
not an artefact of it. Probe, prediction and control in `re/tools/sleep_hour_probe.py`
(`main` = outdoors, `cama` = the witness).

**Finding grade: WITNESSED.** The target is consumed at `0x062e mov si,[bp-6]`, inside the
loop that advances the clock until `g_hour == target` (`0x063b`-`0x0645`).

**A second divergence of the clone in this same routine — CLOSED on 2026-08-08** (card #22).
The binary advances each hour in **six ten-minute steps**, and on **each one** it re-snaps the
NPCs to their schedule and checks whether anything is on top of the party — that is, six
chances per hour for "Thrown out of bed!" to fire. **The clone now does the same**: `bedSleep`
walks `hours * BED_STEPS_PER_HOUR` steps and calls `advanceClock(…, BED_STEP_MINUTES, …)` with
the NPC snap and the gate **inside every turn** (`game/src/core/world/camp.ts:263-273`,
verified 2026-08-12). It does not move the RNG stream; it used to move the SAMPLING, downwards.

🔴 **Until 2026-08-12 this said the clone took "a single 60-minute step per hour" and had "six
times fewer" chances to throw you out of bed.** It was true when written and stopped being
true when `5d7e0a04` landed, with nobody looking again: four days publishing as a live defect
of the port something already fixed. It is withdrawn by naming what it claimed, not by
deleting it — and the citation replacing it was read in today's tree, not copied from the card
that closed it.

⚠ The decoy at the site DID exist and was closed too: the comment on that line read "`0x0647`
advance_clock(10) per step" **right next to a call that passed 60** — it cited the binary
correctly and read as if it described the clone. Today the argument is `BED_STEP_MINUTES` and
comment and code say the same thing. The lesson stays because the mechanism does not depend on
this routine: **a comment citing the binary next to code that does not obey it reads as
documentation of the code**, and anyone auditing in passing takes it for faithful without
looking at the argument.

### 1.4 The frigate's delivery price: the figure does not match and accepting does nothing ⏳ PENDING

**The original:** two defects in the same branch (`SHOPPES2.OVL:0x08a8`).

1. The text promises **87,000** gold and the code compares against `0x2710` = **10,000**.
2. If you accept and can pay, the branch **does nothing**: it echoes "Yes", checks that you
   can afford it, and exits. It does not charge, does not deliver, does not touch the ship
   flags.

**Why it is a bug:** the skiff, in that very same routine, does enter the purchase routine
that charges and delivers. The frigate branch has the check and is missing the effect.

**OpenU5:** does not model the branch. *Port:* `game/src/core/shops/shop-tables.ts:62`
(`SHIP_REPLACEMENT_PRICE = 10000`), and `game/src/core/shops/shops.ts:1057` states in its
docblock that the replacement path is not modelled. Card #13.

🔴 **Correction to this very row (2026-08-06).** It used to say the clone "fixed the price at
10,000 (the code's figure, not the text's)". That is **false**: the constant exists but **no
code reads it**. Its only other appearance in all of `game/src` is the mention inside that
comment. It is a **dead constant**, so the clone fixed no price at all — there simply is no
replacement path. The earlier verification confirmed the symbol **existed** at that line, not
that anything **used** it, and a port citation is only worth something if it checks the latter.

Sibling genre, and that makes three in this same area of the clone: the dead import of
`tavernRoundPrice` in `game/src/ui/shop-console.ts:69` (§1.2) and the two orphaned tavern-bill
strings in i18n. **"Planned and never wired"** describes what happens in shops/tavern better
than three independent oddities.

**Grade:** the original's defect, DERIVED (`re/notes/asm-shoppes-acta.md`, ledger
`SHOPPES2.OVL:2216`). The clone's state, **MEASURED** against the tree (2026-08-06).

### 1.5 A dungeon chest on floor 0 hangs the game ⏳ PENDING

**The original:** chest loot asks for a quantity with **minimum 1 and maximum `floor·8`**
(`SJOG.OVL:0x179e`, the call at `0x1897`). On floor 0 that leaves the maximum at **0**,
below the minimum. And the generator does not check for it:

```
20b0: mov bx, [bp+6]   ; bx = minimum
20b3: mov cx, [bp+4]   ; cx = maximum
20b6: sub cx, bx       ; 0 − 1 = 0xFFFF
20b8: inc cx           ; → 0x0000
20b9: xor dx, dx
20bb: div cx           ; division by zero → INT 00h
```

**Why it is a bug:** `rand_range` is the shared generator for the whole game and it **has
no `maximum < minimum` guard**; the `div` turns the oversight into the end of the run. The
quantity table `DS:0x41C4` = `[31, 0, 3, 3, 3, 7, 7]` corroborates the diagnosis: it has a
zero in exactly the slot this path does not consult.

> 🔴 **CORRECTED (2026-08-07): this is not a hang, it is an ORDERLY EXIT with a message.**
> An earlier version of this entry said «hard hang», and it is measured that it is not.
> Startup installs its own **INT 00h** handler (`0x0244-0x024c` → `CS:0x0212`), which is the
> **Microsoft C runtime** one: it writes `run-time error R6003 - integer divide by 0` to
> STDERR and calls `exit(255)`, closing open handles and restoring the interrupt vectors.
>
> **How the runtime was identified, without relying on reading that body:** the message
> table at `DS:0xa452` (id-word + NUL-terminated string pairs, `0xFFFF` sentinel) decodes to
> `R6000 stack overflow` · `R6001 null pointer assignment` · `R6002 floating point not
> loaded` · `R6003 integer divide by 0` · `R6009 not enough space for environment`. The
> `R60xx` codes are Microsoft C.
>
> **What changes and what does not.** The defect is **still real** —there is no `maximum <
> minimum` guard in a helper the whole game uses— and the port's deliberate divergence
> (throwing instead of dividing) is **still correct**. What changes is the failure mode:
> **the machine does not freeze, the process terminates** and unsaved progress is lost.
>
> **Not measured, and therefore not claimed:** whether termination restores the video mode.
> The exit chain `0x037d`/`0x038c`/`0x07a2` has not been read, so we do **not** claim the
> message is actually visible — only that it is written to STDERR.

**Reach:** it does not happen with the factory maps — there are 3 chests in all of
`DUNGEON.DAT` and none on floor 0. The door stays open for any map generated at runtime.

**OpenU5:** ✅ **already covered**. JavaScript has no integer division by zero, so the hang
is not inherited, and on top of that `OriginalRng.next`
(*Port:* `game/src/core/rng-original.ts:95`, verified 2026-08-05) **rejects the
impossible range** with an explicit error rather than propagating it. That is
a deliberate divergence, authorised by the contract's hangs exception — this is the only bug
in the register the contract **already allowed** fixing before the amendment. One detail
that matters for parity: the rejection happens **before** the generator step is consumed, so
a caller that catches the exception is not left one step ahead in the stream — there is a
test pinning both halves (the impossible range does not advance the seed; the legal domain
comes out identical).

🔴 **CORRECTED 2026-08-06: the conclusion was right and the cited evidence pointed at the
wrong site.** This entry said `rand_range(1, floor·4)` and cited `SJOG.OVL.asm
0x182b-0x183c` as the caller. Re-reading the whole routine turns up **four** calls to the
generator, and the one cited **is neither the loot call nor able to hang**:

| call | minimum | maximum | hangs on floor 0? |
|---|---|---|---|
| `0x183c` (the one cited) | 1 | `floor·4` **+ 4** (`shl`,`shl`,`add ax,4` at `0x1838`) | **no** — the maximum is 4 |
| `0x1855` · `0x186e` | 0 | 7 | no |
| **`0x1897`, branch `si==1`** | **1** | **`floor·8`** (`mov cl,3`/`shl ax,cl` at `0x1886`) | **YES — the maximum is 0** |

Two independent errors cancelled each other into a plausible result: **the wrong call was
cited**, and **its arithmetic was read without the `add ax,4`** — which is exactly what
makes it harmless. The real site shifts by **three bits, not two**: `floor·8`.

The argument convention is established by the body itself — `bx = [bp+6]` is the minimum
because it is what gets **added back at the end** (`add dx, bx` at `0x20bd`) — and by the
caller's `push` order: the `1` is pushed **first**, so it lands in `[bp+6]`.

**Grade:** MEASURED (`rand_range` read in `ULTIMA.EXE.asm` 0x2091-0x20c5 — its prologue sits
at `0x2091`, not `0x2092`, because of the offset described in the prologue note; the four
calls read in `SJOG.OVL.asm` inside routine `0x179e`). The reach — the 3 chests — is
RELAYED from the note.

**The second door at `0x1897` DOES NOT EXIST — measured and closed (2026-08-06).** The
`else` branch takes its maximum from `[si+0x41c4]`, and the suspicion was that some
reachable `si` might land on the slot holding 0 and hang without needing floor 0. It closes
by enumerating two things, rather than by checking whether the 0 is in the table:

- **Which `si` reach the `else`.** The loop starts at `si = 0` (`1826: sub si,si`) and runs
  to `si = 6` (`18ae: inc si` · `cmp si,7` · `jge`). The dispatch peels off **5** (`1849`),
  **6** (`1862`) and **1** (`1878`) into branches of their own, so the `else` at `188c` is
  reached by **exactly `si ∈ {0, 2, 3, 4}`**.
- **What each slot holds.** `DS:0x41C4` = `[31, 0, 3, 3, 3, 7, 7]`, read **byte for byte
  from `DATA.OVL`** (file offset `0x41D4`), not from a hand dump: it is the same table
  checked by `re/tools/test_cmds_parity.py::test_tables_match_data_ovl`, which was written
  to break exactly that circularity. The four slots the `else` consults hold **31, 3, 3, 3**.

⇒ **no `si` reachable by the `else` ever reads a 0**, and the smallest maximum it can
produce is 3. **The only 0 sits in slot 1 — precisely the one slot the `else` can never
read**, because `si == 1` is peeled off into its own branch three instructions earlier.

**And the 0 is not an oversight: it is a marker.** Row 1 is the **gold** row, whose amount
comes not from the table but from `g_floor·8`. The 0 means "this row does not use the
table". Which is to say: the very value that looked like a latent second door is **the sign
of the row that IS the known door**. Hunting for the 0 in the table leads to the right
place for the wrong reason.

**The routine has ONE door, not two.** The four calls to the generator, with their regimes:

| call | minimum | maximum | can `max < min`? |
|---|---|---|---|
| `0x183c` (row guard) | 1 | `floor·4 + 4` ≥ 4 | no, never |
| `0x1855` (`si=5`, potion) | 0 | 7 | no |
| `0x186e` (`si=6`, scroll) | 0 | 7 | no |
| `0x1897` via `si=1` (gold) | 1 | `floor·8` | **YES on floor 0** ← the door |
| `0x1897` via `else` | 1 | `{31,3,3,3}` | no |

*(four `call` instructions, five paths: `0x1897` has two entries.)*

One more bound, so nobody turns it into a probability by ear: on floor 0 the gold row is
only reached if its guard clears, and that guard is **4** against a `rand(1, 4)` roll — the
roll has to land **exactly on the top of its range**. The "1 in 4" that follows from
assuming uniformity is deliberately NOT published here: §2.2 documents that this generator
**is not uniform**, and its distribution over a 4-wide range has not been measured.

### 1.6 "Ship rigged for double speed!" lies about when it takes effect ✅ FIXED

**The original:** the HMS Cape plans give the frigate double speed. But picking the plans up
with (G)et already writes `0xFF` into the byte, so **double speed is active from the moment
you pick them up**. The `or 0x80` that (U)se performs on a byte already reading `0xFF` is a
no-op, and the message `"Ship rigged for double speed!"` (`DS 0x49C2`) is pure decoration.

**Why it is a bug:** the message announces a state change that happened earlier and that
this action does not produce.

**OpenU5:** ✅ **announces where the state changes**. EA's message is now emitted on the
(G)et of the plans, which is where the frigate actually becomes rigged, and the (U)se
aboard emits a truthful echo — our own text, with no DS citation, because it does not exist
in the binary. *Port:* the announcement at `game/src/core/game.ts:4946`, the echo at
`game/src/core/endgame/use-tools.ts:111` (both verified 2026-08-05). **The mechanic is untouched**: double speed is still granted on pickup, and
the (U)se's no-op write is reproduced as-is. The other possible shape — making the (U)se the
thing that grants the speed — was deliberately rejected: that would not have been fixing a
bug but **redesigning** the original's mechanic.

**Grade:** DERIVED (`re/notes/trama-140-acta.md` §1.6).

### 1.7 The wishing well's horse appears inside a wall ⏳ DIVERGENT

**The original:** the well places the horse at `(x+1, y)` **without checking that the cell
is passable**. It can end up inside a wall or in the water.

**Why it is a bug:** it is a blind position write. Its sibling — the step east when getting
out of bed — does the same thing, but there the data backs it up: across the 32 small maps,
all 264 left-bed cells have a right bed to the east, 264 out of 264. At the well there is no
such invariant: the pattern lands in a wall roughly half the time.

**OpenU5:** already diverges (it checks passability). *Port:*
`game/src/core/game.ts:5717` (`spawnWishHorse`, verified 2026-08-05). Card #227.

**Grade:** DERIVED (`re/notes/deriv-211-acta.md`; the sibling's 264/264 census, MEASURED).

### 1.8 Data holes in text ✅ NONE REACHES THE PLAYER

The rule first: when what is missing is the **number** — not the punctuation — it is a hole
and it gets fixed. That is the boundary with §3, and it is deliberate: a missing piece of
data ⇒ fix it; a missing quotation mark ⇒ preserve it.

The two cases in the original that meet it, and what OpenU5 does with each:

- ~~**Donating 0 at a shrine.** The original prints `" gp"` — with no number — and exits
  without donating.~~ 🔴 **REFUTED on 2026-08-07 by reading the whole body of
  `shrine_visit` (`CAST2.OVL:2406`). It is not deleted: it was cited, and anyone who
  remembers it must find here why it falls.** The digit's echo **does** come out, and it
  comes out before the branch: `0x0b26 push si; call putchar` prints the ASCII character
  that was typed **and only afterwards** does `0x0b2a sub si,0x30` decide the branch. With
  `'0'` the screen shows `"0"` and then `DS 0x959c` = `' gp\n'` ⇒ **`"0 gp"`, not `" gp"`**.
  There is no missing number, and therefore this row did not belong in §4. The string
  `DS 0x959c` was read in isolation and the original was charged with a hole that only
  exists outside its emission context.
- **Donating 0 at a shrine — the REAL defect, and it is worse than the one it was charged
  with.** After printing `"0 gp"`, the zero branch **ends the shrine visit** (`0x0b2d jne`
  → `jmp 0xd1d`, the epilogue): it does not donate, it grants no karma, and it **does not
  ask again**. The contrast that makes it a defect and not a design decision sits twenty
  bytes away: the "thou hast not that much gold" branch (`0x0b52`, `DS 0xb6b9`) **does**
  ask again, in a loop (`jmp 0xb5f`). So the original already knows how to re-prompt on
  unusable input, and on zero it does not: one slip on the number pad throws you out of the
  shrine and forces you to walk back in. Valid amounts are `digit × 100` (`0x0b47 imul
  0x64`), with karma `+digit` capped at `0x63`; only keys `0x30`-`0x39` are accepted
  (`0x0b6f`/`0x0b74`), and any other one is re-requested silently — which reinforces that
  the zero exit is the exception and not the rule. *Port:* the pure function
  `shrineDonate` (`game/src/core/world/shrines.ts`) returns `{accepted:false}` without
  emitting text; the ceremonial layer above it (`submitDonation`,
  `game/src/core/world/shrine-ceremonies.ts:446-450`, verified 2026-08-11) prints `"0 gp"`
  and returns with no donation, no karma and **no re-prompt**, while its
  not-enough-gold branch does emit `shrine-donate-prompt` — so today the clone reproduces
  the exit. Whoever wires the re-prompt (F1.3) decides whether to keep it or correct it.
  Bucket **FIX** (does not touch the stream).
- **The tavern bill** from §1.2, which is the same pattern. **OpenU5 does not reproduce it
  either, because it does not emit that sentence at all today** — the `two`…`six` table does
  not exist in the core. *Port:* `game/src/core/shops/shops.ts:899` (`buyTavernRound`,
  verified 2026-08-05), which emits only the round message. If the bill is ever ported it
  must emit "one"; that obligation travels with card #17, where it will be decided.

> **How this was once misread, and the rule that came out of it.** The first version of this
> section listed both as faithfully reproduced, citing `world/shrines.ts:186` as proof. That
> line is a **comment** describing what the original does, not the clone's behaviour. Hence
> the rule the whole register now follows: **the port's state travels with a verified
> file:line citation**, just as the binary travels with its asm citation. A citation to a
> comment is not a citation to behaviour.
>
> **And its converse, for when the register grows**: a row whose port state **nobody has
> opened** is explicitly marked **"(unverified)"**, never left silent. The rule exists so that
> a future row is not read as verified by mere contagion from its neighbours, which is exactly
> how this section came to assert a reproduction that did not exist.
>
> **Coverage today: 22 of 23.** The denominator is the rows of §1 and §2
> — the only ones where "what does OpenU5 do" means anything; §3, §4 and §5 carry no port
> state by design — and the numerator those carrying at least one `*Port:*` file:line
> citation. The missing one is **§2.6**, marked "(unverified)" as the rule above requires.
> `re/tools/test_cobertura_port.py` recomputes both figures from the file itself and goes red
> if this sentence stops matching, in both mirrors; the sentence is regenerated with
> `python3 re/tools/registro_cifras.py --write` — never by hand.
>
> 🔴 **A "12 rows of 12" used to live here, and it was false on both counts.** Withdrawn on
> 2026-08-07 after walking all 25 commits of the file: **12 matches no denominator at any
> point in its history** — total rows 19→21 · §1+§2 rows 14→16 · §1 rows 8→9 · `*Port:*`
> citations 14→16 · rows with a citation 13→15. And "coverage is total" was not true the day
> it was written either: it was **13 of 14**, because §2.6 was already silent. The paragraph
> that INTRODUCED the rule about marking silent rows contained one, and declared itself
> complete.

**Grade (PROPOSED on 2026-08-10 from the evidence this row already cited):** **MEASURED** for
the shrine. The body of `shrine_visit` (`CAST2.OVL:0x0966`) is declared **read in full** —
"CUERPO ENTERO LEÍDO 2026-08-07 · 958 B declared = 958 B actual" (`re/notes/shrines.md` §1) —
and it is that same reading which brings the six addresses this row cites (`0x0b26`, `0x0b2a`,
`0x0b2d`, `0x0b47`, `0x0b6f`, `0x0b74`). A body read instruction by instruction is, by the
table above, MEASURED. The tavern half-line **inherits the grade of §1.2** and is not
re-declared here. **No WITNESS** for either half. ⚠ When citing, use `re/notes/shrines.md` and
**not** `re/notes/cast2-shrines-acta.md`: the latter predates 07-08 and still tabulates
`shrine_visit` as "❌ NOT READ", so anyone citing the wrong record will see this row as
lacking evidence.

### 1.9 Using the Skull Key inside a dungeon spends it and opens nothing ⏳ PENDING

**The original:** the Skull Key handler of the (U)se command decrements the key **before**
checking where you are.

```
CAST.OVL
18c4  dec byte ptr [g_skull_keys]        ← the charge
18c8  print "Skull Key\n"                  (DS 0x48fe)
18cf  cmp byte ptr [g_location], 0x21    ← the gate, AFTER the charge
18d6  cmp byte ptr [g_location], 0x7f
18db  jbe → print "Not here!\n"            (DS 0x4909) and leave
```

In the band `0x21..0x7f` the player loses a key and all they see is "Not here!". The
rejection is especially silent: on that path the result flag is still 1, so the dispatcher's
common tail (`0x1b8a`) **does not even emit the "Failed!"** or its tone.

**That band is the eight dungeons.** It is not a theoretical range.
`MAINOUT.OVL:0x0887-0x088c` writes `g_location = [bp-2] + 1` on entry, and the
`sub ax,0x4000` two instructions earlier pins the base at `0x20` (so the file offset is not
negative), with `0x200`-byte blocks — one dungeon each. Eight dungeons ⇒
**`g_location = 0x21..0x28`**.

**And it is genuinely reachable, even though the dungeon has its own keyboard loop.**
`dng_dispatch_key` (`DUNGEON.OVL:0x06c4`) handles only a handful of codes; **everything else
falls to its default branch (`0x07a0`), which calls `kernel_cmd_dispatch`**
(`ULTIMA.EXE:0x3178`). There `0x34e8 cmp ax,0x55` — the `'U'` — jumps to `0x340c`, which
prints "Use item" and enters the item dispatcher **with no location gate at all**. That is:
be in a dungeon, press `U`, pick the key.

**A second witness, and it comes from the clone itself:** OpenU5 ticket #123 fixed the case
where using the Skull Key inside a dungeon fell through to the surface path and unmagicked a
door on the map above. That fix would not have been needed had the branch been unreachable.

**★ The control sits in the handler next door.** The **carpet**, the immediately preceding
handler in the same dispatcher and the same kind of consumable, does the opposite: two gates
(`0x1869` location, `0x187f` tile) and only then `0x18a1 dec byte [g_carpets]`.
Guard-then-consume, 35 bytes away, inside the same routine — so "sloppy convention of the
era" is not available as a defence: the correct pattern was written right next to it. And
the same text carries two prices: "Not here!" appears **twice** in `DATA.OVL`, `0x48f3` (the
carpet's refusal, free) and `0x4909` (the key's refusal, costs a key); the player sees the
same line and cannot tell them apart. The guard census shows the same contrast:
`g_skull_keys` appears at **2** sites in the whole binary (the `dec` and one inventory read)
and has **not a single `cmp`**; `g_carpets` appears at **10**, with a zero guard
(`CMDS:0x0fd6`), a cap at 99 (`SJOG:0x14a9`) and two ways to replenish.

**The cost is bounded, and that is worth stating:** the `dec` has no guard, but it **cannot
wrap** `0 → 255`, because the item picker never hands over an item with count 0 —
`ZSTATS.OVL:0x05a4 find_next_owned` accepts an entry only if `byte [bx+si] != 0`
(`0x05ba-0x05bd`), and otherwise keeps searching. You lose one key per keypress, not your
inventory. Note that this safety lives in **another** routine, not in the `dec`.

**What it is not:** do not confuse it with the gem. `(V)iew a gem` (`ULTIMA.EXE:0x341a`) has
the same shape — decrement, then look at `g_location` — but **both** its branches do
something (surface viewer `LOOKOBJ.OVL:0x10fc` / dungeon viewer `DNGLOOK.OVL:0x06a8`). There
the item is spent and used; here it is spent and refused.

**OpenU5:** today it **reproduces the bug**. *Port:* `game/src/core/game.ts:3170-3181`
(`useSkullKey`, `skullKeys--` and then
`if (location >= 0x21 && location <= 0x7f) → "Not here!"`, verified 2026-08-07). It belongs
in this section rather than §2 because **it does not touch the stream** — the only
`rand_range` in the dispatcher's 1054 bytes is the carpet's orientation coin (`0x1899`), off
this path — so rule 2 applies. Fixing it means moving the `dec` after the gate.

**Grade:** MEASURED (`re/notes/skullkey-alcanzabilidad.md`; the body of `CAST.OVL:0x1792`
read instruction by instruction in `re/notes/cola-cast-acta.md` §3). **No WITNESS**: the
original has not been run under DOSBox for this row.

---

### 1.10 Passing at the Skull Key prompt spends it **and blows up your own square** ⏳ DIVERGENT

Sibling of 1.9 — same handler, **different path**: you are not in a dungeon, you simply do not
pick a direction.

**The original:** the door-opening worker (`CAST2.OVL:0x0768`) returns **three** distinct values,
and the caller only tells **two** apart.

```
CAST2.OVL:0x0768
 076e  call 0x306        → asks for a direction
 0775  cancelled         → return 0xFFFF
 079d  not a door        → return 0
 07a3  opened            → return 1

CAST.OVL (the key's arm)
 18dd  call → the worker
 18e3  or ax, ax
 18e5  jne 0x18ea        ← 0xFFFF IS NON-ZERO: cancellation enters through the "success" door
 18e7  jmp 0x1b8a          (only the 0 leaves here)
 18ea  cmp byte [g_location], 0x80 ; jb 0x18f4
 18f4  push [g_cmb_scratch_x] ; push [g_cmb_scratch_y]
 18fc  call → explosion_fx_at_cell (ULTIMA.EXE:0x3522)
```

`or ax,ax` splits the space into `{0}` and `{1, 0xFFFF}` — it puts **cancellation in the same
bucket as success**.

**And the coordinates are not empty: they are yours.** `CAST2.OVL:0x0306` seeds
`g_cmb_scratch_x/_y` with the **caster's own cell**, unconditionally and **before** the prompt,
and the SPACE path never touches them. So `0x18f4` hands over **your own square**.

⇒ **Pressing SPACE at the prompt spends the key and fires the cell FX on the party's own square,
with no error message at all.** `explosion_fx_at_cell` (sealed row, whole body read, 66 B) does
`blit_tile(y, x, 0)` — tile 0 —, `noise_burst(2000,3000,10)` and a viewport redraw. **Limit, in
the same sentence:** calling tile 0 "Explosion" comes from
`docs/manual/companion/tiles-lookup.js`, a **third-party** table; from the binary all that is
derived is that it is tile 0. And this entry **does not depend** on that routine's contents: what
is claimed here is **which coordinates it is called with**.

**★ The blindness is COMPLEMENTARY, and only shows when you read the PAIR.** The other caller of
the same worker — the **In Ex Por** spell arm, `CAST.OVL:0x1026` — does `cmp ax,0xffff`, splitting
the space into `{0xFFFF}` and `{0, 1}`: it **gets cancellation right and confuses "there was no
door" with "opened"**. Each handles exactly one of the two failures correctly and the other one
wrongly, and **neither is a superset of the other**. A single ledger row cannot show this; you
have to look at both callers at once.

**OpenU5: DIVERGES, and is not fixed — it is declared.** The clone **spends the key the same way**
(faithful) but on cancellation it **draws nothing**. *Port:* `game/src/core/game.ts:3170-3195`
(`useSkullKey`; cancellation leaves at `game/src/core/game.ts:3187`,
`if (!dir) return events;`) — read and verified 2026-08-07. That is, the clone is **missing** the effect the
original does paint. It is **pure presentation and does not touch the stream** — the dispatcher's
only `rand_range` is the carpet's orientation coin (`0x1899`), off this path — so there is no
parity risk in either direction. It belongs here as a **declared divergence**, not as a pending
fix. ⚠️ Minor citation note: the port's comment attributes cancellation to `0x18e7`, and `0x18e7`
is the **"not a door"** path; cancellation leaves through `0x18ea`.

**Grade:** MEASURED. The complementary-blindness mechanism and the worker's caller census come
from `re/notes/inexpor-dos-llamadores-acta.md` (lane `bugs-original`); **the reading of
`CAST2.OVL:0x0306` that turns the blindness into this concrete consequence is `cola-cast`'s**
(`re/notes/cast2-shrines-acta.md` §1.2). **No WITNESS**: the original has not been run under
DOSBox for this entry.

---

### 1.11 Holding down V eats the gem and does not show you the map ✅ FIXED

**What the player sees:** presses `V` to look at the map, the console says "View a gem!" — and
nothing appears. One gem fewer and a turn spent. Reported by an OpenU5 player in the throne
room of Blackthorn's Palace, with a screenshot.

**The original:** the gem view is a loop that closes on the **first key sitting in the buffer**,
and the keyboard's auto-repeat puts the very key that opened it right there.

```
LOOKOBJ.OVL  gem_view
10fc..1187  draws the 32×32 grid and the marker
118a        sub si, si
118c        jmp 0x11b6              ← straight to the poll, WITHOUT flushing the buffer first
11b6        call → kernel 0x1b38 → 0x1d5e
11bb        je 0x118e               ← 0 = no key, keep blinking
11c3        ret                     ← with a key: EXIT

ULTIMA.EXE  0x1d5e  (the poll)
1d6a        mov ah, 1 ; int 0x16    ← any key? (non-destructive)
1d86        mov ah, 6 ; int 0x21    ← READS it and CONSUMES it
```

A PC keyboard does not tell a repeat from a fresh press: the BIOS puts the same code in the
buffer, and the game **never changes that cadence** — census of the 28 files of the corpus:
every `int 16h` in the game uses `AH=1` (poll) or `AH=2` (modifier keys), and **zero** use
`AH=03h`, which is the function that sets the repeat delay and rate. The system's setting
stands. So while you hold `V` down: the press opens it, the first repeat **closes** it, and the
next one **opens it again**, spending another gem, because the main loop reads it as a fresh `V`.

**Why the player does not connect it to holding the key.** The "View a gem!" echo is only
printed on OPENING (`ULTIMA.EXE:0x341e`, before the gem gate); leaving the loop prints nothing.
With two press-equivalents you see **one** echo and no view: identical to the command being
broken. It is **parity** that decides whether it stays open, not the number of echoes — which
counts openings only.

**Measured in OpenU5 before the fix** (synthetic `V` repeats, counting console echoes and final
state):

| presses | echoes | gems | turns | view |
|---|---|---|---|---|
| 1 | 1 | −1 | 0 | open |
| 2 | 1 | −1 | +1 | **closed ← what the player reported** |
| 3 | 2 | −2 | +1 | open |
| 4 | 2 | −2 | +2 | closed |

**Reachability: high, and nothing exotic about it.** You do not have to mash the key: one
deliberate press held past the system's repeat delay is enough — the delay each player has
configured on their keyboard, and which in the original was their PC's BIOS setting. Looking at
the map is precisely the gesture where one lingers with a finger on the key.

**OpenU5 fixes it:** an auto-repeat no longer closes the view; you have to release and press
again. The press that opens and its repeats are **the same gesture**, and one gesture cannot
open and close at once. *Port:* `game/src/main.ts` (the `canvasGemActive` branch of the
`keydown`), with the zodiac view (`zodiacActive`) brought in line for consistency — that one
spent nothing, it only flickered.

**★ Why the guard is NOT on the whole keyboard.** Because in Ultima V **you walk by holding the
arrow down**: auto-repeat *is* continuous movement. Blocking it in general would have traded
this defect for a bigger regression, and a less faithful one. Only what has no defence is
fixed — that the repeat of the same press which opened a modal should close it — and
walk-by-holding was checked intact after the fix.

**Not to be confused with §1.9.** That entry says that with the gem "the item is spent and is
used", by contrast with the Skull Key which is spent and refused. It still holds: it is about
the location gate, where both gem branches do something. This entry is **another** way to lose
the gem, through the view's loop and not through the gate.

**Grade:** MEASURED on both sides. Binary: bodies of `gem_view` and of the poll read
instruction by instruction, plus the `int 16h` census over the whole corpus. Port: the parity
table above, reproduced on the exact case of the report. **No WITNESS**: the original has not
been run under DOSBox holding the key down.

### 1.12 During animated waits, the last torch's halo is stamped over your window ✅ NOT REPRODUCED

**The original:** the routine that redraws the play window (`ULTIMA.EXE 0x5910`) has two
branches: **full recompute** — it redoes visibility from scratch — and **incremental**, which
walks the 121 squares and repaints with raw terrain **only the ones that are zero** (`59ad`).
The problem is where those zeros come from. The pass that computes the torch halos uses the
party's window buffer as **scratch paper**: it marks every square it visits by writing zero
(`5b89`), and it does so with an index **in the torch's own local coordinates**, while its
writes to the light buffer *do* carry the origin (`5b48-5b5c`). Two different indexing
conventions inside the same routine.

Nobody else leaves zeros behind: after a full recompute, `0x5D0A` walks the 121 squares and
**turns the zeros into `0xFF`** (`5d76-5d8a`). So the only source of zeros in the system is the
light pass's scratch paper ⇒ an incremental frame repaints exactly the visited-footprint of the
**last** torch processed, with the shape it had in its own frame, wherever that lands on your
screen.

**Why it is a bug:** it reveals terrain in the wrong place, and it does so by reusing one buffer
for two meanings. It is not rare, either: `0x10d0` calls the redraw **32 times per invocation**
(loop `0x1070`, every 8 turns of the sound driver) with no game event in between, and the
recompute flag is cleared on the first one (`598a`) — 31 of those 32 passes take the incremental
branch. You see it during animated waits, and it goes away as soon as the party does anything.

**OpenU5:** ✅ **does not reproduce it**. The clone recomputes visibility in full every frame
(`computeVisibleWindow` is a pure function, with no state between frames), so it has neither the
shared buffer nor the two branches. Reproducing it would mean introducing mutable state between
frames plus the flag-driven split, in order to obtain a flicker of misplaced terrain.
*Port:* `game/src/core/world/visibility.ts` (the docblock declares it with the citations).
**Not to be confused with the BRIDGE**, which is the part of that same routine that *is*
mechanics and *is* reproduced (ticket #256): outside the party's radius, a transparent square is
only seen if the **neighbour it was reached from** is lit as well — which is why a torch's halo
only extends your sight once it touches your own circle of light.

**Grade:** DERIVED (`re/notes/visibilidad-256-acta.md` §2; bodies of `0x5910`, `0x5A28`,
`0x5D0A` and `0x5E4A` read instruction by instruction). **No WITNESS**: the original has not
been run under DOSBox to photograph the footprint.

---

### 1.13 Fourteen dungeon rooms leave you locked in forever ✅ FIXED

**What the player sees:** in Doom, goes down from level 2 to level 3 by the U/D ladder, lands
in a room with no walls holding giant rats and wisps, wins the fight — and can no longer move
in any direction. No walking, no Klimb, no spell. The game ends there. Reported by an OpenU5
player on 16-08.

**The original:** the room is fine; what has no exit is the square it puts you back on. When a
room fight ends, `dng_enter_room` **returns you to the cell you entered from**: it saves
`g_party_x/y` on entry (DUNGEON.OVL 0x0084/0x008c) and restores them on both exit branches
(0x00fa-0x0103), without looking at which board edge you left by. And fourteen room cells in
the game have **all four neighbours walled** and no secret door for (S)earch to reveal —
census over the raw bytes of `DUNGEON.DAT`, 14 of the 198 room cells. On the already-cleared
tile (`0xF6 & 0xAF = 0xA6`, 0x00f5) the original accepts nothing:

```
DUNGEON.OVL  0x05FF   walk      hi ∈ {0xB,0xC,0xD} → "Blocked!"   (all four neighbours are 0xB0)
             0x1e5e   Klimb ↑   hi ∈ {0x10,0x30}, or bit 0x08 of the tile WITH the grapple
             0x1e79   Klimb ↓   hi ∈ {0x20,0x30,0x60}
CAST.OVL     0x0fd2   Uus Por   cmp byte ptr [g_location],0x28 ; jmp <silent failure>
             0x0ffc   Des Por   same — 0x28 = 40 = Doom: both are vetoed in that dungeon
```

The pattern repeats in all fourteen: the room cell sits where a **ladder's landing** would be,
and the paired ladder still exists on the adjacent floor (`0x10` up, `0x20` down, `0x30`
both). The room's high nibble does not carry the ladder, so the landing stops being a landing.

**Four of them have an accidental door.** The Klimb-up gate reads bit `0x08` of the **raw**
tile without looking at the high nibble (`1e52: and ax,8`), and in a room cell that bit is
bit 3 of the **room number**. Unintended consequence: the five sealed cells whose room number
is ≥8 read as "lit", and with the Grapple the original climbs out through the ceiling. Four
escape; the fifth is the endgame room (Doom floor 7), where the tile above is a falling pit
and `on_enter` (0x0C76) brings you back — up and down, floor 7 → 7. The landing check does not
stop it because `dng_landing_ok` (0x1C0C) only inspects the destination tile with `mode ≠ 0`,
and Klimb passes `mode = 0`. ⇒ **ten hard traps in 1988**, and four that depend on carrying an
optional item. The one in the report (room 6, bit 3 = 0) is among the ten.

**OpenU5 fixes it** with a new condition in the Klimb gate, declared as a deliberate
divergence where it lives: on an already-cleared room, if the cell on the adjacent floor is
the ladder whose pair occupied this landing, you climb that way. It is not an invented value —
the relation is already in 1988's data; what is restored is the way you came in. Two
properties bound the intervention, both measured:

- **it only acts where the binary offers NOTHING.** If 1988 gave any exit, that one rules and
  the new condition is not even consulted. Without that guard, in Covetous 5 (0,0) and Doom 6
  (1,3) — the two the grapple already opens upwards — it added a DOWN that does not exist, and
  with both directions live the command started asking for the "Klimb-U/D-" prompt where the
  original did not ask;
- **coverage 13 of 14.** The one left out is the ENDGAME room, and it must stay sealed: that
  you cannot walk out of there is the end of the game. Nothing is lost either, because in 1988
  the grapple does climb it and the pit brings you back.

*Port:* `game/src/core/dungeon/dungeon.ts` (`parejaDeEscaleraBajoSala`, with the
minimal-intervention guard in `klimbCaps`). *Guard:* `game/tests/salas-selladas-mazmorra.test.ts`.

**Grade:** MEASURED on both sides. Binary: bodies of `dng_enter_room`, of the Klimb gate, of
`dng_landing_ok` and of the two CAST gates read instruction by instruction, plus the census of
the 14 cells over the raw bytes of `DUNGEON.DAT`. Port: the behaviour of all 14 exercised cell
by cell, before and after. **No WITNESS**: the original has not been run under DOSBox as far as
one of the fourteen. Full derivation in `re/notes/salas-selladas-mazmorra.md`.

---

## 2. Bugs OpenU5 reproduces on purpose

**Almost all of these move the random number stream, or depend on it.** Fixing them would
break the instrument the entire port is verified with. They stay, and they are declared.

There is a **second reason** to reproduce a defect, and §2.7 is the first to use it: when
the defect is that the original **does not do something**, "fixing" it means **inventing
content EA never shipped**. That is not porting, and so it too gets reproduced. *(This
paragraph was added with §2.7: until then the section claimed the stream was the only
reason, and that row would have made the claim false.)*

### 2.1 The nocturnal encounter bonus lives in a dead branch

**The original:** the enemy spawn threshold adds +3 "at night" (`MAINOUT.OVL:0x0D8C`). The
condition is `hour >= 0x20` **or** `hour < 5`. But `0x20` is 32, and the hour lives in
0..23: **that half of the condition is never satisfied**. The bonus only comes in through
the other half, from 00:00 to 04:59.

**Why it matters, and is not a curiosity:** on normal terrain the base threshold is 1, and
the roll is `rand(1,30)`, so **without the bonus nothing ever spawns**. The upshot is that
in the Ultima V of 1988 **dusk, from 20:00 to 23:59, has no random encounters on normal
terrain**. The stretch you would expect to be dangerous is the safest of the day.

**INFERRED, and labelled as such:** `0x20` where the plausible intent was 20 decimal
(`0x14`) looks very much like the classic hex/decimal slip. That is a reading of intent and
supports nothing on its own; the fact — the branch is never satisfied — is MEASURED.

**OpenU5:** reproduces the dead branch. *Port:* `game/src/core/world/loops/spawn.ts:37`
(`spawnThreshold`, verified 2026-08-05). A toggle remains a future possibility, once the
verification mirror no longer depends on the stream.

**Grade:** MEASURED (`re/notes/loops.md` §1.1 and §1.2, asm verified).

### 2.2 The 1-to-30 generator is not uniform

**The original:** `rand30` yields **`P(1) = 4/61`**, `P(2..29) = 2/61`, `P(30) = 1/61`. The
cause is an **inert** sign correction in `ULTIMA.EXE:0x3abe` (0x3ad0-0x3ad1): the
instruction is there and does nothing.

**The derivation, so nobody has to redo it.** `0x3ac9` pushes **60** and calls `0x3aae`,
which is `rand_range(0, n)`. `rand_range` is **inclusive at both ends**, so it returns
`r ∈ 0..60`: **61 equally likely values**. Then `cdq` / `sub ax,dx` / `sar ax,1` is the
integer-divide-by-2 idiom — and that is where the **inert** sign correction lives, because
`r` is never negative — and `0x3ad8`-`0x3adc` raises 0 to 1. Counting:

| value | comes from | cases |
|---|---|--:|
| **1** | `r ∈ {0,1}` (raised by the `inc`) **and** `r ∈ {2,3}` | **4** |
| 2..29 | two `r` each | 56 |
| 30 | `r = 60` | 1 |
| | | **61** |

✅ **INDEPENDENTLY REPLICATED on 2026-08-06.** The `4/61` has been derived **twice, by
different routes, with neither author aware of the other's**: here, from the counting table
above; and in the web area, by **enumerating all 61 values** of `max(1, rand(0,60) >> 1)` —
same result, `P(1)=4/61 · P(30)=1/61`, and in both the distribution sums to its own
denominator. Worth recording because **the grade rises and the number does not show it**: a
figure replicated by two independent methods is no longer «its author derived it».

🔴 **CORRECTED 2026-08-06: this row said `3/61`, and it was false.** The error gives itself
away with one sum: `3 + 28·2 + 1 = 60`, which **is not the denominator**. A distribution
that does not sum to its own denominator is wrong by construction, and checking costs one
line.

**Why it is a bug:** the range mapping loses the uniformity the code itself is trying to
impose. The high end comes up **four** times less often than the low end (`1/61` against
`4/61`).

🔴 **That "four" read "three" until 2026-08-10, and it is the RESIDUE of the 06-08
correction.** That correction replaced `3/61` with `4/61` in the table and in the statement,
and **left untouched the sentence that INTERPRETS the figure** three paragraphs below — which
still carried the old ratio, written in words instead of digits. It is recorded because the
failure mode repeats: **a corrected figure does not drag along the sentences that translate it
into plain language**, and those sentences are precisely the ones quoted outside the document,
because they are the readable ones.

**OpenU5:** reproduces it exactly, and **cannot do otherwise**: it is the heart of the
stream. *Port:* `game/src/core/rng-original.ts:97` (the `% span` over `& 0x7fff`,
verified 2026-08-05). A warning for anyone reimplementing this — it invalidates any "clean" version along
the lines of `1 + rand(0,29)`.

**Grade:** DERIVED from the ledger (`asm-kernel-l4-tanda1`).

### 2.3 The Shadowlord's "is this a person?" check always looks at slot 4

**The original:** when looking for someone to possess, the gate reads the NPC type with
`mov bx, cx` at `TOWN.OVL:0x111f` — but `cx` is the counter of a loop that has **already
finished** and holds 4. It always consults slot 4, not the one being evaluated (its index
had been destroyed by `shl si,4` at `0x1103`).

**Why it is a bug:** the two sibling routines, `0x85e` (Astaroth) and `0x8d4` (Nosfentor),
**do reload the argument** to run their own test. Three sites, the same test, and only one
forgets to reload.

**Real effect:** dormant in the canonical game — the eight virtue cities all have a person
in slot 4, so the gate passes anyway.

**OpenU5:** reproduced, with tests. It consumes 32 rolls no matter what, just like the
original. *Port:* `game/src/core/world/shadowlord-urban.ts:108` (`possessGateRoll`,
verified 2026-08-05).

**Grade:** DERIVED (`re/notes/shadowlord-urban.md` §4).

### 2.4 Off the small map you always read cell (31,31)

**The original:** with an out-of-range coordinate, `get_tile_ptr` neither clamps nor wraps:
it returns a fixed pointer, `DS:0x6A07`, which is the last byte of the buffer — cell
(31,31). It is an out-of-range read that happens to be harmless by accident.

**OpenU5:** reproduced. It is what explains the grass border around Britain.
*Port:* `game/src/core/world/map.ts:48` (`edgeFillTile`, verified 2026-08-05).

**Grade:** DERIVED, already declared in `re/deliberate-divergences.md`.

### 2.5 In Bet Xen summons all four creatures onto the same cell

**The original:** the second loop of `CAST.OVL:0x07b4` **does not call the cell picker
again**, so the up-to-four summoned creatures all appear on top of the same one.

**Why it is a bug:** the picker exists and is used for the first one. Not calling it again
is the definition of an oversight.

**OpenU5:** today it diverges (it calls the picker per creature). *Port:*
`game/src/core/combat/combat.ts:2589` (verified 2026-08-05). **It is being corrected
towards reproducing the bug**, not towards fixing it, precisely because of the stream rule.
Card #6.

**Grade:** MEASURED by the combat lane.

### 2.6 The roll is spent on the dead and on those who resist too

`DUNGEON.OVL:0x0948` asks for its random number always, even for members who cannot be
affected. It consumes stream. Reproduced; it is one of the contract's binding examples.

**OpenU5: (unverified).** Nobody has opened the clone for this row. It consumes stream, so by
rule 1 it is reproduced regardless — but "it is reproduced" is here a CONSEQUENCE OF THE RULE,
not observed behaviour, and the register does not conflate the two.

**Grade (PROPOSED on 2026-08-10 from the evidence this row already cited):** the mechanism is
**MEASURED**. The ledger's `dng_field_sleep` row declares "CUERPO ENTERO LEIDO 0x0948-0x09E4
(`ret`, no args)" and inside that very reading sits, verbatim, this row's claim: "rand_range is
called ALWAYS, with pushes (1, 0x1e) = range 1..30 — that is, the roll is consumed by the dead
and by those who resist too". **No WITNESS**: `re/verified/dungeon.md` files it under "asm +
model↔clone cross-check", not under oracle parity. Note the grade is of the BINARY; the port
state remains **(unverified)**, which is a different thing and lives above.

### 2.7 EA's easter egg is in the data and the code never reaches it ✅ REPRODUCED

**The original.** When you talk to any NPC, before looking at the keywords of that NPC's
script, the game tries a fixed word table: `NAME`, `JOB`, `WORK`, `BYE`, `THANK`, and then a
string of profanities, to which the NPC always gives the same reply —
`"With language like that, how did you become an Avatar?` (`DS 0x93D0`).

The table lives at `DS 0x4AA8` and has **35 entries**. The last one, number **34**, is
**`ELECTRONIC ARTS`**.

It is never tried. The table has **exactly two consumers** across the 28 disassembled files
— the "Your interest?" prompt loop (`TALK.OVL:0x0B43`) and the answer-to-a-question loop
(`TALK.OVL:0x0CB4`) — and **both carry the same bound**:

```
0bab / 0d15:  cmp byte ptr [bp-2], 0x22     ; 0x22 = 34
              jae  <exit>                    ; compared AFTER the increment
```

⇒ indices **0..33** are tried and 34 **never is**. The string `DS 0x9318` has **no other
reference** anywhere in the disassembly: it is dead data.

And the detail that settles it as intended-to-work: **`ELECTRONIC ARTS` is exactly 15
characters**, which is precisely the input buffer's cap for that prompt (`TALK.OVL:0x0A33`,
`mov ax,0xf`). It fits **to the character**. Someone wrote it so it could be typed, and the
loop's `0x22` left it out.

*Grade:* **MEASURED** on both halves — the table's contents read from the data file, and the
bound read in both instructions. The consumer census is by hex over the 28 `.asm` files; it
does not rule out access through a computed pointer, which was not censused.

**What OpenU5 does: it reproduces the defect — and reproducing it meant REMOVING something.**
This is where the case steps outside the section: **the clone did answer**. Its profanity
list held 30 entries — the binary's 29 in the same order **plus `ELECTRONIC ARTS`**, verified
by comparing both lists element by element against the data file. So it was not modelling the
original differently: **it was adding a reply the original does not give**.

That is why this row sits in §2 and not in §1. We do not reproduce it for the stream — this
costs not a single roll; we reproduce it because **making the easter egg work would mean
inventing content EA never shipped**. A faithful clone cannot add.

*Port:* `game/src/core/dialogue/conversation.ts` (`PROFANITY_KEYWORDS`, verified
2026-08-06). The entry removed, with the reason kept next to the code so nobody restores it
"for completeness", and a test that turns red if it comes back.

> **Where the extra entry came from, which is the instructive part.** It was not introduced
> while writing the clone: **it was inherited from a miscount that lived in three places at
> once** — the conversation engine's own comment, the ledger row's citation, and the corpus
> coverage census. All three said "indices 5..33 = 28 profanities + ELECTRONIC ARTS", and
> 5..33 is **29** slots, all 29 of them profanities. Nobody had recounted against the data
> file. **The port inherited the error from the citation, not the other way round**, and all
> three places were corrected in the same commit as this fix — because leaving one means the
> next reader restores the entry from the stale text.

### 2.8 On the 20th of every month, the Shadowlord's withering spares NOTHING

**The original:** when a Shadowlord takes a town, the vegetation withers
(`CAST2.OVL:0x022a`). The sweep seeds its own generator with the day of the month —
`srand(g_day)`— and for each wheat field or tree it rolls `rand(0,7)`: on a 0 that cell **is
spared** (`0x025f`/`0x027e`, `or ax,ax / je`). One in eight, by design.

**On the 20th none is spared.** The reason is not in the sweep: it is in the generator.
`20` is a **fixed point** of `rand_range`'s step function (`ULTIMA.EXE:0x2092`):

```
20 + 0x9248 = 0x925C   ; add ax, 0x9248
ror16(·, 3) = 0x924B   ; d1c8 ×3
    ^ 0x9248 = 0x0003  ; xor ax, 0x9248
      + 0x11 = 20      ; add ax, 0x11   → the seed does NOT change
```

With the seed stuck at 20, `rand(0,7)` returns **always 4** — never 0 — so the sparing gate
**never opens once**. That day the withering is total.

**Measured, day by day** (1000 rolls per day, on the clone's generator, whose parity with
the binary is verified under dosbox-x):

| day | cells spared out of 1000 |
|---|--:|
| 1..19, 21..28 | between **103 and 141** (≈ 1/8, as expected) |
| **20** | **0** |

And the fixed point is **unique**: sweeping all 65,536 states of 16 bits, `20` is the only
value whose step returns itself, and **its only preimage is itself** — you cannot reach it
by stepping, only by seeding it.

**Why it is a bug and not a curiosity:** the code explicitly asks to spare 1/8 and one day
in every 28 spares 0. It is visible on screen, it is deterministic and it happens to
everyone.

**OpenU5:** clones it, and **cannot not clone it** — just like §2.2, the defect lives in the
heart of the generator, and "fixing" it would mean changing the RNG the whole port is
verified against. *Port:* `game/src/core/world/shadowlord-wither.ts:81`
(`new OriginalRng(day & 0xff)`, verified 2026-08-07).

**Grade:** mechanism **MEASURED** (the fixed-point arithmetic checks out by hand in four
lines). Reachability **MEASURED AND POSITIVE**: day 20, deterministic, every month, whenever
the withering runs. **NO LIVE WITNESS** under dosbox-x: the table above comes from the clone
—a parity-verified generator—, not from watching the town wither on the 20th.

**An honest loose end, measured and NEGATIVE — the obvious route is not one.** The game
reseeds the RNG from the wall clock when camping, and the natural suspicion is that 20 could
slip in there. **It cannot.** The routine that builds that seed (`ULTIMA.EXE:0x2056`)
combines hour, minute, second and hundredth and ends with `and ax, 0xfff`; enumerating all
**8,640,000** possible clock readings, **none** produces 20 (in fact only 2688 of the 4096
12-bit values are producible at all, and 20 is not among them). So the camping reseed is
**not** a door to this defect, and the fixed point's worst consequences stay out of
reach — see §4. *(Arithmetic independently replicated on 2026-08-10 and with an executable
source since then: `re/tools/test_semilla_reloj.py`, in the battery, re-extracts the
constants from the body of `0x2056` and re-derives all three figures on every run, checking
them against what is published here.)*

### 2.9 In combat, two of the four field walls do nothing whatsoever

**The original:** the four wall spells (*In Flam Grav*, *In Nox Grav*, *In Zu Grav*,
*In Sanct Grav*) have **two branches** depending on where you are (`CAST.OVL:0x004c`,
`0054 cmp byte [g_location],0x80`). In a **dungeon** they write a field tile into the cell
ahead. In **combat** they seed nothing: they set a "spell weapon" (`00ef mov al,[bx+0x4592]`)
and call the ordinary attack dispatcher — the SAME one Grav Por, Vas Flam and Xen Corp call
(`0100 call 0xffffc14a`, which resolves to `COMSUBS.OVL:0x0c52`). And the damage of those four
weapons comes from the `attackValues` table: **18 · 0 · 21 · 0**.

⇒ In combat, **In Zu Grav and In Sanct Grav spend the mixed spell, spend the mana, spend the
turn and do ZERO** — no field, because that branch seeds none, and no damage, because their
table entry is 0. The other two at least hit (18 and 21), but they raise no wall either: the
player casting "a wall of fire" in the arena is throwing a dart.

**Why this is a bug and not a decision:** both branches exist and do different things on
purpose, so combat is not "unimplemented". What fails is that nobody gave two of the four
weapons a value: the spell reaches the engine and the engine has nothing to apply. A
non-effect that costs resources is not a mechanic.

**How we know combat seeds no field** (rather than us failing to find it): the census is not
"we saw no writes" but **who references the field-tile table**. `DS:0x4596` (tiles
`0x80-0x83`) appears **exactly once in the whole disassembly** — in the dungeon branch. The
combat chain has been read end to end (`attack_dispatch_by_reach` → `player_ranged_attack` /
`melee_strike_resolve` → `hit_roll` / `apply_damage_death_loot`) and none of the four routines
writes a tile.

**OpenU5: CLONED.** The values 18/0/21/0 are transcribed, not "fixed": inventing damage for
*In Zu Grav* would mean inventing content EA never shipped — the same reason as §2.7 — and it
would also move the stream. *Port:* `game/src/core/combat/combat.ts:406`
(`SPELL_WEAPON_STATS`, the four entries including the two zeros) and `:457`
(`combatCastEffect`), wired at `game/src/main.ts:2268` (verified 2026-08-08). A test guards
the clone: `game/tests/field-wall-dungeon.test.ts` turns **red** if anyone "fixes" the zeros.

*Native port (added 2026-09-21):* the same clone lives in `native/core/src/magic_tables.inc`
(`{Field,53,0}/{Field,51,1}/{Field,52,2}/{Field,54,3}`), `native/core/src/combat.cpp`'s
`combat_cast` (the `Field → Attack` rewrite, mirroring `combatCastEffect`) and
`native/core/src/combat_magic.inc`'s `spell_flight` (`{16,30,99,18,0,21,0}`, zeros included).
Its guard is `native/core/tests/batch12_combat_field_test.cpp` (CTest `batch12_combat_field`),
which turns **red** both if combat starts seeding fields and if the two zeros are given
damage. It was opened by a T-Deck hardware report of exactly this defect — "In Flam Grav
flashes but leaves no fire field" — so the ticket now has a *player-side* sighting on real
glass, not only a disassembly reading. Reachability is still **NOT SURVEYED**.

**Grade:** mechanism **MEASURED** (both branches, the weapon table, the damage table and the
whole combat chain, read). **Reachability NOT SURVEYED**: all four are allowed in combat by
the `DS:0x1C90` mask (`0x03`), so the path exists — but nobody has counted how many playthroughs
step on it. Ticket #91.

---

### 2.10 The speaker sweep never reaches its nominal frequency

**The original:** `pcspeaker_glide` (`ULTIMA.EXE:0x43ae`) takes four arguments — start,
end, step, total — and all 30 static call-sites in the game push a concrete `end`. The
routine uses it exactly ONCE: to compute the slope (`0x43bc sub ax,[bp+0xa]` ·
`0x43c2 imul` · `0x43cc idiv` = `trunc(((end − start)·step)/total)`); **it never compares
it** — the loop's cut is `0x43ec cmp di,[bp+4]; jl` against `total`, and each turn's tone
sounds BEFORE the increment. On top of that, the `imul` discards the high word (`0x43c5`
keeps only `ax`; `0x43cb cwd` re-derives the sign) and the `idiv` truncates toward zero, so
the rounding error accumulates turn after turn. Measured site by site
(`re/notes/firma-43ae-todos-los-callsites.md` §3, re-derived in
`re/notes/barrido-137-acta.md`): **the last divisor written to the PIT (`0x22e2
div 0x1234DE → out 0x42` ×2) matches the nominal in NONE of the 30**, and that register
RETAINS what was written until the gate goes OFF (`0x43f7 → 0x230e`). The two worst:
`OUTSUBS.OVL:0x0492` asks for 2500→800 and the sweep dies at **1005 Hz** (+205);
`MAINOUT.OVL:0x113b/0x12a6` asks for 660→150 and dies at **272 Hz** (+122 — more than an
octave above what the caller asked for).

**Why this is a bug and not a decision:** the datum exists and is only half-consumed. Every
caller writes a destination frequency that the routine uses for the slope and never for the
destination — a promise with no effect. The symmetry argument: the sibling primitive
`set_tone` (`0x22e2`) delivers exact Hz (= its argument), and the sweep itself starts
EXACTLY at `start`; only the `end` extreme goes unguaranteed. Where the division is exact
the deviation is one turn (−inc, inaudible as such); where it truncates, it blows up to the
+205 Hz above.

**OpenU5: CLONED** (ticket #137). The port synthesized a clean `start→end` ramp that DID
reach the nominal: an audible CLASS divergence, across all 30 sites at once. The real
staircase is cloned — same truncated increment, same early cut, same effective final
frequency. *Port:* `game/src/skin/fiel/speaker.ts:214` (`glide`, the body's arithmetic with
its 16-bit truncation) and `:1148-1154` (the synthesis switches divisor by divisor with one
`setValueAtTime` per turn, no ramp), in use by the catalog's six glide cues
(`:801`, `:805`, `:808`, `:809`, `:871`, `:874` — verified 2026-08-18). A test guards the
clone: `game/tests/glide-escalera-137.test.ts`, with the binary-derived finals written as
raw literals (1005 · 272 · 233 · 1980 · 1976) — it turns **red** if anyone restores the
ramp. "Fixing" it would mean inventing a sound 1988 never emitted — the same reason as §2.7
and §2.9 — and deciding whether EA "wanted" the short sweep is a reading of intent, not of
bytes.

**Grade:** **MEASURED** (the body `0x43ae-0x43ff` re-read instruction by instruction over
`re/disasm/ULTIMA.EXE.asm:7424-7459`, together with `set_tone` `0x22e2` and the gate-OFF
`0x230e`; the two witness call-sites read raw: `OUTSUBS.OVL.asm:462-470` and
`MAINOUT.OVL.asm:1745-1753`; the 30/30 census, in the note cited). Ticket #137.

---

---

## 3. Errata preserved as part of the original

Here punctuation is missing or a letter is wrong. Fixing it would be correcting the
authors, and this project preserves the game, it does not edit it.

- **`"a crytal sphere"`** — the tile name in `LOOK2.DAT`, with its 1988 typo. Preserved
  byte for byte in OpenU5's data.
- **The guild farewell leaves a quotation mark unclosed** — the string `DS 0x78a0` opens
  `"What else,` and never closes it. Preserved verbatim in
  `game/src/core/world/cmd-strings.ts:473`.
- **`"What didst thou say?"`** (`DS 0x9450`) — same case.
- **The intro dissolve's L5 band repeats one index and omits another** (it repeats `0x13`
  and skips `0x1d`), so one scanline is not revealed on that pass. It is harmless — the
  next band's overlap covers it — and it is preserved; there is a test pinning it precisely
  as proof that the dump is byte-exact.

---

## 4. Latent defects, with no demonstrated live path

Real in the code, without our being able to claim a 1988 player ever hit them. They are
listed for whoever picks up the thread, **not** as bugs anyone suffered.

- **Recruiting someone who is ALREADY travelling with you duplicates their record and
  destroys another** (`TALK.OVL:0x080a`, `join_party`). The scan looks the recruit up by
  the **first three characters** of their name, walking slots **15 down to 1** (0x0818
  starts at 15; 0x089a subtracts 0x20; 0x08a2 stops at 0). A member already in the party
  lives in slots `1..partySize−1` — **inside that scan** — so it finds them. It then runs
  the usual swap (0x08c8-0x0912) against slot `partySize`, copying their 32-byte record
  into the free slot and the free slot's into theirs, and increments `g_party_size`. The
  result: **their record appears twice and the other one is lost**, equipment included,
  because the `repne movsw` moves the whole record. There is no guard in the body; the
  only filter is the name.
  *Reachability:* **OPEN**. It needs a `.TLK` whose recruit opcode names someone who may
  already be in the party — plausible with companions picked back up from an inn, **not
  censused**. MEASURED in the disassembly; the live path is not.
  **OpenU5 does NOT reproduce it**: it guards the case and exits through the binary's own
  "no match" path, **without inventing text** (`core/party.joinByName`, verified
  2026-08-06). Corrupting the roster is not observable fidelity, it is data loss — and
  that is the boundary that separates this section from §2.
- **Stat dispatch with no default case** (`COMBAT.OVL:0x13e2`). The four `cmp`s cover
  −4..−1 and any other value falls to `0x1473`, which reads `[bp-4]` **never written** on
  that path: it returns stack garbage. The two concrete doors are (a) the record with its
  `0x40` bit set and a selector other than 0, and (b) the bit clear and a selector **equal
  to 0**. Of its three callers, two pass safe constants (−1 and −2) and the third
  propagates a value from its own caller: **reachability requires going one level further
  up the call graph**. MEASURED in the disassembly; reachability OPEN.
- **The other half of the RNG fixed point: the retries WITH NO CAP would hang** (§2.8 is
  the reachable half; this one is not). If `g_rng_seed` were 20, `rand_range` would return a
  constant forever, and there are loops that re-roll until the value suits them **with no
  counter**: the midnight Shadowlord reshuffle (`ULTIMA.EXE:0x5039 or di,di / je 0x5004`)
  and the overworld spawn draw. With a constant that collides, neither terminates — it is
  not slowness, it is a hang, and what freezes is the whole game's RNG.
  *Reachability:* **MEASURED AND NEGATIVE on both known routes.** (a) The clock seed cannot
  be 20: of the 8,640,000 possible readings, **zero** produce it (`ULTIMA.EXE:0x2056`,
  `and ax,0xfff`; only 2688 of 4096 values are producible and 20 is not among them). (b) The
  live stream starts at 0 and **its cycle has 47,343 states that do not include 20**; since
  20's only preimage is itself, you cannot fall into it by stepping.
  ⇒ That leaves **only** the `srand(g_day)` door of §2.8, which uses its OWN generator and a
  bounded loop: there it withers too much and terminates, it does not hang. **OpenU5 does
  NOT cap** those loops —the binary does not, and capping would be divergence—: it is
  declared and tested (`game/tests/survival.test.ts`).
  *Grade* (PROPOSED on 2026-08-10 from the cited evidence): the mechanism is **MEASURED**. It
  is the only one of the seven silent rows whose evidence carries the grade in those very
  words, and twice over: `re/notes/kernel-survival.md` §8.3 writes "Same grade as the
  `rand_range` defect with no `max<min` guard: mechanism measured, reachability narrow", and
  the note for the routine containing the loop (`re/notes/reloj-advance-clock.md`,
  `[0x4f7c, 0x51a0)` = 548 B declared = 548 B actual) closes with "**No WITNESS grade**: I have
  not run the original under DOSBox for this row". ⚠ Loose end CLOSED (2026-08-10): the
  reachability arithmetic —the 8,640,000 clock readings and the 2688 of 4096 producible
  values— was **independently replicated** from the constants re-extracted from the
  disassembly (same three figures; 20 remains unproducible) and **has an executable source**
  since then: `re/tools/test_semilla_reloj.py`, in the battery, re-derives the whole thing
  and checks the published figures on every run.
- **`text_gotoxy` ignores its own window's bounds** (`ULTIMA.EXE:0x1bf2`): the window record
  carries bound fields at +2/+3 and the routine compares against the hardcoded physical
  screen values. Invisible for window 0, where they coincide. Also, going out of range is a
  silent no-op — anyone porting this with clamping diverges.
  *Grade* (PROPOSED on 2026-08-10 from the cited evidence): **MEASURED**. The ledger row
  declares "CUERPO ENTERO LEIDO 0x1bf2-0x1c1f (48 B, 23 insn, `ret 4` = TWO arguments)" and
  both claims of this bullet are its own, verbatim: "(a) DOES NOT CONSULT ITS OWN WINDOW'S
  BOUNDS: … this body compares against IMMEDIATES, not against [si+2]/[si+3]" and "(c) OUT OF
  RANGE IS A SILENT NO-OP: it does not clamp, it does not signal". Record:
  `re/notes/asm-kernel-l4-tanda1.md` §3.1, which additionally declares that all three come
  from the body reading and not from the naming sweep's citation. **No WITNESS.**
- **Mix with a name that does not match** enters the quantity selector with a negative index
  and reads out of range (`CMDS.OVL:0x1b13` only checks for −1, not −2). OpenU5 avoids it on
  purpose.
  *Grade:* **UNDETERMINED**, and it is declared rather than passed over in silence. On the
  2026-08-10 grade pass this is the only one of the seven silent rows whose evidence **supports
  none**: the sole source stating the −2 is one line of `re/notes/cast-input.md` §7 —"Mix only
  checks `-1` (0x1b13 `cmp ax,0xffff`); a `-2` falls into the QUANTITY selector with a negative
  index … latent bug of the binary never exercised carefully"— which declares no grade, and the
  record that **does** hold the containing routine's full body (`cmd_mix`, `0x1ad8-0x1c1f`,
  328 B, in `re/notes/asm-town-zstats-acta.md` §66.3) **covers the address but never mentions
  the −2**. That is: the claim is plausible and nobody has read the site with that question in
  hand. No grade is assigned by resemblance to its neighbours — which is exactly how false
  grades are manufactured. What closing it needs: re-read `0x1b13` and the quantity selector,
  checking what they do with −2.
- **`resolve_command_char` can return a monster as a roster index**
  (`ULTIMA.EXE:0x4988`); two of its three relatives do check.
  *Grade* (PROPOSED on 2026-08-10 from the cited evidence): **DERIVED**, over a MEASURED body.
  The record declares "Body read IN FULL `ULTIMA.EXE:0x4988`-`0x4a83` (252 B, `ret` with no
  arguments)", and this bullet's claim is written by that record itself with its grade already
  attached: "**Consequence derived from the ledger layout**: if `g_cmb_actor` points at a
  MONSTER, its +2 field lacks the `0x80` bit and its +3 field is not a roster slot.
  `resolve_command_char` returns it as a party member index all the same"
  (`re/notes/resolve-command-char-178c-acta.md`). So: the body is measured and the consequence
  is a derivation over the layout, and the row takes the grade of the WEAKER of the two, which
  is what the row asserts. **No WITNESS.**
- **The same distance, measured two ways in the same file — the proximity cue is lost at the
  world seam** (`MAINOUT.OVL:0x007a`). Two neighbouring routines compute "distance to the
  party" under different rules:
  - `object_proximity_activate` (whole body read: 96 B, 43 instructions, `ret` with no
    immediate) checks **a single object** —`si = 0x5c62` is a constant, never iterated— under
    four conditions: non-null tile, same floor, and `|Δx| < 6` **and** `|Δy| < 6` as a **plain
    absolute value**. It is an 11×11 Chebyshev box around the party; if it passes, one `call`
    to `ULTIMA.EXE:0x3ae6` and nothing else.
  - `pick_spawn_coords` (`MAINOUT.OVL:0x0f4e`), in the SAME file, measures that same magnitude
    **on the 8-bit torus**: its pair `|Δ| <= 6` / `|Δ| >= 0xFA` are not two thresholds, they
    are **one single condition** with the wrap included, because the coordinates are added in
    8 bits and wrap at 256.

  **Consequence if the coordinates wrap in the first one too**: an object right next to the
  party *on the other side of the seam* yields `|Δ| ≈ 250`, not `< 6`, so **the cue does not
  fire**. A failure by omission, silent, and only at the edge.

  **Grades, deliberately kept apart.** MEASURED: both bodies, the four conditions, the 11×11
  box, and that the asymmetry exists. ALSO MEASURED: this routine **consumes no RNG** — its
  only `call` is presentation. NOT ADJUDICATED: **which of the two rules is the correct one**.
  It may be that in this context the coordinates do not wrap (a different space), or it may be
  a defect of the original; what is measured is the discrepancy, not the verdict. There is a
  **third site** in the kernel with the same 8-bit torus idiom, and there it is used
  **correctly** — which establishes that the idiom is deliberate house style, but does **not**
  settle this site.

  **REACHABILITY: REACHABLE.** Two pieces, with different grades:
  - **Piece 1, MEASURED** over `game/assets/maps/overworld.json` (256×256). In the ±6-cell strip
    on each side of the seam — exactly the reach of the 11×11 box — navigability is nearly
    total: **3039 of 3072** cells navigable in the vertical strip (98.9 %) and **3006 of 3072**
    in the horizontal one (97.9 %). The seam **is not an unreachable zone: it is open sea**, and
    the party crosses it as soon as it sails. The 15 and 26 cells walkable on foot are islets,
    which additionally make the land case reachable.
  - **Piece 2, via TWO INDEPENDENT ROUTES.** For the defect to show, the actor must end up **on
    the other side** of the seam — and the binary produces that twice over.
    **(a) DERIVED:** the spawn draw builds the coordinate by adding in 8 bits, i.e. with wrap,
    and explicitly contemplates the "near from the other side" case in order to reject it (its
    comparison against `0xfa`); a draw that knows how to wrap places on the other side.
    **(b) MEASURED:** the mover `MAINOUT.OVL:0x1578` adds in 16 bits and **stores truncated**
    (`0x16c7 mov al,[bp-6]` / `0x16ca mov [si+0x5c5c],al`, no clamp, no check), so the stored
    coordinate wraps modulo 256. Any actor moved by that routine can end up on the other side.

  **NO LIVE WITNESS**: nobody has seen it happen with the binary running. The exact formula this
  row upholds is **mechanism measured, reachability measured and derived, no witness**.

  **Honest loose end, travelling with the row:** the identity of the occupant of slot 1 comes
  from the port's comment and from reading two other consumers, **not from reading the
  assigner**. Should that slot turn out to be reserved for something that never approaches the
  seam, piece 2 falls and the verdict returns to "theoretical". It is written down so the thread
  can be pulled without redoing everything.

  *Port:* NOT MODELLED — there is no per-OBJECT proximity hook in the clone (verified
  2026-08-06). ⚠ And there is a look-alike that misleads: the clone **does** have proximity,
  but **per TILE** and from another routine (`ambient_sfx_tick 0x4102` →
  `game/src/core/sfx.ts:301`, `game/src/skin/coreview.ts:780`). That is not this one. Whoever
  ports it inherits the whole problem: **implementing either rule without adjudicating first
  is choosing blind.**
- **The weighted creature roll never checks its table's length** (`MAINOUT.OVL:0x0e04`,
  `weighted_pick`, 74 B, whole body read). It rolls `rand_range(0,255)` **once** and walks
  the weight table subtracting while `weight <= remainder`, returning the first index whose
  weight is strictly greater. **The loop compares against no length whatsoever**
  (0x0e33-0x0e3d: `mov al,[bx+si]` / `cmp ax,dx` / `jbe`, with no bound counter): if the
  weights summed to less than 256, the walk would run off the end of the table and return a
  made-up index. Correctness does not live in the code — it lives in the data. Same family
  as the root of TALK: routines that trust their input to be perfect.
  🔴 **And the terminator is not a safety net.** A weight of **0** never stops the loop: the
  continue condition is `weight <= remainder`, and `0 <= remainder` always holds. The `0x00`
  bytes closing two of the tables would be **walked straight past**, not treated as a cap —
  the only thing preventing the overrun is the exact sum.
  *Reachability:* **MEASURED AND NEGATIVE.** The four tables its single caller passes
  (`tile_to_monster`, `MAINOUT.OVL:0x0e4e`, at 0x0eb8/0x0eca/0x0f2c/0x0f3c, pointers DS
  0x2BDC · 0x2BE8 · 0x2BF0 · 0x2BF6) each sum to **exactly 256**, read byte by byte from
  DATA.OVL (fileoff = DS+0x10): 12 weights `60+50+40+30+20+15+15+10+10+3+2+1`, 7
  `64+56+56+32+32+8+8`, 5 `72+72+40+38+34` and 2 `128+128`. And each table's length is
  exactly the gap to the next pointer, which is the control that bounds them without
  assuming them. With the roll capped at 255 the index **always** lands inside, and the two
  `0x00` terminators are never reached. ⇒ with factory data the defect has no live path. What
  holds it is the DATA, not a guard — one edit lowering a weight wakes it up.
  *Port:* **does NOT copy it: it adds the bound the binary lacks**
  (`game/src/core/combat/encounters.ts:170`, `while (i < weights.length && weights[i] <= roll)`).
  A declared divergence, and **not observable** with the factory tables precisely because the
  binary never reaches the edge. The four tables travel byte-dumped in that same file
  (`SPAWN_TABLES`, lines 148-156) and a test blocks them from drifting off `data.json`.
  *Grade* (PROPOSED on 2026-08-10 from the cited evidence): **MEASURED**, and here the evidence
  carries the word literally. The ledger row declares "CUERPO ENTERO LEIDO 0x0e04-0x0e4e (74 B,
  35 insn, `ret 2` = ONE argument … cut VALIDATED against the `size`)" and closes with "I do not
  adjudicate whether some real table sums short —I have not censused them—; **what is MEASURED
  is that the routine does not defend itself**". Reachability was closed afterwards, in another
  body reading, measuring the four tables against the IMAGE, and that note also fixes the grade
  by equivalence: it is "the same grade card #23 gave to the `rand_range` division by zero" —
  mechanism MEASURED, reachability measured and negative. Record in prose:
  `re/notes/asm100-censo-acta.md` §58.2. **No WITNESS.**

---

## 5. What we investigated and is NOT a bug

A bug register without this section is propaganda. Everything here reached the table as a
candidate and left discarded.

### 5.1 Deceit and Despise share a slot — that is DESIGN

Eight dungeons and seven bitmap slots looks like an oversight. It is not: `DUNGEON.CBT` is
exactly 39,424 bytes = 112 records = 7×16, not 8×16. **The data file confirms there were
always seven.** It is reproduced, and it does not count as a bug.

### 5.2 Rel Hur has no bug: the bug would be ours

The wind spell remaps the direction before applying it, and that remapping is **correct in
the binary**. We note it here because anyone porting it by passing the argument through
unchanged swaps the four cardinal points in pairs — and the failure would be silent. It is
a porting hazard, not a 1988 defect.

### 5.3 The shops do not mis-compare upper case

It was written at one point that the blacksmith's menu echoes lower case while the handler
compares upper case. The measurement says otherwise: 53 comparisons against upper case and
**none** against lower case across the whole shop territory. The explanation — that the key
reader normalises — is labelled as inferred by its own author and has not been derived.
With no mechanism, there is no bug to declare.

### 5.4 Room plates that never fire are the mechanic working

In 35 of 128 dungeon rooms there are triggers whose cell is impassable in combat, so they
never activate. It sounds like broken data; it is the normal consequence of the rules — the
plate only fires on a move that succeeds, and that cell cannot be entered. OpenU5
reproduces it faithfully without doing anything special.

### 5.5 Attribute overflows are not 1988 bugs

There is a family of formulas that behaves absurdly with very high attributes: haggling
reaches a **negative price** (the shop pays you) with intelligence ≥ 67, the troll toll
**gives** you gold with strength 99, a character with dexterity 99 ends up nearly immobile
and untouchable.

**None of them is reachable in play.** The game's real attribute ceiling is 30 — the shrines
impose it, and character creation does not come close — and all these formulas port
correctly up to 30. They only bite with an edited save. They are documented as a curiosity,
with their regime declared, and **not** as defects anyone suffered.

---

## 6. Status and pending work

- **Spanish original**: this document is the English mirror of
  `docs/bugs-del-original.md`, which is the source.
- **Public site assembly**: the register travels through **two** mechanisms that are both
  required. The `copy docs/bugs-del-original.md` line puts the **Spanish** version into the
  public tree, and the `re-en` block **overwrites it** with this English one. Neither alone
  is enough: without the `copy` the mirror has nothing to replace; without the `re-en` case
  the register would be published in Spanish. Note that `docs/publicacion/` is excluded from
  the public tree **by design** — a document meant to be published cannot live there.
- **Oracle witnesses**: §1.3 **now has one** — observed in the binary running under DOSBox,
  with the prediction registered beforehand and a negative control (see its entry). It is so
  far the register's only entry graded WITNESSED. §1.2 still lacks one: the gap in the
  sentence is not readable by the cheap route, because the harness's screen reader only works
  in text mode and the game runs in graphics; it would need an execution breakpoint inside a
  tavern purchase with a single member.
