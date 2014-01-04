/*
 * Connect.cpp
 *
 *  Created on: Dec 1, 2010
 *      Author: raluca
 */

#include <stdexcept>
#include <assert.h>
#include <string>
#include <iostream>
#include <sstream>
#include <memory>

#include <main/Connect.hh>
#include <main/macro_util.hh>
#include <util/cryptdb_log.hh>
#include <main/schema.hh>

Connect::Connect(const std::string &server, const std::string &user,
                 const std::string &passwd, uint port)
    : conn(nullptr), close_on_destroy(true)
{
    do_connect(server, user, passwd, port);
}

void
Connect::do_connect(const std::string &server, const std::string &user,
                    const std::string &passwd, uint port)
{
    const char *dummy_argv[] = {
        "progname",
        "--skip-grant-tables",
        "--skip-innodb",
        "--default-storage-engine=MEMORY",
        "--character-set-server=utf8",
        "--language=" MYSQL_BUILD_DIR "/sql/share/"
    };
    assert(0 == mysql_library_init(sizeof(dummy_argv)/sizeof(*dummy_argv),
                                   const_cast<char**>(dummy_argv), 0));

    conn = mysql_init(NULL);

    /* Connect via TCP, and not via Unix domain sockets */
    const uint proto = MYSQL_PROTOCOL_TCP;
    mysql_options(conn, MYSQL_OPT_PROTOCOL, &proto);

    /* Connect to real server even if linked against embedded libmysqld */
    mysql_options(conn, MYSQL_OPT_USE_REMOTE_CONNECTION, 0);

    /* Connect to database */
    if (!mysql_real_connect(conn, server.c_str(), user.c_str(),
                            passwd.c_str(), 0, port, 0,
                            CLIENT_MULTI_STATEMENTS)) {
        LOG(warn) << "connecting to server " << server
                  << " user " << user
                  << " pwd " << passwd
                  << " port " << port;
        LOG(warn) << "mysql_real_connect: " << mysql_error(conn);
        throw std::runtime_error("cannot connect");
    }
}

Connect *Connect::getEmbedded(const std::string &embed_db)
{
    init_mysql(embed_db);

    MYSQL *const m = mysql_init(0);
    assert(m);

    mysql_options(m, MYSQL_OPT_USE_EMBEDDED_CONNECTION, 0);

    if (!mysql_real_connect(m, 0, 0, 0, 0, 0, 0,
                            CLIENT_MULTI_STATEMENTS)) {
        mysql_close(m);
        cryptdb_err() << "mysql_real_connect: " << mysql_error(m);
    }

    Connect *const conn = new Connect(m);
    conn->close_on_destroy = true;

    return conn;
}

// @multiple_resultsets causes us to ignore query results.
// > This is a hack that allows us to deal with the two sets which
// are returned when CALLing a stored procedure.
bool
Connect::execute(const std::string &query, std::unique_ptr<DBResult> *res,
                 bool multiple_resultsets)
{
    //silently ignore empty queries
    if (query.length() == 0) {
        LOG(warn) << "empty query";
        *res = nullptr;
        return true;
    }
    bool success = true;
    if (mysql_query(conn, query.c_str())) {
        LOG(warn) << "mysql_query: " << mysql_error(conn);
        LOG(warn) << "on query: " << query;
        *res = nullptr;
        success = false;
    } else {
        if (false == multiple_resultsets) {
            *res =
                std::unique_ptr<DBResult>(DBResult::wrap(mysql_store_result(conn)));
        } else {
            int status;
            do {
                DBResult_native *const res_native =
                    mysql_store_result(conn);
                if (res_native) {
                    mysql_free_result(res_native);
                } else {
                    assert(mysql_field_count(conn) == 0);
                }
                status = mysql_next_result(conn);
                assert(status <= 0);
            } while (0 == status);

            *res = nullptr;
        }
    }

    void *const ret = create_embedded_thd(0);
    if (!ret) assert(false);

    return success;
}


bool
Connect::execute(const std::string &query, bool multiple_resultsets)
{
    std::unique_ptr<DBResult> aux;
    const bool r = execute(query, &aux, multiple_resultsets);
    return r;
}

std::string
Connect::getError()
{
    return mysql_error(conn);
}

my_ulonglong
Connect::last_insert_id()
{
    return mysql_insert_id(conn);
}

unsigned long
Connect::real_escape_string(char *const to, const char *const from,
                            unsigned long length)
{
    return mysql_real_escape_string(conn, to, from, length);
}

unsigned int
Connect::get_mysql_errno()
{
    return mysql_errno(conn);
}

Connect::~Connect()
{
    if (close_on_destroy) {
        mysql_close(conn);
    }
}

DBResult::DBResult()
{}

DBResult *
DBResult::wrap(DBResult_native *const n)
{
    DBResult *const r = new DBResult();
    r->n = n;
    return r;
}

DBResult::~DBResult()
{
    mysql_free_result(n);
}

static Item *
getItem(char *const content, enum_field_types type, uint len)
{
    if (content == NULL) {
        return new Item_null();
    }
    const std::string content_str = std::string(content, len);
    if (IsMySQLTypeNumeric(type)) {
        const ulonglong val = valFromStr(content_str);
        return new Item_int(val);
    } else {
        return new Item_string(make_thd_string(content_str), len,
                               &my_charset_bin);
    }
}

// > returns the data in the last server response
// > TODO: to optimize return pointer to avoid overcopying large
//   result sets?
// > This function must not return pointers (internal or otherwise) to
//   objects that it owns.
//   > ie, We must be able to delete 'this' without mangling the 'ResType'
//     returned from this->unpack(...).
ResType
DBResult::unpack()
{
    if (nullptr == n) {
        return ResType();
    }

    const size_t rows = static_cast<size_t>(mysql_num_rows(n));
    if (0 == rows) {
        return ResType();
    }

    const int cols = mysql_num_fields(n);

    ResType res;

    for (int j = 0;; j++) {
        MYSQL_FIELD *const field = mysql_fetch_field(n);
        if (!field) {
            break;
        }

        res.names.push_back(field->name);
        res.types.push_back(field->type);
    }

    for (int index = 0;; index++) {
        MYSQL_ROW row = mysql_fetch_row(n);
        if (!row) {
            break;
        }
        unsigned long *const lengths = mysql_fetch_lengths(n);

        std::vector<std::shared_ptr<Item> > resrow;

        for (int j = 0; j < cols; j++) {
            Item *const item = getItem(row[j], res.types[j], lengths[j]);
            resrow.push_back(std::shared_ptr<Item>(item));
        }

        res.rows.push_back(resrow);
    }

    return res;
}
