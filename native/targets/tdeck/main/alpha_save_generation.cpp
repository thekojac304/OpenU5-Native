#include "alpha_save_generation.h"

#include <utility>

#include "esp_log.h"

namespace tdeck {
namespace {
constexpr char kTag[]="AlphaSave";
// The validation boundary: the one place a parsed generation becomes one that
// may replace the live world. Parse it into `s` and run every owner's decoder
// over it. `s` arrives seeded (zeroed for a self-check, the live owners for a
// restore); nothing outside `s` is written.
//
// Batch 28 (H-166). "worldObjects" and "dungeon" are restored only after the
// commit, by synchronize_loaded_world(), whose fallback on a bad payload is an
// empty pool / no session. Both are part of the same 1988 save window as the
// rest (object register 0x5C5A, g_dng_map 0x595A), so a generation whose
// sidecar they would refuse is refused whole here: never NEW's party with an
// emptied pool or a dropped session, and the older generation gets its turn.
bool stage_generation(AlphaSaveCandidate&v,AlphaSaveStage&s){
    openu5::save::SidecarSource source;
    auto err=openu5::save::load_native_state(v.gam.data(),v.gam.size(),&v.side,s.game,s.turn,s.document,source,true);
    if(err==openu5::save::Error::None)err=openu5::save::restore_gameplay(s.document,s.commands,s.outdoor);
    if(err==openu5::save::Error::None)err=openu5::save::restore_terrain(s.document,s.terrain);
    if(err==openu5::save::Error::None)err=openu5::save::restore_npc_walk(s.document,s.game.position.map.location,true,s.actors);
    if(err==openu5::save::Error::None)err=openu5::save::validate_world_objects(s.document);
    if(err==openu5::save::Error::None)err=openu5::save::restore_dungeon(s.document,s.dungeon);
    if(err!=openu5::save::Error::None)ESP_LOGW(kTag,"generation=%llu refused at the semantic gate",(unsigned long long)v.commit.sequence);
    return err==openu5::save::Error::None;
}
}

bool verify_candidate(AlphaSaveCandidate&v,AlphaSaveStage&s){
    v.side.assign(reinterpret_cast<const char*>(v.json.data()),v.json.size());auto &g=v.generation;g.sequence=v.commit.sequence;g.committed=true;g.identity_matches=true;g.gam=v.gam.data();g.gam_size=v.gam.size();g.ool=v.ool.data();g.ool_size=v.ool.size();g.sidecar=&v.side;g.requires_ool=g.requires_sidecar=true;g.gam_crc=v.commit.gam;g.ool_crc=v.commit.ool;g.sidecar_crc=v.commit.json;if(!openu5::save::complete_generation(g))return false;
    s.game={};s.turn={};s.document={};s.commands={};s.outdoor={};s.terrain={};s.actors={};
    return stage_generation(v,s);
}

bool restore_candidate(AlphaSaveCandidate&pick,openu5::CommandContext&c,openu5::OutdoorServices&o,openu5::WorldTerrain&t,openu5::NpcActors&a,openu5::save::Json&retained,AlphaSaveStage&s){
    // Validate and restore transactionally. A newer generation with a valid
    // commit/CRC but incompatible semantic payload must not partially replace
    // live state or prevent recovery from the older generation.
    s.game=c.game;s.turn=c.turn;s.commands=c.commands;s.outdoor=o;s.terrain=t;s.actors=a;s.document={};
    if(!stage_generation(pick,s))return false;
    c.game=std::move(s.game);c.turn=std::move(s.turn);c.commands=std::move(s.commands);
    o=std::move(s.outdoor);t=std::move(s.terrain);a=std::move(s.actors);retained=std::move(s.document);return true;
}

int restore_newest(AlphaSaveCandidate(&candidates)[2],openu5::CommandContext&c,openu5::OutdoorServices&o,openu5::WorldTerrain&t,openu5::NpcActors&a,openu5::save::Json&retained,AlphaSaveStage&s){
    openu5::save::Generation list[2]={candidates[0].generation,candidates[1].generation};
    for(int pass=0;pass<2;++pass){const int selected=openu5::save::select_generation(list,2);if(selected<0)break;
        auto &pick=candidates[selected];if(restore_candidate(pick,c,o,t,a,retained,s))return selected;
        ESP_LOGW(kTag,"generation=%llu slot=%d failed semantic restore; trying fallback",(unsigned long long)pick.commit.sequence,selected);list[selected]={};
    }
    ESP_LOGW(kTag,"no restorable save generation");return -1;
}

} // namespace tdeck
