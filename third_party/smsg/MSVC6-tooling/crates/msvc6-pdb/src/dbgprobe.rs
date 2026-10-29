pub const NAME: &str = "dbgprobe.c";
pub const EXE: &str = "dbgprobe.exe";
pub const SOURCE: &str = r#"#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned __int64 U64;

#define SYMOPT_UNDNAME 0x00000002
#define SYMOPT_LOAD_LINES 0x00000010
#define SYMOPT_FAIL_CRITICAL_ERRORS 0x00000200

#define DBHHEADER_DEBUGDIRS 0x1

typedef struct {
    ULONG SizeOfStruct;
    ULONG TypeIndex;
    U64 Reserved[2];
    ULONG Index;
    ULONG Size;
    U64 ModBase;
    ULONG Flags;
    U64 Value;
    U64 Address;
    ULONG Register;
    ULONG Scope;
    ULONG Tag;
    ULONG NameLen;
    ULONG MaxNameLen;
    CHAR Name[1];
} SYMINFO;

typedef struct {
    DWORD SizeOfStruct;
    PVOID Key;
    DWORD LineNumber;
    PCHAR FileName;
    U64 Address;
} LINE64;

typedef struct {
    DWORD SizeOfStruct;
    U64 BaseOfImage;
    DWORD ImageSize;
    DWORD TimeDateStamp;
    DWORD CheckSum;
    DWORD NumSyms;
    DWORD SymType;
    CHAR ModuleName[32];
    CHAR ImageName[256];
    CHAR LoadedImageName[256];
    CHAR LoadedPdbName[256];
    DWORD CVSig;
    CHAR CVData[MAX_PATH * 3];
    DWORD PdbSig;
    GUID PdbSig70;
    DWORD PdbAge;
    BOOL PdbUnmatched;
    BOOL DbgUnmatched;
    BOOL LineNumbers;
    BOOL GlobalSymbols;
    BOOL TypeInfo;
    BOOL SourceIndexed;
    BOOL Publics;
    DWORD MachineType;
    DWORD Reserved;
} MODINFO64;

typedef struct {
    U64 InstructionOffset;
    U64 ReturnOffset;
    U64 FrameOffset;
    U64 StackOffset;
    U64 BackingStoreOffset;
    U64 FuncTableEntry;
    U64 Params[4];
    U64 Reserved[5];
    BOOL Virtual;
    ULONG Reserved2;
} STACKFRAME64;

typedef struct {
    DWORD ssize;
    DWORD ssig;
    PVOID data;
    DWORD size;
    DWORD flags;
} LOADDATA;

typedef struct {
    U64 ModBase;
    PCHAR FileName;
} SRCFILE;

typedef struct {
    DWORD SizeOfStruct;
    PVOID Key;
    U64 ModBase;
    CHAR Obj[MAX_PATH + 1];
    CHAR FileName[MAX_PATH + 1];
    DWORD LineNumber;
    U64 Address;
} SRCLINE;

typedef struct {
    ULONG Count;
    ULONG Start;
    ULONG ChildId[1];
} CHILDREN;

#define TI_GET_SYMTAG 0
#define TI_GET_SYMNAME 1
#define TI_GET_LENGTH 2
#define TI_GET_TYPE 3
#define TI_GET_BASETYPE 5
#define TI_FINDCHILDREN 7
#define TI_GET_COUNT 12
#define TI_GET_CHILDRENCOUNT 13
#define TI_GET_CLASSPARENTID 18

#define TAG_FUNCTION 5
#define TAG_UDT 11
#define TAG_ENUM 12
#define TAG_FUNCTIONTYPE 13
#define TAG_POINTER 14
#define TAG_ARRAY 15
#define TAG_BASE 16
#define TAG_TYPEDEF 17
#define TAG_FUNCTIONARG 20
#define TAG_MAX 64

