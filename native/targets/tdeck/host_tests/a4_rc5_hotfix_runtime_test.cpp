// Alpha 4 RC5 hotfix (targets/tdeck/ALPHA4_UI.md section 17) -- the two RC4 hardware failures, through the REAL AlphaRuntime
// (real Board over the fake ST7789, raw keys in, the transcript the player reads out).
//
//   R4-4  (U)se > In Mani Corp Scroll > a LIVING party member printed only "Failed!" on the T-Deck; the PARITY2 correction
//         (CAST2.OVL 0x060a "Not dead!" then CAST.OVL 0x1b90 "Failed!") was pinned only by a test that called world_magic()
//         directly, and the unit test's own fixture builds the Command by hand.
//   R4-5  the healer's R (Resurrect) did nothing while H and C worked: UiSession::handle_shop mapped a raw 'r' to
//         ShopAction::Rest for every shop but the Barkeeper, so ShopAction::Resurrect (the only action the healer's
//         HealerNeed/Menu phase accepts for the raise) was never produced by a key.
//
//   U  the (U)se route: picker, "Use on whom?" party pick, the transcript
//        U1 a living target: Resurrection! < Not dead! < Failed! in that order, the scroll spent, the record untouched
//        U2 CONTROL: a dead target is revived and neither line is printed
//        U3 CONTROL: cancelling the "Use on whom?" pick prints neither line and spends nothing (device rule, MANI-FINAL 7.4)
//   H  the healer: H, C and R through a real Talk to the healer of East Britanny
//        H1 R opens the member list (a phase change to HealerMember), exactly as H and C do
//        H2 CONTROL: H heals the chosen member and charges; C cures poison and charges
//        H3 R + the dead companion: raised, status G, XP cut by the karma (exp*karma/100), level and max HP recomputed, HP := the
//           NEW max, gold charged the price the deal named; the Avatar (alive) is untouched
//        H4 karma >= 98 cuts nothing but still recomputes level / max HP from the unchanged XP
//        H5 R + a LIVING member: "Thou hast no need of this art!", nothing charged, nothing altered
//        H6 Mic after R (member list and deal) leaves the shop working and charges nothing
//        H7 R with too little gold raises nobody
//
//   a4_rc5_hotfix_runtime <openu5-alpha1-resources.bin>
#include "a4_ui2_harness.h"
#include "openu5/commands.h"
#include "openu5/quest_state.h"
#include "openu5/shop_orchestration.h"
#include "openu5/shops.h"

using namespace a4_ui2;

namespace {
AudioPackInfo g_audio{};
const std::vector<Member> kTwo = {{"Avatar", 'G', 90}, {"Iolo", 'G', 80}};
const std::vector<Member> kRaise = {{"Avatar", 'G', 90}, {"Iolo", 'D', 0}, {"Shamino", 'G', 70}};

std::string since(Run &h, size_t mark) {
    std::string out;
    for (size_t i = mark; i < h.rt->ui()->transcript_size(); ++i)
        if (const auto *b = h.rt->ui()->transcript_at(i)) out += std::string(b->text) + "\n";
    return out;
}
size_t mark_of(Run &h) { return h.rt->ui()->transcript_size(); }
long at(const std::string &hay, const char *needle) {
    const auto p = hay.find(needle);
    return p == std::string::npos ? -1 : long(p);
}
size_t times(const std::string &hay, const char *needle) {
    size_t n = 0;
    for (auto p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + 1)) ++n;
    return n;
}
std::string flat(std::string s) {
    for (auto &c : s) if (c == '\n') c = '|';
    return s;
}

void surface(Run &h) {
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.transport = TransportMode::Foot;
    h.rt->turn().transport_tile = 0x1c;
    g.party.active_character = 0;
}

