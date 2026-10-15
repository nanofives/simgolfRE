// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll 0x10006a40 (atexitDestroyGlobalList), round 36 batch m36e: the once-guarded teardown of the tracked-object
// list 0x10128428 (byte flag 0x10128444, bit 0, tested and set before LinkedList::~LinkedList via thunk 0x10001979).
// jgld_list.cpp left it unmatched: a plain global's atexit stub `_$E2` has no guard. Of the VC6 constructs tried here
// (plain, `static`, `const`, `#pragma init_seg(lib)`, function-local static, class template static data member) only
// the template static data member gives a guarded destroy stub: its `$E` symbol tests and sets a file-static flag
// `_$S<n>` and then calls the destructor, which is 0x10006a40 instruction for instruction. That construct also guards
// its INITIALIZER (`$D`), while jgld's initializer of the same list, 0x100069b0 (_$E1 in jgld_list.cpp), has no
// guard. So the source construct is NOT determined: this file pins the instruction sequence only.
class LinkedList {
public:
    LinkedList();
    virtual ~LinkedList();
    void* m_data;
    char m_flag;
    int m_count;
    void* m_head;
    void* m_cur;
    int m_ordered;
};
template <class T> struct TrackedListHolder { static LinkedList list; };
template <class T> LinkedList TrackedListHolder<T>::list;
void* trackedListInstance() { return &TrackedListHolder<int>::list; }
// MATCH: jgld.dll 0x10006a40 ?list@?$TrackedListHolder@H@@$E