typedef BOOL(CALLBACK *ENUMSYMCB)(SYMINFO *, ULONG, PVOID);
typedef BOOL(CALLBACK *ENUMSRCCB)(SRCFILE *, PVOID);
typedef BOOL(CALLBACK *ENUMLINECB)(SRCLINE *, PVOID);

typedef DWORD(WINAPI *SETOPTIONS)(DWORD);
typedef BOOL(WINAPI *INITIALIZE)(HANDLE, PCSTR, BOOL);
typedef BOOL(WINAPI *CLEANUP)(HANDLE);
typedef U64(WINAPI *LOADMODULEEX)(HANDLE, HANDLE, PCSTR, PCSTR, U64, DWORD, LOADDATA *, DWORD);
typedef BOOL(WINAPI *GETMODULEINFO64)(HANDLE, U64, MODINFO64 *);
typedef BOOL(WINAPI *ENUMSYMBOLS)(HANDLE, U64, PCSTR, ENUMSYMCB, PVOID);
typedef BOOL(WINAPI *ENUMTYPES)(HANDLE, U64, ENUMSYMCB, PVOID);
typedef BOOL(WINAPI *ENUMSOURCEFILES)(HANDLE, U64, PCSTR, ENUMSRCCB, PVOID);
typedef BOOL(WINAPI *ENUMLINES)(HANDLE, U64, PCSTR, PCSTR, ENUMLINECB, PVOID);
typedef BOOL(WINAPI *FROMADDR)(HANDLE, U64, U64 *, SYMINFO *);
typedef BOOL(WINAPI *LINEFROMADDR64)(HANDLE, U64, DWORD *, LINE64 *);
typedef BOOL(WINAPI *GETTYPEINFO)(HANDLE, U64, ULONG, int, PVOID);
typedef BOOL(WINAPI *TYPEFROMNAME)(HANDLE, U64, PCSTR, SYMINFO *);
typedef BOOL(WINAPI *SETCONTEXT)(HANDLE, STACKFRAME64 *, PVOID);
typedef DWORD(WINAPI *UNDECORATE)(PCSTR, PSTR, DWORD, DWORD);

static SETOPTIONS pSetOptions;
static INITIALIZE pInitialize;
static CLEANUP pCleanup;
static LOADMODULEEX pLoadModuleEx;
static GETMODULEINFO64 pGetModuleInfo64;
static ENUMSYMBOLS pEnumSymbols;
static ENUMTYPES pEnumTypes;
static ENUMSOURCEFILES pEnumSourceFiles;
static ENUMLINES pEnumLines;
static FROMADDR pFromAddr;
static LINEFROMADDR64 pLineFromAddr64;
static GETTYPEINFO pGetTypeInfo;
static TYPEFROMNAME pTypeFromName;
static SETCONTEXT pSetContext;
static UNDECORATE pUndecorate;

static HANDLE g_proc = (HANDLE)0x5157;
static U64 g_base;

static FARPROC bindFn(HMODULE dll, const char *name, int required)
{
    FARPROC f = GetProcAddress(dll, name);
    if (!f && required) {
        printf("BIND\t%s\tmissing\n", name);
        exit(3);
    }
    return f;
}

