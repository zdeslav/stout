#include "stdafx.h"
#include "backends.h"
#include "metrics_server.h"
#include <fstream> 
#include <ctime>
#include <string>
#include <sstream>
#include <iomanip>
#include <list>

namespace metrics
{
    void console_backend::operator()(const stats& stats)
    {
        FOR_EACH (auto& c, stats.counters)
        {
            printf(" C: %s - %.2f 1/s\n", c.first.c_str(), c.second);
        }
        FOR_EACH (auto& g, stats.gauges)
        {
            printf(" G: %s - %lld\n", g.first.c_str(), g.second);
        }
        FOR_EACH (auto& t, stats.timers)
        {
            printf(" H: %s\n", t.second.dump().c_str());
        }
    }

    void file_backend::operator()(const stats& stats)
    {
        std::ofstream ofs;
        ofs.open(m_filename, std::ofstream::out | std::ofstream::app);

        ofs << "@ TS: " << timer::to_string(stats.timestamp) << "\n";

        FOR_EACH (auto& c, stats.counters)
        {
            ofs << " C: " << c.first.c_str() << " - " << c.second << "1/s\n";
        }
        FOR_EACH (auto& g, stats.gauges)
        {
            ofs << " G: " << g.first.c_str() << " - " << g.second << "\n";
        }
        FOR_EACH (auto& t, stats.timers)
        {
            ofs << " H: " << t.second.dump() << "\n";
        }
        ofs << "----------------------------------------------\n";

        ofs.close();
    }

	void json_file_backend::operator()(const stats& stats)
	{
		const char indent[] = "    ";
		std::ofstream ofs;
		ofs.open(m_filename, std::ofstream::out | std::ofstream::app);

		int current_line = 0;

		ofs << "{\n";

		ofs << indent << to_quoted_string("_timestamp") << ": " << to_quoted_string(timer::to_string(stats.timestamp).c_str());

		FOR_EACH(auto& c, stats.counters)
		{
			ofs << ",\n" << indent << to_quoted_string(c.first.c_str()) << ": " << double_to_string(c.second);
}
		FOR_EACH(auto& g, stats.gauges)
		{
			ofs << ",\n" << indent << to_quoted_string(g.first.c_str()) << ": " << g.second;
		}

		FOR_EACH(auto& t, stats.timers)
		{
			ofs << ",\n" << indent << to_quoted_string(t.first.c_str()) << ": ";
			ofs << "{ ";
			ofs << to_quoted_string("avg") << ": " << double_to_string(t.second.avg) << ", ";
			ofs << to_quoted_string("count") << ": " << t.second.count << ", ";
			ofs << to_quoted_string("min") << ": " << double_to_string(t.second.min) << ", ";
			ofs << to_quoted_string("max") << ": " << double_to_string(t.second.max) << ", ";
			ofs << to_quoted_string("stddev") << ": " << double_to_string(t.second.stddev);
			ofs << " }";
		}

		ofs << "\n}\n";

		ofs.close();
	}

	std::string json_file_backend::to_quoted_string(const char *value)
	{
		if (strpbrk(value, "\"\\\b\f\n\r\t") == NULL && !contains_control_character(value)) {
			return std::string("\"") + value + "\"";
		}

		std::string::size_type maxsize = strlen(value) * 2 + 3;
		std::string result;
		result.reserve(maxsize);
		result += "\"";

		for (const char* c = value; *c != 0; ++c)
		{
			switch (*c)
			{
			case '\"':
				result += "\\\"";
				break;
			case '\\':
				result += "\\\\";
				break;
			case '\b':
				result += "\\b";
				break;
			case '\f':
				result += "\\f";
				break;
			case '\n':
				result += "\\n";
				break;
			case '\r':
				result += "\\r";
				break;
			case '\t':
				result += "\\t";
				break;
			default:
				if (is_control_character(*c))
				{
					std::ostringstream oss;
					oss << "\\u" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << static_cast<int>(*c);
					result += oss.str();
				}
				else
				{
					result += *c;
				}
				break;
			}
		}
		result += "\"";
		return result;
	}

	bool json_file_backend::contains_control_character(const char* str)
	{
		while (*str)
		{
			if (is_control_character(*(str++)))
				return true;
		}
		return false;
	}


	inline bool json_file_backend::is_control_character(char ch)
	{
		return ch > 0 && ch <= 0x1f;
	}

	std::string json_file_backend::double_to_string(double value)
	{
		char buffer[32];
		sprintf_s(buffer, sizeof(buffer), "%#.16g", value);
		
		char* ch = buffer + strlen(buffer) - 1;
		if (*ch != '0') {
			return buffer; // nothing to truncate, so save time
		}

		while (ch > buffer && *ch == '0') {
			--ch;
		}

		char* last_nonzero = ch;
		while (ch >= buffer) {
			switch (*ch) {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					--ch;
					continue;
				case '.':
					// Truncate zeroes to save bytes in output, but keep one.
					*(last_nonzero + 2) = '\0';
					return buffer;
				default:
					return buffer;
			}
		}

		return buffer;	
	}

}
