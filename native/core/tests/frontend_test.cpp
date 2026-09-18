#include "openu5/frontend.h"
#include "openu5/frontend_settings.h"
#include "openu5/gameplay_save.h"
#include "openu5/hud.h"
#include "openu5/intro_view.h"
#include "openu5/system_menu.h"
#include "openu5/persistence.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <array>
#include <string>
#include <vector>

using namespace openu5;
namespace {
UiAction confirm(){UiAction a{};a.kind=UiActionKind::Confirm;return a;}
UiAction character(char c){UiAction a{};a.kind=UiActionKind::Character;a.character=char16_t(c);return a;}
UiAction action(UiActionKind k){UiAction a{};a.kind=k;return a;}
uint32_t hash_bytes(const void *p,size_t n){const auto*b=static_cast<const uint8_t*>(p);uint32_t h=2166136261U;for(size_t i=0;i<n;++i)h=(h^b[i])*16777619U;return h;}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"frontend check failed at line %d: %s\n",__LINE__,#x);return __LINE__;}}while(0)
}

int main(int argc,char**argv){
    FrontendSession f;FrontendSettings settings{};settings.developer_tools_visible=true;f.start(0,true,settings);
    CHECK(f.state()==FrontendState::Title);CHECK(f.tick(1400)&&f.state()==FrontendState::IntroAnimation&&f.startup_intro());
    CHECK(f.tick(5600)&&f.state()==FrontendState::AttractDemo);CHECK(!f.tick(17600)&&f.state()==FrontendState::AttractDemo);CHECK(f.handle(confirm(),17601)&&f.state()==FrontendState::MainMenu);
    GameState untouched{};untouched.gold=123;untouched.time={139,4,5,8,35};untouched.equipment_quantities[7]=4;untouched.quest.flags=0x15;untouched.rng.seed(0x5a5a);const GameState copy=untouched;
    TurnState untouched_turn{};untouched_turn.wind=4;untouched_turn.felucca_phase=0x33;CommandState untouched_commands{};untouched_commands.awaiting_troll=true;untouched_commands.troll_toll=75;
    NpcActors untouched_actors{};untouched_actors.count=1;untouched_actors.actors[0].x=17;save::Generation untouched_generations[2]{};untouched_generations[0].sequence=41;untouched_generations[0].committed=true;
    const auto game_hash=hash_bytes(&untouched,sizeof(untouched));const auto turn_hash=hash_bytes(&untouched_turn,sizeof(untouched_turn));const auto command_hash=hash_bytes(&untouched_commands,sizeof(untouched_commands));const auto actor_hash=hash_bytes(&untouched_actors,sizeof(untouched_actors));const auto generation_hash=hash_bytes(untouched_generations,sizeof(untouched_generations));const auto settings_hash=hash_bytes(&f.settings(),sizeof(f.settings()));
    for(int cycle=0;cycle<8;++cycle){const uint32_t at=29600U+uint32_t(cycle)*2U;CHECK(f.handle(character('r'),at)&&f.state()==FrontendState::AttractDemo);CHECK(f.handle(confirm(),at+1)&&f.state()==FrontendState::MainMenu);}
    CHECK(hash_bytes(&untouched,sizeof(untouched))==game_hash&&std::memcmp(&untouched,&copy,sizeof(copy))==0);CHECK(hash_bytes(&untouched_turn,sizeof(untouched_turn))==turn_hash);CHECK(hash_bytes(&untouched_commands,sizeof(untouched_commands))==command_hash);CHECK(hash_bytes(&untouched_actors,sizeof(untouched_actors))==actor_hash);CHECK(hash_bytes(untouched_generations,sizeof(untouched_generations))==generation_hash);CHECK(hash_bytes(&f.settings(),sizeof(f.settings()))==settings_hash);
    CHECK(f.handle(character('u'),130000)&&f.state()==FrontendState::IntroAnimation&&!f.startup_intro());
    CHECK(f.handle(confirm(),130001)&&f.state()==FrontendState::IntroAnimation);
    CHECK(!f.tick(230001)&&f.state()==FrontendState::IntroAnimation);
    for(int i=0;i<20;++i)CHECK(f.handle(confirm(),230002+unsigned(i)));
    CHECK(f.state()==FrontendState::MainMenu);
    CHECK(f.handle(character('u'),230100)&&f.state()==FrontendState::IntroAnimation);CHECK(f.handle(action(UiActionKind::Back),230101)&&f.state()==FrontendState::MainMenu);
    CHECK(f.handle(character('3'),230102));CHECK(f.view().selected_line==7);CHECK(f.handle(character('2'),230103));CHECK(f.view().selected_line==0);
    CHECK(f.handle(character(' '),230104)&&f.state()==FrontendState::Continue);CHECK(f.handle(confirm(),230105));auto intent=f.take_intent();CHECK(intent.kind==FrontendIntentKind::ContinueLatest);f.complete_intent(false,"No active game. Please create a character or transfer one from Ultima IV.");CHECK(f.state()==FrontendState::Error);f.handle(confirm(),230106);
    CHECK(f.handle(character('t'),230106)&&f.state()==FrontendState::MainMenu);CHECK(std::strstr(f.view().footer,"deferred")!=nullptr);CHECK(f.take_intent().kind==FrontendIntentKind::None);
    CHECK(f.handle(character('a'),230107)&&f.state()==FrontendState::Credits);CHECK(std::strstr(f.view().lines[0],"Lord British")!=nullptr);CHECK(f.handle(confirm(),230108)&&f.state()==FrontendState::MainMenu);
    CHECK(f.handle(character('r'),230109)&&f.state()==FrontendState::AttractDemo);f.handle(confirm(),230110);

    FrontendSession edit_flow;edit_flow.start(0,false,{});CHECK(edit_flow.handle(confirm(),1));
    CHECK(edit_flow.handle(character('c'),2)&&edit_flow.view().kind==FrontendViewKind::CharacterName);
    for(char c:std::string("WasdTest"))CHECK(edit_flow.handle(character(c),3));
    CHECK(edit_flow.creation_name_length()==8&&!edit_flow.handle(character('X'),4));
    CHECK(edit_flow.handle(action(UiActionKind::DeleteCharacter),5)&&std::strcmp(edit_flow.creation_name(),"WasdTes")==0);
    CHECK(edit_flow.handle(character('t'),6)&&edit_flow.handle(confirm(),7));
    CHECK(edit_flow.creation_phase()==FrontendCreationPhase::Sex&&edit_flow.view().kind==FrontendViewKind::CharacterGender);
    CHECK(edit_flow.handle(character('m'),8)&&edit_flow.creation_phase()==FrontendCreationPhase::Quiz&&edit_flow.creation_gender()==0x0b);
    CHECK(edit_flow.view().kind==FrontendViewKind::CharacterQuiz);

    CHECK(f.handle(character('c'),230111)&&f.state()==FrontendState::CharacterCreation);
    CHECK(f.handle(confirm(),230112)&&f.state()==FrontendState::MainMenu);
    CHECK(f.handle(character('c'),230113)&&f.state()==FrontendState::CharacterCreation);
    for(char c:std::string("Avery"))CHECK(f.handle(character(c),230112));CHECK(f.handle(confirm(),230113));CHECK(f.handle(character('f'),230114));
    for(int i=0;i<7;++i)CHECK(f.handle(character((i&1)?'B':'A'),230115+unsigned(i)));
    intent=f.take_intent();CHECK(intent.kind==FrontendIntentKind::CreateInitialSave);CHECK(std::strcmp(intent.identity.name,"Avery")==0&&intent.identity.gender==0x0c);
    CHECK(intent.identity.strength>=20&&intent.identity.current_mp==intent.identity.intelligence);
    CHECK(intent.identity.rng_seed_after!=0);
    f.complete_intent(true);CHECK(f.state()==FrontendState::EnterGame);

    FrontendSession load_flow;load_flow.start(0,false,{});CHECK(load_flow.handle(confirm(),1));
    FrontendSaveSlot slots[2]{};slots[0].present=true;slots[1].present=true;slots[1].valid=true;slots[1].sequence=9;std::strcpy(slots[1].name,"Avery");load_flow.set_save_slots(slots);
    CHECK(load_flow.handle(character('j'),2)&&load_flow.state()==FrontendState::Continue);
    UiAction next{};next.kind=UiActionKind::Next;CHECK(load_flow.handle(next,3));CHECK(load_flow.handle(confirm(),4)&&load_flow.state()==FrontendState::Load);CHECK(load_flow.handle(next,5));CHECK(load_flow.handle(confirm(),6));
    auto load_intent=load_flow.take_intent();CHECK(load_intent.kind==FrontendIntentKind::LoadSlot&&load_intent.slot==1);load_flow.complete_intent(true);CHECK(load_flow.state()==FrontendState::EnterGame);

    FrontendSession settings_flow;settings_flow.start(0,false,{});settings_flow.handle(confirm(),1);CHECK(settings_flow.handle(character('s'),2)&&settings_flow.state()==FrontendState::Settings);
    const auto settings_before=settings_flow.view();
    CHECK(std::strstr(settings_before.lines[2],"100%")!=nullptr);
    std::array<std::string,12> settings_lines{};
    for(size_t i=0;i<settings_before.line_count;++i)settings_lines[i]=settings_before.lines[i];
    UiAction settings_east{};settings_east.kind=UiActionKind::Direction;settings_east.direction=Direction::East;
    CHECK(settings_flow.handle(settings_east,3));
    const auto settings_after=settings_flow.view();size_t changed_rows=0;
    for(size_t i=0;i<settings_after.line_count;++i)if(settings_lines[i]!=settings_after.lines[i])++changed_rows;
    CHECK(settings_after.kind==FrontendViewKind::Settings&&settings_after.line_count==settings_before.line_count&&changed_rows==1);
    CHECK(settings_flow.handle(next,3)&&settings_flow.handle(next,3));CHECK(settings_flow.handle(settings_east,3));CHECK(settings_flow.settings().trackball_responsiveness==125);
    CHECK(settings_flow.handle(next,3)&&std::strstr(settings_flow.view().lines[3],"Medium")!=nullptr);
    CHECK(settings_flow.handle(settings_east,3)&&settings_flow.settings().ui_size==2&&std::strstr(settings_flow.view().lines[3],"Large")!=nullptr);
    UiAction back{};back.kind=UiActionKind::Back;CHECK(settings_flow.handle(back,4));CHECK(settings_flow.take_intent().kind==FrontendIntentKind::PersistSettings);

    FrontendSettings changed{};changed.brightness=70;changed.movement_mode=true;changed.trackball_responsiveness=225;changed.ui_size=0;changed.developer_tools_visible=true;changed.sound_volume=35;changed.music_volume=45;changed.touch_controls=true;
    std::string encoded;CHECK(encode_settings(changed,encoded));FrontendSettings decoded{};CHECK(decode_settings(encoded,decoded));CHECK(decoded.brightness==70&&decoded.movement_mode&&decoded.trackball_responsiveness==225&&decoded.sound_volume==35&&decoded.touch_controls);CHECK(!decode_settings("{\"version\":99}",decoded));
    // Existing preset documents migrate in place; malformed percentage steps
    // are rejected instead of silently producing an untestable timing.
    const std::string legacy="{\"version\":1,\"brightness\":70,\"movementMode\":true,\"trackballResponsiveness\":2,\"uiSize\":0,\"developerToolsVisible\":false,\"soundVolume\":0,\"musicVolume\":0,\"touchControls\":false}";
    CHECK(decode_settings(legacy,decoded)&&decoded.trackball_responsiveness==200);

    GameState game{};game.time={139,4,5,8,0};game.position={{0,0},{10,10}};TurnState turn{};turn.felucca_phase=0x33;turn.trammel_phase=0x36;int32_t moons[56]{};
    FrontendSaveSlot menu_slots[2]{};SystemMenuSession system;const auto menu_game_hash=hash_bytes(&game,sizeof(game));const auto menu_turn_hash=hash_bytes(&turn,sizeof(turn));system.open(changed,menu_slots);CHECK(system.active());CHECK(system.handle(next));CHECK(system.handle(next));CHECK(system.handle(next));CHECK(system.handle(confirm()));CHECK(system.view().kind==FrontendViewKind::Settings&&system.view().line_count==5);UiAction east{};east.kind=UiActionKind::Direction;east.direction=Direction::East;CHECK(system.handle(east));auto sys_intent=system.take_intent();CHECK(sys_intent.kind==SystemMenuIntentKind::None);CHECK(hash_bytes(&game,sizeof(game))==menu_game_hash&&hash_bytes(&turn,sizeof(turn))==menu_turn_hash);CHECK(system.handle(back));sys_intent=system.take_intent();CHECK(sys_intent.kind==SystemMenuIntentKind::PersistSettings);CHECK(system.handle(next));CHECK(system.handle(next));CHECK(system.handle(next));CHECK(system.handle(next));CHECK(system.handle(confirm()));CHECK(system.take_intent().kind==SystemMenuIntentKind::OpenDeveloper&&!system.active());
    UiTextBlock prompt_storage[8]{};UiSession live_ui({prompt_storage,8});live_ui.begin_text(UiRequestId::YellText,"Yell what?");CHECK(live_ui.handle_input(character('x')));const auto saved_mode=live_ui.mode();const auto saved_request=live_ui.request();const std::string saved_prompt=live_ui.prompt();const auto saved_input_length=live_ui.input_length();const auto saved_input=live_ui.input_buffer()[0];SystemMenuSession overlay;overlay.open(changed,menu_slots);CHECK(overlay.handle(back)&&!overlay.active());CHECK(live_ui.mode()==saved_mode&&live_ui.request()==saved_request&&saved_prompt==live_ui.prompt()&&live_ui.input_length()==saved_input_length&&live_ui.input_buffer()[0]==saved_input);
    auto hud=hud_world_state(game,turn,moons,56,false);CHECK(hud.sky_visible&&hud.wind_visible);CHECK(hud.felucca==3&&hud.trammel==6);CHECK(hud.mark_count==2&&hud.marks[0].sun&&hud.marks[0].cell==9&&hud.marks[1].cell==0&&hud.marks[1].glyph==0x33);
    CHECK(kHudViewportX+kHudViewportW<=kHudTouchX+kHudTouchW&&kHudViewportY+kHudViewportH<=kHudTouchY&&kHudTouchY+kHudTouchH==240);CHECK(kHudRightX+kHudRightW<=320&&kHudPartyFrameX+kHudPartyFrameW-1<=319&&6*kHudCellHeight<=kHudPartyFrameH&&kHudTranscriptColumns==22&&kHudTranscriptLines==19);
    CHECK(kHudSkyBarY==kHudViewportY&&kHudSkyBarX==kHudViewportX&&kHudSkyBarW==kHudViewportW&&kHudSkyBarY+kHudSkyBarH<=kHudWindBarY);
    CHECK(kHudWindBarY+kHudWindBarH==kHudViewportY+kHudViewportH&&kHudWindBarY+kHudWindBarH<kHudTouchY);
    CHECK(kHudPartyFrameY+kHudPartyFrameH<=kHudWorldFrameY&&kHudWorldFrameY+kHudWorldFrameH<=kHudTranscriptSeparatorY);
    const char*wind_labels[]={"Calm  Winds","North Winds","South Winds","East  Winds","West  Winds"};for(int w=0;w<5;++w){turn.wind=w;CHECK(std::strcmp(hud_world_state(game,turn,moons,56).wind,wind_labels[w])==0);}
    game.time.hour=6;hud=hud_world_state(game,turn,moons,56);CHECK(hud.mark_count==2&&hud.marks[0].cell==11&&hud.marks[1].cell==2);
    game.time.hour=12;hud=hud_world_state(game,turn,moons,56);CHECK(hud.mark_count==1&&hud.marks[0].sun&&hud.marks[0].cell==5);
    game.time.hour=18;hud=hud_world_state(game,turn,moons,56);CHECK(hud.mark_count==1&&!hud.marks[0].sun&&hud.marks[0].cell==8&&hud.marks[0].glyph==0x36);
    game.time.hour=20;hud=hud_world_state(game,turn,moons,56);CHECK(hud.mark_count==1&&!hud.marks[0].sun&&hud.marks[0].cell==6&&hud.marks[0].glyph==0x36);
    game.time.hour=23;hud=hud_world_state(game,turn,moons,56);CHECK(hud.mark_count==2&&hud.marks[0].cell==9&&hud.marks[1].cell==3);
    game.position.map.floor=-1;CHECK(!hud_world_state(game,turn,moons,56).sky_visible);

    // Exact seed-zero golden vectors remain deterministic even though the real
    // frontend now uses INTRO's clock seed.
    GypsyTournament all_a;all_a.begin(15,15,15,0);for(int i=0;i<7;++i)CHECK(all_a.answer(false));auto all_a_id=all_a.finish("Golden",0x0b);CHECK(all_a_id.strength==20&&all_a_id.dexterity==18&&all_a_id.intelligence==22&&all_a_id.current_mp==22);
    GypsyTournament all_b;all_b.begin(15,15,15,0);for(int i=0;i<7;++i)CHECK(all_b.answer(true));auto all_b_id=all_b.finish("Golden",0x0c);CHECK(all_b_id.strength==20&&all_b_id.dexterity==17&&all_b_id.intelligence==18&&all_b_id.current_mp==18);

    CHECK(argc>=4);std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> init((std::istreambuf_iterator<char>(input)),{});CHECK(init.size()>=save::kGamSize);
    std::ifstream ool_input(argv[2],std::ios::binary);std::vector<uint8_t> init_ool((std::istreambuf_iterator<char>(ool_input)),{});CHECK(init_ool.size()==save::kOolSize);
    GameState initial{};TurnState initial_turn{};save::Json retained;save::SidecarSource source{};CHECK(save::load_native_state(init.data(),init.size(),nullptr,initial,initial_turn,retained,source,true)==save::Error::None);
    const auto inventory_before=initial;initial.rng.seed(0x7777);apply_new_journey_identity(initial,intent.identity);CHECK(initial.rng.get_seed()==intent.identity.rng_seed_after);save::Gam gam{};save::Json side;CHECK(save::export_native_state(initial,initial_turn,retained,init.data(),init.size(),gam,side,true)==save::Error::None);
    save::preserve_new_journey_template_bytes(gam,init.data(),init.size());save::Gam expected{};std::copy_n(init.data(),save::kGamSize,expected.begin());std::fill(expected.begin()+2,expected.begin()+11,uint8_t(0));std::copy_n(reinterpret_cast<const uint8_t*>(intent.identity.name),std::strlen(intent.identity.name),expected.begin()+2);expected[11]=intent.identity.gender;expected[14]=intent.identity.strength;expected[15]=intent.identity.dexterity;expected[16]=intent.identity.intelligence;expected[17]=intent.identity.current_mp;if(gam!=expected){for(size_t i=0;i<gam.size();++i)if(gam[i]!=expected[i])std::fprintf(stderr,"GAM diff @0x%zx got=%02x expected=%02x\n",i,unsigned(gam[i]),unsigned(expected[i]));}CHECK(gam==expected);
    auto ool=save::build_ool(retained,init_ool.data(),init_ool.size());std::string side_text;CHECK(save::encode_json(side,side_text)==save::JsonError::None);CHECK(!side_text.empty()&&ool.size()==save::kOolSize);CHECK(std::equal(ool.begin(),ool.end(),init_ool.begin()));
    std::ifstream misc_input(argv[3],std::ios::binary);std::vector<uint8_t> misc((std::istreambuf_iterator<char>(misc_input)),{});CHECK(misc.size()==1871);IntroViewPlayer view_player;CHECK(view_player.bind({misc.data()+704,304,misc.data()+1216,655}));IntroViewFrame vf{};bool seen[4]{},effects[4]{};uint32_t frames=0;while(view_player.cycle()==0&&frames<10000){CHECK(view_player.advance(vf));seen[vf.scene]=true;effects[size_t(vf.effect)]=true;++frames;}CHECK(frames>1000&&frames<10000&&seen[0]&&seen[1]&&seen[2]&&seen[3]&&effects[size_t(IntroViewEffect::Dissolve)]&&effects[size_t(IntroViewEffect::Moongate)]&&effects[size_t(IntroViewEffect::Beam)]&&view_player.cycle()==1);CHECK(misc[706]==0x4f&&misc[704+32+9]==0x9d&&std::strcmp(IntroViewPlayer::scene_title(3),"The Welcoming")==0);
    GameState rebooted{};TurnState rebooted_turn{};save::Json rebooted_retained;source={};CHECK(save::load_native_state(gam.data(),gam.size(),&side_text,rebooted,rebooted_turn,rebooted_retained,source,true)==save::Error::None);
    CHECK(std::strcmp(rebooted.party.characters[0].name,"Avery")==0);CHECK(rebooted.party.characters[0].gender==0x0c);CHECK(rebooted.party.characters[0].strength==initial.party.characters[0].strength);
    CHECK(std::memcmp(rebooted.equipment_quantities,inventory_before.equipment_quantities,sizeof(rebooted.equipment_quantities))==0);
    CHECK(std::memcmp(rebooted.reagent_quantities,inventory_before.reagent_quantities,sizeof(rebooted.reagent_quantities))==0);
    CHECK(std::memcmp(rebooted.spell_quantities,inventory_before.spell_quantities,sizeof(rebooted.spell_quantities))==0);
    CHECK(rebooted.food==inventory_before.food&&rebooted.gold==inventory_before.gold&&rebooted.keys==inventory_before.keys&&rebooted.gems==inventory_before.gems);
    CHECK(rebooted.party.party_size==3&&rebooted.time.year==139&&rebooted.time.month==4&&rebooted.time.day==5&&rebooted.time.hour==8&&rebooted.time.minute==35);CHECK(rebooted.position.map.location==13&&rebooted.position.map.floor==0&&rebooted.position.xy.x==15&&rebooted.position.xy.y==15);CHECK(rebooted.karma==75&&rebooted.transport==TransportMode::Foot&&rebooted.skull_keys==0&&rebooted.torches==4);
    auto expected_state=initial;expected_state.rng=rebooted.rng;CHECK(std::memcmp(&rebooted,&expected_state,sizeof(rebooted))==0);CHECK(std::memcmp(&rebooted_turn,&initial_turn,sizeof(initial_turn))==0);

    save::Generation generations[2]{};CHECK(save::select_generation(generations,2)==-1);generations[0]={1,true,true,gam.data(),gam.size(),ool.data(),ool.size(),&side_text,true,true,save::save_crc32(gam.data(),gam.size()),save::save_crc32(ool.data(),ool.size()),save::save_crc32(reinterpret_cast<const uint8_t*>(side_text.data()),side_text.size())};CHECK(save::select_generation(generations,2)==0);generations[1]=generations[0];generations[1].sequence=2;generations[1].gam_crc^=1;CHECK(save::select_generation(generations,2)==0);
    return 0;
}
