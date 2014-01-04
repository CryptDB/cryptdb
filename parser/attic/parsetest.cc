#include <iostream>

#include <stdio.h>

#include "mysql_glue.hh"
#include "stringify.hh"

#include "sql_priv.h"
#include "unireg.h"
#include "strfunc.h"
#include "sql_class.h"
#include "set_var.h"
#include "sql_base.h"
#include "rpl_handler.h"
#include "sql_parse.h"
#include "sql_plugin.h"
#include "derror.h"

using namespace std;

static void
parse(const char *q)
{
    THD *t = new THD;
    if (t->store_globals())
        printf("store_globals error\n");

    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", q);
    size_t len = strlen(buf);

    alloc_query(t, buf, len + 1);

    Parser_state ps;
    if (!ps.init(t, buf, len)) {
        LEX lex;
        t->lex = &lex;

        lex_start(t);
        mysql_reset_thd_for_next_command(t);

        string db = "current_db";
        t->set_db(db.data(), db.length());

        printf("q=%s\n", buf);
        bool error = parse_sql(t, &ps, 0);
        if (error) {
            printf("parse error: %d %d %d\n", error, t->is_fatal_error,
                   t->is_error());
            printf("parse error: h %p\n", t->get_internal_handler());
            printf("parse error: %d %s\n", t->is_error(), t->stmt_da->message());
        } else {
            cout << "reconstructed query: " << lex << endl;

            // for lex.sql_command SQLCOM_UPDATE
            cout << "value_list: " << lex.value_list << endl;

            // for lex.sql_command SQLCOM_INSERT
            cout << "field_list: " << lex.field_list << endl;
            cout << "many_values: " << lex.many_values << endl;
        }

        t->end_statement();
    } else {
        printf("parser init error\n");
    }

    t->cleanup_after_query();
    delete t;
}

int
main(int ac, char **av)
{
    if (ac != 2) {
        printf("Usage: %s query\n", av[0]);
        exit(1);
    }

    mysql_glue_init();
    const char *q = av[1];
    parse(q);
}
