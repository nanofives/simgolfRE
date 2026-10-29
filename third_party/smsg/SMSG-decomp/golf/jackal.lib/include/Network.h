#ifndef NETWORK_H
#define NETWORK_H

#include <dplay.h>
#include "ClassList.h"
#include "SoundEntry.h"
#include "NetFifo.h"
#define NET_MAX_CONNECTIONS 16
struct NetConnection {
    int m_id;
    int m_guaranteedCount;
    int m_sending;
    unsigned m_avgReplyTime;
    unsigned m_lastTime;
    unsigned char m_flags;
    char m_name[0x43];
};


struct JackalVoiceRx : VoiceRx, virtual HeapNew {};

typedef ClassList<JackalVoiceRx> PeerList;
struct NetPlayerEntry {
    int m_id;
    char m_name[0x20];
    int m_flags;
};

struct NetMessage {
    unsigned short m_type;
    unsigned short m_pad02;
    int m_seq;
    union {
        int m_leaverId;
        unsigned char m_entryCount;
    };
    NetPlayerEntry m_entries[1];
};



struct NetOutbox {
    virtual void slot00();
    virtual void set_fade(unsigned long ms_fade);
    virtual void set_fade_in();
    virtual void load(const char *fname);
    virtual void load(IAVIStream *paviaudio);
    virtual void unload();
    virtual void play();
    virtual void play(unsigned long num);
    virtual void stop();
    virtual void fade();
    virtual void fade(unsigned long ms_fade);
    virtual void fade_in();
    virtual void fade_in(unsigned int ms_fade);
    virtual void ramp();
    virtual void release();
    virtual void get_game_hwnd();
    virtual void set_volume(int vol);
    virtual void set_pan(int pan);
    virtual void set_loop_state();
    virtual void set_delay();
    virtual void set_type();
    virtual void slot21();
    virtual void is_looping();
    virtual void is_playing();
    virtual void is_loaded();
    virtual void get_next();
    virtual void get_prev();
    virtual void set_attrib();
    virtual void get_attrib();
    virtual void get_time();
    virtual void get_type();
    virtual void attach();
    virtual void detach();
    virtual void init();
    virtual MMIOINFO *get_next_buffer();
    virtual void return_buffer(MMIOINFO *buf);
    virtual void start();
    virtual unsigned get_nbuffers();

    char m_pad04[0x5c - 0x04];
};
#define NET_DROP_TOLERANCE_MS(seconds) ((seconds) * 1000)
struct NetPlayerList {
    unsigned char m_count;
    NetPlayerEntry m_entries[1];
};
#define NET_SEND_THROTTLE 100
enum {
    NET_POLLING = 0x00040000,
    NET_LIVE = 0x20000000,
    NET_THROTTLED = 0x40000000
};
#define NET_SEND_RELEASE 50
struct Net {
    virtual ~Net();
    virtual void on_send_failed();
    virtual void on_player_added(char *name, int id);
    virtual void on_player_deleted(char *name, int id);
    virtual void on_name_changed(char *name, int id);
    virtual void on_session_lost();
    virtual void on_host_inherited();

    void send_player_order();
    void close();
    int SendMessageWithRetry(void *data, int len, int dest, int type, int flags);
    int send_packet_type(void *data, int len, int dest, int type, int flags);
    char *get_player_name(int id);
    void process_voice();
    void check_polling();
    int drop_player(int id, int notify);
    int IsHost() { return m_localId == m_hostId; }
    int IsActive();
    int IsBackupHost() { return m_localId == m_conn[1].m_id; }
    void SetTolerance(int button)
    {
        switch (button) {
        case 0: m_timeout = NET_DROP_TOLERANCE_MS(5); break;
        case 1: m_timeout = NET_DROP_TOLERANCE_MS(10); break;
        case 2: m_timeout = NET_DROP_TOLERANCE_MS(20); break;
        case 3: m_timeout = NET_DROP_TOLERANCE_MS(30); break;
        }
    }
    int NetworkUnrecovered(int a, int b)
    {
        int c = a * b;
        if (c > b)
            c -= a;
        if (c < a)
            c += b;
        if (c == a)
            c ^= b;
        if (c > b)
            c -= a;
        if (c < a)
            c += b;
        if (c == a)
            c ^= b;
        return c;
    }

    void add_player(const char *name, int id, int flags);
    void handle_sys_msg();
    int StartPacketVoice(int arg);
    int start_voice(int arg);
    void stop_voice();


    void *m_msgBuf;
    int m_msgBufSize;
    DWORD m_threadId;
    int m_10;
    HANDLE m_ackEvent;
    HANDLE m_msgEvent;
    HANDLE m_recvEvent;
    HANDLE m_wake;
    HANDLE m_event24;
    IDirectPlay2A *m_directPlay;
    Net *m_self;
    void *m_threadBuf;
    int m_lockReady;
    CRITICAL_SECTION m_lock;
    int m_version;
    int m_startKey;
    int m_startVoiceKey;
    int m_startArg;
    int m_voiceSendMode;
    NetOutbox m_outbox;
    PeerList m_peers;
    int m_voiceDest;
    int m_state;
    unsigned m_lastPoll;
    unsigned m_timeout;
    int m_seq;
    NetFifo m_queueF8;
    NetFifo m_loopback;
    NetFifo m_inbox;
    NetConnection m_conn[NET_MAX_CONNECTIONS];
    char m_pad6f0[0x6f8 - 0x6f0];
    int m_playerCount;
    void *m_recvBuf;
    int m_maxRetryLen;
    const GUID *m_appGuid;
    GUID m_spGuid;
    int m_replyTimeout;
    int m_71c;
    char *m_sessionName;
    char *m_sessionPassword;
    char m_pad728[0x77c - 0x728];
    int m_localId;
    int m_hostId;
};

void do_net();
void check_net();

BOOL FAR PASCAL players_in_group_callback(DPID id, DWORD type, LPCDPNAME name, DWORD flags, LPVOID context);

extern NetConnection *g_enumConn;

extern int g_sweepBusy;
extern int g_keepAliveBusy;

extern Net *g_pNetwork;

extern int prev_server_id;

extern IDirectPlay2A *g_directPlay;

inline int Net::IsActive()
{
    return g_directPlay ? 1 : 0;
}

union PlayerNameBuffer {
    DPNAME m_name;
    char m_raw[0x100];
};
extern char g_playerNameBuf[0x100];

#endif
