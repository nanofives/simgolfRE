#!/bin/sh
# Wait for I21's queue.sh to log "[queue done]" (both suites finished), then run post_fix2.sh. Checks once a minute.
G=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I21
while ! grep -q '^\[queue done\]' $G/g_gates.log; do sleep 60; done
echo "[waiter] queue done seen $(date '+%F %T'), starting post_fix2.sh" >> $G/g_gates.log
sh $G/post_fix2.sh
echo "[waiter] post_fix2.sh finished $(date '+%F %T')" >> $G/g_gates.log
