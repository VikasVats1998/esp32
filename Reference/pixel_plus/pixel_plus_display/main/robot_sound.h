#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ROBOT_SOUND_WAKE_JARVIS = 0,
    ROBOT_SOUND_TIME_CMD,
    ROBOT_SOUND_START_TIMER,
    ROBOT_SOUND_STOP_TIMER,
    ROBOT_SOUND_DO_DANCE,
    ROBOT_SOUND_ARE_YOU_DIZZY,
    ROBOT_SOUND_WALK_FORWARD,
    ROBOT_SOUND_WALK_BACKWARD,
    ROBOT_SOUND_TURN_LEFT,
    ROBOT_SOUND_TURN_RIGHT,
    ROBOT_SOUND_MARCH,
    ROBOT_SOUND_DO_JUMP,
    ROBOT_SOUND_SNEAK_AROUND,
    ROBOT_SOUND_GET_BACK,
    ROBOT_SOUND_DO_SPIN,
    ROBOT_SOUND_PATROL,
    ROBOT_SOUND_LOW_POWER,
    ROBOT_SOUND_COUNT,
} RobotSoundID;

void robot_sound_set_volume(int percent);
void play_robot_sound(RobotSoundID sound_id);

#ifdef __cplusplus
}
#endif
