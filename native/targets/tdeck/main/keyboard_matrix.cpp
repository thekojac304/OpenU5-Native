#include "keyboard_matrix.h"

#include <string.h>

namespace tdeck {
namespace {

// Physical matrix and symbol layer from LilyGO Keyboard_ESP32C3.ino.
constexpr uint8_t kBase[kKeyboardColumns][kKeyboardRows] = {
    {'q', 'w', 0, 'a', 0, ' ', 0},
    {'e', 's', 'd', 'p', 'x', 'z', 0},
    {'r', 'g', 't', 0, 'v', 'c', 'f'},
    {'u', 'h', 'y', '\r', 'b', 'n', 'j'},
    {'o', 'l', 'i', '\b', '$', 'm', 'k'},
};

constexpr uint8_t kSymbol[kKeyboardColumns][kKeyboardRows] = {
    {'#', '1', 0, '*', 0, 0, '0'},
    {'2', '4', '5', '@', '8', '7', 0},
    {'3', '/', '(', 0, '?', '9', '6'},
    {'_', ':', ')', 0, '!', ',', ';'},
    {'+', '"', '-', 0, 0, '.', '\''},
};

constexpr bool is_key_down(const uint8_t columns[kKeyboardColumns],
                           size_t column, size_t row)
{
    return (columns[column] & (1U << row)) != 0;
}

constexpr bool is_modifier(size_t column, size_t row)
{
    return (column == 0 && (row == 2 || row == 4)) ||
           (column == 1 && row == 6) || (column == 2 && row == 3);
}

KeyboardModifiers read_modifiers(const uint8_t columns[kKeyboardColumns])
{
    return {
        .symbol = is_key_down(columns, 0, 2),
        .alt = is_key_down(columns, 0, 4),
        .shift = is_key_down(columns, 1, 6) || is_key_down(columns, 2, 3),
    };
}

uint8_t resolve_code(size_t column, size_t row, KeyboardModifiers modifiers)
{
    uint8_t code = modifiers.symbol ? kSymbol[column][row] : kBase[column][row];
    if (modifiers.shift && code >= 'a' && code <= 'z') code -= 'a' - 'A';
    return code;
}

}  // namespace

size_t KeyboardMatrix::apply_snapshot(const uint8_t columns[kKeyboardColumns],
                                      KeyboardEvent *events, size_t capacity)
{
    uint8_t next[kKeyboardColumns]{};
    for (size_t column = 0; column < kKeyboardColumns; ++column) {
        next[column] = columns[column] & 0x7fU;
    }
    const KeyboardModifiers next_modifiers = read_modifiers(next);

    if (!synchronized_) {
        memcpy(columns_, next, sizeof(columns_));
        modifiers_ = next_modifiers;
        for (size_t column = 0; column < kKeyboardColumns; ++column) {
            for (size_t row = 0; row < kKeyboardRows; ++row) {
                active_codes_[column][row] = is_key_down(next, column, row)
                                                    ? resolve_code(column, row, next_modifiers)
                                                    : 0;
            }
        }
        synchronized_ = true;
        return 0;
    }

    size_t count = 0;
    for (size_t column = 0; column < kKeyboardColumns; ++column) {
        const uint8_t changed = columns_[column] ^ next[column];
        for (size_t row = 0; row < kKeyboardRows; ++row) {
            if ((changed & (1U << row)) == 0) continue;
            const bool pressed = is_key_down(next, column, row);
            const uint8_t code = pressed ? resolve_code(column, row, next_modifiers)
                                         : active_codes_[column][row];
            if (pressed) active_codes_[column][row] = code;
            else active_codes_[column][row] = 0;
            if (count < capacity && events != nullptr) {
                events[count] = {
                    .code = code,
                    .column = static_cast<uint8_t>(column),
                    .row = static_cast<uint8_t>(row),
                    .transition = pressed ? KeyTransition::Pressed : KeyTransition::Released,
                    .modifiers = next_modifiers,
                    .modifier_key = is_modifier(column, row),
                };
            }
            ++count;
        }
    }
    memcpy(columns_, next, sizeof(columns_));
    modifiers_ = next_modifiers;
    return count > capacity ? capacity : count;
}

void KeyboardMatrix::desynchronize()
{
    memset(columns_, 0, sizeof(columns_));
    memset(active_codes_, 0, sizeof(active_codes_));
    modifiers_ = {};
    synchronized_ = false;
}

const char *transition_name(KeyTransition transition)
{
    return transition == KeyTransition::Pressed ? "press" : "release";
}

}  // namespace tdeck
