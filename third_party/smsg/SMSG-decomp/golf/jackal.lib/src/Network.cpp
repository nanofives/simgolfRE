#include "jgldefs.h"
#include <stdlib.h>
#include <string.h>
#include "Network.h"
#include "BasePop.h"
#include "InputPoll.h"
#include "HeapMessage.h"
#include "ClassListPush.h"

void do_net()
{
    if (g_pNetwork && g_directPlay)
        g_pNetwork->process_voice();
}

void check_net()
{
    if (g_pNetwork && g_directPlay)
        g_pNetwork->check_polling();
}

void Net::send_player_order()
{
    if (!IsHost())
        return;

    int n = m_playerCount;
    if (n == 0)
        return;

    int size = n * sizeof(NetPlayerList) + 4;
    NetPlayerList *msg = (NetPlayerList *)AllocateBytes(size);
    int i;

    msg->m_count = (unsigned char)n;
    for (i = 0; i < n; i++) {
        msg->m_entries[i].m_id = m_conn[i].m_id;
        ClearString(msg->m_entries[i].m_name);
        m_conn[i].m_name[0x1f] = 0;
        AppendString(msg->m_entries[i].m_name, m_conn[i].m_name);
        msg->m_entries[i].m_flags = m_conn[i].m_flags;
    }
    send_packet_type(msg, size, 0, 0x10, 1);
    if (msg)
        FreeIfNotNull(msg);
}

int Net::StartPacketVoice(int arg)
{
    if (g_directPlay == 0)
        return 7;
    if ((m_state & (NET_LIVE | NET_THROTTLED)) == 0) {
        m_voiceSendMode = 0;
        m_voiceDest = arg;
        m_outbox.start();
        m_state |= NET_LIVE;
    }
    return 0;
}

int Net::start_voice(int arg)
{
    if (g_directPlay == 0)
        return 7;
    if ((m_state & (NET_LIVE | NET_THROTTLED)) == 0) {
        m_voiceSendMode = 1;
        m_voiceDest = arg;
        m_outbox.start();
        m_state |= NET_LIVE;
    }
    return 0;
}

void Net::stop_voice()
{
    if (m_state & NET_LIVE) {
        m_outbox.stop();
        m_state &= ~NET_LIVE;
        send_packet_type(0, 0, m_voiceDest, 0x200, 0);
    }
}

#include <process.h>

static void __cdecl NetworkReceiveThread(void *arglist);

int NetworkOpenSession()
{
    Net *net = g_pNetwork;
    unsigned long thread;

    net->m_threadId = GetCurrentThreadId();
    InitializeCriticalSection(&net->m_lock);
    net->m_lockReady = 1;
    net->m_ackEvent = CreateEventA(0, FALSE, FALSE, 0);
    net->m_msgEvent = CreateEventA(0, FALSE, FALSE, 0);
    net->m_recvEvent = CreateEventA(0, FALSE, FALSE, 0);
    net->m_wake = CreateEventA(0, FALSE, FALSE, 0);
    net->m_event24 = CreateEventA(0, FALSE, FALSE, 0);
    thread = _beginthread(NetworkReceiveThread, 0, 0);
    if (thread == -1)
        return 1;
    SetThreadPriority((HANDLE)thread, THREAD_PRIORITY_ABOVE_NORMAL);
    return 0;
}

