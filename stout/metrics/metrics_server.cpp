#include "stdafx.h"
#include "metrics_server.h"
#include <memory>

namespace metrics
{
    storage g_storage;

    server_config::server_config(unsigned int port) :
        m_port(port),
        m_callback([]{}), // NOP callback
        m_flush_period(60)
    {
        ensure_winsock_started();
    }

    server_config& server_config::flush_every(unsigned int period) {
        if (period < 1 || period > 3600) throw config_exception("Valid flush period is 1-3600 s");

        m_flush_period = period;
        return *this;
    }

    server_config& server_config::pre_flush(FLUSH_FN callback)
    {
        m_callback = callback;
        return *this;
    }

    server_config& server_config::add_server_listener(SERVER_NOTIFICATION_FN callback)
    {
        m_server_cbs.push_back(callback);
        return *this;
    }

    timer_data process_timer(const std::string& name, const std::vector<int>& values)
    {
        timer_data data = { name, values.size(), 0, 0, 0, 0, 0 };   
        if (data.count == 0) return data;

        data.min = values[0];
        data.max = data.min;
        long long square_sum = 0;  // needed for stddev
        FOR_EACH (auto& v, values)
        {
            if (v > data.max) data.max = v;
            if (v < data.min) data.min = v;
            data.sum += v;
            square_sum += v * v;
        }

        data.avg = data.sum / (double)data.count;
        double var = square_sum / (double)data.count - data.avg * data.avg;
        data.stddev = sqrt(var);
        return data;
    }

    stats flush_metrics(const storage& storage, unsigned int period_ms)
    {
        stats stats;
        stats.timestamp = timer::now();

        auto period =  period_ms / 1000.0;

        FOR_EACH (auto& c, storage.counters) stats.counters[c.first] = c.second / period;
        FOR_EACH (auto& g, storage.gauges) stats.gauges[g.first] = g.second;
        FOR_EACH (auto& t, storage.timers) stats.timers[t.first] = process_timer(t.first, t.second);

        return stats; // todo: move
    }

    void process_metric(storage* storage, char* buff, size_t len)
    {
        auto pipe_pos = strrchr(buff, '|');
        auto colon_pos = strrchr(buff, ':');
        if (!colon_pos || !pipe_pos) {
            dbg_print("unknown metric: %s", buff);
            return;
        }

        metric_type metric;
        if (strcmp(pipe_pos, "|h") == 0) metric = histogram;
        else if (strcmp(pipe_pos, "|c") == 0) metric = counter;
        else if (strcmp(pipe_pos, "|ms") == 0) metric = histogram;
        else if (strcmp(pipe_pos, "|g") == 0) { // abs or delta?
            char sign = *(colon_pos + 1);
            metric = (sign == '+' || sign == '-') ? gauge_delta : gauge;
        }
        else {
            dbg_print("unknown metric type: %s", pipe_pos);
            return;
        }

        *pipe_pos = '\0';
        *colon_pos = '\0';
        colon_pos++;
        std::string metric_name = buff;
        int value = atol(colon_pos);

        dbg_print("storing metric %d: %s [%d]", metric, metric_name.c_str(), value);

        switch (metric)
        {
            case metrics::counter:
                storage->counters[metric_name] += value;
                break;
            case metrics::gauge:
                storage->gauges[metric_name] = value;
                break;
            case metrics::gauge_delta:
                storage->gauges[metric_name] += value;
                break;
            case metrics::histogram:
                storage->timers[metric_name].push_back(value);
                break;
        }

        storage->counters[builtin::internal_metrics_count]++;
        storage->gauges[builtin::internal_metrics_last_seen] = timer::now();
    }

    DWORD WINAPI ThreadProc(LPVOID params)
    {     
        const int BUFSIZE = 4096;
        std::unique_ptr<server_config> pcfg(static_cast<server_config*>(params));

        int recvlen, fd;                // # bytes received, our socket
        char buf[BUFSIZE];              // receive buffer 

        if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { // create a UDP socket
            dbg_print("cannot create server socket: error: %d", WSAGetLastError());
            FOR_EACH(auto& cb, pcfg->server_cbs()) cb(StartupFailed);
            return 1;
        }

        auto start = timer::now();         
        SOCK_ADDR_IN myaddr(AF_INET, INADDR_ANY, pcfg->port());

        if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
            dbg_print("bind failed, error: %d", WSAGetLastError());
            closesocket(fd);       
            FOR_EACH(auto& cb, pcfg->server_cbs()) cb(StartupFailed);
            return 1;
        }

        dbg_print("inproc server listening at port %d", pcfg->port());
        
        int maxfd = fd;
        fd_set static_rdset, rdset;
        SOCK_ADDR_IN remaddr;  
        int addrlen = sizeof(remaddr);  // length of addresses 
        timeval timeout = { 0, 250000 };

        FD_ZERO(&static_rdset);
        FD_SET(fd, &static_rdset);

        FOR_EACH(auto& cb, pcfg->server_cbs()) cb(Started);
        while (true) {
            rdset = static_rdset;
            select(maxfd + 1, &rdset, NULL, NULL, &timeout);

            if (FD_ISSET(fd, &rdset)) {
                recvlen = recvfrom(fd, buf, BUFSIZE, 0, (sockaddr*)&remaddr, &addrlen);
                if (recvlen > 0 && recvlen < BUFSIZE) {
                    buf[recvlen] = 0;
                    if (strcmp(buf, "stop") == 0) {
                        dbg_print(" > received STOP cmd, stopping server");
                        closesocket(fd);
                        FOR_EACH(auto& cb, pcfg->server_cbs()) cb(Stopped);
                        return 0;
                    }
                    dbg_print(" > received:%s (%d bytes)", buf, recvlen);
                    process_metric(&g_storage, buf, recvlen);
                }                 
            }

            if (timer::since(start) >= pcfg->flush_period_ms())
            {
                start = timer::now();
                auto& flush_fn = pcfg->flush_fn();
                flush_fn();
                stats stats = flush_metrics(g_storage, pcfg->flush_period_ms());
                g_storage.clear();
                FOR_EACH (auto& backend, pcfg->backends()) backend(stats);
                dbg_print("flush took %d ms", timer::since(start));
            }
        }
    }

    server server::run(const server_config& cfg)
    {
        DWORD thread_id;
        HANDLE h = CreateThread(NULL, 0, ThreadProc, new server_config(cfg), 0, &thread_id);
        if (!h) throw std::runtime_error("Failed creating server thread");
        dbg_print("started inproc server on thread %d", thread_id);
        return server(cfg);
    }

    void server::stop()
    {
        dbg_print("sending stop cmd...");
        const char* cmd = "stop";
        send_to_server(cmd, strlen(cmd));
    }
}