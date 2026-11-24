#include "battery_pmu.h"
#include "pca9685.h"
#include "pixel_voice.h"
#include "robot_sound.h"
#include "speech_wake.h"
#include "bsp/esp-bsp.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "src/misc/lv_math.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(lv_font_montserrat_40);
LV_FONT_DECLARE(lv_font_montserrat_48);
LV_FONT_DECLARE(lv_font_clock_110);

static const char *TAG = "pixel_plus";

typedef enum {
    SCREEN_FACE = 0,
    SCREEN_SETTINGS = 1,
    SCREEN_ALARM = 2,
    SCREEN_CLOCK = 3,
    SCREEN_EMOTIONS = 4,
    SCREEN_SERVOS = 5,
    SCREEN_EMOTES = 6,
} screen_mode_t;

typedef enum {
    EMOTE_NONE = 0,
    EMOTE_LEAN_LEFT,
    EMOTE_LEAN_RIGHT,
    EMOTE_DIZZY_LEFT,
    EMOTE_DIZZY_RIGHT,
    EMOTE_DANCE,
    EMOTE_WALK_FORWARD,
    EMOTE_WALK_BACKWARD,
    EMOTE_TURN_LEFT,
    EMOTE_TURN_RIGHT,
    EMOTE_MARCH,
    EMOTE_SPIN,
    EMOTE_JUMP,
} emote_state_t;

typedef enum {
    WALK_SUB_FEET = 0,
    WALK_SUB_HIPS,
    WALK_SUB_SETTLE,
} walk_subphase_t;

typedef enum {
    TURN_SUB_LEAN_PIVOT = 0,
    TURN_SUB_ROTATE_SWING,
    TURN_SUB_LEAN_OTHER,
    TURN_SUB_STRAIGHTEN_SWING,
} turn_subphase_t;

typedef enum {
    WALK_SIDE_LEFT = 0,
    WALK_SIDE_RIGHT,
} walk_side_t;

typedef enum {
    WALK_MODE_FORWARD = 0,
    WALK_MODE_BACKWARD,
    WALK_MODE_TURN_LEFT,
    WALK_MODE_TURN_RIGHT,
    WALK_MODE_MARCH,
} walk_mode_t;

typedef struct {
    uint16_t phase_min_ticks;
    uint8_t leg_step_deg;
    uint8_t foot_plant_deg;
    uint8_t foot_lift_deg;
    uint8_t ramp_step_deg;
    walk_mode_t mode;
    int8_t foot_tilt_deg;
    int8_t right_foot_plant_bias_deg;
} walk_params_t;

typedef enum {
    JUMP_SUB_CROUCH = 0,
    JUMP_SUB_LAUNCH,
    JUMP_SUB_LAND,
    JUMP_SUB_DONE,
} jump_subphase_t;

typedef enum {
    SPIN_SUB_LEAN_LEFT = 0,
    SPIN_SUB_SWING_FWD,
    SPIN_SUB_SWING_BACK,
} spin_subphase_t;

typedef enum {
    PATROL_SEG_FWD = 0,
    PATROL_SEG_PAUSE,
    PATROL_SEG_TURN,
    PATROL_SEG_TURN_LEFT,
} patrol_seg_kind_t;

typedef struct {
    patrol_seg_kind_t kind;
    uint8_t arg;
} patrol_seg_t;

#define SERVO_CH_LEFT_LEG       0U
#define SERVO_CH_LEFT_FOOT      1U
#define SERVO_CH_RIGHT_LEG      2U
#define SERVO_CH_RIGHT_FOOT     3U
#define EMOTE_LEAN_DELTA_DEG    22
#define EMOTE_DANCE_DELTA_DEG 20
#define EMOTE_DANCE_HALF_PERIOD_TICKS 10
#define EMOTE_DIZZY_MIN_DEG     0
#define EMOTE_DIZZY_MAX_DEG     25
#define EMOTE_DIZZY_SWING_DEG   (EMOTE_DIZZY_MAX_DEG - EMOTE_DIZZY_MIN_DEG)
#define EMOTE_DIZZY_BIAS_DEG    EMOTE_DIZZY_MIN_DEG
#define EMOTE_DIZZY_HALF_PERIOD_TICKS 8
#define EMOTE_WALK_FORWARD_STEP_COUNT   15U
#define EMOTE_WALK_STEP_COUNT           6U
#define WALK_LEG_STEP_DEG               20
#define WALK_FOOT_PLANT_DEG             10
#define WALK_FOOT_LIFT_DEG              16
#define WALK_RAMP_STEP_DEG              1U
#define WALK_PHASE_MIN_MS               50
#define WALK_PHASE_MIN_TICKS            ((WALK_PHASE_MIN_MS + SERVO_RAMP_PERIOD_MS - 1) / SERVO_RAMP_PERIOD_MS)
/** Walk forward: faster phase gate; shorter stride keeps cadence at 1 deg/tick ramp. */
#define WALK_FORWARD_PHASE_MIN_MS       43
#define WALK_FORWARD_PHASE_MIN_TICKS    ((WALK_FORWARD_PHASE_MIN_MS + SERVO_RAMP_PERIOD_MS - 1) / SERVO_RAMP_PERIOD_MS)
#define WALK_FORWARD_LEG_STEP_DEG       10
#define WALK_FORWARD_FOOT_LIFT_DEG      8
#define WALK_FORWARD_FOOT_PLANT_DEG     5
/** Extra right-foot plant angle when bearing weight (shifts COM over right foot). */
#define WALK_FORWARD_RIGHT_FOOT_PLANT_BIAS_DEG 6
#define SERVO_RAMP_STEP_FAST_DEG  2U
#define SERVO_RAMP_STEP_SLOW_DEG  1U
/** Within this many degrees of target, use slow steps for ease-out. */
#define SERVO_RAMP_EASE_ZONE_DEG  10U
/** Final approach: half tick rate so lean settles without overshoot. */
#define SERVO_RAMP_CREEP_ZONE_DEG 6U

typedef enum {
    EMOTION_IDLE = 0,
    EMOTION_HAPPY,
    EMOTION_CONFUSED,
    EMOTION_SERIOUS,
    EMOTION_COUNT,
} emotion_preset_t;

#define EMOTION_SPIRAL_W 162
#define EMOTION_SPIRAL_H 186
/** Oval spiral: wider horizontal inset vs vertical → taller ellipse on portrait canvas. */
#define EMOTION_SPIRAL_RX_PAD 10
#define EMOTION_SPIRAL_RY_PAD 7
#define EMOTION_SPIRAL_TURNS 5
#define EMOTION_SPIRAL_SEGMENTS 200
#define EMOTION_SPIRAL_PHASE_RIGHT_DEG 58
#define FACE_ANIM_PERIOD_MS 100
/** Closed-eye bar height while speech wake is still starting (matches blink min). */
#define FACE_WAKE_BOOT_CLOSED_H 26
#define FACE_EYE_COLOR_BLUE       0x00E5FF
#define FACE_EYE_COLOR_JARVIS     0x00FF55
#define FACE_JARVIS_FLASH_MS      120
#define SERVO_RAMP_PERIOD_MS 10
#define SPEAKER_VOLUME_DEFAULT 100
/** ~same visual spin rate as 22 deg every 33 ms at 10 fps. */
#define EMOTION_SPIRAL_ROTATE_STEP_DEG 66
#define VOICE_DANCE_DURATION_MS 5000
#define VOICE_DIZZY_EMOTE_MS 4000
#define VOICE_DIZZY_CONFUSED_MS 4000
#define VOICE_SPIN_MAX_MS 1200
#define VOICE_PATROL_PAUSE_MS 500
#define VOICE_JUMP_MAX_MS 1500
#define VOICE_SNEAK_STEP_COUNT 8U
#define VOICE_TURN_CYCLE_COUNT 4U
#define VOICE_MARCH_STEP_COUNT 10U
#define VOICE_FLEE_STEP_COUNT 14U
#define SPIN_LEAN_PHASE_MIN_MS 20
#define SPIN_LEAN_PHASE_MIN_TICKS ((SPIN_LEAN_PHASE_MIN_MS + SERVO_RAMP_PERIOD_MS - 1) / SERVO_RAMP_PERIOD_MS)
#define SPIN_SWING_FWD_DEG 15
#define SPIN_SWING_BACK_DEG 30
#define SPIN_SWING_PHASE_MIN_MS 35
#define SPIN_SWING_PHASE_MIN_TICKS ((SPIN_SWING_PHASE_MIN_MS + SERVO_RAMP_PERIOD_MS - 1) / SERVO_RAMP_PERIOD_MS)
#define TURN_SWING_LEG_DEG 18
#define WALK_TURN_FOOT_LIFT_DEG 10
#define WALK_TURN_FOOT_PLANT_DEG 8
/** Turn-right pivot needs extra weight shift onto the right foot. */
#define TURN_RIGHT_EXTRA_PLANT_DEG 6
#define TURN_RIGHT_EXTRA_LIFT_DEG 4
#define WALK_TURN_WADDLE_PHASE_MIN_MS 55
/** Alarm uses EMOTE_MARCH gait; ~2× faster phase/ramp than voice march. */
#define ALARM_MARCH_PHASE_MIN_MS        28
#define ALARM_MARCH_PHASE_MIN_TICKS     ((ALARM_MARCH_PHASE_MIN_MS + SERVO_RAMP_PERIOD_MS - 1) / SERVO_RAMP_PERIOD_MS)
#define ALARM_MARCH_RAMP_STEP_DEG       2U
#define PATROL_TURN_CYCLE_COUNT 3U
#define PATROL_FWD_STEP_COUNT EMOTE_WALK_FORWARD_STEP_COUNT
#define PATROL_PLAN_LEN 4U
#define JUMP_LAUNCH_LEG_DEG 15
#define JUMP_LAUNCH_FOOT_DEG 30
#define WALK_FLEE_FOOT_TILT_DEG (-14)
#define WALK_FLEE_PHASE_MIN_MS 12
#define WALK_FLEE_RAMP_STEP_DEG 4U
#define WALK_FLEE_LEG_STEP_DEG 24
#define WALK_FLEE_FOOT_LIFT_DEG 20
#define WALK_FLEE_FOOT_PLANT_DEG 12
#define WALK_SNEAK_PHASE_MIN_MS 120
#define WALK_SPIN_PHASE_MIN_MS 30
#define WALK_SNEAK_LEG_STEP_DEG 10
#define WALK_SNEAK_FOOT_LIFT_DEG 8
#define JUMP_CROUCH_LEG_DEG 15

LV_DRAW_BUF_DEFINE_STATIC(s_emotion_spiral_drawbuf_left, EMOTION_SPIRAL_W, EMOTION_SPIRAL_H, LV_COLOR_FORMAT_RGB565);
LV_DRAW_BUF_DEFINE_STATIC(s_emotion_spiral_drawbuf_right, EMOTION_SPIRAL_W, EMOTION_SPIRAL_H, LV_COLOR_FORMAT_RGB565);

static lv_obj_t *s_left_eye;
static lv_obj_t *s_right_eye;
static lv_obj_t *s_settings_panel;
static lv_obj_t *s_battery_root;
static lv_obj_t *s_battery_bar;
static lv_obj_t *s_settings_label;
static lv_obj_t *s_brightness_slider;
static lv_obj_t *s_volume_label;
static lv_obj_t *s_volume_slider;
static lv_obj_t *s_servos_btn;
static lv_obj_t *s_emotes_btn;
static lv_obj_t *s_emotions_btn;
static lv_obj_t *s_emotes_panel;
static lv_obj_t *s_emotes_title_label;
static lv_obj_t *s_emote_lean_left_btn;
static lv_obj_t *s_emote_lean_right_btn;
static lv_obj_t *s_emote_dizzy_left_btn;
static lv_obj_t *s_emote_dizzy_right_btn;
static lv_obj_t *s_emote_dance_btn;
static lv_obj_t *s_emote_walk_btn;
static lv_obj_t *s_emote_walk_back_btn;
static lv_obj_t *s_emotes_done_btn;
static lv_obj_t *s_emotions_panel;
static lv_obj_t *s_emotions_title_label;
static lv_obj_t *s_emotions_name_label;
static lv_obj_t *s_emotions_hint_label;
static lv_obj_t *s_emotions_done_btn;
static lv_obj_t *s_servo_panel;
static lv_obj_t *s_servo_title_label;
static lv_obj_t *s_servo_center_btn;
static lv_obj_t *s_servo_done_btn;
static lv_obj_t *s_servo_sliders[PCA9685_SERVO_CHANNEL_COUNT];
static lv_obj_t *s_servo_value_labels[PCA9685_SERVO_CHANNEL_COUNT];
static lv_obj_t *s_servo_set_center_btns[PCA9685_SERVO_CHANNEL_COUNT];
static lv_obj_t *s_left_spiral_canvas;
static lv_obj_t *s_right_spiral_canvas;
static lv_obj_t *s_tear_left;
static lv_obj_t *s_tear_right;
static lv_obj_t *s_happy_mask_left;
static lv_obj_t *s_happy_mask_right;
static lv_obj_t *s_alarm_panel;
static lv_obj_t *s_alarm_time_chip;
static lv_obj_t *s_alarm_hh_btn;
static lv_obj_t *s_alarm_mm_btn;
static lv_obj_t *s_alarm_ss_btn;
static lv_obj_t *s_alarm_hh_label;
static lv_obj_t *s_alarm_mm_label;
static lv_obj_t *s_alarm_ss_label;
static lv_obj_t *s_alarm_action_btn;
static lv_obj_t *s_alarm_action_label;
static lv_obj_t *s_alarm_picker_panel;
static lv_obj_t *s_alarm_picker_hours;
static lv_obj_t *s_alarm_picker_minutes;
static lv_obj_t *s_alarm_picker_seconds;
static lv_obj_t *s_alarm_picker_set_btn;
static lv_obj_t *s_clock_panel;
static lv_obj_t *s_clock_time_label;
static lv_obj_t *s_clock_ampm_label;
static lv_obj_t *s_clock_hint_label;
static lv_obj_t *s_clock_picker_panel;
static lv_obj_t *s_clock_picker_hour;
static lv_obj_t *s_clock_picker_minute;
static lv_obj_t *s_clock_picker_ampm;
static lv_obj_t *s_clock_picker_set_btn;

static uint8_t s_clock_pick_hour12 = 12;
static uint8_t s_clock_pick_minute = 0;
static bool s_clock_pick_pm = false;
static bool s_clock_wall_time_valid = false;

static int16_t s_base_left_x = 51;
static int16_t s_base_right_x = 253;
static int16_t s_base_y = 102;
static int16_t s_eye_w = 162;
static int16_t s_eye_h = 232;
static screen_mode_t s_screen_mode = SCREEN_FACE;
static emotion_preset_t s_emotion = EMOTION_IDLE;

static int16_t s_look_x = 0;
static int16_t s_look_y = 0;
static int16_t s_target_look_x = 0;
static int16_t s_target_look_y = 0;

static uint32_t s_next_look_change_ms = 0;
static uint32_t s_blink_start_ms = 0;
static uint32_t s_blink_duration_ms = 0;
static uint32_t s_next_blink_ms = 0;
static uint32_t s_wink_start_ms = 0;
static uint32_t s_wink_duration_ms = 0;
static uint32_t s_next_wink_ms = 0;
static bool s_wink_left = true;
static bool s_face_wake_boot_pending = true;
static bool s_wake_look_active = false;
static uint8_t s_wake_look_step = 0;
static uint32_t s_wake_look_next_ms = 0;
static uint32_t s_jarvis_flash_until_ms = 0;
static bool s_jarvis_flash_was_active = false;
static bool s_low_power_active = false;
static int s_low_power_saved_brightness = 100;

static uint32_t s_next_battery_poll_ms = 0;
static uint32_t s_last_clock_tick_ms = 0;

static int16_t s_spiral_angle = 0;
static bool s_spiral_force_full_left = true;
static bool s_spiral_force_full_right = true;
static lv_color16_t *s_spiral_scratch = NULL;
static lv_draw_buf_t s_spiral_scratch_buf;
static bool s_spiral_scratch_ready = false;

typedef struct {
    int16_t left_x;
    int16_t left_y;
    int16_t left_w;
    int16_t left_h;
    int16_t right_x;
    int16_t right_y;
    int16_t right_w;
    int16_t right_h;
    lv_opa_t left_eye_opa;
    lv_opa_t right_eye_opa;
    lv_opa_t left_spiral_opa;
    lv_opa_t right_spiral_opa;
    bool mask_visible;
    int16_t mask_w;
    int16_t mask_h;
    int16_t mask_xl;
    int16_t mask_xr;
    int16_t mask_y;
    bool mask_circle;
    lv_opa_t mask_opa;
    bool valid;
} face_geom_cache_t;

static face_geom_cache_t s_face_geom_cache;

/** Emotion swipe transition (emotions screen): interpolate eye shapes and confused spirals. */
#define EMOTION_TRANSITION_MS 400
static emotion_preset_t s_emotion_anim_from = EMOTION_IDLE;
static emotion_preset_t s_emotion_anim_to = EMOTION_IDLE;
static uint32_t s_emotion_anim_start_ms = 0;

typedef enum {
    ALARM_FIELD_HOURS = 0,
    ALARM_FIELD_MINUTES = 1,
    ALARM_FIELD_SECONDS = 2,
} alarm_field_t;

static alarm_field_t s_alarm_selected_field = ALARM_FIELD_HOURS;
static uint8_t s_alarm_hours = 0;
static uint8_t s_alarm_minutes = 1;
static uint8_t s_alarm_seconds = 0;
static bool s_alarm_armed = false;
static bool s_alarm_ringing = false;
static uint32_t s_alarm_deadline_ms = 0;
static uint32_t s_next_beep_ms = 0;
static uint16_t s_servo_angles[PCA9685_SERVO_CHANNEL_COUNT] = { 90, 90, 90, 90 };
static uint16_t s_servo_target[PCA9685_SERVO_CHANNEL_COUNT] = { 90, 90, 90, 90 };
static uint16_t s_servo_center[PCA9685_SERVO_CHANNEL_COUNT] = { 90, 90, 90, 90 };
static uint8_t s_servo_ramp_creep_tick[PCA9685_SERVO_CHANNEL_COUNT];
static bool s_servo_ignore_slider_cb;
static emote_state_t s_emote = EMOTE_NONE;
static uint16_t s_emote_dizzy_tick;
static uint16_t s_emote_dance_tick;
static walk_subphase_t s_walk_subphase;
static turn_subphase_t s_turn_subphase;
static walk_side_t s_walk_side;
static uint16_t s_walk_phase_tick;
static uint8_t s_walk_steps_done;
static uint8_t s_walk_step_target;
static walk_params_t s_walk_params;
static jump_subphase_t s_jump_subphase;
static spin_subphase_t s_spin_subphase;
static uint16_t s_spin_phase_tick;
static bool s_spin_visual_active;
static uint8_t s_patrol_plan_idx;

typedef enum {
    VOICE_SEQ_NONE = 0,
    VOICE_SEQ_DANCE,
    VOICE_SEQ_DIZZY_EMOTE,
    VOICE_SEQ_DIZZY_CONFUSED,
    VOICE_SEQ_WALK,
    VOICE_SEQ_SNEAK,
    VOICE_SEQ_FLEE,
    VOICE_SEQ_TURN,
    VOICE_SEQ_MARCH,
    VOICE_SEQ_SPIN,
    VOICE_SEQ_JUMP,
    VOICE_SEQ_PATROL,
} voice_seq_phase_t;

static bool s_voice_seq_active;
static emotion_preset_t s_voice_saved_emotion;
static lv_timer_t *s_voice_seq_timer;
static voice_seq_phase_t s_voice_seq_phase;

static esp_codec_dev_handle_t s_speaker = NULL;
static esp_codec_dev_handle_t s_microphone = NULL;
static bool s_audio_ready = false;
static int s_speaker_volume = SPEAKER_VOLUME_DEFAULT;

static const i2s_std_config_t s_i2s_16k_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
    .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
        .mclk = BSP_I2S_MCLK,
        .bclk = BSP_I2S_SCLK,
        .ws = BSP_I2S_LCLK,
        .dout = BSP_I2S_DOUT,
        .din = BSP_I2S_DSIN,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = false,
        },
    },
};

static uint32_t rand_range(uint32_t min, uint32_t max)
{
    if (max <= min) {
        return min;
    }
    return min + (esp_random() % (max - min + 1));
}

static void show_clock_screen(void);
static void speech_command_cb(int command_id);
static void voice_sequence_end(void);
static void voice_on_walk_segment_done(void);
static void walk_stop(void);
static void emote_sync_btn_styles(void);
static void emotion_apply_visibility(void);
static void face_wake_boot_complete(void);
static void face_apply_wake_boot_closed_eyes(void);
static void face_geom_cache_invalidate(void);
static void face_wake_look_start(void);
static void face_jarvis_flash_start(void);
static void enter_low_power_mode(void);
static void exit_low_power_mode(void);
static void face_tap_wake_cb(lv_event_t *e);
static void update_eye_geometry(void);

static lv_color_t face_eye_bg_color(uint32_t now)
{
    if (s_jarvis_flash_until_ms != 0 && now < s_jarvis_flash_until_ms) {
        return lv_color_hex(FACE_EYE_COLOR_JARVIS);
    }
    return lv_color_hex(FACE_EYE_COLOR_BLUE);
}

static void face_jarvis_flash_start(void)
{
    const uint32_t now = lv_tick_get();
    s_jarvis_flash_until_ms = now + FACE_JARVIS_FLASH_MS;
    s_jarvis_flash_was_active = true;
    face_geom_cache_invalidate();
}

typedef struct {
    int16_t x;
    int16_t y;
    uint16_t hold_ms;
} wake_look_waypoint_t;

/** Quick scan left → right → up → center when "Jarvis" is heard. */
static const wake_look_waypoint_t s_wake_look_path[] = {
    { -10, 0, 300 },
    { 10, 0, 300 },
    { 8, -5, 280 },
    { -9, -4, 280 },
    { 0, 0, 350 },
};

