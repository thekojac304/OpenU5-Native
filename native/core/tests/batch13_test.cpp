// Batch 13 -- R-21: the (U)se-consumable echo of CAST.OVL's scroll reader and
// potion drinker.
//
// `gameplay_parity` mismatch 2034 is sequence 2034 of check-gameplay.ts (a
// sequence INDEX, not a byte offset): the first scripted `(U)se` of a scroll.
// Native emitted `Used Vas Lor Scroll.` where the reference emits the bare
// category word `Scroll`.
//
// REFERENCE RULE (re/notes/potions-scrolls.md, derived from re/disasm/CAST.OVL.asm):
//
//   * scroll reader CAST.OVL 0x11de
//       11ec: dec byte[bx + 0x5820]   ; consume, always, first
//       11f0: print "Scroll\n\n"      ; DS 0x466a -- the CATEGORY word
//       1205: jmp cs:[bx*2 - 0x2d40]  ; per-scroll handler prints its own DS line
//
//   * potion drinker CAST.OVL 0x135a
//       136a: dec byte[bx + 0x5828]   ; consume, always, first
//       136e: print "Potion\n"        ; DS 0x4706 -- the CATEGORY word
//
// The reader NEVER echoes the scroll's or potion's own name: the picker closes
// without echoing it (unlike (R)eady), and each handler only prints its own DS
// line.  game/src/main.ts states the same rule at both mouths
// (hud.messageAppend("Scroll") / hud.messageAppend("Potion"), both citing
// DS 0x466a / DS 0x4706), including the arena mouth applyCombatScroll.
//
// Second rule, same sites: when a handler has no authored DS line the binary
// prints NOTHING.  Blue always, Yellow when it healed 0, Red/Green/Orange on a
// failed precondition, and White on a successful (cosmetic) reveal all return
// an empty message.  "No effect!" (DS 0x46ec) is An Tym's location gate, not a
// generic potion fallback -- no potion handler reaches it.
//
// Both fabrications entered in the Alpha-20 action-feedback pass, whose own
// write-up records the intent ("emit `Used <authoritative display name>.` ...
// or `No effect!` when no authored result exists") and records that the Node
// parity generators never ran in that session.
//
// The Sfx + MagicCeremony pair is NOT part of the defect and is pinned here:
// CAST2:0x0000 really is invoked, and the indices/gates come from
// game/src/core/magic/ceremony.ts, which derives the whole jump table.
#include "openu5/combat.h"
#include "openu5/commands.h"
#include "openu5/dungeon.h"
#include "openu5/display_names.h"
#include "openu5/world_commands.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace openu5;

namespace {

int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) {
        std::cerr << "batch13: FAIL " << what << "\n";
        ++failures;
    }
}

struct Seen {
    GameEventKind kind{};
    std::string text;
    int note = 0;
};
void capture(void *context, const GameEvent &event) {
    static_cast<std::vector<Seen> *>(context)->push_back(
        {event.kind, event.text ? event.text : "", event.note});
}

std::vector<std::string> messages(const std::vector<Seen> &events) {
    std::vector<std::string> out;
    for (const auto &e : events)
        if (e.kind == GameEventKind::Message)
            out.push_back(e.text);
    return out;
}
bool said(const std::vector<Seen> &events, const std::string &text) {
    for (const auto &m : messages(events))
        if (m == text)
            return true;
    return false;
}
// True when any message echoes the item's own name -- the shape the reference
// never prints from these two readers.
bool echoed_a_name(const std::vector<Seen> &events) {
    for (const auto &m : messages(events))
        if (m.rfind("Used ", 0) == 0)
            return true;
    return false;
}
int ceremony_index(const std::vector<Seen> &events) {
    for (const auto &e : events)
        if (e.kind == GameEventKind::MagicCeremony)
            return e.note;
    return -1;
}
bool has_sfx(const std::vector<Seen> &events, const char *id) {
    for (const auto &e : events)
        if (e.kind == GameEventKind::Sfx && e.text == id)
            return true;
    return false;
}