// ---- U: the (U)se route ---------------------------------------------------------------------------------------------
struct UseOut {
    std::string text;
    int scroll = -1;
    bool inventory = false, party = false;
    CharacterState target{};
};
UseOut use_in_mani(Run &h, int member, bool cancel_pick, const char *tag, bool place = true) {
    auto &g = h.rt->game();
    // The picker lists potions first, then scrolls, then the quest items: leave In Mani Corp as the ONLY row.
    for (auto &q : g.potion_quantities) q = 0;
    for (auto &q : g.scroll_quantities) q = 0;
    g.scroll_quantities[6] = 1;
    g.magic_carpets = 0; g.skull_keys = 0; g.grapple = false; g.spyglass = false; g.sextant = false; g.wooden_box = false;
    if (place) surface(h);
    const size_t m = mark_of(h);
    UseOut o;
    h.key('u');
    o.inventory = h.rt->ui()->mode() == UiMode::InventorySelection;
    h.key('\r');   // the only owned scroll: In Mani Corp
    o.party = h.rt->ui()->mode() == UiMode::PartySelection && h.rt->ui()->request() == UiRequestId::UseTarget;
    for (int i = 0; i < member; ++i) h.down();
    if (cancel_pick) h.mic();
    else h.key('\r');
    o.text = since(h, m);
    h.run(60);
    dump(tag);
    o.scroll = g.scroll_quantities[6];
    o.target = g.party.characters[member];
    return o;
}
bool same(const CharacterState &a, const CharacterState &b) { return std::memcmp(&a, &b, sizeof a) == 0; }

