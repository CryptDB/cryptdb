#include <sstream>
#include <fstream>
#include <assert.h>
#include <lua5.1/lua.hpp>

#include <util/ctr.hh>
#include <util/cryptdb_log.hh>
#include <util/scoped_lock.hh>

#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <parser/sql_utils.hh>

// FIXME: Ownership semantics.
class WrapperState {
    WrapperState(const WrapperState &other);
    WrapperState &operator=(const WrapperState &rhs);

public:
    std::string last_query;
    std::string default_db;
    std::ofstream * PLAIN_LOG;

    WrapperState() {}
    ~WrapperState() {}

    SchemaCache &getSchemaCache() {return schema_cache;}
    const std::unique_ptr<QueryRewrite> &getQueryRewrite() const {
        assert(this->qr);
        return this->qr;
    }
    void setQueryRewrite(QueryRewrite *const in_qr) {
        // assert(!this->qr);
        this->qr = std::unique_ptr<QueryRewrite>(in_qr);
    }

private:
    std::unique_ptr<QueryRewrite> qr;
    SchemaCache schema_cache;
};

static Timer t;

//static EDBProxy * cl = NULL;
static ProxyState * ps = NULL;
static pthread_mutex_t big_lock;

static bool EXECUTE_QUERIES = true;

static std::string TRAIN_QUERY ="";

static bool LOG_PLAIN_QUERIES = false;
static std::string PLAIN_BASELOG = "";


static int counter = 0;

static std::map<std::string, WrapperState*> clients;

static int
returnResultSet(lua_State *L, const ResType &res);

static Item *
make_item_by_type(const std::string &value, enum_field_types type)
{
    Item * i;

    switch(type) {
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_TINY:
        i = new (current_thd->mem_root) Item_int(static_cast<long long>(valFromStr(value)));
        break;

    case MYSQL_TYPE_DOUBLE:
        i = new (current_thd->mem_root) Item_float(value.c_str(),
                                                   value.size());
        break;

    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
        i = new (current_thd->mem_root) Item_string(make_thd_string(value),
                                                    value.length(),
                                                    &my_charset_bin);
        break;

    default:
        thrower() << "unknown data type " << type;
    }
    return i;
}

static Item_null *
make_null(const std::string &name = "")
{
    char *const n = current_thd->strdup(name.c_str());
    return new Item_null(n);
}

static std::string
xlua_tolstring(lua_State *const l, int index)
{
    size_t len;
    char const *const s = lua_tolstring(l, index, &len);
    return std::string(s, len);
}

static void
xlua_pushlstring(lua_State *const l, const std::string &s)
{
    lua_pushlstring(l, s.data(), s.length());
}

