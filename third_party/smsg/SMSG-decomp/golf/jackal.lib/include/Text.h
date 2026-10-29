#ifndef TEXT_H
#define TEXT_H

#include "Heap.h"

struct _iobuf;

struct Text {
    Text(int size);
    virtual ~Text() { shutdown(); }
    Text();
    int init(int size);
    void shutdown();
    void close();
    int open(const char *path, const char *section);
    char *get();
    char *string();
    char *item();
    char *item_string();
    int item_number();
    int item_binary();

    char m_name[0x50];
    char m_path[0x100];
    char *m_line;
    _iobuf *m_file;
    char *m_buf;
    char *m_itemBuf;
};

struct TextIndex {
    TextIndex();
    virtual ~TextIndex();
    int search_index(const char *file, const char *key);

    char m_name[0x100];
    int m_count;
    Heap m_entries;
};

extern Text main_text;
extern char *text_buffer;
extern char *text_item_buffer;
extern TextIndex text_index[4];

void text_close();
int text_open(const char *file, const char *section);
char *text_get();
int text_search_index(const char *file, const char *section);

#endif
