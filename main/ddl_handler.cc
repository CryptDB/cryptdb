#include <main/ddl_handler.hh>
#include <main/rewrite_util.hh>
#include <main/rewrite_main.hh>
#include <main/alter_sub_handler.hh>
#include <main/dispatcher.hh>
#include <main/macro_util.hh>
#include <parser/lex_util.hh>

class CreateTableHandler : public DDLHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *lex,
                                  const ProxyState &ps) const
    {
        const std::string db_name =
            lex->select_lex.table_list.first->db;
        TEST_DatabaseDiscrepancy(db_name, a.getDatabaseName());
        const std::string table =
            lex->select_lex.table_list.first->table_name;
        LEX *const new_lex = copyWithTHD(lex);

        //TODO: support for "create table like"
        if (lex->create_info.options & HA_LEX_CREATE_TABLE_LIKE) {
            cryptdb_err() << "No support for create table like yet. "
                          << "If you see this, please implement me";
        }

        // Create the table regardless of 'IF NOT EXISTS' if the table
        // doesn't exist.
        if (false == a.tableMetaExists(db_name, table)) {
            // TODO: Use appropriate values for has_sensitive and has_salt.
            std::unique_ptr<TableMeta> tm(new TableMeta(true, true));

            // -----------------------------
            //         Rewrite TABLE       
            // -----------------------------
            // HACK.
            // > We know that there is only one table.
            // > We also know that rewrite_table_list is going to fail to
            // find this table in 'a'.
            // > And we know that the table we want is tm with name table.
            // > This will _NOT_ gracefully handle a malformed CREATE TABLE
            // query.
            assert(1 == new_lex->select_lex.table_list.elements);
            // Take the table name straight from 'tm' as
            // Analysis::getAnonTableName relies on SchemaInfo.
            TABLE_LIST *const tbl =
                rewrite_table_list(new_lex->select_lex.table_list.first,
                                   tm->getAnonTableName());
            new_lex->select_lex.table_list =
                *oneElemListWithTHD<TABLE_LIST>(tbl);

            auto it =
                List_iterator<Create_field>(lex->alter_info.create_list);
            new_lex->alter_info.create_list =
                accumList<Create_field>(it,
                    [&a, &ps, &tm] (List<Create_field> out_list,
                                    Create_field *const cf) {
                        return createAndRewriteField(a, ps, cf, tm.get(),
                                                     true, out_list);
                });

            // -----------------------------
            //         Rewrite INDEX
            // -----------------------------
            auto key_it =
                List_iterator<Key>(lex->alter_info.key_list);
            new_lex->alter_info.key_list =
                accumList<Key>(key_it,
                    [&tm, &a] (List<Key> out_list, Key *const key)
                    {
                        auto keys = rewrite_key(*tm.get(), key, a);
                        out_list.concat(vectorToListWithTHD(keys));

                        return out_list;
                    });

            // -----------------------------
            //         Update TABLE       
            // -----------------------------
            a.deltas.push_back(std::unique_ptr<Delta>(
                            new CreateDelta(std::move(tm),
                                            a.getDatabaseMeta(db_name),
                                            IdentityMetaKey(table))));
        } else { // Table already exists.

            // Make sure we aren't trying to create a table that
            // already exists.
            const bool test =
                lex->create_info.options & HA_LEX_CREATE_IF_NOT_EXISTS;
            TEST_TextMessageError(test,
                                  "Table " + table + " already exists!");

            // -----------------------------
            //         Rewrite TABLE       
            // -----------------------------
            new_lex->select_lex.table_list =
                rewrite_table_list(lex->select_lex.table_list, a);
            // > We do not rewrite the fields because presumably the caller
            // can do a CREATE TABLE IF NOT EXISTS for a table that already
            // exists, but with fields that do not actually exist.
            // > This would cause problems when trying to look up FieldMeta
            // for these non-existant fields.
            // > We may want to do some additional non-deterministic
            // anonymization of the fieldnames to prevent information leaks.
            // (ie, server gets compromised, server logged all sql queries,
            // attacker can see that the admin creates the account table
            // with the credit card field every time the server boots)
        }

        return new_lex;
    }
};

// > TODO: mysql permits a single ALTER TABLE command to invoke _multiple_
//   and _different_ subcommands.
//   ie, ALTER TABLE t ADD COLUMN x integer, ADD INDEX i (z);
//   Currently we do not support mixed operations.
//   > Must guarentee that rewrite_table_list is only called one time.
//   > If we drop Keys and Columns in the same query the order is probably
//     going to get changed.
class AlterTableHandler : public DDLHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *lex,
                                  const ProxyState &ps) const
    {
        assert(sub_dispatcher->canDo(lex));
        const std::vector<AlterSubHandler *> &handlers =
            sub_dispatcher->dispatch(lex);

        LEX *new_lex = copyWithTHD(lex);

        for (auto it : handlers) {
            new_lex = it->transformLex(a, new_lex, ps);
        }

        // -----------------------------
        //         Rewrite TABLE
        // -----------------------------
        // > Rewrite after doing the transformations as the handlers
        // expect the original table name to be intact.
        new_lex->select_lex.table_list =
            rewrite_table_list(new_lex->select_lex.table_list, a, true);

        return new_lex;
    }

    const std::unique_ptr<AlterDispatcher> sub_dispatcher;

