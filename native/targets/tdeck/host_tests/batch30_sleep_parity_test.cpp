// Batch 30: shipped-pack, real AlphaRuntime and raw (H)ole-up keys.
// Oracles: CMDS.OVL 0x0647 -> 0x0664 -> 0x0671 -> 0x0677 -> 0x0688;
// kernel 0x2AE8 and TOWN.OVL 0x0170, not the TypeScript bedSleepStep.
#include "../main/alpha_runtime.h"
#include "openu5/rest.h"
#include "openu5/world_terrain.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

using namespace openu5;
namespace {
const tdeck::AlphaResourceOwners *owners;
int checks=0, failures=0;
void check(bool ok,const char *id,const char *why){++checks;if(!ok)++failures;std::printf("%s %s %s\n",ok?"GREEN":"RED",id,why);}
struct Harness {
    std::unique_ptr<tdeck::AlphaRuntime> rt=std::make_unique<tdeck::AlphaRuntime>();
    int64_t us=0; size_t mark=0;
    RestServices original{}, observer{};
    int watch_x=-1,watch_y=-1;
    bool insert_stale_after_first=false;
    struct Seen {int hour,minute,tile,turns;};
    std::vector<Seen> seen;
    Harness(int hour,int minute) {
        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world=owners->world;hf.location_x=owners->location_x;hf.location_y=owners->location_y;
        hf.location_count=owners->location_count;hf.npc_locations=owners->npc_locations;hf.pack=owners;
        rt->attach_host_test_fixture(hf);
        auto &g=rt->game();g.party.character_count=g.party.party_size=2;
        for(int i=0;i<2;++i){auto &m=g.party.characters[i];std::snprintf(m.name,sizeof(m.name),"Sleeper%d",i);
            m.status='G';m.current_hp=500;m.max_hp=900;m.dexterity=30;m.strength=30;}
        g.party.active_character=255;g.time.hour=hour;g.time.minute=minute;g.food=50;
        g.position.map={0,0};g.position.xy={owners->location_x[16],owners->location_y[16]};
        key('e');
    }
    GameState &g(){return rt->game();}
    CommandContext &c(){return rt->command_context_for_test();}
    void key(uint8_t code){tdeck::RawInputEvent e{};e.kind=tdeck::RawInputKind::Keyboard;
        e.code=code;e.transition=tdeck::KeyTransition::Pressed;e.timestamp_us=(us+=100000);rt->handle(e);}
    bool saw(const char *s){auto &u=*rt->ui();for(size_t i=mark;i<u.transcript_size();++i)
        if(auto *b=u.transcript_at(i);b&&std::strstr(b->text,s))return true;return false;}
    void bed(int x,int y){g().position.xy={uint8_t(x),uint8_t(y)};}
    void watch(int x,int y,bool stale=false){watch_x=x;watch_y=y;insert_stale_after_first=stale;
        original=*c().rest_services;observer=original;observer.context=this;
        observer.snap_npcs=[](void *p){auto &h=*static_cast<Harness*>(p);
            h.original.snap_npcs(h.original.context);
            h.seen.push_back({h.g().time.hour,h.g().time.minute,h.tile(h.watch_x,h.watch_y),int(h.g().turns_since_start)});
            if(h.insert_stale_after_first&&h.seen.size()==1){h.c().terrain->hourly.clear();
                h.c().terrain->hourly.push_back({{17,0},h.watch_x,h.watch_y,0x7e});}
        };observer.occupied=[](void *p,int x,int y,int z){auto &h=*static_cast<Harness*>(p);
            return h.original.occupied(h.original.context,x,y,z);};c().rest_services=&observer;}
    void hole(int hours){mark=rt->ui()->transcript_size();key('h');key(uint8_t('0'+hours));key('\r');
        if(watch_x>=0)c().rest_services=&original;}
    int tile(int x,int y){return c().terrain->effective(owners->world,{17,0},x,y);}
    // Use the real map and actual occupancy service. Snap to the upcoming hour
    // during setup so the chosen bed stays free when the loop re-snaps NPCs.
    bool free_bed_at(int hour){auto &g0=g();auto saved=g0.time.hour;auto *s=c().rest_services;
        g0.time.hour=saved;s->snap_npcs(s->context);
        for(int y=0;y<32;++y)for(int x=0;x<31;++x)
            if(c().terrain->raw(owners->world,{17,0},x,y)==0xab &&
               !s->occupied(s->context,x,y,0)){
                g0.time.hour=hour;s->snap_npcs(s->context);
                const bool free_after=!s->occupied(s->context,x,y,0);
                if(free_after){bed(x,y);g0.time.hour=saved;s->snap_npcs(s->context);return true;}
                g0.time.hour=saved;s->snap_npcs(s->context);
            }
        g0.time.hour=saved;return false;}
};
bool lamp_cell(Harness &h,int &x,int &y){for(y=0;y<31;++y)for(x=0;x<32;++x)
    if(h.c().terrain->raw(owners->world,{17,0},x,y)==135){++y;return true;}return false;}

void housekeeping_full(){Harness h(12,0);check(h.g().position.map.location==17&&h.free_bed_at(13),"H156-A0","castle and free shipped bed");
    h.g().party.characters[0].status='P';h.g().food=9;auto start=h.g().turns_since_start;
    h.hole(1);check(h.saw("Zzzzzzz...")&&h.g().time.hour==13&&h.g().time.minute==0,"H156-A1","six 10-minute ticks");
    check(h.g().turns_since_start==start+6,"H156-A2","exactly six housekeeping turns, no extra begin/end tick");
    check(h.g().party.characters[0].current_hp==494,"H156-A3","poison damages once per sleep tick");
    check(h.g().food==9,"H156-A4","no meal outside 06/12/18");}
void timed_expiry(){Harness h(12,0);check(h.free_bed_at(13),"H156-B0","free bed through Q expiry");
    h.g().party.characters[0].status='P';h.g().food=5;h.c().turn.time_spell='Q';h.c().turn.spell_turns=2;
    auto start=h.g().turns_since_start;h.hole(1);
    check(h.g().time.hour==13&&h.g().time.minute==0&&h.g().turns_since_start==start+7,"H156-B1","Q halves first two clock advances; sleep continues to target hour");
    check(h.c().turn.time_spell==0&&h.c().turn.spell_turns==0&&h.g().party.characters[0].current_hp==493,"H156-B2","Q expires after two housekeeping calls; poison continues for all seven");
    check(h.g().food==5,"H156-B3","no meal at the 13:00 hour change");}
void meal(){Harness h(5,50);check(h.free_bed_at(6),"H156-M0","free bed at six");
    h.g().party.characters[0].status='P';h.g().food=5;h.hole(1);
    check(h.g().time.hour==6&&h.g().food==4,"H156-M1","06:00 meal consumes only the poisoned awake eater");}
void ejection(){Harness h(22,50);h.bed(12,10);h.g().party.characters[0].status='P';
    auto start=h.g().turns_since_start;h.hole(2);
    check(h.saw("Thrown out of bed!")&&h.g().time.hour==23&&h.g().time.minute==0,"H156-C1","owner ejects at 23:00");
    check(h.g().turns_since_start==start+1&&h.g().party.characters[0].current_hp==499,"H156-C2","ejecting tick already ran housekeeping, with no later tick");}
void starving(){Harness h(12,50);check(h.free_bed_at(13),"H156-D0","free bed at thirteen");
    h.g().food=0;h.hole(1);check(h.saw("Starving!")&&h.g().time.hour==13,"H156-D1","hour crossing emits starvation through production event route");}
void boundary(int before,int after,const char *id){Harness h(before,50);int x=0,y=0;
    check(lamp_cell(h,x,y)&&h.free_bed_at(after),id,"shipped castle lamp and free bed");
    const int old=h.tile(x,y),raw=h.c().terrain->raw(owners->world,{17,0},x,y);
    h.watch(x,y);h.hole(1);const int expected=after==20?(raw^221):raw;
    check(h.g().time.hour==after&&h.g().time.minute==0&&h.seen.size()==1&&h.seen[0].hour==after&&h.seen[0].minute==0,
          id,"target-hour tick crosses the requested hour");
    check(old==(before==19?raw:(raw^221))&&h.seen.size()==1&&h.seen[0].tile==expected,
          id,"scheduled lamp-adjacent tile is changed inside the boundary tick, before NPC snap");}
void stable(){Harness h(12,0);check(h.free_bed_at(13),"H157-N0","free bed with no 05:00/20:00 crossing");
    int x=0,y=0;check(lamp_cell(h,x,y),"H157-N1","shipped lamp exists");
    // Mark the hourly layer after tick one; only an unwanted per-tick refresh
    // could erase it before the remaining five NPC snaps.
    h.watch(x,y,true);h.hole(1);
    bool same=h.seen.size()==6;for(size_t i=1;i<h.seen.size();++i)same=same&&h.seen[i].tile==0x7e;
    check(h.g().time.hour==13&&same,"H157-N2","no unconditional refresh on ordinary sleep ticks");}
}
int main(int argc,char **argv){if(argc<2)return 2;tdeck::AlphaResourcePack pack;tdeck::AlphaResourceReport report{};
    if(pack.open(argv[1],report)!=ESP_OK)return 2;static tdeck::AlphaResourceOwners o{};
    if(pack.load(o,report)!=ESP_OK)return 2;owners=&o;
    housekeeping_full();timed_expiry();meal();ejection();starving();boundary(19,20,"H157-20");boundary(4,5,"H157-05");stable();
    std::printf("batch30_sleep_parity: %d/%d GREEN, %d RED\n",checks-failures,checks,failures);return failures?1:0;}
