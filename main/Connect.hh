#pragma once

/*
 * Connect.h
 *
 */

#include <vector>
#include <string>
#include <memory>

#include <util/util.hh>
#include <parser/sql_utils.hh>

#include <mysql.h>
typedef MYSQL_RES DBResult_native;

extern "C" void *create_embedded_thd(int client_flag);

class DBResult {
 private:
    DBResult();

 public:
    ~DBResult();
    DBResult_native *n;

    //returns data from this db result
    ResType unpack();

    static DBResult *wrap(DBResult_native *);
};

class Connect {
 public:
    Connect(const std::string &server, const std::string &user,
            const std::string &passwd, uint port = 0);

    Connect(MYSQL *const _conn) : conn(_conn), close_on_destroy(false) { }

    //returns Connect for the embedded server
    static Connect *getEmbedded(const std::string &embed_dir);

    // returns true if execution was ok; caller must delete DBResult
    bool execute(const std::string &query, std::unique_ptr<DBResult> *res,
                 bool multiple_resultsets=false);
    bool execute(const std::string &query, bool multiple_resultsets=false);

    // returns error message if a query caused error
    std::string getError();

    my_ulonglong last_insert_id();
    unsigned long real_escape_string(char *const to,
                                     const char *const from,
                                     unsigned long length);
    unsigned int get_mysql_errno();

    ~Connect();

 private:
    MYSQL *conn;

    void do_connect(const std::string &server, const std::string &user,
                    const std::string &passwd, uint port);

    bool close_on_destroy;
};
