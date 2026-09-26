#include "alpha_audio.h"

#include <cstdio>
#include <memory>
#include <new>

namespace tdeck {

openu5::AudioPackInfo load_audio_pack_info(const char *path) {
    openu5::AudioPackInfo missing{};
    std::FILE *file = path ? std::fopen(path, "rb") : nullptr;
    if (!file) return missing;
    long size = -1;
    if (std::fseek(file, 0, SEEK_END) == 0) size = std::ftell(file);
    if (size <= 0 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return missing;
    }
    if (static_cast<unsigned long>(size) > openu5::kAudioPackMaxBytes) {
        std::fclose(file);
        openu5::AudioPackInfo too_large{};
        too_large.state = openu5::AudioPackState::NotAudioPack;
        too_large.file_size = uint32_t(size);
        return too_large;
    }
    // At most 512 KiB, freed before returning: the pack is validated whole
    // (every CRC) and only its capability record is kept. A3-04 will keep the
    // song and timbre bytes when it has a player for them.
    std::unique_ptr<uint8_t[]> bytes(new (std::nothrow) uint8_t[size_t(size)]);
    if (!bytes) {
        std::fclose(file);
        openu5::AudioPackInfo unread{};
        unread.state = openu5::AudioPackState::Corrupt;
        unread.file_size = uint32_t(size);
        return unread;
    }
    const size_t read = std::fread(bytes.get(), 1, size_t(size), file);
    std::fclose(file);
    return openu5::inspect_audio_pack(bytes.get(), read);
}

} // namespace tdeck
