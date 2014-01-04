/*
 * TestSinglePrinc.cpp
 * -- tests single principal overall
 *
 *
 */

/*
 * TODO: add tests for NULL (inserts with some fields null, selects)
 */

#include <test/TestSinglePrinc.hh>
#include <util/cryptdb_log.hh>

static int ntest = 0;
static int npass = 0;

TestSinglePrinc::TestSinglePrinc()
{

}

TestSinglePrinc::~TestSinglePrinc()
{

}

template <int N>
static ResType
convert(std::string rows[][N], int num_rows)
{
    ResType res;
    for (int j = 0; j < N; j++)
        res.names.push_back(rows[0][j]);
    for (int i = 1; i < num_rows; i++) {
        std::vector<SqlItem> temp;
        for (int j = 0; j < N; j++) {
            /*
             * XXX temporarily fudge this..  Catherine is planning to redo
             * testing so that we don't have to supply expected answers anyway.
             */
            SqlItem item;
            item.null = false;
            item.type = MYSQL_TYPE_BLOB;
            item.data = rows[i][j];
            temp.push_back(item);
        }
        res.rows.push_back(temp);
    }
    return res;
}

static void
CheckSelectResults(const TestConfig &tc, EDBProxy * cl,
                   std::vector<std::string> in, std::vector<ResType> out)
{
    assert_s(
            in.size() == out.size(),
            "different numbers of test queries and expected results");

    std::vector<std::string>::iterator query_it = in.begin();
    std::vector<ResType>::iterator res_it = out.begin();

    LOG(test) << in.size() << "  " << out.size();

    while(query_it != in.end()) {
        ntest++;
        bool passed = true;
        ResType test_res = myExecute(cl, *query_it);
        if (!test_res.ok) {
            LOG(test) << "Query: " << *query_it;
            if (tc.stop_if_fail) {
                assert_s(false, *query_it + " failed");
            }
            passed = false;
        }

        if (passed && !match(test_res, *res_it)) {
            LOG(test) << "From query: " << *query_it;
            cerr << "-----------------------\nExpected result:" << endl;
            PrintRes(*res_it);
            cerr << "Got result:" << endl;
            PrintRes(test_res);
            cerr << "Select or Join test failed\n";
            if (tc.stop_if_fail) {
                assert_s(false, *query_it + " generated the wrong result");
            }
            passed = false;
        }

        if (passed) {
            npass++;
        }

        query_it++;
        res_it++;
    }
}

static void
qUpdateSelect(const TestConfig &tc, EDBProxy *cl,
              const std::string &update, const std::string &select,
              const std::vector<std::string> &exp_names,
              const std::vector<std::vector<std::string> > &exp_rows)
{
    assert_res(myExecute(cl, update), "Query failed, Update or Delete test failed\n");
    ResType expect;
    expect.names = exp_names;

    /*
     * XXX temporarily fudge this..  Catherine is planning to redo testing
     * so that we don't have to supply expected answers anyway.
     */
    for (auto i = exp_rows.begin(); i != exp_rows.end(); i++) {
        std::vector<SqlItem> row;
        for (auto j = i->begin(); j != i->end(); j++) {
            SqlItem item;
            item.type = MYSQL_TYPE_BLOB;
            item.data = *j;
            item.null = (item.data == "NULL");
            row.push_back(item);
        }
        expect.rows.push_back(row);
    }

    ntest++;
    ResType test_res = myExecute(cl, select);
    if (!test_res.ok) {
        cerr << "Update or Delete query " << select << " won't execute\n";
        if (tc.stop_if_fail) {
            assert_s(false, "above query could not execute");
        }
        return;
    }

    if (!match(test_res, expect)) {
        cerr << "Expected result:" << endl;
        PrintRes(expect);
        cerr << "Got result:" << endl;
        PrintRes(test_res);
        if (tc.stop_if_fail) {
            assert_s(false, update+"; "+select+" generated the wrong result");
        }
        return;
    }

    npass++;
}

static void
testCreateDrop(const TestConfig &tc, EDBProxy * cl)
{
    cl->plain_execute("DROP TABLE IF EXISTS table0, table1, table2, table3, table4");

    std::string sql = "CREATE TABLE t1 (id integer, words text)";
    assert_res(cl->execute(sql), "Problem creating table t1 (first time)");
    assert_res(cl->plain_execute(
                 "SELECT * FROM table0"),
             "t1 (first time) was not created properly");

    sql = "CREATE TABLE t2 (id enc integer, other_id integer, words enc text, other_words text)";
    assert_res(cl->execute(sql), "Problem creating table t2 (first time)");
    assert_res(cl->plain_execute(
                 "SELECT * FROM table1"),
             "t2 (first time) was not created properly");

    sql = "DROP TABLE t1";
    assert_res(cl->execute(sql), "Problem dropping t1");
    assert_s(!cl->plain_execute("SELECT * FROM table0").ok, "t1 not dropped");
    sql = "DROP TABLE t2";
    assert_res(cl->execute(sql), "Problem dropping t2");
    assert_s(!cl->plain_execute("SELECT * FROM table1").ok, "t2 not dropped");

    sql = "CREATE TABLE t1 (id integer, words text)";
    assert_res(cl->execute(sql), "Problem creating table t1 (second time)");
    assert_res(cl->plain_execute(
                 "SELECT * FROM table2"),
             "t1 (second time) was not created properly");

    sql =
        "CREATE TABLE t2 (id enc integer, other_id integer, words enc text, other_words text)";
    assert_res(cl->execute(sql), "Problem creating table t2 (second time)");
    assert_res(cl->plain_execute(
                 "SELECT * FROM table3"),
             "t2 (second time) was not created properly");

    assert_res(cl->execute("DROP TABLE t1"), "testCreateDrop won't drop t1");
    assert_res(cl->execute("DROP TABLE t2"), "testCreateDrop won't drop t2");

    if (!PLAIN) {
        sql =
            "CREATE TABLE t3 (id bigint, dec enc decimal(4,5), nnull enc integer NOT NULL, words enc varchar(200), line enc blob NOT NULL)";
        assert_res(cl->execute(sql), "Problem creating table t3");
        assert_res(cl->plain_execute(
                     "SELECT * FROM table4"), "t3 not created properly");

        assert_res(cl->execute("DROP TABLE t3"), "testCreateDrop won't drop t3");
    }
}