static int bindAll(void)
{
    HMODULE dll = LoadLibraryA("dbghelp.dll");
    char path[MAX_PATH];
    if (!dll) {
        printf("BIND\tdbghelp.dll\tnot loaded\terr=%lu\n", GetLastError());
        return 0;
    }
    path[0] = 0;
    GetModuleFileNameA(dll, path, sizeof(path));
    printf("BIND\tdbghelp.dll\t%s\n", path);
    pSetOptions = (SETOPTIONS)bindFn(dll, "SymSetOptions", 1);
    pInitialize = (INITIALIZE)bindFn(dll, "SymInitialize", 1);
    pCleanup = (CLEANUP)bindFn(dll, "SymCleanup", 1);
    pLoadModuleEx = (LOADMODULEEX)bindFn(dll, "SymLoadModuleEx", 1);
    pGetModuleInfo64 = (GETMODULEINFO64)bindFn(dll, "SymGetModuleInfo64", 1);
    pEnumSymbols = (ENUMSYMBOLS)bindFn(dll, "SymEnumSymbols", 1);
    pEnumTypes = (ENUMTYPES)bindFn(dll, "SymEnumTypes", 0);
    pEnumSourceFiles = (ENUMSOURCEFILES)bindFn(dll, "SymEnumSourceFiles", 0);
    pEnumLines = (ENUMLINES)bindFn(dll, "SymEnumLines", 0);
    pFromAddr = (FROMADDR)bindFn(dll, "SymFromAddr", 1);
    pLineFromAddr64 = (LINEFROMADDR64)bindFn(dll, "SymGetLineFromAddr64", 1);
    pGetTypeInfo = (GETTYPEINFO)bindFn(dll, "SymGetTypeInfo", 1);
    pTypeFromName = (TYPEFROMNAME)bindFn(dll, "SymGetTypeFromName", 0);
    pSetContext = (SETCONTEXT)bindFn(dll, "SymSetContext", 0);
    pUndecorate = (UNDECORATE)bindFn(dll, "UnDecorateSymbolName", 0);
    return 1;
}

static const char *symTypeName(DWORD t)
{
    static const char *names[] = {"SymNone", "SymCoff", "SymCv",       "SymPdb",    "SymExport",
                                  "SymDeferred", "SymSym", "SymDia", "SymVirtual"};
    return t < sizeof(names) / sizeof(names[0]) ? names[t] : "?";
}

static void printModule(void)
{
    static const DWORD sizes[] = {sizeof(MODINFO64), 1672, 1664, 584};
    MODINFO64 m;
    int i;
    for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++) {
        memset(&m, 0, sizeof(m));
        m.SizeOfStruct = sizes[i];
        if (pGetModuleInfo64(g_proc, g_base, &m))
            break;
    }
    if (i == (int)(sizeof(sizes) / sizeof(sizes[0]))) {
        printf("MODULE\tfail\terr=%lu\n", GetLastError());
        return;
    }
    printf("MODULE\tstruct=%lu\tbase=%I64x\tsize=%lx\ttimestamp=%lx\tSymType=%lu(%s)\tNumSyms=%lu\t"
           "ModuleName=%s\tImageName=%s\tLoadedImageName=%s\tLoadedPdbName=%s\tCVSig=%lx\t"
           "CVData=%s\tPdbSig=%lx\tPdbSig70={%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\t"
           "PdbAge=%lu\tPdbUnmatched=%d\tDbgUnmatched=%d\tLineNumbers=%d\t"
           "GlobalSymbols=%d\tTypeInfo=%d\tPublics=%d\n",
           m.SizeOfStruct, m.BaseOfImage, m.ImageSize, m.TimeDateStamp, m.SymType,
           symTypeName(m.SymType), m.NumSyms, m.ModuleName, m.ImageName, m.LoadedImageName,
           m.LoadedPdbName, m.CVSig, m.CVData, m.PdbSig, m.PdbSig70.Data1, m.PdbSig70.Data2,
           m.PdbSig70.Data3, m.PdbSig70.Data4[0], m.PdbSig70.Data4[1], m.PdbSig70.Data4[2],
           m.PdbSig70.Data4[3], m.PdbSig70.Data4[4], m.PdbSig70.Data4[5], m.PdbSig70.Data4[6],
           m.PdbSig70.Data4[7], m.PdbAge, m.PdbUnmatched, m.DbgUnmatched, m.LineNumbers,
           m.GlobalSymbols, m.TypeInfo, m.SizeOfStruct >= 1672 ? m.Publics : -1);
}

static unsigned long g_tags[TAG_MAX + 1];
static int g_rows;

