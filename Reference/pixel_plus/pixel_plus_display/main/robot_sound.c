#include "robot_sound.h"

#include "pixel_voice.h"

void robot_sound_set_volume(int percent)
{
    pixel_voice_set_volume(percent);
}

void play_robot_sound(RobotSoundID sound_id)
{
    pixel_voice_enqueue_robot(sound_id);
}
