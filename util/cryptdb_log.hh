#pragma once

#include <iostream>
#include <sstream>
#include <map>
#include <string>

#define LOG_GROUPS(m)       \
    m(warn)                 \
    m(debug)                \
    m(cdb_v)                \
    m(crypto)               \
    m(crypto_v)             \
    m(crypto_data)          \
    m(edb)                  \
    m(edb_v)                \
    m(edb_query)            \
    m(edb_query_plain)      \
    m(edb_perf)             \
    m(encl)                 \
    m(test)                 \
    m(am)                   \
    m(am_v)                 \
    m(mp)                   \
    m(wrapper)              \
    m(all)

enum class log_group {
#define __temp_m(n) log_ ## n,
LOG_GROUPS(__temp_m)
#undef __temp_m
};

static
std::map<std::string, log_group> log_name_to_group = {
#define __temp_m(n) { #n, log_group::log_ ## n },
LOG_GROUPS(__temp_m)
#undef __temp_m
};

class cryptdb_logger : public std::stringstream {
 public:
    cryptdb_logger(log_group g, const char *filearg, uint linearg, const char *fnarg)
        : m(mask(g)), file(filearg), line(linearg), func(fnarg)
    {
    }

    ~cryptdb_logger()
    {
        if (enable_mask & m)
            std::cerr << file << ":" << line
                      << " (" << func << "): "
                      << str() << std::endl;
    }

    static void
    enable(log_group g)
    {
        if (g == log_group::log_all)
            enable_mask = ~0ULL;
        else
            enable_mask |= mask(g);
    }

    static void
    disable(log_group g)
    {
        if (g == log_group::log_all)
            enable_mask = 0;
        else
            enable_mask &= ~mask(g);
    }

    static bool
    enabled(log_group g)
    {
        return enable_mask & mask(g);
    }

    static uint64_t
    mask(log_group g)
    {
        return 1ULL << ((int) g);
    }

    static std::string
    getConf()
    {
        std::stringstream ss;
        ss << enable_mask;
        return ss.str();
    }

    static void
    setConf(std::string conf)
    {
        std::stringstream ss(conf);
        ss >> enable_mask;
    }

 private:
    uint64_t m;
    const char *file;
    uint line;
    const char *func;

    static uint64_t enable_mask;

};

#define LOG(g) \
    (cryptdb_logger(log_group::log_ ## g, __FILE__, __LINE__, __func__))