static int
connect(lua_State *const L)
{
    ANON_REGION(__func__, &perf_cg);
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());

    const std::string client = xlua_tolstring(L, 1);
    const std::string server = xlua_tolstring(L, 2);
    const uint port = luaL_checkint(L, 3);
    const std::string user = xlua_tolstring(L, 4);
    const std::string psswd = xlua_tolstring(L, 5);
    const std::string embed_dir = xlua_tolstring(L, 6);

    ConnectionInfo const ci = ConnectionInfo(server, user, psswd, port);

    if (clients.find(client) != clients.end()) {
           LOG(warn) << "duplicate client entry";
    }

    clients[client] = new WrapperState();

    // Is it the first connection?
    if (!ps) {
        std::cerr << "starting proxy\n";
        //cryptdb_logger::setConf(string(getenv("CRYPTDB_LOG")?:""));

        LOG(wrapper) << "connect " << client << "; "
                     << "server = " << server << ":" << port << "; "
                     << "user = " << user << "; "
                     << "password = " << psswd;

        std::string const false_str = "FALSE";
        const std::string dbname = "cryptdbtest";
        const std::string mkey = "113341234";  // XXX do not change as
                                               // it's used for tpcc exps
        const char * ev = getenv("ENC_BY_DEFAULT");
        if (ev && equalsIgnoreCase(false_str, ev)) {
            std::cerr << "\n\n enc by default false " << "\n\n";
            ps = new ProxyState(ci, embed_dir, mkey,
                                SECURITY_RATING::PLAIN);
        } else {
            std::cerr << "\n\nenc by default true" << "\n\n";
            ps = new ProxyState(ci, embed_dir, mkey,
                                SECURITY_RATING::BEST_EFFORT);
        }

        //may need to do training
        ev = getenv("TRAIN_QUERY");
        if (ev) {
            std::cerr << "Deprecated query training!" << std::endl;
        }

        ev = getenv("EXECUTE_QUERIES");
        if (ev && equalsIgnoreCase(false_str, ev)) {
            LOG(wrapper) << "do not execute queries";
            EXECUTE_QUERIES = false;
        } else {
            LOG(wrapper) << "execute queries";
            EXECUTE_QUERIES = true;
        }

        ev = getenv("LOAD_ENC_TABLES");
        if (ev) {
            std::cerr << "No current functionality for loading tables\n";
            //cerr << "loading enc tables\n";
            //cl->loadEncTables(string(ev));
        }

        ev = getenv("LOG_PLAIN_QUERIES");
        if (ev) {
            std::string logPlainQueries = std::string(ev);
            if (logPlainQueries != "") {
                LOG_PLAIN_QUERIES = true;
                PLAIN_BASELOG = logPlainQueries;
                logPlainQueries += StringFromVal(++counter);

                assert_s(system(("rm -f" + logPlainQueries + "; touch " + logPlainQueries).c_str()) >= 0, "failed to rm -f and touch " + logPlainQueries);

                std::ofstream * const PLAIN_LOG =
                    new std::ofstream(logPlainQueries, std::ios_base::app);
                LOG(wrapper) << "proxy logs plain queries at " << logPlainQueries;
                assert_s(PLAIN_LOG != NULL, "could not create file " + logPlainQueries);
                clients[client]->PLAIN_LOG = PLAIN_LOG;
            } else {
                LOG_PLAIN_QUERIES = false;
            }
        }
    } else {
        if (LOG_PLAIN_QUERIES) {
            std::string logPlainQueries =
                PLAIN_BASELOG+StringFromVal(++counter);
            assert_s(system((" touch " + logPlainQueries).c_str()) >= 0, "failed to remove or touch plain log");
            LOG(wrapper) << "proxy logs plain queries at " << logPlainQueries;

            std::ofstream * const PLAIN_LOG =
                new std::ofstream(logPlainQueries, std::ios_base::app);
            assert_s(PLAIN_LOG != NULL, "could not create file " + logPlainQueries);
            clients[client]->PLAIN_LOG = PLAIN_LOG;
        }
    }

    return 0;
}

static int
disconnect(lua_State *const L)
{
    ANON_REGION(__func__, &perf_cg);
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());

    const std::string client = xlua_tolstring(L, 1);
    if (clients.find(client) == clients.end()) {
        return 0;
    }

    LOG(wrapper) << "disconnect " << client;

    auto ws = clients[client];
    clients[client] = NULL;

    SchemaCache &schema_cache = ws->getSchemaCache();
    TEST_TextMessageError(schema_cache.cleanupStaleness(ps->getEConn()),
                          "Failed to cleanup staleness!");
    delete ws;
    clients.erase(client);

    return 0;
}

