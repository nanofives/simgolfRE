#include "jgldefs.h"
#include <stdlib.h>
#include <string.h>
#include "ListBoxCtl.h"
#include "Heap.h"
#include "JackalUtil.h"

int StringList::fill_entry(StringStruct *item)
{
    if (m_pendingText) {
        if (m_pool)
            item->text = (char *)m_pool->Alloc(strlen(m_pendingText) + 1);
        else
            item->text = (char *)AllocateBytes(strlen(m_pendingText) + 1);
        if (item->text == 0)
            return 4;
        ClearString(item->text);
        AppendString(item->text, m_pendingText);
    }
    item->attr = m_pendingAttr;
    item->span = m_pendingSpan;
    return 0;
}

void StringList::kill_entry(StringStruct *item)
{
    if (item->text && !m_pool) {
        FreeIfNotNull(item->text);
        item->text = 0;
    }
}

int StringList::add_first(const char *text, int key, int attr, int span)
{
    int rc = Push(text, attr, span, key);
    if (rc == 0) {
        for (int i = m_count - 1; i > 0; i--) {
            if (!SetPos(i))
                continue;
            LinkListEntry *a = m_cur;
            if (!SetPos((i + 1) % m_count))
                continue;
            LinkListEntry *b = m_cur;
            a->key ^= b->key;
            b->key ^= a->key;
            a->key ^= b->key;
            void *t = a->elem;
            a->elem = b->elem;
            b->elem = t;
        }
    }
    return rc;
}
