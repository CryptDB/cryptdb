#include <string>
#include <memory>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <list>
#include <algorithm>
#include <stdio.h>
#include <typeinfo>

#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <util/cryptdb_log.hh>
#include <util/enum_text.hh>
#include <main/CryptoHandlers.hh>
#include <parser/lex_util.hh>
#include <main/sql_handler.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <main/metadata_tables.hh>
#include <main/macro_util.hh>

#include "field.h"

extern CItemTypesDir itemTypes;
extern CItemFuncDir funcTypes;
extern CItemSumFuncDir sumFuncTypes;
extern CItemFuncNameDir funcNames;

#define ANON                ANON_NAME(__anon_id_)

//TODO: use getAssert in more places
//TODO: replace table/field with FieldMeta * for speed and conciseness

/*
static Item_field *
stringToItemField(const std::string &field,
                  const std::string &table, Item_field *const itf)
{
    THD *const thd = current_thd;
    assert(thd);
    Item_field *const res = new Item_field(thd, itf);
    res->name = NULL; //no alias
    res->field_name = make_thd_string(field);
    res->table_name = make_thd_string(table);

    return res;
}
*/

std::string global_crash_point = "";

void
crashTest(const std::string &current_point) {
    if (current_point == global_crash_point) {
      throw std::runtime_error("crash test exception");
    }
}

static inline std::string
extract_fieldname(Item_field *const i)
{
    std::stringstream fieldtemp;
    fieldtemp << *i;
    return fieldtemp.str();
}


//TODO: remove this at some point
static inline void
mysql_query_wrapper(MYSQL *const m, const std::string &q)
{
    if (mysql_query(m, q.c_str())) {
        cryptdb_err() << "query failed: " << q
                << " reason: " << mysql_error(m);
    }

    // HACK(stephentu):
    // Calling mysql_query seems to have destructive effects
    // on the current_thd. Thus, we must call create_embedded_thd
    // again.
    void *const ret = create_embedded_thd(0);
    if (!ret) assert(false);
}

bool
sanityCheck(FieldMeta &fm)
{
    for (auto it = fm.children.begin(); it != fm.children.end();
         it++) {
        OnionMeta *const om = (*it).second.get();
        const onion o = (*it).first.getValue();
        const std::vector<SECLEVEL> &secs = fm.getOnionLayout().at(o);
        for (size_t i = 0; i < om->layers.size(); ++i) {
            std::unique_ptr<EncLayer> const &layer = om->layers[i];
            assert(layer->level() == secs[i]);
        }
    }
    return true;
}

static bool
sanityCheck(TableMeta &tm)
{
    for (auto it = tm.children.begin(); it != tm.children.end(); it++) {
        const std::unique_ptr<FieldMeta> &fm = (*it).second;
        assert(sanityCheck(*fm.get()));
    }
    return true;
}

static bool
sanityCheck(DatabaseMeta &dm)
{
    for (auto it = dm.children.begin(); it != dm.children.end(); it++) {
        const std::unique_ptr<TableMeta> &tm = (*it).second;
        assert(sanityCheck(*tm.get()));
    }
    return true;
}

static bool
sanityCheck(SchemaInfo &schema)
{
    for (auto it = schema.children.begin(); it != schema.children.end();
         it++) {
        const std::unique_ptr<DatabaseMeta> &tm = (*it).second;
        assert(sanityCheck(*tm.get()));
    }
    return true;
}

struct RecoveryDetails {
    const bool embedded_begin;
    const bool embedded_complete;
    const bool existed_remote;
    const bool remote_begin;
    const bool remote_complete;
    const std::string query;
    const std::string default_db;

    RecoveryDetails(bool embedded_begin, bool embedded_complete,
                    bool existed_remote, bool remote_begin,
                    bool remote_complete, const std::string &query,
                    const std::string &default_db)
        : embedded_begin(embedded_begin),
          embedded_complete(embedded_complete),
          existed_remote(existed_remote), remote_begin(remote_begin),
          remote_complete(remote_complete), query(query),
          default_db(default_db) {}
};

static bool
false_if_false(bool test, const std::string &new_value)
{
    if (false == test) {
        return test;
    }

    return string_to_bool(new_value);
}
static bool
collectRecoveryDetails(const std::unique_ptr<Connect> &conn,
                       const std::unique_ptr<Connect> &e_conn,
                       unsigned long unfinished_id,
                       std::unique_ptr<RecoveryDetails> *details)
{
    const std::string embedded_completion_table =
        MetaData::Table::embeddedQueryCompletion();
    const std::string remote_completion_table =
        MetaData::Table::remoteQueryCompletion();

    // collect completion data
    std::unique_ptr<DBResult> dbres;
    const std::string embedded_completion_q =
        " SELECT begin, complete, original_query, default_db FROM " +
            embedded_completion_table +
        "  WHERE id = " + std::to_string(unfinished_id) + ";";
    RETURN_FALSE_IF_FALSE(e_conn->execute(embedded_completion_q, &dbres));
    assert(mysql_num_rows(dbres->n) == 1);

    const MYSQL_ROW embedded_row = mysql_fetch_row(dbres->n);
    const unsigned long *const l = mysql_fetch_lengths(dbres->n);
    const std::string string_embedded_begin(embedded_row[0], l[0]);
    const std::string string_embedded_complete(embedded_row[1], l[1]);
    const std::string string_embedded_query(embedded_row[2], l[2]);
    const std::string string_embedded_default_db(embedded_row[3], l[3]);

    const std::string remote_completion_q =
        " SELECT begin, complete FROM " + remote_completion_table +
        "  WHERE embedded_completion_id = " +
                 std::to_string(unfinished_id) + ";";
    RETURN_FALSE_IF_FALSE(conn->execute(remote_completion_q, &dbres));

    const unsigned long remote_row_count = mysql_num_rows(dbres->n);
    const MYSQL_ROW remote_row = mysql_fetch_row(dbres->n);
    assert(!!remote_row == !!remote_row_count);

    std::string string_remote_begin, string_remote_complete;
    const bool existed_remote = !!remote_row;
    if (existed_remote) {
        const unsigned long *const l = mysql_fetch_lengths(dbres->n);
        string_remote_begin    = std::string(remote_row[0], l[0]);
        string_remote_complete = std::string(remote_row[1], l[1]);
        assert(string_to_bool(string_remote_begin) == true);
    }

    const bool embedded_begin = string_to_bool(string_embedded_begin);
    const bool embedded_complete =
        string_to_bool(string_embedded_complete);
    const bool remote_begin =
        false_if_false(existed_remote, string_remote_begin);
    const bool remote_complete =
        false_if_false(existed_remote, string_remote_complete);

    assert(true == embedded_begin);

    *details =
        std::unique_ptr<RecoveryDetails>(
            new RecoveryDetails(embedded_begin, embedded_complete,
                                existed_remote, remote_begin,
                                remote_complete, string_embedded_query,
                                string_embedded_default_db));

    return true;
}

