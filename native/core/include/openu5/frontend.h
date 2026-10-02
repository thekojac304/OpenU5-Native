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
    Error,
    // Alpha 4 A4-SAVE2: Create New Character with no empty save slot -- the
    // player picks the slot the new journey replaces (and confirms it).
    NewJourneySlot,
    // Alpha 4 A4-SAVE3 (targets/tdeck/ALPHA4_UI.md section 5): original PC
    // saves. The page (Import / Export), then the slot each one uses.
    PcTransfer,
    PcImportSlot,
    PcExportSlot
};

enum class FrontendIntentKind : uint8_t {
    None,
    ContinueLatest,
    LoadSlot,
    CreateInitialSave,
    PersistSettings,
    OpenDeveloperTools,
    // Alpha 4 A4-SAVE3: look at the import folder (set_pc_import_status),
    // import it into `slot`, export `slot`.
    InspectPcSaves,
    ImportPcSave,
    ExportPcSave
};

// Alpha 4 A4-ENH1 (targets/tdeck/ALPHA4_UI.md section 10): the trackball's
// speed, a level from 1 (slowest) to 10 (fastest). The device maps each level
// to pulses per step and the shortest gap between steps (input_controller.cpp
// kTrackballLevels). 10 is the pre-A4-ENH1 behaviour at 100 % (one step per
// pulse); 7 is close to the old 25 % minimum. PROVISIONAL: the default and the
// table are tuned on hardware (section 10).
constexpr uint8_t kTrackballSpeedMin = 1, kTrackballSpeedMax = 10;
constexpr uint8_t kTrackballSpeedDefault = 5, kTrackballSpeedLegacy = 10;
constexpr const char *kTrackballSpeedFooter = "Left/right: 1 slow - 10 fast; Mic saves";

struct FrontendSettings {
    uint8_t version = 1;
    uint8_t brightness = 80;
    bool movement_mode = false;
    // Before A4-ENH1: the percentage applied to the physical-detent debounce
    // window (25..300 %). Kept and written unchanged so an older firmware still
    // reads this settings.json; nothing reads it for input any more.
    uint16_t trackball_responsiveness = 100;
    // A4-ENH1: the speed level that replaces it ("trackballSpeed"; a file
    // without the key, written before A4-ENH1, takes the default).
    uint8_t trackball_speed = kTrackballSpeedDefault;
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

// Alpha 4 UI Batch 2 (ALPHA4_UI.md section 2.3). One journey is kept as two
// generations: every save replaces the older one, so the newer is its latest
// save and the other the recovery copy. Ordered by age (commit sequence),
// never by physical slot.
struct SaveList {
    int8_t latest = -1, backup = -1; // physical generation, or -1
};
SaveList order_saves(const FrontendSaveSlot (&generations)[2]);

// Alpha 4 A4-SAVE2 (ALPHA4_UI.md section 4): three manual save slots. Each
// slot is one journey kept as its own two-generation pair (the A4-SAVE1
// recovery model); the player sees the slot, never its generations. A slot's
// load restores its newest generation the gate accepts, falling back to the
// one before it when the newest is refused.
constexpr int kSaveSlotCount = 3;
enum class SaveSlotStatus : uint8_t {
    Empty,      // neither generation has a commit record
    Saved,      // the newest generation is accepted: a load restores it
    Recovered,  // the newest is refused and the one before it accepted: a load restores that one
    Damaged,    // nothing in the slot can be loaded
};
struct FrontendSaveCatalog {
    struct Entry {
        SaveSlotStatus status = SaveSlotStatus::Empty;
        uint64_t sequence = 0;     // the slot's newest commit record: Continue orders the slots by it
        FrontendSaveSlot shown{};  // Saved / Recovered: the generation a load restores
    };
    Entry slots[kSaveSlotCount]{};
};
bool slot_loadable(const FrontendSaveCatalog &, int slot);
/** The slot Continue restores: the loadable slot with the newest save, or -1. */
int continue_slot(const FrontendSaveCatalog &);
/** The lowest empty slot, or -1. */
int first_empty_slot(const FrontendSaveCatalog &);
// Alpha 4 A4-UI3 (ALPHA4_UI.md section 6): a slot is shown as two rows of at
// most kSaveRowChars (36 cells: what Large text fits in the menu's 304 px).
//   row:    "Slot 2  Kojac     CURRENT"  (status word or name, then the tags)
//   detail: "        Lord British's Castle" (the place a load restores)
// EMPTY and DAMAGED carry no metadata. The tag column holds `tag` on slot
// `marked` (CURRENT in game: the live journey's slot; LATEST on the title:
// Continue's slot) and RECOVERED when a load restores the one before the newest.
constexpr size_t kSaveRowChars = 36;
constexpr const char *kCurrentSlotTag = "CURRENT", *kLatestSlotTag = "LATEST";
void format_slot_rows(char *row, char *detail, size_t cap, const FrontendSaveCatalog &, int slot,
                      int marked = -1, const char *tag = nullptr);
/** What a load of the slot restores, for the confirm pages: "Kojac, Britannia", "Damaged save", "Empty slot". */
void format_slot_identity(char *out, size_t cap, const FrontendSaveCatalog &, int slot);
/** The selected row's footer, at most 50 characters: date, time, party, or what Enter will do. */
void format_slot_detail(char *out, size_t cap, const FrontendSaveCatalog &, int slot, bool saving);

// Alpha 4 A4-SAVE3: what the import folder holds, as the PC Save Transfer page
// shows it. The runtime reads the folder when the page opens.
enum class PcImportState : uint8_t { Unknown, Missing, Problem, Ready };
struct PcImportStatus {
    PcImportState state = PcImportState::Unknown;
    char text[48]{};          // Ready: "Kojac, Lord British's Castle"; else why not
    int8_t imported_slot = -1; // these same files were imported before, into this slot
};

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
    // Alpha 4 A4-UI3: a slot page sets one detail row per slot line; the
    // device draws it under that line and selects the two rows together.
    const char *details[kSaveSlotCount]{};
};
/** A4-UI3: the three slot rows of a slot page into `v`, from `rows` (6 rows of 96). */
void list_slots(FrontendView &v, char (*rows)[96], const FrontendSaveCatalog &, int selected,
                int marked, const char *tag);

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
    void set_save_catalog(const FrontendSaveCatalog &catalog) { catalog_ = catalog; }
    void set_pc_import_status(const PcImportStatus &status) { pc_status_ = status; }
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
    FrontendSaveCatalog catalog_{};
    int8_t new_journey_slot_ = -1;   // A4-SAVE2: where CreateInitialSave writes
    bool slot_confirm_ = false;      // NewJourneySlot: the Replace? question is up
    PcImportStatus pc_status_{};     // A4-SAVE3
    int8_t pc_slot_ = -1;            // PcImportSlot: the slot the question is about
    bool pc_confirm_ = false;        // PcImportSlot: the Replace? / Import again? question is up
    bool pc_error_ = false;          // the Error page came from PC Save Transfer (returns there)
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
