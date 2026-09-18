#include "openu5/shrine.h"
#include <cstdio>
#include <string>
namespace openu5 {
namespace {
void event(EventSink sink, GameEventKind kind, const char *text = nullptr) {
    GameEvent e; e.kind=kind; e.text=text; if (sink.emit) sink.emit(sink.context,e);
}
struct Delivery {
    CommandContext &c; ActionResult result;
    void emit(GameEventKind kind,const char *text=nullptr) { ++result.event_count; event(c.events,kind,text); }
    void message(const char *text) { emit(GameEventKind::Message,text); }
    void wait() { emit(GameEventKind::ShrineKeyWait); }
    void sound(const char *id) { emit(GameEventKind::Sfx,id); }
};
}
void shrine_guardian(GameState &g,EventSink sink) {
    auto &p=g.position;
    if (p.map.floor!=0 || p.xy.x!=233 || p.xy.y!=235) return;
    if (g.quest.shrine_quest) event(sink,GameEventKind::Message,"\n\"Pass, Seeker!\"\n");
    else {
        event(sink,GameEventKind::Message,"\n\"Thou art not upon a Sacred Quest!\n");
        event(sink,GameEventKind::Message,"Passage denied!\"\n");
        p.xy.y=uint8_t(p.xy.y+1);
    }
}
void shrine_entry(GameState &g,ShrineServices &s,int32_t tile,EventSink sink) {
    const auto &p=g.position;
    if (p.map.location || p.map.floor || !s.data || tile!=26) return;
    const auto v=shrine_index_at(*s.data,p.xy.x,p.xy.y);
    if (v<0) return;
    s.session.restore=int8_t(v); s.session.x=p.xy.x; s.session.y=p.xy.y;
    event(sink,GameEventKind::ShrineRestorePrompt);
}
ActionResult enter_shrine(CommandContext &c,int32_t tile) {
    Delivery d{c,{}};
    if (!c.shrine_services) { d.result.status=CommandStatus::InvalidContext; return d.result; }
    if (c.game.position.map.location) { d.message("Enter what?"); d.result.status=CommandStatus::Rejected; return d.result; }
    ShrineInput input; input.action=ShrineAction::Codex;
    if (tile==17) d.message("Enter the Shrine of the Codex!");
    else {
        const auto *data=c.shrine_services->data;
        const auto v=data && tile==25 ? shrine_index_at(*data,c.game.position.xy.x,c.game.position.xy.y) : -1;
        if (v<0) { d.message("Enter What?"); d.result.status=CommandStatus::Rejected; return d.result; }
        std::string echo="Enter the shrine of\n";
        // Canonical English DATA.OVL names are bytes; typed answers remain UTF-16.
        for (auto ch:data->virtues[v]) {
            if (ch>127) { d.result.status=CommandStatus::InvalidContext; return d.result; }
            echo+=char(ch);
        }
        echo+='\n'; d.message(echo.c_str()); input.action=ShrineAction::Visit; input.value=v;
    }
    auto result=execute_shrine(c,input); result.event_count+=d.result.event_count; return result;
}
ActionResult execute_shrine(CommandContext &c,ShrineInput input) {
    Delivery d{c,{}};
    if (!c.shrine_services || c.combat || c.dungeon) { d.result.status=CommandStatus::InvalidContext; return d.result; }
    auto &services=*c.shrine_services; auto &session=services.session; const auto *data=services.data;
    auto record=[&](int32_t i){return services.record ? services.record(services.context,i) : nullptr;};
    if (input.action==ShrineAction::Donate) {
        if (input.value<=0) { d.message("0 gp\n"); return d.result; }
        const auto result=shrine_donate(c.game,input.value);
        char text[48]; std::snprintf(text,sizeof(text),"%lld gp\n\n",static_cast<long long>(result.cost)); d.message(text);
        if (!result.accepted) {
            d.message("Thou hast not that much gold!"); d.emit(GameEventKind::ShrineDonatePrompt);
            d.result.status=CommandStatus::AwaitingResponse; return d.result;
        }
        d.message("ALAKAZAM!\n"); d.emit(GameEventKind::RitualInvert,"donation");
        d.sound("shrine-donation"); d.emit(GameEventKind::PartyChanged); return d.result;
    }
    if (input.action==ShrineAction::SubmitRestore) {
        const auto v=session.restore; session.restore=-1;
        if (v<0 || !data) return d.result;
        if (!shrine_restore(c.game,uint8_t(v),input.virtue,input.mantras,session.x,session.y,*data)) d.message("\n");
        else { d.message("\n\nThe Shrine is\nrestored!\n"); d.emit(GameEventKind::MapChanged); }
        return d.result;
    }
    if (input.action==ShrineAction::SubmitVisit) {
        const auto v=session.visit;
        if (v<0 || !data) { session.visit=-1; return d.result; }
        bool empty=input.virtue.empty(); for (auto m:input.mantras) empty=empty || m.empty();
        if (empty) { session.visit=-1; return d.result; }
        if (!shrine_visit_check(uint8_t(v),input.virtue,input.mantras,*data)) {
            session.visit=-1; d.message("\n\nThine thoughts are unfocused.\n"); return d.result;
        }
        const auto mode=shrine_mode(c.game,uint8_t(v));
        const auto *page=mode==ShrineMode::ShowMantra ? record(12+v) : nullptr;
        if (mode==ShrineMode::ShowMantra && !page) { d.result.status=CommandStatus::InvalidContext; return d.result; }
        if (mode==ShrineMode::QuestComplete && !c.game.party.character_count) { d.result.status=CommandStatus::InvalidContext; return d.result; }
        session.visit=-1;
        if (mode==ShrineMode::ShowMantra) {
            shrine_show_mantra(c.game,uint8_t(v));
            d.message("\n\nThe Altar speaks and a Quest is ordained! "); d.wait();
            const auto text=std::string("\n\n\"'Tis now thy sacred Quest to go unto the Codex and learn ")+page+"\"\n";
            d.message(text.c_str()); d.wait(); d.message("\n\"Return again when thy Quest is done!\"\n"); d.sound("shrine-ordained");
        } else if (mode==ShrineMode::Donation) {
            d.emit(GameEventKind::ShrineDonatePrompt); d.result.status=CommandStatus::AwaitingResponse;
        } else {
            const auto attrs=shrine_complete_quest(c.game,uint8_t(v),c.game.party.characters[0]);
            d.message("\n\nA thunderous voice booms:\n\n\"WELL DONE!\"\n\n");
            d.emit(GameEventKind::RitualInvert); d.sound("shrine-well-done"); d.emit(GameEventKind::Quake); d.sound("quake");
            const char *labels[]={"Strength +1\n","Dexterity +1\n","Intelligence +1\n"};
            for (unsigned i=0;i<3;++i) if (attrs & (1u<<i)) d.message(labels[i]);
            d.emit(GameEventKind::PartyChanged);
        }
        return d.result;
    }
    if (!data) return d.result;
    if (input.action==ShrineAction::Visit) {
        if (input.value<0 || input.value>=data->count) { d.result.status=CommandStatus::Rejected; return d.result; }
        session.visit=int8_t(input.value);
        d.message("\nThou dost approach the tranquil Shrine...\n\n");
        d.message("...and thou dost kneel before the Altar.\n\n"); d.emit(GameEventKind::ShrineVisitPrompt);
        d.result.status=CommandStatus::AwaitingResponse; return d.result;
    }
    if (input.action!=ShrineAction::Codex) { d.result.status=CommandStatus::Rejected; return d.result; }
    int32_t v=-1; for (int32_t i=0;i<8;++i) if (c.game.quest.shrine_quest & (1<<i)) { v=i; break; }
    const bool ceremony=v>=0 && ((c.game.quest.shrine_visited | (1<<v)) & 255)==255;
    if (v>=0 && !record(20+v)) { d.result.status=CommandStatus::InvalidContext; return d.result; }
    if (ceremony) for (int32_t i=40;i<45;++i) if (!record(i)) { d.result.status=CommandStatus::InvalidContext; return d.result; }
    d.message("\nThe Codex of Ultimate Wisdom lies before thee...");
    unsigned urns=0;
    for (uint8_t i=1;i<c.game.party.character_count;++i) {
        const auto &ch=c.game.party.characters[i]; if (ch.party_status==127 && ch.name[0]) ++urns;
    }
    if (urns) {
        d.message("\n\nThou dost see\n"); d.message(urns==1 ? "an urn marked:\n\n" : "urns marked:\n\n");
        for (uint8_t i=1;i<c.game.party.character_count;++i) {
            const auto &ch=c.game.party.characters[i]; if (ch.party_status!=127 || !ch.name[0]) continue;
            char name[12]; std::snprintf(name,sizeof(name),"%.9s\n",ch.name); d.message(name);
        }
    }
    d.wait(); d.message("\nThe book is open to the page thou dost seek!\n\n"); d.wait();
    d.message("Upon the hallowed page thou dost read:\n\n"); d.wait();
    const auto lesson=shrine_codex_lesson(c.game);
    if (lesson.virtue<0) { d.message("HOW DID YOU GET HERE?\n"); return d.result; }
    const auto page=std::string("\"")+record(20+lesson.virtue)+"\"\n\n"; d.message(page.c_str());
    d.emit(GameEventKind::PartyChanged); d.wait();
    if (lesson.ceremony) {
        for (unsigned i=0;i<3;++i) { d.emit(GameEventKind::Quake); d.sound("quake"); }
        d.message(record(40)); d.wait(); d.message("Thou dost read:\n\n");
        for (int32_t i=41;i<45;++i) { d.message(record(i)); d.wait(); }
    }
    return d.result;
}
} // namespace openu5