static bool
abortQuery(const std::unique_ptr<Connect> &e_conn,
           unsigned long unfinished_id)
{
    const std::string embedded_completion_table =
        MetaData::Table::embeddedQueryCompletion();

    const std::string update_aborted =
        " UPDATE " + embedded_completion_table +
        "    SET aborted = TRUE"
        "  WHERE id = " + std::to_string(unfinished_id) + ";";

    RETURN_FALSE_IF_FALSE(e_conn->execute("START TRANSACTION"));
    ROLLBACK_AND_RFIF(setBleedingTableToRegularTable(e_conn), e_conn);
    ROLLBACK_AND_RFIF(e_conn->execute(update_aborted), e_conn);
    ROLLBACK_AND_RFIF(e_conn->execute("COMMIT"), e_conn);

    return true;
}

static bool
finishQuery(const std::unique_ptr<Connect> &e_conn,
            unsigned long unfinished_id)
{
    const std::string embedded_completion_table =
        MetaData::Table::embeddedQueryCompletion();

    const std::string update_completed =
        " UPDATE " + embedded_completion_table +
        "    SET complete = TRUE"
        "  WHERE id = " + std::to_string(unfinished_id) + ";";

    RETURN_FALSE_IF_FALSE(e_conn->execute("START TRANSACTION"));
    ROLLBACK_AND_RFIF(setRegularTableToBleedingTable(e_conn), e_conn);
    ROLLBACK_AND_RFIF(e_conn->execute(update_completed), e_conn);
    ROLLBACK_AND_RFIF(e_conn->execute("COMMIT"), e_conn);

    return true;
}

static bool
fixAdjustOnion(const std::unique_ptr<Connect> &conn,
               const std::unique_ptr<Connect> &e_conn,
               unsigned long unfinished_id)
{
    std::unique_ptr<RecoveryDetails> details;
    RETURN_FALSE_IF_FALSE(collectRecoveryDetails(conn, e_conn,
                                                 unfinished_id,
                                                 &details));
    assert(details->remote_begin == details->remote_complete);

    lowLevelSetCurrentDatabase(e_conn, details->default_db);

    // failure after initial embedded queries and before remote queries
    if (false == details->remote_begin) {
        assert(false == details->embedded_complete
               && false == details->existed_remote
               && false == details->remote_complete);
        return abortQuery(e_conn, unfinished_id);
    }

    assert(true == details->remote_complete);

    // failure after remote queries
    {
        assert(false == details->embedded_complete);

        return finishQuery(e_conn, unfinished_id);
    }
}

/*
    Other interesting error codes
    > ER_DUP_KEY
    > ER_KEY_DOES_NOT_EXIST
*/
static bool
recoverableDeltaError(unsigned int err)
{
    const bool ret =
        ER_DB_CREATE_EXISTS == err ||       // Database already exists.
        ER_TABLE_EXISTS_ERROR == err ||     // Table already exists.
        ER_DUP_FIELDNAME == err ||          // Column already exists.
        ER_DUP_KEYNAME == err ||            // Key already exists.
        ER_DB_DROP_EXISTS == err ||         // Database doesn't exist.
        ER_BAD_TABLE_ERROR == err ||        // Table doesn't exist.
        ER_CANT_DROP_FIELD_OR_KEY == err;   // Key/Col doesn't exist.

    return ret;
}

// we use a blacklist to determine if the query is bad and thus failed at
// the remote server. if the query fails, but not for one of these
// reasons, we can be reasonably sure that it did not succeed initially.
// the blacklist include errors related to 'bad mysq/connection state'.
// if a query fails for connectivity reasons during recovery, we still
// don't know anything about why it failed initially; or even if it
// succeeded initially.
//
// essentially the blacklist is all errors that could be thrown against
// a 'good' query.
//
// blacklist taken from here:
//  http://dev.mysql.com/doc/refman/5.0/en/mysql-stat.html
//
// we could potentially use a whitelist which would contain errors that
// won't be caught by query_parse(...) and will result in the query
// failing to execute remotely.
static bool
queryInitiallyFailedErrors(unsigned int err)
{
    // lifted from mysql-src/includes/errmsg.h
    unsigned long cr_unknown_error        = 2000,
                  cr_server_gone_error    = 2006,
                  cr_server_lost          = 2013,
                  cr_commands_out_of_sync = 2014;

    const bool ret =
        cr_unknown_error == err ||
        cr_server_gone_error == err ||
        cr_server_lost == err ||
        cr_commands_out_of_sync == err;

    return !ret;
}

// 'bad_query' is a sanity checking mechanism; if the query is bad
// against the remote database, it should also be bad against the embedded
// database.
static bool
retryQuery(const std::unique_ptr<Connect> &c, const std::string &query,
           bool *const bad_query)
{
    assert(bad_query);

    *bad_query = false;

    if (false == c->execute(query)) {
        const unsigned int err = c->get_mysql_errno();
        // if the error is not recoverable, we must determine if
        // the query failed initially for the same error.
        if (false == recoverableDeltaError(err)) {
            *bad_query = queryInitiallyFailedErrors(err);
            RETURN_FALSE_IF_FALSE(*bad_query);

            // We could abort the query here because we know that
            // the query is bad and can't be processed by the remote
            // or embedded server.
        }
    }

    return true;
}

static bool
fixDDL(const std::unique_ptr<Connect> &conn,
       const std::unique_ptr<Connect> &e_conn,
       unsigned long unfinished_id)
{
    const std::string remote_completion_table =
        MetaData::Table::remoteQueryCompletion();

    std::unique_ptr<RecoveryDetails> details;
    RETURN_FALSE_IF_FALSE(collectRecoveryDetails(conn, e_conn,
                                                 unfinished_id,
                                                 &details));
    assert(true == details->embedded_begin
           && false == details->embedded_complete);

    lowLevelSetCurrentDatabase(e_conn, details->default_db);
    lowLevelSetCurrentDatabase(conn, details->default_db);

    // failure after initial embedded queries and before remote queries
    if (false == details->remote_begin) {
        assert(false == details->embedded_complete
               && false == details->existed_remote
               && false == details->remote_complete);
        return abortQuery(e_conn, unfinished_id);
    }

    // --------------------------------------------------
    //  After this point we must run to completion as we
    //        _may_ have made a DDL modification
    //  > unless we determine that it is a bad query.
    //  -------------------------------------------------

    // ugly sanity checking device
    AssignOnce<bool> remote_bad_query;
    // failure before remote queries complete
    if (false == details->remote_complete) {
        bool rbq;
        // reissue the DDL query against the remote database.
        RETURN_FALSE_IF_FALSE(retryQuery(conn, details->query,
                                         &rbq));
        remote_bad_query = rbq;

        const std::string update_remote_complete =
            "UPDATE " + remote_completion_table +
            "   SET complete = TRUE"
            " WHERE embedded_completion_id = " +
                    std::to_string(unfinished_id) + ";";
        RETURN_FALSE_IF_FALSE(conn->execute(update_remote_complete));
    }

    // failure after remote queries completed
    {
        assert(false == details->embedded_complete);

        // reissue the DDL query against the embedded database
        bool embedded_bad_query;
        RETURN_FALSE_IF_FALSE(retryQuery(e_conn, details->query,
                                         &embedded_bad_query));
        assert(false == remote_bad_query.assigned()
               || embedded_bad_query == remote_bad_query.get());

        if (true == embedded_bad_query) {
            return abortQuery(e_conn, unfinished_id);
        }

        return finishQuery(e_conn, unfinished_id);
    }
}

