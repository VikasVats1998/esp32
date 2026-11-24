idf_component_register(SRCS "lv_font_clock_110.c" "robot_sound.c" "pixel_voice.c" "pca9685.c" "main.c" "battery_pmu.cpp" "speech_wake.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash waveshare__esp32_s3_touch_amoled_1_75 cube32esp__xpowerslib espressif__esp-sr)
