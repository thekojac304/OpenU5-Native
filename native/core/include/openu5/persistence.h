#pragma once
#include "actors.h"
#include "save_json.h"
#include "turn.h"
#include <array>
namespace openu5::save {
constexpr size_t kGamSize = 4192, kOolSize = 512;
using Gam = std::array<uint8_t, kGamSize>;
using Ool = std::array<uint8_t, kOolSize>;
enum class Error {
    None,
    JsonSyntax,
    Capacity,
    Version,
    Characters,
    Schema,
    ShortGam,
    ShortTemplate,
    NativeDomain
};
enum class EnvelopeKind { None, Bad, Ok };
enum class SidecarSource { None, File, Envelope };
struct Envelope {
    EnvelopeKind kind = EnvelopeKind::None;
    Json document;
};
Json empty_sidecar();
bool sane_sidecar(const Json &);
Envelope read_envelope(const uint8_t *, size_t);
Error write_envelope(const Gam &, const Json &meta, const Json &sidecar, std::vector<uint8_t> &out);
Error deserialize(const std::string &, Json &);
Error import_native(const uint8_t *, size_t, const Json &sidecar, Json &state, bool npc_gate = false);
Error import_save(const uint8_t *, size_t, const std::string *explicit_sidecar, Json &state, SidecarSource &,
                  bool npc_gate = false);
Error export_native(const Json &state, const uint8_t *base, size_t, Gam &, Json &sidecar,
                    bool npc_gate = false);
Ool build_ool(const Json &, const uint8_t *base = nullptr, size_t length = 0);
// No .OOL import exists in the reference. Its inactive block is retained as a template.
// JSON documents carry fields owned by the untranslated world/application services.
// These adapters patch/read the existing live structs, never a parallel live state.
Error restore_core(const Json &, GameState &, TurnState &);
void capture_core(const GameState &, const TurnState &, Json &);
// Run enter_npc_map with static schedules first, then overlay the saved walk.
// Missing walk + open fidelity gate clears actors; closed gate uses schedules.
Error restore_npc_walk(const Json &, uint8_t location, bool npc_gate, NpcActors &);
void capture_npc_walk(const NpcActors &, uint8_t location, Json &);
// Semantic hooks: no paths, clocks, random ids, storage calls or hidden state.
Error load_state(const std::string &, GameState &, TurnState &, Json &retained);
Error save_state(const GameState &, const TurnState &, const Json &retained, std::string &out);
Error load_native_state(const uint8_t *, size_t, const std::string *explicit_sidecar, GameState &,
                        TurnState &, Json &retained, SidecarSource &, bool npc_gate = false);
Error export_native_state(const GameState &, const TurnState &, const Json &retained, const uint8_t *base,
                          size_t, Gam &, Json &sidecar, bool npc_gate = false);
// A generation is an adapter transaction, not a change to any save format.
// The adapter must verify identity/digests and commit the complete set atomically.
struct Generation {
    uint64_t sequence = 0;
    bool committed = false, identity_matches = false;
    const uint8_t *gam = nullptr;
    size_t gam_size = 0;
    const uint8_t *ool = nullptr;
    size_t ool_size = 0;
    const std::string *sidecar = nullptr;
    bool requires_ool = false, requires_sidecar = false;
    uint32_t gam_crc = 0, ool_crc = 0, sidecar_crc = 0;
};
uint32_t save_crc32(const uint8_t *, size_t);
bool complete_generation(const Generation &);
// Select greatest valid committed sequence; ties retain first input.
// This is SD transaction recovery, NOT browser loadMostRecentSave's behavior.
int select_generation(const Generation *, size_t);
// Caller increments the autosave pointer only after a successful commit.
int autosave_slot(int64_t pointer);
// Array of save-keys.ts SaveMeta objects. No body-validation fallback (TS rule).
int most_recent_slot(const Json &index);
} // namespace openu5::save
