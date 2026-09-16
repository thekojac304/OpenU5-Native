#include "openu5/commands.h"
#include "openu5/combat.h"
#include "openu5/dungeon.h"
#include "openu5/rest.h"
#include "openu5/transport.h"
#include "openu5/dialogue_orchestration.h"
#include <cstdio>
namespace openu5 {
namespace {
struct Runner {
    CommandContext &c;
    ActionResult result{};
    Rand rand;
    EventSink sink() {
        return {this, [](void *p, const GameEvent &e) { static_cast<Runner *>(p)->emit(e); }};
    }
    void emit(const GameEvent &e) {
        ++result.event_count;
        if (c.events.emit)
            c.events.emit(c.events.context, e);
    }
    void event(GameEventKind kind, const char *text = nullptr) {
        GameEvent e;
        e.kind = kind;
        e.text = text;
        emit(e);
    }
    void message(const char *s) { event(GameEventKind::Message, s); }
    bool effect(CommandEffect e) {
        return c.services.effect && c.services.effect(c.services.context, e, sink());
    }
    ActiveMap map() { return get_active_map(c.world, c.game.position.map).value; }
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
        (void)roll_spawn_gate(rand, under, c.game.position.map.floor, c.game.time.hour);
        // Game.outdoorWorldTurn with combatResources absent ends after this gate.
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
        event(GameEventKind::WalkEcho, names[static_cast<uint8_t>(dir)]);
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
        const auto resolved = resolve_unoccupied_foot_step(c.game, m, dir);
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
            apply_stair_step(c.game, c.world, tile(), dir, transitions());
        turn(true, &s);
        effect(CommandEffect::Moongate);
        effect(CommandEffect::ShrineEntry);
    }
    void enter() {
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
        if (c.game.transport == TransportMode::Horse) {
            message("Klimb--On foot!");
            result.status = CommandStatus::Rejected;
            return;
        }
        const auto under = tile();
        if (under == 200 || under == 201 || under == 134) {
            if (!klimb_ladder(c.game, c.world, under == 200 ? 1 : -1, transitions()))
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
    if (cmd.kind >= CommandKind::Talk && cmd.kind <= CommandKind::EndConversation)
        return execute_dialogue_command(c, cmd);
    if (c.dialogue_services && c.dialogue_services->session.active) {
        ActionResult result;
        result.status = CommandStatus::AwaitingResponse;
        return result;
    }
    if (cmd.kind == CommandKind::EnterDungeon || cmd.kind == CommandKind::DungeonCommand)
        return execute_dungeon_command(c, cmd);
    if ((cmd.kind >= CommandKind::CombatMove && cmd.kind <= CommandKind::Cast) ||
        (cmd.kind >= CommandKind::CombatKlimb && cmd.kind <= CommandKind::CombatOpen)) {
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
                ? CombatAction::Open
                : static_cast<CombatAction>(int(cmd.kind) - int(CommandKind::CombatMove));
        CombatPoint aim{cmd.combat_x, cmd.combat_y};
        const auto status =
            cmd.kind == CommandKind::Cast
                ? combat_cast(arena, static_cast<SpellId>(cmd.item),
                              cmd.has_target ? &aim : nullptr, cmd.member, cmd.cancel_target)
                : combat_action(arena, action,
                                (action == CombatAction::Get || action == CombatAction::Open) &&
                                        !cmd.has_direction
                                    ? -1
                                    : cmd.combat_x,
                                cmd.combat_y);
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
    if (c.combat || c.dungeon ||
        (c.game.position.map.location >= 33 && c.game.position.map.location <= 40) ||
        (static_cast<uint8_t>(cmd.kind) > static_cast<uint8_t>(CommandKind::UseItem) &&
         cmd.kind != CommandKind::Board && cmd.kind != CommandKind::Disembark) ||
        ((cmd.kind == CommandKind::Move || cmd.has_direction) &&
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
    if ((cmd.kind == CommandKind::Move || intercept) &&
        (c.turn.transport_tile != 28 || c.game.transport != TransportMode::Foot)) {
        r.result.status = CommandStatus::Unsupported;
        return r.result;
    }
    if ((cmd.kind == CommandKind::Move && c.game.transport != TransportMode::Foot) ||
        (cmd.kind == CommandKind::Klimb && !c.game.position.map.location)) {
        r.result.status = CommandStatus::Unsupported;
        return r.result;
    }
    // Preflight all possible stagger targets and destination floors before RNG.
    // Bridge tolls / trapdoors need deferred subsystem state and cannot be
    // skipped.
    auto unsupported_tile = [&](int32_t tile) {
        return (c.game.position.map.location && tile == 140) ||
               (!c.game.position.map.location && (tile == 106 || tile == 107));
    };
    bool unsupported = c.game.position.map.location && r.tile() == 140;
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
        unsupported = unsupported || r.tile() == 17 || r.tile() == 25 ||
                      ((id >= 33 && id <= 40) &&
                       (!c.dungeon_context || (id == 40 && !c.dungeon_context->entry_hook)));
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
                          next.value.tile_at(c.game.position.xy.x, c.game.position.xy.y) == 140;
        }
    }
    if (unsupported) {
        r.result.status = CommandStatus::Unsupported;
        return r.result;
    }
    if (cmd.kind == CommandKind::Rest && cmd.hours > 0) {
        const auto eligible = camp_context(c.game, c.turn, r.tile());
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
        if (cmd.item == 35) {
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
        r.result.item = equip_item(c.game, cmd.member, cmd.item, &r.rand);
        r.result.status = r.result.item.ok ? CommandStatus::Success : CommandStatus::Rejected;
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
        const auto e = camp_context(c.game, c.turn, r.tile());
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
        RestContext ctx{c.game, c.turn, r.rand, r.sink(), *c.rest_services, c.sky};
        const auto result = e.bed ? bed_sleep(ctx, cmd.hours) : camp(ctx, cmd.hours, cmd.member);
        if (result.ambush) {
            r.result.status = CommandStatus::AwaitingResponse;
            r.result.pending_camp_enemy = result.enemy;
            c.commands.pending_camp_enemy = result.enemy;
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
