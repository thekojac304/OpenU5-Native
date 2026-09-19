#include "openu5/presentation.h"

#include <algorithm>
#include <array>
#include <cstdlib>

#include "openu5/outdoor.h"
#include "openu5/quest_world.h"
#include "openu5/world_commands.h"

namespace openu5 {
namespace {

constexpr int kHalf = kPresentationWindow / 2;
constexpr int kRadial[36] = {
    50,41,34,29,26,25, 41,32,25,20,17,16, 34,25,18,13,10,9,
    29,20,13,8,5,4, 26,17,10,5,2,1, 25,16,9,4,1,0,
};
constexpr int kNeighbors[8][2] = {
    {-1,0},{-1,1},{0,1},{1,1},{1,0},{1,-1},{0,-1},{-1,-1},
};
constexpr int kOpaque[] = {
    0x09,0x0a,0x0c,0x0d,0x4d,0x4e,0x4f,0x5a,0x97,0xb8,
    0xb9,0xbc,0xd0,0xd1,0xd2,0xd3,0xf8,0xfe,0xff,
};
constexpr int kWindowed[] = {0x4a,0x4b,0xba,0xbb,0x98};
constexpr int kEmitters[] = {0xdc,0xbd,0xbe,0xb2,0xde,0xbf,0xb0,0xb1,0xb3,0xbc};

bool contains(const int *values, size_t count, int value) {
    for (size_t i=0;i<count;++i) if (values[i]==value) return true;
    return false;
}
int radial_offset(int x,int y) {
    x=std::abs(x);y=std::abs(y);
    return x>kHalf||y>kHalf?51:kRadial[kHalf-x+kPresentationWindow*0+(kHalf-y)*6];
}
bool sight_blocking(int tile,int radial) {
    tile&=255;
    if(contains(kOpaque,sizeof(kOpaque)/sizeof(kOpaque[0]),tile))return true;
    return contains(kWindowed,sizeof(kWindowed)/sizeof(kWindowed[0]),tile)&&radial!=1;
}
int effective_terrain(CommandContext &c,const ActiveMap &map,int x,int y) {
    if(map.geometry.wraps){x=wrap_coord(x);y=wrap_coord(y);}
    else if(!in_bounds(x,y,map.geometry))return map.edge_fill_tile;
    int tile=map.tile_at(x,y);
    tile=quest_world_tile(c,map.id,x,y,tile);
    return open_door_tile(c,map.id,x,y,tile);
}

using TileSampler = int (*)(void *,int,int);
void flood(int cc,int cr,int light,TileSampler sample,void *sample_context,
           const uint8_t *illuminated,bool party,uint8_t (&out)[kPresentationCells]) {
    enum : uint8_t { Undecided, Hidden, Visible };
    uint8_t state[kPresentationCells]{};
    int queue[kPresentationCells]{};int head=0,tail=1;
    queue[0]=cr*kPresentationWindow+cc;state[queue[0]]=Visible;
    auto lit=[&](int x,int y){return illuminated&&illuminated[y*kPresentationWindow+x];};
    while(head<tail){const int cur=queue[head++],cx=cur%kPresentationWindow,cy=cur/kPresentationWindow;
        const bool parent_visible=state[cur]==Visible,parent_lit=lit(cx,cy);
        for(const auto &d:kNeighbors){const int nx=cx+d[0],ny=cy+d[1];
            if(nx<0||ny<0||nx>=kPresentationWindow||ny>=kPresentationWindow)continue;
            const int ni=ny*kPresentationWindow+nx;if(state[ni]!=Undecided)continue;
            const int radius=radial_offset(nx-cc,ny-cr);const bool blocks=sight_blocking(sample(sample_context,nx,ny),radius);
            if(radius<=light){state[ni]=Visible;if(!blocks)queue[tail++]=ni;continue;}
            if(!party)continue;
            if(!blocks){state[ni]=lit(nx,ny)?Visible:Hidden;queue[tail++]=ni;continue;}
            if(parent_visible&&parent_lit&&lit(nx,ny))state[ni]=Visible;
        }
    }
    for(int i=0;i<kPresentationCells;++i)out[i]=state[i]==Visible?1:0;
}

struct WorldSampler {CommandContext *context;const ActiveMap *map;Position center;};
int sample_world(void *p,int col,int row){auto &s=*static_cast<WorldSampler*>(p);return effective_terrain(*s.context,*s.map,int(s.center.x)-kHalf+col,int(s.center.y)-kHalf+row);}

void visibility(CommandContext &c,const ActiveMap &map,Position center,uint8_t (&out)[kPresentationCells]) {
    WorldSampler sampler{&c,&map,center};uint8_t emit[kPresentationCells]{};
    constexpr int reach=3;
    for(int er=-reach;er<kPresentationWindow+reach;++er)for(int ec=-reach;ec<kPresentationWindow+reach;++ec){
        if(!contains(kEmitters,sizeof(kEmitters)/sizeof(kEmitters[0]),sample_world(&sampler,ec,er)&255))continue;
        struct EmitterSampler {WorldSampler *world;int col,row;};EmitterSampler es{&sampler,ec,er};
        auto esample=[](void *p,int col,int row){auto &e=*static_cast<EmitterSampler*>(p);return sample_world(e.world,e.col-kHalf+col,e.row-kHalf+row);};
        uint8_t glow[kPresentationCells]{};flood(kHalf,kHalf,10,esample,&es,nullptr,false,glow);
        for(int row=0;row<kPresentationWindow;++row)for(int col=0;col<kPresentationWindow;++col)if(glow[row*kPresentationWindow+col]){
            const int wc=ec-kHalf+col,wr=er-kHalf+row;if(wc>=0&&wr>=0&&wc<kPresentationWindow&&wr<kPresentationWindow)emit[wr*kPresentationWindow+wc]=1;
        }
    }
    flood(kHalf,kHalf,presentation_light_level(c.game,c.turn),sample_world,&sampler,emit,true,out);
}

int party_combat_tile(char c){switch(c){case 'B':case 'S':case 'T':return 0x144;case 'D':case 'M':return 0x140;case 'F':case 'P':case 'R':return 0x148;default:return 0x14c;}}
bool active(const CombatActor&a){return a.status==CombatStatus::Active||a.status==CombatStatus::Sleeping;}

constexpr uint8_t kProgPeople[]={0x02,0x03,0x04,0x05};
constexpr uint8_t kProgGuard[]={0x02,0x84,0x03,0x84,0x04,0x84,0x05};
constexpr uint8_t kProgFour[]={0x01,0x02,0x03,0x04};
constexpr uint8_t kProgBat[]={0x01,0x02,0x03,0x04,0x03,0x04,0x01,0x02};
constexpr uint8_t kProgGazer[]={0x01,0x02,0x03,0x04,0x87};
constexpr uint8_t kProgMimic[]={0x01,0x8f,0x02,0x03,0x04};
constexpr uint8_t kProgSpider[]={0x01,0x02,0x01,0x02,0x03,0x04,0x02,0x03,0x04};
constexpr uint8_t kProgDragon[]={0x02,0x03,0x04,0x03,0x02,0x05};
constexpr uint8_t kProgCorpser[]={0x01,0x8f,0x02,0x03,0x04,0x07};
constexpr uint8_t kProgShark[]={0x02,0x82,0x03,0x82,0x04,0x82,0x06,0x01};
constexpr uint8_t kProgGargoyle[]={0x01,0x02,0x03,0x05};
struct Program {const uint8_t *bytes;uint8_t size;};
Program actor_program(uint8_t base){
    switch(base){
    case 0x40:case 0x44:case 0x48:case 0x4c:case 0x50:case 0x54:case 0x58:case 0x5c:case 0x60:case 0x64:case 0x68:case 0x6c:case 0x74:case 0x78:case 0x7c:
    case 0x80:case 0x84:case 0x88:case 0x90:case 0xa4:case 0xac:case 0xbc:case 0xc4:case 0xcc:case 0xd4:case 0xe0:case 0xec:case 0xf8:case 0xfc:return {kProgPeople,uint8_t(sizeof(kProgPeople))};
    case 0x70:return {kProgGuard,uint8_t(sizeof(kProgGuard))};
    case 0x94:case 0x9c:case 0xa0:case 0xc0:case 0xc8:case 0xd0:case 0xd8:return {kProgFour,uint8_t(sizeof(kProgFour))};
    case 0xf0:return {kProgBat,uint8_t(sizeof(kProgBat))};
    case 0xb0:return {kProgGazer,uint8_t(sizeof(kProgGazer))};
    case 0xa8:return {kProgMimic,uint8_t(sizeof(kProgMimic))};
    case 0x98:return {kProgSpider,uint8_t(sizeof(kProgSpider))};
    case 0xdc:case 0xe4:return {kProgDragon,uint8_t(sizeof(kProgDragon))};
    case 0xf4:return {kProgCorpser,uint8_t(sizeof(kProgCorpser))};
    case 0x8c:return {kProgShark,uint8_t(sizeof(kProgShark))};
    case 0xb8:return {kProgGargoyle,uint8_t(sizeof(kProgGargoyle))};
    default:return {};
    }
}

} // namespace

int32_t presentation_light_level(const GameState &g,const TurnState &t){
    static constexpr int ramp[]={2,5,10,20,34,49};
    const int h=int(g.time.hour),m=int(std::clamp<int32_t>(g.time.minute,0,59));
    const int floor=int(g.position.map.floor)&255;int light;
    if(g.position.map.location==0x19||floor>0x7f||h<5||h>19)light=2;
    else if(h==5)light=ramp[m/10];else if(h==19)light=ramp[(59-m)/10];else light=50;
    if(t.light_spell_minutes>0&&light<18)light=18;
    if(g.torch_turns>0&&light<10)light=10;
    return light;
}

TileAnimationKind tile_animation_kind(int32_t tile){
    if(tile<0)return TileAnimationKind::Static;
    if(tile==1||tile==2||tile==3||tile==0x8f)return TileAnimationKind::WaterScroll;
    if((tile>=0x60&&tile<=0x6f)||(tile>=0x34&&tile<=0x37)||(tile>=0xe4&&tile<=0xe7))return TileAnimationKind::WaterComposite;
    switch(tile){case 0xb0:case 0xb1:case 0xb2:case 0xb3:case 0xbc:case 0xbd:case 0xbe:case 0xbf:case 0xde:return TileAnimationKind::FireNoise;default:break;}
    if((tile>=128&&tile<=131)||(tile>=192&&tile<=194)||(tile>=212&&tile<=219)||(tile>=232&&tile<=239)||(tile>=250&&tile<=253))return TileAnimationKind::TileCycle;
    if(tile>=256&&actor_program(uint8_t(tile&0xfc)).bytes)return TileAnimationKind::ActorProgram;
    return TileAnimationKind::Static;
}

uint8_t ActorAnimationClock::random_byte(){
    uint32_t t=(prng_+=0x6d2b79f5U);
    t=(t^(t>>15))*(t|1U);
    t=t^(t+(t^(t>>7))*(t|61U));
    return uint8_t((t^(t>>14))&0xffU);
}
void ActorAnimationClock::reset(){for(auto&e:entries_)e={};prng_=0x5c5a1d1eU;last_phase_=0;phase_initialized_=false;}
void ActorAnimationClock::tick(){
    for(auto &st:entries_){
        if(!st.id)continue;
        const auto p=actor_program(st.base);
        if(!p.bytes)continue;
        if(st.timer==0x0f)continue;
        if(st.timer){st.timer=uint8_t((st.timer-1)&0x0f);continue;}
        if(st.base!=0x5c&&st.base!=0xa8&&random_byte()<0x80)continue;
        for(int guard=0;guard<32;++guard){const uint8_t op=st.pc<p.size?p.bytes[st.pc]:0;
            if(op>7){st.timer=uint8_t((op-0x80)&0x0f);st.pc=uint8_t((st.pc+1)&0x0f);break;}
            if(op>=1&&op<=4){st.frame=uint16_t(st.bank|uint8_t(st.base+op-1));st.pc=uint8_t((st.pc+1)&0x0f);break;}
            if(op==0){st.pc=0;continue;}
            if(op==5){if(random_byte()>=0x40){st.pc=uint8_t((st.pc+1)&0x0f);continue;}st.frame=uint16_t(st.bank|st.seed);if(st.base==0x5c)st.pc=uint8_t((st.pc+1)&0x0f);else st.timer=6;break;}
            if(op==6){st.pc=random_byte()>=0xc0?uint8_t((st.pc+1)&0x0f):0;continue;}
            st.pc=2;
        }
    }
}
void ActorAnimationClock::render(PresentationSnapshot &s,uint32_t phase,bool frozen){
    for(auto&e:entries_)e.live=false;
    for(int i=0;i<kPresentationCells;++i){if(!s.actor_ids[i]||s.tiles[i]<256)continue;const uint8_t base=uint8_t(s.tiles[i]&0xfc);if(!actor_program(base).bytes)continue;
        Entry *entry=nullptr,*free_entry=nullptr;for(auto&e:entries_){if(e.id==s.actor_ids[i]){entry=&e;break;}if(!e.id&&!free_entry)free_entry=&e;}if(!entry)entry=free_entry;if(!entry)continue;
        const uint16_t bank=uint16_t(s.tiles[i]&0xff00);const uint8_t seed=s.actor_seeds[i]?s.actor_seeds[i]:base;
        if(entry->id!=s.actor_ids[i]||entry->base!=base||entry->bank!=bank||entry->seed!=seed){*entry={};entry->id=s.actor_ids[i];entry->base=base;entry->bank=bank;entry->seed=seed;entry->frame=uint16_t(bank|seed);}entry->live=true;
    }
    if(!phase_initialized_){last_phase_=phase;phase_initialized_=true;}
    else if(phase>=last_phase_){uint32_t first=last_phase_+1;if(phase-first+1>4)first=phase-3;for(uint32_t p=first;p<=phase;++p)if(!frozen&&(p&1U)==0)tick();last_phase_=phase;}
    else last_phase_=phase;
    for(int i=0;i<kPresentationCells;++i){if(!s.actor_ids[i])continue;for(auto&e:entries_)if(e.id==s.actor_ids[i]){s.tiles[i]=int16_t(e.frame);s.animated[i]=1;s.any_animated=true;break;}}
    for(auto&e:entries_)if(e.id&&!e.live)e={};
}

int32_t animated_tile_frame(int32_t tile,uint32_t phase,int64_t turn){
    const auto kind=tile_animation_kind(tile);if(kind!=TileAnimationKind::TileCycle&&kind!=TileAnimationKind::PerTurnCycle)return tile;
    int base=tile,size=1,divisor=2;
    if(tile>=128&&tile<=129){base=128;size=2;divisor=4;}else if(tile>=130&&tile<=131){base=130;size=2;divisor=4;}
    else if(tile>=192&&tile<=194){base=192;size=3;}else if(tile>=212&&tile<=215){base=212;size=4;}
    else if(tile>=216&&tile<=219){base=216;size=4;}else if(tile>=232&&tile<=235){base=232;size=4;}
    else if(tile>=236&&tile<=239){base=236;size=4;}else if(tile>=250&&tile<=251){base=250;size=2;divisor=4;}
    else if(tile>=252&&tile<=253){base=252;size=2;divisor=4;}else {base=tile-(tile%4);size=4;}
    const int64_t step=kind==TileAnimationKind::PerTurnCycle?turn:int64_t(phase/uint32_t(divisor));
    return base+int((tile-base+step)%size);
}

PresentationSnapshot compose_world_presentation(CommandContext &c,const ActiveMap &map,Position center,int32_t avatar_tile){
    PresentationSnapshot s;s.center=center;
    for(int row=0;row<kPresentationWindow;++row)for(int col=0;col<kPresentationWindow;++col){const int i=row*kPresentationWindow+col;s.tiles[i]=int16_t(effective_terrain(c,map,int(center.x)-kHalf+col,int(center.y)-kHalf+row));}
    visibility(c,map,center,s.visible);
    auto cell_at=[&](int x,int y)->int{int dx=x-int(center.x),dy=y-int(center.y);if(map.geometry.wraps){if(dx>128)dx-=256;if(dx< -128)dx+=256;if(dy>128)dy-=256;if(dy< -128)dy+=256;}const int col=dx+kHalf,row=dy+kHalf;if(col<0||row<0||col>=kPresentationWindow||row>=kPresentationWindow)return -1;const int at=row*kPresentationWindow+col;return s.visible[at]?at:-1;};
    auto place=[&](int x,int y,int tile,uint32_t actor_id=0,uint8_t seed=0){const int at=cell_at(x,y);if(at>=0){s.tiles[at]=int16_t(tile);s.actor_ids[at]=actor_id;s.actor_seeds[at]=seed;}};
    if(!map.id.location&&c.outdoor)for(size_t i=0;i<c.outdoor->enemies.size();++i){const auto&e=c.outdoor->enemies[i];place(e.x,e.y,e.tile,0x10000U+uint32_t(e.slot>=0?e.slot:int(i)+32),uint8_t(e.tile&0xfc));}
    if(map.id.location&&c.actors)for(size_t i=0;i<c.actors->count;++i){const auto&a=c.actors->actors[i];if(a.location==map.id.location&&a.z==map.id.floor&&a.schedule.dialog)place(a.x,a.y,a.schedule.type+256,0x20000U+a.schedule.slot,uint8_t(a.schedule.type&0xfc));}
    // R-04 (Batch 2): two reference-faithful layers, not one unified
    // last-write-wins pass. Layer 1 (stationary/non-loot: chest/prop/ship/
    // torch/plot/shadowlord) resolves first-match-per-cell, mirroring
    // game.ts tileAt()'s Array.find. Layer 2 (loot/search only) resolves
    // last-match-per-cell -- LIFO/top-of-stack -- and is always painted
    // *after* layer 1, mirroring game.ts lootRenderTiles(), so loose loot or
    // a search find can never be masked by a stationary object sharing its
    // cell, regardless of QuestWorldServices append order.
    auto*q=c.quest_world;
    if(q&&q->count&&q->read){
        bool claimed[kPresentationCells]{};
        for(size_t i=0;i<q->count(q->context);++i){
            const auto o=q->read(q->context,i);
            if(o.loot||o.search)continue;
            if(o.location!=map.id.location||o.floor!=map.id.floor)continue;
            const int at=cell_at(o.x,o.y);
            if(at<0||claimed[at])continue;
            claimed[at]=true;
            place(o.x,o.y,o.shadowlord?o.tile+256:o.tile,o.shadowlord?0x30000U+uint32_t(o.slot>=0?o.slot:int(i)):0,uint8_t(o.tile&0xfc));
        }
        for(size_t i=0;i<q->count(q->context);++i){
            const auto o=q->read(q->context,i);
            if(!o.loot&&!o.search)continue;
            if(o.location!=map.id.location||o.floor!=map.id.floor)continue;
            place(o.x,o.y,o.tile+256,o.shadowlord?0x30000U+uint32_t(o.slot>=0?o.slot:int(i)):0,uint8_t(o.tile&0xfc));
        }
    }
    s.tiles[kHalf*kPresentationWindow+kHalf]=int16_t(avatar_tile);s.visible[kHalf*kPresentationWindow+kHalf]=1;
    for(int i=0;i<kPresentationCells;++i){if(!s.visible[i]&&s.tiles[i]!=kPresentationOffMap){s.tiles[i]=kPresentationHidden;s.actor_ids[i]=0;}const auto k=tile_animation_kind(s.tiles[i]);s.animated[i]=k==TileAnimationKind::TileCycle||k==TileAnimationKind::WaterScroll||k==TileAnimationKind::WaterComposite||k==TileAnimationKind::FireNoise||k==TileAnimationKind::ActorProgram;s.any_animated|=s.animated[i]!=0;}
    return s;
}

PresentationSnapshot compose_combat_presentation(const CombatState &c,const GameState &g){
    PresentationSnapshot s;s.combat=true;s.center={5,5};std::fill(std::begin(s.visible),std::end(s.visible),uint8_t(1));
    for(int i=0;i<kPresentationCells;++i)s.tiles[i]=c.map.tiles[i];
    for(int i=0;i<c.field_count;++i){const auto&f=c.fields[i];if(f.position.x>=0&&f.position.y>=0&&f.position.x<11&&f.position.y<11)s.tiles[f.position.y*11+f.position.x]=f.tile;}
    for(int i=0;i<kPresentationCells;++i)if(c.loot[i]>0)s.tiles[i]=int16_t(combat_loot_render_tile(c.loot[i]));
    // Loose loot is an ordered object stack, not a second encoding in loot[].
    // It is placed in insertion order, so the last record is both the visible
    // representative and the LIFO item returned by Get.  Rebuilding this layer
    // every snapshot makes removal reveal the next surviving object immediately.
    for(int i=0;i<c.pile_count;++i){const auto &pile=c.piles[i];if(pile.position.x>=0&&pile.position.y>=0&&pile.position.x<11&&pile.position.y<11)s.tiles[pile.position.y*11+pile.position.x]=int16_t(combat_loot_render_tile(pile.id));}
    for(int i=0;i<c.count;++i){const auto&a=c.actors[i];if(!active(a)||a.render_tile==0||(a.enemy&&a.invisible))continue;if(a.position.x<0||a.position.y<0||a.position.x>=11||a.position.y>=11)continue;int tile;if(a.enemy)tile=a.render_tile>=0?a.render_tile+0x100:(a.enemy->tile>=0?a.enemy->tile:0x140+4*a.enemy->index);else {char cls='A';if(a.member<g.party.character_count)cls=g.party.characters[a.member].character_class;tile=a.invisible?0x11d:(a.render_tile>=0?a.render_tile+0x100:party_combat_tile(cls));}const int at=a.position.y*11+a.position.x;s.tiles[at]=int16_t(tile);s.actor_ids[at]=0x40000U+uint32_t(a.id);s.actor_seeds[at]=uint8_t(tile&0xfc);}
    if(c.current>=0&&c.current<c.count){const auto&a=c.actors[c.current];if(active(a)&&a.position.x>=0&&a.position.y>=0&&a.position.x<11&&a.position.y<11){s.active_x=int8_t(a.position.x);s.active_y=int8_t(a.position.y);s.active_enemy=a.member==255||a.charmed;}}
    for(int i=0;i<kPresentationCells;++i){const auto k=tile_animation_kind(s.tiles[i]);s.animated[i]=k==TileAnimationKind::TileCycle||k==TileAnimationKind::WaterScroll||k==TileAnimationKind::WaterComposite||k==TileAnimationKind::FireNoise||k==TileAnimationKind::ActorProgram;s.any_animated|=s.animated[i]!=0;}
    return s;
}

int32_t combat_loot_render_tile(int16_t encoded_loot){
    if(encoded_loot<=0)return kPresentationOffMap;
    return 0x100+(encoded_loot&0x7f);
}

} // namespace openu5
