#pragma once

class config;
class app_runner;

class monitor
{
public:
    monitor(const config& cfg, const app_runner& runner);
    ~monitor();
    void run();
};

