

#pragma warning(disable: 4701)
int p1(int a) { return a * 3; }
#pragma optimize("", off)
int p2(int a) { return a * 5; }
#pragma optimize("", on)
#pragma warning(default: 4701)
#pragma optimize("t", on)
int p3(int a) { return a * 7; }








