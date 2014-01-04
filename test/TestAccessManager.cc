/*
 * TestAccessManager
 * -- tests KeyAccess and MetaAccess
 *
 *
 */

#include <test/TestAccessManager.hh>


static int ntest = 0;
static int npass = 0;

static void
record(const TestConfig &tc, bool result, string test) {
    ntest++;
    if (!result) {
      if (tc.stop_if_fail) {
    assert_s(false,test);
      }
      return;
    }
    npass++;
}

static Prin alice = Prin("u.uname","alice");
static Prin bob = Prin("u.uname","bob");
static Prin chris = Prin("u.uname","chris");

static Prin u1 = Prin("u.uid","1");
static Prin u2 = Prin("u.uid","2");
static Prin u3 = Prin("u.uid","3");

static Prin g5 = Prin("g.gid","5");
static Prin g2 = Prin("g.gid", "2");

static Prin f2 = Prin("f.fid","2");
static Prin f3 = Prin("f.fid","3");

static Prin mlwork = Prin("x.mailing_list","work");
static Prin ml50 = Prin("x.mailing_list","50");

static Prin a5 = Prin("u.acc","5");

static Prin m2 = Prin("m.mess","2");
static Prin m3 = Prin("m.mess","3");
static Prin m4 = Prin("m.mess","4");
static Prin m5 = Prin("m.mess","5");
static Prin m15 = Prin("m.mess","15");

static Prin s4 = Prin("m.sub","4");
static Prin s5 = Prin("m.sub","5");
static Prin s6 = Prin("m.sub","6");
static Prin s7 = Prin("m.sub","7");
static Prin s16 = Prin("m.sub","16");
static Prin s24 = Prin("m.sub","24");

static string secretA = "secretA";
static string secretB = "secretB";
static string secretC = "secretC";

TestAccessManager::TestAccessManager() {}

TestAccessManager::~TestAccessManager() {}

static void
testMeta_native(const TestConfig &tc, Connect * conn) {
    string test = "(native meta) ";
    MetaAccess * meta;
    meta = new MetaAccess(conn, false);

    meta->startEquals("uname");
    meta->startEquals("uid");
    meta->startEquals("gid");
    meta->addEquals("u.uid", "uid");
    meta->addEquals("g.uid","uid");
    meta->addEquals("g.gid","gid");
    meta->addAccess("uid","gid");

    record(tc, !meta->CheckAccess(), test + "CheckAccess--no gives");

    meta->addEquals("u.uname","uname");
    meta->addGives("uname");

    record(tc, !meta->CheckAccess(), test + "CheckAccess--bad tree");

    meta->addAccess("uname","uid");

    record(tc, meta->CheckAccess(), test + "CheckAccess--good tree");
    meta->CreateTables();

    //XXX need to properly test Check functions
    /*meta->startEquals("acc");

    record(tc, meta->addEqualsCheck("u.acc","g.acc") < 0, test + "succeeded in adding illegal equality");
    record(tc, meta->addAccessCheck("g.acc","g.gid") < 0, test + "succeeded in adding illegal access link");

    record(tc, meta->addAccessCheck("u.uname", "u.acc") == 0, test + "failed to add legal access link");
    record(tc, meta->addEqualsCheck("u.acc","g.acc") == 0, test + "failed to add legal equality (case 2)");
    record(tc, meta->addGivesCheck("u.captcha") == 0, test + "failed to add legal givesPsswd");
    record(tc, meta->addEqualsCheck("u.uname", "u.captcha") == 0, test + "failed to add legal equality (case 4)");*/

    delete meta;
}

static KeyAccess *
buildTest(Connect * conn) {
    KeyAccess * am;
    am = new KeyAccess(conn);

    am->startPrinc("uname");
    am->startPrinc("uid");
    am->startPrinc("gid");
    am->startPrinc("fid");
    am->startPrinc("ml");
    am->startPrinc("acc");
    am->startPrinc("mid");
    am->startPrinc("sub");

    am->addToPrinc("u.uid","uid");
    am->addToPrinc("g.uid","uid");
    am->addToPrinc("u.uname","uname");

    am->addSpeaksFor("uname","uid");

    am->addToPrinc("m.uid","uid");
    am->addToPrinc("m.mess","mid");
    am->addToPrinc("u.acc","acc");
    am->addToPrinc("g.gid","gid");

    am->addSpeaksFor("uid","mid");
    am->addSpeaksFor("uid","acc");
    am->addSpeaksFor("uid","gid");

    am->addToPrinc("x.gid","gid");
    am->addToPrinc("f.gid","gid");
    am->addToPrinc("f.fid","fid");
    am->addToPrinc("x.mailing_list","ml");

    am->addSpeaksFor("gid","fid");
    am->addSpeaksFor("gid","ml");

    am->addToPrinc("m.sub","sub");

    am->addSpeaksFor("mid","sub");
    am->addSpeaksFor("gid","acc");
    am->addGives("uname");

    am->startPrinc("msgid");
    am->startPrinc("text");
    am->startPrinc("user");
    am->startPrinc("username");
    am->addToPrinc("msgs.msgid","msgid");
    am->addToPrinc("msgs.msgtext","text");
    am->addSpeaksFor("msgid","text");
    am->addToPrinc("privmsgs.msgid","msgid");
    am->addToPrinc("privmsgs.recid","user");
    am->addToPrinc("users.userid","user");
    am->addSpeaksFor("user","msgid");
    am->addToPrinc("privmsgs.senderid","user");
    am->addSpeaksFor("user","msgid");
    am->addToPrinc("users.username","username");
    am->addGives("username");
    am->addSpeaksFor("username", "user");

    secretA.resize(AES_KEY_BYTES);
    secretB.resize(AES_KEY_BYTES);
    secretC.resize(AES_KEY_BYTES);

    assert_s(am->CreateTables() == 0, "could not create tables");

    return am;
}

