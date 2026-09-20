#include "openu5/dungeon.h"
#include "openu5/dungeon_view.h"
#include "openu5/loot.h"
#include <cstring>
namespace openu5 {
namespace {
constexpr int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
int offset(int f, int x, int y) { return f * 64 + y * 8 + x; }
int cleared_bit(int loc, int room) {
    int i = loc - 33;
    if (i >= 1)
        --i;
    return i * 16 + (room & 15);
}
bool at(const DungeonState &d, int x, int y) {
    const auto &w = d.wanderer;
    return w.type != 255 && w.floor == d.pos.floor && w.x == x && w.y == y;
}
void caps(const GameState &g, const DungeonState &d, bool &up, bool &down) {
    int c = dungeon_cell(d, d.pos.floor, d.pos.x, d.pos.y), t = c >> 4;
    up = t == 1 || t == 3 || ((c & 8) && g.grapple);
    down = t == 2 || t == 3 || t == 6;
    if (up || down || t != 10)
        return;
    int a = dungeon_cell(d, d.pos.floor - 1, d.pos.x, d.pos.y) >> 4;
    int b = dungeon_cell(d, d.pos.floor + 1, d.pos.x, d.pos.y) >> 4;
    up = a == 2 || a == 3;
    down = b == 1 || b == 3;
}
struct Rules {
    GameState &g;
    TurnState &t;
    DungeonState &d;
    DungeonSink sink;
    void event(DungeonEventKind k, const char *text = nullptr, int value = -1, int member = -1) {
        if (sink.emit)
            sink.emit(sink.context, {k, text, value, member});
    }
    void msg(const char *text) { event(DungeonEventKind::Message, text); }
    int rand(int lo, int hi) { return g.rng.next(lo, hi).value; }
    void damage() {
        auto p = party_members(g.party);
        for (int i = 0; i < p.count; ++i) {
            int n = p.indices[i];
            if (g.party.characters[n].status == 'D')
                continue;
            int v = rand(1, 8);
            apply_damage(g, n, v);
            event(DungeonEventKind::Damage, nullptr, v, n);
            event(DungeonEventKind::Sfx, "combat-damage");
        }
    }
    void tick() {
        auto p = party_members(g.party);
        for (int i = 0; i < p.count; ++i) {
            auto &c = g.party.characters[p.indices[i]];
            if (c.status == 'S' && rand(0, 63) < 4)
                c.status = 'G';
        }
        if (t.time_spell == 'T')
            return;
        if (t.time_spell == 'Q') {
            d.quickness_toggle ^= 1;
            if (d.quickness_toggle != 1)
                return;
        }
        auto &w = d.wanderer;
        if (w.type == 255 || w.floor != d.pos.floor)
            return;
        if (w.type != 0x1b)
            for (int i = 0; i < 8; ++i) {
                int dir = rand(0, 3), x = int(w.x) + dx[dir], y = int(w.y) + dy[dir];
                x = x > 7 ? 0 : x < 0 ? 7 : x;
                y = y > 7 ? 0 : y < 0 ? 7 : y;
                int cell = dungeon_cell(d, w.floor, x, y) >> 4;
                if (cell == 6 || cell == 8 || cell >= 10)
                    continue;
                if (x == d.pos.x && y == d.pos.y && rand(0, 7) != 1)
                    continue;
                w.prev_x = w.x;
                w.prev_y = w.y;
                w.x = uint8_t(x);
                w.y = uint8_t(y);
                break;
            }
        if (w.x != d.pos.x || w.y != d.pos.y)
            return;
        w.x = w.prev_x;
        w.y = w.prev_y;
        int dir = ((w.prev_x - 1) & 7) == d.pos.x   ? 1
                  : ((w.prev_x + 1) & 7) == d.pos.x ? 3
                  : ((w.prev_y - 1) & 7) == d.pos.y ? 2
                                                    : 0;
        if (dir != int(d.pos.facing)) {
            static const char *texts[] = {"Attacked from the north!", "Attacked from the east!",
                                          "Attacked from the south!", "Attacked from the west!"};
            msg(texts[dir]);
            d.pos.facing = DungeonFacing(dir);
            event(DungeonEventKind::Turned);
        } else
            msg("Attacked!");
        event(DungeonEventKind::Corridor, nullptr, 0);
    }
    void room(int c) {
        if ((c >> 4) == 10 || dungeon_room_cleared(g, d.pos.dungeon, c & 15))
            msg("Entering room...");
        else
            event(DungeonEventKind::Room, nullptr, dungeon_room_map(d.pos.dungeon, c & 15));
    }
    void enter(int cell) {
        tick();
        int type = cell >> 4, sub = cell & 15;
        if (type == 15 || type == 10) {
            room(cell);
            return;
        }
        if (type == 8) {
            int field = sub & 7;
            if (field < 2) {
                msg(field == 0 ? "Sleep spell!" : "Poison!");
                auto p = party_members(g.party);
                for (int i = 0; i < p.count; ++i) {
                    auto &c = g.party.characters[p.indices[i]];
                    int roll = rand(1, 30);
                    if (c.status != 'D' && roll >= c.dexterity) {
                        c.status = field == 0 ? 'S' : 'P';
                        event(DungeonEventKind::Sfx, "field-afflict");
                    }
                }
                if (field == 0)
                    d.cells[offset(d.pos.floor, d.pos.x, d.pos.y)] = uint8_t(sub & 8);
            } else if (field == 2) {
                msg("Fire!!");
                damage();
            }
        } else if (type == 6) {
            if ((sub & 7) == 1) {
                int f = d.pos.floor;
                int cur = cell;
                while ((cur >> 4) == 6 && (cur & 7) == 1 && f < 8) {
                    msg("Pit Trap!");
                    msg("Falling...");
                    d.cells[offset(f, d.pos.x, d.pos.y)] = uint8_t(0x60 | (cur & 8));
                    d.pos.floor = uint8_t(++f);
                    if (f >= 8) {
                        damage();
                        event(DungeonEventKind::ExitUnderworld);
                        return;
                    }
                    event(DungeonEventKind::FloorChanged);
                    msg("      ...splat!");
                    damage();
                    cur = dungeon_cell(d, f, d.pos.x, d.pos.y);
                }
                if ((cur >> 4) == 15 || (cur >> 4) == 10)
                    room(cur);
                dungeon_respawn(g, d);
            } else if ((sub & 7) == 2) {
                msg("Bomb Trap!");
                msg("KABOOM!!");
                damage();
                d.cells[offset(d.pos.floor, d.pos.x, d.pos.y)] = uint8_t(sub & 8);
            } else
                msg("A pit.");
        } else if (type == 5)
            msg("A fountain.");
        else if (type == 4)
            msg("A chest!");
    }
    void level(bool up, bool magic) {
        if (magic && d.pos.dungeon == 40)
            return;
        msg(magic ? (up ? "Up!" : "Down!") : (up ? "Up!\n" : "Down!\n"));
        if (up && d.pos.floor == 0) {
            event(DungeonEventKind::ExitSurface);
            return;
        }
        if (!up && d.pos.floor >= 7) {
            event(DungeonEventKind::ExitUnderworld);
            return;
        }
        int f = d.pos.floor + (up ? -1 : 1), c = dungeon_cell(d, f, d.pos.x, d.pos.y);
        if (magic && (c >> 4) != 0) {
            msg("Failed!");
            event(DungeonEventKind::Sfx, "dungeon-fail");
            return;
        }
        d.pos.floor = uint8_t(f);
        event(DungeonEventKind::FloorChanged);
        dungeon_respawn(g, d);
        enter(c);
    }
};
} // namespace
uint8_t dungeon_cell(const DungeonState &d, int f, int x, int y) {
    return f < 0 || f >= 8 || x < 0 || x >= 8 || y < 0 || y >= 8 ? 0xb0 : d.cells[offset(f, x, y)];
}
int dungeon_room_map(int loc, int room) { return 16 + (loc - 33 - (loc > 34 ? 1 : 0)) * 16 + room; }
bool dungeon_room_cleared(const GameState &g, int loc, int room) {
    int b = cleared_bit(loc, room);
    return b >= 0 && b < 112 && (g.dungeon_rooms_cleared[b >> 3] & (1 << (b & 7)));
}
void dungeon_mark_room(GameState &g, DungeonState &d, int f, int x, int y) {
    int c = dungeon_cell(d, f, x, y);
    if ((c >> 4) != 15 && (c >> 4) != 10)
        return;
    int key = ((d.pos.dungeon & 15) << 4) + (c & 15), bit = cleared_bit(d.pos.dungeon, c & 15);
    if (key != 0x41 && key != 0x46 && key != 0x4b && key != 0x4c && key != 0x50 && key != 0x5b &&
        bit >= 0 && bit < 112)
        g.dungeon_rooms_cleared[bit >> 3] |= uint8_t(1 << (bit & 7));
    d.cells[offset(f, x, y)] = uint8_t(0xa0 | (c & 15));
}
void dungeon_respawn(GameState &g, DungeonState &d) {
    static const uint8_t types[] = {0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1c, 0x1b},
                         attrs[] = {0x60, 0xa0, 0, 0x90, 0x80, 0x60, 0, 0};
    auto &w = d.wanderer;
    w = {};
    w.bank = uint8_t(g.rng.next(0, 7).value);
    w.type = types[w.bank];
    w.attr = attrs[w.bank];
    w.floor = d.pos.floor;
    for (int i = 0; i < 8; ++i) {
        int n = g.rng.next(0, 63).value, x = n % 8, y = n >> 3,
            t = dungeon_cell(d, d.pos.floor, x, y) >> 4;
        if (!(t < 6 || t == 7) || x == d.pos.x || y == d.pos.y)
            continue;
        w.x = w.prev_x = uint8_t(x);
        w.y = w.prev_y = uint8_t(y);
        if ((w.type == 0x16 || w.type == 0x18) && g.rng.next(0, 99).value > 0x30)
            w.hidden = true;
        return;
    }
    w.type = 255;
    w.bank = 0;
}
void dungeon_load(GameState &g, DungeonState &d, const DungeonData &data, bool under) {
    d.pos = {data.location, 0, 1, 1, DungeonFacing::South};
    d.wanderer = {};
    d.quickness_toggle = 0;
    d.active = true;
    std::memset(d.revealed, 0, sizeof(d.revealed));
    std::memcpy(d.cells, data.cells, 512);
    if (under && data.location != 40)
        d.pos = {data.location, 7, 7, 7, DungeonFacing::West};
    else
        for (int n = 0; n < 64; ++n)
            if ((d.cells[n] >> 4) == 1 || (d.cells[n] >> 4) == 3) {
                d.pos.x = uint8_t(n % 8);
                d.pos.y = uint8_t(n / 8);
                break;
            }
    for (auto &c : d.cells)
        if ((c >> 4) == 15 && dungeon_room_cleared(g, data.location, c & 15))
            c = uint8_t(0xa0 | (c & 15));
    dungeon_respawn(g, d);
}
bool dungeon_klimb_choice(const GameState &g, const DungeonState &d) {
    bool up, down;
    caps(g, d, up, down);
    return up && down;
}
void dungeon_action(GameState &g, TurnState &t, DungeonState &d, DungeonAction action,
                    DungeonSink sink, int dir, int member) {
    Rules r{g, t, d, sink};
    int f = int(d.pos.facing), c = dungeon_cell(d, d.pos.floor, d.pos.x, d.pos.y);
    switch (action) {
    case DungeonAction::Open: {
        if ((c >> 4) == 7) {
            r.msg("Already Open!");
            return;
        }
        if ((c >> 4) != 4) {
            r.msg("What?\n");
            return;
        }
        if (c & 7) {
            auto trap = chest_trap(g, d.pos.dungeon,
                                   g.party.active_character == 255 ? 0 : g.party.active_character,
                                   rng_source(g.rng));
            r.event(DungeonEventKind::Sfx, "dungeon-trap");
            r.msg(trap.message);
            if (trap.damage_mask)
                r.event(DungeonEventKind::DamageScript, nullptr, trap.damage_mask);
        }
        d.cells[offset(d.pos.floor, d.pos.x, d.pos.y)] = uint8_t(0x70 | (c & 8));
        r.msg("Chest opened");
        return;
    }
    case DungeonAction::Get: {
        r.msg("Get\n");
        if ((c >> 4) == 4) {
            r.msg("Must open first!\n");
            return;
        }
        if ((c >> 4) != 7) {
            r.msg("Not here!\n");
            return;
        }
        d.cells[offset(d.pos.floor, d.pos.x, d.pos.y)] = uint8_t(c & 8);
        r.msg("contents\nof chest\nYou find:\n");
        LootGrant grants[7];
        uint8_t count;
        auto error = dungeon_chest_loot(d.pos.floor, rng_source(g.rng), grants, count);
        if (error != Error::None) {
            r.event(DungeonEventKind::Error, nullptr, int(error));
            return;
        }
        for (int i = 0; i < count; ++i) {
            apply_loot_grant(g, grants[i]);
            r.event(DungeonEventKind::Loot, nullptr, grants[i].id, grants[i].quantity);
        }
        return;
    }
    case DungeonAction::Jimmy: {
        if ((c >> 4) == 7) {
            r.msg("Already open!\n");
            return;
        }
        if ((c >> 4) != 4) {
            r.msg("What?\n");
            return;
        }
        if (g.keys <= 0) {
            r.msg("No keys!\n");
            return;
        }
        int active = g.party.active_character;
        if (active >= g.party.character_count) {
            auto p = party_members(g.party);
            active = p.count ? p.indices[0] : -1;
        }
        int dex = active < 0 ? 0 : g.party.characters[active].dexterity;
        if ((c & 7) && r.rand(1, 30) > (((d.pos.floor * 2 - dex + 30) & 65535) >> 1)) {
            r.msg("Chest unlocked\n");
            d.cells[offset(d.pos.floor, d.pos.x, d.pos.y)] = uint8_t(0x40 | (c & 8));
        } else {
            r.msg("Key broke!\n");
            --g.keys;
        }
        return;
    }
    case DungeonAction::Drink: {
        if ((c >> 4) != 5) {
            r.msg("No fountain here.");
            return;
        }
        int active = g.party.active_character;
        if (active >= g.party.character_count) {
            auto p = party_members(g.party);
            active = p.count ? p.indices[0] : -1;
        }
        auto *ch = active < 0 ? nullptr : &g.party.characters[active];
        switch (c & 15) {
        case 0:
            if (ch)
                ch->status = 'G';
            r.msg("Cured!");
            break;
        case 1:
            if (ch)
                ch->current_hp = ch->max_hp;
            r.msg("Healed!");
            r.event(DungeonEventKind::Damage, nullptr, 0, active);
            break;
        case 2:
            if (ch)
                ch->status = 'P';
            r.msg("Poisoned!");
            break;
        default: {
            int v = r.rand(0, 7);
            r.msg("Bad taste.");
            if (ch)
                apply_damage(g, active, v);
            r.event(DungeonEventKind::Damage, nullptr, v, active);
            if (active >= 0)
                r.event(DungeonEventKind::DamageScript, nullptr, 1 << active);
            break;
        }
        }
        return;
    }
    case DungeonAction::Search: {
        if (!g.torch_turns && t.light_spell_minutes <= 0) {
            r.msg("\nYou find:\ndarkness.\n");
            return;
        }
        int face = (f + (dir == 2   ? 3
                         : dir == 3 ? 1
                                    : 0)) %
                   4,
            x = dir == 1 ? d.pos.x : (d.pos.x + dx[face] + 8) % 8,
            y = dir == 1 ? d.pos.y : (d.pos.y + dy[face] + 8) % 8,
            cell = dungeon_cell(d, d.pos.floor, x, y), type = cell >> 4,
            n = offset(d.pos.floor, x, y);
        r.msg("You find:");
        if (type == 13 && !(d.revealed[n >> 3] & (1 << (n & 7)))) {
            d.revealed[n >> 3] |= uint8_t(1 << (n & 7));
            r.msg("A hidden door!");
            return;
        }
        int active = member >= 0 ? member : g.party.active_character;
        if (member < 0 && active >= g.party.character_count) {
            auto p = party_members(g.party);
            active = p.count ? p.indices[0] : -1;
        }
        int dex = active >= 0 && active < g.party.character_count
                      ? g.party.characters[active].dexterity
                      : 15,
            threshold = ((d.pos.floor * 2 - dex + 30) & 65535) >> 1;
        if (type == 4) {
            if (r.rand(1, 30) > threshold)
                r.msg("No trap");
            else {
                int roll = r.rand(1, 8);
                r.msg(roll < 4 ? "A simple trap" : roll < 7 ? "A trap" : "A complex trap");
            }
            return;
        }
        if (type == 12) {
            // One derivation of g_dng_wall_variant, shared with the first-person
            // view (dungeon_view.cpp): the Search message and the wall texture
            // must never be able to disagree about which dungeon this is.
            int variant = dungeon_wall_variant(d.pos.dungeon);
            if (variant == 1)
                r.msg("Nothing on the stalactite.");
            else if (variant == 2)
                r.msg("Nothing in the caved in passage.");
            else {
                r.msg("Nothing hidden on the skeleton.");
                r.msg("It crumbles away.");
                d.cells[n] = uint8_t(0xb0 | (cell & 8));
            }
            return;
        }
        if (type == 6) {
            if ((cell & 7) == 2) {
                if (r.rand(1, 30) > threshold) {
                    r.msg("A bomb trap!");
                    d.cells[n] = uint8_t(cell & 8);
                } else
                    r.msg("Nothing of note.");
            } else if ((cell & 7) == 1) {
                r.msg("A pit!");
                d.cells[n] = uint8_t(0x60 | (cell & 8));
            } else
                r.msg("Nothing hidden\nin the pit.");
            return;
        }
        if (type >= 1 && type <= 3)
            r.msg("Nothing hidden on the ladder.");
        else if (type == 5)
            r.msg("Nothing hidden on the fountain.");
        else if (type == 8) {
            static const char *msg[] = {"A sleep field.", "A poison gas field.", "A wall of fire.",
                                        "An electric field.", "An energy field."};
            r.msg(msg[(cell & 7) < 4 ? cell & 7 : 4]);
        } else if (type == 10 || type == 14 || type == 15)
            r.msg("Nothing hidden on the door.");
        else if (type == 11)
            r.msg("Nothing hidden on the wall.");
        else
            r.msg("Nothing of note.");
        return;
    }
    case DungeonAction::Forward:
    case DungeonAction::Back: {
        r.msg(action == DungeonAction::Forward ? "Advance\n" : "Back up\n");
        int sign = action == DungeonAction::Forward ? 1 : -1;
        int x = (d.pos.x + sign * dx[f] + 8) % 8, y = (d.pos.y + sign * dy[f] + 8) % 8,
            cell = dungeon_cell(d, d.pos.floor, x, y), type = cell >> 4;
        if (cell == 0x83) {
            r.msg("Ouch!");
            r.msg("Electric field!");
            r.event(DungeonEventKind::Sfx, "dungeon-zap");
            r.damage();
            return;
        }
        int n = offset(d.pos.floor, x, y);
        if (type == 11 || type == 12 || (type == 13 && !(d.revealed[n >> 3] & (1 << (n & 7)))) ||
            at(d, x, y)) {
            r.msg("Blocked!");
            return;
        }
        d.pos.x = uint8_t(x);
        d.pos.y = uint8_t(y);
        r.event(DungeonEventKind::Moved);
        r.enter(cell);
        return;
    }
    case DungeonAction::Left:
    case DungeonAction::Right:
        if ((c >> 4) == 14) {
            r.msg("Not in doorway!\n");
            return;
        }
        d.pos.facing = DungeonFacing((f + (action == DungeonAction::Left ? 3 : 1)) % 4);
        r.msg(action == DungeonAction::Left ? "Turn left\n" : "Turn right\n");
        r.event(DungeonEventKind::Turned);
        return;
    case DungeonAction::TurnAround:
        d.pos.facing = DungeonFacing((f + 2) % 4);
        r.msg("Turn around.\n");
        r.event(DungeonEventKind::Turned);
        return;
    case DungeonAction::Klimb: {
        bool up, down;
        caps(g, d, up, down);
        if (dir == 2) {
            r.msg("Pass");
            return;
        }
        if (!up && !down) {
            r.msg(c & 8 ? "Klimb-\nWith What?" : "Klimb-what?");
            return;
        }
        r.level(up && (!down || dir != 1), false);
        return;
    }
    case DungeonAction::MagicUp:
    case DungeonAction::MagicDown:
        r.level(action == DungeonAction::MagicUp, true);
        return;
    case DungeonAction::Attack:
        r.msg("Attack\n");
        if (at(d, (d.pos.x + dx[f]) & 7, (d.pos.y + dy[f]) & 7))
            r.event(DungeonEventKind::Corridor, nullptr, 1);
        else
            r.msg("What?\n");
        return;
    case DungeonAction::Tick:
        r.tick();
        return;
    case DungeonAction::Pass:
        return;
    }
}
} // namespace openu5
