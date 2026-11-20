"""The Python DeskLink client must agree with the C++ firmware code (/tmp/desklink_tool)."""
import os
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from lib import desklink_py as dl

TOOL = os.environ.get("DESKLINK_TOOL", "/tmp/desklink_tool")
run = lambda *a: subprocess.run([TOOL, *a], capture_output=True, text=True)
ok = True

def check(name, cond):
    global ok
    print(("PASS  " if cond else "FAIL  ") + name)
    ok &= bool(cond)

priv, pub = dl.keypair()
priv_hex = format(priv.private_numbers().private_value, "064x")
check("public key matches C++", run("pub", priv_hex).stdout.strip() == pub.hex())
spriv, spub = run("keygen").stdout.split()
key = dl.derive(priv, bytes.fromhex(spub), pub, bytes.fromhex(spub))
kcpp = run("derive", spriv, pub.hex(), pub.hex(), spub).stdout.strip()
check("ECDH + HKDF key agrees (Python client, C++ server)", key.hex() == kcpp)
chal = os.urandom(16)
blob = dl.seal(key, chal, "cmd", b'{"op":"state"}')
check("C++ opens what Python sealed", run("open", kcpp, chal.hex(), "cmd", blob.hex()).stdout.strip() == '{"op":"state"}')
b2 = run("seal", kcpp, chal.hex(), "rsp", '{"ok":true}').stdout.strip()
check("Python opens what C++ sealed", dl.open_(key, chal, "rsp", bytes.fromhex(b2)) == b'{"ok":true}')
sys.exit(0 if ok else 1)
