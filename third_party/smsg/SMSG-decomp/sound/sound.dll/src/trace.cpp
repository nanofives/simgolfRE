#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include "SoundStat.h"

CTrace::CTrace()
{
    m_t1 = 0;
    m_t2 = 0;
    m_d1 = 0;
    m_d2 = 0;
    m_d3 = 0;
    m_d4 = 0;
    strcpy(m_id_string, "blank");
}

CTrace::~CTrace()
{
}

void CTrace::dump_to_file(ofstream *pout_stream)
{
    *pout_stream << '\n';
    *pout_stream << m_id_string << '\n';
    *pout_stream << m_t1 << ' ' << m_t2 << '\n';
    *pout_stream << m_d1 << '\t' << m_d2 << '\t' << m_d3 << '\t' << m_d4 << '\n';
}

void CTrace::set_t1()
{
    m_t1 = timeGetTime();
}

void CTrace::set_t2()
{
    m_t2 = timeGetTime();
}

void CTrace::in_data(int d1, int d2, int d3, int d4)
{
    m_d1 = d1;
    m_d2 = d2;
    m_d3 = d3;
    m_d4 = d4;
}

CTrace &CTrace::operator=(char *in_string)
{
    strcpy(m_id_string, in_string);
    return *this;
}
