// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll LinkedList (debug build, C++ EH on): a circular doubly linked list of ListNode {vptr; prev; next; data;
// flag} with a current position (m_cur, mirrored in m_data/m_flag), and Tracked, a class whose instances add
// themselves to the global list g_tracked (0x10128428) and remove themselves on destruction. Names are chosen here.
// ??_G* are the compiler-generated scalar deleting destructors; _$E4 (calls the other two), _$E1 (construct) and
// _$E3 (atexit registration) are the compiler-generated initializers of g_tracked. The destroy stub 0x10006a40
// guards on bit 0 of 0x10128444, which a plain global does not generate: not matched yet.
#include <windows.h>
struct ListNode {
    ListNode(void* data, char flag);
    virtual ~ListNode();
    ListNode* prev;
    ListNode* next;
    void* data;
    char flag;
};
class LinkedList {
public:
    LinkedList();
    virtual ~LinkedList();
    void add(void* data, char flag);
    void removeCurrent();
    int find(void* data);
    void clear();
    int count();
    void* m_data;
    char m_flag;
    int m_count;
    ListNode* m_head;
    ListNode* m_cur;
    int m_ordered;
};
class Tracked {
public:
    Tracked();
    virtual ~Tracked();
    static void deleteAll();
};
extern const char s_badList[];   // 0x1011d060: the engine's "bad use of linked list" debug message (not reproduced)
LinkedList g_tracked;            // 0x10128428

// MATCH: jgld.dll 0x10007370 ??0ListNode@@QAE@PAXD@Z
ListNode::ListNode(void* data, char flag)
{
    this->data = data;
    this->flag = flag;
    next = 0;
    prev = 0;
}
// MATCH: jgld.dll 0x10007450 ??1ListNode@@UAE@XZ
ListNode::~ListNode()
{
    next = 0;
    prev = 0;
}
// MATCH: jgld.dll 0x100073e0 ??_GListNode@@UAEPAXI@Z
// MATCH: jgld.dll 0x10006c80 ??0LinkedList@@QAE@XZ
LinkedList::LinkedList()
{
    m_head = 0;
    m_cur = 0;
    m_count = 0;
}
// MATCH: jgld.dll 0x10006cf0 ??1LinkedList@@UAE@XZ
LinkedList::~LinkedList()
{
    clear();
}
// MATCH: jgld.dll 0x10006d80 ??_GLinkedList@@UAEPAXI@Z
// MATCH: jgld.dll 0x10006d40 ?count@LinkedList@@QAEHXZ
int LinkedList::count()
{
    return m_count;
}
// MATCH: jgld.dll 0x10006df0 ?clear@LinkedList@@QAEXXZ
void LinkedList::clear()
{
    if (m_head == 0)
        return;
    ListNode* n;
    ListNode* first = m_head->next;
    for (n = first->next; n != m_head; n = first->next) {
        first->next = n->next;
        delete n;
    }
    delete m_head;
    m_ordered = 0;
    m_count = 0;
    m_head = 0;
    m_cur = 0;
}
// MATCH: jgld.dll 0x10006f40 ?add@LinkedList@@QAEXPAXD@Z
void LinkedList::add(void* data, char flag)
{
    if (m_count && m_ordered)
        OutputDebugStringA(s_badList);
    m_ordered = 0;
    ListNode* n = new ListNode(data, flag);
    if (m_head) {
        n->prev = m_head->prev;
        n->next = m_head;
        m_head->prev = n;
        n->prev->next = n;
    } else {
        m_head = n;
        m_head->prev = m_head;
        m_head->next = m_head;
    }
    m_count++;
    m_cur = n;
    m_data = data;
    m_flag = m_cur->flag;
}
// MATCH: jgld.dll 0x10007100 ?removeCurrent@LinkedList@@QAEXXZ
void LinkedList::removeCurrent()
{
    if (m_head == 0)
        return;
    if (m_head == m_head->prev) {
        delete m_head;
        m_head = 0;
        m_cur = 0;
        m_count = 0;
    } else {
        if (m_head == m_cur)
            m_head = m_head->prev;
        ListNode* n = m_cur;
        m_cur = m_cur->prev;
        m_data = m_cur->data;
        m_flag = m_cur->flag;
        n->next->prev = n->prev;
        n->prev->next = n->next;
        delete n;
        m_count--;
    }
}
// MATCH: jgld.dll 0x100072b0 ?find@LinkedList@@QAEHPAX@Z
int LinkedList::find(void* data)
{
    if (m_head == 0)
        return 0;
    ListNode* n;
    ListNode* last = m_head->next;
    n = m_head;
    do {
        if (n->data == data) {
            m_cur = n;
            m_data = m_cur->data;
            m_flag = m_cur->flag;
            return 1;
        }
        n = n->prev;
    } while (n != m_head);
    return 0;
}
// MATCH: jgld.dll 0x10006ab0 ??0Tracked@@QAE@XZ
Tracked::Tracked()
{
    g_tracked.add(this, 0);
}
// MATCH: jgld.dll 0x10006b80 ??1Tracked@@UAE@XZ
Tracked::~Tracked()
{
    g_tracked.find(this);
    g_tracked.removeCurrent();
}
// MATCH: jgld.dll 0x10006b10 ??_GTracked@@UAEPAXI@Z
// MATCH: jgld.dll 0x10006bf0 ?deleteAll@Tracked@@SAXXZ
void Tracked::deleteAll()
{
    while (g_tracked.count())
        delete (Tracked*)g_tracked.m_data;
}
// MATCH: jgld.dll 0x100069b0 _$E1   // _$E2 compiles identically here: name not determined
// MATCH: jgld.dll 0x100069f0 _$E3
// MATCH: jgld.dll 0x10006970 _$E4