static BOOL CALLBACK countSym(SYMINFO *s, ULONG size, PVOID ctx)
{
    (void)size;
    (void)ctx;
    g_tags[s->Tag < TAG_MAX ? s->Tag : TAG_MAX]++;
    if (g_rows)
        printf("SYM\t%I64x\t%lu\t%lu\t%lx\t%s\n", s->Address, s->Size, s->Tag, s->Flags, s->Name);
    return TRUE;
}

static void printTags(const char *row)
{
    int t;
    unsigned long total = 0;
    for (t = 0; t <= TAG_MAX; t++) {
        if (g_tags[t])
            printf("%s\t%d\t%lu\n", row, t, g_tags[t]);
        total += g_tags[t];
    }
    printf("%s\ttotal\t%lu\n", row, total);
    memset(g_tags, 0, sizeof(g_tags));
}

static unsigned long g_files;

static BOOL CALLBACK countFile(SRCFILE *f, PVOID ctx)
{
    (void)ctx;
    g_files++;
    if (g_rows)
        printf("FILE\t%s\n", f->FileName);
    return TRUE;
}

static unsigned long g_lines;

static BOOL CALLBACK printLine(SRCLINE *l, PVOID ctx)
{
    (void)ctx;
    g_lines++;
    printf("LINE\t%I64x\t%s\t%lu\n", l->Address, l->FileName, l->LineNumber);
    return TRUE;
}

static void normalise(const char *in, char *out, int cap)
{
    static const char *drop[] = {"const",      "volatile",   "struct ",    "class ",
                                 "union ",     "enum ",      "__cdecl",    "__stdcall",
                                 "__thiscall", "__fastcall", "__ptr64"};
    static const char *fold[][2] = {{"unsignedlonglongint", "unsigned__int64"},
                                    {"longlongint", "__int64"},
                                    {"unsignedshortint", "unsignedshort"},
                                    {"unsignedlongint", "unsignedlong"},
                                    {"shortint", "short"},
                                    {"longint", "long"},
                                    {"signedchar", "char"},
                                    {"BOOL08", "bool"},
                                    {"(void)", "()"}};
    char buf[2048];
    int i, j, again = 1;
    lstrcpynA(buf, in, sizeof(buf));
    while (again) {
        again = 0;
        for (i = 0; i < (int)(sizeof(drop) / sizeof(drop[0])); i++) {
            char *p = strstr(buf, drop[i]);
            if (p) {
                memmove(p, p + strlen(drop[i]), strlen(p + strlen(drop[i])) + 1);
                again = 1;
            }
        }
    }
    for (i = j = 0; buf[i] && j < cap - 1; i++)
        if (buf[i] != ' ')
            out[j++] = buf[i] == '&' ? '*' : buf[i];
    out[j] = 0;
    for (i = 0; i < (int)(sizeof(fold) / sizeof(fold[0])); i++) {
        size_t from = strlen(fold[i][0]), to = strlen(fold[i][1]);
        char *p = out;
        while ((p = strstr(p, fold[i][0])) != NULL) {
            if (p > out && isalnum((unsigned char)fold[i][0][0]) &&
                (isalnum((unsigned char)p[-1]) || p[-1] == '_')) {
                p++;
                continue;
            }
            memmove(p + to, p + from, strlen(p + from) + 1);
            memcpy(p, fold[i][1], to);
            p += to;
        }
    }
}

static ULONG typeInfo(ULONG id, int what)
{
    ULONG v = 0;
    if (!pGetTypeInfo(g_proc, g_base, id, what, &v))
        return (ULONG)-1;
    return v;
}

static int typeSymName(ULONG id, char *out, int cap)
{
    WCHAR *w = NULL;
    out[0] = 0;
    if (pGetTypeInfo(g_proc, g_base, id, TI_GET_SYMNAME, &w) && w)
        WideCharToMultiByte(CP_ACP, 0, w, -1, out, cap, NULL, NULL);
    return out[0] != 0;
}

