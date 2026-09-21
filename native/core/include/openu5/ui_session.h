#pragma once

#include "commands.h"
#include "shop_orchestration.h"
#include <cstddef>
#include <cstdint>

namespace openu5 {
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
class UiDebugMenu;
#endif
struct DungeonState;

// Presentation state only.  A frontend may render these however it likes.
enum class UiMode : uint8_t {
    Exploration,
    Dungeon,
    Combat,
    Dialogue,
    Shop,
    ShrineSpecial,
    TextEntry,
    NumericEntry,
    YesNo,
    PartySelection,
    InventorySelection,
    EquipmentSelection,
    SpellSelection,
    TargetSelection,
    DebugMenu
};

enum class UiActionKind : uint8_t {
    Direction,
    Character,
    Confirm,
    Cancel,
    Back,
    Next,
    Previous,
    PageUp,
    PageDown,
    SelectIndex,
    TextInput,
    DeleteCharacter,
    SystemMenu
};

// The text pointer is borrowed for handle_input() only.  Character and TextInput
// are UTF-16 to match the native dialogue/shop contracts, not a platform key API.
struct UiAction {
    UiActionKind kind = UiActionKind::Confirm;
    Direction direction = Direction::North;
    char16_t character = 0;
    int32_t index = -1;
    const char16_t *text = nullptr;
    size_t text_length = 0;
};

enum class UiTextChannel : uint8_t {
    Message,
    CommandEcho,
    Prompt,
    Combat,
    Dialogue,
    Shop,
    Quest,
    System,
    Debug
};

enum UiTextFlags : uint8_t {
    UiTextNone = 0,
    UiTextContinuesBefore = 1,
    UiTextContinuesAfter = 2,
    UiTextRune = 4
};

constexpr size_t kUiTranscriptBlockBytes = 160;
constexpr size_t kUiPromptBytes = 96;
constexpr size_t kUiInputUnits = 64;
constexpr size_t kUiRenderedLineBytes = 96;

// Storage is supplied by the owner (internal RAM or PSRAM).  Long events are
// split into adjacent blocks and old blocks are overwritten as a ring.
struct UiTextBlock {
    uint32_t sequence = 0;
    UiTextChannel channel = UiTextChannel::Message;
    uint8_t flags = UiTextNone;
    uint16_t length = 0;
    char text[kUiTranscriptBlockBytes]{};
};

struct UiTranscriptStorage {
    UiTextBlock *blocks = nullptr;
    size_t capacity = 0;
};

struct UiRenderedLine {
    uint32_t sequence = 0;
    UiTextChannel channel = UiTextChannel::Message;
    uint8_t flags = UiTextNone;
    uint16_t length = 0;
    char text[kUiRenderedLineBytes]{};
};

enum class UiRequestId : uint8_t {
    None,
    Direction,
    TownExit,
    Dialogue,
    Shop,
    ShrineVisit,
    ShrineRestore,
    ShrineDonate,
    Blackthorn,
    GuardPassword,
    GuardTribute,
    GuardArrest,
    TrollToll,
    CrystalBall,
    WellDrop,
    FountainDrink,
    WellWish,
    RestHours,
    YellText,
    Status,
    Party,
    Inventory,
    Equipment,
    EquipmentMember,
    UseTarget,
    Spell,
    Target,
    Debug,
    // Batch 9B: the reference's "Will you drink?" beat (DS 0x7700) before a
    // dungeon fountain gulp.  Answering yes dispatches DungeonAction::Drink.
    DungeonDrink,
    // Y-33: Vas Rel Por's bare "To phase:" getkey.  One keypress resolves
    // immediately -- '1'-'8' dispatches the gate with that phase; any other
    // key (including Cancel) dispatches it with no phase, matching the
    // reference's non-retrying, silently-failing getkey (CAST.OVL 0x0d06).
    GatePhase,
    Custom
};

enum class UiIntentKind : uint8_t {
    Command,
    Shop,
    ModalResponse,
    OpenPartySelection,
    OpenInventorySelection,
    OpenEquipmentSelection,
    OpenSpellSelection,
    OpenTargetSelection,
    OpenStatusSelection,
    OpenDebugMenu
};

struct UiModalValue {
    bool accepted = false;
    bool yes = false;
    int32_t number = 0;
    int32_t index = -1;
    const char16_t *text = nullptr; // Borrowed for dispatch() only.
    size_t text_length = 0;
};

struct UiIntent {
    UiIntentKind kind = UiIntentKind::Command;
    UiRequestId request = UiRequestId::None;
    Command command{};
    ShopInput shop{};
    UiModalValue value{};
};

struct UiControllerServices {
    void *context = nullptr;
    // Must consume borrowed text synchronously.  Core adapters normally call
    // execute_command()/execute_shop() and feed resulting events back immediately.
    void (*dispatch)(void *, const UiIntent &) = nullptr;
};

struct UiSelectionItem {
    const char *label = nullptr; // Borrowed until the selection closes.
    bool enabled = true;
};

struct UiSelectionSource {
    void *context = nullptr;
    size_t (*count)(void *) = nullptr;
    UiSelectionItem (*item)(void *, size_t) = nullptr;
};

struct UiSessionConfig {
    uint16_t wrap_columns = 38; // Character-cell hint only; never pixels.
    uint16_t page_rows = 8;
    uint16_t max_input_units = uint16_t(kUiInputUnits - 1);
};

struct UiSelectionView {
    UiMode mode = UiMode::InventorySelection;
    size_t cursor = 0;
    size_t count = 0;
    UiSelectionItem current{};
};

class UiSession {
  public:
    UiSession(UiTranscriptStorage, UiControllerServices = {}, UiSessionConfig = {});