// Compact trace of an event stream: "m:<text>" per message, "s:<id>" per sfx,
// "c:<index>" per ceremony.  Used to pin ORDER, not just membership.
std::string trace(const std::vector<Seen> &events) {
    std::string out;
    for (const auto &e : events) {
        if (!out.empty())
            out += "|";
        if (e.kind == GameEventKind::Message)
            out += "m:" + e.text;
        else if (e.kind == GameEventKind::Sfx)
            out += "s:" + e.text;
        else if (e.kind == GameEventKind::MagicCeremony)
            out += "c:" + std::to_string(e.note);
        else
            out += "?";
    }
    return out;
}

// A Rand that never reaches reroll_potion_color's two special draws (r==0 ->
// forced Orange, r==1 -> random colour), so the colour asked for is the colour
// applied and each case below is deterministic.
int32_t steady_draw(void *, int32_t lo, int32_t hi) {
    const int32_t v = lo + 2;
    return v > hi ? hi : v;
}
Rand steady_rand() { return {nullptr, steady_draw}; }

// ---------------------------------------------------------------------------
// World mouth: world_magic()'s UseItem branch.
// ---------------------------------------------------------------------------
struct World {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    std::vector<uint8_t> tiles;
    MapData map_data{};
    WorldData world{};
    std::vector<Seen> events;

    explicit World(int32_t location = 1) : tiles(32 * 32, 5) {
        map_data = MapData{{LocationId(location), 0}, tiles.data(), tiles.size()};
        world.small_maps = &map_data;
        world.small_map_count = 1;
        game.position = {{4, 4}, {LocationId(location), 0}};
        game.party.character_count = game.party.party_size = 1;
        auto &m = game.party.characters[0];
        std::strncpy(m.name, "Avatar", sizeof(m.name) - 1);
        m.party_status = 0;
        m.status = 'G';
        m.current_hp = 50;
        m.max_hp = 100;
        m.current_mp = 50;
        m.level = 8;
        m.intelligence = 30;
        for (auto &q : game.scroll_quantities)
            q = 2;
        for (auto &q : game.potion_quantities)
            q = 2;
    }

    // Runs one (U)se and returns the events it produced.
    std::vector<Seen> use(int32_t item, int32_t member) {
        events.clear();
        const auto active = get_active_map(world, game.position.map);
        check(active.error == Error::None, "world fixture map loads");
        EventSink sink{&events, capture};
        CommandContext context{game, turn, travel, commands, world};
        Command cmd{};
        cmd.kind = CommandKind::UseItem;
        cmd.item = item;
        cmd.member = member;
        world_magic(context, cmd, active.value, sink, steady_rand());
        return events;
    }
};

// ---------------------------------------------------------------------------
// Arena mouth: combat_use_consumable().
// ---------------------------------------------------------------------------
struct Arena {
    GameState game{};
    TurnState turn{};
    CombatState combat{};
    CombatMap map{};
    std::vector<CombatEnemy> defs;
    std::vector<const CombatEnemy *> def_ptrs;
    std::vector<Seen> events;

    Arena() {
        defs.resize(64);
        for (size_t i = 0; i < defs.size(); ++i) {
            auto &d = defs[i];
            d.index = int32_t(i);
            d.name = "Target";
            d.group_name = "Targets";
            d.hp = 200;
            d.strength = 10;
            d.dexterity = 1;
            d.intelligence = 1;
            d.armor = 0;
            d.damage = 1;
            d.max_per_map = 4;
            d.range = 1;
            d.tile = 0x40;
            d.move_class = 0;
        }
        for (auto &d : defs)
            def_ptrs.push_back(&d);

        game.position = {{40, 41}, {0x80, 0}};
        game.party.character_count = game.party.party_size = 1;
        auto &m = game.party.characters[0];
        std::strncpy(m.name, "Avatar", sizeof(m.name) - 1);
        m.character_class = 'M';
        m.status = 'G';
        m.party_status = 0;
        m.current_hp = m.max_hp = 200;
        m.strength = 20;
        m.dexterity = 30;
        m.intelligence = 30;
        m.current_mp = 99;
        m.level = 8;
        m.helmet = m.armor = m.weapon = m.shield = m.ring = m.amulet = 255;
        for (auto &q : game.scroll_quantities)
            q = 2;
        for (auto &q : game.potion_quantities)
            q = 2;
        for (auto &t : map.tiles)
            t = 5;
        for (int d = 0; d < 4; ++d) {
            map.start_count[d] = 1;
            map.starts[d][0] = {5, 9};
        }
    }