static void face_wake_look_start(void)
{
    if (s_face_wake_boot_pending || s_emotion != EMOTION_IDLE) {
        return;
    }

    s_wake_look_active = true;
    s_wake_look_step = 0;
    s_target_look_x = s_wake_look_path[0].x;
    s_target_look_y = s_wake_look_path[0].y;
    s_wake_look_next_ms = lv_tick_get() + s_wake_look_path[0].hold_ms;
    s_next_look_change_ms = s_wake_look_next_ms + 2000;
}

static void speech_wakeup_voice_cb(void)
{
    if (s_low_power_active) {
        bsp_display_lock((uint32_t)-1);
        exit_low_power_mode();
        bsp_display_unlock();
    }
    face_jarvis_flash_start();
    face_wake_look_start();
    if (s_audio_ready) {
        pixel_voice_play(PIXEL_CHIRP_LISTEN);
    }
}

static const patrol_seg_t s_patrol_plan[PATROL_PLAN_LEN] = {
    { PATROL_SEG_FWD, PATROL_FWD_STEP_COUNT },
    { PATROL_SEG_TURN, PATROL_TURN_CYCLE_COUNT },
    { PATROL_SEG_TURN_LEFT, PATROL_TURN_CYCLE_COUNT },
    { PATROL_SEG_FWD, PATROL_FWD_STEP_COUNT },
};

static void speech_wake_boot_task(void *arg)
{
    esp_codec_dev_handle_t mic = (esp_codec_dev_handle_t)arg;
    /* Let LVGL finish its first frames before AFE grabs heap. */
    vTaskDelay(pdMS_TO_TICKS(3000));
    speech_wake_set_command_cb(speech_command_cb);
    speech_wake_set_wakeup_cb(speech_wakeup_voice_cb);
    esp_err_t err = speech_wake_start(mic);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wake word init failed: %s", esp_err_to_name(err));
    } else if (s_audio_ready) {
        pixel_voice_play(PIXEL_CHIRP_WAKE_UP);
    }
    face_wake_boot_complete();
    vTaskDelete(NULL);
}

static void alarm_shuffle_start(void);
static void alarm_shuffle_stop(void);
static void update_alarm_time_labels(void);
static void update_alarm_action_label(void);

static void alarm_play_sound(bool repeat)
{
    if (s_audio_ready) {
        pixel_voice_play(repeat ? PIXEL_CHIRP_ALARM_REPEAT : PIXEL_CHIRP_ALARM);
    }
}

static void start_alarm_ringing(uint32_t now)
{
    s_alarm_armed = false;
    s_alarm_ringing = true;
    s_next_beep_ms = now + 900;
    s_alarm_hours = 0;
    s_alarm_minutes = 0;
    s_alarm_seconds = 0;
    update_alarm_time_labels();
    alarm_shuffle_start();
    alarm_play_sound(false);
    update_alarm_action_label();
}

static void stop_alarm_ringing(void)
{
    s_alarm_ringing = false;
    s_next_beep_ms = 0;
    alarm_shuffle_stop();
}

static uint32_t alarm_total_ms_from_fields(void)
{
    return ((uint32_t)s_alarm_hours * 3600U + (uint32_t)s_alarm_minutes * 60U + (uint32_t)s_alarm_seconds) * 1000U;
}

