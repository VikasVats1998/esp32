#!/bin/bash
# Makes Chiku's default voice clips (cute, high pitch) with espeak-ng + ffmpeg.
# Output: data/voice/<key>.wav  (16 kHz, 16-bit, mono, plain 44-byte header)
# To use a nicer voice: replace any file with your own recording / TTS output in the same format:
#   ffmpeg -i in.mp3 -ar 16000 -ac 1 -sample_fmt s16 -map_metadata -1 -fflags +bitexact -flags:a +bitexact out.wav
set -e
cd "$(dirname "$0")/../data/voice"
say() {  # key, speed, pitch, text
  espeak-ng -v en-us+f3 -s "$2" -p "$3" -w /tmp/chiku_raw.wav "$4"
  ffmpeg -y -loglevel error -i /tmp/chiku_raw.wav -ar 16000 -ac 1 -sample_fmt s16 \
    -map_metadata -1 -fflags +bitexact -flags:a +bitexact "$1.wav"
}
say hello     150 90 "Hi! I am Chiku!"
say happy     160 95 "Yay! Hehe!"
say excited   175 99 "Wow! Yippee! Yay!"
say shy       125 80 "Oh. Hehe. Stop it."
say love      130 85 "Aww. I love you."
say surprised 170 99 "Whoa!"
say angry     150 40 "Hmph! Stop that!"
say sulk      120 35 "Hmph. I am not talking to you."
say scared    175 99 "Eek! Scary!"
say sad       115 55 "Aww. I am so sad."
say jealous   140 70 "Hey! What about me?"
say alarm     150 90 "Wake up! Wake up! It is time!"
say timer     150 90 "Time is up!"
say goodmorning 140 85 "Good morning!"
ls -l
