// rtc_time.h - clock. NTP when online (Europe/Berlin with DST), PCF85063 RTC
// keeps the time while offline / powered off. The RTC stores UTC.
#pragma once
#include <Arduino.h>
#include <time.h>

namespace rtc_time {
void init();                       // loads time from the RTC chip
void update();                     // call about once a second (handles NTP sync -> RTC)
void startNtp();                   // call when Wi-Fi is up
bool valid();                      // clock has a plausible time
bool localTime(struct tm& out);    // local time (DST applied)
bool rtcPresent();
}  // namespace rtc_time
