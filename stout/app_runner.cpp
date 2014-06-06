#include "stdafx.h"
#include "app_runner.h"
#include "config.h"

app_runner::app_runner(const config& cfg)
{
}


app_runner::~app_runner()
{
}

const app_runner::runtime_list& app_runner::start_apps()
{

    return processes();
}

void app_runner::stop_apps()
{

}