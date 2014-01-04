#pragma once

/*
 * TestQueries.h
 *
 */

#include <signal.h>
#include <stdlib.h>

#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>

#include <test/test_utils.hh>

typedef enum test_mode {
    UNENCRYPTED, SINGLE,
    PROXYPLAIN, PROXYSINGLE,
    TESTINVALID, ENC, PROXYENC
} test_mode;

struct QueryChoice {
    const std::vector<Query> plain;
    const std::vector<Query> single;

    QueryChoice(const std::vector<Query> &plain_arg,
                const std::vector<Query> &single_arg)
        : plain(plain_arg), single(single_arg)
    {
        assert(plain_arg.size() == single_arg.size());
    }

    const std::vector<Query> &choose(test_mode t) const {
        switch (t) {
        case UNENCRYPTED:
        case PROXYPLAIN:
            return plain;

        case SINGLE:
        case PROXYSINGLE:
        case ENC:
            return single;

        default:
            assert(0);
        }
    }

    size_t size() const {
        return plain.size();
    }
};

struct QueryList {
    std::string name;
    QueryChoice create;
    std::vector<Query> common;
    QueryChoice drop;

    QueryList(std::string namearg,
              std::vector<Query> pc,
              std::vector<Query> sc,
              std::vector<Query> c,
              std::vector<Query> pd,
              std::vector<Query> sd)
        : name(namearg),
          create(pc, sc),
          common(c),
          drop(pd, sd)
    {}
};

class Connection {
 public:
    Connection(const TestConfig &tc, test_mode type);
    ~Connection();

    ResType execute(const Query &query);
    my_ulonglong executeLast();

    void restart();
    void start();
    void stop();

    ProxyState *getProxyState()
    {
        assert(re_set.size() == 1);
        return *re_set.begin();
    }

 private:
    test_mode type;
    TestConfig tc;

    //TODO/FIXME: check this
    Rewriter * re;

    //Rewriter object for proxy
    ProxyState * re_proxy;

    //It allow multiple proxy connections however CryptDB
    //doesn't seem to support multiple proxy connections, anyways
    //it serves as a temporary solution for Rewriter instance issue.
    std::set<ProxyState *> re_set;

    //Connect objects for plain
    std::set<Connect *> conn_set;

    //current connection we are on, for multiple connections
    std::set<Connect *>::iterator conn;

    //current connection we are on
    std::set<ProxyState *>::iterator re_it;

    pid_t proxy_pid;

    SchemaCache schema_cache;

    ResType executeConn(const Query &query);
    ResType executeEDBProxy(const Query &query);
    ResType executeRewriter(const Query &query);

    my_ulonglong executeLastConn();
    my_ulonglong executeLastEDB();
    my_ulonglong executeRewriter();

    void executeFail(const Query &query);
};

class TestQueries {
 public:
    TestQueries();
    ~TestQueries();

    static void run(const TestConfig &tc, int argc, char ** argv);

};
