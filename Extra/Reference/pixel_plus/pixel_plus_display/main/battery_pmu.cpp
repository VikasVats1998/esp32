#include "battery_pmu.h"

#include <cstdio>
#include <cstring>

#include "sdkconfig.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

#if CONFIG_XPOWERS_CHIP_AXP2101

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

static const char *TAG_PMU = "battery_pmu";

static XPowersPMU s_pmu;
static bool s_ready;

#endif

extern "C" {

#if CONFIG_XPOWERS_CHIP_AXP2101

esp_err_t battery_pmu_init(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGW(TAG_PMU, "I2C bus not initialized yet");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_pmu.begin(bus, AXP2101_SLAVE_ADDRESS)) {
        ESP_LOGW(TAG_PMU, "AXP2101 not responding on I2C");
        return ESP_FAIL;
    }

    s_pmu.enableBattDetection();
    s_pmu.enableBattVoltageMeasure();
    s_ready = true;
    ESP_LOGI(TAG_PMU, "AXP2101 battery monitor ready");
    return ESP_OK;
}

bool battery_pmu_ready(void)
{
    return s_ready;
}

esp_err_t battery_pmu_format(char *buf, size_t buf_sz)
{
    if (buf == NULL || buf_sz == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ready) {
        buf[0] = '\0';
        return ESP_ERR_INVALID_STATE;
    }

    int pct = s_pmu.getBatteryPercent();
    if (pct >= 0 && pct <= 100) {
        if (s_pmu.isCharging()) {
            snprintf(buf, buf_sz, "%d%% charging", pct);
        } else {
            snprintf(buf, buf_sz, "%d%%", pct);
        }
        return ESP_OK;
    }

    uint16_t mv = s_pmu.getBattVoltage();
    if (mv >= 100) {
        snprintf(buf, buf_sz, "%u.%02u V", (unsigned)(mv / 1000U), (unsigned)((mv % 1000U) / 10U));
        return ESP_OK;
    }

    snprintf(buf, buf_sz, "--");
    return ESP_OK;
}

esp_err_t battery_pmu_get_level(int *pct, bool *charging)
{
    if (pct == NULL || charging == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    *charging = s_pmu.isCharging();

    int soc = s_pmu.getBatteryPercent();
    if (soc >= 0 && soc <= 100) {
        *pct = soc;
        return ESP_OK;
    }

    uint16_t mv = s_pmu.getBattVoltage();
    if (mv >= 3000 && mv <= 4300) {
        int est = (int)((mv - 3000) * 100 / 1200);
        if (est < 0) {
            est = 0;
        }
        if (est > 100) {
            est = 100;
        }
        *pct = est;
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

#else /* !CONFIG_XPOWERS_CHIP_AXP2101 */

esp_err_t battery_pmu_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

bool battery_pmu_ready(void)
{
    return false;
}

esp_err_t battery_pmu_format(char *buf, size_t buf_sz)
{
    if (buf != NULL && buf_sz > 0) {
        buf[0] = '\0';
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t battery_pmu_get_level(int *pct, bool *charging)
{
    (void)pct;
    (void)charging;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif

} /* extern "C" */
