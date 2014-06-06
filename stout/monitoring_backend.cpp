#include "stdafx.h"
#include "monitoring_backend.h"


monitoring_backend::monitoring_backend(const config& cfg) : m_cfg(cfg)
{
    m_baseline.timestamp = 0;
    // prepare validators
}

monitoring_backend::~monitoring_backend()
{
}

void monitoring_backend::operator()(const metrics::stats& stats)
{
    if (m_baseline.timestamp == 0) { // save baseline
        m_baseline = stats;
        return;
    }

    // collect stats
    // run validators on stats

}