static void set_alarm_picker_visible(bool visible)
{
    if (visible) {
        lv_obj_clear_flag(s_alarm_picker_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_alarm_picker_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_alarm_time_from_ms(uint32_t total_ms)
{
    uint32_t total_sec = total_ms / 1000U;
    s_alarm_hours = (uint8_t)((total_sec / 3600U) % 24U);
    s_alarm_minutes = (uint8_t)((total_sec % 3600U) / 60U);
    s_alarm_seconds = (uint8_t)(total_sec % 60U);
}

static void update_alarm_time_labels(void)
{
    char text[8];
    snprintf(text, sizeof(text), "%02u", s_alarm_hours);
    lv_label_set_text(s_alarm_hh_label, text);
    snprintf(text, sizeof(text), "%02u", s_alarm_minutes);
    lv_label_set_text(s_alarm_mm_label, text);
    snprintf(text, sizeof(text), "%02u", s_alarm_seconds);
    lv_label_set_text(s_alarm_ss_label, text);
}

static void update_alarm_action_label(void)
{
    if (s_alarm_ringing) {
        lv_label_set_text(s_alarm_action_label, "STOP");
        lv_obj_set_style_bg_color(s_alarm_action_btn, lv_color_hex(0xD83A3A), LV_PART_MAIN);
    } else if (s_alarm_armed) {
        lv_label_set_text(s_alarm_action_label, "STOP");
        lv_obj_set_style_bg_color(s_alarm_action_btn, lv_color_hex(0xD83A3A), LV_PART_MAIN);
    } else {
        lv_label_set_text(s_alarm_action_label, "START");
        lv_obj_set_style_bg_color(s_alarm_action_btn, lv_color_hex(0x22DD55), LV_PART_MAIN);
    }
}

static void style_alarm_picker_focus(void)
{
    lv_obj_set_style_border_width(s_alarm_picker_hours, s_alarm_selected_field == ALARM_FIELD_HOURS ? 2 : 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_alarm_picker_hours, s_alarm_selected_field == ALARM_FIELD_HOURS ? lv_color_hex(0xFFDD00) : lv_color_hex(0x444444), LV_PART_MAIN);

    lv_obj_set_style_border_width(s_alarm_picker_minutes, s_alarm_selected_field == ALARM_FIELD_MINUTES ? 2 : 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_alarm_picker_minutes, s_alarm_selected_field == ALARM_FIELD_MINUTES ? lv_color_hex(0xFFDD00) : lv_color_hex(0x444444), LV_PART_MAIN);

    lv_obj_set_style_border_width(s_alarm_picker_seconds, s_alarm_selected_field == ALARM_FIELD_SECONDS ? 2 : 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_alarm_picker_seconds, s_alarm_selected_field == ALARM_FIELD_SECONDS ? lv_color_hex(0xFFDD00) : lv_color_hex(0x444444), LV_PART_MAIN);
}

static void sync_picker_with_time_values(void)
{
    lv_roller_set_selected(s_alarm_picker_hours, s_alarm_hours, LV_ANIM_OFF);
    lv_roller_set_selected(s_alarm_picker_minutes, s_alarm_minutes, LV_ANIM_OFF);
    lv_roller_set_selected(s_alarm_picker_seconds, s_alarm_seconds, LV_ANIM_OFF);
}

static void load_clock_from_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open("clock", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    int64_t wall = 0;
    esp_err_t err = nvs_get_i64(h, "wall", &wall);
    nvs_close(h);
    if (err != ESP_OK || wall < (int64_t)1577836800) {
        return;
    }
    struct timeval tv = { .tv_sec = (time_t)wall, .tv_usec = 0 };
    if (settimeofday(&tv, NULL) == 0) {
        s_clock_wall_time_valid = true;
    }
}

static esp_err_t clock_save_wall_time_to_nvs(time_t t)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("clock", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_i64(h, "wall", (int64_t)t);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

static void set_clock_picker_visible(bool visible)
{
    if (s_clock_picker_panel == NULL) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(s_clock_picker_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_clock_picker_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void sync_clock_picker_rollers_from_wall_clock(void)
{
    if (s_clock_picker_hour == NULL) {
        return;
    }

    time_t now = time(NULL);
    struct tm tm_local;
    if (s_clock_wall_time_valid || now >= (time_t)1577836800) {
        localtime_r(&now, &tm_local);
        int h12 = tm_local.tm_hour % 12;
        if (h12 == 0) {
            h12 = 12;
        }
        s_clock_pick_hour12 = (uint8_t)h12;
        s_clock_pick_minute = (uint8_t)tm_local.tm_min;
        s_clock_pick_pm = tm_local.tm_hour >= 12;
    } else {
        s_clock_pick_hour12 = 12;
        s_clock_pick_minute = 0;
        s_clock_pick_pm = false;
    }

    lv_roller_set_selected(s_clock_picker_hour, (uint16_t)(s_clock_pick_hour12 - 1), LV_ANIM_OFF);
    lv_roller_set_selected(s_clock_picker_minute, s_clock_pick_minute, LV_ANIM_OFF);
    lv_roller_set_selected(s_clock_picker_ampm, s_clock_pick_pm ? 1 : 0, LV_ANIM_OFF);
}

static void update_clock_labels(void)
{
    if (s_clock_time_label == NULL || s_clock_ampm_label == NULL) {
        return;
    }

    time_t now = time(NULL);
    if (!s_clock_wall_time_valid && now < (time_t)1577836800) {
        lv_label_set_text(s_clock_time_label, "--:--");
        lv_label_set_text(s_clock_ampm_label, "");
        if (s_clock_hint_label != NULL) {
            lv_obj_clear_flag(s_clock_hint_label, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (s_clock_hint_label != NULL) {
        lv_obj_add_flag(s_clock_hint_label, LV_OBJ_FLAG_HIDDEN);
    }

    struct tm tm_local;
    localtime_r(&now, &tm_local);
    int h12 = tm_local.tm_hour % 12;
    if (h12 == 0) {
        h12 = 12;
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", h12, tm_local.tm_min);
    lv_label_set_text(s_clock_time_label, buf);
    lv_label_set_text(s_clock_ampm_label, tm_local.tm_hour < 12 ? "AM" : "PM");
}

static void load_emotion_from_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open("face", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    uint8_t v = 0;
    esp_err_t err = nvs_get_u8(h, "emotion", &v);
    nvs_close(h);
    if (err != ESP_OK) {
        return;
    }
    /* Migration from older enum values: removed Mad falls back to Idle. */
    if (v == 3) {
        v = EMOTION_IDLE;
    } else if (v > 3) {
        v--;
    }
    if (v >= EMOTION_COUNT) {
        return;
    }
    s_emotion = (emotion_preset_t)v;
}

static esp_err_t emotion_save_to_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("face", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u8(h, "emotion", (uint8_t)s_emotion);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

static esp_err_t servo_center_nvs_key(uint8_t channel, char *key, size_t key_len)
{
    if (channel >= PCA9685_SERVO_CHANNEL_COUNT || key_len < 3U) {
        return ESP_ERR_INVALID_ARG;
    }
    int n = snprintf(key, key_len, "c%u", (unsigned)channel);
    if (n < 0 || (size_t)n >= key_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

static void servo_load_centers_from_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open("servo", NVS_READONLY, &h) != ESP_OK) {
        return;
    }

    bool loaded_any = false;
    for (uint8_t channel = 0; channel < PCA9685_SERVO_CHANNEL_COUNT; ++channel) {
        char key[4];
        if (servo_center_nvs_key(channel, key, sizeof(key)) != ESP_OK) {
            continue;
        }
        uint16_t value = 0;
        if (nvs_get_u16(h, key, &value) == ESP_OK && value <= 180U) {
            s_servo_center[channel] = value;
            loaded_any = true;
        }
    }

    if (!loaded_any) {
        uint16_t legacy[PCA9685_SERVO_CHANNEL_COUNT];
        size_t len = sizeof(legacy);
        if (nvs_get_blob(h, "center", legacy, &len) == ESP_OK && len == sizeof(legacy)) {
            for (uint8_t channel = 0; channel < PCA9685_SERVO_CHANNEL_COUNT; ++channel) {
                if (legacy[channel] <= 180U) {
                    s_servo_center[channel] = legacy[channel];
                    loaded_any = true;
                }
            }
        }
    }

    nvs_close(h);

    if (loaded_any) {
        ESP_LOGI(TAG, "Servo centers loaded: %u %u %u %u", (unsigned)s_servo_center[0],
                 (unsigned)s_servo_center[1], (unsigned)s_servo_center[2], (unsigned)s_servo_center[3]);
    }
}

static esp_err_t servo_save_centers_to_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("servo", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Servo center NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    for (uint8_t channel = 0; channel < PCA9685_SERVO_CHANNEL_COUNT; ++channel) {
        char key[4];
        if (servo_center_nvs_key(channel, key, sizeof(key)) != ESP_OK) {
            continue;
        }
        err = nvs_set_u16(h, key, s_servo_center[channel]);
        if (err != ESP_OK) {
            break;
        }
    }

    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Servo center NVS save failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Servo centers saved: %u %u %u %u", (unsigned)s_servo_center[0],
                 (unsigned)s_servo_center[1], (unsigned)s_servo_center[2], (unsigned)s_servo_center[3]);
    }
    return err;
}

static void servo_apply_loaded_centers_on_boot(void)
{
    for (uint8_t channel = 0; channel < PCA9685_SERVO_CHANNEL_COUNT; ++channel) {
        s_servo_angles[channel] = s_servo_center[channel];
        s_servo_target[channel] = s_servo_center[channel];
    }
}

static const char *emotion_display_name(emotion_preset_t e)
{
    switch (e) {
    case EMOTION_IDLE:
        return "Idle";
    case EMOTION_HAPPY:
        return "Happy";
    case EMOTION_CONFUSED:
        return "Confused";
    case EMOTION_SERIOUS:
        return "Serious";
    default:
        return "?";
    }
}

static void emotion_sync_ui_labels(void)
{
    if (s_emotions_name_label != NULL) {
        lv_label_set_text(s_emotions_name_label, emotion_display_name(s_emotion));
    }
}

static bool emotion_transition_animating(void);

static void face_geom_cache_invalidate(void)
{
    s_face_geom_cache.valid = false;
    s_spiral_force_full_left = true;
    s_spiral_force_full_right = true;
}

static void face_eye_apply_if_changed(int16_t lx, int16_t ly, int16_t lw, int16_t lh, int16_t rx, int16_t ry,
                                      int16_t rw, int16_t rh, lv_opa_t leye_opa, lv_opa_t reye_opa)
{
    face_geom_cache_t *c = &s_face_geom_cache;

    if (c->valid && c->left_x == lx && c->left_y == ly && c->left_w == lw && c->left_h == lh &&
        c->right_x == rx && c->right_y == ry && c->right_w == rw && c->right_h == rh &&
        c->left_eye_opa == leye_opa && c->right_eye_opa == reye_opa) {
        return;
    }

    lv_obj_set_size(s_left_eye, lw, lh);
    lv_obj_set_size(s_right_eye, rw, rh);
    lv_obj_set_pos(s_left_eye, lx, ly);
    lv_obj_set_pos(s_right_eye, rx, ry);
    if (!c->valid || c->left_eye_opa != leye_opa) {
        lv_obj_set_style_opa(s_left_eye, leye_opa, LV_PART_MAIN);
    }
    if (!c->valid || c->right_eye_opa != reye_opa) {
        lv_obj_set_style_opa(s_right_eye, reye_opa, LV_PART_MAIN);
    }

    c->left_x = lx;
    c->left_y = ly;
    c->left_w = lw;
    c->left_h = lh;
    c->right_x = rx;
    c->right_y = ry;
    c->right_w = rw;
    c->right_h = rh;
    c->left_eye_opa = leye_opa;
    c->right_eye_opa = reye_opa;
    c->valid = true;
}

static void face_spiral_opa_apply_if_changed(lv_opa_t spir_opa)
{
    if (s_left_spiral_canvas == NULL) {
        return;
    }

    face_geom_cache_t *c = &s_face_geom_cache;
    if (c->valid && c->left_spiral_opa == spir_opa && c->right_spiral_opa == spir_opa) {
        return;
    }

    lv_obj_set_style_opa(s_left_spiral_canvas, spir_opa, LV_PART_MAIN);
    lv_obj_set_style_opa(s_right_spiral_canvas, spir_opa, LV_PART_MAIN);
    c->left_spiral_opa = spir_opa;
    c->right_spiral_opa = spir_opa;
    c->valid = true;
}

static void spiral_plot_px(lv_draw_buf_t *buf, int32_t x, int32_t y, lv_color16_t c)
{
    if (x < 0 || x >= EMOTION_SPIRAL_W || y < 0 || y >= EMOTION_SPIRAL_H) {
        return;
    }
    lv_color16_t *px = (lv_color16_t *)lv_draw_buf_goto_xy(buf, (uint32_t)x, (uint32_t)y);
    *px = c;
}

static void spiral_draw_segment(lv_draw_buf_t *buf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                                lv_color16_t c)
{
    int32_t dx = LV_ABS(x1 - x0);
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t dy = LV_ABS(y1 - y0);
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = (dx > dy ? dx : -dy) / 2;

    while (true) {
        spiral_plot_px(buf, x0, y0, c);
        spiral_plot_px(buf, x0 + 1, y0, c);
        spiral_plot_px(buf, x0, y0 + 1, c);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int32_t e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy) {
            err += dx;
            y0 += sy;
        }
    }
}

static bool spiral_ensure_scratch(void)
{
    if (s_spiral_scratch != NULL) {
        return true;
    }

    const size_t bytes = (size_t)EMOTION_SPIRAL_W * (size_t)EMOTION_SPIRAL_H * sizeof(lv_color16_t);
    s_spiral_scratch = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_spiral_scratch == NULL) {
        s_spiral_scratch = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return s_spiral_scratch != NULL;
}

static lv_draw_buf_t *spiral_get_scratch_buf(void)
{
    if (!spiral_ensure_scratch()) {
        return NULL;
    }
    if (!s_spiral_scratch_ready) {
        const uint32_t px_count = (uint32_t)EMOTION_SPIRAL_W * (uint32_t)EMOTION_SPIRAL_H;
        const uint32_t buf_bytes = px_count * (uint32_t)sizeof(lv_color16_t);
        if (lv_draw_buf_init(&s_spiral_scratch_buf, EMOTION_SPIRAL_W, EMOTION_SPIRAL_H, LV_COLOR_FORMAT_RGB565,
                             LV_STRIDE_AUTO, s_spiral_scratch, buf_bytes) != LV_RESULT_OK) {
            return NULL;
        }
        s_spiral_scratch_ready = true;
    }
    return &s_spiral_scratch_buf;
}

static void spiral_fill_bg(lv_draw_buf_t *buf)
{
    lv_color16_t bg;
    bg.red = 0x05 >> 3;
    bg.green = 0x05 >> 2;
    bg.blue = 0x08 >> 3;
    for (int32_t y = 0; y < EMOTION_SPIRAL_H; y++) {
        for (int32_t x = 0; x < EMOTION_SPIRAL_W; x++) {
            spiral_plot_px(buf, x, y, bg);
        }
    }
}

static void spiral_render_to_buf(lv_draw_buf_t *buf, int32_t angle_off_deg)
{
    if (buf == NULL) {
        return;
    }

    spiral_fill_bg(buf);

    const int32_t cx = EMOTION_SPIRAL_W / 2;
    const int32_t cy = EMOTION_SPIRAL_H / 2;
    const int32_t rx = EMOTION_SPIRAL_W / 2 - EMOTION_SPIRAL_RX_PAD;
    const int32_t ry = EMOTION_SPIRAL_H / 2 - EMOTION_SPIRAL_RY_PAD;

    lv_color16_t line_c;
    line_c.red = 0 >> 3;
    line_c.green = 0xE5 >> 2;
    line_c.blue = 0xFF >> 3;

    int32_t px = 0;
    int32_t py = 0;
    bool has_prev = false;

    for (int i = 0; i <= EMOTION_SPIRAL_SEGMENTS; i++) {
        int32_t ang = angle_off_deg + (int32_t)i * EMOTION_SPIRAL_TURNS * 360 / EMOTION_SPIRAL_SEGMENTS;
        ang %= 360;
        if (ang < 0) {
            ang += 360;
        }
        int16_t a = (int16_t)ang;

        uint32_t u = (uint32_t)i * 256U / (uint32_t)EMOTION_SPIRAL_SEGMENTS;
        int32_t ru_x = (rx * (int32_t)u) / 256;
        int32_t ru_y = (ry * (int32_t)u) / 256;

        int32_t x = cx + ((ru_x * lv_trigo_cos(a)) >> LV_TRIGO_SHIFT);
        int32_t y = cy + ((ru_y * lv_trigo_sin(a)) >> LV_TRIGO_SHIFT);

        if (has_prev) {
            spiral_draw_segment(buf, px, py, x, y, line_c);
        }
        px = x;
        py = y;
        has_prev = true;

        if ((i & 31) == 0) {
            taskYIELD();
        }
    }
}

static void spiral_apply_diff(lv_obj_t *canvas, lv_draw_buf_t *scratch, bool *force_full)
{
    lv_draw_buf_t *live = lv_canvas_get_draw_buf(canvas);
    if (live == NULL || scratch == NULL) {
        return;
    }

    if (*force_full) {
        for (int32_t y = 0; y < EMOTION_SPIRAL_H; y++) {
            for (int32_t x = 0; x < EMOTION_SPIRAL_W; x++) {
                lv_color16_t *sp = (lv_color16_t *)lv_draw_buf_goto_xy(scratch, (uint32_t)x, (uint32_t)y);
                lv_color16_t *lp = (lv_color16_t *)lv_draw_buf_goto_xy(live, (uint32_t)x, (uint32_t)y);
                *lp = *sp;
            }
        }
        lv_obj_invalidate(canvas);
        *force_full = false;
        return;
    }

    lv_area_t dirty = { .x1 = INT32_MAX, .y1 = INT32_MAX, .x2 = INT32_MIN, .y2 = INT32_MIN };
    bool any = false;

    for (int32_t y = 0; y < EMOTION_SPIRAL_H; y++) {
        for (int32_t x = 0; x < EMOTION_SPIRAL_W; x++) {
            lv_color16_t *sp = (lv_color16_t *)lv_draw_buf_goto_xy(scratch, (uint32_t)x, (uint32_t)y);
            lv_color16_t *lp = (lv_color16_t *)lv_draw_buf_goto_xy(live, (uint32_t)x, (uint32_t)y);
            if (lp->red != sp->red || lp->green != sp->green || lp->blue != sp->blue) {
                *lp = *sp;
                any = true;
                if (x < dirty.x1) {
                    dirty.x1 = x;
                }
                if (y < dirty.y1) {
                    dirty.y1 = y;
                }
                if (x > dirty.x2) {
                    dirty.x2 = x;
                }
                if (y > dirty.y2) {
                    dirty.y2 = y;
                }
            }
        }
    }

    if (any) {
        lv_obj_invalidate_area(canvas, &dirty);
    }
}

static void emotion_redraw_spiral_canvas(lv_obj_t *canvas, int32_t angle_off_deg, bool *force_full)
{
    lv_draw_buf_t *scratch = spiral_get_scratch_buf();
    if (scratch == NULL) {
        return;
    }

    spiral_render_to_buf(scratch, angle_off_deg);
    spiral_apply_diff(canvas, scratch, force_full);
}

static void emotion_redraw_spirals(void)
{
    if (s_left_spiral_canvas == NULL || s_right_spiral_canvas == NULL) {
        return;
    }
    emotion_redraw_spiral_canvas(s_left_spiral_canvas, s_spiral_angle, &s_spiral_force_full_left);
    emotion_redraw_spiral_canvas(s_right_spiral_canvas, s_spiral_angle + EMOTION_SPIRAL_PHASE_RIGHT_DEG,
                                 &s_spiral_force_full_right);
}

static void emotion_apply_visibility(void);

static bool emotion_transition_animating(void)
{
    if (s_emotion_anim_start_ms == 0) {
        return false;
    }
    return (lv_tick_get() - s_emotion_anim_start_ms) < EMOTION_TRANSITION_MS;
}

static void emotion_transition_blend_confused(bool *show_spiral_mix)
{
    if (!emotion_transition_animating()) {
        *show_spiral_mix = false;
        return;
    }
    *show_spiral_mix = (s_emotion_anim_from == EMOTION_CONFUSED || s_emotion_anim_to == EMOTION_CONFUSED);
}

static void emotion_cycle(int delta)
{
    emotion_preset_t basis = s_emotion;
    if (emotion_transition_animating()) {
        basis = s_emotion_anim_to;
    }

    int v = (int)basis + delta;
    while (v < 0) {
        v += EMOTION_COUNT;
    }
    emotion_preset_t to = (emotion_preset_t)(v % EMOTION_COUNT);
    if (to == basis) {
        return;
    }

    s_emotion_anim_from = basis;
    s_emotion_anim_to = to;
    s_emotion_anim_start_ms = lv_tick_get();

    s_emotion = to;
    (void)emotion_save_to_nvs();
    emotion_sync_ui_labels();
    face_geom_cache_invalidate();
    emotion_apply_visibility();
}

static void emotion_set_visual(emotion_preset_t to, bool persist_to_nvs)
{
    emotion_preset_t basis = s_emotion;
    if (emotion_transition_animating()) {
        basis = s_emotion_anim_to;
    }
    if (to == basis) {
        emotion_apply_visibility();
        return;
    }

    s_emotion_anim_from = basis;
    s_emotion_anim_to = to;
    s_emotion_anim_start_ms = lv_tick_get();

    s_emotion = to;
    if (persist_to_nvs) {
        (void)emotion_save_to_nvs();
    }
    emotion_sync_ui_labels();
    face_geom_cache_invalidate();
    emotion_apply_visibility();
}

static void emotion_apply_visibility(void)
{
    bool ev = (s_screen_mode == SCREEN_FACE || s_screen_mode == SCREEN_EMOTIONS);
    if (!ev) {
        if (s_left_spiral_canvas != NULL) {
            lv_obj_add_flag(s_left_spiral_canvas, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_right_spiral_canvas, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_tear_left != NULL) {
            lv_obj_add_flag(s_tear_left, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_tear_right, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_happy_mask_left != NULL) {
            lv_obj_add_flag(s_happy_mask_left, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_happy_mask_right, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    bool spiral_mix = false;
    emotion_transition_blend_confused(&spiral_mix);

    if (s_spin_visual_active) {
        spiral_mix = true;
    }

    if (s_emotion == EMOTION_CONFUSED && !spiral_mix) {
        lv_obj_add_flag(s_left_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_right_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_left_spiral_canvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_right_spiral_canvas, LV_OBJ_FLAG_HIDDEN);
        if (s_tear_left != NULL) {
            lv_obj_add_flag(s_tear_left, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_tear_right, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_happy_mask_left != NULL) {
            lv_obj_add_flag(s_happy_mask_left, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_happy_mask_right, LV_OBJ_FLAG_HIDDEN);
        }
        emotion_redraw_spirals();
    } else if (spiral_mix) {
        lv_obj_clear_flag(s_left_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_right_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_left_spiral_canvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_right_spiral_canvas, LV_OBJ_FLAG_HIDDEN);
        if (s_tear_left != NULL) {
            lv_obj_add_flag(s_tear_left, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_tear_right, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_happy_mask_left != NULL) {
            lv_obj_clear_flag(s_happy_mask_left, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_happy_mask_right, LV_OBJ_FLAG_HIDDEN);
        }
        emotion_redraw_spirals();
    } else {
        lv_obj_clear_flag(s_left_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_right_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_left_spiral_canvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_right_spiral_canvas, LV_OBJ_FLAG_HIDDEN);
        if (s_tear_left != NULL) {
            lv_obj_add_flag(s_tear_left, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_tear_right, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_happy_mask_left != NULL) {
            if (s_emotion == EMOTION_HAPPY || s_emotion == EMOTION_SERIOUS || emotion_transition_animating()) {
                lv_obj_clear_flag(s_happy_mask_left, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(s_happy_mask_right, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(s_happy_mask_left, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(s_happy_mask_right, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

static void battery_sync_visibility(void);

static void set_screen_mode(screen_mode_t mode)
{
    s_screen_mode = mode;

    if (s_emotes_panel != NULL) {
        lv_obj_add_flag(s_emotes_panel, LV_OBJ_FLAG_HIDDEN);
    }

    if (mode == SCREEN_FACE) {
        lv_obj_add_flag(s_settings_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_emotions_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_emotions_name_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_servo_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_alarm_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_clock_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_left_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_right_eye, LV_OBJ_FLAG_HIDDEN);
    } else if (mode == SCREEN_SETTINGS) {
        lv_obj_add_flag(s_left_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_right_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_alarm_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_clock_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_emotions_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_emotions_name_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_servo_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_settings_panel, LV_OBJ_FLAG_HIDDEN);
    } else if (mode == SCREEN_EMOTIONS) {
        lv_obj_add_flag(s_settings_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_alarm_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_clock_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_servo_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_left_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_right_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_emotions_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_emotions_name_label, LV_OBJ_FLAG_HIDDEN);
    } else if (mode == SCREEN_SERVOS) {
        lv_obj_add_flag(s_left_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_right_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_settings_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_emotions_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_emotions_name_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_alarm_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_clock_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_servo_panel, LV_OBJ_FLAG_HIDDEN);
    } else if (mode == SCREEN_EMOTES) {
        lv_obj_add_flag(s_left_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_right_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_settings_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_emotions_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_emotions_name_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_servo_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_alarm_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_clock_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_emotes_panel, LV_OBJ_FLAG_HIDDEN);
    } else if (mode == SCREEN_ALARM) {
        lv_obj_add_flag(s_left_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_right_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_settings_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_emotions_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_emotions_name_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_servo_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_clock_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_alarm_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        /* SCREEN_CLOCK */
        lv_obj_add_flag(s_left_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_right_eye, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_settings_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_emotions_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_emotions_name_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_servo_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_alarm_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_clock_panel, LV_OBJ_FLAG_HIDDEN);
    }

    if (mode != SCREEN_CLOCK) {
        set_clock_picker_visible(false);
    }

    if (mode == SCREEN_FACE || mode == SCREEN_EMOTIONS) {
        face_geom_cache_invalidate();
    }
    emotion_apply_visibility();
    battery_sync_visibility();
}

static void emotions_open_cb(lv_event_t *e)
{
    (void)e;
    set_screen_mode(SCREEN_EMOTIONS);
    emotion_sync_ui_labels();
}

static void emotions_done_cb(lv_event_t *e)
{
    (void)e;
    set_screen_mode(SCREEN_SETTINGS);
}

static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    bsp_display_brightness_set(value);
}

static void speaker_set_volume(int volume)
{
    s_speaker_volume = volume;
    if (s_speaker != NULL) {
        esp_codec_dev_set_out_vol(s_speaker, volume);
    }
}

static void volume_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    speaker_set_volume(value);
    pixel_voice_set_volume(value);
}

static uint16_t servo_clamp_angle(int32_t angle_deg)
{
    if (angle_deg < 0) {
        return 0;
    }
    if (angle_deg > 180) {
        return 180;
    }
    return (uint16_t)angle_deg;
}

static uint16_t servo_physical_from_offset(uint8_t channel, int16_t delta_deg)
{
    if (channel >= PCA9685_SERVO_CHANNEL_COUNT) {
        return 90;
    }
    return servo_clamp_angle((int32_t)s_servo_center[channel] + (int32_t)delta_deg);
}

static void servo_set_target(uint8_t channel, uint16_t angle_deg)
{
    if (channel >= PCA9685_SERVO_CHANNEL_COUNT) {
        return;
    }
    s_servo_target[channel] = servo_clamp_angle(angle_deg);
}

static void update_servo_value_label(uint8_t channel)
{
    if (channel >= PCA9685_SERVO_CHANNEL_COUNT || s_servo_value_labels[channel] == NULL) {
        return;
    }

    char text[16];
    snprintf(text, sizeof(text), "%3u C:%u", (unsigned)s_servo_angles[channel],
             (unsigned)s_servo_center[channel]);
    lv_label_set_text(s_servo_value_labels[channel], text);
}

static void servo_sync_slider(uint8_t channel)
{
    if (channel >= PCA9685_SERVO_CHANNEL_COUNT || s_servo_sliders[channel] == NULL) {
        return;
    }

    s_servo_ignore_slider_cb = true;
    lv_slider_set_value(s_servo_sliders[channel], (int32_t)s_servo_angles[channel], LV_ANIM_OFF);
    s_servo_ignore_slider_cb = false;
}

static void servo_sync_ui(void)
{
    for (uint8_t channel = 0; channel < PCA9685_SERVO_CHANNEL_COUNT; ++channel) {
        servo_sync_slider(channel);
        update_servo_value_label(channel);
    }
}

static esp_err_t servo_apply_angle(uint8_t channel)
{
    if (channel >= PCA9685_SERVO_CHANNEL_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!pca9685_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    return pca9685_set_servo_angle(channel, s_servo_angles[channel]);
}

static bool walk_emote_active(void)
{
    return s_emote == EMOTE_WALK_FORWARD || s_emote == EMOTE_WALK_BACKWARD ||
           s_emote == EMOTE_TURN_LEFT || s_emote == EMOTE_TURN_RIGHT || s_emote == EMOTE_MARCH;
}

static bool walk_emote_is(emote_state_t emote)
{
    return emote == EMOTE_WALK_FORWARD || emote == EMOTE_WALK_BACKWARD ||
           emote == EMOTE_TURN_LEFT || emote == EMOTE_TURN_RIGHT || emote == EMOTE_MARCH;
}

static bool alarm_shuffle_active(void)
{
    return s_alarm_ringing;
}

static bool walk_ramp_active(void)
{
    return walk_emote_active() || alarm_shuffle_active();
}

static bool walk_mode_is_turn(void)
{
    return s_walk_params.mode == WALK_MODE_TURN_LEFT || s_walk_params.mode == WALK_MODE_TURN_RIGHT;
}

static bool walk_mode_is_march(void)
{
    return s_walk_params.mode == WALK_MODE_MARCH;
}

static void motion_play(pixel_chirp_t chirp)
{
    if (s_audio_ready) {
        pixel_voice_play(chirp);
    }
}

static pixel_chirp_t motion_step_chirp(void)
{
    if (s_voice_seq_phase == VOICE_SEQ_FLEE) {
        return PIXEL_CHIRP_MOTION_STEP_FAST;
    }
    if (s_voice_seq_phase == VOICE_SEQ_SNEAK) {
        return PIXEL_CHIRP_MOTION_STEP_QUIET;
    }
    if (walk_mode_is_march()) {
        return PIXEL_CHIRP_MOTION_STEP_MARCH;
    }
    return (s_walk_steps_done & 1u) ? PIXEL_CHIRP_MOTION_STEP_ALT : PIXEL_CHIRP_MOTION_STEP;
}

static void walk_params_apply_waddle_feet(walk_params_t *p)
{
    p->foot_lift_deg = WALK_TURN_FOOT_LIFT_DEG;
    p->foot_plant_deg = WALK_TURN_FOOT_PLANT_DEG;
    p->phase_min_ticks =
        (WALK_TURN_WADDLE_PHASE_MIN_MS + SERVO_RAMP_PERIOD_MS - 1) / SERVO_RAMP_PERIOD_MS;
}

static walk_params_t walk_params_default(void)
{
    walk_params_t p = {
        .phase_min_ticks = WALK_PHASE_MIN_TICKS,
        .leg_step_deg = WALK_LEG_STEP_DEG,
        .foot_plant_deg = WALK_FOOT_PLANT_DEG,
        .foot_lift_deg = WALK_FOOT_LIFT_DEG,
        .ramp_step_deg = WALK_RAMP_STEP_DEG,
        .mode = WALK_MODE_FORWARD,
        .foot_tilt_deg = 0,
        .right_foot_plant_bias_deg = 0,
    };
    return p;
}

static walk_params_t walk_params_for_emote(emote_state_t emote)
{
    walk_params_t p = walk_params_default();
    if (emote == EMOTE_WALK_FORWARD) {
        p.phase_min_ticks = WALK_FORWARD_PHASE_MIN_TICKS;
        p.leg_step_deg = WALK_FORWARD_LEG_STEP_DEG;
        p.foot_lift_deg = WALK_FORWARD_FOOT_LIFT_DEG;
        p.foot_plant_deg = WALK_FORWARD_FOOT_PLANT_DEG;
        p.right_foot_plant_bias_deg = WALK_FORWARD_RIGHT_FOOT_PLANT_BIAS_DEG;
    } else if (emote == EMOTE_WALK_BACKWARD) {
        p.mode = WALK_MODE_BACKWARD;
    } else if (emote == EMOTE_TURN_LEFT) {
        p.mode = WALK_MODE_TURN_LEFT;
        walk_params_apply_waddle_feet(&p);
    } else if (emote == EMOTE_TURN_RIGHT) {
        p.mode = WALK_MODE_TURN_RIGHT;
        walk_params_apply_waddle_feet(&p);
    } else if (emote == EMOTE_MARCH) {
        p.mode = WALK_MODE_MARCH;
        walk_params_apply_waddle_feet(&p);
    }
    return p;
}

static walk_params_t walk_params_alarm_march(void)
{
    walk_params_t p = walk_params_for_emote(EMOTE_MARCH);
    p.phase_min_ticks = ALARM_MARCH_PHASE_MIN_TICKS;
    p.ramp_step_deg = ALARM_MARCH_RAMP_STEP_DEG;
    return p;
}

static walk_params_t walk_params_sneak(void)
{
    walk_params_t p = walk_params_default();
    p.phase_min_ticks =
        (WALK_SNEAK_PHASE_MIN_MS + SERVO_RAMP_PERIOD_MS - 1) / SERVO_RAMP_PERIOD_MS;
    p.leg_step_deg = WALK_SNEAK_LEG_STEP_DEG;
    p.foot_lift_deg = WALK_SNEAK_FOOT_LIFT_DEG;
    p.foot_plant_deg = WALK_FOOT_PLANT_DEG / 2U;
    return p;
}

static walk_params_t walk_params_flee(void)
{
    walk_params_t p = walk_params_default();
    p.phase_min_ticks =
        (WALK_FLEE_PHASE_MIN_MS + SERVO_RAMP_PERIOD_MS - 1) / SERVO_RAMP_PERIOD_MS;
    p.ramp_step_deg = WALK_FLEE_RAMP_STEP_DEG;
    p.leg_step_deg = WALK_FLEE_LEG_STEP_DEG;
    p.foot_lift_deg = WALK_FLEE_FOOT_LIFT_DEG;
    p.foot_plant_deg = WALK_FLEE_FOOT_PLANT_DEG;
    p.mode = WALK_MODE_BACKWARD;
    p.foot_tilt_deg = WALK_FLEE_FOOT_TILT_DEG;
    return p;
}

static bool all_servos_at_target(void)
{
    for (uint8_t channel = 0; channel < PCA9685_SERVO_CHANNEL_COUNT; ++channel) {
        if (s_servo_angles[channel] != s_servo_target[channel]) {
            return false;
        }
    }
    return true;
}

static uint32_t servo_ramp_step_for_remainder(uint32_t rem_deg)
{
    if (rem_deg <= SERVO_RAMP_EASE_ZONE_DEG) {
        return SERVO_RAMP_STEP_SLOW_DEG;
    }
    return SERVO_RAMP_STEP_FAST_DEG;
}

static void servo_ramp_tick(void)
{
    for (uint8_t channel = 0; channel < PCA9685_SERVO_CHANNEL_COUNT; ++channel) {
        if (s_servo_angles[channel] == s_servo_target[channel]) {
            s_servo_ramp_creep_tick[channel] = 0;
            continue;
        }

        int32_t cur = (int32_t)s_servo_angles[channel];
        int32_t tgt = (int32_t)s_servo_target[channel];
        uint32_t rem = (uint32_t)(cur < tgt ? tgt - cur : cur - tgt);

        if (walk_ramp_active()) {
            s_servo_ramp_creep_tick[channel] = 0;
            uint32_t step = s_walk_params.ramp_step_deg;
            int32_t next;

            if (cur < tgt) {
                next = cur + (int32_t)step;
                if (next > tgt) {
                    next = tgt;
                }
            } else {
                next = cur - (int32_t)step;
                if (next < tgt) {
                    next = tgt;
                }
            }

            s_servo_angles[channel] = (uint16_t)next;
            esp_err_t err = servo_apply_angle(channel);
            if (err != ESP_OK && pca9685_ready()) {
                ESP_LOGW(TAG, "Servo %u update failed: %s", (unsigned)channel, esp_err_to_name(err));
            }

            if (s_screen_mode == SCREEN_SERVOS) {
                servo_sync_slider(channel);
                update_servo_value_label(channel);
            }
            continue;
        }

        if (rem <= SERVO_RAMP_CREEP_ZONE_DEG) {
            s_servo_ramp_creep_tick[channel]++;
            if ((s_servo_ramp_creep_tick[channel] & 1U) != 0U) {
                continue;
            }
        } else {
            s_servo_ramp_creep_tick[channel] = 0;
        }

        uint32_t step = servo_ramp_step_for_remainder(rem);
        int32_t next;

        if (cur < tgt) {
            next = cur + (int32_t)step;
            if (next > tgt) {
                next = tgt;
            }
        } else {
            next = cur - (int32_t)step;
            if (next < tgt) {
                next = tgt;
            }
        }

        s_servo_angles[channel] = (uint16_t)next;
        esp_err_t err = servo_apply_angle(channel);
        if (err != ESP_OK && pca9685_ready()) {
            ESP_LOGW(TAG, "Servo %u update failed: %s", (unsigned)channel, esp_err_to_name(err));
        }

        if (s_screen_mode == SCREEN_SERVOS) {
            servo_sync_slider(channel);
            update_servo_value_label(channel);
        }
    }
}

static void emote_tick(void);

static void servo_ramp_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    emote_tick();
    servo_ramp_tick();
}

static void servo_go_to_center(uint8_t channel)
{
    if (channel >= PCA9685_SERVO_CHANNEL_COUNT) {
        return;
    }
    servo_set_target(channel, s_servo_center[channel]);
}

static void servo_set_targets_center_feet(void)
{
    servo_set_target(SERVO_CH_LEFT_FOOT, s_servo_center[SERVO_CH_LEFT_FOOT]);
    servo_set_target(SERVO_CH_RIGHT_FOOT, s_servo_center[SERVO_CH_RIGHT_FOOT]);
}

static void servos_open_cb(lv_event_t *e)
{
    (void)e;
    servo_sync_ui();
    set_screen_mode(SCREEN_SERVOS);
}

static void servos_done_cb(lv_event_t *e)
{
    (void)e;
    servo_save_centers_to_nvs();
    set_screen_mode(SCREEN_SETTINGS);
}

static void servos_center_cb(lv_event_t *e)
{
    (void)e;
    for (uint8_t channel = 0; channel < PCA9685_SERVO_CHANNEL_COUNT; ++channel) {
        servo_go_to_center(channel);
    }
}

static void servo_set_center_cb(lv_event_t *e)
{
    uint8_t channel = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (channel >= PCA9685_SERVO_CHANNEL_COUNT) {
        return;
    }

    s_servo_center[channel] = s_servo_target[channel];
    servo_save_centers_to_nvs();
    update_servo_value_label(channel);
}

static void servo_slider_event_cb(lv_event_t *e)
{
    if (s_servo_ignore_slider_cb) {
        return;
    }

    uint8_t channel = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (channel >= PCA9685_SERVO_CHANNEL_COUNT) {
        return;
    }

    lv_obj_t *slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    if (value < 0) {
        value = 0;
    }

    servo_set_target(channel, (uint16_t)value);
    update_servo_value_label(channel);
}

static void emote_apply_foot_tilt_deg(int16_t delta_deg)
{
    servo_set_target(SERVO_CH_LEFT_FOOT,
                     servo_physical_from_offset(SERVO_CH_LEFT_FOOT, delta_deg));
    servo_set_target(SERVO_CH_RIGHT_FOOT,
                     servo_physical_from_offset(SERVO_CH_RIGHT_FOOT, -delta_deg));
}

static void emote_center_legs(void)
{
    servo_go_to_center(SERVO_CH_LEFT_LEG);
    servo_go_to_center(SERVO_CH_RIGHT_LEG);
}

static void emote_stop(void)
{
    for (uint8_t channel = 0; channel < PCA9685_SERVO_CHANNEL_COUNT; ++channel) {
        servo_go_to_center(channel);
    }
}

/** Left leg +delta, right leg -delta (twist). Flip signs here if a leg runs backward. */
static void emote_apply_dance_legs_deg(int16_t left_delta_deg)
{
    servo_set_target(SERVO_CH_LEFT_LEG,
                     servo_physical_from_offset(SERVO_CH_LEFT_LEG, left_delta_deg));
    servo_set_target(SERVO_CH_RIGHT_LEG,
                     servo_physical_from_offset(SERVO_CH_RIGHT_LEG, -left_delta_deg));
}

static void walk_apply_feet(int16_t left_delta, int16_t right_delta)
{
    servo_set_target(SERVO_CH_LEFT_FOOT,
                     servo_physical_from_offset(SERVO_CH_LEFT_FOOT, left_delta));
    servo_set_target(SERVO_CH_RIGHT_FOOT,
                     servo_physical_from_offset(SERVO_CH_RIGHT_FOOT, right_delta));
}

static void walk_apply_legs_parallel(int16_t delta)
{
    servo_set_target(SERVO_CH_LEFT_LEG, servo_physical_from_offset(SERVO_CH_LEFT_LEG, delta));
    servo_set_target(SERVO_CH_RIGHT_LEG, servo_physical_from_offset(SERVO_CH_RIGHT_LEG, delta));
}

static void walk_set_leg_offset(uint8_t channel, int16_t delta)
{
    servo_set_target(channel, servo_physical_from_offset(channel, delta));
}

/** Right hip servo runs mirrored vs left; negate swing on right leg. */
static int16_t turn_swing_delta_for_leg(uint8_t channel, int16_t swing)
{
    if (channel == SERVO_CH_RIGHT_LEG) {
        return (int16_t)(-swing);
    }
    return swing;
}

static void turn_set_swing_leg(uint8_t channel, int16_t swing)
{
    walk_set_leg_offset(channel, turn_swing_delta_for_leg(channel, swing));
}

static void turn_apply_feet_lean_left(void)
{
    const int16_t plant = (int16_t)s_walk_params.foot_plant_deg;
    const int16_t lift = (int16_t)s_walk_params.foot_lift_deg;
    walk_apply_feet((int16_t)(-plant), lift);
}

static void turn_apply_feet_lean_right(void)
{
    const int16_t plant = (int16_t)s_walk_params.foot_plant_deg;
    const int16_t lift = (int16_t)s_walk_params.foot_lift_deg;
    walk_apply_feet(lift, (int16_t)(-plant));
}

static void turn_apply_feet_lean_right_pivot(void)
{
    const int16_t plant =
        (int16_t)(s_walk_params.foot_plant_deg + TURN_RIGHT_EXTRA_PLANT_DEG);
    const int16_t lift = (int16_t)(s_walk_params.foot_lift_deg + TURN_RIGHT_EXTRA_LIFT_DEG);
    walk_apply_feet(lift, (int16_t)(-plant));
}

/** Turn left: lean L → swing L forward → lean R → straighten L (repeat). Right is mirrored. */
static void turn_apply_pose(void)
{
    const int16_t swing = (int16_t)TURN_SWING_LEG_DEG;
    const bool turn_left = (s_walk_params.mode == WALK_MODE_TURN_LEFT);

    switch (s_turn_subphase) {
    case TURN_SUB_LEAN_PIVOT:
        walk_set_leg_offset(SERVO_CH_LEFT_LEG, 0);
        walk_set_leg_offset(SERVO_CH_RIGHT_LEG, 0);
        if (turn_left) {
            turn_apply_feet_lean_left();
        } else {
            turn_apply_feet_lean_right_pivot();
        }
        break;
    case TURN_SUB_ROTATE_SWING:
        if (turn_left) {
            turn_apply_feet_lean_left();
            turn_set_swing_leg(SERVO_CH_LEFT_LEG, swing);
            walk_set_leg_offset(SERVO_CH_RIGHT_LEG, 0);
        } else {
            turn_apply_feet_lean_right_pivot();
            turn_set_swing_leg(SERVO_CH_RIGHT_LEG, swing);
            walk_set_leg_offset(SERVO_CH_LEFT_LEG, 0);
        }
        break;
    case TURN_SUB_LEAN_OTHER:
        if (turn_left) {
            turn_apply_feet_lean_right();
            turn_set_swing_leg(SERVO_CH_LEFT_LEG, swing);
            walk_set_leg_offset(SERVO_CH_RIGHT_LEG, 0);
        } else {
            turn_apply_feet_lean_left();
            turn_set_swing_leg(SERVO_CH_RIGHT_LEG, swing);
            walk_set_leg_offset(SERVO_CH_LEFT_LEG, 0);
        }
        break;
    case TURN_SUB_STRAIGHTEN_SWING:
        if (turn_left) {
            turn_apply_feet_lean_right();
            walk_set_leg_offset(SERVO_CH_LEFT_LEG, 0);
            walk_set_leg_offset(SERVO_CH_RIGHT_LEG, 0);
        } else {
            turn_apply_feet_lean_left();
            walk_set_leg_offset(SERVO_CH_RIGHT_LEG, 0);
            walk_set_leg_offset(SERVO_CH_LEFT_LEG, 0);
        }
        break;
    default:
        break;
    }
}

static uint16_t spin_phase_min_ticks(void)
{
    if (s_spin_subphase == SPIN_SUB_LEAN_LEFT) {
        return SPIN_LEAN_PHASE_MIN_TICKS;
    }
    return SPIN_SWING_PHASE_MIN_TICKS;
}

static void spin_apply_pose(void)
{
    switch (s_spin_subphase) {
    case SPIN_SUB_LEAN_LEFT:
        walk_set_leg_offset(SERVO_CH_LEFT_LEG, 0);
        walk_set_leg_offset(SERVO_CH_RIGHT_LEG, 0);
        turn_apply_feet_lean_left();
        break;
    case SPIN_SUB_SWING_FWD:
        turn_apply_feet_lean_left();
        turn_set_swing_leg(SERVO_CH_LEFT_LEG, (int16_t)SPIN_SWING_FWD_DEG);
        walk_set_leg_offset(SERVO_CH_RIGHT_LEG, 0);
        break;
    case SPIN_SUB_SWING_BACK:
        turn_apply_feet_lean_left();
        turn_set_swing_leg(SERVO_CH_LEFT_LEG, (int16_t)(-SPIN_SWING_BACK_DEG));
        walk_set_leg_offset(SERVO_CH_RIGHT_LEG, 0);
        break;
    default:
        break;
    }
}

static void spin_complete(void)
{
    s_spin_visual_active = false;
    s_emote = EMOTE_NONE;
    if (s_voice_seq_active && s_voice_seq_phase == VOICE_SEQ_SPIN) {
        voice_sequence_end();
        return;
    }
    emote_stop();
    emote_sync_btn_styles();
}

static void spin_gait_tick(void)
{
    if (++s_spin_phase_tick < spin_phase_min_ticks()) {
        return;
    }
    if (!all_servos_at_target()) {
        return;
    }

    s_spin_phase_tick = 0;

    switch (s_spin_subphase) {
    case SPIN_SUB_LEAN_LEFT:
        s_spin_subphase = SPIN_SUB_SWING_FWD;
        spin_apply_pose();
        motion_play(PIXEL_CHIRP_MOTION_SPIN);
        break;
    case SPIN_SUB_SWING_FWD:
        s_spin_subphase = SPIN_SUB_SWING_BACK;
        spin_apply_pose();
        motion_play(PIXEL_CHIRP_MOTION_SPIN);
        break;
    case SPIN_SUB_SWING_BACK:
        spin_complete();
        break;
    default:
        break;
    }
}

static void turn_gait_tick(void)
{
    if (++s_walk_phase_tick < s_walk_params.phase_min_ticks) {
        return;
    }
    if (!all_servos_at_target()) {
        return;
    }

    s_walk_phase_tick = 0;

    if (s_turn_subphase == TURN_SUB_STRAIGHTEN_SWING) {
        s_walk_steps_done++;
        if (s_walk_steps_done >= s_walk_step_target) {
            walk_stop();
            return;
        }
        s_turn_subphase = TURN_SUB_LEAN_PIVOT;
    } else {
        s_turn_subphase = (turn_subphase_t)((int)s_turn_subphase + 1);
    }

    turn_apply_pose();
    motion_play(PIXEL_CHIRP_MOTION_TURN);
}

static bool walk_feet_at_target(void)
{
    return s_servo_angles[SERVO_CH_LEFT_FOOT] == s_servo_target[SERVO_CH_LEFT_FOOT] &&
           s_servo_angles[SERVO_CH_RIGHT_FOOT] == s_servo_target[SERVO_CH_RIGHT_FOOT];
}

static bool walk_legs_at_target(void)
{
    return s_servo_angles[SERVO_CH_LEFT_LEG] == s_servo_target[SERVO_CH_LEFT_LEG] &&
           s_servo_angles[SERVO_CH_RIGHT_LEG] == s_servo_target[SERVO_CH_RIGHT_LEG];
}

/** Feet first, then hips (or settle for march). */
static void walk_apply_pose(void)
{
    const int16_t plant = (int16_t)s_walk_params.foot_plant_deg;
    const int16_t lift = (int16_t)s_walk_params.foot_lift_deg;
    const int16_t leg_step = (int16_t)s_walk_params.leg_step_deg;

    switch (s_walk_subphase) {
    case WALK_SUB_FEET:
        if (s_walk_side == WALK_SIDE_LEFT) {
            walk_apply_feet((int16_t)(-plant), lift);
        } else {
            const int16_t right_plant =
                (int16_t)(-(plant + (int16_t)s_walk_params.right_foot_plant_bias_deg));
            walk_apply_feet(lift, right_plant);
        }
        break;
    case WALK_SUB_HIPS:
        if (!walk_mode_is_march()) {
            int16_t hip = (s_walk_side == WALK_SIDE_LEFT) ? leg_step : (int16_t)(-leg_step);
            if (s_walk_params.mode == WALK_MODE_BACKWARD) {
                hip = (int16_t)(-hip);
            }
            walk_apply_legs_parallel(hip);
        }
        break;
    case WALK_SUB_SETTLE:
        emote_center_legs();
        servo_set_targets_center_feet();
        break;
    default:
        break;
    }
}

static void walk_advance_step(void)
{
    s_walk_steps_done++;
    if (!s_alarm_ringing && s_walk_steps_done >= s_walk_step_target) {
        walk_stop();
        return;
    }

    s_walk_side = (s_walk_side == WALK_SIDE_LEFT) ? WALK_SIDE_RIGHT : WALK_SIDE_LEFT;
    s_walk_subphase = WALK_SUB_FEET;
    walk_apply_pose();
    if (!s_alarm_ringing) {
        motion_play(motion_step_chirp());
    }
}

static void walk_stop(void)
{
    if (s_voice_seq_active &&
        (s_voice_seq_phase == VOICE_SEQ_WALK || s_voice_seq_phase == VOICE_SEQ_SNEAK ||
         s_voice_seq_phase == VOICE_SEQ_FLEE || s_voice_seq_phase == VOICE_SEQ_TURN ||
         s_voice_seq_phase == VOICE_SEQ_MARCH || s_voice_seq_phase == VOICE_SEQ_PATROL)) {
        voice_on_walk_segment_done();
        return;
    }

    s_walk_params = walk_params_default();
    emote_stop();
    s_emote = EMOTE_NONE;
    emote_sync_btn_styles();
}

static void walk_start_with_params(uint8_t step_count, emote_state_t walk_emote, walk_params_t params)
{
    s_walk_params = params;
    s_walk_step_target = step_count;
    s_walk_steps_done = 0;
    s_walk_side = WALK_SIDE_LEFT;
    s_walk_subphase = WALK_SUB_FEET;
    s_walk_phase_tick = 0;
    s_emote = walk_emote;
    if (walk_mode_is_turn()) {
        s_turn_subphase = TURN_SUB_LEAN_PIVOT;
    }
    emote_center_legs();
    if (params.foot_tilt_deg != 0) {
        emote_apply_foot_tilt_deg(params.foot_tilt_deg);
    } else {
        servo_set_targets_center_feet();
    }
    if (walk_mode_is_turn()) {
        turn_apply_pose();
    } else {
        walk_apply_pose();
    }
    emote_sync_btn_styles();
}

static uint8_t walk_step_count_for_emote(emote_state_t emote)
{
    if (emote == EMOTE_WALK_FORWARD) {
        return EMOTE_WALK_FORWARD_STEP_COUNT;
    }
    return EMOTE_WALK_STEP_COUNT;
}

static void walk_start(uint8_t step_count, emote_state_t walk_emote)
{
    walk_start_with_params(step_count, walk_emote, walk_params_for_emote(walk_emote));
}

static void alarm_shuffle_start(void)
{
    if (s_emote != EMOTE_NONE) {
        s_walk_params = walk_params_default();
        s_emote = EMOTE_NONE;
        emote_sync_btn_styles();
    }
    emote_stop();
    s_walk_params = walk_params_alarm_march();
    s_walk_step_target = 255;
    s_walk_steps_done = 0;
    s_walk_side = WALK_SIDE_LEFT;
    s_walk_subphase = WALK_SUB_FEET;
    s_walk_phase_tick = 0;
    emote_center_legs();
    servo_set_targets_center_feet();
    walk_apply_pose();
}

static void alarm_shuffle_stop(void)
{
    s_walk_params = walk_params_default();
    emote_center_legs();
    servo_set_targets_center_feet();
}

static void walk_tick(void)
{
    if (walk_mode_is_turn()) {
        turn_gait_tick();
        return;
    }

    if (++s_walk_phase_tick < s_walk_params.phase_min_ticks) {
        return;
    }

    if (s_walk_subphase == WALK_SUB_FEET) {
        if (!walk_feet_at_target()) {
            return;
        }
        s_walk_phase_tick = 0;
        if (walk_mode_is_march()) {
            s_walk_subphase = WALK_SUB_SETTLE;
        } else {
            s_walk_subphase = WALK_SUB_HIPS;
        }
        walk_apply_pose();
        return;
    }

    if (s_walk_subphase == WALK_SUB_HIPS) {
        if (!walk_legs_at_target()) {
            return;
        }
        s_walk_phase_tick = 0;
        walk_advance_step();
        return;
    }

    if (s_walk_subphase == WALK_SUB_SETTLE) {
        if (!all_servos_at_target()) {
            return;
        }
        s_walk_phase_tick = 0;
        walk_advance_step();
        return;
    }
}

static void emote_apply_static_emote(emote_state_t emote)
{
    switch (emote) {
    case EMOTE_LEAN_RIGHT:
        emote_center_legs();
        emote_apply_foot_tilt_deg((int16_t)EMOTE_LEAN_DELTA_DEG);
        break;
    case EMOTE_LEAN_LEFT:
        emote_center_legs();
        emote_apply_foot_tilt_deg((int16_t)(-EMOTE_LEAN_DELTA_DEG));
        break;
    default:
        break;
    }
}

static int16_t emote_triangle_swing_deg(int16_t amp_deg, uint16_t half_period_ticks, uint16_t tick)
{
    if (half_period_ticks == 0U || amp_deg <= 0) {
        return 0;
    }

    const uint16_t period = 2U * half_period_ticks;
    uint16_t t = tick % period;
    int32_t amp = amp_deg;

    if (t < half_period_ticks) {
        return (int16_t)((amp * (int32_t)t) / (int32_t)half_period_ticks);
    }
    t -= half_period_ticks;
    return (int16_t)(amp - (amp * (int32_t)t) / (int32_t)half_period_ticks);
}

static int16_t emote_dizzy_swing_deg(void)
{
    return emote_triangle_swing_deg(EMOTE_DIZZY_SWING_DEG, EMOTE_DIZZY_HALF_PERIOD_TICKS, s_emote_dizzy_tick);
}

static void jump_apply_pose(void)
{
    switch (s_jump_subphase) {
    case JUMP_SUB_CROUCH:
        walk_apply_legs_parallel((int16_t)(-JUMP_CROUCH_LEG_DEG));
        servo_set_targets_center_feet();
        break;
    case JUMP_SUB_LAUNCH:
        walk_apply_legs_parallel((int16_t)JUMP_LAUNCH_LEG_DEG);
        walk_apply_feet((int16_t)JUMP_LAUNCH_FOOT_DEG, (int16_t)JUMP_LAUNCH_FOOT_DEG);
        break;
    case JUMP_SUB_LAND:
        emote_stop();
        break;
    default:
        break;
    }
}

static void jump_start(void)
{
    s_jump_subphase = JUMP_SUB_CROUCH;
    s_emote = EMOTE_JUMP;
    jump_apply_pose();
}

static void jump_tick(void)
{
    if (!all_servos_at_target()) {
        return;
    }

    if (s_jump_subphase == JUMP_SUB_CROUCH) {
        s_jump_subphase = JUMP_SUB_LAUNCH;
        jump_apply_pose();
        return;
    }

    if (s_jump_subphase == JUMP_SUB_LAUNCH) {
        s_jump_subphase = JUMP_SUB_LAND;
        jump_apply_pose();
        return;
    }

    if (s_jump_subphase == JUMP_SUB_LAND) {
        motion_play(PIXEL_CHIRP_MOTION_LAND);
        s_jump_subphase = JUMP_SUB_DONE;
        s_emote = EMOTE_NONE;
        if (s_voice_seq_active && s_voice_seq_phase == VOICE_SEQ_JUMP) {
            voice_sequence_end();
        }
    }
}

static void emote_tick(void)
{
    if (s_alarm_ringing) {
        walk_tick();
        return;
    }

    if (s_emote == EMOTE_SPIN) {
        spin_gait_tick();
        return;
    }

    if (walk_emote_active()) {
        walk_tick();
        return;
    }

    if (s_emote == EMOTE_JUMP) {
        jump_tick();
        return;
    }

    if (s_emote == EMOTE_DANCE) {
        servo_set_targets_center_feet();
        s_emote_dance_tick++;
        if (s_emote_dance_tick % EMOTE_DANCE_HALF_PERIOD_TICKS == 0) {
            motion_play(PIXEL_CHIRP_MOTION_DANCE);
        }
        int16_t swing = emote_triangle_swing_deg(EMOTE_DANCE_DELTA_DEG, EMOTE_DANCE_HALF_PERIOD_TICKS,
                                                 s_emote_dance_tick);
        int16_t left_delta = (int16_t)((int32_t)EMOTE_DANCE_DELTA_DEG - 2 * (int32_t)swing);
        emote_apply_dance_legs_deg(left_delta);
        return;
    }

    if (s_emote != EMOTE_DIZZY_LEFT && s_emote != EMOTE_DIZZY_RIGHT) {
        return;
    }

    s_emote_dizzy_tick++;
    if (s_emote_dizzy_tick % EMOTE_DIZZY_HALF_PERIOD_TICKS == 0) {
        motion_play(PIXEL_CHIRP_MOTION_DIZZY);
    }
    int16_t swing = emote_dizzy_swing_deg();
    int16_t delta;

    if (s_emote == EMOTE_DIZZY_LEFT) {
        delta = (int16_t)(-(int32_t)EMOTE_DIZZY_BIAS_DEG - (int32_t)swing);
    } else {
        delta = (int16_t)((int32_t)EMOTE_DIZZY_BIAS_DEG + (int32_t)swing);
    }
    emote_apply_foot_tilt_deg(delta);
}

static void emote_sync_btn_styles(void)
{
    lv_color_t active = lv_color_hex(0xFFDD00);
    lv_color_t idle = lv_color_hex(0x333344);

    if (s_emote_lean_left_btn != NULL) {
        lv_obj_set_style_bg_color(s_emote_lean_left_btn,
                                  s_emote == EMOTE_LEAN_LEFT ? active : idle, LV_PART_MAIN);
    }
    if (s_emote_lean_right_btn != NULL) {
        lv_obj_set_style_bg_color(s_emote_lean_right_btn,
                                  s_emote == EMOTE_LEAN_RIGHT ? active : idle, LV_PART_MAIN);
    }
    if (s_emote_dizzy_left_btn != NULL) {
        lv_obj_set_style_bg_color(s_emote_dizzy_left_btn,
                                  s_emote == EMOTE_DIZZY_LEFT ? active : idle, LV_PART_MAIN);
    }
    if (s_emote_dizzy_right_btn != NULL) {
        lv_obj_set_style_bg_color(s_emote_dizzy_right_btn,
                                  s_emote == EMOTE_DIZZY_RIGHT ? active : idle, LV_PART_MAIN);
    }
    if (s_emote_dance_btn != NULL) {
        lv_obj_set_style_bg_color(s_emote_dance_btn,
                                  s_emote == EMOTE_DANCE ? active : idle, LV_PART_MAIN);
    }
    if (s_emote_walk_btn != NULL) {
        lv_obj_set_style_bg_color(s_emote_walk_btn,
                                  s_emote == EMOTE_WALK_FORWARD ? active : idle, LV_PART_MAIN);
    }
    if (s_emote_walk_back_btn != NULL) {
        lv_obj_set_style_bg_color(s_emote_walk_back_btn,
                                  s_emote == EMOTE_WALK_BACKWARD ? active : idle, LV_PART_MAIN);
    }
}

static void emote_select(emote_state_t emote)
{
    if (s_emote == emote) {
        if (walk_emote_is(emote)) {
            walk_stop();
        } else {
            s_emote = EMOTE_NONE;
            emote_stop();
        }
    } else {
        if (walk_emote_active()) {
            walk_stop();
        }
        s_emote = emote;
        if (walk_emote_is(emote)) {
            walk_start(walk_step_count_for_emote(emote), emote);
        } else if (emote == EMOTE_DIZZY_LEFT || emote == EMOTE_DIZZY_RIGHT) {
            emote_center_legs();
            s_emote_dizzy_tick = 0;
        } else if (emote == EMOTE_DANCE) {
            servo_set_targets_center_feet();
            s_emote_dance_tick = 0;
        } else {
            emote_apply_static_emote(emote);
        }
    }
    emote_sync_btn_styles();
}

static void emote_start(emote_state_t emote)
{
    if (walk_emote_is(emote)) {
        walk_start(walk_step_count_for_emote(emote), emote);
        return;
    }

    s_emote = emote;
    if (emote == EMOTE_DIZZY_LEFT || emote == EMOTE_DIZZY_RIGHT) {
        emote_center_legs();
        s_emote_dizzy_tick = 0;
    } else if (emote == EMOTE_DANCE) {
        servo_set_targets_center_feet();
        s_emote_dance_tick = 0;
    } else {
        emote_apply_static_emote(emote);
    }
    emote_sync_btn_styles();
}

static void emotes_open_cb(lv_event_t *e)
{
    (void)e;
    emote_sync_btn_styles();
    set_screen_mode(SCREEN_EMOTES);
}

static void emotes_done_cb(lv_event_t *e)
{
    (void)e;
    set_screen_mode(SCREEN_SETTINGS);
}

static void emote_lean_left_cb(lv_event_t *e)
{
    (void)e;
    emote_select(EMOTE_LEAN_LEFT);
}

static void emote_lean_right_cb(lv_event_t *e)
{
    (void)e;
    emote_select(EMOTE_LEAN_RIGHT);
}

static void emote_dizzy_left_cb(lv_event_t *e)
{
    (void)e;
    emote_select(EMOTE_DIZZY_LEFT);
}

static void emote_dizzy_right_cb(lv_event_t *e)
{
    (void)e;
    emote_select(EMOTE_DIZZY_RIGHT);
}

static void emote_dance_cb(lv_event_t *e)
{
    (void)e;
    emote_select(EMOTE_DANCE);
}

static void emote_walk_forward_cb(lv_event_t *e)
{
    (void)e;
    emote_select(EMOTE_WALK_FORWARD);
}

static void emote_walk_backward_cb(lv_event_t *e)
{
    (void)e;
    emote_select(EMOTE_WALK_BACKWARD);
}

/** Settings: always show. Face: only if SOC < 25% and not charging. Alarm: hidden. */
static void battery_sync_visibility(void)
{
    if (s_battery_root == NULL) {
        return;
    }

    int pct = 0;
    bool charging = false;
    esp_err_t err = battery_pmu_get_level(&pct, &charging);
    bool have_level = (err == ESP_OK);

    bool show = false;
    /* Settings + picker screens: always show battery bar (same as settings). */
    if (s_screen_mode == SCREEN_SETTINGS || s_screen_mode == SCREEN_EMOTIONS || s_screen_mode == SCREEN_SERVOS ||
        s_screen_mode == SCREEN_EMOTES) {
        show = true;
    } else if (s_screen_mode == SCREEN_FACE && have_level) {
        show = (pct < 25 && !charging);
    }

    if (show) {
        lv_obj_clear_flag(s_battery_root, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_battery_root, LV_OBJ_FLAG_HIDDEN);
    }
}

/** Charging = green; else >50% white, 25–50% yellow, <25% red. */
static lv_color_t battery_bar_fill_color(int pct, bool charging)
{
    if (charging) {
        return lv_color_hex(0x22DD55);
    }
    if (pct < 25) {
        return lv_color_hex(0xFF4444);
    }
    if (pct <= 50) {
        return lv_color_hex(0xFFDD00);
    }
    return lv_color_white();
}

static void update_battery_ui(void)
{
    int pct = 0;
    bool charging = false;
    esp_err_t err = battery_pmu_get_level(&pct, &charging);

    if (err != ESP_OK || s_battery_bar == NULL) {
        if (s_battery_bar != NULL) {
            lv_bar_set_value(s_battery_bar, 0, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(s_battery_bar, lv_color_hex(0x444444), LV_PART_INDICATOR);
        }
        battery_sync_visibility();
        return;
    }

    lv_bar_set_value(s_battery_bar, pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(s_battery_bar, battery_bar_fill_color(pct, charging), LV_PART_INDICATOR);
    battery_sync_visibility();
}

static void alarm_picker_value_changed_cb(lv_event_t *e)
{
    lv_obj_t *roller = lv_event_get_target(e);
    if (roller == s_alarm_picker_hours) {
        s_alarm_hours = (uint8_t)lv_roller_get_selected(s_alarm_picker_hours);
    } else if (roller == s_alarm_picker_minutes) {
        s_alarm_minutes = (uint8_t)lv_roller_get_selected(s_alarm_picker_minutes);
    } else if (roller == s_alarm_picker_seconds) {
        s_alarm_seconds = (uint8_t)lv_roller_get_selected(s_alarm_picker_seconds);
    }
    update_alarm_time_labels();
}

static void alarm_time_segment_tap_cb(lv_event_t *e)
{
    s_alarm_selected_field = (alarm_field_t)(intptr_t)lv_event_get_user_data(e);
    sync_picker_with_time_values();
    style_alarm_picker_focus();
    set_alarm_picker_visible(true);
}

static void alarm_picker_set_cb(lv_event_t *e)
{
    (void)e;
    set_alarm_picker_visible(false);
}

static void alarm_arm_from_ms(uint32_t total_ms)
{
    if (total_ms == 0) {
        return;
    }
    if (s_alarm_ringing) {
        stop_alarm_ringing();
    }
    set_alarm_time_from_ms(total_ms);
    sync_picker_with_time_values();
    update_alarm_time_labels();
    s_alarm_deadline_ms = lv_tick_get() + total_ms;
    s_alarm_armed = true;
    update_alarm_action_label();
}

static void alarm_stop(void)
{
    if (s_alarm_ringing) {
        stop_alarm_ringing();
        update_alarm_action_label();
        return;
    }

    if (s_alarm_armed) {
        s_alarm_armed = false;
        update_alarm_action_label();
    }
}

static void alarm_action_btn_cb(lv_event_t *e)
{
    (void)e;

    if (s_alarm_ringing || s_alarm_armed) {
        alarm_stop();
        return;
    }

    uint32_t total_ms = alarm_total_ms_from_fields();
    alarm_arm_from_ms(total_ms);
}

static void voice_stop_timer(void)
{
    bsp_display_lock((uint32_t)-1);
    alarm_stop();
    bsp_display_unlock();
}

static void clock_picker_value_changed_cb(lv_event_t *e)
{
    (void)e;
    if (s_clock_picker_hour == NULL) {
        return;
    }
    s_clock_pick_hour12 = (uint8_t)(lv_roller_get_selected(s_clock_picker_hour) + 1);
    s_clock_pick_minute = (uint8_t)lv_roller_get_selected(s_clock_picker_minute);
    s_clock_pick_pm = lv_roller_get_selected(s_clock_picker_ampm) != 0;
}

static void clock_open_picker_cb(lv_event_t *e)
{
    (void)e;
    sync_clock_picker_rollers_from_wall_clock();
    set_clock_picker_visible(true);
}

static void clock_picker_set_cb(lv_event_t *e)
{
    (void)e;
    struct tm tm_local = { 0 };
    time_t base = time(NULL);
    if (s_clock_wall_time_valid || base >= (time_t)1577836800) {
        localtime_r(&base, &tm_local);
    } else {
        /* Calendar anchor when wall time was never set (epoch). */
        tm_local.tm_year = 124;
        tm_local.tm_mon = 5;
        tm_local.tm_mday = 1;
    }

    int h24;
    if (s_clock_pick_pm) {
        h24 = (s_clock_pick_hour12 == 12) ? 12 : (s_clock_pick_hour12 + 12);
    } else {
        h24 = (s_clock_pick_hour12 == 12) ? 0 : s_clock_pick_hour12;
    }
    tm_local.tm_hour = h24;
    tm_local.tm_min = s_clock_pick_minute;
    tm_local.tm_sec = 0;
    tm_local.tm_isdst = -1;

    time_t newt = mktime(&tm_local);
    struct timeval tv = { .tv_sec = newt, .tv_usec = 0 };
    if (settimeofday(&tv, NULL) == 0) {
        s_clock_wall_time_valid = true;
        clock_save_wall_time_to_nvs(newt);
    }
    set_clock_picker_visible(false);
    update_clock_labels();
}

static void leave_alarm_screen(void)
{
    if (s_alarm_ringing) {
        stop_alarm_ringing();
    }
    set_alarm_picker_visible(false);
    update_alarm_action_label();
}

static void enter_alarm_screen(void)
{
    update_alarm_time_labels();
    update_alarm_action_label();
}

static int carousel_mode_to_index(screen_mode_t m)
{
    switch (m) {
    case SCREEN_FACE:
        return 0;
    case SCREEN_CLOCK:
        return 1;
    case SCREEN_ALARM:
        return 2;
    default:
        return -1;
    }
}

static screen_mode_t carousel_index_to_mode(int idx)
{
    switch ((idx % 3 + 3) % 3) {
    case 0:
        return SCREEN_FACE;
    case 1:
        return SCREEN_CLOCK;
    default:
        return SCREEN_ALARM;
    }
}

static void carousel_go(int delta)
{
    int idx = carousel_mode_to_index(s_screen_mode);
    if (idx < 0) {
        return;
    }

    int next_idx = (idx + delta + 3) % 3;
    screen_mode_t next_mode = carousel_index_to_mode(next_idx);

    if (s_screen_mode == SCREEN_ALARM && next_mode != SCREEN_ALARM) {
        leave_alarm_screen();
    }

    set_screen_mode(next_mode);

    if (next_mode == SCREEN_ALARM) {
        enter_alarm_screen();
    } else if (next_mode == SCREEN_CLOCK) {
        update_clock_labels();
    }
}

static void show_clock_screen(void)
{
    if (s_clock_panel == NULL) {
        return;
    }

    bsp_display_lock((uint32_t)-1);
    if (s_screen_mode == SCREEN_ALARM) {
        leave_alarm_screen();
    }
    set_clock_picker_visible(false);
    set_screen_mode(SCREEN_CLOCK);
    update_clock_labels();
    bsp_display_unlock();
}

static void show_alarm_screen(void)
{
    if (s_alarm_panel == NULL) {
        return;
    }

    bsp_display_lock((uint32_t)-1);
    if (s_screen_mode != SCREEN_ALARM) {
        if (s_screen_mode == SCREEN_CLOCK) {
            set_clock_picker_visible(false);
        }
        set_screen_mode(SCREEN_ALARM);
        enter_alarm_screen();
    } else {
        set_alarm_picker_visible(false);
        enter_alarm_screen();
    }
    bsp_display_unlock();
}

static void voice_start_timer(void)
{
    if (s_alarm_panel == NULL) {
        return;
    }

    uint32_t total_ms = alarm_total_ms_from_fields();
    if (total_ms == 0) {
        return;
    }

    show_alarm_screen();
    bsp_display_lock((uint32_t)-1);
    alarm_arm_from_ms(total_ms);
    bsp_display_unlock();
}

static void voice_sequence_end(void)
{
    if (s_voice_seq_timer != NULL) {
        lv_timer_delete(s_voice_seq_timer);
        s_voice_seq_timer = NULL;
    }
    s_spin_visual_active = false;
    s_patrol_plan_idx = 0;
    s_spin_subphase = SPIN_SUB_LEAN_LEFT;
    s_spin_phase_tick = 0;
    s_walk_params = walk_params_default();
    emote_stop();
    s_emote = EMOTE_NONE;
    s_jump_subphase = JUMP_SUB_DONE;
    emote_sync_btn_styles();
    emotion_set_visual(s_voice_saved_emotion, false);
    s_voice_seq_active = false;
    s_voice_seq_phase = VOICE_SEQ_NONE;
}

static void enter_low_power_mode(void)
{
    if (s_low_power_active) {
        return;
    }

    if (s_voice_seq_active) {
        voice_sequence_end();
    } else if (s_emote != EMOTE_NONE) {
        emote_stop();
        s_emote = EMOTE_NONE;
        emote_sync_btn_styles();
    }

    alarm_stop();
    set_screen_mode(SCREEN_FACE);
    s_wake_look_active = false;
    s_blink_start_ms = 0;
    s_wink_start_ms = 0;
    s_low_power_active = true;
    face_geom_cache_invalidate();
    update_eye_geometry();
    s_low_power_saved_brightness = bsp_display_brightness_get();
    if (s_low_power_saved_brightness < 10) {
        s_low_power_saved_brightness = 10;
    } else if (s_low_power_saved_brightness > 100) {
        s_low_power_saved_brightness = 100;
    }
    bsp_display_backlight_off();
}

static void exit_low_power_mode(void)
{
    if (!s_low_power_active) {
        return;
    }

    s_low_power_active = false;
    bsp_display_brightness_set(s_low_power_saved_brightness);
    if (s_brightness_slider != NULL) {
        lv_slider_set_value(s_brightness_slider, s_low_power_saved_brightness, LV_ANIM_OFF);
    }
    if (s_left_eye != NULL && s_right_eye != NULL) {
        lv_obj_set_style_radius(s_left_eye, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_radius(s_right_eye, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    }
    face_geom_cache_invalidate();
    update_eye_geometry();
}

static void face_tap_wake_cb(lv_event_t *e)
{
    (void)e;
    if (!s_low_power_active) {
        return;
    }
    bsp_display_lock((uint32_t)-1);
    exit_low_power_mode();
    bsp_display_unlock();
}

static void patrol_run_current_segment(void);
static void patrol_pause_timer_cb(lv_timer_t *timer);

static void patrol_run_current_segment(void)
{
    if (s_patrol_plan_idx >= PATROL_PLAN_LEN) {
        voice_sequence_end();
        return;
    }

    patrol_seg_t seg = s_patrol_plan[s_patrol_plan_idx++];

    switch (seg.kind) {
    case PATROL_SEG_FWD:
        walk_start_with_params(seg.arg, EMOTE_WALK_FORWARD, walk_params_for_emote(EMOTE_WALK_FORWARD));
        break;
    case PATROL_SEG_TURN: {
        walk_params_t params = walk_params_for_emote(EMOTE_TURN_RIGHT);
        walk_start_with_params(seg.arg, EMOTE_TURN_RIGHT, params);
        break;
    }
    case PATROL_SEG_TURN_LEFT: {
        walk_params_t params = walk_params_for_emote(EMOTE_TURN_LEFT);
        walk_start_with_params(seg.arg, EMOTE_TURN_LEFT, params);
        break;
    }
    case PATROL_SEG_PAUSE:
        if (s_voice_seq_timer != NULL) {
            lv_timer_delete(s_voice_seq_timer);
        }
        s_voice_seq_timer = lv_timer_create(patrol_pause_timer_cb, VOICE_PATROL_PAUSE_MS, NULL);
        lv_timer_set_repeat_count(s_voice_seq_timer, 1);
        break;
    default:
        voice_sequence_end();
        break;
    }
}

static void patrol_pause_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    bsp_display_lock((uint32_t)-1);
    s_voice_seq_timer = NULL;
    if (s_voice_seq_active && s_voice_seq_phase == VOICE_SEQ_PATROL) {
        patrol_run_current_segment();
    }
    bsp_display_unlock();
}

static void voice_on_walk_segment_done(void)
{
    switch (s_voice_seq_phase) {
    case VOICE_SEQ_WALK:
    case VOICE_SEQ_SNEAK:
    case VOICE_SEQ_FLEE:
    case VOICE_SEQ_TURN:
    case VOICE_SEQ_MARCH:
        voice_sequence_end();
        break;
    case VOICE_SEQ_PATROL:
        patrol_run_current_segment();
        break;
    default:
        break;
    }
}

static void voice_motion_begin(voice_seq_phase_t phase, emotion_preset_t face)
{
    if (s_voice_seq_active) {
        voice_sequence_end();
    }

    s_voice_saved_emotion = emotion_transition_animating() ? s_emotion_anim_to : s_emotion;
    s_voice_seq_active = true;
    s_voice_seq_phase = phase;

    if (s_screen_mode == SCREEN_ALARM) {
        leave_alarm_screen();
    }
    set_screen_mode(SCREEN_FACE);
    emotion_set_visual(face, false);
}

static void voice_seq_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    bsp_display_lock((uint32_t)-1);
    if (s_voice_seq_phase == VOICE_SEQ_DANCE) {
        voice_sequence_end();
    } else if (s_voice_seq_phase == VOICE_SEQ_DIZZY_EMOTE) {
        emote_stop();
        s_emote = EMOTE_NONE;
        emote_sync_btn_styles();
        emotion_set_visual(EMOTION_CONFUSED, false);
        s_voice_seq_phase = VOICE_SEQ_DIZZY_CONFUSED;
        lv_timer_set_period(s_voice_seq_timer, VOICE_DIZZY_CONFUSED_MS);
        lv_timer_reset(s_voice_seq_timer);
    } else if (s_voice_seq_phase == VOICE_SEQ_DIZZY_CONFUSED) {
        voice_sequence_end();
    } else if (s_voice_seq_phase == VOICE_SEQ_SPIN) {
        voice_sequence_end();
    } else if (s_voice_seq_phase == VOICE_SEQ_JUMP) {
        voice_sequence_end();
    }
    bsp_display_unlock();
}

static void voice_dance_start(void)
{
    bsp_display_lock((uint32_t)-1);
    voice_motion_begin(VOICE_SEQ_DANCE, EMOTION_HAPPY);
    emote_start(EMOTE_DANCE);

    if (s_voice_seq_timer != NULL) {
        lv_timer_delete(s_voice_seq_timer);
    }
    s_voice_seq_timer = lv_timer_create(voice_seq_timer_cb, VOICE_DANCE_DURATION_MS, NULL);
    bsp_display_unlock();
}

static void voice_dizzy_start(void)
{
    emote_state_t dizzy_emote = (esp_random() & 1U) ? EMOTE_DIZZY_LEFT : EMOTE_DIZZY_RIGHT;

    bsp_display_lock((uint32_t)-1);
    if (s_voice_seq_active) {
        voice_sequence_end();
    }

    s_voice_saved_emotion = emotion_transition_animating() ? s_emotion_anim_to : s_emotion;
    s_voice_seq_active = true;
    s_voice_seq_phase = VOICE_SEQ_DIZZY_EMOTE;

    if (s_screen_mode == SCREEN_ALARM) {
        leave_alarm_screen();
    }
    set_screen_mode(SCREEN_FACE);
    emote_start(dizzy_emote);

    if (s_voice_seq_timer != NULL) {
        lv_timer_delete(s_voice_seq_timer);
    }
    s_voice_seq_timer = lv_timer_create(voice_seq_timer_cb, VOICE_DIZZY_EMOTE_MS, NULL);
    bsp_display_unlock();
}

static void voice_walk_forward_start(void)
{
    bsp_display_lock((uint32_t)-1);
    voice_motion_begin(VOICE_SEQ_WALK, EMOTION_HAPPY);
    emote_start(EMOTE_WALK_FORWARD);
    bsp_display_unlock();
}

static void voice_walk_backward_start(void)
{
    bsp_display_lock((uint32_t)-1);
    voice_motion_begin(VOICE_SEQ_WALK, EMOTION_HAPPY);
    emote_start(EMOTE_WALK_BACKWARD);
    bsp_display_unlock();
}

static void voice_turn_left_start(void)
{
    bsp_display_lock((uint32_t)-1);
    voice_motion_begin(VOICE_SEQ_TURN, EMOTION_SERIOUS);
    walk_start_with_params(VOICE_TURN_CYCLE_COUNT, EMOTE_TURN_LEFT, walk_params_for_emote(EMOTE_TURN_LEFT));
    bsp_display_unlock();
}

static void voice_turn_right_start(void)
{
    bsp_display_lock((uint32_t)-1);
    voice_motion_begin(VOICE_SEQ_TURN, EMOTION_SERIOUS);
    walk_start_with_params(VOICE_TURN_CYCLE_COUNT, EMOTE_TURN_RIGHT, walk_params_for_emote(EMOTE_TURN_RIGHT));
    bsp_display_unlock();
}

static void voice_march_start(void)
{
    bsp_display_lock((uint32_t)-1);
    voice_motion_begin(VOICE_SEQ_MARCH, EMOTION_SERIOUS);
    walk_start_with_params(VOICE_MARCH_STEP_COUNT, EMOTE_MARCH, walk_params_for_emote(EMOTE_MARCH));
    bsp_display_unlock();
}

static void voice_sneak_start(void)
{
    bsp_display_lock((uint32_t)-1);
    voice_motion_begin(VOICE_SEQ_SNEAK, EMOTION_SERIOUS);
    walk_start_with_params(VOICE_SNEAK_STEP_COUNT, EMOTE_WALK_FORWARD, walk_params_sneak());
    bsp_display_unlock();
}

static void voice_run_away_start(void)
{
    bsp_display_lock((uint32_t)-1);
    voice_motion_begin(VOICE_SEQ_FLEE, EMOTION_CONFUSED);
    walk_start_with_params(VOICE_FLEE_STEP_COUNT, EMOTE_WALK_BACKWARD, walk_params_flee());
    bsp_display_unlock();
}

static void voice_spin_start(void)
{
    bsp_display_lock((uint32_t)-1);
    voice_motion_begin(VOICE_SEQ_SPIN, EMOTION_HAPPY);
    s_spin_visual_active = true;
    emotion_apply_visibility();
    s_spin_subphase = SPIN_SUB_LEAN_LEFT;
    s_spin_phase_tick = 0;
    s_emote = EMOTE_SPIN;
    spin_apply_pose();

    if (s_voice_seq_timer != NULL) {
        lv_timer_delete(s_voice_seq_timer);
    }
    s_voice_seq_timer = lv_timer_create(voice_seq_timer_cb, VOICE_SPIN_MAX_MS, NULL);
    bsp_display_unlock();
}

static void voice_jump_start(void)
{
    bsp_display_lock((uint32_t)-1);
    voice_motion_begin(VOICE_SEQ_JUMP, EMOTION_HAPPY);
    jump_start();

    if (s_voice_seq_timer != NULL) {
        lv_timer_delete(s_voice_seq_timer);
    }
    s_voice_seq_timer = lv_timer_create(voice_seq_timer_cb, VOICE_JUMP_MAX_MS, NULL);
    bsp_display_unlock();
}

static void voice_patrol_start(void)
{
    bsp_display_lock((uint32_t)-1);
    voice_motion_begin(VOICE_SEQ_PATROL, EMOTION_SERIOUS);
    s_patrol_plan_idx = 0;
    patrol_run_current_segment();
    bsp_display_unlock();
}

static void speech_command_cb(int command_id)
{
    switch (command_id) {
    case SPEECH_CMD_WHAT_TIME_IS_IT:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_TIME_CMD);
        }
        show_clock_screen();
        break;
    case SPEECH_CMD_START_TIMER:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_START_TIMER);
        }
        voice_start_timer();
        break;
    case SPEECH_CMD_SHOW_DANCE:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_DO_DANCE);
        }
        voice_dance_start();
        break;
    case SPEECH_CMD_ARE_YOU_DIZZY:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_ARE_YOU_DIZZY);
        }
        voice_dizzy_start();
        break;
    case SPEECH_CMD_WALK_FORWARD:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_WALK_FORWARD);
        }
        voice_walk_forward_start();
        break;
    case SPEECH_CMD_WALK_BACKWARD:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_WALK_BACKWARD);
        }
        voice_walk_backward_start();
        break;
    case SPEECH_CMD_TURN_LEFT:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_TURN_LEFT);
        }
        voice_turn_left_start();
        break;
    case SPEECH_CMD_TURN_RIGHT:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_TURN_RIGHT);
        }
        voice_turn_right_start();
        break;
    case SPEECH_CMD_MARCH:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_MARCH);
        }
        voice_march_start();
        break;
    case SPEECH_CMD_SPIN:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_DO_SPIN);
        }
        voice_spin_start();
        break;
    case SPEECH_CMD_JUMP:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_DO_JUMP);
        }
        voice_jump_start();
        break;
    case SPEECH_CMD_SNEAK:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_SNEAK_AROUND);
        }
        voice_sneak_start();
        break;
    case SPEECH_CMD_PATROL:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_PATROL);
        }
        voice_patrol_start();
        break;
    case SPEECH_CMD_RUN_AWAY:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_GET_BACK);
        }
        voice_run_away_start();
        break;
    case SPEECH_CMD_STOP_TIMER:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_STOP_TIMER);
        }
        voice_stop_timer();
        break;
    case SPEECH_CMD_LOW_POWER:
        if (s_audio_ready) {
            play_robot_sound(ROBOT_SOUND_LOW_POWER);
        }
        bsp_display_lock((uint32_t)-1);
        enter_low_power_mode();
        bsp_display_unlock();
        break;
    default:
        break;
    }
}