    std::vector<Seen> use(int32_t item) {
        events.clear();
        CombatContext c{game, turn, combat};
        c.enemy_defs = def_ptrs.data();
        c.enemy_def_count = def_ptrs.size();
        c.events = {nullptr, [](void *, const CombatEvent &) {}};
        CombatEnemy foe = defs[0];
        const CombatEnemy *list[] = {&foe};
        const auto built =
            initialize_combat(c, map, CombatDirection::North, list, 1, false, nullptr);
        check(built == CombatResult::Ok, "arena fixture builds");
        EventSink sink{&events, capture};
        const auto used = combat_use_consumable(c, item, sink);
        check(used == CombatResult::Ok, "arena (U)se runs");
        return events;
    }
};

// Reference ceremony table for the world reader, from
// game/src/core/magic/ceremony.ts SCROLL_CEREMONY_INDEX (jump table 0x1205,
// handler by handler).  -1 = no ceremony.
constexpr int kWorldScrollCeremony[8] = {0, -1, 2, 3, 4, -1, -1, 7};
// The arena reader additionally loses In Quas Wis (4): its handler 0x1278 gates
// `g_location <= 0x7f` at 0x127f, before the ceremony.
constexpr int kArenaScrollCeremony[8] = {0, -1, 2, 3, -1, -1, -1, 7};

void group_a_world_scroll_echo() {
    // A1 -- the exact reference stream for Vas Lor: category word, DS line, cue.
    {
        World w;
        const auto ev = w.use(0, -1);
        const auto msg = messages(ev);
        check(msg.size() == 2, "A1 Vas Lor prints exactly two lines");
        check(!msg.empty() && msg[0] == "Scroll", "A1 first line is the DS 0x466a category word");
        check(msg.size() > 1 && msg[1] == "Light!", "A1 second line is the handler's DS 0x4673");
        check(w.game.scroll_quantities[0] == 1, "A1 scroll is consumed");
        check(has_sfx(ev, "scroll-used") && ceremony_index(ev) == 0, "A1 ceremony cue survives");
    }
    // A2 -- no scroll echoes its own name, and the first line is always "Scroll".
    for (int i = 0; i < 8; ++i) {
        World w;
        const auto ev = w.use(i, 0);
        const auto msg = messages(ev);
        check(!msg.empty() && msg[0] == "Scroll",
              "A2 every scroll opens with the category word");
        check(!echoed_a_name(ev), "A2 no scroll echoes its own name");
    }
    // A3 -- the picker labels themselves are untouched: the echo was corrected,
    // not the name table.
    check(std::string(scroll_display_name(0)) == "Vas Lor Scroll", "A3 scroll label table intact");
    check(std::string(scroll_display_name(7)) == "An Tym Scroll", "A3 scroll label table intact");
    check(std::string(potion_display_name(0)) == "Blue Potion", "A3 potion label table intact");
    check(std::string(potion_display_name(7)) == "White Potion", "A3 potion label table intact");
}

