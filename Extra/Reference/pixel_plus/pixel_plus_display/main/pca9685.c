#include "pca9685.h"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PCA9685_TAG "pca9685"

#define PCA9685_I2C_PORT          I2C_NUM_0
#define PCA9685_I2C_SDA           GPIO_NUM_17
#define PCA9685_I2C_SCL           GPIO_NUM_18
#define PCA9685_I2C_SPEED_HZ      400000
#define PCA9685_I2C_TIMEOUT_MS    100
#define PCA9685_I2C_ADDRESS       0x40

#define PCA9685_MODE1             0x00
#define PCA9685_MODE2             0x01
#define PCA9685_PRE_SCALE         0xFE
#define PCA9685_LED0_ON_L         0x06

#define PCA9685_MODE1_RESTART     0x80
#define PCA9685_MODE1_AUTO_INC    0x20
#define PCA9685_MODE1_SLEEP       0x10
#define PCA9685_MODE2_OUTDRV      0x04

#define PCA9685_OSCILLATOR_HZ     25000000UL
#define PCA9685_PWM_STEPS         4096UL
#define PCA9685_PWM_FREQ_HZ       50UL
#define PCA9685_SERVO_MIN_US      500U
#define PCA9685_SERVO_MAX_US      2500U

static i2c_master_bus_handle_t s_bus_handle;
static i2c_master_dev_handle_t s_dev_handle;
static bool s_ready;

static esp_err_t pca9685_write_bytes(const uint8_t *data, size_t len)
{
    if (s_dev_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit(s_dev_handle, data, len, PCA9685_I2C_TIMEOUT_MS);
}

static esp_err_t pca9685_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };
    return pca9685_write_bytes(data, sizeof(data));
}

static esp_err_t pca9685_set_pwm_ticks(uint8_t channel, uint16_t on_ticks, uint16_t off_ticks)
{
    if (channel >= 16U) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[5] = {
        (uint8_t)(PCA9685_LED0_ON_L + channel * 4U),
        (uint8_t)(on_ticks & 0xFFU),
        (uint8_t)((on_ticks >> 8) & 0x0FU),
        (uint8_t)(off_ticks & 0xFFU),
        (uint8_t)((off_ticks >> 8) & 0x0FU),
    };
    return pca9685_write_bytes(data, sizeof(data));
}

static uint8_t pca9685_calc_prescale(uint32_t pwm_freq_hz)
{
    uint32_t denom = pwm_freq_hz * PCA9685_PWM_STEPS;
    uint32_t rounded = (PCA9685_OSCILLATOR_HZ + (denom / 2U)) / denom;

    if (rounded == 0U) {
        rounded = 1U;
    }
    return (uint8_t)(rounded - 1U);
}

static uint16_t pca9685_angle_to_ticks(uint16_t angle_deg)
{
    if (angle_deg > 180U) {
        angle_deg = 180U;
    }

    uint32_t pulse_us = PCA9685_SERVO_MIN_US +
                        ((uint32_t)(PCA9685_SERVO_MAX_US - PCA9685_SERVO_MIN_US) * angle_deg + 90U) / 180U;
    uint32_t period_us = 1000000UL / PCA9685_PWM_FREQ_HZ;
    uint32_t ticks = (pulse_us * PCA9685_PWM_STEPS + (period_us / 2U)) / period_us;

    if (ticks >= PCA9685_PWM_STEPS) {
        ticks = PCA9685_PWM_STEPS - 1U;
    }
    return (uint16_t)ticks;
}

static void pca9685_deinit_handles(void)
{
    if (s_dev_handle != NULL) {
        i2c_master_bus_rm_device(s_dev_handle);
        s_dev_handle = NULL;
    }
    if (s_bus_handle != NULL) {
        i2c_del_master_bus(s_bus_handle);
        s_bus_handle = NULL;
    }
    s_ready = false;
}

esp_err_t pca9685_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_cfg = { 0 };
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.i2c_port = PCA9685_I2C_PORT;
    bus_cfg.sda_io_num = PCA9685_I2C_SDA;
    bus_cfg.scl_io_num = PCA9685_I2C_SCL;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(PCA9685_TAG, "Create I2C bus failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCA9685_I2C_ADDRESS,
        .scl_speed_hz = PCA9685_I2C_SPEED_HZ,
    };
    ret = i2c_master_bus_add_device(s_bus_handle, &dev_cfg, &s_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(PCA9685_TAG, "Add PCA9685 device failed: %s", esp_err_to_name(ret));
        pca9685_deinit_handles();
        return ret;
    }

    uint8_t prescale = pca9685_calc_prescale(PCA9685_PWM_FREQ_HZ);

    ret = pca9685_write_reg(PCA9685_MODE1, PCA9685_MODE1_SLEEP);
    if (ret == ESP_OK) {
        ret = pca9685_write_reg(PCA9685_PRE_SCALE, prescale);
    }
    if (ret == ESP_OK) {
        ret = pca9685_write_reg(PCA9685_MODE2, PCA9685_MODE2_OUTDRV);
    }
    if (ret == ESP_OK) {
        ret = pca9685_write_reg(PCA9685_MODE1, PCA9685_MODE1_AUTO_INC);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(PCA9685_TAG, "Configure PCA9685 failed: %s", esp_err_to_name(ret));
        pca9685_deinit_handles();
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(5));
    ret = pca9685_write_reg(PCA9685_MODE1, PCA9685_MODE1_RESTART | PCA9685_MODE1_AUTO_INC);
    if (ret != ESP_OK) {
        ESP_LOGE(PCA9685_TAG, "Wake PCA9685 failed: %s", esp_err_to_name(ret));
        pca9685_deinit_handles();
        return ret;
    }

    s_ready = true;
    ESP_LOGI(PCA9685_TAG, "PCA9685 ready on GPIO17/GPIO18");
    return ESP_OK;
}

bool pca9685_ready(void)
{
    return s_ready;
}

esp_err_t pca9685_set_servo_angle(uint8_t channel, uint16_t angle_deg)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (channel >= 16U) {
        return ESP_ERR_INVALID_ARG;
    }

    return pca9685_set_pwm_ticks(channel, 0U, pca9685_angle_to_ticks(angle_deg));
}
