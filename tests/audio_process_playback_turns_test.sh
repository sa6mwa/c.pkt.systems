#!/bin/sh
set -eu

probe=$1
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
PATH="$tmp/bin:$PATH" \
"$probe"

actual=$(cat "$tmp/count")
if [ "$actual" != "2" ]; then
  printf 'expected two process playback turns, got %s\n' "$actual" >&2
  exit 1
fi
