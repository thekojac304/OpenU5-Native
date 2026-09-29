#pragma once

#include "audio.h"
#include "state.h"
#include "turn.h"
#include "ui_session.h"
#include <cstddef>
#include <cstdint>

namespace openu5 {

enum class FrontendState : uint8_t {
    Title,
    IntroAnimation,
    AttractDemo,
    MainMenu,
    NewJourney,
    CharacterCreation,
    Continue,
    Load,
    Settings,
    Credits,
    EnterGame,
    Error
};

enum class FrontendIntentKind : uint8_t {
    None,
    ContinueLatest,
    LoadSlot,
    CreateInitialSave,
    PersistSettings,
    OpenDeveloperTools
};

struct FrontendSettings {
    uint8_t version = 1;
    uint8_t brightness = 80;
    bool movement_mode = false;
    // Deterministic percentage applied to the physical-detent debounce window.
    // 100% is the original responsive Alpha timing; settings expose 25..300%.
    uint16_t trackball_responsiveness = 100;
    uint8_t ui_size = 1;                  // Reserved: 0=compact, 1=normal, 2=large.
    bool developer_tools_visible = false;
    // A3-01: the Settings rows "SFX Volume" and "Music Volume", 0..100 % in
    // steps of 10 (0 = mute). Stored since Alpha 2 (settings.json keys
    // soundVolume / musicVolume), so existing cards keep their value. Music
    // Volume is kept even while no music is available (see openu5/audio.h).
    uint8_t sound_volume = 80;
    uint8_t music_volume = 80;
    bool touch_controls = false;           // Future touch frontend.
};

struct NewJourneyIdentity {
    char name[9]{};
    uint8_t gender = 0x0b;
    uint8_t strength = 20, dexterity = 15, intelligence = 15, current_mp = 15;
    // INTRO.OVL seeds the shared kernel RNG from the DOS clock before the
    // menu; FONT.OVL then consumes that same stream for the virtue bracket.
    uint16_t rng_seed_after = 0;
};

struct FrontendIntent {
    FrontendIntentKind kind = FrontendIntentKind::None;
    int8_t slot = -1;
    NewJourneyIdentity identity{};
    FrontendSettings settings{};
};

struct FrontendSaveSlot {
    bool present = false;
    bool valid = false;
    uint64_t sequence = 0; // the commit's; also for a refused generation, so the list can order it
    char name[10]{};
    // Alpha 4 UI Batch 2: what the save list shows about a valid generation,
    // read from the generation the gate staged (never from the live game).
    char place[24]{};                 // the HUD's location caption
    int32_t year = 0, month = 0, day = 0, hour = 0, minute = 0;
    uint8_t party = 0;                // members in the party
};

// Alpha 4 UI Batch 2 (ALPHA4_UI.md section 2.3). The card holds two
// generations of ONE journey: every save replaces the older one, so the
// newer is the player's "Latest" save and the other its "Backup". The list
// is ordered by age (commit sequence), never by physical slot. Latest means
// Continue: its load falls back to the backup if the latest is refused.
struct SaveList {
    int8_t latest = -1, backup = -1; // physical slot, or -1
};
SaveList order_saves(const FrontendSaveSlot (&slots)[2]);
/** Row text, at most kSaveRowChars: "Latest: Avery, Iolo's Hut", "Backup: damaged", ... */
constexpr size_t kSaveRowChars = 36;
void format_save_row(char *out, size_t cap, const FrontendSaveSlot (&slots)[2], bool latest);
/** The selected row's footer, at most 50 characters: date, time, party, or what Enter will do. */
void format_save_detail(char *out, size_t cap, const FrontendSaveSlot (&slots)[2], bool latest);
/** Whether any generation on the card is valid. */
bool any_valid_save(const FrontendSaveSlot (&slots)[2]);

enum class FrontendViewKind : uint8_t {
    Generic,
    Menu,
    Attract,
    CharacterName,
    CharacterGender,
    CharacterQuiz,
    Credits,
    Settings,
    SystemMenu,
    // Alpha 4 UI Batch 2: the Title (and the startup intro that shows it): the
    // credit lines and the "Press a key" prompt, centred under the title art.
    TitleCredits,
};

enum class FrontendCreationPhase : uint8_t { Name, Sex, Quiz };

struct FrontendView {
    FrontendState state = FrontendState::Title;
    FrontendViewKind kind = FrontendViewKind::Generic;
    const char *title = "";
    const char *subtitle = "";
    const char *lines[12]{};
    size_t line_count = 0;
    int selected_line = -1;
    const char *footer = "";
};

// Exact FONT.OVL creation tournament: 8 virtues, elimination rounds 4+2+1.
class GypsyTournament {
  public:
    void begin(uint8_t strength, uint8_t dexterity, uint8_t intelligence,
               uint16_t seed = 0);
    bool answer(bool choose_b);
    bool done() const { return round_ >= 3; }
    uint8_t question_index() const;
    uint8_t answered() const { return answered_; }
    uint16_t seed_after() const { return rng_; }
    uint8_t virtue_a() const { return pending_a_; }
    uint8_t virtue_b() const { return pending_b_; }
    NewJourneyIdentity finish(const char *name, uint8_t gender) const;

