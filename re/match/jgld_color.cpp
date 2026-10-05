// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll 16-bit color helpers (debug build): channel extraction for RGB565 / RGB555 pixels (the display's slot 45,
// +0xb4, returns 1 for 565) and a 16-bit span fill. Names are chosen here.
class Display {
public:
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual void v4();
    virtual void v5();
    virtual void v6();
    virtual void v7();
    virtual void v8();
    virtual void v9();
    virtual void v10();
    virtual void v11();
    virtual void v12();
    virtual void v13();
    virtual void v14();
    virtual void v15();
    virtual void v16();
    virtual void v17();
    virtual void v18();
    virtual void v19();
    virtual void v20();
    virtual void v21();
    virtual void v22();
    virtual void v23();
    virtual void v24();
    virtual void v25();
    virtual void v26();
    virtual void v27();
    virtual void v28();
    virtual void v29();
    virtual void v30();
    virtual void v31();
    virtual void v32();
    virtual void v33();
    virtual void v34();
    virtual void v35();
    virtual void v36();
    virtual void v37();
    virtual void v38();
    virtual void v39();
    virtual void v40();
    virtual void v41();
    virtual void v42();
    virtual void v43();
    virtual void v44();
    virtual int is565();                 // slot 45 (+0xb4)
};
extern Display* g_display;               // 0x10128420
// MATCH: jgld.dll 0x1000e440 ?redOf@@YAHH@Z
int redOf(int c)
{
    if (g_display->is565() == 1)
        return (c >> 8) & 0xf8;
    return (c >> 7) & 0xf8;
}
// MATCH: jgld.dll 0x1000e4c0 ?greenOf@@YAHH@Z
int greenOf(int c)
{
    if (g_display->is565() == 1)
        return (c >> 3) & 0xfc;
    return (c >> 2) & 0xf8;
}
// MATCH: jgld.dll 0x1000e540 ?blueOf@@YAHH@Z
int blueOf(int c)
{
    return (c << 3) & 0xf8;
}
// MATCH: jgld.dll 0x1000af30 ?fillWords@@YAXPAXGH@Z
void fillWords(void* dst, unsigned short c, int n)
{
    int i;
    unsigned int* p;
    unsigned int v;
    v = c << 16 | c;
    if (n & 1) {
        *(unsigned short*)dst = c;
        p = (unsigned int*)((char*)dst + 2);
    } else
        p = (unsigned int*)dst;
    for (i = n / 2; i > 0; i--) {
        *p = v;
        p++;
    }
}