//assumes Select is working
static void
testInsert(const TestConfig &tc, EDBProxy * cl)
{
    cl->plain_execute(
        "DROP TABLE IF EXISTS table0, table1, table2, table3, table4, table5, t1");

    assert_res(myCreate(cl,"CREATE TABLE t1 (id integer primary key auto_increment, age enc integer, salary enc integer, address enc text, name text)",
              "CREATE TABLE t1 (id integer primary key auto_increment, age integer, salary integer, address text, name text)"),
             "testInsert could not create table");

    qUpdateSelect(tc, cl,
        "INSERT INTO t1 VALUES (1, 21, 100, '24 Rosedale, Toronto, ONT', 'Pat Carlson')",
        "SELECT * FROM t1",
        { "id", "age", "salary", "address", "name" },
        { { "1", "21", "100", "24 Rosedale, Toronto, ONT", "Pat Carlson" } });
    qUpdateSelect(tc, cl,
        "INSERT INTO t1 (id, age, salary, address, name) VALUES (2, 23, 101, '25 Rosedale, Toronto, ONT', 'Pat Carlson2')",
        "SELECT * from t1",
        { "id", "age", "salary", "address", "name" },
        { { "1", "21", "100", "24 Rosedale, Toronto, ONT", "Pat Carlson" },
          { "2", "23", "101", "25 Rosedale, Toronto, ONT", "Pat Carlson2" } });
    qUpdateSelect(tc, cl,
        "INSERT INTO t1 (age, address, salary, name, id) VALUES (25, '26 Rosedale, Toronto, ONT', 102, 'Pat2 Carlson', 3)",
        "SELECT * from t1",
        { "id", "age", "salary", "address", "name" },
        { { "1", "21", "100", "24 Rosedale, Toronto, ONT", "Pat Carlson" },
          { "2", "23", "101", "25 Rosedale, Toronto, ONT", "Pat Carlson2" },
          { "3", "25", "102", "26 Rosedale, Toronto, ONT", "Pat2 Carlson" } });
    qUpdateSelect(tc, cl,
        "INSERT INTO t1 (age, address, salary, name) values (26, 'test address', 30, 'test name')",
        "select * from t1",
        { "id", "age", "salary", "address", "name" },
        { { "1", "21", "100", "24 Rosedale, Toronto, ONT", "Pat Carlson" },
          { "2", "23", "101", "25 Rosedale, Toronto, ONT", "Pat Carlson2" },
          { "3", "25", "102", "26 Rosedale, Toronto, ONT", "Pat2 Carlson" },
          { "4", "26", "30", "test address", "test name" } });
    qUpdateSelect(tc, cl,
        "INSERT INTO t1 (age, address, salary, name) values (27, 'test address2', 31, 'test name')",
        "select last_insert_id()",
        { "last_insert_id()" },
        { { "5" } });
    qUpdateSelect(tc, cl,
        "INSERT INTO t1 (id) VALUES (7)",
        "select sum(id) from t1",
        { "sum(id)" },
        { { "22" } });
    qUpdateSelect(tc, cl,
        "INSERT INTO t1 (age) VALUES (40)",
        "select age from t1",
        { "age" },
        { { "21" }, { "23" }, { "25" }, { "26" }, { "27" },
          { "NULL" /* XXX weird way to represent NULL */ }, { "40" } });
    qUpdateSelect(tc, cl,
        "INSERT INTO t1 (address) VALUES ('right star to the right')",
        "select address from t1 where id=9",
        { "address" },
        { { "right star to the right" } });
    qUpdateSelect(tc, cl,
        "INSERT INTO t1 (name) VALUES ('Wendy')",
        "select name from t1 where id=10",
        { "name" },
        { { "Wendy" } });
    qUpdateSelect(tc, cl,
        "INSERT INTO t1 (name, address, id, age) VALUES ('Peter Pan', 'second star to the right and straight on till morning', 42, 10)",
        "select name, address, age from t1 where id=42",
        { "name", "address", "age" },
        { { "Peter Pan", "second star to the right and straight on till morning", "10" } });

    if (!PLAIN) {
        assert_res(cl->execute("DROP TABLE t1"), "testInsert can't drop t1");
    } else {
        assert_res(myExecute(cl,
                           "DELETE FROM t1"),
                 "testInsert can't delete from t1");
    }
}

