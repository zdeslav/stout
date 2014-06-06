#pragma once

#include <map>
#include <vector>
#include "metrics.h"
#include "backends.h"
#include <functional>

namespace metrics
{
    /// Represents events that server notifies the clients about using a 
    /// callback proveded by server_config::on_server_event
    enum server_events
    {
        StartupFailed, ///< an error occurred during server startup
        Started,       ///< server has started successfully
        Stopped        ///< server was stopped gracefully using server::stop()
    };

    /// prototype for function to be called immediately before flush.
    typedef  std::function<void(void)> FLUSH_FN;

    /// a function which takes `const stats&` and returns nothing
    typedef std::function<void(const stats&)> BACKEND_FN;

    /// prototype for function called by server to broadcast notifications
    typedef std::function<void(server_events)> SERVER_NOTIFICATION_FN;


    class server_config;

    /// Handles settings for local server instance.
    class server_config
    {
        unsigned int m_flush_period;
        unsigned int m_port;
        FLUSH_FN m_callback;
        std::vector<SERVER_NOTIFICATION_FN> m_server_cbs;
        std::vector<BACKEND_FN> m_backends;

    public:
        /**
        * Creates a metrics::server_config instance.
        * Configuration object is used to specify the settings for the local server
        * instance, which then runs in the same process, after server::run() is called.
        * This function will try to start winsock (WSAStartup) if it finds that 
        * winsock is not already started.
        *
        * @param port The port on which server is listening. The default is 9999.
        * @throws config_exception Thrown if invalid config parameters are 
        *         specified or if automatic winsock startup fails.
        *
        * Example:
        * ~~~{.cpp}
        * // set up an inproc server 
        * metrics::server start_local_server(unsigned int port)
        * {
        *     auto on_flush = [] { dbg_print("flushing!"); };
        *     auto console = console_backend();
        *     auto file = file_backend("d:\\dev\\metrics\\statsd.data");
        *
        *     auto cfg = metrics::server_config(port)
        *         .pre_flush(on_flush)  // can be used for custom metrics, etc
        *         .flush_every(10)      // flush measurements every 10 seconds
        *         .add_backend(console) // send data to console for display
        *         .add_backend(file);   // send data to file
        *
        *     return server::run(cfg);
        * }
        * ~~~
        */
        server_config(unsigned int port = 9999);

        /**
        * Specifies the default flush period for server. The default is 60s.
        * @param period Flush period, in seconds. Valid values are [1,3600]
        */
        server_config& flush_every(unsigned int period);

        /**
        * Tells the server to run on the same thread on which server::run() was 
        * called from. By default, server is running on another thread.
        */
        server_config& same_thread();

        /**
        * Adds a backend for flushed stats.
        * Backend can be anything that can be converted to `std::function<void(const stats&)>`.
        * It can be a simple function, a lambda or an instance of a  
        * class/structure which implements `void operator()(const stats&)`. 
        *
        * @param backend_instance An instance of a backend. BACKEND_FN is a 
        *        typedef for std::function<void(const stats&)>, a function which
        *        takes `const stats&` and returns nothing
        *
        * Example:
        * ~~~{.cpp}
        * // backend implemented as a function
        * void simple_backend(const stats& stats) {
        *     printf("simple backend: stats have %d timers\n", stats.timers.size() );
        * }
        *
        * // backend implemented as a structure. class is also fine, just make
        * // sure that the operator is declared as public
        * struct another_backend {
        *     void operator()(const stats& stats) {
        *         printf("another backend: stats have %d timers\n", stats.timers.size() );
        *     }
        * };
        *
        * // configure server with these backends
        * void start_local_server()
        * {
        *     file_backend file("statsd.data");   // built-in file backend
        * 
        *     auto cfg = metrics::server_config()
        *         .flush_every(10)                // flush metrics every 10 seconds
        *         .add_backend(&simple_backend)   // use function pointer as backend
        *         .add_backend(another_backend()) // use an instance of a struct
        *         .add_backend([](const stats&) { printf("!"); } ) // use a lambda
        *         .add_backend(console_backend()) // and builtin console backend
        *         .add_backend(file);             // and also to a file
        *
        *     server::run(cfg);
        * }
        * ~~~
        *
        * @see [Running the server](docs/running_server.md)
        * @see file_backend
        * @see console_backend
        */
        server_config& add_backend(BACKEND_FN backend_instance) {
            m_backends.push_back(backend_instance); 
            return *this;
        }