void Net::close()
{
    m_outbox.unload();

    m_timeout = NET_DROP_TOLERANCE_MS(20);
    m_localId = 0;
    m_hostId = 0;
    m_lastPoll = 0;
    m_playerCount = 0;

    if (g_directPlay != 0) {
        EnterCriticalSection(&m_lock);
        SetEvent(m_wake);
        g_directPlay->Close();
        g_directPlay->Release();
        g_directPlay = 0;
        m_directPlay = 0;
        LeaveCriticalSection(&m_lock);
        Sleep(0);
        EnterCriticalSection(&m_lock);
        LeaveCriticalSection(&m_lock);
        m_self = 0;
        m_threadBuf = 0;
        m_threadId = 0;
    }

    if (m_lockReady != 0) {
        m_lockReady = 0;
        DeleteCriticalSection(&m_lock);
    }

    if (m_ackEvent != 0) {
        CloseHandle(m_ackEvent);
        m_ackEvent = 0;
    }
    if (m_msgEvent != 0) {
        CloseHandle(m_msgEvent);
        m_msgEvent = 0;
    }
    if (m_recvEvent != 0) {
        CloseHandle(m_recvEvent);
        m_recvEvent = 0;
    }
    if (m_wake != 0) {
        CloseHandle(m_wake);
        m_wake = 0;
    }
    if (m_event24 != 0) {
        CloseHandle(m_event24);
        m_event24 = 0;
    }

    if (m_sessionName != 0) {
        FreeIfNotNull(m_sessionName);
        m_sessionName = 0;
    }
    if (m_sessionPassword != 0) {
        FreeIfNotNull(m_sessionPassword);
        m_sessionPassword = 0;
    }
    if (m_recvBuf != 0) {
        FreeIfNotNull(m_recvBuf);
        m_recvBuf = 0;
    }
    if (m_msgBuf != 0) {
        free(m_msgBuf);
        m_msgBuf = 0;
    }
    m_msgBufSize = 0;

    m_replyTimeout = 200;
    m_state = 0;

    for (int i = 0; i < NET_MAX_CONNECTIONS; i++) {
        m_conn[i].m_id = 0;
        m_conn[i].m_guaranteedCount = 0;
        m_conn[i].m_sending = 0;
        m_conn[i].m_flags = 0;
    }

    m_peers.remove_all();

    m_voiceDest = 0;
    m_state = 0;
    m_queueF8.close();
    m_loopback.close();
    m_inbox.close();

    memset(&m_spGuid, 0, sizeof m_spGuid);
    m_seq = 1;
    m_startKey = 0;
    m_startVoiceKey = 0;
    m_startArg = 0;
    m_voiceSendMode = 0;
}

BOOL FAR PASCAL players_in_group_callback(DPID id, DWORD type, LPCDPNAME name, DWORD flags, LPVOID context)
{
    int i;
    if (g_enumConn == 0)
        return FALSE;
    for (i = 0; i < NET_MAX_CONNECTIONS; i++) {
        if (g_enumConn[i].m_id == id) {
            if (g_enumConn[i].m_flags & 2)
                g_enumConn[i].m_sending = 1;
            break;
        }
    }
    return TRUE;
}