public:
    AlterTableHandler() : sub_dispatcher(buildAlterSubDispatcher()) {}
};

class DropTableHandler : public DDLHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *lex,
                                  const ProxyState &ps) const
    {
        LEX *final_lex = rewrite(a, lex, ps);
        update(a, lex, ps);

        return final_lex;
    }
    
    LEX *rewrite(Analysis &a, LEX *lex, const ProxyState &ps) const
    {
        LEX *const new_lex = copyWithTHD(lex);
        new_lex->select_lex.table_list =
            rewrite_table_list(lex->select_lex.table_list, a, true);

        return new_lex;
    }

    void update(Analysis &a, LEX *lex, const ProxyState &ps) const
    {
        TABLE_LIST *tbl = lex->select_lex.table_list.first;
        for (; tbl; tbl = tbl->next_local) {
            char* table  = tbl->table_name;

            TEST_DatabaseDiscrepancy(tbl->db, a.getDatabaseName());
            if (lex->drop_if_exists) {
                if (false == a.tableMetaExists(tbl->db, table)) {
                    continue;
                }
            }

            // Remove from *Meta structures.
            TableMeta const &tm = a.getTableMeta(tbl->db, table);
            a.deltas.push_back(std::unique_ptr<Delta>(
                            new DeleteDelta(tm,
                                            a.getDatabaseMeta(tbl->db))));
        }
    }
};

class CreateDBHandler : public DDLHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *const lex,
                                  const ProxyState &ps) const
    {
        const std::string dbname =
            convert_lex_str(lex->name);
        if (false == a.databaseMetaExists(dbname)) {
            std::unique_ptr<DatabaseMeta> dm(new DatabaseMeta());
            a.deltas.push_back(std::unique_ptr<Delta>(
                        new CreateDelta(std::move(dm), a.getSchema(),
                                        IdentityMetaKey(dbname))));
        } else {
            const bool test =
                lex->create_info.options & HA_LEX_CREATE_IF_NOT_EXISTS;
            TEST_TextMessageError(test,
                                "Database " + dbname + " already exists!");
        }

        return copyWithTHD(lex);
    }
};

class ChangeDBHandler : public DDLHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *const lex,
                                  const ProxyState &ps) const
    {
        return copyWithTHD(lex);
    }
};

class DropDBHandler : public DDLHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *const lex,
                                  const ProxyState &ps) const
    {
        const std::string dbname =
            convert_lex_str(lex->name);
        DatabaseMeta &dm = a.getDatabaseMeta(dbname);
        a.deltas.push_back(std::unique_ptr<Delta>(
                                    new DeleteDelta(dm, a.getSchema())));

        return copyWithTHD(lex);
    }
};

class LockTablesHandler : public DDLHandler {
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *const lex,
                                  const ProxyState &ps) const
    {
        LEX *const new_lex = copyWithTHD(lex);
        new_lex->select_lex.table_list =
            rewrite_table_list(lex->select_lex.table_list, a);

        return new_lex;
    }
};

LEX *DDLHandler::transformLex(Analysis &a, LEX *lex,
                              const ProxyState &ps) const
{
    return this->rewriteAndUpdate(a, lex, ps);
}

// FIXME: Add test to make sure handler added successfully.
SQLDispatcher *buildDDLDispatcher()
{
    DDLHandler *h;
    SQLDispatcher *dispatcher = new SQLDispatcher();

    h = new CreateTableHandler();
    dispatcher->addHandler(SQLCOM_CREATE_TABLE, h);

    h = new AlterTableHandler();
    dispatcher->addHandler(SQLCOM_ALTER_TABLE, h);

    h = new DropTableHandler();
    dispatcher->addHandler(SQLCOM_DROP_TABLE, h);

    h = new CreateDBHandler();
    dispatcher->addHandler(SQLCOM_CREATE_DB, h);

    h = new ChangeDBHandler();
    dispatcher->addHandler(SQLCOM_CHANGE_DB, h);

    h = new DropDBHandler();
    dispatcher->addHandler(SQLCOM_DROP_DB, h);

    h = new LockTablesHandler();
    dispatcher->addHandler(SQLCOM_LOCK_TABLES, h);

    return dispatcher;
}
