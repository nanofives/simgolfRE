
struct Base {
    virtual int value(int k);
};

int call_virtual(Base *b)
{
    return b->value(3);
}







