#pragma once

#include <vector>

class config;

struct process_runtime_info {
    DWORD id;
    std::string symbolic_name;
    std::string process_name;
    std::string process_path;
    HANDLE h_proc;
    HANDLE h_thread;
};

class app_runner
{
public:
    typedef std::vector<process_runtime_info> runtime_list;

    app_runner(const config& cfg);
    ~app_runner();

    const runtime_list& start_apps();
    void stop_apps();
    const runtime_list& processes() const { return m_processes; }

private:
    runtime_list m_processes;
    const config& m_cfg;

};

