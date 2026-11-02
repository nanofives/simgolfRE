#!/bin/sh
# P3: one long-session run of the P3 mirror's simgolf_port.exe with test_app_present's _long_run arguments
# (course_season_6y, --mem-every 1000), one w8_lock hold per run, SG_P3_MEMX=1 (extra memory.tsv columns, two extra
# samples before the final one). Usage: sh log/roadmap/P3/run_long.sh <name> [ENV=VAL ...] [-- <extra app args>]
# Output: log/roadmap/P3/runs/<name>/{memory.tsv,session.json,stdout.txt,stderr.txt,meta.txt}
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/P3
O=$R/log/roadmap/P3/runs/$1
shift
ENVS=""
while [ $# -gt 0 ] && [ "$1" != "--" ]; do ENVS="$ENVS $1"; shift; done
[ "$1" = "--" ] && shift
EXTRA="$*"
mkdir -p $O
LOCKPY="$HOME/bin/py -3.12 $R/log/roadmap/S3/w8_lock.py"
$LOCKPY take P3 --what "P3 long run $(basename $O)" --need-gb 8 || exit 3
TMP=$(mktemp -d "$TEMP/p3_long_XXXX" 2>/dev/null || mktemp -d)
EXE=$M/port/build/x64/app/Release/simgolf_port.exe
T0=$(date +%s)
echo "start $(date '+%F %T') envs=[$ENVS] extra=[$EXTRA]" > $O/meta.txt
RP=$(cygpath -w $O/regions.tsv)
rm -f $O/regions.tsv
test-slot bash -c "cd $R && env SG_P3_MEMX=1 SG_P3_REGIONS='$RP' $ENVS '$EXE' --install '$R/original' --data '$TMP/data' --record-dir '$TMP/rec' \
  --live-trace '$R/port/corpus/traces/course_season_6y.trace' --mem-every 1000 $EXTRA > '$O/stdout.txt' 2> '$O/stderr.txt'"
rc=$?
$LOCKPY release P3
T1=$(date +%s)
cp "$TMP/rec/memory.tsv" "$TMP/rec/session.json" $O/ 2>/dev/null
cmp -s "$TMP/rec/framehash.bin" "$R/log/p2/p24/course_season_6y/c_a/framehash.bin" && fh=equal || fh=DIFFERENT
echo "end $(date '+%F %T') rc=$rc seconds=$((T1-T0)) framehash=$fh" >> $O/meta.txt
rm -rf "$TMP"
cat $O/meta.txt
