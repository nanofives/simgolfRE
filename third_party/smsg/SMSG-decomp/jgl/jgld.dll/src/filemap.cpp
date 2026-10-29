#include "filemap.h"
#include <string.h>
#define INVALID_SET_FILE_POINTER  ((DWORD)-1)
Filemap::Filemap()
{
    m_base = 0;
    m_hFile = INVALID_HANDLE_VALUE;
    m_hMap = 0;
}
Filemap::Filemap(const char *name, int sequential)
{
    {
        Filemap scratch;
    }
    open(name, sequential);
}
Filemap::Filemap(const char *name)
{
    {
        Filemap scratch;
    }
    open(name, 0);
}
Filemap::~Filemap()
{
    close();
}
void *Filemap::open_read(const char *name, int sequential)
{
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    if (sequential)
        flags |= FILE_FLAG_SEQUENTIAL_SCAN;
    else
        flags |= FILE_FLAG_RANDOM_ACCESS;
    this->close();
    const char *fn = name;
    this->m_hFile = CreateFileA(fn, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, flags, 0);
    if (this->m_hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND)
            return 0;
        return 0;
    }
    this->m_hMap = CreateFileMappingA(this->m_hFile, 0, PAGE_READONLY, 0, 0, 0);
    if (this->m_hMap == 0) {
        this->close();
        return 0;
    }
    this->m_base = MapViewOfFile(this->m_hMap, FILE_MAP_READ, 0, 0, 0);
    if (this->m_base == 0) {
        this->close();
        return 0;
    }
    this->m_size = GetFileSize(this->m_hFile, 0);
    return this->m_base;
}
void *Filemap::open(const char *name, int sequential)
{
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    if (sequential)
        flags |= FILE_FLAG_SEQUENTIAL_SCAN;
    else
        flags |= FILE_FLAG_RANDOM_ACCESS;
    this->close();
    const char *fn = name;
    this->m_hFile = CreateFileA(fn, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, flags, 0);
    if (this->m_hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND)
            return 0;
        return 0;
    }
    this->m_hMap = CreateFileMappingA(this->m_hFile, 0, PAGE_READWRITE, 0, 0, 0);
    if (this->m_hMap == 0) {
        this->close();
        return 0;
    }
    this->m_base = MapViewOfFile(this->m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (this->m_base == 0) {
        this->close();
        return 0;
    }
    this->m_size = GetFileSize(this->m_hFile, 0);
    return this->m_base;
}
void *Filemap::create(const char *name, DWORD length, int sequential)
{
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    if (sequential)
        flags |= FILE_FLAG_SEQUENTIAL_SCAN;
    else
        flags |= FILE_FLAG_RANDOM_ACCESS;
    this->close();
    this->m_size = length;
    this->m_hFile = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, flags, 0);
    if (this->m_hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND)
            return 0;
        return 0;
    }
    if (SetFilePointer(this->m_hFile, length, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        this->close();
        return 0;
    }
    SetEndOfFile(this->m_hFile);
    if (SetFilePointer(this->m_hFile, 0, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        this->close();
        return 0;
    }
    this->m_hMap = CreateFileMappingA(this->m_hFile, 0, PAGE_READWRITE, 0, 0, 0);
    if (this->m_hMap == 0) {
        this->close();
        return 0;
    }
    this->m_base = MapViewOfFile(this->m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (this->m_base == 0) {
        this->close();
        return 0;
    }
    memset(this->m_base, 0, length);
    return this->m_base;
}
void Filemap::close(void)
{
    if (this->m_base) {
        UnmapViewOfFile(this->m_base);
        this->m_base = 0;
    }
    if (this->m_hMap) {
        CloseHandle(this->m_hMap);
        this->m_hMap = 0;
    }
    if (this->m_hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(this->m_hFile);
        this->m_hFile = INVALID_HANDLE_VALUE;
    }
}
void Filemap::Truncate(uchar *end)
{
    DWORD used;
    if (end < this->m_base) {
        this->close();
        return;
    }
    used = (DWORD)((char *)end - (char *)this->m_base);
    if (this->m_base) {
        UnmapViewOfFile(this->m_base);
        this->m_base = 0;
    }
    if (this->m_hMap) {
        CloseHandle(this->m_hMap);
        this->m_hMap = 0;
    }
    if (SetFilePointer(this->m_hFile, used, 0, 0) == -1) {
        this->close();
        return;
    }
    SetEndOfFile(this->m_hFile);
    if (this->m_hFile) {
        CloseHandle(this->m_hFile);
        this->m_hFile = 0;
    }
}