//assumes am has been built with buildTest
static void
buildBasic(KeyAccess * am) {
    am->insertPsswd(alice,secretA);
    am->insert(alice,u1);
    am->insert(u1,g5);
    am->insert(g5,mlwork);
    am->insert(g5,f2);
    am->insert(g5,f3);
    am->insertPsswd(bob,secretB);
    am->insert(bob,u2);
    am->insert(u2,g5);
    am->removePsswd(alice);
    am->removePsswd(bob);
}

static void
buildAll(KeyAccess * am) {
    buildBasic(am);
    am->insertPsswd(alice,secretA);
    am->insertPsswd(bob,secretB);
    am->insertPsswd(chris,secretC);
    am->insert(g5,a5);
    am->insert(chris,u3);
    am->insert(u1,m5);
    am->insert(u2,m2);
    am->insert(u2,m3);
    am->insert(u3,m15);
    am->insert(m2,s6);
    am->insert(m3,s4);
    am->insert(m4,s6);
    am->insert(m5,s5);
    am->insert(m5,s7);
    am->insert(m15,s16);
    am->insert(m15,s24);
    am->removePsswd(alice);
    am->removePsswd(bob);
    am->removePsswd(chris);
}

static void
testMeta(const TestConfig &tc, KeyAccess * am) {
    string test = "(meta) ";

    std::set<string> generic_gid = am->getEquals("g.gid");
    record(tc, generic_gid.find("f.gid") != generic_gid.end(), test+"f.gid not in getEquals(g.gid)");
    record(tc, generic_gid.find("x.gid") != generic_gid.end(), test+"x.gid not in getEquals(g.gid)");

    std::set<string> generic_uid = am->getEquals("m.uid");
    record(tc, generic_uid.find("u.uid") != generic_uid.end(), test+"u.uid not in getEquals(m.uid)");
    record(tc, generic_uid.find("g.uid") != generic_uid.end(), test+"g.uid not in getEquals(m.uid)");
    record(tc, generic_uid.find("f.gid") == generic_uid.end(), test+"m.uid in getEquals(f.gid)");

    std::set<string> gid_hasAccessTo = am->getTypesHasAccessTo("g.gid");
    record(tc, gid_hasAccessTo.find("f.fid") != gid_hasAccessTo.end(), test+"g.gid does not have access to f.fid");
    record(tc, gid_hasAccessTo.find("x.mailing_list") != gid_hasAccessTo.end(),test+"g.gid does not have access to x.mailing_list");
    record(tc, gid_hasAccessTo.find("g.uid") == gid_hasAccessTo.end(),test+"g.gid does have access to g.uid");
    record(tc, gid_hasAccessTo.find(
                "f.gid") == gid_hasAccessTo.end(),
       test+"getTypesHasAccessTo(g.gid) includes f.gid");
    record(tc, gid_hasAccessTo.find(
                "g.gid") == gid_hasAccessTo.end(),
     test+"getTypesHasAccessTo(g.gid) includes g.gid");

    std::set<string> mess_accessibleFrom = am->getTypesAccessibleFrom("m.mess");
    record(tc, mess_accessibleFrom.find(
                    "m.uid") != mess_accessibleFrom.end(),
       test+"m.mess is not accessible from m.uid");
    record(tc, mess_accessibleFrom.find(
                    "u.uid") != mess_accessibleFrom.end(),
       test+"m.mess is not accessible from u.uid");
    record(tc, mess_accessibleFrom.find(
                    "g.uid") != mess_accessibleFrom.end(),
     test+"m.mess is not accessible from g.uid");
    record(tc, mess_accessibleFrom.find(
                    "g.gid") == mess_accessibleFrom.end(),
       test+"m.mess is accessible from g.gid");
    record(tc, mess_accessibleFrom.find(
                    "u.uname") == mess_accessibleFrom.end(),
       test+"m.mess is accessible from u.uname in one link");

    std::set<string> acc_accessibleFrom = am->getGenAccessibleFrom(
                                   am->getPrincType("u.acc"));
    record(tc, acc_accessibleFrom.find(am->getPrincType(
                          "u.uid")) != acc_accessibleFrom.end(),
       test+"gen acc is not accessible from gen uid");
    record(tc, acc_accessibleFrom.find(am->getPrincType(
                          "g.gid")) != acc_accessibleFrom.end(),
       test+"gen acc is not accessible from gen gid");
    record(tc, acc_accessibleFrom.find(am->getPrincType(
                          "f.fid")) == acc_accessibleFrom.end(),
       test+"gen acc is accessible from gen fid");

    list<Prin> bfs = am->BFS_hasAccess(alice);
    list<string> dfs = am->DFS_hasAccess(alice);
}

