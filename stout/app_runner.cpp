#include "stdafx.h"
#include "app_runner.h"
#include "config.h"

app_runner::app_runner(const config& cfg) : m_cfg(cfg) {}
app_runner::~app_runner()
{
    stop_apps();
    m_processes.clear();
}

PROCESS_INFORMATION StartProcess(const proc_info proc)
{
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    si.cb = sizeof(si);
    memset(&si, 0, sizeof(si));
    
    BOOL res = CreateProcess(proc.process_name.c_str(),
                  "", // todo: support cmdline
                  NULL,
                  NULL,
                  FALSE,
                  CREATE_NEW_CONSOLE, /* | CREATE_SUSPENDED */
                  NULL,
                  NULL, // todo: CWD setting in .ini?
                  &si,
                  &pi
                  );
    if (!res) {
        char txt[256];
        sprintf_s(txt, "Startup of app %s failed with error %d",
                  proc.process_name.c_str(), GetLastError());
        throw stout_exception(txt);
    }
    return pi;

}

const app_runner::runtime_list& app_runner::start_apps()
{                        
    for (const auto& app : m_cfg.processes())
    {
        for (int i = 0; i < app.instance_count; ++i)
        {
            PROCESS_INFORMATION data = StartProcess(app);
            process_runtime_info proc;
            proc.symbolic_name = app.id; 
            proc.process_name = app.process_name;
            proc.process_path = app.process_name;
            proc.id = data.dwProcessId;
            proc.h_proc = data.hProcess;
            proc.h_thread = data.hThread;
            m_processes.push_back(proc);
        }                              
    }
    return processes();
}

void app_runner::stop_apps()
{
    for (auto& proc : m_processes)
    {
        // todo: stop app
        TerminateProcess(proc.h_proc, 0);
        CloseHandle(proc.h_thread); // we don't need it
        CloseHandle(proc.h_proc);
        proc.id = 0;
        proc.h_proc= 0;
        proc.h_thread = 0;
    }
}