static const char *baseTypeName(ULONG bt, U64 len)
{
    switch (bt) {
    case 1:
        return "void";
    case 2:
        return "char";
    case 3:
        return "wchar_t";
    case 6:
        return len == 1 ? "char" : len == 2 ? "short" : len == 8 ? "__int64" : "int";
    case 7:
        return len == 1   ? "unsigned char"
               : len == 2 ? "unsigned short"
               : len == 8 ? "unsigned __int64"
                          : "unsigned int";
    case 8:
        return len == 4 ? "float" : len == 8 ? "double" : "long double";
    case 10:
        return "bool";
    case 13:
        return "long";
    case 14:
        return "unsigned long";
    default:
        return "?base";
    }
}

static void typeName(ULONG id, char *out, int cap, int depth)
{
    ULONG tag = typeInfo(id, TI_GET_SYMTAG);
    char inner[512];
    U64 len = 0;
    out[0] = 0;
    if (depth > 8) {
        lstrcpynA(out, "...", cap);
        return;
    }
    switch (tag) {
    case TAG_BASE:
        if (!typeSymName(id, out, cap)) {
            pGetTypeInfo(g_proc, g_base, id, TI_GET_LENGTH, &len);
            lstrcpynA(out, baseTypeName(typeInfo(id, TI_GET_BASETYPE), len), cap);
        }
        break;
    case TAG_POINTER:
        typeName(typeInfo(id, TI_GET_TYPE), inner, sizeof(inner), depth + 1);
        _snprintf(out, cap, "%s*", inner);
        break;
    case TAG_ARRAY:
        typeName(typeInfo(id, TI_GET_TYPE), inner, sizeof(inner), depth + 1);
        _snprintf(out, cap, "%s[%lu]", inner, typeInfo(id, TI_GET_COUNT));
        break;
    case TAG_UDT:
    case TAG_ENUM:
    case TAG_TYPEDEF:
        typeSymName(id, out, cap);
        break;
    case TAG_FUNCTIONTYPE:
        lstrcpynA(out, "fn", cap);
        break;
    case (ULONG)-1:
        lstrcpynA(out, "...", cap);
        break;
    default:
        _snprintf(out, cap, "?tag%lu", tag);
        break;
    }
    out[cap - 1] = 0;
}

static void functionSig(ULONG fnType, char *out, int cap, char *cls, int clsCap)
{
    char raw[2048], ret[512], arg[512];
    ULONG n, i, parent;
    CHILDREN *kids;
    int at;
    out[0] = 0;
    cls[0] = 0;
    if (typeInfo(fnType, TI_GET_SYMTAG) != TAG_FUNCTIONTYPE) {
        _snprintf(out, cap, "?tag%lu", typeInfo(fnType, TI_GET_SYMTAG));
        out[cap - 1] = 0;
        return;
    }
    typeName(typeInfo(fnType, TI_GET_TYPE), ret, sizeof(ret), 0);
    parent = typeInfo(fnType, TI_GET_CLASSPARENTID);
    if (parent != (ULONG)-1 && parent != 0)
        typeSymName(parent, cls, clsCap);
    n = typeInfo(fnType, TI_GET_CHILDRENCOUNT);
    if (n == (ULONG)-1)
        n = 0;
    at = _snprintf(raw, sizeof(raw), "%s(", ret);
    kids = (CHILDREN *)calloc(1, sizeof(CHILDREN) + n * sizeof(ULONG));
    kids->Count = n;
    if (n && pGetTypeInfo(g_proc, g_base, fnType, TI_FINDCHILDREN, kids)) {
        for (i = 0; i < n && at > 0 && at < (int)sizeof(raw) - 1; i++) {
            ULONG t = kids->ChildId[i];
            if (typeInfo(t, TI_GET_SYMTAG) == TAG_FUNCTIONARG)
                t = typeInfo(t, TI_GET_TYPE);
            typeName(t, arg, sizeof(arg), 0);
            at += _snprintf(raw + at, sizeof(raw) - at, "%s%s", i ? "," : "", arg);
        }
    }
    free(kids);
    if (at > 0 && at < (int)sizeof(raw) - 1)
        _snprintf(raw + at, sizeof(raw) - at, ")");
    raw[sizeof(raw) - 1] = 0;
    normalise(raw, out, cap);
}

