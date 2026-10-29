static char pool[64];

void *operator new(unsigned int n)
{
    return pool;
}

void operator delete(void *p)
{
}

class Base {
public:
    Base();
    virtual ~Base();
    virtual int load(const char *path);
    virtual int play(int *loops);
    virtual int load(char *data, unsigned long size);
    virtual int stop();
    virtual int play(int loops);
    virtual int load();
    virtual int play();
    int m_state;
};

class Derived : public Base {
public:
    Derived();
    ~Derived();
    int play(int loops);
    virtual int pause(int ms);
    virtual int load(int index);
    int m_paused;
};

class Mixin {
public:
    virtual int mix(int a);
    virtual int unmix();
    int m_level;
};

class Combined : public Derived, public Mixin {
public:
    Combined();
    int mix(int a);
    virtual int extra();
};

Base::Base() { m_state = 1; }
Base::~Base() { m_state = 2; }
int Base::load(const char *path) { return path[0] + 101; }
int Base::play(int *loops) { return *loops + 102; }
int Base::load(char *data, unsigned long size) { return data[size] + 103; }
int Base::stop() { return m_state + 104; }
int Base::play(int loops) { return loops * 105; }
int Base::load() { return m_state * 106; }
int Base::play() { return m_state - 107; }

Derived::Derived() { m_paused = 3; }
Derived::~Derived() { m_paused = 4; }
int Derived::play(int loops) { return loops * 201 + m_paused; }
int Derived::pause(int ms) { return ms + 202 + m_paused; }
int Derived::load(int index) { return index * 203 - m_paused; }

int Mixin::mix(int a) { return a * 301 + m_level; }
int Mixin::unmix() { return m_level - 302; }

Combined::Combined() { m_level = 5; }
int Combined::mix(int a) { return a * 401 + m_paused + m_level; }
int Combined::extra() { return m_state * 402 + m_level; }

extern "C" __declspec(dllexport) Base *make(int k)
{
    if (k == 1) {
        return new Base;
    }
    if (k == 2) {
        return new Derived;
    }
    return new Combined;
}

extern "C" __declspec(dllexport) Mixin *mixer(int k)
{
    if (k == 1) {
        return new Mixin;
    }
    return new Combined;
}

class Voice {
public:
    virtual int __stdcall stop(int flag);
    virtual int __stdcall position(int *where);
    int m_handle;
};

int __stdcall Voice::stop(int flag) { return m_handle + flag * 501; }
int __stdcall Voice::position(int *where) { return *where = m_handle - 502; }

extern "C" __declspec(dllexport) Voice *voice(int k)
{
    return new Voice;
}

class Holder {
public:
    virtual int stop(int flag);
    Voice *m_voice;
};

int Holder::stop(int flag) { return m_voice->stop(flag); }

extern "C" __declspec(dllexport) Holder *holder(int k)
{
    return new Holder;
}