//assumes Insert is working
static void
testSelect(const TestConfig &tc, EDBProxy * cl)
{
    cl->plain_execute(
        "DROP TABLE IF EXISTS table0, table1, table2, table3, table4, table5, table6, t1");

    assert_res(myCreate(cl,"CREATE TABLE t1 (id integer, age enc integer, salary enc integer, address enc text, name enc text)",
              "CREATE TABLE t1 (id integer, age integer, salary integer, address text, name text)"),
         "testSelect couldn't create table");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (1, 10, 0, 'first star to the right and straight on till morning', 'Peter Pan')"),
             "testSelect couldn't insert (1)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (2, 16, 1000, 'Green Gables', 'Anne Shirley')"),
             "testSelect couldn't insert (2)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (3, 8, 0, 'London', 'Lucy')"),
             "testSelect couldn't insert (3)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (4, 10, 0, 'London', 'Edmund')"),
             "testSelect couldn't insert (4)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (5, 30, 100000, '221B Baker Street', 'Sherlock Holmes')"),
             "testSelect couldn't insert (5)");

    std::vector<std::string> query;
    std::vector<ResType> reply;

    query.push_back("SELECT * FROM t1");
    std::string rows1[6][5] = { {"id", "age", "salary", "address", "name"},
                           {"1", "10", "0",
                            "first star to the right and straight on till morning",
                            "Peter Pan"},
                           {"2", "16", "1000", "Green Gables", "Anne Shirley"},
                           {"3", "8", "0", "London", "Lucy"},
                           {"4", "10", "0", "London", "Edmund"},
                           {"5", "30", "100000", "221B Baker Street",
                            "Sherlock Holmes"} };
    reply.push_back(convert(rows1,6));

    query.push_back("SELECT max(id) FROM t1");
    std::string rows2[2][1] = { {"max(id)"},
                           {"5"} };
    reply.push_back(convert(rows2,2));

    query.push_back("SELECT max(salary) FROM t1");
    std::string rows3[2][1] = { {"max(salary)"},
                           {"100000"} };
    reply.push_back(convert(rows3,2));

    query.push_back("SELECT COUNT(*) FROM t1");
    std::string rows4[2][1] = { {"COUNT(*)"},
                           {"5"} };
    reply.push_back(convert(rows4,2));

    query.push_back("SELECT COUNT(DISTINCT age) FROM t1");
    std::string rows5[2][1] = { {"COUNT(DISTINCT age)"},
                           {"4"} };
    reply.push_back(convert(rows5,2));

    query.push_back("SELECT COUNT(DISTINCT(address)) FROM t1");
    std::string rows100[2][1] = { {"COUNT(DISTINCT(address))"},
                             {"4"} };
    reply.push_back(convert(rows100,2));

    query.push_back("SELECT name FROM t1");
    std::string rows6[6][1] = { {"name"},
                           {"Peter Pan"},
                           {"Anne Shirley"},
                           {"Lucy"},
                           {"Edmund"},
                           {"Sherlock Holmes"} };
    reply.push_back(convert(rows6,6));

    query.push_back("SELECT address FROM t1");
    std::string rows7[6][1] = { { "address"},
                           {
                               "first star to the right and straight on till morning"
                           },
                           {"Green Gables"},
                           {"London"},
                           {"London"},
                           {"221B Baker Street"} };
    reply.push_back(convert(rows7,6));

    query.push_back(
        "SELECT sum(age), max(salary), min(salary), COUNT(name), address FROM t1");
    std::string rows8[2][5] =
    { {"sum(age)", "max(salary)", "min(salary)", "COUNT(name)", "address"},
      {"74",        "100000",     "0",           "5",
       "first star to the right and straight on till morning"} };
    reply.push_back(convert(rows8,2));

    query.push_back("SELECT * FROM t1 WHERE id = 1");
    std::string rows9[2][5] = { {"id", "age", "salary", "address", "name"},
                           {"1", "10", "0",
                            "first star to the right and straight on till morning",
                            "Peter Pan"} };
    reply.push_back(convert(rows9,2));

    query.push_back("SELECT * FROM t1 WHERE id>3");
    std::string rows10[3][5] = { {"id", "age", "salary", "address", "name"},
                            {"4", "10", "0", "London", "Edmund"},
                            {"5", "30", "100000", "221B Baker Street",
                             "Sherlock Holmes"} };
    reply.push_back(convert(rows10,3));

    query.push_back("SELECT * FROM t1 WHERE age = 8");
    std::string rows11[2][5] = { {"id", "age", "salary", "address", "name"},
                            {"3", "8", "0", "London", "Lucy"} };
    reply.push_back(convert(rows11,2));

    query.push_back("SELECT * FROM t1 WHERE salary = 15");
    reply.push_back(ResType());

    query.push_back("SELECT * FROM t1 WHERE age > 10");
    std::string rows12[3][5] = { {"id", "age", "salary", "address", "name"},
                            {"2", "16", "1000", "Green Gables",
                             "Anne Shirley"},
                            {"5", "30", "100000", "221B Baker Street",
                             "Sherlock Holmes"} };
    reply.push_back(convert(rows12,3));

    query.push_back("SELECT * FROM t1 WHERE age = 10 AND salary = 0");
    std::string rows13[3][5] = { {"id", "age", "salary", "address", "name"},
                            {"1", "10", "0",
                             "first star to the right and straight on till morning",
                             "Peter Pan"},
                            {"4", "10", "0", "London", "Edmund"} };
    reply.push_back(convert(rows13,3));

    query.push_back("SELECT * FROM t1 WHERE age = 10 OR salary = 0");
    std::string rows14[4][5] = { {"id", "age", "salary", "address", "name"},
                            {"1", "10", "0",
                             "first star to the right and straight on till morning",
                             "Peter Pan"},
                            {"3", "8", "0", "London", "Lucy"},
                            {"4", "10", "0", "London", "Edmund"} };
    reply.push_back(convert(rows14,4));

    query.push_back("SELECT * FROM t1 WHERE name = 'Peter Pan'");
    std::string rows15[2][5] = { {"id", "age", "salary", "address", "name"},
                            {"1", "10", "0",
                             "first star to the right and straight on till morning",
                             "Peter Pan"} };
    reply.push_back(convert(rows15,2));

    //-------------------------------------------------------------------------------
    //afer the WHERE statement

    query.push_back("SELECT * FROM t1 WHERE address= 'Green Gables'");
    std::string rows16[2][5] = { {"id", "age", "salary", "address", "name"},
                            {"2", "16", "1000", "Green Gables",
                             "Anne Shirley"} };
    reply.push_back(convert(rows16,2));

    query.push_back("SELECT * FROM t1 WHERE address <= '221C'");
    std::string rows17[2][5] = { {"id", "age", "salary", "address", "name"},
                            {"5", "30", "100000", "221B Baker Street",
                             "Sherlock Holmes"} };
    reply.push_back(convert(rows17,2));

    query.push_back(
        "SELECT * FROM t1 WHERE address >= 'Green Gables' AND age > 9");
    std::string rows18[3][5] = { {"id", "age", "salary", "address", "name"},
                            {"2", "16", "1000", "Green Gables",
                             "Anne Shirley"},
                            {"4", "10", "0", "London", "Edmund"} };
    reply.push_back(convert(rows18,3));

    query.push_back(
        "SELECT * FROM t1 WHERE address >= 'Green Gables' OR age > 9");
    std::string rows19[6][5] = { {"id", "age", "salary", "address", "name"},
                            {"1", "10", "0",
                             "first star to the right and straight on till morning",
                             "Peter Pan"},
                            {"2", "16", "1000", "Green Gables",
                             "Anne Shirley"},
                            {"3", "8", "0", "London", "Lucy"},
                            {"4", "10", "0", "London", "Edmund"},
                            {"5", "30", "100000", "221B Baker Street",
                             "Sherlock Holmes"} };
    reply.push_back(convert(rows19,6));

    query.push_back("SELECT * FROM t1 ORDER BY id");
    std::string rows20[6][5] = { {"id", "age", "salary", "address", "name"},
                            {"1", "10", "0",
                             "first star to the right and straight on till morning",
                             "Peter Pan"},
                            {"2", "16", "1000", "Green Gables",
                             "Anne Shirley"},
                            {"3", "8", "0", "London", "Lucy"},
                            {"4", "10", "0", "London", "Edmund"},
                            {"5", "30", "100000", "221B Baker Street",
                             "Sherlock Holmes"} };
    reply.push_back(convert(rows20,6));

    query.push_back("SELECT * FROM t1 ORDER BY salary");
    std::string rows21[6][5] = { {"id", "age", "salary", "address", "name"},
                            {"1", "10", "0",
                             "first star to the right and straight on till morning",
                             "Peter Pan"},
                            {"3", "8", "0", "London", "Lucy"},
                            {"4", "10", "0", "London", "Edmund"},
                            {"2", "16", "1000", "Green Gables",
                             "Anne Shirley"},
                            {"5", "30", "100000", "221B Baker Street",
                             "Sherlock Holmes"} };
    reply.push_back(convert(rows21,6));

    query.push_back("SELECT * FROM t1 ORDER BY name");
    std::string rows22[6][5] = { {"id", "age", "salary", "address", "name"},
                            {"2", "16", "1000", "Green Gables",
                             "Anne Shirley"},
                            {"4", "10", "0", "London", "Edmund"},
                            {"3", "8", "0", "London", "Lucy"},
                            {"1", "10", "0",
                             "first star to the right and straight on till morning",
                             "Peter Pan"},
                            {"5", "30", "100000", "221B Baker Street",
                             "Sherlock Holmes"} };
    reply.push_back(convert(rows22,6));

    query.push_back("SELECT * FROM t1 ORDER BY address");
    std::string rows23[6][5] = { {"id", "age", "salary", "address", "name"},
                            {"5", "30", "100000", "221B Baker Street",
                             "Sherlock Holmes"},
                            {"1", "10", "0",
                             "first star to the right and straight on till morning",
                             "Peter Pan"},
                            {"2", "16", "1000", "Green Gables",
                             "Anne Shirley"},
                            {"3", "8", "0", "London", "Lucy"},
                            {"4", "10", "0", "London", "Edmund"} };
    reply.push_back(convert(rows23,6));

    query.push_back("SELECT sum(age) FROM t1 GROUP BY address");
    std::string rows24[5][1] = { {"sum(age)"},
                            {"30"},
                            {"10"},
                            {"16"},
                            {"18"} };
    reply.push_back(convert(rows24,5));

    query.push_back("SELECT salary, max(id) FROM t1 GROUP BY salary");
    std::string rows24a[4][2] = { {"salary", "max(id)"},
                            {"0", "4"},
                            {"1000", "2"},
                            {"100000", "5"} };
    reply.push_back(convert(rows24a,4));

    query.push_back("SELECT * FROM t1 GROUP BY age ORDER BY age");
    std::string rows25[5][5] = { {"id", "age", "salary", "address", "name"},
                            {"3", "8", "0", "London", "Lucy"},
                            {"1", "10", "0",
                             "first star to the right and straight on till morning",
                             "Peter Pan"},
                            {"2", "16", "1000", "Green Gables",
                             "Anne Shirley"},
                            {"5", "30", "100000", "221B Baker Street",
                             "Sherlock Holmes"} };
    reply.push_back(convert(rows25,5));

    query.push_back("SELECT * FROM t1 ORDER BY age ASC");
    std::string rows26[6][5] = { {"id", "age", "salary", "address", "name"},
                            {"3", "8", "0", "London", "Lucy"},
                            {"1", "10", "0",
                             "first star to the right and straight on till morning",
                             "Peter Pan"},
                            {"4", "10", "0", "London", "Edmund"},
                            {"2", "16", "1000", "Green Gables",
                             "Anne Shirley"},
                            {"5", "30", "100000", "221B Baker Street",
                             "Sherlock Holmes"} };
    reply.push_back(convert(rows26,6));

    query.push_back("SELECT * FROM t1 ORDER BY age DESC");
    std::string rows27[6][5] = { {"id", "age", "salary", "address", "name"},
                            {"5", "30", "100000", "221B Baker Street",
                             "Sherlock Holmes"},
                            {"2", "16", "1000", "Green Gables",
                             "Anne Shirley"},
                            {"1", "10", "0",
                             "first star to the right and straight on till morning",
                             "Peter Pan"},
                            {"4", "10", "0", "London", "Edmund"},
                            {"3", "8", "0", "London", "Lucy"} };
    reply.push_back(convert(rows27,6));

    //------------------------------------------------------------------------------
    // aliases

    query.push_back("SELECT sum(age) as z FROM t1");
    std::string rows28[2][1] = { {"z"},
                            {"74"} };
    reply.push_back(convert(rows28,2));

    query.push_back("SELECT sum(age) z FROM t1");
    std::string rows29[2][1] = { {"z"},
                            {"74"} };
    reply.push_back(convert(rows29,2));

    query.push_back("SELECT min(t.id) a FROM t1 AS t");
    std::string rows30[2][1] = { {"a"},
                            {"1"} };
    reply.push_back(convert(rows30,2));

    query.push_back("SELECT t.address AS b FROM t1 t");
    std::string rows31[6][1] = { {"b"},
                            {
                                "first star to the right and straight on till morning"
                            },
                            {"Green Gables"},
                            {"London"},
                            {"London"},
                            {"221B Baker Street"} };
    reply.push_back(convert(rows31,6));

    CheckSelectResults(tc, cl, query, reply);

    if (!PLAIN) {
        assert_res(cl->execute("DROP TABLE t1"), "testInsert can't drop t1");
    } else {
        assert_res(myExecute(cl,
                           "DELETE FROM t1"),
                 "testInsert can't delete from t1");
    }
}