static int
rewrite(lua_State *const L)
{
    ANON_REGION(__func__, &perf_cg);
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());

    const std::string client = xlua_tolstring(L, 1);
    if (clients.find(client) == clients.end()) {
        return 0;
    }
    WrapperState *const c_wrapper = clients[client];

    const std::string query = xlua_tolstring(L, 2);
    const unsigned long long _thread_id =
        strtoull(xlua_tolstring(L, 3).c_str(), NULL, 10);

    std::list<std::string> new_queries;

    c_wrapper->last_query = query;
    t.lap_ms();
    if (EXECUTE_QUERIES) {
        try {
            assert(ps);

            SchemaCache &schema_cache = c_wrapper->getSchemaCache();
            std::unique_ptr<QueryRewrite> qr;
            TEST_TextMessageError(retrieveDefaultDatabase(_thread_id,
                                                          ps->getConn(),
                                            &c_wrapper->default_db),
                            "proxy failed to retrieve default database!");
            queryPreamble(*ps, query, &qr, &new_queries, &schema_cache,
                          c_wrapper->default_db);
            assert(qr);

            c_wrapper->setQueryRewrite(qr.release());
        } catch (const SynchronizationException &e) {
            lua_pushboolean(L, false);              // status
            xlua_pushlstring(L, e.to_string());     // error message
            lua_pushnil(L);                         // new queries
            return 3;
        } catch (const AbstractException &e) {
            lua_pushboolean(L, false);              // status
            xlua_pushlstring(L, e.to_string());     // error message
            lua_pushnil(L);                         // new queries
            return 3;
        } catch (const CryptDBError &e) {
            LOG(wrapper) << "cannot rewrite " << query << ": " << e.msg;
            lua_pushboolean(L, false);              // status
            xlua_pushlstring(L, e.msg);             // error message
            lua_pushnil(L);                         // new queries
            return 3;
        }
    }

    if (LOG_PLAIN_QUERIES) {
        *(c_wrapper->PLAIN_LOG) << query << "\n";
    }

    lua_pushboolean(L, true);                       // status
    lua_pushnil(L);                                 // error message

    // NOTE: Potentially out of int range.
    assert(new_queries.size() < INT_MAX);
    lua_createtable(L, static_cast<int>(new_queries.size()), 0);
    const int top = lua_gettop(L);
    int index = 1;
    for (auto it : new_queries) {
        xlua_pushlstring(L, it);                    // new queries
        lua_rawseti(L, top, index);
        index++;
    }

    return 3;
}

inline std::vector<std::shared_ptr<Item> >
itemNullVector(unsigned int count)
{
    std::vector<std::shared_ptr<Item> > out;
    for (unsigned int i = 0; i < count; ++i) {
        out.push_back(std::shared_ptr<Item>(make_null()));
    }

    return out;
}

static void
getResTypeFromLuaTable(lua_State *const L, int fields_index,
                       int rows_index, ResType *const out_res)
{
    /* iterate over the fields argument */
    lua_pushnil(L);
    while (lua_next(L, fields_index)) {
        if (!lua_istable(L, -1))
            LOG(warn) << "mismatch";

        lua_pushnil(L);
        while (lua_next(L, -2)) {
            const std::string k = xlua_tolstring(L, -2);
            if ("name" == k) {
                out_res->names.push_back(xlua_tolstring(L, -1));
            } else if ("type" == k) {
                out_res->types.push_back(static_cast<enum_field_types>(luaL_checkint(L, -1)));
            } else {
                LOG(warn) << "unknown key " << k;
            }
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
    }

    assert(out_res->names.size() == out_res->types.size());

    /* iterate over the rows argument */
    lua_pushnil(L);
    while (lua_next(L, rows_index)) {
        if (!lua_istable(L, -1))
            LOG(warn) << "mismatch";

        /* initialize all items to NULL, since Lua skips
           nil array entries */
        std::vector<std::shared_ptr<Item> > row =
            itemNullVector(out_res->types.size());

        lua_pushnil(L);
        while (lua_next(L, -2)) {
            const int key = luaL_checkint(L, -2) - 1;

            assert(key >= 0
                   && static_cast<uint>(key) < out_res->types.size());
            const std::string data = xlua_tolstring(L, -1);
            Item *const value =
                make_item_by_type(data, out_res->types[key]);
            row[key] = std::shared_ptr<Item>(value);

            lua_pop(L, 1);
        }
        // We can not use this assert because rows that contain many
        // NULLs don't return their columns in a strictly increasing
        // order.
        // assert((unsigned int)key == out_res->names.size() - 1);

        out_res->rows.push_back(row);
        lua_pop(L, 1);
    }
}

static int
envoi(lua_State *const L)
{
    ANON_REGION(__func__, &perf_cg);
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());

    THD *const thd = static_cast<THD *>(create_embedded_thd(0));
    auto thd_cleanup = cleanup([&thd]
        {
            thd->clear_data_list();
            thd->store_globals();
            thd->unlink();
            delete thd;
        });

    const std::string client = xlua_tolstring(L, 1);
    if (clients.find(client) == clients.end()) {
        return 0;
    }
    WrapperState *const c_wrapper = clients[client];

    assert(EXECUTE_QUERIES);
    assert(ps);

    ResType res;
    getResTypeFromLuaTable(L, 2, 3, &res);
    const std::unique_ptr<QueryRewrite> &qr = c_wrapper->getQueryRewrite();
    try {
        const EpilogueResult &epi_result =
            queryEpilogue(*ps, *qr.get(), res, c_wrapper->last_query,
                          c_wrapper->default_db, false);
        if (QueryAction::ROLLBACK == epi_result.action) {
            lua_pushboolean(L, true);           // success
            lua_pushboolean(L, true);           // rollback
            lua_pushnil(L);                     // error message
            lua_pushnil(L);                     // plaintext fields
            lua_pushnil(L);                     // plaintext rows
            return 5;
        }

        assert(QueryAction::VANILLA == epi_result.action);
        return returnResultSet(L, epi_result.res_type);
    } catch (const SynchronizationException &e) {
        lua_pushboolean(L, false);              // status
        lua_pushboolean(L, false);              // rollback
        xlua_pushlstring(L, e.to_string());     // error message
        lua_pushnil(L);                         // plaintext fields
        lua_pushnil(L);                         // plaintext rows
        return 5;
    } catch (const AbstractException &e) {
        lua_pushboolean(L, false);              // status
        lua_pushboolean(L, false);              // rollback
        xlua_pushlstring(L, e.to_string());     // error message
        lua_pushnil(L);                         // plaintext fields
        lua_pushnil(L);                         // plaintext rows
        return 5;
    } catch (const CryptDBError &e) {
        lua_pushboolean(L, false);              // status
        lua_pushboolean(L, false);              // rollback
        xlua_pushlstring(L, e.msg);             // error message
        lua_pushnil(L);                         // plaintext fields
        lua_pushnil(L);                         // plaintext rows
        return 5;
    }
}

