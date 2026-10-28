#!/bin/sh
# S3.8 census: framegate --all with images for the base build (S3_8_base) and the S3.8 build (S3_8), then 8_linegate.py
R=C:/Users/maria/Desktop/Proyectos/SimGolf
B=$R/port/build/S3_head2/port/build
for v in ${VARIANTS:-base s38}; do
  if [ $v = base ]; then D=$B/S3_8_base/Release/portsim_c.dll; else D=$B/S3_8/Release/portsim_c.dll; fi
  SG_PORTSIM_DLL=$D ~/bin/py -3.12 $R/port/build/S3_8_src/port/test/framegate.py --all --jobs 3 --no-probe     --images $R/log/roadmap/S3/8_fg/all_$v --out $R/log/roadmap/S3/8_fg/census_$v.tsv $RESUME
done
~/bin/py -3.12 $R/log/roadmap/S3/8_linegate.py $R/log/roadmap/S3/8_fg/all_base $R/log/roadmap/S3/8_fg/all_s38 --out $R/log/roadmap/S3/8_linegate.tsv
