// H-175: real raw-key Camp path and synchronous right-panel redraw.
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "openu5/rest.h"
#include <cstdio>
#include <memory>
using namespace openu5;
void batch37_reset_screen();
int batch37_panel_draw_count();
int batch37_draw_count();
namespace {
int checks=0,failures=0;
void check(bool good,const char *label){
    ++checks;if(!good)++failures;
    std::printf("%s %s\n",good?"GREEN":"RED",label);
}
const tdeck::AlphaResourceOwners *pack=nullptr;
struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt=std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    int64_t us=0;
    explicit Run(int seed){
        batch37_reset_screen();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world=pack->world;f.npc_locations=pack->npc_locations;f.pack=pack;
        f.location_x=pack->location_x;f.location_y=pack->location_y;
        f.location_count=pack->location_count;f.render_pixels=true;
        rt->attach_host_test_fixture(f);
        auto &g=rt->game();
        g.position.map={0,0};g.position.xy={80,80};
        g.time.hour=12;g.time.minute=55;g.food=80;
        g.party.character_count=g.party.party_size=2;
        g.party.active_character=255;
        for(int i=0;i<2;++i){
            auto &m=g.party.characters[i];
            std::snprintf(m.name,sizeof(m.name),"Member%d",i);
            m.status='G';m.character_class='F';m.level=1;
            m.current_hp=5;m.max_hp=30;
            m.strength=m.dexterity=m.intelligence=10;
        }
        g.party.characters[0].exp=100;
        g.party.characters[1].exp=200;
        g.rng.seed(seed);
        rt->render(board,true);
    }
    void key(uint8_t code,bool alt=false){
        tdeck::RawInputEvent e{};
        e.kind=tdeck::RawInputKind::Keyboard;
        e.transition=tdeck::KeyTransition::Pressed;
        e.code=code;e.modifiers.alt=alt;
        e.timestamp_us=(us+=100000);
        rt->handle(e);
    }
    void camp(){key('h');key('1');key('\r');key('n');}
    bool waiting()const{return rt->ui()->mode()==UiMode::KeyWait&&
        rt->ui()->request()==UiRequestId::CampAdvance;}
};
}
int main(int argc,char **argv){
    if(argc<2)return 2;
    tdeck::AlphaResourcePack source;tdeck::AlphaResourceReport report{};
    if(source.open(argv[1],report)!=ESP_OK)return 2;
    static tdeck::AlphaResourceOwners owners{};
    if(source.load(owners,report)!=ESP_OK)return 2;
    pack=&owners;
    int seed=-1;
    for(int s=1;s<=512;++s){
        Run candidate(s);candidate.camp();
        if(candidate.waiting()){seed=s;break;}
    }
    check(seed>0,"a shipped-pack raw Camp reaches an apparition");
    if(seed<0)return 1;
    Run h(seed);
    const int map_draws=batch37_draw_count();
    h.camp();
    const auto &g=h.rt->game();
    check(h.waiting()&&g.party.characters[0].level==2&&
          g.party.characters[1].level==1,"first raw-key pause leaves second member old");
    check(batch37_panel_draw_count()==0&&batch37_draw_count()==map_draws,
          "first Hail pauses before the status redraw and leaves map untouched");
    h.key('x');
    check(h.waiting()&&g.party.characters[1].level==3,
          "one raw key releases only the first member and presents the second");
    check(batch37_panel_draw_count()==1&&batch37_draw_count()==map_draws,
          "first acknowledgement redraws one status panel without map redraw");
    h.key('s',true); // Alt+S is a getkey, not a save inside the original scene.
    check(h.waiting()&&
          h.rt->commands().camp_advance.phase==CommandState::CampAdvance::Phase::KarmaKey,
          "save shortcut acknowledges the second Hail without opening save");
    check(batch37_panel_draw_count()==2&&batch37_draw_count()==map_draws,
          "second acknowledgement redraws its panel before karma speech");
    h.key('\b');
    check(h.rt->ui()->mode()==UiMode::Exploration&&
          h.rt->commands().camp_advance.phase==CommandState::CampAdvance::Phase::None,
          "Back key acknowledges karma and returns to commands");
    check(g.party.characters[0].exp==100&&g.party.characters[1].exp==200&&
          g.party.characters[0].max_hp==60&&g.party.characters[1].max_hp==90,
          "raw-key path retains the proven XP and HP results");
    std::printf("Batch 41 raw key: %d/%d checks\n",checks-failures,checks);
    return failures?1:0;
}
