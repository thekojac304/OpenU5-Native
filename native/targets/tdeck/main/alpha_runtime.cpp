#include "alpha_runtime.h"
#include "location_names.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <new>

#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "boot_trace.h"
#include "native_renderer.h"
#include "openu5/command_char.h"
#include "openu5/debug_labels.h"
#include "openu5/display_names.h"
#include "openu5/inventory_picker.h"
#include "openu5/loot.h"
#include "openu5/magic.h"
#include "openu5/persistence.h"
#include "openu5/rest.h"
#include "tdeck_board.h"

namespace tdeck {
namespace {
constexpr char kTag[]="AlphaRuntime";
// Batch 22 -- U5OBJ. The four Lord British's Castle basement cells the device
// report is about: the three authored chests and the (9,9) control object.
// Slots are matched by these coordinates in ANY schedule entry, never by an
// assumed slot number.
constexpr int32_t kU5ObjLocation=17,kU5ObjFloor=-1;
constexpr int32_t kU5ObjX[4]={16,17,13,9},kU5ObjY[4]={21,22,23,9};
int u5obj_cell(int32_t x,int32_t y){for(int i=0;i<4;++i)if(kU5ObjX[i]==x&&kU5ObjY[i]==y)return i;return -1;}
bool u5obj_tracked(const openu5::NpcSlot&n){for(int k=0;k<3;++k)if(u5obj_cell(n.x[k],n.y[k])>=0)return true;return false;}
bool u5obj_tracked(const openu5::QuestObject&o){return o.location==kU5ObjLocation&&u5obj_cell(o.x,o.y)>=0;}
const char *u5obj_heap(const void *p){return !p?"none":esp_ptr_external_ram(p)?"psram":esp_ptr_internal(p)?"internal":"other";}
void u5obj_object(const char *stage,size_t i,const openu5::QuestObject&o){
    ESP_LOGI(kTag,"U5OBJ %s_OBJ index=%u loc=%ld floor=%ld x=%ld y=%ld tile=%ld chest=%d prop=%d plot=%d item=%d loot=%d search=%d shadowlord=%d trapped=%d contents=%ld slot=%ld",
             stage,unsigned(i),long(o.location),long(o.floor),long(o.x),long(o.y),long(o.tile),o.chest,o.prop,o.plot,int(o.item),o.loot,o.search,o.shadowlord,o.trapped,long(o.contents),long(o.slot));
}
constexpr uint32_t kInternal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT,kPsram=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
constexpr size_t kAstarBytes=323084,kTranscriptBlocks=96;
constexpr size_t kCreationWidth=320,kCreationHeight=152,kCreationPixels=kCreationWidth*kCreationHeight;
// #324 / R-32 -- the deferred Blackthorn capture turn. The longest real turn
// is the capture entry: 15 events and 50 scene beats; the escorted finale is
// 44 beats. The arena holds the copied narrative for one turn (the seven
// throne-room prints plus the interrogation question are well under 2 KiB).
constexpr size_t kBlackthornSceneSteps=96,kBlackthornSceneTextBytes=4096;
// Y-04 (Batch 7B). The worst real narrative turn plus headroom: the troll
// crossing is 1 + 6*5 + 1 = 32 beats and the refuge script is 16, and either
// can be followed by the rest of its own turn.
constexpr size_t kNarrativeSceneSteps=64,kNarrativeSceneTextBytes=2048;
// One run-n-frames unit (kernel 0x3ae6 / an INT 1Ch tick), the same 55 ms
// calibration the tile-animation tick and the reference pacers already use.
constexpr uint32_t kPresentationUnitMs=55;
constexpr int kVirtueX[8]={40,48,48,40,40,48,40,48};
constexpr int kVirtueY[8]={5,7,4,10,8,0,5,6};
constexpr int64_t kEnemyBeatUs=400000;
int active_member(const openu5::GameState &g){return g.party.active_character<g.party.character_count?g.party.active_character:0;}
const char *shop_item_name(openu5::ShopPhase phase,int id,const openu5::GameState &g){
    if(phase==openu5::ShopPhase::Buy||phase==openu5::ShopPhase::Sell||
       phase==openu5::ShopPhase::BuyDeal||phase==openu5::ShopPhase::SellDeal)
        return openu5::equipment_display_name(id);
    if(phase==openu5::ShopPhase::Reagent||phase==openu5::ShopPhase::ReagentDeal)
        return openu5::reagent_display_name(id);
    if(phase==openu5::ShopPhase::Guild||phase==openu5::ShopPhase::GuildDeal)
        return openu5::guild_item_display_name(id);
    if(phase==openu5::ShopPhase::Ship||phase==openu5::ShopPhase::ShipDeal)
        return openu5::ship_service_display_name(id);
    if(phase==openu5::ShopPhase::Wine)return openu5::wine_display_name(id);
    if((phase==openu5::ShopPhase::HealerMember||phase==openu5::ShopPhase::InnLeaveMember||
        phase==openu5::ShopPhase::InnPickupMember||
        phase==openu5::ShopPhase::HealerDeal||phase==openu5::ShopPhase::InnLeaveDeal||
        (phase>=openu5::ShopPhase::LegacyHeal&&phase<=openu5::ShopPhase::LegacyResurrect))&&
       id>=0&&id<g.party.character_count)return g.party.characters[id].name;
    if(phase==openu5::ShopPhase::HorseDeal)return "Horses";
    if(phase==openu5::ShopPhase::InnRestDeal)return "A Night's Rest";
    if(phase==openu5::ShopPhase::TavernRoundDeal)return "A Round";
    if(phase==openu5::ShopPhase::TavernDrinkDeal)return "Drink";
    if(phase==openu5::ShopPhase::RationsQuantity)return "Food (25 provisions)";
    if(phase==openu5::ShopPhase::RumorDeal)return "Rumour";
    return nullptr;
}
const char *equip_slot_name(openu5::EquipSlot slot){switch(slot){case openu5::EquipSlot::Helmet:return "Head";case openu5::EquipSlot::Armor:return "Armor";case openu5::EquipSlot::Weapon:return "Weapon";case openu5::EquipSlot::Shield:return "Offhand";case openu5::EquipSlot::Ring:return "Ring";case openu5::EquipSlot::Amulet:return "Amulet";default:return "Item";}}
int equipped_in_slot(const openu5::CharacterState &c,openu5::EquipSlot slot){switch(slot){case openu5::EquipSlot::Helmet:return c.helmet;case openu5::EquipSlot::Armor:return c.armor;case openu5::EquipSlot::Weapon:return c.weapon;case openu5::EquipSlot::Shield:return c.shield;case openu5::EquipSlot::Ring:return c.ring;case openu5::EquipSlot::Amulet:return c.amulet;default:return 255;}}
int loot_field_value(const openu5::GameState &g,openu5::LootGrant grant){const auto d=openu5::decode_loot(grant);switch(d.category){case openu5::LootCategory::Gold:return g.gold;case openu5::LootCategory::Keys:return g.keys;case openu5::LootCategory::Gems:return g.gems;case openu5::LootCategory::Torches:return g.torches;case openu5::LootCategory::Food:return g.food;case openu5::LootCategory::Potion:return d.item_index>=0&&d.item_index<8?g.potion_quantities[d.item_index]:-1;case openu5::LootCategory::Scroll:return d.item_index>=0&&d.item_index<8?g.scroll_quantities[d.item_index]:-1;case openu5::LootCategory::Equipment:return d.item_index>=0&&d.item_index<256?g.equipment_quantities[d.item_index]:-1;case openu5::LootCategory::QuestItem:return g.wooden_box?1:0;default:return -1;}}
void log_loot_stack(const openu5::CombatState &combat,int x,int y,const char *phase){
    int count=0,visible=-1;char indices[96]{};size_t used=0;
    for(int i=0;i<combat.pile_count;++i)if(combat.piles[i].position.x==x&&combat.piles[i].position.y==y){
        const int written=std::snprintf(indices+used,sizeof(indices)-used,"%s%d",count?",":"",i);
        if(written>0)
            used=std::min(sizeof(indices)-1,used+size_t(written));
        ++count;
        visible=i;
    }
    ESP_LOGI(kTag,"LOOT_STACK phase=%s x=%d y=%d count=%d indices=%s",phase,x,y,count,count?indices:"-");
    if(visible>=0){const auto &pile=combat.piles[visible];const auto decoded=openu5::decode_loot({pile.id,pile.quantity});char name[96]{};openu5::loot_item_name({pile.id,pile.quantity},name,sizeof(name));ESP_LOGI(kTag,"LOOT_STACK_VISIBLE x=%d y=%d chosen_index=%d type=%s name=%s",x,y,visible,openu5::loot_category_name(decoded.category),name);}
}
const char *status_name(openu5::CommandStatus s){static const char*n[]={"success","rejected","no-op","awaiting response","unsupported","invalid context","core error","needs storage"};return n[std::min<size_t>(size_t(s),7)];}
const char *mode_name(openu5::UiMode m){static const char*n[]={"explore","dungeon","combat","dialogue","shop","special","text","number","yes/no","party","inventory","equipment","spell","target","debug"};return n[std::min<size_t>(size_t(m),14)];}
const char *action_name(openu5::UiActionKind k){static const char*n[]={"direction","character","confirm","cancel","back","next","previous","page-up","page-down","select-index","text","delete","system-menu"};return n[std::min<size_t>(size_t(k),12)];}
const char *frontend_state_name(openu5::FrontendState s){static const char*n[]={"title","intro","attract","menu","new-journey","character-creation","continue","load","settings","credits","enter-game","error"};return n[std::min<size_t>(size_t(s),11)];}
const char *creation_phase_name(openu5::FrontendCreationPhase p){static const char*n[]={"name","gender","questionnaire"};return n[std::min<size_t>(size_t(p),2)];}
const char *frontend_intent_name(openu5::FrontendIntentKind k){static const char*n[]={"none","continue","load-slot","create-initial-save","persist-settings","developer"};return n[std::min<size_t>(size_t(k),5)];}
const char *intent_name(openu5::UiIntentKind k){static const char*n[]={"command","shop","modal","open-party","open-inventory","open-equipment","open-spell","open-target","open-status","open-debug"};return n[std::min<size_t>(size_t(k),9)];}
const char *shortcut_name(DeviceShortcut s){static const char*n[]={"none","developer","save","load","movement-toggle"};return n[std::min<size_t>(size_t(s),4)];}
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
int debug_depth(const openu5::UiDebugMenuView &v){return v.editing?2:v.category>=0?1:0;}
#endif

bool combat_actor_live(const openu5::CombatActor &a){return a.status!=openu5::CombatStatus::Dead&&a.status!=openu5::CombatStatus::Fled&&a.status!=openu5::CombatStatus::Absorbed;}
const char *terrain_source(const openu5::TerrainSample &s){
    return s.wiped?"wipe":s.transient?"transient":s.persistent?"persistent":s.hourly?"hourly":"base";
}
}

esp_err_t AlphaRuntime::initialize(AlphaResourcePack &pack,AlphaResourceReport &report,
                                   openu5::AssetPackReader &tiles,const openu5::AssetPackReport &tile_report){
    debug51::Step initialize_trace("alpha-runtime-initialize-detail");
    tiles_=&tiles;tile_report_=tile_report;alpha_report_=report;
    {
        debug51::Step trace("dma-reserve-startup");
        if(!AlphaSaveService::reserve_dma_headroom())ESP_LOGW(kTag,"Unable to reserve early SD DMA headroom; diagnostics remain active");
    }
    {
        debug51::Step trace("alpha-resource-load-owners");
        ESP_RETURN_ON_ERROR(pack.load(resources_,report),kTag,"load Alpha resource owners");
    }
    {
        // Batch 9C / R-05. The authored dungeon art must be read HERE: main.cpp
        // closes the pack as soon as this returns, which is also why the device
        // never touches SD for art again. A failure is logged and tolerated --
        // the dungeon then paints black instead of the corridor, which is a
        // visible, diagnosable state, not a crash and not a silent fallback to
        // invented geometry.
        debug51::Step trace("dungeon-art-load");
        const esp_err_t art=dungeon_art_.load(pack);
        if(art!=ESP_OK)
            ESP_LOGE(kTag,"Authored dungeon art unavailable (%s); the dungeon viewport will be blank",
                     esp_err_to_name(art));
    }
    alpha_report_=report;
    void *ui_mem=nullptr;
    {
        debug51::Step trace("runtime-psram-allocations");
        astar_scratch_=heap_caps_calloc(1,kAstarBytes,kPsram);transcript_=static_cast<openu5::UiTextBlock*>(heap_caps_calloc(kTranscriptBlocks,sizeof(openu5::UiTextBlock),kPsram));
        viewport_=static_cast<uint16_t*>(heap_caps_malloc(openu5::kViewportPixelCount*sizeof(uint16_t),kPsram));
        creation_canvas_=static_cast<uint16_t*>(heap_caps_calloc(kCreationPixels,sizeof(uint16_t),kPsram));
        tile_cache_storage_=static_cast<uint8_t*>(heap_caps_malloc(openu5::kCachedTileBytes,kPsram));
        debug_view_=static_cast<DeviceDebugScreen*>(heap_caps_calloc(1,sizeof(DeviceDebugScreen),kPsram));
        ui_mem=heap_caps_malloc(sizeof(openu5::UiSession),kPsram);
        // #324 / R-32. kBlackthornSceneSteps is the worst real turn plus
        // headroom: the capture entry is the longest at 15 events and 50
        // beats; the escorted finale is 44 beats.
        blackthorn_script_=static_cast<openu5::BlackthornSceneScript*>(heap_caps_calloc(1,sizeof(openu5::BlackthornSceneScript),kPsram));
        blackthorn_steps_=static_cast<openu5::BlackthornSceneStep*>(heap_caps_calloc(kBlackthornSceneSteps,sizeof(openu5::BlackthornSceneStep),kPsram));
        blackthorn_scene_text_=static_cast<char*>(heap_caps_calloc(kBlackthornSceneTextBytes,1,kPsram));
        blackthorn_scene_grid_=static_cast<int16_t*>(heap_caps_calloc(openu5::kBlackthornSceneCells,sizeof(int16_t),kPsram));
        narrative_steps_=static_cast<openu5::NarrativeSceneStep*>(heap_caps_calloc(kNarrativeSceneSteps,sizeof(openu5::NarrativeSceneStep),kPsram));
        narrative_text_=static_cast<char*>(heap_caps_calloc(kNarrativeSceneTextBytes,1,kPsram));
    }
    if(!astar_scratch_||!transcript_||!viewport_||!creation_canvas_||!tile_cache_storage_||!debug_view_||!ui_mem)return ESP_ERR_NO_MEM;
    if(!blackthorn_script_||!blackthorn_steps_||!blackthorn_scene_text_||!blackthorn_scene_grid_)return ESP_ERR_NO_MEM;
    if(!narrative_steps_||!narrative_text_)return ESP_ERR_NO_MEM;
    {
        debug51::Step trace("presentation-tile-cache-load");
        ESP_RETURN_ON_ERROR(openu5::initialize_tile_cache(tiles,tile_report,tile_cache_storage_,openu5::kCachedTileBytes,tile_cache_),kTag,"cache presentation tiles");
    }
    {
        debug51::Step trace("intro-view-bind");
        if(!intro_view_.bind(resources_.intro_view)){ESP_LOGE(kTag,"Derived INTRO.OVL View data invalid");return ESP_ERR_INVALID_SIZE;}
    }
    ui_=new(ui_mem)openu5::UiSession({transcript_,kTranscriptBlocks},{this,dispatch_ui},{11,12,63});
    openu5::save::SidecarSource source;
    openu5::save::Error load{};
    {
        debug51::Step trace("initial-game-state-load");
        load=openu5::save::load_native_state(resources_.initial_gam,resources_.initial_gam_size,nullptr,game_,turn_,retained_,source,true);
    }
    if(load!=openu5::save::Error::None){ESP_LOGE(kTag,"INIT.GAM import failed: %d",int(load));return ESP_FAIL;}
    context_.actors=&actors_;context_.npc_scratch=reinterpret_cast<openu5::NpcScanGrid*>(astar_scratch_);
    context_.npc_data=resources_.npc_locations;context_.npc_data_count=32;
    context_.locations={resources_.location_x,resources_.location_y,resources_.location_count,resources_.location_count};
    context_.combat_context=&combat_context_;context_.dungeon_context=&dungeon_context_;
    context_.dialogue_services=&dialogue_services_;context_.shop_services=&shop_services_;context_.shrine_services=&shrine_services_;
    context_.outdoor=&outdoor_;context_.terrain=&terrain_;context_.quest_world=&quest_;
    terrain_.writes={this,terrain_write};
    context_.blackthorn=&blackthorn_;
    // #324 / R-32. Wiring capture_tiles is what turns the staged scene on:
    // with the packed throne room absent the capture emits the same
    // text-only stream it always did, which is the degradation every parity
    // harness relies on.
    blackthorn_scene_services_.capture_tiles=resources_.blackthorn_scene_tiles;
    blackthorn_scene_services_.state=&blackthorn_scene_state_;
    blackthorn_scene_services_.script=blackthorn_script_;
    context_.blackthorn_scene=&blackthorn_scene_services_;
    blackthorn_pacer_.attach({blackthorn_steps_,kBlackthornSceneSteps,blackthorn_scene_text_,
                              kBlackthornSceneTextBytes,blackthorn_scene_grid_});
    blackthorn_pacer_.set_unit_ms(kPresentationUnitMs);
    narrative_pacer_.attach({narrative_steps_,kNarrativeSceneSteps,narrative_text_,kNarrativeSceneTextBytes});
    poison_.set_blip_ms(openu5::kPoisonBlipMs);
    look_services_.context=this;look_services_.describe=[](void*p,int32_t tile){auto&r=*static_cast<AlphaRuntime*>(p);return tile>=0&&size_t(tile)<r.resources_.look_count?r.resources_.look_text+r.resources_.look_offsets[tile]:"something";};look_services_.sign=[](void*p,openu5::MapId map,int32_t x,int32_t y){auto&r=*static_cast<AlphaRuntime*>(p);return openu5::resolve_look_sign(r.resources_.signs,r.resources_.sign_count,map,x,y);};context_.look=&look_services_;
    context_.services={this,command_effect,command_reload,banner};context_.events={this,dispatch_event};
    quest_.context=this;quest_.count=object_count;quest_.read=object_read;quest_.reserve=object_reserve;quest_.append=object_append;quest_.erase=object_erase;quest_.write=object_write;u5obj_bind();
    quest_.tile_at=tile_at;quest_.volatile_tile=volatile_tile;quest_.persistent_tile=persistent_tile;
    quest_.search_objects=resources_.search_objects;quest_.search_count=resources_.search_count;quest_.spawns=resources_.shard_spawns;quest_.spawn_count=resources_.shard_spawn_count;
    quest_.moon_phases=resources_.moon_phases;quest_.moon_phase_count=resources_.moon_phase_count;
    dialogue_assets_.bind(resources_.dialogue_data,resources_.dialogue_data_size);dialogue_services_.registry={&dialogue_assets_,AlphaDialogueCache::lookup};
    shrine_services_.data=&resources_.shrine_data;
    shrine_services_.context=this;
    shrine_services_.record=[](void *p,int32_t index)->const char*{auto&r=*static_cast<AlphaRuntime*>(p);return tdeck::misc_text_record({r.resources_.misc_text_offsets,r.resources_.misc_text_records,r.resources_.misc_text_record_count},index);};
    combat_actor_overflow_=static_cast<openu5::CombatActor*>(heap_caps_calloc(32,sizeof(openu5::CombatActor),kPsram));
    combat_pile_overflow_=static_cast<openu5::CombatLootPile*>(heap_caps_calloc(32,sizeof(openu5::CombatLootPile),kPsram));
    combat_fields_=static_cast<openu5::CombatField*>(heap_caps_calloc(32,sizeof(openu5::CombatField),kPsram));
    dungeon_arenas_=static_cast<openu5::DungeonArena*>(heap_caps_calloc(resources_.combat_map_count,sizeof(openu5::DungeonArena),kPsram));
    if(!combat_actor_overflow_||!combat_pile_overflow_||!combat_fields_||!dungeon_arenas_)return ESP_ERR_NO_MEM;
    combat_.actors.overflow=combat_actor_overflow_;combat_.actors.overflow_capacity=32;
    combat_.piles.overflow=combat_pile_overflow_;combat_.piles.overflow_capacity=32;combat_.fields=combat_fields_;
    combat_context_.tables={resources_.combat_tables,resources_.combat_tables+resources_.combat_table_count,
                            resources_.combat_tables+resources_.combat_table_count*2,
                            resources_.combat_tables+resources_.combat_table_count*3,resources_.combat_table_count};
    combat_context_.enemy_defs=resources_.combat_enemy_views;combat_context_.enemy_def_count=resources_.combat_enemy_count;
    combat_resources_.maps=resources_.combat_map_views;combat_resources_.map_count=resources_.combat_map_count;
    combat_resources_.enemies=resources_.combat_enemy_views;combat_resources_.enemy_count=resources_.combat_enemy_count;combat_resources_.tables=combat_context_.tables;
    outdoor_.combat=&combat_context_;outdoor_.resources=&combat_resources_;outdoor_.prize_owner=&quest_;
    quest_.encounter=&combat_context_;quest_.combat_resources=&combat_resources_;
    for(size_t i=0;i<resources_.combat_map_count;++i)dungeon_arenas_[i]={resources_.combat_map_views[i],resources_.combat_sprites+i*16};
    dungeon_encounters_.combat=&combat_context_;dungeon_encounters_.arenas=dungeon_arenas_;dungeon_encounters_.count=resources_.combat_map_count;
    dungeon_context_.encounters=&dungeon_encounters_;
    shop_data_=resources_.shop_data;
    shop_services_.context=this;
    shop_services_.record_present=[](void *p,int32_t index){auto&r=*static_cast<AlphaRuntime*>(p);return index>=0&&size_t(index)<r.resources_.shop_text_record_count;};
    shop_services_.record=[](void *p,int32_t index)->const char*{auto&r=*static_cast<AlphaRuntime*>(p);return index>=0&&size_t(index)<r.resources_.shop_text_record_count?r.resources_.shop_text_records+r.resources_.shop_text_offsets[index]:nullptr;};
    shop_services_.tile=[](void *p,int32_t x,int32_t y){return tile_at(p,x,y);};
    shop_services_.occupied=[](void *p,int32_t x,int32_t y){auto&r=*static_cast<AlphaRuntime*>(p);for(size_t i=0;i<r.actors_.count;++i)if(r.actors_.actors[i].location==r.game_.position.map.location&&r.actors_.actors[i].z==r.game_.position.map.floor&&r.actors_.actors[i].x==x&&r.actors_.actors[i].y==y)return true;for(const auto&o:r.objects_)if(o.location==r.game_.position.map.location&&o.floor==r.game_.position.map.floor&&o.x==x&&o.y==y)return true;return false;};
    shop_services_.plate=[](void *p,int32_t x,int32_t y,int32_t tile){volatile_tile(p,x,y,tile);};
    shop_services_.hour_tiles=[](void *p){auto&r=*static_cast<AlphaRuntime*>(p);r.terrain_.refresh(r.resources_.world,r.game_);};
    shop_services_.wake_npcs=[](void *p){auto&r=*static_cast<AlphaRuntime*>(p);const auto loc=r.game_.position.map.location;if(loc>=1&&loc<=32){auto&n=r.resources_.npc_locations[loc-1];openu5::enter_npc_map(r.actors_,n.slots,n.count,uint8_t(loc),uint8_t(r.game_.time.hour),r.game_.npc_dead[loc-1]);}};
    shop_services_.transactional_services=true;
    dungeon_context_.data=resources_.dungeons;dungeon_context_.count=report.dungeon_count;
    auto transport=openu5::world_transport_services(context_);static openu5::TransportServices transport_owner;transport_owner=transport;context_.transport_services=&transport_owner;
    static openu5::RestServices rest_owner;rest_owner.context=this;rest_owner.snap_npcs=[](void*){};rest_owner.occupied=[](void*,int32_t,int32_t,int32_t){return false;};rest_owner.cell_free=[](void*,int32_t,int32_t){return true;};rest_owner.karma_record=[](void*,int32_t){return "\"Rest well, Avatar. Continue upon the path of virtue.\"";};context_.rest_services=&rest_owner;
    {
        debug51::Step trace("terrain-refresh");
        terrain_.refresh(resources_.world,game_);
    }
    const auto loc=game_.position.map.location;
    if(loc>=1&&loc<=32){
        debug51::Step trace("initial-npc-map-enter");
        auto &n=resources_.npc_locations[loc-1];openu5::enter_npc_map(actors_,n.slots,n.count,uint8_t(loc),uint8_t(game_.time.hour),game_.npc_dead[loc-1]);
    }
    ui_->append(openu5::UiTextChannel::System,"OpenU5 T-Deck Alpha 2.0 Frontend");
    ui_->append(openu5::UiTextChannel::System,"Mic back  Alt+D debug  Alt+S save  Alt+L load");
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    smoke_.bind({&context_,&resources_,&alpha_report_,&tile_report_,ui_});
    void *debug_mem=heap_caps_malloc(sizeof(openu5::UiDebugMenu),kPsram);if(!debug_mem)return ESP_ERR_NO_MEM;debug_=new(debug_mem)openu5::UiDebugMenu(context_);debug_->attach_diagnostics({this,start_smoke});ui_->attach_debug_menu(debug_);
#endif
    {
        debug51::Step trace("settings-load");
        settings_store_.load(settings_);
    }
    input_.set_movement_mode_enabled(settings_.movement_mode);input_.set_trackball_responsiveness(settings_.trackball_responsiveness);
    openu5::FrontendSaveSlot slots[2]{};
    {
        debug51::Step trace("save-slots-inspect");
        save_.inspect(slots);
    }
    {
        debug51::Step trace("frontend-title-entry");
        frontend_.start(uint32_t(esp_timer_get_time()/1000),
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
        true,
#else
        false,
#endif
        settings_);frontend_.set_save_slots(slots);frontend_.set_question_texts(resources_.questions,resources_.question_count);frontend_.set_intro_texts(resources_.intro_scenes,resources_.intro_scene_count);
    }
    ESP_LOGI(kTag,"PSRAM allocations: A*=%zu transcript=%zu viewport=%zu creation_canvas=%zu tile_cache=%zu render_scratch=%zu resources=%zu combat=%zu",kAstarBytes,kTranscriptBlocks*sizeof(openu5::UiTextBlock),openu5::kViewportPixelCount*sizeof(uint16_t),kCreationPixels*sizeof(uint16_t),openu5::kCachedTileBytes,sizeof(DeviceDebugScreen),resources_.psram_bytes,32*sizeof(openu5::CombatActor)+32*sizeof(openu5::CombatLootPile)+32*sizeof(openu5::CombatField));
    log_metrics("initialized");return ESP_OK;
}

void AlphaRuntime::dispatch_ui(void *p,const openu5::UiIntent&i){static_cast<AlphaRuntime*>(p)->dispatch(i);}
void AlphaRuntime::dispatch_event(void *p,const openu5::GameEvent&e){static_cast<AlphaRuntime*>(p)->consume_event(e);}
void AlphaRuntime::consume_event(const openu5::GameEvent&e){
    // R-10: NpcInitiatesTalk/NpcInitiatesShop fire from inside the still-live
    // outer command() call (blackthorn_turn_effect, reached from the town
    // turn tail). UiSession::consume() would translate these into an
    // immediate BeginConversation dispatch -- exactly the re-entrant call
    // into this class's own instrumented command() the adjudication measured
    // as merely tolerated, not the desired architecture. Intercept here,
    // before UiSession ever sees the event, and copy only the stable
    // identity (schedule.slot, location) -- e.npc is borrowed for this
    // synchronous delivery only and must never be retained. The drain runs
    // once the outer input has fully unwound (see handle()).
    if(e.kind==openu5::GameEventKind::NpcInitiatesTalk||e.kind==openu5::GameEventKind::NpcInitiatesShop){
        if(e.npc){
            pending_npc_initiation_=e.kind==openu5::GameEventKind::NpcInitiatesTalk?PendingNpcInitiation::Talk:PendingNpcInitiation::Shop;
            pending_npc_slot_=int16_t(e.npc->schedule.slot);
            pending_npc_location_=e.npc->location;
        }
        return;
    }
    // #324 / R-32. The capture turn is emitted synchronously in full; the
    // scene pacer takes ownership of it from the first scene event onward and
    // hands it back a beat at a time (see service_blackthorn_scene()). Every
    // borrowed payload pointer in `e` dies with this call, so the pacer copies
    // what it keeps.
    if(blackthorn_pacer_.enqueue(e)){
        if(e.kind==openu5::GameEventKind::BlackthornScene)
            ESP_LOGI(kTag,"BLACKTHORN_SCENE_SEGMENT beats=%u state=%d",
                     unsigned(e.blackthorn_scene?e.blackthorn_scene->count:0),int(blackthorn_pacer_.state()));
        dirty_=true;dirty_reason_="blackthorn-scene";
        return;
    }
    // Y-04 (Batch 7B). Refuge and TrollSneak are MODAL narrative scenes: from
    // the first scene event onward the pacer owns the rest of the turn and
    // hands it back a beat at a time, exactly as the Blackthorn pacer above
    // does. Every borrowed payload pointer in `e` dies with this call, so the
    // pacer copies what it keeps.
    if(narrative_pacer_.enqueue(e)){
        if(e.kind==openu5::GameEventKind::Refuge||e.kind==openu5::GameEventKind::TrollSneak)
            ESP_LOGI(kTag,"NARRATIVE_SCENE_BEGIN scene=%d queued=%u",
                     int(narrative_pacer_.scene()),unsigned(narrative_pacer_.queued_steps()));
        dirty_=true;dirty_reason_="narrative-scene";
        return;
    }
    ui_->consume(e);
    if(e.kind==openu5::GameEventKind::PoisonTick){
        // #213. The roster row inversion is the SHARED 0x2a28 primitive, one
        // 93 ms blip per poisoned member in SLOT order. Not modal: the tick
        // happens on every step, so it swallows no input and defers no turn.
        poison_.run(e.slots,e.slot_count,uint32_t(esp_timer_get_time()/1000));
        dirty_=true;dirty_reason_="poison-tick";
        ESP_LOGI(kTag,"POISON_FEEDBACK slots=%u blip_ms=%lu row=%d modal=0",
                 unsigned(e.slot_count),(unsigned long)openu5::kPoisonBlipMs,int(poison_.flash_row()));
    }
    if(e.kind==openu5::GameEventKind::CellExplosion){
        // #201/#243. Presentation only: the world object the ritual destroys
        // has ALREADY been committed by the core, and `under_tile` is what
        // holds its sprite on the cell for the whole choreography instead of
        // deferring that mutation. The lead is the remainder of this device's
        // own shake window -- the reference's `cerrarVentanaDeSacudida`, which
        // pushes the burst past the quake it follows.
        const int64_t now=esp_timer_get_time();
        openu5::WorldFx fx;
        fx.kind=openu5::WorldFxKind::Explosion;
        fx.dx=e.cell_fx.dx;fx.dy=e.cell_fx.dy;
        fx.bursts=e.cell_fx.bursts;fx.pre_delay_units=e.cell_fx.pre_delay_units;
        fx.under_tile=e.cell_fx.under_tile;
        fx.lead_ms=uint32_t(quake_remaining_ms(now));
        world_fx_.push(fx,uint32_t(now/1000));
        dirty_=true;dirty_reason_="cell-explosion";
        ESP_LOGI(kTag,"CELL_EXPLOSION dx=%d dy=%d bursts=%d pause_units=%d under=%d lead_ms=%lu duration_ms=%lu",
                 int(fx.dx),int(fx.dy),int(fx.bursts),int(fx.pre_delay_units),int(fx.under_tile),
                 (unsigned long)fx.lead_ms,(unsigned long)openu5::WorldFxLayer::duration_ms(fx));
    }
    if(e.kind==openu5::GameEventKind::CellProjectile){
        // #313. ONE flight per shot, never one per cell; the origin is the
        // ship for a broadside and the cannon's own cell on foot. The ball
        // lives BETWEEN cells, so it is a sub-cell dot and never a tile write.
        openu5::WorldFx fx;
        fx.kind=openu5::WorldFxKind::Projectile;
        fx.from_dx=e.projectile.from_dx;fx.from_dy=e.projectile.from_dy;
        fx.to_dx=e.projectile.to_dx;fx.to_dy=e.projectile.to_dy;
        world_fx_.push(fx,uint32_t(esp_timer_get_time()/1000));
        dirty_=true;dirty_reason_="cell-projectile";
        ESP_LOGI(kTag,"CELL_PROJECTILE from=(%d,%d) to=(%d,%d) cells=%d duration_ms=%lu",
                 int(fx.from_dx),int(fx.from_dy),int(fx.to_dx),int(fx.to_dy),
                 openu5::WorldFxLayer::projectile_cells(fx),
                 (unsigned long)openu5::WorldFxLayer::duration_ms(fx));
    }
    if(e.kind==openu5::GameEventKind::DungeonEntered){
        dungeon_presentation_pending_=true;
        ESP_LOGI(kTag,"DUNGEON_LOAD_BEGIN dungeon=%u depth=%u",unsigned(dungeon_.pos.dungeon),unsigned(dungeon_.pos.floor));
        ESP_LOGI(kTag,"DUNGEON_LOAD_RESULT result=success reason=normal-init");
        ESP_LOGI(kTag,"DUNGEON_SESSION_STATE active=%d dungeon=%u depth=%u x=%u y=%u facing=%d",dungeon_.active,unsigned(dungeon_.pos.dungeon),unsigned(dungeon_.pos.floor),unsigned(dungeon_.pos.x),unsigned(dungeon_.pos.y),int(dungeon_.pos.facing));
        ESP_LOGI(kTag,"DUNGEON_CONTEXT dungeon=%d",context_.dungeon);
        ESP_LOGI(kTag,"DUNGEON_RENDERER active=%d",dungeon_.active);
    }
    if(e.kind==openu5::GameEventKind::GemView){
        // The core consumes a real gem on successful V, then waits for the
        // presentation to close before charging the deferred turn.  Keep that
        // transaction ordering on the device rather than treating V as text.
        gem_view_active_=true;gem_view_charges_turn_=!e.gem_from_crystal;
        ESP_LOGI(kTag,"VIEW_EFFECT type=gem presentation=%s deferred_turn=%d dungeon=%d",
                 dungeon_.active?"dungeon-floor-map":"world-map",gem_view_charges_turn_,dungeon_.active);
    }
    if(e.kind==openu5::GameEventKind::Zodiac&&e.zodiac){
        zodiac_view_=*e.zodiac;zodiac_view_active_=true;
        dirty_=true;dirty_reason_="zodiac-view";
        ESP_LOGI(kTag,"VIEW_EFFECT type=zodiac presentation=night-sky deferred_turn=0");
    }
    if(e.kind==openu5::GameEventKind::Quake){
        const int64_t now=esp_timer_get_time();
        const bool running=quake_pulses_>0&&(now-quake_start_us_)<int64_t(quake_pulses_)*openu5::kQuakePeriodMs*1000;
        if(running)quake_pulses_+=openu5::kQuakePulses;
        else{quake_start_us_=now;quake_pulses_=openu5::kQuakePulses;}
        dirty_=true;dirty_reason_="quake";
        ESP_LOGI(kTag,"QUAKE pulses=%d extended=%d",quake_pulses_,running);
    }
    if(e.kind==openu5::GameEventKind::MagicCeremony)start_magic_ceremony(e.note);
    if(e.kind==openu5::GameEventKind::MapReveal){
        // e.note is DEATH_VISION_FRAMES=20 (run-n-frames units, world_magic.cpp's
        // emit()); the reference's runMapReveal() converts frames*PAUSE_UNIT_MS
        // to a wall-clock window and swallows input for it.
        map_reveal_end_us_=esp_timer_get_time()+int64_t(e.note)*kPresentationUnitMs*1000;
        dirty_=true;dirty_reason_="map-reveal";
        ESP_LOGI(kTag,"MAP_REVEAL frames=%ld duration_ms=%ld",long(e.note),long(e.note)*long(kPresentationUnitMs));
    }
    if(e.kind==openu5::GameEventKind::Combat&&e.combat&&
       e.combat->kind==openu5::CombatEventKind::Message&&e.combat->text&&
       std::strcmp(e.combat->text,"Blocked!")==0){
        int32_t actor=e.combat->actor;
        if(actor<0&&combat_.current>=0&&combat_.current<combat_.count)
            actor=combat_.actors[combat_.current].id;
        ESP_LOGI(kTag,"COMBAT_TEXT actor=%ld event=Blocked source=combat.move generated=%lu presented=%lu",
                 long(actor),static_cast<unsigned long>(ui_->blocked_events_generated()),
                 static_cast<unsigned long>(ui_->blocked_events_presented()));
    }
}
// #324 / R-32. Releases whatever of the deferred capture turn is due now.
// Returns true when anything moved, so the caller can mark the frame dirty.
// The sink is UiSession's own, deliberately NOT this class's consume_event:
// a released event must reach the session, never be re-offered to the pacer.
bool AlphaRuntime::service_blackthorn_scene(){
    if(!blackthorn_pacer_.active())return false;
    const auto released_before=blackthorn_pacer_.released_steps();
    const auto state_before=blackthorn_pacer_.state();
    const auto phase_before=blackthorn_pacer_.view().phase;
    blackthorn_pacer_.pump(uint32_t(esp_timer_get_time()/1000),ui_->event_sink());
    const auto released=blackthorn_pacer_.released_steps();
    const auto state=blackthorn_pacer_.state();
    const auto phase=blackthorn_pacer_.view().phase;
    if(phase!=phase_before||state!=state_before)
        ESP_LOGI(kTag,"BLACKTHORN_SCENE phase=%d->%d state=%d->%d released=%lu dropped=%lu",
                 int(phase_before),int(phase),int(state_before),int(state),
                 (unsigned long)released,(unsigned long)blackthorn_pacer_.dropped_steps());
    blackthorn_released_=released;
    return released!=released_before||state!=state_before||phase!=phase_before;
}

// Y-04. The reference's `cerrarVentanaDeSacudida` (skin/turn-phase.ts): an
// effect that FOLLOWS a shake does not start inside it -- the binary's
// screen_shake_fx blocks until it is done. Native has no audio catalogue, so
// the shake window is the only lead this device can derive, and it is the one
// the ritual actually needs (three quakes precede its burst).
int64_t AlphaRuntime::quake_remaining_ms(int64_t now_us) const{
    if(quake_pulses_<=0)return 0;
    const int64_t elapsed_ms=(now_us-quake_start_us_)/1000;
    const int64_t window_ms=int64_t(quake_pulses_)*openu5::kQuakePeriodMs;
    return elapsed_ms>=window_ms?0:window_ms-elapsed_ms;
}

// Y-04 (Batch 7B). The beat sink of the narrative pacer. A beat is the whole
// of what the scene changes at that instant: a console line (new, or a
// CONTINUATION of the one on screen -- the three dots of `$ sneaks across`),
// a sound cue, and/or the refuge's visual phase. The pacer owns the timing;
// this owns nothing but the routing.
void AlphaRuntime::narrative_beat(void *p,const openu5::NarrativeSceneBeat &beat){
    auto &self=*static_cast<AlphaRuntime*>(p);
    if(!self.ui_)return;
    if(beat.text){
        if(beat.append)self.ui_->append_continuation(openu5::UiTextChannel::Message,beat.text);
        else self.ui_->append(openu5::UiTextChannel::Message,beat.text);
    }
    // Y-03 owns real audio; the cue is semantic here, as everywhere else.
    if(beat.sfx)ESP_LOGI(kTag,"SFX_CUE id=%s source=narrative-scene",beat.sfx);
    if(beat.phase!=openu5::RefugePhase::None)
        ESP_LOGI(kTag,"REFUGE_SCENE phase=%d",int(beat.phase));
}

// Releases whatever of the deferred Refuge/TrollSneak turn is due now, and
// applies the refuge's state mutation when -- and only when -- the scene has
// finished. The sink is UiSession's own, deliberately NOT this class's
// consume_event: a released event must reach the session, never be re-offered
// to the pacer. Returns true when anything moved.
//
// KNOWN AND BOUNDED: because a released event reaches UiSession directly, it
// does not re-run consume_event's own presentation tail (world fx, poison
// flash). That is the same contract service_blackthorn_scene() has, and it is
// safe for the turns these scenes actually defer -- turn_events() emits
// PoisonTick BEFORE troll_script(), and check_refuge is the terminal event of
// its turn, so no deferred tail carries an fx today. If one ever does, this
// sink is where it would have to grow, not the pacer.
bool AlphaRuntime::service_narrative_scene(){
    if(!narrative_pacer_.active())return false;
    const auto released_before=narrative_pacer_.released_steps();
    const auto phase_before=narrative_pacer_.phase();
    narrative_pacer_.pump(uint32_t(esp_timer_get_time()/1000),{this,narrative_beat},ui_->event_sink());
    const auto released=narrative_pacer_.released_steps();
    const auto phase=narrative_pacer_.phase();
    narrative_released_=released;
    const auto finished=narrative_pacer_.take_completion();
    if(finished!=openu5::NarrativeScene::None)
        ESP_LOGI(kTag,"NARRATIVE_SCENE_END scene=%d released=%lu dropped=%lu",
                 int(finished),(unsigned long)released,(unsigned long)narrative_pacer_.dropped_steps());
    if(finished==openu5::NarrativeScene::Refuge){
        // BLCKTHRN 0x0bfd-0x0c4d, in the reference's own order: the scene
        // comes down FIRST (the pacer has already unmounted it), and only then
        // does the resurrection commit and reveal the castle.
        const auto status=openu5::resolve_refuge(context_,ui_->event_sink());
        ESP_LOGI(kTag,"REFUGE_RESOLVE status=%d location=%u karma=%ld",
                 int(status),unsigned(game_.position.map.location),long(game_.karma));
        dirty_=true;dirty_reason_="refuge-resolve";
    }
    return released!=released_before||phase!=phase_before||finished!=openu5::NarrativeScene::None;
}

// #213. Advances the roster flash. Not modal and not turn-gated: it runs off
// the same frame clock as every other presentation timer here.
bool AlphaRuntime::service_poison_flash(){
    if(!poison_.active())return false;
    const auto row_before=poison_.flash_row();
    poison_.pump(uint32_t(esp_timer_get_time()/1000));
    return poison_.flash_row()!=row_before;
}

void AlphaRuntime::start_magic_ceremony(int index){
    index=std::clamp(index,0,8);const int64_t now=esp_timer_get_time();
    // CAST2:0000 calibrated by the existing DOS audio reference: noise lead,
    // then two mirrored sweeps while the viewport is XOR-inverted.
    magic_invert_start_us_=now+int64_t(0x1f40+0x640*index)*3*1000000/(2*25806);
    magic_invert_end_us_=magic_invert_start_us_+int64_t(2)*(0x2710+0xfa0*index)*1000000/25806;
    magic_was_inverted_=false;dirty_=true;dirty_reason_="magic-ceremony";
    ESP_LOGI(kTag,"MAGIC_FEEDBACK index=%d lead_us=%lld invert_us=%lld audio=semantic async=1",index,(long long)(magic_invert_start_us_-now),(long long)(magic_invert_end_us_-magic_invert_start_us_));
}
void AlphaRuntime::dispatch(const openu5::UiIntent&i){
    ESP_LOGD(kTag,"UI intent=%s request=%d mode=%s",intent_name(i.kind),int(i.request),mode_name(ui_->mode()));
    if(i.kind==openu5::UiIntentKind::Command)command(i.command);
    else if(i.kind==openu5::UiIntentKind::Shop){const int64_t s=esp_timer_get_time();auto input=i.shop;
        // UiSession owns a compact cursor; the authoritative shop contract
        // selects by item/member id. Resolve through the live offerings list.
        if(input.action==openu5::ShopAction::SelectItem||input.action==openu5::ShopAction::SelectMember){openu5::ShopOffer offer{};if(!openu5::shop_offering_at(game_,shop_,shop_data_,size_t(input.value),offer)){ui_->append(openu5::UiTextChannel::System,"Shop selection unavailable");return;}input.value=offer.item;}
        const auto phase_before=shop_.phase;auto result=openu5::execute_shop(context_,input);command_high_us_=std::max<uint32_t>(command_high_us_,uint32_t(esp_timer_get_time()-s));
        ESP_LOGI(kTag,"SERVICE_ACTION keeper=\"%s\" shop_type=%d command=%d service_id=%ld price=%ld quantity=%ld state=%d->%d result=%d",
                 shop_.record&&shop_.record->keeper?shop_.record->keeper:"Merchant",int(shop_.type),int(input.action),
                 long(shop_.item),long(shop_.price),long(shop_.quantity),int(phase_before),int(shop_.phase),int(result.status));
        if(result.status!=openu5::CommandStatus::Success&&result.status!=openu5::CommandStatus::AwaitingResponse)ESP_LOGW(kTag,"Service action failed status=%s",status_name(result.status));}
    else if(i.kind==openu5::UiIntentKind::ModalResponse)modal(i);
    // R-25 (Batch 19). The crystal ball's picker. look.cpp raises
    // CrystalBallPrompt for EVERY tile 0x29, exactly as the reference core
    // does, and the branch decision happens here because only this layer owns
    // the roster: LOOKOBJ 0x09ea's picker asks in just one of its four
    // branches, and on the other three it either hands the member straight
    // back (active, or a single eligible) or prints "None!" and abandons the
    // command. Resolving it here also keeps all three 0x4988 callers on one
    // code path -- the same split ui/pickers.ts makes in the reference port.
    else if(i.kind==openu5::UiIntentKind::OpenPartySelection&&i.request==openu5::UiRequestId::CrystalBall){
        int16_t member=-1;
        if(resolve_command_char_or_prompt(openu5::UiRequestId::CrystalBall,member)==openu5::CommandCharOutcome::Resolved){
            openu5::Command ball;ball.kind=openu5::CommandKind::CrystalBall;ball.member=member;command(ball);
        }
    }
    else if(i.kind==openu5::UiIntentKind::OpenPartySelection)open_selection(openu5::UiMode::PartySelection,i.request);
    else if(i.kind==openu5::UiIntentKind::OpenInventorySelection)open_selection(openu5::UiMode::InventorySelection,i.request);
    else if(i.kind==openu5::UiIntentKind::OpenEquipmentSelection){
        auto *actor=context_.combat?openu5::current_combat_actor(combat_context_):nullptr;
        if(actor&&actor->member!=255){pending_ready_member_=actor->member;open_selection(openu5::UiMode::EquipmentSelection,openu5::UiRequestId::Equipment);}
        else if(game_.party.party_size>1)open_selection(openu5::UiMode::PartySelection,openu5::UiRequestId::EquipmentMember);
        else {pending_ready_member_=int16_t(active_member(game_));open_selection(openu5::UiMode::EquipmentSelection,openu5::UiRequestId::Equipment);}
    }
    // R-25 (Batch 19). CAST.OVL:0x0dd5 resolves WHO casts before the
    // "Spell name:" prompt of 0x11d9 -- the OCR corpus of 49 original
    // Let's-Play routes shows 70 "Cast... Player: <name> Spell name:" rows and
    // zero the other way round -- so the caster gate runs before the spell
    // menu opens, not after it. Combat is branch 1 (no prompt, the acting
    // combatant) and (M)ix is a different routine, so both keep the plain menu.
    else if(i.kind==openu5::UiIntentKind::OpenSpellSelection){
        if(i.request==openu5::UiRequestId::Spell&&!context_.combat){
            pending_caster_=-1;
            int16_t caster=-1;
            if(resolve_command_char_or_prompt(openu5::UiRequestId::CastMember,caster)!=openu5::CommandCharOutcome::Resolved){dirty_=true;return;}
            pending_caster_=caster;
        }
        open_selection(openu5::UiMode::SpellSelection,i.request);
    }
    else if(i.kind==openu5::UiIntentKind::OpenStatusSelection){status_member_=int16_t(active_member(game_));open_selection(openu5::UiMode::PartySelection,i.request);}
    dirty_=true;
}
void AlphaRuntime::command(openu5::Command cmd){
    // R-25 (Batch 19). SJOG cmd_search 0x095c asks for the direction at 0x097e
    // and only THEN calls the picker at 0x09a0, so the command arrives here
    // already aimed and the member is the one thing still missing. Command::
    // member was never set, which left search_world() on its "active, else
    // member 0" fallback -- and that member is the perceiver of every chest
    // trap check. Combat (CombatSearch) and the dungeon corridor (SJOG 0x0646,
    // a different routine that is only partly ported) are deliberately
    // untouched.
    if(cmd.kind==openu5::CommandKind::Search&&cmd.member<0&&!context_.combat){
        int16_t searcher=-1;
        const auto outcome=resolve_command_char_or_prompt(openu5::UiRequestId::SearchMember,searcher);
        if(outcome==openu5::CommandCharOutcome::Prompt){pending_search_=cmd;pending_search_active_=true;dirty_=true;return;}
        if(outcome==openu5::CommandCharOutcome::None){pending_search_active_=false;dirty_=true;return;}
        cmd.member=searcher;
    }
    const bool combat_before=context_.combat&&combat_.initialized;
    const bool dungeon_session_before=dungeon_.active;
    if(cmd.kind==openu5::CommandKind::Enter){
        const auto candidate=openu5::location_at(context_.locations,game_.position.xy.x,game_.position.xy.y);
        ESP_LOGI(kTag,"DUNGEON_ENTER_REQUEST source=normal dungeon=%ld depth=%d entrance=1 x=%u y=%u facing=%d",long(candidate),int(game_.position.map.floor),unsigned(game_.position.xy.x),unsigned(game_.position.xy.y),int(dungeon_.pos.facing));
    }
    if(cmd.kind==openu5::CommandKind::TrollToll&&commands_.awaiting_troll)trace_direct_troll_precombat();
    int combat_open_key=-1,combat_open_candidates=0;bool combat_open_chest=false;
    if(cmd.kind==openu5::CommandKind::CombatOpen){
        auto *actor=openu5::current_combat_actor(combat_context_);
        const int tx=cmd.has_direction?int(cmd.combat_x):-1,ty=cmd.has_direction?int(cmd.combat_y):-1;
        combat_open_key=tx>=0&&tx<openu5::kCombatGrid&&ty>=0&&ty<openu5::kCombatGrid?ty*openu5::kCombatGrid+tx:-1;
        combat_open_chest=combat_open_key>=0&&(combat_.loot[combat_open_key]==1||combat_.loot[combat_open_key]==129);
        combat_open_candidates=combat_open_chest?1:0;
        ESP_LOGI(kTag,"OPEN_TARGET_DIRECTION dir=%s",cmd.has_direction?openu5::direction_name(cmd.direction):"unselected");
        ESP_LOGI(kTag,"OPEN_TARGET_RESOLVED space=combat-grid x=%d y=%d",tx,ty);
        ESP_LOGI(kTag,"OPEN_TARGET_RENDER cell_x=%d cell_y=%d screen_x=%d screen_y=%d",tx,ty,tx>=0?tx*16:-1,ty>=0?ty*16:-1);
        ESP_LOGI(kTag,"OPEN_TARGET_ASSERT logical=%d,%d rendered=%d,%d lookup=%d,%d match=%d",tx,ty,tx,ty,tx,ty,cmd.has_direction);
        ESP_LOGI(kTag,"COMBAT_OPEN_REQUEST actor=%ld source_x=%d source_y=%d dir=%s target_x=%d target_y=%d",actor?long(actor->id):-1,actor?int(actor->position.x):-1,actor?int(actor->position.y):-1,cmd.has_direction?openu5::direction_name(cmd.direction):"unselected",tx,ty);
        if(combat_open_key>=0)ESP_LOGI(kTag,"COMBAT_OPEN_OBJECT index=%d combat_x=%d combat_y=%d encoded=%d chest=%d trap=%d contents=%d state=%d",combat_open_key,tx,ty,int(combat_.loot[combat_open_key]),combat_open_chest,combat_.loot[combat_open_key]==129,int(combat_.chest_contents[combat_open_key]),int(combat_.chest_state[combat_open_key]));
        ESP_LOGI(kTag,"COMBAT_OPEN_LOOKUP candidates=%d chosen=%d",combat_open_candidates,combat_open_chest?combat_open_key:-1);
    }
    int open_x=-1,open_y=-1,open_chosen=-1,open_candidates=0;bool openable=false;
    if(cmd.kind==openu5::CommandKind::Open){const auto d=openu5::direction_delta(cmd.direction);open_x=int(game_.position.xy.x)+d.dx;open_y=int(game_.position.xy.y)+d.dy;if(!game_.position.map.location){open_x&=255;open_y&=255;}ESP_LOGI(kTag,"OPEN_REQUEST player_loc=%u player_floor=%d player_x=%u player_y=%u dir=%s target_x=%d target_y=%d transform=%s",unsigned(game_.position.map.location),int(game_.position.map.floor),unsigned(game_.position.xy.x),unsigned(game_.position.xy.y),openu5::direction_name(cmd.direction),open_x,open_y,game_.position.map.location?"bounded":"wrap256");for(size_t i=0;i<objects_.size();++i){const auto&o=objects_[i];if(o.location!=game_.position.map.location||o.floor!=game_.position.map.floor||o.x!=open_x||o.y!=open_y)continue;const bool candidate=o.chest||(o.loot&&(o.item_id==1||o.item_id==14));++open_candidates;ESP_LOGI(kTag,"OPEN_OBJECT index=%u id=%u type=%s loc=%ld floor=%ld x=%ld y=%ld openable=%d chest=%d trap=%d contents=%ld tile=%ld flags=loot%d/prop%d",unsigned(i),unsigned(i),o.chest?"chest":o.loot?"loot":o.prop?"prop":"other",long(o.location),long(o.floor),long(o.x),long(o.y),candidate,o.chest,o.trapped,long(o.contents),long(o.tile),o.loot,o.prop);if(open_chosen<0){open_chosen=int(i);openable=candidate;}else if(candidate&&!openable){open_chosen=int(i);openable=true;}}if(direct_troll_.active)ESP_LOGI(kTag,"OPEN_EXPECTED_CHEST inserted_x=%ld inserted_y=%ld target_x=%d target_y=%d match=%d",long(direct_troll_.inserted_x),long(direct_troll_.inserted_y),open_x,open_y,direct_troll_.inserted_x==open_x&&direct_troll_.inserted_y==open_y);ESP_LOGI(kTag,"OPEN_LOOKUP target=%d,%d candidates=%d chosen=%d openable=%d",open_x,open_y,open_candidates,open_chosen,openable);}
    const int piles_before=combat_.pile_count;
    openu5::LootGrant pending_pickup{};bool pending_pickup_known=false;int pending_pickup_before=-1,pending_pickup_index=-1,pending_x=-1,pending_y=-1;
    if(cmd.kind==openu5::CommandKind::CombatGet){auto *actor=openu5::current_combat_actor(combat_context_);pending_x=actor?actor->position.x:-1;pending_y=actor?actor->position.y:-1;if(cmd.has_direction){pending_x=cmd.combat_x;pending_y=cmd.combat_y;}ESP_LOGI(kTag,"COMBAT_GET_REQUEST actor=%ld x=%d y=%d dir=%s",actor?long(actor->id):-1,pending_x,pending_y,cmd.has_direction?openu5::direction_name(cmd.direction):"cancelled");int candidates=0;for(int i=combat_.pile_count-1;i>=0;--i)if(combat_.piles[i].position.x==pending_x&&combat_.piles[i].position.y==pending_y){const auto &pile=combat_.piles[i];const auto decoded=openu5::decode_loot({pile.id,pile.quantity});ESP_LOGI(kTag,"COMBAT_GET_CANDIDATE index=%d type=%s item_index=%d qty=%d",i,openu5::loot_category_name(decoded.category),int(decoded.item_index),int(decoded.quantity));++candidates;if(!pending_pickup_known){pending_pickup={pile.id,pile.quantity};pending_pickup_known=true;pending_pickup_index=i;pending_pickup_before=loot_field_value(game_,pending_pickup);}}log_loot_stack(combat_,pending_x,pending_y,"before-get");const auto decoded=openu5::decode_loot(pending_pickup);ESP_LOGI(kTag,"COMBAT_GET_LOOKUP candidates=%d chosen=%d",candidates,pending_pickup_index);if(pending_pickup_known)ESP_LOGI(kTag,"COMBAT_GET_OBJECT index=%d x=%d y=%d raw_type=%d raw_id=%d raw_qty=%d decoded_type=%s decoded_index=%d decoded_qty=%d",pending_pickup_index,pending_x,pending_y,int(pending_pickup.id),int(pending_pickup.id),int(pending_pickup.quantity),openu5::loot_category_name(decoded.category),int(decoded.item_index),int(decoded.quantity));}
    if(cmd.kind==openu5::CommandKind::Get){const auto d=openu5::direction_delta(cmd.direction);int tx=int(game_.position.xy.x)+d.dx,ty=int(game_.position.xy.y)+d.dy;if(!game_.position.map.location){tx&=255;ty&=255;}for(const auto&o:objects_)if(o.loot&&o.location==game_.position.map.location&&o.floor==game_.position.map.floor&&o.x==tx&&o.y==ty){pending_pickup={o.item_id,o.quality};pending_pickup_known=true;pending_pickup_before=loot_field_value(game_,pending_pickup);break;}}
    int ready_member=-1,ready_item=-1,ready_current=-1,ready_pack_before=-1;openu5::EquipSlot ready_slot=openu5::EquipSlot::None;
    if(cmd.kind==openu5::CommandKind::Ready&&cmd.member>=0&&cmd.member<game_.party.character_count&&cmd.item>=0&&cmd.item<48){ready_member=cmd.member;ready_item=cmd.item;ready_slot=openu5::slot_for_equip(cmd.item);ready_current=equipped_in_slot(game_.party.characters[cmd.member],ready_slot);ready_pack_before=game_.equipment_quantities[cmd.item];ESP_LOGI(kTag,"READY_BEFORE member=%d slot=%s current=%d inventory_old=%d inventory_new=%d",ready_member,equip_slot_name(ready_slot),ready_current,ready_current>=0&&ready_current<48?game_.equipment_quantities[ready_current]:0,ready_pack_before);ESP_LOGI(kTag,"READY_APPLY member=%d slot=%s selected=%d",ready_member,equip_slot_name(ready_slot),ready_item);}
    const int view_gems_before=cmd.kind==openu5::CommandKind::ViewGem?game_.gems:-1;
    if(cmd.kind==openu5::CommandKind::ViewGem)
        ESP_LOGI(kTag,"VIEW_INPUT ui=%s context=combat%d,dungeon%d gems=%d",mode_name(ui_->mode()),context_.combat,context_.dungeon,game_.gems);
    int32_t prior_ids[64]{};openu5::CombatStatus prior_status[64]{};
    const int prior_count=combat_before?std::min<int>(combat_.count,64):0;
    for(int i=0;i<prior_count;++i){prior_ids[i]=combat_.actors[i].id;prior_status[i]=combat_.actors[i].status;}
    last_routed_command_=cmd.kind;++routed_command_sequence_;
    const int64_t start=esp_timer_get_time();auto result=openu5::dispatch_world_command(context_,cmd);const auto us=uint32_t(esp_timer_get_time()-start);command_high_us_=std::max(command_high_us_,us);
    if(cmd.kind==openu5::CommandKind::Enter){
        ESP_LOGI(kTag,"DUNGEON_ENTER_COMMAND result=%s",status_name(result.status));
        if(!dungeon_session_before&&dungeon_.active)ESP_LOGI(kTag,"DUNGEON_RUN_COMMAND result=success");
    }
    if(cmd.kind==openu5::CommandKind::Open){
        ESP_LOGI(kTag,"OPEN_RESULT result=%s reason=%s target=%d,%d chosen=%d candidates=%d remaining_objects=%u",status_name(result.status),openable?"exact-openable-object":open_candidates?"objects-not-openable":"no-object-at-exact-target",open_x,open_y,open_chosen,open_candidates,unsigned(objects_.size()));
    }
    if(cmd.kind==openu5::CommandKind::CombatOpen){
        const bool consumed=combat_open_key>=0&&combat_.chest_state[combat_open_key]==openu5::CombatChestState::Consumed;
        ESP_LOGI(kTag,"COMBAT_OPEN_RESULT result=%s reason=%s target_index=%d consumed=%d",status_name(result.status),consumed?"opened-and-consumed":combat_open_chest?"chest-not-consumed":"no-chest-at-exact-target",combat_open_key,consumed);
    }
    if(cmd.kind==openu5::CommandKind::CombatOpen&&result.status==openu5::CommandStatus::Success){
        for(int i=piles_before;i<combat_.pile_count;++i){const auto &grant=combat_.piles[i];const auto decoded=openu5::decode_loot({grant.id,grant.quantity});char name[96]{};openu5::loot_item_name({grant.id,grant.quantity},name,sizeof(name));ESP_LOGI(kTag,"LOOT_REVEAL source=chest item_type=%s item_index=%d display_name=%s qty=%d",openu5::loot_category_name(decoded.category),int(decoded.item_index),name,int(decoded.quantity));ESP_LOGI(kTag,"LOOT_SPAWN x=%d y=%d item_type=%s item_index=%d display_name=%s qty=%d",int(grant.position.x),int(grant.position.y),openu5::loot_category_name(decoded.category),int(decoded.item_index),name,int(decoded.quantity));}
    }
    if(cmd.kind==openu5::CommandKind::CombatGet){const bool removed=pending_pickup_known&&combat_.pile_count==piles_before-1;ESP_LOGI(kTag,"COMBAT_GET_RESULT result=%s reason=%s",status_name(result.status),removed?"awarded-and-removed":pending_pickup_known?"award-rejected-or-retained":"no-loose-loot-at-target");log_loot_stack(combat_,pending_x,pending_y,"after-get");const int remaining=([&](){int n=0;for(int i=0;i<combat_.pile_count;++i)if(combat_.piles[i].position.x==pending_x&&combat_.piles[i].position.y==pending_y)++n;return n;})();ESP_LOGI(kTag,"LOOT_STACK_AFTER_REMOVE x=%d y=%d remaining=%d next_visible=%d",pending_x,pending_y,remaining,([&](){for(int i=combat_.pile_count-1;i>=0;--i)if(combat_.piles[i].position.x==pending_x&&combat_.piles[i].position.y==pending_y)return i;return -1;})());if(removed&&remaining==0&&pending_x>=0&&pending_x<openu5::kCombatGrid&&pending_y>=0&&pending_y<openu5::kCombatGrid){const auto recomposed=openu5::compose_combat_presentation(combat_,game_);const int final_tile=recomposed.tiles[pending_y*openu5::kCombatGrid+pending_x];ESP_LOGI(kTag,"LOOT_CELL_RECOMPOSE x=%d y=%d stack=0 underlying=%d final=%d",pending_x,pending_y,int(combat_.map.tiles[pending_y*openu5::kCombatGrid+pending_x]),final_tile);}}
    if((cmd.kind==openu5::CommandKind::CombatGet||cmd.kind==openu5::CommandKind::Get)&&pending_pickup_known){const auto decoded=openu5::decode_loot(pending_pickup);char name[96]{};openu5::loot_item_name(pending_pickup,name,sizeof(name));const int after=loot_field_value(game_,pending_pickup);if(after!=pending_pickup_before||decoded.category==openu5::LootCategory::QuestItem){ESP_LOGI(kTag,"LOOT_PICKUP source=get item_type=%s item_index=%d display_name=%s qty=%d",openu5::loot_category_name(decoded.category),int(decoded.item_index),name,int(decoded.quantity));ESP_LOGI(kTag,"LOOT_INVENTORY field=%s before=%d after=%d",openu5::loot_category_name(decoded.category),pending_pickup_before,after);}}
    if(cmd.kind==openu5::CommandKind::ViewGem){
        ESP_LOGI(kTag,"VIEW_COMMAND result=%s",status_name(result.status));
        ESP_LOGI(kTag,"VIEW_SELECTION type=gem");
        ESP_LOGI(kTag,"VIEW_RESOURCE type=gems before=%d after=%d action=view",view_gems_before,game_.gems);
    }
    if(ready_member>=0){const int now=equipped_in_slot(game_.party.characters[ready_member],ready_slot);const int old_pack=ready_current>=0&&ready_current<48?game_.equipment_quantities[ready_current]:0;const int new_pack=game_.equipment_quantities[ready_item];ESP_LOGI(kTag,"READY_AFTER member=%d slot=%s current=%d old_returned=%d inventory_new=%d",ready_member,equip_slot_name(ready_slot),now,old_pack,new_pack);if(result.status==openu5::CommandStatus::Success){char feedback[80]{};const char *name=openu5::equipment_display_name(ready_item);if(now==ready_item)std::snprintf(feedback,sizeof(feedback),"Equipped %s.",name?name:"item");else std::snprintf(feedback,sizeof(feedback),"Unready %s.",name?name:"item");ui_->append(openu5::UiTextChannel::System,feedback);}else if(result.item.message&&*result.item.message)ui_->append(openu5::UiTextChannel::System,result.item.message);}
    ESP_LOGI(kTag,"combat command=%d status=%s turns=%lld events=%lu time=%lu us",int(cmd.kind),status_name(result.status),(long long)result.turns,(unsigned long)result.event_count,(unsigned long)us);
    if(!combat_before&&combat_.initialized){
        for(int i=0;i<combat_.count;++i){const auto&a=combat_.actors[i];if(!a.enemy)continue;
            const int definition=a.enemy->index;const int sprite_family=a.enemy->tile>=0?a.enemy->tile:320+definition*4;
            ESP_LOGI(kTag,"ENEMY_ID phase=create actor=%ld definition=%d sprite_family=%d render_tile=%d name_index=%d name=\"%s\" res=v%u.%u/%08lx",
                     long(a.id),definition,sprite_family,int(a.render_tile),definition,
                     a.enemy->name?a.enemy->name:"",unsigned(alpha_report_.version_major),
                      unsigned(alpha_report_.version_minor),(unsigned long)alpha_report_.payload_crc32);
        }
        if(cmd.kind==openu5::CommandKind::TrollToll&&direct_troll_.active)trace_direct_troll_begin();
        save_combat_world_tile();
    }else if(combat_before){
        for(int i=0;i<combat_.count;++i){const auto&a=combat_.actors[i];if(!a.enemy||a.status!=openu5::CombatStatus::Dead)continue;
            bool newly_dead=false;for(int j=0;j<prior_count;++j)if(prior_ids[j]==a.id){newly_dead=prior_status[j]!=openu5::CombatStatus::Dead;break;}
            if(newly_dead){const int definition=a.enemy->index;const int sprite_family=a.enemy->tile>=0?a.enemy->tile:320+definition*4;
                ESP_LOGI(kTag,"ENEMY_ID phase=death actor=%ld definition=%d sprite_family=%d render_tile=%d name_index=%d displayed=\"%s\" res=v%u.%u/%08lx",
                         long(a.id),definition,sprite_family,int(a.render_tile),definition,
                         a.enemy->name?a.enemy->name:"",unsigned(alpha_report_.version_major),
                         unsigned(alpha_report_.version_minor),(unsigned long)alpha_report_.payload_crc32);
                const int cell=int(a.position.y)*openu5::kCombatGrid+int(a.position.x);
                const int encoded=cell>=0&&cell<openu5::kCombatCells?combat_.loot[cell]:0;
                const int render_tile=openu5::combat_loot_render_tile(int16_t(encoded));
                const char *resource=encoded==1||encoded==129?"Chest":encoded==30?"DeadBody":encoded==31?"Splat":"None";
                ESP_LOGI(kTag,"LOOT_CREATE actor=%ld def=%d encounter=%s encoded=%d trapped=%d contents=%d combat_x=%d combat_y=%d world_origin=%ld,%ld loc=%ld floor=%ld",long(a.id),definition,combat_.victory_context==&outdoor_?"roaming":"direct",encoded,encoded==129,cell>=0&&cell<openu5::kCombatCells?int(combat_.chest_contents[cell]):-1,int(a.position.x),int(a.position.y),long(combat_.loot_x),long(combat_.loot_y),long(combat_.encounter_location),long(combat_.encounter_floor));
                if(direct_troll_.active&&(encoded==1||encoded==129)){const auto mapped=openu5::combat_cell_to_world(combat_,a.position.x,a.position.y);ESP_LOGI(kTag,"CHEST_COORD_DERIVE combat=%d,%d arena_origin=%d,%d orientation=%d return_anchor=%ld,%ld offset=%d,%d result=%d,%d",
                    int(a.position.x),int(a.position.y),int(combat_.arena_origin_x),int(combat_.arena_origin_y),int(combat_.arena_entry),long(combat_.loot_x),long(combat_.loot_y),int(a.position.x)-int(combat_.arena_origin_x),int(a.position.y)-int(combat_.arena_origin_y),int(mapped.x),int(mapped.y));}
                ESP_LOGI(kTag,"CORPSE_RENDER actor=%ld definition=%d corpse_definition=%d tile=0x%03x resource=%s",
                          long(a.id),definition,encoded,render_tile,resource);}
        }
        log_combat_counts("player-command");
        if(combat_.ended)ESP_LOGI(kTag,"COMBAT_END reason=%s",combat_.victory?"victory/all-hostiles-gone":"party-escaped-or-lost");
    }
    if(result.status==openu5::CommandStatus::Unsupported||result.status==openu5::CommandStatus::InvalidContext||result.status==openu5::CommandStatus::NeedsStorage)ESP_LOGW(kTag,"Command failed status=%s kind=%d",status_name(result.status),int(cmd.kind));
    // Encounter teardown requires CommandContext::combat to remain true while
    // it copies HP/RNG back and emits CombatEnded.  Run it before deriving the
    // presentation flag from CombatState::ended, or a player-delivered final
    // blow would clear the flag and permanently skip teardown.
    if(!finish_combat_if_needed()){dirty_=true;return;}
    context_.combat=context_.combat_context&&context_.combat_context->combat.initialized&&!context_.combat_context->combat.ended;
    context_.dungeon=dungeon_.active;if(context_.combat)ui_->set_base_mode(openu5::UiMode::Combat);else if(context_.dungeon)ui_->set_base_mode(openu5::UiMode::Dungeon);
    terrain_.refresh(resources_.world,game_);dirty_=true;schedule_combat();
}

bool AlphaRuntime::combat_ai_turn(){
    if(!context_.combat||!combat_.initialized||combat_.ended)return false;
    auto *actor=context_.combat?openu5::current_combat_actor(combat_context_):nullptr;
    if(!actor)return false;
    ESP_LOGD(kTag,"combat state current=%ld actor=%ld member=%u charmed=%d ended=%d queued=%u",
             long(combat_.current),long(actor->id),unsigned(actor->member),actor->charmed,
             combat_.ended,unsigned(combat_input_count_));
    return actor->member==255||actor->charmed;
}

void AlphaRuntime::log_combat_counts(const char *reason) const{
    int alive_hostile=0,escaped=0,dead=0,active=0;
    for(int i=0;i<combat_.count;++i){const auto &a=combat_.actors[i];
        const bool live=combat_actor_live(a);active+=live?1:0;
        if(!a.enemy)continue;
        alive_hostile+=live?1:0;escaped+=a.status==openu5::CombatStatus::Fled?1:0;
        dead+=a.status==openu5::CombatStatus::Dead?1:0;
    }
    const bool scheduled=combat_.current>=0&&combat_.current<combat_.count&&combat_actor_live(combat_.actors[combat_.current]);
    ESP_LOGI(kTag,"COMBAT_COUNTS reason=%s alive_hostile=%d escaped=%d dead=%d active=%d scheduled=%d",
             reason,alive_hostile,escaped,dead,active,scheduled);
    ESP_LOGI(kTag,"COMBAT_END_CHECK reason=%s result=%s",reason,combat_.ended?"end":"continue");
}

void AlphaRuntime::trace_direct_troll_precombat(){
    direct_troll_={};direct_troll_.active=true;direct_troll_.map=game_.position.map;
    direct_troll_.trigger_x=commands_.troll_x;direct_troll_.trigger_y=commands_.troll_y;
    const auto sample=terrain_.inspect(resources_.world,direct_troll_.map,direct_troll_.trigger_x,direct_troll_.trigger_y);
    direct_troll_.bridge_tile=sample.effective;
    ESP_LOGI(kTag,"PRE_COMBAT_WORLD loc=%u floor=%d player_x=%u player_y=%u",unsigned(game_.position.map.location),int(game_.position.map.floor),unsigned(game_.position.xy.x),unsigned(game_.position.xy.y));
    ESP_LOGI(kTag,"PRE_COMBAT_TRIGGER x=%ld y=%ld tile=%ld terrain_override_present=%d terrain_override_tile=%ld",
             long(direct_troll_.trigger_x),long(direct_troll_.trigger_y),long(sample.effective),sample.transient||sample.persistent||sample.wiped,
             long(sample.transient?sample.transient_tile:sample.persistent?sample.persistent_tile:sample.wiped?terrain_.wipe_tile:openu5::kOffMap));
    constexpr int dx[]={0,1,-1,0,0},dy[]={0,0,0,1,-1};
    for(size_t i=0;i<sizeof(dx)/sizeof(*dx);++i){const int x=direct_troll_.trigger_x+dx[i],y=direct_troll_.trigger_y+dy[i];const auto n=terrain_.inspect(resources_.world,direct_troll_.map,x,y);
        ESP_LOGI(kTag,"PRE_COMBAT_TILE x=%d y=%d tile=%ld source=%s",x,y,long(n.effective),terrain_source(n));}
}

void AlphaRuntime::trace_direct_troll_begin(){
    ESP_LOGI(kTag,"DIRECT_TROLL_BEGIN loc=%ld floor=%ld",long(combat_.encounter_location),long(combat_.encounter_floor));
    ESP_LOGI(kTag,"DIRECT_TROLL_CAPTURE player_x=%u player_y=%u trigger_x=%ld trigger_y=%ld return_x=%ld return_y=%ld bridge_tile=%ld",
             unsigned(game_.position.xy.x),unsigned(game_.position.xy.y),long(direct_troll_.trigger_x),long(direct_troll_.trigger_y),
             long(combat_.loot_x),long(combat_.loot_y),long(direct_troll_.bridge_tile));
    ESP_LOGI(kTag,"DIRECT_TROLL_COORDS player=%u,%u blocked_target=%ld,%ld encounter_origin=%ld,%ld return_anchor=%ld,%ld combat_world_origin=%ld,%ld",
             unsigned(game_.position.xy.x),unsigned(game_.position.xy.y),long(direct_troll_.trigger_x),long(direct_troll_.trigger_y),
             long(game_.position.xy.x),long(game_.position.xy.y),long(combat_.loot_x),long(combat_.loot_y),long(combat_.loot_x),long(combat_.loot_y));
}

void AlphaRuntime::trace_direct_troll_tile(const char *stage) const{
    if(!direct_troll_.active)return;
    const auto s=terrain_.inspect(resources_.world,direct_troll_.map,direct_troll_.trigger_x,direct_troll_.trigger_y);
    ESP_LOGI(kTag,"%s trigger_x=%ld trigger_y=%ld authoritative_tile=%ld override_present=%d override_tile=%ld rendered_tile=%ld source=%s",
             stage,long(direct_troll_.trigger_x),long(direct_troll_.trigger_y),long(s.effective),s.transient||s.persistent||s.wiped,
             long(s.transient?s.transient_tile:s.persistent?s.persistent_tile:s.wiped?terrain_.wipe_tile:openu5::kOffMap),long(s.effective),terrain_source(s));
}

void AlphaRuntime::trace_direct_troll_save(const char *stage) const{
    if(!direct_troll_.active)return;
    const auto s=terrain_.inspect(resources_.world,direct_troll_.map,direct_troll_.trigger_x,direct_troll_.trigger_y);
    ESP_LOGI(kTag,"%s x=%ld y=%ld tile=%ld persistent_override_present=%d persistent_override_tile=%ld",
             stage,long(direct_troll_.trigger_x),long(direct_troll_.trigger_y),long(s.effective),s.persistent,long(s.persistent?s.persistent_tile:openu5::kOffMap));
}

void AlphaRuntime::terrain_write(void *p,const openu5::TerrainWrite &w){
    auto &r=*static_cast<AlphaRuntime*>(p);const auto before=r.terrain_.inspect(r.resources_.world,w.map,w.x,w.y);
    ESP_LOGI(kTag,"WORLD_TILE_WRITE caller=%s x=%ld y=%ld old=%ld new=%ld reason=%s permanent=%d loc=%u floor=%d",
             w.caller?w.caller:"unknown",long(w.x),long(w.y),long(before.effective),long(w.new_tile),w.caller?w.caller:"unknown",w.permanent,unsigned(w.map.location),int(w.map.floor));
}

void AlphaRuntime::save_combat_world_tile(){
    const openu5::MapId map{static_cast<openu5::LocationId>(combat_.encounter_location),
                            static_cast<openu5::FloorId>(combat_.encounter_floor)};
    const int x=combat_.loot_x,y=combat_.loot_y;
    const auto active=openu5::get_active_map(resources_.world,map);
    if(active.error!=openu5::Error::None){
        combat_world_tile_saved_=false;
        ESP_LOGW(kTag,"WORLD_PRE_COMBAT unavailable loc=%u floor=%d x=%d y=%d error=%d",
                 unsigned(map.location),int(map.floor),x,y,int(active.error));
        return;
    }
    const int raw=active.value.tile_at(x,y);
    combat_world_tile_before_=terrain_.effective(resources_.world,map,x,y);
    combat_world_tile_saved_=true;
    ESP_LOGI(kTag,"WORLD_PRE_COMBAT loc=%u floor=%d player=%u,%u origin=%d,%d",
             unsigned(map.location),int(map.floor),unsigned(game_.position.xy.x),unsigned(game_.position.xy.y),x,y);
    ESP_LOGI(kTag,"WORLD_TILE_SAVE x=%d y=%d tile=%d raw=%d source=authoritative-world+terrain",
             x,y,combat_world_tile_before_,raw);
}

void AlphaRuntime::log_combat_world_restore(){
    const openu5::MapId map{static_cast<openu5::LocationId>(combat_.encounter_location),
                            static_cast<openu5::FloorId>(combat_.encounter_floor)};
    const int x=combat_.loot_x,y=combat_.loot_y;
    const auto active=openu5::get_active_map(resources_.world,map);
    const int after=active.error==openu5::Error::None?terrain_.effective(resources_.world,map,x,y):openu5::kOffMap;
    ESP_LOGI(kTag,"WORLD_TILE_RESTORE x=%d y=%d source=authoritative-world+terrain before=%d after=%d unchanged=%d",
             x,y,combat_world_tile_before_,after,combat_world_tile_saved_&&combat_world_tile_before_==after);
    const auto player_map=openu5::get_active_map(resources_.world,game_.position.map);
    const int under=player_map.error==openu5::Error::None?
        terrain_.effective(resources_.world,game_.position.map,game_.position.xy.x,game_.position.xy.y):openu5::kOffMap;
    ESP_LOGI(kTag,"WORLD_POST_COMBAT player=%u,%u tile_under_player=%d loc=%u floor=%d",
             unsigned(game_.position.xy.x),unsigned(game_.position.xy.y),under,
             unsigned(game_.position.map.location),int(game_.position.map.floor));
    combat_world_tile_saved_=false;
}

bool AlphaRuntime::finish_combat_if_needed(){
    if(!context_.combat||!combat_.initialized||!combat_.ended)return true;
    if(direct_troll_.active){
        int pending=0;for(int cell=0;cell<openu5::kCombatCells;++cell)pending+=(combat_.loot[cell]==1||combat_.loot[cell]==129)?1:0;
        ESP_LOGI(kTag,"COMBAT_FINISH_BEGIN reason=%s",combat_.victory?"normal-victory-exit":"normal-loss-or-escape-exit");
        ESP_LOGI(kTag,"COMBAT_FINISH_STATE loc=%ld floor=%ld player_return=%u,%u trigger=%ld,%ld pending_chests=%d world_object_count=%u",
                 long(combat_.encounter_location),long(combat_.encounter_floor),unsigned(game_.position.xy.x),unsigned(game_.position.xy.y),
                 long(direct_troll_.trigger_x),long(direct_troll_.trigger_y),pending,unsigned(quest_.count?quest_.count(quest_.context):0));
        trace_direct_troll_tile("WORLD_FINISH_PRE_REBIND");
    }
    ESP_LOGI(kTag,"WORLD_RESTORE_BEGIN loc=%ld floor=%ld origin=%ld,%ld",
             long(combat_.encounter_location),long(combat_.encounter_floor),
             long(combat_.loot_x),long(combat_.loot_y));
    const size_t objects_before=quest_.count?quest_.count(quest_.context):0;
    for(int cell=0;cell<openu5::kCombatCells;++cell)if(combat_.loot[cell]==1||combat_.loot[cell]==129){const auto mapped=openu5::combat_cell_to_world(combat_,cell%openu5::kCombatGrid,cell/openu5::kCombatGrid);ESP_LOGI(kTag,"CHEST_PRE_TEARDOWN source=%s loc=%ld floor=%ld world_x=%d world_y=%d combat_x=%d combat_y=%d encoded=%d trap=%d contents=%d collection_count=%u",combat_.victory_context==&outdoor_?"roaming-latch":"direct-teardown",long(combat_.encounter_location),long(combat_.encounter_floor),int(mapped.x),int(mapped.y),cell%openu5::kCombatGrid,cell/openu5::kCombatGrid,int(combat_.loot[cell]),combat_.loot[cell]==129,int(combat_.chest_contents[cell]),unsigned(objects_before));}
    const auto status=openu5::finish_encounter_combat(context_,combat_);
    ESP_LOGI(kTag,"combat finish result=%d victory=%d",int(status),combat_.victory);
    if(status!=openu5::CombatResult::Ok){
        ESP_LOGE(kTag,"COMBAT_TEARDOWN deferred result=%d",int(status));
        return false;
    }
    const size_t objects_after=quest_.count?quest_.count(quest_.context):0;
    for(size_t i=objects_before;i<objects_after;++i){const auto o=quest_.read(quest_.context,i);if(o.chest)
        ESP_LOGI(kTag,"CHEST_WORLD create source=combat actor=-1 encoded=%d loc=%ld floor=%ld xy=%ld,%ld object_id=%lu flags=chest%s",
                 o.contents,long(o.location),long(o.floor),long(o.x),long(o.y),static_cast<unsigned long>(i),o.trapped?"|trapped":"");}
    context_.combat=false;next_enemy_step_us_=0;combat_input_count_=0;
    terrain_.refresh(resources_.world,game_);
    trace_direct_troll_tile("WORLD_FINISH_POST_REBIND");
    log_combat_world_restore();
    ui_->set_base_mode(dungeon_.active?openu5::UiMode::Dungeon:openu5::UiMode::Exploration);
    for(size_t i=0;i<objects_.size();++i){const auto&o=objects_[i];if(o.chest)ESP_LOGI(kTag,"CHEST_POST_COMBAT loc=%ld floor=%ld x=%ld y=%ld object_id=%u present=1 player_loc=%u player_floor=%d player_x=%u player_y=%u",long(o.location),long(o.floor),long(o.x),long(o.y),unsigned(i),unsigned(game_.position.map.location),int(game_.position.map.floor),unsigned(game_.position.xy.x),unsigned(game_.position.xy.y));}
    if(direct_troll_.active){
        const auto s=terrain_.inspect(resources_.world,direct_troll_.map,direct_troll_.trigger_x,direct_troll_.trigger_y);
        ESP_LOGI(kTag,"POST_COMBAT_WORLD loc=%u floor=%d player_x=%u player_y=%u",unsigned(game_.position.map.location),int(game_.position.map.floor),unsigned(game_.position.xy.x),unsigned(game_.position.xy.y));
        ESP_LOGI(kTag,"POST_COMBAT_TRIGGER x=%ld y=%ld tile=%ld terrain_override_present=%d terrain_override_tile=%ld",
                 long(direct_troll_.trigger_x),long(direct_troll_.trigger_y),long(s.effective),s.transient||s.persistent||s.wiped,
                 long(s.transient?s.transient_tile:s.persistent?s.persistent_tile:s.wiped?terrain_.wipe_tile:openu5::kOffMap));
    }
    dirty_=true;
    return true;
}

void AlphaRuntime::schedule_combat(){
    if(!finish_combat_if_needed())return;
    if(!context_.combat)return;
    auto *actor=openu5::current_combat_actor(combat_context_);
    if(!actor){
        // Batch 9D.  No actor will ever be schedulable again, so no beat can be
        // armed, nothing is queued, and every combat command silently succeeds
        // having done nothing -- the arena keeps the screen and the keyboard
        // and answers neither.  Close it through the core's own end path and
        // run the ordinary teardown, so recovery costs no keypress.
        openu5::close_stranded_combat(combat_context_);
        if(combat_.ended){
            ESP_LOGW(kTag,"COMBAT_STRANDED reason=no-schedulable-actor count=%ld victory=%d -- closing",
                     long(combat_.count),combat_.victory);
            finish_combat_if_needed();
            dirty_=true;
        }
        return;
    }
    if(actor->member!=255&&!actor->charmed){
        const int range=std::max<int32_t>(1,actor->range);
        int initial_x=actor->position.x,initial_y=actor->position.y;
        if(actor->last_target>=0)for(int i=0;i<combat_.count;++i){const auto &candidate=combat_.actors[i];
            if(candidate.id==actor->last_target&&combat_actor_live(candidate)&&
               openu5::combat_distance(actor->position.x-candidate.position.x,
                                       actor->position.y-candidate.position.y)<=range){
                initial_x=candidate.position.x;initial_y=candidate.position.y;break;
            }}
        ui_->set_combat_aim(actor->position.x,actor->position.y,int16_t(range),
                            int16_t(initial_x),int16_t(initial_y));
        next_enemy_step_us_=0;
        return;
    }
    if(!next_enemy_step_us_)next_enemy_step_us_=esp_timer_get_time()+kEnemyBeatUs;
}

void AlphaRuntime::service_combat(){
    if(!finish_combat_if_needed())return;
    if(!context_.combat)return;
    schedule_combat();
    if(next_enemy_step_us_&&esp_timer_get_time()>=next_enemy_step_us_){
        openu5::Command c;c.kind=openu5::CommandKind::CombatEnemyStep;
        const int tracked=std::min<int>(combat_.count,64);
        openu5::CombatStatus prior_status[64]{};openu5::CombatPoint prior_position[64]{};
        for(int i=0;i<tracked;++i){prior_status[i]=combat_.actors[i].status;prior_position[i]=combat_.actors[i].position;}
        const auto before=combat_.current;const int64_t start=esp_timer_get_time();
        const auto result=openu5::dispatch_world_command(context_,c);
        command_high_us_=std::max(command_high_us_,uint32_t(esp_timer_get_time()-start));
        ESP_LOGI(kTag,"combat enemy-step actor-before=%ld status=%s actor-after=%ld ended=%d",
                  long(before),status_name(result.status),long(combat_.current),combat_.ended);
        for(int i=0;i<tracked;++i){const auto &a=combat_.actors[i];
            if(!a.enemy||prior_status[i]==openu5::CombatStatus::Fled||a.status!=openu5::CombatStatus::Fled)continue;
            ESP_LOGI(kTag,"ENEMY_MOVE actor=%ld from=%d,%d to=off-map",
                     long(a.id),int(prior_position[i].x),int(prior_position[i].y));
            ESP_LOGI(kTag,"ENEMY_EXIT actor=%ld def=%d reason=off-map disposition=escaped",
                     long(a.id),a.enemy->index);
            const bool scheduled=combat_.current==i&&combat_actor_live(a);
            ESP_LOGI(kTag,"ENEMY_STATE actor=%ld alive=0 escaped=1 active=0 scheduled=%d",
                     long(a.id),scheduled);
        }
        log_combat_counts("enemy-step");
        if(combat_.ended)ESP_LOGI(kTag,"COMBAT_END reason=%s",combat_.victory?"victory/all-hostiles-gone":"party-escaped-or-lost");
        next_enemy_step_us_=0;dirty_=true;if(!finish_combat_if_needed())return;schedule_combat();
    }
    if(context_.combat&&!combat_ai_turn()&&combat_input_count_){
        const auto pending=combat_input_queue_[0];
        for(size_t i=1;i<combat_input_count_;++i)combat_input_queue_[i-1]=combat_input_queue_[i];
        --combat_input_count_;
        ESP_LOGD(kTag,"combat drain queued action=%s remaining=%u",action_name(pending.kind),unsigned(combat_input_count_));
        ui_->handle_input(pending);dirty_=true;
    }
}

void AlphaRuntime::modal(const openu5::UiIntent&i){if(!i.value.accepted){if(i.request==openu5::UiRequestId::Party)pending_order_from_=-1;if(i.request==openu5::UiRequestId::EquipmentMember||i.request==openu5::UiRequestId::Equipment)pending_ready_member_=-1;if(i.request==openu5::UiRequestId::UseTarget||i.request==openu5::UiRequestId::Inventory)pending_use_item_=-1;if(i.request==openu5::UiRequestId::Target)pending_combat_spell_=-1;if(i.request==openu5::UiRequestId::ShrineVisit||i.request==openu5::UiRequestId::ShrineRestore){shrine_.visit=shrine_.restore=-1;shrine_virtue_length_=0;}
    // FountainDrink (R-09 E): pure flavour text, no command, no HP/state/turn.
    if(i.request==openu5::UiRequestId::FountainDrink)ui_->append(openu5::UiTextChannel::Message,openu5::fountain_drink_result(0,true));
    // R-25 (Batch 19). Cancelling the kernel 0x4988 picker is not "nothing":
    // the routine leaves with -1 and the common epilogue at @0x4a5f prints
    // DS 0xa3da "None!" -- the SAME string and the same exit the zero-eligible
    // branch takes. The command is then abandoned: no vision, the parked
    // Search is dropped unaimed, and the spell menu never opens.
    if(i.request==openu5::UiRequestId::CrystalBall||i.request==openu5::UiRequestId::SearchMember||i.request==openu5::UiRequestId::CastMember){
        if(i.request==openu5::UiRequestId::SearchMember)pending_search_active_=false;
        if(i.request==openu5::UiRequestId::CastMember)pending_caster_=-1;
        ui_->append(openu5::UiTextChannel::Message,openu5::command_char_none());
    }
    return;}openu5::Command c;
    if(i.request==openu5::UiRequestId::Party){if(pending_order_from_<0){pending_order_from_=int16_t(i.value.index);open_selection(openu5::UiMode::PartySelection,openu5::UiRequestId::Party);}else{c.kind=openu5::CommandKind::NewOrder;c.member=pending_order_from_;c.item=int16_t(i.value.index);pending_order_from_=-1;command(c);}}
    // R-22. select_player (ZSTATS.OVL:0x0000) runs FIRST, and the member it
    // returns opens the page axis at member*2. Before Batch 14 this arm
    // reopened the very same picker, which is why (Z) had no pages at all.
    else if(i.request==openu5::UiRequestId::Status&&i.value.index>=0&&size_t(i.value.index)<selection_count_)open_zstats(selections_[i.value.index].value);
    else if(i.request==openu5::UiRequestId::EquipmentMember&&i.value.index>=0&&size_t(i.value.index)<selection_count_){pending_ready_member_=selections_[i.value.index].value;open_selection(openu5::UiMode::EquipmentSelection,openu5::UiRequestId::Equipment);}
    else if(i.request==openu5::UiRequestId::Inventory&&i.value.index>=0&&size_t(i.value.index)<selection_count_){pending_use_item_=selections_[i.value.index].value;c.kind=openu5::CommandKind::UseItem;c.item=pending_use_item_;if(!context_.combat&&(c.item==6||(c.item>=8&&c.item<16))){open_selection(openu5::UiMode::PartySelection,openu5::UiRequestId::UseTarget);}else if(!context_.combat&&(c.item==1||c.item==17)){pending_use_item_=-1;ui_->begin_target(openu5::UiRequestId::UseTarget,"Direction?",c);dirty_=true;}else{pending_use_item_=-1;command(c);}}
    else if(i.request==openu5::UiRequestId::UseTarget&&pending_use_item_>=0){c.kind=openu5::CommandKind::UseItem;c.item=pending_use_item_;c.member=int16_t(i.value.index);pending_use_item_=-1;command(c);}
    else if(i.request==openu5::UiRequestId::Equipment&&i.value.index>=0&&size_t(i.value.index)<selection_count_){
        // Keep the member selection alive.  command() reports the authoritative
        // result; reopening builds rows from the just-mutated equipment fields.
        c.kind=openu5::CommandKind::Ready;c.member=pending_ready_member_>=0?pending_ready_member_:int16_t(active_member(game_));c.item=selections_[i.value.index].value;command(c);
        open_selection(openu5::UiMode::EquipmentSelection,openu5::UiRequestId::Equipment);
    }
    else if(i.request==openu5::UiRequestId::Spell&&i.value.index>=0&&size_t(i.value.index)<selection_count_){cast_selected_spell(selections_[i.value.index].value);}
    // R-25 (Batch 19). "selectedCombatPlayer" only exists in combat, which is
    // branch 1 of kernel 0x4988 (@0x4995): the caster is the ACTING combatant,
    // never the arrow-marked active member. Same correction cast_selected_spell()
    // carries; this arm rebuilds the command from scratch, so it needs it too.
    else if(i.request==openu5::UiRequestId::Target){if(pending_combat_spell_>=0){auto *caster=openu5::current_combat_actor(combat_context_);c.kind=openu5::CommandKind::Cast;c.caster=caster&&caster->member!=255?int16_t(caster->member):int16_t(active_member(game_));c.item=pending_combat_spell_;c.member=int16_t(i.value.index);pending_combat_spell_=-1;command(c);}}
    else if(i.request==openu5::UiRequestId::Custom&&i.value.index>=0&&size_t(i.value.index)<selection_count_){c.kind=openu5::CommandKind::Mix;c.item=selections_[i.value.index].value;c.hours=1;auto*d=openu5::spell_definition(openu5::SpellId(c.item));c.reagent_mask=d?d->reagents:0;command(c);}
    else if(i.request==openu5::UiRequestId::TrollToll){c.kind=openu5::CommandKind::TrollToll;c.member=i.value.yes?1:0;command(c);}
    // R-25 (Batch 19). Was a yes/no arm that dispatched with Command::member
    // left at its never-set -1, which look.cpp rejected outright -- the whole
    // feature was dead. It is now the answer path of the 0x4988 roster picker,
    // and it shares the branch-4 gate with the other two callers: an ineligible
    // pick is a re-ask, so nothing is dispatched. What the CORE still owns is
    // everything after the member exists -- the roll, the comparison, the
    // damage and the gem view.
    else if(i.request==openu5::UiRequestId::CrystalBall&&i.value.index>=0&&size_t(i.value.index)<selection_count_){
        const int16_t member=selections_[i.value.index].value;
        if(accept_command_char_pick(openu5::UiRequestId::CrystalBall,member)){c.kind=openu5::CommandKind::CrystalBall;c.member=member;command(c);}
    }
    // R-25 (Batch 19). The answer paths of the other two 0x4988 callers. Both
    // apply the branch-4 gate first: an ineligible pick is a re-ask, so the
    // parked work stays parked and nothing is dispatched.
    else if(i.request==openu5::UiRequestId::SearchMember&&i.value.index>=0&&size_t(i.value.index)<selection_count_){
        const int16_t member=selections_[i.value.index].value;
        if(accept_command_char_pick(openu5::UiRequestId::SearchMember,member)&&pending_search_active_){
            openu5::Command search=pending_search_;search.member=member;pending_search_active_=false;command(search);
        }
    }
    else if(i.request==openu5::UiRequestId::CastMember&&i.value.index>=0&&size_t(i.value.index)<selection_count_){
        const int16_t member=selections_[i.value.index].value;
        if(accept_command_char_pick(openu5::UiRequestId::CastMember,member)){
            pending_caster_=member;open_selection(openu5::UiMode::SpellSelection,openu5::UiRequestId::Spell);
        }
    }
    // R-26 (Batch 18). BOTH answers dispatch, and the answer travels in
    // Command::member -- the same shape the TrollToll arm above uses.
    // Previously only Yes dispatched (so game.ts dropCoin(false)'s "No" echo
    // never printed), and Yes only "worked" because the default member == -1
    // happens to be truthy in look.cpp's `cmd.member ? "Yes\n" : "No\n"`.
    else if(i.request==openu5::UiRequestId::WellDrop){c.kind=openu5::CommandKind::DropCoin;c.member=i.value.yes?1:0;command(c);}
    else if(i.request==openu5::UiRequestId::WellWish){c.kind=openu5::CommandKind::MakeWish;c.text=i.value.text;c.text_length=i.value.text_length;command(c);}
    else if(i.request==openu5::UiRequestId::ShrineVisit){shrine_virtue_length_=0;ui_->begin_text(openu5::UiRequestId::ShrineRestore,"Virtue?",15);}
    else if(i.request==openu5::UiRequestId::ShrineRestore){if(!shrine_virtue_length_){shrine_virtue_length_=std::min<size_t>(i.value.text_length,63);std::memcpy(shrine_virtue_,i.value.text,shrine_virtue_length_*sizeof(char16_t));shrine_virtue_[shrine_virtue_length_]=0;ui_->begin_text(openu5::UiRequestId::ShrineRestore,"Mantra?",15);}else{openu5::TalkText mantras[3]={{i.value.text,i.value.text_length},{i.value.text,i.value.text_length},{i.value.text,i.value.text_length}};openu5::ShrineInput input;input.action=shrine_.visit>=0?openu5::ShrineAction::SubmitVisit:openu5::ShrineAction::SubmitRestore;input.virtue={shrine_virtue_,shrine_virtue_length_};input.mantras={mantras,3};c.kind=openu5::CommandKind::ShrineAction;c.shrine=&input;shrine_virtue_length_=0;command(c);}}
    else if(i.request==openu5::UiRequestId::ShrineDonate){openu5::ShrineInput input;input.action=openu5::ShrineAction::Donate;input.value=i.value.number;c.kind=openu5::CommandKind::ShrineAction;c.shrine=&input;command(c);}
    // FountainDrink (R-09 E): resolve the picked party member's status against
    // the runtime-owned roster and print the reference's pure flavour line.
    // No command, no HP/state mutation, no turn -- UiSession already entered
    // and exited PartySelection on its own; this only supplies the text.
    else if(i.request==openu5::UiRequestId::FountainDrink&&i.value.index>=0&&size_t(i.value.index)<selection_count_){
        const int16_t member=selections_[i.value.index].value;
        const char status=member>=0&&member<game_.party.character_count?game_.party.characters[member].status:'G';
        ui_->append(openu5::UiTextChannel::Message,openu5::fountain_drink_result(status,false));
    }
}

openu5::CommandCharOutcome AlphaRuntime::resolve_command_char_or_prompt(openu5::UiRequestId request,int16_t &member){
    const auto pick=openu5::resolve_command_char(game_.party);
    if(pick.outcome==openu5::CommandCharOutcome::Resolved){member=int16_t(pick.member);return pick.outcome;}
    if(pick.outcome==openu5::CommandCharOutcome::None){
        // Zero eligible: the epilogue at @0x4a5f prints DS 0xa3da and the
        // command is over -- no prompt, no effect, no turn.
        ui_->append(openu5::UiTextChannel::Message,openu5::command_char_none());dirty_=true;return pick.outcome;
    }
    open_selection(openu5::UiMode::PartySelection,request); // @0x4a02 "Player: "
    return pick.outcome;
}

bool AlphaRuntime::accept_command_char_pick(openu5::UiRequestId request,int16_t member){
    if(openu5::command_char_accepts(game_.party,member))return true;
    ui_->append(openu5::UiTextChannel::Message,openu5::command_char_disabled()); // @0x4a4e
    open_selection(openu5::UiMode::PartySelection,request);                      // @0x4a57: di still 0.
    dirty_=true;return false;
}

void AlphaRuntime::cast_selected_spell(int16_t spell){
    openu5::Command c;c.kind=openu5::CommandKind::Cast;c.item=spell;
    // R-25 (Batch 19). Branch 1 of kernel 0x4988 (@0x4995): in combat
    // g_location is 0xFF, so the routine never asks -- the caster is the
    // ACTING combatant, read as field +3 of g_combat_actor_records[g_cmb_actor]
    // (COMBAT.OVL:0x08f0 sets si = g_cmb_actor, then 0x095e re-enters the same
    // CAST.OVL:0x0dba handler the kernel's 'C' uses). Native used the ACTIVE
    // member here, which is whoever the player marked with the arrow, not
    // whose turn it is -- so a combat cast could spend the wrong character's
    // magic points. Outside combat pending_caster_ carries the member the
    // picker resolved before the spell menu opened (CAST.OVL:0x0dd5 runs
    // BEFORE the "Spell name:" prompt).
    auto *actor=openu5::current_combat_actor(combat_context_);
    if(context_.combat)c.caster=actor&&actor->member!=255?int16_t(actor->member):int16_t(active_member(game_));
    else c.caster=pending_caster_>=0?pending_caster_:int16_t(active_member(game_));
    pending_caster_=-1;
    // Y-33. hours defaults to 0, a VALID Vas Rel Por phase -- every other
    // spell ignores this field, so setting the "no phase chosen" sentinel
    // unconditionally is harmless and closes the gap where an aboard-ship
    // cast (cast_target_prompt returns None, skipping the WorldPhase case
    // below entirely) would otherwise fall through to command(c) with
    // hours=0 and fire the ceremony on a cast the ship should silently fail.
    c.hours=-1;
    const auto *def=openu5::spell_definition(openu5::SpellId(spell));
    if(!def){command(c);return;}
    const char *target=def->target_type?def->target_type:"";
    if(std::strcmp(target,"selectedCombatPlayer")==0){
        pending_combat_spell_=spell;
        open_selection(openu5::UiMode::PartySelection,openu5::UiRequestId::Target);
        return;
    }
    if(std::strcmp(target,"castingCombatPlayer")==0){
        c.member=actor&&actor->member!=255?actor->member:int16_t(active_member(game_));
        command(c);return;
    }
    // Which prompt this cast owes the player, in THIS context, is the shared
    // openu5::cast_target_prompt() seam (R-11 / Batch 5) rather than an
    // inline predicate, so the host suite can drive the same decision this
    // ESP-only function makes.
    switch(openu5::cast_target_prompt(openu5::SpellId(spell),context_.combat,dungeon_.active,
                                       game_.transport==openu5::TransportMode::Ship)){
    case openu5::CastTargetPrompt::CombatReticle:{
        const int16_t x=actor?actor->position.x:5,y=actor?actor->position.y:5;
        ui_->begin_target(openu5::UiRequestId::Target,"Spell aim",c,x,y);
        dirty_=true;return;
    }
    case openu5::CastTargetPrompt::WorldDirection:
        // The ordinary world getdir, identical to every other directional
        // world command (Talk/Open/Push/Klimb) -- UiRequestId::Direction with
        // no reticle cell, so one direction press dispatches the Cast with
        // has_direction set and Cancel dispatches it without one.
        ui_->begin_target(openu5::UiRequestId::Direction,"Direction?",c,-1,-1);
        dirty_=true;return;
    case openu5::CastTargetPrompt::WorldPhase:
        // Y-33. Vas Rel Por's bare "To phase:" getkey. c.hours is already
        // the -1 "no phase chosen" sentinel set above; a valid '1'-'8'
        // overwrites it, and any abort path (bad key, Cancel) leaves it be.
        ui_->begin_target(openu5::UiRequestId::GatePhase,"To phase:",c,-1,-1);
        dirty_=true;return;
    case openu5::CastTargetPrompt::None:
        break;
    }
    command(c);
}

size_t AlphaRuntime::selection_count(void*p){return static_cast<AlphaRuntime*>(p)->selection_count_;}
openu5::UiSelectionItem AlphaRuntime::selection_item(void*p,size_t i){auto&r=*static_cast<AlphaRuntime*>(p);return i<r.selection_count_?openu5::UiSelectionItem{r.selections_[i].label,r.selections_[i].enabled}:openu5::UiSelectionItem{};}
// Mirrors the two narrow world facts UiSession's exploration key routing needs
// into the session, straight from the authoritative owners.  Called immediately
// before each input is routed, so the session can never act on a stale mirror
// (R-19/R-20).
void AlphaRuntime::refresh_session_context(){
    // Sails: the frigate transport range 0x20-0x27 and the Underworld cut-off,
    // the same pair CommandKind::YellSails re-checks in commands.cpp.
    ui_->set_sail_context((turn_.transport_tile&0xf8)==0x20,game_.position.map.location<0x80);
    // Harpsichord: small map only, never in combat or a dungeon, with the
    // harpsichord tile (141 / 0x8D) immediately SOUTH of the party -- the
    // party sits on the chair north of the instrument facing it.  Matches
    // Game.harpsichordSeated() (game/src/core/game.ts).
    bool seated=false;
    if(!context_.combat&&!dungeon_.active&&game_.position.map.location){
        const int y=int(game_.position.xy.y)+1;
        seated=terrain_.effective(resources_.world,game_.position.map,game_.position.xy.x,y)==141;
    }
    ui_->set_harpsichord_active(seated);
    // Dungeon prompts (Batch 9B's two mirrors, wired up in Batch 9D): whether
    // the cell under the party offers a Klimb BOTH ways, and whether it is a
    // fountain.  Same narrow-mirror contract as the two above, pushed through
    // the shared ui_mode_policy.h seam the host suite drives, so the device and
    // dungeon_input_regression/dungeon_combat_regression cannot disagree about
    // when "Klimb-U/D-" and "Will you drink?" appear.
    publish_dungeon_prompt_context(*ui_,game_,dungeon_,context_.dungeon&&dungeon_.active);
    // Transcript page geometry (Batch 4.5C, GAMEPLAY_INTEGRATION_AUDIT.md
    // 4.5C): mirrors whichever transcript-bearing panel is currently showing
    // -- shop log, selector log, or the world/dialogue running log -- so
    // Shift+Up/Shift+Down page by the row/column count actually on screen
    // instead of UiSession's fixed constructor defaults. The world panel's
    // geometry varies with the text-size setting and with whether a context
    // bar is reserving space at the bottom; world_transcript_geometry()
    // (tdeck_board.h) is the same helper Board::render() uses to lay out
    // that panel, so the two can never drift apart.
    size_t transcript_columns=openu5::kHudTranscriptColumns,transcript_rows=kShopLogRows;
    openu5::UiSelectionView selection_probe{};
    if(ui_->mode()==openu5::UiMode::Shop){
        // Defaults above already match the shop log strip.
    }else if(ui_->selection_view(selection_probe)){
        transcript_rows=kSelectorLogRows;
    }else{
        const auto mode=ui_->mode();
        const bool context_active=mode==openu5::UiMode::TargetSelection||mode==openu5::UiMode::TextEntry||
                                   mode==openu5::UiMode::NumericEntry||mode==openu5::UiMode::YesNo;
        world_transcript_geometry(settings_.ui_size,context_active,transcript_columns,transcript_rows);
    }
    ui_->set_transcript_view_metrics(transcript_columns,transcript_rows);
}

void AlphaRuntime::open_selection(openu5::UiMode mode,openu5::UiRequestId request){selection_count_=0;selection_request_=request;
    auto add=[&](int value,const char*name,int qty=1,bool equipped=false){if(selection_count_>=64)return;auto&s=selections_[selection_count_++];s.value=int16_t(value);s.enabled=name&&*name;if(!name||!*name){std::snprintf(s.label,sizeof(s.label),"Unresolved id %d",value);ESP_LOGE(kTag,"UNRESOLVED_NAME selection=%d request=%d",value,int(request));}else format_ready_row(s.label,sizeof(s.label),name,uint16_t(std::max(qty,0)),equipped);};
    if(mode==openu5::UiMode::PartySelection){for(int i=0;i<game_.party.party_size&&i<game_.party.character_count;++i){auto&s=selections_[selection_count_++];s.value=int16_t(i);s.enabled=true;std::snprintf(s.label,sizeof(s.label),"%d %.9s HP%u",i+1,game_.party.characters[i].name,unsigned(game_.party.characters[i].current_hp));}}
    else if(mode==openu5::UiMode::InventorySelection){for(int i=0;i<8;++i)if(game_.potion_quantities[i])add(8+i,openu5::potion_display_name(i),game_.potion_quantities[i]);for(int i=0;i<8;++i)if(game_.scroll_quantities[i])add(i,openu5::scroll_display_name(i),game_.scroll_quantities[i]);
        // Possession gates for the usable-item rows, each read from its
        // authoritative owner: GameState counters/flags, QuestState artifacts and
        // shards, and QuestWorldServices' per-phase moonstones (owned == not
        // buried).  The row selection itself lives in the shared, ESP-free
        // openu5::usable_item_picker_rows() seam that the host Batch 3 Group B
        // test drives, so device and host cannot diverge (R-07/R-08).
        openu5::UsableItemPickerInput usable_input{};usable_input.magic_carpets=game_.magic_carpets;usable_input.skull_keys=game_.skull_keys;usable_input.grapple=game_.grapple;usable_input.spyglass=game_.spyglass;usable_input.sextant=game_.sextant;usable_input.wooden_box=game_.wooden_box;
        for(int a=0;a<3;++a)usable_input.artifacts[a]=game_.quest.artifacts[a];
        for(int a=0;a<3;++a)usable_input.shards[a]=game_.quest.shards[a];
        usable_input.hms_cape=game_.hms_cape;usable_input.black_badge=game_.black_badge;
        for(size_t m=0;m<8&&m<quest_.moonstone_count;++m)usable_input.moonstone_owned[m]=quest_.moonstones&&!quest_.moonstones[m].buried;
        const auto usable_rows=openu5::usable_item_picker_rows(usable_input);
        for(size_t ui=0;ui<usable_rows.count;++ui)add(usable_rows.rows[ui].id,usable_rows.rows[ui].name,usable_rows.rows[ui].quantity);}
    else if(mode==openu5::UiMode::EquipmentSelection){const int member=pending_ready_member_>=0?pending_ready_member_:active_member(game_);const auto ready=openu5::ready_items(game_,member);for(int n=0;n<ready.count;++n){const int i=ready.ids[n];add(i,openu5::equipment_display_name(i),game_.equipment_quantities[i],openu5::is_item_equipped(game_.party.characters[member],i));}}
    else if(mode==openu5::UiMode::SpellSelection){for(int i=0;i<48;++i)if(request==openu5::UiRequestId::Custom||game_.spell_quantities[i]>0)add(i,openu5::spell_display_name(i),game_.spell_quantities[i]);}
    if(!selection_count_){auto&s=selections_[selection_count_++];std::snprintf(s.label,sizeof(s.label),"(None available)");s.enabled=false;}
    // R-25 (Batch 19): the 0x4988 callers carry DS 0xa3c4 "Player: " from the
    // shared seam, so device and host name the prompt from one definition.
    const char *prompt=mode==openu5::UiMode::PartySelection?(request==openu5::UiRequestId::EquipmentMember?"Ready whom?":request==openu5::UiRequestId::UseTarget?"Use on whom?":request==openu5::UiRequestId::FountainDrink?"Who will drink?":request==openu5::UiRequestId::CrystalBall||request==openu5::UiRequestId::SearchMember||request==openu5::UiRequestId::CastMember?openu5::command_char_prompt():"Party"):mode==openu5::UiMode::InventorySelection?"Use item":mode==openu5::UiMode::EquipmentSelection?"Ready":"Spell";
    const size_t initial=mode==openu5::UiMode::PartySelection?size_t(request==openu5::UiRequestId::Status&&status_member_>=0?status_member_:active_member(game_)):0;
    ui_->begin_selection(mode,request,prompt,{this,selection_count,selection_item},initial);
}

// R-10 deferred drain. Runs once the outer input has fully unwound (called
// from handle(), beside synchronize_after_debug()), never nested inside the
// command() call that produced the pending initiation.
void AlphaRuntime::drain_pending_npc_initiation(){
    if(pending_npc_initiation_==PendingNpcInitiation::None)return;
    const auto kind=pending_npc_initiation_;
    const auto slot=pending_npc_slot_;
    const auto location=pending_npc_location_;
    pending_npc_initiation_=PendingNpcInitiation::None;
    pending_npc_slot_=-1;
    pending_npc_location_=0;
    // Current map/location must still be the captured one -- a stale
    // initiation (e.g. a teleport landed mid-cycle) is dropped, never
    // deferred across another world turn.
    if(game_.position.map.location!=location)return;
    // The UI must not already be inside an incompatible active session or
    // modal -- BeginConversation only makes sense from plain Exploration.
    if(ui_->mode()!=openu5::UiMode::Exploration)return;
    openu5::NpcActor *npc=nullptr;
    for(size_t i=0;i<actors_.count;++i){
        auto &a=actors_.actors[i];
        if(a.location==location&&a.schedule.slot==uint8_t(slot)){npc=&a;break;}
    }
    if(!npc)return; // the slot no longer resolves to the intended NPC.
    // The emitter's dialog window (0x80..0xFC) is wider than the shop
    // handoff BeginConversation actually supports (0x81..0x88, batch 4
    // adjudication section C). Rather than inventing a fallback shop or
    // falling through to "Funny, no response!", safely drop an
    // out-of-range shop initiation.
    if(kind==PendingNpcInitiation::Shop&&(npc->schedule.dialog<0x81||npc->schedule.dialog>0x88))return;
    openu5::Command begin;
    begin.kind=openu5::CommandKind::BeginConversation;
    begin.member=int16_t(npc->schedule.slot);
    command(begin);
}
void AlphaRuntime::synchronize_after_debug(openu5::WorldPosition before,bool dungeon_before){
    const bool moved=before.map.location!=game_.position.map.location||before.map.floor!=game_.position.map.floor||before.xy.x!=game_.position.xy.x||before.xy.y!=game_.position.xy.y;
    // The portable picker mutates the authoritative owners.  Rebind the device
    // presentation explicitly on every debug action. A dungeon entry preserves
    // the surface return position, so position-only change detection previously
    // left a stale base mode behind.
    context_.dungeon=dungeon_.active;context_.combat=combat_.initialized&&!combat_.ended;
    terrain_.refresh(resources_.world,game_);
    // Mode arbitration lives in ui_mode_policy.h so the host suite exercises
    // the same session-preserving rule production uses.
    ui_->set_base_mode(resolve_synchronized_base_mode(ui_->base_mode(),context_.combat,dungeon_.active));
    ESP_LOGI(kTag,"debug teleport rebind moved=%d prior_dungeon=%d location=%u floor=%d xy=%u,%u dungeon=%d",
             moved,dungeon_before,
             unsigned(game_.position.map.location),int(game_.position.map.floor),
             unsigned(game_.position.xy.x),unsigned(game_.position.xy.y),dungeon_.active);
    dirty_=true;
}

DeviceDebugScreen AlphaRuntime::debug_screen() const{
    DeviceDebugScreen out{};
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    if(!debug_)return out;
    const auto v=debug_->view();
    if(v.confirming&&v.confirming_sheet){
        // Batch 4.5A-4 Part 1/13: the one shared preset/Certification
        // confirm/effect sheet. Core hands over the exact display name and
        // effect lines (UiDebugMenuView::confirming_sheet); this device
        // layer only formats them into the existing row/status grid --
        // no new screen, no device-owned wording (see debug_labels.h).
        const char *verb=v.category==int(openu5::UiDebugCategory::Certification)?"Run Certification":"Apply preset";
        std::snprintf(out.breadcrumb,sizeof(out.breadcrumb),"Developer > %.32s",v.title?v.title:"");
        std::snprintf(out.status,sizeof(out.status),"%s: %.30s?",verb,v.confirming_sheet->display_name);
        std::snprintf(out.position,sizeof(out.position),"Enter=Apply Back=Cancel");
        out.row_count=std::min(kDebugScreenRows,v.confirming_sheet->effect_count);
        out.selected_row=out.row_count; // informational list -- nothing is "selected"
        for(size_t i=0;i<out.row_count;++i)
            std::snprintf(out.rows[i],sizeof(out.rows[i]),"%.51s",v.confirming_sheet->effects[i]);
        return out;
    }
    if(v.category<0)std::snprintf(out.breadcrumb,sizeof(out.breadcrumb),"Developer");
    else std::snprintf(out.breadcrumb,sizeof(out.breadcrumb),"Developer > %.32s",v.title?v.title:"");
    std::snprintf(out.position,sizeof(out.position),"%u/%u",unsigned(v.cursor+1),unsigned(v.count));
    const size_t start=v.cursor>=kDebugScreenRows?v.cursor-kDebugScreenRows+1:0;
    out.row_count=std::min(kDebugScreenRows,v.count-start);out.selected_row=v.cursor-start;
    // Every visible row -- not just the selected one -- gets its own label
    // and current value from UiDebugMenu; the device layer only formats what
    // core hands it (Batch 4.5A-2 Part 4/6: no device-owned label tables, no
    // cursor-only value visibility).
    for(size_t row=0;row<out.row_count;++row){
        const size_t index=start+row;
        std::snprintf(out.rows[row],sizeof(out.rows[row]),"%.50s",debug_->row_label(index));
        const auto rv=debug_->row_value(index);
        char value[36]{};
        switch(rv.kind){
        case openu5::DebugRowValue::Kind::None:break;
        case openu5::DebugRowValue::Kind::Integer:std::snprintf(value,sizeof(value)," %lld",(long long)rv.value);break;
        case openu5::DebugRowValue::Kind::Boolean:std::snprintf(value,sizeof(value)," %s",rv.value?"Yes":"No");break;
        case openu5::DebugRowValue::Kind::Text:std::snprintf(value,sizeof(value)," %.24s",rv.text?rv.text:"Unknown");break;
        case openu5::DebugRowValue::Kind::TextWithId:std::snprintf(value,sizeof(value)," %.18s [%d]",rv.text?rv.text:"Unknown",int(rv.canonical_id));break;
        case openu5::DebugRowValue::Kind::Unsupported:std::snprintf(value,sizeof(value)," Unsupported");break;
        }
        std::strncat(out.rows[row],value,sizeof(out.rows[row])-std::strlen(out.rows[row])-1);
        if(index==v.cursor&&v.editing)std::strncat(out.rows[row]," [edit]",sizeof(out.rows[row])-std::strlen(out.rows[row])-1);
    }
    if(v.confirming)std::snprintf(out.status,sizeof(out.status),"%s",v.confirmation?v.confirmation:"Confirm?");
    else if(v.category==int(openu5::UiDebugCategory::Diagnostics)){
        const auto s=smoke_.view();
        if(s.running)std::snprintf(out.status,sizeof(out.status),"RUN %u/%u P%u F%u %.20s",unsigned(s.completed),unsigned(s.total),unsigned(s.passed),unsigned(s.failed),s.scenario);
        else if(s.complete)std::snprintf(out.status,sizeof(out.status),"DONE P%u F%u %.30s",unsigned(s.passed),unsigned(s.failed),s.failed?s.first_failure:"all passed");
        else std::snprintf(out.status,sizeof(out.status),"Results: %s",kSmokeTestSdPath);
    } else if(v.has_result)std::snprintf(out.status,sizeof(out.status),"Result: %s",(v.category==int(openu5::UiDebugCategory::Teleport)||v.category==int(openu5::UiDebugCategory::Certification))?openu5::debug_teleport_result_label(v.last_teleport_status,v.teleport_passability_known,v.teleport_passable):openu5::debug_status_name(v.last_status));
#endif
    return out;
}

bool AlphaRuntime::handle(const RawInputEvent&raw){service_combat();openu5::UiAction action;DeviceShortcut shortcut;
    const bool in_frontend=frontend_.active();
    const bool frontend_name=in_frontend&&frontend_.state()==openu5::FrontendState::CharacterCreation&&
                             frontend_.creation_phase()==openu5::FrontendCreationPhase::Name;
    const auto mode_before=in_frontend?(frontend_name?openu5::UiMode::TextEntry:openu5::UiMode::InventorySelection):system_menu_.active()?openu5::UiMode::InventorySelection:ui_->mode();
    const bool translated=input_.translate(raw,mode_before,action,shortcut,
                                           !in_frontend&&ui_->accepts_direction_input());
    if(raw.kind==RawInputKind::Keyboard){
        const bool mic=raw.column==kMicrophoneKeyColumn&&raw.row==kMicrophoneKeyRow;
        const bool sym=raw.column==0&&raw.row==2;
        ESP_LOGI(kTag,"INPUT_EDGE raw=%02x,%02x,%02x,%02x,%02x prev=%02x,%02x,%02x,%02x,%02x matrix=%u,%u physical=%s edge=%s mods=S%d,A%d,H%d printable=%02x base=%02x symbol=%02x ui=%s emitted=%s action_char=%u shortcut=%s",
                 raw.snapshot[0],raw.snapshot[1],raw.snapshot[2],raw.snapshot[3],raw.snapshot[4],
                 raw.previous_snapshot[0],raw.previous_snapshot[1],raw.previous_snapshot[2],raw.previous_snapshot[3],raw.previous_snapshot[4],
                 unsigned(raw.column),unsigned(raw.row),mic?"mic-0,6":sym?"sym-0,2":"matrix-key",
                 transition_name(raw.transition),raw.modifiers.symbol,raw.modifiers.alt,raw.modifiers.shift,
                 raw.code,raw.base_code,raw.symbol_code,mode_name(mode_before),translated?action_name(action.kind):"none",
                 translated?unsigned(action.character):0U,shortcut_name(shortcut));
    }else if(raw.kind==RawInputKind::KeyboardResynchronized)
        ESP_LOGW(kTag,"INPUT_RESYNC ui=%s held gestures abandoned",mode_name(mode_before));
    if(!translated)return false;
    if(in_frontend){
        const auto state_before=frontend_.state();const auto phase_before=frontend_.creation_phase();
        char name_before[9]{};std::snprintf(name_before,sizeof(name_before),"%s",frontend_.creation_name());
        bool accepted=false;
        if(shortcut==DeviceShortcut::MovementModeToggled){settings_.movement_mode=input_.movement_mode_enabled();accepted=settings_store_.save(settings_);}
        else accepted=frontend_.handle(action,uint32_t(raw.timestamp_us/1000));
        if(accepted){settings_=frontend_.settings();input_.set_movement_mode_enabled(settings_.movement_mode);input_.set_trackball_responsiveness(settings_.trackball_responsiveness);}
        const auto state_after=frontend_.state();const auto phase_after=frontend_.creation_phase();
        ESP_LOGI(kTag,"FRONTEND_INPUT raw=%s action=%s char=%u accepted=%d state=%s->%s phase=%s->%s name=\"%s\"->\"%s\"",
                 raw_input_name(raw.kind),action_name(action.kind),unsigned(action.character),accepted,
                 frontend_state_name(state_before),frontend_state_name(state_after),
                 creation_phase_name(phase_before),creation_phase_name(phase_after),name_before,frontend_.creation_name());
        if(state_before==openu5::FrontendState::CharacterCreation||state_after==openu5::FrontendState::CharacterCreation){
            ESP_LOGI(kTag,"CHAR_CREATE action=%s accepted=%d phase=%s->%s length=%u->%u answers=%u stack_margin=%u",
                      action_name(action.kind),accepted,creation_phase_name(phase_before),creation_phase_name(phase_after),
                      unsigned(std::strlen(name_before)),unsigned(frontend_.creation_name_length()),
                      unsigned(frontend_.creation_answered()),unsigned(uxTaskGetStackHighWaterMark(nullptr)*sizeof(StackType_t)));
            if(phase_before==openu5::FrontendCreationPhase::Name)
                ESP_LOGI(kTag,"CHAR_NAME action=%s buffer=\"%s\" length=%u",action_name(action.kind),frontend_.creation_name(),unsigned(frontend_.creation_name_length()));
            if(phase_before==openu5::FrontendCreationPhase::Sex||phase_after==openu5::FrontendCreationPhase::Quiz)
                ESP_LOGI(kTag,"CHAR_GENDER action=%s value=0x%02x advanced=%d",action_name(action.kind),unsigned(frontend_.creation_gender()),phase_after==openu5::FrontendCreationPhase::Quiz);
        }
        if(state_before!=state_after){++frontend_state_change_count_;ESP_LOGI(kTag,"FRONTEND_STATE count=%lu from=%s to=%s reason=input reconstructed=0",
            (unsigned long)frontend_state_change_count_,frontend_state_name(state_before),frontend_state_name(state_after));}
        observed_frontend_state_=state_after;
        if(!accepted)return false;
        service_frontend_intent();dirty_=true;dirty_reason_="frontend-input";return true;
    }
    if(action.kind==openu5::UiActionKind::SystemMenu){
        if(system_menu_.active()){settings_=system_menu_.settings();input_.set_movement_mode_enabled(settings_.movement_mode);input_.set_trackball_responsiveness(settings_.trackball_responsiveness);settings_store_.save(settings_);system_menu_.close();}else{openu5::FrontendSaveSlot slots[2]{};save_.inspect(slots);system_menu_.open(settings_,slots);}
        ESP_LOGI(kTag,"SYSTEM_MENU action=toggle open=%d gameplay_command=none",system_menu_.active());dirty_=true;dirty_reason_="system-menu";return true;
    }
    if(system_menu_.active()){
        const bool accepted=system_menu_.handle(action);if(accepted){settings_=system_menu_.settings();input_.set_movement_mode_enabled(settings_.movement_mode);input_.set_trackball_responsiveness(settings_.trackball_responsiveness);}service_system_menu_intent();
        ESP_LOGI(kTag,"SYSTEM_MENU action=%s accepted=%d open=%d gameplay_command=none",action_name(action.kind),accepted,system_menu_.active());
        dirty_=true;dirty_reason_="system-menu";return accepted;
    }
    // #324 / R-32. While a scene segment is running the reference pacer
    // swallows input (the same rule the shrine rite uses) and the binary is
    // simply inside its own synchronous routine. Two deliberate exceptions:
    // transcript paging, which touches no scene state and dispatches nothing
    // (Batch 4.5C), and the getkey_with_redraw points, which is what the
    // original waits on.
    if(blackthorn_pacer_.modal()&&shortcut==DeviceShortcut::None){
        if(action.kind==openu5::UiActionKind::PageUp||action.kind==openu5::UiActionKind::PageDown){
            // Same pre-routing push the ordinary path does, so a page of
            // paging is a page of what is actually on screen (Batch 4.5C).
            refresh_session_context();
            ui_->handle_input(action);
            dirty_=true;dirty_reason_="transcript-page";
            ESP_LOGI(kTag,"BLACKTHORN_SCENE_INPUT action=%s effect=transcript-page scene_state=%d",
                     action_name(action.kind),int(blackthorn_pacer_.state()));
            return true;
        }
        const bool advanced=action.kind==openu5::UiActionKind::Confirm&&blackthorn_pacer_.advance_key();
        if(advanced){dirty_=true;dirty_reason_="blackthorn-scene";}
        ESP_LOGI(kTag,"BLACKTHORN_SCENE_INPUT action=%s effect=%s scene_state=%d gameplay_command=none",
                 action_name(action.kind),advanced?"key-wait-advance":"swallowed",int(blackthorn_pacer_.state()));
        return true;
    }
    // Y-04. Refuge and TrollSneak are MODAL: the reference pacers swallow input
    // for the whole scene, and neither has a dismissal key -- both advance on
    // their own clock and end by themselves. The same transcript-paging
    // exception applies, for the same reason as above.
    if(narrative_pacer_.modal()&&shortcut==DeviceShortcut::None){
        if(action.kind==openu5::UiActionKind::PageUp||action.kind==openu5::UiActionKind::PageDown){
            refresh_session_context();
            ui_->handle_input(action);
            dirty_=true;dirty_reason_="transcript-page";
            ESP_LOGI(kTag,"NARRATIVE_SCENE_INPUT action=%s effect=transcript-page scene=%d",
                     action_name(action.kind),int(narrative_pacer_.scene()));
            return true;
        }
        ESP_LOGI(kTag,"NARRATIVE_SCENE_INPUT action=%s effect=swallowed scene=%d gameplay_command=none",
                 action_name(action.kind),int(narrative_pacer_.scene()));
        return true;
    }
    // R-12: "revealing traga el input" -- the reference's modal reveal loop
    // (main.ts runMapReveal/cancelMapReveal) reads no keyboard while armed and
    // re-censors itself automatically on the wall-clock timer in render(),
    // never on a keypress; unlike gem view there is no explicit close action.
    if(map_reveal_end_us_>esp_timer_get_time()&&shortcut==DeviceShortcut::None){
        ESP_LOGI(kTag,"MAP_REVEAL_INPUT action=%s effect=swallowed gameplay_command=none",action_name(action.kind));
        return true;
    }
    if(gem_view_active_&&shortcut==DeviceShortcut::None){
        const bool charge=gem_view_charges_turn_;
        gem_view_active_=false;gem_view_charges_turn_=false;
        if(charge){openu5::Command after{};after.kind=openu5::CommandKind::AfterGemView;command(after);}
        ESP_LOGI(kTag,"VIEW_RESULT result=closed deferred_turn=%d ui=%s dungeon=%d",charge,mode_name(ui_->mode()),dungeon_.active);
        dirty_=true;dirty_reason_="gem-view-close";return true;
    }
    // R-13: "se cierra con CUALQUIER tecla" (main.ts) -- same modal-close
    // shape as gem view, but (U)se already spent the turn, so closing never
    // dispatches a command.
    if(zodiac_view_active_&&shortcut==DeviceShortcut::None){
        zodiac_view_active_=false;
        ESP_LOGI(kTag,"VIEW_RESULT result=closed view=zodiac deferred_turn=0 ui=%s",mode_name(ui_->mode()));
        dirty_=true;dirty_reason_="zodiac-view-close";return true;
    }
    // R-22. cmd_zstats (0x0a3a) is a synchronous key loop: while it runs, the
    // game reads no other input at all. Interception happens HERE, before any
    // routing, so a direction can never reach dispatch_world_command() as a
    // Move and a command letter can never arm a prompt behind the modal. The
    // Alt-chorded shortcuts (Developer/Save/Load) and the system menu are left
    // alone, exactly as they are for the gem and zodiac views above.
    if(zstats_open_&&shortcut==DeviceShortcut::None)return handle_zstats_input(action);
    ESP_LOGD(kTag,"UI input mode=%s action=%s char=%u index=%ld pending=%d",mode_name(mode_before),action_name(action.kind),unsigned(action.character),long(action.index),mode_before==openu5::UiMode::TargetSelection);
    const auto before=game_.position;const bool dungeon_before=dungeon_.active;const auto dungeon_pos_before=dungeon_.pos;
    const uint32_t command_sequence_before=routed_command_sequence_;
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    const bool debug_before=debug_&&mode_before==openu5::UiMode::DebugMenu;
    const auto debug_view_before=debug_before?debug_->view():openu5::UiDebugMenuView{};
    bool teleport_action=false;openu5::DebugTeleportRequest teleport_request{};
    openu5::DebugTeleportStatus teleport_status=openu5::DebugTeleportStatus::Applied;
#endif
    if(shortcut==DeviceShortcut::MovementModeToggled){
        ESP_LOGI(kTag,"Movement Mode %s",input_.movement_mode_enabled()?"ON":"OFF");
    }else if(shortcut==DeviceShortcut::DeveloperMenu){
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
        ++debug_open_count_;const bool opened=ui_->open_debug_menu();dirty_reason_="mode-entry";
        ESP_LOGI(kTag,"DEBUG_OPEN count=%lu called=1 opened=%d mode_before=%s mode_after=%s reconstructed=1",
                 (unsigned long)debug_open_count_,opened,mode_name(mode_before),mode_name(ui_->mode()));
#else
        ui_->append(openu5::UiTextChannel::System,"Developer tools disabled");
#endif
    }else if(shortcut==DeviceShortcut::Save){uint32_t ms=0;bool ok=save_.save(context_,outdoor_,terrain_,actors_,retained_,resources_.initial_gam,resources_.initial_gam_size,resources_.initial_ool,resources_.initial_ool_size,ms);if(ok)trace_direct_troll_save("SAVE_WORLD_OVERRIDE");ui_->append(openu5::UiTextChannel::System,ok?"Save complete":"Save failed; prior kept");}
    else if(shortcut==DeviceShortcut::Load){uint32_t ms=0;bool ok=save_.load(context_,outdoor_,terrain_,actors_,retained_,ms);if(ok){const auto loc=game_.position.map.location;if(loc>=1&&loc<=32){auto &n=resources_.npc_locations[loc-1];openu5::enter_npc_map(actors_,n.slots,n.count,uint8_t(loc),uint8_t(game_.time.hour),game_.npc_dead[loc-1]);openu5::save::restore_npc_walk(retained_,uint8_t(loc),true,actors_);}terrain_.refresh(resources_.world,game_);trace_direct_troll_save("LOAD_WORLD_OVERRIDE");}ui_->append(openu5::UiTextChannel::System,ok?"Load complete":"No valid save");}
    else if(context_.combat&&combat_ai_turn()){
        if(combat_input_count_<sizeof(combat_input_queue_)/sizeof(combat_input_queue_[0]))combat_input_queue_[combat_input_count_++]=action;
        ESP_LOGD(kTag,"combat queued action=%s count=%u",action_name(action.kind),unsigned(combat_input_count_));
    }else {if(ui_->mode()==openu5::UiMode::Shop)ui_->set_shop_offer_count(openu5::shop_offering_count(game_,shop_,shop_data_));refresh_session_context();ui_->handle_input(action);
        if(mode_before==openu5::UiMode::Combat&&(action.kind==openu5::UiActionKind::Character)&&
           (action.character==u'o'||action.character==u'O')&&ui_->mode()==openu5::UiMode::TargetSelection&&
           ui_->target_command_kind()==openu5::CommandKind::CombatOpen){
            const auto *actor=openu5::current_combat_actor(combat_context_);
            ESP_LOGI(kTag,"OPEN_TARGET_INIT mode=combat ui=target-selection");
            ESP_LOGI(kTag,"OPEN_TARGET_PLAYER space=combat-grid x=%d y=%d",actor?int(actor->position.x):-1,actor?int(actor->position.y):-1);
            ESP_LOGI(kTag,"OPEN_TARGET_RAW space=combat-grid x=%d y=%d",int(ui_->target_x()),int(ui_->target_y()));
            ESP_LOGI(kTag,"OPEN_TARGET_DIRECTION dir=unselected");
            ESP_LOGI(kTag,"OPEN_TARGET_RESOLVED space=combat-grid x=-1 y=-1");
            ESP_LOGI(kTag,"OPEN_TARGET_RENDER cell_x=-1 cell_y=-1 screen_x=-1 screen_y=-1");
            ESP_LOGI(kTag,"OPEN_TARGET_ASSERT logical=-1,-1 rendered=-1,-1 lookup=-1,-1 match=1");
        }}
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    if(debug_before){const auto after=debug_->view();
        ESP_LOGI(kTag,"DEBUG_ACTION action=%s prior_depth=%d prior_category=%d prior_index=%u new_depth=%d new_category=%d new_index=%u mode=%s->%s",
                 action_name(action.kind),debug_depth(debug_view_before),int(debug_view_before.category),unsigned(debug_view_before.cursor),
                 debug_depth(after),int(after.category),unsigned(after.cursor),mode_name(mode_before),mode_name(ui_->mode()));
        dirty_reason_=action.kind==openu5::UiActionKind::Confirm&&debug_view_before.editing?"value-dirty":"input-dirty";
        const bool teleport=debug_view_before.category==int(openu5::UiDebugCategory::Teleport)&&
                            debug_view_before.cursor==5&&!debug_view_before.editing&&
                            action.kind==openu5::UiActionKind::Confirm;
        if(teleport){teleport_action=true;teleport_request=after.teleport_request;teleport_status=after.last_teleport_status;
            dirty_reason_="teleport-result";teleport_snapshot_pending_=true;}
        int helper=-1;
        if(debug_view_before.category==int(openu5::UiDebugCategory::Equipment)&&debug_view_before.cursor>=3&&debug_view_before.cursor<=6)helper=int(debug_view_before.cursor)-3;
        else if(debug_view_before.category==int(openu5::UiDebugCategory::ShortcutsPresets)&&debug_view_before.cursor<=3)helper=int(debug_view_before.cursor);
        const bool applied=action.kind==openu5::UiActionKind::Confirm&&!debug_view_before.editing&&helper>=0&&
                           (helper!=3||debug_view_before.confirming);
        if(applied&&(helper==0||helper==3))ESP_LOGI(kTag,"DEBUG_MAX_PARTY members=%ld result=%d",long(game_.party.party_size),int(after.last_status));
        if(applied&&(helper==1||helper==3)){int spells=0,items=0;for(auto n:game_.spell_quantities)spells+=n>0;for(int i=0;i<game_.equipment_count&&i<256;++i)items+=game_.equipment_quantities[i]>0;ESP_LOGI(kTag,"DEBUG_MAX_RESOURCES gold=%u spells=%d reagents=99 items=%d",unsigned(game_.gold),spells,items);ESP_LOGI(kTag,"DEBUG_TEST_INVENTORY added=%d skipped=LB-regalia,shards,HMS-Cape,badge,wooden-box reason=quest-progression",items);}
        if(applied&&(helper==2||helper==3))for(int i=0;i<game_.party.party_size&&i<game_.party.character_count;++i){const auto&c=game_.party.characters[i];ESP_LOGI(kTag,"DEBUG_EQUIP_BEST member=%d name=\"%s\" helm=%u armor=%u weapon=%u shield=%u ring=%u amulet=%u",i,c.name,unsigned(c.helmet),unsigned(c.armor),unsigned(c.weapon),unsigned(c.shield),unsigned(c.ring),unsigned(c.amulet));}
    }
#endif
    synchronize_after_debug(before,dungeon_before);
    drain_pending_npc_initiation();
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    if(teleport_action)
        ESP_LOGI(kTag,"DEBUG_TELEPORT type=%d id=%u floor/depth=%d entrance=%d requested_xy=%ld,%ld result=%s game_before=L%u/F%d/%u,%u game_after=L%u/F%d/%u,%u dungeon_before=active%d/F%d/%u,%u dungeon_after=active%d/F%d/%u,%u context_before=dungeon%d context_after=dungeon%d,combat%d",
                 int(teleport_request.kind),unsigned(teleport_request.location),int(teleport_request.floor),teleport_request.standard_entry,long(teleport_request.x),long(teleport_request.y),
                 openu5::debug_teleport_status_name(teleport_status),unsigned(before.map.location),int(before.map.floor),unsigned(before.xy.x),unsigned(before.xy.y),
                 unsigned(game_.position.map.location),int(game_.position.map.floor),unsigned(game_.position.xy.x),unsigned(game_.position.xy.y),
                 dungeon_before,int(dungeon_pos_before.floor),unsigned(dungeon_pos_before.x),unsigned(dungeon_pos_before.y),
                 dungeon_.active,int(dungeon_.pos.floor),unsigned(dungeon_.pos.x),unsigned(dungeon_.pos.y),
                 dungeon_before,context_.dungeon,context_.combat);
    if(teleport_action&&teleport_request.kind==openu5::DebugDestinationKind::Dungeon&&teleport_status==openu5::DebugTeleportStatus::Applied&&dungeon_.active&&context_.dungeon){
        dungeon_presentation_pending_=true;
        ESP_LOGI(kTag,"DUNGEON_ENTER_REQUEST source=debug dungeon=%u depth=%u entrance=%d x=%u y=%u facing=%d",unsigned(teleport_request.location),unsigned(teleport_request.floor),teleport_request.standard_entry,unsigned(dungeon_.pos.x),unsigned(dungeon_.pos.y),int(dungeon_.pos.facing));
        ESP_LOGI(kTag,"DUNGEON_ENTER_COMMAND result=success");
        ESP_LOGI(kTag,"DUNGEON_RUN_COMMAND result=success");
        ESP_LOGI(kTag,"DEBUG_DUNGEON_TELEPORT dungeon=%u depth=%u entrance=%d x=%u y=%u facing=%d",unsigned(dungeon_.pos.dungeon),unsigned(dungeon_.pos.floor),teleport_request.standard_entry,unsigned(dungeon_.pos.x),unsigned(dungeon_.pos.y),int(dungeon_.pos.facing));
        ESP_LOGI(kTag,"DUNGEON_SESSION active=%d dungeon=%u depth=%u x=%u y=%u facing=%d",dungeon_.active,unsigned(dungeon_.pos.dungeon),unsigned(dungeon_.pos.floor),unsigned(dungeon_.pos.x),unsigned(dungeon_.pos.y),int(dungeon_.pos.facing));
        ESP_LOGI(kTag,"UI_MODE %s -> dungeon",mode_name(mode_before));
        ESP_LOGI(kTag,"DUNGEON_RENDERER active=%d dungeon=%u depth=%u",dungeon_.active,unsigned(dungeon_.pos.dungeon),unsigned(dungeon_.pos.floor));
        ESP_LOGI(kTag,"DEBUG_DUNGEON_TELEPORT_RESULT result=success reason=normal-dungeon-init");
    } else if(teleport_action&&teleport_request.kind==openu5::DebugDestinationKind::Dungeon) {
        ESP_LOGE(kTag,"DEBUG_DUNGEON_TELEPORT_RESULT result=failure reason=%s session_active=%d context_dungeon=%d",openu5::debug_teleport_status_name(teleport_status),dungeon_.active,context_.dungeon);
        if(!dungeon_.active)ui_->set_base_mode(openu5::UiMode::Exploration);
    }
#endif
    if(mode_before!=ui_->mode())ESP_LOGI(kTag,"UI_MODE from=%s to=%s",mode_name(mode_before),mode_name(ui_->mode()));
    if(routed_command_sequence_!=command_sequence_before)
        ESP_LOGI(kTag,"INPUT_ROUTE action=%s ui=%s gameplay_command=%d sequence=%lu",action_name(action.kind),mode_name(mode_before),int(last_routed_command_),(unsigned long)routed_command_sequence_);
    else ESP_LOGI(kTag,"INPUT_ROUTE action=%s ui=%s gameplay_command=none",action_name(action.kind),mode_name(mode_before));
    transcript_high_water_=std::max<uint32_t>(transcript_high_water_,uint32_t(ui_->transcript_size()));dirty_=true;return true;
}

const char *AlphaRuntime::overlay() const {static char text[64]{};text[0]=0;
    // #324 / R-32. The original's getkey_with_redraw points have no on-screen
    // cue because a DOS player simply pressed a key; on the handheld the scene
    // would otherwise look frozen. A device affordance, not transcript text.
    if(blackthorn_pacer_.awaiting_key()){std::snprintf(text,sizeof(text),"Enter: continue");return text;}
    if(ui_&&ui_->mode()==openu5::UiMode::Shop)return text;
    if(ui_&&ui_->mode()==openu5::UiMode::TargetSelection){const int x=ui_->target_x(),y=ui_->target_y();if(ui_->target_command_kind()==openu5::CommandKind::Fire){if(ui_->target_has_direction())std::snprintf(text,sizeof(text),"Fire: %s",openu5::direction_name(ui_->target_direction()));else std::snprintf(text,sizeof(text),"Fire: choose direction");}else{const openu5::CombatActor *target=nullptr;if(context_.combat)for(int i=0;i<combat_.count;++i){const auto&a=combat_.actors[i];if(combat_actor_live(a)&&a.position.x==x&&a.position.y==y){target=&a;break;}}if(target){const char *name=target->enemy&&target->enemy->name?target->enemy->name:target->member<game_.party.character_count?game_.party.characters[target->member].name:"Actor";std::snprintf(text,sizeof(text),"Aim: %.16s (%d,%d)",name,x,y);}else std::snprintf(text,sizeof(text),"Aim: empty (%d,%d)",x,y);}}
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    else if(ui_&&ui_->mode()==openu5::UiMode::DebugMenu&&debug_){auto v=debug_->view();std::snprintf(text,sizeof(text),"%s > %s",v.title?v.title:"Debug",v.item?v.item:"");}
#endif
    return text;}

const DeviceShopView *AlphaRuntime::compose_shop_view(){
    if(!ui_||(ui_->mode()!=openu5::UiMode::Shop&&ui_->request()!=openu5::UiRequestId::Shop))return nullptr;
    const bool transactional=shop_.phase==openu5::ShopPhase::Buy||shop_.phase==openu5::ShopPhase::Sell||
        shop_.phase==openu5::ShopPhase::Reagent||shop_.phase==openu5::ShopPhase::Guild||
        shop_.phase==openu5::ShopPhase::Ship||shop_.phase==openu5::ShopPhase::Wine||
        shop_.phase==openu5::ShopPhase::HealerMember||shop_.phase==openu5::ShopPhase::InnLeaveMember||
        shop_.phase==openu5::ShopPhase::InnPickupMember||
        (shop_.phase>=openu5::ShopPhase::BuyDeal&&shop_.phase<=openu5::ShopPhase::RumorDeal)||
        shop_.phase==openu5::ShopPhase::TavernRoundDeal||shop_.phase==openu5::ShopPhase::TavernDrinkDeal||
        shop_.phase==openu5::ShopPhase::RationsQuantity||shop_.phase==openu5::ShopPhase::RumorText||
        (shop_.phase>=openu5::ShopPhase::LegacyHeal&&shop_.phase<=openu5::ShopPhase::LegacyResurrect);
    if(!transactional)return nullptr;
    shop_view_={};shop_view_.active=true;shop_view_.gold=game_.gold;
    std::snprintf(shop_view_.title,sizeof(shop_view_.title),"%s",
                  shop_.record&&shop_.record->name?shop_.record->name:"Shop");
    std::snprintf(shop_view_.keeper,sizeof(shop_view_.keeper),"%s",
                  shop_.record&&shop_.record->keeper?shop_.record->keeper:"Merchant");
    const char *phase="SHOP";bool deal=false;
    switch(shop_.phase){
    case openu5::ShopPhase::Buy:phase="BUY";break;
    case openu5::ShopPhase::Sell:phase="SELL";break;
    case openu5::ShopPhase::Reagent:phase="REAGENTS";break;
    case openu5::ShopPhase::Guild:phase="GUILD";break;
    case openu5::ShopPhase::Ship:phase="SHIPS";break;
    case openu5::ShopPhase::Wine:phase="WINE";break;
    case openu5::ShopPhase::HealerMember:case openu5::ShopPhase::LegacyHeal:
    case openu5::ShopPhase::LegacyCure:case openu5::ShopPhase::LegacyResurrect:phase="CHOOSE COMPANION";break;
    case openu5::ShopPhase::InnLeaveMember:phase="LEAVE COMPANION";break;
    case openu5::ShopPhase::InnPickupMember:phase="PICK UP COMPANION";break;
    case openu5::ShopPhase::Menu:phase="SERVICES";break;
    case openu5::ShopPhase::BuyDeal:phase="CONFIRM PURCHASE";deal=true;break;
    case openu5::ShopPhase::SellDeal:phase="CONFIRM SALE";deal=true;break;
    case openu5::ShopPhase::ReagentDeal:case openu5::ShopPhase::GuildDeal:
    case openu5::ShopPhase::HorseDeal:case openu5::ShopPhase::ShipDeal:
    case openu5::ShopPhase::HealerDeal:case openu5::ShopPhase::InnRestDeal:
    case openu5::ShopPhase::InnLeaveDeal:case openu5::ShopPhase::RumorDeal:
    case openu5::ShopPhase::TavernRoundDeal:case openu5::ShopPhase::TavernDrinkDeal:phase="CONFIRM SERVICE";deal=true;break;
    case openu5::ShopPhase::Tavern:phase="TAVERN";break;
    case openu5::ShopPhase::HealerNeed:phase="HEALING";break;
    default:break;
    }
    shop_view_.total=openu5::shop_offering_count(game_,shop_,shop_data_);
    const size_t cursor=shop_view_.total?std::min<size_t>(size_t(std::max<int32_t>(0,ui_->shop_cursor())),shop_view_.total-1):0;
    shop_view_.page_start=(cursor/kShopVisibleRows)*kShopVisibleRows;
    shop_view_.row_count=std::min(kShopVisibleRows,shop_view_.total-shop_view_.page_start);
    shop_view_.selected_row=cursor-shop_view_.page_start;
    for(size_t row=0;row<shop_view_.row_count;++row){
        openu5::ShopOffer offer{};const size_t index=shop_view_.page_start+row;
        if(!openu5::shop_offering_at(game_,shop_,shop_data_,index,offer))continue;
        const char *name=shop_item_name(shop_.phase,offer.item,game_);
        std::snprintf(shop_view_.rows[row].name,sizeof(shop_view_.rows[row].name),"%s",name?name:"Service");
        shop_view_.rows[row].price=offer.price;shop_view_.rows[row].quantity=offer.quantity;
    }
    if(!shop_view_.total&&(shop_.item>=0||shop_.phase==openu5::ShopPhase::HorseDeal||
                           shop_.phase==openu5::ShopPhase::InnRestDeal)){
        const char *name=shop_item_name(shop_.phase,std::max<int32_t>(0,shop_.item),game_);
        if(name){shop_view_.total=shop_view_.row_count=1;shop_view_.selected_row=0;
            std::snprintf(shop_view_.rows[0].name,sizeof(shop_view_.rows[0].name),"%s",name);
            shop_view_.rows[0].price=shop_.price;shop_view_.rows[0].quantity=shop_.quantity;}
    }
    if(shop_view_.total)std::snprintf(shop_view_.phase,sizeof(shop_view_.phase),"%s %u-%u/%u",phase,
        unsigned(shop_view_.page_start+1),unsigned(shop_view_.page_start+shop_view_.row_count),unsigned(shop_view_.total));
    else std::snprintf(shop_view_.phase,sizeof(shop_view_.phase),"%s",phase);
    shop_view_.context.active=true;
    if(deal)std::snprintf(shop_view_.context.actions,sizeof(shop_view_.context.actions),"%s","Y Yes|N No|Mic Back");
    else if(shop_.phase==openu5::ShopPhase::Menu){
        const char *actions=shop_.type==openu5::ShopType::Blacksmith?"B Buy|S Sell|Mic Leave":
            shop_.type==openu5::ShopType::Healer?"H Heal|C Cure|R Raise":
            shop_.type==openu5::ShopType::InnKeeper?"R Rest|L Leave|P Pick":"Choose|Mic Back";
        std::snprintf(shop_view_.context.actions,sizeof(shop_view_.context.actions),"%s",actions);
    }else if(shop_.phase==openu5::ShopPhase::Tavern)std::snprintf(shop_view_.context.actions,sizeof(shop_view_.context.actions),"%s",kTavernContextActions);
    else if(shop_.phase==openu5::ShopPhase::HealerNeed)std::snprintf(shop_view_.context.actions,sizeof(shop_view_.context.actions),"%s","H Heal|C Cure|R Raise");
    else if(shop_view_.row_count)std::snprintf(shop_view_.context.actions,sizeof(shop_view_.context.actions),"%s","Move|Select|Mic Back");
    else std::snprintf(shop_view_.context.actions,sizeof(shop_view_.context.actions),"%s","Continue|Mic Back");
    std::snprintf(shop_view_.context.status,sizeof(shop_view_.context.status),"%.31s",shop_view_.phase);
    if(ui_->mode()==openu5::UiMode::TextEntry||ui_->mode()==openu5::UiMode::NumericEntry){
        shop_view_.input[0]='>';shop_view_.input[1]=' ';
        for(size_t i=0;i<ui_->input_length()&&i<20;++i)shop_view_.input[i+2]=ui_->input_buffer()[i]<=0x7f?char(ui_->input_buffer()[i]):'?';
        std::snprintf(shop_view_.context.status,sizeof(shop_view_.context.status),"%.14s %.15s",ui_->prompt(),shop_view_.input);
    }
    return &shop_view_;
}
// R-22 -- the (Z)-stats page modal.
//
// cmd_zstats (ZSTATS.OVL:0x0a3a) is a synchronous key loop that owns the
// keyboard until Space or ESC: it never dispatches a command, never advances
// the clock and never draws an RNG roll. The three methods below reproduce
// exactly that. open_zstats() arms the axis, handle_zstats_input() IS the key
// loop (called from handle() before any routing, like the gem/zodiac views),
// and zstats_view() composes the page the renderer paints through the shared,
// ESP-free openu5::compose_zstats_page() seam that the host model tests drive.
openu5::ZStatsInput AlphaRuntime::zstats_input() const{
    openu5::ZStatsInput in{};in.game=&game_;
    // The moonstones' carried/buried state has no GameState field --
    // QuestWorldServices owns it -- so it is pushed in here from that same
    // authoritative owner, exactly as open_selection() does for the (U)se picker.
    for(size_t m=0;m<8&&m<quest_.moonstone_count;++m)
        if(quest_.moonstones&&!quest_.moonstones[m].buried)in.moonstones_owned|=uint8_t(1u<<m);
    return in;
}
openu5::ZStatsPage AlphaRuntime::zstats_view() const{
    if(!zstats_open_)return {};
    return openu5::compose_zstats_page(zstats_input(),zstats_page_,zstats_scroll_);
}
void AlphaRuntime::open_zstats(int member){
    const int party=std::max(1,std::min<int>(game_.party.party_size,game_.party.character_count));
    const int bounded=member>=0&&member<party?member:0;
    zstats_open_=true;
    zstats_page_=openu5::zstats_page_for_member(bounded);
    zstats_scroll_=0;
    status_member_=int16_t(bounded);
    ESP_LOGI(kTag,"ZSTATS_OPEN member=%d page=%d party=%d",bounded,zstats_page_,party);
}
bool AlphaRuntime::handle_zstats_input(const openu5::UiAction &action){
    if(!zstats_open_)return false;
    const int party=std::max(1,std::min<int>(game_.party.party_size,game_.party.character_count));
    // A roster that shrank while the modal was open can strand the axis in the
    // unused window [party*2,0x0b]; fold it back before anything reads it.
    zstats_page_=openu5::zstats_clamp_page(zstats_page_,party);
    const int page_before=zstats_page_;

    // Space (0x0a78) and ESC (0x0a81) are the ONLY keys that close. Back is the
    // T-Deck's other dismissal gesture and is accepted alongside Cancel.
    const bool close=action.kind==openu5::UiActionKind::Cancel||
                     action.kind==openu5::UiActionKind::Back||
                     (action.kind==openu5::UiActionKind::Character&&action.character==u' ');
    if(close){
        zstats_open_=false;zstats_page_=0;zstats_scroll_=0;
        ESP_LOGI(kTag,"ZSTATS_CLOSE reason=%s ui=%s gameplay_command=none",
                 action.kind==openu5::UiActionKind::Character?"space":"escape",mode_name(ui_->mode()));
        dirty_=true;dirty_reason_="zstats-close";return true;
    }

    // '1'-'6' jump to a member's stats page (0x0b12, bounded by g_party_size);
    // '0' jumps to the provisions page (0x0b37).
    if(action.kind==openu5::UiActionKind::Character&&action.character>=u'0'&&action.character<=u'9'){
        const int digit=int(action.character-u'0');
        if(digit==0){zstats_page_=openu5::kZStatsPageProvisions;zstats_scroll_=0;}
        else if(digit-1<party){zstats_page_=openu5::zstats_page_for_member(digit-1);zstats_scroll_=0;}
        // A digit past the party size is bounded away, not honoured (jae 0x0b12).
        if(zstats_page_!=page_before){dirty_=true;dirty_reason_="zstats-page";}
        return true;
    }

    if(action.kind==openu5::UiActionKind::Direction||
       action.kind==openu5::UiActionKind::Next||action.kind==openu5::UiActionKind::Previous||
       action.kind==openu5::UiActionKind::PageUp||action.kind==openu5::UiActionKind::PageDown){
        const bool vertical=action.kind==openu5::UiActionKind::PageUp||
                            action.kind==openu5::UiActionKind::PageDown||
                            (action.kind==openu5::UiActionKind::Direction&&
                             (action.direction==openu5::Direction::North||
                              action.direction==openu5::Direction::South));
        const bool forward=action.kind==openu5::UiActionKind::Next||
                           action.kind==openu5::UiActionKind::PageDown||
                           (action.kind==openu5::UiActionKind::Direction&&
                            (action.direction==openu5::Direction::South||
                             action.direction==openu5::Direction::East));
        // render_item_list's sub-loop (0x07d0): INSIDE a list that actually
        // overflows, up/down SCROLL (0x081c/0x086c) and left/right leave to
        // change page (0x0948). With nothing to scroll they fall through to the
        // axis, exactly as ztats-layout.md section 8.5 refines section 3.
        if(vertical&&openu5::zstats_page_kind(zstats_page_)==openu5::ZStatsPageKind::List){
            const auto list=openu5::zstats_list(zstats_input(),zstats_page_);
            const size_t max_scroll=openu5::zstats_max_scroll(list.count);
            if(max_scroll){
                // PgUp/PgDn step a literal 7 (mov [bp-2],7); an arrow steps 1.
                const size_t step=action.kind==openu5::UiActionKind::PageUp||
                                  action.kind==openu5::UiActionKind::PageDown
                                      ?openu5::kZStatsListRows:size_t(1);
                const size_t before=zstats_scroll_;
                zstats_scroll_=forward?std::min(zstats_scroll_+step,max_scroll)
                                      :(zstats_scroll_>step?zstats_scroll_-step:size_t(0));
                if(zstats_scroll_!=before){dirty_=true;dirty_reason_="zstats-scroll";}
                return true;
            }
        }
        zstats_page_=forward?openu5::zstats_axis_next(zstats_page_,party)
                            :openu5::zstats_axis_prev(zstats_page_,party);
        zstats_scroll_=0;
        if(zstats_page_!=page_before){dirty_=true;dirty_reason_="zstats-page";}
        return true;
    }

    // Everything else -- including 'z' itself, which cmd_zstats' loop has no
    // compare for -- is swallowed with no effect: the modal stays open and the
    // key never reaches gameplay.
    ESP_LOGD(kTag,"ZSTATS_INPUT action=%s effect=swallowed page=%d",action_name(action.kind),zstats_page_);
    return true;
}

const DeviceSelectionView *AlphaRuntime::compose_selection_view(){
    if(!ui_)return nullptr;
    // R-22. While the (Z)-stats modal owns the keyboard it also owns the right
    // panel. It is painted with the SAME compact-selector primitive every other
    // modal uses -- title band, two detail lines, eight text rows, context bar --
    // rather than a second text renderer: the original's page is sixteen cells
    // wide and eight rows tall, which is exactly what this panel already shows.
    // The one thing deliberately NOT reproduced is the IBM.CH box frame around a
    // list (glyphs 0x10/0x11/0x13-0x17): the T-Deck face is ASCII-only and the
    // panel already carries its own cyan rules in the same place. See the R-22
    // residuals in GAMEPLAY_INTEGRATION_AUDIT.md.
    if(zstats_open_){
        const auto page=zstats_view();
        selection_view_={};selection_view_.active=true;selection_view_.detail_panel=true;
        selection_view_.mode=uint8_t(openu5::UiMode::PartySelection);
        // The banner (0x6c70) is the member's name, "Equipment", or the list title.
        std::snprintf(selection_view_.title,sizeof(selection_view_.title),"%.23s",page.banner);
        const int party=std::max(1,std::min<int>(game_.party.party_size,game_.party.character_count));
        if(page.kind==openu5::ZStatsPageKind::List&&page.list_total){
            const size_t first=page.list_scroll+1,last=page.list_scroll+page.row_count;
            std::snprintf(selection_view_.detail,sizeof(selection_view_.detail),"%u-%u of %u%s%s",
                          unsigned(first),unsigned(last),unsigned(page.list_total),
                          page.more_above?" ^":"",page.more_below?" v":"");
        }else if(page.member>=0)
            std::snprintf(selection_view_.detail,sizeof(selection_view_.detail),"Member %d/%d",
                          page.member+1,party);
        std::snprintf(selection_view_.detail2,sizeof(selection_view_.detail2),"%s",
                      page.kind==openu5::ZStatsPageKind::Stats?"Stats":
                      page.kind==openu5::ZStatsPageKind::Arms?"Arms":
                      page.kind==openu5::ZStatsPageKind::Provisions?"Provisions":"Inventory");
        selection_view_.total=selection_view_.row_count=page.row_count;
        selection_view_.page_start=0;
        // No row is a cursor on a stats page: kSelectionVisibleRows is out of
        // range for the highlight, the same sentinel the old detail panel used.
        selection_view_.selected_row=kSelectionVisibleRows;
        for(size_t row=0;row<page.row_count&&row<kSelectionVisibleRows;++row)
            std::snprintf(selection_view_.rows[row],sizeof(selection_view_.rows[row]),"%.23s",page.rows[row].text);
        selection_view_.context.active=true;
        std::snprintf(selection_view_.context.status,sizeof(selection_view_.context.status),"Z-stats");
        std::snprintf(selection_view_.context.actions,sizeof(selection_view_.context.actions),"%s",
                      page.kind==openu5::ZStatsPageKind::List&&page.list_total>openu5::kZStatsListRows
                          ?"L/R Page|U/D Scroll|Mic":"Move Page|0-6 Jump|Mic");
        return &selection_view_;
    }
    openu5::UiSelectionView current{};if(!ui_->selection_view(current))return nullptr;
    selection_view_={};selection_view_.active=true;selection_view_.mode=uint8_t(current.mode);selection_view_.total=current.count;
    const char *title=current.mode==openu5::UiMode::SpellSelection?(selection_request_==openu5::UiRequestId::Custom?"Mix Spell":"Cast Spell"):
        current.mode==openu5::UiMode::EquipmentSelection?"Ready Equipment":current.mode==openu5::UiMode::InventorySelection?"Use Item":
        selection_request_==openu5::UiRequestId::EquipmentMember?"Ready Whom?":selection_request_==openu5::UiRequestId::UseTarget?"Use On Whom?":selection_request_==openu5::UiRequestId::Target?"Spell Target":selection_request_==openu5::UiRequestId::Party?(pending_order_from_>=0?"Move To":"Move From"):"Choose Companion";
    std::snprintf(selection_view_.title,sizeof(selection_view_.title),"%s",title);
    const size_t cursor=current.count?std::min(current.cursor,current.count-1):0;
    if(selection_request_==openu5::UiRequestId::Status&&current.mode==openu5::UiMode::PartySelection&&cursor<selection_count_){
        const int member=selections_[cursor].value;const auto&m=game_.party.characters[member];selection_view_.detail_panel=true;
        std::snprintf(selection_view_.title,sizeof(selection_view_.title),"Z / Stats");
        std::snprintf(selection_view_.detail,sizeof(selection_view_.detail),"Member %zu/%zu",cursor+1,current.count);
        std::snprintf(selection_view_.detail2,sizeof(selection_view_.detail2),"%.9s",m.name);
        const int weapon=m.weapon<48?m.weapon:-1,armor=m.armor<48?m.armor:-1;
        const char *rows[8]{};char values[8][24]{};
        std::snprintf(values[0],sizeof(values[0]),"Class: %c",m.character_class?m.character_class:'?');
        std::snprintf(values[1],sizeof(values[1]),"Status: %c",m.status?m.status:'G');
        std::snprintf(values[2],sizeof(values[2]),"HP: %u/%u",unsigned(m.current_hp),unsigned(m.max_hp));
        std::snprintf(values[3],sizeof(values[3]),"MP: %u",unsigned(m.current_mp));
        std::snprintf(values[4],sizeof(values[4]),"STR %u DEX %u INT %u",unsigned(m.strength),unsigned(m.dexterity),unsigned(m.intelligence));
        std::snprintf(values[5],sizeof(values[5]),"Level %u  XP %u",unsigned(m.level),unsigned(m.exp));
        std::snprintf(values[6],sizeof(values[6]),"Weapon: %.12s",weapon>=0?openu5::equipment_display_name(weapon):"None");
        std::snprintf(values[7],sizeof(values[7]),"Armor: %.13s",armor>=0?openu5::equipment_display_name(armor):"None");
        for(int i=0;i<8;++i){
            rows[i]=values[i];
        }
        selection_view_.page_start=0;
        selection_view_.row_count=8;
        selection_view_.selected_row=kSelectionVisibleRows;
        for(size_t row=0;row<8;++row)std::snprintf(selection_view_.rows[row],sizeof(selection_view_.rows[row]),"%s",rows[row]);
        selection_view_.context.active=true;std::snprintf(selection_view_.context.status,sizeof(selection_view_.context.status),"%.9s",m.name);std::snprintf(selection_view_.context.actions,sizeof(selection_view_.context.actions),"Move|Enter|Mic Back");return &selection_view_;
    }
    selection_view_.page_start=(cursor/kSelectionVisibleRows)*kSelectionVisibleRows;
    selection_view_.row_count=std::min(kSelectionVisibleRows,current.count-selection_view_.page_start);
    selection_view_.selected_row=cursor-selection_view_.page_start;
    for(size_t row=0;row<selection_view_.row_count;++row)
        std::snprintf(selection_view_.rows[row],sizeof(selection_view_.rows[row]),"%.22s",selections_[selection_view_.page_start+row].label);
    if(cursor<selection_count_){const int value=selections_[cursor].value;
        if(current.mode==openu5::UiMode::SpellSelection){const auto*def=openu5::spell_definition(openu5::SpellId(value));std::snprintf(selection_view_.detail,sizeof(selection_view_.detail),"%.31s",openu5::spell_effect_summary(openu5::SpellId(value)));std::snprintf(selection_view_.detail2,sizeof(selection_view_.detail2),"Target: %.16s  MP%u",openu5::spell_target_label(openu5::SpellId(value)),unsigned(std::min<int>(def?def->circle:0,9)));}
        else if(current.mode==openu5::UiMode::EquipmentSelection){const int member=pending_ready_member_>=0?pending_ready_member_:active_member(game_);const auto slot=openu5::slot_for_equip(value);const int equipped=equipped_in_slot(game_.party.characters[member],slot);std::snprintf(selection_view_.detail,sizeof(selection_view_.detail),"Slot: %s",equip_slot_name(slot));std::snprintf(selection_view_.detail2,sizeof(selection_view_.detail2),"Current: %.18s",equipped>=0&&equipped<48?openu5::equipment_display_name(equipped):"None");}
        else if(current.mode==openu5::UiMode::PartySelection&&value>=0&&value<game_.party.character_count){const auto&m=game_.party.characters[value];std::snprintf(selection_view_.detail,sizeof(selection_view_.detail),"HP %u/%u  MP %u  %c",unsigned(m.current_hp),unsigned(m.max_hp),unsigned(m.current_mp),m.status?m.status:'G');}
        else if(current.mode==openu5::UiMode::InventorySelection)std::snprintf(selection_view_.detail,sizeof(selection_view_.detail),"Choose an item to use");}
    selection_view_.context.active=true;
    std::snprintf(selection_view_.context.status,sizeof(selection_view_.context.status),"%zu/%zu",cursor+1,current.count);
    std::snprintf(selection_view_.context.actions,sizeof(selection_view_.context.actions),"Move|Confirm|Mic Back");
    return &selection_view_;
}

DevicePartyHighlight AlphaRuntime::compose_party_highlight() const{
    DevicePartyHighlight out{};out.selected=-1;out.actor=-1;
    // #213: the poison tick's row inversion. It is published here, alongside
    // the picker and combat markers, precisely because all three ARE the same
    // primitive in the binary and may not share a row.
    out.damage_flash=poison_.flash_row();
    if(context_.combat&&combat_.current>=0&&combat_.current<combat_.count){const auto &actor=combat_.actors[combat_.current];if(actor.member!=255)out.actor=int8_t(actor.member);}
    openu5::UiSelectionView view{};if(ui_&&ui_->selection_view(view)&&view.mode==openu5::UiMode::PartySelection&&view.cursor<selection_count_)out.selected=int8_t(selections_[view.cursor].value);
    else if(out.actor>=0)out.selected=out.actor;
    else if(game_.party.active_character<game_.party.character_count)out.selected=int8_t(game_.party.active_character);
    return out;
}

const DeviceContextActionBar *AlphaRuntime::compose_context_bar(){
    context_bar_={};if(!ui_)return nullptr;
    const auto mode=ui_->mode();
    if(mode!=openu5::UiMode::TargetSelection&&mode!=openu5::UiMode::TextEntry&&
       mode!=openu5::UiMode::NumericEntry&&mode!=openu5::UiMode::YesNo&&
       mode!=openu5::UiMode::PartySelection&&mode!=openu5::UiMode::InventorySelection&&
       mode!=openu5::UiMode::EquipmentSelection&&mode!=openu5::UiMode::Shop)return nullptr;
    context_bar_.active=true;
    if(mode==openu5::UiMode::Shop){
        std::snprintf(context_bar_.status,sizeof(context_bar_.status),"%.31s",
                      shop_.record&&shop_.record->keeper?shop_.record->keeper:"Merchant");
        const char *actions="Enter|Next|Mic Back";
        if(shop_.phase==openu5::ShopPhase::Menu)
            actions=shop_.type==openu5::ShopType::Blacksmith?"B Buy|S Sell|Mic Leave":
                    shop_.type==openu5::ShopType::Healer?"H Heal|C Cure|R Raise":
                    shop_.type==openu5::ShopType::InnKeeper?"R Rest|L Leave|P Pick":"Choose|Mic Back";
        else if(shop_.phase==openu5::ShopPhase::Tavern)actions=kTavernContextActions;
        else if(shop_.phase==openu5::ShopPhase::HealerNeed)actions="H Heal|C Cure|R Raise";
        else if(shop_.phase==openu5::ShopPhase::BlacksmithPause)actions="Enter Offer|Mic Back";
        std::snprintf(context_bar_.actions,sizeof(context_bar_.actions),"%s",actions);
        return &context_bar_;
    }
    const char *status=overlay();if(!status||!*status)status=ui_->prompt();
    std::snprintf(context_bar_.status,sizeof(context_bar_.status),"%.31s",status?status:"");
    if((mode==openu5::UiMode::TextEntry||mode==openu5::UiMode::NumericEntry)&&ui_->input_length()){
        char input[16]{};for(size_t i=0;i<ui_->input_length()&&i<sizeof(input)-1;++i)
            input[i]=ui_->input_buffer()[i]<=0x7f?char(ui_->input_buffer()[i]):'?';
        std::snprintf(context_bar_.status,sizeof(context_bar_.status),"%.14s >%.14s",ui_->prompt(),input);
    }
    const char *actions=mode==openu5::UiMode::TargetSelection?"Move|Confirm|Mic Back":
                        mode==openu5::UiMode::YesNo?"Y Yes|N No|Mic Back":
                        mode==openu5::UiMode::TextEntry||mode==openu5::UiMode::NumericEntry?"Enter Accept|Mic Back":
                        "Move|Select|Mic Back";
    std::snprintf(context_bar_.actions,sizeof(context_bar_.actions),"%s",actions);
    return &context_bar_;
}
void AlphaRuntime::compose_creation_art(){
    std::fill(creation_canvas_,creation_canvas_+kCreationPixels,uint16_t(0));
    auto blit=[&](size_t id,int dx,int dy){
        if(id>=11)return;
        const auto&s=resources_.creation_sprites[id];if(!s.pixels)return;
        for(int y=0;y<int(s.height);++y)for(int x=0;x<int(s.width);++x){const int px=dx+x,py=dy+y;if(px<0||py<0||px>=int(kCreationWidth)||py>=int(kCreationHeight))continue;const auto*source=s.pixels+(size_t(y)*s.width+size_t(x))*3;if(source[2]<128)continue;creation_canvas_[size_t(py)*kCreationWidth+size_t(px)]=uint16_t(source[0])|uint16_t(uint16_t(source[1])<<8);}
    };
    const auto a=std::min<uint8_t>(frontend_.creation_virtue_a(),7),b=std::min<uint8_t>(frontend_.creation_virtue_b(),7);
    blit(1,16,5);blit(1,200,5);blit(size_t(2+a),kVirtueX[a],kVirtueY[a]);blit(size_t(2+b),kVirtueX[b]+184,kVirtueY[b]);
}

esp_err_t AlphaRuntime::render(Board&board,bool force){
    if(applied_brightness_!=settings_.brightness){ESP_RETURN_ON_ERROR(board.set_brightness(settings_.brightness),kTag,"apply persistent display brightness");applied_brightness_=settings_.brightness;}
    if(frontend_.active()){
        const int64_t now=esp_timer_get_time();
        const uint32_t tick=uint32_t(now/55000);
        const bool changed=frontend_.tick(uint32_t(now/1000));service_frontend_intent();
        if(frontend_.active()){
            const auto state=frontend_.state();
            const bool state_changed=state!=observed_frontend_state_;
            if(state_changed){
                ++frontend_state_change_count_;
                ESP_LOGI(kTag,"FRONTEND_STATE count=%lu from=%s to=%s reason=tick-or-intent reconstructed=0",
                         (unsigned long)frontend_state_change_count_,frontend_state_name(observed_frontend_state_),frontend_state_name(state));
                observed_frontend_state_=state;
            }
            const bool attract=state==openu5::FrontendState::AttractDemo;
            const bool creation_quiz=state==openu5::FrontendState::CharacterCreation&&frontend_.creation_phase()==openu5::FrontendCreationPhase::Quiz;
            const bool creation_chrome=state==openu5::FrontendState::CharacterCreation&&!creation_quiz;
            const bool title_visible=state==openu5::FrontendState::Title||frontend_.startup_intro()||state==openu5::FrontendState::MainMenu||attract||creation_chrome;
            // Character creation is an input-critical static screen. Retain the
            // reference title frame, but stop the fire animation and its SPI
            // transfer while name, gender and questionnaire input are active.
            const bool title_animation=state==openu5::FrontendState::Title||state==openu5::FrontendState::MainMenu||attract;
            const uint32_t title_frame=tick/5;
            const uint32_t absolute_attract_frame=tick;
            if(state_changed&&attract){attract_origin_frame_=absolute_attract_frame;rendered_attract_frame_=UINT32_MAX;intro_view_.reset();}
            const uint32_t attract_frame=absolute_attract_frame-attract_origin_frame_;
            const bool title_due=title_animation&&title_frame!=rendered_frontend_title_frame_;
            const bool attract_due=attract&&attract_frame!=rendered_attract_frame_;
            if(!dirty_&&!force&&!changed&&!state_changed&&!title_due&&!attract_due)return ESP_OK;
            const char *render_reason=force?"full-redraw":state_changed||changed?"state-transition":dirty_?dirty_reason_:attract_due?"attract-tick":"title-animation";
            const uint16_t *preview=nullptr;
            openu5::RenderReport report{};
            const bool render_attract=attract&&(dirty_||force||state_changed||changed||attract_due);
            const int64_t render_start=esp_timer_get_time();
            if(render_attract){
                if(!intro_view_.advance(intro_frame_))return ESP_FAIL;
                ESP_LOGI(kTag,"ATTRACT_TICK tick=%lu frame=%lu scene=%u title=\"%s\" cycle=%lu demo_local=1 exact_reference=1",
                         (unsigned long)tick,(unsigned long)attract_frame,unsigned(intro_frame_.scene),
                         openu5::IntroViewPlayer::scene_title(intro_frame_.scene),(unsigned long)intro_frame_.cycle);
                const auto rendered=openu5::render_intro_view(tile_cache_,intro_frame_,tick,viewport_,openu5::kViewportPixelCount,report);
                if(rendered!=ESP_OK)return rendered;
                preview=viewport_;
            }
            const uint16_t *title_art=title_visible?resources_.intro_title+
                (title_animation?(title_frame&3U):0U)*320*110:nullptr;
            const uint16_t *panel_art=frontend_.state()==openu5::FrontendState::Credits?resources_.credits_panel:nullptr;
            const uint16_t *creation_art=nullptr;if(creation_quiz){compose_creation_art();creation_art=creation_canvas_;}
            frontend_view_=frontend_.view();if(attract)frontend_view_.subtitle=openu5::IntroViewPlayer::scene_title(intro_frame_.scene);++frontend_reconstruction_count_;
            assert(frontend_.active() && "frontend renderer requires frontend ownership");
            const auto e=board.show_frontend(frontend_view_,preview,title_art,panel_art,creation_art,settings_.ui_size);
            const auto render_us=uint32_t(esp_timer_get_time()-render_start);
            frontend_render_high_us_=std::max(frontend_render_high_us_,render_us);render_high_us_=std::max(render_high_us_,render_us);
            const auto stack=uxTaskGetStackHighWaterMark(nullptr)*sizeof(StackType_t);
            const auto internal=heap_caps_get_free_size(kInternal),psram=heap_caps_get_free_size(kPsram);
            ESP_LOGI(kTag,"FRONTEND_RENDER state=%s reason=%s full_redraw=%d screen_clear=%d dirty_regions=%u pixels=%u animation_tick=%lu render_us=%lu reconstructed=%lu internal=%zu psram=%zu stack_margin=%u",
                     frontend_state_name(state),render_reason,board.debug_last_full_redraw(),board.debug_last_full_redraw(),
                     unsigned(board.debug_last_dirty_regions()),unsigned(board.debug_last_pixels()),(unsigned long)tick,
                     (unsigned long)render_us,(unsigned long)frontend_reconstruction_count_,internal,psram,unsigned(stack));
            if(state==openu5::FrontendState::Settings)ESP_LOGI(kTag,"SETTINGS_RENDER reason=%s full_redraw=%d dirty_regions=%u pixels=%u us=%lu",
                render_reason,board.debug_last_full_redraw(),unsigned(board.debug_last_dirty_regions()),unsigned(board.debug_last_pixels()),(unsigned long)render_us);
            if(render_attract)ESP_LOGI(kTag,"ATTRACT_FRAME frame=%lu step=%lu crc=%08lx render_us=%lu pixels=%u",
                (unsigned long)attract_frame,(unsigned long)intro_frame_.scene,(unsigned long)report.viewport_crc32,
                (unsigned long)render_us,unsigned(board.debug_last_pixels()));
            dirty_=e!=ESP_OK;
            if(e==ESP_OK){dirty_reason_="frontend-tick";if(title_animation)rendered_frontend_title_frame_=title_frame;if(render_attract)rendered_attract_frame_=attract_frame;}
            return e;
        }
        dirty_=true;force=true;
    }
    if(system_menu_.active()){
        assert(!frontend_.active() && "system menu cannot overlap frontend state");
        if(!dirty_&&!force)return ESP_OK;
        const int64_t start=esp_timer_get_time();
        frontend_view_=system_menu_.view();const auto e=board.show_frontend(frontend_view_,nullptr,nullptr,nullptr,nullptr,settings_.ui_size);
        const auto us=uint32_t(esp_timer_get_time()-start);
        render_high_us_=std::max(render_high_us_,us);
        ESP_LOGI(kTag,"SYSTEM_MENU_RENDER reason=%s full_redraw=%d dirty_regions=%u pixels=%u us=%lu",
                 force?"full-redraw":dirty_reason_,board.debug_last_full_redraw(),
                 unsigned(board.debug_last_dirty_regions()),unsigned(board.debug_last_pixels()),
                 (unsigned long)us);
        dirty_=e!=ESP_OK;
        return e;
    }
    service_combat();
    if(service_blackthorn_scene()){dirty_=true;dirty_reason_="blackthorn-scene";}
    if(service_narrative_scene()){dirty_=true;dirty_reason_="narrative-scene";}
    if(service_poison_flash()){dirty_=true;dirty_reason_="poison-tick";}
    assert(!frontend_.active() && !system_menu_.active() && "gameplay renderer lacks display ownership");
    if(smoke_.pump()){dirty_=true;dirty_reason_="smoke-test-progress";}
    const int64_t now=esp_timer_get_time();DeviceShortcut held_shortcut{};
    if(input_.update(now,ui_->mode(),held_shortcut)){
        dirty_=true;dirty_reason_="input-dirty";ESP_LOGI(kTag,"INPUT_HOLD physical=mic-0,6 threshold_us=1100000 ui=%s emitted=movement-toggle state=%s gameplay_command=none",mode_name(ui_->mode()),input_.movement_mode_enabled()?"ON":"OFF");
    }
    const bool magic_inverted=magic_invert_end_us_>now&&now>=magic_invert_start_us_;
    if(magic_inverted!=magic_was_inverted_){dirty_=true;dirty_reason_=magic_inverted?"magic-invert":"magic-restore";}
    // R-12: the view "re-censors itself on expiry" (the reference's own
    // phrasing) -- a plain timer read, no explicit close command, and the
    // last revealed frame must still be replaced by one more real render so
    // the censorship comes back on screen instead of freezing revealed.
    const bool map_reveal_active=map_reveal_end_us_>now;
    if(map_reveal_active!=map_reveal_was_active_){dirty_=true;dirty_reason_=map_reveal_active?"map-reveal":"map-reveal-end";}
    map_reveal_was_active_=map_reveal_active;
    if(!map_reveal_active)map_reveal_end_us_=0;
    int quake_offset_px=0;
    if(quake_pulses_>0){
        const int64_t elapsed_ms=(now-quake_start_us_)/1000;
        if(elapsed_ms>=int64_t(quake_pulses_)*openu5::kQuakePeriodMs)quake_pulses_=0;
        else quake_offset_px=openu5::quake_offset_at(int32_t(elapsed_ms),quake_pulses_);
    }
    const bool quake_active=quake_pulses_>0;
    if(quake_active!=quake_was_active_){dirty_=true;dirty_reason_=quake_active?"quake":"quake-end";}
    quake_was_active_=quake_active;
    // Y-04 (Batch 7B). Three more presentation timers, all of them on the same
    // frame clock as the quake above: none blocks, none freezes the loop.
    openu5::WorldFxOp world_fx_ops[openu5::kWorldFxSlots*2]{};
    const size_t world_fx_count=world_fx_.paint(uint32_t(now/1000),world_fx_ops,
                                                sizeof(world_fx_ops)/sizeof(world_fx_ops[0]));
    const bool world_fx_active=world_fx_.active();
    if(world_fx_active!=world_fx_was_active_){dirty_=true;dirty_reason_=world_fx_active?"world-fx":"world-fx-end";}
    world_fx_was_active_=world_fx_active;
    const bool poison_active=poison_.active();
    if(poison_active!=poison_was_active_){dirty_=true;dirty_reason_=poison_active?"poison-tick":"poison-tick-end";}
    poison_was_active_=poison_active;
    const bool narrative_active=narrative_pacer_.active();
    if(narrative_active!=narrative_was_active_){dirty_=true;dirty_reason_=narrative_active?"narrative-scene":"narrative-scene-end";}
    narrative_was_active_=narrative_active;
    const uint32_t tick=uint32_t(now/55000);
    const bool debug_mode=ui_->mode()==openu5::UiMode::DebugMenu;
    // A world/combat animation flag must never wake the modal Developer UI.
    // Alpha 1.3 entered here every 55 ms, then converted animation_only to a
    // full debug-screen clear below: the physical flashing root cause.
    // Quake oscillates within its own active window (8 pulses of down/rest,
    // not one fixed state like the magic-invert flash), so it needs the same
    // periodic re-render pump as tile animation, not just a dirty flag on
    // the start/stop transition.
    bool animation_only=!debug_mode&&!dirty_&&!force&&(animation_visible_||quake_active||world_fx_active||poison_active)&&tick!=rendered_animation_tick_;
    if(dungeon_presentation_pending_){force=true;animation_only=false;}
    if(!dirty_&&!force&&!animation_only)return ESP_OK;
    const char *render_reason=force?"full-redraw":animation_only?"animation-tick":dirty_reason_;
    auto &snapshot=presentation_;snapshot={};
    openu5::ActiveMap world_gem_map{};bool world_gem_map_ready=false;
    // #324 / R-32 -- the fourth presentation source. It outranks the others
    // because the capture scene is a staged cutscene: while it is mounted the
    // ordinary Palace-lobby world must not be what the viewport shows, which
    // was the whole visible defect. It draws from the pacer's own room and
    // cast; no gameplay position is consulted or rewritten to produce it.
    const bool blackthorn_source=blackthorn_pacer_.mounted();
    // Y-04 -- the refuge's own staged window (BLCKTHRN 0x0962). Like the
    // capture scene it outranks the world: while the party sleeps in the
    // nothingness the ordinary map must NOT be what the viewport shows.
    const bool refuge_source=!blackthorn_source&&narrative_pacer_.mounted();
    const bool combat_source=!blackthorn_source&&!refuge_source&&context_.combat&&combat_.initialized;
    const bool dungeon_source=!blackthorn_source&&!refuge_source&&!combat_source&&context_.dungeon&&dungeon_.active;
    // One source decision owns gameplay presentation.  In particular, the
    // surface return coordinate is deliberately absent from the dungeon arm.
    const char *presentation_source=blackthorn_source?"blackthorn-scene":refuge_source?"refuge-scene":combat_source?"combat":dungeon_source?"dungeon3d":"world";
    if(blackthorn_source)snapshot=openu5::compose_blackthorn_presentation(blackthorn_pacer_.view());
    else if(refuge_source)snapshot=openu5::compose_refuge_presentation(narrative_pacer_.phase(),
        turn_.transport_tile>=0?int16_t(turn_.transport_tile+0x100):int16_t(tile_report_.avatar_tile));
    else if(combat_source)snapshot=openu5::compose_combat_presentation(combat_,game_);
    else if(dungeon_source)snapshot.center={dungeon_.pos.x,dungeon_.pos.y};
    else{auto active=openu5::get_active_map(resources_.world,game_.position.map);if(active.error!=openu5::Error::None){
        // A missing {location,floor} map (e.g. a resource pack built before a
        // forced-relocation destination such as Blackthorn's deposit() target
        // was authored) leaves this render bailing out silently every tick,
        // so the previous frame (a stale scene) stays on screen forever with
        // no trace of why. Log it once per occurrence so a hardware retest
        // can tell "no map for this position" apart from every other stale-
        // frame cause instead of guessing.
        ESP_LOGE(kTag,"WORLD_MAP_MISSING location=%u floor=%d xy=%u,%u",unsigned(game_.position.map.location),
                 int(game_.position.map.floor),unsigned(game_.position.xy.x),unsigned(game_.position.xy.y));
        return ESP_FAIL;
    }const int avatar=turn_.transport_tile>=0?turn_.transport_tile+0x100:tile_report_.avatar_tile;snapshot=openu5::compose_world_presentation(context_,active.value,game_.position.xy,avatar,map_reveal_active);u5obj_trace_present(snapshot);if(gem_view_active_){world_gem_map=active.value;world_gem_map_ready=true;}}
    ESP_LOGI(kTag,"PRESENTATION_DISPATCH ui=%s combat=%d dungeon=%d source=%s",mode_name(ui_->mode()),combat_source,dungeon_source,presentation_source);
    int16_t open_marker_x=-1,open_marker_y=-1;
    if(snapshot.combat&&ui_->take_target_render_marker(open_marker_x,open_marker_y)){
        snapshot.target_x=int8_t(open_marker_x);snapshot.target_y=int8_t(open_marker_y);snapshot.target_valid=true;
    } else if(ui_->mode()==openu5::UiMode::TargetSelection&&(snapshot.combat||ui_->target_command_kind()==openu5::CommandKind::Fire)&&ui_->target_has_cell()){snapshot.target_x=int8_t(ui_->target_x());snapshot.target_y=int8_t(ui_->target_y());if(ui_->target_command_kind()==openu5::CommandKind::Fire)snapshot.target_valid=ui_->target_has_direction();else if(ui_->target_command_kind()==openu5::CommandKind::CombatAttack){snapshot.target_valid=false;for(int i=0;i<combat_.count;++i){const auto&a=combat_.actors[i];if(combat_actor_live(a)&&a.position.x==snapshot.target_x&&a.position.y==snapshot.target_y){snapshot.target_valid=true;break;}}}}
    if(!snapshot.combat&&direct_troll_.active&&game_.position.map.location==direct_troll_.map.location&&
       game_.position.map.floor==direct_troll_.map.floor){
        int dx=direct_troll_.trigger_x-int(game_.position.xy.x),dy=direct_troll_.trigger_y-int(game_.position.xy.y);
        if(!game_.position.map.location){if(dx>128)dx-=256;if(dx<-128)dx+=256;if(dy>128)dy-=256;if(dy<-128)dy+=256;}
        const int cell_x=dx+5,cell_y=dy+5;int object_tile=-1;
        for(size_t i=0;i<objects_.size();++i){const auto&o=objects_[i];if(o.location==game_.position.map.location&&o.floor==game_.position.map.floor&&o.x==direct_troll_.trigger_x&&o.y==direct_troll_.trigger_y){object_tile=o.tile;break;}}
        const bool in_view=cell_x>=0&&cell_x<11&&cell_y>=0&&cell_y<11;
        const int final_tile=in_view?int(snapshot.tiles[cell_y*11+cell_x]):openu5::kOffMap;
        const char *source=!in_view?"off-viewport":dx==0&&dy==0?"avatar":object_tile>=0?"object":"terrain";
        const auto terrain=terrain_.inspect(resources_.world,direct_troll_.map,direct_troll_.trigger_x,direct_troll_.trigger_y);
        ESP_LOGI(kTag,"WORLD_RENDER_CELL world_x=%ld world_y=%ld terrain_tile=%ld object_tile=%d final_tile=%d source=%s",
                 long(direct_troll_.trigger_x),long(direct_troll_.trigger_y),long(terrain.effective),object_tile,final_tile,source);
    }
    if(!animation_only&&!context_.combat)for(size_t i=0;i<objects_.size();++i){const auto&o=objects_[i];if(!o.chest||o.location!=game_.position.map.location||o.floor!=game_.position.map.floor)continue;int dx=o.x-int(game_.position.xy.x),dy=o.y-int(game_.position.xy.y);if(!o.location){if(dx>128)dx-=256;if(dx< -128)dx+=256;if(dy>128)dy-=256;if(dy< -128)dy+=256;}if(dx>=-5&&dx<=5&&dy>=-5&&dy<=5)ESP_LOGI(kTag,"CHEST_RENDER source=world-object loc=%ld floor=%ld world_x=%ld world_y=%ld screen_x=%d screen_y=%d combat_x=-1 combat_y=-1 tile=%ld object_id=%u transform=%s",long(o.location),long(o.floor),long(o.x),long(o.y),dx+5,dy+5,long(o.tile),unsigned(i),o.location?"bounded-delta":"wrapped-delta");}
    if(teleport_snapshot_pending_){ESP_LOGI(kTag,"TELEPORT_RENDERER map=L%u/F%d xy=%u,%u snapshot_center=%d,%d combat=%d dungeon=%d",
        unsigned(game_.position.map.location),int(game_.position.map.floor),unsigned(game_.position.xy.x),unsigned(game_.position.xy.y),
        int(snapshot.center.x),int(snapshot.center.y),snapshot.combat,dungeon_.active);teleport_snapshot_pending_=false;}
    // The scene bakes its own cast into the window at fixed reference cells;
    // the actor-program clock would wander them off their staged positions.
    if(!debug_mode&&!dungeon_source&&!blackthorn_source&&!refuge_source)actor_animation_.render(snapshot,tick,turn_.time_spell=='T');
    // Y-04 (#201/#243/#313). The world fx are a TEMPORARY per-cell override of
    // the already-composed window: no world tile, object table or save is
    // touched to show them. They go on last so the burst lands over whatever
    // the composer produced, exactly as the original blits over the redrawn
    // viewport. Cells travel as offsets from the party, which is the window
    // centre in every one of these sources.
    const bool world_fx_paintable=!debug_mode&&!dungeon_source&&!gem_view_active_&&!zodiac_view_active_;
    if(world_fx_paintable&&world_fx_count)
        openu5::apply_world_fx(snapshot,world_fx_ops,
                               std::min(world_fx_count,sizeof(world_fx_ops)/sizeof(world_fx_ops[0])));
    openu5::RenderReport report{};
    const int64_t start=esp_timer_get_time();
    esp_err_t e=ESP_OK;
    uint16_t dungeon_primitives=0;
    if(!debug_mode){
        if(dungeon_source)ESP_LOGI(kTag,"DUNGEON_DRAW_BEGIN viewport=176x176 source=%s",gem_view_active_?"gem-view":"dungeon3d");
        if(zodiac_view_active_){
            e=openu5::render_zodiac_view(zodiac_view_,viewport_,openu5::kViewportPixelCount,report,dungeon_primitives);
            presentation_source="zodiac-view";
        } else if(dungeon_source&&gem_view_active_){
            e=openu5::render_dungeon_gem_view(dungeon_,tile_cache_,viewport_,openu5::kViewportPixelCount,report,dungeon_primitives);
            presentation_source="gem-view";
        } else if(gem_view_active_&&world_gem_map_ready){
            e=openu5::render_world_gem_view(world_gem_map,game_.position.xy,tile_cache_,viewport_,openu5::kViewportPixelCount,report,dungeon_primitives);
            presentation_source="gem-view";
        } else if(dungeon_source){
            // R-05. The wall variant is a pure function of the dungeon, so the
            // cache key changes only when the party enters a dungeon with a
            // different bank -- a turn, a step or a level change never reloads.
            dungeon_art_.select({openu5::dungeon_art_wall_bank_index(
                openu5::dungeon_wall_variant(dungeon_.pos.dungeon))});
            e=openu5::render_dungeon_view(game_,turn_,dungeon_,dungeon_art_.surfaces(),tick,
                                          viewport_,openu5::kViewportPixelCount,report,dungeon_primitives);
        }
        else{u5obj_trace_render(snapshot);
            e=openu5::render_snapshot(tile_cache_,snapshot,tick,game_.turns_since_start,
                                       viewport_,openu5::kViewportPixelCount,report);}
    }
    // The cannon ball lives BETWEEN cells, so it is painted into the rasterized
    // window rather than composed into the snapshot (a cell blit cannot place
    // it). It precedes the invert/shake post-processes so those act on the
    // whole picture, as they do on the device's screen.
    if(e==ESP_OK&&world_fx_paintable){
        for(size_t i=0;i<world_fx_count&&i<sizeof(world_fx_ops)/sizeof(world_fx_ops[0]);++i){
            if(world_fx_ops[i].kind!=openu5::WorldFxOpKind::Dot)continue;
            openu5::paint_world_fx_dot(viewport_,world_fx_ops[i].dx_milli,world_fx_ops[i].dy_milli);
            report.viewport_crc32=openu5::recompute_viewport_crc32(viewport_,openu5::kViewportPixelCount);
        }
    }
    if(e==ESP_OK&&magic_inverted&&!debug_mode){
        for(size_t p=0;p<openu5::kViewportPixelCount;++p)
            viewport_[p]=magic_xor_palette_pixel(viewport_[p],tile_cache_.palette);
        report.viewport_crc32^=0xa5c35a3cU;
    }
    if(e==ESP_OK&&quake_offset_px>0&&!debug_mode){
        openu5::shift_viewport_vertically(viewport_,quake_offset_px);
        report.viewport_crc32=openu5::recompute_viewport_crc32(viewport_,openu5::kViewportPixelCount);
    }
    const DeviceDebugScreen *debug_ptr=nullptr;
    if(debug_mode){
        *debug_view_=debug_screen();debug_ptr=debug_view_;animation_only=false;++debug_render_count_;
    }
    const auto hud=openu5::hud_world_state(game_,turn_,resources_.moon_phases,resources_.moon_phase_count,dungeon_.active);
    // R-05: while the dungeon3d source owns the viewport, the two strips carry
    // the dungeon's level and facing. R-17/Y-14: the gem view is instead its
    // own full-square composition -- `full_square_viewport` below drops the
    // strips entirely rather than overdrawing them across it. The zodiac view
    // still keeps the world bars (unchanged, out of this batch's scope).
    const auto dungeon_bands=openu5::hud_dungeon_bands(dungeon_,dungeon_source&&!gem_view_active_&&!zodiac_view_active_);
    if(e==ESP_OK)e=board.show_alpha(viewport_,*ui_,game_,turn_,hud,resources_.runes_font,overlay(),report.animated_cells,
                                     animation_only,debug_ptr,
                                     input_.movement_mode_active(ui_->mode(),ui_->accepts_direction_input()),
                                     settings_.ui_size,compose_shop_view(),compose_selection_view(),compose_context_bar(),compose_party_highlight(),report.viewport_crc32,&dungeon_bands,
                                     gem_view_active_);
    const auto us=uint32_t(esp_timer_get_time()-start);render_high_us_=std::max(render_high_us_,us);
    if(dungeon_source){
        dungeon_render_high_us_=std::max(dungeon_render_high_us_,us);
        ESP_LOGI(kTag,"DUNGEON_FRAME dungeon=%u depth=%u x=%u y=%u facing=%d command=%d source=%s",
                 unsigned(dungeon_.pos.dungeon),unsigned(dungeon_.pos.floor),unsigned(dungeon_.pos.x),unsigned(dungeon_.pos.y),
                 int(dungeon_.pos.facing),int(last_routed_command_),presentation_source);
        ESP_LOGI(kTag,"DUNGEON_DRAW_%s viewport=176x176 primitives=%u us=%lu",
                 e==ESP_OK?"END":"FAILED",unsigned(dungeon_primitives),(unsigned long)us);
        if(dungeon_presentation_pending_&&e==ESP_OK){
            ESP_LOGI(kTag,"DUNGEON_PRESENTATION_ENTER full_redraw=1 viewport=176x176 source=%s",presentation_source);
            dungeon_presentation_pending_=false;
        }
    }
    if(debug_mode){
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
        const auto view=debug_->view();
        ESP_LOGI(kTag,"DEBUG_RENDER count=%lu reason=%s clear=%d full_redraw=%d dirty_regions=%u pixels=%u reconstructed=0 category=%d index=%u depth=%d us=%lu",
                 (unsigned long)debug_render_count_,render_reason,board.debug_last_full_redraw(),
                 board.debug_last_full_redraw(),unsigned(board.debug_last_dirty_regions()),
                 unsigned(board.debug_last_pixels()),int(view.category),unsigned(view.cursor),
                 debug_depth(view),(unsigned long)us);
#else
        ESP_LOGI(kTag,"DEBUG_RENDER count=%lu reason=%s clear=%d full_redraw=%d dirty_regions=%u pixels=%u reconstructed=0 us=%lu",
                 (unsigned long)debug_render_count_,render_reason,board.debug_last_full_redraw(),
                 board.debug_last_full_redraw(),unsigned(board.debug_last_dirty_regions()),
                 unsigned(board.debug_last_pixels()),(unsigned long)us);
#endif
    }
    if(!animation_only)ESP_LOGI(kTag,"render %lu us reason=%s animated=%u crc=%08lx",
                                (unsigned long)us,render_reason,unsigned(report.animated_cell_count),
                                (unsigned long)report.viewport_crc32);
    animation_visible_=debug_mode?false:snapshot.any_animated;rendered_animation_tick_=tick;magic_was_inverted_=magic_inverted;if(!magic_inverted&&magic_invert_end_us_&&now>=magic_invert_end_us_)magic_invert_end_us_=magic_invert_start_us_=0;
    dirty_=e!=ESP_OK;if(e==ESP_OK)dirty_reason_="input-dirty";return e;
}

void AlphaRuntime::synchronize_loaded_world(){
    // #324 / R-32. A capture scene is pure presentation and is never saved;
    // a load arriving mid-scene must take the stage down rather than leave a
    // throne room drawn over a completely different world.
    blackthorn_pacer_.cancel();blackthorn_scene_state_={};
    // Y-04. Same rule for the narrative scenes and the live fx: all three are
    // pure presentation, none is saved, and a load arriving mid-scene must
    // take them down rather than leave a refuge stage, a half-played burst or
    // a stuck roster flash over a completely different world. The narrative
    // pacer is cancelled WITHOUT reporting a completion, so a load can never
    // trigger the resurrection the scene would otherwise have applied.
    narrative_pacer_.cancel();world_fx_.clear();poison_.cancel();
    actors_={};const auto loc=game_.position.map.location;if(loc>=1&&loc<=32){auto &n=resources_.npc_locations[loc-1];openu5::enter_npc_map(actors_,n.slots,n.count,uint8_t(loc),uint8_t(game_.time.hour),game_.npc_dead[loc-1]);openu5::save::restore_npc_walk(retained_,uint8_t(loc),true,actors_);}terrain_.refresh(resources_.world,game_);combat_.initialized=false;
    // R-14: the pool has no owner-side backing store to diff against the new
    // document, so a load must clear it before reconstructing -- otherwise a
    // save taken while save B's world objects are live would leak them into
    // save A's world (see object_append/object_erase; capture_world_objects).
    ESP_LOGI(kTag,"U5OBJ CLEAR site=load-restore pool_before=%u",unsigned(objects_.size()));
    objects_.clear();
    if(openu5::save::restore_world_objects(retained_,quest_)!=openu5::save::Error::None)ESP_LOGW(kTag,"WORLD_OBJECTS_RESTORE_FAILED domain-invalid sidecar; world objects left empty");
    ESP_LOGI(kTag,"WORLD_OBJECTS_RESTORE count=%u",unsigned(objects_.size()));
    // R-15: an in-progress dungeon session round-trips through the sidecar;
    // a surface save (the common case) or a domain-invalid document both
    // fall back to no active session rather than leaking the prior one.
    if(openu5::save::restore_dungeon(retained_,dungeon_)!=openu5::save::Error::None){dungeon_={};ESP_LOGW(kTag,"DUNGEON_RESTORE_FAILED domain-invalid sidecar; dungeon session left inactive");}
    ESP_LOGI(kTag,"DUNGEON_RESTORE active=%d dungeon=%u depth=%u",dungeon_.active,unsigned(dungeon_.pos.dungeon),unsigned(dungeon_.pos.floor));
}

void AlphaRuntime::service_frontend_intent(){
    const auto intent=frontend_.take_intent();if(intent.kind==openu5::FrontendIntentKind::None)return;bool ok=false;uint32_t ms=0;
    const auto margin_before=uxTaskGetStackHighWaterMark(nullptr)*sizeof(StackType_t);
    ESP_LOGI(kTag,"FRONTEND_INTENT intent=%s storage_begin=%d phase=%s name=\"%s\" gender=0x%02x stack_margin=%u",
             frontend_intent_name(intent.kind),intent.kind!=openu5::FrontendIntentKind::OpenDeveloperTools,
             creation_phase_name(frontend_.creation_phase()),frontend_.creation_name(),unsigned(frontend_.creation_gender()),unsigned(margin_before));
    if(intent.kind==openu5::FrontendIntentKind::OpenDeveloperTools){
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
        frontend_.enter_game();ok=ui_->open_debug_menu();
#endif
        ESP_LOGI(kTag,"DEVELOPER_ENTRY storage_touched=0 new_journey_touched=0 ok=%d stack_margin=%u",ok,unsigned(uxTaskGetStackHighWaterMark(nullptr)*sizeof(StackType_t)));
        dirty_=true;dirty_reason_="developer-entry";return;
    }
    if(intent.kind==openu5::FrontendIntentKind::ContinueLatest)ok=save_.load(context_,outdoor_,terrain_,actors_,retained_,ms);
    else if(intent.kind==openu5::FrontendIntentKind::LoadSlot)ok=save_.load_slot(intent.slot,context_,outdoor_,terrain_,actors_,retained_,ms);
    else if(intent.kind==openu5::FrontendIntentKind::CreateInitialSave){openu5::save::SidecarSource source;const auto err=openu5::save::load_native_state(resources_.initial_gam,resources_.initial_gam_size,nullptr,game_,turn_,retained_,source,true);ok=err==openu5::save::Error::None;if(ok){
        // A New Journey is a clean INIT.GAM-derived session. None of these
        // runtime-only owners may leak a prior debug/gameplay session into it.
        commands_={};travel_={};dialogue_={};shop_={};shrine_={};blackthorn_={};
        outdoor_.enemies.clear();outdoor_.enemy_view.clear();outdoor_.object_view.clear();
        outdoor_.has_chunk_origin=false;terrain_.persistent.clear();terrain_.clear_residence();
        ESP_LOGI(kTag,"U5OBJ CLEAR site=new-journey pool_before=%u",unsigned(objects_.size()));
        objects_.clear();actors_={};dungeon_={};combat_.initialized=false;actor_animation_.reset();
        openu5::apply_new_journey_identity(game_,intent.identity);synchronize_loaded_world();
        ok=save_.save(context_,outdoor_,terrain_,actors_,retained_,resources_.initial_gam,resources_.initial_gam_size,resources_.initial_ool,resources_.initial_ool_size,ms,true);
    }}
    else if(intent.kind==openu5::FrontendIntentKind::PersistSettings){settings_=intent.settings;input_.set_movement_mode_enabled(settings_.movement_mode);input_.set_trackball_responsiveness(settings_.trackball_responsiveness);ok=settings_store_.save(settings_);ESP_LOGI(kTag,"FRONTEND_INTENT intent=%s storage_end=1 ok=%d ms=%lu stack_margin=%u",frontend_intent_name(intent.kind),ok,(unsigned long)ms,unsigned(uxTaskGetStackHighWaterMark(nullptr)*sizeof(StackType_t)));return;}
    if(ok&&(intent.kind==openu5::FrontendIntentKind::ContinueLatest||intent.kind==openu5::FrontendIntentKind::LoadSlot))synchronize_loaded_world();
    ESP_LOGI(kTag,"FRONTEND_INTENT intent=%s storage_end=1 ok=%d ms=%lu stack_margin=%u",frontend_intent_name(intent.kind),ok,(unsigned long)ms,unsigned(uxTaskGetStackHighWaterMark(nullptr)*sizeof(StackType_t)));
    frontend_.complete_intent(ok, intent.kind==openu5::FrontendIntentKind::ContinueLatest?"No active game. Please create a character or transfer one from Ultima IV.":intent.kind==openu5::FrontendIntentKind::LoadSlot?"Save is missing or corrupt":save_.last_failure()[0]?save_.last_failure():"Initial save could not be created");
    if(intent.kind==openu5::FrontendIntentKind::CreateInitialSave){openu5::FrontendSaveSlot slots[2]{};save_.inspect(slots);frontend_.set_save_slots(slots);}dirty_=true;
}

void AlphaRuntime::service_system_menu_intent(){
    const auto intent=system_menu_.take_intent();if(intent.kind==openu5::SystemMenuIntentKind::None||intent.kind==openu5::SystemMenuIntentKind::Resume)return;
    uint32_t ms=0;bool ok=true;
    if(intent.kind==openu5::SystemMenuIntentKind::Save){ok=save_.save(context_,outdoor_,terrain_,actors_,retained_,resources_.initial_gam,resources_.initial_gam_size,resources_.initial_ool,resources_.initial_ool_size,ms);if(ok)trace_direct_troll_save("SAVE_WORLD_OVERRIDE");ui_->append(openu5::UiTextChannel::System,ok?"Save complete":"Save failed; prior kept");}
    else if(intent.kind==openu5::SystemMenuIntentKind::LoadLatest)ok=save_.load(context_,outdoor_,terrain_,actors_,retained_,ms);
    else if(intent.kind==openu5::SystemMenuIntentKind::LoadSlot)ok=save_.load_slot(intent.slot,context_,outdoor_,terrain_,actors_,retained_,ms);
    else if(intent.kind==openu5::SystemMenuIntentKind::PersistSettings){settings_=intent.settings;input_.set_movement_mode_enabled(settings_.movement_mode);input_.set_trackball_responsiveness(settings_.trackball_responsiveness);ok=settings_store_.save(settings_);}
    else if(intent.kind==openu5::SystemMenuIntentKind::OpenDeveloper){
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
        ++debug_open_count_;ok=ui_->open_debug_menu();dirty_reason_="mode-entry";
#else
        ok=false;
#endif
        ESP_LOGI(kTag,"DEBUG_OPEN source=system-menu opened=%d gameplay_command=none",ok);
    }
    else if(intent.kind==openu5::SystemMenuIntentKind::ReturnToTitle){system_menu_.close();frontend_.start(uint32_t(esp_timer_get_time()/1000),
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
        true,
#else
        false,
#endif
        settings_);frontend_.set_question_texts(resources_.questions,resources_.question_count);frontend_.set_intro_texts(resources_.intro_scenes,resources_.intro_scene_count);openu5::FrontendSaveSlot slots[2]{};save_.inspect(slots);frontend_.set_save_slots(slots);return;}
    if((intent.kind==openu5::SystemMenuIntentKind::LoadLatest||intent.kind==openu5::SystemMenuIntentKind::LoadSlot)&&ok){synchronize_loaded_world();trace_direct_troll_save("LOAD_WORLD_OVERRIDE");system_menu_.close();ui_->append(openu5::UiTextChannel::System,"Load complete");}
    else if((intent.kind==openu5::SystemMenuIntentKind::LoadLatest||intent.kind==openu5::SystemMenuIntentKind::LoadSlot)&&!ok)ui_->append(openu5::UiTextChannel::System,"No valid save");
}

void AlphaRuntime::log_metrics(const char*where)const{const auto stack=uxTaskGetStackHighWaterMark(nullptr)*sizeof(StackType_t);const auto internal=heap_caps_get_free_size(kInternal),psram=heap_caps_get_free_size(kPsram);ESP_LOGI(kTag,"METRICS %s internal=%zu psram=%zu stack_margin=%u render_high_us=%lu frontend_render_high_us=%lu command_high_us=%lu transcript=%lu/%zu",where,internal,psram,unsigned(stack),(unsigned long)render_high_us_,(unsigned long)frontend_render_high_us_,(unsigned long)command_high_us_,(unsigned long)transcript_high_water_,kTranscriptBlocks);const auto&m=input_.direction_metrics();ESP_LOGI(kTag,"TRACKBALL_INPUT raw_edges=%lu accepted=%lu suppressed=%lu",(unsigned long)m.trackball_raw_edges(),(unsigned long)m.trackball_accepted(),(unsigned long)m.trackball_suppressed());ESP_LOGI(kTag,"TRACKBALL_SETTINGS percent=%u min_interval_us=%lld debounce_us=%lld accel=1.00",unsigned(settings_.trackball_responsiveness),(long long)m.trackball_debounce_us(),(long long)m.trackball_debounce_us());if(internal<32768)ESP_LOGW(kTag,"LOW INTERNAL RAM: %zu",internal);if(stack<4096)ESP_LOGW(kTag,"LOW MAIN STACK MARGIN: %u",unsigned(stack));}

size_t AlphaRuntime::object_count(void*p){return static_cast<AlphaRuntime*>(p)->objects_.size();}
openu5::QuestObject AlphaRuntime::object_read(void*p,size_t i){auto&r=*static_cast<AlphaRuntime*>(p);return i<r.objects_.size()?r.objects_[i]:openu5::QuestObject{};}
bool AlphaRuntime::object_reserve(void*p,size_t n){auto&r=*static_cast<AlphaRuntime*>(p);const size_t need=n*sizeof(openu5::QuestObject);const size_t free_psram=heap_caps_get_free_size(kPsram);const bool ok=free_psram>=need+32768;
    // Batch 22 (C3). objects_ is a std::vector with the default allocator, so
    // it is operator new -> malloc, NOT heap_caps_malloc(kPsram): with
    // CONFIG_SPIRAM_USE_MALLOC=y and SPIRAM_MALLOC_ALWAYSINTERNAL=4096 a block
    // of <= 4 KiB is taken from internal RAM first. RESERVE_DONE reports
    // which heap the vector's storage actually landed in.
    if(r.u5obj_hydrating_)ESP_LOGI(kTag,"U5OBJ RESERVE request=%u current_size=%u current_capacity=%u object_bytes=%u need_bytes=%u target_bytes=%u free_internal=%u largest_internal=%u free_psram=%u largest_psram=%u free_default=%u guard=free_psram>=need+32768 result=%d",
        unsigned(n),unsigned(r.objects_.size()),unsigned(r.objects_.capacity()),unsigned(sizeof(openu5::QuestObject)),unsigned(need),unsigned((r.objects_.size()+n)*sizeof(openu5::QuestObject)),
        unsigned(heap_caps_get_free_size(kInternal)),unsigned(heap_caps_get_largest_free_block(kInternal)),unsigned(free_psram),unsigned(heap_caps_get_largest_free_block(kPsram)),unsigned(heap_caps_get_free_size(MALLOC_CAP_DEFAULT)),ok);
    if(!ok)return false;
    r.objects_.reserve(r.objects_.size()+n);
    if(r.u5obj_hydrating_)ESP_LOGI(kTag,"U5OBJ RESERVE_DONE size=%u capacity=%u storage_heap=%s",unsigned(r.objects_.size()),unsigned(r.objects_.capacity()),u5obj_heap(r.objects_.data()));
    return true;}
void AlphaRuntime::object_append(void*p,const openu5::QuestObject&o){
    auto&r=*static_cast<AlphaRuntime*>(p);const auto before=r.objects_.size();
    const bool direct=o.chest&&r.context_.combat&&r.direct_troll_.active&&r.combat_.victory_context!=&r.outdoor_;
    if(direct){
        ESP_LOGI(kTag,"CHEST_INSERT_BEGIN source=direct-troll");
        ESP_LOGI(kTag,"CHEST_INSERT_COORD loc=%ld floor=%ld x=%ld y=%ld",long(o.location),long(o.floor),long(o.x),long(o.y));
        ESP_LOGI(kTag,"CHEST_INSERT_OBJECT id=%u type=chest flags=%s trap=%d contents=%ld",unsigned(before),o.trapped?"trapped":"none",o.trapped,long(o.contents));
    }
    r.objects_.push_back(o);
    if(!r.u5obj_hydrating_&&u5obj_tracked(o))u5obj_object("APPEND",before,o);
    if(o.chest)ESP_LOGI(kTag,"CHEST_INSERT source=%s loc=%ld floor=%ld world_x=%ld world_y=%ld object_id=%u type=chest flags=%s trap=%d contents=%ld collection_count_before=%u after=%u",r.context_.combat?(r.combat_.victory_context==&r.outdoor_?"roaming-victory-latch":"direct-combat-teardown"):"authored/hydrated",long(o.location),long(o.floor),long(o.x),long(o.y),unsigned(before),o.trapped?"trapped":"none",o.trapped,long(o.contents),unsigned(before),unsigned(r.objects_.size()));
    if(direct){
        r.direct_troll_.inserted_x=o.x;r.direct_troll_.inserted_y=o.y;
        const auto &found=r.objects_[before];
        ESP_LOGI(kTag,"CHEST_INSERT_COUNTS before=%u after=%u",unsigned(before),unsigned(r.objects_.size()));
        ESP_LOGI(kTag,"CHEST_VERIFY_POSTINSERT found=%d index=%u loc=%ld floor=%ld x=%ld y=%ld type=%s",found.chest,unsigned(before),long(found.location),long(found.floor),long(found.x),long(found.y),found.chest?"chest":"other");
    }
}
void AlphaRuntime::object_erase(void*p,size_t i){auto&r=*static_cast<AlphaRuntime*>(p);if(i<r.objects_.size()){if(u5obj_tracked(r.objects_[i]))u5obj_object(r.u5obj_hydrating_?"ERASE_BY_HYDRATION_DISCARD":"ERASE",i,r.objects_[i]);r.objects_.erase(r.objects_.begin()+ptrdiff_t(i));}}
void AlphaRuntime::object_write(void*p,size_t i,const openu5::QuestObject&o){auto&r=*static_cast<AlphaRuntime*>(p);if(i<r.objects_.size()){if(u5obj_tracked(r.objects_[i])||u5obj_tracked(o))u5obj_object("WRITE",i,o);r.objects_[i]=o;}}
int32_t AlphaRuntime::tile_at(void*p,int32_t x,int32_t y){auto&r=*static_cast<AlphaRuntime*>(p);return r.terrain_.effective(r.resources_.world,r.game_.position.map,x,y);}
void AlphaRuntime::volatile_tile(void*p,int32_t x,int32_t y,int32_t tile){auto&r=*static_cast<AlphaRuntime*>(p);r.terrain_.set(r.game_.position.map,x,y,tile,false,"quest.volatile_tile");}
void AlphaRuntime::persistent_tile(void*p,int32_t x,int32_t y,int32_t tile){auto&r=*static_cast<AlphaRuntime*>(p);r.terrain_.set(r.game_.position.map,x,y,tile,true,"quest.persistent_tile");}
bool AlphaRuntime::command_effect(void*,openu5::CommandEffect,openu5::EventSink){return false;}void AlphaRuntime::command_reload(void*p,openu5::ReloadEffect e,uint8_t loc,openu5::EventSink){auto&r=*static_cast<AlphaRuntime*>(p);
    // Batch 22 (C2/C4). The device has no HydrateInterior/DiscardInterior arm
    // here: the core consumes those before forwarding. hydrate_calls counts
    // the core hydrations of location 17 so far; if it did not move since the
    // previous RELOAD line, nothing hydrated the pool for this entry.
    if(loc==kU5ObjLocation&&(e==openu5::ReloadEffect::HydrateInterior||e==openu5::ReloadEffect::DiscardInterior||e==openu5::ReloadEffect::EnterNpcs))
        ESP_LOGI(kTag,"U5OBJ RELOAD effect=%s loc=%u hydrate_calls=%u pool_size=%u player=L%u/F%d",e==openu5::ReloadEffect::HydrateInterior?"HydrateInterior":e==openu5::ReloadEffect::DiscardInterior?"DiscardInterior":"EnterNpcs",
                 unsigned(loc),unsigned(r.u5obj_hydrate_calls_),unsigned(r.objects_.size()),unsigned(r.game_.position.map.location),int(r.game_.position.map.floor));
    if(e==openu5::ReloadEffect::EnterNpcs&&loc>=1&&loc<=32){auto&n=r.resources_.npc_locations[loc-1];openu5::enter_npc_map(r.actors_,n.slots,n.count,loc,uint8_t(r.game_.time.hour),r.game_.npc_dead[loc-1]);}else if(e==openu5::ReloadEffect::ClearEnemies)r.outdoor_.enemies.clear();else if(e==openu5::ReloadEffect::ClearTerrain)r.terrain_.clear_residence();else if(e==openu5::ReloadEffect::RefreshHourTiles)r.terrain_.refresh(r.resources_.world,r.game_);}
// Batch 22 -- U5OBJ checkpoints, in the order a basement visit reaches them:
// SOURCE/SRC (table selected) -> RESERVE -> HYDRATE (per tracked slot) ->
// POST_HYDRATE (pool right after hydration) -> RELOAD (device notified) ->
// PRE_PRESENT (pool at the first basement snapshot) -> PRESENT (composer) ->
// RENDER (tile handed to render_snapshot). Everything is gated on location 17.
void AlphaRuntime::u5obj_bind(){hydration_trace_={this,u5obj_begin,u5obj_slot,u5obj_end};quest_.hydration_trace=&hydration_trace_;}
void AlphaRuntime::u5obj_begin(void*p,int32_t loc,const openu5::NpcLocationData*src,size_t tables){
    auto&r=*static_cast<AlphaRuntime*>(p);r.u5obj_hydrating_=loc==kU5ObjLocation;r.u5obj_accepted_=r.u5obj_dropped_=0;if(!r.u5obj_hydrating_)return;
    ESP_LOGI(kTag,"U5OBJ SOURCE loc=%ld floor=%d hour=%u table=%s table_loc=%u count=%u slots=%s tables=%u npc_dead=0x%08lx pool_before=%u",long(loc),int(r.game_.position.map.floor),unsigned(r.game_.time.hour),
             src?"found":"missing",src?unsigned(src->location):0u,src?unsigned(src->count):0u,src&&src->slots?"present":"null",unsigned(tables),(unsigned long)(loc>=1&&loc<=32?r.game_.npc_dead[loc-1]:0),unsigned(r.objects_.size()));
    if(src&&src->slots)for(size_t i=0;i<src->count;++i){const auto&n=src->slots[i];if(!u5obj_tracked(n))continue;
        ESP_LOGI(kTag,"U5OBJ SRC slot=%u index=%u type=%u(0x%02x) tile=%u dialog=%u x=%u,%u,%u y=%u,%u,%u z=%u,%u,%u schedule=%u,%u,%u,%u ai=%u,%u,%u",unsigned(n.slot),unsigned(i),unsigned(n.type),unsigned(n.type),unsigned(n.type)+256u,unsigned(n.dialog),
                 unsigned(n.x[0]),unsigned(n.x[1]),unsigned(n.x[2]),unsigned(n.y[0]),unsigned(n.y[1]),unsigned(n.y[2]),unsigned(n.z[0]),unsigned(n.z[1]),unsigned(n.z[2]),
                 unsigned(n.times[0]),unsigned(n.times[1]),unsigned(n.times[2]),unsigned(n.times[3]),unsigned(n.ai[0]),unsigned(n.ai[1]),unsigned(n.ai[2]));}
}
void AlphaRuntime::u5obj_slot(void*p,size_t i,const openu5::NpcSlot&n,uint8_t schedule,const char*drop,const openu5::QuestObject*o){
    auto&r=*static_cast<AlphaRuntime*>(p);if(!r.u5obj_hydrating_)return;if(drop)++r.u5obj_dropped_;else ++r.u5obj_accepted_;if(!u5obj_tracked(n))return;
    if(drop||!o)ESP_LOGI(kTag,"U5OBJ HYDRATE slot=%u index=%u DROP reason=%s type=%u schedule=%u",unsigned(n.slot),unsigned(i),drop?drop:"no-object",unsigned(n.type),unsigned(schedule));
    else ESP_LOGI(kTag,"U5OBJ HYDRATE slot=%u index=%u ACCEPT schedule=%u loc=%ld floor=%ld x=%ld y=%ld tile=%ld chest=%d prop=%d plot=%d item=%d contents=%ld pool_index=%u",unsigned(n.slot),unsigned(i),unsigned(schedule),
                  long(o->location),long(o->floor),long(o->x),long(o->y),long(o->tile),o->chest,o->prop,o->plot,int(o->item),long(o->contents),unsigned(r.objects_.size()));
}
void AlphaRuntime::u5obj_end(void*p,int32_t loc,bool result,const char*reason){
    auto&r=*static_cast<AlphaRuntime*>(p);r.u5obj_hydrating_=false;if(loc!=kU5ObjLocation)return;++r.u5obj_hydrate_calls_;
    ESP_LOGI(kTag,"U5OBJ POST_HYDRATE result=%d reason=%s pool_size=%u accepted=%u dropped=%u hydrate_calls=%u",result,reason?reason:"?",unsigned(r.objects_.size()),unsigned(r.u5obj_accepted_),unsigned(r.u5obj_dropped_),unsigned(r.u5obj_hydrate_calls_));
    r.u5obj_dump_pool("POST_HYDRATE");
}
void AlphaRuntime::u5obj_dump_pool(const char*stage)const{
    // Tracked x/y on ANY floor of location 17, so a floor-encoding slip (e.g.
    // 255 instead of -1) shows up as an object at the right cell, wrong floor.
    for(int c=0;c<4;++c){bool any=false;for(size_t i=0;i<objects_.size();++i){const auto&o=objects_[i];if(o.location!=kU5ObjLocation||o.x!=kU5ObjX[c]||o.y!=kU5ObjY[c])continue;any=true;u5obj_object(stage,i,o);}
        if(!any)ESP_LOGI(kTag,"U5OBJ %s_OBJ x=%ld y=%ld none",stage,long(kU5ObjX[c]),long(kU5ObjY[c]));}
}
void AlphaRuntime::u5obj_trace_present(const openu5::PresentationSnapshot&s){
    const auto m=game_.position.map;if(m.location!=kU5ObjLocation||m.floor!=kU5ObjFloor){u5obj_in_basement_=false;return;}
    const bool entry=!u5obj_in_basement_;const int half=openu5::kPresentationWindow/2;
    if(entry){u5obj_in_basement_=true;u5obj_present_seen_=0;size_t here=0,basement=0;for(const auto&o:objects_)if(o.location==kU5ObjLocation){++here;if(o.floor==kU5ObjFloor)++basement;}
        ESP_LOGI(kTag,"U5OBJ PRE_PRESENT pool_size=%u loc17_objects=%u loc17_basement_objects=%u hydrate_calls=%u player=L%u/F%d xy=%u,%u hour=%u",unsigned(objects_.size()),unsigned(here),unsigned(basement),unsigned(u5obj_hydrate_calls_),
                 unsigned(m.location),int(m.floor),unsigned(game_.position.xy.x),unsigned(game_.position.xy.y),unsigned(game_.time.hour));
        u5obj_dump_pool("PRE_PRESENT");}
    for(int i=0;i<4;++i){const int col=kU5ObjX[i]-int(s.center.x)+half,row=kU5ObjY[i]-int(s.center.y)+half;
        const bool in=col>=0&&row>=0&&col<openu5::kPresentationWindow&&row<openu5::kPresentationWindow;const uint8_t bit=uint8_t(1u<<i);
        // After the entry line, a cell is reported once more: the first
        // snapshot that actually has it inside the 11x11 window.
        if(!entry&&(!in||(u5obj_present_seen_&bit)))continue;
        if(in)u5obj_present_seen_|=bit;
        int index=-1;long object_tile=-1;for(size_t k=0;k<objects_.size();++k){const auto&o=objects_[k];if(o.loot||o.search||o.location!=m.location||o.floor!=m.floor||o.x!=kU5ObjX[i]||o.y!=kU5ObjY[i])continue;index=int(k);object_tile=long(o.shadowlord?o.tile+256:o.tile);break;}
        int npc=-1;for(size_t k=0;k<actors_.count;++k){const auto&a=actors_.actors[k];if(a.location==m.location&&a.z==m.floor&&a.x==kU5ObjX[i]&&a.y==kU5ObjY[i]&&a.schedule.dialog){npc=a.schedule.type+256;break;}}
        const long terrain=long(terrain_.inspect(resources_.world,m,kU5ObjX[i],kU5ObjY[i]).effective);
        const int at=in?row*openu5::kPresentationWindow+col:-1;const int final_tile=in?int(s.tiles[at]):int(openu5::kPresentationOffMap);
        const char *winner=!in?"off-window":col==half&&row==half?"avatar":final_tile==openu5::kPresentationHidden?"hidden":index>=0&&final_tile==object_tile?"object":npc>=0&&final_tile==npc?"npc":final_tile==terrain?"terrain":"other";
        ESP_LOGI(kTag,"U5OBJ PRESENT x=%ld y=%ld in_window=%d cell=%d,%d visible=%d terrain=%ld npc_tile=%d object_present=%d object_index=%d object_tile=%ld final_tile=%d winner=%s center=%u,%u",long(kU5ObjX[i]),long(kU5ObjY[i]),in,col,row,in?int(s.visible[at]):-1,
                 terrain,npc,index>=0,index,object_tile,final_tile,winner,unsigned(s.center.x),unsigned(s.center.y));
        if(in)u5obj_render_pending_|=bit;}
}
void AlphaRuntime::u5obj_trace_render(const openu5::PresentationSnapshot&s){
    if(!u5obj_render_pending_)return;
    const int half=openu5::kPresentationWindow/2;
    for(int i=0;i<4;++i){if(!(u5obj_render_pending_&(1u<<i)))continue;const int col=kU5ObjX[i]-int(s.center.x)+half,row=kU5ObjY[i]-int(s.center.y)+half;
        if(col<0||row<0||col>=openu5::kPresentationWindow||row>=openu5::kPresentationWindow)continue;
        ESP_LOGI(kTag,"U5OBJ RENDER x=%ld y=%ld cell=%d,%d tile=%d",long(kU5ObjX[i]),long(kU5ObjY[i]),col,row,int(s.tiles[row*openu5::kPresentationWindow+col]));}
    u5obj_render_pending_=0;
}
const char *AlphaRuntime::banner(void*,uint8_t loc){return location_display_name(loc);}
void AlphaRuntime::start_smoke(void*p,int group){auto&r=*static_cast<AlphaRuntime*>(p);r.smoke_.start(group);r.dirty_=true;r.dirty_reason_="smoke-test-start";}
} // namespace tdeck