void group_b_world_potion_echo() {
    // B1 -- category word, never the colour name, for all eight colours.
    for (int c = 0; c < 8; ++c) {
        World w;
        const auto ev = w.use(8 + c, 0);
        const auto msg = messages(ev);
        check(!msg.empty() && msg[0] == "Potion",
              "B1 every potion opens with the DS 0x4706 category word");
        check(!echoed_a_name(ev), "B1 no potion echoes its own name");
    }
    // B2 -- silent success: Blue on a sleeping member wakes them and the
    // binary prints no effect line at all.
    {
        World w;
        w.game.party.characters[0].status = 'S';
        const auto ev = w.use(8, 0);
        check(messages(ev).size() == 1, "B2 silent Blue success prints only the category word");
        check(!said(ev, "No effect!"), "B2 silent success invents no fallback line");
        check(w.game.party.characters[0].status == 'G', "B2 Blue still wakes the sleeper");
    }
    // B3 -- silent failure: Red on an unpoisoned member does nothing and, again,
    // prints nothing.  "No effect!" is An Tym's gate string, not a potion line.
    {
        World w;
        const auto ev = w.use(10, 0);
        check(messages(ev).size() == 1, "B3 silent Red failure prints only the category word");
        check(!said(ev, "No effect!"), "B3 silent failure invents no fallback line");
    }
    // B4 -- an authored DS line is still printed, so B2/B3 are not "print less".
    {
        World w;
        w.game.party.characters[0].status = 'P';
        const auto ev = w.use(10, 0);
        check(said(ev, "Poison cured!"), "B4 authored DS 0x4717 line survives");
        check(messages(ev).size() == 2, "B4 authored line follows the category word");
    }
    // B5 -- cancelling the target picker still spends the potion, prints the
    // category word, and skips the ceremony (CAST.OVL 0x1394/0x1396).
    {
        World w;
        const auto ev = w.use(8, -1);
        check(messages(ev).size() == 1, "B5 cancelled potion prints only the category word");
        check(w.game.potion_quantities[0] == 1, "B5 cancelled potion is still consumed");
        check(ceremony_index(ev) == -1, "B5 cancelled potion skips the ceremony");
    }
}

void group_c_arena_echo() {
    // C1 -- the arena scroll mouth obeys the same rule (main.ts applyCombatScroll).
    {
        Arena a;
        const auto ev = a.use(0);
        const auto msg = messages(ev);
        check(msg.size() == 2, "C1 arena Vas Lor prints exactly two lines");
        check(!msg.empty() && msg[0] == "Scroll", "C1 arena scroll opens with the category word");
        check(msg.size() > 1 && msg[1] == "Light!", "C1 arena scroll keeps its DS line");
        check(!echoed_a_name(ev), "C1 arena scroll echoes no name");
        check(has_sfx(ev, "scroll-used") && ceremony_index(ev) == 0,
              "C1 arena ceremony cue survives");
    }
    // C2 -- and the arena potion mouth: category word, no name, and never the
    // fabricated fallback under any reroll outcome.
    for (int c = 0; c < 8; ++c) {
        Arena a;
        const auto ev = a.use(8 + c);
        const auto msg = messages(ev);
        check(!msg.empty() && msg[0] == "Potion", "C2 arena potion opens with the category word");
        check(!echoed_a_name(ev), "C2 arena potion echoes no name");
        check(!said(ev, "No effect!"), "C2 arena potion invents no fallback line");
    }
}

