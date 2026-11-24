#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t battery_pmu_init(void);
bool battery_pmu_ready(void);

/** Write human-readable status into buf (e.g. "87%" or "3.95 V" or "--"). */
esp_err_t battery_pmu_format(char *buf, size_t buf_sz);

/**
 * Battery level for UI (0–100). Uses SOC from PMU when valid; otherwise estimates from voltage.
 * On success sets *charging from the PMU.
 */
esp_err_t battery_pmu_get_level(int *pct, bool *charging);

#ifdef __cplusplus
}
#endif