static void
testSingleUser(const TestConfig &tc, KeyAccess * am) {
    string test = "(single) ";

    record(tc, am->insertPsswd(alice,secretA) == 0, "insert alice failed");

    am->insert(alice, u1);
    am->insert(u1, g5);
    am->insert(g5,f2);
    string f2_key1 = marshallBinary(am->getKey(f2));
    record(tc, f2_key1.length() > 0, test + "alice cannot access forumkey");
    am->removePsswd(alice);
    record(tc, am->getKey(alice).length() == 0, test + "alice's key accesible with no one logged on");
    record(tc, am->getKey(u1).length() == 0, test + "u1's key accesible with no one logged on");
    record(tc, am->getKey(g5).length() == 0, test + "g5's key accesible with no one logged on");
    record(tc, am->getKey(f2).length() == 0, test + "f2's key accesible with no one logged on");
    am->insertPsswd(alice,secretA);
    string f2_key2 = marshallBinary(am->getKey(f2));
    record(tc, f2_key2.length() > 0, test + "alice cannot access forumkey");
    record(tc, f2_key1.compare(f2_key2) == 0, test + "forum keys are not equal for alice");
}

static void
testMultiBasic(const TestConfig &tc, KeyAccess * am) {
    string test = "(multi basic) ";
    am->insertPsswd(alice,secretA);
    am->insert(alice,u1);
    am->insert(u1,g5);
    am->insert(g5,f2);
    string f2_key1 = marshallBinary(am->getKey(f2));
    am->insertPsswd(bob,secretB);
    am->insert(bob,u2);
    am->insert(u2,g5);

    record(tc, am->getKey(f2).length() > 0,
       test+"forum 2 key not accessible with both alice and bob logged on");
    am->removePsswd(alice);
    string f2_key2 = marshallBinary(am->getKey(f2));
    record(tc, f2_key2.length() > 0,
       test+"forum 2 key not accessible with bob logged on");
    record(tc, f2_key2.compare(f2_key1) == 0,
       test+"forum 2 key is not the same for bob as it was for alice");
    am->insert(g5,f3);
    string f3_key1 = marshallBinary(am->getKey(f3));
    record(tc, f3_key1.length() > 0,
       test+"forum 3 key not acessible with bob logged on");
    am->removePsswd(bob);
    record(tc, am->getKey(alice).length() == 0,
       test+"can access alice's key with no one logged in");
    record(tc, am->getKey(bob).length() == 0,
       test+"can access bob's key with no one logged in");
    record(tc, am->getKey(u1).length() == 0,
       test+"can access user 1 key with no one logged in");
    record(tc, am->getKey(u2).length() == 0,
       test+"can access user 2 key with no one logged in");
    record(tc, am->getKey(g5).length() == 0,
       test+"can access group 5 key with no one logged in");
    record(tc, am->getKey(f2).length() == 0,
       test+"can access forum 2 key with no one logged in");
    record(tc, am->getKey(f3).length() == 0,
       test+"can access forum 3 key with no one logged in");
    am->insertPsswd(alice, secretA);
    string f3_key2 = marshallBinary(am->getKey(f3));
    record(tc, f3_key2.length() > 0,
       test+"forum 3 key not accessible with alice logged on");
    record(tc, f3_key1.compare(f3_key2) == 0,
       test+"forum 3 key is not the same for alice as it was for bob");
    am->removePsswd(alice);
    am->insert(g5,mlwork);
    record(tc, am->getKey(mlwork).length() == 0,
       test+"can access mailing list work key with no one logged in");
    record(tc, am->insertPsswd(alice, secretA) == 0, "insert alice failed (4)");
    string work_key1 = marshallBinary(am->getKey(mlwork));
    record(tc, work_key1.length() > 0,
       test+"mailing list work key inaccessible when alice is logged on");
    am->removePsswd(alice);
    am->insertPsswd(bob, secretB);
    string work_key2 = marshallBinary(am->getKey(mlwork));
    record(tc, work_key2.length() > 0,
       test+"mailing list work key inaccessible when bob is logged on");
    record(tc, work_key1.compare(work_key2) == 0,
       test+"mailing list work key is not the same for bob as it was for alice");
}

