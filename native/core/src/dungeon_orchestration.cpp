#include "openu5/dungeon.h"
#include "openu5/loot.h"
namespace openu5 {
namespace {
void emit(EventSink sink, GameEventKind k, const char *text = nullptr) {
    GameEvent e;
    e.kind = k;
    e.text = text;
    if (sink.emit)
        sink.emit(sink.context, e);
}
void reload(CommandContext &c, ReloadEffect e) {
    if (c.services.reload)
        c.services.reload(c.services.context, e, 0, c.events);
}
void translate(CommandContext &c) {
    auto &ctx = *c.dungeon_context;
    for (size_t i = 0; i < ctx.scratch.count; ++i) {
        auto e = ctx.scratch.events[i];
        switch (e.kind) {
        case DungeonEventKind::Message:
            emit(c.events, GameEventKind::Message, e.text);
            break;
        case DungeonEventKind::Sfx:
            emit(c.events, GameEventKind::Sfx, e.text);
            break;
        case DungeonEventKind::ExitSurface:
            exit_dungeon(c, false);
            break;
        case DungeonEventKind::ExitUnderworld:
            exit_dungeon(c, true);
            break;
        case DungeonEventKind::Room:
            if (ctx.start_room)
                ctx.start_room(ctx.context, e.value, c.events);
            break;
        case DungeonEventKind::Corridor:
            if (ctx.start_corridor)
                ctx.start_corridor(ctx.context, e.value == 1, c.events);
            break;
        case DungeonEventKind::Loot: {
            char text[128];
            loot_item_name({e.value, e.member}, text, sizeof(text));
            emit(c.events, GameEventKind::Message, text);
            break;
        }
        case DungeonEventKind::DamageScript: {
            GameEvent event;
            event.kind = GameEventKind::PoisonTick;
            for (int n = 0; n < 6; ++n)
                if (e.value & (1 << n))
                    event.slots[event.slot_count++] = uint8_t(n);
            if (c.events.emit)
                c.events.emit(c.events.context, event);
            break;
        }
        default:
            break;
        }
    }
}
} // namespace
void exit_dungeon(CommandContext &c, bool under) {
    auto &d = c.dungeon_context->state;
    size_t i = d.active ? size_t(d.pos.dungeon - 1) : 32;
    d.active = false;
    c.dungeon = false;
    c.game.position.map = {0, int16_t(under ? 255 : 0)};
    c.game.position.xy = {i < c.locations.x_count ? c.locations.x[i] : uint8_t(0),
                          i < c.locations.y_count ? c.locations.y[i] : uint8_t(0)};
    if (under)
        reload(c, ReloadEffect::HydrateUnderworld);
    emit(c.events, GameEventKind::Message, under ? "Exit to Underworld!" : "Exit to Britannia!");
    emit(c.events, GameEventKind::DungeonExited);
    emit(c.events, GameEventKind::MapChanged);
}
void dungeon_combat_return(CommandContext &c, int delta, int border, bool victory) {
    auto &ctx = *c.dungeon_context;
    auto &d = ctx.state;
    int cause = ctx.corridor_cause;
    ctx.corridor_cause = -1;
    ctx.room_entry_valid = false;
    if (!d.active)
        return;
    if ((cause >= 0 || !victory) && delta) {
        int f = d.pos.floor + delta;
        if (f < 0)
            exit_dungeon(c, false);
        else if (f >= 8)
            exit_dungeon(c, true);
        else {
            d.pos.floor = uint8_t(f);
            emit(c.events, GameEventKind::MapChanged);
        }
    } else if (cause == 0 && border >= 0) {
        static constexpr int dx[] = {1, -1, 0, 0}, dy[] = {0, 0, 1, -1}, facing[] = {1, 3, 2, 0};
        d.pos.x = uint8_t((d.pos.x + dx[border]) & 7);
        d.pos.y = uint8_t((d.pos.y + dy[border]) & 7);
        d.pos.facing = DungeonFacing(facing[border]);
        emit(c.events, GameEventKind::MapChanged);
    }
    if (cause >= 0 && d.active)
        dungeon_respawn(c.game, d);
}
static ActionResult run_dungeon_command(CommandContext &c, Command cmd) {
    ActionResult result;
    if (!c.dungeon_context || c.combat) {
        result.status = CommandStatus::InvalidContext;
        return result;
    }
    auto &ctx = *c.dungeon_context;
    auto &d = ctx.state;
    if((ctx.count && !ctx.data) || (c.locations.x_count && !c.locations.x) ||
       (c.locations.y_count && !c.locations.y) || c.game.party.character_count>16 ||
       c.game.party.party_size<0 || c.game.party.party_size>6 ||
       (d.active && (d.pos.floor>7 || d.pos.x>7 || d.pos.y>7 || int(d.pos.facing)>3))) {
        result.status=CommandStatus::InvalidContext;return result;
    }
    if (cmd.kind == CommandKind::EnterDungeon) {
        const DungeonData *data = nullptr;
        for (size_t i = 0; i < ctx.count; ++i)
            if (ctx.data[i].location == cmd.member) {
                data = &ctx.data[i];
                break;
            }
        if (!data) {
            result.status = CommandStatus::NoOp;
            return result;
        }
        const char *banner =
            c.services.banner ? c.services.banner(c.services.context, data->location) : nullptr;
        if (banner)
            emit(c.events, GameEventKind::Message, banner);
        dungeon_load(c.game, d, *data, cmd.hours == 255);
        c.dungeon = true;
        GameEvent e;
        e.kind = GameEventKind::DungeonEntered;
        e.dungeon_id = data->location;
        
        if (c.events.emit)
            c.events.emit(c.events.context, e);
        reload(c, ReloadEffect::ClearEnemies);
        return result;
    }
    if (!d.active) {
        result.status = CommandStatus::NoOp;
        return result;
    }
    if (cmd.item < 0 || cmd.item > int(DungeonAction::Search)) {
        result.status = CommandStatus::Unsupported;
        return result;
    }
    auto action = DungeonAction(cmd.item);
    auto before = c.game.turns_since_start;
    TurnResult tr;
    if (action != DungeonAction::Attack)
        tr = advance_turn(c.game, c.turn, 1, rng_source(c.game.rng), c.sky);
    ctx.scratch.count = 0;
    DungeonSink sink{&ctx.scratch, [](void *p, const DungeonEvent &e) {
                         auto &s = *static_cast<DungeonScratch *>(p);
                         s.events[s.count++] = e;
                     }};
    dungeon_action(c.game, c.turn, d, action, sink, cmd.hours, cmd.member);
    for (size_t i = 0; i < ctx.scratch.count; ++i)
        if (ctx.scratch.events[i].kind == DungeonEventKind::Error) {
            result.status = CommandStatus::CoreError;
            result.error = Error(ctx.scratch.events[i].value);
            return result;
        }
    if (action != DungeonAction::Attack && action != DungeonAction::Tick) {
        bool ticked = false;
        for (size_t i = 0; i < ctx.scratch.count; ++i) {
            auto k = ctx.scratch.events[i].kind;
            if (k == DungeonEventKind::Moved || k == DungeonEventKind::FloorChanged ||
                k == DungeonEventKind::ExitSurface || k == DungeonEventKind::ExitUnderworld)
                ticked = true;
        }
        if (!ticked)
            dungeon_action(c.game, c.turn, d, DungeonAction::Tick, sink);
    }
    for (int i = 0; i < tr.message_count; ++i)
        emit(c.events, GameEventKind::Message, turn_message_text(tr.messages[size_t(i)]));
    translate(c);
    if (action != DungeonAction::Attack) {
        if (ctx.rescue_hook && action != DungeonAction::Tick)
            ctx.rescue_hook(ctx.context, c.events);
        if (c.services.effect)
            c.services.effect(c.services.context, CommandEffect::Refuge, c.events);
    }
    result.turns = c.game.turns_since_start - before;
    return result;
}
ActionResult execute_dungeon_command(CommandContext &c,Command cmd){
    struct Delivery {EventSink sink;uint32_t count=0;} delivery{c.events};
    c.events={&delivery,[](void *p,const GameEvent &e){auto &d=*static_cast<Delivery*>(p);++d.count;if(d.sink.emit)d.sink.emit(d.sink.context,e);}};
    auto result=run_dungeon_command(c,cmd);c.events=delivery.sink;result.event_count=delivery.count;return result;
}
} // namespace openu5

