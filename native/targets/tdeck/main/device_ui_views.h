#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace tdeck {

inline bool ui_scale_requires_full_layout(uint8_t cached,uint8_t current) {
    return cached!=current;
}

struct DeviceTextMetrics {
    uint8_t glyph_width=5,glyph_height=7,cell_width=6,line_height=8;
};

// Three real raster geometries.  The former mapping collapsed Small and Medium
// to the same 5x7 path and made Large a horizontally stretched 10x7 glyph.
constexpr DeviceTextMetrics ui_text_metrics(uint8_t size) {
    return size==0 ? DeviceTextMetrics{4,6,5,7}
         : size>=2 ? DeviceTextMetrics{7,9,8,10}
                   : DeviceTextMetrics{5,7,6,8};
}

constexpr int kContextBarTop=215,kContextBarStatusY=217,kContextBarActionsY=229;
constexpr int kContextBarStatusH=9,kContextBarActionsH=9;
constexpr size_t kContextBarActionColumns=22;
constexpr char kTavernContextActions[]="A Round|D Drink|R Food";
static_assert(sizeof(kTavernContextActions)-1<=kContextBarActionColumns);
struct DeviceContextActionBar {
    bool active=false;
    char status[32]{},actions[48]{};
};

constexpr bool context_action_bar_does_not_overlap(int content_bottom) {
    return content_bottom<=kContextBarTop;
}

constexpr int kCharacterTitleArtY=4,kCharacterTitleArtH=110;
constexpr int kCharacterTextY=116,kCharacterTextRowH=9;
constexpr int kCharacterQuizArtH=152,kCharacterQuizTextY=154;

constexpr size_t kShopVisibleRows=6;
struct DeviceShopRow { char name[24]{}; int32_t price=0,quantity=0; };
struct DeviceShopView {
    bool active=false;
    char title[32]{},keeper[24]{},phase[64]{},input[24]{};
    DeviceContextActionBar context{};
    int32_t gold=0;
    size_t total=0,page_start=0,row_count=0,selected_row=0;
    DeviceShopRow rows[kShopVisibleRows]{};
};

constexpr size_t kSelectionVisibleRows=8;
struct DeviceSelectionView {
    bool active=false,detail_panel=false;
    uint8_t mode=0;
    char title[24]{},detail[32]{},detail2[32]{};
    DeviceContextActionBar context{};
    size_t total=0,page_start=0,row_count=0,selected_row=0;
    char rows[kSelectionVisibleRows][24]{};
};

inline bool selection_uses_transcript(const DeviceSelectionView &view) {
    return !view.detail_panel;
}

inline void format_ready_row(char *out,size_t capacity,const char *name,
                             uint16_t quantity,bool equipped) {
    // Equipment state is authoritative over pack count: an item may be worn
    // while another copy remains in the pack, and the selector must never hide
    // that fact behind the quantity label.
    if(equipped&&quantity>0)std::snprintf(out,capacity,"%s x%u [equipped]",name,unsigned(quantity));
    else if(equipped)std::snprintf(out,capacity,"%s [equipped]",name);
    else if(quantity>1)std::snprintf(out,capacity,"%s x%u",name,unsigned(quantity));
    else if(quantity==1)std::snprintf(out,capacity,"%s",name);
    else std::snprintf(out,capacity,"%s",name);
}

inline uint16_t magic_xor_palette_pixel(uint16_t pixel,const uint16_t *palette) {
    for(int color=0;color<16;++color)if(pixel==palette[color])return palette[color^15];
    return pixel;
}

struct DevicePartyHighlight {
    int8_t selected=-1;
    int8_t actor=-1;
    // Y-04 (#213). The roster row in REVERSE VIDEO, the binary's shared 0x2a28
    // primitive. It outranks `selected`/`actor` while it lasts, because 0x2a28
    // is an XOR over the framebuffer -- see openu5::roster_invert_row, which
    // owns the precedence rule this field is the top of.
    int8_t damage_flash=-1;
};

inline bool selection_row_needs_redraw(const DeviceSelectionView &current,
                                        const DeviceSelectionView &cached,size_t row,bool first=false) {
    const bool have=row<current.row_count,had=row<cached.row_count;
    return first||have!=had||row==current.selected_row||row==cached.selected_row||
        (have&&std::strcmp(current.rows[row],cached.rows[row])!=0);
}

inline bool shop_row_needs_redraw(const DeviceShopView &current,
                                  const DeviceShopView &cached,size_t row,bool first=false) {
    const bool have=row<current.row_count,had=row<cached.row_count;
    return first||have!=had||row==current.selected_row||row==cached.selected_row||
        (have&&(std::strcmp(current.rows[row].name,cached.rows[row].name)!=0||
                current.rows[row].price!=cached.rows[row].price||
                current.rows[row].quantity!=cached.rows[row].quantity));
}

}  // namespace tdeck
