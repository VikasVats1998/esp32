// rtc_time.cpp - see rtc_time.h
#include "rtc_time.h"
#include "config.h"
#include "bus.h"
#include "events.h"
#include <sys/time.h>
#include <esp_sntp.h>

namespace rtc_time {

static bool present = false;
static bool ntpStarted = false, ntpDone = false;
static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 15); }
static uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

static void setTz() { setenv("TZ", TIMEZONE_POSIX, 1); tzset(); }

static bool readRtcUtc(struct tm& t) {
  uint8_t r[7];
  if (!bus::readRegs(I2C_ADDR_PCF85063, 0x04, r, 7)) return false;
  if (r[0] & 0x80) return false;                    // oscillator stopped: time invalid
  t = {};
  t.tm_sec = bcd2bin(r[0] & 0x7F);
  t.tm_min = bcd2bin(r[1] & 0x7F);
  t.tm_hour = bcd2bin(r[2] & 0x3F);
  t.tm_mday = bcd2bin(r[3] & 0x3F);
  t.tm_wday = r[4] & 7;
  t.tm_mon = bcd2bin(r[5] & 0x1F) - 1;
  t.tm_year = bcd2bin(r[6]) + 100;
  return true;
}

static void writeRtcUtc() {
  time_t now = time(nullptr);
  struct tm t;
  gmtime_r(&now, &t);
  uint8_t r[7] = {bin2bcd(t.tm_sec), bin2bcd(t.tm_min), bin2bcd(t.tm_hour), bin2bcd(t.tm_mday),
                  (uint8_t)t.tm_wday, bin2bcd(t.tm_mon + 1), bin2bcd(t.tm_year - 100)};
  bus::writeRegs(I2C_ADDR_PCF85063, 0x04, r, 7);
}

void init() {
  setTz();
  present = bus::probe(I2C_ADDR_PCF85063);
  if (!present) return;
  struct tm t;
  if (readRtcUtc(t) && t.tm_year >= 124) {          // year >= 2024
    setenv("TZ", "UTC0", 1); tzset();
    time_t utc = mktime(&t);
    setTz();
    struct timeval tv = {utc, 0};
    settimeofday(&tv, nullptr);
  }
}

bool rtcPresent() { return present; }

bool valid() { return time(nullptr) > 1704067200; }   // after 2024-01-01

bool localTime(struct tm& out) {
  time_t now = time(nullptr);
  localtime_r(&now, &out);
  return valid();
}

void startNtp() {
  if (ntpStarted) return;
  ntpStarted = true;
  configTzTime(TIMEZONE_POSIX, NTP_SERVER, "time.google.com");
}

void update() {
  // After NTP succeeds the first time, copy the time into the RTC chip.
  if (ntpStarted && !ntpDone && valid()) {
    struct tm t;
    if (getLocalTime(&t, 10) && sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      ntpDone = true;
      if (present) writeRtcUtc();
      events::publish(events::EV_TIME_SYNCED);
    }
  }
}

}  // namespace rtc_time
