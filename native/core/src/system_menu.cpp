#include "openu5/system_menu.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace openu5 {namespace {bool up(const UiAction&a){return a.kind==UiActionKind::Previous||(a.kind==UiActionKind::Direction&&a.direction==Direction::North);}bool down(const UiAction&a){return a.kind==UiActionKind::Next||(a.kind==UiActionKind::Direction&&a.direction==Direction::South);}bool left(const UiAction&a){return a.kind==UiActionKind::Direction&&a.direction==Direction::West;}bool right(const UiAction&a){return a.kind==UiActionKind::Direction&&a.direction==Direction::East;}
// A4-ENH2: the Cheats page lists groups and each group page its cheats.
// CheatKind is append-only (its value is its save bit), so a group is a run
// of this order table, not a range of the enum.
constexpr CheatKind kCheatOrder[]={CheatKind::GodMode,CheatKind::HealParty,CheatKind::CureParty,CheatKind::RestoreMp,CheatKind::ReviveParty,
 CheatKind::AddGold,CheatKind::MaxGold,CheatKind::MaxFood,CheatKind::MaxKeys,CheatKind::MaxTorches,CheatKind::MaxGems,CheatKind::GiveReagents,
 CheatKind::NoHunger,CheatKind::NoPoisonDamage,CheatKind::NoRandomEncounters};
static_assert(sizeof(kCheatOrder)/sizeof(kCheatOrder[0])==size_t(CheatKind::Count),"every cheat in a group");
constexpr uint8_t kGroupFirst[]={0,5,12,15};
constexpr const char*kGroupNames[]={"Party","Inventory","World"};
constexpr uint8_t kGroupCount=uint8_t(sizeof(kGroupNames)/sizeof(kGroupNames[0]));
constexpr const char*kGroupHelp[]={"God Mode, heal, cure, magic, revive","Gold, food, keys, torches, gems, reagents","Hunger, poison and random encounters"};
constexpr const char*kCheatHelp[]={"Party members take no damage","Restore every living member's HP","Cure poison and sleep","Left/right: amount. Enter adds it (max 9999)","Set gold to 9999",
 "Refill the spellcasters' magic points","Raise the dead with full HP","Set food to 9999","Set keys to 99","Set torches to 99","Set gems to 99","99 of each of the eight reagents",
 "No food is eaten and no one starves","Poison takes no HP; the status stays","No random monsters, overworld or dungeon"};
static_assert(sizeof(kCheatHelp)/sizeof(kCheatHelp[0])==size_t(CheatKind::Count),"a help line per cheat");
CheatKind group_cheat(uint8_t group,uint8_t row){return kCheatOrder[kGroupFirst[group]+row];}
uint8_t group_size(uint8_t group){return uint8_t(kGroupFirst[group+1]-kGroupFirst[group]);}}
void SystemMenuSession::open(const FrontendSettings&s,const FrontendSaveCatalog&catalog,int journey_slot){active_=true;page_=Page::Root;cursor_=settings_cursor_=0;settings_=s;catalog_=catalog;journey_slot_=int8_t(journey_slot);confirm_slot_=-1;pending_={};notice_[0]=0;}
// Alpha 4 A4-SAVE2: the Save and Load pages start on the live journey's slot;
// without one, Save on the first empty slot and Load on Continue's.
uint8_t SystemMenuSession::slot_cursor(bool saving)const{
 if(journey_slot_>=0&&journey_slot_<kSaveSlotCount)return uint8_t(journey_slot_);
 const int c=saving?first_empty_slot(catalog_):continue_slot(catalog_);return uint8_t(c>=0?c:0);}