static int
returnResultSet(lua_State *const L, const ResType &rd)
{
    lua_pushboolean(L, true);                   // status
    lua_pushboolean(L, false);                  // rollback
    lua_pushnil(L);                             // error message

    /* return decrypted result set */
    lua_createtable(L, (int)rd.names.size(), 0);
    int const t_fields = lua_gettop(L);
    for (uint i = 0; i < rd.names.size(); i++) {
        lua_createtable(L, 0, 1);
        int const t_field = lua_gettop(L);

        /* set name for field */
        xlua_pushlstring(L, rd.names[i]);       // plaintext fields
        lua_setfield(L, t_field, "name");

/*
        // FIXME.
        // set type for field
        lua_pushinteger(L, rd.types[i]);
        lua_setfield(L, t_field, "type");
*/

        /* insert field element into fields table at i+1 */
        lua_rawseti(L, t_fields, i+1);
    }

    lua_createtable(L, static_cast<int>(rd.rows.size()), 0);
    int const t_rows = lua_gettop(L);
    for (uint i = 0; i < rd.rows.size(); i++) {
        lua_createtable(L, static_cast<int>(rd.rows[i].size()), 0);
        int const t_row = lua_gettop(L);

        for (uint j = 0; j < rd.rows[i].size(); j++) {
            if (NULL == rd.rows[i][j]) {
                lua_pushnil(L);                 // plaintext rows
            } else {
                xlua_pushlstring(L,             // plaintext rows
                                 ItemToString(*rd.rows[i][j]));
            }
            lua_rawseti(L, t_row, j+1);
        }

        lua_rawseti(L, t_rows, i+1);
    }

    return 5;
}

static const struct luaL_reg
cryptdb_lib[] = {
#define F(n) { #n, n }
    F(connect),
    F(disconnect),
    F(rewrite),
    F(envoi),
    { 0, 0 },
};

extern "C" int lua_cryptdb_init(lua_State * L);

int
lua_cryptdb_init(lua_State *const L)
{
    luaL_openlib(L, "CryptDB", cryptdb_lib, 0);
    return 1;
}
