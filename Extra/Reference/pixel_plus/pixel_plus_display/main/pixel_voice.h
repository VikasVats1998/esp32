#pragma once

#include "esp_codec_dev.h"
#include "esp_err.h"
#include "robot_sound.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PIXEL_CHIRP_NONE = 0,
    PIXEL_CHIRP_LISTEN,
    PIXEL_CHIRP_WAKE_UP,
    PIXEL_CHIRP_MOTION_STEP,
    PIXEL_CHIRP_MOTION_STEP_ALT,
    PIXEL_CHIRP_MOTION_STEP_QUIET,
    PIXEL_CHIRP_MOTION_STEP_FAST,
    PIXEL_CHIRP_MOTION_STEP_MARCH,
    PIXEL_CHIRP_MOTION_TURN,
    PIXEL_CHIRP_MOTION_SPIN,
    PIXEL_CHIRP_MOTION_DANCE,
    PIXEL_CHIRP_MOTION_DIZZY,
    PIXEL_CHIRP_MOTION_LAND,
    PIXEL_CHIRP_ALARM,
    PIXEL_CHIRP_ALARM_REPEAT,
} pixel_chirp_t;

esp_err_t pixel_voice_init(esp_codec_dev_handle_t speaker);
void pixel_voice_set_volume(int percent);
void pixel_voice_play(pixel_chirp_t chirp);
void pixel_voice_enqueue_robot(RobotSoundID sound_id);
bool pixel_voice_busy(void);

#ifdef __cplusplus
}
#endif
