#include "openu5/magic.h"
#include <algorithm>
#include <cstring>
namespace openu5 {
namespace {
#include "magic_tables.inc"
int r30(Rand r) { return std::max<int32_t>(1, r(0, 60) >> 1); }
} // namespace
const SpellDef *spell_definition(SpellId id) {
    return unsigned(id) < 49 ? &kSpells[unsigned(id)] : nullptr;
}
const char *spell_effect_summary(SpellId id) {
    // Batch 8 / audit R-16 adjudication: nine entries below were hand-typed
    // (commit c18f5b64) without cross-checking MagicDefinitions.json and drifted
    // to describe a DIFFERENT spell's effect. Corrected against
    // MagicDefinitions.json::SimpleDescription, cross-checked where the JSON's
    // own accuracy was in question (In Zu -- re/notes/fx-lineaoe-negate-derivation.md
    // mode-1 "dormir"; Quas An Wis / In An -- game/src/core/magic/tables.ts
    // TIME_STATUS.confusion/negate). In Ex Por (#26) keeps its own note below --
    // its old "Animates an object" text matched the ANIMATION-ONLY belief that
    // re/notes/magic.md retracted 2026-08-07; MagicDefinitions.json's
    // "unlocks magical locks" is the corrected, authoritative claim there, not
    // a stale one. See GAMEPLAY_INTEGRATION_AUDIT.md R-16 for the full table.
    static constexpr const char *kSummaries[48] = {
        "Light / night vision", "Magic missile", "Awakens a companion", "Cures poison",
        "Heals a companion", "Makes objects vanish", "Unlocks or disarms", "Repels a creature",
        "Changes the wind", "Reveals caster's location", "Summons a creature", "Creates food",
        "Strong light / night vision", "Powerful fire attack", "Creates a fire field", "Creates a poison field",
        "Creates a sleep field", "Short-range blink", "Dispels a field", "Protects from sleep",
        "Creates an energy field", "Climb up one dungeon level", "Descend one dungeon level", "Reveals a creature",
        "Summons insect swarms", "Seals a door", "Unlocks magical locks", "Fully heals a companion",
        "Puts enemies to sleep", "Speeds a companion", "Earthquake attack", "Charms multiple enemies",
        "Negates magic", "Reveals the surrounding map", "Charms a creature", "Polymorphs a creature",
        "Grants invisibility", "Deadly map attack", "Creates an illusion", "Views a gem map",
        "Blasts foes with poison", "Causes fear", "Resurrects a companion", "Summons a daemon",
        "Death bolt", "Fire bolt", "Moonstone gate travel", "Stops passage of time"
    };
    return unsigned(id) < 48 ? kSummaries[unsigned(id)] : nullptr;
}
const char *spell_target_label(SpellId id) {
    // Batch 8 / audit R-16: An Tym (#47) was the sole confirmed kTargets defect
    // -- "Direction" left over from a hand-typed table (commit c18f5b64) even
    // though An Tym's target_type is noSelection and its TimeStatus effect (a
    // global time-stop, re/notes/fx-lineaoe-negate-derivation.md SS2) never
    // consumes a direction, exactly like its noSelection siblings Quas An Wis
    // and In An, both already "World". The other 11 spells the audit flagged
    // (5,6,7,13,18,23,25,26,28,34,38) are NOT defects: their raw target_type
    // string is the unreliable one (see the cast_target_prompt comment below),
    // and "Direction" correctly names the real input -- a combat-reticle aim or
    // a world/line getdir, both direction-driven -- that cast_target_prompt()
    // and the line-spell getdir (re/notes/fx-lineaoe-negate-derivation.md SS1)
    // already implement for them.
    static constexpr const char *kTargets[48] = {
        "World", "Direction", "Party member", "Party member", "Party member", "Direction",
        "Direction", "Direction", "World", "World", "World", "Party", "World", "Direction",
        "Area", "Area", "Area", "Direction", "Direction", "Self", "Area", "Dungeon",
        "Dungeon", "Direction", "Area", "Direction", "Direction", "Party member", "Direction",
        "Self", "Area", "World", "World", "World", "Direction", "Self", "Self", "Area",
        "Direction", "World", "Direction", "World", "Party member", "World", "Direction", "Direction",
        "World", "World"
    };
    return unsigned(id) < 48 ? kTargets[unsigned(id)] : nullptr;
}
// Batch 5 seam (audit R-11).  The decision AlphaRuntime::cast_selected_spell
// makes about which prompt a (C)ast owes the player, extracted so the host
// suite can drive it: cast_selected_spell lives behind ESP-IDF headers (audit
// Y-05), so the Batch 3 precedent (openu5::usable_item_picker_rows) applies --
// extract the decision, test it on the host, let the device call it.
//
// COMBAT is the port's existing rule, byte for byte: any target_type naming a
// map position, a map unit or a direction opens the aim reticle.
//
// WORLD keys on the EFFECT, not on target_type.  target_type is unreliable
// here (audit R-16: spell_target_label contradicts it for 12 of 48 spells) and
// it over-selects: An Ylem (Poof) and An Grav (Dispel) carry a selectedMapUnit
// target_type yet have NO world effect at all.  The reference's world
// dispatcher (game/src/main.ts, doCast) arms a getdir for exactly three effect
// descriptors -- sealDoor (An Ex Por), disarmOrOpen (An Sanct) and blink
// (In Por) -- and falls off the end of its else-if chain for An Ylem/An Grav,
// consuming the charge and doing nothing without ever prompting.
//
// In Ex Por (Unlock, formerly believed Animation/no-op) is NOT a fourth member
// of that "no effect" group -- re/notes/magic.md's 2026-08-07 correction
// proved its world branch (CAST:0x1026) calls the same magic_door_open_worker
// as the Skull Key, so the reference itself is stale here (game/src/core/magic/
// cast.ts case 26 still returns castAnimOnly and is tracked separately,
// content-audit.md PENDIENTE(3)).  Batch 8B wired the real getdir + tile
// mutation natively: Unlock now joins Seal/Disarm/Blink below, and
// world_magic.cpp's Unlock branch (sibling to its Seal/Disarm block) applies
// the identical 0x97/0x98 -> 0xB8/0xBA transform the Skull Key already uses
// (commands.cpp, CommandKind::UseItem case 17), gated by the same pre-flight
// mutable-terrain guard extended to item 26.
//
// UNDERGROUND there is no getdir at all.  The reference's doDungeonCast
// resolves An Sanct against the party's dungeon FACING (applyAnSanctOpenChest)
// and has no seal or blink branch, and the dungeon command path never reaches
// world_magic.
CastTargetPrompt cast_target_prompt(SpellId id, bool in_combat, bool in_dungeon) {
    const auto *d = spell_definition(id);
    if (!d)
        return CastTargetPrompt::None;
    if (in_combat) {
        const char *target = d->target_type ? d->target_type : "";
        return std::strstr(target, "MapPosition") || std::strstr(target, "MapUnit") ||
                       std::strcmp(target, "direction") == 0
                   ? CastTargetPrompt::CombatReticle
                   : CastTargetPrompt::None;
    }
    if (in_dungeon)
        return CastTargetPrompt::None;
    switch (kEffects[unsigned(id)].kind) {
    case MagicEffect::Seal:
    case MagicEffect::Disarm:
    case MagicEffect::Blink:
    case MagicEffect::Unlock:
        return CastTargetPrompt::WorldDirection;
    default:
        return CastTargetPrompt::None;
    }
}
bool mix_spell(GameState &g, SpellId id, int32_t qty) {
    const auto *d = spell_definition(id);
    if (!d || unsigned(id) > 47 || qty <= 0)
        return false;
    for (int i = 0; i < 8; ++i)
        if ((d->reagents & (1 << i)) && g.reagent_quantities[i] < qty)
            return false;
    for (int i = 0; i < 8; ++i)
        if (d->reagents & (1 << i))
            g.reagent_quantities[i] -= qty;
    g.spell_quantities[unsigned(id)] =
        int32_t(std::min<int64_t>(99, int64_t(g.spell_quantities[unsigned(id)]) + qty));
    return true;
}
CastResult cast_spell(GameState &g, TurnState &t, CharacterState &caster, SpellId id,
                      CastContext ctx, Rand r) {
    const auto *d = spell_definition(id);
    if (!d)
        return {false, false, "No effect!", {}};
    bool absorbed = ctx.absorbed >= 0
                        ? ctx.absorbed != 0
                        : !ctx.combat && ctx.location > 0 && ctx.location < 128 &&
                              ((ctx.location == 18 && !g.worn_crown) || ctx.location == 29);
    if (absorbed)
        return {false, false, "Absorbed!", {}};
    int bit = ctx.combat || ctx.location >= 128 ? 1
              : ctx.location == 0               ? 8
              : ctx.location <= 32              ? 4
                                                : 2;
    if (!(d->time_bits & bit))
        return {false, false, "Not here!", {}};
    auto &q = g.spell_quantities[unsigned(id)];
    if (q <= 0)
        return {false, false, "None mixed!", {}};
    --q;
    if (caster.current_mp < d->circle)
        return {false, true, "M.P. too low!", {}};
    caster.current_mp = uint8_t(caster.current_mp - d->circle);
    if (caster.level < d->circle)
        return {false, true, "", {}};
    SpellEffect fx = kEffects[unsigned(id)];
    switch (fx.kind) {
    case MagicEffect::Light:
        t.light_spell_minutes = fx.value;
        break;
    case MagicEffect::TimeStatus:
        t.time_spell = char(fx.value);
        t.spell_turns = fx.extra;
        break;
    case MagicEffect::Wind: {
        constexpr int winds[] = {0, 4, 3, 1, 2};
        fx.value = int16_t(ctx.wind_arrow < 5 ? winds[ctx.wind_arrow] : 0);
        t.wind = fx.value;
        t.wind_drift_counter = 0;
        break;
    }
    case MagicEffect::Food:
        fx.value = int16_t(r(1, 3));
        g.food = uint16_t(std::min<int32_t>(9999, g.food + fx.value));
        break;
    case MagicEffect::Summon: {
        int v = r(0, 15);
        fx.value = int16_t(v < 6 ? 20 : v < 11 ? 22 : v < 14 ? 21 : 34);
        break;
    }
    default:
        break;
    }
    return {true, true, "", fx};
}
bool apply_target_spell(CharacterState &p, MagicEffect kind, uint8_t karma, Rand r) {
    switch (kind) {
    case MagicEffect::Mani: {
        if (p.status == 'D')
            return false;
        int old = p.current_hp;
        p.current_hp = uint16_t(std::min<int32_t>(p.max_hp, p.current_hp + r30(r)));
        return p.current_hp > old;
    }
    case MagicEffect::FullHeal:
        if (p.status == 'D')
            return false;
        p.current_hp = p.max_hp;
        return true;
    case MagicEffect::Cure:
        if (p.status != 'P')
            return false;
        p.status = 'G';
        return true;
    case MagicEffect::Awaken:
        if (p.status != 'S')
            return false;
        p.status = 'G';
        return true;
    case MagicEffect::Resurrect: {
        if (p.status != 'D')
            return false;
        p.status = 'G';
        p.current_hp = 1;
        if (p.character_class == 'A' || p.character_class == 'M')
            p.current_mp = p.intelligence;
        else if (p.character_class == 'B')
            p.current_mp = uint8_t(p.intelligence >> 1);
        if (karma < 98)
            p.exp = uint16_t(int(p.exp) * karma / 100);
        p.level = 1;
        for (int n = p.exp / 100; n > 0; n >>= 1)
            ++p.level;
        p.max_hp = uint16_t(30 * p.level);
        return true;
    }
    default:
        return false;
    }
}
} // namespace openu5