void SystemMenuSession::set_notice(const char*text){std::snprintf(notice_,sizeof(notice_),"%s",text?text:"");}
bool SystemMenuSession::handle(const UiAction&a){if(!active_)return false;notice_[0]=0;if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){if(page_==Page::Root){pending_.kind=SystemMenuIntentKind::Resume;close();}else{if(page_==Page::Confirm){page_=Page::Save;cursor_=uint8_t(confirm_slot_);return true;}if(page_==Page::Settings){pending_.kind=SystemMenuIntentKind::PersistSettings;pending_.settings=settings_;}if(page_==Page::Custom||page_==Page::CheatGroup){const bool c=page_==Page::Custom;page_=c?Page::Difficulty:Page::Cheats;cursor_=c?uint8_t(Difficulty::Custom):cheat_group_;return true;}cursor_=page_==Page::Cheats?uint8_t(kCheatsRow):page_==Page::Difficulty?uint8_t(kDifficultyRow):0;page_=Page::Root;}return true;}if(page_==Page::Root){const uint8_t count=kRootRowCount;if(up(a))cursor_=uint8_t((cursor_+count-1)%count);else if(down(a))cursor_=uint8_t((cursor_+1)%count);else if(a.kind==UiActionKind::Confirm){switch(cursor_){case 0:pending_.kind=SystemMenuIntentKind::Resume;close();break;case 1:page_=Page::Save;cursor_=slot_cursor(true);break;case 2:page_=Page::Load;cursor_=slot_cursor(false);break;case 3:page_=Page::Settings;settings_cursor_=0;break;case kDifficultyRow:page_=Page::Difficulty;cursor_=uint8_t(enhanced_.difficulty);break;case kCheatsRow:page_=Page::Cheats;cursor_=0;break;case kReturnToTitleRow:pending_.kind=SystemMenuIntentKind::ReturnToTitle;close();break;}}else return false;return true;}
// Alpha 4 A4-ENH1 (targets/tdeck/ALPHA4_UI.md section 10): the difficulty
// presets, one row each; the cursor starts on the journey's own. Enter asks the
// runtime to switch to the row's preset (from the next turn on).
// A4-ENH2: Enter on Custom also opens its page.
if(page_==Page::Difficulty){const uint8_t n=uint8_t(Difficulty::Count);
 if(up(a))cursor_=uint8_t((cursor_+n-1)%n);else if(down(a))cursor_=uint8_t((cursor_+1)%n);
 else if(a.kind==UiActionKind::Confirm){pending_.kind=SystemMenuIntentKind::SetDifficulty;pending_.difficulty=Difficulty(cursor_);pending_.custom=enhanced_.custom;if(pending_.difficulty==Difficulty::Custom){page_=Page::Custom;cursor_=0;}}
 else{return false;}
 return true;}
// A4-ENH2: the Custom difficulty, a row per rule; left/right steps the row's
// value through its choices and sends it at once (it applies from the next
// turn, as a preset does).
if(page_==Page::Custom){const uint8_t n=uint8_t(RuleField::Count);
 if(up(a))cursor_=uint8_t((cursor_+n-1)%n);else if(down(a))cursor_=uint8_t((cursor_+1)%n);
 else if(left(a)||right(a)){auto&v=enhanced_.custom.*kRuleChoices[cursor_].field;v=rule_step(RuleField(cursor_),v,left(a)?-1:1);
  pending_.kind=SystemMenuIntentKind::SetDifficulty;pending_.difficulty=Difficulty::Custom;pending_.custom=enhanced_.custom;}
 else{return false;}
 return true;}
// A4-ENH2: the Cheats page lists the groups; Enter opens one.
if(page_==Page::Cheats){
 if(up(a))cursor_=uint8_t((cursor_+kGroupCount-1)%kGroupCount);else if(down(a))cursor_=uint8_t((cursor_+1)%kGroupCount);
 else if(a.kind==UiActionKind::Confirm){cheat_group_=cursor_;page_=Page::CheatGroup;cursor_=0;}
 else{return false;}
 return true;}
// A4-ENH1: a group's cheats, one row each. Enter asks the runtime to apply the
// row's cheat (apply_cheat, the one place a cheat changes the game); on Add
// Gold, left/right picks the amount first.
if(page_==Page::CheatGroup){const uint8_t n=group_size(cheat_group_);const CheatKind k=group_cheat(cheat_group_,cursor_);
 if(up(a))cursor_=uint8_t((cursor_+n-1)%n);else if(down(a))cursor_=uint8_t((cursor_+1)%n);
 else if((left(a)||right(a))&&k==CheatKind::AddGold)gold_step_=uint8_t((gold_step_+(left(a)?kAddGoldAmountCount-1:1))%kAddGoldAmountCount);
 else if(a.kind==UiActionKind::Confirm){pending_.kind=SystemMenuIntentKind::Cheat;pending_.cheat=k;pending_.amount=kAddGoldAmounts[gold_step_];}
 else{return false;}
 return true;}
