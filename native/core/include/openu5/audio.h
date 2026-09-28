#pragma once

#include <cstddef>
#include <cstdint>

// Alpha 3 A3-01 -- the audio vocabulary, the audio service and the backend
// seam. Architecture and derivations: native/targets/tdeck/ALPHA3_AUDIO.md.
//
// Four layers, and only the first one is gameplay:
//   1. game logic emits SEMANTIC cues (GameEventKind::Sfx with a cue id);
//   2. AudioService (this file) decides what reaches the output, per channel,
//      with the user's volumes and the installed music capability;
//   3. an AudioBackend renders it (device: I2S; host: recording or silent);
//   4. the audio pack (audio_pack.h) says whether music exists at all.
//
// Nothing in this file reads or writes GameState, the clock or any RNG, and
// no call here waits on audio hardware: a backend that cannot take a request
// says so and the request is dropped, never retried. Scene timing is owned by
// scene_timing.h / the scene pacers (Batch 51) and never by audio completion.
namespace openu5 {

// ---------------------------------------------------------------------------
// Sound effects -- one id per semantic cue.
//
// The first block is the reference catalogue, game/src/core/sfx.ts `SfxId`,
// in ITS order (a drift test compares the two). Each id is one family of the
// original's PC-speaker call sites (re/notes/sfx-catalog.md section 6). Ids
// are APPENDED, never renumbered: a later batch may persist or table them.
// ---------------------------------------------------------------------------
enum class SfxId : uint8_t {
    None = 0,
    CombatHit, CombatHitHeavy, CombatDamage, CombatDefeat, CastSpell, SpellZap, LineSpray,
    TimeSpell, ShrineWellDone, ShrineDonation, ShrineOrdained, Sceptre, BlackthornMaterialize,
    ShardSweep, VictoryFanfare, Moongate, Quake, WaterfallFall, ShadowlordAnnounce,
    InstrumentNote, ShopTransaction, CannonFire, CombatEscape, CombatReject, CombatAbsorbed,
    RingVanishes, AmbientFountain, AmbientWaterfall, AmbientClockTick, AmbientClockTock,
    AmbientClockChime, MoveStep, MoveBlocked, SearchFail, DungeonTrap, DungeonZap,
    FieldAfflict, DungeonFail, TorchBorrowed, MirrorBreak, ApparitionMaterialize,
    ApparitionArpeggio, ApparitionHealChime, ApparitionChord, RefugeThunder, IntroThunder,
    IntroChime, IntroSummon, TitleFizzle, TitleCrackle, BardSong, EndgameOrb,
    // Native cue names with no entry in the reference catalogue. The first
    // three always precede a MagicCeremony event, whose index selects the
    // original CAST2.OVL:0x0000 ceremony sound (noise lead + two mirrored
    // sweeps); invalid-magic has no adjudicated original sound.
    SpellCast, PotionUsed, ScrollUsed, InvalidMagic,
    // Device diagnostics only (Developer > Diagnostics); never emitted by play.
    DiagnosticTone,
    // A3-03: original PC-speaker sites the reference catalogue does not name,
    // each re-read from the binary (re/tools/a3_03_cue_sites.py). Most are
    // derived at presentation time from a message the core already prints.
    SceptreReclaimed,   // kernel 0x6221 "The Sceptre is reclaimed!"
    CombatCharm,        // SJOG 0x2218 " passes out!" / COMSUBS 0x01b1 " possessed!"
    CombatSummon,       // COMSUBS 0x02cb " gates in a daemon!"
    CombatGrazed,       // COMSUBS 0x0352 " grazed!"
    CombatDraggedUnder, // COMSUBS 0x03d6 " dragged under!"
    CombatEngulfed,     // COMBAT 0x07f1 "ARGH!"
    CombatRegurgitated, // COMBAT 0x1cbf " regurgitated!"
    CombatFoodStolen,   // COMBAT 0x03b6 " stole some food!"
    ShipCollision,      // MAINOUT 0x0300 "COLLISION!"
    ShipSinking,        // MAINOUT 0x113b (no skiff: DROWNING) / 0x12a6 WHIRLPOOL
    TheftDetected,      // TALK 0x11a8 "Something was stolen!"
    WishGranted,        // LOOKOBJ 0x0129 the wishing well's "Poof!"
    TrapdoorFall,       // TOWN 0x0fa0-0x101e the location-29 trapdoor
    ShardNoEffect,      // CAST 0x166d the shard held at the wrong flame
    RefugeSlumber,      // BLCKTHRN 0x0a22-0x0a49 "But thy slumber is disturbed!"
    RefugeRevival,      // BLCKTHRN 0x0b5d-0x0b8d "Strange words are intoned."
    Count
};
constexpr size_t kSfxIdCount = size_t(SfxId::Count);
/** The reference catalogue's size: CombatHit .. EndgameOrb. */
constexpr size_t kReferenceSfxCount = size_t(SfxId::EndgameOrb);
/** The first id A3-03 appended (every id from here on is Original). */
constexpr SfxId kFirstA303SfxId = SfxId::SceptreReclaimed;

enum class SfxOrigin : uint8_t {
    None,
    Original,   // a 1988 PC-speaker call site (sfx-catalog.md)
    NativeHook, // a native cue name (see the SfxId comment)
    Diagnostic  // device self-test
};

/** The cue string the core emits (GameEvent::text), e.g. "move-blocked". */
const char *sfx_cue(SfxId);
/** Cue string -> id; SfxId::None for null, empty or unknown text. PURE. */
SfxId sfx_from_cue(const char *cue);
SfxOrigin sfx_origin(SfxId);

// ---------------------------------------------------------------------------
// Music -- only ever from a supported community music patch (see
// audio_pack.h). The stock 1988 DOS game has NO music: its only audio
// hardware is the PC speaker (re/notes/audio-profile-1988.md).
//
// Songs are the Exodus Project "Ultima V Upgrade" 1.0 driver's own ids: the
// index of mid.drv's 16-entry file table at offset 0x20
// (re/notes/music-location-mapping.md section 2).
// ---------------------------------------------------------------------------
enum class MusicSong : uint8_t {
    Theme = 0x0, BritannicLands = 0x1, Hornpipe = 0x2, Engagement = 0x3, Stones = 0x4,
    Greyson = 0x5, Fanfare = 0x6, Monarch = 0x7, Tarantella = 0x8, HallsOfDoom = 0x9,
    WorldsBelow = 0xa, Blackthorn = 0xb, LadyNan = 0xc, Reunion = 0xd, RuleBritannia = 0xe,
    Amiga = 0xf,
    None = 0xff
};
constexpr size_t kMusicSongCount = 16;
/** The patch's Files.txt title, for logs and diagnostics ("" for None). */
const char *music_song_title(MusicSong);

/**
 * What should be playing, named by situation rather than by track. Two
 * families, because the patch's driver treats them differently: location
 * contexts are DERIVED by mid.drv selector 0x00 from the game state on every
 * key poll; scripted contexts are IMPOSED by a scene through their own
 * selectors and freeze the location rule until it is re-enabled. Mirrors
 * game/src/ui/music.ts `MusicContext`, in the same order. APPEND only.
 */
enum class MusicContext : uint8_t {
    // selector 0x00, the switch at mid.drv 0x016d
    Overworld, Underworld, Frigate, Cities, Lighthouse, Hut, Castle, BlackthornPalace, Village,
    Keep, Principle, Dungeon, Combat, Victory,
    // scripted selectors 0x09/0x0c/0x12/0x15/0x1b and the tables 0x223/0x24a
    Title, Creation, Shrine, Camp, IntroStones, IntroHalls, IntroGreyson, EndgameStones,
    EndgameLadyNan, Reunion, Finale,
    // selector 0x03: stop
    Silence,
    Count
};
constexpr size_t kMusicContextCount = size_t(MusicContext::Count);
const char *music_context_name(MusicContext);
/** Context -> the song the driver plays for it; MusicSong::None = silence. */
MusicSong song_for_context(MusicContext);

/** The four game globals mid.drv 0x016d reads, as the port holds them. */
struct LocationMusicInput {
    uint8_t location = 0;        // g_location [0x5893]
    uint8_t floor = 0;           // g_floor [0x5895] (0 = Britannia, else Underworld)
    uint8_t transport_tile = 0;  // g_transport_tile [0x587c]
    bool in_combat = false;      // g_location == 0xff while an arena runs
    bool combat_victory = false; // g_cmb_victory_flag [0x58a3]
};
/**
 * The location selector, branch for branch in the driver's order: combat ->
 * frigate -> location. Locations outside 0x00..0x28 (demo, endgame scenes)
 * give Silence. PURE; derived in music-location-mapping.md section 1.2.
 */
MusicContext music_context_for_location(const LocationMusicInput &);

/**
 * The intro's page-range table, mid.drv 0x223 (selector 0x06), indexed by
 * the cinematic page INTRO.OVL 0x0adc passes in BL. Pages outside 0..0x15
 * are Silence (game/src/ui/music.ts introPageContext, ported). PURE.
 */
MusicContext intro_page_music_context(uint8_t page);
/**
 * The endgame's scene-range table, mid.drv 0x24a (selector 0x18), indexed by
 * the scene ENDGAME.OVL 0x0aee passes in BL. Scenes outside 0..7 are Silence
 * (game/src/ui/music.ts endgameSceneContext, ported). PURE.
 */
MusicContext endgame_scene_music_context(uint8_t scene);

// ---------------------------------------------------------------------------
// Capability. What the user's own asset set supports (decided by the packer,
// recorded in the audio pack) and what the device can therefore offer.
// ---------------------------------------------------------------------------
enum class MusicCapability : uint8_t {
    StockNoMusic = 0,          // no trace of a music patch: correct, no music
    SupportedMusicPatch = 1,   // Exodus U5 Upgrade 1.0, complete and valid
    IncompleteMusicPatch = 2,  // parts of that patch, not enough to play
    UnknownMusicVariant = 3    // a music driver/patch we cannot vouch for
};
const char *music_capability_name(MusicCapability);

/** What the Settings row and the service see at run time. */
enum class MusicAvailability : uint8_t {
    Available,        // audio pack valid and it holds the supported patch
    StockNoMusic,     // audio pack valid: the game files have no music
    IncompletePatch,  // audio pack valid: the patch is incomplete
    UnknownVariant,   // audio pack valid: an unsupported music variant
    NoAudioPack,      // /ultima5/openu5-audio.bin is not on the card
    AudioPackInvalid  // present but stale, corrupt or inconsistent
};
const char *music_availability_name(MusicAvailability);
/** One short line for the Settings footer; nullptr when music is available. */
const char *music_unavailable_reason(MusicAvailability);

// ---------------------------------------------------------------------------
// Volume. Both settings are 0..100 %, stepped by 10 in Settings, 80 by
// default (FrontendSettings::sound_volume / music_volume, persisted in
// settings.json since Alpha 2). 0 is mute.
// ---------------------------------------------------------------------------
constexpr uint8_t kVolumeMin = 0, kVolumeMax = 100, kVolumeStep = 10;
constexpr uint8_t kDefaultSfxVolume = 80, kDefaultMusicVolume = 80;
constexpr uint16_t kUnityGainQ15 = 32767;
/** One Settings step: `direction` < 0 lowers, otherwise raises; clamped, no wrap. */
uint8_t step_volume(uint8_t current, int direction);
/**
 * Percent -> linear Q15 gain on a square law (perceived loudness grows
 * roughly with the square of amplitude): 0 -> 0, 50 -> 8191, 100 -> 32767
 * exactly. Values above 100 are treated as 100. PURE.
 */
uint16_t volume_to_gain_q15(uint8_t volume);
/** Scale one PCM sample; saturates, so no gain can overflow int16. PURE. */
int16_t apply_gain_q15(int16_t sample, uint16_t gain_q15);

// ---------------------------------------------------------------------------
// Backend seam. The device implements it over I2S; the host records it.
// CONTRACT: every call returns without waiting for audio hardware, and a
// backend never calls back into the service or the game.
// ---------------------------------------------------------------------------
enum class AudioChannel : uint8_t { Sfx, Music };

struct SfxRequest {
    SfxId id = SfxId::None;
    int32_t param = 0;       // GameEvent::note (harpsichord digit, ceremony index...)
    uint16_t gain_q15 = 0;   // the SFX channel gain at submission
    uint32_t sequence = 0;   // monotonic, per service
};

class AudioBackend {
  public:
    virtual ~AudioBackend() = default;
    /** false = not taken (busy, unsupported id or failed); the service drops it. */
    virtual bool play_sfx(const SfxRequest &) = 0;
    virtual void stop_sfx() = 0;
    /** false = could not start; the service then treats music as stopped. */
    virtual bool start_music(MusicSong, uint16_t gain_q15) = 0;
    virtual void stop_music() = 0;
    /** Live gain of one channel (0 = silent). Never touches the other channel. */
    virtual void set_gain(AudioChannel, uint16_t gain_q15) = 0;
};

/** The silent backend: takes everything, renders nothing. */
class NullAudioBackend final : public AudioBackend {
  public:
    bool play_sfx(const SfxRequest &) override { return true; }
    void stop_sfx() override {}
    bool start_music(MusicSong, uint16_t) override { return true; }
    void stop_music() override {}
    void set_gain(AudioChannel, uint16_t) override {}
};

/**
 * The semantic audio service. Policy (ALPHA3_AUDIO.md "Service policy"):
 *  - SFX: requests go to the backend in emission order with the SFX gain;
 *    volume 0 drops them (mute); a refusal is counted, never retried.
 *  - Music: a context names a song; nothing reaches the backend unless music
 *    is Available; the same song is never restarted (the driver's
 *    `cmp al,[0x11e]`); volume 0 stops the song but keeps the context, and
 *    raising the volume starts the context's song again.
 *  - No ducking and no SFX/music interaction: in the patched original the
 *    MIDI card and the PC speaker were separate hardware.
 *  - A3-05 mutes (Alt+Shift+M / Alt+Shift+S): a per-channel flag on top of
 *    the volume, session-only. A muted channel behaves exactly as volume 0
 *    (SFX dropped, the song stopped) while the configured volume, which
 *    Settings shows and settings.json stores, is never written.
 */
class AudioService {
  public:
    struct Stats {
        // sfx_muted: not submitted because the output is silent (volume 0 or
        // no backend). sfx_refused: the backend declined; dropped, not retried.
        uint32_t sfx_requested = 0, sfx_submitted = 0, sfx_muted = 0, sfx_refused = 0, sfx_unknown = 0;
        uint32_t music_requested = 0, music_started = 0, music_unavailable = 0, music_refused = 0;
    };

