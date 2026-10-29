#include "jgldefs.h"
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Text.h"
#include "Log.h"
#include "DataPath.h"
#include "StringUtil.h"
#include "JackalUtil.h"
#include "Strings.h"

int stoi(const char *s);
int btoi(const char *s);

Text main_text(0x200);
char *text_buffer = main_text.m_buf;
char *text_item_buffer = main_text.m_itemBuf;
TextIndex text_index[4];

char *g_textFileName = main_text.m_name;
char **g_textLastLine = &main_text.m_line;
char text_ext[20] = ".txt";

void text_close()
{
    main_text.close();
}

int text_open(const char *file, const char *section)
{
    return main_text.open(file, section);
}

char *text_get()
{
    return main_text.get();
}

Text::Text()
{
    m_name[0] = 0;
    m_line = 0;
    m_file = 0;
    m_buf = 0;
    m_itemBuf = 0;
}

Text::Text(int size)
{
    m_name[0] = 0;
    m_line = 0;
    m_file = 0;
    m_buf = 0;
    m_itemBuf = 0;
    init(size);
}

int Text::init(int size)
{
    shutdown();
    m_buf = (char *)AllocateBytes(size);
    if (m_buf == 0)
        return 4;
    m_itemBuf = (char *)AllocateBytes(size);
    return m_itemBuf != 0 ? 0 : 4;
}

void Text::shutdown()
{
    close();
    if (m_buf != 0) {
        FreeIfNotNull(m_buf);
        m_buf = 0;
    }
    if (m_itemBuf != 0) {
        FreeIfNotNull(m_itemBuf);
        m_itemBuf = 0;
    }
}

void Text::close()
{
    if (m_file != 0) {
        fclose(m_file);
        m_file = 0;
    }
}

int Text::open(const char *path, const char *section)
{
    int canRewind;
    char want[0x50];
    int offset;

    canRewind = 0;
    if (path && strnicmp(path, m_name, strlen(path))) {
        strcpy(m_name, path);
        if (!strchr(m_name, '.'))
            strcat(m_name, text_ext);
        close();
        m_file = env_open(m_name, "rt");
        if (!m_file)
            return 1;
        strcpy(m_path, file_path);
    } else if (m_file) {
        canRewind = 1;
    } else {
        m_file = env_open(m_name, "rt");
        if (!m_file)
            return 1;
        strcpy(m_path, file_path);
    }
    if (!section)
        return 0;
    strcpy(want, "#");
    strcat(want, section);
    offset = text_search_index(m_name, section);
    if (offset >= 0) {
        log_say("Seeking to", section, offset, 0, 0);
        fseek(m_file, offset, SEEK_SET);
        canRewind = 1;
    }
    for (;;) {
        if (feof(m_file)) {
            if (!canRewind) {
                close();
                return 1;
            }
            canRewind = 0;
            rewind(m_file);
        }
        if (!fgets(m_buf, 0x1ff, m_file) && !feof(m_file)) {
            close();
            return 1;
        }
        if (!feof(m_file)) {
            kill_lf(m_buf);
            purge_spaces(m_buf);
        }
        if (!stricmp(want, m_buf))
            break;
    }
    m_line = m_buf;
    return 0;
}

char *Text::get()
{
    if (feof(m_file)) {
        m_buf[0] = 0;
        return 0;
    }
    if (!fgets(m_buf, 0x1ff, m_file)) {
        m_buf[0] = 0;
        return m_buf;
    }
    kill_lf(m_buf);
    purge_spaces(m_buf);
    m_line = m_buf;
    return m_buf;
}

char *Text::string()
{
    return strings.put(get());
}

char *Text::item()
{
    char *p = m_itemBuf;
    while (*m_line && *m_line != ',')
        *p++ = *m_line++;
    *p = 0;
    if (*m_line)
        m_line++;
    purge_spaces(m_itemBuf);
    return m_itemBuf;
}

char *Text::item_string()
{
    return strings.put(item());
}

int Text::item_number()
{
    return stoi(item());
}

int Text::item_binary()
{
    return btoi(item());
}

TextIndex::TextIndex()
{
    m_count = 0;
    m_name[0] = 0;
}

TextIndex::~TextIndex()
{
    m_count = 0;
    m_name[0] = 0;
}

int TextIndex::search_index(const char *file, const char *key)
{
    char buf[512];
    int n;
    char *p;

    strcpy(buf, file);
    if (strchr(buf, '.') == 0)
        strcat(buf, text_ext);
    if (stricmp(buf, m_name) != 0)
        return -1;
    if (*key == '#')
        key++;
    n = m_count;
    p = (char *)m_entries.m_block;
    for (;;) {
        char *entry = p;
        p += 4;
        if (stricmp(key, p) == 0)
            return *(int *)entry;
        while (*p)
            p++;
        p++;
        if (--n == 0)
            return -1;
    }
}

int text_search_index(const char *file, const char *section)
{
    for (int i = 0; i < 4; i++) {
        if (text_index[i].m_count != 0) {
            int r = text_index[i].search_index(file, section);
            if (r >= 0)
                return r;
        }
    }
    return -1;
}

int g_textReaderSpare[2] = {0};
