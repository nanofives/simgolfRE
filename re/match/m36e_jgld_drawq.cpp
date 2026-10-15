// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll Sprite::drawQ 0x100175d0 (extent 566 in re/match/boundaries.tsv), round 36 batch m36e. Class shapes as in
// jgld_blitY17.cpp (drawAlpha, same file jglsprite.cpp: same palette save/restore, lock/unlock and _RPTF0 reports).
// drawQ draws nothing: depth 8 reports, depth 16 reports per destination depth, any other depth reports; 0x17 for the
// unsupported cases. The depth-8 case holds a destination switch the original never dispatches: its bodies (reports at
// base+0x14 and +0x20, 0x10017693 / 0x100176c7) follow the case's `break` with no compare in front of them, and its
// switch temporary is the frame slot ebp-0x14 that no instruction touches (the live inner switch uses ebp-0x18).
// VC6 /Od drops that switch's dispatch and any arm only the dispatch reaches, but its registers were already
// allocated: the original's scratch-register order (ecx/edx in the reports at 0x1001769f and 0x100176d3) needs the
// dispatch on dst->format()[0] plus ONE more register-using statement between the base+0x14 and base+0x20 reports.
// A dropped `case 16:` arm `result = a;`, `result = dst->format()[1];`, or a dead `result = a;` / `a = b;` after the
// base+0x14 report's break all compile to the same 182 instructions: what stood there is NOT determined.
// Report strings are masked operands (0x1011d56c for the base+0xf/+0x14/+0x29 reports, 0x1011d52c for the others).
#define _DEBUG
#include <windows.h>
#include <crtdbg.h>
class Pal16;
class Surface {
public:
    virtual void s0(); virtual void s1(); virtual void s2(); virtual void s3(); virtual void s4(); virtual void s5(); virtual void s6(); virtual void s7(); virtual void s8(); virtual void s9();
    virtual void t10(); virtual void t11(); virtual void t12(); virtual void t13(); virtual void t14(); virtual void t15(); virtual void t16(); virtual void t17(); virtual void t18(); virtual void t19(); virtual void t20(); virtual void t21(); virtual void t22(); virtual void t23(); virtual void t24(); virtual void t25(); virtual void t26(); virtual void t27(); virtual void t28(); virtual void t29(); virtual void t30(); virtual void t31(); virtual void t32(); virtual void t33(); virtual void t34(); virtual void t35(); virtual void t36(); virtual void t37(); virtual void t38(); virtual void t39(); virtual void t40(); virtual void t41(); virtual void t42(); virtual void t43(); virtual void t44(); virtual void t45(); virtual void t46(); virtual void t47(); virtual void t48(); virtual void t49(); virtual void t50(); virtual void t51(); virtual void t52(); virtual void t53(); virtual void t54(); virtual void t55(); virtual void t56();
    virtual int* format();                   // slot 57 (+0xe4): [0] bits per pixel
};
extern const char s_msgA[];                  // 0x1011d56c (binary text, not reproduced)
extern const char s_msgB[];                  // 0x1011d52c (binary text, not reproduced)
class Sprite {
public:
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7();
    virtual int lock();                      // slot 8 (+0x20)
    virtual void unlock(int flag);           // slot 9 (+0x24)
    int drawQ(Surface* dst, int a, int b, int c, int d, Pal16* pal);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    int m_14, m_18, m_1c;
    int m_depth;                             // +0x20: source bits per pixel
};

// MATCH: jgld.dll 0x100175d0 ?drawQ@Sprite@@QAEHPAVSurface@@HHHHPAVPal16@@@Z
int Sprite::drawQ(Surface* dst, int a, int b, int c, int d, Pal16* pal)
{
    Pal16* saved;
    int result;
    result = 0;
    saved = m_palette;
    if (pal != 0)
        m_palette = pal;
    if (dst == 0)
        return 16;
    if (!lock())
        return 7;
    switch (m_depth) {
    case 8:
        // (line padding: with /ZI each report's __LINE__ is relative to the opening brace)
        //
        _RPTF0(_CRT_WARN, s_msgA);
        break;
        switch (dst->format()[0]) {
        case 8:
            //
            _RPTF0(_CRT_WARN, s_msgA);
            break;
        case 16: result = a; break;     // dropped by VC6 (see the header): its content is NOT determined
        default:
            //
            //
            //
            //
            //
            //
            //
            //
            _RPTF0(_CRT_WARN, s_msgB);
            result = 23;
        }
        break;
    case 16:
        switch (dst->format()[0]) {
        case 8:
            //
            //
            _RPTF0(_CRT_WARN, s_msgA);
            break;
        default:
            //
            //
            //
            _RPTF0(_CRT_WARN, s_msgB);
            result = 23;
        }
        break;
    default:
        //
        //
        //
        _RPTF0(_CRT_WARN, s_msgB);
        result = 23;
    }
    m_palette = saved;
    unlock(1);
    return result;
}
