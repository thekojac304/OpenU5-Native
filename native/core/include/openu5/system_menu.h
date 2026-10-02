#pragma once
#include "enhanced.h"
#include "frontend.h"
namespace openu5 {
// A4-ENH1: OpenDeveloper is kept (no value renumbers) but nothing produces it:
// the menu has no Developer row; Alt+D opens the tools on every screen.
// A4-ENH1 appends Cheat: `cheat` (and for Add Gold `amount`) is what the
// runtime hands to apply_cheat() (enhanced.h).
// SetDifficulty (appended too) carries the chosen difficulty in `difficulty`
// and, since A4-ENH2, the journey's Custom values in `custom` (a Custom page
// edit sends one at once; a preset sends them back unchanged).
enum class SystemMenuIntentKind:uint8_t{None,Resume,Save,LoadLatest,LoadSlot,PersistSettings,OpenDeveloper,ReturnToTitle,Cheat,SetDifficulty};
struct SystemMenuIntent{SystemMenuIntentKind kind=SystemMenuIntentKind::None;int8_t slot=-1;FrontendSettings settings{};CheatKind cheat=CheatKind::Count;int32_t amount=0;Difficulty difficulty=Difficulty::Original;GameplayRules custom=kOriginalRules;};
class SystemMenuSession{
public:
 // A4-SAVE2: `journey_slot` is the slot the live game was loaded from or
 // last saved in (-1: none); the Save and Load pages start on it.
 void open(const FrontendSettings&,const FrontendSaveCatalog&,int journey_slot=-1);bool active()const{return active_;}bool handle(const UiAction&);FrontendView view()const;SystemMenuIntent take_intent();void close(){active_=false;page_=Page::Root;}
 const FrontendSettings& settings()const{return settings_;}
 // A3-01: whether the Music Volume row is live (openu5/audio.h).
 void set_music_availability(MusicAvailability a){music_availability_=a;}
 // A3-05: the session mutes the volume rows show, and the rows a key edited
 // since the last call (kSfxVolumeEdited / kMusicVolumeEdited).
 void set_audio_mutes(bool sfx,bool music){sfx_muted_=sfx;music_muted_=music;}
 uint8_t take_volume_edits(){const uint8_t e=volume_edits_;volume_edits_=0;return e;}
 // A3-04G: the save list after a Save made from this menu (the pages
 // listed the card as it was when the menu opened). Page and cursor stay.
 void set_save_catalog(const FrontendSaveCatalog&c,int journey_slot){catalog_=c;journey_slot_=int8_t(journey_slot);}
 // Alpha 4 UI Batch 2: a one-line result in the footer (after Save Game),
 // until the next key the menu handles.
 void set_notice(const char*text);
 // A4-ENH1: the live journey's Enhanced state, which the Cheats page shows
 // (the runtime sets it when the menu opens and after each cheat).
 void set_enhanced(const EnhancedState&e){enhanced_=e;}
 // A4-POLISH3: false = no keyboard answered at boot (the row's footer says so).
 void set_keyboard_light_available(bool a){keyboard_light_available_=a;}
 // The root's rows: Difficulty and Cheats after Settings, Return to Title last.
 enum RootRow:uint8_t{kResumeRow,kSaveRow,kLoadRow,kSettingsPageRow,kDifficultyRow,kCheatsRow,kReturnToTitleRow,kRootRowCount};
 // A3-01: the Settings page's rows, in order. A4-ENH1 removed the last one,
 // "Developer: Visible/Hidden"; A4-POLISH3 appended "Keyboard Backlight".
 enum SettingsRow:uint8_t{kBrightnessRow,kMovementRow,kTrackballRow,kTextSizeRow,kSfxVolumeRow,kMusicVolumeRow,kKeyboardLightRow,kSettingsRowCount};
private:
 // A4-SAVE2: Save (Slots 1-3) and Confirm ("Overwrite Slot N?", No first).
 // A4-ENH2: Custom, the Custom difficulty's values (left/right edits them).
 // A4-ENH2: Cheats lists the cheat groups, CheatGroup one group's cheats.
 enum class Page:uint8_t{Root,Load,Settings,Save,Confirm,Cheats,Difficulty,Custom,CheatGroup};EnhancedState enhanced_{};uint8_t gold_step_=1,cheat_group_=0;bool active_=false;Page page_=Page::Root;uint8_t cursor_=0,settings_cursor_=0;FrontendSettings settings_{};MusicAvailability music_availability_=MusicAvailability::NoAudioPack;FrontendSaveCatalog catalog_{};int8_t journey_slot_=-1,confirm_slot_=-1;uint8_t slot_cursor(bool saving)const;char notice_[64]{};SystemMenuIntent pending_{};bool sfx_muted_=false,music_muted_=false,keyboard_light_available_=true;uint8_t volume_edits_=0;
};
}