static void
testJoin(const TestConfig &tc, EDBProxy * cl)
{
    cl->plain_execute(
        "DROP TABLE IF EXISTS table0, table1, table2, table3, table4, table5, table6, table7, table8, t1, t2");

    assert_res(myCreate(cl,"CREATE TABLE t1 (id integer, age enc integer, salary enc integer, address enc text, name enc text)",
              "CREATE TABLE t1 (id integer, age integer, salary integer, address text, name text)"),
                 "testJoin couldn't create table");

    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (1, 10, 0, 'first star to the right and straight on till morning', 'Peter Pan')"),
             "testJoin couldn't insert (1)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (2, 16, 1000, 'Green Gables', 'Anne Shirley')"),
             "testJoin couldn't insert (2)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (3, 8, 0, 'London', 'Lucy')"),
             "testJoin couldn't insert (3)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (4, 10, 0, 'London', 'Edmund')"),
             "testJoin couldn't insert (4)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (5, 30, 100000, '221B Baker Street', 'Sherlock Holmes')"),
             "testJoin couldn't insert (5)");

    assert_res(myCreate(cl,"CREATE TABLE t2 (id integer, books enc integer, name enc text)",
              "CREATE TABLE t2 (id integer, books integer, name text)"),
                 "testJoin couldn't create table");
    assert_res(myExecute(cl, "INSERT INTO t2 VALUES (1, 6, 'Peter Pan')"),
             "testJoin couldn't insert (1)");
    assert_res(myExecute(cl, "INSERT INTO t2 VALUES (2, 8, 'Anne Shirley')"),
             "testJoin couldn't insert (2)");
    assert_res(myExecute(cl, "INSERT INTO t2 VALUES (3, 7, 'Lucy')"),
             "testJoin couldn't insert (3)");
    assert_res(myExecute(cl, "INSERT INTO t2 VALUES (4, 7, 'Edmund')"),
             "testJoin couldn't insert (4)");
    assert_res(myExecute(cl,
                       "INSERT INTO t2 VALUES (10, 4, '221B Baker Street')"),
             "testJoin couldn't insert (5)");

    std::vector<std::string> query;
    std::vector<ResType> reply;

    //int comparison
    query.push_back("SELECT address FROM t1, t2 WHERE t1.id=t2.id");
    std::string rows1[5][1] = { {"address"},
                           {
                               "first star to the right and straight on till morning"
                           },
                           {"Green Gables"},
                           {"London"},
                           {"London"} };
    reply.push_back(convert(rows1,5));
    query.push_back(
        "SELECT t1.id, t2.id, age, books, t2.name FROM t1, t2 WHERE t1.id=t2.id");
    std::string rows2[5][5] = { {"id", "id", "age", "books", "name"},
                           {"1", "1", "10", "6", "Peter Pan"},
                           {"2", "2", "16", "8", "Anne Shirley"},
                           {"3", "3", "8", "7", "Lucy"},
                           {"4", "4", "10", "7", "Edmund"} };
    reply.push_back(convert(rows2,5));
    query.push_back(
        "SELECT t1.name, age, salary, t2.name, books FROM t1, t2 WHERE t1.age=t2.books");
    std::string rows3[2][5] = { {"name", "age", "salary", "name", "books"},
                           {"Lucy", "8", "0", "Anne Shirley", "8"} };
    reply.push_back(convert(rows3,2));

    //string comparison
    query.push_back(
        "SELECT t1.id, t2.id, age, books, t2.name FROM t1, t2 WHERE t1.name=t2.name");
    std::string rows4[5][5] = { {"id", "id", "age", "books", "name"},
                           {"1", "1", "10", "6", "Peter Pan"},
                           {"2", "2", "16", "8", "Anne Shirley"},
                           {"3", "3", "8", "7", "Lucy"},
                           {"4", "4", "10", "7", "Edmund"} };
    reply.push_back(convert(rows4,5));
    query.push_back(
        "SELECT t1.id, age, address, t2.id, books FROM t1, t2 WHERE t1.address=t2.name");
    std::string rows5[2][5] = { {"id", "age", "address", "id", "books"},
                           {"5", "30", "221B Baker Street", "10", "4"} };
    reply.push_back(convert(rows5,2));

    //with aliases
    query.push_back("SELECT address FROM t1 AS a, t2 WHERE a.id=t2.id");
    std::string rows11[5][1] = { {"address"},
                            {
                                "first star to the right and straight on till morning"
                            },
                            {"Green Gables"},
                            {"London"},
                            {"London"} };
    reply.push_back(convert(rows11,5));
    query.push_back(
        "SELECT a.id, b.id, age, books, b.name FROM t1 a, t2 AS b WHERE a.id=b.id");
    std::string rows12[5][5] = { {"id", "id", "age", "books", "name"},
                            {"1", "1", "10", "6", "Peter Pan"},
                            {"2", "2", "16", "8", "Anne Shirley"},
                            {"3", "3", "8", "7", "Lucy"},
                            {"4", "4", "10", "7", "Edmund"} };
    reply.push_back(convert(rows12,5));
    query.push_back(
        "SELECT t1.name, age, salary, b.name, books FROM t1, t2 b WHERE t1.age=b.books");
    std::string rows13[2][5] = { {"name", "age", "salary", "name", "books"},
                            {"Lucy", "8", "0", "Anne Shirley", "8"} };
    reply.push_back(convert(rows13,2));

    CheckSelectResults(tc, cl, query, reply);

    if (!PLAIN) {
        assert_res(cl->execute("DROP TABLE t1, t2"), "testInsert can't drop t1");
    } else {
        assert_res(myExecute(cl,
                           "DELETE FROM t1"),
                 "testInsert can't delete from t1");
        assert_res(myExecute(cl,
                           "DELETE FROM t2"),
                 "testInsert can't delete from t2");
    }

}

