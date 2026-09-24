// H-174: capture the status panel during the real raw-key bed command.
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "openu5/rest.h"
#include <cstdio>
#include <cstring>
#include <memory>
using namespace openu5;
void batch37_reset_screen();
uint16_t batch37_pixel(int, int);
int batch37_fill_count();
int batch37_draw_count();
int batch37_panel_draw_count();
uint16_t batch37_first_panel_map_pixel();
namespace {
int checks=0, failures=0;
void check(bool ok,const char *id,const char *meaning) {
    ++checks;if(!ok)++failures;
    std::printf("%s %s %s\n",ok?"GREEN":"RED",id,meaning);
}
constexpr int map_x=92,map_y=92;
constexpr int status_x=200,hp_x=201,selected_x=202,clock_y=68;
uint16_t status(int row) { return batch37_pixel(status_x,4+row*8); }
uint16_t hp(int row) { return batch37_pixel(hp_x,4+row*8); }
struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt=std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    int64_t us=0;
    explicit Run(const tdeck::AlphaResourceOwners &pack) {
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world=pack.world;f.npc_locations=pack.npc_locations;f.pack=&pack;
        f.location_x=pack.location_x;f.location_y=pack.location_y;
        f.location_count=pack.location_count;f.render_pixels=true;
        rt->attach_host_test_fixture(f);
        auto &g=rt->game();g.position.map={0,0};
        g.position.xy={pack.location_x[16],pack.location_y[16]};
        g.time.hour=12;g.time.minute=0;g.food=100;
        key('e');
        g.position.xy={9,7};
        g.party.party_size=g.party.character_count=2;
        std::strcpy(g.party.characters[0].name,"Avatar");
        std::strcpy(g.party.characters[1].name,"Poisoned");
        g.party.characters[0].status='G';g.party.characters[1].status='P';
        for(int i=0;i<2;++i){g.party.characters[i].current_hp=100;g.party.characters[i].max_hp=100;}
        g.party.active_character=0;
        rt->render(board,true);
    }
    void key(uint8_t code) {
        tdeck::RawInputEvent e{};e.kind=tdeck::RawInputKind::Keyboard;
        e.transition=tdeck::KeyTransition::Pressed;e.code=code;
        e.timestamp_us=(us+=100000);rt->handle(e);
    }
    void sleep() {key('h');key('1');key('\r');}
};
struct Watch {
    Run &r;EventSink downstream{};const RestServices *old_ptr=nullptr;RestServices original{},wrapped{};
    int zzz=0,snaps=0,at_zzz_draws=-1,at_zzz_fills=-1,at_zzz_panels=-1;
    int at_zzz_state=0,at_zzz_selected=0,at_zzz_status=0,at_zzz_hp=0,at_zzz_clock=0;
    uint16_t at_zzz_map=0,first_tick_status=0,first_tick_hp=0,first_tick_clock=0;
    bool every_tick_current=true,every_tick_black=true;
    static void event(void *p,const GameEvent &e) {
        auto &w=*static_cast<Watch*>(p);
        if(e.kind==GameEventKind::Message&&e.text&&std::strstr(e.text,"Zzzzzzz")) {
            ++w.zzz;const auto &g=w.r.rt->game();
            w.at_zzz_state=g.party.characters[0].status;
            w.at_zzz_selected=g.party.active_character;
            w.at_zzz_status=status(0);w.at_zzz_hp=hp(1);
            w.at_zzz_clock=batch37_pixel(status_x,clock_y);
            w.at_zzz_map=batch37_pixel(map_x,map_y);
            w.at_zzz_draws=batch37_draw_count();
            w.at_zzz_fills=batch37_fill_count();
            w.at_zzz_panels=batch37_panel_draw_count();
        }
        if(w.downstream.emit)w.downstream.emit(w.downstream.context,e);
    }
    static void snap(void *p) {
        auto &w=*static_cast<Watch*>(p);++w.snaps;
        if(w.snaps==1){w.first_tick_status=status(0);w.first_tick_hp=hp(1);
            w.first_tick_clock=batch37_pixel(status_x,clock_y);}
        w.every_tick_current &= status(0)=='S' && hp(1)==100-w.snaps &&
            batch37_pixel(status_x,clock_y)==720+10*w.snaps &&
            batch37_panel_draw_count()==1+w.snaps;
        w.every_tick_black &= batch37_pixel(map_x,map_y)==0;
        w.original.snap_npcs(w.original.context);
    }
    static bool occupied(void *p,int x,int y,int z) {
        auto &w=*static_cast<Watch*>(p);return w.original.occupied(w.original.context,x,y,z);
    }
    explicit Watch(Run &run):r(run) {
        auto &c=r.rt->command_context_for_test();
        downstream=c.events;c.events={this,event};
        old_ptr=c.rest_services;original=*old_ptr;wrapped=original;wrapped.context=this;
        wrapped.snap_npcs=snap;wrapped.occupied=occupied;c.rest_services=&wrapped;
    }
    ~Watch(){auto &c=r.rt->command_context_for_test();c.events=downstream;c.rest_services=old_ptr;}
};
}
int main(int argc,char **argv) {
    if(argc<2)return 2;
    tdeck::AlphaResourcePack p;tdeck::AlphaResourceReport report{};
    if(p.open(argv[1],report)!=ESP_OK)return 2;
    static tdeck::AlphaResourceOwners pack{};
    if(p.load(pack,report)!=ESP_OK)return 2;
    batch37_reset_screen();Run bed(pack);auto &g=bed.rt->game();
    check(g.position.map.location==17&&bed.rt->command_context().terrain->raw(pack.world,{17,0},9,7)==0xab,
          "A1","authored castle bed reached by raw Enter");
    check(status(0)=='G'&&status(1)=='P'&&hp(1)==100&&batch37_pixel(status_x,clock_y)==720,
          "A2","pre-sleep panel shows G, poisoned HP 100 and 12:00");
    check(batch37_pixel(map_x,map_y)==0xffff&&batch37_panel_draw_count()==0,
          "A3","pre-sleep map visible with no intermediate panel draw");
    const int initial_draws=batch37_draw_count();
    {
        Watch w(bed);bed.sleep();
        check(w.zzz==1&&w.at_zzz_state=='S',"B1","sleeping status assigned before Zzz");
        check(w.at_zzz_status=='S'&&w.at_zzz_hp==100&&w.at_zzz_clock==720,
              "B2","intermediate panel presents sleeping status before Zzz");
        check(w.at_zzz_selected==255&&batch37_pixel(selected_x,4)==0,
              "B3","original status draw clears sleeping active character");
        check(w.at_zzz_panels==1,"B4","one entry panel draw before Zzz");
        check(w.at_zzz_map==0xffff&&w.at_zzz_fills==0&&w.at_zzz_draws==initial_draws,
              "B5","panel draw precedes Zzz and blackout without map redraw");
        check(batch37_first_panel_map_pixel()==0xffff,"B6","entry panel refresh preceded black fill");
        check(w.snaps==6&&w.first_tick_status=='S'&&w.first_tick_hp==99&&w.first_tick_clock==730,
              "C1","first tick redraw follows poison and clock housekeeping");
        check(w.every_tick_current,"C2","each tick refreshes S, poison HP and clock once");
        check(w.every_tick_black,"C3","all tick panel refreshes preserve map blackout");
        check(batch37_panel_draw_count()==7&&batch37_fill_count()==1&&batch37_draw_count()==initial_draws,
              "C4","one entry and six tick panel draws; one fill; no full redraw");
        check(status(0)=='S'&&hp(1)==94&&batch37_pixel(status_x,clock_y)==780,
              "D1","last panel frame persists through wake");
        check(g.party.characters[0].status=='G'&&g.party.characters[1].status=='P'&&
              g.party.characters[1].current_hp==94&&g.position.xy.x==10,
              "D2","game state and east-step epilogue remain intact");
        check(batch37_pixel(map_x,map_y)==0&&batch37_pixel(1,map_y)==0x1357,
              "D3","black map and frame persist after command");
    }
    check(bed.rt->render(bed.board)==ESP_OK&&status(0)=='G'&&
          batch37_pixel(map_x,map_y)==0xffff,"E1","ordinary redraw restores awake panel and map");
    std::printf("batch39_party_panel: %d/%d GREEN, %d RED\n",checks-failures,checks,failures);
    return failures?1:0;
}
