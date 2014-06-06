// stout.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "config.h"
#include "app_runner.h"
#include "collector.h"
#include "metrics/metrics_server.h"
#include "monitoring_backend.h"
#include <iostream>

#define TCLAP_NAMESTARTSTRING "--"
#define TCLAP_FLAGSTARTSTRING "-"

#include "tclap/cmdline.h"

using namespace metrics;

metrics::server start_server(const config& cfg)
{
    auto on_flush = [] { printf("flushing!"); }; // check differences

    console_backend console;
    monitoring_backend mon(cfg);

    auto server_cfg = metrics::server_config(cfg.server_port())
        //.pre_flush(on_flush) 
        .flush_every(cfg.sampling_time())
        .add_backend(mon)
        .add_backend(console);

    return server::run(server_cfg);
}

int _tmain(int argc, _TCHAR* argv[])
{
    using namespace TCLAP;
    TCLAP::CmdLine cmd("Executes load test for applications", ' ', "0.1", true);
    TCLAP::ValueArg<std::string> iniFileArg("i", "ini", "ini file containing the configuration", true, "", "file path");
    cmd.add(iniFileArg);
    cmd.parse(argc, argv);

    if (!iniFileArg.isSet()) return 1;

    try 
    {
        config cfg = config::load(iniFileArg.getValue());
        app_runner runner(cfg);
        collector collector(cfg, runner);

        auto server = start_server(cfg);                                                
        runner.start_apps();
        collector.run();

        printf("Monitoring started, press ENTER to exit...\n");
        char buff[32];
        gets_s(buff, 30);

        runner.stop_apps();
        server.stop();
    }
    catch (const stout_exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }            

    return 0;
}

