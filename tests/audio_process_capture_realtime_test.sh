#!/bin/sh
set -eu

if [ "$#" -lt 1 ]; then
  printf 'usage: audio_process_capture_realtime_test.sh [runner ...] <probe>\n' >&2
  exit 2
fi

tmp=${TMPDIR:-/tmp}/cpkt-audio-process-capture-$$
mkdir -p "$tmp/bin"
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

cat >"$tmp/bin/arecord" <<'EOS'
#!/bin/sh
state=${CPKT_FAKE_ARECORD_STATE:?}
generation=1
if [ -f "$state" ]; then
  generation=$(($(cat "$state") + 1))
fi
printf '%s\n' "$generation" >"$state"
export CPKT_FAKE_ARECORD_GENERATION=$generation
exec python3 -c '
import os
import signal
import sys
import time

running = True

def stop(signum, frame):
    global running
    running = False

signal.signal(signal.SIGTERM, stop)
sample = int(os.environ["CPKT_FAKE_ARECORD_GENERATION"]) * 1000
chunk = int(sample).to_bytes(2, "little", signed=True) * 256
while running:
    try:
        sys.stdout.buffer.write(chunk)
        sys.stdout.buffer.flush()
    except BrokenPipeError:
        break
    time.sleep(0.02)
'
EOS
chmod +x "$tmp/bin/arecord"

CPKT_FAKE_ARECORD_STATE="$tmp/generation" \
PATH="$tmp/bin:$PATH" \
"$@"

actual=$(cat "$tmp/generation")
if [ "$actual" != "1" ]; then
  printf 'expected one stable process capture generation, got %s\n' "$actual" >&2
  exit 1
fi

cat >"$tmp/bin/arecord" <<'EOS'
#!/bin/sh
exit 0
EOS
chmod +x "$tmp/bin/arecord"

CPKT_AUDIO_EXPECT_CAPTURE_EOF=1 \
PATH="$tmp/bin:$PATH" \
"$@"
