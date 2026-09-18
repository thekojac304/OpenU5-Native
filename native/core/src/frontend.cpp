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
    "Settings", "Developer"
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
    *this=FrontendSession{};developer_build_=developer;settings_=s;settings_.developer_tools_visible&=developer;enter(FrontendState::Title,now);
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
size_t FrontendSession::menu_count()const{return developer_build_&&settings_.developer_tools_visible?8:7;}
void FrontendSession::set_save_slots(const FrontendSaveSlot(&s)[2]){saves_[0]=s[0];saves_[1]=s[1];}
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
    case 1: enter(FrontendState::NewJourney,now);begin_creation(now);break;
    case 2: std::snprintf(notice_,sizeof(notice_),"Ultima IV transfer is deferred");entered_ms_=now;break;
    case 3: intro_page_=1;enter(FrontendState::IntroAnimation,now);break;
    case 4: enter(FrontendState::Credits,now);break;
    case 5: enter(FrontendState::AttractDemo,now);break;
    case 6: enter(FrontendState::Settings,now);settings_cursor_=0;break;
    case 7: pending_.kind=FrontendIntentKind::OpenDeveloperTools;break;
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
    if(state_==FrontendState::Error){enter(FrontendState::MainMenu,now);return true;}
    if(state_==FrontendState::MainMenu){
        entered_ms_=now;const auto count=menu_count();
        const char hotkeys[]="JCTUARSD";if(k){const char*p=std::strchr(hotkeys,k);if(p&&size_t(p-hotkeys)<count){cursor_=uint8_t(p-hotkeys);activate_menu(now);return true;}}
        if(is_up(a)||k=='1'||k=='3')cursor_=uint8_t((cursor_+count-1)%count);else if(is_down(a)||k=='2'||k=='4')cursor_=uint8_t((cursor_+1)%count);
        else if(a.kind==UiActionKind::Confirm||k==' ')activate_menu(now);
        else return false;
        return true;
    }
    if(state_==FrontendState::Continue){
        if(is_up(a)||is_down(a))cursor_^=1;else if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back)enter(FrontendState::MainMenu,now);
        else if(a.kind==UiActionKind::Confirm){if(cursor_==0)pending_.kind=FrontendIntentKind::ContinueLatest;else{enter(FrontendState::Load,now);cursor_=0;}}
        else return false;
        return true;
    }
    if(state_==FrontendState::Load){
        if(is_up(a)||is_down(a))cursor_^=1;else if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){enter(FrontendState::MainMenu,now);}
        else if(a.kind==UiActionKind::Confirm){if(saves_[cursor_].valid){pending_.kind=FrontendIntentKind::LoadSlot;pending_.slot=int8_t(cursor_);}else std::snprintf(notice_,sizeof(notice_),"No valid save in this slot");}
        else return false;
        return true;
    }
    if(state_==FrontendState::Settings){
        if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){pending_.kind=FrontendIntentKind::PersistSettings;pending_.settings=settings_;enter(FrontendState::MainMenu,now);return true;}
        const uint8_t setting_count=developer_build_?5U:4U;
        if(is_up(a))settings_cursor_=uint8_t((settings_cursor_+setting_count-1U)%setting_count);else if(is_down(a))settings_cursor_=uint8_t((settings_cursor_+1U)%setting_count);
        else if(is_left(a)||is_right(a)||a.kind==UiActionKind::Confirm){const int delta=is_left(a)?-1:1;switch(settings_cursor_){case 0:settings_.brightness=uint8_t(std::clamp(int(settings_.brightness)+delta*10,10,100));break;case 1:settings_.movement_mode=!settings_.movement_mode;break;case 2:settings_.trackball_responsiveness=uint16_t(std::clamp(int(settings_.trackball_responsiveness)+delta*25,25,300));break;case 3:settings_.ui_size=uint8_t((int(settings_.ui_size)+delta+3)%3);break;case 4:if(developer_build_)settings_.developer_tools_visible=!settings_.developer_tools_visible;break;}}
        else return false;
        return true;
    }
    if(state_==FrontendState::CharacterCreation){
        if(a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back){enter(FrontendState::MainMenu,now);return true;}
        if(creation_==FrontendCreationPhase::Name){if(a.kind==UiActionKind::DeleteCharacter&&name_length_){name_[--name_length_]=0;return true;}if(a.kind==UiActionKind::Character&&a.character>=32&&a.character<=126&&name_length_<8){name_[name_length_++]=char(a.character);name_[name_length_]=0;return true;}if(a.kind==UiActionKind::Confirm){if(name_length_)creation_=FrontendCreationPhase::Sex;else enter(FrontendState::MainMenu,now);return true;}return false;}
        if(creation_==FrontendCreationPhase::Sex){if(k=='M'||k=='F'){gender_=k=='M'?0x0b:0x0c;tournament_.begin(15,15,15,creation_seed_);creation_=FrontendCreationPhase::Quiz;return true;}return false;}
        if(creation_==FrontendCreationPhase::Quiz&&(k=='A'||k=='B')){tournament_.answer(k=='B');if(tournament_.done()){pending_.kind=FrontendIntentKind::CreateInitialSave;pending_.identity=tournament_.finish(name_,gender_);enter(FrontendState::NewJourney,now);}return true;}return false;
    }
    return false;
}
FrontendIntent FrontendSession::take_intent(){auto out=pending_;pending_={};return out;}
void FrontendSession::complete_intent(bool ok,const char*message){
    if(ok&&(state_==FrontendState::Continue||state_==FrontendState::Load||state_==FrontendState::NewJourney)){state_=FrontendState::EnterGame;return;}
    if(!ok){state_=FrontendState::Error;std::snprintf(notice_,sizeof(notice_),"%.63s",message?message:"Unable to complete request");}
}
FrontendView FrontendSession::view()const{
    FrontendView v{};v.state=state_;static char dynamic[12][96]{};for(auto &line:dynamic)line[0]=0;
    if(state_==FrontendState::Title||state_==FrontendState::IntroAnimation){v.title="ULTIMA V";v.subtitle="WARRIORS OF DESTINY";v.lines[v.line_count++]="Lord British presents";v.lines[v.line_count++]="Copyright 1988 Lord British";v.footer=intro_page_?"Enter advances; Mic returns":"Press a key";if(intro_page_){std::snprintf(dynamic[0],96,"The Summoning - scene %u of 21",unsigned(intro_page_));v.lines[0]=dynamic[0];v.line_count=1;if(intro_texts_&&intro_page_<=intro_text_count_)v.lines[v.line_count++]=intro_texts_[intro_page_-1];}return v;}
    if(state_==FrontendState::AttractDemo){v.kind=FrontendViewKind::Attract;v.title="ULTIMA V";v.subtitle="The Summoning";v.lines[v.line_count++]="A moongate opens in the View";v.lines[v.line_count++]="The Avatar approaches Britannia";v.footer="Any key returns to the menu";return v;}
    if(state_==FrontendState::MainMenu){v.kind=FrontendViewKind::Menu;v.title="ULTIMA V";v.subtitle="WARRIORS OF DESTINY";for(size_t i=0;i<menu_count();++i)v.lines[v.line_count++]=kMenu[i];v.selected_line=cursor_;v.footer=notice_[0]?notice_:"Select: arrows / Enter / J C T U A R";return v;}
    if(state_==FrontendState::Credits){v.kind=FrontendViewKind::Credits;v.title="Acknowledgements";v.lines[v.line_count++]="Produced and Designed by Lord British";v.footer="Press a key";return v;}
    if(state_==FrontendState::Continue){v.kind=FrontendViewKind::Menu;v.title="Journey Onward";v.lines[v.line_count++]="Continue Latest";v.lines[v.line_count++]="Recovery / Load Previous";v.selected_line=cursor_;v.footer="Enter selects; Mic returns";return v;}
    if(state_==FrontendState::Load){v.kind=FrontendViewKind::Menu;v.title="Recovery / Load Previous";for(int i=0;i<2;++i){std::snprintf(dynamic[i],96,"Generation %d: %s",i+1,saves_[i].valid?(saves_[i].name[0]?saves_[i].name:"valid save"):saves_[i].present?"corrupt":"empty");v.lines[v.line_count++]=dynamic[i];}v.selected_line=cursor_;v.footer=notice_[0]?notice_:"Enter loads; Mic returns";return v;}
    if(state_==FrontendState::Settings){v.kind=FrontendViewKind::Settings;v.title="Settings";static const char*ui_sizes[]={"Small","Medium","Large"};std::snprintf(dynamic[0],96,"Brightness: %u%%",settings_.brightness);std::snprintf(dynamic[1],96,"Movement default: %s",settings_.movement_mode?"On":"Off");std::snprintf(dynamic[2],96,"Trackball: %u%%",unsigned(settings_.trackball_responsiveness));std::snprintf(dynamic[3],96,"Text / UI: %s",ui_sizes[std::min<unsigned>(settings_.ui_size,2)]);for(int i=0;i<4;++i)v.lines[v.line_count++]=dynamic[i];if(developer_build_){std::snprintf(dynamic[4],96,"Developer: %s",settings_.developer_tools_visible?"Visible":"Hidden");v.lines[v.line_count++]=dynamic[4];}v.selected_line=settings_cursor_;v.footer="Left/right changes; Mic saves";return v;}
    if(state_==FrontendState::CharacterCreation){v.title="The Summoning";if(creation_==FrontendCreationPhase::Name){v.kind=FrontendViewKind::CharacterName;v.lines[v.line_count++]="By what name shalt thou be known?";std::snprintf(dynamic[0],96,": %s_",name_);v.lines[v.line_count++]=dynamic[0];v.footer="Enter accepts; Mic returns";}else if(creation_==FrontendCreationPhase::Sex){v.kind=FrontendViewKind::CharacterGender;v.lines[v.line_count++]="Art thou Male or Female?";v.lines[v.line_count++]="(M)ale     (F)emale";v.footer="Choose M or F; Mic returns";}else{v.kind=FrontendViewKind::CharacterQuiz;std::snprintf(dynamic[0],96,"Question %u of 7",unsigned(tournament_.answered()+1));v.lines[v.line_count++]=dynamic[0];const auto qi=tournament_.question_index();if(questions_&&qi<question_count_)v.lines[v.line_count++]=questions_[qi];else{std::snprintf(dynamic[1],96,"A) %s",virtue_name(tournament_.virtue_a()));std::snprintf(dynamic[2],96,"B) %s",virtue_name(tournament_.virtue_b()));v.lines[v.line_count++]=dynamic[1];v.lines[v.line_count++]=dynamic[2];}v.footer="Choose A or B; Mic returns";}return v;}
    if(state_==FrontendState::Error){v.title="Journey interrupted";v.lines[v.line_count++]=notice_;v.footer="Press a key to return";return v;}
    v.title="Preparing Britannia";v.footer="Please wait";return v;
}
void apply_new_journey_identity(GameState&g,const NewJourneyIdentity&i){
    if(!g.party.character_count)return;
    auto &a=g.party.characters[0];std::memset(a.name,0,sizeof(a.name));std::memcpy(a.name,i.name,std::min<size_t>(8,std::strlen(i.name)));a.gender=i.gender;a.strength=i.strength;a.dexterity=i.dexterity;a.intelligence=i.intelligence;a.current_mp=i.current_mp;g.rng.seed(i.rng_seed_after);
}
} // namespace openu5