    UiMode mode() const { return mode_; }
    UiMode base_mode() const { return base_mode_; }
    UiRequestId request() const { return request_; }
    const char *prompt() const { return prompt_; }
    const char16_t *input_buffer() const { return input_; }
    size_t input_length() const { return input_length_; }
    uint32_t event_sequence() const { return event_sequence_; }
    size_t transcript_size() const { return transcript_count_; }
    size_t transcript_capacity() const { return transcript_.capacity; }
    size_t scroll_offset_lines() const { return scroll_lines_; }
    uint32_t blocked_events_generated() const { return blocked_events_generated_; }
    uint32_t blocked_events_presented() const { return blocked_events_presented_; }
    ShopPhase shop_phase() const { return shop_phase_; }
    ShopType shop_type() const { return shop_type_; }
    int32_t shop_cursor() const { return shop_cursor_; }
    void set_shop_offer_count(size_t count) { shop_offer_count_=count; if(!count)shop_cursor_=0;else if(size_t(shop_cursor_)>=count)shop_cursor_=int32_t(count-1); }

    // --- Narrow world-context mirrors (GAMEPLAY_INTEGRATION_AUDIT.md R-19/R-20).
    // UiSession does not own GameState/TurnState, so the two facts exploration
    // key routing needs are pushed in from the owner, exactly like
    // set_shop_offer_count(). AlphaRuntime::refresh_session_context() writes both
    // from authoritative runtime state immediately before every key is routed;
    // host tests write them directly. Nothing here derives or caches world state
    // on its own -- these are mirrors, not a second source of truth.
    //
    // Sails (R-19): frigate_aboard mirrors (TurnState::transport_tile & 0xf8) ==
    // 0x20 and location_allows_sails mirrors position.map.location < 0x80 -- the
    // same two predicates the reference's yell() dispatcher tests before routing
    // to yellSails() (game/src/core/game.ts) and the same pair
    // CommandKind::YellSails re-checks authoritatively in commands.cpp.
    void set_sail_context(bool frigate_aboard, bool location_allows_sails) {
        sail_context_frigate_ = frigate_aboard;
        sail_context_location_ok_ = location_allows_sails;
    }
    bool sail_context_frigate() const { return sail_context_frigate_; }
    bool sail_context_location_ok() const { return sail_context_location_ok_; }
    // Harpsichord (R-20): mirrors Game.harpsichordSeated() -- small map (location
    // != 0), not in combat and not in a dungeon, with the harpsichord tile 0x8D
    // immediately south of the party. While it is true, digit keys play a note
    // instead of selecting the active player, matching the reference's key
    // routing order (main.ts: the harpsichord intercept runs before the
    // set-active-player arm).
    void set_harpsichord_active(bool at_harpsichord) { harpsichord_active_ = at_harpsichord; }
    bool harpsichord_active() const { return harpsichord_active_; }
    // Dungeon prompt context (Batch 9B, R-05 part 3): the same narrow-mirror
    // shape as the two above.  Two reference dungeon commands do not decide for
    // the player when the cell offers a choice:
    //   * (K)limb on a cell with a ladder BOTH ways opens "Klimb-U/D-" (0x6cba)
    //     -- dungeon_klimb_choice() is the authoritative predicate;
    //   * (D)rink while standing ON a fountain asks "Will you drink?" (DS
    //     0x7700) before the gulp.
    // UiSession owns no dungeon state, so the owner pushes both immediately
    // before each key is routed.  dungeon_action() re-checks each one
    // authoritatively ("No fountain here.", and Klimb's own up/down caps), so a
    // stale mirror can only mis-route a prompt, never mis-apply an action --
    // exactly the R-19 sail-context contract.
    void set_dungeon_prompt_context(bool klimb_needs_choice, bool fountain_here) {
        dungeon_klimb_choice_ = klimb_needs_choice;
        dungeon_fountain_here_ = fountain_here;
    }
    // Named *_prompt so they cannot be confused with the authoritative
    // openu5::dungeon_klimb_choice() predicate these mirror.
    bool dungeon_klimb_prompt() const { return dungeon_klimb_choice_; }
    bool dungeon_fountain_prompt() const { return dungeon_fountain_here_; }
    // Convenience wrapper that derives both predicates from the authoritative
    // owners, so a caller cannot get the derivation subtly wrong.  A cleared
    // context (no live session) resets both.
    void refresh_dungeon_context(const GameState &, const DungeonState &, bool dungeon_active);
    // Transcript page geometry (Batch 4.5C): another narrow mirror of the same
    // shape as set_sail_context()/set_harpsichord_active() above. The owner's
    // renderer is the only thing that knows the actual on-screen column width
    // and row count for whichever transcript-bearing panel is currently
    // showing (they vary with the text-size setting and with whether a
    // context bar is reserving space), so it pushes both in immediately
    // before each key is routed. Shift+Up/Shift+Down (PageUp/PageDown) and
    // wrapped_line_count()'s no-argument overload use these instead of the
    // fixed constructor defaults so a single page of paging matches a single
    // page of what is actually rendered. Zero means "unset"; falls back to
    // the constructor config exactly as before (host tests that never call
    // this keep exercising the fixed defaults).
    void set_transcript_view_metrics(size_t columns, size_t rows) {
        transcript_columns_ = columns;
        transcript_rows_ = rows;
    }

