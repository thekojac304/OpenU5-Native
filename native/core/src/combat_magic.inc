// Engine methods translated from Combat.playerCast and generic enemy abilities.
Rand magic_rng() {
    return {this, [](void *p, int32_t lo, int32_t hi) -> int32_t {
                return static_cast<Engine *>(p)->rand(lo, hi);
            }};
}
const CombatEnemy *definition(int index) {
    return c.enemy_defs && index >= 0 && size_t(index) < c.enemy_def_count ? c.enemy_defs[index]
                                                                           : nullptr;
}
// Batch 21A.  The roster ceiling lives HERE, at the one growth site, instead of
// only in the per-action precondition that used to guard it.
//
// The 1988 combatant table is a fixed 32 x 8-byte array at DS:0xBA14 -- slots
// 0..5 party, 6..31 enemies (re/notes/combat.md section 1).  It cannot hold a
// 33rd combatant, so a divide or a summon with no free slot simply does not
// happen.  CombatActorStorage gives the device 22 inline + 32 overflow = 54,
// comfortably above that ceiling, so refusing here can never drop a spawn the
// original would have made -- and it makes the unchecked `s.actors[s.count++]`
// that used to live on this line impossible to reach out of bounds no matter
// what any caller does first.  The only other `s.count++` is in
// initialize_combat(), already bounded by party <= 6 plus map.unit_count <= 16
// = kCombatActors.
CombatActor *spawn(const CombatEnemy &d, CombatPoint p) {
    if (s.count >= s.actors.capacity())
        return nullptr;
    auto &a = s.actors[s.count++];
    a = CombatActor{};
    a.id = s.count;
    a.enemy = &d;
    a.position = p;
    a.hp = a.max_hp = d.hp;
    a.strength = d.strength;
    a.dexterity = d.dexterity;
    a.intelligence = d.intelligence;
    a.defense = d.armor;
    a.attack = d.damage;
    a.range = std::max<int32_t>(1, d.range);
    int v = (d.dexterity + rand(0, 7) - 4) & 255;
    a.speed = uint8_t(v > 30 ? d.dexterity : v);
    a.counter = uint8_t(36 - a.speed);
    return &a;
}
CombatPoint board_cell() {
    int x = rand(0, 15), y = rand(0, 15);
    return {int16_t(x), int16_t(y)};
}
bool free_def(const CombatEnemy *d, CombatPoint p) {
    return passable(d, p.x, p.y) && !field_blocks(p.x, p.y) && !occupant(p.x, p.y);
}
bool summon_cell(const CombatEnemy *d, int attempts, CombatPoint &p) {
    for (int i = 0; i < attempts; ++i) {
        p = board_cell();
        if (free_def(d, p))
            return true;
    }
    return false;
}
void divide(CombatActor &a) {
    for (int i = 0; i < 8; ++i) {
        CombatPoint p;
        do {
            int x = a.position.x + rand(1, 3) - 2, y = a.position.y + rand(1, 3) - 2;
            p = {int16_t(x), int16_t(y)};
        } while (!inside(p.x, p.y));
        if (!free(a, p.x, p.y))
            continue;
        auto *clone = spawn(*a.enemy, p);
        if (!clone)
            return;
        clone->hp = a.hp;
        clone->max_hp = a.max_hp;
        named(a, " divides!", a.id);
        return;
    }
}
bool saving(CombatActor &a, CombatActor &b) {
    return (b.intelligence - a.intelligence + 30) / 2 > r30();
}
CombatActor *nearest(CombatActor &a) {
    CombatActor *best = nullptr;
    int dist = 99;
    for (int i = 0; i < s.count; ++i) {
        auto &b = s.actors[i];
        if (&a == &b || !active(b) || party_side(a) == party_side(b) || b.invisible)
            continue;
        int d = combat_distance(a.position.x - b.position.x, a.position.y - b.position.y);
        if (d < dist) {
            dist = d;
            best = &b;
        }
    }
    return best;
}
void spell_flight(CombatActor &a, CombatPoint p, int id) {
    static constexpr int attacks[] = {16, 30, 99, 18, 0, 21, 0};
    if (id < 48 || id > 54) {
        message("No effect!", a.id);
        return;
    }
    CombatWeapon w{id, attacks[id - 48], 15};
    int dx = p.x - a.position.x, dy = p.y - a.position.y,
        n = std::max<int32_t>(std::abs(dx), std::abs(dy));
    int x = a.position.x, y = a.position.y;
    for (int i = 1; i <= std::min<int32_t>(n, 15); ++i) {
        x = int(std::floor(a.position.x + double(dx) * i / n + 0.5));
        y = int(std::floor(a.position.y + double(dy) * i / n + 0.5));
        int t = tile(x, y);
        if (t < 0 || size_t(t) >= sizeof(kCombatTileFlags) || !(kCombatTileFlags[t] & 4))
            break;
    }
    auto *b = occupant(x, y);
    if (b && b != &a)
        attack_with(a, *b, w);
    else {
        CombatEvent ev;
        ev.kind = CombatEventKind::Projectile;
        ev.actor = a.id;
        ev.x = x;
        ev.y = y;
        ev.hit = 0;
        emit(ev);
    }
}
void line_cell(CombatActor &a, CombatActor &b, int mode) {
    int type = player(b) ? b.member : b.enemy->index;
    bool immune = type == 14 || type == 15 || type == 47;
    if (mode == 1 || mode == 4) {
        if (saving(a, b) || immune)
            return;
        if (mode == 1) {
            sleep(b);
            named(b, " slept!", a.id, b.id);
        } else
            damage(a, b, 99);
    } else if (mode == 2) {
        if (r30() >= b.intelligence)
            poison(a, b);
    } else if (mode == 3)
        damage(a, b, rand(0, 30));
}
void spray(CombatActor &a, CombatPoint aim, int mode) {
    int dx = (aim.x > a.position.x) - (aim.x < a.position.x),
        dy = (aim.y > a.position.y) - (aim.y < a.position.y);
    if (!dx && !dy)
        return;
    CombatEvent ev;
    ev.kind = CombatEventKind::LineSpray;
    ev.actor = a.id;
    ev.x = dx;
    ev.y = dy;
    ev.mode = int16_t(mode);
    emit(ev);
    constexpr int curve[] = {10,  12, 14, 16, 20, 25, 35, 50, 80, 190, 2000,
                             190, 80, 50, 35, 25, 20, 16, 14, 12, 10};
    int bx = a.position.x * 16, by = a.position.y * 16, ax = dy ? 0 : dx, ay = dy;
    int ox = dy ? bx + 8 : dx < 0 ? bx : bx + 16, oy = dy ? (dy < 0 ? by : by + 16) : by + 8;
    int plx = dy ? (dy < 0 ? -1 : 1) : 0, ply = dy ? 0 : dx < 0 ? 1 : -1;
    bool seen[121]{};
    CombatPoint cells[63];
    int count = 0;
    for (int i = 0; i < 21; ++i) {
        int x = ox, y = oy, weight = curve[i], px = i < 10 ? plx : -plx, py = i < 10 ? ply : -ply;
        for (int step = 0; step < 352; ++step) {
            if (x < 0 || x > 174 || y < 0 || y > 174)
                break;
            if (y & 1) {
                int cx = x >> 4, cy = y >> 4, t = tile(cx, cy);
                if (t < 0 || !(kCombatLos[(t & 255) >> 3] & (128 >> (t & 7))))
                    break;
                int key = cy * 11 + cx;
                if (count < 63 && !seen[key]) {
                    seen[key] = true;
                    cells[count++] = {int16_t(cx), int16_t(cy)};
                }
            }
            weight -= 10;
            x += ax;
            y += ay;
            if (weight < 1) {
                x += px;
                y += py;
                weight += curve[i];
            }
        }
    }
    // A spell can create split actors in cells that will be visited later.
    // At most 63 distinct actors are hit, so this scratch is independent of
    // storage capacity.
    int32_t hit_ids[63];
    int hits = 0;
    for (int i = 0; i < count; ++i) {
        auto *b = occupant(cells[i].x, cells[i].y);
        if (!b || !active(*b))
            continue;
        bool hit = false;
        for (int j = 0; j < hits; ++j)
            if (hit_ids[j] == b->id)
                hit = true;
        if (hit)
            continue;
        hit_ids[hits++] = b->id;
        line_cell(a, *b, mode);
    }
}
void cast_effect(CombatActor &a, SpellEffect fx, const CombatPoint *aim) {
    auto kind = fx.kind;
    if (kind == MagicEffect::Attack) {
        auto *b = aim ? nullptr : nearest(a);
        if (!aim && !b)
            message("Nothing!", a.id);
        else
            spell_flight(a, aim ? *aim : b->position, fx.value);
    } else if (kind == MagicEffect::Quake || kind == MagicEffect::Fear ||
               kind == MagicEffect::Repel) {
        if (kind == MagicEffect::Quake) {
            CombatEvent ev;
            ev.kind = CombatEventKind::Quake;
            ev.actor = a.id;
            emit(ev);
        }
        bool side = party_side(a);
        int count = s.count;
        for (int i = 0; i < count; ++i) {
            auto &b = s.actors[i];
            if (&b == &a || party_side(b) == side || !active(b))
                continue;
            if (kind == MagicEffect::Quake) {
                if (b.intelligence <= r30())
                    damage(a, b, rand(1, 20));
            } else {
                if (kind == MagicEffect::Repel && (!b.enemy || !(b.enemy->abilities & 0x2000)))
                    continue;
                if (!saving(a, b)) {
                    b.hp = 1;
                    b.fleeing = true;
                }
            }
        }
    } else if (kind == MagicEffect::Line) {
        if (aim)
            spray(a, *aim, fx.value);
    } else if (kind == MagicEffect::Charm || kind == MagicEffect::Polymorph ||
               kind == MagicEffect::Illusion) {
        auto *b = aim ? occupant(aim->x, aim->y) : nearest(a);
        if (!b || b == &a) {
            if (kind != MagicEffect::Illusion)
                message("Nothing!", a.id);
            return;
        }
        if (kind == MagicEffect::Illusion) {
            if (!b->enemy)
                return;
            bool exists = false;
            for (int y = 0; y < 11; ++y)
                for (int x = 0; x < 11; ++x)
                    if (free_def(b->enemy, {int16_t(x), int16_t(y)}))
                        exists = true;
            if (!exists)
                return;
            CombatPoint p;
            do {
                p = board_cell();
            } while (!free_def(b->enemy, p));
            auto &clone = s.actors[s.count++];
            clone = *b;
            clone.id = s.count;
            clone.position = p;
        } else {
            if (saving(a, *b))
                return;
            if (kind == MagicEffect::Charm) {
                b->charmed = !b->charmed;
                named(*b, " charmed!", a.id, b->id);
            } else if (auto *rat = definition(20)) {
                b->status = CombatStatus::Dead;
                b->hp = 0;
                spawn(*rat, b->position);
            }
        }
    } else if (kind == MagicEffect::Summon || kind == MagicEffect::Swarms ||
               kind == MagicEffect::Daemon) {
        auto *d = definition(kind == MagicEffect::Daemon   ? 38
                             : kind == MagicEffect::Swarms ? 31
                                                           : fx.value);
        CombatPoint p;
        if (!d || !summon_cell(d, 8, p))
            return;
        int n = kind == MagicEffect::Swarms ? fx.extra : 1;
        for (int i = 0; i < n; ++i) {
            auto *b = spawn(*d, p);
            if (!b)
                break;
            b->charmed = kind != MagicEffect::Daemon || fx.value != 0 || r30() < a.intelligence;
            if (!b->charmed)
                message("Oops...", a.id);
        }
    } else if (kind == MagicEffect::Blink) {
        CombatPoint p;
        if (summon_cell(nullptr, 7, p)) {
            a.position = p;
            triggers(p.x, p.y);
        }
    } else if (kind == MagicEffect::Invisible) {
        a.invisible = true;
        a.render_tile = 29;
    } else if (kind == MagicEffect::Reveal) {
        for (int i = 0; i < s.count; ++i)
            if (!player(s.actors[i]))
                s.actors[i].invisible = false;
    } else if (kind == MagicEffect::Dispel) {
        auto p = aim ? *aim : a.position;
        for (int i = 0; i < s.field_count; ++i)
            if (s.fields[i].position.x == p.x && s.fields[i].position.y == p.y) {
                for (int j = i + 1; j < s.field_count; ++j)
                    s.fields[j - 1] = s.fields[j];
                --s.field_count;
                message("Success!", a.id);
                return;
            }
        message("Failed!", a.id);
    }
}
bool enemy_special(CombatActor &a) {
    const auto &d = *a.enemy;
    if (d.abilities & 0x4000) {
        int slot = rand(0, 31);
        if (slot < s.count) {
            auto &b = s.actors[slot];
            if (player(b) && active(b) && !b.charmed && !b.sleeping && !b.invisible) {
                if ((int(g.party.characters[b.member].intelligence) - d.intelligence + 30) / 2 >
                    r30())
                    return true;
                b.charmed = true;
                named(b, " possessed!", a.id, b.id);
                if (d.index == 38)
                    a.status = CombatStatus::Fled;
                return true;
            }
        }
    }
    if ((d.abilities & 8) && rand(0, 255) < 32) {
        a.invisible = !a.invisible;
        named(a, a.invisible ? " disappears!" : " reappears!", a.id);
        return true;
    }
    if ((d.abilities & 4) && rand(0, 255) < 32) {
        auto *daemon = definition(38);
        CombatPoint p;
        if (daemon && summon_cell(daemon, 1, p)) {
            spawn(*daemon, p);
            named(a, " gates in a daemon!", a.id);
            return true;
        }
    }
    return false;
}
