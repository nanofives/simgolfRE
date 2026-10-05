// FLAGS golf_clean.exe: /O2 /GX
// Global object arrays of golf_clean.exe: each array makes VC6 emit a construct initializer that calls the
// `eh vector constructor iterator' (??_L, /GX) with the element ctor and dtor. Element types are unnamed
// placeholders of the right size; the ctor/dtor addresses are relocated (not compared).
template <int N, int K> struct Elem { Elem(); ~Elem(); char pad[N]; };
Elem<0x84, 0> g_004f65c8[0x24f];     // initializer 0x00403f20
Elem<0x44, 1> g_0050fc20[0x24f];     // initializer 0x00403f80
Elem<0x2c, 2> g_00509688[0x24f];     // initializer 0x00403fe0
Elem<0x58, 3> g_004e9aa0[0x24f];     // initializer 0x00404040
Elem<0x58, 13> g_0081ca10[0xbe];     // initializer 0x0043cc10
Elem<0x6c, 15> g_0080d840[0x12c];     // initializer 0x00448160
Elem<0x2c, 16> g_00820f40[0x5];     // initializer 0x0044ad20
Elem<0x120, 17> g_0083b170[0x4];     // initializer 0x00487e10
// One declaration with several declarators makes one initializer for all of them (0x404c20...).
template <int N, int K> struct Obj { Obj(); ~Obj(); char pad[N]; };
Obj<0x2c, 0> g_0059ca58[0x50], g_0059b740, g_0056a250[0x10], g_00540950[0x2], g_005439e0[0x10], g_005787a0, g_0055e5b0[0x14], g_0058b650, g_005791c8, g_00567b08, g_005787d0, g_005a4100, g_005423e8;   // initializer 0x00404c20
Obj<0x2c, 1> g_00562368[0x20], g_0059d920[0x20], g_0058b358, g_0056a760, g_0056c7b8, g_005a48e8[0x4], g_0059abb8[0x10], g_0059bf98;   // initializer 0x00404e00
Obj<0x2c, 2> g_00540d98[0x20], g_0056a550[0xc], g_0059b050[0x28], g_0053ba48[0x8], g_005628e8[0x1b0], g_0059e7c0[0x78], g_0058a538[0x48];   // initializer 0x00404f50
Obj<0x2c, 3> g_00568258[0x14], g_00587ea8[0xe0], g_005678b8[0x8], g_0056a7b8[0x8], g_0055e930[0xc], g_00542e50[0x4];   // initializer 0x004050b0
Obj<0x2c, 4> g_00541a18[0x10], g_0058b390[0x10], g_00570cb8[0x60], g_005873a8[0x2a], g_00568fc8[0x1c], g_005a7c08[0x18], g_0059ba68[0x1e], g_00583a80[0x2c], g_00586f50[0x2];   // initializer 0x004051f0
Obj<0x2c, 5> g_005a8c90[0x28], g_0058b1a0[0xa], g_00571d40[0xf], g_0059b778[0x11], g_0053fd48[0x46], g_005a34f8[0x46];   // initializer 0x004053a0
Obj<0x2c, 6> g_0053c570[0x20], g_005a4138[0x20], g_00542f28[0x4], g_0059ab00[0x4], g_00520590[0x4], g_0059df20[0x32];   // initializer 0x004054e0
Obj<0x2c, 7> g_0056a948[0x1e], g_00567390[0x1e], g_005a53c8[0x24], g_0058b680[0x24], g_005612e8[0x27], g_0058ccb8[0x27];   // initializer 0x00405620
Obj<0x2c, 8> g_00561260[0x3], g_00541710[0x10], g_005791f8[0x10], g_0053df58[0x28], g_00541580[0x9], g_00567a20[0x5], g_0053de78[0x5], g_0053b7e0[0xe];   // initializer 0x00405760
Obj<0x2c, 9> g_005aaa30[0xdd40], g_005aa748;   // initializer 0x0043d460
// MATCH: golf_clean.exe 0x00403f20 _$E1
// MATCH: golf_clean.exe 0x00403f80 _$E6
// MATCH: golf_clean.exe 0x00403fe0 _$E11
// MATCH: golf_clean.exe 0x00404040 _$E16
// MATCH: golf_clean.exe 0x00404c20 _$E41
// MATCH: golf_clean.exe 0x00404e00 _$E46
// MATCH: golf_clean.exe 0x00404f50 _$E51
// MATCH: golf_clean.exe 0x004050b0 _$E56
// MATCH: golf_clean.exe 0x004051f0 _$E61
// MATCH: golf_clean.exe 0x004053a0 _$E66
// MATCH: golf_clean.exe 0x004054e0 _$E71
// MATCH: golf_clean.exe 0x00405620 _$E76
// MATCH: golf_clean.exe 0x00405760 _$E81
// MATCH: golf_clean.exe 0x0043cc10 _$E21
// MATCH: golf_clean.exe 0x0043d460 _$E86
// MATCH: golf_clean.exe 0x00448160 _$E26
// MATCH: golf_clean.exe 0x0044ad20 _$E31
// MATCH: golf_clean.exe 0x00487e10 _$E36
