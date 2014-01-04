#pragma once

#include <string>

#include <main/rewrite_main.hh>
#include <main/Analysis.hh>
#include <main/rewrite_ds.hh>

#include <sql_list.h>
#include <sql_table.h>

const std::string BOLD_BEGIN = "\033[1m";
const std::string RED_BEGIN = "\033[1;31m";
const std::string GREEN_BEGIN = "\033[1;92m";
const std::string COLOR_END = "\033[0m";

Item *
rewrite(const Item &i, const EncSet &req_enc, Analysis &a);

TABLE_LIST *
rewrite_table_list(const TABLE_LIST * const t, const Analysis &a);

TABLE_LIST *
rewrite_table_list(const TABLE_LIST * const t,
                   const std::string &anon_name);

SQL_I_List<TABLE_LIST>
rewrite_table_list(const SQL_I_List<TABLE_LIST> &tlist, Analysis &a,
                   bool if_exists=false);

List<TABLE_LIST>
rewrite_table_list(List<TABLE_LIST> tll, Analysis &a);

RewritePlan *
gather(const Item &i, Analysis &a);

void
gatherAndAddAnalysisRewritePlan(const Item &i, Analysis &a);

void
optimize(Item ** const i, Analysis &a);

LEX *
begin_transaction_lex(const std::string &dbname);

LEX *
commit_transaction_lex(const std::string &dbname);

std::vector<Create_field *>
rewrite_create_field(const FieldMeta * const fm, Create_field * const f,
                     const Analysis &a);

std::vector<Key *>
rewrite_key(const TableMeta &tm, Key * const key, const Analysis &a);

std::string
bool_to_string(bool b);

bool string_to_bool(const std::string &s);

List<Create_field>
createAndRewriteField(Analysis &a, const ProxyState &ps,
                      Create_field * const cf,
                      TableMeta *const tm, bool new_table,
                      List<Create_field> &rewritten_cfield_list);

Item *
encrypt_item_layers(const Item &i, onion o, const OnionMeta &om,
                    const Analysis &a, uint64_t IV = 0);

std::string
rewriteAndGetSingleQuery(const ProxyState &ps, const std::string &q,
                         SchemaInfo const &schema,
                         const std::string &default_db);

// FIXME(burrows): Generalize to support any container with next AND end
// semantics.
template <typename T>
std::string vector_join(std::vector<T> v, const std::string &delim,
                        const std::function<std::string(T)> &finalize)
{
    std::string accum;
    for (typename std::vector<T>::iterator it = v.begin();
         it != v.end(); ++it) {
        const std::string &element = finalize(static_cast<T>(*it));
        accum.append(element);
        accum.append(delim);
    }

    AssignOnce<std::string> output;
    if (accum.length() > 0) {
        output = accum.substr(0, accum.length() - delim.length());
    } else {
        output = accum;
    }

    return output.get();
}

std::string
escapeString(const std::unique_ptr<Connect> &c,
             const std::string &escape_me);

void
encrypt_item_all_onions(const Item &i, const FieldMeta &fm,
                        uint64_t IV, Analysis &a, std::vector<Item *> *l);

std::vector<onion>
getOnionIndexTypes();

void
typical_rewrite_insert_type(const Item &i, const FieldMeta &fm,
                            Analysis &a, std::vector<Item *> *l);

void
process_select_lex(const st_select_lex &select_lex, Analysis &a);

void
process_table_list(const List<TABLE_LIST> &tll, Analysis &a);

st_select_lex *
rewrite_select_lex(const st_select_lex &select_lex, Analysis &a);

std::string
mysql_noop();

std::string
getDefaultDatabaseForConnection(const std::unique_ptr<Connect> &c);

bool
retrieveDefaultDatabase(unsigned long long thread_id,
                        const std::unique_ptr<Connect> &c,
                        std::string *const out_name);

void
queryPreamble(const ProxyState &ps, const std::string &q,
              std::unique_ptr<QueryRewrite> *qr,
              std::list<std::string> *const out_queryz,
              SchemaCache *const schema,
              const std::string &default_db);

bool
queryHandleRollback(const ProxyState &ps, const std::string &query,
                    SchemaInfo const &schema);

void
prettyPrintQuery(const std::string &query);

class EpilogueResult {
public:
    EpilogueResult(QueryAction action, const ResType &res_type)
        : action(action), res_type(res_type) {}

    const QueryAction action;
    const ResType res_type;
};

EpilogueResult
queryEpilogue(const ProxyState &ps, const QueryRewrite &qr,
              const ResType &res, const std::string &query,
              const std::string &default_db, bool pp);

class SchemaCache {
public:
    SchemaCache() : no_loads(true), id(randomValue() % UINT_MAX) {}

    const SchemaInfo &getSchema(const std::unique_ptr<Connect> &conn,
                                const std::unique_ptr<Connect> &e_conn);
    void updateStaleness(const std::unique_ptr<Connect> &e_conn,
                         bool staleness);
    bool initialStaleness(const std::unique_ptr<Connect> &e_conn);
    bool cleanupStaleness(const std::unique_ptr<Connect> &e_conn);
    void lowLevelCurrentStale(const std::unique_ptr<Connect> &e_conn);
    void lowLevelCurrentUnstale(const std::unique_ptr<Connect> &e_conn);

private:
    std::unique_ptr<const SchemaInfo> schema;
    bool no_loads;
    const unsigned int id;
};

template <typename InType, typename InterimType, typename OutType>
std::function<OutType(InType in)>
fnCompose(std::function<OutType(InterimType)> outer,
          std::function<InterimType(InType)> inner) {

    return [&outer, &inner] (InType in)
    {
        return outer(inner(in));
    };
}

