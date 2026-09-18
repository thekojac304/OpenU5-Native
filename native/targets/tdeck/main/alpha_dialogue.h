#pragma once

#include <cstddef>
#include <cstdint>

#include "openu5/dialogue.h"

namespace tdeck {

// One-script PSRAM cache. The compact corpus remains in the versioned SD pack;
// only the NPC currently being spoken to is expanded into native Talk* views.
class AlphaDialogueCache {
  public:
    ~AlphaDialogueCache();
    void bind(const uint8_t *data, size_t size) { data_ = data; size_ = size; }
    static const openu5::TalkScript *lookup(void *, openu5::TalkMaster, int32_t dialog);

  private:
    const uint8_t *data_ = nullptr;
    size_t size_ = 0;
    uint8_t *arena_ = nullptr;
    size_t capacity_ = 0, used_ = 0;
    openu5::TalkScript script_{};
    openu5::TalkMaster master_ = openu5::TalkMaster::None;
    int32_t dialog_ = 0;

    void clear();
    const openu5::TalkScript *load(openu5::TalkMaster, int32_t);
    void *allocate(size_t, size_t alignment);
};

} // namespace tdeck