static bool
deltaSanityCheck(const std::unique_ptr<Connect> &conn,
                 const std::unique_ptr<Connect> &e_conn)
{
    const std::string embedded_completion =
        MetaData::Table::embeddedQueryCompletion();
    std::unique_ptr<DBResult> dbres;
    const std::string unfinished_deltas =
        " SELECT id, type FROM " + embedded_completion +
        "  WHERE (begin = FALSE OR complete = FALSE)"
        "    AND aborted != TRUE;";
    RETURN_FALSE_IF_FALSE(e_conn->execute(unfinished_deltas, &dbres));
    const unsigned long long unfinished_count = mysql_num_rows(dbres->n);
    std::cerr << GREEN_BEGIN << "there are " << unfinished_count
              << " unfinished deltas" << COLOR_END << std::endl;

    if (0 == unfinished_count) {
        return true;
    } else if (1 < unfinished_count) {
        return false;
    }

    const MYSQL_ROW row = mysql_fetch_row(dbres->n);
    const unsigned long *const l = mysql_fetch_lengths(dbres->n);
    const std::string string_unfinished_id(row[0], l[0]);
    const std::string string_unfinished_type(row[1], l[1]);

    const unsigned long unfinished_id =
        atoi(string_unfinished_id.c_str());
    const CompletionType type =
        TypeText<CompletionType>::toType(string_unfinished_type);

    switch (type) {
        case CompletionType::AdjustOnionCompletion:
            return fixAdjustOnion(conn, e_conn, unfinished_id);
        case CompletionType::DDLCompletion:
            return fixDDL(conn, e_conn, unfinished_id);
        default:
            std::cerr << "unknown completion type" << std::endl;
            return false;
    }
}

// This function will not build all of our tables when it is run
// on an empty database.  If you don't have a parent, your table won't be
// built.  We probably want to seperate our database logic into 3 parts.
//  1> Schema buildling (CREATE TABLE IF NOT EXISTS...)
//  2> INSERTing
//  3> SELECTing
SchemaInfo *
loadSchemaInfo(const std::unique_ptr<Connect> &conn,
               const std::unique_ptr<Connect> &e_conn)
{
    // Must be done before loading the children.
    assert(deltaSanityCheck(conn, e_conn));

    SchemaInfo *const schema = new SchemaInfo();
    // Recursively rebuild the AbstractMeta<Whatever> and it's children.
    std::function<DBMeta *(DBMeta *const)> loadChildren =
        [&loadChildren, &e_conn](DBMeta *const parent) {
            auto kids = parent->fetchChildren(e_conn);
            for (auto it : kids) {
                loadChildren(it);
            }

            return parent;  /* lambda */
        };

    loadChildren(schema);
    // FIXME: Ideally we would do this before loading the schema.
    // But first we must decide on a place to create the database from.
    assert(sanityCheck(*schema));

    return schema;
}

template <typename Type> static void
translatorHelper(std::vector<std::string> texts,
                 std::vector<Type> enums)
{
    TypeText<Type>::addSet(enums, texts);
}

