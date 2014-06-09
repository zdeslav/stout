#include "stdafx.h"
#include "monitoring_backend.h"
#include "config.h"
#include <functional>


monitoring_backend::monitoring_backend(const config& cfg) : m_cfg(cfg)
{
    m_baseline.timestamp = 0;
    m_started_at = GetTickCount();
}

monitoring_backend::~monitoring_backend()
{
}

validator create_validator(const watch& watch, const proc_info& proc)
{
    return validator{ watch, proc };
}

void monitoring_backend::operator()(const metrics::stats& stats)
{
    auto delay = GetTickCount() - m_started_at;
    // skip flushing which happens during initial delay and baseline sampling
    // as they won't contain relevant data
    if (delay < (m_cfg.initial_delay() + m_cfg.sampling_time() - 1) * 1000) return;


    if (m_baseline.timestamp == 0) { // save baseline
        m_baseline = stats;
        printf("baseline assessment done. monitoring started...\n");

        return;
    }

    for (const auto& proc : m_cfg.processes()) {
        for (const auto& watch : proc.m_watches) {
            if (watch.failed_already) continue; // to avoid repeating messages
            auto v = create_validator(watch, proc);
            if (!v.validate(m_baseline, stats))
            {
                watch.mark_failed();
                if (m_cfg.error_reaction() == log_it)
                    printf("<<<<<<<<<<<<<<<<<<<<<<<<<\n");
                else
                {
                    printf("Exiting due to error!");
                    exit(0); // todo: signal main thread to stop apps
                }
            }
        }
    }      
}

double get_value(metrics::timer_data data, e_metric_value value_type)
{
    switch (value_type)
    {
        case avg_value: return data.avg;
        case min_value: return data.min;
        case max_value: return data.max;
        case stddev_value:  return data.stddev;
        default: return 0;
    }
}

bool validator::validate(metrics::stats base, metrics::stats current)
{
    std::string prefix = "stout." + proc.id + "." + watch.counter;

    for (const auto& pair : current.timers) {
        auto counter = pair.first;
        auto timer_data = pair.second;
        if (counter.find(prefix) != 0) continue;

        auto base_data = base.timers[counter];
        if (base_data.metric == "") continue; // didn't exist in baseline

        auto base_val = get_value(base_data, watch.value_type);
        auto curr_val = get_value(timer_data, watch.value_type);

        auto diff = curr_val - base_val;
        auto diff_percent = 0.0;
        if (base_val != 0) diff_percent = 100 * diff / base_val;
        auto comparand = (watch.model == absolute) ? diff : diff_percent;
        bool res = (watch.watch_op == operator_lt) ? comparand < watch.operand : comparand > watch.operand;
        if (!res) 
        {
            printf("ERROR: proc %s failed at metric: %s\n",
                   proc.id.c_str(), watch.to_string().c_str());
            printf("       %s baseline: %g -> current: %g\n",
                   counter.c_str(), base_val, curr_val);

            return false;
        }
    }

    return true;

}