static void mangledSig(const char *mangled, char *out, int cap)
{
    char full[2048], name[1024], ret[1024], args[1024], joined[2048];
    char *at;
    out[0] = 0;
    if (!pUndecorate || mangled[0] != '?')
        return;
    if (!pUndecorate(mangled, full, sizeof(full), 0x2e2) ||
        !pUndecorate(mangled, name, sizeof(name), 0x1000))
        return;
    at = strstr(full, name);
    if (!at || at[strlen(name)] != '(')
        return;
    lstrcpynA(ret, full, (int)(at - full) + 1);
    lstrcpynA(args, at + strlen(name), sizeof(args));
    at = strrchr(args, ')');
    if (at)
        at[1] = 0;
    _snprintf(joined, sizeof(joined), "%s%s", ret[0] ? ret : "void", args);
    joined[sizeof(joined) - 1] = 0;
    normalise(joined, out, cap);
}

static int g_localCount;
static char g_locals[2048];
static int g_localsLen;

static BOOL CALLBACK collectLocal(SYMINFO *s, ULONG size, PVOID ctx)
{
    int room = (int)sizeof(g_locals) - 1 - g_localsLen;
    (void)size;
    (void)ctx;
    g_localCount++;
    if (room > 1) {
        int n = _snprintf(g_locals + g_localsLen, (size_t)room, "%s%s", g_localsLen ? "," : "",
                          s->Name);
        g_localsLen += (n < 0 || n > room) ? room : n;
        g_locals[g_localsLen] = 0;
    }
    return TRUE;
}

static void query(U64 addr, const char *mangled, int locals)
{
    char buf[sizeof(SYMINFO) + 1024];
    SYMINFO *s = (SYMINFO *)buf;
    U64 disp = 0;
    DWORD ldisp = 0;
    LINE64 line;
    char sig[2048], cls[512], und[2048];
    int haveSym, haveLine;

    memset(buf, 0, sizeof(buf));
    s->SizeOfStruct = sizeof(SYMINFO);
    s->MaxNameLen = 1024;
    haveSym = pFromAddr(g_proc, addr, &disp, s);
    memset(&line, 0, sizeof(line));
    line.SizeOfStruct = sizeof(line);
    haveLine = pLineFromAddr64(g_proc, addr, &ldisp, &line);

    printf("Q\t%I64x", addr);
    if (haveSym)
        printf("\tsym=%s\tdisp=%I64u\tsize=%lu\ttag=%lu", s->Name, disp, s->Size, s->Tag);
    else
        printf("\tsym=-\tdisp=-\tsize=-\ttag=-");
    if (haveLine)
        printf("\tline=%s:%lu\tldisp=%lu", line.FileName, line.LineNumber, ldisp);
    else
        printf("\tline=-\tldisp=-");

    sig[0] = cls[0] = und[0] = 0;
    if (haveSym && s->Tag == TAG_FUNCTION) {
        ULONG fnType = s->TypeIndex ? s->TypeIndex : typeInfo(s->Index, TI_GET_TYPE);
        printf("\tti=%lu\tidx=%lu", s->TypeIndex, s->Index);
        if (fnType != (ULONG)-1 && fnType)
            functionSig(fnType, sig, sizeof(sig), cls, sizeof(cls));
    }
    if (mangled && *mangled)
        mangledSig(mangled, und, sizeof(und));
    printf("\tsig=%s\tclass=%s\tund=%s\tsigmatch=%s", sig[0] ? sig : "-", cls[0] ? cls : "-",
           und[0] ? und : "-", (!sig[0] || !und[0]) ? "-" : strcmp(sig, und) == 0 ? "yes" : "no");

    if (locals && pSetContext && haveSym && s->Tag == TAG_FUNCTION) {
        STACKFRAME64 f;
        BOOL ctx;
        memset(&f, 0, sizeof(f));
        f.InstructionOffset = addr;
        g_localCount = 0;
        g_localsLen = 0;
        g_locals[0] = 0;
        ctx = pSetContext(g_proc, &f, NULL);
        if (ctx)
            pEnumSymbols(g_proc, 0, "*", collectLocal, NULL);
        printf("\tlocals=%s%d\t%s", ctx ? "" : "noctx:", g_localCount,
               g_locals[0] ? g_locals : "-");
    }
    printf("\n");
}