static bool
buildTypeTextTranslator()
{
    // Onions.
    const std::vector<std::string> onion_strings
    {
        "oINVALID", "oPLAIN", "oDET", "oOPE", "oAGG", "oSWP"
    };
    const std::vector<onion> onions
    {
        oINVALID, oPLAIN, oDET, oOPE, oAGG, oSWP
    };
    RETURN_FALSE_IF_FALSE(onion_strings.size() == onions.size());
    translatorHelper<onion>(onion_strings, onions);

    // SecLevels.
    const std::vector<std::string> seclevel_strings
    {
        "RND", "DET", "DETJOIN", "OPE", "HOM", "SEARCH", "PLAINVAL",
        "INVALID"
    };
    const std::vector<SECLEVEL> seclevels
    {
        SECLEVEL::RND, SECLEVEL::DET, SECLEVEL::DETJOIN, SECLEVEL::OPE,
        SECLEVEL::HOM, SECLEVEL::SEARCH, SECLEVEL::PLAINVAL,
        SECLEVEL::INVALID
    };
    RETURN_FALSE_IF_FALSE(seclevel_strings.size() == seclevels.size());
    translatorHelper(seclevel_strings, seclevels);

    // MYSQL types.
    const std::vector<std::string> mysql_type_strings
    {
        "MYSQL_TYPE_DECIMAL", "MYSQL_TYPE_TINY", "MYSQL_TYPE_SHORT",
        "MYSQL_TYPE_LONG", "MYSQL_TYPE_FLOAT", "MYSQL_TYPE_DOUBLE",
        "MYSQL_TYPE_NULL", "MYSQL_TYPE_TIMESTAMP", "MYSQL_TYPE_LONGLONG",
        "MYSQL_TYPE_INT24", "MYSQL_TYPE_DATE", "MYSQL_TYPE_TIME",
        "MYSQL_TYPE_DATETIME", "MYSQL_TYPE_YEAR", "MYSQL_TYPE_NEWDATE",
        "MYSQL_TYPE_VARCHAR", "MYSQL_TYPE_BIT", "MYSQL_TYPE_NEWDECIMAL",
        "MYSQL_TYPE_ENUM", "MYSQL_TYPE_SET",
        "MYSQL_TYPE_TINY_BLOB", "MYSQL_TYPE_MEDIUM_BLOB",
        "MYSQL_TYPE_LONG_BLOB", "MYSQL_TYPE_BLOB",
        "MYSQL_TYPE_VAR_STRING", "MYSQL_TYPE_STRING",
        "MYSQL_TYPE_GEOMETRY"
    };
    const std::vector<enum enum_field_types> mysql_types
    {
        MYSQL_TYPE_DECIMAL, MYSQL_TYPE_TINY, MYSQL_TYPE_SHORT,
        MYSQL_TYPE_LONG, MYSQL_TYPE_FLOAT, MYSQL_TYPE_DOUBLE,
        MYSQL_TYPE_NULL, MYSQL_TYPE_TIMESTAMP, MYSQL_TYPE_LONGLONG,
        MYSQL_TYPE_INT24, MYSQL_TYPE_DATE, MYSQL_TYPE_TIME,
        MYSQL_TYPE_DATETIME, MYSQL_TYPE_YEAR, MYSQL_TYPE_NEWDATE,
        MYSQL_TYPE_VARCHAR, MYSQL_TYPE_BIT,
        MYSQL_TYPE_NEWDECIMAL /* 246 */, MYSQL_TYPE_ENUM /* 247 */,
        MYSQL_TYPE_SET /* 248 */, MYSQL_TYPE_TINY_BLOB /* 249 */,
        MYSQL_TYPE_MEDIUM_BLOB /* 250 */,
        MYSQL_TYPE_LONG_BLOB /* 251 */, MYSQL_TYPE_BLOB /* 252 */,
        MYSQL_TYPE_VAR_STRING /* 253 */, MYSQL_TYPE_STRING /* 254 */,
        MYSQL_TYPE_GEOMETRY /* 255 */
    };
    RETURN_FALSE_IF_FALSE(mysql_type_strings.size() ==
                            mysql_types.size());
    translatorHelper(mysql_type_strings, mysql_types);

    // MYSQL item types.
    const std::vector<std::string> mysql_item_strings
    {
        "FIELD_ITEM", "FUNC_ITEM", "SUM_FUNC_ITEM", "STRING_ITEM",
        "INT_ITEM", "REAL_ITEM", "NULL_ITEM", "VARBIN_ITEM",
        "COPY_STR_ITEM", "FIELD_AVG_ITEM", "DEFAULT_VALUE_ITEM",
        "PROC_ITEM", "COND_ITEM", "REF_ITEM", "FIELD_STD_ITEM",
        "FIELD_VARIANCE_ITEM", "INSERT_VALUE_ITEM",
        "SUBSELECT_ITEM", "ROW_ITEM", "CACHE_ITEM", "TYPE_HOLDER",
        "PARAM_ITEM", "TRIGGER_FIELD_ITEM", "DECIMAL_ITEM",
        "XPATH_NODESET", "XPATH_NODESET_CMP", "VIEW_FIXER_ITEM"
    };
    const std::vector<enum Item::Type> mysql_item_types
    {
        Item::Type::FIELD_ITEM, Item::Type::FUNC_ITEM,
        Item::Type::SUM_FUNC_ITEM, Item::Type::STRING_ITEM,
        Item::Type::INT_ITEM, Item::Type::REAL_ITEM,
        Item::Type::NULL_ITEM, Item::Type::VARBIN_ITEM,
        Item::Type::COPY_STR_ITEM, Item::Type::FIELD_AVG_ITEM,
        Item::Type::DEFAULT_VALUE_ITEM, Item::Type::PROC_ITEM,
        Item::Type::COND_ITEM, Item::Type::REF_ITEM,
        Item::Type::FIELD_STD_ITEM, Item::Type::FIELD_VARIANCE_ITEM,
        Item::Type::INSERT_VALUE_ITEM, Item::Type::SUBSELECT_ITEM,
        Item::Type::ROW_ITEM, Item::Type::CACHE_ITEM,
        Item::Type::TYPE_HOLDER, Item::Type::PARAM_ITEM,
        Item::Type::TRIGGER_FIELD_ITEM, Item::Type::DECIMAL_ITEM,
        Item::Type::XPATH_NODESET, Item::Type::XPATH_NODESET_CMP,
        Item::Type::VIEW_FIXER_ITEM
    };
    RETURN_FALSE_IF_FALSE(mysql_item_strings.size() ==
                            mysql_item_types.size());
    translatorHelper(mysql_item_strings, mysql_item_types);

    // ALTER TABLE [table] DISABLE/ENABLE KEYS
    const std::vector<std::string> disable_enable_keys_strings
    {
        "DISABLE", "ENABLE", "LEAVE_AS_IS"
    };
    const std::vector<enum enum_enable_or_disable>
        disable_enable_keys_types
    {
        DISABLE, ENABLE, LEAVE_AS_IS
    };
    RETURN_FALSE_IF_FALSE(disable_enable_keys_strings.size() ==
                            disable_enable_keys_types.size());
    translatorHelper(disable_enable_keys_strings,
                     disable_enable_keys_types);

    // Onion Layouts.
    const std::vector<std::string> onion_layout_strings
    {
        "PLAIN_ONION_LAYOUT", "NUM_ONION_LAYOUT",
        "BEST_EFFORT_NUM_ONION_LAYOUT", "STR_ONION_LAYOUT",
        "BEST_EFFORT_STR_ONION_LAYOUT"
        
    };
    const std::vector<onionlayout> onion_layouts
    {
        PLAIN_ONION_LAYOUT, NUM_ONION_LAYOUT,
        BEST_EFFORT_NUM_ONION_LAYOUT, STR_ONION_LAYOUT,
        BEST_EFFORT_STR_ONION_LAYOUT
    };
    RETURN_FALSE_IF_FALSE(onion_layout_strings.size() ==
                            onion_layouts.size());
    translatorHelper(onion_layout_strings, onion_layouts);

    // Geometry type.
    const std::vector<std::string> geometry_type_strings
    {
        "GEOM_GEOMETRY", "GEOM_POINT", "GEOM_LINESTRING", "GEOM_POLYGON",
        "GEOM_MULTIPOINT", "GEOM_MULTILINESTRING", "GEOM_MULTIPOLYGON",
        "GEOM_GEOMETRYCOLLECTION"
    };
    std::vector<Field::geometry_type> geometry_types
    {
        Field::GEOM_GEOMETRY, Field::GEOM_POINT, Field::GEOM_LINESTRING,
        Field::GEOM_POLYGON, Field::GEOM_MULTIPOINT,
        Field::GEOM_MULTILINESTRING, Field::GEOM_MULTIPOLYGON,
        Field::GEOM_GEOMETRYCOLLECTION
    };
    RETURN_FALSE_IF_FALSE(geometry_type_strings.size() ==
                            geometry_types.size());
    translatorHelper(geometry_type_strings, geometry_types);

    // Security Rating.
    const std::vector<std::string> security_rating_strings
    {
        "SENSITIVE", "BEST_EFFORT", "PLAIN"
    };
    const std::vector<SECURITY_RATING> security_rating_types
    {
        SECURITY_RATING::SENSITIVE, SECURITY_RATING::BEST_EFFORT,
        SECURITY_RATING::PLAIN
    };
    RETURN_FALSE_IF_FALSE(security_rating_strings.size()
                            == security_rating_types.size());
    translatorHelper(security_rating_strings, security_rating_types);

    // Query Completions.
    const std::vector<std::string> completion_strings
    {
        "DDLCompletion", "AdjustOnionCompletion"
    };
    const std::vector<CompletionType> completion_types
    {
        CompletionType::DDLCompletion,
        CompletionType::AdjustOnionCompletion
    };
    RETURN_FALSE_IF_FALSE(completion_strings.size()
                            == completion_types.size());
    translatorHelper(completion_strings, completion_types);

    return true;
}

// Allows us to preserve boolean return values from
// buildTypeTextTranslator, handle it as a static constant in
// Rewriter and panic when it fails.
static bool
buildTypeTextTranslatorHack()
{
    assert(buildTypeTextTranslator());

    return true;
}

