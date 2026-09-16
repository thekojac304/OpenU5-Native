#include "openu5/dialogue.h"
#include <algorithm>
namespace openu5 {
namespace {
int32_t capped(int32_t n, int32_t cap = 99) { return n >= cap ? cap : n+1; }
// Roster names use the existing eight-byte native .GAM domain. No DOS folding:
// joinByName uses JS lowercase, unlike the TLK keyword comparator.
std::u16string lower(TalkText s) {
    std::u16string out(s);
    for (auto &c : out) if (c >= u'A' && c <= u'Z') c = char16_t(c+32);
    return out;
}
TalkText roster_name(const CharacterState &c, char16_t (&buf)[9]) {
    size_t n = 0;
    while (n < 8 && c.name[n]) { buf[n] = static_cast<unsigned char>(c.name[n]); ++n; }
    return {buf,n};
}
}
DialogueEffectResult apply_dialogue_effect(GameState &g, DialogueEffect e, TalkText name, int32_t tile) {
    DialogueEffectResult out;
    switch (e.kind) {
    case DialogueEffectKind::Karma:
        g.karma = uint8_t(std::clamp(int32_t(g.karma)+e.value,int32_t(0),int32_t(99))); break;
    case DialogueEffectKind::Gold: {
        const auto amount = std::max(int32_t(0),e.value);
        if (g.gold >= amount) {
            g.gold = uint16_t(g.gold-amount);
            if (tile >= 0 && (tile & 0xfc) == 0x6c && g.turns_since_start >= 100) {
                g.turns_since_start = 0;
                g.karma = uint8_t(std::min(int(g.karma)+1+(g.gold == 0 ? 2 : 0),99));
            }
        } else { out.messages[0] = u"\"Thou hast not enough gold!\""; out.message_count = 1; }
        break;
    }
    case DialogueEffectKind::GiveItem:
        if (e.value >= 0 && e.value < 0x40 && e.value < g.equipment_count)
            g.equipment_quantities[e.value] = capped(g.equipment_quantities[e.value]);
        else switch (e.value-0x41) {
        case 0: g.food = uint16_t(capped(g.food,9999)); break;
        case 1: g.gold = uint16_t(capped(g.gold,9999)); break;
        case 2: g.keys = capped(g.keys); break;
        case 3: g.gems = capped(g.gems); break;
        case 4: g.torches = capped(g.torches); break;
        case 5: g.grapple = true; break;
        case 6: g.magic_carpets = capped(g.magic_carpets); break;
        case 7: g.sextant = true; break;
        case 8: g.spyglass = true; break;
        case 9: g.black_badge = true; break;
        case 10: g.skull_keys = capped(g.skull_keys); break;
        default: break;
        }
        break;
    case DialogueEffectKind::JoinParty: {
        const auto key = lower(talk_trim(name));
        int found = -1;
        for (uint8_t i = 0; i < g.party.character_count; ++i) {
            char16_t buf[9]{};
            if (lower(roster_name(g.party.characters[i],buf)) == key) { found = i; break; }
        }
        if (found < 0) {
            out.ended = true; out.message_count = 1; out.messages[0] = u"\nSystem Error -\nNo Match!"; break;
        }
        auto &c = g.party.characters[found];
        if (c.party_status == 0) { out.ended = out.despawn_npc = true; break; }
        if (party_members(g.party).count >= kMaxParty) {
            out.message_count = 2;
            out.messages[0] = u"\"Thou hast no room for me in thy party! ";
            out.messages[1] = u"Seek me again if one of thy members doth leave\nthee."; break;
        }
        c.party_status = 0;
        const auto dest = g.party.party_size;
        if (found != dest && dest >= 0 && dest < g.party.character_count) std::swap(c,g.party.characters[dest]);
        ++g.party.party_size;
        out.ended = out.despawn_npc = true; break;
    }
    case DialogueEffectKind::CallGuards: out.alarm = true; break;
    case DialogueEffectKind::End: out.ended = true; break;
    }
    return out;
}
} // namespace openu5
