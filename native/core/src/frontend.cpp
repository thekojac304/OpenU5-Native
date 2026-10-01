#include "openu5/frontend.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace openu5 {
namespace {
constexpr const char *kVirtues[]={"Honesty","Compassion","Valor","Justice","Sacrifice","Honor","Spirituality","Humility"};
constexpr uint8_t kStr[]={0,0,2,0,1,1,1,0};
constexpr uint8_t kDex[]={0,2,0,1,1,0,1,0};
constexpr uint8_t kInt[]={2,0,0,1,0,1,1,0};
constexpr uint8_t kRound[]={4,2,1};
constexpr const char *kMenu[]={
    "Journey Onward", "Create New Character", "Transfer from Ultima IV",
    "Ultima V Introduction", "Acknowledgements", "Return to the View",
    "Settings", "PC Save Transfer", "Developer"
};
bool is_up(const UiAction&a){return a.kind==UiActionKind::Previous||(a.kind==UiActionKind::Direction&&a.direction==Direction::North);}
bool is_down(const UiAction&a){return a.kind==UiActionKind::Next||(a.kind==UiActionKind::Direction&&a.direction==Direction::South);}
bool is_left(const UiAction&a){return a.kind==UiActionKind::Direction&&a.direction==Direction::West;}
bool is_right(const UiAction&a){return a.kind==UiActionKind::Direction&&a.direction==Direction::East;}
char key(const UiAction&a){return a.kind==UiActionKind::Character?char(std::toupper(int(a.character&0xff))):0;}
}

const char *virtue_name(uint8_t v){return v<8?kVirtues[v]:"Unknown";}

uint8_t GypsyTournament::draw(){
    // OriginalRng's kernel recurrence, kept local so presentation cannot consume gameplay RNG.
    uint32_t x=(uint32_t(rng_)+0x9248U)&0xffffU;x=((x>>3)|(x<<13))&0xffffU;
    rng_=uint16_t((x^0x9248U)+0x11U);return uint8_t((rng_&0x7fffU)%8U);
}
uint8_t GypsyTournament::pick(){for(;;){const auto v=draw();if(!used_[v]&&!eliminated_[v]){used_[v]=true;return v;}}}
void GypsyTournament::next(){
    if(done())return;
    if(match_==0&&round_>0)std::fill(std::begin(used_),std::end(used_),false);
    const auto x=pick(),y=pick();pending_a_=std::min(x,y);pending_b_=std::max(x,y);pending_=true;
}
void GypsyTournament::begin(uint8_t s,uint8_t d,uint8_t i,uint16_t seed){
    rng_=seed;std::fill(std::begin(used_),std::end(used_),false);std::fill(std::begin(eliminated_),std::end(eliminated_),false);
    strength_=s;dexterity_=d;intelligence_=i;round_=match_=answered_=0;pending_=false;next();
}
bool GypsyTournament::answer(bool b){
    if(!pending_||done())return false;
    const uint8_t winner=b?pending_b_:pending_a_,loser=b?pending_a_:pending_b_;
    strength_=uint8_t(strength_+kStr[winner]);dexterity_=uint8_t(dexterity_+kDex[winner]);intelligence_=uint8_t(intelligence_+kInt[winner]);eliminated_[loser]=true;pending_=false;
    ++answered_;if(++match_==kRound[round_]){++round_;match_=0;}if(!done())next();return true;
}
uint8_t GypsyTournament::question_index()const{
    const int lo=pending_a_,hi=pending_b_;return uint8_t(28-((8-lo)*(7-lo))/2+(hi-lo)-1);
}
NewJourneyIdentity GypsyTournament::finish(const char*n,uint8_t g)const{
    NewJourneyIdentity out{};std::snprintf(out.name,sizeof(out.name),"%.8s",n?n:"");out.gender=g;
    out.strength=std::max<uint8_t>(strength_,20);out.dexterity=dexterity_;out.intelligence=intelligence_;out.current_mp=intelligence_;out.rng_seed_after=rng_;return out;
}

