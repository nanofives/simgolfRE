#!/bin/sh
# S3.29: the remaining gates after g2_all (stream replays with the walk on), one hold each in this order, then the census with the walk
# on (29_census.sh: one batch of 7 scenarios per hold).
cd "$(dirname "$0")/../../.."
sh log/roadmap/S3/29_gates.sh g2_chain_on fh_s329 fh_w8 g2_stream_off g2_chain_off segs_x64 segs_win32 boot_x64 boot_win32 t_test_gl_raster t_test_static
bash log/roadmap/S3/29_census.sh on
