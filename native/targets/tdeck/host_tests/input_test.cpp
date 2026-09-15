#include <array>
#include <cstdint>

#include "../main/input_controller.h"
#include "../main/keyboard_matrix.h"

using openu5::Direction;
using tdeck::KeyTransition;
using tdeck::KeyboardEvent;
using tdeck::KeyboardMatrix;
using tdeck::RawInputEvent;
using tdeck::RawInputKind;

namespace {

using Snapshot = std::array<uint8_t, tdeck::kKeyboardColumns>;

void set_key(Snapshot &snapshot, size_t column, size_t row, bool down)
{
    if (down) snapshot[column] |= static_cast<uint8_t>(1U << row);
    else snapshot[column] &= static_cast<uint8_t>(~(1U << row));
}

size_t apply(KeyboardMatrix &matrix, const Snapshot &snapshot,
             std::array<KeyboardEvent, tdeck::kKeyboardKeyCount> &events)
{
    return matrix.apply_snapshot(snapshot.data(), events.data(), events.size());
}

RawInputEvent raw_key(const KeyboardEvent &key, int64_t time)
{
    return {
        .kind = RawInputKind::Keyboard,
        .code = key.code,
        .transition = key.transition,
        .modifiers = key.modifiers,
        .column = key.column,
        .row = key.row,
        .modifier_key = key.modifier_key,
        .timestamp_us = time,
    };
}

}  // namespace

int main()
{
    KeyboardMatrix matrix;
    openu5::InputController controller;
    Snapshot snapshot{};
    std::array<KeyboardEvent, tdeck::kKeyboardKeyCount> events{};
    if (apply(matrix, snapshot, events) != 0) return __LINE__;

    Direction direction{};
    int64_t time = 100000;

    // Lowercase press/release survives intact and is never movement.
    set_key(snapshot, 0, 0, true); // Q
    if (apply(matrix, snapshot, events) != 1 || events[0].code != 'q' ||
        events[0].transition != KeyTransition::Pressed || events[0].modifier_key ||
        controller.normalize(raw_key(events[0], time++), direction)) return __LINE__;
    set_key(snapshot, 0, 0, false);
    if (apply(matrix, snapshot, events) != 1 || events[0].code != 'q' ||
        events[0].transition != KeyTransition::Released ||
        controller.normalize(raw_key(events[0], time++), direction)) return __LINE__;

    // Shift remains a distinct modifier event and changes the letter code.
    set_key(snapshot, 1, 6, true); // left Shift
    if (apply(matrix, snapshot, events) != 1 || !events[0].modifier_key ||
        !events[0].modifiers.shift) return __LINE__;
    set_key(snapshot, 0, 0, true);
    if (apply(matrix, snapshot, events) != 1 || events[0].code != 'Q' ||
        !events[0].modifiers.shift ||
        controller.normalize(raw_key(events[0], time++), direction)) return __LINE__;
    set_key(snapshot, 0, 0, false);
    if (apply(matrix, snapshot, events) != 1 || events[0].code != 'Q' ||
        events[0].transition != KeyTransition::Released) return __LINE__;
    set_key(snapshot, 1, 6, false);
    if (apply(matrix, snapshot, events) != 1 || matrix.modifiers().shift) return __LINE__;

    // Symbol itself is retained. Numeric and punctuation layer codes remain
    // available to future text/command handling, including press and release.
    set_key(snapshot, 0, 2, true); // Symbol
    if (apply(matrix, snapshot, events) != 1 || !events[0].modifier_key ||
        !events[0].modifiers.symbol || !matrix.modifiers().symbol) return __LINE__;
    set_key(snapshot, 1, 0, true); // Symbol+E -> '2'
    if (apply(matrix, snapshot, events) != 1 || events[0].code != '2' ||
        !events[0].modifiers.symbol ||
        controller.normalize(raw_key(events[0], time++), direction)) return __LINE__;
    set_key(snapshot, 1, 0, false);
    if (apply(matrix, snapshot, events) != 1 || events[0].code != '2' ||
        events[0].transition != KeyTransition::Released) return __LINE__;
    set_key(snapshot, 0, 3, true); // Symbol+A -> '*'
    if (apply(matrix, snapshot, events) != 1 || events[0].code != '*' ||
        !events[0].modifiers.symbol) return __LINE__;
    set_key(snapshot, 0, 3, false);
    apply(matrix, snapshot, events);
    set_key(snapshot, 0, 2, false);
    if (apply(matrix, snapshot, events) != 1 || matrix.modifiers().symbol) return __LINE__;

    // Alt is preserved independently; it does not alter or consume the code.
    set_key(snapshot, 0, 4, true); // Alt
    if (apply(matrix, snapshot, events) != 1 || !events[0].modifier_key ||
        !events[0].modifiers.alt) return __LINE__;
    set_key(snapshot, 0, 0, true);
    if (apply(matrix, snapshot, events) != 1 || events[0].code != 'q' ||
        !events[0].modifiers.alt ||
        controller.normalize(raw_key(events[0], time++), direction)) return __LINE__;
    set_key(snapshot, 0, 0, false);
    apply(matrix, snapshot, events);
    set_key(snapshot, 0, 4, false);
    if (apply(matrix, snapshot, events) != 1 || matrix.modifiers().alt) return __LINE__;

    // Only trackball edges normalize to directions; debounce stays per direction.
    const std::array<RawInputKind, 4> kinds = {RawInputKind::TrackballUp,
        RawInputKind::TrackballDown, RawInputKind::TrackballLeft, RawInputKind::TrackballRight};
    const std::array<Direction, 4> directions = {Direction::North, Direction::South,
        Direction::West, Direction::East};
    for (size_t i = 0; i < kinds.size(); ++i) {
        RawInputEvent track{.kind = kinds[i], .timestamp_us = 2000000};
        if (!controller.normalize(track, direction) || direction != directions[i]) return __LINE__;
        track.timestamp_us += 1000;
        if (controller.normalize(track, direction)) return __LINE__;
    }

    matrix.desynchronize();
    if (matrix.modifiers().symbol || matrix.modifiers().alt || matrix.modifiers().shift)
        return __LINE__;

    return 0;
}
