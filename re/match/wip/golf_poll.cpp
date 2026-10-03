// golf_clean.exe 0x0045ae70 (release). WIP 77%: VC6 here folds the final test on the first branch (both
// globals were just set to 0) and returns 0 there; the original jumps to the shared test instead. Tried
// `!a && !b` early return and a combined assignment. Not in the 100% suite.
// FLAGS golf_clean.exe: /O2
extern int g_4d3834, g_822b98, g_822b9c;
void f_483c70();                         // 0x483c70
void drain483c30();                      // 0x483c30 (golf_small6.cpp)

// MATCH: golf_clean.exe 0x0045ae70 ?poll45ae70@@YAHXZ
int poll45ae70()
{
    if (g_4d3834) {
        g_4d3834 = 0;
        f_483c70();
        g_822b9c = 0;
        g_822b98 = 0;
    } else {
        f_483c70();
        drain483c30();
    }
    if (g_822b98 || g_822b9c) {
        g_4d3834 = 1;
        return 1;
    }
    return 0;
}

