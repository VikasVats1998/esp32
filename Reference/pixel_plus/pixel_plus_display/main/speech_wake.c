#include "speech_wake.h"

#include "esp_afe_config.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "model_path.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "speech_wake";

#define SPEECH_FEED_CORE  0
#define SPEECH_FETCH_CORE 0
#define SPEECH_FEED_PRIO  6
#define SPEECH_FETCH_PRIO 5
#define SPEECH_MN_TIMEOUT_MS 6000
#define SPEECH_WN_THRESHOLD  0.55f

static esp_codec_dev_handle_t s_mic;
static esp_afe_sr_iface_t *s_afe;
static esp_afe_sr_data_t *s_afe_data;
static srmodel_list_t *s_models;
static esp_mn_iface_t *s_multinet;
static model_iface_data_t *s_mn_data;
static TaskHandle_t s_feed_task;
static TaskHandle_t s_fetch_task;
static volatile bool s_running;
static volatile bool s_wakeup_flag;
static speech_wake_command_cb_t s_command_cb;
static speech_wake_wakeup_cb_t s_wakeup_cb;

void speech_wake_set_command_cb(speech_wake_command_cb_t cb)
{
    s_command_cb = cb;
}

void speech_wake_set_wakeup_cb(speech_wake_wakeup_cb_t cb)
{
    s_wakeup_cb = cb;
}