int Net::SendMessageWithRetry(void *data, int len, int dest, int type, int flags)
{
    BasePop dlg;
    unsigned timeout[NET_MAX_CONNECTIONS];
    int waiting[NET_MAX_CONNECTIONS];
    DWORD sent[NET_MAX_CONNECTIONS];
    HANDLE events[2];
    NetMessage *pkt = 0;
    int size;
    DWORD wait;
    DWORD start;
    DWORD now;
    int pending;
    int i;
    int reliable;
    int choice;

    events[0] = m_ackEvent;
    events[1] = m_msgEvent;

    if (!IsActive())
        return 0;
    if (len < 0 || len > m_maxRetryLen)
        return 0;

    check_polling();

    if ((type & 0xfff0) == 0 && ((m_state & 1) || (flags & 4))) {
        if (dest == m_localId) {
            m_loopback.add(data, len, m_localId, 0, -1);
            goto sent;
        }
        if (dest == 0) {
            m_loopback.add(data, len, m_localId, 0, -1);
            if (m_playerCount < 2)
                goto sent;
        }
    }

    size = len + 12;
    pkt = (NetMessage *)AllocateBytes(size);
    if (pkt == 0)
        return 0;

    reliable = flags & 1;
    if (reliable)
        pkt->m_seq = ++m_seq;
    else
        pkt->m_seq = 0;
    if (data)
        memcpy(&pkt->m_leaverId, data, len);

    if (!reliable || (m_state & 0x10000000)) {
        pkt->m_type = (unsigned short)type;
        DWORD sendFlags = reliable ? DPSEND_GUARANTEED : 0;
        if (g_directPlay == 0) {
            FreeIfNotNull(pkt);
            pkt = 0;
            return 0;
        }
        if (g_directPlay->Send(m_localId, dest, sendFlags, pkt, size)) {
            FreeIfNotNull(pkt);
            pkt = 0;
            return 0;
        }
    } else {
        pkt->m_type = 4;
        pkt->m_type |= type;
        for (i = 0; i < NET_MAX_CONNECTIONS; i++)
            m_conn[i].m_sending = 0;
        g_enumConn = m_conn;
        if (g_directPlay == 0) {
            FreeIfNotNull(pkt);
            pkt = 0;
            return 0;
        }
        g_directPlay->EnumGroupPlayers(dest, 0, players_in_group_callback, 0, 0);

        wait = 0xffffffff;
        for (i = 0; i < NET_MAX_CONNECTIONS; i++) {
            if (m_conn[i].m_sending) {
                waiting[i] = 1;
                timeout[i] = m_conn[i].m_avgReplyTime;
                if (timeout[i] < wait)
                    wait = timeout[i];

                for (;;) {
                    if (g_directPlay == 0) {
                        FreeIfNotNull(pkt);
                        pkt = 0;
                        return 0;
                    }
                    sent[0] = GetMilliseconds();
                    for (int k = 1; k < NET_MAX_CONNECTIONS; k++)
                        sent[k] = sent[0];
                    if (g_directPlay->Send(m_localId, dest, 0, pkt, size)) {
                        FreeIfNotNull(pkt);
                        pkt = 0;
                        return 0;
                    }
                    start = GetMilliseconds();
                    while (GetMilliseconds() - start < 20000) {
                        ResetEvent(m_msgEvent);
                        ResetEvent(m_ackEvent);
                        if (WaitForMultipleObjects(2, events, FALSE, wait) == 1)
                            handle_sys_msg();
                        now = GetMilliseconds();
                        pending = 0;
                        for (int j = 0; j < NET_MAX_CONNECTIONS; j++) {
                            if (waiting[j]) {
                                if (m_conn[j].m_id == 0) {
                                    waiting[j] = 0;
                                } else if (m_conn[j].m_sending == 0) {
                                    m_conn[j].m_avgReplyTime += GetMilliseconds() - sent[j] + 20;
                                    m_conn[j].m_avgReplyTime = m_conn[j].m_avgReplyTime >> 1;
                                    waiting[j] = 0;
                                } else {
                                    if (now - sent[j] > timeout[j]) {
                                        timeout[j] = timeout[j] * 3;
                                        timeout[j] = timeout[j] >> 1;
                                        if (g_directPlay == 0) {
                                            FreeIfNotNull(pkt);
                                            pkt = 0;
                                            return 0;
                                        }
                                        sent[j] = GetMilliseconds();
                                        pending = g_directPlay->Send(m_localId, m_conn[j].m_id, 0, pkt, size);
                                        if (pending) {
                                            MessageBoxA(0, "Oh no!! Send failed!", "Message from the Net Underworld",
                                                0);
                                            FreeIfNotNull(pkt);
                                            pkt = 0;
                                            return 0;
                                        }
                                    }
                                    pending = 1;
                                }
                            }
                        }
                        if (!pending)
                            goto sent;
                    }

                    ClearHeapMessage();
                    int first = 1;
                    for (int n = 0; n < NET_MAX_CONNECTIONS; n++) {
                        if (m_conn[n].m_sending) {
                            AppendHeapMessage(get_player_name(m_conn[n].m_id));
                            if (!first) {
                                AppendHeapMessageComma();
                                AppendHeapMessageSpace();
                            }
                            first = 0;
                        }
                    }
                    AppendHeapMessage(".");
                    parse_says(0, msg, -1, -1);
                    dlg.start("jackal", "NET_SENDTIMEDOUT", -1, 0, 0, 0);
                    choice = dlg.exec(0, 0);
                    switch (choice) {
                    case 0: continue;
                    case 1: close(); break;
                    case 2: quit = 1; break;
                    }
                    FreeIfNotNull(pkt);
                    pkt = 0;
                    return 0;
                }
            }
        }
    }
sent:
    if (pkt) {
        FreeIfNotNull(pkt);
        pkt = 0;
    }
    return m_seq;
}

