#define ASM_BEGIN __asm {
#define STR(x) #x
#define XSTR(x) STR(x)
#pragma message("ASM_BEGIN ; /* z")
const char *u = STR(ASM_BEGIN); /* c */ int n;
const char *v = XSTR(ASM_BEGIN); /* d */ int q;
const char *w = "ASM_BEGIN" "ASM_BEGIN ; /*"; int r; /* e */ int t;
const unsigned short *w2 = L"ASM_BEGIN ; /*"; int r2; /* f */ int t2;
