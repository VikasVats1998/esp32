#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCA9685_SERVO_CHANNEL_COUNT 4U

esp_err_t pca9685_init(void);
bool pca9685_ready(void);
esp_err_t pca9685_set_servo_angle(uint8_t channel, uint16_t angle_deg);

#ifdef __cplusplus
}
#endif
