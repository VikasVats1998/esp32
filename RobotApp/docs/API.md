# Robot HTTP API v1 (Chiku and Jinu)

Both robots serve the same API on port 80 (`http://chiku.local`, `http://jinu.local` or the IP).
POST arguments are URL query parameters unless noted. Every POST answers `{"ok":true}`.
Firmware sources: `Chiku/src/head/net.cpp` and `Jinu/src/net.cpp`. `RobotApp/tools/mock-robot.mjs` implements the same API for development.

## Read

| Request | Answer |
|---|---|
| `GET /api/state` | `robot` (`chiku`/`jinu`), `api`, `fw`, `caps[]`, `ip`, `ssid`, `rssi`, `uptime` (s), `heap`, `mode`, `mute`, `mood`, `brightness`, `volume`, `dist` (cm, -1 = none), `pads` (bit n = pad n touched), `padsOnline`, `avoid`, `gait`, `busy`, `power` (0 active, 1 dim, 2 sleep), `audio`, `servo[]` = `{trim,min,max,inv,cur}`. Chiku adds `screen`, `batt`, `charging`, `pca`, `imu{ok,x,y,z,tilt}`, `cam{alive,ip,state,stream}`, `weather`, `lat`, `lon`, `alarms[]`, `timers[]` |
| `GET /api/events?since=N` | `{"next":N2,"events":[{n,t,type,a,b}]}`: last 48 events with `n > since` (touch gestures, shake, buttons ...). If `next` is smaller than your cursor the robot rebooted |
| `GET /api/voice/list` | `["hello","happy",...]` clips in LittleFS `/voice` |

`caps` (Chiku): `screens alarms timers weather imu cam pan battery display_test`. (Jinu): `display_test rgb_led`.

### Event types
`pad` (a = pad id, b = gesture: 1 tap, 2 double, 3 triple, 4 long press start, 5 released), `button` (a: 1 short, 2 long),
`obstacle` (a = cm), `motion`, `power`, `wake`, `wifi`, `settings`. Chiku also: `tap` `long` `swipe` (screen), `shake` `pickup`
`putdown` `fall` `imu_tap`, `cam`, `alarm_ring` `alarm_stop` `timer_done` `time_synced` `weather` `screen`.
Pad ids: Chiku 0 ear L, 1 ear R, 2 leg L, 3 leg R, 4 body. Jinu 0 head L, 1 head R, 2 leg L, 3 leg R, 4 body.

## Modes

| Request | Effect |
|---|---|
| `POST /api/mode?m=auto` | normal personality |
| `POST /api/mode?m=manual` | no automatic reactions or idle life, never sleeps, head does not follow the look direction: for testing |
| `POST /api/mode?m=explore` | walks and avoids obstacles (plus personality) |
| `POST /api/mute?on=1` | silence everything (`on=0` unmutes) |

## Control

| Request | Effect |
|---|---|
| `POST /api/mood?m=0..15&hold=ms` | face (`hold` default 5000, 0 = stay) |
| `POST /api/look?x=-1..1&y=-1..1` | look direction |
| `POST /api/motion?g=NAME&n=cycles` | gait or emotion gait: home walk back left right bend shakeleg moonwalk dance jump wiggle happy excited angry scared sulk jealous shy love sad surprised tiltl tiltr |
| `POST /api/motion?stop=1` or `?avoid=1\|0` | stop / explore on-off |
| `POST /api/servo/set?ch=&deg=&speed=` | move a servo (speed in deg/s, default 90); obeys trim and limits |
| `POST /api/servo/release` | power the servos off |
| `POST /api/say?key=hello` | voice clip, or babble if missing |
| `POST /api/beep?hz=&ms=` | tone |
| `POST /api/melody?id=0..7&loop=0\|1` | startup alarm happy sad pet timer dizzy click |
| `POST /api/audio/stop` | stop sound |
| `POST /api/display?p=0..6` | test pattern: 0 off, red, green, blue, white, black, grey ramp |
| `POST /api/sim?pad=N&g=1..5` | pretend a pad gesture happened |
| `POST /api/sim?ev=NAME` | Chiku: shake pickup putdown fall tap obstacle touch alarm. Jinu: obstacle button |
| `POST /api/screen?id=0..8` | Chiku screen (Face Clock Weather Timer Sonar Camera Settings Menu Calibration) |

## Settings and system

| Request | Effect |
|---|---|
| `POST /api/set` (JSON body) | any of `brightness` (5-255), `volume` (0-100), `servo` `[{trim,min,max,inv}]`. Chiku: `lat`, `lon`, `alarms` `[{on,h,m,days}]` (days bit0 = Monday) |
| `POST /api/timer?sec=` and `/api/timer/cancel?slot=` | Chiku timers |
| `POST /api/cam?cmd=wake\|sleep\|led_on\|led_off\|snap\|ip` | Chiku camera |
| `POST /api/weather/refresh` | Chiku |
| `POST /api/reboot`, `POST /api/wifi/reset` | |
| `POST /api/voice` (multipart, a `*.wav` file) | upload a clip (16 kHz 16-bit mono) |
| `POST /update` (multipart, file field `fw`, a .bin) | OTA firmware update |
| `POST /api/wifi` (form `ssid`, `pass`) | set Wi-Fi (setup portal) |
