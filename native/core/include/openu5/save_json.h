#pragma once
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
namespace openu5::save {
// A save document, not a second live game. Unknown JSON fields survive edits.
// UTF-16 matches JSON.parse's string domain, including escaped lone surrogates.
struct Json {
    enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
    double number = 0;
    std::u16string string;
    std::vector<Json> values;
    std::vector<std::u16string> keys;
    Json() = default;
    template <class T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    Json(T v) : kind(Number), number(double(v)) {}
    Json(double v) : kind(Number), number(v) {}
    Json(bool v) : kind(Bool), number(v ? 1 : 0) {}
    Json(const char *v);
    static Json array();
    static Json object();
    bool has(const char *) const;
    Json &operator[](const char *);
    const Json &operator[](const char *) const;
    const Json &at(size_t) const;
    void erase(const char *);
    int64_t integer(int64_t fallback = 0) const;
    bool truth() const;
};
constexpr size_t kMaxJsonBytes = 256 * 1024, kMaxJsonNodes = 8192, kMaxJsonDepth = 32;
enum class JsonError { None, Syntax, Capacity };
JsonError parse_json(const std::string &, Json &);
std::string write_json(const Json &);
// Checked writer used by save orchestration. Capacity failure leaves out intact.
JsonError encode_json(const Json &, std::string &out);
} // namespace openu5::save