//assumes Select works
static void
testUpdate(const TestConfig &tc, EDBProxy * cl)
{
    cl->plain_execute(
        "DROP TABLE IF EXISTS table0, table1, table2, table3, table4, table5, table6, table7, table8, table9, table10, t1");

    assert_res(myCreate(cl,"CREATE TABLE t1 (id integer, age enc integer, salary enc integer, address enc text, name enc text)",
              "CREATE TABLE t1 (id integer, age integer, salary integer, address text, name text)"),
                 "testUpdate couldn't create table");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (1, 10, 0, 'first star to the right and straight on till morning', 'Peter Pan')"),
             "testUpdate couldn't insert (1)");

    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (2, 16, 1000, 'Green Gables', 'Anne Shirley')"),
             "testUpdate couldn't insert (2)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (3, 8, 0, 'London', 'Lucy')"),
             "testUpdate couldn't insert (3)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (4, 10, 0, 'London', 'Edmund')"),
             "testUpdate couldn't insert (4)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (5, 30, 100000, '221B Baker Street', 'Sherlock Holmes')"),
             "testUpdate couldn't insert (5)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (6, 11, 0, 'hi', 'noone')"),
             "testUpdate couldn't insert (6)");

    qUpdateSelect(tc, cl, "UPDATE t1 SET salary=0",
                  "SELECT * FROM t1",
                  {"id", "age", "salary", "address", "name"},
                  { {"1", "10", "0",
                     "first star to the right and straight on till morning",
                     "Peter Pan"},
                    {"2", "16", "0", "Green Gables", "Anne Shirley"},
                    {"3", "8", "0", "London", "Lucy"},
                    {"4", "10", "0", "London", "Edmund"},
                    {"5", "30", "0", "221B Baker Street",
                     "Sherlock Holmes"},
                    {"6", "11", "0", "hi", "noone"} });

    qUpdateSelect(tc, cl, "UPDATE t1 SET age=21 WHERE id = 6",
                  "SELECT * FROM t1",
                  {"id", "age", "salary", "address", "name"},
                  { {"1", "10", "0",
                     "first star to the right and straight on till morning",
                     "Peter Pan"},
                    {"2", "16", "0", "Green Gables", "Anne Shirley"},
                    {"3", "8", "0", "London", "Lucy"},
                    {"4", "10", "0", "London", "Edmund"},
                    {"5", "30", "0", "221B Baker Street",
                     "Sherlock Holmes"},
                    {"6", "21", "0", "hi", "noone"} });

    qUpdateSelect(
        tc, cl,
        "UPDATE t1 SET address='Pemberly', name='Elizabeth Darcy' WHERE id=6",
        "SELECT * FROM t1",
        {"id", "age", "salary", "address", "name"},
        { {"1", "10", "0",
           "first star to the right and straight on till morning",
           "Peter Pan"},
          {"2", "16", "0", "Green Gables", "Anne Shirley"},
          {"3", "8", "0", "London", "Lucy"},
          {"4", "10", "0", "London", "Edmund"},
          {"5", "30", "0", "221B Baker Street",
           "Sherlock Holmes"},
          {"6", "21", "0", "Pemberly", "Elizabeth Darcy"} });

    qUpdateSelect(tc, cl, "UPDATE t1 SET salary=55000 WHERE age=30",
                  "SELECT * FROM t1",
                  {"id", "age", "salary", "address", "name"},
                  { {"1", "10", "0",
                     "first star to the right and straight on till morning",
                     "Peter Pan"},
                    {"2", "16", "0", "Green Gables", "Anne Shirley"},
                    {"3", "8", "0", "London", "Lucy"},
                    {"4", "10", "0", "London", "Edmund"},
                    {"5", "30", "55000", "221B Baker Street",
                     "Sherlock Holmes"},
                    {"6", "21", "0", "Pemberly", "Elizabeth Darcy"} });

    qUpdateSelect(tc, cl, "UPDATE t1 SET salary=20000 WHERE address='Pemberly'",
                  "SELECT * FROM t1",
                  { "id", "age", "salary", "address", "name" },
                  { { "1", "10", "0",
                      "first star to the right and straight on till morning",
                      "Peter Pan"},
                    { "2", "16", "0", "Green Gables", "Anne Shirley" },
                    { "3", "8", "0", "London", "Lucy" },
                    { "4", "10", "0", "London", "Edmund" },
                    { "5", "30", "55000", "221B Baker Street",
                      "Sherlock Holmes" },
                    { "6", "21", "20000", "Pemberly",
                      "Elizabeth Darcy" } });

    qUpdateSelect(tc, cl,"SELECT * FROM t1",
                  "SELECT age FROM t1 WHERE age > 20",
                  {"age"},
                  { {"30"},
                    {"21"} });

    qUpdateSelect(tc, cl, "SELECT id FROM t1",
                  "SELECT sum(age) FROM t1",
                  {"sum(age)"},
                  { {"95"} });

    qUpdateSelect(tc, cl, "UPDATE t1 SET age = 20 WHERE name='Elizabeth Darcy'",
                  "SELECT * FROM t1 WHERE age > 20",
                  {"id","age","salary","address","name"},
                  { { "5", "30", "55000", "221B Baker Street",
            "Sherlock Holmes" } });

    qUpdateSelect(tc, cl, "SELECT id FROM t1",
                  "SELECT sum(age) FROM t1",
                  {"sum(age)"},
                  { {"94"} });

    qUpdateSelect(tc, cl, "UPDATE t1 SET age = age + 2",
                  "SELECT age FROM t1",
                  {"age"},
                  { {"12"},
                    {"18"},
                    {"10"},
                    {"12"},
                    {"32"},
                    {"22"} });

    qUpdateSelect(
        tc, cl,
        "UPDATE t1 SET id = id + 10, salary = salary + 19, name = 'xxx', address = 'foo' WHERE address = 'London'",
        "SELECT * FROM t1",
        {"id","age","salary","address","name"},
        { { "1", "12", "0",
            "first star to the right and straight on till morning",
            "Peter Pan"},
          { "2", "18", "0", "Green Gables", "Anne Shirley" },
          { "13", "10", "19", "foo", "xxx" },
          { "14", "12", "19", "foo", "xxx" },
          { "5", "32", "55000", "221B Baker Street",
            "Sherlock Holmes" },
          { "6", "22", "20000", "Pemberly",
            "Elizabeth Darcy" } });

    qUpdateSelect(tc, cl, "SELECT * FROM t1", "SELECT * FROM t1 WHERE address < 'fml'",
          {"id","age","salary","address","name"},
          { {"1","12","0","first star to the right and straight on till morning", "Peter Pan"},
            {"5","32","55000","221B Baker Street","Sherlock Holmes"} });

    qUpdateSelect(tc, cl, "UPDATE t1 SET address = 'Neverland' WHERE id = 1", "SELECT * FROM t1 WHERE address < 'fml'",
          {"id", "age", "salary", "address", "name"},
          { {"5", "32", "55000", "221B Baker Street", "Sherlock Holmes"} });

    if (!PLAIN) {
        assert_res(cl->execute("DROP TABLE t1"), "testUpdate can't drop t1");
    } else {
        assert_res(myExecute(cl,
                           "DELETE FROM t1"),
                 "testUpdate can't delete from t1");
    }
}

