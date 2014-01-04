/*
 * TestQueries.cc
 *  -- end to end query and result test, independant of connection process
 *
 *
 */

#include <algorithm>
#include <stdexcept>
#include <netinet/in.h>

#include <util/errstream.hh>
#include <util/cleanup.hh>
#include <util/cryptdb_log.hh>
#include <main/rewrite_main.hh>

#include <test/TestQueries.hh>


static int ntest = 0;
static int npass = 0;
static test_mode control_type;
static test_mode test_type;
static uint64_t no_conn = 1;
static Connection * control;
static Connection * test;

static QueryList Insert = QueryList("SingleInsert",
    { Query("CREATE TABLE test_insert (id integer , age integer, salary integer, address text, name text)"),
      Query(""),
      Query(""),
      Query("") },
    { Query("CREATE TABLE test_insert (id integer , age integer, salary integer, address text, name text)"),
      Query(""),
      Query(""),
      Query("")},
    // TODO parser currently has no KEY functionality (broken?)
    { Query("INSERT INTO test_insert VALUES (1, 21, 100, '24 Rosedale, Toronto, ONT', 'Pat Carlson')"),
      Query("SELECT * FROM test_insert"),
      Query("INSERT INTO test_insert (id, age, salary, address, name) VALUES (2, 23, 101, '25 Rosedale, Toronto, ONT', 'Pat Carlson2')"),
      Query("SELECT * FROM test_insert"),
      Query("INSERT INTO test_insert (age, address, salary, name, id) VALUES (25, '26 Rosedale, Toronto, ONT', 102, 'Pat2 Carlson', 3)"),
      Query("SELECT * FROM test_insert"),
      Query("INSERT INTO test_insert (age, address, salary, name) VALUES (26, 'test address', 30, 'test name')"),
      Query("SELECT * FROM test_insert"),
      Query("INSERT INTO test_insert (age, address, salary, name) VALUES (27, 'test address2', 31, 'test name')"),
      // Query Fail
      //Query("select last_insert_id()"),

      // This one crashes DBMS! DBMS recovery: ./mysql_upgrade -u root -pletmein 
      //Query("INSERT INTO test_insert (id) VALUES (7)"),
      //Query("select sum(id) from test_insert"),
      Query("INSERT INTO test_insert (age) VALUES (40)"),
      Query("SELECT age FROM test_insert"),
      Query("INSERT INTO test_insert (name) VALUES ('Wendy')"),
      Query("SELECT name FROM test_insert WHERE id=10"),
      Query("INSERT INTO test_insert (name, address, id, age) VALUES ('Peter Pan', 'first star to the right and straight on till morning', 42, 10)"),
      Query("SELECT name, address, age FROM test_insert WHERE id=42") },
    { Query("DROP TABLE test_insert") },
    { Query("DROP TABLE test_insert") } );

//migrated from TestSinglePrinc TestSelect
static QueryList Select = QueryList("SingleSelect",
    { Query("CREATE TABLE IF NOT EXISTS test_select (id integer, age integer, salary integer, address text, name text)"),
      Query(""),
      Query(""),
      Query("") },
    { Query("CREATE TABLE IF NOT EXISTS test_select (id integer, age integer, salary integer, address text, name text)"),
      Query(""),
      Query(""),
      Query("")},
    { Query("INSERT INTO test_select VALUES (1, 10, 0, 'first star to the right and straight on till morning', 'Peter Pan')"),
      Query("INSERT INTO test_select VALUES (2, 16, 1000, 'Green Gables', 'Anne Shirley')"),
      Query("INSERT INTO test_select VALUES (3, 8, 0, 'London', 'Lucy')"),
      Query("INSERT INTO test_select VALUES (4, 10, 0, 'London', 'Edmund')"),
      Query("INSERT INTO test_select VALUES (5, 30, 100000, '221B Baker Street', 'Sherlock Holmes')"),
      Query("SELECT * FROM test_select WHERE id IN (1, 2, 10, 20, 30)"),
      Query("SELECT * FROM test_select WHERE id BETWEEN 3 AND 5"),
      Query("SELECT NULLIF(1, id) FROM test_select"),
      Query("SELECT NULLIF(id, 1) FROM test_select"),
      Query("SELECT NULLIF(id, id) FROM test_select"),
      Query("SELECT NULLIF(1, 2) FROM test_select"),
      Query("SELECT NULLIF(1, 1) FROM test_select"),
      Query("SELECT * FROM test_select"),
      Query("SELECT max(id) FROM test_select"),
      Query("SELECT max(salary) FROM test_select"),
      Query("SELECT COUNT(*) FROM test_select"),
      Query("SELECT COUNT(DISTINCT age) FROM test_select"),
      Query("SELECT COUNT(DISTINCT(address)) FROM test_select"),
      Query("SELECT name FROM test_select"),
      Query("SELECT address FROM test_select"),
      Query("SELECT * FROM test_select WHERE id>3"),
      Query("SELECT * FROM test_select WHERE age = 8"),
      Query("SELECT * FROM test_select WHERE salary=15"),
      Query("SELECT * FROM test_select WHERE age > 10"),
      Query("SELECT * FROM test_select WHERE age = 10 AND salary = 0"),
      Query("SELECT * FROM test_select WHERE age = 10 OR salary = 0"),
      Query("SELECT * FROM test_select WHERE name = 'Peter Pan'"),
      Query("SELECT * FROM test_select WHERE address='Green Gables'"),
      Query("SELECT * FROM test_select WHERE address <= '221C'"),
      Query("SELECT * FROM test_select WHERE address >= 'Green Gables' AND age > 9"),
      Query("SELECT * FROM test_select WHERE address >= 'Green Gables' OR age > 9"),
      Query("SELECT * FROM test_select WHERE address < 'ffFFF'"),
      Query("SELECT * FROM test_select ORDER BY id"),
      Query("SELECT * FROM test_select ORDER BY salary"),
      Query("SELECT * FROM test_select ORDER BY name"),
      Query("SELECT * FROM test_select ORDER BY address"),
      Query("SELECT sum(age) FROM test_select GROUP BY address ORDER BY address"),
      Query("SELECT salary, max(id) FROM test_select GROUP BY salary ORDER BY salary"),
      Query("SELECT * FROM test_select GROUP BY age ORDER BY age"),
      Query("SELECT * FROM test_select ORDER BY age ASC"),
      Query("SELECT * FROM test_select ORDER BY address DESC"),
      Query("SELECT sum(age) as z FROM test_select"),
      Query("SELECT sum(age) z FROM test_select"),
      Query("SELECT min(t.id) a FROM test_select AS t"),
      Query("SELECT t.address AS b FROM test_select t"),
      // Query("SELECT * FROM test_select HAVING age"),
      // Query("SELECT * FROM test_select HAVING age && id"),
      // BestEffort (Add more subquery tests as we expand functionality)
      // Query("SELECT * FROM test_select WHERE id IN (SELECT id FROM test_select)"),
      // Query("SELECT * FROM test_select WHERE id IN (SELECT 1 FROM test_select)")
      },
    { Query("DROP TABLE test_select") },
    { Query("DROP TABLE test_select") } );

//migrated from TestSinglePrinc TestJoin
static QueryList Join = QueryList("SingleJoin",
    { Query("CREATE TABLE test_join1 (id integer, age integer, salary integer, address text, name text)"),
      Query("CREATE TABLE test_join2 (id integer, books integer, name text)"),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query("") },
    { Query("CREATE TABLE test_join1 (id integer, age integer, salary integer, address text, name text)"),
      Query("CREATE TABLE test_join2 (id integer, books integer, name text)"),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query("") },
    { Query("INSERT INTO test_join1 VALUES (1, 10, 0, 'first star to the right and straight on till morning','Peter Pan')"),
      Query("INSERT INTO test_join1 VALUES (2, 16, 1000, 'Green Gables', 'Anne Shirley')"),
      Query("INSERT INTO test_join1 VALUES (3, 8, 0, 'London', 'Lucy')"),
      Query("INSERT INTO test_join1 VALUES (4, 10, 0, 'London', 'Edmund')"),
      Query("INSERT INTO test_join1 VALUES (5, 30, 100000, '221B Baker Street', 'Sherlock Holmes')"),
      Query("INSERT INTO test_join2 VALUES (1, 6, 'Peter Pan')"),
      Query("INSERT INTO test_join2 VALUES (2, 8, 'Anne Shirley')"),
      Query("INSERT INTO test_join2 VALUES (3, 7, 'Lucy')"),
      Query("INSERT INTO test_join2 VALUES (4, 7, 'Edmund')"),
      Query("INSERT INTO test_join2 VALUES (10, 4, '221B Baker Street')"),
      Query("SELECT address FROM test_join1, test_join2 WHERE test_join1.id=test_join2.id"),
      Query("SELECT test_join1.id, test_join2.id, age, books, test_join2.name FROM test_join1, test_join2 WHERE test_join1.id = test_join2.id"),
      Query("SELECT test_join1.name, age, salary, test_join2.name, books FROM test_join1, test_join2 WHERE test_join1.age=test_join2.books"),
      Query("SELECT * FROM test_join1, test_join2 WHERE test_join1.name=test_join2.name"),
      Query("SELECT * FROM test_join1, test_join2 WHERE test_join1.address=test_join2.name"),
      Query("SELECT address FROM test_join1 AS a, test_join2 WHERE a.id=test_join2.id"),
      Query("SELECT a.id, b.id, age, books, b.name FROM test_join1 a, test_join2 AS b WHERE a.id=b.id"),
      Query("SELECT test_join1.name, age, salary, b.name, books FROM test_join1, test_join2 b WHERE test_join1.age = b.books"),
            },
    { Query("DROP TABLE test_join1"),
      Query("DROP TABLE test_join2") },
    { Query("DROP TABLE test_join1"),
      Query("DROP TABLE test_join2") } );

