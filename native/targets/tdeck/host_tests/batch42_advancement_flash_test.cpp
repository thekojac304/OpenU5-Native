// H-176: CampFire standing actor and full-viewport XOR frames through raw Camp input.
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "openu5/rest.h"
#include <cstdio>
#include <memory>
using namespace openu5;
void batch37_reset_screen();
size_t batch37_frame_count();
uint16_t batch37_pixel(int,int);
uint16_t batch37_frame_pixel(size_t,int,int);
int batch37_panel_draw_count();
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
    bool waiting()const{return rt->ui()->mode()==UiMode::KeyWait&&
        rt->ui()->request()==UiRequestId::CampAdvance;}
};
uint16_t px(size_t f,int col,int row) {
    return batch37_frame_pixel(f,col*16+8,row*16+8);
}
bool inverted(size_t a,size_t b) {
    for(int y=0;y<176;++y)for(int x=0;x<176;++x) {
        const auto v=batch37_frame_pixel(a,x,y);
        if(batch37_frame_pixel(b,x,y)!=uint16_t(v^0xffff))return false;
    }
    return true;
}
bool equal(size_t a,size_t b) {
    for(int y=0;y<176;++y)for(int x=0;x<176;++x)
        if(batch37_frame_pixel(a,x,y)!=batch37_frame_pixel(b,x,y))return false;
    return true;
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
    const auto unchanged=arena.starts[int(CombatDirection::South)][3];
    check(first.x>=0&&first.x<11&&first.y>=0&&first.y<11&&
          second.x>=0&&second.x<11&&second.y>=0&&second.y<11&&
          dead.x>=0&&dead.x<11&&dead.y>=0&&dead.y<11&&
          unchanged.x>=0&&unchanged.x<11&&unchanged.y>=0&&unchanged.y<11,
          "shipped CampFire south formation is in the viewport");
    int seed=-1;
    for(int s=1;s<=512;++s) {
        Run candidate(s);candidate.camp();
        if(candidate.waiting()){seed=s;break;}
    }
    check(seed>0,"raw Camp reaches the apparition");
    if(seed<0)return 1;
    Run h(seed);h.camp();
    const auto &g=h.rt->game();
    check(h.waiting()&&g.party.characters[0].level==2&&
          g.party.characters[1].level==1,
          "first Hail holds later member unchanged");
    check(batch37_frame_count()>=5,
          "Camp presents scene, standing tile, XOR and restored frames before Hail");
    check(batch37_ui_draw_count()==1&&batch37_panel_draw_count()==0,
          "flash frames do not repaint party panel before Hail acknowledgement");
    if(batch37_frame_count()>=5) {
        check(px(1,first.x,first.y)==0xeeee&&
              px(1,second.x,second.y)==0xeeee&&
              px(1,unchanged.x,unchanged.y)==0xeeee,
              "scene starts with both live members on sleeping tile 0x11e");
        check(px(2,first.x,first.y)==0x8888&&
              px(2,second.x,second.y)==0xeeee&&
              px(2,unchanged.x,unchanged.y)==0xeeee,
              "first member wakes to Fighter tile 0x148 at authored cell");
        check(inverted(2,3),"one XOR pass inverts every CampFire viewport pixel");
        check(equal(2,4),"first post-chord repaint restores the pre-XOR scene");
    }
    check(g.position.xy.x==80&&g.position.xy.y==80,
          "visual actor placement leaves world coordinates untouched");
    h.rt->render(h.board,true); // the regular pump presents Hail text after the command
    check(batch37_ui_draw_count()==2&&batch37_panel_draw_count()==0&&
          batch37_pixel(201,4)==5&&batch37_pixel(201,12)==5,
          "Hail render changes transcript without repainting pre-key party HP");
    const auto later_pre_advance_hp=g.party.characters[1].current_hp;
    h.key('x');
    check(h.waiting()&&g.party.characters[1].level==3&&
          batch37_panel_draw_count()==1&&batch37_ui_draw_count()==2,
          "acknowledgement redraws one panel then advances the next member");
    check(batch37_pixel(201,4)==60&&batch37_pixel(201,12)==later_pre_advance_hp,
          "acknowledged member HP appears before next member panel update");
    check(batch37_frame_count()>=9,
          "second member has its own standing, XOR and restore frames");
    if(batch37_frame_count()>=9) {
        check(px(6,second.x,second.y)==0x0000&&
              px(6,first.x,first.y)==0x8888,
              "second member uses Mage tile 0x140; first remains standing");
        check(inverted(6,7)&&equal(6,8),
              "second member receives exactly one reversible XOR pulse");
    }
    h.key('x');
    check(h.waiting()&&g.party.characters[3].level==1&&
          h.rt->commands().camp_advance.phase==CommandState::CampAdvance::Phase::KarmaKey,
          "dead and ineligible slots add no Hail wait");
    check(batch37_frame_count()==12&&batch37_panel_draw_count()==4,
          "dead slot emits no scene frames; ineligible live slot still flashes");
    if(batch37_frame_count()==12) {
        check(px(9,unchanged.x,unchanged.y)==0x0000,
              "ineligible live member wakes to Mage tile");
        check(px(9,dead.x,dead.y)==px(1,dead.x,dead.y),
              "dead member cell retains its scene background");
        check(inverted(9,10)&&equal(9,11),
              "ineligible live member has one XOR pulse before karma speech");
    }
    h.key('x');
    check(h.rt->commands().camp_advance.phase==CommandState::CampAdvance::Phase::None&&
          h.rt->ui()->mode()==UiMode::Exploration,
          "karma acknowledgement finishes the scene and resumes commands");
    check(batch37_frame_count()==13&&batch37_world_draw_count()==2&&
          batch37_frame_pixel(12,88,88)==batch37_frame_pixel(0,88,88),
          "scene exit repaints the ordinary world viewport at the original position");
    std::printf("Batch 42 visual: %d/%d checks\n",checks-failures,checks);
    return failures?1:0;
}
