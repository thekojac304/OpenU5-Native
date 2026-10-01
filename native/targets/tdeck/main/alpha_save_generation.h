#pragma once
// The storage-independent half of AlphaSaveService (alpha_save.cpp).
//
// A save generation is three files plus a commit record on the SD card.
// alpha_save.cpp owns reading and writing them; everything that decides
// whether the bytes it read are a generation that may replace the live world
// lives here, so the firmware and the host tests run the same code:
//
//   verify_candidate()  -- CRC/parse (complete_generation) and the semantic
//                          staging of every owner a load will commit. Used by
//                          the post-write self-check, load_slot and inspect.
//   restore_candidate() -- the same staging over copies of the live owners;
//                          commits them only when the whole generation passes.
//   restore_newest()    -- Continue Latest / Alt+L: the newest complete
//                          generation, falling back to the other one.
//
// Batch 28 (H-166) split this out of alpha_save.cpp unchanged; see
// stage_generation() in the .cpp for the validation boundary.
#include <cstdint>
#include <string>
#include <vector>

#include "openu5/commands.h"
#include "openu5/dungeon.h"
#include "openu5/frontend.h"
#include "openu5/gameplay_save.h"

namespace tdeck {

// On-card layout of alpha1-g<slot>.commit: written and read as raw bytes.
struct AlphaSaveCommit { uint32_t magic=0x31533555,version=1;uint64_t sequence=0;uint32_t gam=0,ool=0,json=0; };

struct AlphaSaveCandidate {
    AlphaSaveCommit commit{};
    std::vector<uint8_t> gam, ool, json;
    std::string side;
    openu5::save::Generation generation{};
};

// Scratch copies of every owner a load replaces. Large; the device keeps one
// in PSRAM (AlphaSaveScratch), never on the stack.
struct AlphaSaveStage {
    openu5::GameState game{};
    openu5::TurnState turn{};
    openu5::CommandState commands{};
    openu5::OutdoorServices outdoor{};
    openu5::WorldTerrain terrain{};
    openu5::NpcActors actors{};
    openu5::save::Json document{};
    openu5::DungeonState dungeon{};
};

// Batch 53. The capture chain every save runs over the live owners before
// export_native_state. alpha_save.cpp and the host memory stub both call this
// one function, so an owner added to a save (the moonstones, RB-3) cannot
// reach one of them and not the other.
void capture_save_document(openu5::CommandContext &, const openu5::OutdoorServices &, const openu5::WorldTerrain &,
                           const openu5::NpcActors &, openu5::save::Json &retained);

// `v` holds the commit and the three files' bytes. Fills v.side/v.generation
// and returns whether the generation is usable. The staged owners are left in
// `stage` (inspect() reads the party name from stage.game). A3-04G: the stage's
// previous contents are released FIRST, before the generation is imported, so
// a verify never holds two save documents at once.
bool verify_candidate(AlphaSaveCandidate &v, AlphaSaveStage &stage);
bool restore_candidate(AlphaSaveCandidate &pick, openu5::CommandContext &, openu5::OutdoorServices &,
                       openu5::WorldTerrain &, openu5::NpcActors &, openu5::save::Json &retained,
                       AlphaSaveStage &stage);
// Returns the slot restored, or -1 with every live owner untouched.
int restore_newest(AlphaSaveCandidate (&candidates)[2], openu5::CommandContext &, openu5::OutdoorServices &,
                   openu5::WorldTerrain &, openu5::NpcActors &, openu5::save::Json &retained,
                   AlphaSaveStage &stage);

// Alpha 3 A3-04G (ALPHA3_AUDIO.md section 28). A staged generation is a save
// document imported from the GAM: ~2,700 nodes, ~190 KB on the device, every
// block at most 4 KiB, so internal RAM first (SPIRAM_MALLOC_ALWAYSINTERNAL).
// These give every buffer of a candidate or a stage back to the heap; the
// fixed-size parts (GameState and the rest) stay where they are.
void release_candidate(AlphaSaveCandidate &);
void release_stage(AlphaSaveStage &);
// What the title screen and the System Menu list for a generation that
// verify_candidate() accepted: present, valid, its sequence and the first
// member's name, from the staged game; since Alpha 4 UI Batch 2 also where
// (the HUD's caption), the game date and time, and the party's size.
openu5::FrontendSaveSlot summarize_candidate(const AlphaSaveCandidate &, const AlphaSaveStage &);

// Alpha 4 A4-SAVE1 (ALPHA4_UI.md section 3): which slot a save may write.
//
// A save replaces its target slot's files one by one before the new commit
// record exists, so from the first rename until the post-write check the
// target holds no loadable generation; only the other slot is left to load.
// The invariant: that other slot is the generation Continue would restore.
// Until A4-SAVE1 the target was (newest sequence + 1) & 1, chosen from the
// commit records alone; with a refused newest generation (torn files under an
// intact commit, or a post-write check that failed after the commit) that was
// the older slot -- the only valid generation.
//
// `present[i]`: slot i has a readable commit record `commits[i]`. The newest
// is the highest sequence (uint64_t; on a tie the lower slot, as
// openu5::save::select_generation picks). Returns -1 when neither is present.
int newest_committed_slot(const bool (&present)[2], const AlphaSaveCommit (&commits)[2]);
struct AlphaSaveTarget {
    int slot = 1;           // the slot the save writes (a blank card: slot 1, as always)
    uint64_t sequence = 1;  // newest sequence + 1: the new generation is always the newest
    int keep = -1;          // the slot left untouched (-1: no generation to keep)
};
// `newest_accepted`: the newest slot's generation is one the load gate
// accepts. Accepted, it is kept and the other slot written (the rotation);
// refused, it is the expendable one and the older generation -- Continue's
// fallback -- is kept. Physical slot and sequence parity are never assumed to
// agree (an earlier recovery save breaks the parity).
//
// Alpha 4 A4-SAVE2 (ALPHA4_UI.md section 4): a card holds three slots, each a
// pair of its own, and one sequence runs across all of them, so Continue can
// name the slot saved last. `card_newest` is the highest sequence of any
// commit record on the card (0: none); the new sequence is above it and above
// this pair's newest. The pair's target rule is unchanged.
AlphaSaveTarget choose_save_target(const bool (&present)[2], const AlphaSaveCommit (&commits)[2], bool newest_accepted,
                                   uint64_t card_newest = 0);
// Whether the three files' CRC-32s are the ones the commit record names:
// the bytes are the ones that record was written for.
bool files_match_commit(const AlphaSaveCandidate &);

} // namespace tdeck