    void set_base_mode(UiMode);
    void consume(const GameEvent &);
    EventSink event_sink();
    bool handle_input(const UiAction &);
    // True only when the current mode consumes a semantic Direction.
    bool accepts_direction_input() const;

    void begin_text(UiRequestId, const char *prompt, size_t max_units = kUiInputUnits - 1,
                    bool escape_clears = false);
    void begin_number(UiRequestId, const char *prompt, int32_t minimum, int32_t maximum,
                      size_t max_digits = 9);
    void begin_yes_no(UiRequestId, const char *prompt, bool cancel_means_no = false);
    void begin_selection(UiMode, UiRequestId, const char *prompt, UiSelectionSource,
                         size_t initial = 0);
    void begin_target(UiRequestId, const char *prompt, Command command_template,
                      int16_t x = 0, int16_t y = 0);
    void set_combat_aim(int16_t x, int16_t y, int16_t range,
                        int16_t initial_x, int16_t initial_y) {
        combat_origin_x_=x; combat_origin_y_=y; combat_aim_range_=range;
        combat_initial_x_=initial_x; combat_initial_y_=initial_y;
    }
    void set_combat_origin(int16_t x, int16_t y) { set_combat_aim(x,y,1,x,y); }
    int16_t target_x() const { return pending_command_.combat_x; }
    int16_t target_y() const { return pending_command_.combat_y; }
    CommandKind target_command_kind() const { return pending_command_.kind; }
    bool target_has_direction() const { return pending_command_.has_direction; }
    // An unresolved directional Open has no cell. Never expose Command's
    // zero-initialized storage as a target reticle at (0,0).
    bool target_has_cell() const {
        return pending_command_.kind != CommandKind::CombatOpen ||
               pending_command_.has_direction;
    }
    // A directional command resolves synchronously, so retain its one
    // controller-owned cell for exactly the next rendered frame.
    bool take_target_render_marker(int16_t &x, int16_t &y) {
        if (!target_render_marker_) return false;
        x = target_render_x_; y = target_render_y_;
        target_render_marker_ = false;
        return true;
    }
    Direction target_direction() const { return pending_command_.direction; }
    void cancel_modal();
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    void attach_debug_menu(UiDebugMenu *menu) { debug_menu_ = menu; }
    bool open_debug_menu();
#endif

    bool selection_view(UiSelectionView &) const;
    const UiTextBlock *transcript_at(size_t chronological_index) const;
    size_t wrapped_line_count(size_t columns = 0) const;
    size_t visible_lines(UiRenderedLine *, size_t capacity, size_t columns = 0) const;

    void append(UiTextChannel, const char *, uint8_t flags = UiTextNone);
    void append_utf16(UiTextChannel, const char16_t *, size_t, uint8_t flags = UiTextNone);
    /**
     * Y-04: `messageAppend` -- continue the line already on screen instead of
     * starting a new one. The original's `$ sneaks across` prints its three
     * dots one at a time onto the SAME line (MAINOUT 0x1c56-0x1c65), so a
     * presenter that pushed each dot as its own block would read nothing like
     * the reference. Extends the last block in place when it is still the tail
     * of the same channel and has room; otherwise it degrades to a plain
     * append, which is the worst case the original never reaches.
     */
    void append_continuation(UiTextChannel, const char *);

