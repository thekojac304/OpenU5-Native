#include "openu5/dialogue_orchestration.h"
#include <algorithm>
namespace openu5 {
void dialogue_alarm(NpcActors &actors, uint8_t location, Rand rand) {
    // Slots, not container order, define the RNG stream.
    for (unsigned slot = 0; slot < 32; ++slot) for (size_t i = 0; i < actors.count; ++i) {
        auto &n = actors.actors[i]; auto &s = n.schedule;
        if (n.location != location || s.slot != slot) continue;
        if (s.type == 0xfc || s.type == 0xd8 || s.type == 0x70) {
            for (auto &a : s.ai) a = 7;
            for (auto &t : s.times) t = 0;
        } else {
            if (rand(0,255) >= 128) continue;
            bool scheduled = false; for (auto t : s.times) scheduled |= t != 0;
            if (s.type < 0x40 || s.type >= 0x74 || (s.dialog != 0xfe && !scheduled)) continue;
            s.dialog = 0xfd; for (auto &a : s.ai) a = 3;
        }
    }
}
void dialogue_despawn(GameState &g, NpcActors *actors, const NpcActor &npc) {
    const auto fam = npc.schedule.type & 0xfc;
    const auto loc = g.position.map.location;
    if (((fam < 0x80 && fam != 0x70) || fam == 0xb4) && loc >= 1 && loc <= 32 && npc.schedule.slot < 32)
        g.npc_dead[loc-1] |= uint32_t(1) << npc.schedule.slot;
    if (!actors) return;
    for (size_t i = 0; i < actors->count; ++i) if (actors->actors[i].location == npc.location && actors->actors[i].schedule.slot == npc.schedule.slot) {
        for (size_t j = i+1; j < actors->count; ++j) actors->actors[j-1] = actors->actors[j];
        --actors->count; break;
    }
    // NpcActors IS the native runtime walk state; save-side mirror is deferred.
}
namespace {
std::u16string effective(const CharacterState *c) {
    std::u16string name;
    if (c) for (size_t i = 0; i < 8 && c->name[i]; ++i) name += char16_t(static_cast<unsigned char>(c->name[i]));
    const auto trimmed = talk_trim(name);
    return trimmed.empty() ? u"Avatar" : std::u16string(trimmed);
}
struct Delivery {
    CommandContext &c; DialogueServices &d; ActionResult result{};
    void message(const char *text) {
        GameEvent e; e.kind = GameEventKind::Message; e.text = text;
        ++result.event_count; if (c.events.emit) c.events.emit(c.events.context,e);
    }
    void event(DialogueEventKind kind, const DialogueOutput *o = nullptr, TalkText text = {}, DialogueHandoff handoff = DialogueHandoff::None) {
        DialogueEvent payload{kind,o,text,handoff,d.session.npc.location,d.session.npc.schedule.slot};
        GameEvent e; e.kind = GameEventKind::Dialogue; e.dialogue = &payload;
        ++result.event_count; if (c.events.emit) c.events.emit(c.events.context,e);
    }
    void handoff(DialogueHandoff h) {
        event(DialogueEventKind::Handoff,nullptr,{},h);
        EventSink sink{this,[](void *p,const GameEvent &e) {
            auto &delivery = *static_cast<Delivery *>(p);
            ++delivery.result.event_count;
            if (delivery.c.events.emit) delivery.c.events.emit(delivery.c.events.context,e);
        }};
        if (!d.handoff || !d.handoff(d.context,h,d.session.npc,sink)) {
            d.session.deferred = h; result.status = CommandStatus::Unsupported;
        }
    }
    void end() {
        handoff(DialogueHandoff::QuestEnd);
        d.session.active = false;
        event(DialogueEventKind::Ended);
    }
    void render(const std::vector<DialogueOutput> &outputs, bool input) {
        bool armed = false;
        for (const auto &o : outputs) {
            event(DialogueEventKind::Output,&o);
            if (o.kind == DialogueOutputKind::Prompt) { armed = true; break; }
            if (o.kind != DialogueOutputKind::Effect || !input) continue;
            const auto res = apply_dialogue_effect(c.game,o.effect,d.session.npc_name,d.session.npc.schedule.type);
            for (uint8_t i = 0; i < res.message_count; ++i) event(DialogueEventKind::EffectMessage,nullptr,res.messages[i]);
            if (res.despawn_npc) dialogue_despawn(c.game,c.actors,d.session.npc);
            if (res.alarm && c.actors) dialogue_alarm(*c.actors,c.game.position.map.location,{&c,[](void *p,int32_t lo,int32_t hi)->int32_t {
                auto &context = *static_cast<CommandContext *>(p);
                const auto n = context.game.rng.next(lo,hi).value;
                if (context.rng_trace.emit) context.rng_trace.emit(context.rng_trace.context,"talk-alarm",lo,hi,n);
                return n;
            }});
        }
        const auto &npc = d.session.npc;
        if (d.session.conversation.met_avatar() && npc.location >= 1 && npc.location <= 32 && npc.schedule.slot < 32)
            c.game.npc_met[npc.location-1] |= uint32_t(1) << npc.schedule.slot;
        if (!armed) end(); else result.status = CommandStatus::AwaitingResponse;
    }
};
}
ActionResult execute_dialogue_command(CommandContext &c, Command cmd) {
    ActionResult invalid; invalid.status = CommandStatus::InvalidContext;
    if (!c.dialogue_services || c.combat || c.dungeon) return invalid;
    auto &d = *c.dialogue_services; auto &s = d.session; Delivery out{c,d};
    if (s.active && s.game != &c.game) return invalid;
    if (cmd.kind == CommandKind::EndConversation) {
        if (!s.active) { out.result.status = CommandStatus::NoOp; return out.result; }
        s.conversation.cancel(); out.end(); return out.result;
    }
    if (cmd.kind >= CommandKind::DialogueText && cmd.kind <= CommandKind::DialogueNo) {
        if (!s.active || (cmd.text_length && !cmd.text)) return invalid;
        const TalkText text = cmd.kind == CommandKind::DialogueYes ? u"yes" : cmd.kind == CommandKind::DialogueNo ? u"no" : TalkText(cmd.text ? cmd.text : u"",cmd.text_length);
        out.render(s.conversation.input(text),true); return out.result;
    }
    if (s.active) { out.result.status = CommandStatus::AwaitingResponse; return out.result; }
    if (cmd.kind == CommandKind::Talk && (!cmd.has_direction || uint8_t(cmd.direction) > 3)) return invalid;
    const auto loc = c.game.position.map.location;
    const NpcActor *npc = nullptr;
    const auto delta = direction_delta(cmd.direction);
    if (loc && c.actors) for (size_t i = 0; i < c.actors->count; ++i) {
        const auto &n = c.actors->actors[i];
        if (n.location != loc) continue;
        const bool match = cmd.kind == CommandKind::BeginConversation ? n.schedule.slot == cmd.member :
            (n.z == c.game.position.map.floor && n.x == int(c.game.position.xy.x)+delta.dx && n.y == int(c.game.position.xy.y)+delta.dy);
        if (match) { npc = &n; break; }
    }
    if (!npc) { out.message("Funny, no response!"); out.result.status = CommandStatus::Rejected; return out.result; }
    s.npc = *npc; s.deferred = DialogueHandoff::None;
    const auto dialog = npc->schedule.dialog;
    if (dialog >= 0x81 && dialog <= 0x88) { out.handoff(DialogueHandoff::Shop); return out.result; }
    if (dialog == 0xff && loc == 0x12) { out.handoff(DialogueHandoff::Guard); return out.result; }
    if (dialog == 0xfd || dialog == 0xfe) {
        out.message(dialog == 0xfd ? "\"Don't hurt me!\nPlease go away!\"\n" : "\"Begone,\nvermin!\"\n"); return out.result;
    }
    const auto *script = talk_script_for(d.registry,loc,dialog);
    if (!script) { out.message("Funny, no response!"); out.result.status = CommandStatus::Rejected; return out.result; }
    const CharacterState *avatar = c.game.party.character_count ? &c.game.party.characters[0] : nullptr;
    for (uint8_t i = 0; i < c.game.party.character_count; ++i) if (c.game.party.characters[i].character_class == 'A') { avatar = &c.game.party.characters[i]; break; }
    s.avatar = effective(avatar);
    const auto party = party_members(c.game.party);
    for (uint8_t i = 0; i < party.count; ++i) { s.names[i] = effective(&c.game.party.characters[party.indices[i]]); s.name_views[i] = s.names[i]; }
    s.npc_name.clear(); for (const auto &i : script->name) if (i.op == TalkOp::Text) s.npc_name += i.text;
    s.npc_name = std::u16string(talk_trim(s.npc_name));
    auto ctx = d.language; ctx.avatar_name = s.avatar; ctx.party_names = {s.name_views,party.count}; ctx.has_party_names = true;
    ctx.knows = npc->location >= 1 && npc->location <= 32 && npc->schedule.slot < 32 && (c.game.npc_met[npc->location-1] & (uint32_t(1)<<npc->schedule.slot));
    // Keep language callback context independent of RNG binding.
    // Roll lazily: description/AskName can suspend before the opening coin toss.
    s.game = &c.game; s.trace = c.rng_trace;
    ctx.self_intro_context = &s;
    ctx.self_intro_roll = [](void *p)->int32_t {
        auto &session = *static_cast<DialogueSession *>(p);
        const auto n = session.game->rng.next(0,1).value;
        if (session.trace.emit) session.trace.emit(session.trace.context,"talk-self-intro",0,1,n);
        return n;
    };
    s.conversation.bind(*script,ctx);
    s.active = true; out.render(s.conversation.start(),false); return out.result;
}
} // namespace openu5
