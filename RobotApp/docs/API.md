# Robot HTTP API v1 (Chiku and Jinu)

Both robots serve the same API on port 80 (`http://chiku.local`, `http://jinu.local` or the IP).
POST arguments are URL query parameters unless noted. Every POST answers `{"ok":true}` or `{"ok":true,"msg":"..."}` (the robot's own feedback, e.g. the gait it started). Bad requests get HTTP 400 with a text reason, and are logged.
Firmware sources: `Chiku/src/head/net.cpp` and `Jinu/src/net.cpp`. `RobotApp/tools/mock-robot.mjs` implements the same API for development.

## Read

| Request | Answer |
|---|---|
| `GET /api/state` | `robot` (`chiku`/`jinu`), `api`, `fw`, `caps[]`, `ip`, `ssid`, `rssi`, `uptime` (s), `heap`, `mode`, `mute`, `mood`, `brightness`, `volume`, `dist` (cm, -1 = none), `pads` (bit n = pad n touched), `padsOnline`, `avoid`, `gait`, `busy`, `power` (0 active, 1 dim, 2 sleep), `audio`, `servo[]` = `{trim,min,max,inv,cur}`. Chiku adds `screen`, `screenName`, `rotation` (screen rotation, degrees), `setupDone`, `mic` (sound level dB, -1 = no mic), `micOk`, `batt`, `charging`, `pca`, `imu{ok,x,y,z,tilt}`, `cam{alive,ip,state,stream,snap}` (`snap` = size in bytes of the last snapshot), `weather`, `lat`, `lon`, `alarms[]`, `timers[]` |
| `GET /api/events?since=N` | `{"next":N2,"events":[{n,t,type,a,b}]}`: last 48 events with `n > since` (touch gestures, shake, buttons ...). If `next` is smaller than your cursor the robot rebooted |
| `GET /api/log?since=N` | `{"next":N2,"lines":[{n,t,l,m}]}`: the robot's complete log (last 100 lines): its own `[tag] text` lines, every API call it received (`[api] POST /api/mood m=3`), every event, and the ESP system logs (Wi-Fi, I2C errors ...). `l` = 0 error, 1 warning, 2 info, 3 debug. Also printed on the USB serial port. If `next` is smaller than your cursor the robot rebooted |
| `GET /api/diag` | `{"items":[{name,ok,detail,live}],heap,uptime}`: results of the start-up hardware checks (display, touch, imu, rtc, pmu, psram, audio, servo_driver, pad_expander, filesystem) plus live checks (sonar, wifi, heap; Chiku: imu_gravity, rtc_time, cam_link, battery) |
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
| `POST /api/mood?name=happy&hold=ms&move=1` | face by name (or `m=` index: Jinu 0..15, Chiku 0..21: normal happy sad sleepy angry surprised dizzy love thinking listening jealous sulk shy excited scared cry curious confused bored wink error speaking; `neutral` = normal). `hold` default 5000, 0 = stay. Chiku: `move=1` = the whole emotion (face + body pose + sound) |
| `POST /api/look?x=-1..1&y=-1..1` | look direction |
| `POST /api/motion?g=NAME&n=cycles` | gait or emotion gait: home walk back left right bend shakeleg moonwalk dance jump wiggle happy excited angry scared sulk jealous shy love sad surprised tiltl tiltr; Chiku also (UI kit poses): sleepy curious confused bored dizzy wink thinking listen speak |
| `POST /api/motion?stop=1` or `?avoid=1\|0` | stop / explore on-off |
| `POST /api/servo/set?ch=&deg=&speed=` | move a servo (speed in deg/s, default 90); obeys trim and limits |
| `POST /api/servo/release` | power the servos off |
| `POST /api/say?key=hello` | voice clip, or babble if missing |
| `POST /api/beep?hz=&ms=` | tone |
| `POST /api/melody?id=0..7&loop=0\|1` | startup alarm happy sad pet timer dizzy click |
| `POST /api/audio/stop` | stop sound |
| `POST /api/led?r=&g=&b=` / `?off=1` | Jinu: test colour on the RGB LED / follow the mood again |
| `POST /api/display?p=0..6` | test pattern: 0 off, red, green, blue, white, black, grey ramp |
| `POST /api/sim?pad=N&g=1..5` | pretend a pad gesture happened |
| `POST /api/sim?ev=NAME` | Chiku: shake pickup putdown fall tap obstacle touch alarm. Jinu: obstacle button |
| `POST /api/screen?name=clock` | Chiku screen by name (or `id=` index): face clock weather timer menu alarms moves sensors sonar camera network games desk settings calibration orientation power goodnight ringing notify boot setup |
| `POST /api/notify?title=Doorbell&text=...&mood=surprised` | Chiku: alert screen (big eyes in the mood colour, card with the text, closes after 20 s) + matching emotion. For Home Assistant, Node-RED ... |
| `POST /api/rotation?deg=0..359&save=1` | Chiku: turn the screen picture and touch by any angle; `save=1` keeps it (NVS) |

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

## Desk accessory (see Desk/docs)

Through a robot (the robot holds its own key to the desk; every call needs the phone's secret `token`, else HTTP 401):

| Request | Effect |
|---|---|
| `GET /api/desk` | `{paired,host,ok,hv,h,moving,dir,armed,travel,src,err,age}` (also in `/api/state` as `desk`; `caps` contains `desk`) |
| `POST /api/desk/pair?host=desk.local&token=T` | the robot pairs with the desk (pairing window must be open); `token` (8+ chars) protects the next calls |
| `POST /api/desk/unpair?token=T` | forget the desk |
| `POST /api/desk/cmd?token=T&op=move&dir=up\|down` | hold (repeat every 250 ms), `op=stop`, `op=preset&n=1..4`, `op=save&n=`, `op=tap&key=&ms=`, `op=setheight&h=`, `op=arm&on=1\|0` |

The desk module itself speaks **DeskLink v1** (`Shared/DeskLink`, `Desk/docs/SECURITY.md`): `GET /dl/info`, `GET /dl/challenge`, `POST /dl/pair`, `POST /dl/cmd` (sealed). The phone app talks to it directly.
