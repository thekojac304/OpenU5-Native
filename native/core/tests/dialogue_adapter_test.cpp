#include "openu5/dialogue_orchestration.h"
#include <cstdlib>
#include <iostream>
using namespace openu5;
static int checks=0;
static void check(bool ok){++checks;if(!ok){std::cerr<<"dialogue adapter "<<checks<<" failed\n";std::exit(1);}}
int main(){
    TalkScript records[2];records[0].npc_index=records[1].npc_index=1;
    TalkCatalog catalog;for(auto &v:catalog.masters)v={records,2};TalkRegistry registry{&catalog,talk_catalog_lookup};
    check(talk_script_for(registry,1,1)==&records[1]&&talk_script_for(registry,32,1)==&records[1]);
    check(!talk_script_for(registry,0,1)&&!talk_script_for(registry,33,1)&&!talk_script_for(registry,1,0)&&!talk_script_for(registry,1,128)&&!talk_script_for(registry,1,2));
    check(talk_master(8)==TalkMaster::Towne&&talk_master(9)==TalkMaster::Dwelling&&talk_master(17)==TalkMaster::Castle&&talk_master(25)==TalkMaster::Keep);
    TalkItem ask{TalkOp::AskName},jump{TalkOp::Label,{},0},yes{TalkOp::KarmaPlusOne},no{TalkOp::KarmaMinusOne};
    TalkLine yesline{&yes,1},noline{&no,1};TalkText y=u"y",n=u"n";TalkQA qa[2]={{{&y,1},{&yesline,1}},{{&n,1},{&noline,1}}};
    TalkLine def{};TalkLabel label{0,{}, {&def,1},{qa,2}};records[1].name={&ask,1};records[1].job={&jump,1};records[1].labels={&label,1};
    Conversation bare;bare.bind(records[1],{});check(bare.input(u"name").empty());
    GameState g;g.position={{10,10},{1,0}};g.karma=50;g.party.character_count=1;g.party.party_size=1;g.party.characters[0].party_status=0; // effective Avatar fallback
    TurnState turn;TravelState travel;CommandState commands;WorldData world;CommandContext c{g,turn,travel,commands,world};
    NpcActors actors;actors.count=1;actors.actors[0].location=1;actors.actors[0].x=11;actors.actors[0].y=10;actors.actors[0].schedule.slot=1;actors.actors[0].schedule.dialog=1;c.actors=&actors;
    DialogueSession session;DialogueServices services{session};services.registry=registry;c.dialogue_services=&services;
    Command cmd;cmd.kind=CommandKind::Talk;check(execute_command(c,cmd).status==CommandStatus::InvalidContext);
    cmd.has_direction=true;cmd.direction=Direction::East;g.rng.seed(1);auto r=dispatch_world_command(c,cmd);check(session.active&&r.status==CommandStatus::AwaitingResponse&&r.turns==0&&r.world_turns==0);
    Command input;input.kind=CommandKind::DialogueText;input.text=u"name";input.text_length=4;execute_command(c,input);
    check(session.conversation.pending()==DialoguePending::Name);
    input.text=u"Avat";input.text_length=4;execute_command(c,input);check(g.npc_met[0]==2&&session.conversation.met_avatar());
    input.text=u"job";input.text_length=3;execute_command(c,input);check(session.conversation.pending()==DialoguePending::Label&&session.conversation.active_label()==0);
    Command response;response.kind=CommandKind::DialogueYes;execute_command(c,response);check(g.karma==51&&session.conversation.pending()==DialoguePending::Interest);
    execute_command(c,input);response.kind=CommandKind::DialogueNo;execute_command(c,response);check(g.karma==50);
    const auto seed=g.rng.get_seed();Command pass;pass.kind=CommandKind::Pass;check(execute_command(c,pass).status==CommandStatus::AwaitingResponse&&g.rng.get_seed()==seed&&g.turns_since_start==0);
    Command end;end.kind=CommandKind::EndConversation;r=execute_command(c,end);check(!session.active&&r.status==CommandStatus::Unsupported&&session.deferred==DialogueHandoff::QuestEnd);
    check(execute_command(c,end).status==CommandStatus::NoOp&&execute_command(c,response).status==CommandStatus::InvalidContext);
    services.handoff=[](void *,DialogueHandoff,const NpcActor &,EventSink){return true;};execute_command(c,cmd);check(g.rng.get_seed()==seed);check(execute_command(c,end).status==CommandStatus::Success);
    actors.actors[0].schedule.dialog=0x81;services.handoff=nullptr;r=execute_command(c,cmd);check(r.status==CommandStatus::Unsupported&&!session.active&&session.deferred==DialogueHandoff::Shop&&g.rng.get_seed()==seed);
    g.position.map.location=18;actors.actors[0].location=18;actors.actors[0].schedule.dialog=255;r=execute_command(c,cmd);check(r.status==CommandStatus::Unsupported&&session.deferred==DialogueHandoff::Guard);
    g.position.map.location=0;r=execute_command(c,cmd);check(r.status==CommandStatus::Rejected&&r.event_count==1);
    c.combat=true;check(execute_command(c,cmd).status==CommandStatus::InvalidContext);
    g.equipment_count=48;g.equipment_quantities[63]=0;apply_dialogue_effect(g,{DialogueEffectKind::GiveItem,63},u"");check(g.equipment_quantities[63]==0);
    g.party.characters[0].helmet=63;unequip_slot(g,0,EquipSlot::Helmet);apply_dialogue_effect(g,{DialogueEffectKind::GiveItem,63},u"");check(g.equipment_count==64&&g.equipment_quantities[63]==2);
    std::cout<<checks<<" dialogue adapter checks passed\n";
}