/*static void
testMetaAlterations(const TestConfig &tc, KeyAccess *am) {
    string test = "(meta changes) ";
    buildAll(am);
    record(tc, am->addEquals("u.user","u.uid") == 0, test + "add equality failed (case 3)");

    record(tc, am->getKey(m5).length() == 0, test + "m5 key accessible with no one online!");
    record(tc, am->addEquals("x.mailing_list", "m.mess") == 0, test + "didn't added legal equality (ml = m)");
    record(tc, am->getKey(m5).length() == 0, test + "ml5 key accessible with no one online!");

    record(tc, am->addEquals("g.gid","m.mess") < 0, test + "illegal equality (gave access to previously inaccesible keys) was added");

    record(tc, am->addGives("u.c") == 0, test + "add gives failed");
    record(tc, am->addEquals("u.uid", "u.c") == 0, test + "add equality failed (case 4)");

    record(tc, am->addAccess("u.uid", "f.fid") == 0, test + "add access failed");
    record(tc, am->addAccess("new.thing","m.mess") < 0, test + "illegal access (new principal would break the access tree) was added");

    //check that meta is still correctly structured
    record(tc, am->getGeneric("u.user") == am->getGeneric("u.uid"), test + "u.user and u.uid should have the same generic");
    record(tc, am->getGeneric("x.mailing_list") == am->getGeneric("m.mess"), test + "mailing_list and m.mess should have the same generic");
    record(tc, am->getGeneric("u.c") == am->getGeneric("u.uid"), test + "u.c and u.uid should have the same generic");
    record(tc, am->getGeneric("new.thing") == "", test + "new.thing should not have been created");
    //check that data is still all accessible (or not, as the case may be)...
    //alice
    am->insertPsswd(alice,secretA);
    record(tc, am->getKey(u1).length() > 0, test+"alice can't access u1");
    record(tc, am->getKey(m5).length() > 0, test+"alice can't access m5");
    record(tc, am->getKey(s5).length() > 0, test+"alice can't access s5");
    record(tc, am->getKey(g5).length() > 0, test+"alice can't access g5");
    record(tc, am->getKey(f3).length() > 0, test+"alice can't access f3");
    record(tc, am->getKey(a5).length() > 0, test+"alice can't access a5");
    record(tc, am->getKey(f2).length() > 0, test+"alice can't access f2");
    record(tc, am->getKey(mlwork).length() > 0, test+"alice can't access mlwork");
    record(tc, am->getKey(u2).length() == 0, test+"alice can access u2");
    record(tc, am->getKey(m3).length() == 0, test+"alice can access m3");
    record(tc, am->getKey(s16).length() == 0, test+"alice can access s16");
    record(tc, am->getKey(s6).length() > 0, test+"alice can't access s6 (orphan)");
    am->removePsswd(alice);

    //bob
    am->insertPsswd(bob,secretB);
    record(tc, am->getKey(u2).length() > 0, test+"bob can't access u2");
    record(tc, am->getKey(g5).length() > 0, test+"bob can't access g5");
    record(tc, am->getKey(m2).length() > 0, test+"bob can't access m2");
    record(tc, am->getKey(m4).length() > 0, test+"bob can't access m4 (orphan)");
    record(tc, am->getKey(s6).length() > 0, test+"bob can't access s6");
    record(tc, am->getKey(m3).length() > 0, test+"bob can't access m3");
    record(tc, am->getKey(s4).length() > 0, test+"bob can't access s4");
    record(tc, am->getKey(u3).length() == 0, test+"bob can access u3");
    record(tc, am->getKey(u1).length() == 0, test+"bob can access u1");
    record(tc, am->getKey(m15).length() == 0, test+"bob can access m15");
    am->removePsswd(bob);

    //chris
    am->insertPsswd(chris,secretC);
    record(tc, am->getKey(u3).length() > 0, test+"chris can't access u3");
    record(tc, am->getKey(m15).length() > 0, test+"chris can't access m15");
    record(tc, am->getKey(s16).length() > 0, test+"chris can't access s16");
    record(tc, am->getKey(s24).length() > 0, test+"chris can't access s24");
    record(tc, am->getKey(u2).length() == 0, test+"chris can access u2");
    record(tc, am->getKey(s5).length() == 0, test+"chris can access s5");
    record(tc, am->getKey(mlwork).length() == 0, test+"chris can access mlwork");
    am->removePsswd(chris);
    }*/




static void
testNonTree(const TestConfig &tc, KeyAccess * am) {
    string test = "(non-tree) ";
    buildBasic(am);

    am->insert(g5,a5);
    record(tc, am->getKey(a5).length() == 0,
       test+"can access a5's key with no one logged in");
    am->insertPsswd(alice,secretA);
    string a5_key1 = marshallBinary(am->getKey(a5));
    record(tc, a5_key1.length() > 0,
       test+"cannot access a5's key with alice logged on");
    am->removePsswd(alice);
    am->insertPsswd(bob,secretB);
    string a5_key2 = marshallBinary(am->getKey(a5));\
    record(tc, a5_key2.length() > 0,
       test+"cannot access a5's key with bob logged on");
    record(tc, a5_key1.compare(a5_key2) == 0,
       test+"alice and bob have different a5 keys");

}