  private:
    UiTranscriptStorage transcript_{};
    UiControllerServices services_{};
    UiSessionConfig config_{};
    UiMode base_mode_ = UiMode::Exploration;
    UiMode mode_ = UiMode::Exploration;
    UiMode return_mode_ = UiMode::Exploration;
    // Combat is authoritative core state; this register is written only by
    // CombatStarted and read only by CombatEnded (R-18).
    UiMode pre_combat_mode_ = UiMode::Exploration;
    // Shop/Dialogue/ShrineSpecial are session-owned UI states with their own
    // exit lifecycle. Each owns its return register so combat/other sessions
    // can never mix up which world mode a session should hand control back
    // to (R-18).
    UiMode shop_return_mode_ = UiMode::Exploration;
    UiMode dialogue_return_mode_ = UiMode::Exploration;
    UiMode shrine_return_mode_ = UiMode::Exploration;
    UiRequestId request_ = UiRequestId::None;
    UiSelectionSource selection_{};
    size_t selection_cursor_ = 0;
    size_t transcript_head_ = 0, transcript_count_ = 0;
    size_t scroll_lines_ = 0;
    uint32_t event_sequence_ = 0, block_sequence_ = 0;
    uint32_t blocked_events_generated_ = 0, blocked_events_presented_ = 0;
    uint32_t blocked_block_sequence_ = 0;
    int32_t blocked_actor_ = -1;
    uint16_t blocked_repeat_ = 0;
    char prompt_[kUiPromptBytes]{};
    char16_t input_[kUiInputUnits]{};
    size_t input_length_ = 0, input_limit_ = kUiInputUnits - 1;
    int32_t number_min_ = 0, number_max_ = 0;
    bool cancel_means_no_ = false, escape_clears_ = false;
    Command pending_command_{};
    ShopPhase shop_phase_ = ShopPhase::Closed;
    ShopType shop_type_{};
    int32_t shop_cursor_ = 0;
    size_t shop_offer_count_ = 0;
    int16_t combat_origin_x_ = 0, combat_origin_y_ = 0;
    int16_t combat_initial_x_ = 0, combat_initial_y_ = 0, combat_aim_range_ = 1;
    int16_t target_render_x_ = -1, target_render_y_ = -1;
    bool target_render_marker_ = false;
    // World-context mirrors (see the public setters above).
    bool sail_context_frigate_ = false, sail_context_location_ok_ = false;
    bool harpsichord_active_ = false;
    bool dungeon_klimb_choice_ = false, dungeon_fountain_here_ = false;
    size_t transcript_columns_ = 0, transcript_rows_ = 0;
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    UiDebugMenu *debug_menu_ = nullptr;
    UiMode debug_return_mode_ = UiMode::Exploration;
#endif

    void dispatch(const UiIntent &) const;
    void enter_modal(UiMode, UiRequestId, const char *);
    // Reduces a possibly session-owned mode (Shop/Dialogue/ShrineSpecial) to
    // the world mode underneath it, using that session's own captured return
    // register. A non-session-owned mode is returned unchanged.
    UiMode world_return_mode(UiMode) const;
    // Shared entry point for the four shrine/Blackthorn prompt events.
    // Captures shrine_return_mode_ before base_mode_ is overwritten.
    void enter_shrine_mode();
    // ShrineSpecial has no ordinary input route and no explicit "ended"
    // event. Called after a modal response resolves; if nothing re-armed a
    // new shrine prompt, the ceremony is over.
    void settle_shrine_after_modal();
    void finish_modal(bool accepted, bool yes = false, int32_t number = 0,
                      int32_t index = -1);
    bool handle_modal(const UiAction &);
    bool handle_exploration(const UiAction &);
    bool handle_dungeon(const UiAction &);
    bool handle_combat(const UiAction &);
    bool handle_shop(const UiAction &);
    void command(Command);
    void command_echo(const char *);
    void direction_request(CommandKind, const char *);
    void push_block(UiTextChannel, const char *, size_t, uint8_t);
    // A scrolled-away view (scroll_lines_ > 0) must hold the same lines on
    // screen when the transcript grows -- see the appenders below. Following
    // the newest text (scroll_lines_ == 0) is left alone; that is auto-follow.
    void preserve_scroll_on_growth(size_t lines_before);
    void append_combat_event(const CombatEvent &);
};

} // namespace openu5
