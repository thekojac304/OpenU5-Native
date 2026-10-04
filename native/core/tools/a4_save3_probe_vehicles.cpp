// A4-SAVE3 probe 2: vehicles through Native's codec.
#include "openu5/gameplay_save.h"
#include "openu5/persistence.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
using namespace openu5;
static std::vector<uint8_t> rd(const char *p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
int main() {
    const std::string root = "original/u5/ultima5/";
    auto gam = rd((root + "SAVED.GAM").c_str()), init = rd((root + "INIT.GAM").c_str());
    // Outdoors on the surface at (0x50,0x60), on foot, a frigate parked in slot 30.
    gam[0x2ed] = 0; gam[0x2ef] = 0; gam[0x2f0] = 0x50; gam[0x2f1] = 0x60;
    std::fill(gam.begin() + 0x6b4, gam.begin() + 0x7b4, 0);
    uint8_t *o = gam.data() + 0x6b4;
    o[0] = o[1] = 0x1c; o[2] = 0x50; o[3] = 0x60;
    uint8_t *f = o + 30 * 8;
    f[0] = f[1] = 0x24; f[2] = 0x51; f[3] = 0x60; f[4] = 0; f[5] = 77; f[7] = 1;
    GameState g{}; TurnState t{}; save::Json doc; save::SidecarSource src;
    auto e = save::load_native_state(gam.data(), gam.size(), nullptr, g, t, doc, src, true);
    std::string text; save::encode_json(doc["worldObjects"], text);
    std::printf("import=%d worldObjects=%s\n", int(e), text.c_str());
    std::printf("validate_world_objects on the sidecar-less import = %d (0 = accepted)\n", int(save::validate_world_objects(doc)));
    // A Native-captured ship (capture_world_objects shape) at the same place.
    QuestObject ship; ship.location = 0; ship.floor = 0; ship.x = 0x51; ship.y = 0x60; ship.tile = 0x124; ship.hull = 77; ship.skiffs = 1; ship.ship = true;
    struct Pool { std::vector<QuestObject> v; } pool{{ship}};
    QuestWorldServices q{};
    q.context = &pool;
    q.count = [](void *p) { return static_cast<Pool *>(p)->v.size(); };
    q.read = [](void *p, size_t i) { return static_cast<Pool *>(p)->v[i]; };
    save::capture_world_objects(q, doc);
    save::Gam out{}; save::Json side;
    e = save::export_native_state(g, t, doc, init.data(), init.size(), out, side, true);
    int found = -1;
    for (int s = 1; s < 32; ++s) if ((out[0x6b4 + s * 8] & 0xf8) == 0x20) found = s;
    std::printf("export=%d frigate record in exported object table: %s\n", int(e), found < 0 ? "NONE" : std::to_string(found).c_str());
}
