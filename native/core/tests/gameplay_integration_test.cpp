#include "openu5/gameplay_save.h"
#include <cstdlib>
#include <iostream>
#include <memory>
using namespace openu5;
using save::Json;
static int checks=0;
static void check(bool ok){++checks;if(!ok){std::cerr<<"integration assertion "<<checks<<"\n";std::exit(1);}}
namespace {
struct ObjectPool { std::vector<QuestObject> items; };
size_t pool_count(void *p) { return static_cast<ObjectPool *>(p)->items.size(); }
QuestObject pool_read(void *p, size_t i) { auto &v = static_cast<ObjectPool *>(p)->items; return i < v.size() ? v[i] : QuestObject{}; }
bool pool_reserve(void *p, size_t n) { auto &v = static_cast<ObjectPool *>(p)->items; v.reserve(v.size() + n); return true; }
void pool_append(void *p, const QuestObject &o) { static_cast<ObjectPool *>(p)->items.push_back(o); }
void pool_erase(void *p, size_t i) { auto &v = static_cast<ObjectPool *>(p)->items; if (i < v.size()) v.erase(v.begin() + ptrdiff_t(i)); }
}
int main(){
    auto game=std::make_unique<GameState>();auto &g=*game;TurnState turn;TravelState travel;CommandState commands;OutdoorServices outdoor;
    std::vector<uint8_t> tiles(1024,68);tiles[32+2]=135;tiles[32*3+3]=72;tiles[32*3+4]=73;WorldData world;MapData map{{1,0},tiles.data(),tiles.size()};world.small_maps=&map;world.small_map_count=1;
    g.position={{10,10},{1,0}};g.time.hour=20;WorldTerrain terrain;terrain.refresh(world,g);
    check(terrain.raw(world,{1,0},2,2)==68&&terrain.effective(world,{1,0},2,2)==153);
    check(terrain.effective(world,{1,0},3,3)==3&&terrain.effective(world,{1,0},4,3)==3);
    g.position.xy={3,3};terrain.refresh(world,g);check(terrain.effective(world,{1,0},3,3)==72&&terrain.effective(world,{1,0},4,3)==73&&terrain.effective(world,{1,0},2,2)==153);
    terrain.set({1,0},2,2,292,true);terrain.set({1,0},2,2,186);check(terrain.raw(world,{1,0},2,2)==186&&terrain.effective(world,{1,0},2,2)==186);
    terrain.wiped=true;terrain.wipe_map={1,0};terrain.wipe_tile=143;check(terrain.effective(world,{1,0},2,2)==143);
    terrain.clear_residence();terrain.refresh(world,g);check(terrain.effective(world,{1,0},2,2)==292);
    terrain.remove_boarded({1,0},2,2,293);check(terrain.effective(world,{1,0},2,2)==292);
    terrain.remove_boarded({1,0},2,2,292);check(terrain.effective(world,{1,0},2,2)==153);
    g.time.hour=5;terrain.refresh(world,g);check(terrain.effective(world,{1,0},2,2)==68);
    terrain.set({1,-1},31,31,511,true);Json state=Json::object();save::capture_terrain(terrain,state);std::string buffer;check(save::encode_json(state,buffer)==save::JsonError::None);Json decoded;check(save::parse_json(buffer,decoded)==save::JsonError::None);WorldTerrain restored;check(save::restore_terrain(decoded,restored)==save::Error::None&&restored.persistent.size()==1&&restored.persistent[0].map.floor==-1);
    decoded["mapOverrides"]["1:0:256:1"]=Json(5);check(save::restore_terrain(decoded,restored)==save::Error::NativeDomain&&restored.persistent.size()==1);
    outdoor.enemies.push_back({31,8,300,255,0,true});outdoor.has_chunk_origin=true;outdoor.chunk_x=240;commands.door={{1,-1},31,30,186,4};save::capture_gameplay(commands,outdoor,state);
    CommandState c2;OutdoorServices o2;check(save::restore_gameplay(state,c2,o2)==save::Error::None&&c2.door.tile==186&&o2.enemies.size()==1&&o2.enemies[0].slot==31&&o2.chunk_x==240);
    state["overworldEnemies"].values[0]["x"]=Json(256);check(save::restore_gameplay(state,c2,o2)==save::Error::NativeDomain&&o2.enemies[0].x==255&&c2.door.turns==4);
    state["overworldEnemies"]=Json::array();state["openDoors"].values[0]["turnsLeft"]=Json(-1);check(save::restore_gameplay(state,c2,o2)==save::Error::NativeDomain&&o2.enemies.size()==1);
    CommandContext ctx{g,turn,travel,commands,world};check(world_transport_services(ctx).tile_at==nullptr);
    auto battle=std::make_unique<CombatState>();CombatEnemy enemy;enemy.name="Test";enemy.group_name="TEST";enemy.max_per_map=1;
    const CombatEnemy *defs[]{&enemy};CombatResources resources;resources.enemies=defs;resources.enemy_count=1;
    const auto seed=g.rng.get_seed();commands.door.turns=4;
    check(preflight_encounter_combat(ctx,*battle,resources,0,5)==CombatResult::MissingMap&&g.rng.get_seed()==seed&&commands.door.turns==4);
    check(start_encounter_combat(ctx,*battle,resources,0,5)==CombatResult::MissingMap&&!ctx.combat&&g.rng.get_seed()==seed&&commands.door.turns==4);
    auto arena=std::make_unique<CombatMap>();for(auto &tile:arena->tiles)tile=5;const CombatMap *maps[]{arena.get()};resources.maps=maps;resources.map_count=1;
    check(preflight_encounter_combat(ctx,*battle,resources,0,5)==CombatResult::Ok&&g.rng.get_seed()==seed&&commands.door.turns==4);
    arena->tiles[0]=-1;check(preflight_encounter_combat(ctx,*battle,resources,0,5)==CombatResult::Invalid&&g.rng.get_seed()==seed);

    // R-14: world objects (chests/loot/plot items/...) have no owner-side backing
    // store, so the save/load contract is capture-the-live-pool /
    // clear-then-restore, exactly as synchronize_loaded_world composes it below.
    {
        ObjectPool pool;QuestWorldServices qs;qs.context=&pool;qs.count=pool_count;qs.read=pool_read;qs.reserve=pool_reserve;qs.append=pool_append;qs.erase=pool_erase;
        QuestObject chest;chest.location=5;chest.floor=0;chest.x=10;chest.y=12;chest.tile=257;chest.chest=true;chest.contents=130;chest.trapped=true;
        QuestObject plotItem;plotItem.location=0;plotItem.floor=255;plotItem.x=105;plotItem.y=225;plotItem.tile=183;plotItem.plot=true;plotItem.item=PlotItem::Amulet;plotItem.plot_z=240;
        pool.items={chest,plotItem};
        Json doc=Json::object();save::capture_world_objects(qs,doc);
        std::string buf;check(save::encode_json(doc,buf)==save::JsonError::None);
        Json decoded;check(save::parse_json(buf,decoded)==save::JsonError::None);
        // Contaminate: a stale world (e.g. save B's live objects) must not survive a load.
        QuestObject stale;stale.location=99;stale.tile=999;
        pool.items={stale};
        pool.items.clear();// synchronize_loaded_world's objects_.clear() before restore
        check(save::restore_world_objects(decoded,qs)==save::Error::None);
        check(pool.items.size()==2);
        check(pool.items[0].location==5&&pool.items[0].chest&&pool.items[0].contents==130&&pool.items[0].trapped);
        check(pool.items[1].plot&&pool.items[1].item==PlotItem::Amulet&&pool.items[1].plot_z==240&&pool.items[1].floor==255);
        for(auto &o:pool.items)check(o.location!=99);
        // Domain-invalid entry: reject and leave the (already-cleared) pool empty rather than partially populated.
        decoded["worldObjects"].values[0]["tile"]=Json(99999);
        pool.items={stale};pool.items.clear();
        check(save::restore_world_objects(decoded,qs)==save::Error::NativeDomain);
        check(pool.items.empty());
        // Absent field (a save predating this fix): safe default is an empty pool, not a crash.
        Json noField=Json::object();
        pool.items={stale};pool.items.clear();
        check(save::restore_world_objects(noField,qs)==save::Error::None);
        check(pool.items.empty());
    }

    // R-15: an in-progress dungeon session (position/cells/revealed/wanderer)
    // round-trips through the sidecar; pos.floor/x/y/facing are validated
    // tightly because they are unguarded indices into the cell grid and the
    // facing-delta tables downstream (dungeon.cpp, quest_world.cpp).
    {
        DungeonState d;d.active=true;d.pos={35,3,4,6,DungeonFacing::East};d.quickness_toggle=1;
        for(size_t i=0;i<512;++i)d.cells[i]=uint8_t(i&0xff);
        for(size_t i=0;i<64;++i)d.revealed[i]=uint8_t(i*3);
        d.wanderer={2,0x16,5,3,3,0x60,true,255,255};
        Json dstate=Json::object();save::capture_dungeon(d,dstate);
        std::string dbuf;check(save::encode_json(dstate,dbuf)==save::JsonError::None);
        Json ddecoded;check(save::parse_json(dbuf,ddecoded)==save::JsonError::None);
        DungeonState restored;restored.active=true;restored.pos.dungeon=40;// stale prior session
        check(save::restore_dungeon(ddecoded,restored)==save::Error::None);
        check(restored.active&&restored.pos.dungeon==35&&restored.pos.floor==3&&restored.pos.x==4&&restored.pos.y==6&&restored.pos.facing==DungeonFacing::East);
        check(restored.quickness_toggle==1&&restored.cells[0]==0&&restored.cells[300]==uint8_t(300&0xff)&&restored.revealed[10]==uint8_t(30));
        check(restored.wanderer.type==0x16&&restored.wanderer.hidden&&restored.wanderer.bank==2);
        // Domain-invalid facing (would index the 4-entry direction-delta table
        // out of bounds downstream): rejected without mutating the live session...
        ddecoded["dungeon"]["facing"]=Json(9);
        DungeonState leftover;leftover.active=true;leftover.pos.dungeon=77;
        auto err=save::restore_dungeon(ddecoded,leftover);
        check(err==save::Error::NativeDomain&&leftover.pos.dungeon==77);
        // ...and synchronize_loaded_world's own fallback (`if(err!=None) d={}`) is what actually clears it.
        if(err!=save::Error::None)leftover=DungeonState{};
        check(!leftover.active);
        // Absent field (a surface save, the common case): resets to an inactive session directly.
        Json noDungeon=Json::object();
        DungeonState surfaceLoad;surfaceLoad.active=true;surfaceLoad.pos.dungeon=55;
        check(save::restore_dungeon(noDungeon,surfaceLoad)==save::Error::None&&!surfaceLoad.active);
    }

    std::cout<<checks<<" gameplay owner/domain assertions\n";
}
