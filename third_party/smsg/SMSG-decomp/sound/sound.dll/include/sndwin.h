#ifndef SNDWIN_H
#define SNDWIN_H

struct CSWin_Flags_tag {
    bool wod_ignore : 1;
    bool mod_ignore : 1;
    bool initialized : 1;
};

class CSndWin {
    CSWin_Flags_tag m_flags;
    static const char *m_class_name;
    static const char *m_wnd_name;
    HWND m_hwnd;
    static HINSTANCE m_snd_hinst;
    static WNDCLASSA m_snd_class;

public:
    CSndWin();
    virtual ~CSndWin();
    SNDERR create();
    SNDERR init(HWND game_hwnd);
    SNDERR destroy();
    static void set_snd_hinst(HINSTANCE hinst)
    {
        m_snd_hinst = hinst;
        m_snd_class.hInstance = hinst;
    }
    static HINSTANCE get_snd_hinst();
    static const char *get_wnd_class_name() { return m_class_name; }
    static const char *get_wnd_name() { return m_wnd_name; }
    HWND get_hwnd() { return m_hwnd; }

    SNDERR clean_message_queue();
    void set_wo_processing(bool state) { m_flags.wod_ignore = !state; }
    bool is_wo_ignored() { return m_flags.wod_ignore; }
    void set_mo_processing(bool state) { m_flags.mod_ignore = !state; }
    bool is_mo_ignored() { return m_flags.mod_ignore; }
    bool initialized() { return m_flags.initialized; }
    long m_game_proc;
};

extern CSndWin sound_win;

#endif