static void
testDelete(const TestConfig &tc, EDBProxy * cl)
{
    cl->plain_execute(
        "DROP TABLE IF EXISTS table0, table1, table2, table3, table4, table5, table6, table7, table8, table9, table10, table11, t1");

    assert_res(myCreate(cl,"CREATE TABLE t1 (id integer, age enc integer, salary enc integer, address enc text, name enc text)",
              "CREATE TABLE t1 (id integer, age integer, salary integer, address text, name text)"),
                 "testDelete couldn't create table");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (1, 10, 0, 'first star to the right and straight on till morning', 'Peter Pan')"),
             "testUpdate couldn't insert (1)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (2, 16, 1000, 'Green Gables', 'Anne Shirley')"),
             "testUpdate couldn't insert (2)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (3, 8, 0, 'London', 'Lucy')"),
             "testUpdate couldn't insert (3)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (4, 10, 0, 'London', 'Edmund')"),
             "testUpdate couldn't insert (4)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (5, 30, 100000, '221B Baker Street', 'Sherlock Holmes')"),
             "testUpdate couldn't insert (5)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (6, 21, 20000, 'Pemberly', 'Elizabeth Darcy')"),
             "testUpdate couldn't insert (6)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (7, 10000, 1, 'Mordor', 'Sauron')"),
             "testUpdate couldn't insert (7)");
    assert_res(myExecute(cl,
                       "INSERT INTO t1 VALUES (8, 25, 100, 'The Heath', 'Eustacia Vye')"),
             "testUpdate couldn't insert (8)");

    qUpdateSelect(tc, cl, "DELETE FROM t1 WHERE id=1", "SELECT * FROM t1",
                  {"id", "age", "salary", "address", "name"},
                  { {"2", "16", "1000", "Green Gables", "Anne Shirley"},
                    {"3", "8", "0", "London", "Lucy"},
                    {"4", "10", "0", "London", "Edmund"},
                    {"5", "30", "100000", "221B Baker Street",
                     "Sherlock Holmes"},
                    {"6", "21", "20000", "Pemberly", "Elizabeth Darcy"},
                    {"7", "10000", "1", "Mordor", "Sauron"},
                    {"8", "25", "100", "The Heath", "Eustacia Vye"} });

    qUpdateSelect(tc, cl, "DELETE FROM t1 WHERE age=30", "SELECT * FROM t1",
                  {"id", "age", "salary", "address", "name"},
                  { {"2", "16", "1000", "Green Gables", "Anne Shirley"},
                    {"3", "8", "0", "London", "Lucy"},
                    {"4", "10", "0", "London", "Edmund"},
                    {"6", "21", "20000", "Pemberly", "Elizabeth Darcy"},
                    {"7", "10000", "1", "Mordor", "Sauron"},
                    {"8", "25", "100", "The Heath", "Eustacia Vye"} });

    qUpdateSelect(tc, cl, "DELETE FROM t1 WHERE name='Eustacia Vye'",
                  "SELECT * FROM t1",
                  {"id", "age", "salary", "address", "name"},
                  { {"2", "16", "1000", "Green Gables", "Anne Shirley"},
                    {"3", "8", "0", "London", "Lucy"},
                    {"4", "10", "0", "London", "Edmund"},
                    {"6", "21", "20000", "Pemberly", "Elizabeth Darcy"},
                    {"7", "10000", "1", "Mordor",
                     "Sauron"} });

    qUpdateSelect(tc, cl, "DELETE FROM t1 WHERE address='London'",
                  "SELECT * FROM t1",
                  {"id", "age", "salary", "address", "name"},
                  { {"2", "16", "1000", "Green Gables", "Anne Shirley"},
                    {"6", "21", "20000", "Pemberly", "Elizabeth Darcy"},
                    {"7", "10000", "1", "Mordor",
                     "Sauron"} });

    qUpdateSelect(tc, cl, "DELETE FROM t1 WHERE salary=1", "SELECT * FROM t1",
                  {"id", "age", "salary", "address", "name"},
                  { {"2", "16", "1000", "Green Gables", "Anne Shirley"},
                    {"6", "21", "20000", "Pemberly", "Elizabeth Darcy"} });

    qUpdateSelect(tc, cl, "DELETE FROM t1", "SELECT * FROM t1", {}, {});

    qUpdateSelect(
        tc, cl,
        "INSERT INTO t1 VALUES (1, 10, 0, 'first star to the right and straight on till morning', 'Peter Pan')",
        "SELECT * FROM t1",
        {"id", "age", "salary", "address", "name"},
        { {"1", "10", "0",
           "first star to the right and straight on till morning",
           "Peter Pan"} });

    qUpdateSelect(tc, cl, "DELETE  FROM t1", "SELECT * FROM t1", {}, {});

    if (!PLAIN) {
        assert_res(cl->execute("DROP TABLE t1"), "testDelete can't drop t1");
    } else {
        assert_res(myExecute(cl,
                           "DELETE FROM t1"),
                 "testDelete can't delete from t1");
    }
}

