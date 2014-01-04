#pragma once

namespace {

/**
 * Import database tool class.
 */
class Import
{
    public:
        Import(const char *fname) : filename(fname){}
        ~Import(){}

        void executeQueries(ProxyState& ps);
        void printOutOnly(void);

    private:
        std::string filename;
};
};

