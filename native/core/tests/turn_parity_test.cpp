#include "openu5/actors.h"
#include <vector>
#include <iostream>
#include <cstring>
#include <cstdlib>
using namespace openu5;
#include "../fixtures/turns.inc"
using Wire = std::vector<int64_t>;
struct Reader { const int64_t *p; int64_t get() { return *p++; } };
static void add(Wire &w, std::initializer_list<int64_t> values) { w.insert(w.end(),values); }
static void read_state(Reader &r, GameState &g, TurnState &s) {
    g.time = {int32_t(r.get()),int32_t(r.get()),int32_t(r.get()),int32_t(r.get()),int32_t(r.get())};
    g.position.map = {uint8_t(r.get()),int16_t(r.get())}; g.position.xy = {uint8_t(r.get()),uint8_t(r.get())};
    g.party.party_size=int32_t(r.get());g.party.active_character=uint8_t(r.get());g.food=uint16_t(r.get());g.torch_turns=uint16_t(r.get());g.turns_since_start=r.get();
    s.prev_hour=int32_t(r.get());s.light_spell_minutes=int32_t(r.get());s.drunk_turns=int32_t(r.get());s.time_spell=char(r.get());s.spell_turns=int32_t(r.get());
    g.transport=TransportMode(r.get());s.transport_tile=int32_t(r.get());s.wind=int32_t(r.get());s.wind_drift_counter=int32_t(r.get());s.has_shadowlords=r.get()!=0;
    for(auto &v:s.shadowlord_locations)v=int32_t(r.get());s.skull_tree_day=int32_t(r.get());for(auto &v:s.reagent_days)v=int32_t(r.get());
    s.felucca_phase=int32_t(r.get());s.trammel_phase=int32_t(r.get());g.party.character_count=uint8_t(r.get());
    for(size_t i=0;i<g.party.character_count;i++){auto &c=g.party.characters[i];c.status=char(r.get());c.current_hp=uint16_t(r.get());c.max_hp=uint16_t(r.get());c.ring=uint8_t(r.get());c.strength=uint8_t(r.get());c.dexterity=uint8_t(r.get());c.months_at_inn=uint8_t(r.get());}
    g.rng.seed(int32_t(r.get()));
}
static Wire state(const GameState &g,const TurnState &s) {
    const auto &t=g.time; const auto &p=g.position;
    Wire w={t.year,t.month,t.day,t.hour,t.minute,p.map.location,p.map.floor,p.xy.x,p.xy.y,g.party.party_size,g.party.active_character,g.food,g.torch_turns,g.turns_since_start,
        s.prev_hour,s.light_spell_minutes,s.drunk_turns,s.time_spell,s.spell_turns,int(g.transport),s.transport_tile,s.wind,s.wind_drift_counter,s.has_shadowlords};
    for(auto v:s.shadowlord_locations)w.push_back(v);w.push_back(s.skull_tree_day);for(auto v:s.reagent_days)w.push_back(v);
    add(w,{s.felucca_phase,s.trammel_phase,g.party.character_count});
    for(size_t i=0;i<g.party.character_count;i++){const auto &c=g.party.characters[i];add(w,{c.status,c.current_hp,c.max_hp,c.ring,c.strength,c.dexterity,c.months_at_inn});}
    w.push_back(g.rng.get_seed());return w;
}
static void indices(Wire &w,const MemberIndices &m){w.push_back(m.count);for(size_t i=0;i<m.count;i++)w.push_back(m.values[i]);}
static void result(Wire &w,const TurnResult &r){
    w.push_back(r.message_count);for(size_t i=0;i<r.message_count;i++)w.push_back(int(r.messages[i]));
    indices(w,r.poisoned);indices(w,r.poison_ticks);w.push_back(r.has_spawn);
    if(r.has_spawn)add(w,{r.spawn.roll,r.spawn.threshold,r.spawn.spawn});
    w.push_back(r.has_troll);if(r.has_troll){const auto &b=r.troll;add(w,{b.fired,b.on_foot,b.runs_inner_world_turn,b.payer_index,b.toll,b.indices.count});for(size_t i=0;i<b.indices.count;i++)add(w,{b.indices.values[i],b.dex_rolls[i]});}
    add(w,{r.hazard,r.burning});
}
struct Probe {
    GameState *g; Wire trace,draws; int32_t hooks=0,falls=0,tile=0,trap_mode=0;
    Rand rand(){return {this,[](void *p,int32_t lo,int32_t hi){auto &q=*static_cast<Probe*>(p);auto v=q.g->rng.next(lo,hi).value;add(q.draws,{lo,hi,v});return v;}};}
    TraceSink sink(){return {this,[](void *p,const char *site,int32_t lo,int32_t hi,int32_t v){
        const char *names[]={"?","wind","troll","troll.wind","swamp","burnTick","burn","hazard","housekeeping","spawn","confusion","wake","swampTown","damageTick","wind2","hook"};
        int32_t id=-1;for(int32_t i=0;i<16;i++)if(std::strcmp(site,names[i])==0)id=i;
        add(static_cast<Probe*>(p)->trace,{id,lo,hi,v});
    }};}
    TurnHook hook(){return {this,[](void *p){auto &q=*static_cast<Probe*>(p);q.hooks++;q.rand()(0,31);}};}
};
static NpcSlot read_slot(Reader &r){NpcSlot s;s.slot=uint8_t(r.get());s.type=uint8_t(r.get());s.dialog=uint8_t(r.get());for(auto &v:s.ai)v=uint8_t(r.get());for(auto &v:s.x)v=uint8_t(r.get());for(auto &v:s.y)v=uint8_t(r.get());for(auto &v:s.z)v=uint8_t(r.get());for(auto &v:s.times)v=uint8_t(r.get());return s;}
static NpcActor read_actor(Reader &r){NpcActor n;n.schedule=read_slot(r);n.location=uint8_t(r.get());n.x=int16_t(r.get());n.y=int16_t(r.get());n.z=int16_t(r.get());n.state=uint8_t(r.get());n.served_slot=uint8_t(r.get());n.path_index=int16_t(r.get());n.stuck=int16_t(r.get());for(auto &v:n.path)v=uint8_t(r.get());return n;}
static void actor(Wire &w,const NpcActor &n){const auto &s=n.schedule;add(w,{s.slot,s.type,s.dialog});for(auto v:s.ai)w.push_back(v);for(auto v:s.x)w.push_back(v);for(auto v:s.y)w.push_back(v);for(auto v:s.z)w.push_back(v);for(auto v:s.times)w.push_back(v);add(w,{n.location,n.x,n.y,n.z,n.state,n.served_slot,n.path_index,n.stuck});for(auto v:n.path)w.push_back(v);}
static Wire actors(const NpcActors &list){Wire w={int64_t(list.count)};for(size_t i=0;i<list.count;i++)actor(w,list.actors[i]);return w;}
static void equal(const Wire &a,const int64_t *b,size_t n,size_t row,const char *what){
    if(a.size()!=n){std::cerr<<"Row "<<row<<" "<<what<<" length "<<a.size()<<" != "<<n<<'\n';std::exit(1);}
    for(size_t i=0;i<n;i++)if(a[i]!=b[i]){std::cerr<<"Row "<<row<<" "<<what<<" field "<<i<<": native "<<a[i]<<" TS "<<b[i]<<'\n';std::exit(1);}
}
int main(){
    GameState replay;TurnState supplemental;NpcActors live;OriginalRng npc_rng;
    size_t count=0;
    for(const auto &row:turn_fixture_rows){
        Reader r{turn_fixture_data+row[1]};const auto *expected=r.p+row[2];Wire out;
        switch(row[0]){
        case 0:{
            const auto sequence=r.get();const auto step=r.get(),mode=r.get(),flags=r.get();const int32_t tile=int32_t(r.get()),minutes=int32_t(r.get());TurnPhases p{int32_t(r.get()),int32_t(r.get())};
            if(step==0)read_state(r,replay,supplemental);else{auto w=state(replay,supplemental);equal(w,r.p,w.size(),count,"continuous turn input");r.p+=w.size();}
            auto &g=replay;auto &s=supplemental;Probe probe{&g};probe.tile=tile;probe.trap_mode=int32_t(sequence%24%4);
            std::array<int32_t,56> skyData{};for(size_t i=0;i<56;i++)skyData[i]=48+int32_t(i%8);
            SkyRefresh sky{skyData.data(),skyData.size(),flags&128?33:2};TurnResult res;
            if(mode==0){OutdoorTurnContext c;c.tile_under_party=tile;c.minutes=minutes;c.blocked=flags&1;c.on_bridge=flags&2;c.on_swamp=flags&4;c.skip_world_turn=flags&8;c.sky=&sky;c.after_wind=probe.hook();res=outdoor_turn(g,s,probe.rand(),c,probe.sink());}
            else if(mode==1){TownTurnContext c;c.consumes_turn=flags&1;c.confused=flags&2;c.pre_rolled=flags&4;c.damage_tile=flags&8;c.on_swamp_tile=flags&16;c.second_world_turn=flags&32;c.pass_command=flags&64;c.npc_phases=&p;c.sky=&sky;c.after_housekeeping=probe.hook();
                if(step%2){c.hazard_context=&probe;c.tile_under_party=[](void *v){auto &q=*static_cast<Probe*>(v);return q.falls<(q.trap_mode==0?20:2)?q.tile:143;};c.on_trapdoor=[](void *v){auto &q=*static_cast<Probe*>(v);q.falls++;if(q.trap_mode==1)return TrapdoorOutcome::None;if(q.trap_mode==2)return TrapdoorOutcome::PartyKilled;q.g->position.map.floor--;return TrapdoorOutcome::Fell;};}
                res=town_turn(g,s,probe.rand(),c,probe.sink());
            }else res=advance_turn(g,s,minutes,probe.rand(),&sky);
            out=state(g,s);add(out,{p.mount,p.quickness,probe.hooks,probe.falls});result(out,res);out.push_back(int64_t(probe.trace.size()));out.insert(out.end(),probe.trace.begin(),probe.trace.end());out.push_back(int64_t(probe.draws.size()));out.insert(out.end(),probe.draws.begin(),probe.draws.end());break;
        }
        case 1:{GameState g;TurnState s;s.transport_tile=int32_t(r.get());s.time_spell=char(r.get());g.transport=TransportMode(r.get());bool pass=r.get()!=0;TurnPhases town{int32_t(r.get()),int32_t(r.get())},outside{int32_t(r.get()),int32_t(r.get())};bool a=town_npc_tail_runs(s,town,pass),b=outdoor_world_turn_runs(g,s,outside);out={a,town.mount,town.quickness,b,outside.mount,outside.quickness};break;}
        case 2:{const auto hour=uint8_t(r.get());const auto floor=int16_t(r.get());auto n=read_actor(r);out.push_back(npc_check_schedule(n,hour,floor));actor(out,n);break;}
        case 3:{r.get();const auto seed=int32_t(r.get());r.get();r.get();const size_t n=size_t(r.get());std::array<NpcSlot,32> slots;for(size_t i=0;i<n;i++)slots[i]=read_slot(r);if(enter_npc_map(live,slots.data(),n,2,1,uint32_t(1)<<6)!=ActorError::None)return 2;npc_rng.seed(seed);out=actors(live);out.push_back(npc_rng.get_seed());break;}
        case 4:{const auto scenario=r.get();r.get();GameState g;g.time.hour=1;g.position.map={2,0};g.position.xy={uint8_t(r.get()),uint8_t(r.get())};const auto seed=r.get();const size_t n=size_t(r.get());auto prior=actors(live);equal(Wire(prior.begin()+1,prior.end()),r.p,n*59,count,"continuous NPC input");r.p+=n*59;if(seed!=npc_rng.get_seed())return 3;
            std::array<uint8_t,1024> tiles;for(int y=0;y<32;y++)for(int x=0;x<32;x++)tiles[size_t(y*32+x)]=scenario%4==3||(x==12&&y!=10)?0:scenario>=8&&x==11&&y==9?162:5;
            ActiveMap map{{2,0},MapKind::Small,{32,32,false},tiles.data(),5};if(tick_guards(live,g,map,rng_source(npc_rng))!=ActorError::None || tick_idle_npcs(live,g,map,rng_source(npc_rng))!=ActorError::None)return 4;out=actors(live);out.push_back(npc_rng.get_seed());break;}
        case 5:{const auto loc=int32_t(r.get()),floor=int32_t(r.get()),px=int32_t(r.get()),py=int32_t(r.get());auto entities=[&](){std::vector<PoolEntity> v(size_t(r.get()));for(auto &e:v)e={int32_t(r.get()),int32_t(r.get()),int32_t(r.get()),int32_t(r.get()),int32_t(r.get()),int32_t(r.get())};return v;};auto enemies=entities(),objects=entities();auto pool=compose_world_pool(loc,floor,enemies.data(),enemies.size(),objects.data(),objects.size());for(const auto &e:enemies)out.push_back(e.slot);for(const auto &o:objects)out.push_back(o.slot);uint32_t used=0;for(size_t i=0;i<32;i++){const auto &v=pool[i];add(out,{v.tile,v.x,v.y,v.floor,int(v.kind),int64_t(v.owner_index)});if(v.kind!=PoolOwnerKind::None)used|=uint32_t(1)<<i;}add(out,{acquire_actor_slot(pool,px,py),scan_recyclable_slot(pool,128,255,true,px,py),find_free_actor_slot(used),first_free_recycle_slot(used)});break;}
        case 6:{const int32_t minutes=int32_t(r.get());r.get();const auto variant=r.get();GameState g;TurnState s;read_state(r,g,s);std::array<int32_t,56> phases{};for(size_t i=0;i<56;i++)phases[i]=48+int32_t(i%8);SkyRefresh sky{phases.data(),variant%2?size_t(0):size_t(56),variant>=6?33:2};auto rand=rng_source(g.rng);advance_clock(g,s,minutes,variant&1?&rand:nullptr,variant&2?&sky:nullptr);out=state(g,s);break;}
        case 7:{const auto tile=int32_t(r.get()),floor=int32_t(r.get()),hour=int32_t(r.get());OriginalRng rng(int32_t(r.get()));const auto roll=roll_spawn_gate(rng_source(rng),tile,floor,hour);out={roll.roll,roll.threshold,roll.spawn,rng.get_seed()};break;}
        case 8:{const auto step=r.get();TurnPhases phases{int32_t(r.get()),int32_t(r.get())};
            if(!step)read_state(r,replay,supplemental);else{const auto w=state(replay,supplemental);equal(w,r.p,w.size(),count,"combined turn input");r.p+=w.size();}
            const auto n=size_t(r.get());if(!step){live.count=n;for(size_t i=0;i<n;i++)live.actors[i]=read_actor(r);}else{const auto w=actors(live);equal(Wire(w.begin()+1,w.end()),r.p,n*59,count,"combined actor input");r.p+=n*59;}
            std::array<uint8_t,1024> tiles;tiles.fill(5);ActiveMap map{{2,0},MapKind::Small,{32,32,false},tiles.data(),5};Probe probe{&replay};
            struct HookContext{NpcActors *list;GameState *g;ActiveMap *map;};HookContext hook{&live,&replay,&map};
            TownTurnContext c;c.consumes_turn=step%4!=0;c.second_world_turn=true;c.pass_command=step%3==0;c.npc_phases=&phases;
            c.after_housekeeping={&hook,[](void *p){auto &h=*static_cast<HookContext*>(p);auto rand=rng_source(h.g->rng);if(tick_guards(*h.list,*h.g,*h.map,rand)!=ActorError::None||tick_idle_npcs(*h.list,*h.g,*h.map,rand)!=ActorError::None)std::exit(8);}};
            auto res=town_turn(replay,supplemental,probe.rand(),c,probe.sink());out=state(replay,supplemental);add(out,{phases.mount,phases.quickness});const auto a=actors(live);out.insert(out.end(),a.begin(),a.end());result(out,res);out.push_back(int64_t(probe.trace.size()));out.insert(out.end(),probe.trace.begin(),probe.trace.end());break;}
        default:return 5;
        }
        if(r.p!=expected){std::cerr<<"Input schema length at row "<<count<<" consumed "<<(r.p-(turn_fixture_data+row[1]))<<" expected "<<row[2]<<'\n';return 6;}
        equal(out,expected,row[3],count,"output");++count;
    }
    // Explicit unsupported-path contract: no partial actor or RNG mutation.
    GameState g;g.position.map={2,0};g.time.hour=6;NpcActors n;n.count=1;n.actors[0].schedule.times[1]=6;n.actors[0].schedule.x[1]=10;
    const auto before=actors(n);OriginalRng rng(123);ActiveMap unused;
    if(tick_idle_npcs(n,g,unused,rng_source(rng))!=ActorError::NeedsPathfinding||actors(n)!=before||rng.get_seed()!=123)return 7;
    std::cout<<count<<" world-turn reference cases passed; unsupported walk atomicity passed. sizeof(GameState)="<<sizeof(GameState)<<", TurnState="<<sizeof(TurnState)<<", NpcActor="<<sizeof(NpcActor)<<", NpcActors="<<sizeof(NpcActors)<<", ActorPool="<<sizeof(ActorPool)<<'\n';
}
