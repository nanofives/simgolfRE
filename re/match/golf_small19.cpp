// golf_clean.exe functions, batch 19 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <windows.h>

struct Sel490 { int m_0; int m_4; };
extern Sel490* g_sel83b9b4;                  // 0x83b9b4
extern int g_sel83b9b8, g_sel83b9bc, g_sel83b9c0;
// 3 for a null object; remembers it when +4 is set, then stores the three values.
// MATCH: golf_clean.exe 0x00490c80 ?select490@@YAHPAUSel490@@HHH@Z
int select490(Sel490* p, int a, int b, int c)
{
    if (!p)
        return 3;
    if (p->m_4)
        g_sel83b9b4 = p;
    g_sel83b9b8 = a;
    g_sel83b9bc = b;
    g_sel83b9c0 = c;
    return 0;
}

struct Win47b;
inline void offsetRect(RECT* r, int dx, int dy)
{
    r->left += dx;
    r->right += dx;
    r->top += dy;
    r->bottom += dy;
}
void relayout47e140(Win47b* w, void* area);  // 0x47e140
// Moves the window's rect (+0x1bc when flag 2 of +0xa0 is set, else +0x1ac) so its top-left is (x, y).
struct Win47b {
    char pad[0xa0]; int m_flags; char pad2[0x1ac - 0xa4];
    RECT m_rc; RECT m_rc2; char pad3[0x1dc - 0x1cc]; int m_area;
    int moveTo(int x, int y);
};
// MATCH: golf_clean.exe 0x0047b420 ?moveTo@Win47b@@QAEHHH@Z
int Win47b::moveTo(int x, int y)
{
    if (m_flags & 2)
        offsetRect(&m_rc2, x - m_rc2.left, y - m_rc2.top);
    else
        offsetRect(&m_rc, x - m_rc.left, y - m_rc.top);
    relayout47e140(this, &m_area);
    return 0;
}

// Read-only memory-mapped file: +4 view, +8 file handle, +0xc mapping, +0x10 size.
struct MappedFile {
    int m_0; void* m_view; HANDLE m_file; HANDLE m_map; DWORD m_size;
    void close();                            // 0x492e80
    void* open(const char* name, int sequential);
};
// MATCH: golf_clean.exe 0x00492dd0 ?open@MappedFile@@QAEPAXPBDH@Z
void* MappedFile::open(const char* name, int sequential)
{
    DWORD flags;
    if (sequential)
        flags = FILE_FLAG_SEQUENTIAL_SCAN | FILE_ATTRIBUTE_NORMAL;
    else
        flags = FILE_FLAG_RANDOM_ACCESS | FILE_ATTRIBUTE_NORMAL;
    close();
    m_file = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, flags, 0);
    if (m_file == INVALID_HANDLE_VALUE) {
        GetLastError();
        return 0;
    }
    m_map = CreateFileMappingA(m_file, 0, PAGE_READONLY, 0, 0, 0);
    if (!m_map) {
        close();
        return 0;
    }
    m_view = MapViewOfFile(m_map, FILE_MAP_READ, 0, 0, 0);
    if (!m_view) {
        close();
        return 0;
    }
    m_size = GetFileSize(m_file, 0);
    return m_view;
}