        /**
        * Specifies the function to be called before the values are flushed.
        * You can use this to add some metrics, etc.
        * @param callback Callback function, lambda, functor or anything
        * convertible to `std::function<void(void)>`.
        *
        * Example:
        * ~~~{.cpp}
        * // set up an inproc server listening on port 12345
        * auto cfg = metrics::server_config(12345)
        *     .pre_flush([]{ printf("flushing..."); }); // use lambda
        * 
        * server::run(cfg);                             // start the server
        * ~~~
        */
        server_config& pre_flush(FLUSH_FN callback);

        /**
        * Specifies the function to be called when server broadcasts important
        * notifications. You can use this to get notified when server starts, 
        * stops, etc. Multiple listeners can be registered.
        *
        * @param callback Callback function, lambda, functor or anything
        * convertible to `std::function<void(server_events)>`.
        *
        * Example:
        * ~~~{.cpp}
        * // set up an inproc server 
        * auto cfg = metrics::server_config()
        *     .add_server_listener([](server_events e){ printf("%d", e); }); // use lambda
        * 
        * server::run(cfg);                             // start the server
        * ~~~
        */
        server_config& add_server_listener(SERVER_NOTIFICATION_FN callback);

        unsigned int flush_period_ms() const { return m_flush_period * 1000; }
        unsigned int port() const { return m_port; }
        const FLUSH_FN& flush_fn() const { return m_callback; }
        const std::vector<SERVER_NOTIFICATION_FN>& server_cbs() const { return m_server_cbs; }
        const std::vector<BACKEND_FN>& backends() const { return m_backends; }
    };

    /// Represents a instance of the server.
    class server
    {
        server_config m_cfg;
        server(const server_config& cfg) : m_cfg(cfg) { ; }

    public:
        /// default ctor for cases where object must be instanitated in advance
        server(){;}
        /**
        * starts the server, based on specified configuration
        * @param cfg Setting used by the server
        */
        static server run(const server_config& cfg);

        /**
        * notifies the server to stop processing incoming messages. If server
        * was configured to create a thread, the thread will exit.
        */
        void stop();

        const server_config& config() const { return m_cfg; }
    };

    // storage for raw metric data. values are stored here until they are flushed
    struct storage
    {
        std::map<std::string, unsigned int> counters;
        std::map<std::string, long long> gauges;
        std::map<std::string, std::vector<int> > timers;

        void clear() {
            counters.clear();
            gauges.clear();
            timers.clear();
        }
    };

    /// statistic for a single timer
    struct timer_data
    {
        std::string metric; ///< name of the timer
        int count;          ///< number of entries
        int max;            ///< maximum value of the measured sample
        int min;            ///< minimum value of the measured sample
        long long sum;      ///< sum of all sampled values
        double avg;         ///< average (mean) of samples
        double stddev;      ///< standard deviation

        /// returns a string with textual description of timer data
        std::string dump() const
        {
            char txt[256];
            _snprintf_s(txt, _countof(txt), _TRUNCATE, 
                "%s - cnt: %d, min: %d, max: %d, sum: %lld, avg: %.2f, stddev: %.2f",
                metric.c_str(), count, min, max, sum, avg, stddev);
            return txt;
        }
    };

    /// contains processed metric statistics
    struct stats
    {
        timer::time_point timestamp;
        std::map<std::string, double> counters; ///< counter data
        std::map<std::string, long long> gauges; ///< gauge data
        std::map<std::string, timer_data> timers; ///< timer data
    };
}