int Net::send_packet_type(void *data, int len, int dest, int type, int flags)
{
    BasePop dlg;
    int waiting[NET_MAX_CONNECTIONS];
    unsigned timeout[NET_MAX_CONNECTIONS];
    DWORD sent[NET_MAX_CONNECTIONS];
    HANDLE events[2];
    NetMessage *pkt = 0;
    int size;
    DWORD wait;
    DWORD start;
    DWORD now;
    int target;
    int any;
    int i;
    int reliable;
    int choice;

    events[0] = m_ackEvent;
    events[1] = m_msgEvent;

    if (!IsActive())
        return 0;
    if (len < 0 || len > m_msgBufSize)
        return 0;

    check_polling();

    if ((type & 0xfff0) == 0 && (flags & 4)) {
        if (dest == m_localId) {
            m_loopback.add(data, len, m_localId, 0, -1);
            goto sent;
        }
        if (dest == 0) {
            m_loopback.add(data, len, m_localId, 0, -1);
            if (m_playerCount < 2)
                goto sent;
        }
    }

    size = len + 12;
    pkt = (NetMessage *)AllocateBytes(size);
    if (pkt == 0)
        return 0;

    reliable = flags & 1;
    if (reliable)
        pkt->m_seq = ++m_seq;
    else
        pkt->m_seq = 0;
    if (data)
        memcpy(&pkt->m_leaverId, data, len);

    if (!reliable || (m_state & 0x10000000)) {
        pkt->m_type = (unsigned short)type;
        DWORD sendFlags = reliable ? DPSEND_GUARANTEED : 0;
        if (g_directPlay == 0) {
            FreeIfNotNull(pkt);
            pkt = 0;
            return 0;
        }
        if (g_directPlay->Send(m_localId, dest, sendFlags, pkt, size)) {
            FreeIfNotNull(pkt);
            pkt = 0;
            return 0;
        }
        goto sent;
    }

    pkt->m_type = 4;
    pkt->m_type |= type;

    if (dest != 0) {
        wait = 0xffffffff;
        any = 0;
        for (i = 0; i < NET_MAX_CONNECTIONS; i++) {
            if (m_conn[i].m_id == dest && ((m_conn[i].m_flags & 2) || (type & 0x10))) {
                timeout[i] = m_conn[i].m_avgReplyTime;
                m_conn[i].m_sending = any = 1;
                target = i;
                if (m_conn[i].m_avgReplyTime < wait)
                    wait = m_conn[i].m_avgReplyTime;
            } else {
                m_conn[i].m_sending = 0;
            }
        }
        if (!any)
            goto sent;

        for (;;) {
            sent[target] = GetMilliseconds();
            if (g_directPlay->Send(m_localId, dest, 0, pkt, size)) {
                MessageBoxA(0, "Oh no!! Send failed!", "Message from the Net Underworld", 0);
                FreeIfNotNull(pkt);
                pkt = 0;
                return 0;
            }
            start = GetMilliseconds();
            while (GetMilliseconds() - start < 20000) {
                ResetEvent(m_msgEvent);
                ResetEvent(m_ackEvent);
                if (WaitForMultipleObjects(2, events, FALSE, wait) == 1)
                    handle_sys_msg();
                if (m_conn[target].m_sending == 0 || m_conn[target].m_id == 0)
                    goto acked;
                if (GetMilliseconds() - sent[target] > timeout[target]) {
                    timeout[target] = timeout[target] * 3 >> 1;
                    if (g_directPlay == 0) {
                        FreeIfNotNull(pkt);
                        pkt = 0;
                        return 0;
                    }
                    sent[target] = GetMilliseconds();
                    if (g_directPlay->Send(m_localId, m_conn[target].m_id, 0, pkt, size)) {
                        MessageBoxA(0, "Oh no!! Send failed!", "Message from the Net Underworld", 0);
                        FreeIfNotNull(pkt);
                        pkt = 0;
                        return 0;
                    }
                }
            }

            ClearHeapMessage();
            strcat(msg, get_player_name(m_conn[target].m_id));
            AppendHeapMessage(".");
            parse_says(0, msg, -1, -1);
            dlg.start("jackal", "NET_SENDTIMEDOUT", -1, 0, 0, 0);
            choice = dlg.exec(0, 0);
            switch (choice) {
            case 0: continue;
            case 1: close(); break;
            case 2: quit = 1; break;
            }
            FreeIfNotNull(pkt);
            pkt = 0;
            return 0;
        }
    }

    wait = 0xffffffff;
    any = 0;
    for (i = 0; i < NET_MAX_CONNECTIONS; i++) {
        NetConnection *conn = &m_conn[i];
        if (conn->m_id != 0 && conn->m_id != m_localId && ((conn->m_flags & 2) || (type & 0x10))) {
            m_conn[i].m_sending = 1;
            waiting[i] = 1;
            timeout[i] = conn->m_avgReplyTime;
            any = 1;
            if (conn->m_avgReplyTime < wait)
                wait = conn->m_avgReplyTime;
        } else {
            conn->m_sending = 0;
            waiting[i] = 0;
        }
    }
    if (!any)
        goto sent;

    for (;;) {
        if (g_directPlay == 0) {
            FreeIfNotNull(pkt);
            pkt = 0;
            return 0;
        }
        sent[0] = GetMilliseconds();
        for (int k = 1; k < NET_MAX_CONNECTIONS; k++)
            sent[k] = sent[0];
        if (g_directPlay->Send(m_localId, 0, 0, pkt, size)) {
            MessageBoxA(0, "Oh no!! Send failed!", "Message from the Net Underworld", 0);
            FreeIfNotNull(pkt);
            pkt = 0;
            return 0;
        }
        start = GetMilliseconds();
        while (GetMilliseconds() - start < 20000) {
            ResetEvent(m_msgEvent);
            ResetEvent(m_ackEvent);
            if (WaitForMultipleObjects(2, events, FALSE, wait) == 1)
                handle_sys_msg();
            now = GetMilliseconds();
            any = 0;
            for (int j = 0; j < NET_MAX_CONNECTIONS; j++) {
                if (waiting[j]) {
                    if (m_conn[j].m_id == 0) {
                        waiting[j] = 0;
                    } else if (m_conn[j].m_sending == 0) {
                        m_conn[j].m_avgReplyTime = (GetMilliseconds() + m_conn[j].m_avgReplyTime - sent[j] + 20) >> 1;
                        waiting[j] = 0;
                    } else {
                        if (now - sent[j] > timeout[j]) {
                            timeout[j] = timeout[j] * 3 >> 1;
                            if (g_directPlay == 0) {
                                FreeIfNotNull(pkt);
                                pkt = 0;
                                return 0;
                            }
                            sent[j] = GetMilliseconds();
                            if (g_directPlay->Send(m_localId, m_conn[j].m_id, 0, pkt, size)) {
                                MessageBoxA(0, "Oh no!! Send failed!", "Message from the Net Underworld", 0);
                                FreeIfNotNull(pkt);
                                pkt = 0;
                                return 0;
                            }
                        }
                        any = 1;
                    }
                }
            }
            if (!any)
                goto sent;
        }

        ClearHeapMessage();
        int first = 1;
        for (int n = 0; n < NET_MAX_CONNECTIONS; n++) {
            if (m_conn[n].m_sending) {
                AppendHeapMessage(get_player_name(m_conn[n].m_id));
                if (!first) {
                    AppendHeapMessageComma();
                    AppendHeapMessageSpace();
                }
                first = 0;
            }
        }
        AppendHeapMessage(".");
        parse_says(0, msg, -1, -1);
        dlg.start("jackal", "NET_SENDTIMEDOUT", -1, 0, 0, 0);
        choice = dlg.exec(0, 0);
        switch (choice) {
        case 0: continue;
        case 1: close(); break;
        case 2: quit = 1; break;
        }
        MessageBoxA(0, "Oh no!! Send failed after many tries!", "Message from the Net Underworld", 0);
        FreeIfNotNull(pkt);
        pkt = 0;
        return 0;
    }

acked:
    {
        unsigned &avg = m_conn[target].m_avgReplyTime;
        avg = (GetMilliseconds() + avg - sent[target] + 20) >> 1;
    }
sent:
    if (pkt) {
        FreeIfNotNull(pkt);
        pkt = 0;
    }
    return m_seq;
}

