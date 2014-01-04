/*
 * TestNotConsider
 * -- tests optimization for query passing, specifically that queries with no sensitive
 *    fields are not encrypted
 *
 */

#include <test/TestNotConsider.hh>
#include <util/cryptdb_log.hh>

static int ntest = 0;
static int npass = 0;

TestNotConsider::TestNotConsider() {
}

TestNotConsider::~TestNotConsider() {
}

std::vector<Query> CreateSingle = {
    Query("CREATE TABLE msgs (msgid integer PRIMARY KEY AUTO_INCREMENT, msgtext enc text)"),
    Query("CREATE TABLE privmsg (msgid enc integer, recid enc integer, senderid enc integer)"),
    Query("CREATE TABLE uncrypt (id integer, t text)"),
    Query("CREATE TABLE forum (forumid integer AUTO_INCREMENT PRIMARY KEY, title enc text)"),
    Query("CREATE TABLE post (postid integer AUTO_INCREMENT PRIMARY KEY, forumid enc integer, posttext enc text, author enc integer)"),
    Query("CREATE TABLE u (userid enc integer, username enc text)"),
    Query("CREATE TABLE "+PWD_TABLE_PREFIX+"u (username enc text, psswd enc text)")
};

std::vector<Query> CreateMulti = {
    Query("CREATE TABLE msgs (msgid equals privmsg.msgid integer AUTO_INCREMENT PRIMARY KEY , msgtext encfor msgid text)"),
    Query("CREATE TABLE privmsg (msgid integer, recid equals u.userid speaks_for msgid integer, senderid speaks_for msgid integer)"),
    Query("CREATE TABLE uncrypt (id integer, t text)"),
    Query("CREATE TABLE forum (forumid integer AUTO_INCREMENT PRIMARY KEY, title text)"),
    Query("CREATE TABLE post (postid integer AUTO_INCREMENT PRIMARY KEY, forumid equals forum.forumid integer, posttext encfor forumid text, author equals u.userid speaks_for forumid integer)"),
    Query("CREATE TABLE u (userid equals privmsg.senderid integer, username givespsswd userid text)"),
    Query("COMMIT ANNOTATIONS")
};

std::vector<Query> QueryList = {
    Query("INSERT INTO uncrypt VALUES (1, 'first')"),
    Query("INSERT INTO uncrypt (title) VALUES ('second')"),
    Query("INSERT INTO msgs VALUES (1, 'texty text text')"),
    Query("INSERT INTO post (forumid, posttext, author) VALUES (1, 'words', 1)"),
    Query("INSERT INTO u (1, 'alice')"),
    Query("INSERT INTO "+PWD_TABLE_PREFIX+"u ('alice', 'secretA')"),

    Query("SELECT * FROM uncrypt"),
    Query("SELECT * FROM msgs"),
    Query("SELECT postid FROM post"),
    Query("SELECT posttext FROM post"),
    Query("SELECT recid FROM privmsg WHERE msgid = 1"),
    Query("SELECT postid FROM post WHERE forumid = 1"),
    Query("SELECT postid FROM post WHERE posttext LIKE '%ee%'"),

    Query("SELECT * FROM uncrypt, post"),
    Query("SELECT postid FROM forum, post WHERE forum.formid = post.forumid"),

    Query("UPDATE uncrypt SET t = 'weeeeeee' WHERE id = 3"),
    Query("UPDATE privmsg SET msgid = 4"),

    Query("DELETE FROM uncrypt"),
    Query("DELETE FROM post WHERE postid = 5"),
    Query("DELETE FROM "+PWD_TABLE_PREFIX+"u WHERE username='alice'")
};

std::vector<Query> Drop = {
    Query("DROP TABLE msgs"),
    Query("DROP TABLE privmsg"),
    Query("DROP TABLE uncrypt"),
    Query("DROP TABLE forum"),
    Query("DROP TABLE post"),
    Query("DROP TABLE u")
};

static void
Check(const TestConfig &tc, const std::vector<Query> &queries,
      EDBProxy * cl, bool createdrop) {
    //all create queries should go be considered...
    for (auto q = queries.begin(); q != queries.end(); q++) {
        ntest++;
        command com = getCommand(q->query);
	// NOTE(abw333): this code is currently not in use and it does not work.
        // contact me if this becomes a problem.
        if (cl->considerQuery(com, q->query) == q->test_res) {
            npass++;
        } else {
            LOG(test) << q->query << " had consider " << q->test_res;
            if (tc.stop_if_fail) {
                assert_s(false, "failed!");
            }
        }
    }
    if (!createdrop) {
        return;
    }
    //have to have queries in place to run other tests...
    for (auto q = queries.begin(); q != queries.end(); q++) {
        cl->execute(q->query);
    }
}

static void
Consider(const TestConfig &tc, bool multi) {
    uint64_t mkey = 1144220039;
    std::string masterKey = BytesFromInt(mkey, AES_KEY_BYTES);
    EDBProxy * cl;
    cl = new EDBProxy(tc.host, tc.user, tc.pass, tc.db, tc.port, multi);
    cl->setMasterKey(masterKey);
    if (multi) {
        Check(tc, CreateMulti, cl, true);
    } else {
        Check(tc, CreateSingle, cl, true);
    }

    Check(tc, QueryList, cl, false);

    Check(tc, Drop, cl, true);

    if (!multi) {
        cl->execute("DROP TABLE "+PWD_TABLE_PREFIX+"u");
    }
}

void
TestNotConsider::run(const TestConfig &tc, int argc, char ** argv) {
    //only works on multi-princ, so as not to mess up TCP benchmarking
    Consider(tc, true);

    cerr << "RESULT: " << npass << "/" << ntest << endl;
}
