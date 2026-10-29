#ifndef SOUNDSTAT_H
#define SOUNDSTAT_H

#include <fstream.h>

class CTrace {
public:
    CTrace &operator=(char *in_string);

private:
    char m_id_string[256];
    unsigned long m_t1;
    unsigned long m_t2;
    int m_d1;
    int m_d2;
    int m_d3;
    int m_d4;

public:
    CTrace();
    ~CTrace();
    void in_data(int d1, int d2, int d3, int d4);
    void set_t1();
    void set_t2();
    void dump_to_file(ofstream *pout_stream);
};

#endif
