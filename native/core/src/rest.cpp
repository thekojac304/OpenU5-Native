#include "openu5/rest.h"
#include <algorithm>
#include <cstdio>
namespace openu5 {
namespace {
int32_t count(const GameState &g) {
    const int32_t n = g.party.character_count;
    return g.party.party_size < 0 ? std::max<int32_t>(0, n + g.party.party_size)
                                  : std::min(n, g.party.party_size);
}
void emit(RestContext &c, GameEventKind kind, const char *s = nullptr) {
    if (c.events.emit) {
        GameEvent e;
        e.kind = kind;
        e.text = s;
        c.events.emit(c.events.context, e);
    }
}
void msg(RestContext &c, const char *s) { emit(c, GameEventKind::Message, s); }
void mp(CharacterState &m) {
    if (m.character_class == 'A' || m.character_class == 'M')
        m.current_mp = m.intelligence;
    else if (m.character_class == 'B')
        m.current_mp = uint8_t(m.intelligence >> 1);
}
} // namespace
RestEligibility camp_context(const GameState &g, const TurnState &t, int32_t tile, bool dungeon) {
    if (dungeon)
        return {};
    if (g.transport == TransportMode::Ship) {
        if ((t.transport_tile & 0xfc) == 0x20)
            return {false, false, false, false, "Sails must be\nlowered!\n\n"};
        return {true, true, false, false, ""};
    }
    const auto loc = g.position.map.location;
    if (loc >= 1 && loc <= 32) {
        if (tile == 171)
            return {true, false, true, false, ""};
        return {false, false, false, true, "Hole up- Only in bed!\n"};
    }
    if (loc == 0) {
        if (tile >= 1 && tile <= 3)
            return {false, false, false, false, "On land or ship!\n\n"};
        if (g.transport != TransportMode::Foot)
            return {false, false, false, false, "On foot!\n"};
    }
    return {};
}
int32_t camp_watch_count(const GameState &g) {
    int32_t n = 0;
    for (int32_t i = 0; i < g.party.party_size && i < g.party.character_count; ++i) {
        auto s = g.party.characters[i].status;
        if (s == 'G' || s == 'P')
            ++n;
    }
    return n;
}
CampCell camp_guard_walk(CampCell c, Rand rand, RestServices s) {
    if (rand(0, 3) != 2)
        return c;
    auto next = c;
    const auto dir = rand(0, 3);
    if (dir == 0)
        --next.row;
    else if (dir == 1)
        ++next.row;
    else if (dir == 2)
        ++next.col;
    else
        --next.col;
    if (next.col < 0 || next.col > 10 || next.row < 0 || next.row > 10)
        return c;
    if (s.cell_free && !s.cell_free(s.context, next.col, next.row))
        return c;
    return next;
}
bool camp_hole_up(GameState &g, Rand rand, int32_t guard) {
    for (int32_t i = 0; i < count(g); ++i) {
        auto &m = g.party.characters[i];
        if (m.status == 'D' || i == guard)
            continue;
        m.current_hp = uint16_t(std::min<int32_t>(m.max_hp, m.current_hp + rand(1, 63)));
        mp(m);
    }
    return rand(0, 99) < 25;
}
bool camp_wake(RestContext &c, int32_t guard) {
    if (!c.services.karma_record)
        return false;
    if (camp_hole_up(c.game, c.rand, guard)) {
        msg(c, "An apparition!\n");
        emit(c, GameEventKind::Sfx, "apparition-materialize");
        emit(c, GameEventKind::Sfx, "apparition-arpeggio");
        // State/RNG for every member is committed BEFORE the per-member event loop,
        // as campApparition returns its steps to campWake.
        uint8_t rolls[16]{};
        for (int32_t i = 0; i < count(c.game); ++i) {
            auto &m = c.game.party.characters[i];
            if (m.status == 'D') {
                mp(m);
                continue;
            }
            m.current_hp = m.max_hp;
            m.status = 'G';
            int32_t level = 1;
            for (int32_t x = m.exp / 100; x > 0; x >>= 1)
                ++level;
            level = std::min<int32_t>(level, 8);
            if (level != m.level) {
                m.level = uint8_t(level);
                m.max_hp = uint16_t(30 * level);
                m.current_hp = m.max_hp;
                const auto roll = c.rand(1, 3);
                rolls[i] = uint8_t(roll);
                auto &stat = roll == 1 ? m.strength : roll == 2 ? m.dexterity : m.intelligence;
                stat = uint8_t(std::min<int32_t>(30, stat + 1));
            }
            mp(m);
        }
        for (int32_t i = 0; i < count(c.game); ++i) {
            const auto &m = c.game.party.characters[i];
            if (m.status == 'D')
                continue;
            emit(c, GameEventKind::Sfx, "apparition-heal-chime");
            emit(c, GameEventKind::Sfx, "apparition-chord");
            if (rolls[i]) {
                char text[192];
                const char *words[] = {"stronger!", "quicker!", "wiser!"};
                std::snprintf(text, sizeof(text),
                              "\n\"Hail, %s!\nFor thy valiant deeds, I shall reward thee!\nThou "
                              "art now level %u, and\n%s\" \n",
                              m.name, unsigned(m.level), words[rolls[i] - 1]);
                msg(c, text);
            }
        }
        msg(c, "\n");
        const int32_t idx = c.game.karma / 20;
        // Record text is streamed in one event, with quotes supplied by the caller's
        // record adapter. No owned string buffer or truncation of asset prose.
        msg(c, c.services.karma_record(c.services.context, idx < 4 ? idx : 5));
        msg(c, "\n\nThe strangely familiar old man vanishes...\n");
    }
    msg(c, "Party rested!\n");
    emit(c, GameEventKind::PartyChanged);
    return true;
}
RestResult camp_sleep_step(RestContext &c, int32_t h, int32_t hours, CampCell cell) {
    for (int step = 0; step < 12; ++step) {
        advance_clock(c.game, c.turn, 5, &c.rand, c.sky);
        for (int32_t i = 0;
             i < c.game.party.party_size && i < c.game.party.character_count && i < 6; ++i) {
            auto &m = c.game.party.characters[i];
            if (m.status != 'D' && m.ring == 44 && c.rand(0, 7) == 7)
                m.current_hp = uint16_t(std::min<int32_t>(m.max_hp, m.current_hp + 1));
        }
        if (cell.present)
            cell = camp_guard_walk(cell, c.rand, c.services);
    }
    RestResult r;
    r.guard = cell;
    if (h < hours - 1 && c.rand(0, 63) == 0) {
        constexpr int32_t enemies[] = {41, 20, 21, 24, 22, 25, 36, 20};
        r.ambush = true;
        r.enemy = enemies[c.rand(0, 7)];
        msg(c, "Ambushed!\n\n");
    }
    return r;
}
RestResult camp(RestContext &c, int32_t hours, int32_t guard) {
    if (!c.services.karma_record) {
        RestResult r;
        r.invalid_context = true;
        return r;
    }
    msg(c, "Zzzzzz...\n\n");
    RestResult r;
    if (guard >= 0 && c.services.guard_start)
        r.guard = c.services.guard_start(c.services.context, guard);
    for (int32_t h = 0; h < hours; ++h) {
        r = camp_sleep_step(c, h, hours, r.guard);
        if (r.ambush)
            return r;
    }
    camp_wake(c, guard);
    return r;
}
void bed_sleep_begin(RestContext &c) {
    msg(c, "Zzzzzzz...\n");
    for (int32_t i = 0; i < count(c.game); ++i) {
        auto &m = c.game.party.characters[i];
        if (m.status == 'G')
            m.status = 'S';
    }
}
bool bed_sleep_step(RestContext &c) {
    advance_clock(c.game, c.turn, 10, &c.rand, c.sky);
    c.services.snap_npcs(c.services.context);
    const auto &p = c.game.position;
    if (c.services.occupied(c.services.context, p.xy.x, p.xy.y, p.map.floor)) {
        msg(c, "Thrown out of bed!\n");
        return true;
    }
    return false;
}
bool bed_sleep_end(RestContext &c) {
    if (c.game.position.xy.x == 255)
        return false;
    for (int32_t i = 0; i < count(c.game); ++i) {
        auto &m = c.game.party.characters[i];
        if (m.status == 'S')
            m.status = 'G';
    }
    ++c.game.position.xy.x;
    emit(c, GameEventKind::PartyChanged);
    return true;
}
RestResult bed_sleep(RestContext &c, int32_t hours) {
    RestResult r;
    if (!c.services.snap_npcs || !c.services.occupied || c.game.position.xy.x == 255) {
        r.invalid_context = true;
        return r;
    }
    bed_sleep_begin(c);
    for (int32_t i = 0; i < hours * 6; ++i)
        if (bed_sleep_step(c)) {
            r.thrown_out = true;
            break;
        }
    bed_sleep_end(c);
    return r;
}
void camp_repair_ship(RestContext &c) {
    do {
        c.game.ship_hull = std::min<int32_t>(99, c.game.ship_hull + c.rand(1, 3));
    } while (c.game.ship_hull < 10);
    advance_clock(c.game, c.turn, 25, &c.rand, c.sky);
    char text[48];
    std::snprintf(text, sizeof(text), "Hull now %ld!\n\n", long(c.game.ship_hull));
    msg(c, text);
    emit(c, GameEventKind::PartyChanged);
}
} // namespace openu5
