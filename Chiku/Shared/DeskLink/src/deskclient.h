// deskclient.h - the client side of DeskLink for Chiku and Jinu (Arduino / ESP32 only).
// Pairs with the desk module and sends sealed commands. All calls block for up to a few seconds,
// so call them from the network task, never from the UI task.
#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#include <ArduinoJson.h>
#include "desklink.h"

class DeskClient {
 public:
  // Loads host, client id and key from NVS (namespace "desklink"). Call once after Wi-Fi is configured.
  void begin();
  bool paired() const { return paired_; }
  const String& host() const { return host_; }
  const String& id() const { return id_; }

  // Pairs with the desk module (its pairing window must be open). myName is shown in its client list.
  bool pair(const String& host, const char* myName, String& err);
  void unpair();                                     // forget the key on this side
  // Sends one command, e.g. {"op":"state"}. Returns false (and err) on network or authentication errors.
  bool call(const JsonDocument& request, JsonDocument& response, String& err, uint32_t timeoutMs = 3000);

 private:
  bool resolve(IPAddress& ip, String& err);
  bool paired_ = false;
  String host_, id_;
  desklink::Key key_;
  IPAddress ip_;
  uint32_t ipAt_ = 0;
};
#endif