bool walk_into(Run &h, int location) {
    surface(h);
    h.rt->game().position.xy = {uint8_t(pack->location_x[location - 1]), uint8_t(pack->location_y[location - 1])};
    h.key('e');
    return h.rt->game().position.map.location == location;
}
void test_use() {
    std::printf("\nU  (U)se > In Mani Corp Scroll > a party member (the T-Deck route)\n");
    {
        Run h(kTwo);
        const CharacterState before = h.rt->game().party.characters[1];
        const UseOut o = use_in_mani(h, 1, false, "u1-living");
        check(o.inventory && o.party, "U0 (U)se opens the item picker, the scroll opens \"Use on whom?\" (the device's two modals)");
        const long a = at(o.text, "Resurrection!"), b = at(o.text, "Not dead!"), c = at(o.text, "Failed!");
        check(a >= 0 && b > a && c > b,
              "U1 a LIVING target: Resurrection! < Not dead! < Failed! in the transcript [" + flat(o.text) + "]");
        check(times(o.text, "Not dead!") == 1 && times(o.text, "Failed!") == 1, "U1b each line exactly once");
        check(o.scroll == 0, "U1c the scroll is spent (CAST.OVL 0x11ec consumes it before the verdict): " + n(o.scroll));
        check(same(o.target, before) && o.target.status == 'G', "U1d the living target is byte-for-byte unchanged");
    }
    {
        Run h(kRaise);
        auto &m = h.rt->game().party.characters[1];
        m.exp = 1000;
        const UseOut o = use_in_mani(h, 1, false, "u2-dead");
        check(o.target.status == 'G' && o.target.current_hp == 1 && at(o.text, "Resurrection!") >= 0 &&
                  at(o.text, "Not dead!") < 0 && at(o.text, "Failed!") < 0,
              "U2 CONTROL: a dead target is revived (G, HP 1) and neither \"Not dead!\" nor \"Failed!\" prints [" + flat(o.text) + "]");
        check(o.scroll == 0, "U2b the scroll is spent");
    }
    {   // the device runs its scenes paced (Run(.., paced=true)): the same route must read the same
        Run h(kTwo, true);
        const UseOut o = use_in_mani(h, 1, false, "u4-paced");
        h.run(3000);
        const std::string t = since(h, 0);
        check(at(o.text, "Not dead!") > 0 || at(t, "Not dead!") > 0, "U4p paced scenes: [" + flat(o.text) + "] later [" + flat(t) + "]");
    }
    if (g_audio.state == AudioPackState::Valid) {   // the device plays its sound: the same route with the audio pack mounted
        Run h(kTwo, true, &g_audio);
        const UseOut o = use_in_mani(h, 1, false, "u4-audio");
        h.run(3000);
        check(at(o.text, "Resurrection!") >= 0 && at(o.text, "Not dead!") > at(o.text, "Resurrection!") && at(o.text, "Failed!") > at(o.text, "Not dead!"),
              "U4a with the audio pack mounted and scenes paced: [" + flat(o.text) + "]");
    }
    {   // DISCRIMINATOR: the SPELL form on a living member -- what the transcript reads (MANI-FINAL 1: spell is Failed! only)
        Run h(kTwo);
        auto &g = h.rt->game();
        surface(h);
        for (auto &q : g.spell_quantities) q = 0;
        g.spell_quantities[42] = 3;
        g.party.characters[0].level = 8; g.party.characters[0].current_mp = 99; g.party.characters[0].intelligence = 30;
        const size_t m = mark_of(h);
        h.key('c');
        h.key('\r');
        for (int i = 0; i < 3 && h.rt->ui()->mode() == UiMode::PartySelection; ++i) { h.down(); h.key('\r'); }
        h.run(60);
        dump("u7-spell");
        const std::string t = since(h, m);
        check(at(t, "Failed!") >= 0 && at(t, "Not dead!") < 0, "U7 DISCRIMINATOR: the SPELL on a living member reads [" + flat(t) + "] (Failed! only, as the binary does)");
    }
    for (const bool cancel_first : {false, true}) {   // the checklist's literal order: use it, then again and cancel (and the reverse), 3 scrolls
        Run h(kTwo);
        auto &g = h.rt->game();
        const auto once = [&](bool cancel, const char *tag) {
            const int keep = g.scroll_quantities[6];
            const UseOut o = use_in_mani(h, 1, cancel, tag);
            g.scroll_quantities[6] = keep - (cancel ? 0 : 1);   // use_in_mani resets the count to 1; carry the running total
            return o;
        };
        UseOut a = cancel_first ? once(true, "u6a") : once(false, "u6a");
        UseOut b = cancel_first ? once(false, "u6b") : once(true, "u6b");
        const UseOut &live = cancel_first ? b : a;
        const UseOut &canc = cancel_first ? a : b;
        check(at(live.text, "Resurrection!") >= 0 && at(live.text, "Not dead!") > at(live.text, "Resurrection!") && at(live.text, "Failed!") > at(live.text, "Not dead!") &&
                  at(canc.text, "Not dead!") < 0 && at(canc.text, "Failed!") < 0,
              std::string("U6 ") + (cancel_first ? "cancel, then use" : "use, then cancel") + ": live [" + flat(live.text) + "] cancel [" + flat(canc.text) + "]");
    }
    {   // inside a dungeon (the dungeon orchestration route)
        Run h(kTwo, false, g_audio.state == AudioPackState::Valid ? &g_audio : nullptr);
        auto &g = h.rt->game();
        surface(h);
        constexpr int kDeceit = 33;
        g.position.xy = {pack->location_x[kDeceit - 1], pack->location_y[kDeceit - 1]};
        set_quest_flag(g.quest, QuestFlag::Word33);
        h.key('e');
        const bool in = h.rt->dungeon_state().active;
        const UseOut o = use_in_mani(h, 1, false, "u5-dungeon", false);
        check(in && at(o.text, "Resurrection!") >= 0 && at(o.text, "Not dead!") > at(o.text, "Resurrection!") && at(o.text, "Failed!") > at(o.text, "Not dead!"),
              "U5 in a dungeon (active=" + n(in) + "): [" + flat(o.text) + "]");
    }
    for (const int town : {21, 1}) {   // East Britanny / Moonglow-class towns: the town command route
        Run h(kTwo);
        const bool in = walk_into(h, town);
        const UseOut o = use_in_mani(h, 1, false, town == 21 ? "u4-town21" : "u4-town1", false);
        const long a = at(o.text, "Resurrection!"), b = at(o.text, "Not dead!"), c = at(o.text, "Failed!");
        check(in && a >= 0 && b > a && c > b, "U4 in town " + n(town) + " (location " + n(h.rt->game().position.map.location) + "): Resurrection! < Not dead! < Failed! [" + flat(o.text) + "]");
    }
    {
        Run h(kTwo);
        const CharacterState before = h.rt->game().party.characters[1];
        const UseOut o = use_in_mani(h, 1, true, "u3-cancel");
        check(at(o.text, "Not dead!") < 0 && at(o.text, "Failed!") < 0 && same(o.target, before),
              "U3 CONTROL: cancelling \"Use on whom?\" prints neither line and changes nobody [" + flat(o.text) + "]");
    }
}

// ---- H: the healer --------------------------------------------------------------------------------------------------
constexpr int kEastBritanny = 21;
constexpr uint8_t kHealerDialog = 0x87;

