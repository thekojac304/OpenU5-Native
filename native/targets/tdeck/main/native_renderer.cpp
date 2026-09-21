#include "native_renderer.h"

#include "openu5/dungeon_view.h"
#include "openu5/gem_view.h"
#include "openu5/world_fx.h"

#include <algorithm>
#include <array>

namespace openu5 {
namespace {

constexpr int kHalfViewport = kViewportTiles / 2;
constexpr size_t kMaxRequiredTiles = kViewportTiles * kViewportTiles + 1;

uint8_t pixel4(const uint8_t *tile, int pixel)
{
    const uint8_t packed = tile[pixel / 2];
    return (pixel & 1) ? packed & 0x0f : packed >> 4;
}

void set_pixel4(uint8_t *tile, int pixel, uint8_t value)
{
    auto &packed = tile[pixel / 2];
    if (pixel & 1) packed = uint8_t((packed & 0xf0) | (value & 0x0f));
    else packed = uint8_t((packed & 0x0f) | ((value & 0x0f) << 4));
}

bool fire_mask(uint16_t tile, uint16_t &mask)
{
    switch (tile) {
    case 0xb0: mask=0xc0; return true; case 0xb1: mask=0xc1; return true;
    case 0xb2: mask=0xc2; return true; case 0xb3: mask=0xc3; return true;
    case 0xbc: mask=0xcc; return true; case 0xbd: mask=0xcd; return true;
    case 0xbe: mask=0xce; return true; case 0xbf: mask=0xcf; return true;
    case 0xde: mask=0xc2; return true; default: return false;
    }
}

uint8_t fire_nibble(uint16_t &seed)
{
    seed=uint16_t(seed+0x9248);seed=uint16_t((seed>>3)|(seed<<13));
    seed=uint16_t(seed^0x9248);seed=uint16_t(seed+0x11);return uint8_t(seed&15);
}

bool composite_mask(uint16_t tile,uint16_t &mask)
{
    if(tile>=0x60&&tile<=0x6f){mask=uint16_t(tile+0x10);return true;}
    if(tile>=0x34&&tile<=0x37){mask=uint16_t(0xd0+tile-0x34);return true;}
    if(tile>=0xe4&&tile<=0xe7){mask=uint16_t(0xd0+tile-0xe4);return true;}
    return false;
}

void animated_bitmap(const PresentationTileCache &cache,uint16_t tile,uint32_t tick,
                     uint8_t (&out)[128])
{
    const uint8_t *base=cache.tiles+size_t(tile)*128;std::copy(base,base+128,out);
    if(tile==1||tile==2||tile==3||tile==0x8f){const int shift=int(tick&15);for(int row=0;row<16;++row){const int src=(row-shift+16)&15;std::copy(base+src*8,base+src*8+8,out+row*8);}return;}
    uint16_t mask_tile=0;
    if(composite_mask(tile,mask_tile)){
        uint8_t water[128]{};const uint8_t *water_base=cache.tiles+3*128;const int shift=int(tick&15);
        for(int row=0;row<16;++row){const int src=(row-shift+16)&15;std::copy(water_base+src*8,water_base+src*8+8,water+row*8);}
        const uint8_t *mask=cache.tiles+size_t(mask_tile)*128;
        for(int p=0;p<256;++p)if(pixel4(mask,p)&8)set_pixel4(out,p,pixel4(water,p));
        return;
    }
    if(fire_mask(tile,mask_tile)){
        const uint8_t *mask=cache.tiles+size_t(mask_tile)*128;uint16_t seed=uint16_t(0x1f94U^(tick*257U)^tile);
        for(int p=0;p<256;++p){const auto m=pixel4(mask,p);if(m)set_pixel4(out,p,uint8_t(pixel4(base,p)^(fire_nibble(seed)&m)));}
    }
}

uint32_t crc32_u16le(const uint16_t *pixels, size_t count)
{
    uint32_t crc = 0xffffffffU;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t bytes[] = {static_cast<uint8_t>(pixels[i]),
                                 static_cast<uint8_t>(pixels[i] >> 8)};
        for (uint8_t value : bytes) {
            crc ^= value;
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
            }
        }
    }
    return crc ^ 0xffffffffU;
}

void expand_tile(const uint8_t (&indexed4)[128], const uint16_t (&palette)[16],
                 uint16_t *viewport, int cell_x, int cell_y)
{
    const int origin_x = cell_x * kTilePixels;
    const int origin_y = cell_y * kTilePixels;
    for (int y = 0; y < kTilePixels; ++y) {
        uint16_t *destination = viewport + (origin_y + y) * kViewportPixels + origin_x;
        for (int x_pair = 0; x_pair < kTilePixels / 2; ++x_pair) {
            const uint8_t packed = indexed4[y * (kTilePixels / 2) + x_pair];
            destination[x_pair * 2] = palette[packed >> 4];
            destination[x_pair * 2 + 1] = palette[packed & 0x0f];
        }
    }
}

