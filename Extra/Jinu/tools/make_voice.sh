#!/bin/bash
# Makes Jinu's default voice clips (a cheeky boy voice) with espeak-ng + ffmpeg.
# Output: data/voice/<key>.wav  (16 kHz, 16-bit, mono, plain 44-byte header).
# The file area is only 960 KB, so keep the lines short (total below ~800 KB).
# To use your own voice replace a file (same name) or upload it on the web page:
#   ffmpeg -i in.mp3 -ar 16000 -ac 1 -sample_fmt s16 -map_metadata -1 -fflags +bitexact -flags:a +bitexact out.wav
set -e
cd "$(dirname "$0")/../data/voice"
say() {  # key, speed, pitch, text
  espeak-ng -v en-us+m3 -s "$2" -p "$3" -w /tmp/jinu_raw.wav "$4"
  ffmpeg -y -loglevel error -i /tmp/jinu_raw.wav -ar 16000 -ac 1 -sample_fmt s16 \
    -map_metadata -1 -fflags +bitexact -flags:a +bitexact "$1.wav"
}
say hello     160 75 "Hi! I am Jinu!"
say happy     165 80 "Yay! Hehe!"
say excited   180 85 "Wow! Yippee!"
say shy       135 65 "Oh. Hehe. Stop."
say love      140 70 "Aww. Love you."
say surprised 175 85 "Whoa!"
say angry     160 35 "Hmph! Stop it!"
say sulk      130 30 "Hmph. Not talking."
say scared    180 85 "Eek! Scary!"
say sad       125 45 "Aww. So sad."
say jealous   150 55 "Hey! What about me?"
ls -l; du -ck *.wav | tail -1
