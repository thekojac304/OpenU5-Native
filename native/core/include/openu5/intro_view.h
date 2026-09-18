#pragma once
#include <cstddef>
#include <cstdint>
namespace openu5 {
constexpr int kIntroViewColumns=19,kIntroViewRows=4,kIntroViewCells=kIntroViewColumns*kIntroViewRows;
struct IntroViewData{const uint8_t*maps=nullptr;size_t map_bytes=0;const uint8_t*script=nullptr;size_t script_bytes=0;};
enum class IntroViewEffect:uint8_t{None,Dissolve,Moongate,Beam};
struct IntroViewFrame{uint16_t tiles[kIntroViewCells]{};uint8_t terrain[kIntroViewCells]{};uint8_t scene=0;uint8_t curtain_left=9,curtain_right=9;bool thunder=false,chime=false,summon=false;uint32_t cycle=0;IntroViewEffect effect=IntroViewEffect::None;uint8_t effect_col=0,effect_row=0,effect_step=0;uint16_t effect_tile=0,effect_shown=0;};
class IntroViewPlayer{
public:
 bool bind(IntroViewData);void reset();bool advance(IntroViewFrame&);uint8_t scene()const{return scene_;}uint32_t cycle()const{return cycle_;}static const char*scene_title(uint8_t);
private:
 struct Actor{uint8_t tile=0,col=0,row=0;bool active=false;};enum class Phase:uint8_t{None,Tick,MoonRise,MoonFall,DissolveIn,DissolveOut,Summon};
 IntroViewData data_{};uint8_t terrain_[kIntroViewCells]{};Actor actors_[32]{};size_t pc_=0,loop_pc_=0;uint16_t frames_left_=0,phase_total_=0;uint8_t scene_=0,loop_count_=0,phase_slot_=0,moon_col_=0,moon_row_=0,curtain_left_=9,curtain_right_=9;bool curtain_toggle_=false;Phase phase_=Phase::None;uint32_t cycle_=0;
 bool schedule(Phase,uint16_t,uint8_t slot=0);void finish_phase();void emit(IntroViewFrame&,uint16_t);
};
}