char *Net::get_player_name(int id)
{
    int i;
    for (i = 0; i < NET_MAX_CONNECTIONS; i++)
        if (m_conn[i].m_id == id)
            break;
    if (i == NET_MAX_CONNECTIONS)
        return 0;
    return m_conn[i].m_name;
}

void Net::process_voice()
{
    if (m_state & NET_THROTTLED) {
        if (m_outbox.get_nbuffers() >= NET_SEND_RELEASE)
            return;
        m_state &= ~NET_THROTTLED;
        return;
    }
    if (m_outbox.get_nbuffers() > NET_SEND_THROTTLE) {
        m_state |= NET_THROTTLED;
        m_outbox.stop();
        return;
    }
    MMIOINFO *buf = m_outbox.get_next_buffer();
    if (buf == 0)
        return;
    if (m_voiceSendMode != 0)
        SendMessageWithRetry(buf->pchBuffer, buf->pchEndRead - buf->pchBuffer, m_voiceDest, 0x100, 0);
    else
        send_packet_type(buf->pchBuffer, buf->pchEndRead - buf->pchBuffer, m_voiceDest, 0x100, 0);
    m_outbox.return_buffer(buf);
}

void Net::check_polling()
{
    if (g_directPlay == 0)
        return;
    if (GetMilliseconds() - 2000 <= m_lastPoll)
        return;
    if (g_keepAliveBusy != 0)
        return;
    g_keepAliveBusy = 1;
    send_packet_type(0, 0, 0, 0x20, 0);
    m_lastPoll = GetMilliseconds();
    g_keepAliveBusy = 0;
    if (g_sweepBusy != 0)
        return;
    g_sweepBusy = 1;
    if (m_state & NET_POLLING) {
        if (IsHost()) {
            NetConnection *conn = m_conn;
            int n;
            for (n = NET_MAX_CONNECTIONS; n; n--) {
                if (conn->m_id != 0 && conn->m_id != m_localId) {
                    unsigned heard = conn->m_lastTime;
                    if (heard != 0 && GetMilliseconds() - heard > m_timeout) {
                        if (drop_player(conn->m_id, 0) == 0)
                            conn->m_lastTime = GetMilliseconds();
                    }
                }
                conn++;
            }
        } else if (m_localId == m_conn[1].m_id) {
            unsigned heard = m_conn[0].m_lastTime;
            if (heard != 0 && GetMilliseconds() - heard > m_timeout) {
                if (drop_player(m_conn[0].m_id, 0) == 0)
                    m_conn[0].m_lastTime = GetMilliseconds();
            }
        } else {
            int i;
            for (i = 0; i < 2; i++) {
                unsigned heard = m_conn[i].m_lastTime;
                if (heard == 0)
                    return;
                if (GetMilliseconds() - heard < m_timeout)
                    return;
            }
            if (drop_player(-1, 0) == 0)
                m_conn[0].m_lastTime = m_conn[1].m_lastTime = GetMilliseconds();
        }
    }
    g_sweepBusy = 0;
}