// ---------------------------------------------------------------------------
// E -- WHERE the ceremony sits in the stream.  CAST.OVL 0x135a lists the
// drinker in order:
//
//   136e: print the category word
//   1375: selChar                ; target select (skipped in the arena)
//   1394: if target<0 -> exit    ; cancelling spends the potion, no ceremony
//   139b: anim(color_original)   ; CEREMONY -- before the reroll
//   13a8: r = rand(0,0x0f)       ; reroll
//   13db: jmp <handler>          ; the handler prints its DS line
//
// so the ceremony precedes the effect line, and the index is the colour ASKED
// FOR (the reroll has not happened yet).  game/src/main.ts pins the same order
// at both mouths (applyCombatPotion, and the pickCastTarget callback).
//
// The scroll reader is the other way round: its ceremony lives inside each
// handler, after that handler's own DS line (main.ts emits it after
// `res.messages`).  Both orders are asserted so neither gets "harmonised".
void group_e_ceremony_order() {
    // E1 -- world potion, authored DS line: ceremony before the line.
    {
        World w;
        w.game.party.characters[0].status = 'P';
        const auto ev = w.use(10, 0);
        check(trace(ev) == "m:Potion|s:potion-used|c:2|m:Poison cured!",
              "E1 world potion ceremony precedes the effect line");
    }
    // E2 -- arena potion: same order, no target select in between.
    {
        Arena a;
        a.game.party.characters[0].status = 'P';
        const auto ev = a.use(10);
        const auto t = trace(ev);
        check(t.rfind("m:Potion|s:potion-used|c:2", 0) == 0,
              "E2 arena potion ceremony precedes the effect line");
    }
    // E3 -- scroll, both mouths: ceremony AFTER the handler's DS line.
    {
        World w;
        check(trace(w.use(0, 0)) == "m:Scroll|m:Light!|s:scroll-used|c:0",
              "E3 world scroll ceremony follows its DS line");
        Arena a;
        check(trace(a.use(0)) == "m:Scroll|m:Light!|s:scroll-used|c:0",
              "E3 arena scroll ceremony follows its DS line");
    }
}

void group_d_ceremony_tables() {
    // D1 -- the world reader's ceremony indices, unchanged by this batch.
    for (int i = 0; i < 8; ++i) {
        World w;
        const auto ev = w.use(i, 0);
        check(ceremony_index(ev) == kWorldScrollCeremony[i],
              "D1 world scroll ceremony index matches the derived jump table");
    }
    // D2 -- An Tym's location gate (loc 0x1d / 0x28 -> "No effect!", no setter,
    // so no ceremony).
    for (int32_t location : {29, 40}) {
        World w(location);
        const auto ev = w.use(7, 0);
        check(said(ev, "No effect!"), "D2 An Tym prints its own gate line");
        check(ceremony_index(ev) == -1, "D2 An Tym's gated branch skips the ceremony");
    }
    // D3 -- the arena reader's narrower table.
    for (int i = 0; i < 8; ++i) {
        Arena a;
        const auto ev = a.use(i);
        check(ceremony_index(ev) == kArenaScrollCeremony[i],
              "D3 arena scroll ceremony index matches the derived jump table");
    }
    // D4 -- potion ceremony index is the colour asked for.
    for (int c = 0; c < 8; ++c) {
        World w;
        const auto ev = w.use(8 + c, 0);
        check(ceremony_index(ev) == c, "D4 potion ceremony index is the colour");
        check(has_sfx(ev, "potion-used"), "D4 potion cue survives");
    }
}

// ---------------------------------------------------------------------------
// F -- the ceremony is the (C)ast's, not the overworld's.  There is ONE cast
// dispatcher in the binary (the 48-entry jump table at CAST.OVL 0x0f1a); a
// dungeon cast runs the same handlers, so it reaches CAST2:0x0000 with the same
// circle index.  game/src/main.ts emits it at all three of its cast mouths --
// arena, overworld and dungeon -- through one `emitCastCeremony` helper.
//
// Native re-implements the cast head a third time in dungeon_orchestration.cpp
// and that copy raised no ceremony, so the same spell was ceremonial outdoors
// and silent underground.
// The three weapon-spells and the four line fans, SPELLS_WITHOUT_CEREMONY in
// game/src/core/magic/ceremony.ts: two whole families, each with its own sound.
bool silent_family(int spell) {
    for (int id : {1, 13, 28, 37, 40, 44, 45})
        if (spell == id)
            return true;
    return false;
}

