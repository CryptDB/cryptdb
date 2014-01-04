#include <assert.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

#include <parser/embedmysql.hh>
#include <sql_base.h>
#include <sql_select.h>
#include <sql_delete.h>
#include <sql_insert.h>
#include <sql_update.h>
#include <sql_parse.h>
#include <handler.h>


#include <parser/stringify.hh>

#include <util/errstream.hh>
#include <util/rob.hh>

using namespace std;
static bool embed_active = false;

embedmysql::embedmysql(const std::string &dir)
{
    struct stat st;
    if (stat(dir.c_str(), &st) != 0) {
        fatal() << "ERROR! The proxy_db directory: " << dir << " does not exist";
    }

    if (!__sync_bool_compare_and_swap(&embed_active, false, true))
        fatal() << "only one embedmysql object can exist at once\n";

    char dir_arg[1024];
    snprintf(dir_arg, sizeof(dir_arg), "--datadir=%s", dir.c_str());

    const char *mysql_av[] =
        { "progname",
          "--skip-grant-tables",
          dir_arg,
          "--character-set-server=utf8",
          "--language=" MYSQL_BUILD_DIR "/sql/share/"
        };

    assert(0 == mysql_library_init(sizeof(mysql_av) / sizeof(mysql_av[0]),
                                   (char**) mysql_av, 0));
    m = mysql_init(0);

    mysql_options(m, MYSQL_OPT_USE_EMBEDDED_CONNECTION, 0);
    if (!mysql_real_connect(m, 0, 0, 0, "information_schema", 0, 0, CLIENT_MULTI_STATEMENTS)) {
        mysql_close(m);
        fatal() << "mysql_real_connect: " << mysql_error(m);
    }
}

embedmysql::~embedmysql()
{
    mysql_close(m);
    mysql_server_end();
    assert(__sync_bool_compare_and_swap(&embed_active, true, false));
}

MYSQL *
embedmysql::conn()
{
    /*
     * Need to call mysql_thread_init() in every thread that touches
     * MySQL state.  mysql_server_init() calls it internally.  Safe
     * to call mysql_thread_init() many times.
     */
    mysql_thread_init();
    return m;
}

mysql_thrower::~mysql_thrower()
{
    *this << ": " << current_thd->stmt_da->message();
    throw std::runtime_error(str());
}

extern "C" void *create_embedded_thd(int client_flag);

void
query_parse::cleanup()
{
    if (annot) {
        delete annot;
    }
    if (t) {
        t->end_statement();
        t->cleanup_after_query();
        close_thread_tables(t);
        delete t;
        t = 0;
    }
}

query_parse::~query_parse()
{
    cleanup();
}