int Net::drop_player(int id, int notify)
{
    BasePop dlg;
    int i;
    int choice = 0;

    for (i = 0; i < NET_MAX_CONNECTIONS; i++) {
        if (id == m_conn[i].m_id)
            break;
    }
    if (i == NET_MAX_CONNECTIONS)
        return 0;

    if (!notify) {
        if (!IsHost() && !IsBackupHost()) {
            parse_says(0, get_player_name(m_conn[0].m_id), -1, -1);
            parse_says(1, get_player_name(m_conn[1].m_id), -1, -1);
            dlg.start("jackal", "NET_OKTODROPHOSTS", 1, 0, 0, 0);
            choice = dlg.exec();
            switch (choice) {
            case 0: quit = 1; return 1;
            case 1: return 0;
            case 2:
                dlg.start("jackal", "NET_ADJUSTTOLERANCE", -1, 0, 0, 0);
                choice = dlg.exec();
                SetTolerance(choice);
                return 0;
            default: return 0;
            }
        } else {
            parse_says(0, get_player_name(id), -1, -1);
            dlg.start("jackal", IsHost() ? "NET_OKTODROPCLIENT" : "NET_OKTODROPHOST", 1, 0, 0);
            choice = dlg.exec();
            switch (choice) {
            case 1: return 0;
            case 2:
                dlg.start("jackal", "NET_ADJUSTTOLERANCE", -1, 0, 0);
                choice = dlg.exec();
                SetTolerance(choice);
                return 0;
            }
        }
    }

    char buf[256];
    ClearString(buf);
    for (i = 0; i < NET_MAX_CONNECTIONS; i++) {
        if (m_conn[i].m_id == id) {
            AppendString(buf, m_conn[i].m_name);
            for (; i < NET_MAX_CONNECTIONS - 1; i++)
                m_conn[i] = m_conn[i + 1];
            break;
        }
    }
    if (id == m_hostId) {
        prev_server_id = m_hostId;
        m_hostId = m_conn[0].m_id;
        on_host_inherited();
    }
    m_playerCount--;
    on_player_deleted(buf, id);
    m_peers.remove(id);
    if (!notify)
        send_packet_type(&id, 4, 0, 0x80, 1);
    return 1;
}

