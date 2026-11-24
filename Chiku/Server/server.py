#!/usr/bin/env python3
"""Chiku hub server (stdlib only, Python 3.8+).

Robots (clients) poll POST /api/sync with their telemetry and any device changes made on the
robot. The reply holds the full device list plus per-robot settings, so the robot, the web
dashboard and the phone app always see the same state.

    python3 server.py [--port 8080] [--token chiku]

Web dashboard : http://<this-pc>:8080/
API           : GET /api/state, GET /api/history?id=chiku,
                POST /api/devices/<id> {on, level}, POST /api/robots/<id>/cfg {clapWake, clapSens, wakeWord, calibrate}
AI voice hook : POST /api/ai/chat  (see ai_hook() - plug a model in later)
"""
import argparse
import json
import socket
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

LOCK = threading.Lock()

# type: light (level + colour temperature), fan (level = PWM %), switch, climate (level = degC)
DEVICES = [
    {"id": "lamp", "name": "Desk Lamp",   "type": "light",   "on": True,  "level": 84, "temp": 2700, "load": 11.8},
    {"id": "fan",  "name": "Desk Fan",    "type": "fan",     "on": True,  "level": 72, "pwm_khz": 25},
    {"id": "plug", "name": "Plug Socket", "type": "switch",  "on": False, "level": 0,  "load": 140.0},
    {"id": "ac",   "name": "AC Climate",  "type": "climate", "on": True,  "level": 22, "mode": "COOL"},
    {"id": "aura", "name": "Matrix Aura", "type": "light",   "on": True,  "level": 60, "temp": 4000, "load": 4.2},
]
ENV = {"temp_c": 24.5, "humidity": 52}
ROBOTS = {}                 # id -> last telemetry (plus "seen")
HISTORY = {}                # id -> deque of samples (1 per sync, max 600)
CFG = {}                    # id -> settings pushed to the robot; "calibrate" is a one-shot
AI = {"enabled": False}     # flipped on once ai_hook() is wired up
TOKEN = "chiku"
HIST_KEYS = ("snd", "tilt", "roll", "pitch", "gyro", "temp", "bat", "batv", "rssi", "heap")


def ai_hook(robot_id, text):
    """Placeholder for the AI voice assistant. Later: send `text` (or audio) to a model /
    XiaoZhi server and return {"say": "...", "actions": [{"dev": "lamp", "on": true}]}."""
    return {"say": "AI voice is not enabled on this server yet.", "actions": []}


def apply_patch(dev_id, patch):
    for d in DEVICES:
        if d["id"] == dev_id:
            for k in ("on", "level", "temp"):
                if k in patch:
                    d[k] = patch[k]
            d["rev"] = d.get("rev", 0) + 1
            return True
    return False


def robot_cfg(rid):
    c = CFG.setdefault(rid, {})
    out = dict(c)
    c.pop("calibrate", None)                      # one-shot
    return out


def snapshot():
    now = time.time()
    return {"devices": DEVICES, "env": ENV, "ai": AI, "time": int(now),
            "robots": {k: dict(v, age=round(now - v["seen"], 1)) for k, v in ROBOTS.items()},
            "cfg": CFG}


