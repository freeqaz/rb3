#!/usr/bin/env bash
# Capture a deterministic native song-preview WAV (default highlight = first song,
# seeks to its preview/chorus offset). Usage: _capture_preview.sh OUT.wav [SECS]
set -u
OUT="${1:?out path}"; SECS="${2:-40}"
cd "$(dirname "$0")/../.."
PORT=$(python3 -c "import socket;s=socket.socket();s.bind(('127.0.0.1',0));print(s.getsockname()[1]);s.close()")
LOG="/tmp/rb3-cap-$PORT.log"
rm -f "$OUT"
RB3_GAME=1 RB3_HTTP=1 RB3_HTTP_PORT=$PORT MILO_HEADLESS=1 MILO_AUDIO=1 \
  MILO_AUDIO_BACKEND=null DC3_DUMP_AUDIO="$OUT" DC3_DUMP_SECONDS="$SECS" \
  RB3_DATA=orig-assets/extracted \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn" \
  timeout $((SECS+15)) native/build-native/rb3-native > "$LOG" 2>&1 &
PID=$!
# wait for song_select, then hold on the default song (no nav) so the preview fires
for i in $(seq 1 80); do
  sleep 1
  S=$(python3 -c "import http.client,json;c=http.client.HTTPConnection('127.0.0.1',$PORT,timeout=3);c.request('GET','/api/health');print(json.loads(c.getresponse().read())['data']['currentScreen'])" 2>/dev/null)
  [ "$S" = "song_select_screen" ] && break
done
# deterministic nav: 2 downs lands the highlight on the same song every run and
# triggers SongPreview (StartSongPreview fires on highlight change).
sleep 1
for k in 1 2; do
  python3 -c "import http.client;c=http.client.HTTPConnection('127.0.0.1',$PORT,timeout=5);c.request('POST','/api/input',body='down',headers={'Content-Type':'text/plain'});c.getresponse().read()" 2>/dev/null
  sleep 1.5
done
# let the dump fill to SECS of audio, then it auto-finalizes
sleep $((SECS+2))
kill $PID 2>/dev/null; sleep 1; kill -9 $PID 2>/dev/null
PREV=$(grep -a "PrepareSong\|Preview: Preparing" "$LOG" | head -1)
echo "captured $OUT ($(stat -c %s "$OUT" 2>/dev/null) bytes)"
