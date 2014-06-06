#pragma once

#include <string>
#include "Winsock2.h"

#pragma comment (lib, "ws2_32.lib")

/// all metrics types and functions are defined inside @ref metrics namespace
namespace metrics
{
    enum metric_type
    {
        counter,
        histogram,
        gauge,
        gauge_delta
    };

    /// Represents different groups of built-in metrics. These can be combined
    /// as flags to specify what builtin metrics will be automatically tracked,
    /// e.g. `process | system` means system and process metrics will be tracked
    /// while internal metrics will not.
    /// @see track_default_metrics
    enum builtin_metric
    {
        none    = 0,       ///< no metrics will be tracked
        process = 1,       ///< process related metrics
        system  = 2,       ///< system-wide metrics
        metrics = 4,       ///< internal metrics (last_seen, total count, ...)
        all     = 0xFFFF   ///< track all metrics
    };

    struct SOCK_ADDR_IN : public sockaddr_in {
        SOCK_ADDR_IN() {
            memset((char *)this, 0, sizeof(SOCK_ADDR_IN));
        }

        SOCK_ADDR_IN(int family, unsigned long addr, int port) {
            memset((char *)this, 0, sizeof(SOCK_ADDR_IN));
            sin_family = family;
            sin_addr.s_addr = htonl(addr);
            sin_port = htons(port);
        }
    };

    typedef const char* METRIC_ID;

    void ensure_winsock_started();
    inline void dbg_print(const char* fmt, ...);
    void send_to_server(const char* txt, size_t len);

    // 1700 is VS2012  - VS2010 doesn't support official range based for loop
    #if _MSC_VER < 1700
    #define FOR_EACH(var , range) for each(var in range)
    #else
    #define FOR_EACH(var , range) for (var : range)
    #endif

    // used as a workaround for the fact that VS2010 doesn't support std::chrono
    // while std::chrono is broken in VS2013, see:
    // https://connect.microsoft.com/VisualStudio/feedback/details/719443/
    class timer
    {
    public:
        typedef int time_point;  // todo: ULONGLONG, int is limited to ~50 days
        typedef int duration;
        static time_point now();
        static duration since(int when);
        static std::string to_string(timer::time_point time);
    };
    /// used to notify client code about errors during client or server
    /// configuration
    class config_exception : public std::runtime_error
    {
    public:
        explicit config_exception(const char* message) : std::runtime_error(message){ ; }
    };

    /// lists all the default metrics, which can be automatically tracked
    namespace builtin {
        const char internal_metrics_count[] = "metrics.internal.count"; ///< Number of metrics tracked
        const char internal_metrics_last_seen[] = "metrics.internal.last_seen"; ///< timestamp of last metric

        // GlobalMemoryStatusEx, GetPerformanceInfo, GetSystemTimes
        const char sys_mem_phys_used[] = "sys.mem.phys.total"; ///< Total physical memory
        const char sys_mem_phys_free[] = "sys.mem.phys.free";  ///< Physical memory available
        const char sys_mem_phys_load[] = "sys.mem.phys.load";  ///< Percentage of used physical memory
        const char sys_mem_virtual_used[] = "sys.mem.virt.total"; ///< Total virtual memory
        const char sys_mem_virtual_free[] = "sys.mem.virt.free";  ///< Available virtual memory
        const char sys_mem_pagefile_used[] = "sys.mem.page.total";  ///< Total page memory
        const char sys_mem_pagefile_free[] = "sys.mem.page.free";  ///< Total page memory
        const char sys_cpu_load[] = "sys.cpu.used";  ///< System-wide CPU usage, in %
        const char sys_disk_free_c[] = "sys.disk.free.c";  ///< Free space on C:\ disk
        const char sys_disk_free_d[] = "sys.disk.free.d";  ///< Free space on D:\ disk
        const char sys_count_handles[] = "sys.count.handles";  ///< The current number of open handles
        const char sys_count_processes[] = "sys.count.processes";  ///< The current number of processes
        const char sys_count_threads[] = "sys.count.threads";  ///< The current number of threads

        // GetProcessMemoryInfo, GetProcessTimes
        const char proc_mem_workingset[] = "proc.mem.wset";  ///< The current working set size, in bytes
        const char proc_mem_pagefaults[] = "proc.mem.pagefaults";  ///< The number of page faults.
        const char proc_mem_workingset_peek[] = "proc.mem.wset.peak";  ///< The peak working set size, in bytes.
        const char proc_mem_pagefile[] = "proc.mem.pagefile";  ///< ...
        const char proc_mem_pagefile_peek[] = "proc.mem.pagefile.peak";  ///< The peak value in bytes of the Commit Charge during the lifetime of this process
        const char proc_cpu_load[] = "proc.cpu.total";  ///< ...
        const char proc_cpu_kernel[] = "proc.cpu.used.kernel";  ///< ...
        const char proc_cpu_user[] = "proc.cpu.used.user";  ///< ...
    }

    class client_config;

    /**
    * Configures metrics client.
    * Client configuration object specifies the settings for the client. This
    * function will try to start winsock (WSAStartup) if it finds that winsock
    * is not already started.
    *
    * @param server The address/name of the server where metrics will be sent
    * @param port The port on which the server is listening. Default is 9999
    * @throws config_exception Thrown if specified hostname can't be found,
    *         or if automatic winsock startup fails.
    *
    * Example:
    * ~~~{.cpp}
    * // set up the client
    * metrics::setup_client("127.0.0.1", 9999)  // point client to the server
    *     .set_debug(true)                      // turn on debug tracing
    *     .set_namespace("myapp")               // specify namespace, default is "stats"
    *     .track_default_metrics(metrics::all); // track default system and process metrics
    * ~~~
    *
    * @see client_config
    */
    client_config& setup_client(const std::string& server, unsigned int port = 9999);