static void screen_gesture_event_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_BOTTOM) {
        if (s_screen_mode == SCREEN_FACE) {
            set_screen_mode(SCREEN_SETTINGS);
        }
    } else if (dir == LV_DIR_LEFT) {
        if (s_screen_mode == SCREEN_EMOTIONS) {
            emotion_cycle(1);
        } else if (carousel_mode_to_index(s_screen_mode) >= 0) {
            /* Next: Face → Clock → Timer → Face */
            carousel_go(1);
        }
    } else if (dir == LV_DIR_RIGHT) {
        if (s_screen_mode == SCREEN_EMOTIONS) {
            emotion_cycle(-1);
        } else if (carousel_mode_to_index(s_screen_mode) >= 0) {
            /* Previous: Face ← Timer ← Clock ← Face */
            carousel_go(-1);
        }
    } else if (dir == LV_DIR_TOP) {
        if (s_screen_mode == SCREEN_SETTINGS) {
            set_screen_mode(SCREEN_FACE);
        } else if (s_screen_mode == SCREEN_EMOTIONS || s_screen_mode == SCREEN_SERVOS ||
                   s_screen_mode == SCREEN_EMOTES) {
            set_screen_mode(SCREEN_SETTINGS);
        }
    }
    (void)e;
}

typedef struct {
    int16_t eff_w;
    int16_t eff_h;
    int16_t eff_left_x;
    int16_t eff_right_x;
    int16_t eff_y;
} emotion_eye_geom_t;

