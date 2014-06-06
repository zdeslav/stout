#pragma once

#include <string>

namespace metrics
{
    struct stats;

    /// Simple backend to dump stats to console
    class console_backend
    {
    public:
        /**
        * Dumps the provided statistics data to console
        * @param stats Statistic data resulting from last flush
        */
        void operator()(const stats& stats);
    };

    /// Simple backend to dump stats to file
    class file_backend
    {
        std::string m_filename;
    public:
        /**
        * Creates an instance of file_backend
        * @param filename name of the file where stats will be written
        */
        file_backend(const char* filename) : m_filename(filename){ ; }
        /**
        * Dumps the provided statistics data to file
        * @param stats Statistic data resulting from last flush
        */
        void operator()(const stats& stats);
    };

	/// Simple backend that dums stats to JSON file. 
	class json_file_backend
	{
		std::string m_filename;
	public:
		/**
		* Creates an instance of json_file_backend
		* @param filename name of the file where stats will be written
		*/
		json_file_backend(const char* filename) : m_filename(filename){ ; }
		/**
		* Dumps the provided statistics data to file
		* @param stats Statistic data resulting from last flush
		*/
		void operator()(const stats& stats);
	private:
		std::string to_quoted_string(const char *value);
		static bool contains_control_character(const char* str);
		static bool is_control_character(char ch);
		static std::string double_to_string(double value);
	};
    /*
    class event_log_backend
    {
    public:
    event_log_backend();
    virtual void process_stats(const stats& stats);
    };

    class graphite_backend
    {
    public:
    graphite_backend(const char* host, int port);
    virtual void process_stats(const stats& stats);
    };

    class syslog_udp_backend 
    {
    public:
    syslog_udp_backend(const char* host, int port);
    virtual void process_stats(const stats& stats);
    };

    class syslog_file_backend
    {
    std::string m_filename;
    public:
    syslog_udp_backend(const char* filename) : m_filename(filename){ ; }
    virtual void process_stats(const stats& stats);
    };
    */
}