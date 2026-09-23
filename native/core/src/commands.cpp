#include "openu5/look.h"
#include "openu5/world_terrain.h"
#include "openu5/commands.h"
#include "openu5/combat.h"
#include "openu5/dungeon.h"
#include "openu5/rest.h"
#include "openu5/transport.h"
#include "openu5/dialogue_orchestration.h"
#include "openu5/shop_orchestration.h"
#include "openu5/shrine.h"
#include "openu5/quest_world.h"
#include "openu5/blackthorn.h"
#include "openu5/outdoor.h"
#include "openu5/world_commands.h"
#include "openu5/loot.h"
#include <cstdio>
#include <algorithm>
namespace openu5 {
namespace {
struct Runner {
    CommandContext &c;
    ActionResult result{};
    Rand rand;
    Position attempted_target{};
    EventSink destination = c.events; // Stable downstream sink across nested subsystem handoffs.
    EventSink sink() {
        return {this, [](void *p, const GameEvent &e) { static_cast<Runner *>(p)->emit(e); }};
    }
    void emit(const GameEvent &e) {
        ++result.event_count;
        if (destination.emit)
            destination.emit(destination.context, e);
    }
    void event(GameEventKind kind, const char *text = nullptr) {
        GameEvent e;
        e.kind = kind;
        e.text = text;
        emit(e);
    }
    void message(const char *s) { event(GameEventKind::Message, s); }
    void fire(Command cmd){
        auto &g=c.game;auto *q=c.quest_world;auto projectile=[&](int fx,int fy,int tx,int ty){GameEvent e;e.kind=GameEventKind::CellProjectile;e.projectile={int16_t(fx),int16_t(fy),int16_t(tx),int16_t(ty)};emit(e);};
        if(!g.position.map.location&&cmd.has_direction){
            int ship=c.turn.transport_tile;if((ship&248)!=32){message("What?");return;}
            bool northsouth=int(cmd.direction)<2;if(((ship&1)==0)==northsouth){message("Fire broadsides only!");return;}
            if(!c.outdoor){result.status=CommandStatus::InvalidContext;return;}
            auto &owner=*c.outdoor;auto d=direction_delta(cmd.direction);int range=3,hull=0;bool hit=false;
            for(auto &e:owner.enemies)e.cannon_target=false;
            for(int n=1;n<=3&&!hit;++n)for(auto &e:owner.enemies)if(e.water&&e.x==((g.position.xy.x+d.dx*n)&255)&&e.y==((g.position.xy.y+d.dy*n)&255)){e.cannon_target=true;range=n;hull=e.hull<0?99:e.hull;hit=true;break;}
            event(GameEventKind::Sfx,"cannon-fire");projectile(0,0,d.dx*range,d.dy*range);
            if(hit){turn();int remaining=hull-rand(1,20);if(remaining<0)message("Ship sunk!");for(size_t i=0;i<owner.enemies.size();++i)if(owner.enemies[i].cannon_target){if(remaining<0)owner.enemies.erase(owner.enemies.begin()+ptrdiff_t(i));else{owner.enemies[i].hull=remaining;owner.enemies[i].cannon_target=false;}break;}}
            else {message("Missed!");}event(GameEventKind::MapChanged);return;
        }
        if(!g.position.map.location||(g.position.map.location>=33&&g.position.map.location<=40)){message("What?");return;}
        auto raw=[&](int x,int y){return q&&q->tile_at?q->tile_at(q->context,x,y):get_active_map(c.world,g.position.map).value.tile_at(x,y);};
        constexpr int dx[]={0,1,0,-1},dy[]={-1,0,1,0};int found=-1,cx=0,cy=0,tile=0;
        for(int i=0;i<4;++i){cx=(g.position.xy.x+dx[i])&255;cy=(g.position.xy.y+dy[i])&255;tile=raw(cx,cy);if((tile&252)==180){found=i;break;}}
        if(found<0){message("What?");return;}
        if(!q||!q->volatile_tile){result.status=CommandStatus::InvalidContext;return;}
        message("BOOOM!");event(GameEventKind::Sfx,"cannon-fire");int ox=dx[found],oy=dy[found],dir=tile&3;bool changed=false;
        for(int n=0;n<4;++n){cx=(cx+dx[dir])&255;cy=(cy+dy[dir])&255;ox+=dx[dir];oy+=dy[dir];const NpcActor *npc=nullptr;if(c.actors)for(size_t i=0;i<c.actors->count;++i){auto &a=c.actors->actors[i];if(a.location==g.position.map.location&&a.z==g.position.map.floor&&a.x==cx&&a.y==cy){npc=&a;break;}}
            if(npc){g.karma=uint8_t(g.karma>5?g.karma-5:0);auto killed=*npc;dialogue_despawn(g,c.actors,killed);event(GameEventKind::PartyChanged);changed=true;break;}
            int t=raw(cx,cy);if((t>=151&&t<=153)||(t>=184&&t<=187)){q->volatile_tile(q->context,cx,cy,68);message("Door destroyed!");changed=true;break;}
        }
        projectile(dx[found],dy[found],ox,oy);if(changed)event(GameEventKind::MapChanged);
    }
    void waterfall_fall() {
        auto &g = c.game;
        message("F-A-L-L-S!!!\n");
        g.position.xy.y = uint8_t(wrap_coord(g.position.xy.y + 2));
        event(GameEventKind::Sfx, "waterfall-fall");
        for (int32_t i = 0; i < g.party.party_size && i < g.party.character_count; ++i) {
            auto &ch = g.party.characters[i];
            if (ch.status == 'D') continue;
            const auto rolled = rand(0, 60) >> 1;
            const auto threshold = rolled > 0 ? rolled : 1;
            if (ch.dexterity > threshold) continue;
            if (ch.current_hp) --ch.current_hp;
            if (!ch.current_hp) {
                ch.status = 'D';
                if (g.party.active_character == i) g.party.active_character = 255;
            }
        }
        if (g.position.xy.x == 0x36 && g.position.xy.y == 0x8a) {
            message("Falling into underworld!!\n");
            g.position.map.floor = 255;
            if (c.quest_world && !hydrate_underworld_plot(g, *c.quest_world))
                result.status = CommandStatus::NeedsStorage;
        }
        event(GameEventKind::MapChanged);
    }
    bool effect(CommandEffect e) {
        if(e==CommandEffect::RefreshHourTiles && c.terrain)c.terrain->refresh(c.world,c.game);
        if(e==CommandEffect::Doors && c.commands.door.turns>0)--c.commands.door.turns;
        if (!c.game.position.map.location &&
            (e == CommandEffect::Waterfall || e == CommandEffect::WaterfallUnder)) {
            const auto attempts = e == CommandEffect::Waterfall ? 256 : 1;
            for (int i = 0; i < attempts; ++i) {
                const auto pos = c.game.position.xy;
                const auto t = map().tile_at(pos.x, wrap_coord(pos.y + (e == CommandEffect::Waterfall ? 1 : 0)));
                if ((t & 0xfc) != 0xd4) break;
                waterfall_fall();
            }
        }
        if(c.quest_world && e==CommandEffect::Refuge){auto status=check_refuge(c,sink());if(status==CommandStatus::InvalidContext)result.status=status;return false;}
        if(c.blackthorn && (e==CommandEffect::Capture || e==CommandEffect::Tribute))return blackthorn_turn_effect(c,e,sink(),rand);
        if(c.quest_world && e==CommandEffect::Moongate){auto &q=*c.quest_world;auto &g=c.game;if(g.position.map.location || !q.moon_phases || !moongate_at(g,c.turn,q))return false;event(GameEventKind::Sfx,"moongate");auto phase=active_gate_phase(g,c.turn,q);if((g.time.hour==0&&g.time.minute<10) || phase<0 || size_t(phase)>=q.moonstone_count || q.moonstones[phase].location==255){event(GameEventKind::MapChanged);return false;}auto loc=q.moonstones[phase].location;auto banner=c.services.banner?c.services.banner(c.services.context,loc):nullptr;moonstone_teleport(g,c.turn,c.travel,q.moonstones,q.moonstone_count,phase,banner,transitions());return false;}
        if (c.shrine_services && e==CommandEffect::ShrineGuardian) {
            shrine_guardian(c.game,sink()); return false;
        }
        if (c.shrine_services && e==CommandEffect::ShrineEntry) {
            shrine_entry(c.game,*c.shrine_services,tile(),sink()); return false;
        }
        return c.services.effect && c.services.effect(c.services.context, e, sink());
    }
    ActiveMap map() {
        auto m=get_active_map(c.world,c.game.position.map).value;
        if(c.combat && c.combat_context) {
            m.kind=MapKind::Small;m.geometry={11,11,false};m.resolve_context=c.combat_context;
            m.resolve_tile=[](void *p,MapId,int32_t x,int32_t y,int32_t){return int32_t(static_cast<CombatContext *>(p)->combat.map.tiles[y*11+x]);};
        } else {m.resolve_context=&c;m.resolve_tile=[](void *p,MapId id,int32_t x,int32_t y,int32_t tile){auto &ctx=*static_cast<CommandContext *>(p);if(ctx.transport_services&&ctx.transport_services->tile_at)tile=ctx.transport_services->tile_at(ctx.transport_services->context,x,y);if(ctx.quest_world)tile=quest_world_tile(ctx,id,x,y,tile);return open_door_tile(ctx,id,x,y,tile);};}
        return m;
    }
    int32_t tile() {
        return c.transport_services && c.transport_services->tile_at
                   ? c.transport_services->tile_at(c.transport_services->context,
                                                   c.game.position.xy.x, c.game.position.xy.y)
                   : map().tile_at(c.game.position.xy.x, c.game.position.xy.y);
    }
    int32_t conscious() {
        bool sleeping = false;
        for (int32_t i = 0;
             i < c.game.party.party_size && i < c.game.party.character_count && i < 6; ++i) {
            const auto s = c.game.party.characters[i].status;
            if (s == 'G' || s == 'P')
                return 0;
            sleeping = sleeping || s == 'S';
        }
        return sleeping ? 1 : -1;
    }
    void actors(bool guards = true) {
        if (!c.actors)
            return;
        const auto m = map();
        auto err = guards ? tick_guards(*c.actors, c.game, m, rand) : ActorError::None;
        if (err == ActorError::None)
            err = tick_npcs(*c.actors, c.game, {c.world, m, *c.npc_scratch}, rand);
        if (err != ActorError::None) {
            result.actor_error = err;
            result.status = CommandStatus::CoreError;
        }
    }
    void world(int32_t under) {
        if (!outdoor_world_turn_runs(c.game, c.turn, c.commands.outdoor_phases))
            return;
        ++result.world_turns;
        const auto spawn = roll_spawn_gate(rand, under, c.game.position.map.floor, c.game.time.hour);
        const auto status = outdoor_tick(c, map(), spawn.spawn, rand, sink());
        if (status != CommandStatus::Success) result.status = status;
    }
    void turn_events(const TurnResult &r) {
        if (r.poison_ticks.count) {
            GameEvent e;
            e.kind = GameEventKind::PoisonTick;
            e.slot_count = r.poison_ticks.count;
            for (uint8_t i = 0; i < e.slot_count; ++i)
                e.slots[i] = r.poison_ticks.values[i];
            emit(e);
        }
        for (uint8_t i = 0; i < r.message_count; ++i)
            message(turn_message_text(r.messages[i]));
        if (r.poisoned.count)
            message("Poisoned!");
    }
    void troll_script(const TrollRoll &troll) {
        TrollSneakScript script;
        char names[6][40]{};
        script.beats[script.count++] = {"\nThou spieth trolls under the bridge!\n\n", 10, false};
        for (uint8_t i = 0; i < troll.indices.count; ++i) {
            const auto &ch = c.game.party.characters[troll.indices.values[i]];
            const char *first = ch.name;
            while (*first == ' ' || *first == '\t' || *first == '\r' || *first == '\n') ++first;
            const char *last = first;
            while (*last) ++last;
            while (last > first && (last[-1] == ' ' || last[-1] == '\t' || last[-1] == '\r' || last[-1] == '\n')) --last;
            if (last == first) std::snprintf(names[i], sizeof(names[i]), "Avatar sneaks across");
            else std::snprintf(names[i], sizeof(names[i]), "%.*s sneaks across", int(last-first), first);
            script.beats[script.count++] = {names[i], 5, false};
            script.beats[script.count++] = {".", 5, true};
            script.beats[script.count++] = {".", 5, true};
            script.beats[script.count++] = {".", -1, true};
            script.beats[script.count++] = {"\n", -1, false};
        }
        if (troll.payer_index < 0) script.beats[script.count++] = {"Trolls evaded!\n", -1, false};
        GameEvent e; e.kind = GameEventKind::TrollSneak; e.troll_sneak = &script; emit(e);
    }
    void turn(bool consumed = true, const StepGeometry *step = nullptr, bool pass = false) {
        const bool pre = c.travel.drunk_pre_rolled;
        c.travel.drunk_pre_rolled = false;
        if (c.game.position.map.location == 0) {
            const auto under = tile();
            struct Slow {
                Runner *r;
                int32_t under, speed;
            } slow{this, under, step ? step->speed_class : 0};
            OutdoorTurnContext ctx;
            ctx.tile_under_party = under;
            ctx.blocked = step && step->blocked;
            ctx.on_swamp = step && step->on_swamp;
            ctx.on_bridge = step && step->on_bridge;
            ctx.skip_world_turn = true;
            ctx.sky = c.sky;
            ctx.after_wind = {&slow, [](void *p) {
                                  auto &s = *static_cast<Slow *>(p);
                                  for (int32_t i = 0; i < s.speed; ++i)
                                      s.r->world(s.under);
                                  if (s.speed)
                                      advance_clock(s.r->c.game, s.r->c.turn, s.speed * 2,
                                                    &s.r->rand, s.r->c.sky);
                              }};
            const auto r = outdoor_turn(c.game, c.turn, rand, ctx);
            if (r.burning)
                message("Burning!");
            if (r.hazard) {
                message("EARTHQUAKE!\n");
                event(GameEventKind::Quake);
                event(GameEventKind::Sfx, "quake");
            }
            turn_events(r);
            if (r.has_troll && r.troll.fired && r.troll.on_foot) troll_script(r.troll);
            if (r.has_troll && r.troll.payer_index >= 0) {
                c.commands.awaiting_troll = true;
                c.commands.troll_toll = r.troll.toll;
                c.commands.troll_under_party = under;
                c.commands.troll_x = attempted_target.x;
                c.commands.troll_y = attempted_target.y;
                GameEvent e; e.kind = GameEventKind::TrollTollPrompt; e.note = r.troll.toll; emit(e);
                result.status = CommandStatus::AwaitingResponse;
                return;
            }
            if (!ctx.blocked) {
                effect(CommandEffect::WaterfallUnder);
                effect(CommandEffect::Doors);
                actors(false);
                world(under);
            }
            effect(CommandEffect::Refuge);
            effect(CommandEffect::Waterfall);
            return;
        }
        if (conscious() == -1) {
            if (!effect(CommandEffect::Capture))
                effect(CommandEffect::Refuge);
            return;
        }
        const auto loc = c.game.position.map.location;
        if (c.commands.town_location != loc) {
            c.commands.town_location = loc;
            c.commands.town_phases = {};
        }
        TownTurnContext ctx;
        ctx.consumes_turn = consumed;
        ctx.confused = c.turn.drunk_turns > 0;
        ctx.pre_rolled = pre;
        ctx.pass_command = pass;
        ctx.sky = c.sky;
        ctx.npc_phases = &c.commands.town_phases;
        if (c.actors)
            for (size_t i = 0; i < c.actors->count; ++i) {
                const auto &a = c.actors->actors[i];
                if (a.location == loc && a.z == c.game.position.map.floor)
                    ctx.second_world_turn = true;
            }
        ctx.hazard_context = this;
        if(c.quest_world)ctx.on_trapdoor=[](void *p){auto &r=*static_cast<Runner *>(p);return quest_trapdoor(r.c,r.sink());};
        ctx.tile_under_party = [](void *p) { return static_cast<Runner *>(p)->tile(); };
        ctx.after_housekeeping = {this, [](void *p) {
                                      auto &r = *static_cast<Runner *>(p);
                                      ++r.result.world_turns;
                                      r.actors();
                                  }};
        const auto hour = c.game.time.hour;
        const auto r = town_turn(c.game, c.turn, rand, ctx);
        if (hour != c.game.time.hour && (c.game.time.hour == 20 || c.game.time.hour == 5))
            effect(CommandEffect::RefreshHourTiles);
        turn_events(r);
        effect(CommandEffect::Doors);
        if (effect(CommandEffect::Capture) || effect(CommandEffect::Tribute))
            return;
        effect(CommandEffect::Refuge);
    }
    TransitionServices transitions() {
        return {
            this,
            [](void *p, ReloadEffect e, uint8_t id) {
                auto &r = *static_cast<Runner *>(p);
                if (e == ReloadEffect::ContextTurn)
                    r.turn();
                else if (e == ReloadEffect::RefreshHourTiles)
                    r.effect(CommandEffect::RefreshHourTiles);
                else {
                    if(e==ReloadEffect::ClearTerrain && r.c.terrain)r.c.terrain->clear_residence();
                    if(e==ReloadEffect::ResetDoors)r.c.commands.door.turns=0;
                    if(e==ReloadEffect::ClearEnemies && r.c.outdoor)r.c.outdoor->enemies.clear();
                    if(e==ReloadEffect::HydrateUnderworld && r.c.quest_world && !hydrate_underworld_plot(r.c.game,*r.c.quest_world))
                        r.result.status=CommandStatus::NeedsStorage;
                    if(e==ReloadEffect::UrbanEffects && r.c.quest_world)
                        r.result.status=urban_shadowlord(r.c,r.sink(),r.rand);
                    if(e==ReloadEffect::HydrateInterior && r.c.quest_world && !hydrate_interior_objects(r.c,id))r.result.status=CommandStatus::NeedsStorage;
                    if(e==ReloadEffect::DiscardInterior && r.c.quest_world)discard_interior_objects(r.c.game,*r.c.quest_world,id);
                    // Batch 24: consumed here, on the table the device shares (context_.actors).
                    if(e==ReloadEffect::SnapNpcs && r.c.actors)snap_npcs_to_schedule(*r.c.actors,id,uint8_t(r.c.game.time.hour));
                    if (r.c.services.reload)
                        r.c.services.reload(r.c.services.context, e, id, r.sink());
                    if (e == ReloadEffect::EnterNpcs && r.c.actors) {
                        NpcLocationData data;
                        for (size_t i = 0; i < r.c.npc_data_count; ++i)
                            if (r.c.npc_data[i].location == id) {
                                data = r.c.npc_data[i];
                                break;
                            }
                        const auto err =
                            enter_npc_map(*r.c.actors, data.slots, data.count, id,
                                          uint8_t(r.c.game.time.hour), data.dead_slots |
                                          (id >= 1 && id <= 32 ? r.c.game.npc_dead[id-1] : 0));
                        if (err != ActorError::None) {
                            r.result.actor_error = err;
                            r.result.status = CommandStatus::CoreError;
                        }
                    }
                }
            },
            [](void *p, GameEventKind k, const char *s) { static_cast<Runner *>(p)->event(k, s); }};
    }
    void sync_transport(int32_t tile) {
        c.turn.transport_tile = tile;
        c.game.transport = transport_mode(tile);
    }
    void naval_step(Direction dir) {
        const auto delta = direction_delta(dir);
        const auto x = wrap_coord(c.game.position.xy.x + delta.dx);
        const auto y = wrap_coord(c.game.position.xy.y + delta.dy);
        const auto dest = map().tile_at(x, y), tile = c.turn.transport_tile;
        const auto actor = outdoor_actor_tile(c, x, y);
        const bool occupied = actor != 0 && !boardable_actor_tile(actor, tile);
        const bool sailing = c.turn.sail_dir != 0 && (tile & 0xfc) == 0x20;
        if ((tile & 0xfc) == 0x24) message("Rowing!");
        const auto passable = is_passable(dest, c.game.transport);
        if (passable.value && !occupied) {
            c.game.position.xy = {uint8_t(x), uint8_t(y)};
            event(GameEventKind::Moved);
            effect(CommandEffect::Waterfall);
            return;
        }
        if (sailing) {
            if (dest == 0x47) {
                message("Docked!");
                sync_transport(tile + 4);
            } else {
                const auto damage = rand(1, 30);
                message(dest == 3 ? "BREAKING UP!" : "COLLISION!");
                if (damage >= c.game.ship_hull) {
                    sink_player_ship(c.game, c.turn, rand, sink());
                } else c.game.ship_hull -= damage;
            }
            c.turn.sail_dir = 0;
            return;
        }
        if (occupied && tile >= 0x20 && (actor & 0xfc) == 0xec) return;
        message("Blocked!");
        if (dest == 0x2f) {
            message("OUCH!");
            const auto damage = rand(1, 8);
            auto i = c.game.party.active_character;
            if (i >= c.game.party.character_count) i = 0;
            if (i < c.game.party.character_count) {
                auto &hp = c.game.party.characters[i].current_hp;
                hp = uint16_t(hp > damage ? hp - damage : 0);
            }
        } else event(GameEventKind::Sfx, "move-blocked");
    }
    void naval_turn(int32_t drift = 0) {
        auto &t = c.turn;
        if (c.game.hms_cape) t.hms_cape_toggle ^= 1;
        OutdoorTurnContext ctx;
        ctx.tile_under_party = map().tile_at(c.game.position.xy.x, c.game.position.xy.y);
        ctx.minutes = c.game.hms_cape ? 1 : 2;
        ctx.skip_world_turn = true;
        ctx.sky = c.sky;
        turn_events(outdoor_turn(c.game, t, rand, ctx));
        if (!c.game.hms_cape || t.hms_cape_toggle == 0) world(ctx.tile_under_party);
        if (!drift || !t.sail_dir || !t.wind) return;
        static constexpr Direction courses[]{Direction::North, Direction::West, Direction::East,
                                             Direction::North, Direction::South};
        static constexpr int dx[]{0, 0, 0, -1, 1}, dy[]{0, 1, -1, 0, 0};
        const auto course = direction_delta(courses[t.sail_dir]);
        const auto threshold = (1 + (dx[t.wind] != course.dx) + (dy[t.wind] != course.dy)) % 3;
        if (threshold > t.wind_drift_counter) ++t.wind_drift_counter;
        else {
            t.wind_drift_counter = 0;
            naval_step(courses[drift]);
        }
    }
    void naval_move(Direction dir) {
        static constexpr int facing[]{0, 2, 1, 3}, sail[]{3, 4, 2, 1};
        static constexpr const char *names[]{"North", "South", "East", "West"};
        const auto tile = c.turn.transport_tile, base = tile & 0xfc;
        const auto index = static_cast<uint8_t>(dir);
        const auto next = base + facing[index];
        const bool turned = next != tile, town = c.game.position.map.location != 0;
        if (base == 0x20 && c.turn.sail_dir != sail[index]) {
            c.turn.sail_dir = sail[index];
            c.turn.wind_drift_counter = 0;
        }
        sync_transport(next);
        if (base == 0x20 && !turned) naval_turn(c.turn.sail_dir);
        else if (turned && !town && base != 0x28) {
            char head[16];
            std::snprintf(head, sizeof(head), "Head %s", names[index]);
            message(head);
            if (c.game.ship_hull < 50) message("Hull weak!");
            naval_turn();
        } else {
            if (base == 0x28 || town) {
                char echo[16];
                std::snprintf(echo, sizeof(echo), "%s%s", base == 0x28 ? "Row " : "", names[index]);
                event(GameEventKind::WalkEcho, echo);
            }
            naval_step(dir);
            naval_turn();
        }
        event(GameEventKind::MapChanged);
    }
    void move(Direction dir, bool staggered = false) {
        if (!staggered && c.game.position.map.location && c.turn.drunk_turns > 0) {
            if (c.turn.time_spell != 'T')
                maybe_change_wind(c.turn, rand);
            if (rand(0, 1) == 1) {
                --c.turn.drunk_turns;
                static constexpr Direction dirs[]{Direction::North, Direction::South,
                                                  Direction::East, Direction::West};
                dir = dirs[rand(0, 3)];
                message("Hic!");
            }
            c.travel.drunk_pre_rolled = true;
        }
        static constexpr const char *names[]{"North", "South", "East", "West"};
        char echo[16];
        const auto base = c.turn.transport_tile & 0xfc;
        const char *verb = base == 0x10 ? "Ride " : base == 0x14 ? "Fly " : "";
        std::snprintf(echo, sizeof(echo), "%s%s", verb, names[static_cast<uint8_t>(dir)]);
        const bool naval = c.game.transport == TransportMode::Ship || c.game.transport == TransportMode::Skiff;
        if (!naval) event(GameEventKind::WalkEcho, echo);
        const auto faced = mount_face_tile(c.turn.transport_tile, dir);
        if (faced != c.turn.transport_tile) {
            c.turn.transport_tile = faced;
            event(GameEventKind::MapChanged);
        }
        const auto m = map();
        const auto d = direction_delta(dir);
        if (c.actors && c.game.position.map.location) {
            for (size_t i = 0; i < c.actors->count; ++i) {
                const auto &a = c.actors->actors[i];
                if (a.location == c.game.position.map.location &&
                    a.z == c.game.position.map.floor &&
                    a.x == int32_t(c.game.position.xy.x) + d.dx &&
                    a.y == int32_t(c.game.position.xy.y) + d.dy) {
                    message("Blocked!");
                    event(GameEventKind::Sfx, "move-blocked");
                    result.status = CommandStatus::Rejected;
                    turn();
                    return;
                }
            }
        }
        if (naval) { naval_move(dir); return; }
        Position target{};
        const auto actor = target_for_step(c.game.position.xy, m.geometry, dir, target)
                               ? outdoor_actor_tile(c, target.x, target.y) : 0;
        attempted_target=target;
        const auto resolved = resolve_world_step(c.game, m, dir, c.turn.transport_tile, actor);
        if (resolved.error != Error::None) {
            result.error = resolved.error;
            result.status = CommandStatus::CoreError;
            return;
        }
        const auto &s = resolved.value;
        if (s.exited_map) {
            event(GameEventKind::TownExitPrompt);
            c.commands.awaiting_exit = true;
            result.status = CommandStatus::AwaitingResponse;
            return;
        }
        if (s.message != StepMessage::None)
            message(step_message_text(s.message));
        if (s.blocked) {
            if (s.message == StepMessage::Blocked) {
                if (s.on_cactus) {
                    message("OUCH!");
                    party_random_damage(c.game, rand);
                    event(GameEventKind::PartyChanged);
                } else
                    event(GameEventKind::Sfx, "move-blocked");
            }
            result.status = CommandStatus::Rejected;
            turn(s.minutes > 0, &s);
            return;
        }
        event(GameEventKind::Moved);
        event(GameEventKind::Sfx, "move-step");
        if (!c.game.position.map.location)
            effect(CommandEffect::ShrineGuardian);
        else
            apply_stair_step(c.game, c.travel, c.world, tile(), dir, transitions());
        turn(true, &s);
        effect(CommandEffect::Moongate);
        effect(CommandEffect::ShrineEntry);
    }
    void enter() {
        if (c.shrine_services && (tile()==17 || tile()==25)) {
            const auto action=enter_shrine(c,tile());
            result.event_count+=action.event_count; result.status=action.status; return;
        }
        if (c.game.position.map.location) {
            message("Enter what?");
            result.status = CommandStatus::Rejected;
            return;
        }
        static constexpr int32_t tiles[]{16, 18, 19, 20, 21, 22, 23, 24, 26, 27, 57, 62};
        static constexpr const char *lines[]{"Enter hut",
                                             "Enter keep",
                                             "Enter village",
                                             "Enter towne",
                                             "Enter castle",
                                             "Enter cave",
                                             "Enter mine",
                                             "Enter dungeon",
                                             "Enter ruins",
                                             "Enter lighthouse",
                                             "Enter the palace of Blackthorn!",
                                             "Enter the Castle of Lord British!"};
        int32_t index = -1;
        for (int32_t i = 0; i < 12; ++i)
            if (tiles[i] == tile())
                index = i;
        if (index < 0) {
            message("Enter What?");
            result.status = CommandStatus::Rejected;
            return;
        }
        message(lines[index]);
        if (tile() == 26) {
            turn();
            return;
        }
        const auto id = location_at(c.locations, c.game.position.xy.x, c.game.position.xy.y);
        if (id >= 33 && id <= 40 && c.dungeon_context) {
            auto &d = *c.dungeon_context;
            if(c.quest_world){auto q=doom_entrance(c,id,sink());if(q.status!=CommandStatus::Success || q.turn){result.status=q.status;return;}}
            if (d.entry_hook && d.entry_hook(d.context, uint8_t(id), sink()))
                return;
            Command enter;
            enter.kind = CommandKind::EnterDungeon;
            enter.member = int16_t(id);
            enter.hours = c.game.position.map.floor;
            auto saved = c.events;
            c.events = sink();
            auto action = execute_dungeon_command(c, enter);
            c.events = saved;
            result.status = action.status;
            return;
        }
        bool found = false;
        for (size_t i = 0; i < c.world.small_map_count; ++i)
            if (c.world.small_maps[i].id.location == id)
                found = true;
        if (!id || !found) {
            result.status = CommandStatus::NoOp;
            return;
        }
        const auto banner =
            c.services.banner ? c.services.banner(c.services.context, uint8_t(id)) : nullptr;
        load_small_map(c.game, c.turn, c.travel, uint8_t(id), banner, transitions());
    }
    void klimb(Command cmd) {
        if(!c.game.position.map.location){
            if(!c.game.grapple){message("With what?");result.status=CommandStatus::Rejected;return;}
            if(c.game.transport!=TransportMode::Foot){message("On foot!");result.status=CommandStatus::Rejected;return;}
            if(!cmd.has_direction){event(GameEventKind::NeedsDirection,"klimb");result.status=CommandStatus::AwaitingResponse;return;}
            const auto d=direction_delta(cmd.direction);int x=wrap_coord(c.game.position.xy.x+d.dx),y=wrap_coord(c.game.position.xy.y+d.dy);
            const auto tile=map().tile_at(x,y);
            if(tile!=12){message(tile==13?"Impassable!":"Not climbable!");result.status=CommandStatus::Rejected;return;}
            for(int32_t i=0;i<c.game.party.party_size&&i<c.game.party.character_count;++i){auto &ch=c.game.party.characters[i];if(ch.status=='D')continue;if(ch.dexterity>=rand(1,30))continue;int damage=rand(1,5);ch.current_hp=uint16_t(ch.current_hp>damage?ch.current_hp-damage:0);if(!ch.current_hp)ch.status='D';message("Fell!");}
            c.game.position.xy={uint8_t(x),uint8_t(y)};turn();event(GameEventKind::Moved);return;
        }
        if (c.game.transport == TransportMode::Horse) {
            message("Klimb--On foot!");
            result.status = CommandStatus::Rejected;
            return;
        }
        const auto under = tile();
        if (under == 200 || under == 201 || under == 134) {
            if (!klimb_ladder(c.game, c.travel, c.world, under == 200 ? 1 : -1, transitions()))
                result.status = CommandStatus::Rejected;
            return;
        }
        if (!cmd.has_direction) {
            event(GameEventKind::NeedsDirection, "klimb");
            result.status = CommandStatus::AwaitingResponse;
            return;
        }
        const auto d = direction_delta(cmd.direction);
        const auto x = int32_t(c.game.position.xy.x) + d.dx,
                   y = int32_t(c.game.position.xy.y) + d.dy;
        const auto t = map().tile_at(x, y);
        if (t != 76 && t != 202 && t != 203) {
            message("Klimb-What?");
            result.status = CommandStatus::Rejected;
            return;
        }
        c.game.position.xy = {uint8_t(x), uint8_t(y)};
        turn();
        event(GameEventKind::Moved);
    }
};
} // namespace
static ActionResult execute(CommandContext &c, Command cmd, bool dispatch) {
    if(c.commands.awaiting_troll&&cmd.kind!=CommandKind::TrollToll){ActionResult r;r.status=CommandStatus::AwaitingResponse;return r;}
    if(cmd.kind==CommandKind::BlackthornAction){ActionResult r;if(cmd.item<0||cmd.item>int16_t(BlackthornAction::Arrest)){r.status=CommandStatus::InvalidContext;return r;}auto rand=rng_source(c.game.rng);r.status=blackthorn_action(c,BlackthornAction(cmd.item),TalkText(cmd.text?cmd.text:u"",cmd.text_length),cmd.member!=0,c.events,rand);return r;}
    if(c.blackthorn && (c.blackthorn->shrine>=0||c.blackthorn->password||c.blackthorn->tribute||c.blackthorn->arrest)){ActionResult r;r.status=CommandStatus::AwaitingResponse;return r;}
    if (cmd.kind == CommandKind::ShopAction) {
        if (cmd.item < 0 || cmd.item > int16_t(ShopAction::Text)) { ActionResult r; r.status=CommandStatus::Rejected; return r; }
        return execute_shop(c,{ShopAction(cmd.item),cmd.member,cmd.text,cmd.text_length});
    }
    if (c.shop_services && c.shop_services->session.phase != ShopPhase::Closed) {
        ActionResult r; r.status=CommandStatus::AwaitingResponse; return r;
    }
    if (cmd.kind >= CommandKind::Talk && cmd.kind <= CommandKind::EndConversation)
        return execute_dialogue_command(c, cmd);
    if (c.dialogue_services && c.dialogue_services->session.active) {
        ActionResult result;
        result.status = CommandStatus::AwaitingResponse;
        return result;
    }
    if(cmd.kind==CommandKind::Mix){
        ActionResult result;
        if(c.combat||cmd.item<0||cmd.item>48||c.game.party.character_count>16||c.game.party.party_size<0||c.game.party.party_size>6){result.status=CommandStatus::InvalidContext;return result;}
        auto emit=[&](GameEvent e){++result.event_count;if(c.events.emit)c.events.emit(c.events.context,e);};auto say=[&](const char *s){GameEvent e;e.kind=GameEventKind::Message;e.text=s;emit(e);};
        if(cmd.hours<=0){return result;}if(!cmd.reagent_mask){say("Nothing to mix!");return result;}
        for(int i=0;i<8;++i)if((cmd.reagent_mask&(1<<i))&&c.game.reagent_quantities[i]<cmd.hours){say("Insufficient reagents!");return result;}
        say("Mixing...");for(int i=0;i<8;++i)if(cmd.reagent_mask&(1<<i))c.game.reagent_quantities[i]-=cmd.hours;
        auto *def=spell_definition(SpellId(cmd.item));if(cmd.item<48&&def->reagents==cmd.reagent_mask){c.game.spell_quantities[cmd.item]=std::min<int32_t>(99,c.game.spell_quantities[cmd.item]+cmd.hours);say("Done!");return result;}
        auto opener=first_conscious_index(c.game.party);auto trap=chest_trap(c.game,c.game.position.map.location,opener<0?0:opener,rng_source(c.game.rng));GameEvent e;e.kind=GameEventKind::Sfx;e.text="dungeon-trap";emit(e);say(trap.message);if(trap.damage_mask){e={};e.kind=GameEventKind::PoisonTick;for(int i=0;i<6;++i)if(trap.damage_mask&(1<<i))e.slots[e.slot_count++]=uint8_t(i);emit(e);}return result;
    }
    if (cmd.kind==CommandKind::ShrineAction) {
        if (!cmd.shrine) { ActionResult result; result.status=CommandStatus::InvalidContext; return result; }
        return execute_shrine(c,*cmd.shrine);
    }
    if(cmd.kind==CommandKind::UseMoonstone && !c.combat){ActionResult r;r.status=use_moonstone(c,cmd.item,c.events);return r;}

    if(c.dungeon && !c.combat && cmd.kind==CommandKind::UseItem && (cmd.item==18 || cmd.item==19 || cmd.item==20 || cmd.item==33 || cmd.item==36 || (cmd.item>=29 && cmd.item<=31))){ActionResult r;r.status=use_quest_item(c,cmd.item,c.events);return r;}
    if (cmd.kind == CommandKind::EnterDungeon || cmd.kind == CommandKind::DungeonCommand || (c.dungeon&&!c.combat&&(cmd.kind==CommandKind::Cast||(cmd.kind==CommandKind::UseItem&&cmd.item>=0&&cmd.item<=37))))
        return execute_dungeon_command(c, cmd);
    // Batch 21A.3: SetActivePlayer is a command in BOTH loops.  The overworld /
    // dungeon arm below (kernel 0x4080) stays exactly as it was; in an arena the
    // SAME digit command routes to COMBAT.OVL's own set-active (COMBAT:0x063E
    // @0x09ec / @0x09fe -> SJOG.OVL 0x1F7A), which has different strings, a
    // different validity rule and an action cost.  Without this arm the command
    // fell through to the context gate below and was rejected as InvalidContext,
    // so a selection made before a fight could never be cleared or changed.
    if ((cmd.kind >= CommandKind::CombatMove && cmd.kind < CommandKind::Cast) || (cmd.kind==CommandKind::Cast && c.combat) || (c.combat&&cmd.kind==CommandKind::UseItem&&cmd.item>=0&&cmd.item<=37) ||
        (c.combat && cmd.kind == CommandKind::SetActivePlayer) ||
        (cmd.kind >= CommandKind::CombatKlimb && cmd.kind <= CommandKind::CombatSearch)) {
        ActionResult result;
        if (!c.combat || !c.combat_context || &c.combat_context->game != &c.game ||
            &c.combat_context->turn != &c.turn) {
            result.status = CommandStatus::InvalidContext;
            return result;
        }
        auto arena = *c.combat_context;
        if (cmd.kind == CommandKind::Cast && (cmd.item < 0 || cmd.item > 48)) {
            result.status = CommandStatus::InvalidContext;
            return result;
        }
        struct Delivery {
            CommandContext &context;
            ActionResult &result;
        } delivery{c, result};
        arena.events = {&delivery, [](void *p, const CombatEvent &event) {
                            auto &d = *static_cast<Delivery *>(p);
                            ++d.result.event_count;
                            GameEvent envelope;
                            envelope.kind = GameEventKind::Combat;
                            envelope.combat = &event;
                            if (d.context.events.emit)
                                d.context.events.emit(d.context.events.context, envelope);
                        }};
        arena.trace = c.rng_trace;
        const auto action =
            cmd.kind == CommandKind::CombatKlimb ? CombatAction::Klimb
            : cmd.kind == CommandKind::CombatGet ? CombatAction::Get
            : cmd.kind == CommandKind::CombatOpen
                ? CombatAction::OpenAt
            : cmd.kind == CommandKind::CombatSearch ? CombatAction::Search
            : cmd.kind == CommandKind::SetActivePlayer ? CombatAction::SetActive
                : static_cast<CombatAction>(int(cmd.kind) - int(CommandKind::CombatMove));
        CombatPoint aim{cmd.combat_x, cmd.combat_y};
        EventSink use_events{&delivery,[](void *p,const GameEvent &e){auto &d=*static_cast<Delivery*>(p);++d.result.event_count;if(d.context.events.emit)d.context.events.emit(d.context.events.context,e);}};
        auto use=[&](){
            if(cmd.item<16)return combat_use_consumable(arena,cmd.item,use_events);
            if(arena.combat.actors.capacity()-arena.combat.count<combat_growth_reserve(arena.combat)+4)return CombatResult::NeedsActorStorage;
            auto *actor=current_combat_actor(arena);if(!actor||actor->member==255)return CombatResult::Ok;
            auto out=world_magic(c,cmd,{},use_events,rng_source(arena.combat.rng));
            if(out.status==CommandStatus::InvalidContext)return CombatResult::Invalid;
            if(out.status==CommandStatus::Unsupported)return CombatResult::Unsupported;
            return combat_cast_effect(arena,{});
        };
        const int cast_qty_before=cmd.kind==CommandKind::Cast&&cmd.item>=0&&cmd.item<48?c.game.spell_quantities[cmd.item]:-1;
        int32_t combat_arg=cmd.combat_x;
        // The digit travels in `member`, exactly as the overworld arm reads it.
        if(action==CombatAction::SetActive)combat_arg=cmd.member;
        if((action==CombatAction::Get||action==CombatAction::Search)&&cmd.has_direction){
            combat_arg=cmd.direction==Direction::East?0:cmd.direction==Direction::West?1:cmd.direction==Direction::South?2:3;
        }
        const auto status =
            cmd.kind==CommandKind::UseItem?use():cmd.kind == CommandKind::Cast
                ? combat_cast(arena, static_cast<SpellId>(cmd.item),
                              cmd.has_target ? &aim : nullptr, cmd.member, cmd.cancel_target)
                : combat_action(arena, action,
                                (action == CombatAction::Get || action == CombatAction::OpenAt || action == CombatAction::Search) &&
                                !cmd.has_direction
                                    ? -1
                                    : combat_arg,
                                cmd.combat_y);
        if(cmd.kind==CommandKind::Cast&&cmd.item>=0&&cmd.item<48&&c.game.spell_quantities[cmd.item]<cast_qty_before){
            static constexpr int no_ceremony[]={1,13,37,28,40,44,45};bool ceremonial=cmd.item!=46;for(int id:no_ceremony)ceremonial&=cmd.item!=id;
            if(ceremonial){GameEvent e;e.kind=GameEventKind::Sfx;e.text="spell-cast";use_events.emit(use_events.context,e);e={};e.kind=GameEventKind::MagicCeremony;e.note=spell_definition(SpellId(cmd.item))->circle;use_events.emit(use_events.context,e);}
        }
        result.status = status == CombatResult::Ok ? CommandStatus::Success
                        : (status == CombatResult::NeedsActorStorage ||
                           status == CombatResult::NeedsLootStorage)
                            ? CommandStatus::NeedsStorage
                        : status == CombatResult::Unsupported ? CommandStatus::Unsupported
                                                              : CommandStatus::InvalidContext;
        return result;
    }
    Runner r{c, {}, {&c, [](void *p, int32_t lo, int32_t hi) {
                         auto &ctx = *static_cast<CommandContext *>(p);
                         const auto value = ctx.game.rng.next(lo, hi).value;
                         if (ctx.rng_trace.emit)
                             ctx.rng_trace.emit(ctx.rng_trace.context, "command", lo, hi, value);
                         return value;
                     }}};
    if(!c.combat&&(cmd.kind==CommandKind::NewOrder||cmd.kind==CommandKind::SetActivePlayer)){
        auto &g=c.game;auto &party=g.party;if(party.character_count>kRosterCapacity||party.party_size<0||party.party_size>kMaxParty){r.result.status=CommandStatus::InvalidContext;return r.result;}
        if(cmd.kind==CommandKind::NewOrder){
            if(cmd.member<0||cmd.item<0||cmd.member>=party.character_count||cmd.item>=party.character_count)r.message("nobody!");
            else if(!cmd.member||!cmd.item){std::string text=party.characters[0].name;if(text.empty())text="The Avatar";text+=" must lead!";r.message(text.c_str());}
            else{std::swap(party.characters[cmd.member],party.characters[cmd.item]);r.event(GameEventKind::PartyChanged);}
        }else if(!cmd.member){party.active_character=255;r.message("None!");}
        else{int i=cmd.member-1;if(i<0||i>=party.party_size||i>=party.character_count||party.characters[i].status=='D'||party.characters[i].status=='S'){r.message("Invalid!");if(!g.position.map.location)r.turn();}
            else{party.active_character=uint8_t(i);std::string name=party.characters[i].name;auto first=name.find_first_not_of(" \t\r\n\f\v"),last=name.find_last_not_of(" \t\r\n\f\v");name=first==std::string::npos?"Avatar":name.substr(first,last-first+1);r.message(name.c_str());}}
        return r.result;
    }
    if(!c.combat && (cmd.kind==CommandKind::ViewGem||cmd.kind==CommandKind::AfterGemView||(c.dungeon&&cmd.kind==CommandKind::Ignite))){
        if(c.game.party.character_count>kRosterCapacity||c.game.party.party_size<0||c.game.party.party_size>kMaxParty||(c.dungeon&&(!c.dungeon_context||!c.dungeon_context->state.active))){r.result.status=CommandStatus::InvalidContext;return r.result;}
        if(cmd.kind==CommandKind::ViewGem){if(c.game.gems>0){--c.game.gems;r.event(GameEventKind::PartyChanged);r.event(GameEventKind::GemView);return r.result;}r.message("You have none!\n");}
        if(cmd.kind==CommandKind::Ignite){r.result.item=ignite_torch(c.game,r.rand,c.dungeon_context->state.pos.dungeon);if(!r.result.item.ok)r.message(r.result.item.message);}
        if(c.dungeon){auto tr=advance_turn(c.game,c.turn,1,r.rand,c.sky);for(uint8_t i=0;i<tr.message_count;++i)r.message(turn_message_text(tr.messages[i]));}else r.turn();
        return r.result;
    }
    const bool dungeon_camp=c.dungeon&&(cmd.kind==CommandKind::Rest||cmd.kind==CommandKind::RestCancel);
    // R-06: (R)eady is legal in ALL three contexts, not just the overworld. The
    // reference reaches the same ZSTATS try_equip_or_unequip picker from the
    // overworld loop, the dungeon loop and COMBAT's 'R' dispatcher; only two
    // things differ in an arena, and both belong to the Ready handler below, not
    // to this context gate: equip_item's battle flag (the body-armour lock, ids
    // 9-15) and the CombatActor cache refresh. A dungeon CORRIDOR Ready is an
    // ordinary free action -- c.combat is false there, so it passes battle=false
    // and charges nothing.
    const bool ready_anywhere=cmd.kind==CommandKind::Ready;
    if ((c.combat&&!ready_anywhere) || (c.dungeon&&!dungeon_camp&&!ready_anywhere) ||
        (dungeon_camp&&(!c.dungeon_context||!c.dungeon_context->state.active)) ||
        (c.game.position.map.location >= 33 && c.game.position.map.location <= 40 && !dungeon_camp && !ready_anywhere) ||
        (static_cast<uint8_t>(cmd.kind) > static_cast<uint8_t>(CommandKind::UseItem) &&
         cmd.kind != CommandKind::Board && cmd.kind != CommandKind::Disembark && cmd.kind!=CommandKind::Yell && cmd.kind!=CommandKind::YellSails && cmd.kind!=CommandKind::TrollToll && cmd.kind!=CommandKind::HarpsichordNote && cmd.kind!=CommandKind::Get && cmd.kind!=CommandKind::Search && cmd.kind!=CommandKind::Open && cmd.kind!=CommandKind::Jimmy && cmd.kind!=CommandKind::Push && cmd.kind!=CommandKind::Look && cmd.kind!=CommandKind::CrystalBall && cmd.kind!=CommandKind::DropCoin && cmd.kind!=CommandKind::MakeWish && cmd.kind!=CommandKind::Attack && cmd.kind!=CommandKind::Fire && cmd.kind!=CommandKind::Cast) ||
        ((cmd.kind == CommandKind::Move || cmd.kind == CommandKind::Look || cmd.kind==CommandKind::Attack || cmd.kind==CommandKind::Fire || cmd.kind == CommandKind::Open || cmd.kind == CommandKind::Jimmy || cmd.kind == CommandKind::Push || cmd.kind == CommandKind::Get || cmd.has_direction) &&
         static_cast<uint8_t>(cmd.direction) > 3) ||
        (c.actors && (c.actors->count > 32 || !c.npc_scratch)) ||
        (c.npc_data_count && !c.npc_data) || (c.locations.x_count && !c.locations.x) ||
        (c.locations.y_count && !c.locations.y) ||
        (c.world.small_map_count && !c.world.small_maps) ||
        c.game.party.character_count > kRosterCapacity || c.game.party.party_size < 0 ||
        c.game.party.party_size > kMaxParty) {
        r.result.status = CommandStatus::InvalidContext;
        return r.result;
    }
    if (c.commands.pending_camp_enemy >= 0) {
        r.result.status = CommandStatus::Unsupported;
        return r.result;
    }
    const bool exit = cmd.kind == CommandKind::Exit || cmd.kind == CommandKind::DeclineExit;
    if (c.commands.awaiting_troll && cmd.kind != CommandKind::TrollToll) {
        r.result.status = CommandStatus::AwaitingResponse;
        return r.result;
    }
    if (exit != c.commands.awaiting_exit) {
        r.result.status = CommandStatus::InvalidContext;
        return r.result;
    }
    const auto map = get_active_map(c.world, c.game.position.map);
    if (map.error != Error::None) {
        r.result.status = CommandStatus::CoreError;
        r.result.error = map.error;
        return r.result;
    }
    if (!in_bounds(c.game.position.xy.x, c.game.position.xy.y, map.value.geometry)) {
        r.result.status = CommandStatus::InvalidContext;
        return r.result;
    }
    for (size_t i = 0; i < c.npc_data_count; ++i)
        if (c.npc_data[i].count > 32 || (c.npc_data[i].count && !c.npc_data[i].slots)) {
            r.result.status = CommandStatus::InvalidContext;
            return r.result;
        }
    const bool primary = cmd.kind == CommandKind::Move || cmd.kind == CommandKind::Pass ||
                         cmd.kind == CommandKind::Ignite || cmd.kind == CommandKind::Enter ||
                         (cmd.kind == CommandKind::Klimb && !cmd.has_direction);
    const bool intercept = dispatch && primary && cmd.kind != CommandKind::Move &&
                           c.game.position.map.location && c.turn.drunk_turns > 0;
    // Preflight all possible stagger targets and destination floors before RNG.
    // Bridge tolls / trapdoors need deferred subsystem state and cannot be
    // skipped.
    auto unsupported_tile = [&](int32_t tile) {
        return c.game.position.map.location && tile == 140 && !c.quest_world;
    };
    bool unsupported = c.game.position.map.location && r.tile() == 140 && !c.quest_world;
    if (cmd.kind == CommandKind::Move || intercept) {
        for (int32_t i = 0; i < 4; ++i)
            if (c.turn.drunk_turns > 0 || i == static_cast<int32_t>(cmd.direction)) {
                Position p;
                if (!target_for_step(c.game.position.xy, map.value.geometry, Direction(i), p))
                    continue;
                for (size_t f = 0; f < (c.game.position.map.location ? c.world.small_map_count : 1);
                     ++f) {
                    auto m = map.value;
                    if (c.game.position.map.location) {
                        if (c.world.small_maps[f].id.location != c.game.position.map.location)
                            continue;
                        m = get_active_map(c.world, c.world.small_maps[f].id).value;
                    }
                    unsupported = unsupported || unsupported_tile(m.tile_at(p.x, p.y));
                }
            }
    }
    if (cmd.kind == CommandKind::Enter && !c.game.position.map.location) {
        const auto id = location_at(c.locations, c.game.position.xy.x, c.game.position.xy.y);
        unsupported = unsupported || ((r.tile() == 17 || r.tile() == 25) && !c.shrine_services) ||
                      ((id >= 33 && id <= 40) &&
                       (!c.dungeon_context || (id == 40 && !c.quest_world && !c.dungeon_context->entry_hook)));
    }
    if (cmd.kind == CommandKind::Klimb && (r.tile() == 200 || r.tile() == 201 || r.tile() == 134)) {
        MapId id = c.game.position.map;
        id.floor = int16_t(id.floor + (r.tile() == 200 ? 1 : -1));
        if (floor_exists(c.world, id)) {
            const auto next = get_active_map(c.world, id);
            if (next.error != Error::None) {
                r.result.status = CommandStatus::CoreError;
                r.result.error = next.error;
                return r.result;
            }
            unsupported = unsupported ||
                          (next.value.tile_at(c.game.position.xy.x, c.game.position.xy.y) == 140 && !c.quest_world);
        }
    }
    if (unsupported) {
        r.result.status = CommandStatus::Unsupported;
        return r.result;
    }
    if (cmd.kind == CommandKind::Rest && cmd.hours > 0) {
        const auto eligible = camp_context(c.game, c.turn, r.tile(),dungeon_camp);
        if (!eligible.ship && (eligible.ok && (!c.rest_services ||
                                               (eligible.bed ? (!c.rest_services->snap_npcs ||
                                                                !c.rest_services->occupied ||
                                                                c.game.position.xy.x == 255)
                                                             : !c.rest_services->karma_record)))) {
            r.result.status = CommandStatus::Unsupported;
            return r.result;
        }
        if (cmd.hours > 9) {
            r.result.status = CommandStatus::InvalidContext;
            return r.result;
        }
    }
    const auto before = c.game.turns_since_start;
    if (intercept) {
        if (c.turn.time_spell != 'T')
            maybe_change_wind(c.turn, r.rand);
        c.travel.drunk_pre_rolled = true;
        if (r.rand(0, 1) == 1) {
            --c.turn.drunk_turns;
            const auto dir = Direction(r.rand(0, 3));
            r.message("Hic!");
            r.move(dir, true);
            r.result.turns = c.game.turns_since_start - before;
            return r.result;
        }
    }
    if (dispatch && primary && c.game.position.map.location && r.conscious() == 1) {
        r.message("Zzzzzz...\n");
        r.turn();
        r.result.turns = c.game.turns_since_start - before;
        return r.result;
    }
    switch (cmd.kind) {
    case CommandKind::Fire:r.fire(cmd);break;
    case CommandKind::Cast: {
        if(cmd.item==46 && !c.quest_world){r.result.status=CommandStatus::InvalidContext;break;}
        auto action=world_magic(c,cmd,r.map(),r.sink(),r.rand);r.result.status=action.status;
        if(action.gate){auto &q=*c.quest_world;int phase=cmd.hours;if((c.turn.transport_tile&240)==32 || phase<0 || size_t(phase)>=q.moonstone_count)r.message("Failed!");else {auto loc=q.moonstones[phase].location;auto banner=c.services.banner?c.services.banner(c.services.context,loc):nullptr;if(!moonstone_teleport(c.game,c.turn,c.travel,q.moonstones,q.moonstone_count,phase,banner,r.transitions()))r.message("Failed!");}}
        if(action.turn){r.turn();}break;
    }
    case CommandKind::Look:case CommandKind::CrystalBall:case CommandKind::DropCoin:case CommandKind::MakeWish: r.result.status=world_look(c,cmd,r.map(),r.sink(),r.rand);break;
    case CommandKind::Attack:case CommandKind::Open:case CommandKind::Jimmy:case CommandKind::Push: {
        auto action=world_interaction(c,cmd,r.map(),r.sink(),r.rand);
        r.result.status=action.status;
        if(action.turn)r.turn();
        if(action.map_after)r.event(GameEventKind::MapChanged);
        if(action.moved_after)r.event(GameEventKind::Moved);
        break;
    }
    case CommandKind::TrollToll: {
        if (!c.commands.awaiting_troll) { r.result.status = CommandStatus::NoOp; break; }
        c.commands.awaiting_troll = false;
        const auto paid = int32_t(c.game.gold) - c.commands.troll_toll;
        if (cmd.member != 0 && int16_t(uint16_t(paid)) >= 0) {
            c.game.gold = uint16_t(paid);
            r.effect(CommandEffect::Doors);
            r.actors(false);
            r.world(c.commands.troll_under_party);
            r.effect(CommandEffect::Waterfall);
        } else {
            auto *arena=c.outdoor?c.outdoor->combat:c.quest_world?c.quest_world->encounter:nullptr;
            auto *assets=c.outdoor?c.outdoor->resources:c.quest_world?c.quest_world->combat_resources:nullptr;
            if(!arena || !assets)break;
            const auto saved = c.events; c.events = r.sink();
            auto &encounter = *arena;
            auto resources=*assets; resources.remove_enemy=nullptr;
            const auto status = start_encounter_combat(c, encounter.combat, resources,
                                                       41, r.tile(), -1, CombatDirection::South, false);
            c.events = saved;
            if (status == CombatResult::Ok) {
                encounter.combat.loot_x=c.commands.troll_x;
                encounter.combat.loot_y=c.commands.troll_y;
                encounter.combat.has_world_loot_origin=true;
                c.combat_context = &encounter;
            }
            else r.result.status = CommandStatus::CoreError;
        }
        break;
    }
    case CommandKind::YellSails:
        if ((c.turn.transport_tile & 0xf8) != 0x20 || c.game.position.map.location >= 0x80) {
            r.message("what?");
            r.result.status = CommandStatus::Rejected;
        } else {
            const bool furl = (c.turn.transport_tile & 0xfc) == 0x20;
            r.message(furl ? "FURL!" : "HOIST!");
            r.sync_transport(c.turn.transport_tile + (furl ? 4 : -4));
            if (furl) c.turn.sail_dir = 0;
            r.turn();
            r.event(GameEventKind::MapChanged);
        }
        break;
    case CommandKind::Get:case CommandKind::Search:{auto q=cmd.kind==CommandKind::Get?get_quest_object(c,cmd.direction,r.sink()):search_world(c,cmd.has_direction?&cmd.direction:nullptr,r.sink(),r.rand,cmd.member);r.result.status=q.status;if(q.turn)r.turn();break;}
    case CommandKind::Yell: {
        auto q=yell_in_world(c,TalkText(cmd.text?cmd.text:u"",cmd.text_length),r.sink());
        r.result.status=q.status;if(q.turn)r.turn();if(q.map_after_turn)r.event(GameEventKind::MapChanged);break;
    }
    case CommandKind::HarpsichordNote:
        r.result.status=play_harpsichord(c,cmd.item,r.sink());break;
    case CommandKind::Board:
    case CommandKind::Disembark: {
        const auto *s = c.transport_services;
        if (!s || !s->tile_at || !s->reserve || !s->remove_boarded || !s->drop || !s->park_ship) {
            r.result.status = CommandStatus::InvalidContext;
            break;
        }
        const auto pos = c.game.position;
        int x = pos.xy.x, y = pos.xy.y;
        int under = s->tile_at(s->context, x, y);
        TransportResult tr;
        if (cmd.kind == CommandKind::Board) {
            int32_t hull = c.game.ship_hull, skiffs = c.game.ship_skiffs;
            if (s->ship_at && s->ship_at(s->context, pos, hull, skiffs)) {
                c.game.ship_hull = hull;
                c.game.ship_skiffs = skiffs;
            }
            tr = board_transport(c.game, under >= 256 ? under - 256 : 0, c.turn.transport_tile,
                                 s->horse_owned && s->horse_owned(s->context, pos));
            if (tr.damaged_warning)
                r.message("DANGER: SHIP BADLY DAMAGED!");
            if (tr.skiff_warning)
                r.message("WARNING: NO SKIFFS ON BOARD!");
            r.message(tr.message);
            if (tr.ok) {
                c.turn.transport_tile = tr.tile;
                c.game.transport = transport_mode(tr.tile);
                s->remove_boarded(s->context, pos, under);
            }
        } else {
            bool land = false;
            for (auto dir :
                 {Direction::North, Direction::South, Direction::East, Direction::West}) {
                auto delta = direction_delta(dir);
                int tile = s->tile_at(s->context, x + delta.dx, y + delta.dy);
                if (tile >= 0 && tile_properties(tile).value.walkable)
                    land = true;
            }
            bool parked = (c.turn.transport_tile & 0xfc) == 0x24;
            if (!s->reserve(s->context, parked)) {
                r.result.status = CommandStatus::NeedsStorage;
                break;
            }
            tr = disembark_transport(c.game, c.turn.transport_tile, land, (under & 0xfe) == 0x6a,
                                     under >= 0 && tile_properties(under).value.walkable);
            r.message(tr.message);
            if (tr.ok) {
                if (tr.drop_tile >= 0)
                    s->drop(s->context, pos, tr.drop_tile + 256);
                if (tr.parked_ship_tile >= 0)
                    s->park_ship(s->context, pos, tr.parked_ship_tile + 256, c.game.ship_hull,
                                 c.game.ship_skiffs);
                c.turn.transport_tile = tr.tile;
                c.game.transport = transport_mode(tr.tile);
            }
        }
        if (tr.ok) {
            r.turn();
            r.event(GameEventKind::MapChanged);
        } else
            r.result.status = CommandStatus::Rejected;
        break;
    }
    case CommandKind::UseItem: {
        // Extended use-table IDs, not equipment IDs. These Game methods do not
        // validate ownership (the selector does) and do not consume a turn.
        char text[96];
        if(cmd.item>=0 && cmd.item<16){auto a=world_magic(c,cmd,r.map(),r.sink(),r.rand);r.result.status=a.status;} else if (cmd.item==18 || cmd.item==19 || cmd.item==20 || cmd.item==33 || cmd.item==36 || (cmd.item>=29 && cmd.item<=31)) {
            r.result.status=use_quest_item(c,cmd.item,r.sink());
        } else if(cmd.item>=21&&cmd.item<=28){r.result.status=use_moonstone(c,cmd.item-21,r.sink());
        } else if (cmd.item == 16) {
            r.message("Carpet");
            if(c.game.position.map.location>=33){r.message("Not here!");r.result.status=CommandStatus::Rejected;}
            else if(c.game.transport==TransportMode::Ship){r.message("X-it ship first!");r.result.status=CommandStatus::Rejected;}
            else if(c.game.transport!=TransportMode::Foot){r.message("Only on foot!");r.result.status=CommandStatus::Rejected;}
            else if(c.game.magic_carpets>0){r.message("Boarded!");r.sync_transport(20+r.rand(0,1));--c.game.magic_carpets;}
        } else if (cmd.item == 17) {
            if(cmd.has_direction&&(!c.quest_world||!c.quest_world->volatile_tile)){r.result.status=CommandStatus::InvalidContext;break;}
            --c.game.skull_keys;r.message("Skull Key");
            const auto loc=c.game.position.map.location;
            if(loc>=33&&loc<=127){r.message("Not here!");r.result.status=CommandStatus::Rejected;break;}
            if(loc>=128||!cmd.has_direction)break;
            const auto d=direction_delta(cmd.direction);int x=c.game.position.xy.x+d.dx,y=c.game.position.xy.y+d.dy;
            if(!loc){x&=255;y&=255;}
            auto *q=c.quest_world;const auto tile=q->tile_at?q->tile_at(q->context,x,y):get_active_map(c.world,c.game.position.map).value.tile_at(x,y);
            if(tile==151||tile==152){q->volatile_tile(q->context,x,y,tile==151?184:186);r.event(GameEventKind::MapChanged);}
        } else if (cmd.item == 32) {
            r.message("Spyglass");
            if(c.game.position.map.location>=33 || c.game.position.map.floor<0 || c.game.position.map.floor>=128)r.message("Not here!");
            else if(c.game.time.hour>=6 && c.game.time.hour<=18)r.message("No stars!");
            else {r.message("Looking...");emit_zodiac(c,r.sink(),r.rand);}
        } else if (cmd.item == 35) {
            r.message("Watch");
            const auto &t = c.game.time;
            const int32_t hour = t.hour % 12 == 0 ? 12 : t.hour % 12;
            std::snprintf(text, sizeof(text), "The pocket watch reads %ld:%02ld %s.", long(hour),
                          long(t.minute), t.hour <= 11 ? "AM" : "PM");
            r.message(text);
        } else if (cmd.item == 34) {
            r.message("Sextant");
            if (c.game.position.map.floor > 127 || c.game.position.map.location != 0) {
                r.message("Only outdoors!");
                r.result.status = CommandStatus::Rejected;
            } else if (c.game.time.hour > 5 && c.game.time.hour < 19) {
                r.message("Only at night!");
                r.result.status = CommandStatus::Rejected;
            } else {
                std::snprintf(text, sizeof(text), "Position: %u, %u",
                              unsigned(c.game.position.xy.x), unsigned(c.game.position.xy.y));
                r.message(text);
            }
        } else if (cmd.item == 37) {
            r.message("Box");
            r.message("How?");
            r.result.status = CommandStatus::NoOp;
        } else
            r.result.status = CommandStatus::Unsupported;
        break;
    }
    case CommandKind::Ready:
        // battle = c.combat: inside an arena the reference passes the equip its
        // in-combat flag, which locks body armour (ids 9-15). A dungeon corridor
        // is NOT battle -- c.combat is false there, so armour changes stay legal,
        // matching the reference's `g_location > 0x7f` (any COMBAT map) gate
        // rather than "is the party underground".
        r.result.item = equip_item(c.game, cmd.member, cmd.item, &r.rand, c.combat);
        r.result.status = r.result.item.ok ? CommandStatus::Success : CommandStatus::Rejected;
        // A successful in-combat change must reach the arena, or the character
        // keeps swinging the weapon they just took off (reference:
        // game.readyItem() -> combat.syncPlayerEquip()). equip_item() stays
        // CombatState-unaware; this handler owns the orchestration, and the
        // helper touches only the equipment-derived cache.
        //
        // Turn cost: the reference charges the combat turn ONCE per 'R'
        // COMMAND, when the Ready picker closes (main.ts' closeAndEndTurn ->
        // CombatSession.playerReady), not once per item equipped inside the
        // still-open picker. That is a picker-lifecycle event this per-equip
        // command does not model -- AlphaRuntime reopens the selector after each
        // Ready -- so charging here would consume one combat turn per equip and
        // diverge from the reference. Left to the picker owner; see
        // GAMEPLAY_INTEGRATION_AUDIT.md R-06.
        if (r.result.item.ok && c.combat && c.combat_context &&
            &c.combat_context->game == &c.game)
            resync_player_equipment(*c.combat_context, cmd.member);
        break;
    case CommandKind::Unready:
        r.result.item = unequip_slot(c.game, cmd.member, cmd.slot);
        r.result.status = r.result.item.ok ? CommandStatus::Success : CommandStatus::Rejected;
        break;
    case CommandKind::Ignite:
        r.result.item = ignite_torch(c.game, r.rand, c.game.position.map.location);
        if (!r.result.item.ok) {
            r.message(r.result.item.message);
            r.result.status = CommandStatus::Rejected;
        }
        r.turn();
        break;
    case CommandKind::Rest: {
        const auto e = camp_context(c.game, c.turn, r.tile(),dungeon_camp);
        if (!e.ok) {
            r.message(e.message);
            r.turn();
            r.result.status = CommandStatus::Rejected;
            break;
        }
        if (e.ship) {
            RestContext ctx{c.game, c.turn, r.rand, r.sink(), {}, c.sky};
            camp_repair_ship(ctx);
            break;
        }
        if (cmd.hours <= 0) {
            r.result.status = CommandStatus::NoOp;
            break;
        }
        // H-148. `snap_npcs` is the port's hook for TOWN.OVL:0x1694
        // `town_populate_npcs`, which CMDS.OVL:0x0677 calls on every 10-minute
        // tick of the bed hole-up loop (inside it -- 0x068d `je 0x634` jumps
        // back). That is ONE routine over ONE 1988 table: 0x16a2-0x16b9 zeroes
        // all 31 interior object slots and every live NPC objIdx, then
        // 0x16c9-0x171b re-places every .NPC slot whose type byte is non-zero.
        // Chests are .NPC type 1 (TOWN.OVL:0x1795), and nothing records that
        // one was opened -- SJOG.OVL:0x112C only zeroes the slot -- so a looted
        // vault refills while the party sleeps. The port splits that table into
        // NpcActors and the quest-object pool, so the per-tick hook has to
        // drive both halves; the host owns only the NPC half.
        struct BedPopulate {
            Runner *runner;
            CommandContext *context;
            RestServices host;
        } populate{&r, &c, *c.rest_services};
        RestServices bed = *c.rest_services;
        if (e.bed) {
            bed.context = &populate;
            bed.snap_npcs = [](void *p) {
                auto &b = *static_cast<BedPopulate *>(p);
                if (b.host.snap_npcs)
                    b.host.snap_npcs(b.host.context);
                if (b.context->quest_world &&
                    !hydrate_interior_objects(*b.context, b.context->game.position.map.location))
                    b.runner->result.status = CommandStatus::NeedsStorage;
            };
            bed.occupied = [](void *p, int32_t x, int32_t y, int32_t z) {
                auto &b = *static_cast<BedPopulate *>(p);
                return b.host.occupied ? b.host.occupied(b.host.context, x, y, z) : false;
            };
        }
        RestContext ctx{c.game, c.turn, r.rand, r.sink(), bed, c.sky, c.terrain, &c.world};
        // The low-level Rest command has long accepted an already resolved
        // member (parity drivers and noninteractive callers). Only a choice
        // coming from the kernel-style watch picker needs its 'G' gate here.
        const auto guard = e.bed ? -1 : cmd.watch_requested
            ? camp_guard_choice(c.game, cmd.member) : cmd.member;
        if (!e.bed && cmd.watch_requested && guard < 0)
            r.message("None posted!\n\n");
        const auto result = e.bed ? bed_sleep(ctx, cmd.hours) : camp(ctx, cmd.hours, guard);
        if (result.ambush) {
            auto *arena=c.outdoor?c.outdoor->combat:c.quest_world?c.quest_world->encounter:nullptr;
            auto *assets=c.outdoor?c.outdoor->resources:c.quest_world?c.quest_world->combat_resources:nullptr;
            if(arena&&assets){
                auto resources=*assets;resources.remove_enemy=nullptr;
                const auto saved=c.events;c.events=r.sink();
                auto status=start_encounter_combat(c,arena->combat,resources,result.enemy,r.tile(),0,CombatDirection::South,false);
                c.events=saved;
                r.result.status=status==CombatResult::Ok?CommandStatus::Success:status==CombatResult::NeedsActorStorage?CommandStatus::NeedsStorage:CommandStatus::InvalidContext;
                if(status==CombatResult::Ok)c.combat_context=arena;
            }else{
                r.result.status = CommandStatus::AwaitingResponse;
                r.result.pending_camp_enemy = result.enemy;
                c.commands.pending_camp_enemy = result.enemy;
            }
        }
        break;
    }
    case CommandKind::RestCancel:
        r.result.status = CommandStatus::NoOp;
        break;
    case CommandKind::Move:
        r.move(cmd.direction);
        break;
    case CommandKind::Pass:
        r.turn(true, nullptr, true);
        break;
    case CommandKind::Enter:
        r.enter();
        break;
    case CommandKind::Exit:
    case CommandKind::DeclineExit:
        r.result.error =
            confirm_town_exit(c.game, cmd.kind == CommandKind::Exit, c.locations, r.transitions());
        if (r.result.error == Error::None)
            c.commands.awaiting_exit = false;
        else
            r.result.status = CommandStatus::CoreError;
        break;
    case CommandKind::Klimb:
        r.klimb(cmd);
        break;
    case CommandKind::KlimbCancel:
        if (c.game.position.map.location)
            r.turn();
        else
            r.result.status = CommandStatus::NoOp;
        break;
    case CommandKind::AutoSleep:
        if (!c.game.position.map.location || r.conscious() != 1)
            r.result.status = CommandStatus::NoOp;
        else {
            r.message("Zzzzzz...\n");
            r.turn();
        }
        break;
    default:
        r.result.status = CommandStatus::Unsupported;
        break;
    }
    r.result.turns = c.game.turns_since_start - before;
    return r.result;
}
ActionResult execute_command(CommandContext &c, Command cmd) { return execute(c, cmd, false); }
ActionResult dispatch_world_command(CommandContext &c, Command cmd) {
    return execute(c, cmd, true);
}
} // namespace openu5
