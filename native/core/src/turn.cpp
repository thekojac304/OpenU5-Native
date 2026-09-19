#include "openu5/turn.h"
#include <algorithm>
namespace openu5 {
Rand rng_source(OriginalRng &rng) {
    return {&rng, [](void *p, int32_t lo, int32_t hi) { return static_cast<OriginalRng *>(p)->next(lo, hi).value; }};
}
const char *turn_message_text(TurnMessage m) {
    switch (m) {
    case TurnMessage::Starving: return "Starving!";
    case TurnMessage::Hic: return "Hic!";
    case TurnMessage::Trapdoor: return "A TRAPDOOR!";
    case TurnMessage::Burning: return "Burning!";
    }
    return "";
}
static void message(TurnResult &r, TurnMessage m) { r.messages[r.message_count++] = m; }
static void member(MemberIndices &r, int32_t i) { r.values[r.count++] = uint8_t(i); }
bool refresh_moon_phase_latch(GameState &g, TurnState &s, const SkyRefresh &sky, bool changed) {
    if (!changed || sky.location >= 0x21 || (g.position.map.floor & 255) >= 128) return false;
    const int64_t i = (int64_t(g.time.day) - 1) * 2;
    auto phase = [&](int64_t n) { return n >= 0 && uint64_t(n) < sky.count ? sky.phases[n] : 0x30; };
    s.felucca_phase = phase(i); s.trammel_phase = phase(i + 1);
    return true;
}
void advance_clock(GameState &g, TurnState &s, int32_t minutes, const Rand *rand, const SkyRefresh *sky) {
    if (minutes == 0) {
        if (sky) refresh_moon_phase_latch(g, s, *sky, g.time.hour != s.prev_hour);
        return;
    }
    if (minutes < 0) return;
    if (s.time_spell == 'Q') { minutes >>= 1; if (minutes == 0) minutes = 1; }
    s.prev_hour = g.time.hour;
    if (s.time_spell == 'T') return;
    auto &t = g.time;
    t.minute += minutes;
    g.torch_turns = uint16_t(std::max<int32_t>(0, int32_t(g.torch_turns) - minutes));
    if (s.light_spell_minutes) s.light_spell_minutes = std::max<int32_t>(0, s.light_spell_minutes - minutes);
    // Deliberately one carry, unlike time.ts:advanceMinutes.
    if (t.minute > 59) {
        t.minute -= 60; ++t.hour;
        if (t.hour > 23) {
            t.hour = 0;
            if (rand && s.has_shadowlords) {
                for (auto &loc : s.shadowlord_locations) {
                    if (loc >= 128 || loc < 0) continue; // -1 encodes absent array entry.
                    int32_t next;
                    do { next = (*rand)(1,8); }
                    while (next == g.position.map.location || std::find(s.shadowlord_locations.begin(), s.shadowlord_locations.end(), next) != s.shadowlord_locations.end());
                    loc = next;
                }
            }
            ++t.day;
            if (t.day > 28) {
                t.day = 1; s.skull_tree_day = 0; s.reagent_days = {};
                for (uint8_t i = 0; i < g.party.character_count; ++i)
                    if (g.party.characters[i].months_at_inn < 25) ++g.party.characters[i].months_at_inn;
                if (++t.month > 13) { t.month = 1; ++t.year; }
            }
        }
    }
    if (sky) refresh_moon_phase_latch(g, s, *sky, g.time.hour != s.prev_hour);
}
void apply_damage(GameState &g, int32_t i, int32_t amount) {
    if (i < 0 || i >= g.party.character_count) return;
    auto &ch = g.party.characters[i];
    const int64_t hp = int64_t(ch.current_hp) - amount;
    ch.current_hp = uint16_t(std::max<int64_t>(0, hp));
    if (hp <= 0) { ch.status = 'D'; if (g.party.active_character == i) g.party.active_character = 255; }
}
void party_random_damage(GameState &g, Rand rand) {
    for (int32_t i = 0; i < g.party.party_size && i < 6; ++i) {
        if (i < g.party.character_count && g.party.characters[i].status == 'D') continue;
        apply_damage(g, i, rand(1,8)); // Missing short-roster entries still consume a draw in TS.
    }
}
static void housekeeping_into(GameState &g, TurnState &s, Rand rand, TurnResult &out) {
    int32_t eaters = 0;
    const int32_t n = std::min<int32_t>({g.party.party_size, int32_t(g.party.character_count), 6});
    for (int32_t i = 0; i < n; ++i) {
        const char status = g.party.characters[i].status;
        if (status == 'D' || status == 'S') continue;
        if (status == 'P') { apply_damage(g,i,1); member(out.poison_ticks,i); }
        ++eaters;
    }
    if (g.time.hour != s.prev_hour) {
        if (g.food == 0) { message(out,TurnMessage::Starving); party_random_damage(g,rand); }
        else if (g.time.hour == 6 || g.time.hour == 12 || g.time.hour == 18) g.food = uint16_t(std::max<int32_t>(0,int32_t(g.food)-eaters));
        s.prev_hour = g.time.hour;
    }
    ++g.turns_since_start;
    if (s.time_spell && s.spell_turns > 0 && s.spell_turns != 255)
        if (--s.spell_turns == 0) s.time_spell = 0;
    for (int32_t i = 0; i < n; ++i) {
        auto &ch = g.party.characters[i];
        if (ch.status != 'D' && ch.ring == 44 && rand(0,7) == 7)
            ch.current_hp = uint16_t(std::min(int32_t(ch.current_hp)+1,int32_t(ch.max_hp)));
    }
}
TurnResult turn_housekeeping(GameState &g, TurnState &s, Rand rand) {
    TurnResult out; housekeeping_into(g,s,rand,out); return out;
}
TurnResult advance_turn(GameState &g, TurnState &s, int32_t minutes, Rand rand, const SkyRefresh *sky) {
    advance_clock(g,s,minutes,&rand,sky);
    return turn_housekeeping(g,s,rand);
}
bool maybe_change_wind(TurnState &s, Rand rand) {
    if (rand(0,63) != 0) return false;
    for (;;) {
        const int32_t w = rand(0,4);
        if (w != 0 || rand(0,255) >= 192) { s.wind = w; s.wind_drift_counter = 0; return true; }
    }
}
int32_t spawn_threshold(int32_t tile, int32_t floor, int32_t hour) {
    if (floor > 127) return 3;
    int32_t base = tile >= 32 && tile <= 38 ? 0 : tile == 4 || (tile >= 9 && tile <= 15) ? 2 : 1;
    if (hour >= 32 || hour < 5) base += 3;
    return base;
}
SpawnRoll roll_spawn_gate(Rand rand, int32_t tile, int32_t floor, int32_t hour) {
    const int32_t roll = rand(1,30), threshold = spawn_threshold(tile,floor,hour);
    return {roll,threshold,threshold > roll};
}
bool town_npc_tail_runs(const TurnState &s, TurnPhases &p, bool pass) {
    if (s.transport_tile >= 18 && s.transport_tile < 22 && !pass) { p.mount ^= 1; if (p.mount) return false; }
    if (s.time_spell == 'T') return false;
    if (s.time_spell == 'Q') { p.quickness ^= 1; if (p.quickness) return false; }
    return true;
}
bool outdoor_world_turn_runs(const GameState &g, const TurnState &s, TurnPhases &p) {
    if (s.time_spell == 'T') return false;
    if (s.time_spell == 'Q') { p.quickness ^= 1; if (p.quickness) return false; }
    if (g.transport == TransportMode::Horse || g.transport == TransportMode::Carpet) { p.mount ^= 1; if (p.mount) return false; }
    return true;
}
struct Tracer {
    Rand source; TraceSink sink; const char *site = "?";
    Rand rand() { return {this, [](void *p, int32_t lo, int32_t hi) {
        auto &t = *static_cast<Tracer *>(p); const auto value = t.source(lo,hi);
        if (t.sink.emit) t.sink.emit(t.sink.context,t.site,lo,hi,value);
        return value;
    }}; }
};
static void swamp(GameState &g, Rand rand, MemberIndices &out, bool town) {
    for (int32_t i = 0; i < g.party.party_size && i < g.party.character_count && i < 6; ++i) {
        auto &ch = g.party.characters[i];
        if (ch.status == 'D' || ch.status == 'P') continue;
        if (rand(town ? 0 : 1,town ? 29 : 30) > ch.dexterity) { ch.status = 'P'; member(out,i); }
    }
}
TurnResult outdoor_turn(GameState &g, TurnState &s, Rand source, const OutdoorTurnContext &ctx, TraceSink sink) {
    Tracer t{source,sink}; Rand rand = t.rand(); TurnResult r;
    if (s.time_spell != 'T') { t.site = "wind"; maybe_change_wind(s,rand); }
    ctx.after_wind();
    if (ctx.blocked) return r;
    advance_clock(g,s,ctx.minutes,&rand,ctx.sky);
    if (ctx.on_bridge) {
        r.has_troll = true; t.site = "troll";
        auto &b = r.troll;
        if (rand(0,7) == 0) {
            b.fired = true;
            if (g.transport == TransportMode::Foot) {
                b.on_foot = b.runs_inner_world_turn = true;
                if (s.time_spell != 'T') { t.site = "troll.wind"; maybe_change_wind(s,rand); t.site = "troll"; }
                const auto first = first_conscious_index(g.party);
                for (int32_t i = 0; i < g.party.party_size && i < g.party.character_count && i < 6; ++i) {
                    const auto &ch = g.party.characters[i];
                    if (ch.status == 'D' || ch.status == 'S') continue;
                    // Reference-faithful 1..30 vs. dexterity check (matches the original
                    // disassembly exactly). Max Dexterity (30) can never fail this roll,
                    // so a maxed-Dex character is legitimately immune to being chosen as
                    // the bridge-troll toll payer -- an incidental effect of Max Stats,
                    // not a gameplay bug. Do not change. See GAMEPLAY_INTEGRATION_AUDIT.md.
                    const auto roll = rand(1,30); b.dex_rolls[b.indices.count] = roll; member(b.indices,i);
                    if (ch.dexterity < roll) { b.payer_index = i; b.toll = 99 - 3*(first < 0 ? 0 : g.party.characters[first].strength); break; }
                }
            }
        }
    }
    if (ctx.on_swamp && g.transport == TransportMode::Foot) { t.site = "swamp"; swamp(g,rand,r.poisoned,false); }
    if (ctx.tile_under_party == 143) {
        if (s.time_spell != 'T') { t.site = "burnTick"; maybe_change_wind(s,rand); }
        r.burning = true; t.site = "burn"; party_random_damage(g,rand);
    }
    t.site = "hazard";
    if (g.position.map.floor != 0 && rand(0,255) == 105) { r.hazard = true; party_random_damage(g,rand); }
    t.site = "housekeeping"; housekeeping_into(g,s,rand,r);
    if (s.time_spell != 'T' && !ctx.skip_world_turn) { t.site = "spawn"; r.has_spawn = true; r.spawn = roll_spawn_gate(rand,ctx.tile_under_party,g.position.map.floor,g.time.hour); }
    return r;
}
TurnResult town_turn(GameState &g, TurnState &s, Rand source, const TownTurnContext &ctx, TraceSink sink) {
    Tracer t{source,sink}; Rand rand = t.rand(); TurnResult r;
    if (!ctx.pre_rolled && s.time_spell != 'T') { t.site = "wind"; maybe_change_wind(s,rand); }
    if (ctx.confused && !ctx.pre_rolled) {
        t.site = "confusion";
        if (rand(0,1) == 1) { s.drunk_turns = std::max<int32_t>(0,s.drunk_turns-1); rand(0,3); message(r,TurnMessage::Hic); }
    }
    if (!ctx.consumes_turn) return r;
    advance_clock(g,s,1,&rand,ctx.sky);
    for (int32_t i = 0; i < g.party.party_size && i < g.party.character_count && i < 6; ++i) {
        if (g.party.characters[i].status == 'S') { t.site = "wake"; if (rand(0,15) == 15) g.party.characters[i].status = 'G'; }
    }
    for (int32_t lap = 0;; ++lap) {
        const bool live = ctx.tile_under_party != nullptr;
        const int32_t tile = live ? ctx.tile_under_party(ctx.hazard_context) : -1;
        if (live && tile == 140 && (s.transport_tile & 254) != 20 && ctx.on_trapdoor) {
            message(r,TurnMessage::Trapdoor);
            if (ctx.on_trapdoor(ctx.hazard_context) == TrapdoorOutcome::Fell && lap < 8) continue;
            break;
        }
        if ((live ? tile == 4 : ctx.on_swamp_tile) && g.transport == TransportMode::Foot) { t.site = "swampTown"; swamp(g,rand,r.poisoned,true); }
        if (live ? tile == 188 || tile == 143 : ctx.damage_tile) {
            if (s.time_spell != 'T') { t.site = "damageTick"; maybe_change_wind(s,rand); }
            message(r,TurnMessage::Burning); t.site = "burn"; party_random_damage(g,rand);
        }
        break;
    }
    t.site = "housekeeping"; housekeeping_into(g,s,rand,r);
    TurnPhases fresh;
    if (town_npc_tail_runs(s,ctx.npc_phases ? *ctx.npc_phases : fresh,ctx.pass_command)) {
        ctx.after_housekeeping();
        if (ctx.second_world_turn && s.time_spell != 'T') { t.site = "wind2"; maybe_change_wind(s,rand); }
    }
    return r;
}
}
