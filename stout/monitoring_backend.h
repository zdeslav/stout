#pragma once

#include "config.h"
#include "metrics/metrics_server.h"

class config;

struct validator {
    watch watch;
    proc_info proc;
    bool validate(metrics::stats base, metrics::stats current);
};

class monitoring_backend
{
public:
    monitoring_backend(const config& cfg);
    ~monitoring_backend();
    void operator()(const metrics::stats& stats);

private:
    metrics::stats m_baseline;
    const config& m_cfg;
    int m_started_at;

    bool check(const std::string& which, metrics::timer_data base, metrics::timer_data current);


};

