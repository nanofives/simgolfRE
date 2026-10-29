#ifndef GSOBJECT_H
#define GSOBJECT_H
class LinkedList;
class GSObject {
public:
    GSObject();
    virtual ~GSObject();
    static LinkedList object_list;
    int               m_reserved04;
};
#endif
