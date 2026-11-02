#!/bin/sh
# S3.29 close-out (lead, 2026-10-31): after w8_fix_i21.py + w8_fix_i21b.py on port/build/S3_29, the gates that show the default path is
# unchanged, one w8_lock hold each (29_gates.sh's main loop). Steps' lines: 29_out/g_gates.log; this script's own output 29_out/final.log.
# Run it inside one test slot: test-slot bash -c 'sh log/roadmap/S3/29_final.sh > log/roadmap/S3/29_out/final.log 2>&1'
cd "$(dirname "$0")/../../.."
sh log/roadmap/S3/29_gates.sh build_x64 build_w32 fh_s329 fh_w8 g2_stream_off g2_chain_off g2_chain_on boot_x64 boot_win32 \
   t_test_gl_raster t_test_static t_test_gl_wgl t_test_gl_wgl_selfcheck
~/bin/py -3.12 log/roadmap/S3/29_framehash.py cmp log/roadmap/S3/29_out/framehash_s329.json log/roadmap/S3/29_out/framehash_w8.json
echo S329_FINAL_DONE
