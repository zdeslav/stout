#include "stdafx.h"
#include "metrics.h"
#include "string.h"
#include "stdlib.h"
#include <cstdarg>

#define thread_local __declspec( thread )

namespace metrics
{
    client_config g_client;

    void ensure_winsock_started()
    {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        bool started = s != INVALID_SOCKET || WSAGetLastError() != WSANOTINITIALISED;
        if (s != INVALID_SOCKET) closesocket(s);
        if (started) return;

        WORD wVersionRequested = MAKEWORD(2, 2);
        WSADATA wsaData;

        int err = WSAStartup(wVersionRequested, &wsaData);
        if (err != 0)
        {
            char msg[256];
            _snprintf_s(msg, _countof(msg), _TRUNCATE, "WSAStartup failed with error: %d", err);
            dbg_print(msg);
            throw config_exception(msg);
        }
    }


    timer::time_point timer::now(){ return GetTickCount(); }
    timer::duration timer::since(int when){ return now() - when; }
    std::string timer::to_string(timer::time_point time)
    { 
        auto diff = now() - time;
        FILETIME tm;
        SYSTEMTIME st;

        GetLocalTime(&st);
        SystemTimeToFileTime(&st, &tm);

        _ULARGE_INTEGER ui;
        ui.LowPart = tm.dwLowDateTime;
        ui.HighPart = tm.dwHighDateTime;
        ULONGLONG back = diff * 1000;
        ui.QuadPart = ui.QuadPart - back;
        tm.dwLowDateTime = ui.LowPart;
        tm.dwHighDateTime = ui.HighPart;
        FileTimeToSystemTime(&tm, &st);
        char txt[256];
        sprintf_s(txt, "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        return txt; 
    }

    client_config& setup_client(const std::string& server, unsigned int port)
    {
        if (server.size() < 1)  throw config_exception("specified server can't be an empty string");

        ensure_winsock_started();

        g_client.m_server = server;
        g_client.m_port = port;

        // let's cache the server address for later use
        memset((char*)&g_client.m_svr_address, 0, sizeof(g_client.m_svr_address));
        g_client.m_svr_address.sin_family = AF_INET;
        g_client.m_svr_address.sin_port = htons(g_client.m_port);

        struct hostent* hp = gethostbyname(g_client.m_server.c_str());
        if (!hp) {
            char msg[256];
            auto fmt = "Could not obtain address of %s. Error: %d";
            auto server = g_client.m_server.c_str();
            _snprintf_s(msg, _countof(msg), _TRUNCATE, fmt, server, WSAGetLastError());
            dbg_print(msg);
            throw config_exception(msg);
        }

        memcpy((void *)&g_client.m_svr_address.sin_addr, hp->h_addr_list[0], hp->h_length);
        return g_client;
    }

    client_config::client_config() :
        m_debug(false),
        m_defaults_period(60),
        m_default_metrics(none),
        m_port(0),
        m_namespace("stats")
    {;}

    client_config& client_config::set_debug(bool debug) {
        m_debug = debug;
        return *this;
    }
    client_config& client_config::track_default_metrics(builtin_metric which, unsigned int period) {
        if (period < 1)  throw config_exception("specified period must be greater than 0");
        m_defaults_period = period;
        m_default_metrics = which;
        return *this;
    }
    client_config& client_config::set_namespace(const std::string& ns) {
        m_namespace = ns;
        return *this;
    }

    bool client_config::is_debug() const { return m_debug; }
    const char* client_config::get_namespace() const { return m_namespace.c_str(); }

    const char* const fmt(metric_type m) 
    {
        switch (m) {
            case histogram: return "%s.%s:%d|ms";
            case gauge: return "%s.%s:%d|g";
            case gauge_delta: return "%s.%s:%+d|g";
            case counter: return "%s.%s:%d|c";
            default: throw std::runtime_error("unsupported metric type");
        }
    }

    void send_to_server(const char* txt, size_t len)
    {
        thread_local static SOCKET fd = INVALID_SOCKET;

        if (fd == INVALID_SOCKET) {
            fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd == INVALID_SOCKET) { // create a UDP socket
                dbg_print("cannot create client socket: error: %d", WSAGetLastError());
                return;
            }
        }

        /* send a message to the server */   
        const sockaddr* paddr = (sockaddr*) g_client.server_address();
        if (sendto(fd, txt, len, 0, paddr, sizeof(*g_client.server_address())) == SOCKET_ERROR) {
            dbg_print("sendto failed, error: %d", WSAGetLastError());
        }
    }

    inline void dbg_print(const char* fmt, ...) {
        if (!g_client.is_debug()) return;

        printf("%d [%d]: ", GetTickCount(), GetCurrentThreadId());
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
    }

    template <metric_type m>
    void signal(const char* metric, int val) {
        char txt[256]; 
        auto ns = g_client.get_namespace();
        int ret = _snprintf_s(txt, _countof(txt), _TRUNCATE, fmt(m), ns, metric, val);

        if (ret < 1) {
            dbg_print("error: metric %s didn't fit", metric);
        }
        else {
            send_to_server(txt, strlen(txt));
            dbg_print("%s", txt);
        }
    }

    auto_timer::auto_timer(METRIC_ID metric) : m_metric(metric), m_started_at(timer::now()) {}
    auto_timer::~auto_timer() { signal<histogram>(m_metric, timer::since(m_started_at)); }

    void inc(METRIC_ID metric, int inc)
    {
        signal<counter>(metric, inc);
    }

    void measure(METRIC_ID metric, int value)
    {
        signal<histogram>(metric, value);
    }

    void set(METRIC_ID metric, unsigned int value)
    {
        signal<gauge>(metric, value);
    }

    void set_delta(METRIC_ID metric, int value)
    {
        signal<gauge_delta>(metric, value);
    }
}