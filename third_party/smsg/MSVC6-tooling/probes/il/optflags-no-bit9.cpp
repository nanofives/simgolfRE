





int o1(int a) { return a * 3; }
#pragma optimize("agpstwy", on)
int o2(int a) { try { return a * 5; } catch (...) { return 0; } }
#pragma optimize("", off)
int o3(int a) { __try { return a * 7; } __except (1) { return 0; } }
#pragma optimize("", on)




