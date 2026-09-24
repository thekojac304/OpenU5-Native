// H-172: inspect physical viewport pixels while the real raw-key bed command runs.
// Original CMDS: G->S, status draw, Zzz, color-zero copy fill (8,8)-(183,183),
// then every ten-minute tick while the viewport stays black; next redraw restores.
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
bool batch37_map_black();
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char *id, const char *why) {
    ++checks; if (!ok) ++failures;
    std::printf("%s %s %s\n", ok ? "GREEN" : "RED", id, why);
}
constexpr int cx = 4 + 88, cy = 4 + 88;
struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    int64_t us = 0;
    explicit Run(const tdeck::AlphaResourceOwners &pack, uint8_t location) {
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world=pack.world; f.npc_locations=pack.npc_locations; f.pack=&pack;
        f.location_x=pack.location_x; f.location_y=pack.location_y;
        f.location_count=pack.location_count; f.render_pixels=true;
        rt->attach_host_test_fixture(f);
        auto &g=rt->game();g.position.map={0,0};
        g.position.xy={pack.location_x[location-1],pack.location_y[location-1]};
        g.time.hour=12;g.time.minute=0;g.food=100;
        g.party.party_size=g.party.character_count=1;
        g.party.characters[0].status='G';g.party.characters[0].current_hp=100;
        g.party.characters[0].max_hp=100;
        key('e');
        g.position.xy={9,7};
        rt->render(board,true);
    }
    void key(uint8_t code) {
        tdeck::RawInputEvent e{};e.kind=tdeck::RawInputKind::Keyboard;
        e.transition=tdeck::KeyTransition::Pressed;e.code=code;
        e.timestamp_us=(us+=100000);rt->handle(e);
    }
    void hole(int hours) { key('h');key(uint8_t('0'+hours));key('\r'); }
};
struct Watch {
    Run &r; EventSink downstream{}; const RestServices *original_ptr=nullptr; RestServices original{}, wrapped{};
    int zzz=0, zzz_status=0, zzz_minute=-1, zzz_turns=-1;
    uint16_t zzz_pixel=0, first_tick_pixel=0; int snaps=0;
    bool all_ticks_black=true;
    static void event(void *p,const GameEvent &e) {
        auto &w=*static_cast<Watch*>(p);
        if (e.kind==GameEventKind::Message&&e.text&&std::strstr(e.text,"Zzzzzzz")) {
            ++w.zzz;const auto &g=w.r.rt->game();
            w.zzz_status=g.party.characters[0].status;
            w.zzz_minute=g.time.minute;
            w.zzz_turns=int(g.turns_since_start);
            w.zzz_pixel=batch37_pixel(cx,cy);
        }
        if (w.downstream.emit) w.downstream.emit(w.downstream.context,e);
    }
    static void snap(void *p) {
        auto &w=*static_cast<Watch*>(p);
        ++w.snaps;
        const uint16_t pixel=batch37_pixel(cx,cy);
        if (w.snaps==1) w.first_tick_pixel=pixel;
        w.all_ticks_black &= batch37_map_black();
        w.original.snap_npcs(w.original.context);
    }
    static bool occupied(void *p,int x,int y,int z) {
        auto &w=*static_cast<Watch*>(p);
        return w.original.occupied(w.original.context,x,y,z);
    }
    explicit Watch(Run &run):r(run) {
        auto &c=r.rt->command_context_for_test();
        downstream=c.events;c.events={this,event};
        original_ptr=c.rest_services;original=*original_ptr;wrapped=original;wrapped.context=this;
        wrapped.snap_npcs=snap;wrapped.occupied=occupied;c.rest_services=&wrapped;
    }
    ~Watch() { auto &c=r.rt->command_context_for_test();c.events=downstream;c.rest_services=original_ptr; }
};
}
int main(int argc,char **argv) {
    if(argc<2)return 2;
    tdeck::AlphaResourcePack p;tdeck::AlphaResourceReport report{};
    if(p.open(argv[1],report)!=ESP_OK)return 2;
    static tdeck::AlphaResourceOwners pack{};
    if(p.load(pack,report)!=ESP_OK)return 2;
    batch37_reset_screen();
    Run bed(pack,17);
    auto &g=bed.rt->game();
    check(g.position.map.location==17&&bed.rt->command_context().terrain->raw(pack.world,{17,0},9,7)==0xab,
          "A1","authored castle bed reached through Enter");
    check(batch37_pixel(cx,cy)==0xffff&&batch37_pixel(cx,5)==0x07ff&&batch37_pixel(1,cy)==0x1357,
          "A2","pre-sleep rendered map, HUD strip and frame differ");
    const int turns=int(g.turns_since_start);
    {
        Watch w(bed);bed.hole(1);
        check(w.zzz==1&&w.zzz_status=='S'&&w.zzz_minute==0&&w.zzz_turns==turns,
              "B1","party sleeps and Zzz precedes any ten-minute tick");
        check(w.zzz_pixel==0xffff,"B2","Zzz occurs before viewport fill");
        check(w.snaps==6&&w.first_tick_pixel==0&&w.all_ticks_black,
              "B3","first and every ten-minute snap sees black viewport");
        check(batch37_map_black()&&batch37_pixel(cx,5)==0x07ff&&batch37_pixel(1,cy)==0x1357,
              "B4","whole map stays black through wake while HUD and frame remain");
        check(g.time.hour==13&&g.turns_since_start==turns+6&&g.party.characters[0].status=='G'&&g.position.xy.x==10,
              "B5","sleep mechanics and east-step epilogue intact");
        check(batch37_fill_count()==1,"B6","one bed-entry fill only");
    }
    const int before=batch37_draw_count();
    check(bed.rt->render(bed.board)==ESP_OK&&batch37_draw_count()==before+1&&batch37_pixel(cx,cy)==0xffff,
          "B7","normal post-command redraw restores map pixels");
    batch37_reset_screen();
    Run camp(pack,17);
    auto &cg=camp.rt->game();cg.position.map={0,0};cg.position.xy={80,80};
    camp.rt->render(camp.board,true);
    camp.hole(1);
    check(batch37_fill_count()==0&&batch37_pixel(cx,cy)==0xffff,"C1","Camp never fills bed viewport");
    batch37_reset_screen();
    Run cancel(pack,13);
    auto &hg=cancel.rt->game();auto &actors=cancel.rt->actors();
    NpcActor *hostile=nullptr;for(size_t i=0;i<actors.count;++i)
        if(actors.actors[i].schedule.slot==1)hostile=&actors.actors[i];
    check(hostile!=nullptr,"D1","authored Iolo's Hut hostile is loaded");
    if(hostile){hostile->x=12;hostile->y=13;hg.position.xy={12,14};
        cancel.rt->render(cancel.board,true);cancel.hole(1);
        check(batch37_fill_count()==0&&batch37_pixel(cx,cy)==0xffff&&hg.party.characters[0].status=='G',
              "D2","hostile pre-sleep cancellation does not blank viewport");}
    std::printf("batch37_bed_viewport: %d/%d GREEN, %d RED\n",checks-failures,checks,failures);
    return failures?1:0;
}
