#pragma once
#include "commands.h"
namespace openu5 {
struct LookSign {const char *text=nullptr;const uint8_t *raw=nullptr;size_t raw_size=0;};
struct LookSignRecord {
    MapId map{};
    uint8_t x=0,y=0;
    LookSign value{};
};
// Coordinate lookup shared by resource-backed frontends. Extracted small-map
// basement signs use byte floor 255; lookup aliases that to runtime floor -1.
// Sign text is not a tile description: LOOK2 deliberately stores "*" for all
// five sign faces.
LookSign resolve_look_sign(const LookSignRecord *,size_t,MapId,int32_t,int32_t);
// Borrowed synchronous descriptions; the asset owner supplies LOOK2 with the
// authoritative TileData fallback and decoded sign text/raw bytes.
struct LookServices {
    void *context=nullptr;
    const char *(*describe)(void *,int32_t)=nullptr;
    LookSign (*sign)(void *,MapId,int32_t,int32_t)=nullptr;
};
struct ZodiacView {
    struct Star { uint8_t x=0,y=0; } stars[80];
    struct Sign { uint8_t star_x=0,line_x=0,y=0; bool has_line=false; } signs[8];
};
void emit_zodiac(CommandContext &,EventSink,Rand);
CommandStatus world_look(CommandContext &,Command,const ActiveMap &,EventSink,Rand);
// Pure fountain-drink flavour text (reference: main.ts's pickMember("Who will
// drink?")). Presentation only -- no state, no HP, no turn, no command. The
// picked member's party status decides the line; cancelled overrides both.
const char *fountain_drink_result(char status, bool cancelled);
}
