// wake.cpp - see wake.h
#include "wake.h"
#include "config.h"
#include "logbuf.h"
#include "events.h"
#include "audio.h"
#include "mic.h"
#include "storage.h"
#include <ESP_SR.h>
#include <esp_partition.h>

namespace wake {

static bool up = false;
static const char* err = "not started";

// Generated with: python3 gen_sr_commands.py "Turn on the light,Switch on the light;Turn off the light,Switch off the light;
//   Start fan;Stop fan;Go to sleep;Be happy;Dance;Brighter;Dimmer"   (see ESP_SR/tools)
static const sr_cmd_t CMDS[] = {
  {C_LIGHT_ON, "Turn on the light", "TkN nN jc LiT"},
  {C_LIGHT_ON, "Switch on the light", "SWgp nN jc LiT"},
  {C_LIGHT_OFF, "Turn off the light", "TkN eF jc LiT"},
  {C_LIGHT_OFF, "Switch off the light", "SWgp eF jc LiT"},
  {C_FAN_ON, "Start fan", "STnRT FaN"},
  {C_FAN_OFF, "Stop fan", "STnP FaN"},
  {C_SLEEP, "Go to sleep", "Gb To SLmP"},
  {C_HAPPY, "Be happy", "Bm haPm"},
  {C_DANCE, "Dance", "DaNS"},
  {C_BRIGHTER, "Brighter", "BRiTk"},
  {C_DIMMER, "Dimmer", "DgMk"},
};

static void onSr(sr_event_t event, int commandId, int phraseId) {
  switch (event) {
    case SR_EVENT_WAKEWORD:
      LOGI("wake", "wake word heard");
      events::publish(events::EV_WAKEWORD, 0);
      break;
    case SR_EVENT_WAKEWORD_CHANNEL:
      ESP_SR.setMode(SR_MODE_COMMAND);                // now listen for one command
      break;
    case SR_EVENT_COMMAND:
      LOGI("wake", "command %d (%s)", commandId, phraseId >= 0 && phraseId < (int)(sizeof(CMDS) / sizeof(CMDS[0])) ? CMDS[phraseId].str : "?");
      events::publish(events::EV_VOICE, commandId);
      ESP_SR.setMode(SR_MODE_WAKEWORD);
      break;
    case SR_EVENT_TIMEOUT:
      events::publish(events::EV_VOICE_END);
      ESP_SR.setMode(SR_MODE_WAKEWORD);
      break;
    default: break;
  }
}

bool running() { return up; }
const char* error() { return err; }

void init() {
  if (!storage::get().wakeWord) { err = "disabled in settings"; return; }
  if (!audio::micOnline()) { err = "no microphone"; return; }
  if (!esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "model")) {
    err = "no 'model' partition (flash srmodels.bin, see docs/WAKEWORD.md)";
    LOGW("wake", "%s", err);
    return;
  }
  mic::useExternalReader(true);                       // the engine owns the mic stream from now on
  ESP_SR.onEvent(onSr);
  up = ESP_SR.begin(audio::i2sRef(), CMDS, sizeof(CMDS) / sizeof(CMDS[0]), SR_CHANNELS_STEREO, SR_MODE_WAKEWORD);
  if (!up) { mic::useExternalReader(false); err = "speech engine failed to start"; }
  else err = "";
  LOGI("wake", "speech engine %s", up ? "running (say \"Hi ESP\")" : err);
}

}  // namespace wake
