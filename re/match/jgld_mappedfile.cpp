// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll memory-mapped file (debug build). Names are chosen here.
#include <windows.h>
#include <string.h>
class MappedFile {
public:
    void close();                                // 0x10002e10
    void* openRead(const char* name, int sequential);
    void* openWrite(const char* name, int sequential);
    void* create(const char* name, DWORD size, int sequential);
    int m_0;
    void* m_view;
    HANDLE m_file;
    HANDLE m_map;
    DWORD m_size;
};
// MATCH: jgld.dll 0x10002870 ?openRead@MappedFile@@QAEPAXPBDH@Z
void* MappedFile::openRead(const char* name, int sequential)
{
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    if (sequential)
        flags |= FILE_FLAG_SEQUENTIAL_SCAN;
    else
        flags |= FILE_FLAG_RANDOM_ACCESS;
    close();
    const char* path = name;
    m_file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, flags, 0);
    if (m_file == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND)
            return 0;
        return 0;
    }
    m_map = CreateFileMappingA(m_file, 0, PAGE_READONLY, 0, 0, 0);
    if (m_map == 0) {
        close();
        return 0;
    }
    m_view = MapViewOfFile(m_map, FILE_MAP_READ, 0, 0, 0);
    if (m_view == 0) {
        close();
        return 0;
    }
    m_size = GetFileSize(m_file, 0);
    return m_view;
}
// MATCH: jgld.dll 0x10002a20 ?openWrite@MappedFile@@QAEPAXPBDH@Z
void* MappedFile::openWrite(const char* name, int sequential)
{
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    if (sequential)
        flags |= FILE_FLAG_SEQUENTIAL_SCAN;
    else
        flags |= FILE_FLAG_RANDOM_ACCESS;
    close();
    const char* path = name;
    m_file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, flags, 0);
    if (m_file == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND)
            return 0;
        return 0;
    }
    m_map = CreateFileMappingA(m_file, 0, PAGE_READWRITE, 0, 0, 0);
    if (m_map == 0) {
        close();
        return 0;
    }
    m_view = MapViewOfFile(m_map, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (m_view == 0) {
        close();
        return 0;
    }
    m_size = GetFileSize(m_file, 0);
    return m_view;
}
// MATCH: jgld.dll 0x10002bd0 ?create@MappedFile@@QAEPAXPBDKH@Z
void* MappedFile::create(const char* name, DWORD size, int sequential)
{
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    if (sequential)
        flags |= FILE_FLAG_SEQUENTIAL_SCAN;
    else
        flags |= FILE_FLAG_RANDOM_ACCESS;
    close();
    m_size = size;
    m_file = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, flags, 0);
    if (m_file == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND)
            return 0;
        return 0;
    }
    if (SetFilePointer(m_file, size, 0, FILE_BEGIN) == -1) {
        close();
        return 0;
    }
    SetEndOfFile(m_file);
    if (SetFilePointer(m_file, 0, 0, FILE_BEGIN) == -1) {
        close();
        return 0;
    }
    m_map = CreateFileMappingA(m_file, 0, PAGE_READWRITE, 0, 0, 0);
    if (m_map == 0) {
        close();
        return 0;
    }
    m_view = MapViewOfFile(m_map, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (m_view == 0) {
        close();
        return 0;
    }
    memset(m_view, 0, size);
    return m_view;
}
