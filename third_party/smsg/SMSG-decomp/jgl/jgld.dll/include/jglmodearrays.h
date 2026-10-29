#ifndef JGLMODEARRAYS_H
#define JGLMODEARRAYS_H
struct DisplayMode;
struct DevModeA;
struct DisplayDevice {
    char raw[0x94];
};
class DisplayDeviceArray {
public:
    DisplayDeviceArray();
    DisplayDeviceArray(int unrecovered) {}
    virtual ~DisplayDeviceArray();
    void           Reset();
    int            Init();
    DisplayDevice *m_data;
    int            m_capacity;
    int            m_count;
    int            m_grow;
};
class DisplayModeArray {
public:
    DisplayModeArray();
    DisplayModeArray(int unrecovered) {}
    virtual ~DisplayModeArray();
    void         Reset();
    int          Init();
    void         Clear();
    int          GetCount();
    int          add(DisplayMode mode);
    DisplayMode *m_pData;
    int          m_capacity;
    int          m_count;
    int          m_growBy;
};
class DevModeArray {
public:
    DevModeArray();
    DevModeArray(int unrecovered) {}
    virtual ~DevModeArray();
    void      Reset();
    int       Init();
    void      Clear();
    int       GetCount();
    int       add(DevModeA dm);
    DevModeA *m_pData;
    int       m_capacity;
    int       m_count;
    int       m_growBy;
};
extern "C" DisplayModeArray mode_array;
extern "C" DevModeArray     dev_array;
#endif