//migrated from TestSinglePrinc TestUpdate
static QueryList Update = QueryList("SingleUpdate",
    { Query("CREATE TABLE test_update (id integer, age integer, salary integer, address text, name text)"),
      Query(""),
      Query(""),
      Query(""),
      Query("") },
    { Query("CREATE TABLE test_update (id integer, age integer, salary integer, address text, name text)"),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
    { Query("INSERT INTO test_update VALUES (1, 10, 0, 'first star to the right and straight on till morning','Peter Pan')"),
      Query("INSERT INTO test_update VALUES (2, 16, 1000, 'Green Gables', 'Anne Shirley')"),
      Query("INSERT INTO test_update VALUES (3, 8, 0, 'London', 'Lucy')"),
      Query("INSERT INTO test_update VALUES (4, 10, 0, 'London', 'Edmund')"),
      Query("INSERT INTO test_update VALUES (5, 30, 100000, '221B Baker Street', 'Sherlock Holmes')"),
      Query("INSERT INTO test_update VALUES (6, 11, 0 , 'hi', 'no one')"),
      Query("SELECT * FROM test_update"),
      Query("UPDATE test_update SET age = age, address = name"),
      Query("SELECT * FROM test_update"),
      Query("UPDATE test_update SET name = address"),
      Query("SELECT * FROM test_update"),
      Query("UPDATE test_update SET salary=0"),
      Query("SELECT * FROM test_update"),
      Query("UPDATE test_update SET age=21 WHERE id = 6"),
      Query("SELECT * FROM test_update"),
      Query("UPDATE test_update SET address='Pemberly', name='Elizabeth Darcy' WHERE id=6"),
      Query("SELECT * FROM test_update"),
      Query("UPDATE test_update SET salary=55000 WHERE age=30"),
      Query("SELECT * FROM test_update"),
      Query("UPDATE test_update SET salary=20000 WHERE address='Pemberly'"),
      Query("SELECT * FROM test_update"),
      Query("SELECT age FROM test_update WHERE age > 20"),
      Query("SELECT id FROM test_update"),
      Query("SELECT sum(age) FROM test_update"),
      Query("UPDATE test_update SET age=20 WHERE name='Elizabeth Darcy'"),
      Query("SELECT * FROM test_update WHERE age > 20"),
      Query("SELECT sum(age) FROM test_update"),
      Query("UPDATE test_update SET age = age + 2"),
      Query("SELECT age FROM test_update"),
      Query("UPDATE test_update SET id = id + 10, salary = salary + 19, name = 'xxx', address = 'foo' WHERE address = 'London'"),
      Query("SELECT * FROM test_update"),
      Query("SELECT * FROM test_update WHERE address < 'fml'"),
      Query("UPDATE test_update SET address = 'Neverland' WHERE id=1"),
      Query("SELECT * FROM test_update") },
    { Query("DROP TABLE test_update") },
    { Query("DROP TABLE test_update") } );


static QueryList HOM = QueryList("HOMAdd",
    { Query("CREATE TABLE test_HOM (id integer, age integer, salary integer, address text, name text)"),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
    { Query("CREATE TABLE test_HOM (id integer, age integer, salary integer, address text, name text)"),
      Query(""),
      Query(""),
      Query(""),
      Query("") },
    { Query("INSERT INTO test_HOM VALUES (1, 10, 0, 'first star to the right and straight on till morning','Peter Pan')"),
      Query("INSERT INTO test_HOM VALUES (2, 16, 1000, 'Green Gables', 'Anne Shirley')"),
      Query("INSERT INTO test_HOM VALUES (3, 8, 0, 'London', 'Lucy')"),
      Query("INSERT INTO test_HOM VALUES (4, 10, 0, 'London', 'Edmund')"),
      Query("INSERT INTO test_HOM VALUES (5, 30, 100000, '221B Baker Street', 'Sherlock Holmes')"),
      Query("INSERT INTO test_HOM VALUES (6, 21, 2000, 'Pemberly', 'Elizabeth')"),
      Query("INSERT INTO test_HOM VALUES (7, 10000, 1, 'Mordor', 'Sauron')"),
      Query("INSERT INTO test_HOM VALUES (8, 25, 100, 'The Heath', 'Eustacia Vye')"),
      Query("INSERT INTO test_HOM VALUES (12, NULL, NULL, 'nucwinter', 'gasmask++')"),
      Query("SELECT * FROM test_HOM"),
      Query("SELECT SUM(age) FROM test_HOM"),
      Query("SELECT * FROM test_HOM"),
      Query("SELECT SUM(age) FROM test_HOM"),
      Query("UPDATE test_HOM SET age = age + 1"),
      Query("SELECT SUM(age) FROM test_HOM"),
      Query("SELECT * FROM test_HOM"),
      Query("UPDATE test_HOM SET age = age + 3 WHERE id=1"),
      Query("SELECT * FROM test_HOM"),

      Query("UPDATE test_HOM SET age = 100 WHERE id = 1"),
      Query("SELECT * FROM test_HOM WHERE age = 100"),

      Query("SELECT COUNT(*) FROM test_HOM WHERE age > 100"),
      Query("SELECT COUNT(*) FROM test_HOM WHERE age < 100"),
      Query("SELECT COUNT(*) FROM test_HOM WHERE age <= 100"),
      Query("SELECT COUNT(*) FROM test_HOM WHERE age >= 100"),
      Query("SELECT COUNT(*) FROM test_HOM WHERE age = 100") },
    { Query("DROP TABLE test_HOM") },
    { Query("DROP TABLE test_HOM") } );

//migrated from TestDelete
static QueryList Delete = QueryList("SingleDelete",
    { Query("CREATE TABLE test_delete (id integer, age integer, salary integer, address text, name text)"),
      Query(""),
      Query(""),
      Query(""),
      Query("") },
    { Query("CREATE TABLE test_delete (id integer, age integer, salary integer, address text, name text)"),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
      // Query Fail
      //Query("CRYPTDB test_delete.age ENC"),
      //Query("CRYPTDB test_delete.salary ENC"),
      //Query("CRYPTDB test_delete.address ENC"),
      //Query("CRYPTDB test_delete.name ENC") },
    { Query("INSERT INTO test_delete VALUES (1, 10, 0, 'first star to the right and straight on till morning','Peter Pan')"),
      Query("INSERT INTO test_delete VALUES (2, 16, 1000, 'Green Gables', 'Anne Shirley')"),
      Query("INSERT INTO test_delete VALUES (3, 8, 0, 'London', 'Lucy')"),
      Query("INSERT INTO test_delete VALUES (4, 10, 0, 'London', 'Edmund')"),
      Query("INSERT INTO test_delete VALUES (5, 30, 100000, '221B Baker Street', 'Sherlock Holmes')"),
      Query("INSERT INTO test_delete VALUES (6, 21, 2000, 'Pemberly', 'Elizabeth')"),
      Query("INSERT INTO test_delete VALUES (7, 10000, 1, 'Mordor', 'Sauron')"),
      Query("INSERT INTO test_delete VALUES (8, 25, 100, 'The Heath', 'Eustacia Vye')"),
      Query("DELETE FROM test_delete WHERE id=1"),
      Query("SELECT * FROM test_delete"),
      Query("DELETE FROM test_delete WHERE age=30"),
      Query("SELECT * FROM test_delete"),
      Query("DELETE FROM test_delete WHERE name='Eustacia Vye'"),
      Query("SELECT * FROM test_delete"),
      Query("DELETE FROM test_delete WHERE address='London'"),
      Query("SELECT * FROM test_delete"),
      Query("DELETE FROM test_delete WHERE salary = 1"),
      Query("SELECT * FROM test_delete"),
      Query("INSERT INTO test_delete VALUES (1, 10, 0, 'first star to the right and straight on till morning','Peter Pan')"),
      Query("SELECT * FROM test_delete"),
      Query("DELETE FROM test_delete"),
      Query("SELECT * FROM test_delete") },
    { Query("DROP TABLE test_delete") },
    { Query("DROP TABLE test_delete") } );

/*
//migrated from TestSearch
static QueryList Search = QueryList("SingleSearch",
    { Query("CREATE TABLE test_search (id integer, searchable text)"),
      Query("") },
    { Query("CREATE TABLE test_search (id integer, searchable text)"),
      Query("") },
    // Query Fail
    //Query("CRYPTDB test_search.seachable ENC") },
    { Query("CREATE TABLE test_search (id integer, searchable text)"),
      Query("") },
    { Query("INSERT INTO test_search VALUES (1, 'short text')"),
      Query("INSERT INTO test_search VALUES (2, 'Text with CAPITALIZATION')"),
      Query("INSERT INTO test_search VALUES (3, '')"),
      Query("INSERT INTO test_search VALUES (4, 'When I have fears that I may cease to be, before my pen has gleaned my teeming brain; before high piled books in charactery hold like ruch garners the full-ripened grain. When I behold on the nights starred face huge cloudy symbols of high romance and think that I may never live to trace their shadows with the magic hand of chance; when I feel fair creature of the hour that I shall never look upon thee more, never have relish of the faerie power of unreflecting love, I stand alone on the edge of the wide world and think till love and fame to nothingness do sink')"),
      Query("SELECT * FROM test_search WHERE searchable LIKE '%text%'"),
      Query("SELECT * FROM test_search WHERE searchable LIKE 'short%'"),
      Query("SELECT * FROM test_search WHERE searchable LIKE ''"),
      Query("SELECT * FROM test_search WHERE searchable LIKE '%capitalization'"),
      Query("SELECT * FROM test_search WHERE searchable LIKE 'noword'"),
      Query("SELECT * FROM test_search WHERE searchable LIKE 'when%'"),
      Query("SELECT * FROM test_search WHERE searchable < 'slow'"),
      Query("UPDATE test_search SET searchable='text that is new' WHERE id=1"),
      Query("SELECT * FROM test_search WHERE searchable < 'slow'") },
    { Query("DROP TABLE test_search") },
    { Query("DROP TABLE test_search") },
    { Query("DROP TABLE test_search") } );
*/

static QueryList Basic = QueryList("MultiBasic",
    { Query(""),
      Query(""),
      Query("CREATE TABLE t1 (id integer, post text, age bigint)"),
      Query(""),
      Query(""),
      Query("CREATE TABLE u_basic (id integer, username text)"),
      Query("CREATE TABLE "+PWD_TABLE_PREFIX+"u_basic (username text, psswd text)") },
    { Query(""),
      Query(""),
      Query("CREATE TABLE t1 (id integer, post text, age bigint)"),
      Query(""),
      Query(""),
      Query("CREATE TABLE u_basic (id integer, username text)"),
      Query("CREATE TABLE "+PWD_TABLE_PREFIX+"u_basic (username text, psswd text)") },
    {     
        //Query Fail
      //Query("INSERT INTO "+PWD_TABLE_PREFIX+"u_basic (username, psswd) VALUES ('alice', 'secretalice')"),*/
      //Query("DELETE FROM "+PWD_TABLE_PREFIX+"u_basic WHERE username='alice'"),
      //Query("INSERT INTO "+PWD_TABLE_PREFIX+"u_basic (username, psswd) VALUES ('alice', 'secretalice')"),

      Query("INSERT INTO u_basic VALUES (1, 'alice')"),
      Query("SELECT * FROM u_basic"),
      Query("INSERT INTO t1 VALUES (1, 'text which is inserted', 23)"),
      Query("SELECT * FROM t1"),
      Query("SELECT post from t1 WHERE id = 1 AND age = 23"),
      Query("UPDATE t1 SET post='hello!' WHERE age > 22 AND id =1"),
      Query("SELECT * FROM t1"),
      
      // Query Fail
      //Query("INSERT INTO "+PWD_TABLE_PREFIX+"u_basic (username, psswd) VALUES ('raluca','secretraluca')"),
      
      Query("INSERT INTO u_basic VALUES (2, 'raluca')"),
      Query("SELECT * FROM u_basic"),
      Query("INSERT INTO t1 VALUES (2, 'raluca has text here', 5)"),
      Query("SELECT * FROM t1") },
    { Query("DROP TABLE u_basic"),
      Query("DROP TABLE t1"),
      Query("DROP TABLE "+PWD_TABLE_PREFIX+"u_basic") },
    { Query("DROP TABLE u_basic"),
      Query("DROP TABLE t1"),
      Query("DROP TABLE "+PWD_TABLE_PREFIX+"u_basic") });

//migrated from PrivMessages
static QueryList PrivMessages = QueryList("MultiPrivMessages",
    { Query("CREATE TABLE msgs (msgid integer, msgtext text)"),
      Query("CREATE TABLE privmsg (msgid integer, recid integer, senderid integer)"),
      Query("CREATE TABLE u_mess (userid integer, username text)"),
      Query("CREATE TABLE "+PWD_TABLE_PREFIX+"u_mess (username text, psswd text)"),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
    { Query("CREATE TABLE msgs (msgid integer, msgtext text)"),
      Query("CREATE TABLE privmsg (msgid integer, recid integer, senderid integer)"),
      Query("CREATE TABLE u_mess (userid integer, username text)"),
      Query("CREATE TABLE "+PWD_TABLE_PREFIX+"u_mess (username text, psswd text)"),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
    {     
    // Query Fail
    // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u_mess (username, psswd) VALUES ('alice', 'secretalice')"),

    // Query Fail
    // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u_mess (username, psswd) VALUES ('bob', 'secretbob')"),
      Query("INSERT INTO u_mess VALUES (1, 'alice')"),
      Query("INSERT INTO u_mess VALUES (2, 'bob')"),
      Query("INSERT INTO privmsg (msgid, recid, senderid) VALUES (9, 1, 2)"),
      Query("INSERT INTO msgs VALUES (1, 'hello world')"),
      Query("SELECT msgtext FROM msgs WHERE msgid=1"),
      // Why broken?
      // Query("SELECT msgtext FROM msgs, privmsg, u_mess WHERE username = 'alice' AND userid = recid AND msgs.msgid = privmsg.msgid"),
      Query("INSERT INTO msgs VALUES (9, 'message for alice from bob')"),
      // Why broken?
      // Query("SELECT msgtext FROM msgs, privmsg, u_mess WHERE username = 'alice' AND userid = recid AND msgs.msgid = privmsg.msgid")
    },
    { Query("DROP TABLE msgs"),
      Query("DROP TABLE privmsg"),
      Query("DROP TABLE u_mess"),
      Query("DROP TABLE "+PWD_TABLE_PREFIX+"u_mess") },
    { Query("DROP TABLE msgs"),
      Query("DROP TABLE privmsg"),
      Query("DROP TABLE u_mess"),
      Query("DROP TABLE "+PWD_TABLE_PREFIX+"u_mess") });

//migrated from UserGroupForum
static QueryList UserGroupForum = QueryList("UserGroupForum",
    { Query("CREATE TABLE u (userid integer, username text)"),
      Query("CREATE TABLE usergroup (userid integer, groupid integer)"),
      Query("CREATE TABLE groupforum (forumid integer, groupid integer, optionid integer)"),
      Query("CREATE TABLE forum (forumid integer, forumtext text)"),
      Query("CREATE TABLE "+PWD_TABLE_PREFIX+"u (username text, psswd text)"),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
    { Query("CREATE TABLE u (userid integer, username text)"),
      Query("CREATE TABLE usergroup (userid integer, groupid integer)"),
      Query("CREATE TABLE groupforum (forumid integer, groupid integer, optionid integer)"),
      Query("CREATE TABLE forum (forumid integer, forumtext text)"),
      Query("CREATE TABLE "+PWD_TABLE_PREFIX+"u (username text, psswd text)"),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
    { 
      // Query Fail
      // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u (username, psswd) VALUES ('alice', 'secretalice')"),
      // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u (username, psswd) VALUES ('bob', 'secretbob')"),
      // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u (username, psswd) VALUES ('chris', 'secretchris')"),

      // Alice, Bob, Chris all logged on

      Query("INSERT INTO u VALUES (1, 'alice')"),
      Query("INSERT INTO u VALUES (2, 'bob')"),
      Query("INSERT INTO u VALUES (3, 'chris')"),

      Query("INSERT INTO usergroup VALUES (1,1)"),
      Query("INSERT INTO usergroup VALUES (2,2)"),
      Query("INSERT INTO usergroup VALUES (3,1)"),
      Query("INSERT INTO usergroup VALUES (3,2)"),

      //Alice is in group 1, Bob in group 2, Chris in group 1 & group 2

      Query("SELECT * FROM usergroup"),
      Query("INSERT INTO groupforum VALUES (1,1,14)"),
      Query("INSERT INTO groupforum VALUES (1,1,20)"),

      //Group 1 has access to forum 1

      Query("SELECT * FROM groupforum"),
      Query("INSERT INTO forum VALUES (1, 'success-- you can see forum text')"),
      
      // Query Fail
      // Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='alice'"),
      // Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='bob'"),
      // Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='chris'"),

      // All users logged off at this point

      // alice
      
      // Query Fail
      // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u (username, psswd) VALUES ('alice', 'secretalice')"),
      // only Alice logged in and she should see forum 1
      Query("SELECT forumtext FROM forum WHERE forumid=1"),
      
      // Query Fail
      // Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='alice'"),


      // bob

      // Query Fail
      // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u (username, psswd) VALUES ('bob', 'secretbob')"),
      // only Bob logged in and he should not see forum 1
      Query("SELECT forumtext FROM forum WHERE forumid=1"),
      
      // Query Fail
      // Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='bob'"),


      // chris
      
      // Query Fail
      // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u (username, psswd) VALUES ('chris', 'secretchris')"),

      // only Chris logged in and he should see forum 1
      Query("SELECT forumtext FROM forum WHERE forumid=1"),
      // change forum text while Chris logged in
      Query("UPDATE forum SET forumtext='you win!' WHERE forumid=1"),
      Query("SELECT forumtext FROM forum WHERE forumid=1"),
      
      // Query Fail
      // Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='chris'"),


      // alice

      // Query Fail
      // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u (username, psswd) VALUES ('alice','secretalice')"),

      // only Alice logged in and she should see new text in forum 1
      Query("SELECT forumtext FROM forum WHERE forumid=1"),
      // create an orphaned forum
      Query("INSERT INTO forum VALUES (2, 'orphaned text! everyone should be able to see me')"),
      // only Alice logged in and she should see text in orphaned forum 2
      Query("SELECT forumtext FROM forum WHERE forumid=2"),
      
      // Query Fail
      // Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='alice'"),


      // bob

      // Query Fail
      // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u (username, psswd) VALUES ('bob', 'secretbob')"),
      // only Bob logged in and he should see text in orphaned forum 2
      Query("SELECT forumtext FROM forum WHERE forumid=2"),
      
      // Query Fail
      // Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='bob'"),


      // chris
      
      // Query Fail
      // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u (username, psswd) VALUES ('chris','secretchris')"),
      
      // only Chris logged in and he should see text in orphaned forum 2
      Query("SELECT forumtext FROM forum WHERE forumid=2"),
      // de-orphanize forum 2 -- now only accessible by group 2
      Query("INSERT INTO groupforum VALUES (2,2,20)"),
      // only Chris logged in and he should see text in both forum 1 and forum 2
      // Query("SELECT forumtext FROM forum AS f, groupforum AS g, usergroup AS ug, u WHERE f.forumid=g.forumid AND g.groupid=ug.groupid AND ug.userid=u.userid AND u.username='chris' AND g.optionid=20"),
      
      // Query Fail
      // Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='chris'"),


      // bob

      // Query Fail
      // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u (username, psswd) VALUES ('bob','secretbob')"),

      // only Bob logged in and he should see text in forum 2
      // Query("SELECT forumtext FROM forum AS f, groupforum AS g, usergroup AS ug, u WHERE f.forumid=g.forumid AND g.groupid=ug.groupid AND ug.userid=u.userid AND u.username='bob' AND g.optionid=20"),
      
      // Query Fail
      // Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='bob'"),


      // alice

      // Query Fail
      // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u (username, psswd) VALUES ('alice','secretalice')"),

      // only Alice logged in and she should see text in forum 1
      // Query("SELECT forumtext FROM forum AS f, groupforum AS g, usergroup AS ug, u WHERE f.forumid=g.forumid AND g.groupid=ug.groupid AND ug.userid=u.userid AND u.username='alice' AND g.optionid=20"),
      
      // Query Fail
      // Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='alice'"),

      // all logged out at this point

      // give group 2 access to forum 1 with the wrong access IDs -- since the forum will be inaccessible to group 2, doesn't matter that no one is logged in
      Query("INSERT INTO groupforum VALUES (1,2,2)"),
      Query("INSERT INTO groupforum VALUES (1,2,0)"),
      // attempt to gice group 2 actual access to the forum -- should fail, because no one is logged in
      Query("INSERT INTO groupforum VALUES (1,2,20)"),


      // bob

      // Query Fail
      // Query("INSERT INTO "+PWD_TABLE_PREFIX+"u (username, psswd) VALUES ('bob', 'secretbob')"),
      // only Bob logged in and he should still not have access to forum 1
      Query("SELECT forumtext FROM forum WHERE forumid=1")
      
    },
      // Query Fail
      // Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='bob'")},
    { Query("DROP TABLE u"),
      Query("DROP TABLE usergroup"),
      Query("DROP TABLE groupforum"),
      Query("DROP TABLE forum"),
      Query("DROP TABLE "+PWD_TABLE_PREFIX+"u") },
    { Query("DROP TABLE u"),
      Query("DROP TABLE usergroup"),
      Query("DROP TABLE groupforum"),
      Query("DROP TABLE forum"),
      Query("DROP TABLE "+PWD_TABLE_PREFIX+"u") });

static QueryList Auto = QueryList("AutoInc",
    { Query("CREATE TABLE msgs (msgid integer PRIMARY KEY AUTO_INCREMENT, zooanimals integer, msgtext text)"),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
    { Query("CREATE TABLE msgs (msgid integer PRIMARY KEY AUTO_INCREMENT, zooanimals integer, msgtext text)"),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
    { Query("INSERT INTO msgs (msgtext, zooanimals) VALUES ('hello world', 100)"),
      Query("INSERT INTO msgs (msgtext, zooanimals) VALUES ('hello world2', 21)"),
      Query("INSERT INTO msgs (msgtext, zooanimals) VALUES ('hello world3', 10909)"),
      Query("SELECT msgtext FROM msgs WHERE msgid=1"),
      Query("SELECT msgtext FROM msgs WHERE msgid=2"),
      Query("SELECT msgtext FROM msgs WHERE msgid=3"),
      Query("INSERT INTO msgs VALUES (3, 2012, 'sandman') ON DUPLICATE KEY UPDATE zooanimals = VALUES(zooanimals), zooanimals = 22"),
      Query("SELECT * FROM msgs"),
      Query("SELECT SUM(zooanimals) FROM msgs"),
      Query("INSERT INTO msgs VALUES (3, 777, 'golfpants') ON DUPLICATE KEY UPDATE zooanimals = 16, zooanimals = VALUES(zooanimals)"),
      Query("SELECT * FROM msgs"),
      Query("SELECT SUM(zooanimals) FROM msgs"),
      Query("INSERT INTO msgs VALUES (9, 105, 'message for alice from bob')"),
      Query("INSERT INTO msgs VALUES (9, 201, 'whatever') ON DUPLICATE KEY UPDATE msgid = msgid + 10"),
      Query("SELECT * FROM msgs"),
      // Query("INSERT INTO msgs VALUES (1, 9001, 'lights are on') ON DUPLICATE KEY UPDATE msgid = zooanimals + 99, zooanimals=VALUES(zooanimals)"),
      Query("SELECT * FROM msgs"),
      Query("INSERT INTO msgs VALUES (2, 1998, 'stacksondeck') ON DUPLICATE KEY UPDATE zooanimals = VALUES(zooanimals), msgtext = VALUES(msgtext)"),
      Query("SELECT * FROM msgs"),
      Query("SELECT SUM(zooanimals) FROM msgs"),
      },
    { Query("DROP TABLE msgs")},
    { Query("DROP TABLE msgs")});

/*
 * Add additional tests once functional.
 * > HOM
 * > OPE
 */
static QueryList Negative = QueryList("Negative",
    { Query("CREATE TABLE negs (a integer, b integer, c integer)"),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
    { Query("CREATE TABLE negs (a integer, b integer, c integer)"),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
    { Query("INSERT INTO negs (a, b, c) VALUES (10, -20, -100)"),
      Query("INSERT INTO negs (a, b, c) VALUES (-100, 50, -12)"),
      Query("INSERT INTO negs (a, b, c) VALUES (-8, -50, -18)"),
      Query("SELECT a FROM negs WHERE b = -50 OR b = 50"),
      Query("SELECT a FROM negs WHERE c = -100 OR b = -20"),
      // Query("SELECT a FROM negs WHERE -c = 100"),
      Query("INSERT INTO negs (c) VALUES (-1009)"),
      Query("INSERT INTO negs (c) VALUES (1009)"),
      Query("SELECT * FROM negs WHERE c = -1009")},
    { Query("DROP TABLE negs")},
    { Query("DROP TABLE negs")});

static QueryList Null = QueryList("Null",
    { Query("CREATE TABLE test_null (uid integer, age integer, address text)"),
      Query("CREATE TABLE u_null (uid integer, username text)"),
      Query("CREATE TABLE "+PWD_TABLE_PREFIX+"u_null (username text, password text)"),
      Query(""),
      Query("") },
    { Query("CREATE TABLE test_null (uid integer, age integer, address text)"),
      Query(""),
      Query(""),
      // Query Fail
      //Query("CRYPTDB test_null.age ENC"),
      //Query("CRYPTDB test_null.address ENC"),
      Query("CREATE TABLE u_null (uid integer, username text)"),
      Query("CREATE TABLE " + PWD_TABLE_PREFIX + "u_null (username text, password text)")},
    //can only handle NULL's on non-principal fields
      { 
        // Query Fail
        //Query("INSERT INTO "+PWD_TABLE_PREFIX+"u_null (username, password) VALUES ('alice', 'secretA')"),
        Query("INSERT INTO u_null VALUES (1, 'alice')"),
        Query("INSERT INTO u_null VALUES (), ()"),
        Query("INSERT INTO u_null VALUES (2, 'somewhere'), (3, 'cookies')"),

        Query("INSERT INTO test_null (uid, age) VALUES (1, 20)"),
        Query("SELECT * FROM test_null"),
        Query("INSERT INTO test_null (uid, address) VALUES (1, 'somewhere over the rainbow')"),
        Query("INSERT INTO test_null () VALUES (), ()"),
        Query("INSERT INTO test_null VALUES (), ()"),
        Query("INSERT INTO test_null (address, age) VALUES ('cookies', 1)"),
        Query("INSERT INTO test_null (age, address) VALUES (1, 'two')"),
        Query("INSERT INTO test_null (address, uid) VALUES ('three', 4)"),
        Query("SELECT * FROM test_null"),
        Query("INSERT INTO test_null (uid, age) VALUES (1, NULL)"),
        Query("SELECT * FROM test_null"),
        Query("INSERT INTO test_null (uid, address) VALUES (1, NULL)"),
        Query("SELECT * FROM test_null"),
        Query("INSERT INTO test_null VALUES (1, 25, 'Australia')"),
        Query("SELECT * FROM test_null"),
        Query("SELECT * FROM test_null WHERE uid = 1"),
        Query("SELECT * FROM test_null WHERE age < 50"),
        Query("SELECT SUM(uid) FROM test_null"),
        Query("SELECT MAX(uid) FROM test_null"),
        Query("SELECT * FROM test_null"),
        Query("SELECT * FROM test_null WHERE address = 'cookies'"),
        Query("SELECT * FROM test_null WHERE address < 'amber'"),
        // Query("SELECT * FROM test_null WHERE address LIKE 'aaron'"),
        Query("SELECT * FROM test_null LEFT JOIN u_null"
              "    ON test_null.uid = u_null.uid"),
        Query("SELECT * FROM test_null, u_null"),
        Query("SELECT * FROM test_null RIGHT JOIN u_null"
              "    ON test_null.uid = u_null.uid"),
        Query("SELECT * FROM test_null, u_null"),
        Query("SELECT * FROM test_null RIGHT JOIN u_null"
              "    ON test_null.address = u_null.username"),
        Query("SELECT * FROM test_null, u_null"),
        Query("SELECT * FROM test_null LEFT JOIN u_null"
              "    ON u_null.username = test_null.address"),
        Query("SELECT * FROM test_null, u_null")},
    { Query("DROP TABLE test_null"),
      Query("DROP TABLE u_null"),
      Query("DROP TABLE "+PWD_TABLE_PREFIX+"u_null") },
    { Query("DROP TABLE test_null"),
      Query("DROP TABLE u_null"),
      Query("DROP TABLE "+PWD_TABLE_PREFIX+"u_null") });

static QueryList ManyConnections = QueryList("Multiple connections",
    { Query("CREATE TABLE msgs (msgid integer PRIMARY KEY AUTO_INCREMENT, msgtext text)"),
      Query("CREATE TABLE privmsg (msgid integer, recid integer, senderid integer)"),
      Query("CREATE TABLE forum (forumid integer AUTO_INCREMENT PRIMARY KEY, title text)"),
      Query("CREATE TABLE post (postid integer AUTO_INCREMENT PRIMARY KEY, forumid integer, posttext text, author integer)"),
      Query("CREATE TABLE u_conn (userid integer, username text)"),
      Query("CREATE TABLE "+PWD_TABLE_PREFIX+"u_conn (username text, psswd text)"),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
    { Query("CREATE TABLE msgs (msgid integer PRIMARY KEY AUTO_INCREMENT, msgtext text)"),
      Query("CRYPTDB msgs.msgtext ENC"),
      Query("CREATE TABLE privmsg (msgid integer, recid integer, senderid integer)"),
      Query("CRYPTDB privmsg.recid ENC"),
      Query("CRYPTDB privmsg.senderid ENC"),
      Query("CREATE TABLE forum (forumid integer AUTO_INCREMENT PRIMARY KEY, title text)"),
      Query("CRYPTDB forum.title ENC"),
      Query("CREATE TABLE post (postid integer AUTO_INCREMENT PRIMARY KEY, forumid integer, posttext text, author integer)"),
      Query("CRYPTDB post.posttext ENC"),
      Query("CREATE TABLE u_conn (userid enc integer, username enc text)"),
      Query("CRYPTDB u_conn.userid ENC"),
      Query("CRYPTDB u_conn.username ENC"),
      Query("CREATE TABLE "+PWD_TABLE_PREFIX+"u_conn (username text, psswd text)"),
      Query(""),
      Query(""),
      Query("")},
    { Query("INSERT INTO "+PWD_TABLE_PREFIX+"u_conn (username, psswd) VALUES ('alice','secretA')"),
      Query("INSERT INTO "+PWD_TABLE_PREFIX+"u_conn (username, psswd) VALUES ('bob','secretB')"),
      Query("INSERT INTO u_conn VALUES (1, 'alice')"),
      Query("INSERT INTO u_conn VALUES (2, 'bob')"),
      Query("INSERT INTO privmsg (msgid, recid, senderid) VALUES (9, 1, 2)"),
      Query("SELECT LAST_INSERT_ID()"),
      Query("INSERT INTO forum (title) VALUES ('my first forum')"),
      Query("SELECT LAST_INSERT_ID()"),
      Query("INSERT INTO forum (title) VALUES ('my first forum')"),
      Query("SELECT LAST_INSERT_ID()"),
      Query("INSERT INTO forum VALUES (11, 'testtest')"),
      Query("INSERT INTO post (forumid, posttext, author) VALUES (1,'first post in first forum!', 1)"),
      Query("SELECT LAST_INSERT_ID()"),
      Query("INSERT INTO msgs (msgtext) VALUES ('hello world')"),
      Query("SELECT LAST_INSERT_ID()"),
      Query("INSERT INTO forum (title) VALUES ('two fish')"),
      Query("INSERT INTO post (forumid, posttext, author) VALUES (12,'red fish',2)"),
      Query("INSERT INTO post (forumid, posttext, author) VALUES (12,'blue fish',1)"),
      Query("SELECT LAST_INSERT_ID()"),
      Query("INSERT INTO msgs (msgtext) VALUES ('hello world2')"),
      Query("SELECT LAST_INSERT_ID()"),
      Query("INSERT INTO post (forumid, posttext, author) VALUES (12,'black fish, blue fish',1)"),
      Query("INSERT INTO post (forumid, posttext, author) VALUES (12,'old fish, new fish',2)"),
      Query("SELECT LAST_INSERT_ID()"),
      Query("INSERT INTO msgs (msgtext) VALUES ('hello world3')"),
      Query("SELECT LAST_INSERT_ID()"),
      Query("SELECT msgtext FROM msgs WHERE msgid=1"),
      Query("SELECT * FROM forum"),
      Query("SELECT msgtext FROM msgs WHERE msgid=2"),
      Query("SELECT msgtext FROM msgs WHERE msgid=3"),
      Query("SELECT post.* FROM post, forum WHERE post.forumid = forum.forumid AND forum.title = 'two fish'"),
      Query("SELECT msgtext FROM msgs, privmsg, u_conn WHERE username = 'alice' AND userid = recid AND msgs.msgid = privmsg.msgid"),
      Query("INSERT INTO msgs VALUES (9, 'message for alice from bob')"),
            //Query("SELECT LAST_INSERT_ID()"),
      Query("SELECT msgtext FROM msgs, privmsg, u_conn WHERE username = 'alice' AND userid = recid AND msgs.msgid = privmsg.msgid") },
    { Query("DROP TABLE msgs"),
      Query("DROP TABLE privmsg"),
      Query("DROP TABLE forum"),
      Query("DROP TABLE post"),
      Query("DROP TABLE u_conn"),
      Query("DROP TABLE "+PWD_TABLE_PREFIX+"u_conn") },
    { Query("DROP TABLE msgs"),
      Query("DROP TABLE privmsg"),
      Query("DROP TABLE forum"),
      Query("DROP TABLE post"),
      Query("DROP TABLE u_conn"),
      Query("DROP TABLE "+PWD_TABLE_PREFIX+"u_conn") });

static QueryList BestEffort = QueryList("BestEffort",
    { Query("CREATE TABLE t (x integer, y integer)"),
      Query(""),
      Query(""),
      Query("")},
    { Query("CREATE TABLE t (x integer, y integer)"),
      Query(""),
      Query(""),
      Query("")},
    { Query("INSERT INTO t VALUES (1, 100)"),
      Query("INSERT INTO t VALUES (22, 413)"),
      Query("INSERT INTO t VALUES (1001, 15)"),
      Query("INSERT INTO t VALUES (19, 18)"),
      Query("SELECT * FROM t"),
      Query("SELECT x*x FROM t"),
      Query("SELECT x*y FROM t"),
      Query("SELECT 2+2 FROM t"),
      Query("SELECT x+2+x FROM t"),
      Query("SELECT 2+x+2 FROM t"),
      // Query("SELECT 2+2+x FROM t"),
      Query("SELECT x+y+3+4 FROM t"),
      Query("SELECT 2*x*2*y FROM t"),
      Query("SELECT x, y FROM t WHERE x AND y"), 
      Query("SELECT x, y FROM t WHERE x = 1 AND y < 200"), 
      Query("SELECT x, y FROM t WHERE x AND y = 15"), 
      Query("SELECT 10, x+y FROM t WHERE x") },
    { Query("DROP TABLE t")},
    { Query("DROP TABLE t")});

// We do not support queries like this.
// > INSERT INTO t () VALUES ();
// > INSERT INTO t VALUES (DEFAULT);
// > INSERT INTO t VALUES (DEFAULT(x));
static QueryList DefaultValue = QueryList("DefaultValue",
    { Query("CREATE TABLE def_0 (x INTEGER NOT NULL DEFAULT 10,"
      "                    y VARCHAR(100) NOT NULL DEFAULT 'purpflowers',"
      "                    z INTEGER)"),
      Query("CREATE TABLE def_1 (a INTEGER NOT NULL DEFAULT '100',"
      "                    b INTEGER,"
      "                    c VARCHAR(100))"),
      Query(""),
      Query("")},
    { Query("CREATE TABLE def_0 (x INTEGER NOT NULL DEFAULT 10,"
      "                    y VARCHAR(100) NOT NULL DEFAULT 'purpflowers',"
      "                    z INTEGER)"),
      Query("CREATE TABLE def_1 (a INTEGER NOT NULL DEFAULT '100',"
      "                    b INTEGER,"
      "                    c VARCHAR(100))"),
      Query(""),
      Query("")},
    { Query("INSERT INTO def_0 VALUES (100, 'singsongs', 500),"
            "                         (220, 'heyfriend', 15)"),
      Query("INSERT INTO def_0 (z) VALUES (500), (220), (32)"),
      Query("INSERT INTO def_0 (z, x) VALUES (500, '500')"),
      Query("INSERT INTO def_0 (z) VALUES (100)"),
      Query("INSERT INTO def_1 VALUES (250, 10000, 'smile!')"),
      Query("INSERT INTO def_1 (a, b, c) VALUES (250, 100, '!')"),
      Query("INSERT INTO def_1 (b, c) VALUES (250, 'smile!'),"
            "                                (99, 'happybday!')"),
      Query("SELECT * FROM def_0, def_1"),
      Query("SELECT * FROM def_0 WHERE x = 10"),
      Query("INSERT INTO def_0 (z) VALUES (500), (12), (19)"),
      Query("SELECT * FROM def_0 WHERE x = 10"),
      Query("SELECT * FROM def_0 WHERE y = 'purpflowers'"),
      Query("INSERT INTO def_0 (z) VALUES (450), (200), (300)"),
      Query("SELECT * FROM def_0 WHERE y = 'purpflowers'"),
      Query("SELECT * FROM def_0, def_1")},
    { Query("DROP TABLE def_0"),
      Query("DROP TABLE def_1")},
    { Query("DROP TABLE def_0"),
      Query("DROP TABLE def_1")});

static QueryList Decimal = QueryList("Decimal",
    { Query("CREATE TABLE dec_0 (x DECIMAL(10, 5),"
      "                    y DECIMAL(10, 5) NOT NULL DEFAULT 12.125)"),
      Query("CREATE TABLE dec_1 (a INTEGER, b DECIMAL(4, 2))"),
      Query(""),
      Query("")},
    { Query("CREATE TABLE dec_0 (x DECIMAL(10, 5),"
      "                    y DECIMAL(10, 5) NOT NULL DEFAULT 12.125)"),
      Query("CREATE TABLE dec_1 (a INTEGER, b DECIMAL(4, 2))"),
      Query(""),
      Query("")},
    { Query("INSERT INTO dec_0 VALUES (50, 100.5)"),
      Query("INSERT INTO dec_0 VALUES (20.50, 1000.5)"),
      Query("INSERT INTO dec_0 VALUES (50, 100.59)"),
      Query("INSERT INTO dec_0 (x) VALUES (1.1)"),
      Query("INSERT INTO dec_1 VALUES (8, 1000.5)"),
      // Query("INSERT INTO dec_1 VALUES (118, -49.2)"),
      // Query("INSERT INTO dec_1 VALUES (5, -49.2)"),
      Query("SELECT * FROM dec_0 WHERE x = 50"),
      Query("SELECT * FROM dec_0 WHERE x < 50"),
      Query("SELECT * FROM dec_0 WHERE y = 100.5"),
      Query("SELECT * FROM dec_0"),
      Query("INSERT INTO dec_0 VALUES (19, 100.5)"),
      Query("SELECT * FROM dec_0 WHERE y = 100.5"),
      // Query("SELECT * FROM dec_1 WHERE a = 5 AND b = -49.2"),
      Query("SELECT * FROM dec_1"),
      Query("SELECT * FROM dec_0, dec_1 WHERE dec_0.y = dec_1.b")},
    { Query("DROP TABLE dec_0"),
      Query("DROP TABLE dec_1")},
    { Query("DROP TABLE dec_0"),
      Query("DROP TABLE dec_1")});

static QueryList NonStrictMode = QueryList("NonStrictMode",
    { Query("CREATE TABLE not_strict (x INTEGER NOT NULL,"
      "                         y INTEGER NOT NULL DEFAULT 100,"
      "                         z VARCHAR(100) NOT NULL)"),
      Query(""),
      Query(""),
      Query("")},
    { Query("CREATE TABLE not_strict (x INTEGER NOT NULL,"
      "                         y INTEGER NOT NULL DEFAULT 100,"
      "                         z VARCHAR(100) NOT NULL)"),
      Query(""),
      Query(""),
      Query("")},
    { Query("INSERT INTO not_strict VALUES (150, 230, 'flowers')"),
      Query("INSERT INTO not_strict VALUES (850, 930, 'rainbow')"),
      Query("INSERT INTO not_strict (y) VALUES (11930)"),
      Query("SELECT * FROM not_strict WHERE x = 0"),
      Query("SELECT * FROM not_strict WHERE z = ''"),
      Query("INSERT INTO not_strict (y) VALUES (1212)"),
      Query("SELECT * FROM not_strict WHERE x = 0"),
      Query("SELECT * FROM not_strict WHERE z = ''"),
      Query("INSERT INTO not_strict (x) VALUES (0)"),
      Query("INSERT INTO not_strict (x, z) VALUES (12001, 'sun')"),
      Query("INSERT INTO not_strict (z) VALUES ('curtlanguage')"),
      Query("SELECT * FROM not_strict WHERE x = 0"),
      Query("SELECT * FROM not_strict WHERE z = ''"),
      Query("SELECT * FROM not_strict WHERE x < 110"),
      Query("SELECT * FROM not_strict")},
    { Query("DROP TABLE not_strict")},
    { Query("DROP TABLE not_strict")});

static QueryList Transactions = QueryList("Transactions",
    { Query("CREATE TABLE trans (a integer, b integer, c integer)ENGINE=InnoDB"),
      Query(""),
      Query(""), 
      Query(""),
      Query("")},
    { Query("CREATE TABLE trans (a integer, b integer, c integer)ENGINE=InnoDB"),
      Query(""),
      Query(""),
      Query(""),
      Query("")},
    { Query("INSERT INTO trans VALUES (1, 2, 3)"),
      Query("INSERT INTO trans VALUES (33, 22, 11)"),
      Query("SELECT * FROM trans"),
      Query("START TRANSACTION"),
      Query("INSERT INTO trans VALUES (333, 222, 111)"),
      Query("ROLLBACK"),
      Query("SELECT * FROM trans"),
      Query("INSERT INTO trans VALUES (45, 22, 15)"),
      Query("UPDATE trans SET a = a + 1, b = c + 1"),
      Query("START TRANSACTION"),
      Query("UPDATE trans SET a = a + a, b = b + 12"),
      Query("SELECT * FROM trans"),
      Query("ROLLBACK"),
      Query("SELECT * FROM trans"),
      Query("START TRANSACTION"),
      Query("UPDATE trans SET a = c + 1, b = a + 1"),
      Query("COMMIT"),
      Query("ROLLBACK"),
      Query("SELECT * FROM trans"),

      Query("START TRANSACTION"),
      Query("UPDATE trans SET a = a + 1, c = 50 WHERE a < 50000"),
      // commit required for control database
      Query("COMMIT"),
      Query("SELECT * FROM trans"),

      Query("START TRANSACTION"),
      Query("INSERT INTO trans VALUES (1, 50, 150)"),
      Query("UPDATE trans SET b = b + 10 WHERE c = 50"),
      // commit required for control database.
      Query("COMMIT"),
      Query("SELECT * FROM trans")},
    { Query("DROP TABLE trans")},
    { Query("DROP TABLE trans")});

static QueryList TableAliases = QueryList("TableAliases",
    { Query("CREATE TABLE star (a integer, b integer, c integer)"),
      Query("CREATE TABLE mercury (a integer, b integer, c integer)"),
      Query("CREATE TABLE moon (x integer, y integer, z integer)"),
      Query("")},
    { Query("CREATE TABLE star (a integer, b integer, c integer)"),
      Query("CREATE TABLE mercury (a integer, b integer, c integer)"),
      Query("CREATE TABLE moon (x integer, y integer, z integer)"),
      Query("")},
    { Query("INSERT INTO star VALUES (55, 66, 77), (99, 22, 109)"),
      Query("INSERT INTO mercury VALUES (55, 18, 17), (16, 15, 14)"),
      Query("INSERT INTO moon VALUES (55, 18, 1), (22, 22, 444)"),
      Query("SELECT s.a, e.b FROM star AS s INNER JOIN mercury AS e"
            "                  ON s.a = e.a"
            "               WHERE s.c < 100"),
      Query("SELECT * FROM mercury INNER JOIN mercury AS e"
            "           ON mercury.a = e.a"),
      Query("SELECT o.x, o.y FROM moon AS o INNER JOIN moon AS o2"
            "                  ON o.x = o2.y"),
      Query("SELECT mercury.a, mercury.b, e.a FROM star AS mercury"
            " INNER JOIN mercury AS e ON (mercury.a = e.a)"
            " WHERE mercury.b <> 18 AND mercury.b <> 15")
    },
    { Query("DROP TABLE star"),
      Query("DROP TABLE mercury"),
      Query("DROP TABLE moon")},
    { Query("DROP TABLE star"),
      Query("DROP TABLE mercury"),
      Query("DROP TABLE moon")});

static QueryList DDL = QueryList("DDL",
    { Query("CREATE TABLE ddl_test (a integer, b integer, c integer)")},
    { Query("CREATE TABLE ddl_test (a integer, b integer, c integer)")},
    { Query("INSERT INTO ddl_test VALUES (1, 2, 3)"),
      Query("SELECT * FROM ddl_test"),
      Query("ALTER TABLE ddl_test DROP COLUMN a"),
      Query("SELECT * FROM ddl_test"),
      Query("ALTER TABLE ddl_test ADD COLUMN a integer"),
      Query("SELECT * FROM ddl_test"),
      Query("INSERT INTO ddl_test VALUES (3, 4, 5), (5, 6, 7),(7, 8, 9)"),
      Query("SELECT * FROM ddl_test"),
      Query("ALTER TABLE ddl_test DROP COLUMN b, DROP COLUMN c"),
      Query("SELECT * FROM ddl_test"),
      Query("ALTER TABLE ddl_test ADD COLUMN b integer, DROP COLUMN a,"
            "                     ADD COLUMN c integer"),
      Query("INSERT INTO ddl_test VALUES (15, '1212'), (20, '7676')"),
      Query("SELECT * FROM ddl_test"),
      Query("DELETE FROM ddl_test"),
      Query("INSERT INTO ddl_test VALUES (12, 15), (44, 14), (19, 5)"),
      Query("ALTER TABLE ddl_test ADD PRIMARY KEY(b)"),
      Query("SELECT * FROM ddl_test"),
      Query("ALTER TABLE ddl_test DROP PRIMARY KEY,"
            "                     ADD PRIMARY KEY(b)"),
      Query("SELECT * FROM ddl_test"),
      Query("ALTER TABLE ddl_test ADD INDEX j(b), ADD INDEX k(b, c)"),
      Query("SELECT * FROM ddl_test"),
      Query("ALTER TABLE ddl_test DROP INDEX j, DROP INDEX k,"
            "                     ADD INDEX j(b), ADD INDEX k(b, c)"),
      Query("SELECT * FROM ddl_test")},
    { Query("DROP TABLE ddl_test")},
    { Query("DROP TABLE ddl_test")});

// the oDET column will encrypt three times and two of these will pad;
// the maximum field size if 2**32 - 1; so the maximum field size
// we support is 2**32 - 1 - (2 * AES_BLOCK_BYTES)
static QueryList MiscBugs = QueryList("MiscBugs",
    { Query("CREATE TABLE crawlies (purple VARCHAR(4294967263),"
            "                       pink VARCHAR(0))"),
      // Query("CREATE TABLE enums (x enum('this', 'that'))"),
      Query("CREATE TABLE bugs (spider TEXT)"),
      Query("CREATE TABLE more_bugs (ant INTEGER)")},
    { Query("CREATE TABLE crawlies (purple VARCHAR(4294967263),"
            "                       pink VARCHAR(0))"),
      // Query("CREATE TABLE enums (x enum('this', 'that'))"),
      Query("CREATE TABLE bugs (spider TEXT)"),
      Query("CREATE TABLE more_bugs (ant INTEGER)")},
    { Query("INSERT INTO bugs VALUES ('8legs'), ('crawly'), ('manyiz')"),
      Query("INSERT INTO more_bugs VALUES (9012), (2913), (19114)"),
      // Query("INSERT INTO enums VALUES ('this'), ('that')"),
      // Query("SELECT * FROM enums"),                 // proxy test
      // Query("SELECT spider + spider FROM bugs"),    // proxy test
      // Query("SELECT spider + spider FROM bugs"),    // proxy test
      // Query("SELECT SUM(spider) FROM bugs"),
      // Query("SELECT GREATEST(ant, 5000) FROM more_bugs"),
      // Query("SELECT GREATEST(12, ant, 5000) FROM more_bugs"),
      // Query("CREATE TABLE crawlers (pink DATE)"),
      // Query("SELECT * FROM bugs, more_bugs, crawlers, crawlies")
    },
    { Query("DROP TABLE crawlies"),
      Query("DROP TABLE bugs"),
      Query("DROP TABLE more_bugs")},
    { Query("DROP TABLE crawlies"),
      Query("DROP TABLE bugs"),
      Query("DROP TABLE more_bugs")});

//-----------------------------------------------------------------------

Connection::Connection(const TestConfig &input_tc, test_mode input_type) {
    tc = input_tc;
    type = input_type;
    //cl = 0;
    proxy_pid = -1;

    try {
        start();
    } catch (...) {
        stop();
        throw;
    }
}


Connection::~Connection() {
    stop();
}

void
Connection::restart() {
    stop();
    start();
}


void
Connection::start() {
    std::cerr << "start " << tc.db << std::endl;
    uint64_t mkey = 1133421234;
    std::string masterKey = BytesFromInt(mkey, AES_KEY_BYTES);
    switch (type) {
        //plain -- new connection straight to the DB
    case UNENCRYPTED:
    {
	Connect *const c =
	    new Connect(tc.host, tc.user, tc.pass, tc.port);
	conn_set.insert(c);
	this->conn = conn_set.begin();
	break;
            }
    //single -- new Rewriter
    case SINGLE:
	break;
    case PROXYPLAIN:
	//break;
    case PROXYENC:
    {
	//TODO:  a separate process for proxy
	
    }
    case ENC:
    {
	ConnectionInfo ci(tc.host, tc.user, tc.pass);
	const std::string master_key = "2392834";
	ProxyState *const ps =
	    new ProxyState(ci, tc.shadowdb_dir, master_key);
                re_set.insert(ps);
                this->re_it = re_set.begin();
    }
    break;
    
    default:
	assert_s(false, "invalid type passed to Connection");
    }
}

void
Connection::stop() {
    switch (type) {
    case PROXYPLAIN:
        //break;
    case ENC:
        for (auto r = re_set.begin(); r != re_set.end(); r++) {
            delete *r;
        }
        re_set.clear();
        break;
    case SINGLE:
        break;
    case UNENCRYPTED:
        for (auto c = conn_set.begin(); c != conn_set.end(); c++) {
            delete *c;
        }
        conn_set.clear();
        break;
    default:
        break;
    }
}

ResType
Connection::execute(const Query &query) {
    switch (type) {
    case ENC:
        return executeRewriter(query);
    case UNENCRYPTED:
    case PROXYPLAIN:
        return executeConn(query);
        //break;
        //return executeConn(query);
    case SINGLE:
        break;
    default:
        assert_s(false, "unrecognized type in Connection");
    }
    return ResType(false);
}

void
Connection::executeFail(const Query &query) {
    //cerr << type << " " << query << endl;
    LOG(test) << "Query: " << query.query << " could not execute" << std::endl;
}

/*ResType
Connection::executeEDBProxy(string query) {
    ResType res = cl->execute(query);
    if (!res.ok) {
        executeFail(query);
    }
    return res;
    }*/

ResType
Connection::executeConn(const Query &query) {
    std::unique_ptr<DBResult> dbres(nullptr);

    //cycle through connections of which should execute query
    conn++;
    if (conn == conn_set.end()) {
        conn = conn_set.begin();
    }

    //cout << query << endl;
    if (!(*conn)->execute(query.query, &dbres)) {
        executeFail(query);
        return ResType(false);
    }
    return dbres->unpack();
}

ResType
Connection::executeRewriter(const Query &query) {
    //translate the query
    //
    //
    re_it++;
    if (re_it == re_set.end()) {
        re_it = re_set.begin();
    }

    //cout << query << endl;
    ProxyState *const ps = *re_it;
    // If this assert fails, deteremine if one schema_cache makes sense
    // for multiple connections.
    assert(re_set.size() == 1);
    const std::string &default_db =
        getDefaultDatabaseForConnection(ps->getConn());

    return executeQuery(*ps, query.query, default_db,
                        &this->schema_cache).res_type;
}

my_ulonglong
Connection::executeLast() {
    switch(type) {
    case SINGLE:
        break;
    case UNENCRYPTED:
    case PROXYPLAIN:
        // break;
    case PROXYSINGLE:
        //TODO(ccarvalho) check this 
        break;

    default:
        assert_s(false, "type does not exist");
    }
    return 0;
}

my_ulonglong
Connection::executeLastConn() {
    conn++;
    if (conn == conn_set.end()) {
        conn = conn_set.begin();
    }
    return (*conn)->last_insert_id();
}

my_ulonglong
Connection::executeLastEDB() {
    std::cerr << "No functionality for LAST_INSERT_ID() without proxy" << std::endl;
    return 0;
}

//----------------------------------------------------------------------

static bool
CheckAnnotatedQuery(const TestConfig &tc,
                    const Query &control_query,
                    const Query &test_query)
{
    const std::string empty_str = "";
    std::string r;
    ntest++;

    std::vector<std::string> cps = test_query.crash_points;
    for (auto cp = cps.begin(); cp != cps.end(); ++cp) {
        global_crash_point = *cp;

        try {
            if (test_query.query != empty_str) {
                test->execute(test_query);
            }
        } catch (const std::runtime_error &e) {
            if (strcmp(e.what(), "crash test exception") != 0) {
                throw;
            }
        }
    }
    global_crash_point = empty_str;

    LOG(test) << "control query: " << control_query.query;
    const ResType control_res =
        (empty_str == control_query.query) ? ResType(true) :
                                control->execute(control_query);

    LOG(test) << "test query: " << test_query.query;
    const ResType test_res =
        (empty_str == test_query.query) ? ResType(true) :
                                    test->execute(test_query);

    if (control_res.ok != test_res.ok) {
        LOG(warn) << "control " << control_res.ok
            << ", test " << test_res.ok
            << ", and true is " << true
            << " for query: " << test_query.query;

        if (tc.stop_if_fail)
            thrower() << "stop on failure";
        return false;
    } else if (!match(test_res, control_res)) {
        LOG(warn) << "result mismatch for query: " << test_query.query;
        LOG(warn) << "control is:";
        printRes(control_res);
        LOG(warn) << "test is:";
        printRes(test_res);

        if (tc.stop_if_fail) {
            LOG(warn) << "RESULT: " << npass << "/" << ntest;
            thrower() << "stop on failure";
        }
        return false;
    } else {
        npass++;
        return true;
    }
}

static bool
CheckQuery(const TestConfig &tc, const Query &query) {
    std::cerr << "--------------------------------------------------------------------------------" << "\n";
    //TODO: should be case insensitive
    if (query.query == "SELECT LAST_INSERT_ID()") {
        ntest++;
        switch(test_type) {
            case UNENCRYPTED:
            case PROXYPLAIN:
                //break;
            case ENC:
                //TODO(ccarvalho): check proxy
            default:
                LOG(test) << "not a valid case of this test; skipped";
                break;
        }

        npass++;
        return true;
    }

    return CheckAnnotatedQuery(tc, query, query);
}

struct Score {
    Score(const std::string &name) : success(0), total(0), name(name) {}
    void mark(bool t) {t ? pass() : fail();}
    std::string stringify() {
        return name + ":\t" + std::to_string(success) + "/" +
               std::to_string(total);
    }

private:
    unsigned int success;
    unsigned int total;
    std::string name;

    void pass() {++success, ++total;}
    void fail() {++total;}
};

static Score
CheckQueryList(const TestConfig &tc, const QueryList &queries) {
    Score score(queries.name);
    for (unsigned int i = 0; i < queries.create.size(); i++) {
        Query control_query = queries.create.choose(control_type)[i];
        Query test_query = queries.create.choose(test_type)[i]; 
        score.mark(CheckAnnotatedQuery(tc, control_query, test_query));
    }

    for (auto q = queries.common.begin(); q != queries.common.end(); q++) {
        switch (test_type) {
        case PLAIN:
        case SINGLE:
        case PROXYPLAIN:
           // break;
        case ENC:
            score.mark(CheckQuery(tc, *q));
            break;

        default:
            assert_s(false, "test_type invalid");
        }
    }

    for (unsigned int i = 0; i < queries.drop.size(); i++) {
        Query control_query = queries.drop.choose(control_type)[i];
        Query test_query = queries.drop.choose(test_type)[i];
        score.mark(CheckAnnotatedQuery(tc, control_query, test_query));
    }

    return score;
}

static void
RunTest(const TestConfig &tc) {
    // ###############################
    //      TOTAL RESULT: 469/471
    // ###############################

    std::vector<Score> scores;

    assert(testSlowMatch());

    // Pass 50/50
    scores.push_back(CheckQueryList(tc, Select));

    // Pass 31/31
    scores.push_back(CheckQueryList(tc, HOM));

    // Pass 20/20
    scores.push_back(CheckQueryList(tc, Insert));

    // Pass 27/27
    scores.push_back(CheckQueryList(tc, Join));

    // Pass 21/21
    scores.push_back(CheckQueryList(tc, Basic));

    // Pass 40/40
    scores.push_back(CheckQueryList(tc, Update));

    // Pass 28/28
    scores.push_back(CheckQueryList(tc, Delete));

    // Pass ?/?
    // scores.push_back(CheckQueryList(tc, Search));

    // Pass 20/20
    scores.push_back(CheckQueryList(tc, PrivMessages));

    // Pass 44/44
    scores.push_back(CheckQueryList(tc, UserGroupForum));

    // Pass 41/41
    scores.push_back(CheckQueryList(tc, Null));

    // Pass 21/21
    ProxyState *const ps = test->getProxyState();
    if (ps->defaultSecurityRating() == SECURITY_RATING::BEST_EFFORT) {
        scores.push_back(CheckQueryList(tc, BestEffort));
    }

    // Pass 27/27
    scores.push_back(CheckQueryList(tc, Auto));

    // Pass ?/?
    // scores.push_back(CheckQueryList(tc, Negative));

    // Pass 21/21
    scores.push_back(CheckQueryList(tc, DefaultValue));

    // Pass ?/?
    // scores.push_back(CheckQueryList(tc, Decimal));

    // Pass 20/20
    scores.push_back(CheckQueryList(tc, NonStrictMode));

    // Pass 32/34
    // NOTE: two queries should fail
    scores.push_back(CheckQueryList(tc, Transactions));

    // Pass 14/14
    scores.push_back(CheckQueryList(tc, TableAliases));

    // Pass 25/25
    scores.push_back(CheckQueryList(tc, DDL));

    // Pass 8/8
    scores.push_back(CheckQueryList(tc, MiscBugs));

    for (auto it : scores) {
        std::cout << it.stringify() << std::endl;
    }

    /*
    //everything has to restart so that last_insert_id() are lined up
    test->restart();
    control->restart();
    CheckQueryList(tc, ManyConnections);
    */
}


//---------------------------------------------------------------------

TestQueries::TestQueries() {
}

TestQueries::~TestQueries() {
}

static test_mode
string_to_test_mode(const std::string &s)
{
    if (s == "plain")
        return UNENCRYPTED;
    else if (s == "single")
        return SINGLE;
    else if (s == "proxy-plain")
        return PROXYPLAIN;
    else if (s == "enc")
        return ENC;
    else
        thrower() << "unknown test mode " << s;
    return TESTINVALID;
}

void
TestQueries::run(const TestConfig &tc, int argc, char ** argv) {
    switch(argc) {
    case 4:
        //TODO check that argv[3] is a proper int-string
        no_conn = valFromStr(argv[3]);
    case 3:
        control_type = string_to_test_mode(argv[1]);
        test_type = string_to_test_mode(argv[2]);
        break;
    default:
        std::cerr << "Usage:" << std::endl
             << "    .../tests/test queries control-type test-type [num_conn]" << std::endl
             << "Possible control and test types:" << std::endl
             << "    plain" << std::endl
             << "    single" << std::endl
             << "    proxy-plain" << std::endl
             << "    enc" << std::endl
             << "single make connections through EDBProxy" << std::endl
             << "proxy-* makes connections *'s encryption type through the proxy" << std::endl
             << "num_conn is the number of conns made to a single db (default 1)" << std::endl
             << "    for num_conn > 1, control and test should both be proxy-* for valid results" << std::endl;
        return;
    }

    if (no_conn > 1) {
        switch(test_type) {
        case UNENCRYPTED:
        case SINGLE:
            break;
        case PROXYPLAIN:
           // break;
        case ENC:
            //TODO(ccarvalho) check this
            break;
        default:
            std::cerr << "test_type does not exist" << std::endl;
        }
    }


    try {
        TestConfig control_tc = TestConfig();
        control_tc.db = control_tc.db+"_control";

        Connection test_(tc, test_type);
        test = &test_;
        test->execute("CREATE DATABASE IF NOT EXISTS " + tc.db + ";");
        test->execute("USE " + tc.db + ";");

        Connection control_(control_tc, control_type);
        control = &control_;
        control->execute("CREATE DATABASE IF NOT EXISTS " + control_tc.db + ";");
        control->execute("USE " + control_tc.db + ";");

        enum { nrounds = 1 };
        for (uint i = 0; i < nrounds; i++)
            RunTest(tc);

        std::cerr << "RESULT: " << npass << "/" << ntest << std::endl;
    } catch (const AbstractException &e) {
        std::cout << e << std::endl;
        throw;
    }
}

