#include "openu5/audio.h"

#include <cstdio>
#include <cstring>

// Alpha 3 A3-01. Tables and policy only; no hardware, no game state, no RNG.
namespace openu5 {
namespace {

struct SfxRow {
    const char *cue;
    SfxOrigin origin;
};

// Index = SfxId. The reference rows are game/src/core/sfx.ts, in its order.
constexpr SfxRow kSfx[] = {
    {nullptr, SfxOrigin::None},
    {"combat-hit", SfxOrigin::Original},             // NB(10,3000,2000) kernel 0x35de
    {"combat-hit-heavy", SfxOrigin::Original},       // NB(40,3000,500) kernel 0x35c9
    {"combat-damage", SfxOrigin::Original},          // NB(10,1600,2000) kernel 0x2a68
    {"combat-defeat", SfxOrigin::Original},          // NB(40,3000,500) kernel 0x2fe3
    {"cast-spell", SfxOrigin::Original},             // reference attribution disputed (sfx.ts)
    {"spell-zap", SfxOrigin::Original},              // TS(0x2648,1,28000,1000,2) CAST 0x0d85
    {"line-spray", SfxOrigin::Original},             // NB + crackle, CAST2
    {"time-spell", SfxOrigin::Original},             // CAST2 0x0000 tables 0x4af6..0x4b2c
    {"shrine-well-done", SfxOrigin::Original},       // CAST2 mirrored sweeps
    {"shrine-donation", SfxOrigin::Original},        // CAST2 mirrored sweeps
    {"shrine-ordained", SfxOrigin::Original},        // CAST2 seven-note melody
    {"sceptre", SfxOrigin::Original},                // TS(0x1450,1,50000,5000,1) CAST 0x198f (A3-03)
    {"blackthorn-materialize", SfxOrigin::Original}, // TS(0xaf0,1,0x32c8,0x64,5) BLCKTHRN 0x083f
    {"shard-sweep", SfxOrigin::Original},            // CAST 0x15f0/0x1620 siren legs
    {"victory-fanfare", SfxOrigin::Original},        // TSx4 kernel 0x4368
    {"moongate", SfxOrigin::Original},               // TS(0x170c,1,30000,2000,2) kernel 0x48e5
    {"quake", SfxOrigin::Original},                  // screen-shake rumble
    {"waterfall-fall", SfxOrigin::Original},         // GL(2500->800,1,300) OUTSUBS 0x0492
    {"shadowlord-announce", SfxOrigin::Original},    // TS(0x19c8,1,60000,2000,1) TOWN 0x11e9
    {"instrument-note", SfxOrigin::Original},        // TS(note[0x2746],1,4000,20000,-4) TOWN 0x0e6d
    {"shop-transaction", SfxOrigin::Original},       // TSx6 SHOPPES 0x13b0
    {"cannon-fire", SfxOrigin::Original},            // GL(1000->200,5,300) CMDS 0x09d5
    {"combat-escape", SfxOrigin::Original},          // GL(1200->2000,1,40) CMDS 0x18ac
    {"combat-reject", SfxOrigin::Original},
    {"combat-absorbed", SfxOrigin::Original},        // GL(1200->2000,1,40) SJOG 0x1f08
    {"ring-vanishes", SfxOrigin::Original},          // GL(1200->2000,1,40) ZSTATS 0x0e42
    {"ambient-fountain", SfxOrigin::Original},       // NB(10,30,25000) kernel 0x42c4
    {"ambient-waterfall", SfxOrigin::Original},      // NB(20,60,10000) kernel 0x42be
    {"ambient-clock-tick", SfxOrigin::Original},     // beep(3000,3) kernel 0x42a1
    {"ambient-clock-tock", SfxOrigin::Original},     // beep(2000,3) kernel 0x429c
    {"ambient-clock-chime", SfxOrigin::Original},    // TS(3116,1,2000,20000,-10) kernel 0x428b
    {"move-step", SfxOrigin::Original},              // NBx2 kernel 0x433e
    {"move-blocked", SfxOrigin::Original},           // beep(0xa5,0xc8) MAINOUT 0x0344 / TOWN 0x0849
    {"search-fail", SfxOrigin::Original},
    {"dungeon-trap", SfxOrigin::Original},           // NB(40,3000,500) kernel 0x2fe3
    {"dungeon-zap", SfxOrigin::Original},            // NB(1,500,20000) DUNGEON 0x04b9
    {"field-afflict", SfxOrigin::Original},          // NB(1,50,3500) DUNGEON 0x099e/0x0a30
    {"dungeon-fail", SfxOrigin::Original},           // GL(800->2000,1,50) DUNGEON 0x1cfb
    {"torch-borrowed", SfxOrigin::Original},         // GL(800->2000,1,50) SJOG 0x1a21
    {"mirror-break", SfxOrigin::Original},
    {"apparition-materialize", SfxOrigin::Original}, // TS(0xa3c,1,10000,2500,6) OUTSUBS 0x067b
    {"apparition-arpeggio", SfxOrigin::Original},    // TS(tbl[0x3a26],1,5000,200,13) OUTSUBS 0x0698
    {"apparition-heal-chime", SfxOrigin::Original},  // TS(0x157c,1,5000,200,13) OUTSUBS 0x0896
    {"apparition-chord", SfxOrigin::Original},       // TS(0x157c,1,60000,2500,1) OUTSUBS 0x08c1
    {"refuge-thunder", SfxOrigin::Original},         // BLCKTHRN party_refuge
    {"intro-thunder", SfxOrigin::Original},          // NB(20,60,10000) FONT 0x03ca
    {"intro-chime", SfxOrigin::Original},
    {"intro-summon", SfxOrigin::Original},
    {"title-fizzle", SfxOrigin::Original},
    {"title-crackle", SfxOrigin::Original},
    {"bard-song", SfxOrigin::Original},              // tables 0x6a48/0x6a34
    {"endgame-orb", SfxOrigin::Original},            // ENDGAME 0x078f/0x0987
    {"spell-cast", SfxOrigin::NativeHook},
    {"potion-used", SfxOrigin::NativeHook},
    {"scroll-used", SfxOrigin::NativeHook},
    {"invalid-magic", SfxOrigin::NativeHook},
    {"diagnostic-tone", SfxOrigin::Diagnostic},
    // A3-03 (sfx_inventory.cpp has the sites and the derivations).
    {"sceptre-reclaimed", SfxOrigin::Original},      // TS(0xfd2,1,65000,1,1) kernel 0x6221
    {"combat-charm", SfxOrigin::Original},           // TS(0xc1c,1,30000,1000,2) SJOG 0x2218
    {"combat-summon", SfxOrigin::Original},          // TS(0xac8,1,5000,1000,15) COMSUBS 0x02cb
    {"combat-grazed", SfxOrigin::Original},          // GL(1200->2000,1,40) COMSUBS 0x0352
    {"combat-dragged-under", SfxOrigin::Original},   // GL(1200->2000,1,40) COMSUBS 0x03d6
    {"combat-engulfed", SfxOrigin::Original},        // NB(40,3000,500) COMBAT 0x07f1
    {"combat-regurgitated", SfxOrigin::Original},    // NB(1,7000,600) COMBAT 0x1cbf
    {"combat-food-stolen", SfxOrigin::Original},     // GL(800->2000,1,50) COMBAT 0x03b6
    {"ship-collision", SfxOrigin::Original},         // NB(100,2000,300) MAINOUT 0x0300
    {"ship-sinking", SfxOrigin::Original},           // GL(660->150,40,7800) MAINOUT 0x113b / 0x12a6
    {"theft-detected", SfxOrigin::Original},         // GL(800->2000,1,50) TALK 0x11a8
    {"wish-granted", SfxOrigin::Original},           // NB(10,3000,2000) LOOKOBJ 0x0129
    {"trapdoor-fall", SfxOrigin::Original},          // set_tone ramp 1000..251 + NB, TOWN 0x0fa0
    {"shard-no-effect", SfxOrigin::Original},        // GL(800->2000,1,50) CAST 0x166d
    {"refuge-slumber", SfxOrigin::Original},         // TS x6 tables DS 0x3720.. BLCKTHRN 0x0a34
    {"refuge-revival", SfxOrigin::Original},         // TS(0x8e30/(i+7),1,30000,2000,2) BLCKTHRN 0x0b8d
};
static_assert(sizeof(kSfx) / sizeof(kSfx[0]) == kSfxIdCount, "one row per SfxId");

constexpr const char *kSongTitles[kMusicSongCount] = {
    "Ultima V Theme",       "Britannic Lands",    "Cap'n Johne's Hornpipe", "Engagement and Melee",
    "Stones",               "Greyson's Tale",     "Fanfare for the Virtuous", "The Missing Monarch",
    "Villager Tarantella",  "Halls of Doom",      "Worlds Below",           "Lord Blackthorn",
    "Dream of Lady Nan",    "Joyous Reunion",     "Rule Britannia",         "Amiga Theme"};

struct ContextRow {
    const char *name;
    MusicSong song;
};

// Index = MusicContext. game/src/ui/music.ts CONTEXT_SONG, row for row.
constexpr ContextRow kContexts[] = {
    {"overworld", MusicSong::BritannicLands},  // loc 0, g_floor == 0
    {"underworld", MusicSong::WorldsBelow},    // loc 0, g_floor != 0
    {"frigate", MusicSong::Hornpipe},          // (g_transport_tile & 0xf8) == 0x20
    {"cities", MusicSong::Tarantella},         // loc 0x01..0x08
    {"lighthouse", MusicSong::LadyNan},        // loc 0x09..0x0c
    {"hut", MusicSong::Greyson},               // loc 0x0d..0x10
    {"castle", MusicSong::Monarch},            // loc 0x11
    {"blackthorn", MusicSong::Blackthorn},     // loc 0x12
    {"village", MusicSong::Greyson},           // loc 0x13..0x18
    {"keep", MusicSong::LadyNan},              // loc 0x19..0x1d
    {"principle", MusicSong::Fanfare},         // loc 0x1e..0x20
    {"dungeon", MusicSong::HallsOfDoom},       // loc 0x21..0x28
    {"combat", MusicSong::Engagement},         // loc 0xff, victory flag 0
    {"victory", MusicSong::Theme},             // loc 0xff, victory flag set
    {"title", MusicSong::Theme},               // sel 0x09
    {"creation", MusicSong::Amiga},            // sel 0x0c
    {"shrine", MusicSong::Stones},             // sel 0x12
    {"camp", MusicSong::Stones},               // sel 0x12
    {"intro-stones", MusicSong::Stones},       // sel 0x06, table 0x223 pages 0..7
    {"intro-halls", MusicSong::HallsOfDoom},   // sel 0x06, pages 8..0x0e
    {"intro-greyson", MusicSong::Greyson},     // sel 0x06, pages 0x0f..0x15
    {"endgame-stones", MusicSong::Stones},     // sel 0x18, table 0x24a scenes 0..3
    {"endgame-ladynan", MusicSong::LadyNan},   // sel 0x18, scenes 4..7
    {"reunion", MusicSong::Reunion},           // sel 0x15
    {"finale", MusicSong::RuleBritannia},      // sel 0x1b
    {"silence", MusicSong::None},              // sel 0x03
};
static_assert(sizeof(kContexts) / sizeof(kContexts[0]) == kMusicContextCount, "one row per MusicContext");

} // namespace

const char *sfx_cue(SfxId id) {
    const size_t i = size_t(id);
    return i < kSfxIdCount ? kSfx[i].cue : nullptr;
}

SfxId sfx_from_cue(const char *cue) {
    if (!cue || !*cue) return SfxId::None;
    for (size_t i = 1; i < kSfxIdCount; ++i)
        if (std::strcmp(kSfx[i].cue, cue) == 0) return SfxId(i);
    return SfxId::None;
}

SfxOrigin sfx_origin(SfxId id) {
    const size_t i = size_t(id);
    return i < kSfxIdCount ? kSfx[i].origin : SfxOrigin::None;
}

const char *music_song_title(MusicSong song) {
    const size_t i = size_t(song);
    return i < kMusicSongCount ? kSongTitles[i] : "";
}

const char *music_context_name(MusicContext context) {
    const size_t i = size_t(context);
    return i < kMusicContextCount ? kContexts[i].name : "";
}

MusicSong song_for_context(MusicContext context) {
    const size_t i = size_t(context);
    return i < kMusicContextCount ? kContexts[i].song : MusicSong::None;
}

// mid.drv 0x016d, in its order: the combat sentinel, then the frigate, then
// the location ranges. The frozen-mode branch (0x0170) is not a location
// question: scripted contexts are requested by name instead.
MusicContext music_context_for_location(const LocationMusicInput &in) {
    if (in.in_combat) return in.combat_victory ? MusicContext::Victory : MusicContext::Combat; // 0x0185
    if ((in.transport_tile & 0xf8) == 0x20) return MusicContext::Frigate;                        // 0x019d
    const uint8_t loc = in.location;
    if (loc == 0x00) return in.floor == 0 ? MusicContext::Overworld : MusicContext::Underworld;  // 0x01ae
    if (loc <= 0x08) return MusicContext::Cities;                                                // 0x01c3
    if (loc <= 0x0c) return MusicContext::Lighthouse;                                            // 0x01cd
    if (loc <= 0x10) return MusicContext::Hut;                                                   // 0x01d7
    if (loc == 0x11) return MusicContext::Castle;                                                // 0x01e1
    if (loc == 0x12) return MusicContext::BlackthornPalace;                                      // 0x01eb
    if (loc <= 0x18) return MusicContext::Village;                                               // 0x01f5
    if (loc <= 0x1d) return MusicContext::Keep;                                                  // 0x01ff
    if (loc <= 0x20) return MusicContext::Principle;                                             // 0x0209
    if (loc <= 0x28) return MusicContext::Dungeon;                                               // 0x0213
    return MusicContext::Silence;                                                                // 0x021d
}

MusicContext intro_page_music_context(uint8_t page) {
    if (page <= 0x07) return MusicContext::IntroStones;
    if (page <= 0x0e) return MusicContext::IntroHalls;
    if (page <= 0x15) return MusicContext::IntroGreyson;
    return MusicContext::Silence;
}

MusicContext endgame_scene_music_context(uint8_t scene) {
    if (scene <= 0x03) return MusicContext::EndgameStones;
    if (scene <= 0x07) return MusicContext::EndgameLadyNan;
    return MusicContext::Silence;
}

const char *music_capability_name(MusicCapability c) {
    switch (c) {
    case MusicCapability::StockNoMusic: return "stock-no-music";
    case MusicCapability::SupportedMusicPatch: return "supported-music-patch";
    case MusicCapability::IncompleteMusicPatch: return "incomplete-music-patch";
    case MusicCapability::UnknownMusicVariant: return "unknown-music-variant";
    }
    return "invalid";
}

const char *music_availability_name(MusicAvailability a) {
    switch (a) {
    case MusicAvailability::Available: return "available";
    case MusicAvailability::StockNoMusic: return "stock-no-music";
    case MusicAvailability::IncompletePatch: return "incomplete-patch";
    case MusicAvailability::UnknownVariant: return "unknown-variant";
    case MusicAvailability::NoAudioPack: return "no-audio-pack";
    case MusicAvailability::AudioPackInvalid: return "audio-pack-invalid";
    }
    return "invalid";
}

const char *music_unavailable_reason(MusicAvailability a) {
    switch (a) {
    case MusicAvailability::Available: return nullptr;
    case MusicAvailability::StockNoMusic: return "Stock DOS game files have no music";
    case MusicAvailability::IncompletePatch: return "Music patch files are incomplete";
    case MusicAvailability::UnknownVariant: return "Unsupported music patch variant";
    case MusicAvailability::NoAudioPack: return "No audio pack: npm run pack:audio";
    case MusicAvailability::AudioPackInvalid: return "Audio pack stale or corrupt: rebuild";
    }
    return "Music unavailable";
}

uint8_t step_volume(uint8_t current, int direction) {
    const int v = int(current > kVolumeMax ? kVolumeMax : current) + (direction < 0 ? -kVolumeStep : kVolumeStep);
    return uint8_t(v < kVolumeMin ? kVolumeMin : v > kVolumeMax ? kVolumeMax : v);
}

uint16_t volume_to_gain_q15(uint8_t volume) {
    const uint32_t v = volume > kVolumeMax ? kVolumeMax : volume;
    return uint16_t(v * v * kUnityGainQ15 / (uint32_t(kVolumeMax) * kVolumeMax));
}

int16_t apply_gain_q15(int16_t sample, uint16_t gain_q15) {
    const uint16_t g = gain_q15 > kUnityGainQ15 ? kUnityGainQ15 : gain_q15;
    const int32_t scaled = (int32_t(sample) * int32_t(g)) / 32768;
    return int16_t(scaled < -32768 ? -32768 : scaled > 32767 ? 32767 : scaled);
}

void AudioService::attach(AudioBackend *backend) {
    if (backend_ && backend_ != backend) {
        backend_->stop_sfx();
        backend_->stop_music();
    }
    backend_ = backend;
    song_ = MusicSong::None; // a newly attached backend is playing nothing
    if (backend_) {
        backend_->set_gain(AudioChannel::Sfx, volume_to_gain_q15(sfx_volume_));
        backend_->set_gain(AudioChannel::Music, volume_to_gain_q15(music_volume_));
    }
    sync_music();
}

void AudioService::set_music_availability(MusicAvailability availability) {
    availability_ = availability;
    sync_music();
}

// A channel is only touched when ITS volume changes: the device re-applies
// every setting after any Settings edit, and an SFX edit must not reach the
// music channel (attach() is what sends both gains to a new backend).
void AudioService::set_sfx_volume(uint8_t volume) {
    const uint8_t v = volume > kVolumeMax ? kVolumeMax : volume;
    if (v == sfx_volume_) return;
    sfx_volume_ = v;
    if (backend_) backend_->set_gain(AudioChannel::Sfx, volume_to_gain_q15(sfx_volume_));
}

void AudioService::set_music_volume(uint8_t volume) {
    const uint8_t v = volume > kVolumeMax ? kVolumeMax : volume;
    if (v == music_volume_) return;
    music_volume_ = v;
    if (backend_) backend_->set_gain(AudioChannel::Music, volume_to_gain_q15(music_volume_));
    sync_music();
}

void AudioService::play_sfx(SfxId id, int32_t param) {
    ++stats_.sfx_requested;
    if (id == SfxId::None || size_t(id) >= kSfxIdCount) {
        ++stats_.sfx_unknown;
        return;
    }
    if (!backend_ || sfx_volume_ == 0) {
        ++stats_.sfx_muted;
        return;
    }
    SfxRequest request{};
    request.id = id;
    request.param = param;
    request.gain_q15 = volume_to_gain_q15(sfx_volume_);
    request.sequence = ++sequence_;
    if (backend_->play_sfx(request)) ++stats_.sfx_submitted;
    else ++stats_.sfx_refused; // dropped: never retried, never waited on
}

void AudioService::stop_sfx() {
    if (backend_) backend_->stop_sfx();
}

void AudioService::play_music(MusicContext context) {
    ++stats_.music_requested;
    context_ = size_t(context) < kMusicContextCount ? context : MusicContext::Silence;
    if (!has_music()) ++stats_.music_unavailable;
    sync_music();
}

void AudioService::stop_music() {
    context_ = MusicContext::Silence;
    sync_music();
}

void AudioService::flush_for_load() { stop_sfx(); }

// The one place music reaches the backend. What SHOULD play is a pure
// function of (availability, volume, context); the backend is only told when
// that changes, so a repeated request never restarts a song.
void AudioService::sync_music() {
    const MusicSong want =
        has_music() && music_volume_ > 0 ? song_for_context(context_) : MusicSong::None;
    if (!backend_) {
        song_ = MusicSong::None;
        return;
    }
    if (want == song_) return;
    if (want == MusicSong::None) {
        backend_->stop_music();
        song_ = MusicSong::None;
        return;
    }
    if (backend_->start_music(want, volume_to_gain_q15(music_volume_))) {
        song_ = want;
        ++stats_.music_started;
    } else {
        song_ = MusicSong::None;
        ++stats_.music_refused;
    }
}

void format_sfx_volume_row(char *out, size_t size, uint8_t volume) {
    std::snprintf(out, size, "SFX Volume: %u%%", unsigned(volume > kVolumeMax ? kVolumeMax : volume));
}

void format_music_volume_row(char *out, size_t size, uint8_t volume, MusicAvailability availability) {
    if (availability == MusicAvailability::Available)
        std::snprintf(out, size, "Music Volume: %u%%", unsigned(volume > kVolumeMax ? kVolumeMax : volume));
    else
        std::snprintf(out, size, "Music Volume: Unavailable");
}

} // namespace openu5
