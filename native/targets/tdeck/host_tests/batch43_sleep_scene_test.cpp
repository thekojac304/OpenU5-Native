#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "openu5/rest.h"
#include <cstdio>
#include <memory>
using namespace openu5;
void batch37_reset_screen();
size_t batch37_frame_count();
uint16_t batch37_frame_pixel(size_t,int,int);
uint16_t batch37_pixel(int,int);
int batch37_panel_draw_count();
uint16_t batch37_panel_value(size_t,int);
int batch37_ui_draw_count();
int batch37_world_draw_count();
namespace {
int checks=0,failures=0;
void check(bool good,const char *label) {
    ++checks;if(!good)++failures;
    std::printf("%s %s\n",good?"GREEN":"RED",label);
}
const tdeck::AlphaResourceOwners *pack=nullptr;
struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt=std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    int64_t us=0;
    explicit Run(int seed) {
        batch37_reset_screen();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world=pack->world;f.npc_locations=pack->npc_locations;f.pack=pack;
        f.location_x=pack->location_x;f.location_y=pack->location_y;
        f.location_count=pack->location_count;
        f.render_pixels=true;f.indexed_test_tiles=true;
        rt->attach_host_test_fixture(f);
        auto &g=rt->game();
        g.position.map={0,0};g.position.xy={80,80};
        g.time.hour=12;g.time.minute=55;g.food=80;
        g.party.character_count=g.party.party_size=4;
        g.party.active_character=255;
        for(int i=0;i<4;++i) {
            auto &m=g.party.characters[i];
            std::snprintf(m.name,sizeof(m.name),"Member%d",i);
            m.status='G';m.character_class=i?'M':'F';m.level=1;
            m.current_hp=5;m.max_hp=30;
            m.strength=m.dexterity=m.intelligence=10;
        }
        g.party.characters[0].exp=100;
        g.party.characters[1].exp=200;
        g.party.characters[2].status='D';
        g.party.characters[3].exp=0;
        g.rng.seed(seed);
        rt->render(board,true);
    }
    void key(uint8_t code) {
        tdeck::RawInputEvent e{};
        e.kind=tdeck::RawInputKind::Keyboard;
        e.transition=tdeck::KeyTransition::Pressed;
        e.code=code;e.timestamp_us=(us+=100000);
        rt->handle(e);
    }
    void camp(){key('h');key('1');key('\r');key('n');}
    void watched(){key('h');key('1');key('\r');key('y');key('2');}
    bool waiting()const{return rt->ui()->mode()==UiMode::KeyWait&&
        rt->ui()->request()==UiRequestId::CampAdvance;}
};
uint16_t px(size_t frame,int col,int row) {
    return batch37_frame_pixel(frame,col*16+8,row*16+8);
}
}
int main(int argc,char **argv) {
    if(argc<2)return 2;
    tdeck::AlphaResourcePack source;tdeck::AlphaResourceReport report{};
    if(source.open(argv[1],report)!=ESP_OK)return 2;
    static tdeck::AlphaResourceOwners owners{};
    if(source.load(owners,report)!=ESP_OK)return 2;
    pack=&owners;
    const auto &arena=*pack->combat_map_views[0];
    const auto first=arena.starts[int(CombatDirection::South)][0];
    const auto second=arena.starts[int(CombatDirection::South)][1];
    const auto dead=arena.starts[int(CombatDirection::South)][2];
    int hit=-1,miss=-1;
    for(int s=1;s<=512&&(hit<0||miss<0);++s) {
        Run candidate(s);candidate.camp();
        if(candidate.waiting())hit=s;
        else if(candidate.rt->ui()->mode()==UiMode::Exploration &&
                candidate.rt->game().time.hour==13)miss=s;
    }
    check(hit>0&&miss>0,"deterministic hit and missed-gate Camp seeds");
    if(hit<0||miss<0)return 1;
    Run successful(hit);successful.camp();
    auto &g=successful.rt->game();
    check(successful.waiting()&&g.party.characters[0].level==2&&
          g.party.characters[1].level==1,"first Hail still stages advancement");
    check(batch37_frame_count()>=6,"sleep scene precedes apparition and standing/XOR frames");
    check(batch37_ui_draw_count()==1,"sleep and apparition frames leave the text window untouched");
    if(batch37_frame_count()>=2) {
        check(px(1,first.x,first.y)==0xeeee&&px(1,second.x,second.y)==0xeeee,
              "pre-apparition CampFire shows sleeping living members");
        check(px(1,dead.x,dead.y)!=0xeeee&&
              arena.tiles[5*11+5]==179&&px(1,5,5)!=0x4444,
              "sleep scene has empty dead cell and original fire tile");
        check(px(2,5,5)==0x4444&&px(2,first.x,first.y)==0xeeee,
              "apparition materializes over fire before first sleeper wakes");
    }
    check(batch37_panel_draw_count()>=2&&batch37_pixel(200,4)=='S'&&
          batch37_pixel(200,12)=='S',"sleep scene presents S in the party panel");
    check(batch37_panel_value(0,0)=='S'&&batch37_panel_value(0,1)=='S'&&
          batch37_panel_value(0,2)=='D'&&batch37_panel_value(1,6)==13*60,
          "status shows sleeping roster and advances clock before apparition");
    check(g.position.xy.x==80&&g.position.xy.y==80,
          "scene does not move the world party");
    const auto hit_rng=g.rng.get_seed();
    Run missed(miss);missed.camp();
    const auto &m=missed.rt->game();
    check(!missed.waiting()&&m.time.hour==13&&m.party.characters[0].level==1,
          "missed gate completes Camp without advancement");
    check(batch37_frame_count()>=3&&
          px(1,first.x,first.y)==0xeeee&&
          px(1,second.x,second.y)==0xeeee&&
          arena.tiles[5*11+5]==179&&px(1,5,5)!=0x4444,
          "missed gate still displays the pre-apparition sleep scene");
    check(m.party.characters[0].status=='G'&&m.party.characters[1].status=='G'&&
          m.position.xy.x==80&&m.position.xy.y==80,
          "missed gate restores roster and world position");
    check(batch37_frame_count()>=3&&
          batch37_frame_pixel(batch37_frame_count()-1,88,88)==
          batch37_frame_pixel(0,88,88),
          "scene exit restores the ordinary world viewport");
    check(hit_rng==20&&m.rng.get_seed()==66,
          "Camp and gate RNG order matches unchanged Batch 42");
    Run watched(hit);watched.watched();
    const auto guard=arena.starts[int(CombatDirection::South)][1];
    check(batch37_frame_count()>=2&&
          px(1,first.x,first.y)==0xeeee&&px(1,guard.x,guard.y)==0x0000,
          "posted Mage guard stands beside sleepers in the first scene frame");
    check(batch37_panel_value(0,0)=='S'&&batch37_panel_value(0,1)=='G',
          "guard stays G while other live members sleep");
    bool guard_moved=false;
    for(int seed=1;seed<=128&&!guard_moved;++seed){
        Run moving(seed);moving.watched();
        guard_moved=batch37_frame_count()>=4&&
            px(1,guard.x,guard.y)==0x0000&&
            px(2,guard.x,guard.y)!=0x0000&&px(2,5,5)!=0x4444;
    }
    check(guard_moved,"posted guard movement repaints the CampFire sleep scene");
    Run ineligible(hit);
    ineligible.rt->game().party.characters[0].exp=0;
    ineligible.rt->game().party.characters[1].exp=0;
    ineligible.camp();
    check(ineligible.waiting()&&
          ineligible.rt->commands().camp_advance.phase==
              CommandState::CampAdvance::Phase::KarmaKey&&
          batch37_frame_count()>=3&&px(1,5,5)!=0x4444&&
          px(2,5,5)==0x4444,
          "successful gate mounts sleep and apparition scenes without a level gain");
    Run poisoned(miss);poisoned.rt->game().party.characters[1].status='P';
    poisoned.camp();
    check(batch37_panel_value(0,1)=='P'&&
          px(1,second.x,second.y)==0x0000&&
          poisoned.rt->game().party.characters[1].status=='P',
          "poisoned Mage remains standing and P through a missed gate");
    std::printf("RNG hit=%u miss=%u\n",unsigned(hit_rng),unsigned(m.rng.get_seed()));
    std::printf("Batch 43 sleep scene: %d/%d checks\n",checks-failures,checks);
    return failures?1:0;
}