//l gets updated to the new level
static std::string
removeOnionLayer(const Analysis &a, const TableMeta &tm,
                 const FieldMeta &fm,
                 OnionMetaAdjustor *const om_adjustor,
                 SECLEVEL *const new_level,
                 std::vector<std::unique_ptr<Delta> > *const deltas)
{
    // Remove the EncLayer.
    EncLayer const &back_el = om_adjustor->popBackEncLayer();

    // Update the Meta.
    deltas->push_back(std::unique_ptr<Delta>(
                        new DeleteDelta(back_el,
                                        om_adjustor->getOnionMeta())));
    const SECLEVEL local_new_level = om_adjustor->getSecLevel();

    //removes onion layer at the DB
    const std::string dbname = a.getDatabaseName();
    const std::string anon_table_name = tm.getAnonTableName();
    Item_field *const salt =
        new Item_field(NULL, dbname.c_str(), anon_table_name.c_str(),
                       fm.getSaltName().c_str());

    const std::string fieldanon = om_adjustor->getAnonOnionName();
    Item_field *const field =
        new Item_field(NULL, dbname.c_str(), anon_table_name.c_str(),
                       fieldanon.c_str());

    Item *const decUDF = back_el.decryptUDF(field, salt);

    std::stringstream query;
    query << " UPDATE " << dbname << "." << anon_table_name
          << "    SET " << fieldanon  << " = " << *decUDF
          << ";";

    std::cerr << "\nADJUST: \n" << query.str() << std::endl;

    //execute decryption query

    LOG(cdb_v) << "adjust onions: \n" << query.str() << std::endl;

    *new_level = local_new_level;
    return query.str();
}

/*
 * Adjusts the onion for a field fm/itf to level: tolevel.
 *
 * Issues queries for decryption to the DBMS.
 *
 * Adjusts the schema metadata at the proxy about onion layers. Propagates the
 * changed schema to persistent storage.
 *
 */
static std::pair<std::vector<std::unique_ptr<Delta> >,
                 std::list<std::string>>
adjustOnion(const Analysis &a, onion o, const TableMeta &tm,
            const FieldMeta &fm, SECLEVEL tolevel)
{
    std::cout << "onion: " << TypeText<onion>::toText(o) << std::endl;
    // Make a copy of the onion meta for the purpose of making
    // modifications during removeOnionLayer(...)
    OnionMetaAdjustor om_adjustor(*fm.getOnionMeta(o));
    SECLEVEL newlevel = om_adjustor.getSecLevel();
    assert(newlevel != SECLEVEL::INVALID);

    std::list<std::string> adjust_queries;
    std::vector<std::unique_ptr<Delta> > deltas;
    while (newlevel > tolevel) {
        auto query =
            removeOnionLayer(a, tm, fm, &om_adjustor, &newlevel,
                             &deltas);
        adjust_queries.push_back(query);
    }
    TEST_UnexpectedSecurityLevel(o, tolevel, newlevel);

    return make_pair(std::move(deltas), adjust_queries);
}
//TODO: propagate these adjustments in the embedded database?

static inline bool
FieldQualifies(const FieldMeta *const restriction,
               const FieldMeta *const field)
{
    return !restriction || restriction == field;
}

template <class T>
static Item *
do_optimize_const_item(T *i, Analysis &a) {

    return i;

    /* TODO for later
    if (i->const_item()) {
        // ask embedded DB to eval this const item,
        // then replace this item with the eval-ed constant
        //
        // WARNING: we must make sure that the primitives like
        // int literals, string literals, override this method
        // and not ask the server.

        // very hacky...
        stringstream buf;
        buf << "SELECT " << *i;
        string q(buf.str());
        LOG(cdb_v) << q;

	DBResult * dbres = NULL;
	assert(a.ps->e_conn->execute(q, dbres));

        THD *thd = current_thd;
        assert(thd != NULL);

        MYSQL_RES *r = dbres->n;
        if (r) {
            Item *rep = NULL;

            assert(mysql_num_rows(r) == 1);
            assert(mysql_num_fields(r) == 1);

            MYSQL_FIELD *field = mysql_fetch_field_direct(r, 0);
            assert(field != NULL);

            MYSQL_ROW row = mysql_fetch_row(r);
            assert(row != NULL);

            char *p = row[0];
            unsigned long *lengths = mysql_fetch_lengths(r);
            assert(lengths != NULL);
            if (p) {

                LOG(cdb_v) << "p: " << p;
                LOG(cdb_v) << "field->type: " << field->type;

                switch (field->type) {
                    case MYSQL_TYPE_SHORT:
                    case MYSQL_TYPE_LONG:
                    case MYSQL_TYPE_LONGLONG:
                    case MYSQL_TYPE_INT24:
                        rep = new Item_int((long long) strtoll(p, NULL, 10));
                        break;
                    case MYSQL_TYPE_FLOAT:
                    case MYSQL_TYPE_DOUBLE:
                        rep = new Item_float(p, lengths[0]);
                        break;
                    case MYSQL_TYPE_DECIMAL:
                    case MYSQL_TYPE_NEWDECIMAL:
                        rep = new Item_decimal(p, lengths[0], i->default_charset());
                        break;
                    case MYSQL_TYPE_VARCHAR:
                    case MYSQL_TYPE_VAR_STRING:
                        rep = new Item_string(thd->strdup(p),
                                              lengths[0],
                                              i->default_charset());
                        break;
                    default:
                        // TODO(stephentu): implement the rest of the data types
                        break;
                }
            } else {
                // this represents NULL
                rep = new Item_null();
            }
            mysql_free_result(r);
            if (rep != NULL) {
                rep->name = i->name;
                return rep;
            }
        } else {
            // some error in dealing with the DB
            LOG(warn) << "could not retrieve result set";
        }
    }
    return i;

    */
}

Item *
decrypt_item_layers(Item *const i, const FieldMeta *const fm, onion o,
                    uint64_t IV)
{
    assert(!i->is_null());

    Item *dec = i;

    const OnionMeta *const om = fm->getOnionMeta(o);
    assert(om);
    const auto &enc_layers = om->layers;
    for (auto it = enc_layers.rbegin(); it != enc_layers.rend(); ++it) {
        dec = (*it)->decrypt(dec, IV);
        LOG(cdb_v) << "dec okay";
    }

    return dec;
}


// returns the intersection of the es and fm.encdesc
// by also taking into account what onions are stale
// on fm
/*static OnionLevelFieldMap
intersect(const EncSet & es, FieldMeta * fm) {
    OnionLevelFieldMap res;

    for (auto it : es.osl) {
        onion o = it.first;
        auto ed_it = fm->encdesc.olm.find(o);
        if ((ed_it != fm->encdesc.olm.end()) && (!fm->onions[o]->stale)) {
            //an onion to keep
            res[o] = LevelFieldPair(min(it.second.first, ed_it->second), fm);
        }
    }

    return res;
}
*/
/*
 * Actual item handlers.
 */
static void optimize_select_lex(st_select_lex *select_lex, Analysis & a);