struct Shopper {
    Run h;
    const NpcSlot *slot = nullptr;
    explicit Shopper(const std::vector<Member> &party) : h(party) {}
    GameState &g() { return h.rt->game(); }
    CommandContext &ctx() { return h.rt->command_context_for_test(); }
    const ShopSession &shop() { return ctx().shop_services->session; }
    UiMode mode() { return h.rt->ui()->mode(); }
    ShopPhase phase() { return shop().phase; }
};
int shop_hour(int location, uint8_t dialog, const NpcSlot *&slot_out) {
    const auto &n = pack->npc_locations[location - 1];
    for (size_t i = 0; i < n.count; ++i) {
        if (n.slots[i].dialog != dialog) continue;
        for (int hour = 8; hour < 20; ++hour)
            if (shop_is_open(n.slots[i].times, uint8_t(hour))) { slot_out = &n.slots[i]; return hour; }
    }
    return -1;
}
const NpcActor *find_actor(Shopper &s, int slot) {
    for (size_t i = 0; i < s.h.rt->actors().count; ++i) {
        const auto &a = s.h.rt->actors().actors[i];
        if (a.location == s.g().position.map.location && a.schedule.slot == slot) return &a;
    }
    return nullptr;
}
/** Enter East Britanny, stand beside the healer and Talk to it; true = the shop session opened. */
bool open_shop_in(Shopper &s, int location, int hour, int slot) {
    s.g().time.hour = uint8_t(hour);
    surface(s.h);
    s.g().position.xy = {uint8_t(pack->location_x[location - 1]), uint8_t(pack->location_y[location - 1])};
    s.h.key('e');
    if (s.g().position.map.location != location) return false;
    const auto *a = find_actor(s, slot);
    if (!a) return false;
    constexpr Direction dirs[] = {Direction::South, Direction::North, Direction::East, Direction::West};
    for (auto d : dirs) {
        const auto dd = direction_delta(d);
        const int x = a->x + dd.dx, y = a->y + dd.dy;
        const int t = s.ctx().terrain->effective(s.ctx().world, s.g().position.map, x, y);
        if (!is_passable(t, TransportMode::Foot).value || s.ctx().shop_services->occupied(s.ctx().shop_services->context, x, y)) continue;
        s.g().position.xy = {uint8_t(x), uint8_t(y)};
        const Direction toward = d == Direction::South ? Direction::North : d == Direction::North ? Direction::South
                               : d == Direction::East ? Direction::West : Direction::East;
        s.h.key('t');
        s.h.ball(toward == Direction::North ? RawInputKind::TrackballUp : toward == Direction::South ? RawInputKind::TrackballDown
                 : toward == Direction::East ? RawInputKind::TrackballRight : RawInputKind::TrackballLeft);
        return s.mode() == UiMode::Shop;
    }
    return false;
}
/** Past the greeting to the Heal/Cure/Resurrect prompt. */
bool to_menu(Shopper &s) {
    for (int i = 0; i < 4 && s.phase() != ShopPhase::HealerNeed && s.phase() != ShopPhase::Menu; ++i) s.h.key('y');
    return s.phase() == ShopPhase::HealerNeed || s.phase() == ShopPhase::Menu;
}
bool in_list(Shopper &s) { return s.phase() == ShopPhase::HealerMember || (s.phase() >= ShopPhase::LegacyHeal && s.phase() <= ShopPhase::LegacyResurrect); }
bool ready(Shopper &s, const std::vector<Member> &, int &slot) {
    const NpcSlot *sl = nullptr;
    const int hour = shop_hour(kEastBritanny, kHealerDialog, sl);
    if (hour < 0 || !sl) return false;
    slot = sl->slot;
    return open_shop_in(s, kEastBritanny, hour, slot) && to_menu(s);
}

