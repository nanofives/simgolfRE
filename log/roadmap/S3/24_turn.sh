#!/bin/bash
# S3.24: wait for my turn (no older wait file, lock free), take the lock, run one command, release.
# usage: bash 24_turn.sh "<what>" <command...>
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
WHAT="$1"; shift
[ -f $S/w8_wait_S3.24 ] || echo "$(date '+%H:%M:%S') S3.24 wants: $WHAT" > $S/w8_wait_S3.24
mine=$(grep -oE '[0-9]{2}:[0-9]{2}(:[0-9]{2})?' $S/w8_wait_S3.24 | head -1)
while true; do
  older=0
  for f in $S/w8_wait_*; do
    [ "$f" = "$S/w8_wait_S3.24" ] && continue
    [ -f "$f" ] || continue
    t=$(grep -oE '[0-9]{2}:[0-9]{2}(:[0-9]{2})?' "$f" | head -1)
    [[ "$t" < "$mine" ]] && older=1
  done
  if [ $older = 0 ] && mkdir $S/w8_heavy.lock 2>/dev/null; then break; fi
  sleep 10
done
echo "S3.24 $WHAT $(date '+%H:%M')" > $S/w8_heavy.lock/owner.txt
rm -f $S/w8_wait_S3.24
until ~/bin/py -3.12 $S/w8_commit_free.py; do rm -rf $S/w8_heavy.lock; sleep 300; mkdir $S/w8_heavy.lock 2>/dev/null || continue; echo "S3.24 $WHAT $(date '+%H:%M')" > $S/w8_heavy.lock/owner.txt; done
echo "[turn] start $(date '+%H:%M:%S') $WHAT"
"$@"
rc=$?
grep -q "S3.24" $S/w8_heavy.lock/owner.txt && rm -rf $S/w8_heavy.lock
echo "[turn] end $(date '+%H:%M:%S') rc=$rc"
exit $rc
