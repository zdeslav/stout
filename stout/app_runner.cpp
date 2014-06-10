#include "stdafx.h"
#include "app_runner.h"
#include "config.h"
#include <tlhelp32.h>

app_runner::app_runner(const config& cfg) : m_cfg(cfg) {}
app_runner::~app_runner()
{
    stop_apps();
    m_processes.clear();
}

PROCESS_INFORMATION AttachToProcess(const proc_info proc)
{
    PROCESS_INFORMATION pi;
    PROCESSENTRY32 pe32;

    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        throw stout_exception("can't retrieve process info");
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        throw stout_exception("can't retrieve process info");
    }

    do
    {
        if (!strstr(pe32.szExeFile, proc.process_name.c_str())) continue; // todo: ends with

        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID);
        if (hProcess == NULL) throw stout_exception("opening process failed");

        pi.dwProcessId = pe32.th32ProcessID;
        pi.dwThreadId = 0;
        pi.hProcess = hProcess;
        CloseHandle(hProcessSnap);
        return pi;

    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);  
    std::string msg = "can't find process " + proc.process_name;
    throw stout_exception(msg.c_str());
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
        process_runtime_info proc; 
        PROCESS_INFORMATION data;

        for (int i = 0; i < app.instance_count; ++i)
        {
            if (app.attach) 
            {
                data = AttachToProcess(app);
            }
            else
            {
                data = StartProcess(app);
            }

            proc.symbolic_name = app.id; 
            proc.process_name = app.process_name;
            proc.process_path = app.process_name;
            proc.id = data.dwProcessId;
            proc.h_proc = data.hProcess;
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
        CloseHandle(proc.h_proc);
        proc.id = 0;
        proc.h_proc= 0;
    }
}