#include "stdafx.h"
#include "collector.h"
#include "app_runner.h"
#include "config.h"
#include "metrics\metrics.h"
#include <psapi.h>


collector::collector(const config& cfg, const app_runner& runner) 
: m_cfg(cfg), m_runner(runner)
{
}


collector::~collector()
{
}

DWORD WINAPI collector::thread_proc(LPVOID params)
{
    collector* coll = (collector*)params;
    auto delay = coll->m_cfg.initial_delay();
    auto processes = coll->m_runner.processes();

    Sleep(delay * 1000);
    printf("initial delay expired - data collection started...\n");

    while (true) {
        // todo: use PDH?
        for (auto proc : processes) {
            PROCESS_MEMORY_COUNTERS_EX pmc;
            if (!GetProcessMemoryInfo(proc.h_proc, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
            {
                continue;
            }
            else
            {
                char txt[256];
                sprintf_s(txt, "%s.mem_ws.%d", proc.symbolic_name.c_str(), proc.id);
                metrics::measure(txt, pmc.WorkingSetSize / 1024);
                sprintf_s(txt, "%s.mem_pb.%d", proc.symbolic_name.c_str(), proc.id);
                metrics::measure(txt, pmc.PrivateUsage / 1024);
                // todo: log
            }
        }
        Sleep(1000); // todo: make configurable
    }
}

void collector::run()
{
    DWORD thread_id;
    HANDLE h = CreateThread(NULL, 0, thread_proc, this, 0, &thread_id);
    if (!h) throw std::runtime_error("Failed creating collectpr thread");
    CloseHandle(h);

    // wait / delay



}