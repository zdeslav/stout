#include "stdafx.h"
#include "config.h"
#include <map>
#include <algorithm>

const char* STOUT_COMMON = "STOUT::COMMON";
const char* STOUT_BACKENDS = "STOUT::BACKENDS";

using namespace std;


struct KeyDict
{    
    typedef std::pair<std::string, std::string> ini_entry;
    std::vector<ini_entry> keys;

    bool get(int& val, const char* key_name, int default) 
    {
        auto it = find_if(begin(keys), end(keys), 
                          [=](const ini_entry& e) { return !_strcmpi(e.first.c_str(), key_name);  });

        val = (it == keys.end()) ? default : strtol(it->second.c_str(), NULL, 10);
        return it != keys.end();
    }
    bool get(std::string& val, const char* key_name, const char* default)
    {
        auto it = find_if(begin(keys), end(keys), 
                          [=](const ini_entry& e) { return !_strcmpi(e.first.c_str(), key_name);  });

        val = (it == keys.end()) ? default : it->second;
        return it != keys.end();
    }
};

// quick and dirty - do a proper parser when we know more about syntax
watch parse_watch(const std::string& exp)
{
    // todo: provide more context (process + ...)
    string err_msg = "Invalid monitor expression: METRIC = " + exp;
    char tmp[256];
    if (STRUNCATE == strncpy_s(tmp, 256, exp.c_str(), _TRUNCATE))
        throw stout_exception(err_msg.c_str());

    _strlwr_s(tmp);
    watch w;
    char* pos = tmp;
    char* ctxt = NULL;
    pos = strtok_s(pos, " ", &ctxt);
    if (pos)w.counter = pos; // todo: verify if known counter

    pos = strtok_s(NULL, " ", &ctxt);
    if (!pos) throw stout_exception(err_msg.c_str());
    if (pos[0] == '<') w.watch_op = operator_lt;
    else if (pos[0] == '>') w.watch_op = operator_gt;
    else throw stout_exception(err_msg.c_str());

    pos = strtok_s(NULL, " ", &ctxt);
    if (!pos) throw stout_exception(err_msg.c_str());
    w.operand = strtol(pos, &pos, 10);
    if (*pos != 0 && *pos != '%') throw stout_exception(err_msg.c_str());  
    w.model = (*pos == '%') ? relative : absolute;

    pos = strtok_s(NULL, " ", &ctxt);
    if (pos)  throw stout_exception(err_msg.c_str()); // fail on unknown suffix

    return w;
}

void fill_watches(watch_list& watches, const KeyDict& dict)
{
    for (const auto& pair : dict.keys) {
        if (_strcmpi(pair.first.c_str(), "METRIC")) continue;

        watch w = parse_watch(pair.second);
        watches.push_back(w);
    }
}

KeyDict get_keys(const std::string& ini_file, const std::string& section)
{
    KeyDict dict;
    char buff[10240];
    GetPrivateProfileSection(section.c_str(), buff, _countof(buff), ini_file.c_str()); // todo: error handling
    char* ps = buff;

    do {
        std::string line = ps;
        auto pos = line.find('=');
        if (pos != string::npos) {
            std::string key(ps, pos);
            std::string val(ps + pos + 1);
            dict.keys.push_back(KeyDict::ini_entry(key, val));
        }

        ps += (strlen(ps) + 1);
    } while (*ps != '\0');

    return dict;
}

config::config(const std::string& ini_file)
{
    m_ini_file = ini_file;

    std::vector<std::string> sections;
    sections.reserve(100);

    char buff[10240];
    GetPrivateProfileSectionNames(buff, _countof(buff), ini_file.c_str()); // todo: error handling

    char* ps = buff;
    do {
        sections.push_back(ps);
        ps += (strlen(ps) + 1);
    } while (*ps != '\0');

    ParseCommon();
    ParseBackends();
    for (const auto& section : sections)
    {
        if (_stricmp(section.c_str(), STOUT_COMMON) &&
            _stricmp(section.c_str(), STOUT_BACKENDS))
            ParseProcessSection(section);
    }
}

void config::ParseCommon()
{
    auto keys = get_keys(m_ini_file, STOUT_COMMON);

    keys.get(m_server_port, "SERVER_PORT", 9999); // todo: use ephemeral ports
    keys.get(m_initial_delay, "DELAY", 5);
    keys.get(m_sampling_time, "SAMPLING_TIME", 60);
    keys.get(m_testrun_duration, "DURATION", 60);

    string err;
    keys.get(err, "ON_ERROR", "LOG");
    m_error_reaction = (_strcmpi(err.c_str(), "STOP") == 0 ? stop_test : log_it);

    fill_watches(m_watches, keys);
}

void config::ParseBackends()
{
    auto dict = get_keys(m_ini_file, STOUT_BACKENDS);
    for (const auto& pair : dict.keys) {
        AddBackend(pair.first, pair.second);
    }
}

void config::AddBackend(const std::string& backend_name, const std::string& args)
{   
    backend be = { backend_name, args };
    m_backends.push_back(be);
}

void config::ParseProcessSection(const std::string& section)
{
    auto keys = get_keys(m_ini_file, section);
    proc_info proc;
    proc.id = section; // todo: replace ' ' with '_'
    if (keys.get(proc.process_name, "ATTACH", ""))
    {
        proc.attach = true;
    }
    else
    {    
        keys.get(proc.process_name, "START", "");
        proc.attach = false;
    }
    if (proc.process_name.empty()) // todo: add process name to exc message
    {
        string msg = "START or ATTACH must be specified for each process (" + section + ")";
        throw stout_exception(msg.c_str());
    }
    keys.get(proc.instance_count, "COUNT", 1);
    
    // now collect metrics for process
    proc.m_watches.insert(proc.m_watches.end(), m_watches.begin(), m_watches.end()); // first add common watches
    fill_watches(proc.m_watches, keys);

    m_processes.push_back(proc);
}

config config::load(const std::string& ini_file)
{
    return config(ini_file);
}