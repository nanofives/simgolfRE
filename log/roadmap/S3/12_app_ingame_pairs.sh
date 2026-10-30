#!/bin/sh
# S3.12: test_app_ingame on the reference REF (default port/build/S3_12_e179 = plain e179bd24) and wave 6 (S3_12), interleaved, N pairs
# (default 2). Each run's output goes to log/roadmap/S3/12_out/$PAIRS/<tree>_<k>.txt (default pairs_e179) with the machine load before
# it, its facts to .../facts_<tree>_<k>/ (SG_APP_FACTS_DIR on wave 6; the reference writes log/roadmap/S7/8_app_ingame_facts.json, which
# is copied there and restored from the byte copy).
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
O=$R/log/roadmap/S3/12_out/${PAIRS:-pairs_e179}
mkdir -p $O
for k in $(seq 1 ${N:-2}); do
  for T in ${REF:-S3_12_e179} S3_12; do
    (cd $R/port/build/$T && TREE=$T . $R/log/roadmap/S3/12_env.sh &&
     load=$(powershell -NoProfile -Command "[int](Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average") &&
     mkdir -p $O/facts_${T}_$k &&
     echo "== $T run $k $(date '+%F %T') load_before=${load}%" > $O/${T}_$k.txt &&
     SG_APP_FACTS_DIR=$O/facts_${T}_$k ~/bin/py -3.12 -m pytest port/test/test_app_ingame.py -m "" -q -p no:cacheprovider >> $O/${T}_$k.txt 2>&1)
    f=$R/log/roadmap/S7/8_app_ingame_facts.json
    c=$R/log/roadmap/S3/12_out/facts_head/8_app_ingame_facts.json
    cmp -s $c $f || { cp $f $O/facts_${T}_$k/8_app_ingame_facts.from_S7.json; cp -p $c $f; echo "restored $f" >> $O/${T}_$k.txt; }
    echo "[$T $k] $(tail -1 $O/${T}_$k.txt | grep -v restored; grep -v '^restored' $O/${T}_$k.txt | tail -1)" >> $O/pairs.log
  done
done
echo "[pairs done]" >> $O/pairs.log
