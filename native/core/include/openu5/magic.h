#pragma once
#include "turn.h"
namespace openu5 {
enum class SpellId : uint8_t {
    InLor,
    GravPor,
    AnZu,
    AnNox,
    Mani,
    AnYlem,
    AnSanct,
    AnXenCorp,
    RelHur,
    InWis,
    KalXen,
    InXenMani,
    VasLor,
    VasFlam,
    InFlamGrav,
    InNoxGrav,
    InZuGrav,
    InPor,
    AnGrav,
    InSanct,
    InSanctGrav,
    UusPor,
    DesPor,
    WisQuas,
    InBetXen,
    AnExPor,
    InExPor,
    VasMani,
    InZu,
    RelTym,
    InVasPorYlem,
    QuasAnWis,
    InAn,
    WisAnYlem,
    AnXenEx,
    RelXenBet,
    SanctLor,
    XenCorp,
    InQuasXen,
    InQuasWis,
    InNoxHur,
    InQuasCorp,
    InManiCorp,
    KalXenCorp,
    InVasGravCorp,
    InFlamHur,
    VasRelPor,
    AnTym,
    Nox
};
struct SpellDef {
    const char *key, *name, *target_type, *type, *time_permitted;
    uint8_t circle, reagents, time_bits;
};
const SpellDef *spell_definition(SpellId);
// Compact player-facing copy derived from the same SpellEffect table used to
// execute the spell.  These are deliberately not translations of spell words.
const char *spell_effect_summary(SpellId);
const char *spell_target_label(SpellId);
enum class MagicEffect : uint8_t {
    None,
    Light,
    Attack,
    Awaken,
    Cure,
    Mani,
    FullHeal,
    Poof,
    Disarm,
    Repel,
    Wind,
    Peer,
    Summon,
    Food,
    Field,
    Blink,
    Dispel,
    TimeStatus,
    Ascend,
    Descend,
    Reveal,
    Swarms,
    Seal,
    Animation,
    Line,
    Quake,
    DeathVision,
    Charm,
    Polymorph,
    Invisible,
    Fear,
    Resurrect,
    Daemon,
    Gate,
    Illusion
};
struct SpellEffect {
    MagicEffect kind = MagicEffect::None;
    int16_t value = 0, extra = 0;
};
struct CastResult {
    bool ok = false, consumed = false;
    const char *message = "";
    SpellEffect effect{};
};
struct CastContext {
    int32_t location = 0;
    bool combat = false;
    int8_t absorbed = -1;
    uint8_t wind_arrow = 0;
};
// The caller supplies the active RNG stream; combat must use CombatState.rng.
CastResult cast_spell(GameState &, TurnState &, CharacterState &, SpellId, CastContext, Rand);
bool mix_spell(GameState &, SpellId, int32_t quantity);
bool apply_target_spell(CharacterState &, MagicEffect, uint8_t karma, Rand);
} // namespace openu5
