# Platform-independent shop commerce

The repository TypeScript is the authority. This module translates all eight
merchant categories and the commerce/state portions of their callers. It does
not implement menus, keyboard handling, prose composition, translation, sound,
rendering, hardware controls or filesystem access.

## Source map

| Reference | Native owner |
| --- | --- |
| `core/shops/shops.ts`: shopBuyPrice/shopSellPrice and price wrappers | `shops.cpp`: `shop_buy_price`, `shop_sell_price`, guild/ration/horse/ship/wine/inn prices |
| blacksmithStock, reagentPriceAt/reagentGrantQty, healerPrices | borrowed `ShopData`; `shop_offerings`; town-indexed healer price views |
| shoppeKeeperAt, LOCATION_NAMES, shop-tables.ts SHOP_TOWNES | normalized borrowed `ShopRecord` catalog, `shop_lookup`, `shop_town_index` |
| buyEquipment/sellEquipment/buyReagent/buyGuildItem | corresponding native transactions on the existing pack |
| buyHorse/buyShip/buyWine/buyRations/buyTavernRound/payRumor/healerHeal | corresponding native transactions and `TavernResult` |
| innAt/innGuestCount/innRest/innLeave/innPickup/innNightPass | corresponding native helpers, existing roster and `advance_clock` |
| party.ts rosterLeaveCompact/rosterPickupInsert | inn leave/pickup roster shifts; active-character asymmetry retained |
| world/shop-hours.ts shopIsOpen | `shop_is_open`, using existing `schedule_index` |
| shops.ts postPurchaseDrain; Game.shopPostPurchaseDrain | `shop_post_purchase_drain`, shared `TravelState::shadowlord_here` and live RNG |
| ui/shop-console.ts start, greetYes, menuKey, pick*/deal*/epilogue/leave | `shop_orchestration.cpp`: semantic session phases/actions and offer enumeration |
| shop-console.ts tavernRationsQty/tavernRumor/drunkGateKey | semantic UTF-16 Text / Confirm / Decline inputs, bounded rumor matching |
| shoppe-greetings.ts branch/pitch availability and random selection | generated `shop_greetings.inc`; caller supplies record availability |
| Game.findStableSpot/stableHorse/spawnDockShip/innSleepUntilMorning | shop world callbacks, exact S/N/E/W search, payment/drain/delivery/night order |
| main.ts startTalk and native dialogue Shop handoff | `begin_shop`, automatic `DialogueServices` handoff when ShopServices is attached |

Core paths above are relative to `game/src/`. Constants are generated from
unchanged TS exports by `generate-shop-tables.ts` and
`generate-shop-greetings.ts`, with CTest drift checks. No reference runtime was
modified. The existing character, pack, gold, time, RNG and world owners are reused.

## Semantic API and events

Attach caller-owned `ShopServices` / `ShopSession` to `CommandContext`.
`begin_shop` accepts a merchant NPC, applies the schedule gate before mounted
refusal, resolves its type/town and starts the session. The existing Talk and
BeginConversation commands use this automatically; without ShopServices the old
explicit deferred handoff remains available.

Use `execute_shop` with ShopAction, or CommandKind::ShopAction with
`Command.item = ShopAction`, `Command.member = item/member id`, and borrowed
UTF-16 `text/text_length`. Item inputs are **actual equipment/reagent/service
IDs**, not menu row indexes. There are no hardware keys. `shop_offerings` streams
available IDs, prices and quantities without allocation or RNG draws. Unrelated
world/dialogue commands are blocked while a shop owns input.

Continue acknowledges a pause; Confirm/Decline answer the current question.
Cancel represents closing a picker (or an empty text response), and respects
phase-specific behavior. A pending yes/no question does not acquire a new Escape
meaning. End is programmatic session teardown. In particular, the reference's
Enter behavior at a reagent list closes that list/session.

