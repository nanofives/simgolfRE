#!/bin/sh
# Replay every pass of a gl_corpus.py corpus through glreplay.exe and print one line per step.
# Usage: re/tools/glexact/replay_all.sh log/p5/gl/corpus/<label>   (build first: re/tools/glexact/build.bat)
ROOT="$1"
HERE="$(cd "$(dirname "$0")" && pwd)"
pass=0; fail=0
for s in $(ls "$ROOT/snaps"); do
  out=$("$HERE/glreplay.exe" "$ROOT" "$s" 2>&1)
  blocks=$(echo "$out" | grep -o 'draw blocks in the pass: [0-9]*' | grep -o '[0-9]*$')
  changed=$(echo "$out" | grep -o 'game changed in the pass: [0-9]*' | grep -o '[0-9]*$')
  diff=$(echo "$out" | grep -o 'differing from the game.s result: [0-9]*' | grep -o '[0-9]*$')
  extra=$(echo "$out" | grep -o 'skipped: [0-9]*, unhandled calls: [0-9]*')
  if [ "$diff" = "0" ]; then v=EXACT; pass=$((pass+1)); else v="DIFF($diff px)"; fail=$((fail+1)); fi
  echo "$s | blocks $blocks | game changed $changed px | $v | $extra"
done
echo "exact $pass, different $fail"
