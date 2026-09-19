#include "openu5/ui_debug_menu.h"
#include <algorithm>
#include <limits>

namespace openu5 {
namespace {
constexpr const char *categories[] = {
    "Teleport / Map Picker", "Party", "Stats", "Inventory", "Equipment", "Reagents",
    "Quest / Progression", "Time", "Transport", "NPC / Dungeon State",
    "Shortcuts / Presets", "Diagnostics / Smoke Tests"};
constexpr const char *teleport_items[]={"Destination","Floor","X","Y","Use default entrance","Teleport"};
constexpr const char *party_items[]={"Character","Party size","Heal party","Clear status","Revive party"};
constexpr const char *stat_items[]={"Character","Strength","Dexterity","Intelligence","Current MP","Current HP","Max HP","Experience","Level"};
constexpr const char *inventory_items[]={"Food","Gold","Keys","Gems","Torches","Skull keys","Magic carpets","Inventory index","Equipment quantity","Spell quantity","Scroll quantity","Potion quantity"};
constexpr const char *equipment_items[]={"Character","Slot","Item id","Max Party","Max Resources","Equip Best Gear","Full Test Setup"};
constexpr const char *reagent_items[]={"Reagent index","Quantity"};
constexpr const char *quest_items[]={"Quest flag","Toggle quest flag","Quest item","Toggle quest item","Shrine quest bits","Shrine visited bits","Doom bits","Kill Shadowlords"};
constexpr const char *time_items[]={"Year","Month","Day","Hour","Minute","Turns since start"};
constexpr const char *transport_items[]={"Mode","Ship hull","Ship skiffs","Wind","Sail direction","HMS Cape toggle"};
constexpr const char *npc_items[]={"Location index","NPC index","Toggle dead","Toggle met","Dungeon slot","Room","Toggle room cleared","Clear overworld enemies"};
constexpr const char *shortcut_items[]={"Max Party","Max Resources","Equip Best Gear","Full Test Setup","Kill Shadowlords","Preset: Maxed party","Preset: Stocked inventory","Preset: Combat","Preset: Dungeon","Preset: Shrine","Preset: Quest","Preset: Transport","Preset: Endgame","Preset: Low health/status","Preset: Save/load"};
constexpr const char *diagnostic_items[]={"Run All","Overworld","Local Maps","Dialogue","Shops / Inns","Inventory / Equipment","Combat","Dungeons","Shrines / Special","Transport","Quest / Progression","Persistence","Device Input","Debug Tools","Resources","Presentation"};
template<size_t N> constexpr size_t countof(const char *const (&)[N]){return N;}
int64_t clamp_add(int64_t value,int delta,int64_t lo,int64_t hi){
    if(delta>0 && value>=hi)return lo;
    if(delta<0 && value<=lo)return hi;
    return std::max(lo,std::min(hi,value+delta));
}
}

void UiDebugMenu::open(){open_=true;editing_=false;edit_typed_=false;confirming_=false;category_=-1;cursor_=0;standard_entry_=true;last_status_=DebugStatus::Applied;last_teleport_status_=DebugTeleportStatus::Applied;has_result_=false;}
void UiDebugMenu::close(){open_=false;editing_=false;edit_typed_=false;confirming_=false;category_=-1;cursor_=0;}

const char *UiDebugMenu::category_name(size_t i) const{return i<size_t(UiDebugCategory::Count)?categories[i]:"";}
size_t UiDebugMenu::item_count() const{
 if(category_<0)return size_t(UiDebugCategory::Count);
 switch(UiDebugCategory(category_)){
 case UiDebugCategory::Teleport:return countof(teleport_items);case UiDebugCategory::Party:return countof(party_items);
 case UiDebugCategory::Stats:return countof(stat_items);case UiDebugCategory::Inventory:return countof(inventory_items);
 case UiDebugCategory::Equipment:return countof(equipment_items);case UiDebugCategory::Reagents:return countof(reagent_items);
 case UiDebugCategory::QuestProgression:return countof(quest_items);case UiDebugCategory::Time:return countof(time_items);
 case UiDebugCategory::Transport:return countof(transport_items);case UiDebugCategory::NpcDungeonState:return countof(npc_items);
 case UiDebugCategory::ShortcutsPresets:return countof(shortcut_items);case UiDebugCategory::Count:break;
 case UiDebugCategory::Diagnostics:return countof(diagnostic_items);
 }return 0;
}
const char *UiDebugMenu::item_name() const{
 if(category_<0)return category_name(cursor_);
 switch(UiDebugCategory(category_)){
 case UiDebugCategory::Teleport:return teleport_items[cursor_];case UiDebugCategory::Party:return party_items[cursor_];
 case UiDebugCategory::Stats:return stat_items[cursor_];case UiDebugCategory::Inventory:return inventory_items[cursor_];
 case UiDebugCategory::Equipment:return equipment_items[cursor_];case UiDebugCategory::Reagents:return reagent_items[cursor_];
 case UiDebugCategory::QuestProgression:return quest_items[cursor_];case UiDebugCategory::Time:return time_items[cursor_];
 case UiDebugCategory::Transport:return transport_items[cursor_];case UiDebugCategory::NpcDungeonState:return npc_items[cursor_];
 case UiDebugCategory::ShortcutsPresets:return shortcut_items[cursor_];case UiDebugCategory::Count:break;
 case UiDebugCategory::Diagnostics:return diagnostic_items[cursor_];
 }return "";
}
DebugDestination UiDebugMenu::destination() const{
 auto d=debug_destination_at(context_,destination_);return d.error==Error::None?d.value:DebugDestination{};
}

bool UiDebugMenu::item_edit_range(int64_t &v,int64_t &lo,int64_t &hi) const{
 auto &g=context_.game;auto &t=context_.turn;lo=0;hi=99;v=0;
 switch(UiDebugCategory(category_)){
 case UiDebugCategory::Teleport:{
  if(cursor_==0){v=int64_t(destination_);hi=std::max<int64_t>(0,int64_t(debug_destination_count(context_))-1);return true;}
  if(cursor_==1){v=int64_t(floor_);hi=std::max<int64_t>(0,int64_t(debug_floor_count(context_,destination()))-1);return true;}
  if(cursor_==2){v=teleport_x_;lo=-32768;hi=32767;return true;}if(cursor_==3){v=teleport_y_;lo=-32768;hi=32767;return true;}
  if(cursor_==4){v=standard_entry_;hi=1;return true;}return false;}
 case UiDebugCategory::Party:if(cursor_==0){v=int64_t(member_);hi=g.party.character_count?g.party.character_count-1:0;return true;}if(cursor_==1){v=g.party.party_size;hi=g.party.character_count;return true;}return false;
 case UiDebugCategory::Stats:{if(cursor_==0){v=int64_t(member_);hi=g.party.character_count?g.party.character_count-1:0;return true;}if(member_>=g.party.character_count)return false;auto &c=g.party.characters[member_];switch(cursor_){case 1:v=c.strength;hi=30;break;case 2:v=c.dexterity;hi=30;break;case 3:v=c.intelligence;hi=30;break;case 4:v=c.current_mp;break;case 5:v=c.current_hp;hi=9999;break;case 6:v=c.max_hp;hi=9999;break;case 7:v=c.exp;hi=9999;break;case 8:v=c.level;lo=1;hi=8;break;default:return false;}return true;}
 case UiDebugCategory::Inventory:{if(cursor_==7){v=int64_t(inventory_index_);hi=255;return true;}if(cursor_>=8){v=0;const size_t i=inventory_index_;if(cursor_==8)v=i<256?g.equipment_quantities[i]:0;else if(cursor_==9)v=i<48?g.spell_quantities[i]:0;else if(cursor_==10)v=i<8?g.scroll_quantities[i]:0;else v=i<8?g.potion_quantities[i]:0;return true;}switch(cursor_){case 0:v=g.food;hi=9999;break;case 1:v=g.gold;hi=9999;break;case 2:v=g.keys;break;case 3:v=g.gems;break;case 4:v=g.torches;break;case 5:v=g.skull_keys;break;case 6:v=g.magic_carpets;break;default:return false;}return true;}
 case UiDebugCategory::Equipment:if(cursor_==0){v=int64_t(member_);hi=g.party.character_count?g.party.character_count-1:0;return true;}if(cursor_==1){v=int64_t(equip_slot_);hi=5;return true;}if(cursor_==2){v=255;if(g.party.character_count){const auto &c=g.party.characters[member_];switch(equip_slot_){case 0:v=c.helmet;break;case 1:v=c.armor;break;case 2:v=c.weapon;break;case 3:v=c.shield;break;case 4:v=c.ring;break;case 5:v=c.amulet;break;default:break;}}hi=255;return true;}return false;
 case UiDebugCategory::Reagents:if(cursor_==0){v=int64_t(inventory_index_);hi=7;return true;}v=g.reagent_quantities[std::min<size_t>(inventory_index_,7)];return true;
 case UiDebugCategory::QuestProgression:if(cursor_==0){v=int64_t(quest_flag_);hi=int64_t(QuestFlag::Count)-1;return true;}if(cursor_==1){v=quest_flag(g.quest,QuestFlag(quest_flag_));hi=1;return true;}if(cursor_==2){v=int64_t(quest_item_);hi=5;return true;}if(cursor_==3){v=quest_item_<3?g.quest.shards[quest_item_]:g.quest.artifacts[quest_item_-3];hi=1;return true;}if(cursor_==4){v=g.quest.shrine_quest;hi=255;return true;}if(cursor_==5){v=g.quest.shrine_visited;hi=255;return true;}if(cursor_==6){v=g.quest.doom_bits;hi=255;return true;}return false;
 case UiDebugCategory::Time:switch(cursor_){case 0:v=g.time.year;lo=0;hi=9999;break;case 1:v=g.time.month;lo=1;hi=6;break;case 2:v=g.time.day;lo=1;hi=28;break;case 3:v=g.time.hour;hi=23;break;case 4:v=g.time.minute;hi=59;break;case 5:v=g.turns_since_start;hi=std::numeric_limits<int32_t>::max();break;default:return false;}return true;
 case UiDebugCategory::Transport:switch(cursor_){case 0:v=int(g.transport);hi=4;break;case 1:v=g.ship_hull;hi=9999;break;case 2:v=g.ship_skiffs;break;case 3:v=t.wind;hi=4;break;case 4:v=t.sail_dir;hi=4;break;case 5:v=t.hms_cape_toggle;hi=1;break;default:return false;}return true;
 case UiDebugCategory::NpcDungeonState:switch(cursor_){case 0:v=int64_t(npc_location_);hi=31;break;case 1:v=int64_t(npc_index_);hi=31;break;case 2:v=(g.npc_dead[npc_location_]>>npc_index_)&1;hi=1;break;case 3:v=(g.npc_met[npc_location_]>>npc_index_)&1;hi=1;break;case 4:v=int64_t(dungeon_slot_);hi=7;break;case 5:v=int64_t(dungeon_room_);hi=15;break;case 6:{size_t byte=dungeon_slot_*2+dungeon_room_/8;v=byte<14?((g.dungeon_rooms_cleared[byte]>>(dungeon_room_%8))&1):0;hi=1;break;}default:return false;}return true;
 case UiDebugCategory::ShortcutsPresets:case UiDebugCategory::Diagnostics:case UiDebugCategory::Count:return false;
 }return false;
}

void UiDebugMenu::apply_value(int64_t value){auto &g=context_.game;auto &t=context_.turn;
 switch(UiDebugCategory(category_)){
 case UiDebugCategory::Teleport:if(cursor_==0){destination_=size_t(value);floor_=0;standard_entry_=true;}else if(cursor_==1)floor_=size_t(value);else if(cursor_==2)teleport_x_=int32_t(value);else if(cursor_==3)teleport_y_=int32_t(value);else if(cursor_==4)standard_entry_=value!=0;break;
 case UiDebugCategory::Party:if(cursor_==0)member_=size_t(value);else set(debug_set_resource(g,DebugResource::PartySize,value));break;
 case UiDebugCategory::Stats:{if(cursor_==0){member_=size_t(value);break;}static constexpr DebugCharacterNumber fields[]={DebugCharacterNumber::Strength,DebugCharacterNumber::Dexterity,DebugCharacterNumber::Intelligence,DebugCharacterNumber::CurrentMp,DebugCharacterNumber::CurrentHp,DebugCharacterNumber::MaxHp,DebugCharacterNumber::Experience,DebugCharacterNumber::Level};set(debug_set_character_number(g,member_,fields[cursor_-1],int32_t(value)));break;}
 case UiDebugCategory::Inventory:{if(cursor_==7){inventory_index_=size_t(value);break;}if(cursor_>=8){static constexpr DebugInventory kinds[]={DebugInventory::Equipment,DebugInventory::Spells,DebugInventory::Scrolls,DebugInventory::Potions};set(debug_set_inventory_quantity(g,kinds[cursor_-8],inventory_index_,int32_t(value)));break;}static constexpr DebugResource resources[]={DebugResource::Food,DebugResource::Gold,DebugResource::Keys,DebugResource::Gems,DebugResource::Torches,DebugResource::SkullKeys,DebugResource::MagicCarpets};set(debug_set_resource(g,resources[cursor_],value));break;}
 case UiDebugCategory::Equipment:if(cursor_==0)member_=size_t(value);else if(cursor_==1)equip_slot_=size_t(value);else if(cursor_==2){static constexpr EquipSlot slots[]={EquipSlot::Helmet,EquipSlot::Armor,EquipSlot::Weapon,EquipSlot::Shield,EquipSlot::Ring,EquipSlot::Amulet};set(debug_set_equipment_slot(g,member_,slots[equip_slot_],int32_t(value)));}break;
 case UiDebugCategory::Reagents:if(cursor_==0)inventory_index_=size_t(value);else set(debug_set_inventory_quantity(g,DebugInventory::Reagents,inventory_index_,int32_t(value)));break;
 case UiDebugCategory::QuestProgression:if(cursor_==0)quest_flag_=size_t(value);else if(cursor_==1)set(debug_set_quest_flag(g,QuestFlag(quest_flag_),value!=0));else if(cursor_==2)quest_item_=size_t(value);else if(cursor_==3)set(debug_set_quest_item(g,DebugQuestItem(quest_item_),value!=0));else {static constexpr DebugQuestNumber n[]={DebugQuestNumber::ShrineQuestBitmap,DebugQuestNumber::ShrineVisitedBitmap,DebugQuestNumber::ShadowlordDoomBits};set(debug_set_quest_number(g,n[cursor_-4],int32_t(value)));}break;
 case UiDebugCategory::Time:if(cursor_<5)set(debug_set_clock(g,DebugClockPart(cursor_),int32_t(value)));else set(debug_set_resource(g,DebugResource::TurnsSinceStart,value));break;
 case UiDebugCategory::Transport:if(cursor_==0)set(debug_set_transport(g,TransportMode(value)));else if(cursor_==1)set(debug_set_resource(g,DebugResource::ShipHull,value));else if(cursor_==2)set(debug_set_resource(g,DebugResource::ShipSkiffs,value));else {static constexpr DebugRuntimeNumber n[]={DebugRuntimeNumber::Wind,DebugRuntimeNumber::SailDirection,DebugRuntimeNumber::HmsCapeToggle};set(debug_set_runtime_number(t,n[cursor_-3],int32_t(value)));}break;
 case UiDebugCategory::NpcDungeonState:if(cursor_==0)npc_location_=size_t(value);else if(cursor_==1)npc_index_=size_t(value);else if(cursor_==2)set(debug_set_npc_flag(g,DebugNpcFlag::Dead,npc_location_,npc_index_,value!=0));else if(cursor_==3)set(debug_set_npc_flag(g,DebugNpcFlag::Met,npc_location_,npc_index_,value!=0));else if(cursor_==4)dungeon_slot_=size_t(value);else if(cursor_==5)dungeon_room_=size_t(value);else set(debug_set_dungeon_room_cleared(g,dungeon_slot_,dungeon_room_,value!=0));break;
 case UiDebugCategory::ShortcutsPresets:case UiDebugCategory::Diagnostics:case UiDebugCategory::Count:break;
 }}

void UiDebugMenu::apply_action(){
 switch(UiDebugCategory(category_)){
 case UiDebugCategory::Teleport:if(cursor_==5){auto d=destination();DebugTeleportRequest r;r.kind=d.kind;r.location=d.location;
  // Default Entrance (standard_entry_) always resolves the canonical signed
  // small-map arrival floor (z=0), never the floor cursor's list ordinal --
  // ordinal 0 is the lowest sorted floor (e.g. a basement), not signed z=0.
  // Manual floor selection (standard_entry_==false) remains ordinal-driven.
  if(d.kind==DebugDestinationKind::SmallMap&&standard_entry_){r.floor=kSmallMapEntryFloor;}
  else{auto f=debug_floor_at(context_,d,floor_);r.floor=f.error==Error::None?f.value:0;}
  r.x=teleport_x_;r.y=teleport_y_;r.standard_entry=standard_entry_;last_teleport_request_=r;auto out=apply_debug_teleport(context_,r);last_teleport_status_=out.status;last_teleport_passability_known_=out.passability_known;last_teleport_passable_=out.passable;last_status_=out.status==DebugTeleportStatus::Applied?DebugStatus::Applied:DebugStatus::CoreRejected;has_result_=true;}break;
 case UiDebugCategory::Party:if(cursor_==2)set(debug_restore_party(context_.game,DebugPartyRestore::Heal));else if(cursor_==3)set(debug_restore_party(context_.game,DebugPartyRestore::ClearStatus));else if(cursor_==4)set(debug_restore_party(context_.game,DebugPartyRestore::Revive));break;
 case UiDebugCategory::Equipment:if(cursor_==3)set(apply_debug_shortcut(context_,DebugShortcut::MaximizeAll));else if(cursor_==4)set(apply_debug_shortcut(context_,DebugShortcut::MaxResources));else if(cursor_==5)set(apply_debug_shortcut(context_,DebugShortcut::BestEquipment));else if(cursor_==6)set(apply_debug_shortcut(context_,DebugShortcut::FullMaxParty));break;
 case UiDebugCategory::QuestProgression:if(cursor_==7)set(apply_debug_shortcut(context_,DebugShortcut::KillShadowlords));break;
 case UiDebugCategory::NpcDungeonState:if(cursor_==7)set(debug_clear_overworld_enemies(context_));break;
 case UiDebugCategory::ShortcutsPresets:if(cursor_==0)set(apply_debug_shortcut(context_,DebugShortcut::MaximizeAll));else if(cursor_==1)set(apply_debug_shortcut(context_,DebugShortcut::MaxResources));else if(cursor_==2)set(apply_debug_shortcut(context_,DebugShortcut::BestEquipment));else if(cursor_==3)set(apply_debug_shortcut(context_,DebugShortcut::FullMaxParty));else if(cursor_==4)set(apply_debug_shortcut(context_,DebugShortcut::KillShadowlords));else set(apply_debug_preset(context_,DebugPreset(cursor_-5)));break;
 case UiDebugCategory::Diagnostics:if(diagnostics_.start)diagnostics_.start(diagnostics_.context,cursor_?int(cursor_-1):-1);break;
 default:break;
 }}

bool UiDebugMenu::action_requires_confirmation() const{
 return (category_==int16_t(UiDebugCategory::Equipment)&&cursor_==6)||
        (category_==int16_t(UiDebugCategory::ShortcutsPresets)&&cursor_==3);
}
void UiDebugMenu::enter_or_apply(){int64_t v,lo,hi;if(item_edit_range(v,lo,hi)){editing_=true;edit_typed_=false;edit_value_=v;edit_min_=lo;edit_max_=hi;}else if(action_requires_confirmation())confirming_=true;else apply_action();}
bool UiDebugMenu::handle_input(const UiAction &a){if(!open_)return false;
 if(confirming_){if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){confirming_=false;return true;}if(a.kind==UiActionKind::Confirm){confirming_=false;apply_action();}return true;}
 if(editing_){if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){editing_=false;edit_typed_=false;return true;}if(a.kind==UiActionKind::Confirm){apply_value(edit_value_);editing_=false;edit_typed_=false;return true;}if(a.kind==UiActionKind::Character&&a.character>=u'0'&&a.character<=u'9'){const int digit=a.character-u'0';edit_value_=edit_typed_?std::min<int64_t>(edit_max_,edit_value_*10+digit):std::min<int64_t>(edit_max_,digit);edit_value_=std::max(edit_min_,edit_value_);edit_typed_=true;return true;}int delta=0;if(a.kind==UiActionKind::Next)delta=1;else if(a.kind==UiActionKind::Previous)delta=-1;else if(a.kind==UiActionKind::Direction)delta=(a.direction==Direction::East||a.direction==Direction::South)?1:-1;if(delta){edit_value_=clamp_add(edit_value_,delta,edit_min_,edit_max_);edit_typed_=false;}return true;}
 if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){if(category_<0)close();else{category_=-1;cursor_=0;}return true;}
 const auto n=item_count();if(!n)return true;
 if(a.kind==UiActionKind::Next)cursor_=(cursor_+1)%n;else if(a.kind==UiActionKind::Previous)cursor_=(cursor_+n-1)%n;else if(a.kind==UiActionKind::Direction){if(a.direction==Direction::South||a.direction==Direction::East)cursor_=(cursor_+1)%n;else cursor_=(cursor_+n-1)%n;}else if(a.kind==UiActionKind::SelectIndex&&a.index>=0&&size_t(a.index)<n){cursor_=size_t(a.index);if(category_<0){category_=int16_t(cursor_);cursor_=0;if(category_==int16_t(UiDebugCategory::Teleport))standard_entry_=true;}else enter_or_apply();}else if(a.kind==UiActionKind::Confirm){if(category_<0){category_=int16_t(cursor_);cursor_=0;if(category_==int16_t(UiDebugCategory::Teleport))standard_entry_=true;}else enter_or_apply();}return true;}
UiDebugMenuView UiDebugMenu::view() const{UiDebugMenuView v;v.open=open_;v.editing=editing_;v.confirming=confirming_;v.category=category_;v.cursor=cursor_;v.count=item_count();v.title=category_<0?"Developer":category_name(size_t(category_));v.item=item_name();v.confirmation=confirming_?"Apply full test setup?":nullptr;v.last_status=last_status_;v.last_teleport_status=last_teleport_status_;v.teleport_request=last_teleport_request_;v.teleport_passability_known=last_teleport_passability_known_;v.teleport_passable=last_teleport_passable_;v.has_result=has_result_;if(editing_){v.editable=true;v.value=edit_value_;v.minimum=edit_min_;v.maximum=edit_max_;}else{int64_t x,lo,hi;if(category_>=0&&item_edit_range(x,lo,hi)){v.editable=true;v.value=x;v.minimum=lo;v.maximum=hi;}}return v;}
} // namespace openu5
