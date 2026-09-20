#include "native_renderer.h"

#include "openu5/dungeon_view.h"
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
constexpr uint16_t kDungeonCeiling = 0x0841;
constexpr uint16_t kDungeonFloor = 0x2104;
constexpr uint16_t kDungeonWall = 0x7a08;
constexpr uint16_t kDungeonMortar = 0xce79;
constexpr uint16_t kDungeonFeature = 0x07e0;
constexpr uint16_t kDungeonDanger = 0xf800;

// R-05.  The EGA-16 entries the reference's PACKLESS dungeon renderer uses
// (skin/fiel/dungeon.ts drawSidePlaceholder / drawFrontPlaceholder /
// drawContents), converted to RGB565.  The device is still packless -- the
// DNG*.16 slice atlases and the ITEMS.16 feature bank are not in the asset pack
// yet -- so it draws the reference's OWN fallback rather than an invented one.
constexpr uint16_t kEga0 = 0x0000;  // #000000
constexpr uint16_t kEga6 = 0xaaa0;  // #aa5500 brown  (door panel, closed chest)
constexpr uint16_t kEga7 = 0xad55;  // #aaaaaa light grey (front wall, right side)
constexpr uint16_t kEga8 = 0x52aa;  // #555555 dark grey  (left side, open chest)
constexpr uint16_t kEga10 = 0x57ea; // #55ff55
constexpr uint16_t kEga11 = 0x57ff; // #55ffff light cyan (fountain)
constexpr uint16_t kEga12 = 0xfaaa; // #ff5555
constexpr uint16_t kEga13 = 0xfabf; // #ff55ff
constexpr uint16_t kEga14 = 0xffea; // #ffff55 yellow (ladder)

/**
 * Ring box at `depth` -- the reference's `ringBox()`, derived from the real
 * SIDE_X slice table rather than from hand-tuned constants.  `near` spans to the
 * viewport edge at depth 0.
 */
struct DungeonRing { int l, r, top, bot; };
DungeonRing dungeon_ring(int depth) {
    const int d = depth < 0 ? 0 : depth > kDungeonMaxDepth ? kDungeonMaxDepth : depth;
    return {kDungeonSideXLeft[d], kDungeonSideXRight[d] + (d == 0 ? 24 : 0),
            kDungeonSliceY + d * 18, kDungeonSliceY + kDungeonSliceHeight - d * 18};
}

/** Feature anchor box at `depth` -- the reference's `featureBox()`. */
struct DungeonFeatureBox { int cx, floor_y, s; };
DungeonFeatureBox dungeon_feature_box(int depth) {
    static const int inner_l[4] = {40, 72, 88, 96};
    static const int inner_r[4] = {152, 120, 104, 96};
    static const int floor_y[4] = {150, 122, 106, 98};
    const int d = depth < 0 ? 0 : depth > kDungeonMaxDepth ? kDungeonMaxDepth : depth;
    const int w = std::max(8, inner_r[d] - inner_l[d]);
    return {kDungeonCenterX, floor_y[d], w};
}

/**
 * Wall tint by `g_dng_wall_variant`.  The original swaps the whole slice ATLAS
 * (DNG1 olive / DNG2 red / DNG3 grey); packless, the placeholder can only carry
 * the hue.  Replace this with atlas selection once R-05's asset half lands.
 */
uint16_t dungeon_variant_wall(uint8_t variant, uint16_t base) {
    switch (variant) {
    case 1: return 0x9d45; // olive
    case 2: return 0xa984; // red-brown
    default: return base;  // grey
    }
}

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

/**
 * Fill the trapezoid a side slice occupies between its near and far rings --
 * the packless stand-in for the pre-drawn perspective slice.  Scanline fill
 * between the two interpolated edges, so it clips like any other primitive.
 */