static void
testOrphans(const TestConfig &tc, KeyAccess * am) {
    string test = "(orphan) ";
    buildBasic(am);
    am->insertPsswd(bob,secretB);
    am->insert(g5,a5);
    am->removePsswd(bob);

    am->insert(m2,s6);
    record(tc, (am->getKey(s6)).length() > 0,
       test+"s6 key does not exist as an orphan");
    record(tc, (am->getKey(m2)).length() > 0,
       test+"m2 key does not exist as an orphan");
    string s6_key1 = marshallBinary(am->getKey(s6));
    string m2_key1 = marshallBinary(am->getKey(m2));

    am->insert(u2,m2);
    record(tc, (am->getKey(m2)).length() == 0,
       test+"m2 key is available when bob is logged off");
    record(tc, (am->getKey(s6)).length() == 0,
       test+"s6 key is available when bob is logged off");

    am->insertPsswd(bob,secretB);
    record(tc, (am->getKey(s6)).length() > 0,
       test+"s6 key is not available when bob is logged on");
    record(tc, (am->getKey(m2)).length() > 0,
       test+"m2 key is not available when bob is logged on");
    string s6_key3 = marshallBinary(am->getKey(s6));
    string m2_key3 = marshallBinary(am->getKey(m2));
    record(tc, s6_key1.compare(s6_key3) == 0, test+"s6 key does not match");
    record(tc, m2_key1.compare(m2_key3) == 0, test+"m2 key does not match");

    am->insert(m3,s4);
    record(tc, (am->getKey(s4)).length() > 0,
       test+"s4 key does not exist as an orphan");
    record(tc, (am->getKey(m3)).length() > 0,
       test+"m3 key does not exist as an orphan");
    string s4_key1 = marshallBinary(am->getKey(s4));
    string m3_key1 = marshallBinary(am->getKey(m3));
    am->insert(u2,m3);
    string s4_key2 = marshallBinary(am->getKey(s4));
    string m3_key2 = marshallBinary(am->getKey(m3));
    record(tc, s4_key1.compare(s4_key2) == 0, test+"s4 key does not match");
    record(tc, m3_key1.compare(m3_key2) == 0, test+"m3 key does not match");
    am->removePsswd(bob);
    record(tc, (am->getKey(s4)).length() == 0, test+"s4 key is available when bob is logged off");
    record(tc, (am->getKey(m3)).length() == 0, test+"m3 key is available when bob is logged off");
    am->insertPsswd(bob,secretB);
    string s4_key3 = marshallBinary(am->getKey(s4));
    string m3_key3 = marshallBinary(am->getKey(m3));
    record(tc, s4_key1.compare(s4_key3) == 0, test+"s4 key does not match 1v3");
    record(tc, m3_key1.compare(m3_key3) == 0, test+"m3 key does not match 1v3");

    am->insert(m4,s6);
    record(tc, (am->getKey(m4)).length() > 0, test+"m4 key does not exist as orphan");
    record(tc, (am->getKey(s6)).length() > 0, test+"s6 key does not exist as orphan AND as accessible by bob");
    string m4_key1 = marshallBinary(am->getKey(m4));
    string s6_key4 = marshallBinary(am->getKey(s6));
    record(tc, s6_key1.compare(s6_key4) == 0, test+"s6 key does not match 1v4");
    am->removePsswd(bob);
    record(tc, (am->getKey(m4)).length() > 0, test+"m4 key does not exist as orphan");
    record(tc, (am->getKey(s6)).length() > 0, test+"s6 key does not exist a child of an orphan");
    string m4_key2 = marshallBinary(am->getKey(m4));
    string s6_key5 = marshallBinary(am->getKey(s6));
    record(tc, s6_key1.compare(s6_key5) == 0, test+"s6 key does not match 1v5");
    record(tc, m4_key1.compare(m4_key2) == 0, test+"m4 key does not match 1v2");


    am->insert(m5,s5);
    am->insert(m5,s7);
    string m5_key = am->getKey(m5);
    string s5_key = am->getKey(s5);
    string s7_key = am->getKey(s7);
    record(tc, m5_key.length() > 0, "message 5 key (orphan) not available");
    record(tc, s5_key.length() > 0, "subject 5 key (orphan) not available");
    record(tc, s7_key.length() > 0, "subject 7 key (orphan) not available");
    string m5_key1 = marshallBinary(m5_key);
    string s5_key1 = marshallBinary(s5_key);
    string s7_key1 = marshallBinary(s7_key);
    am->insert(u1,m5);
    m5_key = am->getKey(m5);
    s5_key = am->getKey(s5);
    s7_key = am->getKey(s7);
    assert_s((am->getKey(alice)).length() == 0, "alice is not logged off");
    record(tc, m5_key.length() == 0, test+"message 5 key available with alice not logged on");
    record(tc, s5_key.length() == 0, test+"subject 5 key available with alice not logged on");
    record(tc, s7_key.length() == 0, test+"subject 7 key available with alice not logged on");
    am->insertPsswd(alice,secretA);
    m5_key = am->getKey(m5);
    s5_key = am->getKey(s5);
    s7_key = am->getKey(s7);
    string m5_key2 = marshallBinary(m5_key);
    string s5_key2 = marshallBinary(s5_key);
    string s7_key2 = marshallBinary(s7_key);
    record(tc, m5_key.length() > 0, test+"message 5 key not available with alice logged on");
    record(tc, m5_key1.compare(m5_key2) == 0, "message 5 key is different");
    record(tc, s5_key.length() > 0, test+"subject 5 key not available with alice logged on");
    record(tc, s5_key1.compare(s5_key2) == 0, "subject 5 key is different");
    record(tc, s7_key.length() > 0, test+"subject 7 key not available with alice logged on");
    record(tc, s7_key1.compare(s7_key2) == 0, "subject 7 key is different");


    am->insert(u3,m15);
    string m15_key = am->getKey(m15);
    record(tc, m15_key.length() > 0, test+"cannot access message 15 key (orphan)");
    string m15_key1 = marshallBinary(m15_key);
    string u3_key = am->getKey(u3);
    record(tc, u3_key.length() > 0, test+"cannot access user 3 key (orphan)");
    string u3_key1 = marshallBinary(u3_key);
    am->insert(m15, s24);
    string s24_key = am->getKey(s24);
    record(tc, s24_key.length() > 0, test+"cannot access subject 24 key (orphan)");
    string s24_key1 = marshallBinary(s24_key);
    am->insertPsswd(chris, secretC);
    string chris_key = am->getKey(chris);
    record(tc, chris_key.length() > 0, test+"cannot access chris key with chris logged on");
    string chris_key1 = marshallBinary(chris_key);
    am->insert(chris, u3);
    chris_key = am->getKey(chris);
    record(tc, chris_key.length() > 0, test+"cannot access chris key after chris->u3 insert");
    string chris_key2 = marshallBinary(chris_key);
    record(tc, chris_key1.compare(chris_key2) == 0,
             test+"chris key is different for orphan and chris logged on");

    am->removePsswd(chris);
    record(tc, (am->getKey(chris)).length() == 0, test+"can access chris key with chris offline");
    record(tc, (am->getKey(u3)).length() == 0, test+"can access user 3 key with chris offline");
    record(tc, (am->getKey(m15)).length() == 0, test+"can access message 15 key with chris offline");
    record(tc, (am->getKey(s24)).length() == 0, test+"can access subject 24 key with chris offline");

    am->insertPsswd(chris,secretC);
    chris_key = am->getKey(chris);
    record(tc, chris_key.length() > 0,
           test+"cannot access chris key with chris logged on after logging off");
    string chris_key3 = marshallBinary(chris_key);
    record(tc, chris_key1.compare(chris_key3) == 0,
           test+"chris key is different for orphan and chris logged on after logging off");
    u3_key = am->getKey(u3);
    record(tc, u3_key.length() > 0,
           test+"cannot access user 3 key with chris logged on after logging off");
    string u3_key2 = marshallBinary(u3_key);
    record(tc, u3_key1.compare(u3_key2) == 0,
           test+"user 3 key is different for orphan and chris logged on after logging off");
    m15_key = am->getKey(m15);
    record(tc, m15_key.length() > 0,
           test+"cannot access message 15 key with chris logged on after logging off");
    string m15_key2 = marshallBinary(m15_key);
    record(tc, m15_key1.compare(m15_key2) == 0,
           test+"message 15 key is different for orphan and chris logged on after logging off");
    s24_key = am->getKey(s24);
    record(tc, s24_key.length() > 0,
           test+"cannot access subject 24 key with chris logged on after logging off");
    string s24_key2 = marshallBinary(s24_key);
    record(tc, s24_key1.compare(s24_key2) == 0,
           test+"subject 24 key is different for orphan and chris logged on after logging off");

    string s16_key = am->getKey(s16);
    string s16_key1 = marshallBinary(s16_key);
    record(tc, s16_key.length() > 0, test+"orphan subject 16 did not get a key generated for it");
    am->insert(m15, s16);
    s16_key = am->getKey(s16);
    string s16_key2 = marshallBinary(s16_key);
    record(tc, s16_key.length() > 0, test+"subject 16 does not have key being de-orphanized");
    record(tc, s16_key1.compare(s16_key2) == 0,
           test+"subject 16 has a different key after being orphanized");
    am->removePsswd(chris);
    record(tc, (am->getKey(s16)).length() == 0, test+"can access subject 16 key with chris offline");
    am->insertPsswd(chris, secretC);

    s16_key = am->getKey(s16);
    string s16_key3 = marshallBinary(s16_key);
    record(tc, s16_key.length() > 0,
           test+"subject 16 does not have key after chris logs off and on");
    record(tc, s16_key1.compare(s16_key2) == 0,
    test+"subject 16 has a different key after chris logs out and back in");
}