static void runQueries(const char *path, int locals)
{
    FILE *f = fopen(path, "r");
    char text[4096];
    if (!f) {
        printf("QUERIES\t%s\tcannot open\n", path);
        return;
    }
    while (fgets(text, sizeof(text), f)) {
        char *p = text, *end;
        U64 addr;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || !*p)
            continue;
        addr = strtoul(p, &end, 16);
        while (*end == ' ' || *end == '\t')
            end++;
        p = end;
        while (*end && *end != '\n' && *end != '\r' && *end != ' ' && *end != '\t')
            end++;
        *end = 0;
        query(addr, p, locals);
    }
    fclose(f);
}

static void lookupType(const char *name)
{
    char buf[sizeof(SYMINFO) + 512];
    SYMINFO *s = (SYMINFO *)buf;
    U64 len = 0;
    memset(buf, 0, sizeof(buf));
    s->SizeOfStruct = sizeof(SYMINFO);
    s->MaxNameLen = 512;
    if (!pTypeFromName || !pTypeFromName(g_proc, g_base, name, s)) {
        printf("TYPE\t%s\tnotfound\n", name);
        return;
    }
    pGetTypeInfo(g_proc, g_base, s->TypeIndex, TI_GET_LENGTH, &len);
    printf("TYPE\t%s\tfound\ttag=%lu\tlen=%I64u\tchildren=%ld\n", name, s->Tag, len,
           (long)typeInfo(s->TypeIndex, TI_GET_CHILDRENCOUNT));
}

static U64 hex(const char *s)
{
    return strtoul(s, NULL, 16);
}

