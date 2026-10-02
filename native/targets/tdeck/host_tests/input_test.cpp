#include <array>
#include <cstdint>
#include <string>

#include "../main/input_controller.h"
#include "../main/keyboard_matrix.h"
#include "../main/keyboard_recovery.h"
#include "../main/device_ui_views.h"
#include "../main/location_names.h"
#include "../main/ui_input_adapter.h"

using openu5::Direction;
using tdeck::KeyTransition;
using tdeck::KeyboardEvent;
using tdeck::KeyboardMatrix;
using tdeck::RawInputEvent;
using tdeck::RawInputKind;

namespace {

using Snapshot = std::array<uint8_t, tdeck::kKeyboardColumns>;

void set_key(Snapshot &snapshot, size_t column, size_t row, bool down)
{
    if (down) snapshot[column] |= static_cast<uint8_t>(1U << row);
    else snapshot[column] &= static_cast<uint8_t>(~(1U << row));
}

size_t apply(KeyboardMatrix &matrix, const Snapshot &snapshot,
             std::array<KeyboardEvent, tdeck::kKeyboardKeyCount> &events)
{
    return matrix.apply_snapshot(snapshot.data(), events.data(), events.size());
}

RawInputEvent raw_key(const KeyboardEvent &key, int64_t time)
{
    return {
        .kind = RawInputKind::Keyboard,
        .code = key.code,
        .transition = key.transition,
        .modifiers = key.modifiers,
        .column = key.column,
        .row = key.row,
        .modifier_key = key.modifier_key,
        .timestamp_us = time,
    };
}

RawInputEvent mic(KeyTransition transition, int64_t time, uint8_t code = 0)
{
    return {
        .kind = RawInputKind::Keyboard,
        .code = code,
        .transition = transition,
        .column = tdeck::kMicrophoneKeyColumn,
        .row = tdeck::kMicrophoneKeyRow,
        .timestamp_us = time,
    };
}

RawInputEvent letter(uint8_t code, int64_t time)
{
    return {
        .kind = RawInputKind::Keyboard,
        .code = code,
        .transition = KeyTransition::Pressed,
        .timestamp_us = time,
    };
}

}  // namespace