constexpr uint16_t kDungeonBlack = 0x0000;

void dungeon_pixel(uint16_t *pixels, int x, int y, uint16_t color, uint16_t &primitives) {
    if (x < 0 || y < 0 || x >= kViewportPixels || y >= kViewportPixels) return;
    pixels[y * kViewportPixels + x] = color;
    ++primitives;
}

void dungeon_rect(uint16_t *pixels, int left, int top, int right, int bottom,
                  uint16_t color, uint16_t &primitives) {
    left = std::max(0, left); top = std::max(0, top);
    right = std::min(kViewportPixels - 1, right); bottom = std::min(kViewportPixels - 1, bottom);
    for (int y = top; y <= bottom; ++y)
        for (int x = left; x <= right; ++x) dungeon_pixel(pixels, x, y, color, primitives);
}

void dungeon_line(uint16_t *pixels, int x0, int y0, int x1, int y1,
                  uint16_t color, uint16_t &primitives) {
    const int dx = x0 < x1 ? x1 - x0 : x0 - x1;
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = y0 < y1 ? y0 - y1 : y1 - y0;
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        dungeon_pixel(pixels, x0, y0, color, primitives);
        if (x0 == x1 && y0 == y1) break;
        const int doubled = error * 2;
        if (doubled >= dy) { error += dy; x0 += sx; }
        if (doubled <= dx) { error += dx; y0 += sy; }
    }
}

void dungeon_report(RenderReport &report, const DungeonState &d, uint16_t *pixels) {
    report.left = 0; report.top = 0; report.right = 7; report.bottom = 7;
    report.center_map_tile = dungeon_cell(d, d.pos.floor, d.pos.x, d.pos.y);
    report.avatar_tile = 0;
    report.viewport_bytes = uint32_t(kViewportPixelCount * sizeof(uint16_t));
    report.viewport_crc32 = crc32_u16le(pixels, kViewportPixelCount);
}

void marker_pixel(uint16_t *viewport,int cell_x,int cell_y,int x,int y,uint16_t color)
{
    const int px=cell_x*kTilePixels+x,py=cell_y*kTilePixels+y;
    if(px>=0&&py>=0&&px<kViewportPixels&&py<kViewportPixels)
        viewport[py*kViewportPixels+px]=color;
}

void active_marker(uint16_t *viewport,int cell_x,int cell_y,uint16_t color)
{
    // Four heavy corners remain legible over animated sprites without hiding
    // the combatant.  This is intentionally not the targeting shape.
    for(int n=1;n<=5;++n)for(int thickness=0;thickness<2;++thickness){
        marker_pixel(viewport,cell_x,cell_y,n,thickness,color);
        marker_pixel(viewport,cell_x,cell_y,15-n,thickness,color);
        marker_pixel(viewport,cell_x,cell_y,n,15-thickness,color);
        marker_pixel(viewport,cell_x,cell_y,15-n,15-thickness,color);
        marker_pixel(viewport,cell_x,cell_y,thickness,n,color);
        marker_pixel(viewport,cell_x,cell_y,15-thickness,n,color);
        marker_pixel(viewport,cell_x,cell_y,thickness,15-n,color);
        marker_pixel(viewport,cell_x,cell_y,15-thickness,15-n,color);
    }
}

void target_reticle(uint16_t *viewport,int cell_x,int cell_y,uint16_t color)
{
    // Inset box plus center tick: visually distinct from the active-actor
    // corner brackets and still exposes most of the target sprite.
    for(int n=3;n<=12;++n){
        marker_pixel(viewport,cell_x,cell_y,n,3,color);
        marker_pixel(viewport,cell_x,cell_y,n,12,color);
        marker_pixel(viewport,cell_x,cell_y,3,n,color);
        marker_pixel(viewport,cell_x,cell_y,12,n,color);
    }
    for(int n=6;n<=9;++n){
        marker_pixel(viewport,cell_x,cell_y,n,7,color);
        marker_pixel(viewport,cell_x,cell_y,7,n,color);
    }
}

}  // namespace

esp_err_t initialize_tile_cache(AssetPackReader &assets,const AssetPackReport &pack,
                                uint8_t *storage,size_t storage_size,PresentationTileCache &cache)
{
    cache={};if(!assets.is_open()||pack.tile_count!=512||!storage||storage_size<kCachedTileBytes)return ESP_ERR_INVALID_ARG;
    if(assets.read_palette(cache.palette)!=ESP_OK)return ESP_FAIL;
    for(uint16_t tile=0;tile<512;++tile){uint8_t record[128]{};if(assets.read_tile(tile,record)!=ESP_OK)return ESP_FAIL;std::copy(record,record+128,storage+size_t(tile)*128);}
    cache.tiles=storage;return ESP_OK;
}

