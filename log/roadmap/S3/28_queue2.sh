#!/bin/bash
# S3.28: after the census (28_queue.sh prints S328_CENSUS_DONE), the edge-tile recording s328_s2b, its export (kind 5 only) into
# port/corpus/gl/s328_s2b, then the gates (one S3.28 hold each; x64 rebuilt first: control 18 was added after the census build).
cd /c/Users/maria/Desktop/Proyectos/SimGolf
PY=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe
until grep -q S328_CENSUS_DONE log/roadmap/S3/28_out/queue.log 2>/dev/null; do sleep 20; done
while ~/bin/py -3.12 log/roadmap/S3/w8_lock.py status 2>&1 | grep -q "step=S3.28"; do sleep 10; done
SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 log/roadmap/S3/w8_lock.py run S3.28 --what "record s328_s2b (game boot, instance s3_w8)" -- \
  $PY log/roadmap/S3/28_gl_record.py --scenario cov_r_s2_screens --instance s3_w8 --label s328_s2b --inject-at course > log/roadmap/S3/28_out/rec_s328_s2b.log 2>&1
echo "record rc=$? $(tail -n 1 log/roadmap/S3/28_out/rec_s328_s2b.log | cut -c1-200)"
~/bin/py -3.12 re/tools/scenario_refs.py check --scenario cov_r_s2_screens --run s328_s2b 2>&1 | tail -n 1
(cd port/build/S3_28 && ~/bin/py -3.12 -m port.test.glcorpus export ../../../log/p5/gl/corpus/s328_s2b s328_s2b 5)
bash log/roadmap/S3/28_gates.sh build_x64 build_w32 pytest_x64 pytest_w32 segs_x64 segs_w32 boot_x64 boot_w32
echo S328_QUEUE2_DONE