if(page_==Page::Load||page_==Page::Save){
// Alpha 4 A4-SAVE2 (ALPHA4_UI.md section 4): Slots 1-3. Load restores the
// slot (its newest accepted generation); an empty slot saves at once, an
// occupied one asks first.
if(up(a))cursor_=uint8_t((cursor_+kSaveSlotCount-1)%kSaveSlotCount);else if(down(a))cursor_=uint8_t((cursor_+1)%kSaveSlotCount);else if(a.kind==UiActionKind::Confirm){char text[64];const auto st=catalog_.slots[cursor_].status;
 if(page_==Page::Load){if(slot_loadable(catalog_,cursor_)){pending_.kind=SystemMenuIntentKind::LoadSlot;pending_.slot=int8_t(cursor_);}else{std::snprintf(text,sizeof(text),st==SaveSlotStatus::Empty?"Slot %u is empty":"Slot %u is damaged and cannot load",unsigned(cursor_+1));set_notice(text);}}
 else if(st==SaveSlotStatus::Empty){pending_.kind=SystemMenuIntentKind::Save;pending_.slot=int8_t(cursor_);page_=Page::Root;cursor_=1;}
 else{confirm_slot_=int8_t(cursor_);page_=Page::Confirm;cursor_=0;}}else return false;return true;}