esp_err_t render_view(AssetPackReader &assets, const AssetPackReport &pack,
                      uint8_t center_x, uint8_t center_y, uint16_t *rgb565,
                      size_t pixel_count, RenderReport &report)
{
    report = {};
    if (!assets.is_open() || rgb565 == nullptr || pixel_count < kViewportPixelCount ||
        pack.world_width != 256 || pack.world_height != 256 ||
        pack.initial_map_width != 32 || pack.initial_map_height != 32 || pack.tile_count != 512 ||
        pack.avatar_tile >= pack.tile_count) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t palette[16]{};
    if (assets.read_palette(palette) != ESP_OK) return ESP_FAIL;

    std::array<uint8_t, kViewportTiles * kViewportTiles> map_tiles{};
    const bool britannia = pack.initial_location == 0 && pack.initial_floor == 0;
    const bool iolos_hut = pack.initial_location == 13 && pack.initial_floor == 0;
    if (!britannia && !iolos_hut) return ESP_ERR_NOT_SUPPORTED;

    if (britannia) {
        report.map_context = "Britannia surface (256x256 wrapped)";
        for (int row = 0; row < kViewportTiles; ++row) {
            const uint8_t map_y = static_cast<uint8_t>(center_y - kHalfViewport + row);
            const uint8_t map_x = static_cast<uint8_t>(center_x - kHalfViewport);
            const size_t first = std::min<size_t>(kViewportTiles, 256U - map_x);
            uint8_t *destination = &map_tiles[row * kViewportTiles];
            if (assets.read_world_span(map_y, map_x, destination, first) != ESP_OK) return ESP_FAIL;
            if (first < kViewportTiles &&
                assets.read_world_span(map_y, 0, destination + first, kViewportTiles - first) != ESP_OK) {
                return ESP_FAIL;
            }
        }
    } else {
        report.map_context = "Iolo's Hut (location 13 floor 0, 32x32 local)";
        uint8_t edge_fill = 0;
        if (assets.read_initial_map_span(31, 31, &edge_fill, 1) != ESP_OK) return ESP_FAIL;
        map_tiles.fill(edge_fill);
        const int left = static_cast<int>(center_x) - kHalfViewport;
        for (int row = 0; row < kViewportTiles; ++row) {
            const int map_y = static_cast<int>(center_y) - kHalfViewport + row;
            if (map_y < 0 || map_y >= 32) continue;
            const int first_x = std::max(0, left);
            const int last_x = std::min(31, left + kViewportTiles - 1);
            if (first_x > last_x) continue;
            const size_t count = static_cast<size_t>(last_x - first_x + 1);
            uint8_t *destination = &map_tiles[row * kViewportTiles + (first_x - left)];
            if (assets.read_initial_map_span(static_cast<uint8_t>(map_y),
                                             static_cast<uint8_t>(first_x), destination,
                                             count) != ESP_OK) {
                return ESP_FAIL;
            }
        }
    }

    std::array<uint16_t, kMaxRequiredTiles> required{};
    size_t required_count = 0;
    const auto add_required = [&](uint16_t tile_id) {
        for (size_t i = 0; i < required_count; ++i) {
            if (required[i] == tile_id) return;
        }
        required[required_count++] = tile_id;
    };
    for (uint8_t tile : map_tiles) add_required(tile);
    add_required(pack.avatar_tile);

    uint8_t indexed4[128]{};
    for (size_t required_index = 0; required_index < required_count; ++required_index) {
        const uint16_t tile_id = required[required_index];
        if (assets.read_tile(tile_id, indexed4) != ESP_OK) return ESP_FAIL;
        ++report.tile_records_read;
        for (int row = 0; row < kViewportTiles; ++row) {
            for (int col = 0; col < kViewportTiles; ++col) {
                if (map_tiles[row * kViewportTiles + col] == tile_id) {
                    expand_tile(indexed4, palette, rgb565, col, row);
                }
            }
        }
        if (tile_id == pack.avatar_tile) {
            // CoreView's faithful ordering replaces the complete center tile last.
            expand_tile(indexed4, palette, rgb565, kHalfViewport, kHalfViewport);
        }
    }

    report.left = static_cast<int16_t>(center_x) - kHalfViewport;
    report.top = static_cast<int16_t>(center_y) - kHalfViewport;
    report.right = static_cast<int16_t>(center_x) + kHalfViewport;
    report.bottom = static_cast<int16_t>(center_y) + kHalfViewport;
    report.center_map_tile = map_tiles[kHalfViewport * kViewportTiles + kHalfViewport];
    report.avatar_tile = pack.avatar_tile;
    report.viewport_crc32 = crc32_u16le(rgb565, kViewportPixelCount);
    return ESP_OK;
}