    /**
    * handles client settings.
    */
    class client_config
    {
        friend client_config& setup_client(const std::string& server, unsigned int port);

        bool m_debug;
        unsigned int m_port;
        unsigned int m_defaults_period;
        builtin_metric m_default_metrics;
        std::string m_namespace;
        std::string m_server;
        SOCK_ADDR_IN m_svr_address;

    public:
        client_config();

        /**
        * Toggles the debug printing. By default, it is turned off.
        * @param debug Set to `true` to turn debug output on, `false` otherwise.
        */
        client_config& set_debug(bool debug);

        /**
        * Tells client to track default system and process metrics (CPU load,
        * free memory, disk usage, etc). Metrics will be collected on a separate
        * thread, and they are all handled as gauges.
        *
        * For list of supported default metrics, check constants in
        * metrics::builtin namespace
        *
        * @param which  Which groups of metrics will be tracked
        * @param period How often, in seconds, will default metrics be collected.
        *               The default value is 45 seconds, valid values are > 0.
        * @throws config_exception Thrown if period is set to 0
        */
        client_config& track_default_metrics(builtin_metric which = all, unsigned int period = 45);

        /**
        * Specifies the namespace to be used for metrics. The default is "stats"
        * @param ns New namespace to be used
        */
        client_config& set_namespace(const std::string& ns);

        /**
        * Returns whether the debug tracing is active
        * @return `true` if debug tracing is on, `false` otherwise.
        */
        bool is_debug() const;

        /**
        * Returns the current metrics namespace.
        * @return String specifying the namespace.
        */
        const char* get_namespace() const;

        const sockaddr_in* server_address() const { return &m_svr_address; }

    };

    extern client_config  g_client;

    /**
     * Provides automatic timing.
     * When you create an instance, it will note the current time. Then in the
     * destructor, it will calculate the total duration, and save it as a timer.
     *
     * ~~~ {.cpp}
     * void some_function() {
     *     metrics::auto_timer _("app.fn.duration");
     *     // do something lengthy
     *     ...
     * }   // here, timer 'app.fn.duration' will be stored
     * ~~~
     */
    class auto_timer
    {
        timer::time_point m_started_at;
        METRIC_ID m_metric;

    public:
        /**
            constructs an auto_timer.
            @param metric The name of the metric to be stored
        */
        explicit auto_timer(METRIC_ID metric);
        ~auto_timer();

    private:
        auto_timer(const auto_timer&);
        auto_timer& operator=(const auto_timer&);
    };

    /**
    *  Increments the specified counter metric
    *
    *  @param metric The name of the counter to be incremented
    *  @param inc Amount by which to increment the counter. Default value is 1.
    *
    * ~~~ {.cpp}
    * void on_login(const char* user) {
    *     metrics::inc("app.logins");
    *     if (!login(user)) {
    *        metrics::inc("app.logins.failed");
    *        ...
    *     }
    * }
    * ~~~
    */
    void inc(METRIC_ID metric, int inc = 1);

    /**
    *  Sets the specified timer/histogram metric
    *
    *  @param metric The name of the timer/histogram to be updated
    *  @param value Amount to which metric will be set.
    *
    * ~~~ {.cpp}
    * void on_login(const char* user) {
    *     auto time = get_time_ms();
    *     // perform login
    *     ...
    *     metrics::measure("app.login.duration", get_time_ms() - time);
    *     // you might want to use metrics::auto_timer to simplify this
    *     // or, even simpler, MEASURE_FN() macro
    * }
    * ~~~
    *
    * @see auto_timer
    */
    void measure(METRIC_ID metric, int value);

    /**
    *  Sets the specified gauge metric
    *  @param metric The name of the gauge to be updated
    *  @param value Amount to which the gauge will be set.
    *
    * ~~~ {.cpp}
    * void on_login(const char* user) {
    *     ...
    *     metrics::set("app.login.total_users", total_users());
    * }
    * ~~~
    */
    void set(METRIC_ID metric, unsigned int value);

    /**
    *  Sets the delta for specified gauge metric
    *  @param metric The name of the gauge to be updated
    *  @param value Amount to be added to the the gauge.
    *
    * ~~~ {.cpp}
    * void on_login(const char* user) {
    *     ...
    *     metrics::set("free_space", 2000);       // free_space => 2000
    *     metrics::set("free_space", 3000);       // free_space => 3000
    *     metrics::set_delta("free_space", 100);  // free_space => 3100
    *     metrics::set_delta("free_space", -600); // free_space => 2500
    * }
    * ~~~
    */
    void set_delta(METRIC_ID metric, int value);
}


/**
* Convenience macro to create an auto_timer with metric name set to
* 'app.fn.function_name'
*
*  ~~~ {.cpp}
*  void some_function() {
*      MEASURE_FN();
*      // do something lengthy
*  }   // here, timer 'app.fn.some_function' will be stored
*  ~~~
*/
#define MEASURE_FN() metrics::auto_timer m_at__("app.fn."__FUNCTION__)

