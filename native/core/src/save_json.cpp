#include "openu5/save_json.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
namespace openu5::save {
namespace {
std::u16string key(const char *s) {
    std::u16string r;
    while (*s)
        r += char16_t(uint8_t(*s++));
    return r;
}
const Json missing;
} // namespace
Json::Json(const char *v) : kind(String), string(key(v)) {}
Json Json::array() {
    Json j;
    j.kind = Array;
    return j;
}
Json Json::object() {
    Json j;
    j.kind = Object;
    return j;
}
bool Json::has(const char *s) const { return std::find(keys.begin(), keys.end(), key(s)) != keys.end(); }
Json &Json::operator[](const char *s) {
    auto k = key(s);
    auto it = std::find(keys.begin(), keys.end(), k);
    if (it != keys.end())
        return values[size_t(it - keys.begin())];
    if (kind != Object) {
        *this = object();
    }
    keys.push_back(k);
    values.emplace_back();
    return values.back();
}
const Json &Json::operator[](const char *s) const {
    auto it = std::find(keys.begin(), keys.end(), key(s));
    return it == keys.end() ? missing : values[size_t(it - keys.begin())];
}
const Json &Json::at(size_t i) const { return i < values.size() ? values[i] : missing; }
void Json::erase(const char *s) {
    auto it = std::find(keys.begin(), keys.end(), key(s));
    if (it != keys.end()) {
        values.erase(values.begin() + (it - keys.begin()));
        keys.erase(it);
    }
}
int64_t Json::integer(int64_t fallback) const {
    return kind == Number && std::isfinite(number) && number >= -9007199254740991.0 &&
                   number <= 9007199254740991.0
               ? int64_t(number)
               : fallback;
}
bool Json::truth() const {
    return kind == Array || kind == Object || (kind == String ? !string.empty() : number != 0);
}
namespace {
struct Parser {
    const std::string &s;
    size_t p = 0, nodes = 0;
    JsonError error = JsonError::None;
    void ws() {
        while (p < s.size() && (s[p] == ' ' || s[p] == '\r' || s[p] == '\n' || s[p] == '\t'))
            ++p;
    }
    bool fail(JsonError e = JsonError::Syntax) {
        error = e;
        return false;
    }
    bool str(std::u16string &out) {
        if (p >= s.size() || s[p++] != '"')
            return fail();
        while (p < s.size()) {
            uint32_t c = uint8_t(s[p++]);
            if (c == '"')
                return true;
            if (c < 32)
                return fail();
            if (c == '\\') {
                if (p >= s.size())
                    return fail();
                c = uint8_t(s[p++]);
                if (c == 'u') {
                    c = 0;
                    for (int i = 0; i < 4; ++i) {
                        if (p >= s.size())
                            return fail();
                        char h = s[p++];
                        int n = h >= '0' && h <= '9'   ? h - '0'
                                : h >= 'a' && h <= 'f' ? h - 'a' + 10
                                : h >= 'A' && h <= 'F' ? h - 'A' + 10
                                                       : -1;
                        if (n < 0)
                            return fail();
                        c = c * 16 + uint32_t(n);
                    }
                } else if (c == 'b')
                    c = 8;
                else if (c == 'f')
                    c = 12;
                else if (c == 'n')
                    c = 10;
                else if (c == 'r')
                    c = 13;
                else if (c == 't')
                    c = 9;
                else if (c != '"' && c != '\\' && c != '/')
                    return fail();
            } else if (c >= 128) {
                // TextDecoder/File.text replacement-mode UTF-8: consume a
                // valid prefix, leave the offending byte for the next step.
                const uint32_t lead = c;
                int n = c >= 0xf0 && c <= 0xf4   ? 3
                        : c >= 0xe0 && c <= 0xef ? 2
                        : c >= 0xc2 && c <= 0xdf ? 1
                                                 : 0;
                if (!n)
                    c = 0xfffd;
                else {
                    c &= uint32_t((1 << (6 - n)) - 1);
                    for (int i = 0; i < n; ++i) {
                        const unsigned lo = i == 0 && lead == 0xe0   ? 0xa0
                                            : i == 0 && lead == 0xf0 ? 0x90
                                                                     : 0x80;
                        const unsigned hi = i == 0 && lead == 0xed   ? 0x9f
                                            : i == 0 && lead == 0xf4 ? 0x8f
                                                                     : 0xbf;
                        if (p >= s.size() || uint8_t(s[p]) < lo || uint8_t(s[p]) > hi) {
                            c = 0xfffd;
                            break;
                        }
                        c = (c << 6) | (uint8_t(s[p++]) & 63);
                    }
                }
            }
            if (c > 65535) {
                c -= 65536;
                out += char16_t(0xd800 + (c >> 10));
                out += char16_t(0xdc00 + (c & 1023));
            } else
                out += char16_t(c);
        }
        return fail();
    }
    bool value(Json &j, size_t depth) {
        if (++nodes > kMaxJsonNodes || depth > kMaxJsonDepth)
            return fail(JsonError::Capacity);
        ws();
        if (p >= s.size())
            return fail();
        char c = s[p];
        if (c == '"') {
            j.kind = Json::String;
            return str(j.string);
        }
        if (c == '{' || c == '[') {
            ++p;
            bool obj = c == '{';
            j = obj ? Json::object() : Json::array();
            ws();
            char end = obj ? '}' : ']';
            if (p < s.size() && s[p] == end) {
                ++p;
                return true;
            }
            while (p < s.size()) {
                std::u16string k;
                if (obj) {
                    if (!str(k))
                        return false;
                    ws();
                    if (p >= s.size() || s[p++] != ':')
                        return fail();
                }
                Json v;
                if (!value(v, depth + 1))
                    return false;
                if (obj) {
                    auto it = std::find(j.keys.begin(), j.keys.end(), k);
                    if (it != j.keys.end())
                        j.values[size_t(it - j.keys.begin())] = std::move(v);
                    else {
                        j.keys.push_back(k);
                        j.values.push_back(std::move(v));
                    }
                } else
                    j.values.push_back(std::move(v));
                ws();
                if (p >= s.size())
                    return fail();
                c = s[p++];
                if (c == end)
                    return true;
                if (c != ',')
                    return fail();
                ws();
            }
            return fail();
        }
        for (const auto *literal : {"null", "true", "false"}) {
            size_t n = std::char_traits<char>::length(literal);
            if (s.compare(p, n, literal) == 0) {
                p += n;
                if (literal[0] != 'n')
                    j = Json(literal[0] == 't');
                return true;
            }
        }
        size_t start = p;
        if (c == '-')
            ++p;
        if (p >= s.size())
            return fail();
        if (s[p] == '0')
            ++p;
        else {
            if (s[p] < '1' || s[p] > '9')
                return fail();
            while (p < s.size() && s[p] >= '0' && s[p] <= '9')
                ++p;
        }
        if (p < s.size() && s[p] == '.') {
            ++p;
            size_t b = p;
            while (p < s.size() && s[p] >= '0' && s[p] <= '9')
                ++p;
            if (b == p)
                return fail();
        }
        if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) {
            ++p;
            if (p < s.size() && (s[p] == '+' || s[p] == '-'))
                ++p;
            size_t b = p;
            while (p < s.size() && s[p] >= '0' && s[p] <= '9')
                ++p;
            if (b == p)
                return fail();
        }
        j = Json(std::strtod(s.substr(start, p - start).c_str(), nullptr));
        return true;
    }
};
void quote(std::string &s, const std::u16string &v) {
    s += '"';
    for (char16_t c : v) {
        if (c >= 32 && c < 127 && c != '"' && c != '\\')
            s += char(c);
        else {
            char b[7];
            std::snprintf(b, sizeof(b), "\\u%04x", unsigned(c));
            s += b;
        }
    }
    s += '"';
}
void emit(std::string &s, const Json &j) {
    switch (j.kind) {
    case Json::Null:
        s += "null";
        break;
    case Json::Bool:
        s += j.truth() ? "true" : "false";
        break;
    case Json::Number: {
        char b[32];
        if (!std::isfinite(j.number))
            s += "null";
        else {
            std::snprintf(b, sizeof(b), "%.17g", j.number == 0 ? 0 : j.number);
            s += b;
        }
        break;
    }
    case Json::String:
        quote(s, j.string);
        break;
    case Json::Array:
    case Json::Object: {
        bool obj = j.kind == Json::Object;
        s += obj ? '{' : '[';
        for (size_t i = 0; i < j.values.size(); ++i) {
            if (i)
                s += ',';
            if (obj) {
                quote(s, j.keys[i]);
                s += ':';
            }
            emit(s, j.values[i]);
        }
        s += obj ? '}' : ']';
        break;
    }
    }
}
} // namespace
JsonError parse_json(const std::string &s, Json &out) {
    if (s.size() > kMaxJsonBytes)
        return JsonError::Capacity;
    Parser p{s};
    Json j;
    if (!p.value(j, 0)) {
        return p.error;
    }
    p.ws();
    if (p.p != s.size())
        return JsonError::Syntax;
    out = std::move(j);
    return JsonError::None;
}
std::string write_json(const Json &j) {
    std::string s;
    emit(s, j);
    return s;
}
namespace {
size_t quoted_size(const std::u16string &v) {
    size_t n = 2;
    for (auto c : v) {
        n += c >= 32 && c < 127 && c != '"' && c != '\\' ? 1 : 6;
        if (n > kMaxJsonBytes)
            return n;
    }
    return n;
}
bool budget(const Json &j, size_t depth, size_t &nodes, size_t &bytes) {
    if (++nodes > kMaxJsonNodes || depth > kMaxJsonDepth)
        return false;
    switch (j.kind) {
    case Json::Null:
        bytes += 4;
        break;
    case Json::Bool:
        bytes += j.truth() ? 4 : 5;
        break;
    case Json::String:
        bytes += quoted_size(j.string);
        break;
    case Json::Number: {
        char b[32];
        bytes += std::isfinite(j.number)
                     ? size_t(std::snprintf(b, sizeof(b), "%.17g", j.number == 0 ? 0 : j.number))
                     : 4;
        break;
    }
    case Json::Array:
    case Json::Object:
        bytes += 2;
        if (j.kind == Json::Object && j.keys.size() != j.values.size())
            return false;
        for (size_t i = 0; i < j.values.size(); ++i) {
            if (i)
                ++bytes;
            if (j.kind == Json::Object)
                bytes += quoted_size(j.keys[i]) + 1;
            if (bytes > kMaxJsonBytes || !budget(j.values[i], depth + 1, nodes, bytes))
                return false;
        }
        break;
    }
    return bytes <= kMaxJsonBytes;
}
} // namespace
JsonError encode_json(const Json &j, std::string &out) {
    size_t nodes = 0, bytes = 0;
    if (!budget(j, 0, nodes, bytes))
        return JsonError::Capacity;
    out = write_json(j);
    return JsonError::None;
}
} // namespace openu5::save
