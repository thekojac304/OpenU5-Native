#include "openu5/magic.h"
#include <algorithm>
namespace openu5 {
namespace {
#include "magic_tables.inc"
int r30(Rand r) { return std::max<int32_t>(1, r(0, 60) >> 1); }
} // namespace
const SpellDef *spell_definition(SpellId id) {
    return unsigned(id) < 49 ? &kSpells[unsigned(id)] : nullptr;
}
const char *spell_effect_summary(SpellId id) {
    static constexpr const char *kSummaries[48] = {
        "Light / night vision", "Magic missile", "Awakens a companion", "Cures poison",
        "Heals a companion", "Teleports a map creature", "Unlocks or disarms", "Repels a creature",
        "Changes the wind", "View a gem map", "Summons a creature", "Creates food",
        "Strong light / night vision", "Powerful fire attack", "Creates a fire field", "Creates a poison field",
        "Creates a sleep field", "Short-range blink", "Dispels a field", "Protects from sleep",
        "Creates an energy field", "Climb up one dungeon level", "Descend one dungeon level", "Reveals a creature",
        "Summons insect swarms", "Seals a door", "Animates an object", "Fully heals a companion",
        "Makes a creature invisible", "Speeds a companion", "Earthquake attack", "Negates magic",
        "Shows nearby deaths", "Reveals the surrounding map", "Charms a creature", "Polymorphs a creature",
        "Protects a companion", "Deadly map attack", "Creates an illusion", "Views a gem map",
        "Fear attack", "Puts creatures to sleep", "Resurrects a companion", "Summons a daemon",
        "Death bolt", "Fire bolt", "Moonstone gate travel", "Slows a creature"
    };
    return unsigned(id) < 48 ? kSummaries[unsigned(id)] : nullptr;
}
const char *spell_target_label(SpellId id) {
    static constexpr const char *kTargets[48] = {
        "World", "Direction", "Party member", "Party member", "Party member", "Direction",
        "Direction", "Direction", "World", "World", "World", "Party", "World", "Direction",
        "Area", "Area", "Area", "Direction", "Direction", "Self", "Area", "Dungeon",
        "Dungeon", "Direction", "Area", "Direction", "Direction", "Party member", "Direction",
        "Self", "Area", "World", "World", "World", "Direction", "Self", "Self", "Area",
        "Direction", "World", "Direction", "World", "Party member", "World", "Direction", "Direction",
        "World", "Direction"
    };
    return unsigned(id) < 48 ? kTargets[unsigned(id)] : nullptr;
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
