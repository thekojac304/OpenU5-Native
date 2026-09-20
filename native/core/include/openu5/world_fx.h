#pragma once

#include <cstddef>
#include <cstdint>

#include "presentation.h"

// Y-04 -- the WORLD FX channel: an ephemeral paint over a single world cell,
// outside combat. Native mirror of game/src/skin/world-fx.ts, which is the
// reference module that owns both of the channels implemented here.
//
// The two channels share one primitive on purpose, because the reference
// shares it: a live list of fx, each with its own start instant, drained by a
// painter that receives CELL OFFSETS relative to the party (the presentation
// snapshot carries no absolute position, and the binary expresses the cell the
// same way). They are NOT two rendering systems.
//
// DERIVATION
//
//   CellExplosion -- CAST.OVL 0x16f4 -> `explosion_fx_at_cell`
//   (ULTIMA.EXE 0x3522): `bursts` times over the cell, blit of tile 0 (named
//   `Explosion` in TileData) + noise_burst + viewport_redraw. The repeat count
//   (7 in the shard ritual, 1 in the Blackthorn sacrifice) and the cell are
//   DERIVED; the cadence between blits is Class C -- it is not a readable
//   constant in the binary, it is whatever `viewport_redraw` costs per frame.
//   `under_tile` (#243) is the slot byte the cell KEEPS SHOWING for the whole
//   choreography: the binary writes its state changes AFTER the seven
//   explosions (CAST 0x1708 follows 0x16e1-0x16fa), while native's
//   `remove_shadowlord_at` commits synchronously and the snapshot the device
//   paints from is already mutated. Without it the cell would show the Flame
//   from the first frame. This is presentation only -- no state, no RNG, no
//   deferred mutation.
//
//   CellProjectile -- CMDS.OVL, one flight PER SHOT (never per cell):
//   `push origX / origY / dstX / dstY / 1` -> `call 0xffffbc6a` at all three
//   call sites (broadside with impact 0x0A45, broadside without 0x0AD2, cannon
//   on foot 0x0CFE). The ORIGIN differs between surfaces: a broadside pushes
//   `5 / 5` (0x0A48-0x0A49), the window centre, so the ball leaves the SHIP at
//   offset (0,0); on foot the origin is `dir+-1 + 5` (0x0BAD/0x0BB9) and is
//   never rewritten in the loop, so the ball leaves the CANNON's cell.
//   The target segment of the far call relocates on load and is opaque to
//   static reading (#311/#313), so the FRAME CADENCE is not derivable: the
//   55 ms/cell below is borrowed from the already-calibrated combat projectile
//   and is declared Class C, exactly as the reference declares it.
//
// Nothing here reads GameState, TurnState or any RNG.
namespace openu5 {

/** Tile blitted by `explosion_fx_at_cell` (TileData names it `Explosion`). */
constexpr int16_t kWorldFxExplosionTile = 0;

/**
 * Unit of the `0x3ae6` pauses -- the SAME calibration the troll toll script
 * and every other native scene pacer uses (one INT 1Ch tick). Reused rather
 * than re-invented: if that calibration is ever corrected, both move together.
 */
constexpr uint32_t kWorldFxPauseUnitMs = 55;

/** Duration of one burst blit (declared Class C: `viewport_redraw`'s rhythm). */
constexpr uint32_t kWorldFxExplosionBurstMs = 60;

/**
 * CANNON FLIGHT CADENCE -- Class C TAKEN ON LOAN, NOT DERIVED (#313). These
 * are the 55 ms/cell of the COMBAT projectile, borrowed because it is the
 * sibling effect that already has a witness. The NUMBER is copied, not the
 * accreditation: it sits loose here on purpose so that the day a witness for
 * the cannon appears it can be recalibrated without touching combat's.
 */
constexpr uint32_t kWorldFxProjectileMsPerCell = 55;

/** Side of the projectile glyph, in logical 320x200 px. Presentation only. */
constexpr int kWorldFxProjectileDotPx = 4;

/**
 * Sprite bank offset. `under_tile` travels RAW (the slot byte -- 0xFC for a
 * Shadowlord), never a banked index, exactly as the reference sends it; this
 * layer banks it when it emits the paint op, which is the same split the
 * binary makes (the slot holds 0xFC, the cell blit reads the mobile bank).
 */
constexpr int16_t kWorldFxSpriteBank = 0x100;

/** Fixed-point scale of a projectile's sub-cell position. */
constexpr int32_t kWorldFxMilliCell = 1000;

enum class WorldFxKind : uint8_t { Explosion, Projectile };

/** One fx, as the core's event describes it. */
struct WorldFx {
    WorldFxKind kind = WorldFxKind::Explosion;
    // Explosion: the cell, as an offset from the party (the binary says
    // `(party_x, party_y - 1)` in CAST 0x16ad; that is (0,-1) here).
    int16_t dx = 0, dy = 0;
    int16_t bursts = 0;
    int16_t pre_delay_units = 0;
    /** Raw slot byte still shown under the burst; 0 = none (see header). */
    int16_t under_tile = 0;
    /**
     * Wait, in ms, BEFORE the `pre_delay_units` pause (#243): everything the
     * binary does between the keypress and the first `explosion_fx_at_cell`
     * that cannot be expressed in `0x3ae6` units because it belongs to another
     * clock -- in native, the live viewport shake. The owner computes it,
     * because only the owner knows how much of its own quake window is left.
     * The fx is ALIVE throughout the wait: that is what holds `under_tile` on
     * the cell.
     */
    uint32_t lead_ms = 0;
    // Projectile: origin and destination, as offsets from the party.
    int16_t from_dx = 0, from_dy = 0, to_dx = 0, to_dy = 0;
};

enum class WorldFxOpKind : uint8_t {
    /** Replace one cell's tile for this frame only. `dx/dy` are whole cells. */
    Blit,
    /** Mark a point at FRACTIONAL cell coordinates (the ball flies between
     *  cells, so a cell blit cannot express it). */
    Dot,
};

/** One paint instruction for one frame. Never mutates the world tile. */
struct WorldFxOp {
    WorldFxOpKind kind = WorldFxOpKind::Blit;
    int16_t tile = 0;                        // Blit: already banked.
    int32_t dx_milli = 0, dy_milli = 0;      // 1000 = one cell.
    int16_t dx = 0, dy = 0;                  // Blit: the same offset, whole.
};

/** Concurrent fx the layer holds. The ritual needs one; the headroom is cheap. */
constexpr size_t kWorldFxSlots = 8;

/**
 * Stateful layer: keeps the live fx with their start instants and expires them
 * by itself. It knows no atlas -- the owner turns the ops into pixels, so this
 * file is coupled to no renderer and is fully host-testable.
 */
class WorldFxLayer {
  public:
    /** Returns false when every slot is busy (the fx is then dropped). */
    bool push(const WorldFx &, uint32_t now_ms);

