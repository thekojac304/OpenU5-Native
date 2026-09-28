#pragma once
#include "frontend.h"
namespace openu5 {
enum class SystemMenuIntentKind:uint8_t{None,Resume,Save,LoadLatest,LoadSlot,PersistSettings,OpenDeveloper,ReturnToTitle};
struct SystemMenuIntent{SystemMenuIntentKind kind=SystemMenuIntentKind::None;int8_t slot=-1;FrontendSettings settings{};};
class SystemMenuSession{
public:
 void open(const FrontendSettings&,const FrontendSaveSlot(&)[2]);bool active()const{return active_;}bool handle(const UiAction&);FrontendView view()const;SystemMenuIntent take_intent();void close(){active_=false;page_=Page::Root;}
 const FrontendSettings& settings()const{return settings_;}
 // A3-01: whether the Music Volume row is live (openu5/audio.h).
 void set_music_availability(MusicAvailability a){music_availability_=a;}
 // A3-05: the session mutes the volume rows show, and the rows a key edited
 // since the last call (kSfxVolumeEdited / kMusicVolumeEdited).
 void set_audio_mutes(bool sfx,bool music){sfx_muted_=sfx;music_muted_=music;}
 uint8_t take_volume_edits(){const uint8_t e=volume_edits_;volume_edits_=0;return e;}
 // A3-04G: the save list after a Save made from this menu (the Load page
 // listed the card as it was when the menu opened). Page and cursor stay.
 void set_save_slots(const FrontendSaveSlot(&s)[2]){saves_[0]=s[0];saves_[1]=s[1];}
 // A3-01: the Settings page's rows, in order. Developer stays last.
 enum SettingsRow:uint8_t{kBrightnessRow,kMovementRow,kTrackballRow,kTextSizeRow,kSfxVolumeRow,kMusicVolumeRow,kDeveloperRow,kSettingsRowCount};
private:
 enum class Page:uint8_t{Root,Load,Settings};bool active_=false;Page page_=Page::Root;uint8_t cursor_=0,settings_cursor_=0;FrontendSettings settings_{};MusicAvailability music_availability_=MusicAvailability::NoAudioPack;FrontendSaveSlot saves_[2]{};SystemMenuIntent pending_{};bool sfx_muted_=false,music_muted_=false;uint8_t volume_edits_=0;
};
}