static esp_err_t speech_mn_init(void)
{
    char *mn_name = esp_srmodel_filter(s_models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    if (mn_name == NULL) {
        ESP_LOGE(TAG, "MultiNet model not found in 'model' partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MultiNet model: %s", mn_name);

    s_multinet = esp_mn_handle_from_name(mn_name);
    if (s_multinet == NULL) {
        ESP_LOGE(TAG, "esp_mn_handle_from_name failed");
        return ESP_FAIL;
    }

    s_mn_data = s_multinet->create(mn_name, SPEECH_MN_TIMEOUT_MS);
    if (s_mn_data == NULL) {
        ESP_LOGE(TAG, "MultiNet create failed");
        return ESP_FAIL;
    }

    const int afe_chunksize = s_afe->get_fetch_chunksize(s_afe_data);
    const int mn_chunksize = s_multinet->get_samp_chunksize(s_mn_data);
    if (afe_chunksize != mn_chunksize) {
        ESP_LOGE(TAG, "AFE/MultiNet chunk mismatch: afe=%d mn=%d", afe_chunksize, mn_chunksize);
        s_multinet->destroy(s_mn_data);
        s_mn_data = NULL;
        return ESP_FAIL;
    }

    if (s_multinet->switch_loader_mode != NULL) {
        s_mn_data = s_multinet->switch_loader_mode(s_mn_data, ESP_MN_LOAD_FROM_PSRAM_FLASH);
    }

    ESP_LOGI(TAG, "internal heap before MultiNet commands: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    ESP_ERROR_CHECK(esp_mn_commands_alloc(s_multinet, s_mn_data));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_WHAT_TIME_IS_IT, "what time is it"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_START_TIMER, "start the timer"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_SHOW_DANCE, "do a dance"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_ARE_YOU_DIZZY, "are you dizzy"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_STOP_TIMER, "stop the timer"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_WALK_FORWARD, "walk forward"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_WALK_BACKWARD, "walk backward"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_TURN_LEFT, "turn left"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_TURN_RIGHT, "turn right"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_SPIN, "do a spin"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_JUMP, "do a jump"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_SNEAK, "sneak around"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_PATROL, "patrol"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_RUN_AWAY, "get back"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_MARCH, "march"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_LOW_POWER, "go to sleep"));
    ESP_ERROR_CHECK(esp_mn_commands_add(SPEECH_CMD_LOW_POWER, "low power mode"));

    esp_mn_error_t *mn_err = esp_mn_commands_update();
    if (mn_err != NULL) {
        ESP_LOGE(TAG, "MultiNet command update failed");
        esp_mn_commands_free();
        s_multinet->destroy(s_mn_data);
        s_mn_data = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "internal heap after MultiNet commands: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    return ESP_OK;
}

static void speech_feed_task(void *arg)
{
    (void)arg;

    const int chunk = s_afe->get_feed_chunksize(s_afe_data);
    const int channels = s_afe->get_feed_channel_num(s_afe_data);
    const size_t buf_bytes = (size_t)chunk * (size_t)channels * sizeof(int16_t);
    int16_t *buf = heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf == NULL) {
        ESP_LOGE(TAG, "feed buffer alloc failed (%u bytes)", (unsigned)buf_bytes);
        vTaskDelete(NULL);
        return;
    }

    while (s_running) {
        int ret = esp_codec_dev_read(s_mic, buf, (int)buf_bytes);
        if (ret != ESP_CODEC_DEV_OK) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        s_afe->feed(s_afe_data, buf);
    }

    heap_caps_free(buf);
    vTaskDelete(NULL);
}

static void speech_fetch_task(void *arg)
{
    (void)arg;

    while (s_running) {
        afe_fetch_result_t *result = s_afe->fetch(s_afe_data);
        if (result == NULL || result->ret_value != ESP_OK) {
            continue;
        }

        if (result->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "Wake word detected: jarvis");
            s_multinet->clean(s_mn_data);
            s_wakeup_flag = true;
            if (s_wakeup_cb != NULL) {
                s_wakeup_cb();
            }
        }

        if (s_wakeup_flag) {
            esp_mn_state_t mn_state = s_multinet->detect(s_mn_data, result->data);
            if (mn_state == ESP_MN_STATE_DETECTED) {
                esp_mn_results_t *mn = s_multinet->get_results(s_mn_data);
                ESP_LOGI(TAG, "Command activated: id=%d text=\"%s\" prob=%.2f",
                         mn->command_id[0], mn->string, mn->prob[0]);
                if (s_command_cb != NULL) {
                    s_command_cb(mn->command_id[0]);
                }
            } else if (mn_state == ESP_MN_STATE_TIMEOUT) {
                ESP_LOGI(TAG, "Command window timed out");
                s_afe->enable_wakenet(s_afe_data);
                s_wakeup_flag = false;
            }
        }
    }

    vTaskDelete(NULL);
}

esp_err_t speech_wake_start(esp_codec_dev_handle_t mic)
{
    if (mic == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_running) {
        return ESP_ERR_INVALID_STATE;
    }

    s_mic = mic;
    s_wakeup_flag = false;

    s_models = esp_srmodel_init("model");
    if (s_models == NULL) {
        ESP_LOGE(TAG, "failed to load speech models from 'model' partition");
        return ESP_FAIL;
    }

    afe_config_t *cfg = afe_config_init("M", s_models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    if (cfg == NULL) {
        ESP_LOGE(TAG, "afe_config_init failed");
        esp_srmodel_deinit(s_models);
        s_models = NULL;
        return ESP_FAIL;
    }
    cfg->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    cfg->vad_init = false;
    cfg->ns_init = false;
    cfg->wakenet_mode = DET_MODE_95;
    cfg->afe_ringbuf_size = 5;
    cfg = afe_config_check(cfg);

    ESP_LOGI(TAG, "internal heap before AFE: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    s_afe = (esp_afe_sr_iface_t *)esp_afe_handle_from_config(cfg);
    s_afe_data = s_afe->create_from_config(cfg);

    ESP_LOGI(TAG, "internal heap after AFE: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    afe_config_free(cfg);
    if (s_afe_data == NULL) {
        ESP_LOGE(TAG, "AFE create_from_config failed");
        esp_srmodel_deinit(s_models);
        s_models = NULL;
        return ESP_FAIL;
    }

    if (s_afe->set_wakenet_threshold(s_afe_data, 1, SPEECH_WN_THRESHOLD) != 1) {
        ESP_LOGW(TAG, "set_wakenet_threshold failed");
    } else {
        ESP_LOGI(TAG, "wake word threshold set to %.2f", SPEECH_WN_THRESHOLD);
    }

    esp_err_t mn_err = speech_mn_init();
    if (mn_err != ESP_OK) {
        speech_wake_stop();
        return mn_err;
    }

    s_running = true;

    BaseType_t feed_ok = xTaskCreatePinnedToCore(
        speech_feed_task, "speech_feed", 6144, NULL, SPEECH_FEED_PRIO, &s_feed_task, SPEECH_FEED_CORE);
    BaseType_t fetch_ok = xTaskCreatePinnedToCore(
        speech_fetch_task, "speech_fetch", 6144, NULL, SPEECH_FETCH_PRIO, &s_fetch_task, SPEECH_FETCH_CORE);
    if (feed_ok != pdPASS || fetch_ok != pdPASS) {
        ESP_LOGE(TAG, "failed to create speech tasks");
        speech_wake_stop();
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "started");
    return ESP_OK;
}

void speech_wake_stop(void)
{
    s_running = false;
    s_wakeup_flag = false;

    if (s_fetch_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(50));
        s_fetch_task = NULL;
    }
    if (s_feed_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(50));
        s_feed_task = NULL;
    }

    esp_mn_commands_free();

    if (s_mn_data != NULL && s_multinet != NULL) {
        s_multinet->destroy(s_mn_data);
        s_mn_data = NULL;
    }
    s_multinet = NULL;

    if (s_afe_data != NULL && s_afe != NULL) {
        s_afe->destroy(s_afe_data);
        s_afe_data = NULL;
    }
    s_afe = NULL;

    if (s_models != NULL) {
        esp_srmodel_deinit(s_models);
        s_models = NULL;
    }

    s_mic = NULL;
}
