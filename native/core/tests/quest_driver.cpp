#include "openu5/quest.h"
#include "openu5/persistence.h"
#include "openu5/shrine.h"
#include "openu5/quest_world.h"
#include "openu5/blackthorn.h"
#include "openu5/dungeon.h"
#include "quest_combat_fixture.h"
#include "quest_theft_watchdog.h"
#include <fstream>
#include <iostream>
#include <memory>
using namespace openu5;
using openu5::save::Json;
struct WorldFixture { Json objects; std::vector<uint8_t> tiles,live_tiles,town_tiles;GameState *game=nullptr; int side=256; std::vector<std::string> records,misc,narration,karma; };
static const char *plot_names[]={"amulet","crown","sceptre","wooden-box","carpet","shard-falsehood","shard-hatred","shard-cowardice"};
static QuestObject read_object(void *p,size_t i) {
    const auto &o=static_cast<WorldFixture *>(p)->objects.values[i];QuestObject v;
    v.location=int32_t(o["location"].integer());v.floor=int32_t(o["floor"].integer());v.x=int32_t(o["x"].integer());v.y=int32_t(o["y"].integer());v.tile=int32_t(o["tile"].integer());v.plot_z=int32_t(o["plotZ"].integer());v.plot=o["kind"].string==u"plot";v.shadowlord=o["kind"].string==u"shadowlord";
    v.chest=o["kind"].string==u"chest";v.prop=o["kind"].string==u"prop";v.contents=int32_t(o["contents"].integer());v.trapped=o["trapped"].truth();v.search=o["kind"].string==u"search";v.loot=o["kind"].string==u"loot";v.item_id=int32_t(o[v.search?"search":"loot"]["id"].integer());v.quality=int32_t(o[v.search?"search":"loot"][v.search?"quality":"qty"].integer());for(int j=0;j<8;++j)if(o["plotItem"].string==Json(plot_names[j]).string)v.item=PlotItem(j);return v;
}
static void append_object(void *p,const QuestObject &o) {
    Json v=Json::object();v["location"]=Json(o.location);v["floor"]=Json(o.floor);v["x"]=Json(o.x);v["y"]=Json(o.y);v["tile"]=Json(o.tile);v["kind"]=Json(o.plot?"plot":"shadowlord");
    if(o.chest){v["kind"]=Json("chest");v["contents"]=Json(o.contents);v["trapped"]=Json(o.trapped);}if(o.prop)v["kind"]=Json("prop");if(o.search){v["kind"]=Json("search");v["search"]=Json::object();v["search"]["id"]=Json(o.item_id);v["search"]["quality"]=Json(o.quality);}if(o.plot){v["plotItem"]=Json(plot_names[int(o.item)]);if(o.plot_z>=0)v["plotZ"]=Json(o.plot_z);}static_cast<WorldFixture *>(p)->objects.values.push_back(std::move(v));
}
int main(int argc,char **argv) {
    if (argc!=3) return 2;
    std::ifstream in(argv[1]); std::ofstream out(argv[2]); std::string line;
    while (std::getline(in,line)) {
        Json q,r=Json::object();
        if (save::parse_json(line,q)!=save::JsonError::None) return 3;
        auto owned=std::make_unique<GameState>(); GameState &g=*owned; TurnState t;
        Json retained=q["state"];
        if (save::restore_core(q["state"],g,t)!=save::Error::None) { std::cerr<<"restore failed\n"; return 4; }
        auto n=[&](const char *key){return int32_t(q[key].integer());};
        auto lines=[&](const QuestLines &l){r["lines"]=Json::array(); for(uint8_t i=0;i<l.count;++i) r["lines"].values.emplace_back(l.lines[i]);};
        const auto op=q["op"].string;
        if (op==u"world-flow") {
            WorldFixture fixture;fixture.game=&g;Json &objects=fixture.objects;objects=retained["worldObjects"];if(objects.kind!=Json::Array)objects=Json::array();
            QuestWorldServices services;services.context=&fixture;for(const auto &v:q["karma"].values)fixture.karma.push_back("\""+QuestCombatFixture::text(v)+"\"");services.karma_record=[](void *p,int32_t i)->const char *{auto &v=static_cast<WorldFixture *>(p)->karma;return i>=0&&size_t(i)<v.size()?v[i].c_str():nullptr;};
            services.count=[](void *p){return static_cast<WorldFixture *>(p)->objects.values.size();};services.read=read_object;
            services.reserve=[](void *p,size_t n){auto &v=static_cast<WorldFixture *>(p)->objects.values;v.reserve(v.size()+n);return true;};services.append=append_object;
            services.erase=[](void *p,size_t i){auto &v=static_cast<WorldFixture *>(p)->objects.values;v.erase(v.begin()+ptrdiff_t(i));};
            std::vector<ShardSpawn> spawns;for(const auto &v:q["data"]["shardSpawns"].values)spawns.push_back({int32_t(v["x"].integer()),int32_t(v["y"].integer()),int32_t(v["z"].integer())});services.spawns=spawns.data();services.spawn_count=spawns.size();
            std::vector<SearchObject> search;for(const auto &v:q["data"]["searchObjects"].values)search.push_back({int32_t(v["id"].integer()),int32_t(v["quality"].integer()),int32_t(v["location"].integer()),int32_t(v["floor"].integer()),int32_t(v["x"].integer()),int32_t(v["y"].integer())});services.search_objects=search.data();services.search_count=search.size();
            std::vector<Moonstone> moons;for(const auto &m:retained["moonstones"].values)moons.push_back({uint8_t(m["x"].integer()),uint8_t(m["y"].integer()),int16_t(m["z"].integer()),uint8_t(m["location"].integer()),m["buried"].truth()});services.moonstones=moons.data();services.moonstone_count=moons.size();
            for(const auto &r:q["records"].values){std::string text;for(auto ch:r.string)text+=char(ch);fixture.records.push_back(text);}services.end_record=[](void *p,int32_t i)->const char *{auto &v=static_cast<WorldFixture *>(p)->records;return i>=0&&size_t(i)<v.size()?v[size_t(i)].c_str():nullptr;};
            if(q["endgameText"].kind==Json::Object){services.endgame_script=true;fixture.records.clear();for(const auto &v:q["endgameText"]["dialogue"].values)fixture.records.push_back(QuestCombatFixture::text(v));for(const auto &v:q["endgameText"]["narration"].values)fixture.narration.push_back(QuestCombatFixture::text(v));services.end_narration=[](void *p,int32_t i)->const char *{auto &v=static_cast<WorldFixture *>(p)->narration;return i>=0&&size_t(i)<v.size()?v[i].c_str():nullptr;};}std::vector<TalkText> words;for(const auto &v:q["data"]["wordsOfPower"].values)words.push_back(v.string);services.words={words.data(),words.size()};
            std::vector<uint8_t> xs,ys;for(const auto &v:q["data"]["locationsX"].values)xs.push_back(uint8_t(v.integer()));for(const auto &v:q["data"]["locationsY"].values)ys.push_back(uint8_t(v.integer()));
            TravelState travel;CommandState commands;WorldData world;auto &tiles=fixture.tiles;tiles.assign(65536,5);if(q["realOverworld"].truth()){std::ifstream mapfile("native/core/build-quests/overworld.bin",std::ios::binary);mapfile.read(reinterpret_cast<char *>(tiles.data()),65536);if(!mapfile)return 25;}fixture.side=g.position.map.location?32:256;for(size_t i=0;i<q["tiles"].values.size();++i)tiles[i]=uint8_t(q["tiles"].at(i).integer());fixture.live_tiles=tiles;MapData small{g.position.map,tiles.data(),1024};
            world.overworld=world.underworld=tiles.data();world.overworld_size=world.underworld_size=tiles.size();world.small_maps=&small;world.small_map_count=1;MapData mapset[2]={small,{}};if(q["townTiles"].kind==Json::Array){for(auto &v:q["townTiles"].values)fixture.town_tiles.push_back(uint8_t(v.integer()));mapset[1]={{17,0},fixture.town_tiles.data(),fixture.town_tiles.size()};world.small_maps=mapset;world.small_map_count=2;}
            CommandContext ctx{g,t,travel,commands,world};ctx.quest_world=&services;std::vector<int32_t> phases;for(auto &v:q["data"]["moonPhases"].values)phases.push_back(int32_t(v.integer()));SkyRefresh sky{phases.data(),phases.size(),g.position.map.location};if(!phases.empty()){services.moon_phases=phases.data();services.moon_phase_count=phases.size();ctx.sky=&sky;}ctx.locations={xs.data(),ys.data(),xs.size(),ys.size()};g.rng.seed(n("seed"));
            auto dungeon_state=std::make_unique<DungeonState>();auto dungeon_scratch=std::make_unique<DungeonScratch>();DungeonContext dungeon_context{*dungeon_state,*dungeon_scratch};
            if(q["dungeon"].kind==Json::Object){const auto &d=q["dungeon"];ctx.dungeon=true;ctx.dungeon_context=&dungeon_context;dungeon_state->active=true;dungeon_state->pos={40,uint8_t(d["floor"].integer()),uint8_t(d["x"].integer()),uint8_t(d["y"].integer()),DungeonFacing(d["facing"].integer())};for(size_t i=0;i<512;++i)dungeon_state->cells[i]=uint8_t(d["cells"].at(i).integer());}
            BlackthornSession blackthorn;ShrineSession shrine_session;ShrineData shrine_data;shrine_data.count=8;for(size_t i=0;i<8;++i){shrine_data.x[i]=int32_t(q["data"]["shrineX"].at(i).integer());shrine_data.y[i]=int32_t(q["data"]["shrineY"].at(i).integer());shrine_data.mantras[i]=q["data"]["mantras"].at(i).string;shrine_data.virtues[i]=q["data"]["virtues"].at(i).string;}
            for(const auto &r:q["misc"].values){std::string text;for(auto ch:r.string)text+=char(ch);fixture.misc.push_back(text);}
            ShrineServices shrine_services{shrine_session,&shrine_data,&fixture,[](void *p,int32_t i)->const char *{auto &r=static_cast<WorldFixture *>(p)->misc;return i>=0&&size_t(i)<r.size()?r[size_t(i)].c_str():nullptr;}};
            if(q.has("misc")){ctx.blackthorn=&blackthorn;ctx.shrine_services=&shrine_services;}
            services.tile_at=[](void *p,int32_t x,int32_t y){auto &f=*static_cast<WorldFixture *>(p);if(!f.town_tiles.empty()&&f.game->position.map.location==17){if(x<0||y<0||x>=32||y>=32)return -1;return int32_t(f.town_tiles[y*32+x]);}if(f.side==256){x&=255;y&=255;}if(x<0||y<0||x>=f.side||y>=f.side)return -1;return int32_t(f.live_tiles[size_t(y*f.side+x)]);};services.volatile_tile=[](void *p,int32_t x,int32_t y,int32_t tile){auto &f=*static_cast<WorldFixture *>(p);if(x>=0&&y>=0&&x<f.side&&y<f.side)f.live_tiles[size_t(y*f.side+x)]=uint8_t(tile);};
            std::vector<NpcSlot> slots;for(const auto &v:q["npcs"].values){NpcSlot n;n.slot=uint8_t(v["slot"].integer());n.type=uint8_t(v["type"].integer());n.dialog=uint8_t(v["dialogNumber"].integer());for(int i=0;i<3;++i){n.ai[i]=uint8_t(v["aiTypes"].at(i).integer());n.x[i]=uint8_t(v["x"].at(i).integer());n.y[i]=uint8_t(v["y"].at(i).integer());n.z[i]=uint8_t(v["z"].at(i).integer());}for(int i=0;i<4;++i)n.times[i]=uint8_t(v["times"].at(i).integer());slots.push_back(n);}
            NpcActors actors;auto npc_scratch=std::make_unique<NpcScanGrid>();NpcLocationData npcdata{g.position.map.location,slots.data(),slots.size()};
            if(!slots.empty()){enter_npc_map(actors,slots.data(),slots.size(),g.position.map.location,uint8_t(g.time.hour),g.npc_dead[g.position.map.location-1]);ctx.actors=&actors;ctx.npc_scratch=npc_scratch.get();ctx.npc_data=&npcdata;ctx.npc_data_count=1;}
            if(q["here"].kind==Json::Number)travel.shadowlord_here=int8_t(q["here"].integer());
            QuestCombatFixture combat_assets;auto battle=std::make_unique<CombatState>();CombatContext combat{g,t,*battle};CombatField combat_fields[16];CombatActor actor_overflow[128];battle->actors.overflow=actor_overflow;battle->actors.overflow_capacity=128;
            if(q["combat"].kind==Json::Object){combat_assets.load(q["combat"]);services.combat_resources=&combat_assets.resources;services.encounter=&combat;combat.tables=combat_assets.resources.tables;combat.enemy_defs=combat_assets.enemy_ptrs.data();combat.enemy_def_count=combat_assets.enemy_ptrs.size();}
            r["steps"]=Json::array();
            for(const auto &a:q["actions"].values){
                Json step=Json::object();step["events"]=Json::array();step["combatEvents"]=Json::array();ctx.events={&step["events"],[](void *p,const GameEvent &e){
                    Json v=Json::object();const char *kind=e.kind==GameEventKind::Message?"message":e.kind==GameEventKind::Sfx?"sfx":e.kind==GameEventKind::Quake?"quake":e.kind==GameEventKind::MapChanged?"map-changed":e.kind==GameEventKind::PartyChanged?"party-changed":e.kind==GameEventKind::CellExplosion?"cell-explosion":e.kind==GameEventKind::GameWon?"game-won":e.kind==GameEventKind::Endgame?"endgame":e.kind==GameEventKind::CombatStarted?"combat-started":e.kind==GameEventKind::CombatEnded?"combat-ended":e.kind==GameEventKind::WalkEcho?"walk-echo":e.kind==GameEventKind::Moved?"moved":e.kind==GameEventKind::ShrineKeyWait?"shrine-key-wait":e.kind==GameEventKind::ShrineVisitPrompt?"shrine-visit-prompt":e.kind==GameEventKind::Refuge?"refuge":e.kind==GameEventKind::GuardPasswordPrompt?"blackthorn-guard-password-prompt":e.kind==GameEventKind::GuardTributePrompt?"guard-tribute-prompt":e.kind==GameEventKind::GuardArrestPrompt?"guard-arrest-prompt":e.kind==GameEventKind::BlackthornPrompt?"blackthorn-interrogation-prompt":"unknown";
                    if(e.kind==GameEventKind::GuardTributePrompt){v["charity"]=Json(e.note<0);if(e.note>=0)v["toll"]=Json(e.note);}
                    v["kind"]=Json(kind);v["text"]=Json(e.text?e.text:"");if(e.kind==GameEventKind::Sfx && e.text && std::string(e.text)=="instrument-note")v["note"]=Json(e.note);
                    if(e.refuge){v["refuge"]=Json::object();v["refuge"]["beats"]=Json::array();for(auto &beat:e.refuge->beats){Json b=Json::object();if(beat.scene)b["scene"]=Json(beat.scene);if(beat.message)b["message"]=Json(beat.message);if(beat.sfx){b["sfx"]=Json::object();b["sfx"]["id"]=Json(beat.sfx);}if(beat.delay>=0)b["delayUnits"]=Json(beat.delay);v["refuge"]["beats"].values.push_back(std::move(b));}}
                    if(e.endgame){v["endgame"]=Json::object();auto &script=v["endgame"];script["ending"]=Json(e.endgame->victory?"victory":"stranded");script["beats"]=Json::array();for(int i=0;i<e.endgame->count;++i){auto &beat=e.endgame->beats[i];Json b=Json::object();b["phase"]=Json(beat.phase);if(beat.message)b["message"]=Json(beat.message);if(beat.reply)b["reply"]=Json(beat.reply);if(beat.delay>=0)b["delayUnits"]=Json(beat.delay);if(beat.page>=0)b["page"]=Json(beat.page);script["beats"].values.push_back(std::move(b));}}
                    if(e.kind==GameEventKind::CellExplosion){v["cellFx"]=Json::object();auto &fx=v["cellFx"];fx["dx"]=Json(e.cell_fx.dx);fx["dy"]=Json(e.cell_fx.dy);fx["bursts"]=Json(e.cell_fx.bursts);fx["preDelayUnits"]=Json(e.cell_fx.pre_delay_units);fx["underTile"]=Json(e.cell_fx.under_tile);}static_cast<Json *>(p)->values.push_back(std::move(v));
                }};
                combat.events={&step["combatEvents"],[](void *p,const CombatEvent &e){const char *kinds[]={"message","echo","moved","attacked","died","turn","ended","projectile","quake","line-spray"};Json v=Json::array();v.values.emplace_back(kinds[int(e.kind)]);v.values.emplace_back(e.text?e.text:"");for(int n:{e.actor,e.target,e.x,e.y,e.damage})v.values.emplace_back(n);static_cast<Json *>(p)->values.push_back(std::move(v));}};
                auto kind=a["kind"].string;Command cmd;cmd.item=int16_t(a["item"].integer());
                if(kind==u"hydrate"){if(!hydrate_underworld_plot(g,services))return 12;}
                else if(kind==u"fixed"){auto &map=combat_assets.maps[0];uint8_t sprites[16]{};auto &units=q["combat"]["combatMaps"].at(0)["units"];for(size_t i=0;i<units.values.size();++i)sprites[i]=uint8_t(units.at(i)["sprite"].integer());FixedCombatSetup fixed{sprites,7,combat_fields,16};if(start_fixed_combat(ctx,combat,map,fixed,CombatDirection::South,true)!=CombatResult::Ok)return 21;}
                else if(kind==u"battle-fight"){for(int tick=0;tick<(a.has("repeat")?a["repeat"].integer():1);++tick){auto *u=current_combat_actor(combat);if(u){CombatResult result;if(u->member==255)result=combat_action(combat,CombatAction::EnemyStep);else{CombatActor *target=nullptr;int best=999;for(int i=0;i<battle->count;++i){auto &e=battle->actors[i];if(e.member!=255||(e.status!=CombatStatus::Active&&e.status!=CombatStatus::Sleeping))continue;int dist=std::max(std::abs(int(e.position.x)-u->position.x),std::abs(int(e.position.y)-u->position.y));if(dist<best||(dist==best&&target&&e.id<target->id)){target=&e;best=dist;}}if(!target)result=combat_action(combat,CombatAction::Pass);else if(best<=u->range)result=combat_action(combat,CombatAction::Attack,target->position.x,target->position.y);else result=combat_action(combat,CombatAction::Move,target->position.x!=u->position.x?(target->position.x>u->position.x?0:1):(target->position.y>u->position.y?2:3));}if(result!=CombatResult::Ok)return 28;}}}
                else if(kind==u"battle-hold"){auto *actor=current_combat_actor(combat);if(actor && combat_action(combat,actor->member!=255?CombatAction::Pass:CombatAction::EnemyStep)!=CombatResult::Ok)return 27;}
                else if(kind==u"battle-step"){auto *actor=current_combat_actor(combat);if(actor){if(combat_action(combat,actor->member!=255?CombatAction::Move:CombatAction::EnemyStep,2)!=CombatResult::Ok)return 26;}}else if(kind==u"battle-sceptre"){use_quest_item(ctx,20,ctx.events);}else if(kind==u"battle-move"||kind==u"battle-pass"||kind==u"battle-enemy"){auto action=kind==u"battle-move"?CombatAction::Move:kind==u"battle-pass"?CombatAction::Pass:CombatAction::EnemyStep;if(combat_action(combat,action,int32_t(a["dir"].integer()))!=CombatResult::Ok)return 22;}
                else if(kind==u"battle-end"){if(ctx.combat&&finish_encounter_combat(ctx,*battle)!=CombatResult::Ok)return 23;}
                else if(kind==u"doom"){if(doom_entrance(ctx,40,ctx.events).status==CommandStatus::InvalidContext)return 18;}
                else if(kind==u"npc-attack"){bool found=false;for(size_t i=0;i<actors.count;++i)if(actors.actors[i].schedule.slot==a["slot"].integer()){found=true;if(town_attack_commit(ctx,actors.actors[i],true,ctx.events)==CommandStatus::InvalidContext)return 19;break;}if(!found)return 20;}
                else if(kind==u"interior"){if(!hydrate_interior_objects(ctx,g.position.map.location))return 16;} else if(kind==u"urban"){if(urban_shadowlord(ctx,ctx.events,{&g,[](void *p,int32_t lo,int32_t hi){return static_cast<GameState *>(p)->rng.next(lo,hi).value;}})!=CommandStatus::Success)return 17;} else if(kind==u"shrine-answer"){TalkText mantras[3]={a["mantra"].string,a["mantra"].string,a["mantra"].string};ShrineInput input{ShrineAction::SubmitVisit,0,a["word"].string,{mantras,3}};execute_shrine(ctx,input);}else if(kind==u"refuge-check"){if(check_refuge(ctx,ctx.events)==CommandStatus::InvalidContext)return 24;}else if(kind==u"refuge-resolve"){resolve_refuge(ctx,ctx.events);}else if(kind==u"check-guard"){blackthorn_turn_effect(ctx,g.position.map.location==18?CommandEffect::Capture:CommandEffect::Tribute,ctx.events,rng_source(g.rng));}else if(kind==u"password"||kind==u"tribute"||kind==u"arrest"){blackthorn_action(ctx,kind==u"password"?BlackthornAction::Password:kind==u"tribute"?BlackthornAction::Tribute:BlackthornAction::Arrest,a["word"].string,a["agree"].truth(),ctx.events,rng_source(g.rng));}else if(kind==u"capture"||kind==u"answer"){cmd.kind=CommandKind::BlackthornAction;cmd.item=kind==u"capture"?0:1;cmd.text=a["word"].string.data();cmd.text_length=a["word"].string.size();auto ar=execute_command(ctx,cmd);if(ar.status==CommandStatus::InvalidContext)return 15;}
                else if(kind==u"absorb"){if(absorption_endgame(ctx,ctx.events)==CommandStatus::InvalidContext)return 14;}
                else {cmd.kind=kind==u"move"?CommandKind::Move:kind==u"enter"?CommandKind::Enter:kind==u"yell"?CommandKind::Yell:kind==u"note"?CommandKind::HarpsichordNote:kind==u"search"?CommandKind::Search:kind==u"get"?CommandKind::Get:kind==u"moon"?CommandKind::UseMoonstone:CommandKind::UseItem;cmd.has_direction=a.has("dir");cmd.direction=Direction(a["dir"].integer());cmd.text=a["word"].string.data();cmd.text_length=a["word"].string.size();auto ar=execute_command(ctx,cmd);if(ar.status==CommandStatus::InvalidContext || ar.status==CommandStatus::CoreError){std::cerr<<"world command rejected: "<<save::write_json(q)<<"\n";return 13;}}
                if(!moons.empty()){retained["moonstones"]=Json::array();for(auto &m:moons){Json v=Json::object();v["x"]=Json(m.x);v["y"]=Json(m.y);v["z"]=Json(m.z);v["location"]=Json(m.location);v["buried"]=Json(m.buried);retained["moonstones"].values.push_back(std::move(v));}}
                if(q["dungeon"].kind==Json::Object){step["dungeonCells"]=Json::array();for(auto cell:dungeon_state->cells)step["dungeonCells"].values.emplace_back(cell);}
                Json state=retained;save::capture_core(g,t,state);state["worldObjects"]=objects;step["state"]=std::move(state);step["seed"]=Json(g.rng.get_seed());step["progress"]=Json(services.melody_progress);step["passage"]=Json(services.passage_open);if(services.combat_resources){step["combat"]=Json::object();auto &b=step["combat"];b["active"]=Json(ctx.combat);if(ctx.combat){b["seed"]=Json(battle->rng.get_seed());b["map"]=Json(battle->map.index);b["tiles"]=Json::array();for(auto tile:battle->map.tiles)b["tiles"].values.emplace_back(tile);b["absorbed"]=Json(battle->absorbed_any);b["current"]=Json(battle->current<0?-1:battle->actors[battle->current].id);b["actors"]=Json::array();for(int i=0;i<battle->count;++i){auto &a=battle->actors[i];Json v=Json::array();for(int n:{a.id,int(a.member),a.enemy?a.enemy->index:-1,int(a.position.x),int(a.position.y),a.hp,a.max_hp,a.strength,a.dexterity,a.intelligence,a.defense,a.attack,a.range,int(a.status),int(a.speed),int(a.counter)})v.values.emplace_back(n);b["actors"].values.push_back(std::move(v));}}}
                if(!services.combat_resources)step.erase("combatEvents");if(q["inspectTiles"].truth()){step["tiles"]=Json::array();for(int i=0;i<fixture.side*fixture.side;++i)step["tiles"].values.emplace_back(fixture.live_tiles[i]);}if(!slots.empty()){step["npcs"]=Json::array();for(size_t i=0;i<actors.count;++i){const auto &a=actors.actors[i];if(a.location!=g.position.map.location)continue;Json v=Json::object();v["slot"]=Json(a.schedule.slot);v["dialog"]=Json(a.schedule.dialog);v["ai"]=Json::array();for(auto x:a.schedule.ai)v["ai"].values.emplace_back(x);step["npcs"].values.push_back(std::move(v));}}
                r["steps"].values.push_back(std::move(step));
            }retained["worldObjects"]=std::move(objects);
        } else if (op==u"shrine-flow") {
            ShrineData data; data.count=8;
            for (size_t i=0;i<8;++i) {
                data.virtues[i]=q["data"]["virtues"].at(i).string; data.mantras[i]=q["data"]["mantras"].at(i).string;
                data.x[i]=int32_t(q["data"]["shrineX"].at(i).integer()); data.y[i]=int32_t(q["data"]["shrineY"].at(i).integer());
            }
            std::vector<std::string> records;
            for (const auto &v:q["records"].values) { std::string s; for (auto ch:v.string) s+=char(ch); records.push_back(s); }
            ShrineSession session;
            ShrineServices services{session,&data,&records,[](void *p,int32_t i)->const char * {
                const auto &v=*static_cast<std::vector<std::string> *>(p);
                return i>=0 && size_t(i)<v.size() ? v[size_t(i)].c_str() : nullptr;
            }};
            TravelState travel; CommandState commands; WorldData world;
            std::vector<uint8_t> map(65536,5);
            world.overworld=map.data(); world.overworld_size=map.size();
            world.underworld=map.data(); world.underworld_size=map.size();
            CommandContext ctx{g,t,travel,commands,world}; ctx.shrine_services=&services;
            r["steps"]=Json::array();
            for (const auto &a:q["actions"].values) {
                Json step=Json::object(); step["events"]=Json::array();
                ctx.events={&step["events"],[](void *p,const GameEvent &e) {
                    Json v=Json::object(); const char *kind="unknown";
                    switch(e.kind) {
                    case GameEventKind::Message:kind="message";break; case GameEventKind::PartyChanged:kind="party-changed";break;
                    case GameEventKind::MapChanged:kind="map-changed";break;case GameEventKind::Quake:kind="quake";break;
                    case GameEventKind::Sfx:kind="sfx";break;case GameEventKind::ShrineKeyWait:kind="shrine-key-wait";break;
                    case GameEventKind::ShrineVisitPrompt:kind="shrine-visit-prompt";break;
                    case GameEventKind::ShrineRestorePrompt:kind="shrine-restore-prompt";break;
                    case GameEventKind::ShrineDonatePrompt:kind="shrine-donate-prompt";break;
                    case GameEventKind::RitualInvert:kind="ritual-invert";break;default:break;
                    }
                    v["kind"]=Json(kind); v["text"]=Json(e.text ? e.text : "");
                    static_cast<Json *>(p)->values.push_back(std::move(v));
                }};
                const auto action=int32_t(a["action"].integer());
                if (action==5) shrine_guardian(g,ctx.events);
                else if (action==6) shrine_entry(g,services,int32_t(a["tile"].integer()),ctx.events);
                else if (action==7) {
                    map[size_t(g.position.xy.y)*256+g.position.xy.x]=uint8_t(a["tile"].integer());
                    Command cmd; cmd.kind=CommandKind::Enter;
                    const auto result=execute_command(ctx,cmd);
                    if (result.status==CommandStatus::InvalidContext || result.status==CommandStatus::Unsupported || result.status==CommandStatus::CoreError) return 11;
                }
                else {
                    std::vector<TalkText> mantras; for (const auto &m:a["mantras"].values) mantras.push_back(m.string);
                    ShrineInput input{ShrineAction(action),int32_t(a["value"].integer()),a["virtue"].string,{mantras.data(),mantras.size()}};
                    Command cmd; cmd.kind=CommandKind::ShrineAction; cmd.shrine=&input;
                    const auto result=execute_command(ctx,cmd);
                    if (result.status==CommandStatus::InvalidContext || result.status==CommandStatus::Unsupported) return 10;
                }
                step["visit"]=Json(session.visit); step["restore"]=Json(session.restore);
                Json state=q["state"]; save::capture_core(g,t,state); step["state"]=std::move(state);
                r["steps"].values.push_back(std::move(step));
            }
        } else if (op==u"destroy") { const auto a=destroy_shadowlord(g,n("i")); r["ok"]=Json(a.ok); r["message"]=Json("");r["message"].string=a.message; }
        else if (op==u"grant") r["message"]=Json(grant_plot_item(g,PlotItem(n("item"))));
        else if (op==u"ritual") {
            const auto a=cast_shard_into_flame(n("i"),n("x"),n("y"),n("location"),n("floor"),n("above"),n("summoned"));
            lines(a.text); r["destroyed"]=Json(a.destroyed); r["doomBit"]=Json(a.doom_bit);
        } else if (op==u"summon") {
            bool alive[3]; for(int i=0;i<3;++i) alive[i]=q["alive"].at(size_t(i)).truth();
            const auto i=summon_shadowlord(q["word"].string,n("y"),alive,q["present"].truth());
            r["idx"]=Json(i);
        } else if (op==u"word") {
            std::vector<TalkText> words; std::vector<int32_t> adjacent;
            for (const auto &v:q["words"].values) words.push_back(v.string);
            for (const auto &v:q["adjacent"].values) adjacent.push_back(int32_t(v.integer()));
            const auto a=yell_word_of_power({words.data(),words.size()},q["word"].string,{adjacent.data(),adjacent.size()});
            lines(a.text); r["uttered"]=Json(a.uttered); r["opened"]=Json(a.opened); r["location"]=Json(a.location);
        } else if (op==u"rescue") {
            r["ready"]=Json(endgame_ready(g)); r["ending"]=Json(int(rescue_lord_british(g,q["absorption"].truth())));
        } else if (op==u"melody") { const auto a=advance_melody(n("progress"),n("digit")); r["progress"]=Json(a.progress); r["complete"]=Json(a.complete); }
        else if (op==u"donate") { const auto a=shrine_donate(g,n("cycles")); r["accepted"]=Json(a.accepted); r["cost"]=Json(a.cost); }
        else if (op==u"codex") { const auto a=shrine_codex_lesson(g); r["virtue"]=Json(a.virtue); r["ceremony"]=Json(a.ceremony); }
        else if (op==u"shrine") {
            r["mode"]=Json(int(shrine_mode(g,uint8_t(n("v")))));
            if (n("action")==0) shrine_show_mantra(g,uint8_t(n("v")));
            else r["attrs"]=Json(shrine_complete_quest(g,uint8_t(n("v")),g.party.characters[0]));
        } else if (op==u"check" || op==u"restore") {
            ShrineData d; d.count=8;
            for(size_t i=0;i<8;++i) { d.virtues[i]=q["data"]["virtues"].at(i).string; d.mantras[i]=q["data"]["mantras"].at(i).string;
                d.x[i]=int32_t(q["data"]["shrineX"].at(i).integer()); d.y[i]=int32_t(q["data"]["shrineY"].at(i).integer()); }
            std::vector<TalkText> mantras; for(const auto &v:q["mantras"].values) mantras.push_back(v.string);
            const TalkView<TalkText> m{mantras.data(),mantras.size()};
            r["ok"]=Json(op==u"check" ? shrine_visit_check(uint8_t(n("v")),q["virtue"].string,m,d) :
                shrine_restore(g,uint8_t(n("v")),q["virtue"].string,m,n("x"),n("y"),d));
        } else if (op==u"theft") {
            g.rng.seed(n("seed"));
            // The theft re-roll (TALK.OVL 0x11c7) is unbounded on both parity sides;
            // the OBSERVATION is bounded. See tests/quest_theft_watchdog.h -- that
            // escape must stay an unwind, never a longjmp out of this frame.
            const auto observed=openu5_test::observe_faulinei_theft(g,n("here"));
            if (observed.nonterminating) { r["nonterminating"]=Json(true); }
            else {
                const auto &a=observed.result;
                r["kind"]=Json(int(a.kind)); r["index"]=Json(a.index); r["amount"]=Json(a.amount);
            }
            r["seed"]=Json(g.rng.get_seed());
        } else if (op==u"playtime") { const auto a=endgame_playtime(g); r["years"]=Json(a.years); r["months"]=Json(a.months); r["days"]=Json(a.days); }
        else if (op==u"native-roundtrip") {
            save::Gam base{},gam{}; Json side,imported;
            if (save::export_native_state(g,t,retained,base.data(),base.size(),gam,side)!=save::Error::None) return 7;
            if (save::import_native(gam.data(),gam.size(),side,imported)!=save::Error::None) return 8;
            if (save::restore_core(imported,g,t)!=save::Error::None) return 9;
            retained=std::move(imported);
        } else if (op!=u"roundtrip") return 5;
        save::capture_core(g,t,retained); r["state"]=std::move(retained);
        out<<save::write_json(r)<<'\n';
    }
    std::cerr<<"GameState="<<sizeof(GameState)<<" QuestState="<<sizeof(QuestState)<<'\n';
    return in.bad() || !out ? 6 : 0;
}