PAGE = r"""<!doctype html><html lang=en><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Chiku Hub</title>
<style>
:root{--bg:#0d1117;--card:#161b22;--line:#30363d;--txt:#e6edf3;--dim:#8b949e;--cyan:#00e5ff;--orange:#ffb35c;--green:#2be08a;--red:#ff5c5c;color-scheme:dark}
@media (prefers-color-scheme:light){:root{--bg:#f4f6f8;--card:#fff;--line:#d0d7de;--txt:#1f2328;--dim:#59636e;--cyan:#0086a8;--orange:#b86b00;--green:#0a8f4f;--red:#d1242f;color-scheme:light}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--txt);font:15px/1.4 system-ui,sans-serif}
main{max-width:1000px;margin:auto;padding:16px}
header{display:flex;align-items:center;gap:12px;margin-bottom:12px}h1{font-size:20px;margin:0;flex:1}
.pill{border:1px solid var(--line);border-radius:99px;padding:3px 12px;font-size:13px;color:var(--dim)}
.pill.ok{color:var(--green);border-color:var(--green)}.pill.bad{color:var(--red);border-color:var(--red)}
.grid{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(290px,1fr))}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:14px}
.card h2{font-size:12px;letter-spacing:.08em;text-transform:uppercase;color:var(--dim);margin:0 0 10px;font-weight:600}
.row{display:flex;align-items:center;gap:10px;margin:6px 0}.grow{flex:1}
.big{font-size:34px;font-weight:600;line-height:1}small,.dim{color:var(--dim)}
.kv{display:grid;grid-template-columns:1fr 1fr;gap:6px 14px}.kv div b{display:block;font-size:18px}
input[type=range]{width:100%;accent-color:var(--cyan)}
button{background:var(--card);color:var(--txt);border:1px solid var(--line);border-radius:10px;padding:7px 14px;font:inherit;cursor:pointer}
button:hover{border-color:var(--cyan)}button.on{background:color-mix(in srgb,var(--cyan) 18%,var(--card));border-color:var(--cyan)}
canvas{width:100%;display:block}.bar{height:10px;border-radius:6px;background:var(--line);overflow:hidden}.bar i{display:block;height:100%;background:var(--green);width:0}
.dev.light .big{color:var(--orange)}.dev.fan .big{color:var(--cyan)}
.switch{position:relative;width:44px;height:24px;flex:none}.switch input{opacity:0;width:0;height:0}
.switch span{position:absolute;inset:0;background:var(--line);border-radius:24px;transition:.2s}
.switch span:before{content:"";position:absolute;width:18px;height:18px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.2s}
.switch input:checked+span{background:var(--green)}.switch input:checked+span:before{transform:translateX(20px)}
</style>
<main>
<header><h1>Chiku Hub</h1><span class=pill id=ai>AI voice: later</span><span class=pill id=link>no robot</span></header>

<div class=grid>
 <section class=card><h2>Motion · gyroscope</h2>
  <canvas id=level width=300 height=240></canvas>
  <div class=kv style="margin-top:8px"><div><small>Tilt</small><b id=tilt>-</b></div><div><small>Rotation</small><b id=gyro>-</b></div>
  <div><small>Roll / pitch</small><b id=rp>-</b></div><div><small>Orientation</small><b id=flip>-</b></div></div>
  <div class=row><button id=cal>Recalibrate (keep still)</button></div>
 </section>

 <section class=card><h2>Sound · microphone</h2>
  <div class=big id=snd>-</div><div class=bar style="margin:8px 0"><i id=sndbar></i></div>
  <canvas id=c_snd width=300 height=70></canvas>
  <div class=row><span class=grow>Clap wake</span><label class=switch><input type=checkbox id=clapWake><span></span></label></div>
  <div class=row><small>Sensitivity</small><input type=range id=clapSens min=0 max=100></div>
  <div class=row><span class=grow>Voice wake word <small>("Hi ESP")</small></span><label class=switch><input type=checkbox id=wakeWord><span></span></label></div>
 </section>

 <section class=card><h2>Power · temperature</h2>
  <div class=row><div class=big id=bat>-</div><div class=grow><div class=bar><i id=batbar></i></div><small id=batinfo></small></div></div>
  <canvas id=c_bat width=300 height=70></canvas>
  <div class=kv style="margin-top:8px"><div><small>Core temperature</small><b id=temp>-</b></div><div><small>Wi-Fi</small><b id=rssi>-</b></div>
  <div><small>Free memory</small><b id=heap>-</b></div><div><small>Firmware</small><b id=fw>-</b></div></div>
  <canvas id=c_temp width=300 height=70 style="margin-top:8px"></canvas>
 </section>
</div>

<h2 style="font-size:12px;letter-spacing:.08em;text-transform:uppercase;color:var(--dim);margin:18px 2px 8px">Devices</h2>
<div class=grid id=devs></div>

<script>
const $=id=>document.getElementById(id);
const hdr=()=>({'X-Token':localStorage.t||'chiku'});
const post=(u,p)=>fetch(u,{method:'POST',headers:hdr(),body:JSON.stringify(p)});
let hold=0, ROBOT='chiku';

function spark(cv,vals,color,min,max){
  const c=cv.getContext('2d'),w=cv.width,h=cv.height;c.clearRect(0,0,w,h);
  const st=getComputedStyle(document.documentElement);
  c.strokeStyle=st.getPropertyValue('--line');c.lineWidth=1;c.beginPath();c.moveTo(0,h-.5);c.lineTo(w,h-.5);c.stroke();
  if(vals.length<2)return;
  if(min==null)min=Math.min(...vals);if(max==null)max=Math.max(...vals);if(max-min<1){max=min+1}
  c.strokeStyle=st.getPropertyValue(color);c.lineWidth=2;c.beginPath();
  vals.forEach((v,i)=>{const x=i/(vals.length-1)*w,y=h-4-(v-min)/(max-min)*(h-8);i?c.lineTo(x,y):c.moveTo(x,y)});c.stroke();
}
function bubble(roll,pitch,flip){
  const cv=$('level'),c=cv.getContext('2d'),w=cv.width,h=cv.height,cx=w/2,cy=h/2,R=100;
  const st=getComputedStyle(document.documentElement);c.clearRect(0,0,w,h);
  c.strokeStyle=st.getPropertyValue('--line');c.lineWidth=2;
  [R,R*.5,12].forEach(r=>{c.beginPath();c.arc(cx,cy,r,0,7);c.stroke()});
  c.beginPath();c.moveTo(cx-R-10,cy);c.lineTo(cx+R+10,cy);c.moveTo(cx,cy-R-10);c.lineTo(cx,cy+R+10);c.stroke();
  let x=Math.max(-1,Math.min(1,roll/45))*R,y=Math.max(-1,Math.min(1,pitch/45))*R;const l=Math.hypot(x,y);if(l>R){x*=R/l;y*=R/l}
  c.fillStyle=st.getPropertyValue(l<8?'--green':'--orange');c.beginPath();c.arc(cx+x,cy-y,14,0,7);c.fill();
}
function bindCfg(id,key,type){
  const el=$(id);
  el.onchange=()=>{hold=Date.now()+2500;post('/api/robots/'+ROBOT+'/cfg',{[key]:type=='b'?el.checked:+el.value})};
  el.oninput=()=>{hold=Date.now()+2500};
}
bindCfg('clapWake','clapWake','b');bindCfg('wakeWord','wakeWord','b');bindCfg('clapSens','clapSens','n');
$('cal').onclick=()=>post('/api/robots/'+ROBOT+'/cfg',{calibrate:true});

async function load(){
  let s;try{s=await (await fetch('/api/state')).json()}catch(e){$('link').textContent='server offline';$('link').className='pill bad';return}
  $('ai').textContent='AI voice: '+(s.ai.enabled?'on':'later');
  const ids=Object.keys(s.robots);
  if(!ids.length){$('link').textContent='no robot connected';$('link').className='pill bad'}
  else{ROBOT=ids.includes('chiku')?'chiku':ids[0];const r=s.robots[ROBOT];
    $('link').textContent=ROBOT+' · '+(r.age<5?'online':'seen '+Math.round(r.age)+'s ago');$('link').className='pill '+(r.age<5?'ok':'bad');
    $('tilt').textContent=r.tilt+'°';$('gyro').textContent=r.gyro+' °/s';$('rp').textContent=r.roll+'° / '+r.pitch+'°';
    $('flip').textContent=r.flip?'upside down':'upright';bubble(r.roll||0,r.pitch||0,r.flip);
    $('snd').textContent=(r.snd??'-')+(r.snd!=null?' %':'');$('sndbar').style.width=(r.snd||0)+'%';
    $('bat').textContent=(r.bat<0?'--':r.bat+'%');$('batbar').style.width=Math.max(0,r.bat)+'%';
    $('batinfo').textContent=(r.batv||'-')+' V'+(r.chg?' · charging':'')+(r.usb?' · USB':'');
    $('temp').textContent=r.temp+' °C';$('rssi').textContent=r.rssi+' dBm';$('heap').textContent=r.heap+' KB';$('fw').textContent=r.fw;
    if(Date.now()>hold){const c=s.cfg[ROBOT]||{};
      if('clapWake' in r)$('clapWake').checked=r.clapWake;if('wakeWord' in r)$('wakeWord').checked=r.wakeWord;if('clapSens' in r)$('clapSens').value=r.clapSens;}
  }
  if(Date.now()>hold)$('devs').innerHTML=s.devices.map(d=>`<section class="card dev ${d.type}"><div class=row><b class=grow>${d.name}</b>
   <small>${d.load?d.load+' W':d.type}</small><label class=switch><input type=checkbox ${d.on?'checked':''} onchange="post('/api/devices/${d.id}',{on:this.checked})"><span></span></label></div>
   <div class=big>${d.on?d.level:0}<small style="font-size:16px"> ${d.type=='climate'?'°C':'%'}</small></div>
   <input type=range min=0 max=${d.type=='climate'?30:100} value=${d.level} oninput="hold=Date.now()+1500" onchange="post('/api/devices/${d.id}',{level:+this.value})"></section>`).join('');
}
async function hist(){
  try{const h=await (await fetch('/api/history?id='+ROBOT)).json();const g=k=>h.map(x=>x[k]).filter(v=>v!=null);
   spark($('c_snd'),g('snd'),'--cyan',0,100);spark($('c_bat'),g('bat'),'--green',0,100);spark($('c_temp'),g('temp'),'--orange')}catch(e){}
}
setInterval(load,1000);setInterval(hist,3000);load();hist();
</script></html>"""