esp_err_t render_active_view(AssetPackReader &assets, const AssetPackReport &pack,
                             const ActiveMap &map, Position center, uint16_t avatar_tile,
                             uint16_t *rgb565, size_t pixel_count, RenderReport &report)
{
    report = {};
    if (!assets.is_open() || !rgb565 || pixel_count < kViewportPixelCount ||
        pack.tile_count != 512 || avatar_tile >= pack.tile_count) return ESP_ERR_INVALID_ARG;
    uint16_t palette[16]{}; if (assets.read_palette(palette) != ESP_OK) return ESP_FAIL;
    std::array<uint16_t,kViewportTiles*kViewportTiles> map_tiles{};
    for(int row=0;row<kViewportTiles;++row)for(int col=0;col<kViewportTiles;++col){
        int x=int(center.x)-kHalfViewport+col,y=int(center.y)-kHalfViewport+row;
        if(map.geometry.wraps){x=wrap_coord(x);y=wrap_coord(y);}
        int tile=map.tile_at(x,y);map_tiles[size_t(row*kViewportTiles+col)]=uint16_t(tile<0?0:tile&511);
    }
    std::array<uint16_t,kMaxRequiredTiles> required{};size_t required_count=0;
    auto add=[&](uint16_t tile){for(size_t i=0;i<required_count;++i)if(required[i]==tile)return;required[required_count++]=tile;};
    for(auto tile:map_tiles)
        add(tile);
    add(avatar_tile);uint8_t indexed4[128]{};
    for(size_t i=0;i<required_count;++i){if(assets.read_tile(required[i],indexed4)!=ESP_OK)return ESP_FAIL;++report.tile_records_read;
        for(int row=0;row<kViewportTiles;++row)for(int col=0;col<kViewportTiles;++col)if(map_tiles[size_t(row*kViewportTiles+col)]==required[i])expand_tile(indexed4,palette,rgb565,col,row);
        if(required[i]==avatar_tile)expand_tile(indexed4,palette,rgb565,kHalfViewport,kHalfViewport);
    }
    report.left=int16_t(int(center.x)-kHalfViewport);report.top=int16_t(int(center.y)-kHalfViewport);
    report.right=int16_t(int(center.x)+kHalfViewport);report.bottom=int16_t(int(center.y)+kHalfViewport);
    report.center_map_tile=uint8_t(map_tiles[kHalfViewport*kViewportTiles+kHalfViewport]);report.avatar_tile=avatar_tile;
    report.viewport_crc32=crc32_u16le(rgb565,kViewportPixelCount);report.map_context=map.kind==MapKind::Small?"small map":map.kind==MapKind::Underworld?"underworld":"Britannia";return ESP_OK;
}

esp_err_t render_snapshot(const PresentationTileCache &cache,const PresentationSnapshot &snapshot,
                          uint32_t animation_tick,int64_t world_turn,uint16_t *rgb565,
                          size_t pixel_count,RenderReport &report)
{
    report={};if(!cache.tiles||!rgb565||pixel_count<kViewportPixelCount)return ESP_ERR_INVALID_ARG;
    std::fill(rgb565,rgb565+kViewportPixelCount,uint16_t(0));
    std::array<uint16_t,kMaxRequiredTiles> required{};size_t count=0;
    auto add=[&](uint16_t tile){for(size_t i=0;i<count;++i)if(required[i]==tile)return;required[count++]=tile;};
    int16_t frames[kPresentationCells]{};
    for(int i=0;i<kPresentationCells;++i){const int raw=snapshot.tiles[i];if(raw<0){frames[i]=int16_t(raw);continue;}const int frame=animated_tile_frame(raw,animation_tick,world_turn);frames[i]=int16_t(frame);add(uint16_t(frame));report.animated_cells[i]=snapshot.animated[i];report.animated_cell_count=uint16_t(report.animated_cell_count+(snapshot.animated[i]?1U:0U));}
    uint8_t bitmap[128]{};
    for(size_t n=0;n<count;++n){animated_bitmap(cache,required[n],animation_tick,bitmap);for(int row=0;row<kViewportTiles;++row)for(int col=0;col<kViewportTiles;++col)if(frames[row*kViewportTiles+col]==int16_t(required[n]))expand_tile(bitmap,cache.palette,rgb565,col,row);}
    if(snapshot.active_x>=0&&snapshot.active_y>=0&&snapshot.active_x<kViewportTiles&&snapshot.active_y<kViewportTiles)
        active_marker(rgb565,snapshot.active_x,snapshot.active_y,
                      cache.palette[snapshot.active_enemy?12:14]);
    if(snapshot.target_x>=0&&snapshot.target_y>=0&&snapshot.target_x<kViewportTiles&&snapshot.target_y<kViewportTiles)
        target_reticle(rgb565,snapshot.target_x,snapshot.target_y,
                       cache.palette[snapshot.target_valid?11:12]);
    report.left=int16_t(int(snapshot.center.x)-kHalfViewport);report.top=int16_t(int(snapshot.center.y)-kHalfViewport);
    report.right=int16_t(int(snapshot.center.x)+kHalfViewport);report.bottom=int16_t(int(snapshot.center.y)+kHalfViewport);
    report.center_map_tile=uint8_t(std::max<int16_t>(0,snapshot.tiles[kHalfViewport*kViewportTiles+kHalfViewport]));
    report.viewport_crc32=crc32_u16le(rgb565,kViewportPixelCount);report.viewport_bytes=uint32_t(kViewportPixelCount*sizeof(uint16_t));
    report.map_context=snapshot.combat?"combat authoritative snapshot":"world authoritative snapshot";return ESP_OK;
}

