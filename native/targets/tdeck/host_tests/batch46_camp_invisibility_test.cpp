// H-179: shipped ULTIMA.EXE 0x6794/0x68ae and OUTSUBS 0x0850-0x0874.
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "openu5/rest.h"
#include <cstdio>
#include <memory>
#include <vector>
using namespace openu5;
void batch37_reset_screen();
size_t batch37_frame_count();
uint16_t batch37_frame_pixel(size_t,int,int);
uint16_t batch37_panel_value(size_t,int);
namespace {
const tdeck::AlphaResourceOwners *pack;
int checks=0,failures=0;
void check(bool ok,const char *why){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"GREEN":"RED",why);}
struct Roll {int lo,hi,value;char status;};
struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt=std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    std::vector<Roll> rolls;
    int64_t us=0;
    explicit Run(int seed){
        batch37_reset_screen();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world=pack->world;f.npc_locations=pack->npc_locations;f.pack=pack;
        f.location_x=pack->location_x;f.location_y=pack->location_y;
        f.location_count=pack->location_count;f.render_pixels=true;f.indexed_test_tiles=true;
        rt->attach_host_test_fixture(f);
        auto &g=rt->game();g.position.map={0,0};g.position.xy={80,80};
        g.time.hour=12;g.time.minute=55;g.food=80;
        g.party.character_count=g.party.party_size=4;g.party.active_character=255;
        for(int i=0;i<4;++i){auto &m=g.party.characters[i];
            std::snprintf(m.name,sizeof(m.name),"Member%d",i);
            m.status='G';m.character_class=i?'M':'F';m.level=1;
            m.current_hp=m.max_hp=30;m.strength=m.dexterity=m.intelligence=10;
            m.ring=255;
        }
        g.party.characters[2].status='D';g.party.characters[3].status='P';
        g.rng.seed(seed);
        rt->command_context_for_test().rng_trace={this,[](void *p,const char *,int32_t lo,int32_t hi,int32_t value){
            auto &r=*static_cast<Run*>(p);
            r.rolls.push_back({int(lo),int(hi),int(value),r.rt->game().party.characters[0].status});
        }};
        rt->render(board,true);
    }
    void key(uint8_t code){tdeck::RawInputEvent e{};e.kind=tdeck::RawInputKind::Keyboard;
        e.transition=tdeck::KeyTransition::Pressed;e.code=code;e.timestamp_us=(us+=100000);rt->handle(e);}
    void camp(bool watch){key('h');key('1');key('\r');key(watch?'y':'n');if(watch)key('2');}
    bool waiting()const{return rt->ui()->mode()==UiMode::KeyWait&&rt->ui()->request()==UiRequestId::CampAdvance;}
    GameState &g(){return rt->game();}
};
uint16_t px(size_t frame,int col,int row){return batch37_frame_pixel(frame,col*16+8,row*16+8);}
bool oracle(const Run &r,int seed){OriginalRng o(seed);
    for(const auto &d:r.rolls)if(d.value!=o.next(d.lo,d.hi).value)return false;
    return r.rt->game().rng.get_seed()==o.get_seed();}
}
int main(int argc,char **argv){
    if(argc<2)return 2;
    tdeck::AlphaResourcePack source;tdeck::AlphaResourceReport report{};
    if(source.open(argv[1],report)!=ESP_OK)return 2;
    static tdeck::AlphaResourceOwners owners{};
    if(source.load(owners,report)!=ESP_OK)return 2;pack=&owners;
    const auto &arena=*pack->combat_map_views[0];
    const auto a=arena.starts[int(CombatDirection::South)][0];
    const auto b=arena.starts[int(CombatDirection::South)][1];
    const auto d=arena.starts[int(CombatDirection::South)][2];
    const auto p=arena.starts[int(CombatDirection::South)][3];
    Run control(1);control.camp(true);
    check(batch37_frame_count()>=2&&px(1,a.x,a.y)==0xeeee&&px(1,b.x,b.y)==0x0000&&
          px(1,p.x,p.y)==0x0000&&px(1,d.x,d.y)!=0xdddd,
          "control: sleeper, ordinary guard, poisoned actor and dead background");
    check(batch37_panel_value(0,0)=='S'&&batch37_panel_value(0,1)=='G'&&
          batch37_panel_value(0,2)=='D'&&batch37_panel_value(0,3)=='P',
          "control panel retains S/G/D/P roster states");
    check(!control.rolls.empty()&&control.rolls[0].hi==63&&oracle(control,1),
          "control starts with wind and matches original RNG through gate");

    Run ring(1);ring.g().party.characters[0].ring=42;
    ring.g().party.characters[1].ring=42;
    ring.g().party.characters[2].ring=42;
    ring.g().party.characters[3].ring=42;ring.camp(true);
    check(batch37_frame_count()>=2&&px(1,a.x,a.y)==0xeeee,
          "ring wearer who sleeps still shows 0x11e on scene mount");
    check(px(1,b.x,b.y)==0xdddd&&px(1,p.x,p.y)==0xdddd,
          "surviving ring guard and poisoned actor show outline tile 0x11d at mount");
    check(px(1,d.x,d.y)!=0xdddd&&px(1,5,5)!=0x4444,
          "dead ring wearer leaves background and the central fire is unchanged");
    // A longer watch gives the guard an intermediate walk repaint.
    Run cadence(1);cadence.g().time.minute=0;
    cadence.g().party.characters[1].ring=42;
    cadence.g().party.characters[3].ring=42;cadence.camp(true);
    bool redrawn_outline=false;
    for(size_t f=2;f+1<batch37_frame_count();++f)
        redrawn_outline|=px(f,p.x,p.y)==0xdddd;
    check(redrawn_outline&&oracle(cadence,1),
          "poisoned ring wearer keeps outline through guard-move redraw and exact RNG");    check(batch37_panel_value(0,0)=='S'&&batch37_panel_value(0,1)=='G'&&
          batch37_panel_value(0,2)=='D'&&batch37_panel_value(0,3)=='P',
          "ring changes no status-panel member row");
    check(ring.rolls.size()>=4&&ring.rolls[0].hi==15&&ring.rolls[1].hi==15&&
          ring.rolls[2].hi==15&&ring.rolls[0].status=='G'&&ring.rolls[3].hi==63&&
          ring.rolls[3].status=='S'&&oracle(ring,1),
          "three living entry expiry draws precede S and wind; full RNG matches oracle");
    check(ring.g().party.characters[0].ring==42&&ring.g().party.characters[1].ring==42&&
          ring.g().party.characters[2].ring==42&&ring.g().party.characters[3].ring==42,
          "seed 1 leaves living rings equipped and skips dead member's expiry");
    std::printf("RNG control=%u ring=%u draws=%zu/%zu\n",
          unsigned(control.g().rng.get_seed()),unsigned(ring.g().rng.get_seed()),
          control.rolls.size(),ring.rolls.size());

    Run expired(72);expired.g().party.characters[1].ring=42;expired.camp(true);
    check(expired.rolls.size()>=2&&expired.rolls[0].hi==15&&expired.rolls[0].value==11&&
          expired.g().party.characters[1].ring==255&&px(1,b.x,b.y)==0x0000,
          "entry expiry removes guard outline before Camp mount");
    check(oracle(expired,72),"expired-ring Camp keeps exact RNG state");

    int hit=-1;
    for(int seed=1;seed<=256&&hit<0;++seed){Run probe(seed);
        probe.g().party.characters[3].ring=42;probe.camp(false);
        if(probe.waiting()&&probe.g().party.characters[3].ring==42)hit=seed;}
    check(hit>0,"deterministic surviving-ring apparition reaches Hail");
    if(hit>0){Run wake(hit);wake.g().party.characters[3].ring=42;wake.camp(false);
        check(wake.waiting()&&px(1,p.x,p.y)==0xdddd,
              "poisoned ring actor is outlined before apparition");
        // The apparition writes each live member's class standing frame after
        // the sleep scene; the last live slot is reached after acknowledgements.
        for(int i=0;i<3&&wake.waiting();++i)wake.key('x');
        bool standing=false,xor_frame=false;
        for(size_t f=2;f<batch37_frame_count();++f){
            standing|=px(f,p.x,p.y)==0x0000;
            xor_frame|=px(f,p.x,p.y)==0xffff;
        }
        check(standing&&xor_frame&&wake.g().party.characters[3].ring==42,
              "apparition overwrites outline with Mage standing tile then XORs it; ring persists");
        check(oracle(wake,hit),"apparition and advancement RNG retain original draw sequence");
    }
    std::printf("Batch 46 Camp invisibility: %d/%d checks\n",checks-failures,checks);
    return failures?1:0;
}
