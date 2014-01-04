#pragma once

#include <string>
#include <memory>

#include <main/Connect.hh>

namespace MetaData {
    bool initialize(const std::unique_ptr<Connect> &conn,
                    const std::unique_ptr<Connect> &e_conn,
                    const std::string &prefix);

    namespace Table {
        std::string metaObject();
        std::string bleedingMetaObject();
        std::string embeddedQueryCompletion();
        std::string staleness();
        std::string remoteQueryCompletion();
    };

    namespace Proc {
        std::string currentTransactionID();
        std::string homAdditionTransaction();
        std::string adjustOnion();
    };

    namespace DB {
        std::string embeddedDB();
        std::string remoteDB();
    };

    namespace Internal {
        void initPrefix(const std::string &prefix);
        const std::string &getPrefix();
        const std::string &lowLevelPrefix(const char *const p);
    };
};

