#pragma once

#include <atomic>
#include <cstdint>

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "openu5/audio.h"

namespace tdeck {

// A3-01 proof of concept: the T-Deck Plus speaker, and nothing more.
//
// Output path (LilyGO's board definition, examples/UnitTest/utilities.h and
// examples/SimpleTone/SimpleTone.ino): an I2S-input speaker amplifier on
// BCK GPIO 7, WS GPIO 5, DOUT GPIO 6, no MCLK, no control bus, powered by the
// board's peripheral enable (GPIO 10, already driven high by Board). One slot
// of 16-bit PCM; the amplifier has no volume register, so the gain is applied
// to the samples.
//
// It renders ONLY SfxId::DiagnosticTone (Developer > Diagnostics > Audio test
// tone). Every gameplay request is declined, so the AudioService drops it and
// the game stays silent until A3-02 adds the effect synthesizer.
//
// Nothing here can stall the game loop: the I2S channel, its DMA buffers and
// the audio task are created on the first accepted request (a device that
// never plays the tone never touches the audio hardware), the game thread
// only posts to a queue with a zero timeout, and every blocking I2S write
// happens on the audio task, pinned to core 1 where no game task runs.
class TdeckAudioBackend final : public openu5::AudioBackend {
  public:
    static constexpr uint32_t kSampleRateHz = 16000;

    bool play_sfx(const openu5::SfxRequest &) override;
    void stop_sfx() override;
    bool start_music(openu5::MusicSong, uint16_t) override { return false; }
    void stop_music() override {}
    void set_gain(openu5::AudioChannel, uint16_t gain_q15) override;
    /** Empty until a bring-up step fails; then the ESP-IDF error of that step. */
    const char *last_error() const { return error_; }

  private:
    bool ensure_started();
    static void task_entry(void *);
    void run();
    void render_tone();

    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_ = nullptr;
    i2s_chan_handle_t tx_ = nullptr;
    std::atomic<uint16_t> sfx_gain_{0};
    std::atomic<bool> stop_{false};
    bool failed_ = false;
    char error_[64]{};
};

} // namespace tdeck