    bool active() const { return count_ != 0; }
    size_t live_count() const { return count_; }
    void clear() { count_ = 0; }

    /** Cells a flight crosses (Chebyshev, at least 1 so nothing divides by 0). */
    static int projectile_cells(const WorldFx &);

    /** Total ms an fx occupies from its start (lead and pause included). */
    static uint32_t duration_ms(const WorldFx &);

    /**
     * Interpolated position of the live flight, in milli-cells. PURE: it
     * neither expires nor advances anything, so a second consumer (a shader
     * pass, a test) can read it without stealing the layer's own frame.
     */
    bool projectile_at(uint32_t now_ms, int32_t &dx_milli, int32_t &dy_milli) const;

    /** ms until the layer is idle again; 0 when it already is. */
    uint32_t remaining_ms(uint32_t now_ms) const;

    /**
     * Emit this frame's ops and expire what has run out. Layer order is the
     * binary's own: `under_tile` FIRST (what the cell still holds --
     * `viewport_redraw` repaints it between blit and blit), the burst ON TOP.
     * Returns the number of ops written; ops beyond `capacity` are not written.
     */
    size_t paint(uint32_t now_ms, WorldFxOp *out, size_t capacity);

  private:
    struct Live {
        WorldFx fx{};
        uint32_t t0 = 0;
    };
    Live live_[kWorldFxSlots]{};
    size_t count_ = 0;
};

/**
 * Composite the frame's Blit ops into an already-composed presentation window.
 * TEMPORARY, per frame and per cell: it overwrites the snapshot the renderer
 * is about to rasterize and NEVER touches the world tile, the object table or
 * any state -- which is the whole reason this channel exists rather than a
 * mutation. `dx/dy` are offsets from the window's centre (where the party is).
 * Ops that fall outside the 11x11 window are clipped away, exactly as the
 * original's cell blit is clipped by the viewport it draws into.
 * Returns how many ops actually landed.
 */
size_t apply_world_fx(PresentationSnapshot &, const WorldFxOp *, size_t count);

} // namespace openu5
