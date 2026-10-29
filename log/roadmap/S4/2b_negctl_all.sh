#!/usr/bin/env bash
# S4.2b: every live negative control of the four accepted steps on the consolidated core (log/roadmap/S4/2b_notes.md).
# Until wave 1 is committed the sound_replay copy runs through 2b_dev.py; afterwards replace `2b_dev.py sr` by
# `-m port.test.sound_replay`.
cd "$(dirname "$0")/../../.."
SR="$HOME/bin/py -3.12 log/roadmap/S4/2b_dev.py sr negctl"
O=log/roadmap/S4
$SR --step S4.2 --mode 1 --out $O/2b_negctl_s42_m1.tsv
$SR --step S4.4 --mode 1 --out $O/2b_negctl_s44_m1.tsv
$SR --step S4.4 --mode 2 --out $O/2b_negctl_s44_m2.tsv
$SR --step S4.6 --mode 1 --out $O/2b_negctl_s46_m1.tsv
$SR --step S4.6 --mode 2 --out $O/2b_negctl_s46_m2.tsv
for m in 3 4 5 6 7 8; do $SR --step S4.6 --mode $m --functions "Voice::update" --out $O/2b_negctl_s46_m$m.tsv; done
$SR --step S4.8 --family ds --mode 1 --out $O/2b_negctl_s48ds_m1.tsv
$SR --step S4.8 --family dev --mode 1 --out $O/2b_negctl_s48dev_m1.tsv
$SR --step S4.8 --family ds --mode 2 --out $O/2b_negctl_s48ds_m2.tsv
$SR --step S4.8 --family ds --mode 3 --out $O/2b_negctl_s48ds_m3.tsv
echo "negctl all done"
