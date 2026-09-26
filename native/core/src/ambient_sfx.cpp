#include "openu5/ambient_sfx.h"

// Alpha 3 A3-03. ambient_sfx_tick 0x4102, branch for branch.
namespace openu5 {

uint8_t ambient_tile_class(int32_t tile) {
    if (tile < 0 || tile > 0xff) return 0;
    if ((tile & 0xfe) == 0xfa) return 1; // 0x41d0: Clock 0xfa / 0xfb
    if ((tile & 0xfc) == 0xd4) return 2; // 0x41df: Waterfall 0xd4..0xd7
    if ((tile & 0xfc) == 0xd8) return 3; // 0x41ed: Fountain 0xd8..0xdb
    return 0;                            // class 4 (a bard sprite, 0x41f8) is not ported
}

uint8_t ambient_nearest_class(const AmbientWindow &w) {
    constexpr int c = AmbientWindow::kSide / 2;
    int best = 0x33; // 0x410f: [bp-2] = 0x33; the farthest corner (50) still counts
    uint8_t cls = 0;
    for (int x = 0; x < AmbientWindow::kSide; ++x)       // x outer
        for (int y = 0; y < AmbientWindow::kSide; ++y) { // y inner
            const int d = (x - c) * (x - c) + (y - c) * (y - c);
            if (d >= best) continue; // 0x41b8 jge: only strictly nearer
            const uint8_t k = ambient_tile_class(w.tiles[y * AmbientWindow::kSide + x]);
            if (!k) continue;
            best = d;
            cls = k;
        }
    return cls;
}

uint8_t ambient_chime_hour(uint8_t hour) { return hour == 0 ? 12 : hour > 12 ? uint8_t(hour - 12) : hour; }

SfxId AmbientTicker::tick(uint8_t nearest_class) {
    SfxId cue = SfxId::None;
    switch (nearest_class) {
    case 1: // 0x4262
        if (chimes_ && (phase_ == 0 || phase_ == 4)) cue = SfxId::AmbientClockChime; // 0x4277
        else if (phase_ == 0) cue = SfxId::AmbientClockTick;                         // 0x4299
        else if (phase_ == 4) cue = SfxId::AmbientClockTock;                         // 0x42ad
        break;
    case 2: cue = SfxId::AmbientWaterfall; break; // 0x42b2, no phase gate
    case 3: cue = SfxId::AmbientFountain; break;  // 0x42c4, no phase gate
    default: break;
    }
    if (chimes_ && (phase_ == 0 || phase_ == 4)) --chimes_; // 0x430e-0x4323, whatever the class
    phase_ = uint8_t((phase_ + 1) & 7);                      // 0x4327-0x4337
    return cue;
}

} // namespace openu5
