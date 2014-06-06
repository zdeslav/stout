#pragma once

#include <string>
#include <vector>

enum e_error_reaction {
    log_it,
    stop_test
};

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
    std::string counter;
    int operand;
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
    int server_port() const { return 9999; }
    int initial_delay() const { return 5000; }
    int sampling_time() const { return 5000; }
    int testrun_duration() const { return 5; }
    e_error_reaction error_reaction() const { return log_it; }
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