typedef struct emotion_mask_layout_s {
    int16_t w;
    int16_t h;
    int16_t xl;
    int16_t xr;
    int16_t y;
    bool circle;
} emotion_mask_layout_t;

static void face_mask_apply_if_changed(const emotion_mask_layout_t *ml, lv_opa_t opa)
{
    if (s_happy_mask_left == NULL) {
        return;
    }

    face_geom_cache_t *c = &s_face_geom_cache;
    bool hidden = (ml->w <= 0 || opa == LV_OPA_TRANSP);

    if (c->valid && c->mask_visible == !hidden && c->mask_opa == opa) {
        if (hidden) {
            return;
        }
        if (c->mask_w == ml->w && c->mask_h == ml->h && c->mask_xl == ml->xl && c->mask_xr == ml->xr &&
            c->mask_y == ml->y && c->mask_circle == ml->circle) {
            return;
        }
    }

    if (hidden) {
        lv_obj_add_flag(s_happy_mask_left, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_happy_mask_right, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_happy_mask_left, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_happy_mask_right, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(s_happy_mask_left, opa, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_happy_mask_right, opa, LV_PART_MAIN);
        lv_obj_set_size(s_happy_mask_left, ml->w, ml->h);
        lv_obj_set_size(s_happy_mask_right, ml->w, ml->h);
        lv_obj_set_style_radius(s_happy_mask_left, ml->circle ? LV_RADIUS_CIRCLE : 0, LV_PART_MAIN);
        lv_obj_set_style_radius(s_happy_mask_right, ml->circle ? LV_RADIUS_CIRCLE : 0, LV_PART_MAIN);
        lv_obj_set_pos(s_happy_mask_left, ml->xl, ml->y);
        lv_obj_set_pos(s_happy_mask_right, ml->xr, ml->y);
    }

    c->mask_visible = !hidden;
    c->mask_w = ml->w;
    c->mask_h = ml->h;
    c->mask_xl = ml->xl;
    c->mask_xr = ml->xr;
    c->mask_y = ml->y;
    c->mask_circle = ml->circle;
    c->mask_opa = opa;
    c->valid = true;
}

static bool face_needs_eye_geom_update(bool confused_visual)
{
    if (s_screen_mode != SCREEN_FACE && s_screen_mode != SCREEN_EMOTIONS) {
        return false;
    }
    if (emotion_transition_animating()) {
        return true;
    }
    if (confused_visual && s_emotion == EMOTION_CONFUSED) {
        return false;
    }
    if (!s_face_geom_cache.valid) {
        return true;
    }
    if (s_blink_start_ms > 0 || s_wink_start_ms > 0) {
        return true;
    }
    if (s_look_x != s_target_look_x || s_look_y != s_target_look_y) {
        return true;
    }
    if (s_jarvis_flash_until_ms != 0 && lv_tick_get() < s_jarvis_flash_until_ms) {
        return true;
    }
    return false;
}

static bool emotion_uses_mask(emotion_preset_t e)
{
    return e == EMOTION_HAPPY || e == EMOTION_SERIOUS;
}

static void emotion_compute_eye_geom(emotion_preset_t e, emotion_eye_geom_t *out)
{
    out->eff_w = s_eye_w;
    out->eff_h = s_eye_h;
    out->eff_left_x = s_base_left_x;
    out->eff_right_x = s_base_right_x;
    out->eff_y = s_base_y;

    switch (e) {
    case EMOTION_IDLE:
        break;
    case EMOTION_HAPPY:
        out->eff_w = s_eye_w - 6;
        out->eff_h = s_eye_h - 76;
        out->eff_y = s_base_y + 38;
        out->eff_left_x = s_base_left_x + 3;
        out->eff_right_x = s_base_right_x + 3;
        break;
    case EMOTION_SERIOUS:
        out->eff_w = s_eye_w - 6;
        out->eff_h = s_eye_h - 24;
        out->eff_y = s_base_y + 4;
        out->eff_left_x = s_base_left_x + 3;
        out->eff_right_x = s_base_right_x + 3;
        break;
    case EMOTION_CONFUSED:
        /* Collapsed pupil strip while spirals take over (blend only). */
        out->eff_w = s_eye_w;
        out->eff_h = 14;
        out->eff_y = s_base_y + (s_eye_h / 2) - 7;
        out->eff_left_x = s_base_left_x;
        out->eff_right_x = s_base_right_x;
        break;
    default:
        break;
    }
}

static int16_t emotion_lerp_i16(int16_t a, int16_t b, uint32_t t256)
{
    return (int16_t)((int32_t)a + ((int32_t)(b - a) * (int32_t)t256) / 256);
}

static uint32_t emotion_smoothstep_u256(uint32_t t256)
{
    if (t256 >= 256) {
        return 256;
    }
    uint64_t t = t256;
    uint64_t num = t * t * (768 - 2 * t);
    return (uint32_t)(num / (256ULL * 256));
}

static void emotion_mask_layout_for(emotion_preset_t e, const emotion_eye_geom_t *eye, emotion_mask_layout_t *m)
{
    m->w = 0;
    m->h = 0;
    m->xl = 0;
    m->xr = 0;
    m->y = 0;
    m->circle = false;

    if (!emotion_uses_mask(e)) {
        return;
    }

    int16_t left_h = eye->eff_h;
    int16_t happy_eye_y = eye->eff_y + s_look_y + (eye->eff_h - left_h) / 2;

    if (e == EMOTION_HAPPY) {
        m->w = eye->eff_w - 8;
        m->h = eye->eff_h - 8;
        m->circle = true;
        m->xl = eye->eff_left_x + s_look_x + 4;
        m->xr = eye->eff_right_x + s_look_x + 4;
        m->y = happy_eye_y + 34;
    } else {
        int16_t cut_h = eye->eff_h / 2 + 6;
        m->w = eye->eff_w + 6;
        m->h = cut_h;
        m->circle = false;
        m->xl = eye->eff_left_x + s_look_x - 3;
        m->xr = eye->eff_right_x + s_look_x - 3;
        m->y = happy_eye_y - 4;
    }
}

static void emotion_apply_mask_layout(const emotion_mask_layout_t *ml, lv_opa_t opa)
{
    face_mask_apply_if_changed(ml, opa);
}

static void emotion_transition_complete_tick(void)
{
    if (s_emotion_anim_start_ms == 0) {
        return;
    }
    if ((lv_tick_get() - s_emotion_anim_start_ms) < EMOTION_TRANSITION_MS) {
        return;
    }
    s_emotion_anim_start_ms = 0;
    if (s_left_eye != NULL) {
        lv_obj_set_style_opa(s_left_eye, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(s_right_eye, LV_OPA_COVER, LV_PART_MAIN);
    }
    if (s_left_spiral_canvas != NULL) {
        lv_obj_set_style_opa(s_left_spiral_canvas, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(s_right_spiral_canvas, LV_OPA_COVER, LV_PART_MAIN);
    }
    if (s_happy_mask_left != NULL) {
        lv_obj_set_style_opa(s_happy_mask_left, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(s_happy_mask_right, LV_OPA_COVER, LV_PART_MAIN);
    }
    face_geom_cache_invalidate();
    emotion_apply_visibility();
}

/** Interpolate eyes (and optional spiral/mask crossfade) during emotion picker swipe. */
static void emotion_update_eye_geometry_transition(void)
{
    uint32_t elapsed = lv_tick_get() - s_emotion_anim_start_ms;
    uint32_t t256 = (elapsed >= EMOTION_TRANSITION_MS) ? 256 : (elapsed * 256 / EMOTION_TRANSITION_MS);
    uint32_t et = emotion_smoothstep_u256(t256);

    emotion_preset_t ef = s_emotion_anim_from;
    emotion_preset_t eto = s_emotion_anim_to;
    bool fc = (ef == EMOTION_CONFUSED);
    bool tc = (eto == EMOTION_CONFUSED);

    lv_color_t col = face_eye_bg_color(lv_tick_get());

    emotion_eye_geom_t ga;
    emotion_eye_geom_t gb;
    emotion_compute_eye_geom(ef, &ga);
    emotion_compute_eye_geom(eto, &gb);

    int16_t eff_w = emotion_lerp_i16(ga.eff_w, gb.eff_w, et);
    int16_t eff_h = emotion_lerp_i16(ga.eff_h, gb.eff_h, et);
    int16_t eff_y = emotion_lerp_i16(ga.eff_y, gb.eff_y, et);
    int16_t eff_left_x = emotion_lerp_i16(ga.eff_left_x, gb.eff_left_x, et);
    int16_t eff_right_x = emotion_lerp_i16(ga.eff_right_x, gb.eff_right_x, et);

    int16_t left_h = eff_h;
    int16_t right_h = eff_h;

    lv_opa_t eye_opa = LV_OPA_COVER;
    lv_opa_t spir_opa = LV_OPA_TRANSP;

    if (tc && !fc) {
        eye_opa = (lv_opa_t)(LV_OPA_COVER - (LV_OPA_COVER * et) / 256);
        spir_opa = (lv_opa_t)((LV_OPA_COVER * et) / 256);
    } else if (fc && !tc) {
        eye_opa = (lv_opa_t)((LV_OPA_COVER * et) / 256);
        spir_opa = (lv_opa_t)(LV_OPA_COVER - (LV_OPA_COVER * et) / 256);
    }

    lv_obj_set_style_bg_color(s_left_eye, col, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_right_eye, col, LV_PART_MAIN);

    face_eye_apply_if_changed(eff_left_x + s_look_x, eff_y + s_look_y + (eff_h - left_h) / 2, eff_w, left_h,
                              eff_right_x + s_look_x, eff_y + s_look_y + (eff_h - right_h) / 2, eff_w, right_h,
                              eye_opa, eye_opa);

    face_spiral_opa_apply_if_changed(spir_opa);

    emotion_mask_layout_t ma = { 0 };
    emotion_mask_layout_t mb = { 0 };
    emotion_mask_layout_t ml;

    if (!fc && !tc) {
        emotion_mask_layout_for(ef, &ga, &ma);
        emotion_mask_layout_for(eto, &gb, &mb);
        ml.w = emotion_lerp_i16(ma.w, mb.w, et);
        ml.h = emotion_lerp_i16(ma.h, mb.h, et);
        ml.xl = emotion_lerp_i16(ma.xl, mb.xl, et);
        ml.xr = emotion_lerp_i16(ma.xr, mb.xr, et);
        ml.y = emotion_lerp_i16(ma.y, mb.y, et);
        ml.circle = (emotion_uses_mask(ef) && emotion_uses_mask(eto)) ? (et < 140) ? ma.circle : mb.circle
                                                                      : emotion_uses_mask(eto) ? mb.circle
                                                                                              : ma.circle;

        lv_opa_t mask_opa = LV_OPA_TRANSP;
        bool mf = emotion_uses_mask(ef);
        bool mt = emotion_uses_mask(eto);
        if (!mf && !mt) {
            mask_opa = LV_OPA_TRANSP;
        } else if (!mf && mt) {
            mask_opa = (lv_opa_t)((LV_OPA_COVER * et) / 256);
        } else if (mf && !mt) {
            mask_opa = (lv_opa_t)(LV_OPA_COVER - (LV_OPA_COVER * et) / 256);
        } else {
            mask_opa = LV_OPA_COVER;
        }

        emotion_apply_mask_layout(&ml, mask_opa);
        return;
    }

    /* To/from confused: fade masks when happy/serious is on either end. */
    emotion_eye_geom_t eye_for_mask;
    eye_for_mask.eff_w = eff_w;
    eye_for_mask.eff_h = eff_h;
    eye_for_mask.eff_y = eff_y;
    eye_for_mask.eff_left_x = eff_left_x;
    eye_for_mask.eff_right_x = eff_right_x;

    lv_opa_t mask_opa = LV_OPA_TRANSP;
    emotion_preset_t mask_src = EMOTION_IDLE;

    if (tc && !fc && emotion_uses_mask(eto)) {
        mask_opa = (lv_opa_t)((LV_OPA_COVER * et) / 256);
        mask_src = eto;
    } else if (fc && !tc && emotion_uses_mask(ef)) {
        mask_opa = (lv_opa_t)(LV_OPA_COVER - (LV_OPA_COVER * et) / 256);
        mask_src = ef;
    }

    if (mask_opa != LV_OPA_TRANSP && emotion_uses_mask(mask_src)) {
        emotion_mask_layout_for(mask_src, &eye_for_mask, &ml);
        emotion_apply_mask_layout(&ml, mask_opa);
    } else {
        emotion_apply_mask_layout(&ma, LV_OPA_TRANSP);
    }
}

static void update_eye_geometry(void)
{
    if (s_screen_mode != SCREEN_FACE && s_screen_mode != SCREEN_EMOTIONS) {
        return;
    }

    if (s_face_wake_boot_pending) {
        face_apply_wake_boot_closed_eyes();
        return;
    }

    if (s_low_power_active) {
        face_apply_wake_boot_closed_eyes();
        return;
    }

    emotion_transition_complete_tick();

    bool animating = emotion_transition_animating();

    if (s_emotion == EMOTION_CONFUSED && !animating) {
        return;
    }

    if (animating) {
        emotion_update_eye_geometry_transition();
        return;
    }

    const uint32_t now = lv_tick_get();
    int16_t eff_w = s_eye_w;
    int16_t eff_h = s_eye_h;
    int16_t eff_left_x = s_base_left_x;
    int16_t eff_right_x = s_base_right_x;
    int16_t eff_y = s_base_y;
    lv_color_t col = face_eye_bg_color(now);

    switch (s_emotion) {
    case EMOTION_IDLE:
        break;
    case EMOTION_HAPPY:
        /* Circle-based eyes; mask keeps only the top crescent. */
        eff_w = s_eye_w - 6;
        eff_h = s_eye_h - 76;
        eff_y = s_base_y + 38;
        eff_left_x = s_base_left_x + 3;
        eff_right_x = s_base_right_x + 3;
        break;
    case EMOTION_SERIOUS:
        /* Half-eye look with straight top cut. */
        eff_w = s_eye_w - 6;
        eff_h = s_eye_h - 24;
        eff_y = s_base_y + 4;
        eff_left_x = s_base_left_x + 3;
        eff_right_x = s_base_right_x + 3;
        break;
    default:
        break;
    }

    int16_t left_h = eff_h;
    int16_t right_h = eff_h;

    /* Blink / wink for idle, happy, and serious (confused returns earlier). */
    if (s_blink_start_ms > 0) {
        uint32_t elapsed = now - s_blink_start_ms;
        if (elapsed >= s_blink_duration_ms) {
            s_blink_start_ms = 0;
            s_next_blink_ms = now + rand_range(2200, 5200);
        } else {
            uint32_t half = s_blink_duration_ms / 2;
            uint32_t phase = (elapsed <= half) ? elapsed : (s_blink_duration_ms - elapsed);
            uint32_t min_h = 26;
            if ((uint32_t)eff_h > min_h) {
                uint32_t blink_h = min_h + ((uint32_t)eff_h - min_h) * phase / (half ? half : 1);
                left_h = (int16_t)blink_h;
                right_h = (int16_t)blink_h;
            }
        }
    }

    if (s_wink_start_ms > 0) {
        uint32_t elapsed = now - s_wink_start_ms;
        if (elapsed >= s_wink_duration_ms) {
            s_wink_start_ms = 0;
            s_next_wink_ms = now + rand_range(4500, 9000);
        } else {
            uint32_t half = s_wink_duration_ms / 2;
            uint32_t phase = (elapsed <= half) ? elapsed : (s_wink_duration_ms - elapsed);
            uint32_t min_h = 22;
            if ((uint32_t)eff_h > min_h) {
                uint32_t wink_h = min_h + ((uint32_t)eff_h - min_h) * phase / (half ? half : 1);
                if (s_wink_left) {
                    left_h = (int16_t)wink_h;
                } else {
                    right_h = (int16_t)wink_h;
                }
            }
        }
    }

    lv_obj_set_style_bg_color(s_left_eye, col, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_right_eye, col, LV_PART_MAIN);

    face_eye_apply_if_changed(eff_left_x + s_look_x, eff_y + s_look_y + (eff_h - left_h) / 2, eff_w, left_h,
                              eff_right_x + s_look_x, eff_y + s_look_y + (eff_h - right_h) / 2, eff_w, right_h,
                              LV_OPA_COVER, LV_OPA_COVER);

    if ((s_emotion == EMOTION_HAPPY || s_emotion == EMOTION_SERIOUS) && s_happy_mask_left != NULL) {
        int16_t happy_eye_y = eff_y + s_look_y + (eff_h - left_h) / 2;
        emotion_mask_layout_t ml = { 0 };
        if (s_emotion == EMOTION_HAPPY) {
            ml.w = eff_w - 8;
            ml.h = eff_h - 8;
            ml.circle = true;
            ml.xl = eff_left_x + s_look_x + 4;
            ml.xr = eff_right_x + s_look_x + 4;
            ml.y = happy_eye_y + 34;
        } else {
            int16_t cut_h = eff_h / 2 + 6;
            ml.w = eff_w + 6;
            ml.h = cut_h;
            ml.circle = false;
            ml.xl = eff_left_x + s_look_x - 3;
            ml.xr = eff_right_x + s_look_x - 3;
            ml.y = happy_eye_y - 4;
        }
        face_mask_apply_if_changed(&ml, LV_OPA_COVER);
    } else if (s_happy_mask_left != NULL) {
        emotion_mask_layout_t ml = { 0 };
        face_mask_apply_if_changed(&ml, LV_OPA_TRANSP);
    }
}

static void face_apply_wake_boot_closed_eyes(void)
{
    if (s_left_eye == NULL || s_right_eye == NULL) {
        return;
    }

    const int16_t closed_h = FACE_WAKE_BOOT_CLOSED_H;
    lv_obj_set_style_radius(s_left_eye, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_right_eye, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_left_eye, lv_color_hex(FACE_EYE_COLOR_BLUE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_right_eye, lv_color_hex(FACE_EYE_COLOR_BLUE), LV_PART_MAIN);

    face_eye_apply_if_changed(s_base_left_x, s_base_y + (s_eye_h - closed_h) / 2, s_eye_w, closed_h,
                              s_base_right_x, s_base_y + (s_eye_h - closed_h) / 2, s_eye_w, closed_h,
                              LV_OPA_COVER, LV_OPA_COVER);
}

static void face_wake_boot_complete(void)
{
    bsp_display_lock((uint32_t)-1);
    s_face_wake_boot_pending = false;
    if (s_left_eye != NULL && s_right_eye != NULL) {
        lv_obj_set_style_radius(s_left_eye, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_radius(s_right_eye, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    }
    face_geom_cache_invalidate();
    update_eye_geometry();
    bsp_display_unlock();
}

static void face_idle_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_low_power_active) {
        return;
    }

    uint32_t now = lv_tick_get();

    if (s_alarm_armed) {
        uint32_t remaining_ms =
            (s_alarm_deadline_ms > now) ? (s_alarm_deadline_ms - now) : 0;
        if (!s_alarm_ringing && remaining_ms < 1000U) {
            start_alarm_ringing(now);
        } else {
            set_alarm_time_from_ms(remaining_ms);
            update_alarm_time_labels();
        }
    }

    if (s_alarm_ringing && now >= s_next_beep_ms) {
        s_next_beep_ms = now + 900;
        alarm_play_sound(true);
    }

    if (s_battery_root != NULL && now >= s_next_battery_poll_ms) {
        s_next_battery_poll_ms = now + 2000;
        update_battery_ui();
    }

    if (s_screen_mode == SCREEN_CLOCK && s_clock_panel != NULL && (now - s_last_clock_tick_ms) >= 1000) {
        s_last_clock_tick_ms = now;
        update_clock_labels();
    }

    const bool jarvis_flash = (s_jarvis_flash_until_ms != 0 && now < s_jarvis_flash_until_ms);
    if (s_jarvis_flash_was_active && !jarvis_flash) {
        s_jarvis_flash_until_ms = 0;
        face_geom_cache_invalidate();
    }
    s_jarvis_flash_was_active = jarvis_flash;

    /* Look / blink / wink only on idle eyes; other emotions stay fixed and centered. */
    if (!s_face_wake_boot_pending && s_emotion != EMOTION_CONFUSED) {
        if (s_emotion == EMOTION_IDLE) {
            if (s_wake_look_active) {
                if (now >= s_wake_look_next_ms) {
                    s_wake_look_step++;
                    if (s_wake_look_step >= sizeof(s_wake_look_path) / sizeof(s_wake_look_path[0])) {
                        s_wake_look_active = false;
                        s_next_look_change_ms = now + rand_range(900, 2400);
                    } else {
                        const wake_look_waypoint_t *wp = &s_wake_look_path[s_wake_look_step];
                        s_target_look_x = wp->x;
                        s_target_look_y = wp->y;
                        s_wake_look_next_ms = now + wp->hold_ms;
                    }
                }
            } else if (now >= s_next_look_change_ms) {
                s_target_look_x = (int16_t)rand_range(0, 20) - 10;
                s_target_look_y = (int16_t)rand_range(0, 10) - 5;
                s_next_look_change_ms = now + rand_range(900, 2400);
            }

            if (s_blink_start_ms == 0 && now >= s_next_blink_ms) {
                s_blink_start_ms = now;
                s_blink_duration_ms = rand_range(200, 400);
            }

            if (s_wink_start_ms == 0 && now >= s_next_wink_ms) {
                s_wink_start_ms = now;
                s_wink_duration_ms = rand_range(200, 400);
                s_wink_left = (esp_random() & 1U) == 0;
            }

            s_look_x += (s_target_look_x - s_look_x) / 3;
            s_look_y += (s_target_look_y - s_look_y) / 3;
        } else {
            s_wake_look_active = false;
            s_target_look_x = 0;
            s_target_look_y = 0;
            s_look_x += (0 - s_look_x) / 4;
            s_look_y += (0 - s_look_y) / 4;
            s_blink_start_ms = 0;
            s_wink_start_ms = 0;
        }
    } else {
        s_wake_look_active = false;
        s_target_look_x = 0;
        s_target_look_y = 0;
        s_look_x += (0 - s_look_x) / 4;
        s_look_y += (0 - s_look_y) / 4;
        s_blink_start_ms = 0;
        s_wink_start_ms = 0;
    }

    bool confused_visual = (s_emotion == EMOTION_CONFUSED) ||
                           (emotion_transition_animating() &&
                            (s_emotion_anim_from == EMOTION_CONFUSED || s_emotion_anim_to == EMOTION_CONFUSED));
    bool spin_visual = s_spin_visual_active &&
                       (s_screen_mode == SCREEN_FACE || s_screen_mode == SCREEN_EMOTIONS);
    if ((confused_visual || spin_visual) &&
        (s_screen_mode == SCREEN_FACE || s_screen_mode == SCREEN_EMOTIONS)) {
        const int16_t rotate_step =
            spin_visual ? (int16_t)(EMOTION_SPIRAL_ROTATE_STEP_DEG * 2) : EMOTION_SPIRAL_ROTATE_STEP_DEG;
        s_spiral_angle = (int16_t)((s_spiral_angle + rotate_step + 360) % 360);
        emotion_redraw_spirals();
    }

    if (face_needs_eye_geom_update(confused_visual || spin_visual)) {
        update_eye_geometry();
    }
}

void app_main(void)
{
    /* POSIX TZ for localtime(); wall clock is set from the clock screen or NVS. */
    setenv("TZ", "UTC0", 1);
    tzset();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    load_clock_from_nvs();
    load_emotion_from_nvs();
    servo_load_centers_from_nvs();
    servo_apply_loaded_centers_on_boot();

    bsp_display_cfg_t disp_cfg = {
        .lv_adapter_cfg = {
            .task_stack_size = 8 * 1024,
            .task_priority = 6,
            .task_core_id = 1,
            .tick_period_ms = 1,
            .task_min_delay_ms = 1,
            .task_max_delay_ms = 15,
            .stack_in_psram = false,
        },
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };
    bsp_display_start_with_config(&disp_cfg);

    esp_err_t bat_err = battery_pmu_init();
    if (bat_err != ESP_OK) {
        ESP_LOGW(TAG, "Battery gauge init failed: %s", esp_err_to_name(bat_err));
    }

    esp_err_t servo_err = pca9685_init();
    if (servo_err != ESP_OK) {
        ESP_LOGW(TAG, "PCA9685 init failed: %s", esp_err_to_name(servo_err));
    } else {
        for (uint8_t channel = 0; channel < PCA9685_SERVO_CHANNEL_COUNT; ++channel) {
            s_servo_target[channel] = s_servo_angles[channel];
            servo_err = servo_apply_angle(channel);
            if (servo_err != ESP_OK) {
                ESP_LOGW(TAG, "Servo %u default angle failed: %s", (unsigned)channel, esp_err_to_name(servo_err));
            }
        }
    }

    // Match Waveshare demo usage: block until LVGL lock is available.
    bsp_display_lock((uint32_t)-1);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(screen, face_tap_wake_cb, LV_EVENT_CLICKED, NULL);

    lv_color_t eye_color = lv_color_hex(FACE_EYE_COLOR_BLUE);

    s_left_eye = lv_obj_create(screen);
    lv_obj_remove_style_all(s_left_eye);
    lv_obj_set_size(s_left_eye, s_eye_w, s_eye_h);
    lv_obj_set_style_bg_color(s_left_eye, eye_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_left_eye, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_left_eye, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_pos(s_left_eye, s_base_left_x, s_base_y);

    s_right_eye = lv_obj_create(screen);
    lv_obj_remove_style_all(s_right_eye);
    lv_obj_set_size(s_right_eye, s_eye_w, s_eye_h);
    lv_obj_set_style_bg_color(s_right_eye, eye_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_right_eye, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_right_eye, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_pos(s_right_eye, s_base_right_x, s_base_y);

    LV_DRAW_BUF_INIT_STATIC(s_emotion_spiral_drawbuf_left);
    LV_DRAW_BUF_INIT_STATIC(s_emotion_spiral_drawbuf_right);

    s_left_spiral_canvas = lv_canvas_create(screen);
    lv_canvas_set_draw_buf(s_left_spiral_canvas, &s_emotion_spiral_drawbuf_left);
    lv_obj_set_size(s_left_spiral_canvas, EMOTION_SPIRAL_W, EMOTION_SPIRAL_H);
    lv_obj_set_pos(s_left_spiral_canvas, s_base_left_x + (s_eye_w - EMOTION_SPIRAL_W) / 2,
                   s_base_y + (s_eye_h - EMOTION_SPIRAL_H) / 2);
    lv_obj_add_flag(s_left_spiral_canvas, LV_OBJ_FLAG_HIDDEN);

    s_right_spiral_canvas = lv_canvas_create(screen);
    lv_canvas_set_draw_buf(s_right_spiral_canvas, &s_emotion_spiral_drawbuf_right);
    lv_obj_set_size(s_right_spiral_canvas, EMOTION_SPIRAL_W, EMOTION_SPIRAL_H);
    lv_obj_set_pos(s_right_spiral_canvas, s_base_right_x + (s_eye_w - EMOTION_SPIRAL_W) / 2,
                   s_base_y + (s_eye_h - EMOTION_SPIRAL_H) / 2);
    lv_obj_add_flag(s_right_spiral_canvas, LV_OBJ_FLAG_HIDDEN);

    s_tear_left = lv_obj_create(screen);
    lv_obj_remove_style_all(s_tear_left);
    lv_obj_set_size(s_tear_left, 14, 18);
    lv_obj_set_style_radius(s_tear_left, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_tear_left, lv_color_hex(0x55CCFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_tear_left, LV_OPA_80, LV_PART_MAIN);
    lv_obj_add_flag(s_tear_left, LV_OBJ_FLAG_HIDDEN);

    s_tear_right = lv_obj_create(screen);
    lv_obj_remove_style_all(s_tear_right);
    lv_obj_set_size(s_tear_right, 14, 18);
    lv_obj_set_style_radius(s_tear_right, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_tear_right, lv_color_hex(0x55CCFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_tear_right, LV_OPA_80, LV_PART_MAIN);
    lv_obj_add_flag(s_tear_right, LV_OBJ_FLAG_HIDDEN);

    s_happy_mask_left = lv_obj_create(screen);
    lv_obj_remove_style_all(s_happy_mask_left);
    lv_obj_set_style_bg_color(s_happy_mask_left, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_happy_mask_left, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_happy_mask_left, 22, LV_PART_MAIN);
    lv_obj_add_flag(s_happy_mask_left, LV_OBJ_FLAG_HIDDEN);

    s_happy_mask_right = lv_obj_create(screen);
    lv_obj_remove_style_all(s_happy_mask_right);
    lv_obj_set_style_bg_color(s_happy_mask_right, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_happy_mask_right, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_happy_mask_right, 22, LV_PART_MAIN);
    lv_obj_add_flag(s_happy_mask_right, LV_OBJ_FLAG_HIDDEN);

    s_settings_panel = lv_obj_create(screen);
    lv_obj_set_size(s_settings_panel, 390, 310);
    lv_obj_center(s_settings_panel);
    /* Transparent panel: true-black AMOLED screen shows through; no gray card. */
    lv_obj_set_style_bg_opa(s_settings_panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_settings_panel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_settings_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_settings_panel, LV_OBJ_FLAG_HIDDEN);

    s_brightness_slider = lv_slider_create(s_settings_panel);
    lv_obj_set_size(s_brightness_slider, 300, 18);
    lv_slider_set_range(s_brightness_slider, 10, 100);
    lv_slider_set_value(s_brightness_slider, bsp_display_brightness_get(), LV_ANIM_OFF);
    lv_obj_align(s_brightness_slider, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_add_event_cb(s_brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_settings_label = lv_label_create(s_settings_panel);
    lv_label_set_text(s_settings_label, "Brightness");
    lv_obj_set_style_text_color(s_settings_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_settings_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align_to(s_settings_label, s_brightness_slider, LV_ALIGN_OUT_TOP_MID, 0, -10);

    s_volume_slider = lv_slider_create(s_settings_panel);
    lv_obj_set_size(s_volume_slider, 300, 18);
    lv_slider_set_range(s_volume_slider, 0, 100);
    lv_slider_set_value(s_volume_slider, s_speaker_volume, LV_ANIM_OFF);
    lv_obj_align_to(s_volume_slider, s_brightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 24);
    lv_obj_add_event_cb(s_volume_slider, volume_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_volume_label = lv_label_create(s_settings_panel);
    lv_label_set_text(s_volume_label, "Volume");
    lv_obj_set_style_text_color(s_volume_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_volume_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align_to(s_volume_label, s_volume_slider, LV_ALIGN_OUT_TOP_MID, 0, -10);

    s_emotions_btn = lv_button_create(s_settings_panel);
    lv_obj_set_size(s_emotions_btn, 268, 44);
    lv_obj_align_to(s_emotions_btn, s_volume_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    lv_obj_set_style_radius(s_emotions_btn, 22, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_emotions_btn, lv_color_hex(0x333344), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emotions_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_emotions_btn, emotions_open_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *emotions_btn_lbl = lv_label_create(s_emotions_btn);
    lv_label_set_text(emotions_btn_lbl, "Emotions");
    lv_obj_set_style_text_color(emotions_btn_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(emotions_btn_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(emotions_btn_lbl);

    s_emotes_btn = lv_button_create(s_settings_panel);
    lv_obj_set_size(s_emotes_btn, 268, 44);
    lv_obj_align_to(s_emotes_btn, s_emotions_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    lv_obj_set_style_radius(s_emotes_btn, 22, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_emotes_btn, lv_color_hex(0x333344), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emotes_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_emotes_btn, emotes_open_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *emotes_btn_lbl = lv_label_create(s_emotes_btn);
    lv_label_set_text(emotes_btn_lbl, "Emotes");
    lv_obj_set_style_text_color(emotes_btn_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(emotes_btn_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(emotes_btn_lbl);

    s_servos_btn = lv_button_create(s_settings_panel);
    lv_obj_set_size(s_servos_btn, 268, 44);
    lv_obj_align_to(s_servos_btn, s_emotes_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    lv_obj_set_style_radius(s_servos_btn, 22, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_servos_btn, lv_color_hex(0x333344), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_servos_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_servos_btn, servos_open_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *servos_btn_lbl = lv_label_create(s_servos_btn);
    lv_label_set_text(servos_btn_lbl, "Servos");
    lv_obj_set_style_text_color(servos_btn_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(servos_btn_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(servos_btn_lbl);

    s_emotions_panel = lv_obj_create(screen);
    lv_obj_set_size(s_emotions_panel, 466, 118);
    lv_obj_align(s_emotions_panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_emotions_panel, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_emotions_panel, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emotions_panel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_emotions_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_emotions_panel, LV_OBJ_FLAG_HIDDEN);

    s_emotions_title_label = lv_label_create(s_emotions_panel);
    lv_label_set_text(s_emotions_title_label, "");
    lv_obj_add_flag(s_emotions_title_label, LV_OBJ_FLAG_HIDDEN);

    s_emotions_name_label = lv_label_create(screen);
    lv_label_set_text(s_emotions_name_label, emotion_display_name(s_emotion));
    lv_obj_set_style_text_color(s_emotions_name_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_emotions_name_label, &lv_font_montserrat_32, LV_PART_MAIN);
    /* Keep clear of top-center battery icon. */
    lv_obj_align(s_emotions_name_label, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_add_flag(s_emotions_name_label, LV_OBJ_FLAG_HIDDEN);

    s_emotions_hint_label = lv_label_create(s_emotions_panel);
    lv_label_set_text(s_emotions_hint_label, "");
    lv_obj_set_style_text_color(s_emotions_hint_label, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_emotions_hint_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(s_emotions_hint_label, LV_ALIGN_BOTTOM_LEFT, 14, -12);
    lv_obj_add_flag(s_emotions_hint_label, LV_OBJ_FLAG_HIDDEN);

    s_emotions_done_btn = lv_button_create(s_emotions_panel);
    lv_obj_set_size(s_emotions_done_btn, 102, 46);
    lv_obj_align(s_emotions_done_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(s_emotions_done_btn, lv_color_hex(0xFFDD00), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emotions_done_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_emotions_done_btn, 23, LV_PART_MAIN);
    lv_obj_add_event_cb(s_emotions_done_btn, emotions_done_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *emotions_done_lbl = lv_label_create(s_emotions_done_btn);
    lv_label_set_text(emotions_done_lbl, "Done");
    lv_obj_set_style_text_color(emotions_done_lbl, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(emotions_done_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(emotions_done_lbl);

    s_servo_panel = lv_obj_create(screen);
    lv_obj_set_size(s_servo_panel, 466, 466);
    lv_obj_set_pos(s_servo_panel, 0, 0);
    lv_obj_set_style_radius(s_servo_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_servo_panel, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_servo_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_servo_panel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_servo_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_servo_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_servo_panel, LV_OBJ_FLAG_HIDDEN);

    s_servo_title_label = lv_label_create(s_servo_panel);
    lv_label_set_text(s_servo_title_label, "Servos");
    lv_obj_set_style_text_color(s_servo_title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_servo_title_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_align(s_servo_title_label, LV_ALIGN_TOP_MID, 0, 28);

    for (uint8_t channel = 0; channel < PCA9685_SERVO_CHANNEL_COUNT; ++channel) {
        int16_t y = (int16_t)(-96 + (int16_t)channel * 64);
        char channel_text[8];

        lv_obj_t *servo_channel_label = lv_label_create(s_servo_panel);
        snprintf(channel_text, sizeof(channel_text), "S%u", (unsigned)channel);
        lv_label_set_text(servo_channel_label, channel_text);
        lv_obj_set_style_text_color(servo_channel_label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(servo_channel_label, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(servo_channel_label, LV_ALIGN_LEFT_MID, 28, y);

        s_servo_sliders[channel] = lv_slider_create(s_servo_panel);
        lv_obj_set_size(s_servo_sliders[channel], 160, 18);
        lv_slider_set_range(s_servo_sliders[channel], 0, 180);
        lv_slider_set_value(s_servo_sliders[channel], (int32_t)s_servo_angles[channel], LV_ANIM_OFF);
        lv_obj_align(s_servo_sliders[channel], LV_ALIGN_LEFT_MID, 96, y);
        lv_obj_add_event_cb(s_servo_sliders[channel], servo_slider_event_cb, LV_EVENT_VALUE_CHANGED,
                            (void *)(intptr_t)channel);

        s_servo_set_center_btns[channel] = lv_button_create(s_servo_panel);
        lv_obj_set_size(s_servo_set_center_btns[channel], 52, 36);
        lv_obj_align_to(s_servo_set_center_btns[channel], s_servo_sliders[channel], LV_ALIGN_OUT_RIGHT_MID, 8, 0);
        lv_obj_set_style_bg_color(s_servo_set_center_btns[channel], lv_color_hex(0x333344), LV_PART_MAIN);
        lv_obj_set_style_border_width(s_servo_set_center_btns[channel], 0, LV_PART_MAIN);
        lv_obj_set_style_radius(s_servo_set_center_btns[channel], 8, LV_PART_MAIN);
        lv_obj_add_event_cb(s_servo_set_center_btns[channel], servo_set_center_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)channel);
        lv_obj_t *servo_set_lbl = lv_label_create(s_servo_set_center_btns[channel]);
        lv_label_set_text(servo_set_lbl, "Set");
        lv_obj_set_style_text_color(servo_set_lbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(servo_set_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_center(servo_set_lbl);

        s_servo_value_labels[channel] = lv_label_create(s_servo_panel);
        lv_obj_set_style_text_color(s_servo_value_labels[channel], lv_color_hex(0xFFDD00), LV_PART_MAIN);
        lv_obj_set_style_text_font(s_servo_value_labels[channel], &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align_to(s_servo_value_labels[channel], s_servo_set_center_btns[channel], LV_ALIGN_OUT_RIGHT_MID, 8,
                        0);
        update_servo_value_label(channel);
    }

    s_servo_center_btn = lv_button_create(s_servo_panel);
    lv_obj_set_size(s_servo_center_btn, 170, 56);
    lv_obj_align(s_servo_center_btn, LV_ALIGN_BOTTOM_LEFT, 38, -28);
    lv_obj_set_style_bg_color(s_servo_center_btn, lv_color_hex(0x333344), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_servo_center_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_servo_center_btn, 28, LV_PART_MAIN);
    lv_obj_add_event_cb(s_servo_center_btn, servos_center_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *servo_center_lbl = lv_label_create(s_servo_center_btn);
    lv_label_set_text(servo_center_lbl, "Go center");
    lv_obj_set_style_text_color(servo_center_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(servo_center_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(servo_center_lbl);

    s_servo_done_btn = lv_button_create(s_servo_panel);
    lv_obj_set_size(s_servo_done_btn, 170, 56);
    lv_obj_align(s_servo_done_btn, LV_ALIGN_BOTTOM_RIGHT, -38, -28);
    lv_obj_set_style_bg_color(s_servo_done_btn, lv_color_hex(0xFFDD00), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_servo_done_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_servo_done_btn, 28, LV_PART_MAIN);
    lv_obj_add_event_cb(s_servo_done_btn, servos_done_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *servo_done_lbl = lv_label_create(s_servo_done_btn);
    lv_label_set_text(servo_done_lbl, "Done");
    lv_obj_set_style_text_color(servo_done_lbl, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(servo_done_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(servo_done_lbl);

    s_emotes_panel = lv_obj_create(screen);
    lv_obj_set_size(s_emotes_panel, 466, 466);
    lv_obj_set_pos(s_emotes_panel, 0, 0);
    lv_obj_set_style_radius(s_emotes_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_emotes_panel, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_emotes_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emotes_panel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_emotes_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_emotes_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_emotes_panel, LV_OBJ_FLAG_HIDDEN);

    s_emotes_title_label = lv_label_create(s_emotes_panel);
    lv_label_set_text(s_emotes_title_label, "Emotes");
    lv_obj_set_style_text_color(s_emotes_title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_emotes_title_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_align(s_emotes_title_label, LV_ALIGN_TOP_MID, 0, 28);

    s_emote_lean_left_btn = lv_button_create(s_emotes_panel);
    lv_obj_set_size(s_emote_lean_left_btn, 268, 48);
    lv_obj_align(s_emote_lean_left_btn, LV_ALIGN_CENTER, 0, -150);
    lv_obj_set_style_bg_color(s_emote_lean_left_btn, lv_color_hex(0x333344), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emote_lean_left_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_emote_lean_left_btn, 24, LV_PART_MAIN);
    lv_obj_add_event_cb(s_emote_lean_left_btn, emote_lean_left_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lean_left_lbl = lv_label_create(s_emote_lean_left_btn);
    lv_label_set_text(lean_left_lbl, "Lean left");
    lv_obj_set_style_text_color(lean_left_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lean_left_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(lean_left_lbl);

    s_emote_lean_right_btn = lv_button_create(s_emotes_panel);
    lv_obj_set_size(s_emote_lean_right_btn, 268, 48);
    lv_obj_align(s_emote_lean_right_btn, LV_ALIGN_CENTER, 0, -98);
    lv_obj_set_style_bg_color(s_emote_lean_right_btn, lv_color_hex(0x333344), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emote_lean_right_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_emote_lean_right_btn, 24, LV_PART_MAIN);
    lv_obj_add_event_cb(s_emote_lean_right_btn, emote_lean_right_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lean_right_lbl = lv_label_create(s_emote_lean_right_btn);
    lv_label_set_text(lean_right_lbl, "Lean right");
    lv_obj_set_style_text_color(lean_right_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lean_right_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(lean_right_lbl);

    s_emote_dizzy_left_btn = lv_button_create(s_emotes_panel);
    lv_obj_set_size(s_emote_dizzy_left_btn, 268, 48);
    lv_obj_align(s_emote_dizzy_left_btn, LV_ALIGN_CENTER, 0, -46);
    lv_obj_set_style_bg_color(s_emote_dizzy_left_btn, lv_color_hex(0x333344), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emote_dizzy_left_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_emote_dizzy_left_btn, 24, LV_PART_MAIN);
    lv_obj_add_event_cb(s_emote_dizzy_left_btn, emote_dizzy_left_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *dizzy_left_lbl = lv_label_create(s_emote_dizzy_left_btn);
    lv_label_set_text(dizzy_left_lbl, "Dizzy left");
    lv_obj_set_style_text_color(dizzy_left_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(dizzy_left_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(dizzy_left_lbl);

    s_emote_dizzy_right_btn = lv_button_create(s_emotes_panel);
    lv_obj_set_size(s_emote_dizzy_right_btn, 268, 48);
    lv_obj_align(s_emote_dizzy_right_btn, LV_ALIGN_CENTER, 0, 6);
    lv_obj_set_style_bg_color(s_emote_dizzy_right_btn, lv_color_hex(0x333344), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emote_dizzy_right_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_emote_dizzy_right_btn, 24, LV_PART_MAIN);
    lv_obj_add_event_cb(s_emote_dizzy_right_btn, emote_dizzy_right_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *dizzy_right_lbl = lv_label_create(s_emote_dizzy_right_btn);
    lv_label_set_text(dizzy_right_lbl, "Dizzy right");
    lv_obj_set_style_text_color(dizzy_right_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(dizzy_right_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(dizzy_right_lbl);

    s_emote_dance_btn = lv_button_create(s_emotes_panel);
    lv_obj_set_size(s_emote_dance_btn, 268, 48);
    lv_obj_align(s_emote_dance_btn, LV_ALIGN_CENTER, 0, 58);
    lv_obj_set_style_bg_color(s_emote_dance_btn, lv_color_hex(0x333344), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emote_dance_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_emote_dance_btn, 24, LV_PART_MAIN);
    lv_obj_add_event_cb(s_emote_dance_btn, emote_dance_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *dance_lbl = lv_label_create(s_emote_dance_btn);
    lv_label_set_text(dance_lbl, "Dance");
    lv_obj_set_style_text_color(dance_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(dance_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(dance_lbl);

    s_emote_walk_btn = lv_button_create(s_emotes_panel);
    lv_obj_set_size(s_emote_walk_btn, 268, 48);
    lv_obj_align(s_emote_walk_btn, LV_ALIGN_CENTER, 0, 110);
    lv_obj_set_style_bg_color(s_emote_walk_btn, lv_color_hex(0x333344), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emote_walk_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_emote_walk_btn, 24, LV_PART_MAIN);
    lv_obj_add_event_cb(s_emote_walk_btn, emote_walk_forward_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *walk_lbl = lv_label_create(s_emote_walk_btn);
    lv_label_set_text(walk_lbl, "Walk forward");
    lv_obj_set_style_text_color(walk_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(walk_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(walk_lbl);

    s_emote_walk_back_btn = lv_button_create(s_emotes_panel);
    lv_obj_set_size(s_emote_walk_back_btn, 268, 48);
    lv_obj_align(s_emote_walk_back_btn, LV_ALIGN_CENTER, 0, 162);
    lv_obj_set_style_bg_color(s_emote_walk_back_btn, lv_color_hex(0x333344), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emote_walk_back_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_emote_walk_back_btn, 24, LV_PART_MAIN);
    lv_obj_add_event_cb(s_emote_walk_back_btn, emote_walk_backward_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *walk_back_lbl = lv_label_create(s_emote_walk_back_btn);
    lv_label_set_text(walk_back_lbl, "Walk backward");
    lv_obj_set_style_text_color(walk_back_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(walk_back_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(walk_back_lbl);

    s_emotes_done_btn = lv_button_create(s_emotes_panel);
    lv_obj_set_size(s_emotes_done_btn, 170, 56);
    lv_obj_align(s_emotes_done_btn, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_set_style_bg_color(s_emotes_done_btn, lv_color_hex(0xFFDD00), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_emotes_done_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_emotes_done_btn, 28, LV_PART_MAIN);
    lv_obj_add_event_cb(s_emotes_done_btn, emotes_done_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *emotes_done_lbl = lv_label_create(s_emotes_done_btn);
    lv_label_set_text(emotes_done_lbl, "Done");
    lv_obj_set_style_text_color(emotes_done_lbl, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(emotes_done_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(emotes_done_lbl);

    s_clock_panel = lv_obj_create(screen);
    lv_obj_set_size(s_clock_panel, 466, 466);
    lv_obj_set_pos(s_clock_panel, 0, 0);
    lv_obj_set_style_radius(s_clock_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_clock_panel, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_clock_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_clock_panel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_clock_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_clock_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_clock_panel, LV_OBJ_FLAG_HIDDEN);

    s_clock_time_label = lv_label_create(s_clock_panel);
    lv_label_set_text(s_clock_time_label, "--:--");
    lv_obj_set_style_text_color(s_clock_time_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_clock_time_label, &lv_font_clock_110, LV_PART_MAIN);
    lv_obj_align(s_clock_time_label, LV_ALIGN_CENTER, 0, -44);

    s_clock_ampm_label = lv_label_create(s_clock_panel);
    lv_label_set_text(s_clock_ampm_label, "");
    lv_obj_set_style_text_color(s_clock_ampm_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_clock_ampm_label, &lv_font_clock_110, LV_PART_MAIN);
    lv_obj_align_to(s_clock_ampm_label, s_clock_time_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    s_clock_hint_label = lv_label_create(s_clock_panel);
    lv_label_set_text(s_clock_hint_label, "Tap time to set");
    lv_obj_set_style_text_color(s_clock_hint_label, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_clock_hint_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(s_clock_hint_label, LV_ALIGN_BOTTOM_MID, 0, -36);

    lv_obj_add_flag(s_clock_time_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_clock_time_label, clock_open_picker_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_clock_ampm_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_clock_ampm_label, clock_open_picker_cb, LV_EVENT_CLICKED, NULL);

    s_clock_picker_panel = lv_obj_create(s_clock_panel);
    lv_obj_set_size(s_clock_picker_panel, 430, 306);
    lv_obj_align(s_clock_picker_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(s_clock_picker_panel, 20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_clock_picker_panel, lv_color_hex(0x1C1C1C), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_clock_picker_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_clock_picker_panel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_clock_picker_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_clock_picker_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_clock_picker_panel, LV_OBJ_FLAG_HIDDEN);

    s_clock_picker_hour = lv_roller_create(s_clock_picker_panel);
    lv_obj_set_size(s_clock_picker_hour, 112, 212);
    lv_obj_align(s_clock_picker_hour, LV_ALIGN_LEFT_MID, 12, -20);
    lv_roller_set_options(s_clock_picker_hour,
        "01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12",
        LV_ROLLER_MODE_INFINITE);
    lv_obj_set_style_text_color(s_clock_picker_hour, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_clock_picker_hour, lv_color_hex(0xFFDD00), LV_PART_SELECTED);
    lv_obj_set_style_text_font(s_clock_picker_hour, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_clock_picker_hour, &lv_font_montserrat_32, LV_PART_SELECTED);
    lv_obj_add_event_cb(s_clock_picker_hour, clock_picker_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_clock_picker_minute = lv_roller_create(s_clock_picker_panel);
    lv_obj_set_size(s_clock_picker_minute, 128, 212);
    lv_obj_align(s_clock_picker_minute, LV_ALIGN_CENTER, 0, -20);
    lv_roller_set_options(s_clock_picker_minute,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59",
        LV_ROLLER_MODE_INFINITE);
    lv_obj_set_style_text_color(s_clock_picker_minute, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_clock_picker_minute, lv_color_hex(0xFFDD00), LV_PART_SELECTED);
    lv_obj_set_style_text_font(s_clock_picker_minute, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_clock_picker_minute, &lv_font_montserrat_32, LV_PART_SELECTED);
    lv_obj_add_event_cb(s_clock_picker_minute, clock_picker_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_clock_picker_ampm = lv_roller_create(s_clock_picker_panel);
    lv_obj_set_size(s_clock_picker_ampm, 96, 212);
    lv_obj_align(s_clock_picker_ampm, LV_ALIGN_RIGHT_MID, -12, -20);
    lv_roller_set_options(s_clock_picker_ampm, "AM\nPM", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_style_text_color(s_clock_picker_ampm, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_clock_picker_ampm, lv_color_hex(0xFFDD00), LV_PART_SELECTED);
    lv_obj_set_style_text_font(s_clock_picker_ampm, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_clock_picker_ampm, &lv_font_montserrat_32, LV_PART_SELECTED);
    lv_obj_add_event_cb(s_clock_picker_ampm, clock_picker_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_clock_picker_set_btn = lv_button_create(s_clock_picker_panel);
    lv_obj_set_size(s_clock_picker_set_btn, 220, 76);
    lv_obj_align(s_clock_picker_set_btn, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_bg_color(s_clock_picker_set_btn, lv_color_hex(0xFFDD00), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_clock_picker_set_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_clock_picker_set_btn, 38, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_clock_picker_set_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_clock_picker_set_btn, clock_picker_set_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clock_set_lbl = lv_label_create(s_clock_picker_set_btn);
    lv_label_set_text(clock_set_lbl, "Set");
    lv_obj_set_style_text_color(clock_set_lbl, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(clock_set_lbl, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_center(clock_set_lbl);

    update_clock_labels();

    s_alarm_panel = lv_obj_create(screen);
    lv_obj_set_size(s_alarm_panel, 466, 466);
    lv_obj_set_pos(s_alarm_panel, 0, 0);
    lv_obj_set_style_radius(s_alarm_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_alarm_panel, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_alarm_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_alarm_panel, 0, LV_PART_MAIN);
    lv_obj_add_flag(s_alarm_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_alarm_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_alarm_panel, LV_SCROLLBAR_MODE_OFF);

    s_alarm_time_chip = lv_obj_create(s_alarm_panel);
    lv_obj_set_size(s_alarm_time_chip, 400, 112);
    lv_obj_align(s_alarm_time_chip, LV_ALIGN_CENTER, 0, -22);
    lv_obj_set_style_radius(s_alarm_time_chip, 30, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_alarm_time_chip, lv_color_hex(0x8A8FA0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_alarm_time_chip, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_alarm_time_chip, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_alarm_time_chip, 8, LV_PART_MAIN);
    lv_obj_remove_flag(s_alarm_time_chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_alarm_time_chip, LV_SCROLLBAR_MODE_OFF);

    s_alarm_hh_btn = lv_button_create(s_alarm_time_chip);
    lv_obj_set_size(s_alarm_hh_btn, 102, 88);
    lv_obj_align(s_alarm_hh_btn, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_bg_opa(s_alarm_hh_btn, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_alarm_hh_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_alarm_hh_btn, alarm_time_segment_tap_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ALARM_FIELD_HOURS);
    s_alarm_hh_label = lv_label_create(s_alarm_hh_btn);
    lv_obj_set_style_text_color(s_alarm_hh_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_alarm_hh_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_center(s_alarm_hh_label);

    lv_obj_t *colon_1 = lv_label_create(s_alarm_time_chip);
    lv_label_set_text(colon_1, ":");
    lv_obj_set_style_text_color(colon_1, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(colon_1, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_align(colon_1, LV_ALIGN_LEFT_MID, 114, 0);

    s_alarm_mm_btn = lv_button_create(s_alarm_time_chip);
    lv_obj_set_size(s_alarm_mm_btn, 102, 88);
    lv_obj_align(s_alarm_mm_btn, LV_ALIGN_LEFT_MID, 132, 0);
    lv_obj_set_style_bg_opa(s_alarm_mm_btn, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_alarm_mm_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_alarm_mm_btn, alarm_time_segment_tap_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ALARM_FIELD_MINUTES);
    s_alarm_mm_label = lv_label_create(s_alarm_mm_btn);
    lv_obj_set_style_text_color(s_alarm_mm_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_alarm_mm_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_center(s_alarm_mm_label);

    lv_obj_t *colon_2 = lv_label_create(s_alarm_time_chip);
    lv_label_set_text(colon_2, ":");
    lv_obj_set_style_text_color(colon_2, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(colon_2, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_align(colon_2, LV_ALIGN_LEFT_MID, 238, 0);

    s_alarm_ss_btn = lv_button_create(s_alarm_time_chip);
    lv_obj_set_size(s_alarm_ss_btn, 102, 88);
    lv_obj_align(s_alarm_ss_btn, LV_ALIGN_LEFT_MID, 256, 0);
    lv_obj_set_style_bg_opa(s_alarm_ss_btn, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_alarm_ss_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_alarm_ss_btn, alarm_time_segment_tap_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ALARM_FIELD_SECONDS);
    s_alarm_ss_label = lv_label_create(s_alarm_ss_btn);
    lv_obj_set_style_text_color(s_alarm_ss_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_alarm_ss_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_center(s_alarm_ss_label);

    s_alarm_action_btn = lv_button_create(s_alarm_panel);
    lv_obj_set_size(s_alarm_action_btn, 220, 76);
    lv_obj_align(s_alarm_action_btn, LV_ALIGN_CENTER, 0, 76);
    lv_obj_set_style_radius(s_alarm_action_btn, 38, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_alarm_action_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_alarm_action_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_alarm_action_btn, alarm_action_btn_cb, LV_EVENT_CLICKED, NULL);
    s_alarm_action_label = lv_label_create(s_alarm_action_btn);
    lv_obj_set_style_text_color(s_alarm_action_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_alarm_action_label, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_center(s_alarm_action_label);

    s_alarm_picker_panel = lv_obj_create(s_alarm_panel);
    lv_obj_set_size(s_alarm_picker_panel, 430, 306);
    lv_obj_align(s_alarm_picker_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(s_alarm_picker_panel, 20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_alarm_picker_panel, lv_color_hex(0x1C1C1C), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_alarm_picker_panel, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_alarm_picker_panel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_alarm_picker_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_alarm_picker_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_alarm_picker_panel, LV_OBJ_FLAG_HIDDEN);

    s_alarm_picker_hours = lv_roller_create(s_alarm_picker_panel);
    lv_obj_set_size(s_alarm_picker_hours, 128, 212);
    lv_obj_align(s_alarm_picker_hours, LV_ALIGN_LEFT_MID, 12, -20);
    lv_roller_set_options(s_alarm_picker_hours,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23",
        LV_ROLLER_MODE_INFINITE);
    lv_obj_set_style_text_color(s_alarm_picker_hours, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_alarm_picker_hours, lv_color_hex(0xFFDD00), LV_PART_SELECTED);
    lv_obj_set_style_text_font(s_alarm_picker_hours, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_alarm_picker_hours, &lv_font_montserrat_32, LV_PART_SELECTED);
    lv_obj_add_event_cb(s_alarm_picker_hours, alarm_picker_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_alarm_picker_minutes = lv_roller_create(s_alarm_picker_panel);
    lv_obj_set_size(s_alarm_picker_minutes, 128, 212);
    lv_obj_align(s_alarm_picker_minutes, LV_ALIGN_CENTER, 0, -20);
    lv_roller_set_options(s_alarm_picker_minutes,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59",
        LV_ROLLER_MODE_INFINITE);
    lv_obj_set_style_text_color(s_alarm_picker_minutes, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_alarm_picker_minutes, lv_color_hex(0xFFDD00), LV_PART_SELECTED);
    lv_obj_set_style_text_font(s_alarm_picker_minutes, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_alarm_picker_minutes, &lv_font_montserrat_32, LV_PART_SELECTED);
    lv_obj_add_event_cb(s_alarm_picker_minutes, alarm_picker_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_alarm_picker_seconds = lv_roller_create(s_alarm_picker_panel);
    lv_obj_set_size(s_alarm_picker_seconds, 128, 212);
    lv_obj_align(s_alarm_picker_seconds, LV_ALIGN_RIGHT_MID, -12, -20);
    lv_roller_set_options(s_alarm_picker_seconds,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59",
        LV_ROLLER_MODE_INFINITE);
    lv_obj_set_style_text_color(s_alarm_picker_seconds, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_alarm_picker_seconds, lv_color_hex(0xFFDD00), LV_PART_SELECTED);
    lv_obj_set_style_text_font(s_alarm_picker_seconds, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_alarm_picker_seconds, &lv_font_montserrat_32, LV_PART_SELECTED);
    lv_obj_add_event_cb(s_alarm_picker_seconds, alarm_picker_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_alarm_picker_set_btn = lv_button_create(s_alarm_picker_panel);
    lv_obj_set_size(s_alarm_picker_set_btn, 220, 76);
    lv_obj_align(s_alarm_picker_set_btn, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_bg_color(s_alarm_picker_set_btn, lv_color_hex(0xFFDD00), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_alarm_picker_set_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_alarm_picker_set_btn, 38, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_alarm_picker_set_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_alarm_picker_set_btn, alarm_picker_set_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *picker_set_lbl = lv_label_create(s_alarm_picker_set_btn);
    lv_label_set_text(picker_set_lbl, "Set");
    lv_obj_set_style_text_color(picker_set_lbl, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(picker_set_lbl, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_center(picker_set_lbl);

    update_alarm_time_labels();
    sync_picker_with_time_values();
    style_alarm_picker_focus();
    update_alarm_action_label();

    /* Status-bar battery (top center of screen; outline + fill bar + nub). */
    s_battery_root = lv_obj_create(screen);
    lv_obj_set_size(s_battery_root, 52, 16);
    lv_obj_set_style_bg_opa(s_battery_root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_battery_root, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_battery_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_battery_root, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *battery_shell = lv_obj_create(s_battery_root);
    lv_obj_set_size(battery_shell, 40, 12);
    lv_obj_align(battery_shell, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_bg_opa(battery_shell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(battery_shell, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(battery_shell, lv_color_hex(0xBBBBBB), LV_PART_MAIN);
    lv_obj_set_style_border_opa(battery_shell, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(battery_shell, 3, LV_PART_MAIN);
    lv_obj_remove_flag(battery_shell, LV_OBJ_FLAG_SCROLLABLE);

    s_battery_bar = lv_bar_create(battery_shell);
    lv_bar_set_range(s_battery_bar, 0, 100);
    lv_bar_set_value(s_battery_bar, 0, LV_ANIM_OFF);
    lv_obj_set_size(s_battery_bar, 30, 6);
    lv_obj_center(s_battery_bar);
    lv_obj_set_style_radius(s_battery_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_battery_bar, lv_color_hex(0x181818), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_battery_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_battery_bar, 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_battery_bar, lv_color_hex(0x444444), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_battery_bar, LV_OPA_COVER, LV_PART_INDICATOR);

    lv_obj_t *battery_nub = lv_obj_create(s_battery_root);
    lv_obj_set_size(battery_nub, 3, 8);
    lv_obj_align(battery_nub, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(battery_nub, lv_color_hex(0xBBBBBB), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(battery_nub, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(battery_nub, 1, LV_PART_MAIN);
    lv_obj_set_style_border_width(battery_nub, 0, LV_PART_MAIN);
    lv_obj_remove_flag(battery_nub, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_move_foreground(s_battery_root);

    s_next_battery_poll_ms = 0;
    update_battery_ui();

    // Shared 16 kHz I2S for speaker (alarm beep) and microphone (wake word).
    if (bsp_audio_init(&s_i2s_16k_cfg) == ESP_OK) {
        esp_codec_dev_sample_info_t fs = {
            .sample_rate = 16000,
            .bits_per_sample = 16,
            .channel = 1,
        };

        s_speaker = bsp_audio_codec_speaker_init();
        if (s_speaker != NULL && esp_codec_dev_open(s_speaker, &fs) == ESP_CODEC_DEV_OK) {
            speaker_set_volume(s_speaker_volume);
            pixel_voice_set_volume(s_speaker_volume);
            if (pixel_voice_init(s_speaker) != ESP_OK) {
                ESP_LOGW(TAG, "pixel_voice init failed");
            }
            robot_sound_set_volume(s_speaker_volume);
            s_audio_ready = true;
        }

        s_microphone = bsp_audio_codec_microphone_init();
        if (s_microphone != NULL && esp_codec_dev_open(s_microphone, &fs) == ESP_CODEC_DEV_OK) {
            esp_codec_dev_set_in_gain(s_microphone, 36.0f);
        } else {
            ESP_LOGW(TAG, "Microphone init failed");
        }
    }

    bsp_display_brightness_set(bsp_display_brightness_get());

    uint32_t now = lv_tick_get();
    s_next_look_change_ms = now + rand_range(300, 1000);
    s_next_blink_ms = now + rand_range(1200, 2400);
    s_next_wink_ms = now + rand_range(2800, 5200);

    lv_timer_create(face_idle_timer_cb, FACE_ANIM_PERIOD_MS, NULL);
    lv_timer_create(servo_ramp_timer_cb, SERVO_RAMP_PERIOD_MS, NULL);

    emotion_sync_ui_labels();
    emotion_apply_visibility();
    update_eye_geometry();

    bsp_display_unlock();

    if (s_microphone != NULL) {
        xTaskCreate(speech_wake_boot_task, "wake_boot", 4096, s_microphone, 3, NULL);
    } else {
        face_wake_boot_complete();
    }

    ESP_LOGI(TAG, "Rendered cyan robot eyes with idle animation");
}
