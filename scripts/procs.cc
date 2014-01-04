#include <string>
#include <main/Analysis.hh>
#include <main/stored_procedures.hh>
#include <main/Connect.hh>
#include <main/metadata_tables.hh>

static void
dumpProcsAndFunctions()
{
    auto procs = getStoredProcedures();
    for (auto it : procs) {
        std::cout << it << std::endl;
    }

    auto funcs = getAllUDFs();
    for (auto it : funcs) {
        std::cout << it << std::endl;
    }
}

int main()
{
    const std::string &prefix = 
        getenv("CRYPTDB_NAME") ? getenv("CRYPTDB_NAME")
                               : "generic_prefix_";
    MetaData::Internal::initPrefix(prefix);
    dumpProcsAndFunctions();

    return 0;
}
