#include "openu5/transport.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace openu5;
struct Hash { uint32_t h=2166136261u; void n(int64_t v){for(int i=0;i<4;i++)h=(h^((uint32_t(v)>>(i*8))&255))*16777619u;} void s(const std::string &v){n(v.size());for(unsigned char c:v)h=(h^c)*16777619u;} };
struct Events { struct Beat {std::string text;int pause;bool append;};struct E {GameEventKind kind;std::string text;std::vector<int> slots;std::vector<Beat> beats;int toll=0;};std::vector<E> values; };
int main(int argc,char **argv){
 if(argc!=2)return 2; std::ifstream in(argv[1]);int from,terrain,v,count=0;
 const char *names[]={"message","moved","map-changed","party-changed","town-exit-prompt","walk-echo","sfx","poison-tick","quake","needs-direction"};
 while(in>>from>>terrain>>v){
  GameState g;TurnState t;TravelState travel;CommandState commands;bool town=(v&1)!=0;
  std::vector<uint8_t> tiles(town?1024:65536,uint8_t(terrain));MapData small{{2,0},tiles.data(),1024};WorldData world;world.overworld=tiles.data();world.overworld_size=tiles.size();world.small_maps=&small;world.small_map_count=1;
  CommandContext c{g,t,travel,commands,world};g.rng.seed(uint32_t(from*8191+terrain*31+v));g.position={{uint8_t(v&2?(town?31:255):10),10},{uint8_t(town?2:0),0}};g.time={139,1,1,5,59};g.party.party_size=g.party.character_count=1;g.party.active_character=255;auto &ch=g.party.characters[0];ch.status='G';ch.current_hp=ch.max_hp=100;ch.ring=255;ch.party_status=0;ch.strength=ch.dexterity=20;g.food=100;g.ship_hull=v&2?1:99;g.ship_skiffs=v%3;g.magic_carpets=v%2;g.transport=transport_mode(from);t.transport_tile=from;t.prev_hour=5;t.time_spell=v&4?'Q':0;t.spell_turns=255;t.wind=v%5;g.hms_cape=(v&4)!=0;
  g.gold=v&2?500:0;
  Events events;c.events={&events,[](void *p,const GameEvent &e){Events::E row{e.kind,e.text?e.text:"",{},{},e.note};for(uint8_t i=0;i<e.slot_count;++i)row.slots.push_back(e.slots[i]);if(e.troll_sneak)for(uint8_t i=0;i<e.troll_sneak->count;++i){auto &b=e.troll_sneak->beats[i];row.beats.push_back({b.text,b.pause_units,b.append});}static_cast<Events*>(p)->values.push_back(row);}};
  for(int a=0;a<4;a++){
   events.values.clear();commands.awaiting_exit=false;execute_command(c,{CommandKind::Move,Direction(a)});if(commands.awaiting_troll){Command toll{CommandKind::TrollToll};toll.member=v&2?1:0;execute_command(c,toll);}Hash h;
   for(int64_t n:{int64_t(g.rng.get_seed()),int64_t(g.position.xy.x),int64_t(g.position.xy.y),int64_t(t.transport_tile),int64_t(g.transport),int64_t(g.ship_hull),int64_t(g.ship_skiffs),int64_t(g.magic_carpets),g.turns_since_start,int64_t(g.time.hour),int64_t(g.time.minute),int64_t(t.prev_hour),int64_t(g.food),int64_t(t.wind),int64_t(t.wind_drift_counter),int64_t(t.sail_dir),int64_t(t.hms_cape_toggle),int64_t(commands.outdoor_phases.mount),int64_t(commands.outdoor_phases.quickness),int64_t(commands.town_phases.mount),int64_t(commands.town_phases.quickness),int64_t(ch.current_hp),int64_t(ch.status)})h.n(n);
   h.n(g.gold);
   for(const auto &e:events.values){h.s(e.kind==GameEventKind::TrollSneak?"troll-sneak":e.kind==GameEventKind::TrollTollPrompt?"troll-toll-prompt":names[int(e.kind)]);h.s(e.text);for(auto i:e.slots)h.n(i);if(e.kind==GameEventKind::TrollTollPrompt)h.n(e.toll);for(const auto &b:e.beats){h.s(b.text);h.n(b.pause);h.n(b.append);}}uint32_t expected;in>>expected;
   if(h.h!=expected){std::cerr<<"movement mismatch transport="<<from<<" terrain="<<terrain<<" variant="<<v<<" action="<<a<<" actual="<<h.h<<" expected="<<expected<<"\n";for(const auto &e:events.values)std::cerr<<int(e.kind)<<":"<<e.text<<"\n";return 1;}
  }++count;
 }
 std::cout<<count<<" movement sequences passed\n";return count?0:2;
}