/**
 * Blit one authored image into the viewport.  The source is the original's own
 * 4bpp indexed data (HIGH nibble = left pixel); an index becomes a colour only
 * here, through the EGA table, so the packed bytes stay byte-identical to the
 * source file.  Transparency is the container's AND-mask -- bit 1 is
 * background -- and never colour-0 keying, which would punch a hole through the
 * open chest's black interior.  `mirror` and `vflip` flip inside the blit's own
 * destination box, which is exactly what the binary's 0x8b7c / 0x8a2c
 * primitives do; the clipping is per-pixel, so a slice that overhangs the
 * 176x176 window (every 164-tall slice at Y=14 overhangs by two rows) is cut
 * rather than wrapped.
 */
void dungeon_blit_surface(uint16_t *pixels, const DungeonArtBlit &blit,
                          const DungeonArtSurface &surface) {
    const int w = int(surface.w), h = int(surface.h);
    const size_t pixel_stride = size_t(w >> 1);
    const size_t mask_stride = size_t(w >> 3);
    for (int row = 0; row < h; ++row) {
        const int y = int(blit.y) + (blit.vflip ? h - 1 - row : row);
        if (y < 0 || y >= kViewportPixels) continue;
        const uint8_t *source = surface.pixels + size_t(row) * pixel_stride;
        const uint8_t *mask = surface.mask ? surface.mask + size_t(row) * mask_stride : nullptr;
        for (int column = 0; column < w; ++column) {
            if (mask && ((mask[column >> 3] >> (7 - (column & 7))) & 1) != 0) continue;
            const int x = int(blit.x) + (blit.mirror ? w - 1 - column : column);
            if (x < 0 || x >= kViewportPixels) continue;
            const uint8_t byte = source[column >> 1];
            const uint8_t index = (column & 1) ? uint8_t(byte & 0x0f) : uint8_t(byte >> 4);
            pixels[y * kViewportPixels + x] = kDungeonEgaRgb565[index];
        }
    }
}

esp_err_t render_dungeon_view(const GameState &g, const TurnState &t, const DungeonState &d,
                              const DungeonArtSurfaces &art, uint32_t phase, uint16_t *pixels,
                              size_t count, RenderReport &report, uint16_t &primitives) {
    report = {}; primitives = 0;
    if (!d.active || !pixels || count < kViewportPixelCount) return ESP_ERR_INVALID_ARG;
    // R-05.  WHAT is drawn is decided by openu5::plan_dungeon_view() in
    // native/core -- the portable, host-tested seam that mirrors the original's
    // dng_draw_view driver.  Batch 9C added the second seam:
    // openu5::dungeon_art_blits() turns each op into the authored images the
    // original blits for it.  This function only PAINTS; it re-derives neither
    // the plan nor the mapping, so the picture cannot disagree with the core
    // about what the party is looking at or with the art about how it looks.
    const auto plan = plan_dungeon_view(g, t, d);

    // Base black.  It is the unlit viewport, and it is also the vanishing point
    // beyond `light_depth` -- every lit cell is covered by slices whose floor
    // speckle and ceiling are baked in, so nothing else needs to fill it.
    for (size_t i = 0; i < kViewportPixelCount; ++i) pixels[i] = kDungeonBlack;

    // Light gate (DUNGEON:0x1AD6): with neither torch nor light spell the
    // original skips the whole raycast and the viewport stays black.
    if (!plan.lit) {
        primitives = 0;
        dungeon_report(report, d, pixels);
        report.map_context = "dungeon3d unlit viewport";
        return ESP_OK;
    }

    const auto catalog = dungeon_art_authored_catalog();
    uint16_t painted = 0;
    for (uint8_t i = 0; i < plan.count; ++i) {
        DungeonArtBlit blits[kDungeonMaxBlitsPerOp]{};
        const size_t n = dungeon_art_blits(plan.ops[i], catalog, phase, blits, kDungeonMaxBlitsPerOp);
        for (size_t k = 0; k < n; ++k) {
            const DungeonArtSurface *surface = dungeon_art_surface(art, blits[k]);
            if (!surface) continue; // art absent: leave the black rather than guess.
            dungeon_blit_surface(pixels, blits[k], *surface);
            ++painted;
        }
    }
    // `primitives` counts BLITS here, not pixels.  Before Batch 9C it counted
    // per-pixel writes of the wireframe stand-in, which a 176x176 authored
    // repaint would overflow; a blit count is both in range and the number the
    // metrics line actually wants to show.
    primitives = painted;
    dungeon_report(report, d, pixels);
    report.map_context = painted ? "dungeon3d authored viewport" : "dungeon3d viewport without art";
    return ESP_OK;
}

