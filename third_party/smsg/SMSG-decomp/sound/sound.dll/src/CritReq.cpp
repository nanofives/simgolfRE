#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <string.h>
#include "sound.h"
#include "sounddevice.h"

CCriticalRequest::CCriticalRequest()
{
    m_type = REQUEST_NULL;
    m_psound = 0;
    m_data1 = 0;
}

CCriticalRequest::CCriticalRequest(REQUEST_TYPE type, Sound *psound)
{
    m_type = type;
    m_psound = psound;
}

CCriticalRequest::~CCriticalRequest()
{
    m_psound = 0;
    m_type = REQUEST_NULL;
}

CCriticalRequestQueue::CCriticalRequestQueue()
{
    for (int i = 0; i < 0x1000; i++)
        m_queue[i] = 0;
    m_GT_index = 0;
    m_TT_index = 0;
}

CCriticalRequestQueue::~CCriticalRequestQueue()
{
    for (int i = 0; i < 0x1000; i++)
        m_queue[i] = 0;
    m_GT_index = 0;
    m_TT_index = 0;
}

CCriticalRequest *CCriticalRequestQueue::operator[](unsigned index)
{
    return m_queue[index & 0xfff];
}
