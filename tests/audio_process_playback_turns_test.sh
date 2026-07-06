#!/bin/sh
set -eu

if [ "$#" -lt 1 ]; then
  printf 'usage: audio_process_playback_turns_test.sh [runner ...] <probe>\n' >&2
  exit 2
fi

tmp=${TMPDIR:-/tmp}/cpkt-audio-process-playback-$$
mkdir -p "$tmp/bin"
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

cat >"$tmp/bin/aplay" <<'EOS'
#!/bin/sh
count=${CPKT_FAKE_APLAY_COUNT:?}
current=0
if [ -f "$count" ]; then
  current=$(cat "$count")
fi
current=$((current + 1))
printf '%s\n' "$current" >"$count"
cat >/dev/null
EOS
chmod +x "$tmp/bin/aplay"

CPKT_FAKE_APLAY_COUNT="$tmp/count" \
CPKT_AUDIO_PLAYBACK_PROBE_START_ONLY=1 \
PATH="$tmp/bin:$PATH" \
"$@"

if [ -f "$tmp/count" ]; then
  printf 'process playback start spawned aplay before audio was written\n' >&2
  exit 1
fi

CPKT_FAKE_APLAY_COUNT="$tmp/count" \
CPKT_AUDIO_PLAYBACK_PROBE_FRAMES=8000 \
PATH="$tmp/bin:$PATH" \
"$@"

actual=$(cat "$tmp/count")
if [ "$actual" != "2" ]; then
  printf 'expected two process playback turns, got %s\n' "$actual" >&2
  exit 1
fi

cat >"$tmp/bin/aplay" <<'EOS'
#!/bin/sh
cat >/dev/null
exit 7
EOS
chmod +x "$tmp/bin/aplay"

CPKT_AUDIO_PLAYBACK_PROBE_EXPECT_FAILURE=1 \
CPKT_FAKE_APLAY_COUNT="$tmp/count" \
PATH="$tmp/bin:$PATH" \
"$@"

cat >"$tmp/bin/aplay" <<'EOS'
#!/bin/sh
exit 7
EOS
chmod +x "$tmp/bin/aplay"

CPKT_AUDIO_PLAYBACK_PROBE_EXPECT_FAILURE=1 \
CPKT_AUDIO_PLAYBACK_PROBE_FRAMES=262144 \
CPKT_FAKE_APLAY_COUNT="$tmp/count" \
PATH="$tmp/bin:$PATH" \
"$@"