/**
 * EGA colour for one DUNGEON gem cell, ported from the reference's per-type
 * jump-table (gemmap.ts GLYPH/WALL_DENSE/FOUNTAIN_COLOR/FIELD_STRIPES). The
 * exact RUNES glyph shapes are simplified to a solid fill (Class-C: category
 * parity over pixel parity), but the COLOR is the original's exact per-type
 * EGA index, painted through the live asset palette so it matches the rest
 * of the renderer. Returns false for the original's own un-painted cells
 * (corridor 0x0, open chest 0x7, the marker's own seed cell) -- left black.
 */
bool gem_dungeon_color(const PresentationTileCache &cache, uint8_t type, uint16_t &color) {
    switch (type) {
    case 0x1: case 0x2: case 0x3: color = cache.palette[0x7]; return true; // stairs: gray
    case 0x4: case 0xe: case 0xa: case 0xf: color = cache.palette[0xe]; return true; // chest/door/room: yellow
    case 0x5: color = cache.palette[0x9]; return true;  // fountain: bright blue
    case 0x6: color = cache.palette[0xc]; return true;  // trap: bright red
    case 0x8: color = cache.palette[0xd]; return true;  // magic field: bright magenta (1st FIELD_STRIPES)
    case 0xb: color = cache.palette[0xf]; return true;  // wall (solid or dense): white
    case 0xc: case 0xd: color = cache.palette[0x1]; return true; // special wall / secret door: blue
    default: return false; // 0x0 corridor, 0x7 open chest: the original leaves these black.
    }
}

esp_err_t render_dungeon_gem_view(const DungeonState &d, const PresentationTileCache &cache,
                                  uint16_t *pixels, size_t count, RenderReport &report,
                                  uint16_t &primitives) {
    report = {}; primitives = 0;
    if (!d.active || !pixels || count < kViewportPixelCount) return ESP_ERR_INVALID_ARG;
    std::fill(pixels, pixels + kViewportPixelCount, kDungeonBlack);
    const GemView v = build_dungeon_gem_view(d);
    constexpr int cell = kViewportPixels / kGemDungeonDisplay; // 176 / 22 = 8
    for (int row = 0; row < v.height; ++row) {
        for (int col = 0; col < v.width; ++col) {
            const uint8_t type = v.cells[row][col].value;
            if (type == kGemDungeonUnreached) continue;
            uint16_t color = 0;
            if (!gem_dungeon_color(cache, type, color)) continue;
            const int x = col * cell, y = row * cell;
            dungeon_rect(pixels, x + 1, y + 1, x + cell - 2, y + cell - 2, color, primitives);
        }
    }
    // Party marker: RUNES rhombus in the reference, bright green, painted
    // last so it is never occluded by a neighbouring cell's fill.
    const int mx = v.marker_x * cell, my = v.marker_y * cell;
    dungeon_rect(pixels, mx + 2, my + 2, mx + cell - 3, my + cell - 3, cache.palette[0xa], primitives);
    dungeon_report(report, d, pixels); report.map_context = "View Gem dungeon floor";
    return ESP_OK;
}

/**
 * EGA colour for one OVERWORLD/TOWN gem category, ported from the reference's
 * per-category jump-table (gemmap-overworld.ts drawCell/GEM_CATEGORY). The
 * exact per-tile micro-patterns (dots/lines/frames) are simplified to a
 * solid fill (Class-C), but every colour below is the original's own EGA
 * index for that category, painted through the live asset palette. Category
 * 0 (void) and 16 (road, painted separately for its green+red identity) are
 * handled by the caller.
 */
uint16_t gem_world_color(const PresentationTileCache &cache, uint8_t category) {
    switch (category) {
    case 1: case 2: case 9: return cache.palette[0xa]; // bright green: grass/fill/forest
    case 3: return cache.palette[0x4]; // red
    case 4: case 5: case 6: case 7: case 14: return cache.palette[0xf]; // white: hard terrain/signs
    case 8: return cache.palette[0xe]; // yellow: hills
    case 10: case 11: case 12: case 15: return cache.palette[0x9]; // bright blue: coast/water/fountain
    default: return cache.palette[0xa];
    }
}

