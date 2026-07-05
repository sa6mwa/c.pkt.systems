#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  printf 'usage: audio_live_vox_playback_rx_order_test.sh <cpkt_audio_live_vox_c89_example>\n' >&2
  exit 2
fi

example=$1
tmp=${TMPDIR:-/tmp}/cpkt-audio-live-vox-order-$$
mkdir -p "$tmp/bin"
trap 'status=$?; if [ -n "${child_pid:-}" ]; then kill "$child_pid" >/dev/null 2>&1 || true; wait "$child_pid" >/dev/null 2>&1 || true; fi; rm -rf "$tmp"; exit "$status"' EXIT HUP INT TERM

cat >"$tmp/bin/arecord" <<'EOS'
#!/usr/bin/env python3
import os
import signal
import struct
import sys
import time

running = True

def stop(signum, frame):
    global running
    running = False

signal.signal(signal.SIGTERM, stop)

state = os.environ["CPKT_FAKE_ARECORD_STATE"]
try:
    with open(state, "r", encoding="ascii") as f:
        generation = int(f.read().strip() or "0") + 1
except FileNotFoundError:
    generation = 1
with open(state, "w", encoding="ascii") as f:
    f.write(str(generation))

def emit(sample, frames):
    chunk = struct.pack("<h", sample) * 160
    remaining = frames
    while running and remaining > 0:
        take = min(160, remaining)
        sys.stdout.buffer.write(chunk[:take * 2])
        sys.stdout.buffer.flush()
        remaining -= take
        time.sleep(0.005)

if generation == 1:
    emit(12000, 1600)
    emit(0, 4800)
while running:
    emit(0, 1600)
EOS
chmod +x "$tmp/bin/arecord"

cat >"$tmp/bin/aplay" <<'EOS'
#!/usr/bin/env bash
set -euo pipefail
touch "${CPKT_FAKE_APLAY_STARTED:?}"
cat >/dev/null &
cat_pid=$!
sleep 0.6
touch "${CPKT_FAKE_APLAY_DONE:?}"
wait "$cat_pid"
EOS
chmod +x "$tmp/bin/aplay"

CPKT_FAKE_APLAY_STARTED="$tmp/aplay.started" \
CPKT_FAKE_APLAY_DONE="$tmp/aplay.done" \
CPKT_FAKE_ARECORD_STATE="$tmp/arecord.state" \
PATH="$tmp/bin:$PATH" \
"$example" --backend process --seconds 2 --hang-ms 100 --prebuffer-ms 0 \
  >"$tmp/stdout" 2>"$tmp/stderr" &
child_pid=$!

deadline=$((SECONDS + 8))
rx_before_playback_done=0
while kill -0 "$child_pid" >/dev/null 2>&1; do
  if grep -q '^RX segment=' "$tmp/stdout" 2>/dev/null && [ ! -f "$tmp/aplay.done" ]; then
    rx_before_playback_done=1
    break
  fi
  if [ "$SECONDS" -ge "$deadline" ]; then
    printf 'live vox example timed out\n' >&2
    ls -la "$tmp" >&2 || true
    cat "$tmp/stdout" >&2 || true
    cat "$tmp/stderr" >&2 || true
    exit 1
  fi
  sleep 0.02
done

if [ "$rx_before_playback_done" -ne 0 ]; then
  printf 'RX segment was emitted before playback process finished\n' >&2
  cat "$tmp/stdout" >&2 || true
  cat "$tmp/stderr" >&2 || true
  exit 1
fi

wait "$child_pid"
child_pid=

if ! grep -q '^PLAYBACK segment=0$' "$tmp/stdout"; then
  printf 'expected playback marker in live vox output\n' >&2
  cat "$tmp/stdout" >&2
  cat "$tmp/stderr" >&2
  exit 1
fi
if ! grep -q '^RX segment=0$' "$tmp/stdout"; then
  printf 'expected RX segment marker in live vox output\n' >&2
  cat "$tmp/stdout" >&2
  cat "$tmp/stderr" >&2
  exit 1
fi