void Net::add_player(const char *name, int id, int flags)
{
    int i;

    for (i = 0; i < NET_MAX_CONNECTIONS; i++) {
        if (m_conn[i].m_id == 0)
            break;
        if (m_conn[i].m_id == id)
            return;
    }
    if (i < NET_MAX_CONNECTIONS) {
        m_conn[i].m_id = id;
        m_conn[i].m_flags = flags;
        ClearString(m_conn[i].m_name);
        AppendString(m_conn[i].m_name, name);
        if (flags & 1)
            m_hostId = id;
    }
    ++m_playerCount;
    if (IsHost())
        send_player_order();
    on_player_added(m_conn[i].m_name, m_conn[i].m_id);
    m_peers.add(m_conn[i].m_id);
    JackalVoiceRx *voice = m_peers.current_entry();
    voice->init(0x1000, 0, 0);
}

void Net::handle_sys_msg()
{
    if (g_directPlay == 0)
        return;

    void *buf = m_msgBuf;
    if (buf == 0)
        return;

    int from;
    DWORD size;

    while (m_inbox.get(buf, &from, 0, 0)) {
        if (from != DPID_SYSMSG) {
            NetMessage *msg = (NetMessage *)buf;
            unsigned short type = msg->m_type;
            if (type & 0x80) {
                drop_player(msg->m_leaverId, 1);
            } else if (type & 0x10) {
                int i;
                for (i = 0; i < msg->m_entryCount; i++)
                    add_player(msg->m_entries[i].m_name, msg->m_entries[i].m_id, msg->m_entries[i].m_flags);
            }
        } else {
            DPMSG_GENERIC *sys = (DPMSG_GENERIC *)buf;
            switch (sys->dwType) {
            case DPSYS_CREATEPLAYERORGROUP: {
                DPMSG_CREATEPLAYERORGROUP *m = (DPMSG_CREATEPLAYERORGROUP *)sys;
                if (IsHost() && m->dwPlayerType == DPPLAYERTYPE_PLAYER) {
                    size = sizeof g_playerNameBuf;
                    g_directPlay->GetPlayerName(m->dpId, g_playerNameBuf, &size);
                    int flags = m->lpData ? *(unsigned char *)m->lpData : 0;
                    add_player(((PlayerNameBuffer *)g_playerNameBuf)->m_name.lpszLongNameA, m->dpId, flags);
                }
                break;
            }
            case DPSYS_DESTROYPLAYERORGROUP: {
                DPMSG_DESTROYPLAYERORGROUP *m = (DPMSG_DESTROYPLAYERORGROUP *)sys;
                if (m->dwPlayerType == DPPLAYERTYPE_PLAYER)
                    drop_player(m->dpId, 1);
                break;
            }
            case DPSYS_SESSIONLOST:
                on_session_lost();
                close();
                break;
            case DPSYS_SETPLAYERORGROUPDATA: {
                DPMSG_SETPLAYERORGROUPDATA *m = (DPMSG_SETPLAYERORGROUPDATA *)sys;
                int i;
                for (i = 0; i < NET_MAX_CONNECTIONS; i++)
                    if (m_conn[i].m_id == (int)m->dpId)
                        break;
                if (i != NET_MAX_CONNECTIONS && m->lpData != 0)
                    m_conn[i].m_flags = *(unsigned char *)m->lpData;
                break;
            }
            case DPSYS_SETPLAYERORGROUPNAME: {
                DPMSG_SETPLAYERORGROUPNAME *m = (DPMSG_SETPLAYERORGROUPNAME *)sys;
                on_name_changed(m->dpnName.lpszLongNameA, m->dpId);
                break;
            }
            }
        }
    }
}