static Item *getLeftExpr(const Item_in_subselect &i)
{
    Item *const left_expr =
        i.*rob<Item_in_subselect, Item*,
                &Item_in_subselect::left_expr>::ptr();
    assert(left_expr);

    return left_expr;

}

// HACK: Forces query down to PLAINVAL.
static class ANON : public CItemSubtypeIT<Item_subselect,
                                          Item::Type::SUBSELECT_ITEM> {
    virtual RewritePlan *
    do_gather_type(const Item_subselect &i, Analysis &a) const
    {
        const std::string why = "subselect";

        // Gather subquery.
        std::unique_ptr<Analysis>
            subquery_analysis(new Analysis(a.getDatabaseName(),
                                           a.getSchema()));
        const st_select_lex *const select_lex =
            RiboldMYSQL::get_select_lex(i);
        process_select_lex(*select_lex, *subquery_analysis);

        // HACK: Forces the subquery to use PLAINVAL for it's
        // projections.
        auto item_it =
            RiboldMYSQL::constList_iterator<Item>(select_lex->item_list);
        for (;;) {
            const Item *const item = item_it++;
            if (!item) {
                break;
            }

            const std::unique_ptr<RewritePlan> &item_rp =
                subquery_analysis->rewritePlans[item];
            TEST_NoAvailableEncSet(item_rp->es_out, i.type(),
                                   PLAIN_EncSet, why,
                            std::vector<std::shared_ptr<RewritePlan> >());
            item_rp->es_out = PLAIN_EncSet;
        }

        const EncSet out_es = PLAIN_EncSet;
        const reason rsn = reason(out_es, why, i);

        switch (RiboldMYSQL::substype(i)) {
            case Item_subselect::subs_type::SINGLEROW_SUBS:
                break;
            case Item_subselect::subs_type::EXISTS_SUBS:
                assert(false);
            case Item_subselect::subs_type::IN_SUBS: {
                const Item *const left_expr =
                    getLeftExpr(static_cast<const Item_in_subselect &>(i));
                RewritePlan *const rp_left_expr =
                    gather(*left_expr, *subquery_analysis.get());
                a.rewritePlans[left_expr] =
                    std::unique_ptr<RewritePlan>(rp_left_expr);
                break;
            }
            case Item_subselect::subs_type::ALL_SUBS:
                assert(false);
            case Item_subselect::subs_type::ANY_SUBS:
                assert(false);
            default:
                FAIL_TextMessageError("Unknown subquery type!");
        }

        return new RewritePlanWithAnalysis(out_es, rsn,
                                           std::move(subquery_analysis));
    }

    virtual Item * do_optimize_type(Item_subselect *i, Analysis & a) const {
        optimize_select_lex(i->get_select_lex(), a);
        return i;
    }

    virtual Item *
    do_rewrite_type(const Item_subselect &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        const RewritePlanWithAnalysis &rp_w_analysis =
            static_cast<const RewritePlanWithAnalysis &>(rp);
        const st_select_lex *const select_lex =
            RiboldMYSQL::get_select_lex(i);

        // ------------------------------
        //    General Subquery Rewrite
        // ------------------------------
        st_select_lex *const new_select_lex =
            rewrite_select_lex(*select_lex, *rp_w_analysis.a.get());

        // Rewrite table names.
        new_select_lex->top_join_list =
            rewrite_table_list(select_lex->top_join_list,
                               *rp_w_analysis.a.get());

        // Rewrite SELECT params.
        // HACK: The engine inside of the Item_subselect _can_ have a
        // pointer back to the Item_subselect that contains it.
        // > ie, subselect_single_select_engine::join::select_lex
        // > The way this is done varies from engine to engine thus a
        //   general solution seems difficuly.
        // > set_select_lex() attemps to rectify this problem in other
        //   cases
        memcpy(const_cast<st_select_lex *>(select_lex), new_select_lex,
               sizeof(st_select_lex));

        // ------------------------------
        //   Specific Subquery Rewrite
        // ------------------------------
        {
            switch (RiboldMYSQL::substype(i)) {
                case Item_subselect::subs_type::SINGLEROW_SUBS:
                    return new Item_singlerow_subselect(new_select_lex);
                case Item_subselect::subs_type::EXISTS_SUBS:
                    assert(false);
                case Item_subselect::subs_type::IN_SUBS: {
                    const Item *const left_expr =
                        getLeftExpr(static_cast<const Item_in_subselect &>(i));
                    const std::unique_ptr<RewritePlan> &rp_left_expr =
                        constGetAssert(a.rewritePlans, left_expr);
                    Item *const new_left_expr =
                        itemTypes.do_rewrite(*left_expr, constr,
                                             *rp_left_expr.get(), a);
                    return new Item_in_subselect(new_left_expr,
                                                 new_select_lex);
                }
                case Item_subselect::subs_type::ALL_SUBS:
                    assert(false);
                case Item_subselect::subs_type::ANY_SUBS:
                    assert(false);
                default:
                    FAIL_TextMessageError("Unknown subquery type!");
            }
        }
    }
} ANON;

// NOTE: Shouldn't be needed unless we allow mysql to rewrite subqueries.
static class ANON : public CItemSubtypeIT<Item_cache, Item::Type::CACHE_ITEM> {
    virtual RewritePlan *do_gather_type(const Item_cache &i,
                                        Analysis &a) const
    {
        UNIMPLEMENTED;
        return NULL;

        /*
        TEST_TextMessageError(false ==
                                i->field()->orig_table->alias_name_used,
                              "Can not mix CACHE_ITEM and table alias.");
        const std::string table_name =
            std::string(i->field()->orig_table->alias);
        const std::string field_name =
            std::string(i->field()->field_name);
        OnionMeta *const om =
            a.getOnionMeta(table_name, field_name, oPLAIN);
        if (a.getOnionLevel(om) != SECLEVEL::PLAINVAL) {
            const FieldMeta *const fm =
                a.getFieldMeta(table_name, field_name);

            throw OnionAdjustExcept(oPLAIN, fm, SECLEVEL::PLAINVAL,
                                    table_name);
        }

        const EncSet out_es = PLAIN_EncSet;
        tr = reason(out_es, "is cache item", i);

        return new RewritePlan(out_es, tr);
        */

        /*
        Item *example = i->*rob<Item_cache, Item*, &Item_cache::example>::ptr();
        if (example)
            return gather(example, tr, a);
        return tr.encset;
        UNIMPLEMENTED;
        return NULL;
        */
    }

    virtual Item * do_optimize_type(Item_cache *i, Analysis & a) const
    {
        // TODO(stephentu): figure out how to use rob here
        return i;
    }

    virtual Item *do_rewrite_type(const Item_cache &i, const OLK &constr,
                                  const RewritePlan &rp, Analysis &a)
        const
    {
        UNIMPLEMENTED;
        return NULL;
    }
} ANON;

/*
 * Some helper functions.
 */

