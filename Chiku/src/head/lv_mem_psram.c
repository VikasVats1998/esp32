/* lv_mem_psram.c - LVGL allocator that keeps every LVGL object in PSRAM (LV_STDLIB_CUSTOM).
 * Internal RAM is scarce (Wi-Fi + the speech engine need most of it), so LVGL must not use it. */
#include "lvgl.h"
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM
#include "esp_heap_caps.h"

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void * mem, size_t bytes) { LV_UNUSED(mem); LV_UNUSED(bytes); return NULL; }
void lv_mem_remove_pool(lv_mem_pool_t pool) { LV_UNUSED(pool); }

void * lv_malloc_core(size_t size)
{
    void * p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_malloc(size, MALLOC_CAP_8BIT);     /* PSRAM full: fall back to internal */
}

void * lv_realloc_core(void * p, size_t new_size)
{
    void * q = heap_caps_realloc(p, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return q ? q : heap_caps_realloc(p, new_size, MALLOC_CAP_8BIT);
}

void lv_free_core(void * p) { heap_caps_free(p); }

void lv_mem_monitor_core(lv_mem_monitor_t * mon_p) { LV_UNUSED(mon_p); }
lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
#endif
