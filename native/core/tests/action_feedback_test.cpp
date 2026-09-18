#include "openu5/display_names.h"
#include "openu5/world_commands.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace openu5;

namespace {
struct Seen {
    GameEventKind kind{};
    std::string text;
    int note=0;
};
void check(bool value,const char *message) {
    if(!value){std::cerr<<"action feedback regression: "<<message<<"\n";std::exit(1);}
}
void capture(void *context,const GameEvent &event) {
    static_cast<std::vector<Seen> *>(context)->push_back(
        {event.kind,event.text?event.text:"",event.note});
}
bool has_text(const std::vector<Seen> &events,const std::string &text) {
    for(const auto &event:events)if(event.kind==GameEventKind::Message&&event.text==text)return true;
    return false;
}
bool has_event(const std::vector<Seen> &events,GameEventKind kind,int note=-1) {
    for(const auto &event:events)if(event.kind==kind&&(note<0||event.note==note))return true;
    return false;
}
}

int main() {
    GameState game{};TurnState turn{};TravelState travel{};CommandState commands{};
    std::vector<uint8_t> tiles(32*32,5);MapData map_data{{1,0},tiles.data(),tiles.size()};
    WorldData world{};world.small_maps=&map_data;world.small_map_count=1;
    game.position={{4,4},{1,0}};game.party.character_count=game.party.party_size=1;
    auto &member=game.party.characters[0];member.party_status=0;member.status='G';
    member.current_hp=50;member.max_hp=100;member.current_mp=50;member.level=8;
    member.intelligence=30;
    CommandContext context{game,turn,travel,commands,world};
    const auto active=get_active_map(world,game.position.map);check(active.error==Error::None,"test map loads");
    std::vector<Seen> events;EventSink sink{&events,capture};

    game.scroll_quantities[0]=1;Command scroll{};scroll.kind=CommandKind::UseItem;scroll.item=0;
    world_magic(context,scroll,active.value,sink,rng_source(game.rng));
    check(game.scroll_quantities[0]==0,"scroll quantity decrements");
    check(has_text(events,std::string("Used ")+scroll_display_name(0)+"."),"scroll names the used item");
    check(has_text(events,"Light!"),"scroll reports its result");
    check(has_event(events,GameEventKind::Sfx)&&has_event(events,GameEventKind::MagicCeremony,0),
          "scroll emits semantic audio and ceremony events");

    events.clear();game.potion_quantities[0]=1;Command potion{};potion.kind=CommandKind::UseItem;potion.item=8;potion.member=0;
    world_magic(context,potion,active.value,sink,rng_source(game.rng));
    check(game.potion_quantities[0]==0,"potion quantity decrements");
    check(has_text(events,std::string("Used ")+potion_display_name(0)+"."),"potion names the used item");
    check(has_event(events,GameEventKind::Sfx)&&has_event(events,GameEventKind::MagicCeremony,0),
          "potion emits semantic audio and ceremony events");
    size_t potion_messages=0;for(const auto &event:events)if(event.kind==GameEventKind::Message)++potion_messages;
    check(potion_messages>=2,"potion reports a meaningful result");

    events.clear();game.spell_quantities[0]=1;Command spell{};spell.kind=CommandKind::Cast;spell.item=0;spell.caster=0;
    world_magic(context,spell,active.value,sink,rng_source(game.rng));
    check(game.spell_quantities[0]==0,"spell quantity decrements");
    check(has_event(events,GameEventKind::Sfx)&&has_event(events,GameEventKind::MagicCeremony,1),
          "ordinary spell emits circle-indexed semantic feedback");
    std::cout<<"scroll, potion, and spell feedback passed\n";
}