static void
optimize_select_lex(st_select_lex *select_lex, Analysis & a)
{
    auto item_it = List_iterator<Item>(select_lex->item_list);
    for (;;) {
        if (!item_it++)
            break;
        optimize(item_it.ref(), a);
    }

    if (select_lex->where)
        optimize(&select_lex->where, a);

    if (select_lex->join &&
        select_lex->join->conds &&
        select_lex->where != select_lex->join->conds)
        optimize(&select_lex->join->conds, a);

    if (select_lex->having)
        optimize(&select_lex->having, a);

    for (ORDER *o = select_lex->group_list.first; o; o = o->next)
        optimize(o->item, a);
    for (ORDER *o = select_lex->order_list.first; o; o = o->next)
        optimize(o->item, a);
}

static void
optimize_table_list(List<TABLE_LIST> *tll, Analysis &a)
{
    List_iterator<TABLE_LIST> join_it(*tll);
    for (;;) {
        TABLE_LIST *t = join_it++;
        if (!t)
            break;

        if (t->nested_join) {
            optimize_table_list(&t->nested_join->join_list, a);
            return;
        }

        if (t->on_expr)
            optimize(&t->on_expr, a);

        if (t->derived) {
            st_select_lex_unit *u = t->derived;
            optimize_select_lex(u->first_select(), a);
        }
    }
}

static bool
noRewrite(const LEX &lex) {
    switch (lex.sql_command) {
    case SQLCOM_SHOW_DATABASES:
    case SQLCOM_SET_OPTION:
    case SQLCOM_BEGIN:
    case SQLCOM_ROLLBACK:
    case SQLCOM_COMMIT:
    case SQLCOM_SHOW_TABLES:
    case SQLCOM_SHOW_VARIABLES:
    case SQLCOM_UNLOCK_TABLES:
        return true;
    case SQLCOM_SELECT: {

    }
    default:
        return false;
    }

    return false;
}

static std::string
lex_to_query(LEX *const lex)
{
    std::ostringstream o;
    o << *lex;
    return o.str();
}

const bool Rewriter::translator_dummy = buildTypeTextTranslatorHack();
const std::unique_ptr<SQLDispatcher> Rewriter::dml_dispatcher =
    std::unique_ptr<SQLDispatcher>(buildDMLDispatcher());
const std::unique_ptr<SQLDispatcher> Rewriter::ddl_dispatcher =
    std::unique_ptr<SQLDispatcher>(buildDDLDispatcher());

// NOTE : This will probably choke on multidatabase queries.
RewriteOutput *
Rewriter::dispatchOnLex(Analysis &a, const ProxyState &ps,
                        const std::string &query)
{
    std::unique_ptr<query_parse> p;
    try {
        p = std::unique_ptr<query_parse>(
                new query_parse(a.getDatabaseName(), query));
    } catch (std::runtime_error &e) {
        FAIL_TextMessageError("Bad Query: [" + query + "]\t"
                              "Error Data: " + e.what());
    }
    LEX *const lex = p->lex();

    LOG(cdb_v) << "pre-analyze " << *lex;

    // optimization: do not process queries that we will not rewrite
    if (noRewrite(*lex)) {
        return new SimpleOutput(query);
    } else if (dml_dispatcher->canDo(lex)) {
        const SQLHandler &handler = dml_dispatcher->dispatch(lex);
        AssignOnce<LEX *> out_lex;

        try {
            out_lex = handler.transformLex(a, lex, ps);
        } catch (OnionAdjustExcept e) {
            LOG(cdb_v) << "caught onion adjustment";
            std::cout << "Adjusting onion!" << std::endl;
            std::pair<std::vector<std::unique_ptr<Delta> >,
                      std::list<std::string>>
                out_data = adjustOnion(a, e.o, e.tm, e.fm, e.tolevel);
            std::vector<std::unique_ptr<Delta> > &deltas =
                out_data.first;
            const std::list<std::string>  &adjust_queries =
                out_data.second;
            std::function<std::string(const std::string &)>
                hackEscape = [&ps](const std::string &s)
            {
                return escapeString(ps.getConn(), s);
            };
            return new AdjustOnionOutput(query, std::move(deltas),
                                         adjust_queries, hackEscape);
        }

        // Return if it's a regular DML query.
        if (false == a.special_update) {
            return new DMLOutput(query, lex_to_query(out_lex.get()));
        }

        // Handle HOMorphic UPDATE.
        const auto plain_table =
            lex->select_lex.top_join_list.head()->table_name;
        const auto crypted_table =
            out_lex.get()->select_lex.top_join_list.head()->table_name;
        std::string where_clause;
        if (lex->select_lex.where) {
            std::ostringstream where_stream;
            where_stream << " " << *lex->select_lex.where << " ";
            where_clause = where_stream.str();
        } else {
            where_clause = " TRUE ";
        }

        return new SpecialUpdate(query,  plain_table, crypted_table,
                                 where_clause, a.getDatabaseName(), ps);
    } else if (ddl_dispatcher->canDo(lex)) {
        const SQLHandler &handler = ddl_dispatcher->dispatch(lex);
        LEX *const out_lex = handler.transformLex(a, lex, ps);
        // HACK.
        const std::string &original_query =
            lex->sql_command != SQLCOM_LOCK_TABLES ? query : "do 0";

        return new DDLOutput(original_query, lex_to_query(out_lex),
                             std::move(a.deltas));
    } else {
        return NULL;
    }
}

struct DirectiveData {
    std::string table_name;
    std::string field_name;
    SECURITY_RATING sec_rating;

    DirectiveData(const std::string query)
    {
        std::list<std::string> tokens = split(query, " ");
        assert(tokens.size() == 4);
        tokens.pop_front();

        table_name = tokens.front();
        tokens.pop_front();

        field_name = tokens.front();
        tokens.pop_front();

        sec_rating = TypeText<SECURITY_RATING>::toType(tokens.front());
        tokens.pop_front();
    }
};

// FIXME: Implement.
// SYNTAX
// > DIRECTIVE UPDATE cryptdb_metadata
//                SET <table_name | field_name | rating> = [value]
// > DIRECTIVE SELECT <table_name | field_name | rating>
//               FROM cryptdb_metadata
//              WHERE <table_name | field_name | rating> = [value]
RewriteOutput *
Rewriter::handleDirective(Analysis &a, const ProxyState &ps,
                          const std::string &query)
{
    DirectiveData data(query);
    const FieldMeta &fm =
        a.getFieldMeta(a.getDatabaseName(), data.table_name,
                       data.field_name);
    const SECURITY_RATING current_rating = fm.getSecurityRating();
    if (current_rating < data.sec_rating) {
        FAIL_TextMessageError("cryptdb does not support going to a more"
                              " secure rating!");
    } else if (current_rating == data.sec_rating) {
        return new SimpleOutput(mysql_noop());
    } else {
        // Actually do things.
        FAIL_TextMessageError("implement handleDirective!");
    }
}

static
bool
cryptdbDirective(const std::string &query)
{
    std::size_t found = query.find("DIRECTIVE");
    if (std::string::npos == found) {
        return false;
    }
    
    for (std::size_t i = 0; i < found; ++i) {
        if (!std::isspace(query[i])) {
            return false;
        }
    }

    return true;
}