static void __cdecl NetworkReceiveThread(void *arglist)
{
    Net *net = g_pNetwork;
    HANDLE events[2];
    DPID from, to;
    DWORD size;

    events[0] = net->m_recvEvent;
    events[1] = net->m_wake;
    while (WaitForMultipleObjects(2, events, FALSE, INFINITE) == WAIT_OBJECT_0) {
        EnterCriticalSection(&net->m_lock);
        if (g_directPlay != 0) {
            size = net->m_maxRetryLen;
            while (g_directPlay->Receive(&from, &to, DPRECEIVE_ALL, net->m_threadBuf, &size) == DP_OK) {
                net->m_inbox.add(net->m_threadBuf, size, from, to, -1);
                SetEvent(net->m_msgEvent);
                size = net->m_maxRetryLen;
            }
        }
        LeaveCriticalSection(&net->m_lock);
    }
    SetEvent(net->m_event24);
}

static int s_unused0 = 0;
NetConnection *g_enumConn = 0;
static struct {
    int words[11];
} s_unused1 = {0};
char g_playerNameBuf[0x100] = {0};
static int s_unused2[4] = {0};
int prev_server_id = 0;
IDirectPlay2A *g_directPlay = 0;
static int s_unused3 = 0;
Net *g_pNetwork = 0;
int g_sweepBusy = 0;
int g_keepAliveBusy = 0;

#include <dplay.h>
#include <dplobby.h>

HRESULT CreateDirectPlay(LPGUID provider, LPDIRECTPLAY2A *dp)
{
    LPDIRECTPLAY dp1 = NULL;
    HRESULT hr = DirectPlayCreate(provider, &dp1, NULL);
    if (FAILED(hr))
        return hr;
    hr = dp1->QueryInterface(IID_IDirectPlay2A, (LPVOID *)dp);
    dp1->Release();
    return hr;
}

BOOL FAR PASCAL ReadINetAddress(REFGUID dataType, DWORD size, LPCVOID data, LPVOID host)
{
    if (dataType == DPAID_INet) {
        memcpy(host, data, size);
        return FALSE;
    }
    return TRUE;
}

const GUID *ConnectionProvider(int type)
{
    switch (type) {
    case 0: return &DPSPGUID_TCPIP;
    case 1: return &DPSPGUID_MODEM;
    case 2: return &DPSPGUID_SERIAL;
    }
    return NULL;
}

BOOL GetLobbyHostAddress(char *host)
{
    LPDIRECTPLAYLOBBYA lobby = NULL;
    LPDPLCONNECTION settings;
    DWORD size = 0;
    BOOL found = FALSE;

    if (FAILED(DirectPlayLobbyCreateA(NULL, &lobby, NULL, NULL, 0)))
        return FALSE;
    if (lobby->GetConnectionSettings(0, NULL, &size) == DPERR_BUFFERTOOSMALL) {
        settings = (LPDPLCONNECTION)AllocateBytes(size);
        if (settings != NULL) {
            if (lobby->GetConnectionSettings(0, settings, &size) == DP_OK) {
                host[0] = 0;
                lobby->EnumAddress(ReadINetAddress, settings->lpAddress, settings->dwAddressSize, host);
                found = host[0] != 0;
            }
            FreeIfNotNull(settings);
        }
    }
    lobby->Release();
    return found;
}

struct ProviderQuery {
    const GUID *provider;
    BOOL installed;
};

BOOL FAR PASCAL MatchProvider(LPGUID provider, LPSTR name, DWORD major, DWORD minor, LPVOID context)
{
    ProviderQuery *query = (ProviderQuery *)context;
    if (*provider == *query->provider) {
        query->installed = TRUE;
        return FALSE;
    }
    return TRUE;
}

BOOL ProviderInstalled(int type)
{
    ProviderQuery query;
    query.provider = ConnectionProvider(type);
    query.installed = FALSE;
    if (query.provider != NULL)
        DirectPlayEnumerateA(MatchProvider, &query);
    return query.installed;
}

GUID g_sessionGuid = {0x02d1b480, 0xd680, 0x11d0, {0x82, 0x1e, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
GUID g_lobbyGuid = {0xd8d29744, 0x208a, 0x11d0, {0xbc, 0x9d, 0x00, 0xa0, 0x24, 0x29, 0x67, 0xb6}};
