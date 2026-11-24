# Gimbal remote: security and safety

A gimbal that pans by itself is a nuisance, not a danger, but nobody else should be able to steer your camera.

## Link (RemoteLink v1, `Extra/Shared/RemoteLink`)

* **Radio**: ESP-NOW (2.4 GHz, no router). Fixed Wi-Fi channel (default 1).
* **Every frame is authenticated**: HMAC-SHA256 over the whole frame (8 byte tag). A frame without the right tag is ignored. Forging one needs the 32 byte key.
* **Replay protection**: each frame has `(session, sequence)`. The session is a boot counter kept in flash by each device. A frame is accepted only if it is newer than everything accepted before,
  so recorded frames are useless, also after the remote or the gimbal was restarted. Frames are also bound to the **gimbal's boot session**: a frame recorded before the gimbal restarted is refused.
* **Pairing**: ECDH (P-256) + HKDF, key never sent. The gimbal accepts pairing only for **2 minutes after power-up**, the remote only for 30 s after you hold B1 + B4. You must be at both devices.
  Both sides check a confirmation tag so a wrong key is noticed.
* Tested on a PC (`Extra/Shared/RemoteLink/test/run.sh`): tampering, wrong key, truncation, replay (also across restarts), pairing agreement.

## Failsafe (the important part)

| Situation | Reaction of the gimbal module |
|---|---|
| no valid frame for 250 ms (remote off, out of range, jammed, crashed) | all keys released, running macro cancelled, stick centred for 150 ms, then stick pins Hi-Z: **the gimbal's own joystick works again** |
| a key stays held for 20 s | released |
| module reboots | all outputs released at boot (pull-downs keep the relays off) |
| bad frames | counted, ignored |

Jamming the radio therefore **cannot make the gimbal move**; it can only stop the remote from working.

## Not protected

* **Frame contents are not encrypted**: someone listening can see stick positions and buttons (but cannot change or replay them).
* **Pairing has no confirmation code**: during the 2-minute window after the gimbal powers up, a nearby attacker who is faster than you could pair instead of your remote. Power-cycle the gimbal only when you pair.
* **Keys live in the ESP32 flash unencrypted** (enable ESP32 flash encryption if this matters). Physical access to the gimbal or the remote gives the key.
* The Bluetooth link between gimbal and phone is DJI's, untouched.
* Nothing stops someone from pressing the physical buttons.