LEX *
query_parse::lex()
{
    return t->lex;
}
/*
static void
cloneItemInOrder(ORDER * o) {
    assert_s((*o->item)->type() == Item::Type::FIELD_ITEM, " support for order by/group by non-field not currently implemented" );
    Item ** tmp = (Item **)malloc(sizeof(Item *));
    *tmp = new  Item_field(current_thd, static_cast<Item_field *>(*o->item));
    assert_s(*tmp, "clone item failed on order by element, elements perhaps non constant which is not currently implemented");
    o->item = tmp;
}
*/
query_parse::query_parse(const std::string &db, const std::string &q)
{
    assert(create_embedded_thd(0));
    t = current_thd;
    assert(t != NULL);

    //if first word of query is CRYPTDB, we can't use the embedded db
    //  set annotation to true and return

    if (strncmp(toLowerCase(q).c_str(), "cryptdb", 7) == 0) {
        annot = new Annotation(q);
        return;
    } else {
        annot = NULL;
    }

    try {
        t->set_db(db.data(), db.length());
        mysql_reset_thd_for_next_command(t);
        t->stmt_arena->state = Query_arena::STMT_INITIALIZED;

        char buf[q.size() + 1];
        memcpy(buf, q.c_str(), q.size());
        buf[q.size()] = '\0';
        size_t len = q.size();

        alloc_query(t, buf, len + 1);

        if (ps.init(t, buf, len))
            mysql_thrower() << "Parser_state::init";

        if (parse_sql(t, &ps, 0))
            mysql_thrower() << "parse_sql";

        LEX *lex = t->lex;

        switch (lex->sql_command) {
        case SQLCOM_SHOW_DATABASES:
        case SQLCOM_SHOW_TABLES:
        case SQLCOM_SHOW_FIELDS:
        case SQLCOM_SHOW_KEYS:
        case SQLCOM_SHOW_VARIABLES:
        case SQLCOM_SHOW_STATUS:
        case SQLCOM_SHOW_ENGINE_LOGS:
        case SQLCOM_SHOW_ENGINE_STATUS:
        case SQLCOM_SHOW_ENGINE_MUTEX:
        case SQLCOM_SHOW_PROCESSLIST:
        case SQLCOM_SHOW_MASTER_STAT:
        case SQLCOM_SHOW_SLAVE_STAT:
        case SQLCOM_SHOW_GRANTS:
        case SQLCOM_SHOW_CREATE:
        case SQLCOM_SHOW_CHARSETS:
        case SQLCOM_SHOW_COLLATIONS:
        case SQLCOM_SHOW_CREATE_DB:
        case SQLCOM_SHOW_TABLE_STATUS:
        case SQLCOM_SHOW_TRIGGERS:
        case SQLCOM_LOAD:
        case SQLCOM_SET_OPTION:
        case SQLCOM_LOCK_TABLES:
        case SQLCOM_UNLOCK_TABLES:
        case SQLCOM_GRANT:
        case SQLCOM_CHANGE_DB:
        case SQLCOM_CREATE_DB:
        case SQLCOM_DROP_DB:
        case SQLCOM_ALTER_DB:
        case SQLCOM_REPAIR:
        case SQLCOM_ROLLBACK:
        case SQLCOM_ROLLBACK_TO_SAVEPOINT:
        case SQLCOM_COMMIT:
        case SQLCOM_SAVEPOINT:
        case SQLCOM_RELEASE_SAVEPOINT:
        case SQLCOM_SLAVE_START:
        case SQLCOM_SLAVE_STOP:
        case SQLCOM_BEGIN:
        case SQLCOM_CREATE_TABLE:
        case SQLCOM_CREATE_INDEX:
        case SQLCOM_ALTER_TABLE:
        case SQLCOM_DROP_TABLE:
        case SQLCOM_DROP_INDEX:
            return;
        case SQLCOM_INSERT:
        case SQLCOM_DELETE:
            if (string(lex->select_lex.table_list.first->table_name).substr(0, PWD_TABLE_PREFIX.length()) == PWD_TABLE_PREFIX) {
                return;
            }
        default:
            break;
        }

        /*
         * Helpful in understanding what's going on: JOIN::prepare(),
         * handle_select(), and mysql_select() in sql_select.cc.  Also
         * initial code in mysql_execute_command() in sql_parse.cc.
         */

        lex->select_lex.context.resolve_in_table_list_only(
            lex->select_lex.table_list.first);

        if (t->fill_derived_tables())
            mysql_thrower() << "fill_derived_tables";

        if (open_normal_and_derived_tables(t, lex->query_tables, 0))
            mysql_thrower() << "open_normal_and_derived_tables";

        if (lex->sql_command == SQLCOM_SELECT) {
            if (!lex->select_lex.master_unit()->is_union() &&
                !lex->select_lex.master_unit()->fake_select_lex)
            {
                JOIN *j = new JOIN(t, lex->select_lex.item_list,
                                   lex->select_lex.options, 0);
                if (j->prepare(&lex->select_lex.ref_pointer_array,
                               lex->select_lex.table_list.first,
                               lex->select_lex.with_wild,
                               lex->select_lex.where,
                               lex->select_lex.order_list.elements
                                   + lex->select_lex.group_list.elements,
                               lex->select_lex.order_list.first,
                               lex->select_lex.group_list.first,
                               lex->select_lex.having,
                               lex->proc_list.first,
                               &lex->select_lex,
                               &lex->unit))
                    mysql_thrower() << "JOIN::prepare";
            } else {
                thrower() << "skip unions for now (union=" << lex->select_lex.master_unit()->is_union()
                          << ", fake_select_lex=" << lex->select_lex.master_unit()->fake_select_lex << ")";
                if (lex->unit.prepare(t, 0, 0))
                    mysql_thrower() << "UNIT::prepare";

                /* XXX unit->cleanup()? */

                /* XXX
                 * for unions, it is insufficient to just print lex->select_lex,
                 * because there are other select_lex's in the unit..
                 */
            }
        } else if (lex->sql_command == SQLCOM_DELETE) {
            if (mysql_prepare_delete(t, lex->query_tables, &lex->select_lex.where))
                mysql_thrower() << "mysql_prepare_delete";

            if (lex->select_lex.setup_ref_array(t, lex->select_lex.order_list.elements))
                mysql_thrower() << "setup_ref_array";

            List<Item> fields;
            List<Item> all_fields;
            if (setup_order(t, lex->select_lex.ref_pointer_array,
                            lex->query_tables, fields, all_fields,
                            lex->select_lex.order_list.first))
                mysql_thrower() << "setup_order";
        } else if (lex->sql_command == SQLCOM_INSERT) {
            List_iterator_fast<List_item> its(lex->many_values);
            List_item *values = its++;

            if (mysql_prepare_insert(t, lex->query_tables, lex->query_tables->table,
                                     lex->field_list, values,
                                     lex->update_list, lex->value_list,
                                     lex->duplicates,
                                     &lex->select_lex.where,
                                     /* select_insert */ 0,
                                     0, 0))
                mysql_thrower() << "mysql_prepare_insert";

            for (;;) {
                values = its++;
                if (!values)
                    break;

                if (setup_fields(t, 0, *values, MARK_COLUMNS_NONE, 0, 0))
                    mysql_thrower() << "setup_fields";
            }
        } else if (lex->sql_command == SQLCOM_UPDATE) {
            if (mysql_prepare_update(t, lex->query_tables, &lex->select_lex.where,
                                     lex->select_lex.order_list.elements,
                                     lex->select_lex.order_list.first))
                mysql_thrower() << "mysql_prepare_update";

            if (setup_fields_with_no_wrap(t, 0, lex->select_lex.item_list,
                                          MARK_COLUMNS_NONE, 0, 0))
                mysql_thrower() << "setup_fields_with_no_wrap";

            if (setup_fields(t, 0, lex->value_list,
                             MARK_COLUMNS_NONE, 0, 0))
                mysql_thrower() << "setup_fields";

            List<Item> all_fields;
            if (fix_inner_refs(t, all_fields, &lex->select_lex,
                               lex->select_lex.ref_pointer_array))
                mysql_thrower() << "fix_inner_refs";
        } else {
            thrower() << "don't know how to prepare command " << lex->sql_command;
        }
    } catch (...) {
        cleanup();
        throw;
    }

}