int main(int argc, char **argv)
{
    const char *image = NULL, *module = NULL, *path = NULL, *queries = NULL;
    const char *cvName = NULL;
    DWORD cvSig = 0, cvAge = 0;
    DWORD options = SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS;
    U64 base = 0;
    DWORD size = 0;
    int locals = 0, enumSyms = 0, enumTypes = 0, enumFiles = 0, enumLines = 0, cleanup = 0;
    const char *types[32];
    int nTypes = 0, i;
    LOADDATA ld, *pld = NULL;
    unsigned char *dd = NULL;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-i") && i + 1 < argc)
            image = argv[++i];
        else if (!strcmp(a, "-m") && i + 1 < argc)
            module = argv[++i];
        else if (!strcmp(a, "-p") && i + 1 < argc)
            path = argv[++i];
        else if (!strcmp(a, "-o") && i + 1 < argc)
            options = (DWORD)hex(argv[++i]);
        else if (!strcmp(a, "-b") && i + 1 < argc)
            base = hex(argv[++i]);
        else if (!strcmp(a, "-s") && i + 1 < argc)
            size = (DWORD)hex(argv[++i]);
        else if (!strcmp(a, "-c") && i + 3 < argc) {
            cvName = argv[++i];
            cvSig = (DWORD)hex(argv[++i]);
            cvAge = (DWORD)atoi(argv[++i]);
        } else if (!strcmp(a, "-a") && i + 1 < argc)
            queries = argv[++i];
        else if (!strcmp(a, "-l"))
            locals = 1;
        else if (!strcmp(a, "-e"))
            enumSyms = 1;
        else if (!strcmp(a, "-E"))
            enumSyms = g_rows = 1;
        else if (!strcmp(a, "-T"))
            enumTypes = 1;
        else if (!strcmp(a, "-F"))
            enumFiles = 1;
        else if (!strcmp(a, "-L"))
            enumLines = 1;
        else if (!strcmp(a, "-C"))
            cleanup = 1;
        else if (!strcmp(a, "-t") && i + 1 < argc && nTypes < 32)
            types[nTypes++] = argv[++i];
        else {
            printf("usage: dbgprobe -i <image> [-m mod] [-p path] [-o opts] [-b base] [-s size]\n"
                   "       [-c pdb sig age] [-a queries] [-l] [-e|-E] [-T] [-F] [-L] [-C] [-t type]...\n");
            return 2;
        }
    }
    if (!image) {
        printf("usage: dbgprobe -i <image> ...\n");
        return 2;
    }
    if (!bindAll())
        return 3;
    printf("OPTIONS\t%lx\tset=%lx\n", options, pSetOptions(options));
    if (!pInitialize(g_proc, path, FALSE)) {
        printf("INIT\tfail\terr=%lu\n", GetLastError());
        return 4;
    }
    printf("INIT\tok\tpath=%s\n", path ? path : "-");

    if (cvName) {
        DWORD rec = 16 + (DWORD)strlen(cvName) + 1;
        IMAGE_DEBUG_DIRECTORY *d;
        dd = (unsigned char *)calloc(1, sizeof(IMAGE_DEBUG_DIRECTORY) + rec);
        d = (IMAGE_DEBUG_DIRECTORY *)dd;
        d->Type = IMAGE_DEBUG_TYPE_CODEVIEW;
        d->SizeOfData = rec;
        d->PointerToRawData = sizeof(IMAGE_DEBUG_DIRECTORY);
        memcpy(dd + sizeof(*d), "NB10", 4);
        memcpy(dd + sizeof(*d) + 8, &cvSig, 4);
        memcpy(dd + sizeof(*d) + 12, &cvAge, 4);
        strcpy((char *)dd + sizeof(*d) + 16, cvName);
        memset(&ld, 0, sizeof(ld));
        ld.ssize = sizeof(ld);
        ld.ssig = DBHHEADER_DEBUGDIRS;
        ld.data = dd;
        ld.size = sizeof(IMAGE_DEBUG_DIRECTORY) + rec;
        pld = &ld;
        printf("LOADDATA\tDEBUGDIRS\t%s\tsig=%lx\tage=%lu\n", cvName, cvSig, cvAge);
    }

    SetLastError(0);
    g_base = pLoadModuleEx(g_proc, NULL, image, module, base, size, pld, 0);
    printf("LOAD\t%s\tbase=%I64x\terr=%lu\n", image, g_base, GetLastError());
    if (!g_base) {
        pCleanup(g_proc);
        return 5;
    }
    printModule();

    if (enumSyms) {
        pEnumSymbols(g_proc, g_base, "*", countSym, NULL);
        printTags("ENUM");
    }
    if (enumTypes && pEnumTypes) {
        int keep = g_rows;
        g_rows = 0;
        pEnumTypes(g_proc, g_base, countSym, NULL);
        printTags("TYPES");
        g_rows = keep;
    }
    if (enumFiles && pEnumSourceFiles) {
        g_files = 0;
        pEnumSourceFiles(g_proc, g_base, "*", countFile, NULL);
        printf("FILES\t%lu\n", g_files);
    }
    if (enumLines) {
        g_lines = 0;
        if (!pEnumLines)
            printf("LINES\tSymEnumLines not exported\n");
        else if (!pEnumLines(g_proc, g_base, NULL, NULL, printLine, NULL))
            printf("LINES\tfail\terr=%lu\n", GetLastError());
        printf("LINES\t%lu\n", g_lines);
    }
    for (i = 0; i < nTypes; i++)
        lookupType(types[i]);
    if (queries)
        runQueries(queries, locals);
    printModule();
    fflush(stdout);
    if (cleanup)
        pCleanup(g_proc);
    free(dd);
    return 0;
}
"#;