class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *a):
        pass

    def _send(self, code, body, ctype="application/json"):
        data = body.encode() if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "X-Token, Content-Type")
        self.end_headers()
        self.wfile.write(data)

    def _json_body(self):
        n = int(self.headers.get("Content-Length", 0))
        try:
            return json.loads(self.rfile.read(n) or b"{}")
        except ValueError:
            return None

    def _authed(self):
        return self.headers.get("X-Token", "") == TOKEN

    def do_OPTIONS(self):                               # CORS preflight (phone app / other pages)
        self._send(204, "")

    def do_GET(self):
        path, _, query = self.path.partition("?")
        if path == "/":
            return self._send(200, PAGE, "text/html; charset=utf-8")
        if path == "/api/state":
            with LOCK:
                return self._send(200, json.dumps(snapshot()))
        if path == "/api/history":
            rid = dict(p.split("=", 1) for p in query.split("&") if "=" in p).get("id", "chiku")
            with LOCK:
                return self._send(200, json.dumps(list(HISTORY.get(rid, []))))
        self._send(404, '{"error":"not found"}')

    def do_POST(self):
        body = self._json_body()
        if body is None:
            return self._send(400, '{"error":"bad json"}')
        if not self._authed():
            return self._send(401, '{"error":"bad token"}')
        path = self.path
        if path == "/api/sync":                         # robot -> server -> robot
            rid = str(body.get("id", "robot"))[:32]
            with LOCK:
                tel = body.get("tel", {})
                tel["seen"] = time.time()
                ROBOTS[rid] = tel
                h = HISTORY.setdefault(rid, deque(maxlen=600))
                h.append({"t": int(tel["seen"]), **{k: tel[k] for k in HIST_KEYS if k in tel}})
                for c in body.get("cmds", []):
                    apply_patch(c.get("dev"), c)
                reply = snapshot()
                reply["cfg"] = robot_cfg(rid)           # this robot's settings (one-shots are cleared)
                return self._send(200, json.dumps(reply))
        if path.startswith("/api/devices/"):            # web dashboard / app
            with LOCK:
                ok = apply_patch(path.rsplit("/", 1)[1], body)
            return self._send(200 if ok else 404, '{"ok":%s}' % ("true" if ok else "false"))
        if path.startswith("/api/robots/") and path.endswith("/cfg"):
            rid = path.split("/")[3]
            with LOCK:
                c = CFG.setdefault(rid, {})
                for k in ("clapWake", "wakeWord", "clapSens", "calibrate"):
                    if k in body:
                        c[k] = body[k]
            return self._send(200, '{"ok":true}')
        if path == "/api/ai/chat":
            return self._send(200, json.dumps(ai_hook(body.get("id", "robot"), body.get("text", ""))))
        self._send(404, '{"error":"not found"}')


BEACON_PORT = 41234


def beacon(port):
    """Tells every robot on the LAN where the hub is (UDP broadcast every 2 s), so the robots
    need no address typed in. A robot that already has a server address keeps it."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    msg = json.dumps({"hub": "chiku", "port": port}).encode()
    while True:
        try:
            sock.sendto(msg, ("255.255.255.255", BEACON_PORT))
        except OSError:
            pass                                       # no network right now
        time.sleep(2)


def main():
    global TOKEN
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--token", default="chiku")
    a = ap.parse_args()
    TOKEN = a.token
    threading.Thread(target=beacon, args=(a.port,), daemon=True).start()
    print("Chiku hub on http://0.0.0.0:%d/  (token: %s), announcing itself on the LAN" % (a.port, TOKEN))
    ThreadingHTTPServer(("0.0.0.0", a.port), H).serve_forever()


if __name__ == "__main__":
    main()