`GameEventKind::Shop` carries synchronous borrowed Entered, State, Result and
Exited metadata. State identifies an offer/list, confirmation, member selection,
quantity/text request or pause. Result preserves TxResult text/reason and service
metadata; tavern results additionally retain price-line, plate and service-count
fields. World mutations/callbacks remain ordered. Dialogue emits its existing
Handoff envelope before the shop lifecycle. Opening refusals are ordinary Message
events. A sink must consume payloads during its callback, not retain pointers.

These semantic envelopes are native API metadata, **not a claim that TypeScript
emits identically named GameEvents**. ShopConsole's message/armKey/armText,
refreshGold and picker callbacks are presentation, not the GameEvent union.
Their raw menu rows, formatted speech, prompt echoes and sound calls are excluded.
The replay compares the commerce projection: available offers, active pending
transaction, phase, flags, result, mutations, clock, RNG and world-effect order.
It does not claim byte parity of ShopConsole's rendered console transcript.
Stale private TS pending objects after a declined healer offer are not active
transactions; they are excluded from that projection.

## Preserved behavior

* Buy: `base + trunc(base*(100-3*INT)/100)`; sell: `trunc(3*INT*base/100)+1`.
  No new minimum price, karma adjustment or reputation discount is introduced.
* Equipment is bought one at a time. Arrows/quarrels fill to 99 and cannot be
  sold. Sell offers include owned ammunition and zero-base items: rejection is
  in the selection path, not a fabricated availability filter. Sales leave
  readied equipment unchanged and cap gold at 9999.
* Reagents use the city/slot's **fixed grant**, with a pre-purchase cap check.
  No reagent quantity prompt is invented. Guild lots grant 3/4/5, charging even
  when the inventory is already full. Equipment/reagent counts saturate at 99.
* Rations buy repeatedly until quantity, gold or food=9999 stops the loop.
  Zero/invalid quantity does not pay. The caller's partial-purchase branch skips
  the drain; a zero purchase with food<3 grants the actual random scraps.
* Healers distinguish the core helper's need check from the caller's live/dead
  gate, Minoc free healing/cure, Skara Brae charity, and the legacy missing-text
  path (including its distinct drain/mark behavior).
* Inn capacity precedes payment. Poisoned guests die on rest/pickup; class MP,
  roster order, months and active-character reindexing follow TS. Night runs
  twelve separate five-minute calls, then regeneration/nine-minute steps,
  including the 1000-step time-stop guard, hour-tile callbacks and live RNG.
* Tavern meals, house drinks, wines, drunkenness, rumors, repeated transactions,
  and subtype availability remain separate. Drunkenness is gated at exactly
  three prior cups, before wine selection, and declining the warning can apply
  the penalty even if the wine list is then cancelled. All 26 rumor keywords
  retain trimming, 15-code-unit slicing and first-occurrence/boundary matching.
* Falsehood drain follows **executable caller behavior**, including tavern and
  inn calls that contradict older comments. No draw occurs with another/no
  Shadowlord. Missing rumor text skips the optional-chain RNG draw.
* Paid horses require an available S/N/E/W cell before payment; ships request
  a parked object with hull 99, correct tile and skiffs. Existing world owners
  supply storage and mutations. Missing owners or failed reservation are explicit
  Unsupported/NeedsStorage outcomes before payment. No second object pool exists.
* Ordinary shop interactions consume zero world turns. Only inn night progresses
  the clock. The optional shared RNG trace observes every shop draw.

The numeric domain is the existing bounded native state: byte character stats,
word gold/food, byte equipment IDs, canonical reagent slots and a 16-record
roster. Price intermediates use int64 to preserve truncation. Invalid native
indices return InvalidInput; a malformed negative-price transaction that would
leave the existing word-sized gold domain returns NumericRange without wrapping.
This is not arbitrary-JavaScript-number compatibility. Reference fallbacks for
missing table entries remain zero where the TS uses `?? 0`; absent stock rows
remain empty, rather than accidentally offering equipment zero.

## Assets and evidence

Authoritative inputs: `original/u5/ultima5/DATA.OVL`, `SHOPPE.DAT`, and repository
`game/src/core/data/ShoppeKeeperMap.json`. The existing extractor produces
`game/assets/data.json` and `game/assets/shoppe.json`. The real-shop generator
compares every consumed data array with fresh `parseDataOvl`, and the complete
shop text pool with `parseShoppeDat` plus `extractCompressedWords`.
It fails on missing/stale assets; it never invents a shop or silently skips one.

