#include "openu5/persistence.h"
#include <fstream>
#include <iostream>
using namespace openu5;
using namespace openu5::save;
std::string utf8(const Json &j) {
    std::string s;
    for (size_t i = 0; i < j.string.size(); ++i) {
        uint32_t c = j.string[i];
        if (c >= 0xd800 && c <= 0xdbff && i + 1 < j.string.size()) {
            c = 0x10000 + ((c - 0xd800) << 10) + (j.string[++i] - 0xdc00);
        }
        if (c < 128)
            s += char(c);
        else if (c < 2048) {
            s += char(0xc0 | (c >> 6));
            s += char(0x80 | (c & 63));
        } else if (c < 65536) {
            s += char(0xe0 | (c >> 12));
            s += char(0x80 | ((c >> 6) & 63));
            s += char(0x80 | (c & 63));
        } else {
            s += char(0xf0 | (c >> 18));
            s += char(0x80 | ((c >> 12) & 63));
            s += char(0x80 | ((c >> 6) & 63));
            s += char(0x80 | (c & 63));
        }
    }
    return s;
}
std::vector<uint8_t> unhex(const Json &j) {
    std::vector<uint8_t> b;
    for (size_t i = 0; i + 1 < j.string.size(); i += 2) {
        auto digit = [](char16_t c) { return c >= 'a' ? c - 'a' + 10 : c - '0'; };
        b.push_back(uint8_t(digit(j.string[i]) * 16 + digit(j.string[i + 1])));
    }
    return b;
}
template <class T> Json hex(const T &b) {
    constexpr char h[] = "0123456789abcdef";
    std::string s;
    for (auto v : b) {
        s += h[v >> 4];
        s += h[v & 15];
    }
    return Json(s.c_str());
}
int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    std::ifstream in(argv[1]);
    std::ofstream out(argv[2]);
    std::string line;
    while (std::getline(in, line)) {
        Json q, r = Json::object();
        if (parse_json(line, q) != JsonError::None)
            return 3;
        auto op = q["op"].string;
        auto b = unhex(q["gam"]);
        auto tmpl = unhex(q["template"]);
        auto ool = unhex(q["ool"]);
        bool gate = q["gate"].truth();
        if (op == u"export") {
            Gam g{};
            Json side;
            auto e = export_native(q["state"], tmpl.data(), tmpl.size(), g, side, gate);
            r["error"] = Json(int(e));
            if (e == save::Error::None) {
                r["gam"] = hex(g);
                r["sidecar"] = side;
                r["ool"] = hex(build_ool(q["state"], ool.data(), ool.size()));
                Json state;
                import_native(g.data(), g.size(), side, state, gate);
                r["imported"] = state;
                std::vector<uint8_t> envelope;
                if (write_envelope(g, q["meta"], side, envelope) != save::Error::None)
                    return 6;
                r["envelope"] = hex(envelope);
            }
        } else if (op == u"import") {
            Json state;
            SidecarSource source = SidecarSource::None;
            std::string side;
            if (q.has("sidecarText")) {
                side = utf8(q["sidecarText"]);
            }
            auto e =
                import_save(b.data(), b.size(), q.has("sidecarText") ? &side : nullptr, state, source, gate);
            r["error"] = Json(int(e));
            if (e == save::Error::None) {
                r["state"] = state;
                r["source"] = Json(int(source));
            }
        } else if (op == u"envelope") {
            auto e = read_envelope(b.data(), b.size());
            r["kind"] = Json(int(e.kind));
            if (e.kind == EnvelopeKind::Ok)
                r["envelope"] = e.document;
        } else if (op == u"deserialize") {
            Json state;
            auto e = deserialize(utf8(q["text"]), state);
            r["error"] = Json(int(e));
            if (e == save::Error::None)
                r["state"] = state;
        } else if (op == u"bind") {
            GameState g{};
            TurnState t{};
            g.rng.seed(12345);
            Json retained;
            auto e = load_state(write_json(q["state"]), g, t, retained);
            r["error"] = Json(int(e));
            if (e == save::Error::None) {
                Json state;
                g.gold += 1;
                g.position.xy.x = uint8_t(g.position.xy.x + 1);
                std::string serialized;
                if (save_state(g, t, retained, serialized) != save::Error::None)
                    return 7;
                parse_json(serialized, state);
                r["state"] = state;
                r["seed"] = Json(int(g.rng.get_seed()));
                auto gold = g.gold;
                auto before = write_json(retained);
                auto failed = load_state("{", g, t, retained);
                r["atomic"] = Json(failed == save::Error::JsonSyntax && g.gold == gold &&
                                   before == write_json(retained));
            }
        } else if (op == u"recovery") {
            std::vector<Generation> gs;
            std::vector<std::vector<uint8_t>> bytes, ools;
            std::vector<std::string> sides;
            auto count = q["generations"].values.size();
            bytes.resize(count);
            ools.resize(count);
            sides.resize(count);
            for (size_t i = 0; i < count; ++i) {
                const auto &v = q["generations"].at(i);
                bytes[i] = unhex(v["gam"]);
                ools[i] = unhex(v["ool"]);
                sides[i] = write_json(v["sidecar"]);
                Generation g;
                g.sequence = uint64_t(v["sequence"].integer());
                g.committed = v["committed"].truth();
                g.identity_matches = v["identity"].truth();
                g.gam = bytes[i].data();
                g.gam_size = bytes[i].size();
                g.ool = ools[i].empty() ? nullptr : ools[i].data();
                g.ool_size = ools[i].size();
                g.sidecar = v.has("sidecar") ? &sides[i] : nullptr;
                g.requires_sidecar = v["requireSidecar"].truth();
                g.requires_ool = v["requireOol"].truth();
                g.gam_crc = save_crc32(g.gam, g.gam_size);
                g.ool_crc = save_crc32(g.ool, g.ool_size);
                g.sidecar_crc =
                    save_crc32(reinterpret_cast<const uint8_t *>(sides[i].data()), sides[i].size());
                if (v["badCrc"].truth())
                    ++g.gam_crc;
                gs.push_back(g);
            }
            r["selected"] = Json(select_generation(gs.data(), gs.size()));
        } else if (op == u"npc") {
            NpcActors actors;
            actors.count = 3;
            for (size_t i = 0; i < 3; ++i) {
                auto &a = actors.actors[i];
                a.schedule.slot = uint8_t(i + 1);
                a.location = 17;
                a.x = 1;
                a.y = 2;
                a.z = 0;
            }
            auto e = restore_npc_walk(q["state"], 17, gate, actors);
            r["error"] = Json(int(e));
            if (e == save::Error::None) {
                Json s;
                capture_npc_walk(actors, 17, s);
                r["walk"] = s["npcWalk"];
            }
        } else if (op == u"metadata") {
            r["selected"] = Json(most_recent_slot(q["index"]));
            r["slot"] = Json(autosave_slot(q["pointer"].integer()));
        } else if (op == u"json") {
            Json j;
            auto e = parse_json(utf8(q["text"]), j);
            r["error"] = Json(int(e));
            if (e == JsonError::None)
                r["value"] = j;
        } else if (op == u"contracts") {
            int passed = 0;
            Json huge("");
            huge.string.resize(kMaxJsonBytes + 1, u'x');
            std::string untouched = "sentinel";
            if (encode_json(huge, untouched) == JsonError::Capacity && untouched == "sentinel")
                ++passed;
            Json many = Json::array();
            many.values.resize(kMaxJsonNodes, Json(0));
            if (encode_json(many, untouched) == JsonError::Capacity && untouched == "sentinel")
                ++passed;
            const std::string crcInput = "123456789";
            if (save_crc32(reinterpret_cast<const uint8_t *>(crcInput.data()), crcInput.size()) == 0xcbf43926)
                ++passed;
            Gam gam{};
            gam[0x204] = 12;
            gam[0xfff] = 0xab;
            const std::string name = "123456789";
            for (size_t i = 0; i < 9; ++i)
                gam[2 + i] = uint8_t(name[i]);
            GameState g;
            TurnState t;
            Json retained;
            SidecarSource source = SidecarSource::File;
            g.rng.seed(4321);
            if (load_native_state(gam.data(), gam.size(), nullptr, g, t, retained, source) ==
                    save::Error::None &&
                g.gold == 12 && source == SidecarSource::None && g.rng.get_seed() == 4321 &&
                std::string(g.party.characters[0].name) == name)
                ++passed;
            g.gold = 345;
            Gam encoded{};
            Json side;
            retained["mapOverrides"]["0:0:1:2"] = Json(77);
            if (export_native_state(g, t, retained, gam.data(), gam.size(), encoded, side) ==
                    save::Error::None &&
                encoded[0x204] == 89 && encoded[0x205] == 1 && encoded[0xfff] == 0xab &&
                side["gameState"]["mapOverrides"]["0:0:1:2"].integer() == 77)
                ++passed;
            if (load_native_state(gam.data(), 1, nullptr, g, t, retained, source) == save::Error::ShortGam &&
                g.gold == 345)
                ++passed;
            std::vector<uint8_t> envelope = {1, 2, 3};
            if (write_envelope(gam, huge, empty_sidecar(), envelope) == save::Error::Capacity &&
                envelope == std::vector<uint8_t>({1, 2, 3}))
                ++passed;
            auto tooWide = retained;
            tooWide["food"] = Json(65536);
            if (restore_core(tooWide, g, t) == save::Error::NativeDomain && g.gold == 345)
                ++passed;
            r["passed"] = Json(passed);
        } else if (op == u"sizes") {
            r["GameState"] = Json(int(sizeof(GameState)));
            r["TurnState"] = Json(int(sizeof(TurnState)));
            r["Json"] = Json(int(sizeof(Json)));
            r["Generation"] = Json(int(sizeof(Generation)));
        } else
            return 4;
        out << write_json(r) << '\n';
    }
    return in.bad() || !out ? 5 : 0;
}
