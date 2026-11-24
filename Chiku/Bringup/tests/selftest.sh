#!/bin/bash
# Tests the toolkit itself against simulators (no hardware needed). Needs: python3 + pyserial + cryptography, node + the RobotApp dependencies.
cd "$(dirname "$0")/.."
PY=${PYTHON:-python3}
fail=0
check() { if "$@"; then echo "OK    $*"; else echo "FAIL  $*"; fail=1; fi; }
cleanup() { kill $PIDS 2>/dev/null; }
trap cleanup EXIT
PIDS=""

echo "== crypto interop: Python vs C++ (Shared/DeskLink)"
check g++ -std=c++17 -o /tmp/desklink_tool ../Shared/DeskLink/test/desklink_tool.cpp ../Shared/DeskLink/src/desklink.cpp -lmbedcrypto
check $PY tests/test_interop.py

echo "== robots (mock) and desk (mock)"
(cd ../RobotApp && node tools/mock-robot.mjs > /dev/null 2>&1) & PIDS="$PIDS $!"
(cd ../RobotApp && PAIRING_MS=600000 npx tsx tools/mock-desk.mts > /dev/null 2>&1) & PIDS="$PIDS $!"
sleep 5
check $PY bringup.py --yes robot localhost:8787 --active
check $PY bringup.py --yes robot localhost:8788 --active
check $PY bringup.py --yes desk localhost:8790 --pair --active

echo "== serial boards (simulated)"
for prof in gimbal remote chiku; do
  $PY tests/fake_device.py $prof --boot-delay 1.5 > /tmp/fake_$prof.port & PIDS="$PIDS $!"
  sleep 0.7
  port=$(head -1 /tmp/fake_$prof.port)
  case $prof in
    gimbal) check $PY bringup.py --yes gimbal --port $port --outputs ;;
    remote) check $PY bringup.py --yes remote --port $port ;;
    chiku)  check $PY bringup.py --yes boot chiku --port $port ;;
  esac
done
# radio link test with a simulated gimbal log and a silent remote
$PY tests/fake_device.py gimbal-link --boot-delay 0.5 > /tmp/fake_gl.port & PIDS="$PIDS $!"
$PY tests/fake_device.py remote-link --boot-delay 0.5 > /tmp/fake_rl.port & PIDS="$PIDS $!"
sleep 0.5
check $PY bringup.py --yes link --gimbal-port $(head -1 /tmp/fake_gl.port) --remote-port $(head -1 /tmp/fake_rl.port)
# a board with a failing sensor must be reported as a FAILURE (the tool must not say everything is fine)
$PY tests/fake_device.py chiku-bad --boot-delay 1.5 > /tmp/fake_bad.port & PIDS="$PIDS $!"
sleep 0.7
if $PY bringup.py --yes boot chiku --port $(head -1 /tmp/fake_bad.port) > /tmp/bad.out 2>&1; then echo "FAIL  the failing board was reported as OK"; fail=1; else echo "OK    failing sensor is reported as failure"; fi
grep -q "pad_expander" paste-me.txt && echo "OK    paste-me.txt names the failing item" || { echo "FAIL  paste-me.txt does not mention pad_expander"; fail=1; }
exit $fail