    /** nullptr = silent. Re-sends both channel gains to the new backend. */
    void attach(AudioBackend *);
    void set_music_availability(MusicAvailability);
    MusicAvailability music_availability() const { return availability_; }
    bool has_music() const { return availability_ == MusicAvailability::Available; }

    void set_sfx_volume(uint8_t);
    void set_music_volume(uint8_t);
    uint8_t sfx_volume() const { return sfx_volume_; }
    uint8_t music_volume() const { return music_volume_; }
    void set_sfx_muted(bool);
    void set_music_muted(bool);
    bool sfx_muted() const { return sfx_muted_; }
    bool music_muted() const { return music_muted_; }

    void play_sfx(SfxId, int32_t param = 0);
    void stop_sfx();
    void play_music(MusicContext);
    /** Selector 0x03: stop, and remember Silence as the context. */
    void stop_music();
    /** A load replaces the world: flush SFX of the old one. Music is untouched. */
    void flush_for_load();

    MusicContext current_music_context() const { return context_; }
    /** The song the backend was told to play; None while silent. */
    MusicSong current_song() const { return song_; }
    const Stats &stats() const { return stats_; }

  private:
    void sync_music();
    // The SFX channel's live gain. Music needs none: its mute stops the song.
    uint16_t sfx_gain() const { return sfx_muted_ ? 0 : volume_to_gain_q15(sfx_volume_); }
    AudioBackend *backend_ = nullptr;
    MusicAvailability availability_ = MusicAvailability::NoAudioPack;
    uint8_t sfx_volume_ = kDefaultSfxVolume, music_volume_ = kDefaultMusicVolume;
    bool sfx_muted_ = false, music_muted_ = false;
    MusicContext context_ = MusicContext::Silence;
    MusicSong song_ = MusicSong::None;
    uint32_t sequence_ = 0;
    Stats stats_{};
};

// ---------------------------------------------------------------------------
// Settings rows (System Menu and title Settings share these).
// ---------------------------------------------------------------------------
/**
 * A3-05. Which volume rows a Settings key edited (SystemMenuSession /
 * FrontendSession::take_volume_edits): editing a row unmutes that channel.
 */
enum VolumeEdit : uint8_t { kSfxVolumeEdited = 1, kMusicVolumeEdited = 2 };
/** "SFX Volume: 80%", or "SFX Volume: 80% (muted)" under the A3-05 mute. */
void format_sfx_volume_row(char *out, size_t size, uint8_t volume, bool muted = false);
/** "Music Volume: 80%", or "Music Volume: Unavailable" when music is not available. */
void format_music_volume_row(char *out, size_t size, uint8_t volume, MusicAvailability, bool muted = false);

} // namespace openu5
