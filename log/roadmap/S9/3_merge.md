# S9.3 merge lines (for the integrator)
port/src/sim/modules.inc, append one line:
    SG_MODULE(net)
After editing modules.inc touch port/src/sim/registry.cpp. New files: port/include/portsim/sim/net.h, port/src/sim/net.cpp,
net_basic.inc, net_queue.inc, net_poll.inc, net_control.inc, net_handle.inc, net_dialog.inc, net_testhost.inc, net_registry.inc,
port/test/test_net.py. No tick.cpp kDrops change: nothing in tick calls the module. Mirror: log/roadmap/S9/3_mirror.py
(port/build/S9_3_root, build dir port/build/S9_3).
