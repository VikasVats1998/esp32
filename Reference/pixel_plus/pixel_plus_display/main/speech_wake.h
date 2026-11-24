#pragma once

#include "esp_codec_dev.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPEECH_CMD_WHAT_TIME_IS_IT    1
#define SPEECH_CMD_START_TIMER        2
#define SPEECH_CMD_SHOW_DANCE         3
#define SPEECH_CMD_ARE_YOU_DIZZY      4
#define SPEECH_CMD_STOP_TIMER         5
#define SPEECH_CMD_WALK_FORWARD       6
#define SPEECH_CMD_WALK_BACKWARD      7
#define SPEECH_CMD_TURN_LEFT          8
#define SPEECH_CMD_TURN_RIGHT         9
#define SPEECH_CMD_SPIN              10
#define SPEECH_CMD_JUMP              11
#define SPEECH_CMD_SNEAK             12
#define SPEECH_CMD_PATROL            13
#define SPEECH_CMD_RUN_AWAY          14
#define SPEECH_CMD_MARCH             15
#define SPEECH_CMD_LOW_POWER         16

typedef void (*speech_wake_command_cb_t)(int command_id);
typedef void (*speech_wake_wakeup_cb_t)(void);

void speech_wake_set_command_cb(speech_wake_command_cb_t cb);
void speech_wake_set_wakeup_cb(speech_wake_wakeup_cb_t cb);
esp_err_t speech_wake_start(esp_codec_dev_handle_t mic);
void speech_wake_stop(void);

#ifdef __cplusplus
}
#endif
