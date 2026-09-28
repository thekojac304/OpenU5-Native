#include "openu5/sfx_inventory.h"

#include "openu5/sfx_synth.h"

#include <cstring>

// Alpha 3 A3-03. Every site below was re-read from the shipped binaries:
// re/tools/a3_01_sound_census.py lists the calls (native/core/a3-01-sound-census.log),
// re/tools/a3_03_cue_sites.py prints the pushes and the message printed next to
// each one (native/core/a3-03-cue-sites.log). "Adjacent" below means the print
// and the speaker call sit in one basic block, print first.
namespace openu5 {

const char *sfx_status_name(SfxStatus s) {
    switch (s) {
    case SfxStatus::Implemented: return "Implemented";
    case SfxStatus::DeferredMusic: return "DeferredMusic";
    case SfxStatus::DeferredPresentation: return "DeferredPresentation";
    case SfxStatus::EvidenceUnknown: return "EvidenceUnknown";
    case SfxStatus::IntentionallySilent: return "IntentionallySilent";
    case SfxStatus::NoNativeEvent: return "NoNativeEvent";
    }
    return "invalid";
}

SfxStatus sfx_status(SfxId id) {
    switch (id) {
    case SfxId::CastSpell: case SfxId::SpellCast: case SfxId::PotionUsed: case SfxId::ScrollUsed:
        return SfxStatus::IntentionallySilent;
    case SfxId::LineSpray: case SfxId::CombatReject: case SfxId::InvalidMagic:
        return SfxStatus::EvidenceUnknown;
    case SfxId::TitleFizzle: case SfxId::TitleCrackle: case SfxId::BardSong: case SfxId::EndgameOrb:
        return SfxStatus::DeferredPresentation;
    case SfxId::None: case SfxId::Count:
        return SfxStatus::IntentionallySilent;
    default:
        return sfx_supported(id) ? SfxStatus::Implemented : SfxStatus::EvidenceUnknown;
    }
}

const char *sfx_status_note(SfxId id) {
    switch (id) {
    case SfxId::CastSpell:
        return "reference name for the 0x4368 fanfare, disputed (sfx-catalog 3.5); never emitted -- casting sounds via MagicCeremony";
    case SfxId::SpellCast: case SfxId::PotionUsed: case SfxId::ScrollUsed:
        return "marker: the MagicCeremony event that follows plays CAST2 0x0000 (time-spell)";
    case SfxId::LineSpray:
        return "CAST 0x1f60 fan (NB 0x1fbc + set_tone crackle 0x1c00 drawn from the game RNG): no native event marks the fan";
    case SfxId::CombatReject:
        return "SJOG 0x1f26 funnel (two beeps 0x1f5a/0x1f62): its callers are not mapped to native refusals";
    case SfxId::InvalidMagic:
        return "spell failures glide per spell (CAST 0x0eb2/0x11d3/0x1325/0x1ba7); none maps to the native generic failure";
    case SfxId::TitleFizzle: case SfxId::TitleCrackle:
        return "EGA.DRV dissolve / subtitle crackle: the device title has no dissolve or crackle";
    case SfxId::BardSong:
        return "kernel 0x42d2 class 4: Camp keeps the sound-off lute branch (Batch 51); a bard sprite gate needs the 0xac64 layer";
    case SfxId::EndgameOrb:
        return "ENDGAME 0x078f / 0x0987: the endgame cinematic is deferred (D-54)";
    case SfxId::MoveStep: return "emitted by the core (world steps); derived from the arena's Moved event (SJOG 0x1d32)";
    case SfxId::MoveBlocked: return "emitted by the core; derived from the arena's Blocked! / All must use the same exit!";
    case SfxId::CombatHit: case SfxId::CombatHitHeavy:
        return "derived from Combat Attacked by the target's side";
    case SfxId::CombatDefeat:
        return "the reference's name for the 0x2fd0 chest-trap burst (= dungeon-trap); never emitted: a kill sounds only 0x3564's hit burst (A3-05)";
    case SfxId::TimeSpell: return "derived from MagicCeremony(index)";
    case SfxId::VictoryFanfare: return "derived from the latch's VICTORY! (not the Ended line); emitted by the shard ritual";
    case SfxId::AmbientFountain: case SfxId::AmbientWaterfall: case SfxId::AmbientClockTick:
    case SfxId::AmbientClockTock: case SfxId::AmbientClockChime:
        return "the runtime's ambient ticker (ambient_sfx.h), one tick per 55 ms while the original would wait in 0x266c";
    case SfxId::IntroThunder: case SfxId::IntroChime: case SfxId::IntroSummon:
        return "derived from the device's IntroViewFrame flags as each frame is shown";
    case SfxId::ShopTransaction: return "derived from a successful healer Shop result";
    case SfxId::RefugeSlumber: case SfxId::RefugeRevival: case SfxId::RefugeThunder: return "a Refuge beat cue";
    case SfxId::BlackthornMaterialize: case SfxId::ShardSweep: return "a Blackthorn beat cue / emitted by the shard ritual";
    case SfxId::DiagnosticTone: return "Developer > Diagnostics";
    default:
        return sfx_supported(id) ? "emitted by the core or derived from its message" : "no program";
    }
}

namespace {
using S = SfxStatus;
using I = SfxId;
constexpr bool C = true, H = false; // census / found by hand

// One row per call. Order: the A3-01 census (kernel, then overlays A..Z),
// then the calls the linear census misses.
constexpr SfxSite kSites[] = {
    // -- ULTIMA.EXE (kernel)
    {"ULTIMA.EXE", 0x22cd, "SET_TONE", I::None, S::Implemented, C, "beep 0x22c0's own set_tone"},
    {"ULTIMA.EXE", 0x22da, "SPK_STOP", I::None, S::Implemented, C, "beep 0x22c0's own stop"},
    {"ULTIMA.EXE", 0x2a68, "NOISE_BURST", I::CombatDamage, S::Implemented, C, "party_member_take_damage 0x2a52"},
    {"ULTIMA.EXE", 0x2fe3, "NOISE_BURST", I::DungeonTrap, S::Implemented, C, "chest_trap 0x2fd0 (SJOG 0x1222/0x1323, CMDS 0x1c04); not a combat death (A3-05)", I::CombatDefeat},
    {"ULTIMA.EXE", 0x30b8, "SET_TONE", I::Quake, S::Implemented, C, "screen_shake_rumble 0x3072 pass 1 (rand 0x13..0x96)", I::RefugeThunder},
    {"ULTIMA.EXE", 0x30ec, "SET_TONE", I::Quake, S::Implemented, C, "screen_shake_rumble pass 2"},
    {"ULTIMA.EXE", 0x311d, "SET_TONE", I::Quake, S::Implemented, C, "screen_shake_rumble pass 3"},
    {"ULTIMA.EXE", 0x3150, "SET_TONE", I::Quake, S::Implemented, C, "screen_shake_rumble pass 4"},
    {"ULTIMA.EXE", 0x316e, "SPK_STOP", I::Quake, S::Implemented, C, "screen_shake_rumble end"},
    {"ULTIMA.EXE", 0x355a, "NOISE_BURST", I::None, S::EvidenceUnknown, C, "0x350a impact on a party-relative cell: callers not mapped"},
    {"ULTIMA.EXE", 0x35c9, "NOISE_BURST", I::CombatHitHeavy, S::Implemented, C, "0x3564 hit flash, target a party member"},
    {"ULTIMA.EXE", 0x35de, "NOISE_BURST", I::CombatHit, S::Implemented, C, "0x3564 hit flash, target an enemy"},
    {"ULTIMA.EXE", 0x428b, "TONE_SWEEP", I::AmbientClockChime, S::Implemented, C, "ambient class 1, [0x5884] != 0"},
    {"ULTIMA.EXE", 0x42a1, "BEEP", I::AmbientClockTick, S::Implemented, C, "ambient class 1 phase 0 (phase 4 enters at 0x429c)"},
    {"ULTIMA.EXE", 0x42be, "NOISE_BURST", I::AmbientWaterfall, S::Implemented, C, "ambient class 2 (class 3 enters at 0x42bd)"},
    {"ULTIMA.EXE", 0x42fb, "TONE_SWEEP", I::BardSong, S::DeferredPresentation, C, "ambient class 4: a bard sprite in view"},
    {"ULTIMA.EXE", 0x434a, "NOISE_BURST", I::MoveStep, S::Implemented, C, "sfx_footstep 0x433e burst 1"},
    {"ULTIMA.EXE", 0x4364, "NOISE_BURST", I::MoveStep, S::Implemented, C, "sfx_footstep 0x433e burst 2"},
    {"ULTIMA.EXE", 0x438b, "TONE_SWEEP", I::VictoryFanfare, S::Implemented, C, "sfx_victory_fanfare 0x4368, x3"},
    {"ULTIMA.EXE", 0x43a5, "TONE_SWEEP", I::VictoryFanfare, S::Implemented, C, "sfx_victory_fanfare 0x4368, last"},
    {"ULTIMA.EXE", 0x43d9, "SET_TONE", I::None, S::Implemented, C, "glide 0x43ae's own set_tone"},
    {"ULTIMA.EXE", 0x43f7, "SPK_STOP", I::None, S::Implemented, C, "glide 0x43ae's own stop"},
    {"ULTIMA.EXE", 0x48e5, "TONE_SWEEP", I::Moongate, S::Implemented, C, "tile under the party is 0xdc"},
    {"ULTIMA.EXE", 0x6221, "TONE_SWEEP", I::SceptreReclaimed, S::Implemented, C, "adjacent 'The Sceptre is reclaimed!'"},
    {"ULTIMA.EXE", 0x6a3c, "GLIDE", I::RingVanishes, S::Implemented, C, "adjacent 'A ring has vanished!'"},
    // -- BLCKTHRN.OVL
    {"BLCKTHRN.OVL", 0x03e3, "TONE_SWEEP", I::ShardSweep, S::Implemented, C, "sacrifice siren, rising 460 calls"},
    {"BLCKTHRN.OVL", 0x0405, "TONE_SWEEP", I::ShardSweep, S::Implemented, C, "sacrifice siren, falling 460 calls"},
    {"BLCKTHRN.OVL", 0x083f, "TONE_SWEEP", I::BlackthornMaterialize, S::Implemented, C, "Blackthorn materializes"},
    {"BLCKTHRN.OVL", 0x0a34, "TONE_SWEEP", I::RefugeSlumber, S::Implemented, C, "Refuge: 6 notes after 'But thy slumber is disturbed!'"},
    {"BLCKTHRN.OVL", 0x0b8d, "TONE_SWEEP", I::RefugeRevival, S::Implemented, C, "Refuge: one tone per revived member"},
    // -- CAST.OVL
    {"CAST.OVL", 0x029a, "GLIDE", I::None, S::EvidenceUnknown, C, "after a redraw; owning spell not mapped"},
    {"CAST.OVL", 0x0d85, "TONE_SWEEP", I::SpellZap, S::NoNativeEvent, C, "adjacent 'Magic absorbed!' (tile 0xfc): not printed natively"},
    {"CAST.OVL", 0x0e6e, "TONE_SWEEP", I::SpellZap, S::Implemented, C, "adjacent 'Absorbed!'"},
    {"CAST.OVL", 0x0eb2, "GLIDE", I::None, S::EvidenceUnknown, C, "adjacent 'Not here!' of one spell: not mapped"},
    {"CAST.OVL", 0x11d3, "GLIDE", I::None, S::EvidenceUnknown, C, "adjacent 'Failed!' of one spell: not mapped"},
    {"CAST.OVL", 0x1325, "GLIDE", I::None, S::EvidenceUnknown, C, "adjacent 'No effect!' (locations 0x1d/0x28): not mapped"},
    {"CAST.OVL", 0x15f0, "TONE_SWEEP", I::ShardSweep, S::Implemented, C, "shard ritual, rising 460 calls"},
    {"CAST.OVL", 0x1620, "TONE_SWEEP", I::ShardSweep, S::Implemented, C, "shard ritual, falling 460 calls"},
    {"CAST.OVL", 0x166d, "GLIDE", I::ShardNoEffect, S::Implemented, C, "adjacent 'No effect!' (the wrong flame)"},
    {"CAST.OVL", 0x198f, "TONE_SWEEP", I::Sceptre, S::Implemented, C, "after 'Wielding the Sceptre of Lord British...'"},
    {"CAST.OVL", 0x19e0, "NOISE_BURST", I::Sceptre, S::Implemented, C, "one per dissolved field"},
    {"CAST.OVL", 0x1ba7, "GLIDE", I::None, S::EvidenceUnknown, C, "adjacent 'Failed!' of one spell: not mapped"},
    {"CAST.OVL", 0x1c00, "SET_TONE", I::LineSpray, S::EvidenceUnknown, C, "line-spell crackle, pitch from the game RNG"},
    {"CAST.OVL", 0x1f51, "SPK_STOP", I::LineSpray, S::EvidenceUnknown, C, "line-spell crackle end"},
    {"CAST.OVL", 0x1fbc, "NOISE_BURST", I::LineSpray, S::EvidenceUnknown, C, "line-spell fan lead"},
    // -- CAST2.OVL
    {"CAST2.OVL", 0x001d, "NOISE_BURST", I::TimeSpell, S::Implemented, C, "ceremony 0x0000 lead"},
    {"CAST2.OVL", 0x0056, "TONE_SWEEP", I::TimeSpell, S::Implemented, C, "ceremony rising sweep"},
    {"CAST2.OVL", 0x006d, "TONE_SWEEP", I::TimeSpell, S::Implemented, C, "ceremony falling sweep"},
    {"CAST2.OVL", 0x0094, "TONE_SWEEP", I::None, S::EvidenceUnknown, C, "no message adjacent; owner not mapped"},
    {"CAST2.OVL", 0x0560, "TONE_SWEEP", I::None, S::EvidenceUnknown, C, "no message adjacent; owner not mapped"},
    {"CAST2.OVL", 0x0aed, "TONE_SWEEP", I::ShrineOrdained, S::Implemented, C, "ORDAINED 7-note table melody"},
    {"CAST2.OVL", 0x0be3, "TONE_SWEEP", I::ShrineDonation, S::Implemented, C, "ALAKAZAM rising 460 calls"},
    {"CAST2.OVL", 0x0c05, "TONE_SWEEP", I::ShrineDonation, S::Implemented, C, "ALAKAZAM falling 460 calls"},
    {"CAST2.OVL", 0x0c57, "TONE_SWEEP", I::ShrineWellDone, S::Implemented, C, "WELL DONE rising 460 calls"},
    {"CAST2.OVL", 0x0c79, "TONE_SWEEP", I::ShrineWellDone, S::Implemented, C, "WELL DONE falling 460 calls"},
    // -- CMDS.OVL
    {"CMDS.OVL", 0x09d5, "GLIDE", I::CannonFire, S::Implemented, C, "broadside"},
    {"CMDS.OVL", 0x0c05, "GLIDE", I::CannonFire, S::Implemented, C, "adjacent 'BOOOM!' (cannon on foot)"},
    {"CMDS.OVL", 0x11b2, "TONE_SWEEP", I::None, S::NoNativeEvent, C, "adjacent 'A shadowlord appears': not printed natively"},
    {"CMDS.OVL", 0x18ac, "GLIDE", I::CombatEscape, S::Implemented, C, "escape handler 0x17ec success"},
    // -- COMBAT.OVL
    {"COMBAT.OVL", 0x01b2, "GLIDE", I::None, S::EvidenceUnknown, C, "750->400 glide, no message: not mapped"},
    {"COMBAT.OVL", 0x033c, "GLIDE", I::None, S::EvidenceUnknown, C, "750->400 glide, no message: not mapped"},
    {"COMBAT.OVL", 0x03b6, "GLIDE", I::CombatFoodStolen, S::Implemented, C, "adjacent ' stole some food!'"},
    {"COMBAT.OVL", 0x04f6, "GLIDE", I::None, S::EvidenceUnknown, C, "1200->2000 glide, no message: not mapped"},
    {"COMBAT.OVL", 0x07f1, "NOISE_BURST", I::CombatEngulfed, S::Implemented, C, "adjacent 'ARGH!' (a dragged member's turn)"},
    {"COMBAT.OVL", 0x0958, "TONE_SWEEP", I::SpellZap, S::Implemented, C, "adjacent 'Absorbed!' (location 0x12, no crown)"},
    {"COMBAT.OVL", 0x1cbf, "NOISE_BURST", I::CombatRegurgitated, S::Implemented, C, "adjacent ' regurgitated!'"},
    // -- COMSUBS.OVL
    {"COMSUBS.OVL", 0x01b1, "TONE_SWEEP", I::CombatCharm, S::Implemented, C, "adjacent ' possessed!'"},
    {"COMSUBS.OVL", 0x02cb, "TONE_SWEEP", I::CombatSummon, S::Implemented, C, "adjacent ' gates in a daemon!'"},
    {"COMSUBS.OVL", 0x0352, "GLIDE", I::CombatGrazed, S::Implemented, C, "adjacent ' grazed!'"},
    {"COMSUBS.OVL", 0x03d6, "GLIDE", I::CombatDraggedUnder, S::Implemented, C, "adjacent ' dragged under!'"},
    {"COMSUBS.OVL", 0x06de, "NOISE_BURST", I::None, S::EvidenceUnknown, C, "NB(0x320, computed, 0x2bc): owner not mapped"},
    {"COMSUBS.OVL", 0x0acb, "GLIDE", I::None, S::EvidenceUnknown, C, "1300->300 glide, no message: not mapped"},
    {"COMSUBS.OVL", 0x0c0b, "GLIDE", I::None, S::EvidenceUnknown, C, "400->750 glide, no message: not mapped"},
    // -- DUNGEON.OVL
    {"DUNGEON.OVL", 0x04b9, "NOISE_BURST", I::DungeonZap, S::Implemented, C, "electric field"},
    {"DUNGEON.OVL", 0x099e, "NOISE_BURST", I::FieldAfflict, S::Implemented, C, "sleep field, per afflicted member"},
    {"DUNGEON.OVL", 0x0a30, "NOISE_BURST", I::FieldAfflict, S::Implemented, C, "poison field, per afflicted member"},
    {"DUNGEON.OVL", 0x103c, "NOISE_BURST", I::None, S::NoNativeEvent, C, "dungeon teleport ring tick: the ring is not ported as such"},
    {"DUNGEON.OVL", 0x1483, "GLIDE", I::None, S::EvidenceUnknown, C, "cell reveal 0x145c: context not identified"},
    {"DUNGEON.OVL", 0x1cfb, "GLIDE", I::DungeonFail, S::Implemented, C, "Uus/Des Por 'Failed!'"},
    // -- ENDGAME.OVL
    {"ENDGAME.OVL", 0x078f, "TONE_SWEEP", I::EndgameOrb, S::DeferredPresentation, C, "' lives!' (endgame cinematic, D-54)"},
    {"ENDGAME.OVL", 0x0987, "TONE_SWEEP", I::EndgameOrb, S::DeferredPresentation, C, "orb launch (endgame cinematic, D-54)"},
    // -- FONT.OVL (the intro's scene engine)
    {"FONT.OVL", 0x03ca, "NOISE_BURST", I::IntroThunder, S::Implemented, C, "moongate rise / fall"},
    {"FONT.OVL", 0x0403, "BEEP", I::IntroChime, S::Implemented, C, "moongate chime, frame 0 / 4"},
    {"FONT.OVL", 0x088d, "NOISE_BURST", I::IntroSummon, S::Implemented, C, "SUMMON opcode"},
    // -- LOOKOBJ.OVL
    {"LOOKOBJ.OVL", 0x0129, "NOISE_BURST", I::WishGranted, S::Implemented, C, "adjacent '|Poof!|' (the wishing well)"},
    // -- MAINOUT.OVL
    {"MAINOUT.OVL", 0x0300, "NOISE_BURST", I::ShipCollision, S::Implemented, C, "after 'COLLISION!' unless 'Docked!'"},
    {"MAINOUT.OVL", 0x0344, "BEEP", I::MoveBlocked, S::Implemented, C, "wall bump"},
    {"MAINOUT.OVL", 0x113b, "GLIDE", I::ShipSinking, S::Implemented, C, "ship sunk with no skiff, before 'DROWNING!!!'"},
    {"MAINOUT.OVL", 0x11ec, "GLIDE", I::None, S::EvidenceUnknown, C, "after a random tile write: owner not mapped"},
    {"MAINOUT.OVL", 0x12a6, "GLIDE", I::ShipSinking, S::Implemented, C, "after 'WHIRLPOOL!'"},
    {"MAINOUT.OVL", 0x13e9, "GLIDE", I::None, S::EvidenceUnknown, C, "after rand(0,7)==0: owner not mapped"},
    // -- OUTSUBS.OVL
    {"OUTSUBS.OVL", 0x0492, "GLIDE", I::WaterfallFall, S::Implemented, C, "F-A-L-L-S!!!"},
    {"OUTSUBS.OVL", 0x067b, "TONE_SWEEP", I::ApparitionMaterialize, S::Implemented, C, "Camp apparition"},
    {"OUTSUBS.OVL", 0x0698, "TONE_SWEEP", I::ApparitionArpeggio, S::Implemented, C, "Camp apparition"},
    {"OUTSUBS.OVL", 0x0896, "TONE_SWEEP", I::ApparitionHealChime, S::Implemented, C, "Camp apparition"},
    {"OUTSUBS.OVL", 0x08c1, "TONE_SWEEP", I::ApparitionChord, S::Implemented, C, "Camp apparition"},
    // -- SHOPPES.OVL
    {"SHOPPES.OVL", 0x13d8, "TONE_SWEEP", I::ShopTransaction, S::Implemented, C, "healer jingle 1/6"},
    {"SHOPPES.OVL", 0x13ef, "TONE_SWEEP", I::ShopTransaction, S::Implemented, C, "healer jingle 2/6"},
    {"SHOPPES.OVL", 0x1417, "TONE_SWEEP", I::ShopTransaction, S::Implemented, C, "healer jingle 3/6"},
    {"SHOPPES.OVL", 0x142b, "TONE_SWEEP", I::ShopTransaction, S::Implemented, C, "healer jingle 4/6"},
    {"SHOPPES.OVL", 0x144f, "TONE_SWEEP", I::ShopTransaction, S::Implemented, C, "healer jingle 5/6"},
    {"SHOPPES.OVL", 0x1466, "TONE_SWEEP", I::ShopTransaction, S::Implemented, C, "healer jingle 6/6"},
    // -- SJOG.OVL
    {"SJOG.OVL", 0x0237, "NOISE_BURST", I::SearchFail, S::Implemented, C, "adjacent 'Plague!' (searching remains)"},
    {"SJOG.OVL", 0x0c31, "GLIDE", I::None, S::NoNativeEvent, C, "chest-object jimmy 'Key broke!': no native chest-object jimmy"},
    {"SJOG.OVL", 0x1a21, "GLIDE", I::TorchBorrowed, S::Implemented, C, "'Borrowed!'"},
    {"SJOG.OVL", 0x1c1a, "BEEP", I::MoveBlocked, S::Implemented, C, "adjacent 'All must use the same exit!'"},
    {"SJOG.OVL", 0x1c37, "GLIDE", I::CombatEscape, S::Implemented, C, "adjacent 'Escape!' (a member leaves the arena)"},
    {"SJOG.OVL", 0x1d59, "BEEP", I::MoveBlocked, S::Implemented, C, "adjacent 'Blocked!' (arena move)"},
    {"SJOG.OVL", 0x1f08, "GLIDE", I::CombatAbsorbed, S::Implemented, C, "adjacent ' is absorbed!'"},
    {"SJOG.OVL", 0x1f5a, "BEEP", I::CombatReject, S::EvidenceUnknown, C, "reject funnel 0x1f26, beep 1"},
    {"SJOG.OVL", 0x1f62, "BEEP", I::CombatReject, S::EvidenceUnknown, C, "reject funnel 0x1f26, beep 2"},
    {"SJOG.OVL", 0x2218, "TONE_SWEEP", I::CombatCharm, S::Implemented, C, "adjacent ' passes out!'"},
    // -- TALK.OVL
    {"TALK.OVL", 0x11a8, "GLIDE", I::TheftDetected, S::Implemented, C, "adjacent 'Something was stolen!'"},
    // -- TOWN.OVL
    {"TOWN.OVL", 0x0849, "BEEP", I::MoveBlocked, S::Implemented, C, "wall bump"},
    {"TOWN.OVL", 0x0a75, "NOISE_BURST", I::MirrorBreak, S::Implemented, C, "mirror, 18 bursts"},
    {"TOWN.OVL", 0x0e6d, "TONE_SWEEP", I::InstrumentNote, S::Implemented, C, "harpsichord"},
    {"TOWN.OVL", 0x0fbb, "SET_TONE", I::TrapdoorFall, S::Implemented, C, "location 29 trapdoor: 1000..251 ramp"},
    {"TOWN.OVL", 0x0fd3, "SPK_STOP", I::TrapdoorFall, S::Implemented, C, "ramp end"},
    {"TOWN.OVL", 0x101e, "NOISE_BURST", I::TrapdoorFall, S::Implemented, C, "one burst per member as it dies"},
    {"TOWN.OVL", 0x11e9, "TONE_SWEEP", I::ShadowlordAnnounce, S::Implemented, C, "'An air of ... doth surround thee'"},
    // -- ZSTATS.OVL
    {"ZSTATS.OVL", 0x0e42, "GLIDE", I::RingVanishes, S::Implemented, C, "'Ring vanishes!'"},
    // -- found by hand (the linear sweep lands mid-instruction or the call is a wrapper)
    {"ULTIMA.EXE", 0x429c, "BEEP", I::AmbientClockTock, S::Implemented, H, "ambient class 1 phase 4 (alignment gap in the sweep)"},
    {"ULTIMA.EXE", 0x42c4, "NOISE_BURST", I::AmbientFountain, S::Implemented, H, "ambient class 3 enters the 0x42be call"},
    {"SJOG.OVL", 0x1d32, "FOOTSTEP", I::MoveStep, S::Implemented, H, "arena move calls sfx_footstep 0x433e"},
    {"EGA.DRV", 0x269f, "NOISE_BURST", I::TitleFizzle, S::DeferredPresentation, H, "title dissolve (driver's own noise_burst)"},
    {"EGA.DRV", 0x29c5, "NOISE_BURST", I::TitleCrackle, S::DeferredPresentation, H, "title subtitle crackle"},
};
} // namespace

const SfxSite *sfx_sites(size_t &count) {
    count = sizeof(kSites) / sizeof(kSites[0]);
    return kSites;
}

namespace {
bool ends_with(const char *s, const char *suffix) {
    const size_t n = std::strlen(s), m = std::strlen(suffix);
    return n >= m && std::strcmp(s + n - m, suffix) == 0;
}
bool starts_with(const char *s, const char *prefix) { return std::strncmp(s, prefix, std::strlen(prefix)) == 0; }
} // namespace

SfxId sfx_for_combat_text(const char *text, bool ended) {
    if (!text || ended) return SfxId::None; // the Ended line repeats VICTORY! and can repeat itself
    if (std::strcmp(text, "VICTORY!") == 0) return SfxId::VictoryFanfare;             // COMBAT 0x0d02
    if (std::strcmp(text, "Escape!") == 0) return SfxId::CombatEscape;                 // SJOG 0x1c37 / CMDS 0x18ac
    if (std::strcmp(text, "Blocked!") == 0 || std::strcmp(text, "All must use the same exit!") == 0)
        return SfxId::MoveBlocked;                                                     // SJOG 0x1d59 / 0x1c1a
    if (ends_with(text, " is absorbed!")) return SfxId::CombatAbsorbed;                // SJOG 0x1f08
    if (ends_with(text, " passes out!") || ends_with(text, " possessed!")) return SfxId::CombatCharm;
    if (ends_with(text, " gates in a daemon!")) return SfxId::CombatSummon;            // COMSUBS 0x02cb
    if (ends_with(text, " grazed!")) return SfxId::CombatGrazed;                       // COMSUBS 0x0352
    if (ends_with(text, " dragged under!")) return SfxId::CombatDraggedUnder;          // COMSUBS 0x03d6
    if (std::strcmp(text, "ARGH!") == 0) return SfxId::CombatEngulfed;                 // COMBAT 0x07f1
    if (ends_with(text, " regurgitated!")) return SfxId::CombatRegurgitated;          // COMBAT 0x1cbf
    if (ends_with(text, " stole some food!")) return SfxId::CombatFoodStolen;          // COMBAT 0x03b6
    if (starts_with(text, "Absorbed!")) return SfxId::SpellZap;                        // COMBAT 0x0958
    if (std::strcmp(text, "\nThou dost find\nPlague!") == 0) return SfxId::SearchFail; // SJOG 0x0237
    return SfxId::None;
}

SfxId sfx_for_world_text(const char *text) {
    if (!text) return SfxId::None;
    if (std::strcmp(text, "COLLISION!") == 0) return SfxId::ShipCollision;                // MAINOUT 0x0300
    if (std::strcmp(text, "DROWNING!!!") == 0) return SfxId::ShipSinking;                 // MAINOUT 0x113b
    if (std::strcmp(text, "\nWHIRLPOOL!\n") == 0) return SfxId::ShipSinking;              // MAINOUT 0x12a6
    if (std::strcmp(text, "\nSomething was stolen!\n") == 0) return SfxId::TheftDetected; // TALK 0x11a8
    if (std::strcmp(text, "\nPoof!\n") == 0) return SfxId::WishGranted;                   // LOOKOBJ 0x0129
    if (std::strcmp(text, "Absorbed!") == 0) return SfxId::SpellZap;                      // CAST 0x0e6e
    if (std::strcmp(text, "The Sceptre is reclaimed!\n") == 0) return SfxId::SceptreReclaimed; // kernel 0x6221
    if (std::strcmp(text, "\n\nNo effect!\n") == 0) return SfxId::ShardNoEffect;          // CAST 0x166d
    return SfxId::None;
}

} // namespace openu5