if(page_==Page::Confirm){
// "Overwrite Slot N?": No is row 0 and the default; No and Back leave the slot alone.
if(up(a)||down(a))cursor_^=1;else if(a.kind==UiActionKind::Confirm){if(cursor_==1){pending_.kind=SystemMenuIntentKind::Save;pending_.slot=confirm_slot_;page_=Page::Root;cursor_=1;}else{char text[64];std::snprintf(text,sizeof(text),"Slot %d kept",confirm_slot_+1);page_=Page::Save;cursor_=uint8_t(confirm_slot_);set_notice(text);}}else return false;return true;}if(up(a))settings_cursor_=uint8_t((settings_cursor_+kSettingsRowCount-1)%kSettingsRowCount);else if(down(a))settings_cursor_=uint8_t((settings_cursor_+1)%kSettingsRowCount);else if(left(a)||right(a)||a.kind==UiActionKind::Confirm){const int d=left(a)?-1:1;switch(settings_cursor_){case kBrightnessRow:settings_.brightness=uint8_t(std::clamp(int(settings_.brightness)+d*10,10,100));break;case kMovementRow:settings_.movement_mode=!settings_.movement_mode;break;case kTrackballRow:settings_.trackball_speed=uint8_t(std::clamp(int(settings_.trackball_speed)+d,int(kTrackballSpeedMin),int(kTrackballSpeedMax)));break;case kTextSizeRow:settings_.ui_size=uint8_t((int(settings_.ui_size)+d+3)%3);break;
// A3-01. Music Volume is kept, not edited, while no music is available: the
// row says so and the stored preference survives for a later music pack.
case kSfxVolumeRow:settings_.sound_volume=step_volume(settings_.sound_volume,d);volume_edits_|=kSfxVolumeEdited;break;case kMusicVolumeRow:if(music_availability_==MusicAvailability::Available){settings_.music_volume=step_volume(settings_.music_volume,d);volume_edits_|=kMusicVolumeEdited;}break;}}else return false;return true;}
SystemMenuIntent SystemMenuSession::take_intent(){auto i=pending_;pending_={};return i;}
FrontendView SystemMenuSession::view()const{FrontendView v{};v.kind=page_==Page::Settings?FrontendViewKind::Settings:FrontendViewKind::SystemMenu;v.title="System Menu";static char d[9][96]{};if(page_==Page::Root){for(auto s:{"Resume","Save Game","Load Game","Settings","Difficulty","Cheats","Return to Title"})v.lines[v.line_count++]=s;v.selected_line=cursor_;
 // Alpha 4 UI Batch 2: the selected row says what it will do.
 v.footer=notice_[0]?notice_:cursor_==1?"Choose a slot to save this journey in":cursor_==2?"Choose a saved journey to load":cursor_==kDifficultyRow?"Original, or a gentler journey":cursor_==kCheatsRow?"Optional cheats; using one marks the save":size_t(cursor_)+1==v.line_count?"Unsaved progress will be lost":"Alt+M or Mic: resume";}
 else if(page_==Page::Load||page_==Page::Save){const bool saving=page_==Page::Save;v.title=saving?"Save Game":"Load Game";v.subtitle=saving?"Choose a slot":"Choose a saved journey";
 // Alpha 4 A4-UI3: the live journey's slot is CURRENT -- where it was loaded
 // from or last saved, not a claim that the card matches the running game.
 list_slots(v,d,catalog_,cursor_,journey_slot_,kCurrentSlotTag);format_slot_detail(d[6],96,catalog_,cursor_,saving);v.footer=notice_[0]?notice_:d[6];}
 else if(page_==Page::Difficulty||page_==Page::Custom){
 // A4-ENH2: under the four rows, the selected one's actual values; the
 // Custom page is those values alone, each its own row.
 const bool custom=page_==Page::Custom;const Difficulty sel=custom?Difficulty::Custom:Difficulty(cursor_);
 const GameplayRules r=sel==Difficulty::Custom?enhanced_.custom:gameplay_rules(sel);
 v.title=custom?"Custom Difficulty":"Difficulty";std::snprintf(d[8],96,enhanced_.toggles?"Now: %s (World cheats take precedence)":"Now: %s (kept with this journey)",difficulty_name(enhanced_.difficulty));v.subtitle=d[8];
 if(!custom)for(uint8_t i=0;i<uint8_t(Difficulty::Count);++i){v.lines[v.line_count++]=difficulty_name(Difficulty(i));}
 for(uint8_t i=0;i<uint8_t(RuleField::Count);++i){char*row=d[i];if(!custom){*row++=' ';*row++=' ';}format_rule(row,94,RuleField(i),r);v.lines[v.line_count++]=d[i];}
 v.selected_line=cursor_;
 v.footer=notice_[0]?notice_:custom?"Left/right changes; kept with this journey":sel==Difficulty::Original?"The 1988 rules, unchanged":sel==Difficulty::Custom?"Enter: use Custom and set its values":"Enter: use these rules";}
 else if(page_==Page::Cheats||page_==Page::CheatGroup){v.subtitle=enhanced_.cheats_used?"This journey has used cheats":"Using one marks this journey's save";
 if(page_==Page::Cheats){v.title="Cheats";for(auto s:kGroupNames)v.lines[v.line_count++]=s;v.selected_line=cursor_;v.footer=notice_[0]?notice_:kGroupHelp[cursor_%kGroupCount];}
 else{v.title=kGroupNames[cheat_group_];
  // A4-ENH2: a toggle shows its state, Add Gold its amount, the rest their names.
  for(uint8_t i=0;i<group_size(cheat_group_);++i){const CheatKind k=group_cheat(cheat_group_,i);const char*s=cheat_name(k);
   if(cheat_is_toggle(k)){std::snprintf(d[i],96,"%s: %s",s,cheat_on(enhanced_,k)?"On":"Off");s=d[i];}
   else if(k==CheatKind::AddGold){std::snprintf(d[i],96,"%s: +%d",s,int(kAddGoldAmounts[gold_step_]));s=d[i];}
   v.lines[v.line_count++]=s;}
  v.selected_line=cursor_;v.footer=notice_[0]?notice_:kCheatHelp[unsigned(group_cheat(cheat_group_,cursor_))];}}
 else if(page_==Page::Confirm){std::snprintf(d[0],96,"Overwrite Slot %d?",confirm_slot_+1);v.title=d[0];format_slot_identity(d[1],96,catalog_,confirm_slot_);v.subtitle=d[1];v.lines[v.line_count++]="No, keep it";v.lines[v.line_count++]="Yes, overwrite";v.selected_line=cursor_;v.footer=cursor_==0?"Keeps the saved journey":"The saved journey in this slot is replaced";}else{static const char*ui_sizes[]={"Small","Medium","Large"};v.title="Settings";std::snprintf(d[0],96,"Brightness: %u%%",settings_.brightness);std::snprintf(d[1],96,"Movement default: %s",settings_.movement_mode?"On":"Off");std::snprintf(d[2],96,"Trackball speed: %u/10",unsigned(settings_.trackball_speed));std::snprintf(d[3],96,"Text / UI: %s",ui_sizes[std::min<unsigned>(settings_.ui_size,2)]);format_sfx_volume_row(d[4],96,settings_.sound_volume,sfx_muted_);format_music_volume_row(d[5],96,settings_.music_volume,music_availability_,music_muted_);for(int i=0;i<kSettingsRowCount;++i)v.lines[v.line_count++]=d[i];v.selected_line=settings_cursor_;const char*why=settings_cursor_==kMusicVolumeRow?music_unavailable_reason(music_availability_):nullptr;v.footer=why?why:settings_cursor_==kTrackballRow?kTrackballSpeedFooter:"Left/right changes; Mic saves";}
 return v;}
}
