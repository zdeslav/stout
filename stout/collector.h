#pragma once

class config;
#include "app_runner.h"

class collector
{
public:
    collector(const config& cfg, const app_runner& runner);
    ~collector();
    void run();
private:
    static DWORD WINAPI thread_proc(LPVOID params);
    const app_runner& m_runner;  
    const config& m_cfg;
};

