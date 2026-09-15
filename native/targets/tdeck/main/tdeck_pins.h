#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

namespace tdeck::pins {

// Official LilyGO T-Deck/T-Deck Plus shared SPI wiring.
inline constexpr spi_host_device_t kSharedSpiHost = SPI2_HOST;
inline constexpr gpio_num_t kPowerEnable = GPIO_NUM_10;
inline constexpr gpio_num_t kSpiMosi = GPIO_NUM_41;
inline constexpr gpio_num_t kSpiMiso = GPIO_NUM_38;
inline constexpr gpio_num_t kSpiClock = GPIO_NUM_40;
inline constexpr gpio_num_t kTftChipSelect = GPIO_NUM_12;
inline constexpr gpio_num_t kSdChipSelect = GPIO_NUM_39;
inline constexpr gpio_num_t kLoraChipSelect = GPIO_NUM_9;
inline constexpr gpio_num_t kTftDataCommand = GPIO_NUM_11;
inline constexpr gpio_num_t kTftBacklight = GPIO_NUM_42;
inline constexpr gpio_num_t kI2cData = GPIO_NUM_18;
inline constexpr gpio_num_t kI2cClock = GPIO_NUM_8;
inline constexpr gpio_num_t kKeyboardInterrupt = GPIO_NUM_46;
// LilyGO UnitTest mapping: G01=up, G03=down, G04=left, G02=right.
inline constexpr gpio_num_t kTrackballUp = GPIO_NUM_3;
inline constexpr gpio_num_t kTrackballDown = GPIO_NUM_15;
inline constexpr gpio_num_t kTrackballLeft = GPIO_NUM_1;
inline constexpr gpio_num_t kTrackballRight = GPIO_NUM_2;

}  // namespace tdeck::pins
