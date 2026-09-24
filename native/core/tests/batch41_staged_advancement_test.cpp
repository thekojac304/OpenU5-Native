// Batch 41: OUTSUBS 0x070b-0x0799 and 0x079c-0x090e through Camp and UiSession.
#include "openu5/commands.h"
#include "openu5/rest.h"
#include "openu5/ui_session.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace openu5;
namespace {
int checks=0, failures=0;
void check(bool yes,const char *name) {
    ++checks;
    if(!yes) ++failures;
    std::printf("%s %s\n",yes?"GREEN":"RED",name);
}
struct Draw { int lo,hi,value; };
struct Snapshot {
    int first=0,second=0,first_hp=0,second_hp=0,draws=0,stat_draws=0,panels=0,chimes=0;
    uint32_t seed=0;
    CommandState::CampAdvance::Phase phase{};
    int slot=0;
};
struct Fixture {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    std::array<uint8_t,65536> tiles{};
    std::array<uint8_t,1024> local_tiles{};
    MapData local{{17,0},local_tiles.data(),local_tiles.size()};
    WorldData world{tiles.data(),tiles.data(),tiles.size(),tiles.size(),&local,1};
    RestServices rest{};
    std::array<UiTextBlock,128> blocks{};
    UiSession ui;
    std::vector<Draw> draws;
    std::vector<Snapshot> waits;
    std::string text;
    int panels=0,chimes=0,party_changes=0;
    int first_chime_second_level=-1,second_chime_second_level=-1;
    CommandStatus last_status=CommandStatus::Success;
    Fixture():ui({blocks.data(),blocks.size()},{this,[](void *p,const UiIntent &i){
        if(i.kind==UiIntentKind::Command) static_cast<Fixture*>(p)->issue(i.command.kind,i.command.hours);
    }}) {
        tiles.fill(5);local_tiles.fill(171);
        game.position.map={0,0};game.position.xy={80,80};
        game.time.hour=12;game.time.minute=55;game.food=80;
        game.party.character_count=4;game.party.party_size=4;game.party.active_character=255;
        for(int i=0;i<4;++i){
            auto &m=game.party.characters[i];
            std::snprintf(m.name,sizeof(m.name),"Member%d",i);
            m.status='G';m.character_class='F';m.level=1;m.max_hp=30;m.current_hp=5;
            m.strength=m.dexterity=m.intelligence=10;
        }
        game.party.characters[0].exp=100;
        game.party.characters[1].exp=6400;game.party.characters[1].status='D';
        game.party.characters[2].exp=200;
        game.party.characters[3].exp=99;
        rest.context=this;
        rest.snap_npcs=[](void*){};
        rest.occupied=[](void*,int32_t,int32_t,int32_t){return false;};
        rest.karma_record=[](void*,int32_t){return "\"Karma record\"";};
    }
    Snapshot snapshot() const {
        const auto &a=game.party.characters;
        int stats=0;
        for(const auto &d:draws)if(d.lo==1&&d.hi==3)++stats;
        return {a[0].level,a[2].level,a[0].current_hp,a[2].current_hp,
                int(draws.size()),stats,panels,chimes,game.rng.get_seed(),
                commands.camp_advance.phase,commands.camp_advance.slot};
    }
    ActionResult issue(CommandKind kind,int hours=0) {
        CommandContext c{game,turn,travel,commands,world};
        c.rest_services=&rest;
        c.events={this,[](void *p,const GameEvent &e){
            auto &f=*static_cast<Fixture*>(p);
            if(e.kind==GameEventKind::Message&&e.text) f.text+=e.text;
            if(e.kind==GameEventKind::Sfx&&e.text&&
               std::strcmp(e.text,"apparition-heal-chime")==0){
                ++f.chimes;
                if(f.chimes==1) f.first_chime_second_level=f.game.party.characters[2].level;
                if(f.chimes==2) f.second_chime_second_level=f.game.party.characters[2].level;
            }
            if(e.kind==GameEventKind::CampStatusRefresh) ++f.panels;
            if(e.kind==GameEventKind::PartyChanged) ++f.party_changes;
            f.ui.consume(e);
            if(e.kind==GameEventKind::CampKeyWait) f.waits.push_back(f.snapshot());
        }};
        c.rng_trace={this,[](void *p,const char*,int32_t lo,int32_t hi,int32_t value){
            static_cast<Fixture*>(p)->draws.push_back({int(lo),int(hi),int(value)});
        }};
        Command cmd{};cmd.kind=kind;cmd.hours=int16_t(hours);
        const auto result=execute_command(c,cmd);
        last_status=result.status;
        return result;
    }
};
UiAction direction(){UiAction a{};a.kind=UiActionKind::Direction;a.direction=Direction::North;return a;}
UiAction character(char16_t c){UiAction a{};a.kind=UiActionKind::Character;a.character=c;return a;}
UiAction cancel(){UiAction a{};a.kind=UiActionKind::Cancel;return a;}
}
int main() {
    int seed=-1;
    for(int s=1;s<=512;++s){
        Fixture f;f.game.rng.seed(s);f.issue(CommandKind::Rest,1);
        if(!f.waits.empty()){
            f.ui.handle_input(direction());
            if(f.waits.size()==2&&f.waits[0].seed!=f.waits[1].seed){seed=s;break;}
        }
    }
    check(seed>0,"apparition seed has two eligible live members");
    if(seed<0)return 1;
    Fixture f;f.game.rng.seed(seed);
    const auto initial=f.issue(CommandKind::Rest,1);
    check(initial.status==CommandStatus::AwaitingResponse&&f.waits.size()==1&&
          f.ui.mode()==UiMode::KeyWait&&f.ui.request()==UiRequestId::CampAdvance,
          "first advancement opens one blocking getkey");
    const auto a=f.waits[0];
    check(a.first==2&&a.second==1&&a.first_hp==60&&a.second_hp<90,
          "member one changed while member two remains at old level and HP");
    check(a.stat_draws==1&&f.draws.back().lo==1&&f.draws.back().hi==3&&
          a.phase==CommandState::CampAdvance::Phase::MemberKey&&a.slot==0,
          "first stat draw and continuation belong to member one");
    check(a.panels==0&&a.chimes==1&&f.first_chime_second_level==1,
          "first message holds before its panel draw and next member cue");
    check(f.text.find("Hail, Member0!")!=std::string::npos&&
          f.text.find("Hail, Member2!")==std::string::npos&&
          f.text.find("Karma record")==std::string::npos,
          "first cue contains only the first member");
    const auto seed_at_first=a.seed;
    const auto blocked=f.issue(CommandKind::Pass);
    check(blocked.status==CommandStatus::AwaitingResponse&&
          f.game.rng.get_seed()==seed_at_first&&f.waits.size()==1,
          "world commands cannot run during the getkey");
    f.ui.handle_input(direction());
    check(f.waits.size()==2&&f.ui.mode()==UiMode::KeyWait&&
          f.last_status==CommandStatus::AwaitingResponse,
          "one directional key reaches only the next member wait");
    const auto b=f.waits[1];
    check(b.first==2&&b.second==3&&b.first_hp==60&&b.second_hp==90,
          "second member mutates only after first acknowledgement");
    OriginalRng expected(a.seed);
    expected.next(1,3);
    check(b.stat_draws==2&&b.draws==a.draws+1&&b.seed==expected.get_seed()&&b.slot==2&&
          f.second_chime_second_level==1,
          "second stat draw is deferred until second member's cue");
    check(b.panels==2&&b.chimes==2,
          "first member and dead slot redraw before second member's message");
    check(f.text.find("Hail, Member2!")!=std::string::npos&&
          f.text.find("Hail, Member1!")==std::string::npos,
          "second cue follows roster order and dead member has no Hail");
    f.ui.handle_input(character(u'x'));
    check(f.waits.size()==3&&f.ui.mode()==UiMode::KeyWait&&
          f.waits[2].phase==CommandState::CampAdvance::Phase::KarmaKey,
          "second key reaches the separate karma speech wait");
    check(f.waits[2].panels==4&&f.waits[2].stat_draws==2&&
          f.waits[2].draws==b.draws&&
          f.game.party.characters[1].level==1&&f.game.party.characters[3].level==1,
          "all four slots redraw; dead and ineligible slots consume no stat draw");
    check(f.text.find("Karma record")!=std::string::npos&&
          f.text.find("vanishes")==std::string::npos&&f.party_changes==0,
          "karma speech holds before the apparition vanishes");
    f.ui.handle_input(cancel());
    check(f.last_status==CommandStatus::Success&&f.ui.mode()==UiMode::Exploration&&
          f.commands.camp_advance.phase==CommandState::CampAdvance::Phase::None,
          "Escape is a getkey acknowledgement and completes Camp");
    check(f.game.rng.get_seed()==b.seed&&f.party_changes==1&&
          f.text.find("vanishes")!=std::string::npos&&
          f.text.find("Party rested!")!=std::string::npos,
          "final key returns to Camp completion without another RNG draw");
    check(f.game.party.characters[0].exp==100&&f.game.party.characters[2].exp==200,
          "XP survives staged presentation");
    std::printf("Batch 41 staged advancement: %d/%d checks\n",checks-failures,checks);
    return failures?1:0;
}
