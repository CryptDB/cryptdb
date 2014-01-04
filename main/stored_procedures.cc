#include <string>

#include <main/stored_procedures.hh>
#include <main/macro_util.hh>
#include <main/metadata_tables.hh>

std::vector<std::string>
getStoredProcedures()
{
    const std::string current_transaction_id =
        MetaData::Proc::currentTransactionID();
    const std::string hom_addition_transaction =
        MetaData::Proc::homAdditionTransaction();
    const std::string adjust_onion =
        MetaData::Proc::adjustOnion();
    const std::string remote_completion_table =
        MetaData::Table::remoteQueryCompletion();

    const std::vector<std::string> add_procs({
        // ---------------------------------------
        //      def currentTransactionID(id)
        // ---------------------------------------
        " CREATE PROCEDURE " + current_transaction_id + "\n"
        "   (OUT out_id VARCHAR(20))\n"
        " BEGIN\n"
        "   SELECT trx_id INTO out_id FROM INFORMATION_SCHEMA.INNODB_TRX\n"
        "    WHERE INFORMATION_SCHEMA.INNODB_TRX.TRX_MYSQL_THREAD_ID =\n"
        "          (SELECT CONNECTION_ID())\n"
        "      AND INFORMATION_SCHEMA.INNODB_TRX.TRX_STATE =\n"
        "          'RUNNING';\n"
        " END\n",

        // ----------------------------------------------
        //   def homAdditionTransaction(delete, insert)
        // ----------------------------------------------
        /*
            The corner case for this procedure is a call graph like this.
            >>> START TRANSACTION;
            >>> UPDATE t SET x = x + 1;
                > homAdditionTransaction(...);
            >>> ...
                > ...
                > ...
            >>> COMMIT;

            Where '>>>' are queries issue by user and '>' are CryptDB
            transformations.

            (*) It appers that transactions are not propagated to the
            INFORMATION_SCHEMA.INNODB_TRX table until a query is
            issued against a table that supports transactions.

            So it follows that in the above sequence the
            'START TRANSACTION' issued by the user will essentially be
            ignored in favor of a transaction started by
            homAdditionTransction(). This should lead to consistent,
            expected behavior provided (*) is correct rationale.
        */
        " CREATE PROCEDURE " + hom_addition_transaction + "\n"
        "       (IN delete_query VARBINARY(50000),\n"
        "        IN insert_query VARBINARY(50000))\n"
        " BEGIN\n"
        "   DECLARE old_transaction_id VARCHAR(20);\n\n"

        "   CALL " + current_transaction_id + " (old_transaction_id);\n\n"

            // Start a transaction if necessary.
        "   IF old_transaction_id IS NULL THEN\n"
        "       START TRANSACTION;\n"
        "   END IF;\n\n"

            // DELETE old values pertaining to WHERE clause
        "   SET @query = delete_query;\n"
        "   PREPARE dq FROM @query;\n"
        "   EXECUTE dq;\n\n"

            // INSERT new values using subquery from temp table.
        "   SET @query = insert_query;\n"
        "   PREPARE iq FROM @query;\n"
        "   EXECUTE iq;\n\n"

            // Close our transaction if we started one.
        "   IF old_transaction_id IS NULL THEN\n"
        "       COMMIT;\n\n"
        "   END IF;\n"
        " END\n",

        // ---------------------------------------
        //         def adjustOnion(id)
        // ---------------------------------------
        // NOTE: If we need N queries, we will have to use an additional
        // table + cursors.
        " CREATE PROCEDURE " + adjust_onion + "\n"
        "       (IN completion_id INTEGER,\n"
        "        IN adjust_query0 VARBINARY(500),\n"
        "        IN adjust_query1 VARBINARY(500))\n"
        " BEGIN\n"
        "   DECLARE old_transaction_id VARCHAR(20);\n"
        "   DECLARE b_reissue BOOLEAN;\n\n"

            // Are we in a transaction?
        "   CALL " + current_transaction_id + "(old_transaction_id);\n\n"

            // if not, we will want to reissue the original query
        "   IF old_transaction_id IS NULL THEN\n"
        "       SET b_reissue = TRUE;\n"
        "   ELSE\n"
        "       SET b_reissue = FALSE;\n"
        "   END IF;\n\n"

            // cancel pending transaction
        "   ROLLBACK;\n\n"

            // start a new one for this proc call
        "   START TRANSACTION;\n\n"

            // first onion adjustment
        "   SET @query = adjust_query0;\n"
        "   PREPARE aq0 FROM @query;\n"
        "   EXECUTE aq0;\n\n"

            // (possibly) second onion adjustment
        "   SET @query = adjust_query1;\n"
        "   PREPARE aq1 FROM @query;\n"
        "   EXECUTE aq1;\n\n"

            // update metadata used for recovery
        "   INSERT INTO " + remote_completion_table + "\n"
        "      (begin, complete, embedded_completion_id, reissue) VALUES\n"
        "       (TRUE,  TRUE,     completion_id,          b_reissue);\n\n"

        "   COMMIT;\n"
        " END\n"});

    return add_procs;
}

static bool
addStoredProcedures(const std::unique_ptr<Connect> &conn)
{
    auto procs = getStoredProcedures();
    for (auto it : procs) {
        RETURN_FALSE_IF_FALSE(conn->execute(it));
    }

    return true;
}

static bool
dropStoredProcedures(const std::unique_ptr<Connect> &conn)
{
    const std::string current_transaction_id =
        MetaData::Proc::currentTransactionID();
    const std::string hom_addition_transaction =
        MetaData::Proc::homAdditionTransaction();
    const std::string adjust_onion =
        MetaData::Proc::adjustOnion();

    const std::vector<std::string>
        drop_procs({"DROP PROCEDURE IF EXISTS " +
                        current_transaction_id,
                    "DROP PROCEDURE IF EXISTS " +
                        hom_addition_transaction,
                    "DROP PROCEDURE IF EXISTS " +
                        adjust_onion});

    for (auto it : drop_procs) {
        RETURN_FALSE_IF_FALSE(conn->execute(it));
    }

    return true;
}

bool
loadStoredProcedures(const std::unique_ptr<Connect> &conn)
{
    RETURN_FALSE_IF_FALSE(dropStoredProcedures(conn));
    RETURN_FALSE_IF_FALSE(addStoredProcedures(conn));

    return true;
}