static void
testRemove(const TestConfig &tc, KeyAccess * am) {
    string test = "(remove) ";
    buildAll(am);
    string m4_key = am->getKey(m4);
    record(tc, m4_key.length() > 0, test+"message 4 key (orphan) not available first");

    //bob
    am->insertPsswd(bob,secretB);
    string s4_key = am->getKey(s4);
    record(tc, s4_key.length() > 0, test+"cannot access subject 4 key with bob logged on");
    string s4_key1 = marshallBinary(s4_key);
    string s6_key = am->getKey(s6);
    record(tc, s6_key.length() > 0, test+"cannot access subject 6 key with bob logged on");
    string s6_key1 = marshallBinary(s6_key);
    string m2_key = am->getKey(m2);
    record(tc, m2_key.length() > 0, test+"cannot access message 2 key with bob logged on");
    string m2_key1 = marshallBinary(m2_key);
    string m3_key = am->getKey(m3);
    record(tc, m3_key.length() > 0, test+"cannot access message 3 key with bob logged on");
    string m3_key1 = marshallBinary(m3_key);
    string u2_key = am->getKey(u2);
    record(tc, u2_key.length() > 0, test+ "cannot access user 2 key with bob logged on");
    string u2_key1 = marshallBinary(u2_key);
    m4_key = am->getKey(m4);
    record(tc, m4_key.length() > 0, test+"message 4 key (orphan) not available 1");

    string g5_key = am->getKey(g5);
    record(tc, g5_key.length() > 0, test+"cannot access group 5 key with bob logged on");
    string g5_key1 = marshallBinary(g5_key);
    string f2_key = am->getKey(f2);
    record(tc, f2_key.length() > 0, test+"cannot access forum 2 key with bob logged on");
    string f2_key1 = marshallBinary(f2_key);
    string f3_key = am->getKey(f3);
    record(tc, f3_key.length() > 0, test+"cannot access forum 3 key with bob logged on");
    string f3_key1 = marshallBinary(f3_key);
    string a5_key = am->getKey(a5);
    record(tc, a5_key.length() > 0, test+"cannot access account 5 key with bob logged on");
    string a5_key1 = marshallBinary(a5_key);
    string u1_key = am->getKey(u1);
    record(tc, u1_key.length() == 0, test+"user 1 key available when Alice not logged on");
    //alice, bob
    am->insertPsswd(alice,secretA);
    m4_key = am->getKey(m4);
    record(tc, m4_key.length() > 0, test+"message 4 key (orphan) not available 2");

    am->remove(u2, g5);
    s4_key = am->getKey(s4);
    record(tc, s4_key.length() > 0, test+"cannot access subject 4 key with bob logged on");
    string s4_key5 = marshallBinary(s4_key);
    record(tc, s4_key1.compare(s4_key5) == 0, test+"Subject 4 has changed");
    s6_key = am->getKey(s6);
    record(tc, s6_key.length() > 0, test+"cannot access subject 6 key with bob logged on");
    string s6_key6 = marshallBinary(s6_key);
    record(tc, s6_key1.compare(s6_key6) == 0, test+"Subject 6 has changed");
    m2_key = am->getKey(m2);
    record(tc, m2_key.length() > 0, test+"cannot access message 2 key with bob logged on");
    string m2_key4 = marshallBinary(m2_key);
    record(tc, m2_key1.compare(m2_key4) == 0, "Message 2 has changed");
    m3_key = am->getKey(m3);
    record(tc, m3_key.length() > 0, test+"cannot access message 3 key with bob logged on");
    string m3_key5 = marshallBinary(m3_key);
    record(tc, m3_key1.compare(m3_key5) == 0, test+"Message 3 has changed");
    g5_key = am->getKey(g5);
    record(tc, g5_key.length() > 0, test+"cannot access group 5 key with alice logged on");
    string g5_key2 = marshallBinary(g5_key);
    record(tc, g5_key1.compare(g5_key2) == 0, test+"Group 5 key has changed");
    f2_key = am->getKey(f2);
    record(tc, f2_key.length() > 0, test+"cannot access forum 2 key with alice logged on");
    string f2_key5 = marshallBinary(f2_key);
    record(tc, f2_key1.compare(f2_key5) == 0, test+"Forum 2 key has changed");
    f3_key = am->getKey(f3);
    record(tc, f3_key.length() > 0, test+"cannot access forum 3 key with alice logged on");
    string f3_key4 = marshallBinary(f3_key);
    record(tc, f3_key1.compare(f3_key4) == 0, test+"Forum 3 key has changed");
    a5_key = am->getKey(a5);
    record(tc, a5_key.length() > 0, test+"cannot access account 5 key with alice logged on");
    string a5_key4 = marshallBinary(a5_key);
    record(tc, a5_key1.compare(a5_key4) == 0, test+"Account 5 key has changed");
    m4_key = am->getKey(m4);
    record(tc, m4_key.length() > 0, test+"message 4 key (orphan) not available 3");

    //bob
    am->removePsswd(alice);
    g5_key = am->getKey(g5);
    record(tc, g5_key.length() == 0, test+"group 5 key available when alice is logged off");
    a5_key = am->getKey(a5);
    record(tc, a5_key.length() == 0, test+"account 5 key available when alice is logged off");
    f2_key = am->getKey(f2);
    record(tc, f2_key.length() == 0, test+"forum 2 key available when alice is logged off");
    f3_key = am->getKey(f3);
    record(tc, f3_key.length() == 0, test+"forum 3 key available when alice is logged off");
    m4_key = am->getKey(m4);
    record(tc, m4_key.length() > 0, test+"message 4 key (orphan) not available 4");

    //bob, chris
    am->insertPsswd(chris, secretC);
    string s24_key = am->getKey(s24);
    record(tc, s24_key.length() > 0, test+"subject 24 key is not accessible with chris logged on");
    string s24_key1 = marshallBinary(s24_key);
    string m15_key = am->getKey(m15);
    record(tc, m15_key.length() > 0, test+"message 15 key is not accessible with chris logged on");
    string m15_key3 = marshallBinary(m15_key);
    string u3_key = am->getKey(u3);
    record(tc, u3_key.length() > 0, test+"user 3 key is not accessible with chris logged on");
    string u3_key1 = marshallBinary(u3_key);

    am->remove(u3,m15);
    s24_key = am->getKey(s24);
    record(tc, s24_key.length() == 0, test+"subject 24 key is accessible after removal");
    m15_key = am->getKey(m15);
    record(tc, m15_key.length() == 0, test+"message 15 key is accessible after removal");
    u3_key = am->getKey(u3);
    record(tc, u3_key.length() > 0, test+"user 3 key is not accessible with chris after u3->m15 removal");
    string u3_key4 = marshallBinary(u3_key);
    record(tc, u3_key1.compare(
                             u3_key4) == 0,
             test+"user 3 key is not the same after u3->m15 removal");
    m4_key = am->getKey(m4);
    record(tc, m4_key.length() > 0, test+"message 4 key (orphan) not available 5");


    am->remove(g5,f3);
    //alice, bob, chris
    am->insertPsswd(alice, secretA);
    g5_key = am->getKey(g5);
    record(tc, g5_key.length() > 0, test+"cannot access group 5 key with alice logged on");
    string g5_key3 = marshallBinary(g5_key);
    record(tc, g5_key1.compare(g5_key3) == 0, test+"Group 5 key has changed");
    f2_key = am->getKey(f2);
    record(tc, f2_key.length() > 0, test+"cannot access forum 2 key with alice logged on");
    string f2_key6 = marshallBinary(f2_key);
    record(tc, f2_key1.compare(f2_key6) == 0, test+"Forum 2 key has changed");
    a5_key = am->getKey(a5);
    record(tc, a5_key.length() > 0, test+"cannot access account 5 key with alice logged on");
    string a5_key5 = marshallBinary(a5_key);
    record(tc, a5_key1.compare(a5_key5) == 0, test+"Account 5 key has changed");
    g5_key = am->getKey(g5);
    f3_key = am->getKey(f3);
    record(tc, f3_key.length() == 0, test+"forum 3 key available when alice is logged off");

    //alice, chris
    am->removePsswd(bob);
    s6_key = am->getKey(s6);
    record(tc, s6_key.length() > 0, test+"subject 6 key, attached to orphan m4 not accessible");
    string s6_key7 = marshallBinary(s6_key);
    record(tc, s6_key1.compare(s6_key7) == 0, test+"subject 6 key has changed");
    m4_key = am->getKey(m4);
    record(tc, m4_key.length() > 0, test+"message 4 key (orphan) not available last");
    string m4_key1 = marshallBinary(m4_key);

    am->remove(m4,s6);
    m3_key = am->getKey(m3);
    record(tc, m3_key.length() == 0, test+"message 3 key available when bob is logged off");
    m2_key = am->getKey(m2);
    record(tc, m2_key.length() == 0, test+"message 2 key available when bob is logged off");
    s6_key = am->getKey(s6);
    record(tc, s6_key.length() == 0, test+"subject 6 key available when bob is logged off");
    m4_key = am->getKey(m4);
    record(tc, m4_key.length() > 0, test+"message 4 key (orphan) not available after remove");
    string m4_key4 = marshallBinary(m4_key);
    record(tc, m4_key1.compare(
                               m4_key4) == 0, test+"message 4 key has changed after remove");
}

