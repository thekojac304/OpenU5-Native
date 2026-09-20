#include "openu5/hud.h"

namespace openu5 {
namespace {constexpr const char*winds[]={"Calm  Winds","North Winds","South Winds","East  Winds","West  Winds"};bool below(int floor){return floor<0||floor>=0x80;}void add(HudWorldState&o,int cell,uint8_t glyph,bool sun){if(cell>=0&&cell<12&&o.mark_count<3)o.marks[o.mark_count++]={int8_t(cell),glyph,sun};}}
HudWorldState hud_world_state(const GameState&g,const TurnState&t,const int32_t*phases,size_t count,bool dungeon){
    HudWorldState o{};const int loc=g.position.map.location;o.sky_visible=!dungeon&&loc<0x21&&loc!=0x19&&!below(g.position.map.floor);o.wind_visible=o.sky_visible;
    const int wind=t.wind>=0&&t.wind<5?t.wind:0;o.wind=winds[wind];
    int f=t.felucca_phase,tr=t.trammel_phase;if(f<0x30||f>0x37||tr<0x30||tr>0x37){const int day=g.time.day;if(phases&&day>=1&&size_t(day*2)<=count){f=phases[(day-1)*2];tr=phases[(day-1)*2+1];}else f=tr=0;}if(f>=0x30)f-=0x30;if(tr>=0x30)tr-=0x30;o.felucca=uint8_t(f&7);o.trammel=uint8_t(tr&7);
    if(o.sky_visible){add(o,17-g.time.hour,0x2a,true);int fc=8-g.time.hour;if(fc<-12)fc+=24;add(o,fc,uint8_t(0x30+o.felucca),false);int tc=2-g.time.hour;if(tc<-12)tc+=24;add(o,tc,uint8_t(0x30+o.trammel),false);}return o;
}

HudDungeonBands hud_dungeon_bands(const DungeonState &d,bool dungeon_active){
    HudDungeonBands o{};
    o.active=dungeon_active&&d.active;
    if(!o.active)return o;
    // "L1".."L8" -- g_floor 0..7 is level 1..8 (dungeon.md S4).
    const int level=int(d.pos.floor)+1;
    o.level[0]='L';o.level[1]=char('0'+(level<1?1:level>8?8:level));o.level[2]=0;
    // "Dir:" + the direction name right-justified in a field of 7, exactly as
    // dng_draw_panel (DUNGEON:0x01D2) lays it out.
    static const char *names[4]={"North","East","South","West"};
    const char *name=names[unsigned(d.pos.facing)&3];
    size_t n=0;while(name[n])++n;
    const size_t pad=n>=7?0:7-n;
    size_t at=0;
    o.direction[at++]='D';o.direction[at++]='i';o.direction[at++]='r';o.direction[at++]=':';
    for(size_t i=0;i<pad;++i)o.direction[at++]=' ';
    for(size_t i=0;i<n;++i)o.direction[at++]=name[i];
    o.direction[at]=0;
    return o;
}
}