The keeper catalog contains **46 shops**: 9 blacksmiths, 9 barkeepers, 3 horse
sellers, 4 shipwrights, 5 reagent sellers, 3 guilds, 7 healers and 6 inns.
The horse price/town table also contains Lycaeum, for which the keeper catalog
has no entry. That distinction remains intact; its price branch is covered in
the helper suite without fabricating a 47th keeper.

**202,712 helper + 584,406 session snapshots = 787,118 new;
2,029,288 cumulative parity/compatibility snapshots.**

Session routes include 63 real equipment buy offers, 432 equipment sale selection
paths (48 IDs at each blacksmith, including rejection paths), 22 reagent offers,
9 guild lots, 8 ship offers, 21 healer services and 18 inn services. Tavern tests
cover its nine locations, subtype gates, all six wines and all 26 rumor keywords;
all three catalog horses are exercised. The coverage file's 600 route labels
include attempted unavailable service choices and are **not** a count of 600
successful purchases or distinct wares. Real-data tests also exercise absent
text-pool and absent-name fallback configurations, clearly separate from asset skips.
There are **no shop asset-dependent skips**. The pre-existing missing ad01.gam
save remains missing and is not required or fabricated by this batch.

Helper replay compares repeated success/failure, exact-gold/one-short/zero-gold,
price boundary/truncation cases, all pack counts and roster/equipment state,
returned text/reasons/coordinates, service effects, time and RNG. Session replay
executes actual ShopConsole methods and Game horse/dock/night methods through a
headless presentation adapter, then compares after each semantic action. Native
contract assertions separately cover dialogue-event order, input ownership,
invalid IDs/context, missing stock and storage reservation.

## Memory and deferred work

ShopSession is **56 B host / 44 B ESP**, caller-owned; GameState remains
**2232 B (+0)**. The implementation uses no heap, no event queue and no copied
shop database. Text input is a borrowed view; rumor matching uses a **30 B**
fixed buffer. Offer enumeration streams one record. Inn compaction uses a
34-byte CharacterState copy. Stack-frame measurements and complete validation
are recorded in VALIDATION.md. Borrowed table/catalog/text availability data
must outlive the session; original shop assets can remain SD-backed behind a
future asset owner rather than becoming copied session state.

Session state and RNG are not added to save formats. Gold, inventory, roster,
time and parked-world-object changes use existing persistence-compatible owners;
the application still owns capture/restore and world-owner callbacks.

Deferred: full quests/progression and their generic service hooks, original
console transcript/presentation adapters, full command/UI integration, general
non-foot travel, T-Deck controls, FATFS persistence wiring, rendering and audio.
There is no invented training/guild progression service: this reference guild
sells keys, gems and torches. Ship replacement pricing is a constant/comment in
the reference, not an executable purchase path, and is not invented here.
No additional commerce rule blocks starting the separate quest/progression
translation; world/service ownership and UI/storage integration remain explicit.

## Reproduction

From the repository root, generate assets with the existing `npm run extract --
--skip-tiles` when needed, then:

```powershell
node --import tsx native/core/tools/generate-shop-tables.ts
node --import tsx native/core/tools/generate-shop-greetings.ts
node --import tsx native/core/tools/generate-shop-fixtures.ts
node --import tsx native/core/tools/generate-shop-flow-fixtures.ts
node node_modules/typescript/bin/tsc -p native/core/tools/tsconfig.json
cmake --build native/core/build-zig
ctest --test-dir native/core/build-zig -j 4 --output-on-failure
python native/core/tools/prepare-shop-baseline.py
powershell -File native/core/tools/validate-shops-esp.ps1
```

Fresh checkouts also need the earlier generated transports/loot/dialogue fixtures
listed in README.md. The baseline preparer archives HEAD into an ignored local
folder; run it before committing this batch to compare against the preceding
revision. Validation never flashes, resets or otherwise operates the device.
