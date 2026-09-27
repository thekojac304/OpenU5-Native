#pragma once
// Alpha 3 A3-04E host-test seam: ESP-IDF's driver/ledc.h (the backlight PWM),
// as far as tdeck_board.cpp uses it. Field order follows ESP-IDF 6.1.
#include <cstdint>

#include "driver/gpio.h"
#include "esp_err.h"

typedef enum { LEDC_LOW_SPEED_MODE = 0 } ledc_mode_t;
typedef enum { LEDC_TIMER_10_BIT = 10 } ledc_timer_bit_t;
typedef enum { LEDC_TIMER_0 = 0 } ledc_timer_t;
typedef enum { LEDC_AUTO_CLK = 0 } ledc_clk_cfg_t;
typedef enum { LEDC_CHANNEL_0 = 0 } ledc_channel_t;
typedef enum { LEDC_INTR_DISABLE = 0 } ledc_intr_type_t;
typedef enum { LEDC_SLEEP_MODE_NO_ALIVE_NO_PD = 0 } ledc_sleep_mode_t;

typedef struct {
    ledc_mode_t speed_mode;
    ledc_timer_bit_t duty_resolution;
    ledc_timer_t timer_num;
    uint32_t freq_hz;
    ledc_clk_cfg_t clk_cfg;
    bool deconfigure;
} ledc_timer_config_t;

typedef struct {
    int gpio_num;
    ledc_mode_t speed_mode;
    ledc_channel_t channel;
    ledc_intr_type_t intr_type;
    ledc_timer_t timer_sel;
    uint32_t duty;
    int hpoint;
    ledc_sleep_mode_t sleep_mode;
    struct {
        unsigned int output_invert : 1;
    } flags;
    bool deconfigure;
} ledc_channel_config_t;

esp_err_t ledc_timer_config(const ledc_timer_config_t *config);
esp_err_t ledc_channel_config(const ledc_channel_config_t *config);
esp_err_t ledc_set_duty(ledc_mode_t mode, ledc_channel_t channel, uint32_t duty);
esp_err_t ledc_update_duty(ledc_mode_t mode, ledc_channel_t channel);
