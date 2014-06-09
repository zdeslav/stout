#pragma once

#include <string>
#include <vector>

enum e_error_reaction {
    log_it,
    stop_test
};

enum e_metric_value {
    avg_value,
    min_value,
    max_value,
    stddev_value
};

const char* value_type_to_string(e_metric_value type);

enum e_watch_model {
    relative,
    absolute
};
enum e_watch_operator {
    operator_lt,
    operator_gt
};


struct watch {  
    e_watch_model model; 
    e_watch_operator watch_op;
    e_metric_value value_type;
    std::string counter;
    int operand;
    mutable bool failed_already;
    std::string to_string()
    {
        char txt[256];
        sprintf_s(txt, "%s.%s %s %d %s",
                  counter.c_str(),
                  value_type_to_string(value_type),
                  (watch_op == operator_lt) ? "<" : ">",
                  operand,
                  (model == relative) ? "%" : " ");
        return txt;
    }

    void mark_failed() const { failed_already = true; }
};

struct backend {
    std::string name;
    std::string args;
};

typedef std::vector<watch> watch_list;

struct proc_info {
    std::string id;
    std::string process_name;  
    int instance_count;
    bool attach;
    watch_list m_watches;
};

class config
{
public:
    typedef std::vector<backend> backend_list;
    typedef std::vector<proc_info> processes_list;

    static config load(const std::string& ini_file);
    int server_port() const { return m_server_port; }
    int initial_delay() const { return m_initial_delay; }
    int sampling_time() const { return m_sampling_time; }
    int testrun_duration() const { return m_testrun_duration; }
    e_error_reaction error_reaction() const { return m_error_reaction; }
    const backend_list& backends() const { return m_backends; }
    const processes_list& processes() const { return m_processes; }

private:
    config(const std::string& ini_file);

    void ParseCommon();
    void ParseBackends();
    void ParseProcessSection(const std::string& section);
    void AddBackend(const std::string& backend, const std::string& args);

    backend_list m_backends;
    processes_list m_processes;
    watch_list m_watches;

    std::string m_ini_file;
    int m_server_port;
    int m_initial_delay;
    int m_sampling_time;
    int m_testrun_duration;
    e_error_reaction m_error_reaction;
};

