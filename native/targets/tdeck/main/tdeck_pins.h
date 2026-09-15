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

}  // namespace tdeck::pins