void dungeon_side_quad(uint16_t *pixels, const DungeonRing &near, const DungeonRing &far,
                       bool left, uint16_t fill, uint16_t &primitives) {
    const int nx = left ? near.l : near.r, fx = left ? far.l : far.r;
    const int span = far.top - near.top;
    if (span <= 0) return;
    for (int i = 0; i <= span; ++i) {
        const int y_top = near.top + i, y_bot = near.bot - i;
        const int x = nx + (fx - nx) * i / span;
        const int a = left ? nx : x, b = left ? x : nx;
        dungeon_line(pixels, a, y_top, b, y_top, fill, primitives);
        dungeon_line(pixels, a, y_bot, b, y_bot, fill, primitives);
        dungeon_pixel(pixels, x, y_top, kEga0, primitives);
        dungeon_pixel(pixels, x, y_bot, kEga0, primitives);
    }
    dungeon_line(pixels, nx, near.top, nx, near.bot, kEga0, primitives);
    dungeon_line(pixels, fx, far.top, fx, far.bot, kEga0, primitives);
}

bool dungeon_wall(const DungeonState &d, int x, int y) {
    const int type = dungeon_cell(d, d.pos.floor, (x + 8) & 7, (y + 8) & 7) >> 4;
    if (type == 11 || type == 12) return true;
    if (type != 13) return false;
    const int n = int(d.pos.floor) * 64 + ((y + 8) & 7) * 8 + ((x + 8) & 7);
    return (d.revealed[n >> 3] & (1U << (n & 7))) == 0;
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

esp_err_t render_dungeon_view(const GameState &g, const TurnState &t, const DungeonState &d,
                              uint16_t *pixels, size_t count,
                              RenderReport &report, uint16_t &primitives) {
    report = {}; primitives = 0;
    if (!d.active || !pixels || count < kViewportPixelCount) return ESP_ERR_INVALID_ARG;
    // R-05.  WHAT is drawn is decided by openu5::plan_dungeon_view() in
    // native/core -- the portable, host-tested seam that mirrors the original's
    // dng_draw_view driver.  This function only PAINTS the plan; it no longer
    // re-derives geometry, sight or contents from the map, so the picture cannot
    // disagree with the core about what the party is looking at.
    const auto plan = plan_dungeon_view(g, t, d);

    // Light gate (DUNGEON:0x1AD6): with neither torch nor light spell the
    // original skips the whole raycast and the viewport stays black.
    if (!plan.lit) {
        for (size_t i = 0; i < kViewportPixelCount; ++i) pixels[i] = kDungeonBlack;
        primitives = uint16_t(kViewportPixelCount);
        dungeon_report(report, d, pixels);
        report.map_context = "dungeon3d unlit viewport";
        return ESP_OK;
    }

    for (int y = 0; y < kViewportPixels; ++y) {
        const uint16_t color = y < kViewportPixels / 2 ? kDungeonCeiling : kDungeonFloor;
        for (int x = 0; x < kViewportPixels; ++x) pixels[y * kViewportPixels + x] = color;
    }
    primitives = uint16_t(kViewportPixelCount);

    const uint16_t wall_tint = dungeon_variant_wall(plan.wall_variant, kEga7);

    for (uint8_t i = 0; i < plan.count; ++i) {
        const auto &op = plan.ops[i];
        switch (op.kind) {
        case DungeonOpKind::Side: {
            // base = slice - depth.  Bases 0 (plain wall), 4 (side door) and
            // 0x14 (alcove) are solid; 0x10 is an OPEN passage, which the
            // reference paints black so the corridor reads as continuing.
            const int base = int(op.slice) - int(op.depth);
            const bool solid = base != 0x10;
            const bool left = op.side == DungeonSide::Left;
            const auto near_ring = dungeon_ring(op.depth);
            const auto far_ring = dungeon_ring(std::min<int>(op.depth + 1, kDungeonMaxDepth));
            const uint16_t fill = solid ? (left ? kEga8 : wall_tint) : kEga0;
            dungeon_side_quad(pixels, near_ring, far_ring, left, fill, primitives);
            const int edge_a = left ? near_ring.l : far_ring.r;
            const int edge_b = left ? far_ring.l : near_ring.r;
            const int inner = std::min(edge_a, edge_b) + 1;
            const int outer = std::max(edge_a, edge_b) - 1;
            if (base == 4) {
                // A side DOOR.  Packless, mark it so a doorway on the wall
                // beside you is not mistaken for plain masonry.
                const int h = (near_ring.bot - near_ring.top) / 3;
                if (outer > inner)
                    dungeon_rect(pixels, inner, near_ring.bot - h, outer, near_ring.bot - 2,
                                 kEga6, primitives);
            } else if (base == 0x14) {
                // An ALCOVE (special wall 0xC): a recessed dark panel.
                const int mid = (near_ring.top + near_ring.bot) / 2;
                if (outer > inner)
                    dungeon_rect(pixels, inner, mid - 12, outer, mid + 12, kEga0, primitives);
            } else if (solid) {
                for (int y = near_ring.top + 12; y < near_ring.bot; y += 20)
                    dungeon_line(pixels, edge_a, y, edge_b, y, kDungeonMortar, primitives);
            }
            break;
        }
        case DungeonOpKind::Front: {
            // The dead end sits on the ring BEHIND the last open cell, exactly
            // as the reference's drawFrontPlaceholder anchors it.
            const auto box = dungeon_ring(std::min<int>(op.depth + 1, kDungeonMaxDepth));
            const int base = int(op.slice) - int(op.depth);
            dungeon_rect(pixels, box.l, box.top, box.r, box.bot,
                         base == 0x18 ? kEga8 : wall_tint, primitives);
            dungeon_line(pixels, box.l, box.top, box.r, box.top, kEga0, primitives);
            dungeon_line(pixels, box.l, box.bot, box.r, box.bot, kEga0, primitives);
            dungeon_line(pixels, box.l, box.top, box.l, box.bot, kEga0, primitives);
            dungeon_line(pixels, box.r, box.top, box.r, box.bot, kEga0, primitives);
            if (base == 12) {
                // Dead end WITH A DOOR: rooms-broke, a normal door, a room, or a
                // revealed secret door.  Before Batch 9 all four painted a blank
                // grey wall, so a doorway was indistinguishable from masonry.
                const int w = (box.r - box.l) * 2 / 5;
                const int h = (box.bot - box.top) * 62 / 100;
                const int cx = (box.l + box.r) / 2;
                dungeon_rect(pixels, cx - w / 2, box.bot - h, cx + w / 2, box.bot, kEga6,
                             primitives);
            } else if (base == 8) {
                for (int y = box.top + 5; y < box.bot; y += 10)
                    dungeon_line(pixels, box.l, y, box.r, y, kDungeonMortar, primitives);
                for (int x = box.l + 8; x < box.r; x += 16)
                    dungeon_line(pixels, x, box.top, x, box.bot, kDungeonMortar, primitives);
            }
            break;
        }
        case DungeonOpKind::Feature: {
            const bool field = op.cell_type == uint8_t(DungeonCellKind::MagicField);
            if (!field && !dungeon_feature_drawable(op.cell_type, op.sub)) break;
            const auto box = dungeon_feature_box(op.depth);
            switch (DungeonCellKind(op.cell_type)) {
            case DungeonCellKind::LadderUp:
            case DungeonCellKind::LadderDown:
            case DungeonCellKind::LadderUpDown: {
                const int h = box.s * 9 / 10, top = box.floor_y - h, rail = std::max(2, box.s / 8);
                dungeon_line(pixels, box.cx - rail, box.floor_y, box.cx - rail, top, kEga14,
                             primitives);
                dungeon_line(pixels, box.cx + rail, box.floor_y, box.cx + rail, top, kEga14,
                             primitives);
                for (int r = 0; r <= 4; ++r) {
                    const int y = top + h * r / 4;
                    dungeon_line(pixels, box.cx - rail, y, box.cx + rail, y, kEga14, primitives);
                }
                break;
            }
            case DungeonCellKind::Chest:
            case DungeonCellKind::OpenChest: {
                const int w = std::max(2, box.s / 6), h = std::max(2, box.s * 28 / 100);
                const bool closed = DungeonCellKind(op.cell_type) == DungeonCellKind::Chest;
                dungeon_rect(pixels, box.cx - w, box.floor_y - h, box.cx + w, box.floor_y,
                             closed ? kEga6 : kEga8, primitives);
                break;
            }
            case DungeonCellKind::Fountain: {
                const int rx = std::max(2, box.s / 5), ry = std::max(2, box.s * 8 / 100);
                const int cy = box.floor_y - box.s / 10;
                dungeon_line(pixels, box.cx - rx, cy, box.cx + rx, cy, kEga11, primitives);
                dungeon_line(pixels, box.cx - rx, cy - ry, box.cx + rx, cy - ry, kEga11,
                             primitives);
                dungeon_line(pixels, box.cx - rx, cy - ry, box.cx - rx, cy, kEga11, primitives);
                dungeon_line(pixels, box.cx + rx, cy - ry, box.cx + rx, cy, kEga11, primitives);
                break;
            }
            case DungeonCellKind::Trap: {
                const int w = std::max(2, box.s / 5);
                dungeon_rect(pixels, box.cx - w, box.floor_y - 3, box.cx + w, box.floor_y,
                             kDungeonDanger, primitives);
                break;
            }
            case DungeonCellKind::MagicField: {
                static const uint16_t colours[4] = {kEga13, kEga10, kEga12, kEga11};
                const uint16_t colour = colours[op.sub & 3];
                const int w = std::max(2, box.s / 4), h = std::max(2, box.s / 2);
                const int top = box.floor_y - h;
                dungeon_line(pixels, box.cx - w, top, box.cx + w, top, colour, primitives);
                dungeon_line(pixels, box.cx - w, box.floor_y, box.cx + w, box.floor_y, colour,
                             primitives);
                dungeon_line(pixels, box.cx - w, top, box.cx - w, box.floor_y, colour, primitives);
                dungeon_line(pixels, box.cx + w, top, box.cx + w, box.floor_y, colour, primitives);
                break;
            }
            default: break;
            }
            break;
        }
        case DungeonOpKind::Monster: {
            // Tables 0x2E2A (X by depth) and 0x2E32 (Y by [row][depth]); row 1 is
            // the CEILING row -- a lurking spider or slime, not invisibility.
            static const int mon_x[3] = {72, 80, 88};
            static const int mon_y[2][3] = {{86, 96, 98}, {40, 70, 85}};
            const int di = std::min(std::max(int(op.depth), 1), 3) - 1;
            const int x = mon_x[di], y = mon_y[op.ceiling ? 1 : 0][di];
            const int half = std::max(6, kDungeonCenterX - x);
            dungeon_rect(pixels, x, y, kDungeonCenterX + half, y + half, kDungeonDanger,
                         primitives);
            break;
        }
        }
    }
    dungeon_report(report, d, pixels); report.map_context = "dungeon3d authoritative viewport";
    return ESP_OK;
}

esp_err_t render_dungeon_gem_view(const DungeonState &d, uint16_t *pixels, size_t count,
                                  RenderReport &report, uint16_t &primitives) {
    report = {}; primitives = 0;
    if (!d.active || !pixels || count < kViewportPixelCount) return ESP_ERR_INVALID_ARG;
    std::fill(pixels, pixels + kViewportPixelCount, kDungeonBlack);
    constexpr int size = 22, cell = 8, center = 11;
    std::array<uint8_t, size * size> reached{};
    std::array<uint16_t, size * size> queue{};
    size_t head = 0, tail = 0;
    reached[center * size + center] = 1;
    queue[tail++] = uint16_t(center * size + center);
    constexpr int nx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    constexpr int ny[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    auto type_at = [&](int col, int row) {
        return int(dungeon_cell(d, d.pos.floor, (int(d.pos.x) + col - center + 8) & 7,
                                (int(d.pos.y) + row - center + 8) & 7) >> 4);
    };
    while (head < tail) {
        const int at = queue[head++], col = at % size, row = at / size;
        for (int n = 0; n < 8; ++n) {
            const int x = col + nx[n], y = row + ny[n];
            if (x < 0 || x >= size || y < 0 || y >= size) continue;
            const int next = y * size + x;
            if (reached[next]) continue;
            reached[next] = 1;
            if (!dungeon_wall(d, int(d.pos.x) + x - center, int(d.pos.y) + y - center))
                queue[tail++] = uint16_t(next);
        }
    }
    for (int row = 0; row < size; ++row) for (int col = 0; col < size; ++col) {
        if (!reached[row * size + col]) continue;
        const int type = type_at(col, row);
        const int x = col * cell, y = row * cell;
        if (type == 11) dungeon_rect(pixels, x, y, x + 7, y + 7, kDungeonMortar, primitives);
        else if (type == 12 || type == 13) dungeon_rect(pixels, x + 1, y + 1, x + 6, y + 6, kDungeonWall, primitives);
        else if (type == 8) dungeon_rect(pixels, x + 1, y + 2, x + 6, y + 5, kDungeonDanger, primitives);
        else if (type >= 1 && type <= 7) dungeon_rect(pixels, x + 2, y + 2, x + 5, y + 5, kDungeonFeature, primitives);
    }
    dungeon_rect(pixels, center * cell + 2, center * cell + 2, center * cell + 5, center * cell + 5,
                 kDungeonFeature, primitives);
    dungeon_report(report, d, pixels); report.map_context = "View Gem dungeon floor";
    return ESP_OK;
}

esp_err_t render_world_gem_view(const ActiveMap &map, Position center, uint16_t *pixels,
                                size_t count, RenderReport &report, uint16_t &primitives) {
    report = {}; primitives = 0;
    if (!pixels || count < kViewportPixelCount) return ESP_ERR_INVALID_ARG;
    std::fill(pixels, pixels + kViewportPixelCount, kDungeonBlack);
    constexpr int cells = 32, pixel = 5, origin = 8;
    for (int row = 0; row < cells; ++row) for (int col = 0; col < cells; ++col) {
        int x = int(center.x) + col - cells / 2, y = int(center.y) + row - cells / 2;
        if (map.geometry.wraps) { x = wrap_coord(x); y = wrap_coord(y); }
        else if (x < 0 || y < 0 || x >= map.geometry.width || y >= map.geometry.height) continue;
        const int tile = map.tile_at(x, y);
        // Gem view is a terrain-category map, not an object-layer snapshot.
        const uint16_t color = (tile & 3) == 3 ? 0x001f : (tile & 7) == 0 ? 0x07e0 :
                               (tile & 15) < 4 ? 0x7be0 : 0x8410;
        dungeon_rect(pixels, origin + col * pixel, origin + row * pixel,
                     origin + col * pixel + pixel - 1, origin + row * pixel + pixel - 1,
                     color, primitives);
    }
    dungeon_rect(pixels, origin + 16 * pixel, origin + 16 * pixel,
                 origin + 16 * pixel + pixel - 1, origin + 16 * pixel + pixel - 1,
                 kDungeonFeature, primitives);
    report.left = int16_t(int(center.x) - 16); report.top = int16_t(int(center.y) - 16);
    report.right = int16_t(int(center.x) + 15); report.bottom = int16_t(int(center.y) + 15);
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