  private:
    uint16_t rng_ = 0;
    bool used_[8]{}, eliminated_[8]{};
    uint8_t strength_ = 15, dexterity_ = 15, intelligence_ = 15;
    uint8_t round_ = 0, match_ = 0, pending_a_ = 0, pending_b_ = 1;
    uint8_t answered_ = 0;
    bool pending_ = false;
    uint8_t draw();
    uint8_t pick();
    void next();
};

class FrontendSession {
  public:
    void start(uint32_t now_ms, bool developer_build, const FrontendSettings &settings = {});
    bool tick(uint32_t now_ms);
    bool handle(const UiAction &, uint32_t now_ms);
    FrontendState state() const { return state_; }
    FrontendView view() const;
    FrontendIntent take_intent();
    void complete_intent(bool success, const char *message = nullptr);
    void enter_game() { state_ = FrontendState::EnterGame; }
    void set_save_slots(const FrontendSaveSlot (&slots)[2]);
    void set_question_texts(const char *const *questions, size_t count) {
        questions_ = questions; question_count_ = count;
    }
    void set_intro_texts(const char *const *scenes, size_t count) {
        intro_texts_ = scenes; intro_text_count_ = count;
    }
    const FrontendSettings &settings() const { return settings_; }
    /** A3-01: whether the Music Volume row is live (openu5/audio.h). */
    void set_music_availability(MusicAvailability a) { music_availability_ = a; }
    /** A3-05: as SystemMenuSession's (the title Settings shares the rows). */
    void set_audio_mutes(bool sfx, bool music) { sfx_muted_ = sfx; music_muted_ = music; }
    uint8_t take_volume_edits() { const uint8_t e = volume_edits_; volume_edits_ = 0; return e; }
    bool active() const { return state_ != FrontendState::EnterGame; }
    FrontendCreationPhase creation_phase() const { return creation_; }
    const char *creation_name() const { return name_; }
    uint8_t creation_name_length() const { return name_length_; }
    uint8_t creation_gender() const { return gender_; }
    uint8_t creation_answered() const { return tournament_.answered(); }
    uint8_t creation_virtue_a() const { return tournament_.virtue_a(); }
    uint8_t creation_virtue_b() const { return tournament_.virtue_b(); }
    bool startup_intro() const {
        return state_ == FrontendState::IntroAnimation && intro_page_ == 0;
    }

    // INTRO waits 200 polling ticks. The checked-in witness is approximately
    // twelve seconds; this is presentation timing, not a gameplay clock.
    static constexpr uint32_t kMenuIdleMs = 12000;

  private:
    FrontendState state_ = FrontendState::Title;
    FrontendCreationPhase creation_ = FrontendCreationPhase::Name;
    FrontendIntent pending_{};
    FrontendSettings settings_{};
    MusicAvailability music_availability_ = MusicAvailability::NoAudioPack;
    bool sfx_muted_ = false, music_muted_ = false;
    uint8_t volume_edits_ = 0;
    FrontendSaveSlot saves_[2]{};
    GypsyTournament tournament_{};
    uint32_t entered_ms_ = 0;
    uint8_t cursor_ = 0, intro_page_ = 0, settings_cursor_ = 0;
    char name_[9]{}, notice_[64]{};
    uint8_t name_length_ = 0, gender_ = 0x0b;
    bool developer_build_ = false;
    bool creation_seeded_ = false;
    uint16_t creation_seed_ = 0;
    const char *const *questions_ = nullptr;
    size_t question_count_ = 0;
    const char *const *intro_texts_ = nullptr;
    size_t intro_text_count_ = 0;

    void enter(FrontendState, uint32_t now_ms);
    size_t menu_count() const;
    void activate_menu(uint32_t now_ms);
    void begin_creation(uint32_t now_ms);
};

void apply_new_journey_identity(GameState &, const NewJourneyIdentity &);
const char *virtue_name(uint8_t);

} // namespace openu5
