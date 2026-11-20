# Desk module security

A remote control for something that moves must be hard to misuse. This is what protects it and what does not.

## Goals

1. Only devices **you paired** can move the desk.
2. A recorded message **cannot be replayed**.
3. A bug, a lost Wi-Fi connection or a crashed app must **not leave the desk moving**.
4. Someone on your Wi-Fi who is not paired cannot even guess their way in.

## The link (DeskLink v1, `Shared/DeskLink`)

* **Pairing** (ECDH, curve P-256). Both sides make a key pair, exchange public keys and compute the same secret; the key is
  `HKDF-SHA256(secret, info = clientPub || serverPub)`. The key itself never travels. Every client (phone, Chiku, Jinu) gets its **own** key (max 4 clients).
* **Pairing window**: only for **3 minutes after the module is powered up**. To pair a new device you must be at the desk and power-cycle it (physical presence).
* **Every command**: the client asks for a **one-time challenge** (16 random bytes, valid 10 s, used once, even if the message that follows is wrong) and sends the command
  sealed with **AES-256-GCM**; the challenge is the authenticated data. So a copy of a request is worthless (the challenge is gone), the answer is bound to the same
  challenge and cannot be replayed to another request, and any change of the message fails the tag check. No clock is needed on the module.
* **Lock-out**: 5 bad messages within a minute lock all commands for a minute; the module also limits requests to 30 per second.
* **Constant time** comparison for challenges; invalid public keys are rejected.
* The C++ (firmware) and TypeScript (app) implementations are tested against each other (`Shared/DeskLink/test/run.sh`): same key, each opens the other's messages,
  and replay / wrong purpose / tampering are rejected on both sides.

## Safety rules in the firmware (not security, but they matter more)

* UP and DOWN are never pressed together; a direction change waits 200 ms.
* **Dead-man**: a held UP/DOWN is released 450 ms after the last `move` message. The client must keep sending (the app and robots do every 250 ms). Lose Wi-Fi, close the app,
  crash: the desk stops within half a second.
* One continuous run is limited to 25 s, then a 3 s cool-down.
* **Child lock** (`arm off`): all movement commands are refused. The handset keeps working.
* All button outputs are LOW at power-up and on any error; 10 k pull-downs keep them released while the chip resets.
* The desk's own anti-collision still applies, and pressing any key on the handset stops the desk.

## Robots

Chiku and Jinu pair with the desk themselves. Because a robot holds a key, its **HTTP API for the desk is protected by a secret token** that only your phone knows
(created in the phone's secure storage, sent when the phone pairs the robot). Without the token `/api/desk/cmd` answers 401. (The robots' other API endpoints are still open on your LAN, as before.)

## What is NOT protected (be aware)

* **The Wi-Fi transport is plain HTTP**: the *contents* of commands and answers are encrypted, but someone who can capture your Wi-Fi traffic sees that messages exist, their size and timing.
* **Pairing is not authenticated by a code**: during the 3-minute window an attacker on your Wi-Fi who is faster than you could pair. Keep the window short (power-cycle only when you pair)
  and check Control, Desk, Diagnostics, Clients afterwards: only your devices should be listed. If something is wrong, power-cycle and unpair it.
* **Keys are stored in the ESP32's flash unencrypted** unless you enable ESP32 flash encryption. Someone with the module in hand can read them. (On the phone they are in the Keychain / Keystore.)
* **Firmware updates (OTA)** only work in the pairing window and need the module's 6-digit PIN (shown in the serial log at boot). Changing Wi-Fi needs the PIN too once Wi-Fi is set.
* **Physical access** to the handset always wins: anyone can press its keys.
* The robots' own `/update` and other API endpoints are open on your LAN (they were before this accessory); a token only guards the desk functions.

## If you lose a device

Power-cycle the desk and, within 3 minutes, use another paired device (Control, Desk, Diagnostics) or erase the module's flash to remove all keys: `pio run -e desk -t erase`.
