# S3.12 builds: sh log/roadmap/S3/12_build.sh <tree under port/build> <arch> [...]  (log: log/roadmap/S3/12_build_<tree>_<arch>.log)
set -u
R=/c/Users/maria/Desktop/Proyectos/SimGolf
T=$1; shift
for A in "$@"; do
  L=$R/log/roadmap/S3/12_build_${T}_${A}.log
  (cd $R/port/build/$T && SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed' ~/bin/py -3.12 port/test/build.py --arch $A) > $L 2>&1
  echo "BUILD $T $A rc=$? $(tail -1 $L)"
done