void FrontendSession::start(uint32_t now,bool developer,const FrontendSettings&s){
    // A3-01: music availability is device configuration (the audio pack read
    // at boot), not title-screen state: a Return to Title must keep it.
    const auto music=music_availability_;
    *this=FrontendSession{};music_availability_=music;developer_build_=developer;settings_=s;settings_.developer_tools_visible&=developer;enter(FrontendState::Title,now);
}
void FrontendSession::enter(FrontendState s,uint32_t now){
    state_=s;entered_ms_=now;notice_[0]=0;
    if(s==FrontendState::MainMenu){
        cursor_=0;
        if(!creation_seeded_){
            const uint32_t hundredths=(now/10U)%100U,seconds=(now/1000U)%60U;
            const uint32_t minutes=(now/60000U)%60U,hours=(now/3600000U)%24U;
            creation_seed_=time_hash_seed(int32_t(hours),int32_t(minutes),int32_t(seconds),int32_t(hundredths));
            creation_seeded_=true;
        }
    }
}
size_t FrontendSession::menu_count()const{return developer_build_&&settings_.developer_tools_visible?9:8;}
// Alpha 4 UI Batch 2: a journey's two generations by age.
SaveList order_saves(const FrontendSaveSlot(&s)[2]){
    SaveList l;
    if(s[0].present&&s[1].present){l.latest=int8_t(s[1].sequence>s[0].sequence?1:0);l.backup=int8_t(1-l.latest);}
    else if(s[0].present)l.latest=0;else if(s[1].present)l.latest=1;
    return l;
}
// Alpha 4 A4-SAVE2 (ALPHA4_UI.md section 4): the three manual slots, shared by
// the title's Load Game page, the System Menu's Save and Load pages and the
// New Journey slot choice.
bool slot_loadable(const FrontendSaveCatalog&c,int slot){
    if(slot<0||slot>=kSaveSlotCount)return false;
    const auto st=c.slots[slot].status;return st==SaveSlotStatus::Saved||st==SaveSlotStatus::Recovered;
}
int continue_slot(const FrontendSaveCatalog&c){
    int best=-1;
    for(int i=0;i<kSaveSlotCount;++i)if(slot_loadable(c,i)&&(best<0||c.slots[i].sequence>c.slots[best].sequence))best=i;
    return best;
}
int first_empty_slot(const FrontendSaveCatalog&c){
    for(int i=0;i<kSaveSlotCount;++i)if(c.slots[i].status==SaveSlotStatus::Empty)return i;
    return -1;
}
// Alpha 4 A4-UI3 (ALPHA4_UI.md section 6): one formatter for every slot list.
void format_slot_rows(char*row,char*detail,size_t cap,const FrontendSaveCatalog&c,int slot,int marked,const char*tag){
    if(!row||!detail||!cap)return;
    row[0]=detail[0]=0;if(slot<0||slot>=kSaveSlotCount)return;
    const auto&e=c.slots[slot];const bool held=slot_loadable(c,slot);
    const char*what=e.status==SaveSlotStatus::Empty?"EMPTY":!held?"DAMAGED":e.shown.name[0]?e.shown.name:"Unnamed";
    const bool mark=slot==marked&&tag,rec=e.status==SaveSlotStatus::Recovered;
    char text[96];const int n=int(std::min(kSaveRowChars,cap-1));
    // A name is at most 8 letters (the game's limit), so the tags stay in one column and in 36 cells.
    if(mark||rec)std::snprintf(text,sizeof(text),"Slot %d  %-8.8s  %s%s%s",slot+1,what,mark?tag:"",mark&&rec?", ":"",rec?"RECOVERED":"");
    else std::snprintf(text,sizeof(text),"Slot %d  %.8s",slot+1,what);
    std::snprintf(row,cap,"%.*s",n,text);
    std::snprintf(text,sizeof(text),"        %s",held?e.shown.place[0]?e.shown.place:"Unknown place":"Cannot be loaded");
    if(e.status!=SaveSlotStatus::Empty)std::snprintf(detail,cap,"%.*s",n,text);
}
void format_slot_identity(char*out,size_t cap,const FrontendSaveCatalog&c,int slot){
    if(!out||!cap)return;
    if(slot_loadable(c,slot)){const auto&v=c.slots[slot].shown;std::snprintf(out,cap,"%s, %s",v.name[0]?v.name:"Unnamed",v.place[0]?v.place:"Unknown place");}
    else std::snprintf(out,cap,"%s",slot>=0&&slot<kSaveSlotCount&&c.slots[slot].status==SaveSlotStatus::Damaged?"Damaged save":"Empty slot");
}
void list_slots(FrontendView&v,char(*rows)[96],const FrontendSaveCatalog&c,int selected,int marked,const char*tag){
    v.line_count=0;
    for(int i=0;i<kSaveSlotCount;++i){format_slot_rows(rows[2*i],rows[2*i+1],96,c,i,marked,tag);v.lines[v.line_count++]=rows[2*i];v.details[i]=rows[2*i+1];}
    v.selected_line=selected;
}
void format_slot_detail(char*out,size_t cap,const FrontendSaveCatalog&c,int slot,bool saving){
    if(!out||!cap)return;
    if(slot<0||slot>=kSaveSlotCount){out[0]=0;return;}
    const auto&e=c.slots[slot];
    switch(e.status){
    case SaveSlotStatus::Empty:std::snprintf(out,cap,"%s",saving?"Empty. Enter saves here":"Empty slot");break;
    case SaveSlotStatus::Damaged:std::snprintf(out,cap,"%s",saving?"Damaged. Enter replaces it":"Damaged: this slot cannot be loaded");break;
    case SaveSlotStatus::Recovered:std::snprintf(out,cap,"%s",saving?"Last save damaged. Enter replaces it":"Last save damaged; Enter loads the one before");break;
    case SaveSlotStatus::Saved:{const auto&v=e.shown;std::snprintf(out,cap,"%ld-%ld-%ld %02ld:%02ld, party of %u. %s",long(v.month),long(v.day),long(v.year),
                                        long(v.hour),long(v.minute),unsigned(v.party),saving?"Enter replaces it":"Enter loads");break;}
    }
}
bool FrontendSession::tick(uint32_t now){
    if(state_==FrontendState::Title&&now-entered_ms_>=1400){enter(FrontendState::IntroAnimation,now);return true;}
    if(state_==FrontendState::IntroAnimation&&intro_page_==0&&now-entered_ms_>=4200){enter(FrontendState::AttractDemo,now);return true;}
    // FONT.OVL's View script restarts forever; only input returns to the menu.
    if(state_==FrontendState::MainMenu&&now-entered_ms_>=kMenuIdleMs){enter(FrontendState::AttractDemo,now);return true;}
    // The authoritative 21-scene Summoning waits for input after every scene
    // except scene zero. It has no five-second page timer.
    return false;
}
void FrontendSession::begin_creation(uint32_t now){name_[0]=0;name_length_=0;gender_=0x0b;creation_=FrontendCreationPhase::Name;enter(FrontendState::CharacterCreation,now);}
void FrontendSession::activate_menu(uint32_t now){
    switch(cursor_){
    case 0: enter(FrontendState::Continue,now);cursor_=0;break;
    // Alpha 4 A4-SAVE2: a new journey takes the lowest empty slot; with none
    // empty the player chooses the one it replaces (never silently).
    case 1: new_journey_slot_=int8_t(first_empty_slot(catalog_));
        if(new_journey_slot_>=0){enter(FrontendState::NewJourney,now);begin_creation(now);}
        else{enter(FrontendState::NewJourneySlot,now);slot_confirm_=false;const int c=continue_slot(catalog_);cursor_=uint8_t(c>=0?c:0);}
        break;
    case 2: std::snprintf(notice_,sizeof(notice_),"Ultima IV transfer is deferred");entered_ms_=now;break;
    case 3: intro_page_=1;enter(FrontendState::IntroAnimation,now);break;
    case 4: enter(FrontendState::Credits,now);break;
    case 5: enter(FrontendState::AttractDemo,now);break;
    case 6: enter(FrontendState::Settings,now);settings_cursor_=0;break;
    // Alpha 4 A4-SAVE3: the page asks the runtime to look at the import folder.
    case 7: enter(FrontendState::PcTransfer,now);cursor_=0;pc_status_=PcImportStatus{};pending_.kind=FrontendIntentKind::InspectPcSaves;break;
    case 8: pending_.kind=FrontendIntentKind::OpenDeveloperTools;break;
    }
}
bool FrontendSession::handle(const UiAction&a,uint32_t now){
    const char k=key(a);
    if(state_==FrontendState::Title||state_==FrontendState::AttractDemo){enter(FrontendState::MainMenu,now);return true;}
    if(state_==FrontendState::IntroAnimation){
        if(intro_page_){
            if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){intro_page_=0;enter(FrontendState::MainMenu,now);}
            else if(++intro_page_>21){intro_page_=0;enter(FrontendState::MainMenu,now);}
            else entered_ms_=now;
        }else enter(FrontendState::AttractDemo,now);
        return true;
    }
    if(state_==FrontendState::Credits){enter(FrontendState::MainMenu,now);return true;}
    if(state_==FrontendState::Error){if(pc_error_){pc_error_=false;enter(FrontendState::PcTransfer,now);cursor_=0;}else enter(FrontendState::MainMenu,now);return true;}
    if(state_==FrontendState::MainMenu){
        entered_ms_=now;const auto count=menu_count();
        const char hotkeys[]="JCTUARSPD";if(k){const char*p=std::strchr(hotkeys,k);if(p&&size_t(p-hotkeys)<count){cursor_=uint8_t(p-hotkeys);activate_menu(now);return true;}}
        if(is_up(a)||k=='1'||k=='3')cursor_=uint8_t((cursor_+count-1)%count);else if(is_down(a)||k=='2'||k=='4')cursor_=uint8_t((cursor_+1)%count);
        else if(a.kind==UiActionKind::Confirm||k==' ')activate_menu(now);
        else return false;
        return true;
    }
    if(state_==FrontendState::Continue){
        if(is_up(a)||is_down(a))cursor_^=1;else if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back)enter(FrontendState::MainMenu,now);
        else if(a.kind==UiActionKind::Confirm){if(cursor_==0)pending_.kind=FrontendIntentKind::ContinueLatest;else{enter(FrontendState::Load,now);const int c=continue_slot(catalog_);cursor_=uint8_t(c>=0?c:0);}}
        else return false;
        return true;
    }
    if(state_==FrontendState::Load){
        // Alpha 4 A4-SAVE2: Slots 1-3; the cursor starts on the slot Continue
        // would restore. Back returns to the Journey Onward page it came from.
        if(is_up(a)){cursor_=uint8_t((cursor_+kSaveSlotCount-1)%kSaveSlotCount);notice_[0]=0;}
        else if(is_down(a)){cursor_=uint8_t((cursor_+1)%kSaveSlotCount);notice_[0]=0;}
        else if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){enter(FrontendState::Continue,now);cursor_=1;}
        else if(a.kind==UiActionKind::Confirm){
            if(slot_loadable(catalog_,cursor_)){pending_.kind=FrontendIntentKind::LoadSlot;pending_.slot=int8_t(cursor_);}
            else std::snprintf(notice_,sizeof(notice_),catalog_.slots[cursor_].status==SaveSlotStatus::Empty?"Slot %u is empty":"Slot %u is damaged and cannot load",unsigned(cursor_+1));}
        else return false;
        return true;
    }
    if(state_==FrontendState::NewJourneySlot){
        // Every slot holds a journey: choose the one to replace, then answer
        // "Replace Slot N?" -- No is the default and Back never replaces.
        // Nothing is written here; the save happens when creation completes.
        if(!slot_confirm_){
            if(is_up(a))cursor_=uint8_t((cursor_+kSaveSlotCount-1)%kSaveSlotCount);else if(is_down(a))cursor_=uint8_t((cursor_+1)%kSaveSlotCount);
            else if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){enter(FrontendState::MainMenu,now);cursor_=1;}
            else if(a.kind==UiActionKind::Confirm){new_journey_slot_=int8_t(cursor_);slot_confirm_=true;cursor_=0;}
            else return false;
            return true;
        }
        if(is_up(a)||is_down(a))cursor_^=1;
        else if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back||(a.kind==UiActionKind::Confirm&&cursor_==0)){slot_confirm_=false;cursor_=uint8_t(new_journey_slot_);new_journey_slot_=-1;}
        else if(a.kind==UiActionKind::Confirm){enter(FrontendState::NewJourney,now);begin_creation(now);}
        else return false;
        return true;
    }
    // Alpha 4 A4-SAVE3 (ALPHA4_UI.md section 5): PC Save Transfer. Nothing is
    // written until an intent is taken; the runtime does the work.
    if(state_==FrontendState::PcTransfer){
        if(is_up(a)||is_down(a)){cursor_^=1;notice_[0]=0;}
        else if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){enter(FrontendState::MainMenu,now);cursor_=7;}
        else if(a.kind==UiActionKind::Confirm){
            if(cursor_==1){enter(FrontendState::PcExportSlot,now);const int c=continue_slot(catalog_);cursor_=uint8_t(c>=0?c:0);}
            else if(pc_status_.state==PcImportState::Ready){enter(FrontendState::PcImportSlot,now);pc_confirm_=false;pc_slot_=-1;const int e=first_empty_slot(catalog_);cursor_=uint8_t(e>=0?e:0);}
            else std::snprintf(notice_,sizeof(notice_),"%s",pc_status_.state==PcImportState::Unknown?"Still reading the import folder":pc_status_.text);}
        else return false;
        return true;
    }
    if(state_==FrontendState::PcImportSlot){
        // An empty slot imports at once. An occupied or damaged slot, or PC
        // files imported before, ask first: No is the default, Back never imports.
        if(!pc_confirm_){
            if(is_up(a)){cursor_=uint8_t((cursor_+kSaveSlotCount-1)%kSaveSlotCount);notice_[0]=0;}
            else if(is_down(a)){cursor_=uint8_t((cursor_+1)%kSaveSlotCount);notice_[0]=0;}
            else if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){enter(FrontendState::PcTransfer,now);cursor_=0;}
            else if(a.kind==UiActionKind::Confirm){pc_slot_=int8_t(cursor_);notice_[0]=0;
                if(catalog_.slots[cursor_].status==SaveSlotStatus::Empty&&pc_status_.imported_slot<0){pending_.kind=FrontendIntentKind::ImportPcSave;pending_.slot=pc_slot_;}
                else{pc_confirm_=true;cursor_=0;}}
            else return false;
            return true;
        }
        if(is_up(a)||is_down(a))cursor_^=1;
        else if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back||(a.kind==UiActionKind::Confirm&&cursor_==0)){pc_confirm_=false;cursor_=uint8_t(pc_slot_);std::snprintf(notice_,sizeof(notice_),"Import cancelled. Slot %d is unchanged",pc_slot_+1);pc_slot_=-1;}
        else if(a.kind==UiActionKind::Confirm){pc_confirm_=false;cursor_=uint8_t(pc_slot_);pending_.kind=FrontendIntentKind::ImportPcSave;pending_.slot=pc_slot_;}
        else return false;
        return true;
    }
    if(state_==FrontendState::PcExportSlot){
        if(is_up(a)){cursor_=uint8_t((cursor_+kSaveSlotCount-1)%kSaveSlotCount);notice_[0]=0;}
        else if(is_down(a)){cursor_=uint8_t((cursor_+1)%kSaveSlotCount);notice_[0]=0;}
        else if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){enter(FrontendState::PcTransfer,now);cursor_=1;}
        else if(a.kind==UiActionKind::Confirm){
            if(slot_loadable(catalog_,cursor_)){pending_.kind=FrontendIntentKind::ExportPcSave;pending_.slot=int8_t(cursor_);}
            else std::snprintf(notice_,sizeof(notice_),catalog_.slots[cursor_].status==SaveSlotStatus::Empty?"Slot %u is empty":"Slot %u cannot be loaded; nothing to export",unsigned(cursor_+1));}
        else return false;
        return true;
    }
    if(state_==FrontendState::Settings){
        if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){pending_.kind=FrontendIntentKind::PersistSettings;pending_.settings=settings_;enter(FrontendState::MainMenu,now);return true;}
        // A3-01: rows 4/5 are SFX and Music Volume (the System Menu's order);
        // Developer stays last and exists only in developer builds.
        const uint8_t setting_count=developer_build_?7U:6U;
        if(is_up(a))settings_cursor_=uint8_t((settings_cursor_+setting_count-1U)%setting_count);else if(is_down(a))settings_cursor_=uint8_t((settings_cursor_+1U)%setting_count);
        else if(is_left(a)||is_right(a)||a.kind==UiActionKind::Confirm){const int delta=is_left(a)?-1:1;switch(settings_cursor_){case 0:settings_.brightness=uint8_t(std::clamp(int(settings_.brightness)+delta*10,10,100));break;case 1:settings_.movement_mode=!settings_.movement_mode;break;case 2:settings_.trackball_responsiveness=uint16_t(std::clamp(int(settings_.trackball_responsiveness)+delta*25,25,300));break;case 3:settings_.ui_size=uint8_t((int(settings_.ui_size)+delta+3)%3);break;case 4:settings_.sound_volume=step_volume(settings_.sound_volume,delta);volume_edits_|=kSfxVolumeEdited;break;case 5:if(music_availability_==MusicAvailability::Available){settings_.music_volume=step_volume(settings_.music_volume,delta);volume_edits_|=kMusicVolumeEdited;}break;case 6:if(developer_build_)settings_.developer_tools_visible=!settings_.developer_tools_visible;break;}}
        else return false;
        return true;
    }
    if(state_==FrontendState::CharacterCreation){
        if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){enter(FrontendState::MainMenu,now);return true;}
        if(creation_==FrontendCreationPhase::Name){if(a.kind==UiActionKind::DeleteCharacter&&name_length_){name_[--name_length_]=0;return true;}if(a.kind==UiActionKind::Character&&a.character>=32&&a.character<=126&&name_length_<8){name_[name_length_++]=char(a.character);name_[name_length_]=0;return true;}if(a.kind==UiActionKind::Confirm){if(name_length_)creation_=FrontendCreationPhase::Sex;else enter(FrontendState::MainMenu,now);return true;}return false;}
        if(creation_==FrontendCreationPhase::Sex){if(k=='M'||k=='F'){gender_=k=='M'?0x0b:0x0c;tournament_.begin(15,15,15,creation_seed_);creation_=FrontendCreationPhase::Quiz;return true;}return false;}
        if(creation_==FrontendCreationPhase::Quiz&&(k=='A'||k=='B')){tournament_.answer(k=='B');if(tournament_.done()){pending_.kind=FrontendIntentKind::CreateInitialSave;pending_.slot=new_journey_slot_;pending_.identity=tournament_.finish(name_,gender_);enter(FrontendState::NewJourney,now);}return true;}return false;
    }
    return false;
}
FrontendIntent FrontendSession::take_intent(){auto out=pending_;pending_={};return out;}
void FrontendSession::complete_intent(bool ok,const char*message){
    if(ok&&(state_==FrontendState::Continue||state_==FrontendState::Load||state_==FrontendState::NewJourney)){state_=FrontendState::EnterGame;return;}
    // Alpha 4 A4-SAVE3: back on the PC Save Transfer page, saying what happened.
    const bool pc=state_==FrontendState::PcImportSlot||state_==FrontendState::PcExportSlot;
    if(ok&&pc){cursor_=state_==FrontendState::PcExportSlot?1:0;state_=FrontendState::PcTransfer;std::snprintf(notice_,sizeof(notice_),"%.63s",message?message:"Done");return;}
    if(!ok){pc_error_=pc;state_=FrontendState::Error;std::snprintf(notice_,sizeof(notice_),"%.63s",message?message:"Unable to complete request");}
}
FrontendView FrontendSession::view()const{
    FrontendView v{};v.state=state_;static char dynamic[12][96]{};for(auto &line:dynamic)line[0]=0;
    if(state_==FrontendState::Title||state_==FrontendState::IntroAnimation){if(!intro_page_)v.kind=FrontendViewKind::TitleCredits;v.title="ULTIMA V";v.subtitle="WARRIORS OF DESTINY";v.lines[v.line_count++]="Lord British presents";v.lines[v.line_count++]="Copyright 1988 Lord British";v.footer=intro_page_?"Enter advances; Mic returns":"Press a key";if(intro_page_){std::snprintf(dynamic[0],96,"The Summoning - scene %u of 21",unsigned(intro_page_));v.lines[0]=dynamic[0];v.line_count=1;if(intro_texts_&&intro_page_<=intro_text_count_)v.lines[v.line_count++]=intro_texts_[intro_page_-1];}return v;}
    if(state_==FrontendState::AttractDemo){v.kind=FrontendViewKind::Attract;v.title="ULTIMA V";v.subtitle="The Summoning";v.lines[v.line_count++]="A moongate opens in the View";v.lines[v.line_count++]="The Avatar approaches Britannia";v.footer="Any key returns to the menu";return v;}
    if(state_==FrontendState::MainMenu){v.kind=FrontendViewKind::Menu;v.title="ULTIMA V";v.subtitle="WARRIORS OF DESTINY";for(size_t i=0;i<menu_count();++i)v.lines[v.line_count++]=kMenu[i];v.selected_line=cursor_;v.footer=notice_[0]?notice_:"Select: arrows / Enter / J C T U A R";
        // Alpha 4 A4-SAVE2: where Create New Character will save.
        if(!notice_[0]&&cursor_==1){const int e=first_empty_slot(catalog_);if(e>=0)std::snprintf(dynamic[0],96,"New journey: saved in empty Slot %d",e+1);else std::snprintf(dynamic[0],96,"%s","Slots full: you choose one to replace");v.footer=dynamic[0];}
        if(!notice_[0]&&cursor_==7)v.footer="Import or export original PC/DOS saves";
        return v;}
    if(state_==FrontendState::Credits){v.kind=FrontendViewKind::Credits;v.title="Acknowledgements";v.lines[v.line_count++]="Produced and Designed by Lord British";v.footer="Press a key";return v;}
    if(state_==FrontendState::Continue){v.kind=FrontendViewKind::Menu;v.title="Journey Onward";const int c=continue_slot(catalog_);
        // Alpha 4 A4-SAVE2: Continue restores the slot with the newest save.
        // A4-UI3: and names it as the slot pages do.
        if(c>=0){format_slot_identity(dynamic[2],96,catalog_,c);std::snprintf(dynamic[0],96,"Latest: Slot %d, %.40s",c+1,dynamic[2]);}
        else std::snprintf(dynamic[0],96,"%s",catalog_.slots[0].status==SaveSlotStatus::Empty&&catalog_.slots[1].status==SaveSlotStatus::Empty&&catalog_.slots[2].status==SaveSlotStatus::Empty?"No saved journey on this card":"No saved journey can be loaded");
        v.subtitle=dynamic[0];v.lines[v.line_count++]="Continue";v.lines[v.line_count++]="Load Game";v.selected_line=cursor_;
        if(cursor_==0&&c>=0&&catalog_.slots[c].status==SaveSlotStatus::Recovered)std::snprintf(dynamic[1],96,"%s","Latest save damaged; Enter loads the one before");
        else if(cursor_==0&&c>=0)std::snprintf(dynamic[1],96,"Enter continues Slot %d; Mic returns",c+1);else std::snprintf(dynamic[1],96,"%s",cursor_==0?"Enter continues; Mic returns":"Choose a saved journey");
        v.footer=dynamic[1];return v;}
    if(state_==FrontendState::Load){v.kind=FrontendViewKind::Menu;v.title="Load Game";v.subtitle="Choose a saved journey";
        // A4-UI3: the title's slot pages mark the slot Continue restores.
        list_slots(v,dynamic,catalog_,cursor_,continue_slot(catalog_),kLatestSlotTag);
        format_slot_detail(dynamic[6],96,catalog_,cursor_,false);v.footer=notice_[0]?notice_:dynamic[6];return v;}
    if(state_==FrontendState::NewJourneySlot){v.kind=FrontendViewKind::Menu;
        if(!slot_confirm_){v.title="New Journey";v.subtitle="Every slot is in use. Choose one to replace";
            list_slots(v,dynamic,catalog_,cursor_,continue_slot(catalog_),kLatestSlotTag);v.footer="Enter chooses; Mic returns";return v;}
        std::snprintf(dynamic[8],96,"Replace Slot %d?",new_journey_slot_+1);v.title=dynamic[8];
        format_slot_identity(dynamic[7],96,catalog_,new_journey_slot_);v.subtitle=dynamic[7];
        v.lines[v.line_count++]="No, keep it";v.lines[v.line_count++]="Yes, start a new journey here";v.selected_line=cursor_;
        v.footer=cursor_==0?"Keeps the saved journey":"The saved journey in this slot is replaced";return v;}
    if(state_==FrontendState::PcTransfer){v.kind=FrontendViewKind::Menu;v.title="PC Save Transfer";
        if(pc_status_.state==PcImportState::Unknown)v.subtitle="Looking in /ultima5/import ...";
        else if(pc_status_.state==PcImportState::Ready){if(pc_status_.imported_slot>=0)std::snprintf(dynamic[0],96,"PC save: %s (in Slot %d)",pc_status_.text,pc_status_.imported_slot+1);else std::snprintf(dynamic[0],96,"PC save: %s",pc_status_.text);v.subtitle=dynamic[0];}
        else v.subtitle=pc_status_.text;
        v.lines[v.line_count++]="Import the PC save into a slot";v.lines[v.line_count++]="Export a slot as a PC save";v.selected_line=cursor_;
        v.footer=notice_[0]?notice_:cursor_==0?"Put SAVED.GAM + SAVED.OOL in /ultima5/import":"Writes /ultima5/export/slotN";return v;}
    if(state_==FrontendState::PcImportSlot){v.kind=FrontendViewKind::Menu;
        if(!pc_confirm_){v.title="Import PC Save";v.subtitle="Choose the slot it goes into";
            list_slots(v,dynamic,catalog_,cursor_,continue_slot(catalog_),kLatestSlotTag);
            format_slot_detail(dynamic[6],96,catalog_,cursor_,true);v.footer=notice_[0]?notice_:dynamic[6];return v;}
        const bool occupied=catalog_.slots[pc_slot_].status!=SaveSlotStatus::Empty;
        if(occupied)std::snprintf(dynamic[8],96,"Replace Slot %d?",pc_slot_+1);else std::snprintf(dynamic[8],96,"%s","Import it again?");v.title=dynamic[8];
        if(pc_status_.imported_slot>=0)std::snprintf(dynamic[7],96,"These PC files went into Slot %d before",pc_status_.imported_slot+1);else format_slot_identity(dynamic[7],96,catalog_,pc_slot_);v.subtitle=dynamic[7];
        v.lines[v.line_count++]="No, keep it";v.lines[v.line_count++]="Yes, import here";v.selected_line=cursor_;
        v.footer=cursor_==0?"Nothing is written":occupied?"The journey in this slot is replaced":"A second copy of the PC journey";return v;}
    if(state_==FrontendState::PcExportSlot){v.kind=FrontendViewKind::Menu;v.title="Export to PC";v.subtitle="Choose the slot to export";
        list_slots(v,dynamic,catalog_,cursor_,continue_slot(catalog_),kLatestSlotTag);const auto st=catalog_.slots[cursor_].status;
        if(st==SaveSlotStatus::Saved)std::snprintf(dynamic[6],96,"Enter writes /ultima5/export/slot%d",cursor_+1);
        else std::snprintf(dynamic[6],96,"%s",st==SaveSlotStatus::Empty?"Empty slot":st==SaveSlotStatus::Damaged?"Damaged: nothing to export":"Last save damaged; exports the one before");
        v.footer=notice_[0]?notice_:dynamic[6];return v;}
    if(state_==FrontendState::Settings){v.kind=FrontendViewKind::Settings;v.title="Settings";static const char*ui_sizes[]={"Small","Medium","Large"};std::snprintf(dynamic[0],96,"Brightness: %u%%",settings_.brightness);std::snprintf(dynamic[1],96,"Movement default: %s",settings_.movement_mode?"On":"Off");std::snprintf(dynamic[2],96,"Trackball: %u%%",unsigned(settings_.trackball_responsiveness));std::snprintf(dynamic[3],96,"Text / UI: %s",ui_sizes[std::min<unsigned>(settings_.ui_size,2)]);format_sfx_volume_row(dynamic[4],96,settings_.sound_volume,sfx_muted_);format_music_volume_row(dynamic[5],96,settings_.music_volume,music_availability_,music_muted_);for(int i=0;i<6;++i)v.lines[v.line_count++]=dynamic[i];if(developer_build_){std::snprintf(dynamic[6],96,"Developer: %s",settings_.developer_tools_visible?"Visible":"Hidden");v.lines[v.line_count++]=dynamic[6];}v.selected_line=settings_cursor_;const char*why=settings_cursor_==5?music_unavailable_reason(music_availability_):nullptr;v.footer=why?why:"Left/right changes; Mic saves";return v;}
    if(state_==FrontendState::CharacterCreation){v.title="The Summoning";if(creation_==FrontendCreationPhase::Name){v.kind=FrontendViewKind::CharacterName;v.lines[v.line_count++]="By what name shalt thou be known?";std::snprintf(dynamic[0],96,": %s_",name_);v.lines[v.line_count++]=dynamic[0];v.footer="Enter accepts; Mic returns";}else if(creation_==FrontendCreationPhase::Sex){v.kind=FrontendViewKind::CharacterGender;v.lines[v.line_count++]="Art thou Male or Female?";v.lines[v.line_count++]="(M)ale     (F)emale";v.footer="Choose M or F; Mic returns";}else{v.kind=FrontendViewKind::CharacterQuiz;std::snprintf(dynamic[0],96,"Question %u of 7",unsigned(tournament_.answered()+1));v.lines[v.line_count++]=dynamic[0];const auto qi=tournament_.question_index();if(questions_&&qi<question_count_)v.lines[v.line_count++]=questions_[qi];else{std::snprintf(dynamic[1],96,"A) %s",virtue_name(tournament_.virtue_a()));std::snprintf(dynamic[2],96,"B) %s",virtue_name(tournament_.virtue_b()));v.lines[v.line_count++]=dynamic[1];v.lines[v.line_count++]=dynamic[2];}v.footer="Choose A or B; Mic returns";}return v;}
    if(state_==FrontendState::Error){v.title=pc_error_?"PC Save Transfer":"Journey interrupted";v.lines[v.line_count++]=notice_;v.footer="Press a key to return";return v;}
    v.title="Preparing Britannia";v.footer="Please wait";return v;
}
void apply_new_journey_identity(GameState&g,const NewJourneyIdentity&i){
    if(!g.party.character_count)return;
    auto &a=g.party.characters[0];std::memset(a.name,0,sizeof(a.name));std::memcpy(a.name,i.name,std::min<size_t>(8,std::strlen(i.name)));a.gender=i.gender;a.strength=i.strength;a.dexterity=i.dexterity;a.intelligence=i.intelligence;a.current_mp=i.current_mp;g.rng.seed(i.rng_seed_after);
}
} // namespace openu5
