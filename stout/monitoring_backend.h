#pragma once

#include "metrics/metrics_server.h"

class config;


class monitoring_backend
{
public:
    monitoring_backend(const config& cfg);
    ~monitoring_backend();
    void operator()(const metrics::stats& stats);

private:
    metrics::stats m_baseline;
    const config& m_cfg;

};

