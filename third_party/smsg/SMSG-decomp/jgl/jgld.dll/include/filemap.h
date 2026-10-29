#ifndef FILEMAP_H
#define FILEMAP_H
#include "jglwin.h"
class Filemap {
public:
    Filemap();
    Filemap(const char *name, int sequential);
    Filemap(const char *name);
    virtual ~Filemap();
    void *open_read(const char *name, int sequential);
    void *open(const char *name, int sequential);
    void *create(const char *name, DWORD length, int sequential);
    void  close(void);
    void  Truncate(uchar *end);
    void *GetData() const
    {
        return m_base;
    }
    DWORD GetSize() const
    {
        return GetFileSize(m_hFile, 0);
    }
    void  *m_base;
    HANDLE m_hFile;
    HANDLE m_hMap;
    DWORD  m_size;
};
#endif
