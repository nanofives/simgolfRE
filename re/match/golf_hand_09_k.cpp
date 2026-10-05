// FLAGS golf_clean.exe: /O2 /GX
#include <string.h>
struct VB4896b0 { virtual ~VB4896b0(); };
struct Obj4896b0 : virtual VB4896b0 { };
struct Node4896b0 : virtual VB4896b0 { int m_4; Obj4896b0* m_data; Node4896b0* m_next; };
struct List4896b0 {
    virtual void v0();
    virtual void onRemove(Obj4896b0*);
    int m_4;
    Node4896b0* m_head;
    Node4896b0* m_next;
    int m_count;
    int m_14;
    void* m_18;
    void clear()
    {
        if (m_head) {
            for (int i = 0; i < m_count; i++) {
                m_next = m_head->m_next;
                Obj4896b0* o = m_head->m_data;
                onRemove(o);
                delete o;
                m_head->m_data = 0;
                delete m_head;
                m_head = m_next;
            }
            m_head = 0;
            m_14 = 0;
            m_count = 0;
        }
    }
    void reset() { clear(); m_14 = 0; m_18 = 0; }
    void attach(void* src) { clear(); m_18 = src; m_14 = 0; }
};
struct File4896b0 { int load(const char*); char pad[0xbc]; };   // 0x474820
struct C4896b0 {
    int m_0;
    File4896b0 m_4;
    List4896b0 m_list;
    void reset();          // 0x4894b0
    int open(const char* name);
};
// MATCH: golf_clean.exe 0x004896b0 ?open@C4896b0@@QAEHPBD@Z
int C4896b0::open(const char* name)
{
    reset();
    *(const char**)((char*)this + 0x20) = name;
    if (name) {
        if (m_4.load(name))
            return 4;
        m_list.attach(&m_4);
        return 0;
    }
    m_list.reset();
    return 0;
}
