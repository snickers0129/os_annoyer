#!/usr/bin/env bash
# Generate the Annoyer system narration clips via ElevenLabs.
#
# Requires the ELEVENLABS_API_KEY env var, e.g.
#   ELEVENLABS_API_KEY=sk_... ./generate_sounds.sh
#
# Outputs to ./sounds/01/ ready to copy onto the SD card's /01/ folder.

set -euo pipefail

: "${ELEVENLABS_API_KEY:?Set ELEVENLABS_API_KEY first}"

VOICE_ID="${VOICE_ID:-pNInz6obpgDQGcFmaJgB}"   # Adam (deep narrator)
MODEL="${MODEL:-eleven_multilingual_v2}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTDIR="$SCRIPT_DIR/sounds/01"
mkdir -p "$OUTDIR"

# Voice settings tuned for "futuristic / epic / theatrical":
#   stability 0.35  -> more emotional variation
#   style 0.65      -> exaggerated delivery
#   speaker_boost   -> clearer presence
#
# Optional per-clip overrides:
#   $3 = style       (default 0.65 — exaggerated; lower for short single words
#                     where the AI tends to improvise/hallucinate)
#   $4 = stability   (default 0.35 — more variation; raise for predictability)
gen() {
  local fname="$1"
  local text="$2"
  local style="${3:-0.65}"
  local stab="${4:-0.35}"
  printf "  %-32s " "$fname"

  if [ -s "$OUTDIR/$fname" ]; then
    echo "skipped (already exists)"
    return 0
  fi

  local payload
  payload=$(printf '{"text":"%s","model_id":"%s","voice_settings":{"stability":%s,"similarity_boost":0.75,"style":%s,"use_speaker_boost":true}}' \
            "$text" "$MODEL" "$stab" "$style")

  local tmp="$OUTDIR/$fname.tmp"
  local code
  code=$(curl -sS -X POST \
    "https://api.elevenlabs.io/v1/text-to-speech/$VOICE_ID" \
    -H "xi-api-key: $ELEVENLABS_API_KEY" \
    -H "Content-Type: application/json" \
    -H "Accept: audio/mpeg" \
    -d "$payload" \
    --output "$tmp" \
    -w "%{http_code}")

  if [ "$code" != "200" ]; then
    echo "FAILED (HTTP $code)"
    cat "$tmp"; echo
    rm -f "$tmp"
    return 1
  fi

  # 25 ms fade-in to suppress startup click, plus 300 ms trailing silence
  # so the DFPlayer's PlayFinished event fires before the spoken audio
  # actually ends (otherwise the next play command cuts the tail off).
  ffmpeg -y -loglevel error -i "$tmp" \
    -af "afade=t=in:st=0:d=0.025,apad=pad_dur=0.3" \
    -codec:a libmp3lame -b:a 128k "$OUTDIR/$fname"
  rm -f "$tmp"
  echo "ok ($(stat -f%z "$OUTDIR/$fname") bytes)"
}

gen "001_select_mode.mp3"        "Select mode."
gen "002_sound_machine.mp3"      "Sound machine."
gen "003_noise_maker.mp3"        "Noise maker."
gen "004_daily_annoyer.mp3"      "Daily annoyer."
gen "005_hourly_annoyer.mp3"     "Hourly annoyer."
gen "007_select_sound.mp3"       "Select sound."
gen "008_starting_now.mp3"       "Starting now!"
gen "009_adjust_volume.mp3"      "Adjust volume."
gen "010_new_volume_saved.mp3"   "New volume saved."
gen "011_random_sound.mp3"       "Random sound."
# Short single words: lower style + higher stability so the AI doesn't improvise.
# (Default style=0.65 gave us "Sows quotes" on the lone word "Quotes.")
gen "012_quotes.mp3"             "Quotes." "0.1" "0.7"
gen "013_up.mp3"                 "Up."     "0.1" "0.7"
gen "014_down.mp3"               "Down."   "0.1" "0.7"

echo
echo "Done: $OUTDIR"
