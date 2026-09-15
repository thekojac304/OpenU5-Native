#include "openu5/combat.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
namespace openu5 {
namespace {
// Generated properties from the authoritative final TILE_INFO, not world
// movement.
#include "combat_tiles.inc"
bool active(const CombatActor &a) {
    return a.status != CombatStatus::Dead && a.status != CombatStatus::Fled &&
           a.status != CombatStatus::Absorbed;
}
bool player(const CombatActor &a) { return a.member != 255; }
bool party_side(const CombatActor &a) { return player(a) != a.charmed; }
bool any_side(const CombatState &s, bool party, bool alive = false) {
    for (int i = 0; i < s.count; ++i)
        if ((alive ? s.actors[i].status != CombatStatus::Dead : active(s.actors[i])) &&
            party_side(s.actors[i]) == party)
            return true;
    return false;
}
bool inside(int x, int y) { return x >= 0 && y >= 0 && x < 11 && y < 11; }
int table(const int32_t *v, size_t n, int id) { return v && id >= 0 && size_t(id) < n ? v[id] : 0; }
bool supported(const CombatEnemy &d) {
    // Splitting expands the TS vector without a fixed limit; magic decision logic
    // is deferred.
    return d.name && std::strlen(d.name) <= 120 &&
           !(d.abilities & (0x4000 | 0x1000 | 0x0080 | 0x0020 | 0x0008 | 0x0004));
}
struct Engine {
    CombatContext &c;
    CombatState &s;
    GameState &g;
    char name_scratch[9]{};
    explicit Engine(CombatContext &ctx) : c(ctx), s(ctx.combat), g(ctx.game) {}
    int rand(int lo, int hi) {
        auto v = s.rng.next(lo, hi).value;
        if (c.trace.emit)
            c.trace.emit(c.trace.context, "combat", lo, hi, v);
        return v;
    }
    int r30() { return std::max<int32_t>(1, rand(0, 60) >> 1); }
    const char *name(const CombatActor &a) {
        if (!player(a))
            return a.enemy->name;
        const char *p = g.party.characters[a.member].name;
        auto space = [](char ch) {
            return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
        };
        size_t begin = 0, end = 0;
        while (end < 8 && p[end])
            ++end;
        while (begin < end && space(p[begin]))
            ++begin;
        while (end > begin && space(p[end - 1]))
            --end;
        if (begin == end)
            return "Avatar";
        std::memcpy(name_scratch, p + begin, end - begin);
        name_scratch[end - begin] = 0;
        return name_scratch;
    }
    void emit(CombatEvent e) {
        if (c.events.emit)
            c.events.emit(c.events.context, e);
    }
    void message(const char *text, int actor = -1, int target = -1,
                 CombatEventKind kind = CombatEventKind::Message) {
        CombatEvent e;
        e.kind = kind;
        e.text = text;
        e.actor = actor;
        e.target = target;
        emit(e);
    }
    void named(const CombatActor &a, const char *suffix, int actor = -1, int target = -1) {
        char text[160];
        std::snprintf(text, sizeof(text), "%s%s", name(a), suffix);
        message(text, actor, target);
    }
    CombatActor *occupant(int x, int y) {
        for (int i = 0; i < s.count; ++i) {
            auto &a = s.actors[i];
            if (active(a) && a.position.x == x && a.position.y == y)
                return &a;
        }
        return nullptr;
    }
    int tile(int x, int y) { return inside(x, y) ? s.map.tiles[y * 11 + x] : -1; }
    bool passable(const CombatEnemy *def, int x, int y) {
        int t = tile(x, y);
        if (t < 0)
            return false;
        if (s.loot[y * 11 + x] == 1 || s.loot[y * 11 + x] == 129)
            t = 1;
        if (size_t(t) >= sizeof(kCombatTileFlags))
            return false;
        if (!def)
            return (kCombatTileFlags[t] & 1) != 0;
        bool water = t < 4 || (t & 0xf0) == 0x60;
        switch (def->move_class) {
        case 1:
            return water;
        case 2:
            return water || (kCombatTileFlags[t] & 2);
        case 4:
            return !water;
        case 7:
            return t == 4;
        case 8:
            return t == 5;
        case 9:
            return t == 1;
        case 10:
            return t == 7;
        case 0:
            return (kCombatTileFlags[t] & 2) != 0;
        default:
            return false;
        }
    }
    bool free(const CombatActor &a, int x, int y) {
        return passable(a.enemy, x, y) && !occupant(x, y);
    }
    void triggers(int x, int y) {
        if (!s.room)
            return;
        for (int i = 0; i < s.map.trigger_count; ++i) {
            auto &t = s.map.triggers[i];
            if (t.at.x != x || t.at.y != y)
                continue;
            for (auto p : {t.first, t.second})
                if (inside(p.x, p.y))
                    s.map.tiles[p.y * 11 + p.x] = t.tile;
            t.at = {255, 255};
        }
    }
    void sleep(CombatActor &a) {
        if (player(a)) {
            auto &r = g.party.characters[a.member];
            if (r.status == 'D')
                return;
            r.status = 'S';
            a.render_tile = 30;
            if (g.party.active_character == a.member)
                g.party.active_character = 255;
        }
        a.sleeping = true;
    }
    void wake(CombatActor &a) {
        a.sleeping = false;
        if (player(a)) {
            auto &r = g.party.characters[a.member];
            if (r.status == 'S')
                r.status = 'G';
            if (a.render_tile == 30)
                a.render_tile = a.invisible ? 29 : -1;
        }
    }
    int defense(const CharacterState &r) {
        int d = 0;
        for (int id : {r.helmet, r.armor, r.weapon, r.shield, r.ring, r.amulet})
            if (id != 255)
                d += table(c.tables.defense, c.tables.count, id);
        return d;
    }
    void status_pass(CombatActor &a) {
        if (!player(a) || a.sleeping || !active(a))
            return;
        auto &r = g.party.characters[a.member];
        if (r.ring == 42) {
            a.invisible = true;
            a.render_tile = 29;
        } else if (r.ring == 44) {
            for (int i = 0; i < std::min<int32_t>(g.party.party_size, int32_t(g.party.character_count));
                 ++i) {
                auto &p = g.party.characters[i];
                if (p.status == 'D' || p.ring != 44)
                    continue;
                if (rand(0, 7) != 7)
                    continue;
                p.current_hp = uint16_t(std::min<int32_t>(int(p.current_hp) + 1, int(p.max_hp)));
                for (int j = 0; j < s.count; ++j)
                    if (s.actors[j].member == i)
                        s.actors[j].hp = std::min<int32_t>(s.actors[j].hp + 1, s.actors[j].max_hp);
            }
        }
    }
    void roster_dead(CombatActor &a) {
        a.hp = 0;
        a.status = CombatStatus::Dead;
        g.party.characters[a.member].current_hp = 0;
        if (g.party.active_character == a.member)
            g.party.active_character = 255;
        if (inside(a.position.x, a.position.y))
            s.loot[a.position.y * 11 + a.position.x] = 30;
    }
    CombatActor *current() {
        if (combat_over(s))
            return nullptr;
        if (s.current >= 0 && active(s.actors[s.current]))
            return &s.actors[s.current];
        s.current = -1;
        for (int k = 0; k < 258 * s.count; ++k) {
            int i = s.scan;
            s.scan = uint8_t((s.scan + 1) % s.count);
            auto &a = s.actors[i];
            if (!active(a))
                continue;
            if (player(a) && g.party.characters[a.member].status == 'D') {
                roster_dead(a);
                continue;
            }
            a.counter = uint8_t(a.counter - 1);
            if (a.counter)
                continue;
            a.counter = uint8_t(36 - a.speed);
            ++s.action_count;
            bool skip = false;
            if (player(a) && g.party.active_character != 255 &&
                a.member != g.party.active_character)
                for (int j = 0; j < s.count; ++j)
                    if (s.actors[j].member == g.party.active_character && active(s.actors[j]))
                        skip = true;
            if (skip) {
                status_pass(a);
                continue;
            }
            s.current = int16_t(i);
            if (player(a)) {
                auto &r = g.party.characters[a.member];
                if (r.weapon == 35 || r.shield == 35)
                    a.charmed = true;
            }
            return &a;
        }
        return nullptr;
    }
    void latch() {
        if (!s.victory && !any_side(s, false) && any_side(s, true, true)) {
            s.victory = true;
            message("VICTORY!");
        }
    }
    void collapse() {
        if (s.ended || any_side(s, true) || !any_side(s, false))
            return;
        for (int i = 0; i < s.count; ++i) {
            auto &a = s.actors[i];
            if (player(a) && a.charmed) {
                a.charmed = false;
                named(a, " passes out!", a.id);
                unequip_item_by_id(g, a.member, 35);
                a.defense = defense(g.party.characters[a.member]);
                sleep(a);
                return;
            }
        }
    }
    void end() {
        s.ended = true;
        message(s.victory ? "VICTORY!" : "BATTLE IS LOST!", -1, -1, CombatEventKind::Ended);
    }
    void advance() {
        s.queue_live = false;
        s.queue_count = 0;
        int closing = s.current;
        if (closing >= 0)
            s.actors[closing].last_attacker = -1;
        s.current = -1;
        if (closing >= 0) {
            auto &a = s.actors[closing];
            if (party_side(a))
                status_pass(a);
            if (active(a) || a.status == CombatStatus::Dead) {
                int t = tile(a.position.x, a.position.y);
                if (t == 143 || t == 188)
                    damage(a, a, rand(0, 10));
                else if (t == 4 && (player(a) || ((64 + 4 * a.enemy->index) & 255) < 128))
                    poison(a, a);
            }
        }
        latch();
        collapse();
        if (combat_over(s)) {
            end();
            return;
        }
        auto *a = current();
        if (a) {
            CombatEvent e;
            e.kind = CombatEventKind::Turn;
            e.actor = a->id;
            emit(e);
        }
    }
    void remove_weapon(CombatActor &a, int id, bool all = false) {
        for (int i = 0; i < a.weapon_count;) {
            if (a.weapons[i].id == id) {
                for (int j = i + 1; j < a.weapon_count; ++j)
                    a.weapons[j - 1] = a.weapons[j];
                --a.weapon_count;
                if (!all)
                    return;
            } else
                ++i;
        }
    }
    void ammo(CombatActor &a, CombatWeapon w, int dist) {
        if (!player(a))
            return;
        int id = ammo_item_for(w.id);
        if (id >= 0) {
            auto &q = g.equipment_quantities[id];
            q = (q - 1) & 255;
            if (q)
                return;
            int n = 0;
            for (int i = 0; i < std::min<int32_t>(g.party.party_size, int32_t(g.party.character_count));
                 ++i) {
                if (unequip_item_by_id(g, i, w.id) == EquipSlot::None)
                    continue;
                ++n;
                for (int j = 0; j < s.count; ++j)
                    if (s.actors[j].member == i)
                        remove_weapon(s.actors[j], w.id);
            }
            g.equipment_quantities[w.id] = (g.equipment_quantities[w.id] + n) & 255;
        } else if (is_thrown_weapon(w.id) && dist > 1) {
            auto &q = g.equipment_quantities[w.id];
            if (q > 0)
                --q;
            else {
                unequip_item_by_id(g, a.member, w.id, true);
                remove_weapon(a, w.id, true);
            }
        }
    }
    bool hit(CombatActor &a, CombatActor &b, int id) {
        if (id == 35 || id == 39 || id == 40)
            return true;
        int ds = (b.sleeping || (!player(b) && (c.turn.time_spell == 'T' || b.enemy->index == 26)))
                     ? 1
                     : b.speed;
        int as = (!player(a) ? bool(a.enemy->abilities & 0x8000)
                             : table(c.tables.strength_range, c.tables.count, id - 1) == 8)
                     ? a.strength
                     : a.speed;
        return r30() >= (ds - as + 30) / 2;
    }
    void attacked(CombatActor &a, CombatActor &b, int dmg, int hit_value = 1, bool grazed = false,
                  const char *text = nullptr) {
        CombatEvent e;
        e.kind = CombatEventKind::Attacked;
        e.actor = a.id;
        e.target = b.id;
        e.damage = dmg;
        e.hit = int8_t(hit_value);
        e.grazed = grazed ? 1 : -1;
        e.text = text;
        emit(e);
    }
    void graze(CombatActor &a, CombatActor &b) {
        char text[160];
        std::snprintf(text, sizeof(text), "%s grazed!", name(b));
        attacked(a, b, 0, 1, true, text);
    }
    void wound(CombatActor &b) {
        int base = b.max_hp >> 2;
        int level = 4;
        b.fleeing = false;
        if (b.hp < base) {
            level = 1;
            b.fleeing = true;
        } else if (b.hp < base * 2) {
            level = 2;
            b.fleeing = rand(0, 256) > 251;
        } else if (b.hp < base * 3)
            level = 3;
        const char *labels[] = {"", " critical!", " heavily wounded!", " lightly wounded!",
                                " barely wounded!"};
        named(b, labels[level], -1, b.id);
    }
    void kill(CombatActor &b, CombatActor &a) {
        b.hp = 0;
        b.status = CombatStatus::Dead;
        if (player(b)) {
            roster_dead(b);
            g.party.characters[b.member].status = 'D';
        }
        char text[160];
        std::snprintf(text, sizeof(text), "%s killed!", name(b));
        CombatEvent e;
        e.kind = CombatEventKind::Died;
        e.actor = a.id;
        e.target = b.id;
        e.x = b.position.x;
        e.y = b.position.y;
        e.text = text;
        emit(e);
        if (!player(b)) {
            auto &d = *b.enemy;
            if (player(a)) {
                int xp = (d.hp >> 2) + 1;
                auto &r = g.party.characters[a.member];
                r.exp = uint16_t(std::min<int32_t>(9999, int(r.exp) + xp));
                s.xp[a.member] += xp;
            }
            int x = b.position.x, y = b.position.y;
            if (!(d.abilities & (0x100 | 0x10)) && inside(x, y)) {
                int k = y * 11 + x;
                if (d.index == 30)
                    s.map.tiles[k] = 76;
                else if (tile(x, y) != 135 && tile(x, y) >= 4) {
                    if (r30() <= d.treasure) {
                        ++s.spoil_chests;
                        s.chest_contents[k] = int16_t(d.treasure);
                        s.loot[k] = r30() < d.treasure ? 129 : 1;
                    } else
                        s.loot[k] = 31;
                }
            }
        }
        latch();
        collapse();
        if (combat_over(s))
            end();
    }
    void damage(CombatActor &a, CombatActor &b, int dmg, bool magic = false) {
        int d = std::max<int32_t>(0, dmg);
        if (!player(b)) {
            if ((b.enemy->abilities & 0x2000) && !magic)
                d /= 2;
            if (b.enemy->abilities & 0x800)
                d = 0;
        }
        if (d == 99 || b.hp <= d) {
            attacked(a, b, d);
            kill(b, a);
            return;
        }
        b.hp -= d;
        if (player(b)) {
            char text[160];
            bool dragged = a.enemy && a.enemy->index == 45;
            if (dragged) {
                b.dragged = true;
                b.render_tile = 0;
            }
            std::snprintf(text, sizeof(text), "%s%s", name(b),
                          dragged ? " dragged under!" : " hit!");
            CombatEvent e;
            e.kind = CombatEventKind::Attacked;
            e.actor = a.id;
            e.target = b.id;
            e.damage = d;
            e.hit = 1;
            e.dragged = dragged ? 1 : -1;
            e.text = text;
            emit(e);
            g.party.characters[b.member].current_hp = uint16_t(b.hp);
        } else {
            attacked(a, b, d);
            wound(b);
        }
    }
    void poison(CombatActor &a, CombatActor &b) {
        if (player(b) && g.party.characters[b.member].status == 'G') {
            g.party.characters[b.member].status = 'P';
            named(b, " is poisoned!", a.id, b.id);
            return;
        }
        int d = rand(0, 20);
        if (d < 1)
            graze(a, b);
        else
            damage(a, b, d);
    }
    void strike(CombatActor &a, CombatActor &b, CombatWeapon w) {
        if (a.enemy && (a.enemy->abilities & (2 | 0x400)) && rand(0, 3) != 0) {
            poison(a, b);
            return;
        }
        if (a.enemy && a.enemy->index == 28 && !b.sleeping) {
            sleep(b);
            named(b, " slept!", a.id, b.id);
            return;
        }
        int base = a.attack;
        if (player(a)) {
            base = w.id == 39 ? 99 : w.id == 40 ? 0 : w.id == 255 ? 1 : w.attack;
            if (w.id == 39) {
                message("Thy sword hath shattered!", a.id);
                remove_weapon(a, w.id);
            } else if (w.id != 40 && w.id != 255 && base > 1 && base != 99)
                base = rand(1, base);
        }
        int dmg = base == 99 ? 99 : base - (b.defense > 0 ? rand(1, b.defense) : 0);
        if (dmg < 1)
            graze(a, b);
        else
            damage(a, b, dmg, player(a) && w.id >= 35);
    }
    CombatPoint adjacent(CombatActor &b, CombatActor &a) {
        for (;;) {
            int x = b.position.x + rand(1, 3) - 2, y = b.position.y + rand(1, 3) - 2;
            if (inside(x, y) && (x != a.position.x || y != a.position.y))
                return {int16_t(x), int16_t(y)};
        }
    }
    void attack_with(CombatActor &a, CombatActor &b, CombatWeapon w, bool enemy_ranged = false) {
        bool ranged = w.range > 1;
        if (ranged)
            ammo(a, w, combat_distance(a.position.x - b.position.x, a.position.y - b.position.y));
        if (hit(a, b, w.id)) {
            strike(a, b, w);
            return;
        }
        if (ranged) {
            auto p = adjacent(b, a);
            auto *v = occupant(p.x, p.y);
            if (v && v != &a) {
                strike(a, *v, w);
                return;
            }
        }
        char text[160];
        std::snprintf(text, sizeof(text), "%s missed!", name(b));
        attacked(a, b, -1, 0, false, enemy_ranged ? nullptr : text);
    }
    bool disabled(CombatActor &a) {
        if (a.dragged) {
            message("ARGH!", a.id);
            if (a.speed > r30()) {
                a.dragged = false;
                a.render_tile = -1;
                named(a, " regurgitated!", a.id);
            }
            advance();
            return true;
        }
        if (a.sleeping) {
            if (rand(0, 255) < 16)
                wake(a);
            message("Zzzzz...", a.id);
            advance();
            return true;
        }
        return false;
    }
    void queue(CombatActor &a) {
        if (s.queue_live)
            return;
        s.queue_live = true;
        s.queue_count = 0;
        for (int i = 0; i < a.weapon_count; ++i)
            if (a.weapons[i].attack > 0)
                s.queue[s.queue_count++] = a.weapons[i];
        if (!s.queue_count) {
            s.queue[0] = {};
            s.queue_count = 1;
        }
    }
    CombatWeapon shift() {
        CombatWeapon w{};
        if (s.queue_count) {
            w = s.queue[0];
            for (int i = 1; i < s.queue_count; ++i)
                s.queue[i - 1] = s.queue[i];
            --s.queue_count;
        }
        return w;
    }
    void escape(CombatActor &a, int dir) {
        if ((c.turn.transport_tile & 0xf8) == 0x20) {
            message("Stay with ship!", a.id);
            return;
        }
        if (s.room && s.escape_border >= 0 && s.escape_border != dir) {
            message("All must use the same exit!", a.id);
            return;
        }
        s.escape_border = int16_t(dir);
        bool leaving = !any_side(s, false);
        a.status = CombatStatus::Fled;
        message(leaving ? "Leave!" : "Escape!", a.id);
        advance();
    }
    void move(CombatActor &a, int dir) {
        static constexpr int dx[] = {1, -1, 0, 0, 1, -1, 1, -1}, dy[] = {0, 0, 1, -1, -1, -1, 1, 1};
        int x = a.position.x + dx[dir], y = a.position.y + dy[dir];
        if (!inside(x, y)) {
            int border = y < 0 ? 3 : y > 10 ? 2 : x > 10 ? 0 : 1;
            escape(a, border);
            return;
        }
        if (!free(a, x, y)) {
            message("Blocked!");
            return;
        }
        a.position = {int16_t(x), int16_t(y)};
        triggers(x, y);
        CombatEvent e;
        e.kind = CombatEventKind::Moved;
        e.actor = a.id;
        e.x = x;
        e.y = y;
        emit(e);
        advance();
    }
    bool interference(CombatActor &a, int id) {
        if (id != 26 && id != 28 && id != 36 && id != 19 && id != 17)
            return false;
        if (a.last_attacker < 1 || c.turn.time_spell == 'T')
            return false;
        for (int i = 0; i < s.count; ++i) {
            auto &b = s.actors[i];
            if (b.id == a.last_attacker && active(b) && !b.sleeping &&
                combat_distance(a.position.x - b.position.x, a.position.y - b.position.y) == 1) {
                named(b, " interferes!", a.id);
                return true;
            }
        }
        return false;
    }
    void attack(CombatActor &a, int tx, int ty, bool cancel) {
        queue(a);
        auto w = shift();
        int range = std::max<int32_t>(1, w.range);
        if (cancel) {
            if (range <= 1)
                message("Nothing!", a.id);
        } else if (range > 1) {
            auto *target = occupant(tx, ty);
            a.last_target = target ? target->id : -1;
            if (!interference(a, w.id)) {
                int x = tx, y = ty;
                if (w.id != 25 && w.id != 34) {
                    int dx = tx - a.position.x, dy = ty - a.position.y,
                        steps = std::max<int32_t>(std::abs(dx), std::abs(dy));
                    x = a.position.x;
                    y = a.position.y;
                    for (int i = 1; i <= std::min<int32_t>(steps, range); ++i) {
                        x = int(std::floor(a.position.x + double(dx) * i / steps + 0.5));
                        y = int(std::floor(a.position.y + double(dy) * i / steps + 0.5));
                        int t = tile(x, y);
                        if (t < 0 || size_t(t) >= sizeof(kCombatTileFlags) ||
                            !(kCombatTileFlags[t] & 4))
                            break;
                    }
                }
                auto *b = occupant(x, y);
                if (b && b != &a)
                    attack_with(a, *b, w);
                else {
                    ammo(a, w, combat_distance(a.position.x - x, a.position.y - y));
                    CombatEvent e;
                    e.kind = CombatEventKind::Projectile;
                    e.actor = a.id;
                    e.x = x;
                    e.y = y;
                    e.hit = 0;
                    emit(e);
                }
            }
        } else {
            auto *b = occupant(tx, ty);
            if (!b || b == &a)
                message("Nothing!", a.id);
            else if (combat_distance(a.position.x - tx, a.position.y - ty) > 1)
                message("Out of range.", a.id);
            else {
                a.last_target = b->id;
                attack_with(a, *b, w);
            }
        }
        if (!s.queue_count || combat_over(s))
            advance();
    }
    CombatActor *target(CombatActor &a) {
        bool confused = c.turn.time_spell == 'C' && r30() > a.intelligence;
        CombatActor *best = nullptr;
        int dist = 99;
        for (int i = s.count - 1; i >= 0; --i) {
            auto &b = s.actors[i];
            if (&a == &b || !active(b))
                continue;
            if (confused ? party_side(b) != party_side(a) : party_side(b) == party_side(a))
                continue;
            if (b.invisible && (!a.enemy || a.enemy->index != 47) && g.position.map.location != 40)
                continue;
            int d = combat_distance(a.position.x - b.position.x, a.position.y - b.position.y);
            if (d < dist) {
                dist = d;
                best = &b;
            }
        }
        return best;
    }
    bool los(CombatActor &a, CombatActor &b) {
        int dx = b.position.x - a.position.x, dy = b.position.y - a.position.y,
            n = std::max<int32_t>(std::abs(dx), std::abs(dy));
        for (int i = 1; i < n; ++i) {
            int x = int(std::floor(a.position.x + double(dx) * i / n + 0.5)),
                y = int(std::floor(a.position.y + double(dy) * i / n + 0.5));
            int t = tile(x, y);
            if (t < 0 || !(kCombatLos[(t & 255) >> 3] & (128 >> (t & 7))))
                return false;
        }
        return true;
    }
    bool enemy_attack(CombatActor &a) {
        auto *b = target(a);
        if (!b) {
            for (int i = 0; i < s.count; ++i)
                if (!player(s.actors[i]) && active(s.actors[i])) {
                    s.actors[i].hp = 1;
                    s.actors[i].fleeing = true;
                }
            return false;
        }
        int dist = combat_distance(a.position.x - b->position.x, a.position.y - b->position.y);
        int reach = player(a) ? 1 : a.range;
        if (a.charmed) {
            if (dist != 1)
                return false;
            b->last_attacker = a.id;
            if (hit(a, *b, 33))
                strike(a, *b, {33, a.attack, 1});
            return true;
        }
        if (dist > reach)
            return false;
        if (dist > 1) {
            if (a.enemy->index != 26 && rand(0, 255) >= 128)
                return false;
            if (!los(a, *b))
                return false;
            attack_with(a, *b, {-1, a.attack, a.range}, true);
            return true;
        }
        b->last_attacker = a.id;
        if (!hit(a, *b, -1))
            return true;
        if (a.enemy && (a.enemy->abilities & 0x200) && rand(0, 3) != 0 && g.food > 0) {
            g.food = uint16_t(std::max<int32_t>(0, int(g.food) - 5));
            char text[160];
            std::snprintf(text, sizeof(text), "A %s stole some food!", name(a));
            message(text, a.id);
            return true;
        }
        strike(a, *b, {-1, a.attack, 1});
        return true;
    }
    bool enemy_cell(CombatActor &a, int x, int y, bool flee) {
        if (!inside(x, y)) {
            if (!flee && !a.fleeing)
                return false;
            a.status = CombatStatus::Fled;
            a.hp = 0;
            named(a, " escapes!", a.id);
            if (combat_over(s))
                end();
            return true;
        }
        if (!free(a, x, y))
            return false;
        a.position = {int16_t(x), int16_t(y)};
        triggers(x, y);
        CombatEvent e;
        e.kind = CombatEventKind::Moved;
        e.actor = a.id;
        e.x = x;
        e.y = y;
        emit(e);
        return true;
    }
    bool enemy_move(CombatActor &a, bool flee) {
        if (a.enemy && (a.enemy->stationary || a.enemy->index == 26 || a.enemy->index == 27))
            return false;
        auto *b = target(a);
        if (!b && !flee)
            return false;
        auto sign = [](int v) { return (v > 0) - (v < 0); };
        int dx, dy;
        if (b) {
            dx = sign(b->position.x - a.position.x);
            dy = sign(b->position.y - a.position.y);
            if (flee || a.fleeing) {
                dx = -dx;
                dy = -dy;
            }
        } else {
            dx = -sign(5 - a.position.x);
            if (!dx)
                dx = 1;
            dy = -sign(5 - a.position.y);
        }
        bool first = rand(0, 255) > 127;
        if (first && dx && enemy_cell(a, a.position.x + dx, a.position.y, flee))
            return true;
        if (dy && enemy_cell(a, a.position.x, a.position.y + dy, flee))
            return true;
        static constexpr int rx[] = {0, 1, 0, -1}, ry[] = {1, 0, -1, 0};
        for (int i = 0; i < 4; ++i) {
            int r = rand(0, 3);
            if (enemy_cell(a, a.position.x + rx[r], a.position.y + ry[r], flee))
                return true;
        }
        return false;
    }
    void enemy_turn(CombatActor &a) {
        if (c.turn.time_spell == 'T')
            return;
        if (c.turn.time_spell == 'Q' && rand(0, 1) == 0)
            return;
        if (a.sleeping) {
            if (rand(0, 16) == 16) {
                wake(a);
                named(a, " wakes!", a.id);
            } else
                named(a, " sleeps.", a.id);
            return;
        }
        if (a.fleeing) {
            if (!player(a)) {
                if (rand(0, 3) == 3)
                    a.hp = std::min<int32_t>(a.max_hp, a.hp + 1);
                int base = a.max_hp >> 2;
                a.fleeing = a.hp < base;
                if (a.hp >= base && a.hp < base * 2)
                    a.fleeing = rand(0, 256) > 251;
            }
            if (!enemy_move(a, true))
                enemy_attack(a);
            return;
        }
        if (!enemy_attack(a))
            enemy_move(a, false);
    }
};
} // namespace
int32_t combat_distance(int32_t dx, int32_t dy) {
    int rest = dx * dx + dy * dy, n = 0, odd = 1;
    while (rest >= odd) {
        rest -= odd;
        odd += 2;
        ++n;
    }
    return n;
}
bool combat_over(const CombatState &s) { return s.ended || !any_side(s, true); }
CombatActor *current_combat_actor(CombatContext &c) { return Engine(c).current(); }
CombatResult initialize_combat(CombatContext &c, const CombatMap &map, CombatDirection dir,
                               const CombatEnemy *const *enemies, size_t count, bool room) {
    if (int(dir) > 3 || count > 26 || map.unit_count > 16 || map.trigger_count > 8 ||
        &map == &c.combat.map || c.game.party.character_count > 16 || c.game.party.party_size < 0 ||
        c.game.party.party_size > 6)
        return CombatResult::Invalid;
    for (auto n : map.start_count)
        if (n > 6)
            return CombatResult::Invalid;
    for (int t : map.tiles)
        if (t < 0 || size_t(t) >= sizeof(kCombatTileFlags) || kCombatTileFlags[t] == 255)
            return CombatResult::Invalid;
    for (size_t i = 0; i < count; ++i)
        if (!enemies || !enemies[i] || !supported(*enemies[i]))
            return CombatResult::Unsupported;
    for (int i = 0; i < map.unit_count; ++i)
        if (!inside(map.units[i].x, map.units[i].y))
            return CombatResult::Invalid;
    auto &s = c.combat;
    // Construct directly in caller storage; avoid a CombatState-sized stack temporary.
    s.~CombatState();
    new (&s) CombatState();
    s.map = map;
    s.room = room;
    s.rng.seed(c.game.rng.get_seed());
    s.initialized = true;
    std::fill(std::begin(s.loot), std::end(s.loot), int16_t(-1));
    std::fill(std::begin(s.chest_contents), std::end(s.chest_contents), int16_t(-1));
    Engine e(c);
    auto party = party_members(c.game.party);
    int d = int(dir);
    for (int i = 0; i < party.count; ++i) {
        int idx = party.indices[i];
        auto &r = c.game.party.characters[idx];
        if (r.status == 'D')
            continue;
        auto &a = s.actors[s.count++];
        a.id = s.count;
        a.member = uint8_t(idx);
        int n = map.start_count[d];
        a.position = n ? map.starts[d][std::min<int32_t>(i, n - 1)] : CombatPoint{};
        a.hp = r.current_hp;
        a.max_hp = r.max_hp;
        a.strength = r.strength;
        a.dexterity = r.dexterity;
        a.intelligence = r.intelligence;
        a.speed = r.dexterity;
        a.counter = uint8_t(36 - a.speed);
        a.defense = e.defense(r);
        for (int id : {r.helmet, r.weapon, r.shield}) {
            int attack = table(c.tables.attack, c.tables.count, id);
            if (id == 255 || attack <= 0)
                continue;
            int range = table(c.tables.range, c.tables.count, id);
            a.weapons[a.weapon_count++] = {id, attack, range == 0 ? 1 : range};
        }
        if (!a.weapon_count) {
            a.weapons[0] = {};
            a.weapon_count = 1;
        }
        a.attack = a.weapons[0].attack;
        a.range = std::max<int32_t>(1, a.weapons[0].range);
        if (r.status == 'S')
            e.sleep(a);
        else
            e.status_pass(a);
    }
    CombatPoint slots[16];
    bool used[16]{};
    std::copy(std::begin(map.units), std::end(map.units), slots);
    for (int i = map.unit_count - 1; i > 0; --i)
        std::swap(slots[i], slots[e.rand(0, i)]);
    for (size_t n = 0; n < count; ++n) {
        int chosen = -1;
        for (int i = 0; i < map.unit_count; ++i)
            if (!used[i] && e.passable(enemies[n], slots[i].x, slots[i].y)) {
                chosen = i;
                break;
            }
        if (chosen < 0)
            for (int i = 0; i < map.unit_count; ++i)
                if (!used[i]) {
                    chosen = i;
                    break;
                }
        if (chosen < 0)
            break;
        used[chosen] = true;
        auto &a = s.actors[s.count++];
        auto &def = *enemies[n];
        a.enemy = &def;
        a.id = s.count;
        a.position = slots[chosen];
        a.hp = a.max_hp = def.hp;
        a.strength = def.strength;
        a.dexterity = def.dexterity;
        a.intelligence = def.intelligence;
        a.defense = def.armor;
        a.attack = def.damage;
        a.range = std::max<int32_t>(1, def.range);
        int v = (def.dexterity + e.rand(0, 7) - 4) & 255;
        a.speed = uint8_t(v > 30 ? def.dexterity : v);
        a.counter = uint8_t(36 - a.speed);
    }
    s.victory = !any_side(s, false) && any_side(s, true, true);
    return CombatResult::Ok;
}
CombatResult combat_action(CombatContext &c, CombatAction action, int32_t x, int32_t y) {
    if (!c.combat.initialized || int(action) > int(CombatAction::EnemyStep))
        return CombatResult::Invalid;
    if (action == CombatAction::Move && (x < 0 || x > 7))
        return CombatResult::Invalid;
    if (action == CombatAction::Escape && (x < 0 || x > 3))
        return CombatResult::Invalid;
    if (action == CombatAction::Attack && (x < 0 || x > 10 || y < 0 || y > 10))
        return CombatResult::Invalid;
    Engine e(c);
    auto *a = e.current();
    if (!a)
        return CombatResult::Ok;
    if (action == CombatAction::EnemyStep) {
        if (!player(*a) || a->charmed) {
            e.enemy_turn(*a);
            e.advance();
        }
        return CombatResult::Ok;
    }
    if (!player(*a))
        return CombatResult::Ok;
    if (action == CombatAction::Escape) {
        e.escape(*a, x);
        return CombatResult::Ok;
    }
    if (action == CombatAction::EscapeQuick) {
        if (c.combat.room)
            e.message("Escape-Not here!", -1, -1, CombatEventKind::Echo);
        else if (any_side(c.combat, false))
            e.message("Escape-Not yet!", -1, -1, CombatEventKind::Echo);
        else {
            e.message("Escape!", -1, -1, CombatEventKind::Echo);
            for (int i = 0; i < c.combat.count; ++i)
                if (player(c.combat.actors[i]) && active(c.combat.actors[i]))
                    c.combat.actors[i].status = CombatStatus::Fled;
            c.combat.current = -1;
            c.combat.queue_live = false;
            c.combat.queue_count = 0;
            e.end();
        }
        return CombatResult::Ok;
    }
    if (action == CombatAction::Yield) {
        e.advance();
        return CombatResult::Ok;
    }
    if (e.disabled(*a))
        return CombatResult::Ok;
    switch (action) {
    case CombatAction::Move:
        e.move(*a, x);
        break;
    case CombatAction::Attack:
        e.attack(*a, x, y, false);
        break;
    case CombatAction::AttackCancel:
        e.attack(*a, 0, 0, true);
        break;
    case CombatAction::Pass:
        e.message("Pass", a->id, -1, CombatEventKind::Echo);
        e.advance();
        break;
    default:
        return CombatResult::Invalid;
    }
    return CombatResult::Ok;
}
CombatResult start_encounter_combat(CommandContext &world, CombatState &state,
                                    const CombatResources &res, int32_t enemy, int32_t tile,
                                    int32_t map_override, CombatDirection entry, bool intro) {
    if (world.combat || enemy < 0 || size_t(enemy) >= res.enemy_count || !res.enemies ||
        !res.enemies[enemy])
        return CombatResult::Invalid;
    int index = map_override >= 0 ? map_override
                                  : std::max<int32_t>(0, tile >= 0 && size_t(tile) < sizeof(kCombatMapIndex)
                                                    ? int(kCombatMapIndex[tile])
                                                    : -2);
    const CombatMap *map = res.maps && size_t(index) < res.map_count ? res.maps[index] : nullptr;
    if (!map && res.maps && res.map_count)
        map = res.maps[0];
    if (!map)
        return CombatResult::MissingMap;
    static constexpr int friends[] = {33, 1,  1,  3,  4,  4,  4,  4,  4,  4,  10, 4,
                                      12, 13, 14, 15, 17, 16, 17, 19, 33, 21, 20, 33,
                                      24, 26, 35, 21, 21, 24, 30, 24, 41, 0,  22, 36,
                                      35, 23, 39, 39, 40, 20, 42, 43, 44, 45, 20, 38};
    const auto *base = res.enemies[enemy];
    if (base->group_name && std::strlen(base->group_name) > 160)
        return CombatResult::Invalid;
    if (!supported(*base))
        return CombatResult::Unsupported;
    // Reject a potentially unsupported companion before touching either stream.
    int fi = base->index >= 0 && base->index < 48 ? friends[base->index] : base->index;
    const auto *companion =
        fi >= 0 && size_t(fi) < res.enemy_count && res.enemies[fi] ? res.enemies[fi] : base;
    if (!supported(*companion))
        return CombatResult::Unsupported;
    if (map->unit_count > 16 || map->trigger_count > 8 || int(entry) > 3 ||
        world.game.party.party_size > 6 || world.game.party.party_size < 0 ||
        world.game.party.character_count > 16)
        return CombatResult::Invalid;
    for (int t : map->tiles)
        if (t < 0 || size_t(t) >= sizeof(kCombatTileFlags) || kCombatTileFlags[t] == 255)
            return CombatResult::Invalid;
    for (int i = 0; i < map->unit_count; ++i)
        if (!inside(map->units[i].x, map->units[i].y))
            return CombatResult::Invalid;
    for (auto n : map->start_count)
        if (n > 6)
            return CombatResult::Invalid;
    auto draw = [&](int lo, int hi) {
        int v = world.game.rng.next(lo, hi).value;
        if (world.rng_trace.emit)
            world.rng_trace.emit(world.rng_trace.context, "encounter", lo, hi, v);
        return v;
    };
    if (res.reset_doors)
        res.reset_doors(res.context);
    int n = base->max_per_map;
    int loc = world.game.position.map.location;
    if (loc >= 1 && loc <= 32 && base->index != 12)
        n = 1;
    else if (n != 1 && n != 8 && n != 16) {
        n = draw(1, std::max<int32_t>(1, n));
        n = draw(1, n);
        if (n > 25)
            n = 26;
    }
    const CombatEnemy *group[26]{};
    group[0] = base;
    for (int i = 1; i < n; ++i)
        group[i] = i < n / 4 + 1 && draw(0, 8) == 0 ? companion : base;
    CombatContext c{world.game, world.turn, state, res.tables, {}, world.rng_trace};
    auto result = initialize_combat(c, *map, entry, group, size_t(n));
    if (result != CombatResult::Ok)
        return result;
    world.combat = true;
    world.commands.pending_camp_enemy = -1;
    if (res.remove_enemy)
        res.remove_enemy(res.context, enemy);
    auto emit = [&](GameEventKind kind, const char *text = nullptr) {
        GameEvent e;
        e.kind = kind;
        e.text = text;
        if (world.events.emit)
            world.events.emit(world.events.context, e);
    };
    if (intro)
        emit(GameEventKind::Message, "Attacked!\n");
    if (base->group_name && *base->group_name) {
        char text[192];
        int spaces = std::max<int32_t>(0, (16 - int(std::strlen(base->group_name))) / 2);
        std::snprintf(text, sizeof(text), "%*s%s\n", spaces, "", base->group_name);
        emit(GameEventKind::Message, text);
    }
    emit(GameEventKind::Message, "*** CONFLICT ***\n");
    Rand rand{&world, [](void *p, int32_t lo, int32_t hi) -> int32_t {
                  auto &w = *static_cast<CommandContext *>(p);
                  int v = w.game.rng.next(lo, hi).value;
                  if (w.rng_trace.emit)
                      w.rng_trace.emit(w.rng_trace.context, "encounter", lo, hi, v);
                  return v;
              }};
    auto expired = roll_ring_expiry(world.game, rand);
    for (int i = 0; i < expired.count; ++i) {
        unequip_item_by_id(world.game, expired.members[i], expired.ids[i]);
        emit(GameEventKind::Message, "A ring has vanished!\n");
        emit(GameEventKind::Sfx, "ring-vanishes");
        emit(GameEventKind::PartyChanged);
    }
    emit(GameEventKind::CombatStarted);
    return CombatResult::Ok;
}
CombatResult finish_encounter_combat(CommandContext &world, CombatState &state) {
    if (!world.combat || !state.initialized)
        return CombatResult::Invalid;
    auto emit = [&](GameEventKind kind, const char *text = nullptr) {
        GameEvent e;
        e.kind = kind;
        e.text = text;
        if (world.events.emit)
            world.events.emit(world.events.context, e);
    };
    if (!state.victory)
        emit(GameEventKind::Message, "BATTLE IS LOST!");
    for (int i = 0; i < state.count; ++i) {
        auto &a = state.actors[i];
        if (!player(a))
            continue;
        auto &r = world.game.party.characters[a.member];
        r.current_hp = uint16_t(std::max<int32_t>(a.status == CombatStatus::Dead ? 0 : 1, a.hp));
        if (a.status == CombatStatus::Dead)
            r.status = 'D';
    }
    world.game.rng.seed(state.rng.get_seed());
    world.combat = false;
    world.combat_context = nullptr;
    state.initialized = false;
    emit(GameEventKind::CombatEnded);
    if (world.services.effect)
        world.services.effect(world.services.context, CommandEffect::Refuge, world.events);
    return CombatResult::Ok;
}
} // namespace openu5
