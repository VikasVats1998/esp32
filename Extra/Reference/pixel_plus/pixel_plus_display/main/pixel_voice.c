#include "pixel_voice.h"

/* Short chirps share I2S with the wake-word mic; pause AFE during playback if false triggers occur. */
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "pixel_voice";

#define PIXEL_VOICE_SAMPLE_RATE 16000
#define PIXEL_VOICE_PCM_MAX     12288
/** Trailing silence so the codec does not pop when playback stops. */
#define PIXEL_VOICE_TAIL_MS     48
#define PIXEL_VOICE_QUEUE_LEN   4
#define PIXEL_VOICE_MIN_GAP_MS  80
/** Sine peak scale before codec volume (110% of prior level). */
#define PIXEL_VOICE_AMP_BASE    12100
#define PIXEL_VOICE_AMP_MAX     13200
#define PIXEL_VOICE_TASK_STACK  4096
#define PIXEL_VOICE_TASK_PRIO   4

typedef enum {
    VOICE_MSG_CHIRP = 0,
    VOICE_MSG_ROBOT,
} voice_msg_kind_t;

typedef struct {
    voice_msg_kind_t kind;
    union {
        pixel_chirp_t chirp;
        RobotSoundID robot;
    } u;
} voice_msg_t;

typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
    int16_t glide_hz_per_ms;
    uint8_t amp_scale_pct;
} pixel_note_t;

typedef struct {
    const pixel_note_t *notes;
    uint8_t note_count;
} pixel_phrase_t;

static esp_codec_dev_handle_t s_speaker;
static QueueHandle_t s_queue;
static TaskHandle_t s_task;
static int s_volume_pct = 100;
static volatile bool s_busy;
static uint32_t s_last_done_ms;
static bool s_ready;

static int16_t *s_pcm;

#define NOTE_END { 0, 0, 0, 0 }

