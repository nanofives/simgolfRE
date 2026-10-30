# S3.15: run 15_split_all.py over every scenario with FAIL rows in batches of 8, taking the wave 7 heavy token per batch
R=/c/Users/maria/Desktop/Proyectos/SimGolf
L=$R/log/roadmap/S3
cd $R/port/build/S3_15
export SG_PORTSIM_DLL=$PWD/port/build/x64/Release/portsim_c.dll SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
unset SG_GL_BACKEND
ALL=$(~/bin/py -3.12 -c "import csv;print(' '.join(sorted({r['scenario'] for r in csv.DictReader(open('../../../log/roadmap/merges/I14/census.tsv',encoding='utf-8'),delimiter='\t') if r['status']=='FAIL'})))")
set -- $ALL
while [ $# -gt 0 ]; do
  B=""; for i in 1 2 3 4 5 6 7 8; do [ $# -gt 0 ] && { B="$B,$1"; shift; }; done; B=${B#,}
  until mkdir $L/w7_heavy.lock 2>/dev/null; do echo "token busy: $(cat $L/w7_heavy.lock/owner.txt 2>/dev/null)"; sleep 150; done
  echo "S3.15 terrain split batch $B $(date '+%F %T')" > $L/w7_heavy.lock/owner.txt
  ~/bin/py -3.12 ../../../log/roadmap/S3/15_split_all.py --scenarios $B --out ../../../log/roadmap/S3/15_split_rows.tsv
  rm -rf $L/w7_heavy.lock
  sleep 20
done
echo SPLIT_ALL_DONE
