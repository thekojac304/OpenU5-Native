// Alpha 3 A3-04A -- the ESP-IDF 6.1 i2s_std TX descriptor ring, modelled
// for the host (ALPHA3_AUDIO.md section 18.13). Shared by a3_04a_audio_stream
// and, since A3-04B, a3_04b_perf (section 19.11): moved here unchanged from
// the A3-04A test so both drive the production AudioRingPump against the
// same model.
#pragma once

#include "openu5/audio_stream.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <vector>

namespace openu5_test {
using namespace openu5;

// ===========================================================================
// The driver model. ESP-IDF 6.1 components/esp_driver_i2s/i2s_common.c, TX,
// DMA memory path, auto_clear_after_cb = true -- the configuration
// tdeck_audio.cpp uses. Line references are to that file.
//  - msg_queue holds finished descriptors, capacity desc_num - 1 (line 468).
//  - The EOF ISR (lines 825-867): on_sent; if the queue is full, drop its
//    OLDEST entry and call on_send_q_ovf; zero the finished buffer; push it.
//  - i2s_channel_write (1605-1650): take a descriptor from the queue when the
//    current one is full / unset / the queue is nearly full, waiting up to
//    the timeout; copy.
//  - i2s_channel_preload_data (1546-1600): READY state only; the first call
//    resets the queue to descriptors 1..n-1 and loads descriptor 0; loads
//    in order; says 0 bytes once all n hold data.
//  - enable (i2s_tx_channel_start, 244-265): DMA from descriptor 0, whatever
//    each holds. disable (1500-1540): stop, reset the queue, forget the
//    current descriptor; buffers keep their contents.
// Ground truth the driver cannot see is recorded on the side: which block
// every descriptor held when it played, and whether it was fresh.
// ===========================================================================
class VirtualI2sRing final : public PcmRingSink {
  public:
    struct Played {
        uint32_t id = 0; // written block id (1-based), 0 = not fresh
        bool silent = true;
        uint64_t start_us = 0;
    };

    explicit VirtualI2sRing(size_t n, uint32_t block_us = kAudioBlockUs) : n_(n), block_us_(block_us), desc_(n) {
        for (auto &d : desc_) d.pcm.assign(kAudioBlockFrames, 0);
    }

    bool preload(const int16_t *block) override {
        if (enabled_) return false;
        if (curr_ < 0) {
            queue_.clear();
            for (size_t i = 1; i < n_; ++i) queue_.push_back(i);
            curr_ = 0;
            curr_full_ = false;
        }
        if (curr_full_) {
            if (queue_.empty()) return false;
            curr_ = int(queue_.front());
            queue_.pop_front();
            curr_full_ = false;
        }
        load(size_t(curr_), block);
        ++preloads;
        return true;
    }
    bool enable() override {
        if (enabled_) return false;
        ++enables;
        if (fail_enable) return false;
        enabled_ = true;
        playing_ = 0;
        block_start_ = now_;
        return true;
    }
    void disable() override {
        if (!enabled_) return;
        ++disables;
        for (auto &d : desc_) {
            if (d.fresh) {
                cut_ids.push_back(d.id);
                if (!is_silent(d.pcm)) ++cut_real_blocks; // real audio that never (fully) played
            }
            d.fresh = false; // contents stay, exactly like the hardware
        }
        enabled_ = false;
        queue_.clear();
        curr_ = -1;
        curr_full_ = false;
    }
    bool write(const int16_t *block) override {
        if (!enabled_) return false;
        const bool need = curr_ < 0 || curr_full_ || (n_ - 1 - queue_.size()) <= 1;
        if (need) {
            uint64_t waited = 0;
            while (queue_.empty()) {
                const uint64_t eof = block_start_ + block_us_;
                if (waited + (eof - now_) > timeout_us) {
                    advance(timeout_us - waited);
                    ++write_timeouts;
                    return false;
                }
                waited += eof - now_;
                advance(eof - now_);
            }
            curr_ = int(queue_.front());
            queue_.pop_front();
        }
        load(size_t(curr_), block);
        ++writes;
        return true;
    }
    uint32_t blocks_played() const override { return played_; }
    uint32_t underrun_events() const override { return overflows_; }

    /** The producer is busy or stalled for `us`; the DMA keeps playing. */
    void advance(uint64_t us) {
        const uint64_t target = now_ + us;
        while (enabled_ && block_start_ + block_us_ <= target) eof();
        now_ = target;
    }
    uint64_t now() const { return now_; }
    bool enabled() const { return enabled_; }
    size_t descriptors() const { return n_; }

    std::vector<std::vector<int16_t>> written; // every block handed over, by id - 1
    std::vector<Played> played;                // every descriptor the DMA finished while enabled
    std::vector<uint32_t> cut_ids;             // fresh blocks still queued when the channel was turned off
    uint32_t dry_plays = 0, stale_plays = 0, cut_real_blocks = 0;
    uint32_t preloads = 0, writes = 0, enables = 0, disables = 0, write_timeouts = 0;
    bool fail_enable = false;
    uint64_t timeout_us = 200000; // tdeck_audio.cpp's write timeout

  private:
    struct Desc {
        std::vector<int16_t> pcm;
        uint32_t id = 0;
        bool fresh = false;
    };
    static bool is_silent(const std::vector<int16_t> &pcm) {
        return std::all_of(pcm.begin(), pcm.end(), [](int16_t v) { return v == 0; });
    }
    void load(size_t index, const int16_t *block) {
        Desc &d = desc_[index];
        std::copy(block, block + kAudioBlockFrames, d.pcm.begin());
        written.emplace_back(block, block + kAudioBlockFrames);
        d.id = uint32_t(written.size());
        d.fresh = true;
        curr_full_ = true;
    }
    void eof() {
        Desc &d = desc_[playing_];
        const bool silent = is_silent(d.pcm);
        played.push_back({d.fresh ? d.id : 0u, silent, block_start_});
        if (!d.fresh) {
            if (silent) ++dry_plays;
            else ++stale_plays;
        }
        d.fresh = false;
        ++played_; // on_sent
        if (queue_.size() == n_ - 1) {
            queue_.pop_front();
            ++overflows_; // on_send_q_ovf
        }
        std::fill(d.pcm.begin(), d.pcm.end(), int16_t(0)); // auto_clear_after_cb
        d.id = 0;
        queue_.push_back(playing_);
        playing_ = (playing_ + 1) % n_;
        block_start_ += block_us_;
        now_ = block_start_;
    }

    size_t n_;
    uint64_t block_us_;
    std::vector<Desc> desc_;
    std::deque<size_t> queue_;
    int curr_ = -1;
    bool curr_full_ = false;
    bool enabled_ = false;
    size_t playing_ = 0;
    uint64_t now_ = 0, block_start_ = 0;
    uint32_t played_ = 0, overflows_ = 0;
};

inline uint64_t ring_clock(void *ctx) { return static_cast<VirtualI2sRing *>(ctx)->now(); }

} // namespace openu5_test