esp_err_t render_world_gem_view(const ActiveMap &map, Position center, const PresentationTileCache &cache,
                                uint16_t *pixels, size_t count, RenderReport &report,
                                uint16_t &primitives) {
    report = {}; primitives = 0;
    if (!pixels || count < kViewportPixelCount) return ESP_ERR_INVALID_ARG;
    std::fill(pixels, pixels + kViewportPixelCount, kDungeonBlack);
    const GemView v = build_world_gem_view(map, center);
    constexpr int pixel = 5, origin = 8; // 32*5 + 2*8 = 176: the full square, no crop.
    for (int row = 0; row < v.height; ++row) {
        for (int col = 0; col < v.width; ++col) {
            const uint8_t cat = v.cells[row][col].value;
            const int x = origin + col * pixel, y = origin + row * pixel;
            if (cat == 0) continue; // void: leave black, as the original does.
            if (cat == 13) { // swamp: fixed half green / half blue (Class-C).
                dungeon_rect(pixels, x, y, x + pixel - 1, y + (pixel - 1) / 2, cache.palette[0xa], primitives);
                dungeon_rect(pixels, x, y + (pixel - 1) / 2 + 1, x + pixel - 1, y + pixel - 1,
                             cache.palette[0x9], primitives);
                continue;
            }
            if (cat == 16) { // road: green background + red centre (Class-C: no edge-connectivity).
                dungeon_rect(pixels, x, y, x + pixel - 1, y + pixel - 1, cache.palette[0xa], primitives);
                dungeon_rect(pixels, x + 1, y + 1, x + pixel - 2, y + pixel - 2, cache.palette[0x4], primitives);
                continue;
            }
            dungeon_rect(pixels, x, y, x + pixel - 1, y + pixel - 1, gem_world_color(cache, cat), primitives);
        }
    }
    const int mx = origin + v.marker_x * pixel, my = origin + v.marker_y * pixel;
    dungeon_rect(pixels, mx, my, mx + pixel - 1, my + pixel - 1, cache.palette[0xf], primitives);

    int32_t origin_x = 0, origin_y = 0;
    if (map.geometry.wraps) {
        const GemChunkOrigin o = gem_chunk_origin(center.x, center.y);
        origin_x = o.x; origin_y = o.y;
    }
    report.left = int16_t(origin_x); report.top = int16_t(origin_y);
    report.right = int16_t(origin_x + kGemWindow - 1); report.bottom = int16_t(origin_y + kGemWindow - 1);
    report.viewport_bytes = uint32_t(kViewportPixelCount * sizeof(uint16_t));
    report.viewport_crc32 = crc32_u16le(pixels, kViewportPixelCount);
    report.map_context = "View Gem world terrain";
    return ESP_OK;
}

esp_err_t render_zodiac_view(const ZodiacView &view, uint16_t *pixels, size_t count,
                             RenderReport &report, uint16_t &primitives)
{
    report = {}; primitives = 0;
    if (!pixels || count < kViewportPixelCount) return ESP_ERR_INVALID_ARG;
    std::fill(pixels, pixels + kViewportPixelCount, kDungeonBlack);
    constexpr uint16_t kStarColor = 0xffff;   // white background stars
    constexpr uint16_t kSignColor = 0xffe0;   // yellow zodiac glyph marker
    constexpr uint16_t kLineColor = 0x7bef;   // dim Shadowlord-track line
    for (const auto &star : view.stars) dungeon_pixel(pixels, star.x, star.y, kStarColor, primitives);
    for (const auto &sign : view.signs) {
        // "la línea nace 8 px a la izquierda" (zodiac-view.ts) -- a short
        // horizontal segment from line_x to star_x, drawn only when a
        // Shadowlord occupies that sign's city (the zodiac's one gameplay
        // tell; the rest is cosmetic).
        if (sign.has_line) dungeon_line(pixels, sign.line_x, sign.y, sign.star_x, sign.y, kLineColor, primitives);
        dungeon_rect(pixels, sign.star_x, sign.y, sign.star_x + 1, sign.y + 1, kSignColor, primitives);
    }
    report.viewport_bytes = uint32_t(kViewportPixelCount * sizeof(uint16_t));
    report.viewport_crc32 = crc32_u16le(pixels, kViewportPixelCount);
    report.map_context = "Zodiac view";
    return ESP_OK;
}

void shift_viewport_vertically(uint16_t *pixels, int offset_px)
{
    if (!pixels || offset_px <= 0) return;
    if (offset_px > kViewportPixels) offset_px = kViewportPixels;
    for (int y = kViewportPixels - 1; y >= 0; --y) {
        const int src_y = std::max(0, y - offset_px);
        if (src_y == y) continue;
        std::copy(pixels + src_y * kViewportPixels, pixels + (src_y + 1) * kViewportPixels,
                  pixels + y * kViewportPixels);
    }
}