static void
testSearch(const TestConfig &tc, EDBProxy * cl)
{
    cl->plain_execute(
        "DROP TABLE IF EXISTS table0, table1, table2, table3, table4, table5, table6, table7, table8, table9, table10, table11, table12, t3");

    assert_res(myCreate(cl, "CREATE TABLE t3 (id integer, searchable enc search text)",
                        "CREATE TABLE t3 (id integer, searchable text)"),
                 "testSearch couldn't create table");

    assert_res(myExecute(cl, "INSERT INTO t3 VALUES (1, 'short text')"),
             "testSearch couldn't insert (1)");
    assert_res(myExecute(cl,
                       "INSERT INTO t3 VALUES (2, 'Text with CAPITALIZATION')"),
             "testSearch couldn't insert (2)");
    assert_res(myExecute(cl, "INSERT INTO t3 VALUES (3, '')"),
             "testSearch couldn't insert (3)");
    assert_res(myExecute(cl,
                       "INSERT INTO t3 VALUES (4, 'When I have fears that I may cease to be, before my pen has gleaned my teaming brain; before high-piled books in charactery hold like rich garners the full-ripened grain.  When I behold upon the nights starred face Huge cloudy symbols of high romance And think that I may never live to trace Their shadows with the magic hand of chance.  And when I feel, fair creature of the hour That I shall never look upon thee more, Never have relish of the faerie power Of unreflecting love, I stand alone of the edge of the wide world and think, to love and fame to nothingness do sink')"),
             "testSearch couldn't insert (4)");

    std::vector<std::string> query;
    std::vector<ResType> reply;

    query.push_back("SELECT * FROM t3 WHERE searchable LIKE '%text%'");
    std::string rows1[3][2] = { {"id", "searchable"},
                           {"1", "short text"},
                           {"2", "Text with CAPITALIZATION"} };
    reply.push_back(convert(rows1,3));

    query.push_back("SELECT * FROM t3 WHERE searchable LIKE 'short%'");
    std::string rows2[2][2] = { {"id", "searchable"},
                             {"1", "short text"}};
    reply.push_back(convert(rows2,2));

    query.push_back("SELECT * FROM t3 WHERE searchable LIKE ''");
    std::string rows3[2][2] = { {"id", "searchable"},
                           {"3", ""} };
    reply.push_back(convert(rows3,2));

    query.push_back("SELECT * FROM t3 WHERE searchable LIKE '%capitalization'");
    std::string rows4[2][2] = { {"id", "searchable"},
                           {"2", "Text with CAPITALIZATION"} };
    reply.push_back(convert(rows4,2));

    query.push_back("SELECT * FROM t3 WHERE searchable LIKE 'noword'");
    reply.push_back(ResType());

    query.push_back("SELECT * FROM t3 WHERE searchable LIKE 'when%'");
    std::string rows5[2][2] = { {"id", "searchable"},
                           {"4",
                            "When I have fears that I may cease to be, before my pen has gleaned my teaming brain; before high-piled books in charactery hold like rich garners the full-ripened grain.  When I behold upon the nights starred face Huge cloudy symbols of high romance And think that I may never live to trace Their shadows with the magic hand of chance.  And when I feel, fair creature of the hour That I shall never look upon thee more, Never have relish of the faerie power Of unreflecting love, I stand alone of the edge of the wide world and think, to love and fame to nothingness do sink"} };
    reply.push_back(convert(rows5,2));

    query.push_back("SELECT * FROM t3 WHERE searchable < 'slow'");
    string rows6[3][2] = { {"id","searchable"},
              {"1", "short text"},
              {"3",""} };
    reply.push_back(convert(rows6,3));

    CheckSelectResults(tc, cl, query, reply);

    qUpdateSelect(tc, cl,"UPDATE t3 SET searchable='text that is new' WHERE id=1",
          "SELECT * FROM t3 WHERE searchable < 'slow'",
          {"id","searchable"},
          { {"3",""} } );

    if (!PLAIN) {
        assert_res(cl->execute("DROP TABLE t3"), "testSearch can't drop t3");
    } else {
        assert_res(myExecute(cl,
                           "DELETE FROM t3"),
                 "testSearch can't delete from t3");
    }
}

void
TestSinglePrinc::run(const TestConfig &tc, int argc, char ** argv)
{
    EDBProxy * cl;
    uint64_t mkey = 113341234;
    string masterKey = BytesFromInt(mkey, AES_KEY_BYTES);
    cl = new EDBProxy(tc.host, tc.user, tc.pass, tc.db, tc.port, false);
    cl->setMasterKey(masterKey);

    struct {
        const char *name;
        void (*f)(const TestConfig &, EDBProxy *);
    } tests[] = {
#define E(x) { #x, &x }
        E(testCreateDrop),
        E(testInsert),
        E(testSelect),
        E(testJoin),
        E(testUpdate),
        E(testDelete),
        E(testSearch),
    };

    for (uint i = 0; i < NELEM(tests); i++) {
        cerr << "running " << tests[i].name << endl;
        tests[i].f(tc, cl);
    }

    cerr << "RESULT: " << npass << "/" << ntest << " passed" << endl;
    delete cl;
}