static void
testThreshold(const TestConfig &tc, KeyAccess * am) {
    string test = "(threshold) ";
    buildBasic(am);
    am->insertPsswd(chris,secretC);
    am->insert(chris, u3);
    am->insert(u3,g2);
    string ml50_key1;
    string ml50_key;
    for (unsigned int i = 1; i < 110; i++) {
        Prin ml = Prin("x.mailing_list", strFromVal(i));
        am->insert(g2, ml);
        if(i == 50) {
            ml50_key = am->getKey(ml50);
            record(tc, ml50_key.length() > 0,
                     test+"could not access ml50 key just after it's inserted");
            ml50_key1 = marshallBinary(ml50_key);
        }
    }

    am->removePsswd(chris);
    ml50_key = am->getKey(ml50);
    record(tc, ml50_key.length() == 0, test+"ml50 key available after chris logs off");
    am->insertPsswd(chris, secretC);
    record(tc, am->getKey(u3).length() != 0, test+"can't access u3 key after chris logs back on");
    PrinKey ml50_pkey = am->getUncached(ml50);
    record(tc,ml50_pkey.key.length() != 0,
           test+"can't access ml50 key after chris logs back on");
    ml50_key = am->getKey(ml50);
    string ml50_key2 = marshallBinary(ml50_key);
    record(tc, ml50_key1.compare(
                                ml50_key2) == 0,
           test+"ml 50 key is different after chris logs on and off");

    for (unsigned int i = 6; i < 110; i++) {
        Prin ml = Prin("x.mailing_list", strFromVal(i));
        am->remove(g2,ml);
    }

    ml50_key = am->getKey(ml50);
    record(tc, ml50_key.length() == 0,
             "ml50 key exists after the hundred group keys have been removed");
}