QueryRewrite
Rewriter::rewrite(const ProxyState &ps, const std::string &q,
                  SchemaInfo const &schema,
                  const std::string &default_db)
{
    LOG(cdb_v) << "q " << q;
    assert(0 == mysql_thread_init());
    //assert(0 == create_embedded_thd(0));

    Analysis analysis(default_db, schema);

    RewriteOutput *output;
    if (cryptdbDirective(q)) {
        output = Rewriter::handleDirective(analysis, ps, q);
    } else {
        // NOTE: Care what data you try to read from Analysis
        // at this height.
        output = Rewriter::dispatchOnLex(analysis, ps, q);
        if (!output) {
            output = new SimpleOutput(mysql_noop());
        }
    }

    return QueryRewrite(true, analysis.rmeta, output);
}

//TODO: replace stringify with <<
std::string ReturnField::stringify() {
    std::stringstream res;

    res << " is_salt: " << is_salt << " filed_called " << field_called;
    res << " fm  " << olk.key << " onion " << olk.o;
    res << " salt_pos " << salt_pos;

    return res.str();
}
std::string ReturnMeta::stringify() {
    std::stringstream res;
    res << "rmeta contains " << rfmeta.size() << " elements: \n";
    for (auto it : rfmeta) {
        res << it.first << " " << it.second.stringify() << "\n";
    }
    return res.str();
}

ResType
Rewriter::decryptResults(const ResType &dbres, const ReturnMeta &rmeta)
{
    const unsigned int rows = dbres.rows.size();
    LOG(cdb_v) << "rows in result " << rows << "\n";
    const unsigned int cols = dbres.names.size();

    ResType res;

    // un-anonymize the names
    for (auto it = dbres.names.begin();
        it != dbres.names.end(); it++) {
        const unsigned int index = it - dbres.names.begin();
        const ReturnField &rf = rmeta.rfmeta.at(index);
        if (!rf.getIsSalt()) {
            //need to return this field
            res.names.push_back(rf.fieldCalled());
            // switch types to original ones : TODO

        }
    }

    const unsigned int real_cols = res.names.size();

    //allocate space in results for decrypted rows
    res.rows = std::vector<std::vector<std::shared_ptr<Item> > >(rows);
    for (unsigned int i = 0; i < rows; i++) {
        res.rows[i] = std::vector<std::shared_ptr<Item> >(real_cols);
    }

    // decrypt rows
    unsigned int col_index = 0;
    for (unsigned int c = 0; c < cols; c++) {
        const ReturnField &rf = rmeta.rfmeta.at(c);
        if (rf.getIsSalt()) {
            continue;
        }

        FieldMeta *const fm = rf.getOLK().key;
        for (unsigned int r = 0; r < rows; r++) {
            if (!fm || dbres.rows[r][c]->is_null()) {
                res.rows[r][col_index] = dbres.rows[r][c];
            } else {
                uint64_t salt = 0;
                const int salt_pos = rf.getSaltPosition();
                if (salt_pos >= 0) {
                    Item_int *const salt_item =
                        static_cast<Item_int *>(dbres.rows[r][salt_pos].get());
                    assert_s(!salt_item->null_value, "salt item is null");
                    salt = salt_item->value;
                }

                std::shared_ptr<Item> dec_item(
                    decrypt_item_layers(dbres.rows[r][c].get(),
                                        fm, rf.getOLK().o, salt));
                res.rows[r][col_index] = dec_item;
            }
        }
        col_index++;
    }

    return res;
}

static ResType
mysql_noop_res(const ProxyState &ps)
{
    std::unique_ptr<DBResult> noop_dbres;
    assert(ps.getConn()->execute(mysql_noop(), &noop_dbres));
    return ResType(noop_dbres->unpack());
}

EpilogueResult
executeQuery(const ProxyState &ps, const std::string &q,
             const std::string &default_db,
             SchemaCache *const schema_cache, bool pp)
{
    assert(schema_cache);

    std::unique_ptr<QueryRewrite> qr;
    // out_queryz: queries intended to be run against remote server.
    std::list<std::string> out_queryz;
    queryPreamble(ps, q, &qr, &out_queryz, schema_cache, default_db);
    assert(qr);

    std::unique_ptr<DBResult> dbres;
    for (auto it : out_queryz) {
        if (true == pp) {
            prettyPrintQuery(it);
        }

        TEST_Sync(ps.getConn()->execute(it, &dbres,
                                    qr->output->multipleResultSets()),
                  "failed to execute query!");
        // XOR: Either we have one result set, or we were expecting
        // multiple result sets and we threw them all away.
        assert(!!dbres != !!qr->output->multipleResultSets());
    }

    // ----------------------------------
    //       Post Query Processing
    // ----------------------------------
    const ResType &res =
        dbres ? ResType(dbres->unpack()) : mysql_noop_res(ps);
    assert(res.success());
    const EpilogueResult epi_result =
        queryEpilogue(ps, *qr.get(), res, q, default_db, pp);
    assert(epi_result.res_type.success());

    return epi_result;
}

void
printRes(const ResType &r) {

    //if (!cryptdb_logger::enabled(log_group::log_edb_v))
    //return;

    std::stringstream ssn;
    for (unsigned int i = 0; i < r.names.size(); i++) {
        char buf[400];
        snprintf(buf, sizeof(buf), "%-25s", r.names[i].c_str());
        ssn << buf;
    }
    std::cerr << ssn.str() << std::endl;
    //LOG(edb_v) << ssn.str();

    /* next, print out the rows */
    for (unsigned int i = 0; i < r.rows.size(); i++) {
        std::stringstream ss;
        for (unsigned int j = 0; j < r.rows[i].size(); j++) {
            char buf[400];
            std::stringstream sstr;
            sstr << *r.rows[i][j];
            snprintf(buf, sizeof(buf), "%-25s", sstr.str().c_str());
            ss << buf;
        }
        std::cerr << ss.str() << std::endl;
        //LOG(edb_v) << ss.str();
    }
}

EncLayer &OnionMetaAdjustor::getBackEncLayer() const
{
    return *duped_layers.back();
}

EncLayer &OnionMetaAdjustor::popBackEncLayer()
{
    EncLayer *const out_layer = duped_layers.back();
    duped_layers.pop_back();

    return *out_layer;
}

SECLEVEL OnionMetaAdjustor::getSecLevel() const
{
    return duped_layers.back()->level();
}

const OnionMeta &OnionMetaAdjustor::getOnionMeta() const
{
    return original_om;
}

std::string OnionMetaAdjustor::getAnonOnionName() const
{
    return original_om.getAnonOnionName();
}

std::vector<EncLayer *>
OnionMetaAdjustor::pullCopyLayers(OnionMeta const &om)
{
    std::vector<EncLayer *> v;

    auto const &enc_layers = om.layers;
    for (auto it = enc_layers.begin(); it != enc_layers.end(); it++) {
        v.push_back((*it).get());
    }

    return v;
}