#define PHRASE(name, ...) \
    static const pixel_note_t name##_notes[] = { __VA_ARGS__, NOTE_END }; \
    static const pixel_phrase_t name = { name##_notes, \
        (uint8_t)(sizeof(name##_notes) / sizeof(name##_notes[0]) - 1) }

/* Wake word: legato "hello" — A4→C#5→E5 (see render legato). */
PHRASE(phrase_listen,
       { 440, 95, 0, 72 }, { 554, 85, 0, 78 }, { 659, 125, 0, 85 });

/* Boot ready: legato "wake up" — G4→B4→D5→G5. */
PHRASE(phrase_wake_up,
       { 392, 72, 0, 70 }, { 494, 68, 0, 75 }, { 587, 64, 0, 80 }, { 784, 115, 0, 86 });

/* Voice commands: legato melodic "words" (freq_hz, duration_ms, glide, amp%). */
PHRASE(phrase_robot_time,
       { 523, 90, 0, 76 }, { 659, 90, 0, 80 }, { 784, 115, 0, 85 });
PHRASE(phrase_robot_start_timer,
       { 440, 52, 0, 74 }, { 554, 52, 0, 78 }, { 659, 52, 0, 82 }, { 880, 95, 0, 86 });
PHRASE(phrase_robot_stop_timer,
       { 440, 95, 0, 80 }, { 330, 140, 0, 76 });
PHRASE(phrase_robot_dance,
       { 600, 68, 0, 78 }, { 850, 52, 0, 82 }, { 700, 68, 0, 78 }, { 950, 88, 0, 85 });
PHRASE(phrase_robot_dizzy,
       { 880, 42, 0, 80 }, { 740, 42, 0, 78 }, { 600, 58, 0, 76 }, { 440, 115, 0, 74 });
PHRASE(phrase_robot_walk_fwd,
       { 600, 72, 0, 78 }, { 750, 88, 0, 84 });
PHRASE(phrase_robot_walk_back,
       { 480, 78, 0, 74 }, { 480, 78, 0, 72 });
PHRASE(phrase_robot_turn_left,
       { 800, 62, 0, 80 }, { 600, 95, 0, 76 });
PHRASE(phrase_robot_turn_right,
       { 600, 62, 0, 76 }, { 800, 95, 0, 82 });
PHRASE(phrase_robot_march,
       { 350, 52, 0, 72 }, { 350, 52, 0, 74 }, { 350, 52, 0, 72 });
PHRASE(phrase_robot_jump,
       { 300, 26, 0, 70 }, { 500, 26, 0, 76 }, { 900, 36, 0, 84 }, { 988, 85, 0, 88 });
PHRASE(phrase_robot_sneak,
       { 220, 38, 0, 48 }, { 240, 38, 0, 50 }, { 220, 38, 0, 48 });
PHRASE(phrase_robot_get_back,
       { 988, 30, 0, 82 }, { 820, 30, 0, 80 }, { 988, 30, 0, 82 }, { 820, 38, 0, 78 });
PHRASE(phrase_robot_spin,
       { 500, 36, 0, 74 }, { 650, 36, 0, 78 }, { 800, 36, 0, 82 }, { 1000, 36, 0, 86 },
       { 880, 52, 0, 88 });
PHRASE(phrase_robot_patrol,
       { 550, 78, 0, 76 }, { 750, 62, 0, 80 }, { 620, 95, 0, 78 });
PHRASE(phrase_robot_low_power,
       { 440, 88, 0, 72 }, { 370, 95, 0, 68 }, { 294, 130, 0, 64 });

/* During-motion legato motifs (quieter than command fanfares). */
PHRASE(phrase_motion_step,
       { 600, 48, 0, 52 }, { 750, 58, 0, 56 });
PHRASE(phrase_motion_step_alt,
       { 659, 48, 0, 54 }, { 784, 58, 0, 58 });
PHRASE(phrase_motion_step_quiet,
       { 220, 42, 0, 38 }, { 240, 42, 0, 40 });
PHRASE(phrase_motion_step_fast,
       { 700, 40, 0, 58 }, { 880, 48, 0, 62 });
PHRASE(phrase_motion_step_march,
       { 350, 48, 0, 50 }, { 350, 48, 0, 52 });
PHRASE(phrase_motion_turn,
       { 700, 45, 0, 54 }, { 520, 58, 0, 50 });
PHRASE(phrase_motion_spin,
       { 500, 38, 0, 52 }, { 720, 48, 0, 58 });
PHRASE(phrase_motion_dance,
       { 600, 42, 0, 54 }, { 850, 38, 0, 58 });
PHRASE(phrase_motion_dizzy,
       { 740, 36, 0, 52 }, { 600, 48, 0, 50 });
PHRASE(phrase_motion_land,
       { 440, 42, 0, 56 }, { 330, 62, 0, 52 });

/* Classic two-tone alarm: A5↔B5 trill then urgent rise to C6. */
PHRASE(phrase_alarm,
       { 880, 140, 0, 88 }, { 988, 140, 0, 92 }, { 880, 140, 0, 88 }, { 988, 140, 0, 92 },
       { 880, 120, 0, 86 }, { 988, 120, 0, 90 }, { 1047, 200, 0, 94 });
PHRASE(phrase_alarm_repeat,
       { 988, 90, 0, 90 }, { 880, 90, 0, 88 }, { 988, 110, 0, 92 });

static const pixel_phrase_t *phrase_for_chirp(pixel_chirp_t chirp)
{
    switch (chirp) {
    case PIXEL_CHIRP_LISTEN:
        return &phrase_listen;
    case PIXEL_CHIRP_WAKE_UP:
        return &phrase_wake_up;
    case PIXEL_CHIRP_MOTION_STEP:
        return &phrase_motion_step;
    case PIXEL_CHIRP_MOTION_STEP_ALT:
        return &phrase_motion_step_alt;
    case PIXEL_CHIRP_MOTION_STEP_QUIET:
        return &phrase_motion_step_quiet;
    case PIXEL_CHIRP_MOTION_STEP_FAST:
        return &phrase_motion_step_fast;
    case PIXEL_CHIRP_MOTION_STEP_MARCH:
        return &phrase_motion_step_march;
    case PIXEL_CHIRP_MOTION_TURN:
        return &phrase_motion_turn;
    case PIXEL_CHIRP_MOTION_SPIN:
        return &phrase_motion_spin;
    case PIXEL_CHIRP_MOTION_DANCE:
        return &phrase_motion_dance;
    case PIXEL_CHIRP_MOTION_DIZZY:
        return &phrase_motion_dizzy;
    case PIXEL_CHIRP_MOTION_LAND:
        return &phrase_motion_land;
    case PIXEL_CHIRP_ALARM:
        return &phrase_alarm;
    case PIXEL_CHIRP_ALARM_REPEAT:
        return &phrase_alarm_repeat;
    default:
        return NULL;
    }
}

static const pixel_phrase_t *phrase_for_robot(RobotSoundID sound_id)
{
    switch (sound_id) {
    case ROBOT_SOUND_WAKE_JARVIS:
        return &phrase_listen;
    case ROBOT_SOUND_TIME_CMD:
        return &phrase_robot_time;
    case ROBOT_SOUND_START_TIMER:
        return &phrase_robot_start_timer;
    case ROBOT_SOUND_STOP_TIMER:
        return &phrase_robot_stop_timer;
    case ROBOT_SOUND_DO_DANCE:
        return &phrase_robot_dance;
    case ROBOT_SOUND_ARE_YOU_DIZZY:
        return &phrase_robot_dizzy;
    case ROBOT_SOUND_WALK_FORWARD:
        return &phrase_robot_walk_fwd;
    case ROBOT_SOUND_WALK_BACKWARD:
        return &phrase_robot_walk_back;
    case ROBOT_SOUND_TURN_LEFT:
        return &phrase_robot_turn_left;
    case ROBOT_SOUND_TURN_RIGHT:
        return &phrase_robot_turn_right;
    case ROBOT_SOUND_MARCH:
        return &phrase_robot_march;
    case ROBOT_SOUND_DO_JUMP:
        return &phrase_robot_jump;
    case ROBOT_SOUND_SNEAK_AROUND:
        return &phrase_robot_sneak;
    case ROBOT_SOUND_GET_BACK:
        return &phrase_robot_get_back;
    case ROBOT_SOUND_DO_SPIN:
        return &phrase_robot_spin;
    case ROBOT_SOUND_PATROL:
        return &phrase_robot_patrol;
    case ROBOT_SOUND_LOW_POWER:
        return &phrase_robot_low_power;
    default:
        return NULL;
    }
}

static bool chirp_is_motion(pixel_chirp_t chirp)
{
    return chirp >= PIXEL_CHIRP_MOTION_STEP && chirp <= PIXEL_CHIRP_MOTION_LAND;
}

static bool chirp_is_low_priority(pixel_chirp_t chirp)
{
    return chirp_is_motion(chirp);
}

static int32_t note_amplitude(const pixel_note_t *note)
{
    int32_t amp = (PIXEL_VOICE_AMP_BASE * (int32_t)s_volume_pct * (int32_t)note->amp_scale_pct) / 10000;
    if (amp > PIXEL_VOICE_AMP_MAX) {
        amp = PIXEL_VOICE_AMP_MAX;
    }
    return amp;
}

static size_t render_phrase(const pixel_phrase_t *phrase, int16_t *out, size_t out_max)
{
    size_t out_idx = 0;
    const float two_pi = 6.283185307f;
    float phase = 0.0f;

    for (uint8_t n = 0; n < phrase->note_count && phrase->notes[n].duration_ms > 0; n++) {
        const pixel_note_t *note = &phrase->notes[n];
        const int samples = (int)note->duration_ms * PIXEL_VOICE_SAMPLE_RATE / 1000;

        if (note->freq_hz == 0) {
            for (int i = 0; i < samples && out_idx < out_max; i++, out_idx++) {
                out[out_idx] = 0;
            }
            continue;
        }

        const bool is_first = (n == 0);
        const bool is_last = (n + 1 >= phrase->note_count);
        const int max_attack = PIXEL_VOICE_SAMPLE_RATE * 18 / 1000;
        const int max_decay = PIXEL_VOICE_SAMPLE_RATE * 45 / 1000;
        int attack_samples = 0;
        int decay_samples = 0;

        /* Legato sine: one fade-in at start, one fade-out at end — no clicks between notes. */
        if (is_first) {
            attack_samples = samples / 4;
            if (attack_samples > max_attack) {
                attack_samples = max_attack;
            }
        }
        if (is_last) {
            decay_samples = samples / 3;
            if (decay_samples > max_decay) {
                decay_samples = max_decay;
            }
        }

        const int32_t peak_amp = note_amplitude(note);
        float freq = (float)note->freq_hz;
        const float glide = (float)note->glide_hz_per_ms / 1000.0f;

        for (int i = 0; i < samples && out_idx < out_max; i++, out_idx++) {
            int32_t env = peak_amp;
            if (attack_samples > 0 && i < attack_samples) {
                env = (peak_amp * i) / attack_samples;
            } else if (decay_samples > 0 && i >= samples - decay_samples) {
                const int decay_i = i - (samples - decay_samples);
                env = (peak_amp * (decay_samples - decay_i)) / decay_samples;
            }

            phase += two_pi * freq / (float)PIXEL_VOICE_SAMPLE_RATE;
            if (phase > two_pi) {
                phase -= two_pi;
            }
            const int32_t sample = (int32_t)(sinf(phase) * (float)env);
            out[out_idx] = (int16_t)sample;
            freq += glide * (float)PIXEL_VOICE_SAMPLE_RATE;
        }
    }

    return out_idx;
}

static void speaker_write_pcm(const int16_t *samples, size_t count)
{
    if (s_speaker == NULL || samples == NULL || count == 0) {
        return;
    }

    const size_t chunk_samples = 512;
    size_t offset = 0;

    while (offset < count) {
        const size_t n = (count - offset > chunk_samples) ? chunk_samples : (count - offset);
        esp_codec_dev_write(s_speaker, (void *)&samples[offset], (int)(n * sizeof(int16_t)));
        offset += n;
    }
}

static void speaker_write_tail_silence(void)
{
    if (s_speaker == NULL || s_pcm == NULL) {
        return;
    }

    const size_t tail_samples = (size_t)PIXEL_VOICE_TAIL_MS * PIXEL_VOICE_SAMPLE_RATE / 1000;
    if (tail_samples == 0 || tail_samples > PIXEL_VOICE_PCM_MAX) {
        return;
    }

    memset(s_pcm, 0, tail_samples * sizeof(int16_t));
    speaker_write_pcm(s_pcm, tail_samples);
}

static void play_phrase_blocking(const pixel_phrase_t *phrase)
{
    if (phrase == NULL || s_speaker == NULL || s_pcm == NULL) {
        return;
    }

    const size_t samples = render_phrase(phrase, s_pcm, PIXEL_VOICE_PCM_MAX);
    if (samples == 0) {
        return;
    }

    speaker_write_pcm(s_pcm, samples);
    speaker_write_tail_silence();
}

static void play_chirp_blocking(pixel_chirp_t chirp)
{
    play_phrase_blocking(phrase_for_chirp(chirp));
}

static void voice_task(void *arg)
{
    (void)arg;
    voice_msg_t msg;

    for (;;) {
        if (xQueueReceive(s_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        s_busy = true;
        if (msg.kind == VOICE_MSG_ROBOT) {
            play_phrase_blocking(phrase_for_robot(msg.u.robot));
        } else {
            play_chirp_blocking(msg.u.chirp);
        }
        s_busy = false;
        s_last_done_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
}

static bool voice_enqueue(const voice_msg_t *msg, bool allow_drop_low)
{
    if (!s_ready || s_queue == NULL) {
        return false;
    }

    voice_msg_t drop;
    if (xQueueSend(s_queue, msg, 0) != pdTRUE) {
        if (allow_drop_low) {
            return false;
        }
        if (xQueueReceive(s_queue, &drop, 0) == pdTRUE) {
            const bool drop_low = (drop.kind == VOICE_MSG_CHIRP && chirp_is_low_priority(drop.u.chirp));
            if (!drop_low) {
                (void)xQueueSend(s_queue, &drop, 0);
            }
        }
        if (xQueueSend(s_queue, msg, 0) != pdTRUE) {
            return false;
        }
    }
    return true;
}

static bool rate_limited(pixel_chirp_t chirp)
{
    if (chirp == PIXEL_CHIRP_NONE || chirp == PIXEL_CHIRP_LISTEN || chirp == PIXEL_CHIRP_WAKE_UP ||
        chirp_is_motion(chirp)) {
        return false;
    }
    const uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (s_last_done_ms != 0 && (now - s_last_done_ms) < PIXEL_VOICE_MIN_GAP_MS) {
        return true;
    }
    return false;
}

esp_err_t pixel_voice_init(esp_codec_dev_handle_t speaker)
{
    if (speaker == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_pcm == NULL) {
        const size_t bytes = (size_t)PIXEL_VOICE_PCM_MAX * sizeof(int16_t);
        s_pcm = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_pcm == NULL) {
            s_pcm = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }
        if (s_pcm == NULL) {
            ESP_LOGE(TAG, "pcm alloc failed (%u bytes)", (unsigned)bytes);
            return ESP_ERR_NO_MEM;
        }
    }

    s_speaker = speaker;
    s_queue = xQueueCreate(PIXEL_VOICE_QUEUE_LEN, sizeof(voice_msg_t));
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "queue create failed");
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(voice_task, "pixel_voice", PIXEL_VOICE_TASK_STACK, NULL, PIXEL_VOICE_TASK_PRIO,
                    &s_task) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        ESP_LOGE(TAG, "task create failed");
        return ESP_ERR_NO_MEM;
    }

    s_ready = true;
    ESP_LOGI(TAG, "ready (pcm %u bytes)", (unsigned)((size_t)PIXEL_VOICE_PCM_MAX * sizeof(int16_t)));
    return ESP_OK;
}

void pixel_voice_set_volume(int percent)
{
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    s_volume_pct = percent;
}

void pixel_voice_play(pixel_chirp_t chirp)
{
    if (chirp == PIXEL_CHIRP_NONE) {
        return;
    }
    if (rate_limited(chirp)) {
        return;
    }

    const voice_msg_t msg = {
        .kind = VOICE_MSG_CHIRP,
        .u = { .chirp = chirp },
    };
    (void)voice_enqueue(&msg, chirp_is_low_priority(chirp));
}

void pixel_voice_enqueue_robot(RobotSoundID sound_id)
{
    if (sound_id >= ROBOT_SOUND_COUNT) {
        return;
    }

    const voice_msg_t msg = {
        .kind = VOICE_MSG_ROBOT,
        .u = { .robot = sound_id },
    };
    (void)voice_enqueue(&msg, false);
}

bool pixel_voice_busy(void)
{
    return s_busy || uxQueueMessagesWaiting(s_queue) > 0;
}