void
TestAccessManager::run(const TestConfig &tc, int argc, char ** argv)
{
    cerr << "testing meta locally..." << endl;
    testMeta_native(tc, new Connect(tc.host, tc.user, tc.pass, tc.db));

    KeyAccess * ka;
    ka = buildTest(new Connect(tc.host, tc.user, tc.pass, tc.db));

    cerr << "testing meta section of KeyAccess..." << endl;
    testMeta(tc, ka);

    cerr << "single user tests..." << endl;
    testSingleUser(tc, ka);

    delete ka;
    ka = buildTest(new Connect(tc.host, tc.user, tc.pass, tc.db));
    cerr << "multi user tests..." << endl;
    testMultiBasic(tc, ka);

    delete ka;
    ka = buildTest(new Connect(tc.host, tc.user, tc.pass, tc.db));
    cerr << "acyclic graphs (not a tree) tests..." << endl;
    testNonTree(tc, ka);

    ka->~KeyAccess();
    ka = buildTest(new Connect(tc.host, tc.user, tc.pass, tc.db));
    cerr << "orphan tests..." << endl;
    testOrphans(tc, ka);

    ka->~KeyAccess();
    ka = buildTest(new Connect(tc.host, tc.user, tc.pass, tc.db));
    cerr << "remove tests..." << endl;
    testRemove(tc, ka);

    ka->~KeyAccess();
    ka = buildTest(new Connect(tc.host, tc.user, tc.pass, tc.db));
    cerr << "threshold tests... (this may take a while)" << endl;
    testThreshold(tc, ka);

    /*ka->~KeyAccess();
    ka = buildTest(new Connect(tc.host, tc.user, tc.pass, tc.db));
    cerr << "altering meta post data intertion tests..." << endl;
    testMetaAlterations(tc, ka);*/

    cerr << "RESULT: " << npass << "/" << ntest << " passed" << endl;

    delete ka;
}