void paint_world_fx_dot(uint16_t *pixels, int32_t dx_milli, int32_t dy_milli)
{
    if (!pixels) return;
    // The window's centre cell is where the party stands; a milli-cell offset
    // therefore lands at (5 + dx) tiles from the left edge, plus half a tile.
    constexpr int32_t half_window = kViewportTiles / 2;
    const int32_t x = ((half_window * kWorldFxMilliCell + dx_milli) * kTilePixels) /
                          kWorldFxMilliCell + kTilePixels / 2;
    const int32_t y = ((half_window * kWorldFxMilliCell + dy_milli) * kTilePixels) /
                          kWorldFxMilliCell + kTilePixels / 2;
    constexpr int32_t side = kWorldFxProjectileDotPx;
    // The same white as the combat missile (the sibling effect this cadence is
    // borrowed from); RGB565 saturated.
    constexpr uint16_t colour = 0xffff;
    for (int32_t row = y - side / 2; row < y - side / 2 + side; ++row) {
        if (row < 0 || row >= kViewportPixels) continue;
        for (int32_t col = x - side / 2; col < x - side / 2 + side; ++col) {
            if (col < 0 || col >= kViewportPixels) continue;
            pixels[row * kViewportPixels + col] = colour;
        }
    }
}

uint32_t recompute_viewport_crc32(const uint16_t *pixels, size_t count) { return crc32_u16le(pixels, count); }

esp_err_t render_intro_view(const PresentationTileCache &cache,const IntroViewFrame &frame,
                            uint32_t tick,uint16_t *pixels,size_t count,RenderReport &report)
{
    constexpr int width=kIntroViewColumns*kTilePixels;
    constexpr size_t required=size_t(width)*kIntroViewRows*kTilePixels;
    report={};if(!cache.tiles||!pixels||count<required)return ESP_ERR_INVALID_ARG;
    std::fill(pixels,pixels+required,uint16_t(0));uint8_t ground[128]{},actor[128]{};
    for(int row=0;row<kIntroViewRows;++row)for(int col=0;col<kIntroViewColumns;++col){
        const int i=row*kIntroViewColumns+col;const uint16_t tile=frame.tiles[i];if(tile==0xffffU)continue;
        const uint8_t terrain=frame.terrain[i];animated_bitmap(cache,uint16_t(animated_tile_frame(terrain,tick,0)),tick,ground);
        const bool has_actor=tile>=0x100U&&tile<512U;if(has_actor)animated_bitmap(cache,tile,tick,actor);
        for(int y=0;y<16;++y)for(int x=0;x<16;++x){const int p=y*16+x;uint8_t color=pixel4(ground,p);if(has_actor){const uint8_t over=pixel4(actor,p);if(over)color=over;}pixels[(row*16+y)*width+col*16+x]=cache.palette[color];}
    }
    if(frame.effect==IntroViewEffect::Dissolve&&frame.effect_col<kIntroViewColumns&&frame.effect_row<kIntroViewRows){
        animated_bitmap(cache,frame.effect_tile,tick,actor);
        for(int y=0;y<16;++y)for(int x=0;x<16;++x){unsigned rank=0;for(unsigned bit=0;bit<4;++bit){const unsigned q=((unsigned(y)>>bit)&1U)*2U+((unsigned(x)>>bit)&1U);constexpr unsigned offset[4]={0,2,3,1};rank=rank*4U+offset[q];}const int p=y*16+x;const uint8_t over=pixel4(actor,p);if(rank<frame.effect_shown&&over)pixels[(int(frame.effect_row)*16+y)*width+int(frame.effect_col)*16+x]=cache.palette[over];}
    }else if(frame.effect==IntroViewEffect::Moongate&&frame.effect_col<kIntroViewColumns&&frame.effect_row<kIntroViewRows){
        animated_bitmap(cache,frame.effect_tile,tick,actor);const int height=std::min<int>(15,frame.effect_step);
        for(int y=16-height;y<16;++y)for(int x=0;x<16;++x){const uint8_t over=pixel4(actor,y*16+x);if(over)pixels[(int(frame.effect_row)*16+y)*width+int(frame.effect_col)*16+x]=cache.palette[over];}
    }else if(frame.effect==IntroViewEffect::Beam){
        int x=128+9*int(frame.effect_step),y=32+3*int(frame.effect_step);for(int n=0;n<=9;++n){const int px=x+n,py=y+n/3;if(px>=0&&px<width&&py>=0&&py<kIntroViewRows*16){pixels[py*width+px]=cache.palette[15];if(py+1<kIntroViewRows*16)pixels[(py+1)*width+px]=cache.palette[11];}}
    }
    report.viewport_bytes=uint32_t(required*sizeof(uint16_t));report.viewport_crc32=crc32_u16le(pixels,required);report.map_context="INTRO.OVL scripted View";return ESP_OK;
}

}  // namespace openu5
