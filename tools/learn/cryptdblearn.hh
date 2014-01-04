#pragma once

#include <stdio.h>
#include <iostream>
#include <rewrite_main.hh>

// Anonymous namespace
namespace {

typedef enum {
    MODE_FILE,
    MODE_FROM_SCRATCH,
    MODE_INVALID
}mode_e;

//TODO: dunno if I'll keep using this..
typedef struct _learn_mode_t
{
    mode_e mode;
    const char *envname;
} learn_mode_t;
static learn_mode_t __attribute__((unused))modes[]=
{
    {MODE_FILE, "TRUE"},
    {MODE_FROM_SCRATCH, "FALSE"},
    {MODE_INVALID, "FALSE"},
};

class Learn
{
    public:

        Learn(mode_e mode, ProxyState& ps, const std::string &dbname,
              const std::string &filename)
            : m_totalnum(0), m_success_num(0), m_errnum(0),
            m_mode(mode), m_ps(ps), m_dbname(dbname), m_filename(filename){}

        ~Learn(){}

        void trainFromFile(ProxyState &ps);
        void trainFromScratch(ProxyState &ps);

        void status();

    private:
        int m_totalnum;
        int m_success_num;
        int m_errnum;
        mode_e m_mode;
        ProxyState& m_ps;
        std::string m_dbname;
        std::string m_filename;
        std::vector<query_parse*>qvec;

};

};