int main()
{
    char ready_row[40]{};
    tdeck::format_ready_row(ready_row,sizeof(ready_row),"Jeweled Sword",0,true);
    if(std::string(ready_row)!="Jeweled Sword [equipped]"||
       std::string(ready_row).find("x0")!=std::string::npos)return __LINE__;
    tdeck::DeviceSelectionView stats{};stats.detail_panel=true;
    if(tdeck::selection_uses_transcript(stats))return __LINE__;
    const uint16_t palette[16]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    for(uint16_t color=0;color<16;++color){
        const auto inverted=tdeck::magic_xor_palette_pixel(palette[color],palette);
        if(inverted!=palette[color^15]||
           tdeck::magic_xor_palette_pixel(inverted,palette)!=palette[color])return __LINE__;
    }
    if(tdeck::magic_xor_palette_pixel(0xffff,palette)!=0xffff)return __LINE__;
    if(tdeck::kCharacterTitleArtY+tdeck::kCharacterTitleArtH>tdeck::kCharacterTextY||
       tdeck::kCharacterQuizArtH>tdeck::kCharacterQuizTextY)return __LINE__;
    const auto small=tdeck::ui_text_metrics(0),medium=tdeck::ui_text_metrics(1),large=tdeck::ui_text_metrics(2);
    if(!(small.glyph_width<medium.glyph_width&&medium.glyph_width<large.glyph_width&&
         small.glyph_height<medium.glyph_height&&medium.glyph_height<large.glyph_height&&
         small.cell_width<medium.cell_width&&medium.cell_width<large.cell_width&&
         tdeck::ui_scale_requires_full_layout(0,1)&&tdeck::ui_scale_requires_full_layout(1,2)&&
         !tdeck::ui_scale_requires_full_layout(1,1)))return __LINE__;
    if(!tdeck::context_action_bar_does_not_overlap(tdeck::kContextBarTop)||
       tdeck::context_action_bar_does_not_overlap(tdeck::kContextBarTop+1))return __LINE__;
    const std::string tavern_actions=tdeck::kTavernContextActions;
    if(tavern_actions.size()>tdeck::kContextBarActionColumns||
       tavern_actions.find("Round|D")==std::string::npos||
       tavern_actions.find("Drink|R")==std::string::npos)return __LINE__;
    KeyboardMatrix matrix;
    openu5::InputController controller;
    Snapshot snapshot{};
    std::array<KeyboardEvent, tdeck::kKeyboardKeyCount> events{};
    if (apply(matrix, snapshot, events) != 0) return __LINE__;

    Direction direction{};
    int64_t time = 100000;

    // Lowercase press/release survives intact and is never movement.
    set_key(snapshot, 0, 0, true); // Q
    if (apply(matrix, snapshot, events) != 1 || events[0].code != 'q' ||
        events[0].transition != KeyTransition::Pressed || events[0].modifier_key ||
        controller.normalize(raw_key(events[0], time++), direction)) return __LINE__;
    set_key(snapshot, 0, 0, false);
    if (apply(matrix, snapshot, events) != 1 || events[0].code != 'q' ||
        events[0].transition != KeyTransition::Released ||
        controller.normalize(raw_key(events[0], time++), direction)) return __LINE__;

    // Shift remains a distinct modifier event and changes the letter code.
    set_key(snapshot, 1, 6, true); // left Shift
    if (apply(matrix, snapshot, events) != 1 || !events[0].modifier_key ||
        !events[0].modifiers.shift) return __LINE__;
    set_key(snapshot, 0, 0, true);
    if (apply(matrix, snapshot, events) != 1 || events[0].code != 'Q' ||
        !events[0].modifiers.shift ||
        controller.normalize(raw_key(events[0], time++), direction)) return __LINE__;
    set_key(snapshot, 0, 0, false);
    if (apply(matrix, snapshot, events) != 1 || events[0].code != 'Q' ||
        events[0].transition != KeyTransition::Released) return __LINE__;
    set_key(snapshot, 1, 6, false);
    if (apply(matrix, snapshot, events) != 1 || matrix.modifiers().shift) return __LINE__;

    // Symbol itself is retained. Numeric and punctuation layer codes remain
    // available to future text/command handling, including press and release.
    set_key(snapshot, 0, 2, true); // Symbol
    if (apply(matrix, snapshot, events) != 1 || !events[0].modifier_key ||
        !events[0].modifiers.symbol || !matrix.modifiers().symbol) return __LINE__;
    set_key(snapshot, 1, 0, true); // Symbol+E -> '2'
    if (apply(matrix, snapshot, events) != 1 || events[0].code != '2' ||
        !events[0].modifiers.symbol ||
        controller.normalize(raw_key(events[0], time++), direction)) return __LINE__;
    set_key(snapshot, 1, 0, false);
    if (apply(matrix, snapshot, events) != 1 || events[0].code != '2' ||
        events[0].transition != KeyTransition::Released) return __LINE__;
    set_key(snapshot, 0, 3, true); // Symbol+A -> '*'
    if (apply(matrix, snapshot, events) != 1 || events[0].code != '*' ||
        !events[0].modifiers.symbol) return __LINE__;
    set_key(snapshot, 0, 3, false);
    apply(matrix, snapshot, events);
    set_key(snapshot, 0, 2, false);
    if (apply(matrix, snapshot, events) != 1 || matrix.modifiers().symbol) return __LINE__;

    // Alt is preserved independently; it does not alter or consume the code.
    set_key(snapshot, 0, 4, true); // Alt
    if (apply(matrix, snapshot, events) != 1 || !events[0].modifier_key ||
        !events[0].modifiers.alt) return __LINE__;
    set_key(snapshot, 0, 0, true);
    if (apply(matrix, snapshot, events) != 1 || events[0].code != 'q' ||
        !events[0].modifiers.alt ||
        controller.normalize(raw_key(events[0], time++), direction)) return __LINE__;
    set_key(snapshot, 0, 0, false);
    apply(matrix, snapshot, events);
    set_key(snapshot, 0, 4, false);
    if (apply(matrix, snapshot, events) != 1 || matrix.modifiers().alt) return __LINE__;

    // Physical Alpha 1.4 evidence identifies Mic/0 as matrix (0,6). Its base
    // code is zero and Symbol changes that same physical key to printable '0'.
    // Identity must therefore be bound before layer/character translation.
    set_key(snapshot,tdeck::kMicrophoneKeyColumn,tdeck::kMicrophoneKeyRow,true);
    if(apply(matrix,snapshot,events)!=1||events[0].column!=0||events[0].row!=6||
       events[0].code!=0||events[0].base_code!=0||events[0].symbol_code!='0'||
       events[0].modifiers.symbol)return __LINE__;
    set_key(snapshot,tdeck::kMicrophoneKeyColumn,tdeck::kMicrophoneKeyRow,false);
    if(apply(matrix,snapshot,events)!=1||events[0].code!=0)return __LINE__;
    set_key(snapshot,0,2,true);apply(matrix,snapshot,events);
    set_key(snapshot,tdeck::kMicrophoneKeyColumn,tdeck::kMicrophoneKeyRow,true);
    if(apply(matrix,snapshot,events)!=1||events[0].column!=0||events[0].row!=6||
       events[0].code!='0'||!events[0].modifiers.symbol)return __LINE__;
    set_key(snapshot,tdeck::kMicrophoneKeyColumn,tdeck::kMicrophoneKeyRow,false);
    if(apply(matrix,snapshot,events)!=1||events[0].code!='0')return __LINE__;
    set_key(snapshot,0,2,false);apply(matrix,snapshot,events);

    // A short Mic gesture emits Cancel on release in game and developer UI.
    tdeck::UiInputAdapter adapter;
    openu5::UiAction action{};
    tdeck::DeviceShortcut shortcut{};
    for (auto mode : {openu5::UiMode::Exploration, openu5::UiMode::Combat,
                      openu5::UiMode::TargetSelection, openu5::UiMode::TextEntry,
                      openu5::UiMode::Shop, openu5::UiMode::DebugMenu}) {
        if (adapter.translate(mic(KeyTransition::Pressed,time),mode,action,shortcut))
            return __LINE__;
        if (!adapter.translate(mic(KeyTransition::Released,time+100000),mode,action,shortcut) ||
            action.kind!=openu5::UiActionKind::Cancel ||
            shortcut!=tdeck::DeviceShortcut::None) return __LINE__;
        time+=200000;
        // Y-29: Symbol+Mic/0 must bypass the short/long special case entirely
        // and produce the literal digit, since it is the only physical route
        // to a literal '0' (and thus to SetActivePlayer's clear-active-player
        // branch) on T-Deck hardware. A plain Mic press (above) is unaffected.
        auto symbol_mic=mic(KeyTransition::Pressed,time,0);symbol_mic.modifiers.symbol=true;
        if(!adapter.translate(symbol_mic,mode,action,shortcut)||
           action.kind!=openu5::UiActionKind::Character||action.character!=u'0'||
           shortcut!=tdeck::DeviceShortcut::None) return __LINE__;
        symbol_mic.transition=KeyTransition::Released;symbol_mic.timestamp_us+=100000;
        if (adapter.translate(symbol_mic,mode,action,shortcut)) return __LINE__;
        time+=200000;
    }

    // Alt+M is the semantic SystemMenu action. Mic remains Cancel/Back.
    auto system_key=letter('m',time++);system_key.modifiers.alt=true;
    if(!adapter.translate(system_key,openu5::UiMode::TextEntry,action,shortcut)||
       action.kind!=openu5::UiActionKind::SystemMenu||shortcut!=tdeck::DeviceShortcut::None)
        return __LINE__;

    // A 1.1 s hold toggles exactly once at the threshold.  Its release never
    // emits Cancel, and a later hold can toggle the mode back off.
    tdeck::UiInputAdapter movement;
    movement.set_trackball_speed(10); // A4-ENH1: these checks are the one-pulse-one-step (old 100 %) contract
    if(movement.translate(mic(KeyTransition::Pressed,2000000),openu5::UiMode::Exploration,
                          action,shortcut))return __LINE__;
    if(movement.update(3099999,openu5::UiMode::Exploration,shortcut))return __LINE__;
    if(!movement.update(3100000,openu5::UiMode::Exploration,shortcut)||
       shortcut!=tdeck::DeviceShortcut::MovementModeToggled||
       !movement.movement_mode_enabled())return __LINE__;
    if(movement.update(3200000,openu5::UiMode::Exploration,shortcut))return __LINE__;
    if(movement.translate(mic(KeyTransition::Released,3300000),openu5::UiMode::Exploration,
                          action,shortcut))return __LINE__;
    if(!movement.movement_mode_enabled())return __LINE__;

    if(movement.translate(mic(KeyTransition::Pressed,4000000),openu5::UiMode::Dungeon,
                          action,shortcut))return __LINE__;
    if(!movement.update(5100000,openu5::UiMode::Dungeon,shortcut)||
       movement.movement_mode_enabled())return __LINE__;
    if(movement.translate(mic(KeyTransition::Released,5200000),openu5::UiMode::Dungeon,
                          action,shortcut))return __LINE__;
    if(movement.translate(mic(KeyTransition::Pressed,6000000),openu5::UiMode::Exploration,
                          action,shortcut))return __LINE__;
    if(!movement.translate(mic(KeyTransition::Released,7100000),openu5::UiMode::Exploration,
                           action,shortcut)||
       shortcut!=tdeck::DeviceShortcut::MovementModeToggled||
       !movement.movement_mode_enabled())return __LINE__;

    // WASD is a semantic Direction wherever UiSession says Direction is
    // accepted, including command prompts and combat targeting.
    const std::array<uint8_t,4> wasd={'w','a','s','d'};
    const std::array<Direction,4> wasd_directions={Direction::North,Direction::West,
                                                   Direction::South,Direction::East};
    const std::array<RawInputKind,4> prompt_track={RawInputKind::TrackballUp,
        RawInputKind::TrackballLeft,RawInputKind::TrackballDown,RawInputKind::TrackballRight};
    int prompt_mode_index=0;
    for(auto mode:{openu5::UiMode::Exploration,openu5::UiMode::Dungeon,
                   openu5::UiMode::Combat,openu5::UiMode::TargetSelection}){
        for(size_t i=0;i<wasd.size();++i){
            if(!movement.translate(letter(wasd[i],7200000+int64_t(i)),mode,action,shortcut)||
               action.kind!=openu5::UiActionKind::Direction||
               action.direction!=wasd_directions[i])return __LINE__;
            const auto from_wasd=action;
            RawInputEvent track{.kind=prompt_track[i],
                .timestamp_us=7500000+prompt_mode_index*400000+int64_t(i)*50000};
            if(!movement.translate(track,mode,action,shortcut)||
               action.kind!=from_wasd.kind||action.direction!=from_wasd.direction)
                return __LINE__;
        }
        ++prompt_mode_index;
    }
    for(auto mode:{openu5::UiMode::TextEntry,openu5::UiMode::NumericEntry,
                    openu5::UiMode::Dialogue,openu5::UiMode::Shop,
                    openu5::UiMode::DebugMenu}){
        if(movement.movement_mode_active(mode)||
           !movement.translate(letter('w',7300000),mode,action,shortcut)||
           action.kind!=openu5::UiActionKind::Character||action.character!=u'w')
            return __LINE__;
    }
    // Alpha 2.0 character creation must declare name entry as TextEntry and
    // must not opt into direction-accepting WASD. This keeps all eight name
    // characters live even when Movement Mode is enabled and makes Backspace
    // a DeleteCharacter action instead of leaving the frontend.
    for(uint8_t code:wasd)
        if(!movement.translate(letter(code,7320000+code),openu5::UiMode::TextEntry,
                               action,shortcut,false)||
           action.kind!=openu5::UiActionKind::Character||action.character!=code)
            return __LINE__;
    if(!movement.translate(letter('\b',7321000),openu5::UiMode::TextEntry,
                           action,shortcut,false)||
       action.kind!=openu5::UiActionKind::DeleteCharacter)return __LINE__;
    for(uint8_t code:{uint8_t('m'),uint8_t('f')})
        if(!movement.translate(letter(code,7322000+code),openu5::UiMode::InventorySelection,
                               action,shortcut,false)||
           action.kind!=openu5::UiActionKind::Character||action.character!=code)
            return __LINE__;
    // Direction-accepting selection/shop phases are signalled by UiSession;
    // the same mode without that signal retains ordinary shortcut letters.
    for(auto mode:{openu5::UiMode::Shop,openu5::UiMode::EquipmentSelection})
        for(size_t i=0;i<wasd.size();++i)
            if(!movement.translate(letter(wasd[i],7350000+int64_t(i)),mode,action,shortcut,true)||
               action.kind!=openu5::UiActionKind::Direction||
               action.direction!=wasd_directions[i])return __LINE__;
    tdeck::UiInputAdapter ordinary;
    for(size_t i=0;i<wasd.size();++i)
        if(!ordinary.translate(letter(wasd[i],7400000+int64_t(i)),
                               openu5::UiMode::Exploration,action,shortcut)||
           action.kind!=openu5::UiActionKind::Character||
           action.character!=char16_t(wasd[i]))return __LINE__;

    // A raw-layer resynchronization abandons a potentially missed Mic release.
    tdeck::UiInputAdapter resynchronized;
    if(resynchronized.translate(mic(KeyTransition::Pressed,8000000),
                                openu5::UiMode::Exploration,action,shortcut))return __LINE__;
    RawInputEvent reset{.kind=RawInputKind::KeyboardResynchronized,
                        .timestamp_us=8100000};
    if(resynchronized.translate(reset,openu5::UiMode::Exploration,action,shortcut))
        return __LINE__;
    if(resynchronized.translate(mic(KeyTransition::Released,9500000),
                                openu5::UiMode::Exploration,action,shortcut)||
       resynchronized.movement_mode_enabled())return __LINE__;
    if(resynchronized.translate(mic(KeyTransition::Pressed,9600000),
                                openu5::UiMode::DebugMenu,action,shortcut))return __LINE__;
    if(!resynchronized.translate(mic(KeyTransition::Released,9700000),
                                 openu5::UiMode::DebugMenu,action,shortcut)||
       action.kind!=openu5::UiActionKind::Cancel)return __LINE__;

    // A4-ENH1 (ALPHA4_UI.md section 10) replaced the percentage debounce
    // (25..300 % = a 48..4 ms same-direction window, one step per pulse) with
    // speed levels. Level 10 IS the old 100 % path: one step per pulse and the
    // same 12 ms window, checked below exactly as the percentage was.
    {
        const auto &legacy=openu5::trackball_tuning(10);
        if(legacy.pulses_per_step!=1||legacy.step_gap_ms!=0||legacy.window_ms!=12||
           &openu5::trackball_tuning(0)!=&openu5::trackball_tuning(1)||
           &openu5::trackball_tuning(99)!=&openu5::trackball_tuning(10))return __LINE__;
        // Every faster level needs no more pulses and no longer a gap.
        for(uint8_t level=2;level<=10;++level){
            const auto &slow=openu5::trackball_tuning(uint8_t(level-1)),&fast=openu5::trackball_tuning(level);
            if(fast.pulses_per_step>slow.pulses_per_step||fast.step_gap_ms>slow.step_gap_ms||
               (fast.pulses_per_step==slow.pulses_per_step&&fast.step_gap_ms==slow.step_gap_ms))return __LINE__;
        }
    }
    controller.set_trackball_level(10);
    const std::array<RawInputKind, 4> kinds = {RawInputKind::TrackballUp,
        RawInputKind::TrackballDown, RawInputKind::TrackballLeft, RawInputKind::TrackballRight};
    const std::array<Direction, 4> directions = {Direction::North, Direction::South,
        Direction::West, Direction::East};
    for (size_t i = 0; i < kinds.size(); ++i) {
        RawInputEvent track{.kind = kinds[i], .timestamp_us = 2000000};
        if (!controller.normalize(track, direction) || direction != directions[i]) return __LINE__;
        track.timestamp_us += 1000;
        if (controller.normalize(track, direction)) return __LINE__;
        track.timestamp_us += 11000;
        if (!controller.normalize(track, direction) || direction != directions[i]) return __LINE__;
    }
    // A4-ENH1: the accumulator, level by level (PROVISIONAL tuning; these
    // check the model, not the numbers' final values).
    auto pulse=[&](openu5::InputController &c,RawInputKind k,int64_t at){
        RawInputEvent e{.kind=k,.timestamp_us=at};return c.normalize(e,direction);};
    {
        // S1. The default level 5: three pulses on an axis make one step; a
        // tiny roll of one or two moves nothing. The remainder carries on.
        openu5::InputController c;
        if(c.trackball_level()!=5)return __LINE__;
        int64_t t=300000000;
        if(pulse(c,RawInputKind::TrackballRight,t)||pulse(c,RawInputKind::TrackballRight,t+20000)||
           !pulse(c,RawInputKind::TrackballRight,t+40000)||direction!=Direction::East||c.held_x()!=0)return __LINE__;
        // S2. Inside the 100 ms step gap the roll is dropped, not banked.
        if(pulse(c,RawInputKind::TrackballRight,t+60000)||pulse(c,RawInputKind::TrackballRight,t+80000)||
           c.held_x()!=0||c.stats().step_gap!=2)return __LINE__;
        if(pulse(c,RawInputKind::TrackballRight,t+150000)||pulse(c,RawInputKind::TrackballRight,t+170000)||
           !pulse(c,RawInputKind::TrackballRight,t+190000))return __LINE__;
        // S3. A tiny roll, a pause past the idle reset, another tiny roll: no step.
        t+=2000000;
        if(pulse(c,RawInputKind::TrackballUp,t)||pulse(c,RawInputKind::TrackballUp,t+20000)||c.held_y()!=-2)return __LINE__;
        if(pulse(c,RawInputKind::TrackballUp,t+20000+openu5::InputController::kIdleResetUs)||c.held_y()!=-1||
           c.stats().idle_resets!=1)return __LINE__;
        // S4. A reversal starts the count afresh: no unwinding of the old way
        // (one up pulse is held from S3; the first down pulse clears it).
        t+=20000+openu5::InputController::kIdleResetUs+30000;
        if(pulse(c,RawInputKind::TrackballDown,t)||c.held_y()!=1||c.stats().reversals!=1)return __LINE__;
        if(pulse(c,RawInputKind::TrackballDown,t+20000)||!pulse(c,RawInputKind::TrackballDown,t+40000)||
           direction!=Direction::South)return __LINE__;
        // S5. Contact bounce (a same-direction pulse inside 4 ms) is not distance.
        t+=2000000;
        if(pulse(c,RawInputKind::TrackballLeft,t)||pulse(c,RawInputKind::TrackballLeft,t+2000)||c.held_x()!=-1||
           c.stats().bounced!=1)return __LINE__;
        // S6. A diagonal roll: each axis counts on its own, so the steps alternate.
        openu5::InputController diag;
        t+=2000000;
        int east=0,north=0;Direction order[8]{};int n=0;
        for(int i=0;i<12;++i){
            const bool x=i%2==0;
            if(pulse(diag,x?RawInputKind::TrackballRight:RawInputKind::TrackballUp,t+i*30000)){
                (direction==Direction::East?east:north)++;if(n<8)order[n++]=direction;}
        }
        if(east<1||north<1||n<2||order[0]==order[1])return __LINE__;
    }
    {
        // S7. Low, medium and high are meaningfully different: one second of a
        // fast roll (a pulse every 10 ms) steps at most 4 / 10 times at levels
        // 1 / 5, at least 40 at level 10 (the old 100 %), and the steps a
        // level makes never outnumber the next faster level's.
        int steps[11]{};
        for(uint8_t level=1;level<=10;++level){
            openu5::InputController c;c.set_trackball_level(level);
            for(int i=0;i<100;++i)steps[level]+=pulse(c,RawInputKind::TrackballRight,500000000+i*10000)?1:0;
            if(level>1&&steps[level]<steps[level-1])return __LINE__;
        }
        if(steps[1]>4||steps[5]>10||steps[10]<40||steps[1]>=steps[5]||steps[5]>=steps[10])return __LINE__;
        // A slow, deliberate roll (a pulse every 120 ms): level 10 steps on
        // every pulse; level 1 needs eight.
        openu5::InputController slow1;slow1.set_trackball_level(1);
        openu5::InputController slow10;slow10.set_trackball_level(10);
        // (24 pulses: eight make a step, the 250 ms gap then drops two.)
        int s1=0,s10=0;
        for(int i=0;i<24;++i){s1+=pulse(slow1,RawInputKind::TrackballDown,600000000+i*120000)?1:0;
                              s10+=pulse(slow10,RawInputKind::TrackballDown,600000000+i*120000)?1:0;}
        if(s1!=2||s10!=24)return __LINE__;
        // A new level forgets a partial step.
        openu5::InputController change;
        pulse(change,RawInputKind::TrackballRight,700000000);
        change.set_trackball_level(4);
        if(change.held_x()!=0)return __LINE__;
    }

    // Switching repeatedly between trackball and WASD does not share debounce
    // state or create duplicate semantic moves.
    for(int i=0;i<4;++i){
        RawInputEvent track{.kind=kinds[size_t(i)],.timestamp_us=10000000+i*40000};
        if(!movement.translate(track,openu5::UiMode::Exploration,action,shortcut)||
           action.kind!=openu5::UiActionKind::Direction||
           action.direction!=directions[size_t(i)])return __LINE__;
        if(!movement.translate(letter(wasd[size_t(i)],track.timestamp_us+1),
                               openu5::UiMode::Exploration,action,shortcut)||
           action.kind!=openu5::UiActionKind::Direction||
           action.direction!=wasd_directions[size_t(i)])return __LINE__;
        track.timestamp_us+=2000;
        if(movement.translate(track,openu5::UiMode::Exploration,action,shortcut))
            return __LINE__;
    }

    // The device map picker must never fall back to "Location N" for any
    // canonical local map or dungeon.
    for(int location=1;location<=40;++location)
        if(!tdeck::location_display_name(uint8_t(location))||
           !*tdeck::location_display_name(uint8_t(location)))return __LINE__;
    if(std::string(tdeck::location_display_name(2))!="Britain"||
       std::string(tdeck::location_display_name(40))!="Doom"||
       tdeck::location_display_name(41)!=nullptr)return __LINE__;

    // Transport loss preserves the stable snapshot, then the first recovered
    // snapshot is installed as a quiet baseline without phantom releases.
    set_key(snapshot,0,4,true);apply(matrix,snapshot,events);
    if(!matrix.modifiers().alt)return __LINE__;
    matrix.desynchronize();
    if(matrix.synchronized()||!matrix.modifiers().alt)return __LINE__;
    Snapshot recovered{};
    if(apply(matrix,recovered,events)!=0||!matrix.synchronized()||
       matrix.modifiers().symbol||matrix.modifiers().alt||matrix.modifiers().shift)
        return __LINE__;

    // A transient retries once without a raw-mode write. Repeated faults reset
    // the bus, send raw mode once, and wait a complete controller scan.
    tdeck::KeyboardRecoveryPolicy recovery{};recovery.read_succeeded(0);
    recovery.read_failed(1000);
    if(recovery.failures!=1||recovery.step!=tdeck::KeyboardRecoveryStep::Read||
       recovery.due_us!=41000)return __LINE__;
    recovery.read_succeeded(recovery.due_us);
    if(recovery.failures||recovery.step!=tdeck::KeyboardRecoveryStep::Read)return __LINE__;
    recovery.read_failed(100000);recovery.read_failed(140000);
    if(recovery.step!=tdeck::KeyboardRecoveryStep::ResetBus)return __LINE__;
    recovery.bus_reset_finished(150000,true);
    if(recovery.step!=tdeck::KeyboardRecoveryStep::EnterRawMode||
       recovery.due_us!=160000)return __LINE__;
    recovery.raw_mode_finished(160000,true);
    if(recovery.step!=tdeck::KeyboardRecoveryStep::Read||recovery.due_us!=200000)
        return __LINE__;
    recovery.read_succeeded(200000);if(recovery.failures)return __LINE__;

    // A shop cursor move inside one page dirties exactly the old and new rows;
    // authoritative names/prices stay together and never become Choice/ID text.
    tdeck::DeviceShopView old_shop{},new_shop{};old_shop.active=new_shop.active=true;
    old_shop.row_count=new_shop.row_count=3;old_shop.selected_row=0;new_shop.selected_row=1;
    const char *offer_names[]={"Mystic Armour","Chain Armour","Plate Armour"};
    const int offer_prices[]={19,35,70};
    for(size_t i=0;i<3;++i){std::strcpy(old_shop.rows[i].name,offer_names[i]);std::strcpy(new_shop.rows[i].name,offer_names[i]);old_shop.rows[i].price=new_shop.rows[i].price=offer_prices[i];}
    size_t dirty_rows=0;for(size_t i=0;i<tdeck::kShopVisibleRows;++i)dirty_rows+=tdeck::shop_row_needs_redraw(new_shop,old_shop,i)?1U:0U;
    if(dirty_rows!=2)return __LINE__;
    for(size_t i=0;i<3;++i)if(std::strstr(new_shop.rows[i].name,"Choice")||
                               std::strstr(new_shop.rows[i].name," id ")||
                               new_shop.rows[i].price<=0)return __LINE__;

    // A glyph-scale change alters every row's geometry and therefore must
    // invalidate the whole Settings/gameplay layout, not one retained row.
    if(!tdeck::ui_scale_requires_full_layout(1,2)||
       tdeck::ui_scale_requires_full_layout(2,2))return __LINE__;

    // ---- Alpha 4 A4-ENH1 (ALPHA4_UI.md section 10): the trackball click ----
    {
        auto click=[](KeyTransition t,int64_t at){RawInputEvent e{};e.kind=RawInputKind::TrackballClick;
            e.transition=t;e.timestamp_us=at;return e;};
        // C1. One press toggles Movement (WASD) Mode ON at the press edge itself:
        // no hold, no release needed first, no Confirm / character / direction.
        tdeck::UiInputAdapter ball;
        int64_t t=50000000;
        // A sentinel action: the click must leave it untouched (it makes no
        // action at all -- a default UiAction would be Confirm).
        auto sentinel=[&]{action={};action.kind=openu5::UiActionKind::Next;action.index=12345;};
        auto untouched=[&]{return action.kind==openu5::UiActionKind::Next&&action.index==12345;};
        sentinel();
        if(!ball.translate(click(KeyTransition::Pressed,t),openu5::UiMode::Exploration,action,shortcut)||
           shortcut!=tdeck::DeviceShortcut::MovementModeToggled||!ball.movement_mode_enabled()||!untouched())
            return __LINE__;
        // C2. The press bouncing (release + press 1-2 ms later) and the release
        // emit nothing: one physical press is one toggle.
        if(ball.translate(click(KeyTransition::Released,t+1000),openu5::UiMode::Exploration,action,shortcut)||
           ball.translate(click(KeyTransition::Pressed,t+2000),openu5::UiMode::Exploration,action,shortcut)||
           ball.translate(click(KeyTransition::Released,t+90000),openu5::UiMode::Exploration,action,shortcut)||
           ball.translate(click(KeyTransition::Pressed,t+92000),openu5::UiMode::Exploration,action,shortcut)||
           ball.translate(click(KeyTransition::Released,t+94000),openu5::UiMode::Exploration,action,shortcut)||
           !ball.movement_mode_enabled())return __LINE__;
        // C3. The next deliberate click turns it OFF, again on the press edge.
        t+=400000;
        if(!ball.translate(click(KeyTransition::Pressed,t),openu5::UiMode::Exploration,action,shortcut)||
           shortcut!=tdeck::DeviceShortcut::MovementModeToggled||ball.movement_mode_enabled())return __LINE__;
        // C4. Held down: a repeated press with no release between never re-toggles.
        if(ball.translate(click(KeyTransition::Pressed,t+500000),openu5::UiMode::Exploration,action,shortcut)||
           ball.translate(click(KeyTransition::Pressed,t+2000000),openu5::UiMode::Exploration,action,shortcut)||
           ball.movement_mode_enabled())return __LINE__;
        if(ball.translate(click(KeyTransition::Released,t+2100000),openu5::UiMode::Exploration,action,shortcut))
            return __LINE__;
        t+=2400000;
        // C5. Every UI mode: the click toggles and is never a key for what is open.
        bool expected=false;
        for(auto mode:{openu5::UiMode::Exploration,openu5::UiMode::Dungeon,openu5::UiMode::Combat,
                       openu5::UiMode::TargetSelection,openu5::UiMode::TextEntry,openu5::UiMode::NumericEntry,
                       openu5::UiMode::Dialogue,openu5::UiMode::Shop,openu5::UiMode::DebugMenu,
                       openu5::UiMode::InventorySelection,openu5::UiMode::YesNo}){
            expected=!expected;
            sentinel();
            if(!ball.translate(click(KeyTransition::Pressed,t),mode,action,shortcut,true)||
               shortcut!=tdeck::DeviceShortcut::MovementModeToggled||ball.movement_mode_enabled()!=expected||
               !untouched())return __LINE__;
            if(ball.translate(click(KeyTransition::Released,t+80000),mode,action,shortcut,true))return __LINE__;
            t+=300000;
        }
        // C6. A roll pulse inside the 60 ms after either click edge is the ball
        // rocking under the finger: dropped, so the click cannot also move.
        tdeck::UiInputAdapter rock;
        rock.set_trackball_speed(10);
        t+=1000000;
        rock.translate(click(KeyTransition::Pressed,t),openu5::UiMode::Exploration,action,shortcut);
        RawInputEvent jiggle{.kind=RawInputKind::TrackballRight,.timestamp_us=t+20000};
        if(rock.translate(jiggle,openu5::UiMode::Exploration,action,shortcut))return __LINE__;
        rock.translate(click(KeyTransition::Released,t+100000),openu5::UiMode::Exploration,action,shortcut);
        jiggle.timestamp_us=t+150000;
        if(rock.translate(jiggle,openu5::UiMode::Exploration,action,shortcut))return __LINE__;
        jiggle.timestamp_us=t+161000;
        if(!rock.translate(jiggle,openu5::UiMode::Exploration,action,shortcut)||
           action.kind!=openu5::UiActionKind::Direction||action.direction!=Direction::East)return __LINE__;
        const auto &rs=rock.direction_metrics().stats();
        if(rs.click_guarded!=2||rock.direction_metrics().click_filter().accepted()!=1)return __LINE__;
        // C7. The Mic hold still toggles the same mode; the click toggles it back.
        tdeck::UiInputAdapter both;
        both.translate(mic(KeyTransition::Pressed,90000000),openu5::UiMode::Exploration,action,shortcut);
        if(!both.update(91100000,openu5::UiMode::Exploration,shortcut)||!both.movement_mode_enabled())return __LINE__;
        both.translate(mic(KeyTransition::Released,91200000),openu5::UiMode::Exploration,action,shortcut);
        if(!both.translate(click(KeyTransition::Pressed,91500000),openu5::UiMode::Exploration,action,shortcut)||
           both.movement_mode_enabled())return __LINE__;
    }
    // ---- A4-ENH1: the trackball instrumentation ----
    {
        openu5::InputController probe;probe.set_trackball_level(10);
        Direction d{};
        // A roll: right pulses 20 ms apart, one 2 ms bounce, then quiet.
        int64_t t=200000000;
        for(int i=0;i<5;++i){RawInputEvent e{.kind=RawInputKind::TrackballRight,.timestamp_us=t+i*20000};probe.normalize(e,d);}
        RawInputEvent bounce{.kind=RawInputKind::TrackballRight,.timestamp_us=t+4*20000+2000};
        if(probe.normalize(bounce,d))return __LINE__;
        const auto &s=probe.stats();
        if(s.edges[3]!=6||s.steps[3]!=5||s.bounced!=1||s.gaps[2]!=0||s.gaps[3]!=4||s.gaps[0]!=1)return __LINE__;
        openu5::TrackballGesture g{};
        if(probe.take_gesture(g))return __LINE__; // still rolling
        probe.poll(t+82000+openu5::InputController::kGestureGapUs-1);
        if(probe.take_gesture(g))return __LINE__;
        probe.poll(t+82000+openu5::InputController::kGestureGapUs);
        if(!probe.take_gesture(g)||g.edges[3]!=6||g.steps[3]!=5||g.duration_ms!=82||g.min_gap_ms!=2||
           probe.take_gesture(g)||s.gestures!=1||s.longest_gesture!=6)return __LINE__;
        // A second roll up, then the report: every row fits the Developer
        // screen (51 characters), and it names what the tuner needs.
        for(int i=0;i<3;++i){RawInputEvent e{.kind=RawInputKind::TrackballUp,.timestamp_us=t+1000000+i*30000};probe.normalize(e,d);}
        probe.poll(t+2000000);
        char lines[24][52]{};
        const size_t n=openu5::format_trackball_report(probe,&lines[0][0],52,24);
        std::string all;
        for(size_t i=0;i<n;++i){if(std::strlen(lines[i])>51)return __LINE__;all+=lines[i];all+="\n";}
        if(all.find("TRACKBALL STATS")==std::string::npos||all.find("Pulses  U 3  D 0  L 0  R 6")==std::string::npos||
           all.find("Steps   U 3  D 0  L 0  R 5")==std::string::npos||all.find("bounce 1")==std::string::npos||
           all.find(" U3>3 60 ms")==std::string::npos||all.find(" R6>5 82 ms, min gap 2 ms")==std::string::npos||
           all.find("U3>3")>all.find("R6>5"))return __LINE__;
        const size_t small=openu5::format_trackball_report(probe,&lines[0][0],52,3);
        if(small!=3)return __LINE__;
        probe.reset_stats();
        if(probe.stats().edges[3]||probe.stats().gestures||probe.take_gesture(g))return __LINE__;
    }

    return 0;
}
