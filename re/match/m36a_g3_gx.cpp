// FLAGS sound.dll: /O2 /GX
// m36a (round 36) group g3: sound.dll functions with an EH frame (new-expression cleanup).
#include <string.h>

namespace m36a_g3gx {

struct Triple {
    int a, b, c;
};

struct WaveInDeviceBase {
    WaveInDeviceBase();
    virtual ~WaveInDeviceBase();
    char pad4[0x24 - 4];
};

struct WaveInDevice : WaveInDeviceBase {
    Triple m_24;
    int m_30, m_34, m_38, m_3c, m_40;
    char pad44[0x4c - 0x44];
    WaveInDevice()
    {
        m_30 = 0;
        m_34 = 0;
        m_38 = 0;
        m_40 = 0x7f;
        memset(&m_24, 0, sizeof(m_24));
        m_3c = 0;
    }
    virtual ~WaveInDevice();
};

extern WaveInDevice *g_dllWaveInDevice;   // 0x100b4a24

struct Dll_Wave_In_Device {
    static int create_device(WaveInDevice **out);
};

// MATCH: sound.dll 0x1000e910 ?create_device@Dll_Wave_In_Device@m36a_g3gx@@SAHPAPAUWaveInDevice@2@@Z
int Dll_Wave_In_Device::create_device(WaveInDevice **out)
{
    if (g_dllWaveInDevice == 0) {
        g_dllWaveInDevice = new WaveInDevice;
        *out = g_dllWaveInDevice;
        return 0;
    }
    return 0xc;
}

struct WaveInCapture {
    WaveInCapture();
    virtual ~WaveInCapture();
    int allocCaptureBuffers(int count, int size);
    char pad4[0x70 - 4];
};

extern WaveInCapture *g_waveInDevice;   // 0x100b4a04

struct Wave_In_Device {
    char pad0[0x28];
    int m_open;         // +0x28
    char pad2c[0x48 - 0x2c];
    int m_48;           // +0x48
    int openCapture(int a, int b);
};

// MATCH: sound.dll 0x1000ead0 ?openCapture@Wave_In_Device@m36a_g3gx@@QAEHHH@Z
int Wave_In_Device::openCapture(int a, int b)
{
    if (m_open)
        return 0;
    if (g_waveInDevice == 0) {
        g_waveInDevice = new WaveInCapture;
        int r = g_waveInDevice->allocCaptureBuffers(10, 0x168);
        if (r) {
            if (g_waveInDevice)
                delete g_waveInDevice;
            g_waveInDevice = 0;
            return r;
        }
        m_48 = a;
    }
    m_open = 1;
    return 0;
}

} // namespace m36a_g3gx