void group_f_dungeon_cast_ceremony() {
    struct Fixture {
        GameState game{};
        TurnState turn{};
        TravelState travel{};
        CommandState commands{};
        std::vector<uint8_t> terrain;
        WorldData world{};
        DungeonState d{};
        DungeonScratch scratch{};
        DungeonData data{};
        DungeonContext dc{d, scratch};
        std::vector<Seen> events;

        Fixture() : terrain(65536, 5) {
            world.overworld = terrain.data();
            world.overworld_size = terrain.size();
            game.position = {{10, 20}, {0, 0}};
            game.party.character_count = game.party.party_size = 1;
            auto &m = game.party.characters[0];
            std::strncpy(m.name, "Avatar", sizeof(m.name) - 1);
            m.status = 'G';
            m.party_status = 0;
            m.current_hp = m.max_hp = 200;
            m.current_mp = 99;
            m.level = 8;
            m.intelligence = 30;
            m.helmet = m.armor = m.weapon = m.shield = m.ring = m.amulet = 255;
            for (auto &q : game.spell_quantities)
                q = 20;
            data.location = 33;
            dc.data = &data;
            dc.count = 1;
        }

        std::vector<Seen> cast(int32_t spell) {
            events.clear();
            CommandContext ctx{game, turn, travel, commands, world};
            ctx.dungeon_context = &dc;
            Command enter{};
            enter.kind = CommandKind::EnterDungeon;
            enter.member = 33;
            check(execute_command(ctx, enter).status == CommandStatus::Success && d.active,
                  "F setup: production EnterDungeon succeeds");
            ctx.dungeon = true;
            ctx.events = {&events, capture};
            Command cmd{};
            cmd.kind = CommandKind::Cast;
            cmd.item = spell;
            cmd.caster = 0;
            cmd.member = 0;
            execute_command(ctx, cmd);
            return events;
        }
    };

    // F1 -- In Lor (0, circle 1) underground raises the same ceremony it raises
    // above ground.
    {
        Fixture f;
        check(ceremony_index(f.cast(0)) == 1, "F1 dungeon cast raises the circle-indexed ceremony");
    }
    // F2 -- sweep all 48.  Whenever a dungeon cast raises the ceremony at all,
    // the index is the spell's circle, floor(spell/6)+1 (CAST.OVL 0x0e0a-0x0e14
    // computes exactly that and spends the mana with it).  Spells the dungeon
    // gates away with "Not here!" never got to cast, so they raise nothing --
    // that is the reference's own `if (r.ok) emitCastCeremony(...)`.
    {
        int ceremonial = 0;
        for (int spell = 0; spell < 48; ++spell) {
            Fixture f;
            const auto ev = f.cast(spell);
            const int index = ceremony_index(ev);
            if (index < 0)
                continue;
            ++ceremonial;
            check(index == spell / 6 + 1, "F2 dungeon ceremony index is the spell's circle");
            check(!silent_family(spell), "F2 no silent-family spell raises a ceremony");
            check(spell != 46, "F2 Vas Rel Por defers its ceremony to the phase gate");
        }
        // Guards against a vacuous sweep: the underground really does cast.
        check(ceremonial >= 20, "F2 the sweep actually exercised ceremonial casts");
    }
    // F3 -- and the seven that have their own sound underground too: the three
    // weapon-spells and the four line fans stay silent (SPELLS_WITHOUT_CEREMONY).
    for (int spell : {1, 13, 28, 37, 40, 44, 45}) {
        Fixture f;
        check(ceremony_index(f.cast(spell)) == -1,
              "F3 the weapon-spell and line-fan families raise no ceremony underground");
    }
}

} // namespace

int main() {
    group_a_world_scroll_echo();
    group_b_world_potion_echo();
    group_c_arena_echo();
    group_d_ceremony_tables();
    group_e_ceremony_order();
    group_f_dungeon_cast_ceremony();
    if (failures) {
        std::cerr << "batch13: " << failures << " failing checks\n";
        return 1;
    }
    std::cout << "batch13: (U)se consumable echo parity passed\n";
    return 0;
}
