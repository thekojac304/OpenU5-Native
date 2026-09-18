#pragma once

#include <stddef.h>
#include <stdint.h>

namespace tdeck {

constexpr size_t kKeyboardColumns = 5;
constexpr size_t kKeyboardRows = 7;
constexpr size_t kKeyboardKeyCount = kKeyboardColumns * kKeyboardRows;

// Authoritative T-Deck Plus hardware position for the physical Mic/0 key.
// Its base-layer code is zero and its Symbol-layer code is '0', so device UI
// code must bind this position before any character/layer translation.
constexpr uint8_t kMicrophoneKeyColumn = 0;
constexpr uint8_t kMicrophoneKeyRow = 6;

struct KeyboardModifiers {
    bool symbol = false;
    bool alt = false;
    bool shift = false;
};

enum class KeyTransition : uint8_t { Pressed, Released };

struct KeyboardEvent {
    uint8_t code = 0;
    uint8_t column = 0;
    uint8_t row = 0;
    KeyTransition transition = KeyTransition::Released;
    KeyboardModifiers modifiers{};
    bool modifier_key = false;
    uint8_t base_code = 0;
    uint8_t symbol_code = 0;
    uint8_t snapshot[kKeyboardColumns]{};
    uint8_t previous_snapshot[kKeyboardColumns]{};
};

/** Converts authoritative five-column matrix snapshots into key edges. */
class KeyboardMatrix {
public:
    size_t apply_snapshot(const uint8_t columns[kKeyboardColumns],
                          KeyboardEvent *events, size_t capacity);
    void desynchronize();
    bool synchronized() const { return synchronized_; }
    KeyboardModifiers modifiers() const { return modifiers_; }
    const uint8_t *columns() const { return columns_; }

private:
    uint8_t columns_[kKeyboardColumns]{};
    uint8_t active_codes_[kKeyboardColumns][kKeyboardRows]{};
    KeyboardModifiers modifiers_{};
    bool synchronized_ = false;
};

const char *transition_name(KeyTransition transition);

}  // namespace tdeck