void test_healer() {
    std::printf("\nH  the healer: H, C and R through the real shop route\n");
    {   // H2 CONTROL -- Heal and Cure reach their member list and complete.
        Shopper s(kRaise);
        int slot = 0;
        const bool ok = ready(s, kRaise, slot);
        check(ok, "H0 East Britanny's healer opens and reaches the Heal/Cure/Resurrect prompt (phase " + n(int(s.phase())) + ")");
        if (!ok) return;
        auto &g = s.g();
        g.gold = 5000;
        g.party.characters[0].current_hp = 10;   // hurt
        const int g0 = g.gold;
        s.h.key('h');
        const bool heal_list = in_list(s);
        s.h.key('a');            // the Avatar
        s.h.key('y');            // the deal
        check(heal_list && g.party.characters[0].current_hp == g.party.characters[0].max_hp && g.gold < g0,
              "H2a CONTROL: H lists the party, heals the Avatar to full and charges (gold " + n(g0) + " -> " + n(g.gold) + ")");
        to_menu(s);             // "Anything else?" -> Yes
        g.party.characters[2].status = 'P';
        const int g1 = g.gold;
        s.h.key('c');
        const bool cure_list = in_list(s);
        s.h.key('c');            // 'c' = member index 2 (Shamino)
        s.h.key('y');
        check(cure_list && g.party.characters[2].status == 'G' && g.gold < g1,
              "H2b CONTROL: C lists the party, cures Shamino's poison and charges (gold " + n(g1) + " -> " + n(g.gold) + ")");
    }
    for (const int karma : {50, 99}) {
        Shopper s(kRaise);
        int slot = 0;
        if (!ready(s, kRaise, slot)) { check(false, "H3 the shop did not open"); continue; }
        auto &g = s.g();
        g.gold = 5000;
        g.karma = uint8_t(karma);
        auto &io = g.party.characters[1];
        io.exp = 1000;
        io.level = 1;
        io.max_hp = 30;
        io.character_class = 'F';
        const CharacterState avatar = g.party.characters[0], shamino = g.party.characters[2];
        s.h.key('r');
        const bool listed = in_list(s);
        check(listed, "H1/" + n(karma) + " R opens the member list like H and C do (phase " + n(int(s.phase())) + ")");
        if (!listed) { s.h.key('b'); continue; }
        s.h.key('b');   // Iolo
        const bool deal = s.phase() == ShopPhase::HealerDeal;
        const int price = s.shop().price;
        check(deal && price > 0, "H3a/" + n(karma) + " the dead companion is selectable and the deal names a price (" + n(price) + ")");
        const int before = g.gold;
        s.h.key('y');
        const int exp = karma < 98 ? 1000 * karma / 100 : 1000;
        int level = 1;
        for (int e = exp / 100; e > 0; e >>= 1) ++level;
        check(io.status == 'G' && io.exp == exp && io.level == level && io.max_hp == 30 * level && io.current_hp == io.max_hp,
              "H3b/" + n(karma) + " raised: G, XP " + n(io.exp) + " (want " + n(exp) + "), level " + n(io.level) + " (want " + n(level) + "), max HP " +
                  n(io.max_hp) + ", HP " + n(io.current_hp));
        check(g.gold == before - price, "H3c/" + n(karma) + " charged exactly the quoted price: " + n(before) + " -> " + n(g.gold));
        check(same(g.party.characters[0], avatar) && same(g.party.characters[2], shamino),
              "H3d/" + n(karma) + " the living Avatar and Shamino are untouched");
        check(s.mode() == UiMode::Shop, "H3e/" + n(karma) + " the shop stays open");
    }
    {   // H5: a living member -- no need of this art.
        Shopper s(kRaise);
        int slot = 0;
        if (!ready(s, kRaise, slot)) { check(false, "H5 the shop did not open"); return; }
        auto &g = s.g();
        g.gold = 5000;
        const CharacterState avatar = g.party.characters[0];
        s.h.key('r');
        const size_t m = mark_of(s.h);
        s.h.key('a');
        const std::string t = since(s.h, m);
        check(g.gold == 5000 && same(g.party.characters[0], avatar) && s.phase() != ShopPhase::HealerDeal,
              "H5 R then a LIVING member: no deal, nothing charged, nothing changed [" + flat(t) + "]");
    }
    {   // H6: Mic backs out, twice (list, then deal).
        Shopper s(kRaise);
        int slot = 0;
        if (!ready(s, kRaise, slot)) { check(false, "H6 the shop did not open"); return; }
        auto &g = s.g();
        g.gold = 5000;
        s.h.key('r');
        s.h.mic();                      // back out of the member list: "Anything else?"
        const bool back1 = !in_list(s) && s.mode() == UiMode::Shop && g.gold == 5000 && g.party.characters[1].status == 'D';
        to_menu(s);                     // Yes -> the Heal/Cure/Resurrect prompt again
        s.h.key('r');
        s.h.key('b');
        const bool deal = s.phase() == ShopPhase::HealerDeal;
        s.h.mic();                      // decline the deal from the keyboard's Mic
        const bool back2 = s.phase() != ShopPhase::HealerDeal && g.gold == 5000 && g.party.characters[1].status == 'D' && s.mode() == UiMode::Shop;
        check(back1 && deal && back2,
              "H6 Mic from the member list and from the deal charges nothing and raises nobody; the shop still answers (back1=" + n(back1) + " deal=" + n(deal) + " back2=" + n(back2) + " phase " + n(int(s.phase())) + ")");
        to_menu(s);
        s.h.key('h');
        check(in_list(s), "H6b and H still opens its list afterwards");
    }
    {   // H8 CONTROLS -- the same raw 'r' still means Rest at an inn and Rations at a tavern (R is per shop, not Resurrect everywhere)
        int inn_ok = 0, inn_tried = 0, tav_ok = 0, tav_tried = 0;
        for (const int town : {2, 3, 7, 20, 22, 24}) {              // innkeepers: towns_InnKeeper
            const NpcSlot *sl = nullptr;
            const int hour = shop_hour(town, 0x88, sl);
            if (hour < 0 || !sl) continue;
            Shopper s(kRaise);
            s.g().gold = 5000;
            if (!open_shop_in(s, town, hour, sl->slot)) continue;
            ++inn_tried;
            for (int i = 0; i < 4 && s.phase() != ShopPhase::Menu; ++i) s.h.key('y');
            if (s.phase() != ShopPhase::Menu) continue;
            s.h.key('r');
            inn_ok += s.phase() == ShopPhase::InnRestDeal;
            if (inn_tried >= 2) break;
        }
        for (const int town : {1, 2, 3, 4, 8, 19, 22, 24, 30}) {    // barkeepers: towns_Barkeeper
            const NpcSlot *sl = nullptr;
            const int hour = shop_hour(town, 0x82, sl);
            if (hour < 0 || !sl) continue;
            Shopper s(kRaise);
            s.g().gold = 5000;
            if (!open_shop_in(s, town, hour, sl->slot)) continue;
            for (int i = 0; i < 4 && s.phase() != ShopPhase::Tavern; ++i) s.h.key('y');
            if (s.phase() != ShopPhase::Tavern) continue;
            ++tav_tried;
            s.h.key('r');
            tav_ok += s.phase() == ShopPhase::RationsQuantity;
            if (tav_tried >= 3) break;
        }
        check(inn_tried > 0 && inn_ok == inn_tried, "H8a CONTROL: R at an innkeeper still asks to Rest (" + n(inn_ok) + "/" + n(inn_tried) + " inns)");
        check(tav_tried > 0 && tav_ok > 0, "H8b CONTROL: R at a barkeeper that sells rations still asks how many (" + n(tav_ok) + "/" + n(tav_tried) + " taverns)");
    }
    {   // H7: too little gold.
        Shopper s(kRaise);
        int slot = 0;
        if (!ready(s, kRaise, slot)) { check(false, "H7 the shop did not open"); return; }
        auto &g = s.g();
        g.gold = 1;
        s.h.key('r');
        s.h.key('b');
        s.h.key('y');
        check(g.party.characters[1].status == 'D' && g.gold == 1, "H7 R with 1 gp raises nobody and charges nothing");
    }
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: a4_rc5_hotfix_runtime <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load\n");
        return 1;
    }
    g_dump = arg_after(argc, argv, "--dump");
    if (argc > 2 && std::strncmp(argv[2], "--", 2) != 0) g_audio = tdeck::load_audio_pack_info(argv[2]);
    static std::vector<DungeonArena> arenas;
    for (size_t i = 0; i < pack->combat_map_count; ++i) arenas.push_back({pack->combat_map_views[i], pack->combat_sprites + i * 16});
    g_arenas = arenas.data();
    g_arena_count = arenas.size();
    test_use();
    test_healer();
    std::printf("A4 RC5 hotfix runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
