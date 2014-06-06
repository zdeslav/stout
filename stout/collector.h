#pragma once

class config;
class app_runner;

class collector
{
public:
    collector(const config& cfg, const app_runner& runner);
    ~collector();
    void run();
};

