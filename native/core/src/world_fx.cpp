#include "openu5/world_fx.h"

#include <cstdlib>

// Line-by-line mirror of the reference `WorldFxLayer` (game/src/skin/world-fx.ts).
// The derivation lives in the header; only the arithmetic lives here.
namespace openu5 {
namespace {
int abs16(int v) { return v < 0 ? -v : v; }
} // namespace

bool WorldFxLayer::push(const WorldFx &fx, uint32_t now_ms) {
    if (count_ >= kWorldFxSlots) return false;
    live_[count_].fx = fx;
    live_[count_].t0 = now_ms;
    ++count_;
    return true;
}

int WorldFxLayer::projectile_cells(const WorldFx &fx) {
    const int x = abs16(int(fx.to_dx) - int(fx.from_dx));
    const int y = abs16(int(fx.to_dy) - int(fx.from_dy));
    const int chebyshev = x > y ? x : y;
    return chebyshev > 1 ? chebyshev : 1;
}

uint32_t WorldFxLayer::duration_ms(const WorldFx &fx) {
    if (fx.kind == WorldFxKind::Projectile)
        return uint32_t(projectile_cells(fx)) * kWorldFxProjectileMsPerCell;
    const int32_t bursts = fx.bursts > 0 ? fx.bursts : 0;
    const int32_t pause = fx.pre_delay_units > 0 ? fx.pre_delay_units : 0;
    return fx.lead_ms + uint32_t(pause) * kWorldFxPauseUnitMs +
           uint32_t(bursts) * kWorldFxExplosionBurstMs;
}

bool WorldFxLayer::projectile_at(uint32_t now_ms, int32_t &dx_milli,
                                 int32_t &dy_milli) const {
    for (size_t i = 0; i < count_; ++i) {
        const auto &fx = live_[i].fx;
        if (fx.kind != WorldFxKind::Projectile) continue;
        const uint32_t duration = duration_ms(fx);
        if (!duration) continue;
        const uint32_t dt = now_ms - live_[i].t0;
        if (int32_t(dt) < 0 || dt >= duration) continue;
        // Integer form of the reference's `from + (to - from) * dt / dur`.
        const int32_t span = int32_t(dt) * kWorldFxMilliCell / int32_t(duration);
        dx_milli = int32_t(fx.from_dx) * kWorldFxMilliCell +
                   (int32_t(fx.to_dx) - int32_t(fx.from_dx)) * span;
        dy_milli = int32_t(fx.from_dy) * kWorldFxMilliCell +
                   (int32_t(fx.to_dy) - int32_t(fx.from_dy)) * span;
        return true;
    }
    return false;
}

uint32_t WorldFxLayer::remaining_ms(uint32_t now_ms) const {
    uint32_t remaining = 0;
    for (size_t i = 0; i < count_; ++i) {
        const uint32_t duration = duration_ms(live_[i].fx);
        const uint32_t dt = now_ms - live_[i].t0;
        if (dt >= duration) continue;
        const uint32_t left = duration - dt;
        if (left > remaining) remaining = left;
    }
    return remaining;
}

size_t WorldFxLayer::paint(uint32_t now_ms, WorldFxOp *out, size_t capacity) {
    size_t written = 0;
    size_t survivors = 0;
    auto push_op = [&](const WorldFxOp &op) {
        if (out && written < capacity) out[written] = op;
        ++written;
    };
    for (size_t i = 0; i < count_; ++i) {
        const auto item = live_[i];
        const auto &fx = item.fx;
        const uint32_t duration = duration_ms(fx);
        const uint32_t dt = now_ms - item.t0;
        if (dt >= duration) continue; // expired
        live_[survivors++] = item;
        if (fx.kind == WorldFxKind::Projectile) {
            int32_t dx_milli = 0, dy_milli = 0;
            if (projectile_at(now_ms, dx_milli, dy_milli)) {
                WorldFxOp op;
                op.kind = WorldFxOpKind::Dot;
                op.dx_milli = dx_milli;
                op.dy_milli = dy_milli;
                push_op(op);
            }
            continue;
        }
        // The tile the original keeps in the cell for the WHOLE choreography
        // (the Shadowlord, in the ritual). It goes first so the burst lands ON
        // TOP of it, not beside it. Banked here and not in the emitter: the
        // descriptor carries the raw slot byte, and the high bank is render
        // space -- the same split the binary makes.
        if (fx.under_tile) {
            WorldFxOp op;
            op.kind = WorldFxOpKind::Blit;
            op.tile = int16_t(fx.under_tile + kWorldFxSpriteBank);
            op.dx = fx.dx;
            op.dy = fx.dy;
            op.dx_milli = int32_t(fx.dx) * kWorldFxMilliCell;
            op.dy_milli = int32_t(fx.dy) * kWorldFxMilliCell;
            push_op(op);
        }
        if (dt < fx.lead_ms) continue; // waiting on the shake / blocking audio
        const uint32_t after_lead = dt - fx.lead_ms;
        const uint32_t pause_ms =
            uint32_t(fx.pre_delay_units > 0 ? fx.pre_delay_units : 0) * kWorldFxPauseUnitMs;
        if (after_lead < pause_ms) continue; // still in the pause: silence, no paint
        const uint32_t after_pause = after_lead - pause_ms;
        // The blits alternate presence and absence: the original repaints the
        // viewport between one and the next, so the cell FLICKERS. Painting all
        // seven back to back would be a tile held still for 420 ms.
        const uint32_t index = after_pause / kWorldFxExplosionBurstMs;
        if (index % 2 == 0) {
            WorldFxOp op;
            op.kind = WorldFxOpKind::Blit;
            op.tile = kWorldFxExplosionTile;
            op.dx = fx.dx;
            op.dy = fx.dy;
            op.dx_milli = int32_t(fx.dx) * kWorldFxMilliCell;
            op.dy_milli = int32_t(fx.dy) * kWorldFxMilliCell;
            push_op(op);
        }
    }
    count_ = survivors;
    return written;
}

size_t apply_world_fx(PresentationSnapshot &snapshot, const WorldFxOp *ops, size_t count) {
    if (!ops) return 0;
    constexpr int half = kPresentationWindow / 2;
    size_t applied = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto &op = ops[i];
        if (op.kind != WorldFxOpKind::Blit) continue; // the dot lives in pixel space
        const int col = half + int(op.dx), row = half + int(op.dy);
        if (col < 0 || row < 0 || col >= kPresentationWindow || row >= kPresentationWindow)
            continue; // clipped by the viewport, as the original's blit is
        const int at = row * kPresentationWindow + col;
        snapshot.tiles[at] = op.tile;
        // The fx is drawn regardless of the light radius: the original blits
        // straight into the window without consulting the censorship bitmap.
        snapshot.visible[at] = 1;
        const auto kind = tile_animation_kind(op.tile);
        const bool animated = kind != TileAnimationKind::Static &&
                              kind != TileAnimationKind::ActorProgram;
        snapshot.animated[at] = animated ? 1 : 0;
        snapshot.any_animated = snapshot.any_animated || animated;
        ++applied;
    }
    return applied;
}

} // namespace openu5
