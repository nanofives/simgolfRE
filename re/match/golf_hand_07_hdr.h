#define V4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define V8(n) V4(n##a) V4(n##b)
#define V16(n) V8(n##c) V8(n##d)
#define V32(n) V16(n##e) V16(n##f)
