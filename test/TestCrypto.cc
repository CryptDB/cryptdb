/*
 * TestCrypto
 * -- tests crypto work; migrated from test.cc
 *
 *
 */

#include <util/cryptdb_log.hh>
#include <crypto/pbkdf2.hh>
#include <crypto/ECJoin.hh>
#include <test/TestCrypto.hh>

using namespace NTL;

TestCrypto::TestCrypto()
{
}

TestCrypto::~TestCrypto()
{
}

static void
testBasics()
{
    enum { nround = 10000 };
    for (uint i = 0; i < nround; i++) {
        size_t len = randomValue() % 1024;
        string plaintext = randomBytes((uint) len);

        string secretKey = randomBytes(16);
        string salt = randomBytes(16);

        AES_KEY * encKey = get_AES_enc_key(secretKey);
        AES_KEY * decKey = get_AES_dec_key(secretKey);

        string enc = encrypt_AES_CBC(plaintext, encKey, salt);
        string dec = decrypt_AES_CBC(enc, decKey, salt);

        assert_s(dec == plaintext, "CBC encryption failed");

        enc = encrypt_AES_CMC(plaintext, encKey);
        dec = decrypt_AES_CMC(enc, decKey);

        assert_s(dec == plaintext, "CMC encryption failed");

        delete encKey;
        delete decKey;
    }

    for (uint i = 0; i < nround; i++) {
        // our blowfish is hacked up to do just 60 bits rather than 64
        uint64_t plaintext = randomValue() >> 4;
        string key = randomBytes(16);
        blowfish k(key);
        uint64_t c = k.encrypt(plaintext);
        uint64_t p2 = k.decrypt(c);

        if (plaintext != p2)
            cerr << "plaintext:  " << hex << plaintext << endl
                 << "ciphertext: " << hex << c << endl
                 << "plaintext2: " << hex << p2 << endl;
        assert_s(plaintext == p2, "BF enc/dec failed");
    }

}


static void
testOnions () {



}

static void
testOPE()
{

    LOG(crypto) << "Test OPE";

    unsigned int noSizes = 6;
    unsigned int plaintextSizes[] = {16, 32, 64,  128, 256, 512, 1024};
    unsigned int ciphertextSizes[] = {32, 64, 128, 256, 288, 768, 1536};

    unsigned int noValues = 100;

    string key = "secret aes key!!";

    for (unsigned int i = 0; i < noSizes; i++) {
        unsigned int ptextsize = plaintextSizes[i];
        unsigned int ctextsize =  ciphertextSizes[i];
        LOG(crypto) << "-- testing plaintext size " << ptextsize << " and ciphertext size " << ctextsize;

        OPE * ope = new OPE(string(key, AES_KEY_BYTES), ptextsize, ctextsize);

        //Test it on "noValues" random values
        for (unsigned int j = 0; j < noValues; j++) {

            string data = randomBytes(ptextsize/bitsPerByte);

	    // cerr << "data is " << stringToByteInts(data) << "\n";

            string enc = ope->encrypt(data);
	    //cerr << "enc\n";
	    string dec = ope->decrypt(enc);
	    //cerr << "Dec \n";

            //cerr << "enc is " << stringToByteInts(enc) << "\n";
            //cerr << "dec is " << stringToByteInts(dec) << "\n";
            assert_s(valFromStr(dec) == valFromStr(data), "decryption does not match original data "  + StringFromVal(ptextsize) + " " + StringFromVal(ctextsize));
	    //cerr << "After assertions \n";

        }

    }

    LOG(crypto) << "OPE Test OK \n";
}

static void
testHGD()
{
    ZZ sum0 = to_ZZ(0);
    ZZ sum1 = to_ZZ(0);

    enum { nrounds = 1000 };
    ZZ marked_balls = to_ZZ(12);

    for (int i = 0; i < nrounds; i++) {
        ZZ seed = RandomBits_ZZ(256);
        ZZ total_balls = to_ZZ(0x40000000);
        ZZ sample0 = HGD(total_balls/2, marked_balls, total_balls-marked_balls,
                         seed, 256, 100);
        ZZ sample1 = HGD(total_balls/2-1, marked_balls, total_balls-marked_balls,
                        seed, 256, 100);
        // cout << "sample0: " << sample0 << endl;
        // cout << "sample1: " << sample1 << endl;
        sum0 += sample0;
        sum1 += sample1;
    }

    if (sum0 == 0 || sum1 == 0 ||
        sum0 == nrounds*marked_balls || sum1 == nrounds*marked_balls)
    {
        cerr << "HGD is broken, with high probability: "
             << nrounds << " "
             << marked_balls << " "
             << sum0 << " "
             << sum1 << endl;
    }
}

static void
testPKCS()
{

}

static void
printhex(const char *s, const u_int8_t *buf, size_t len)
{
    size_t i;

    printf("%s: ", s);
    for (i = 0; i < len; i++)
        printf("%02x", buf[i]);
    printf("\n");
    fflush(stdout);
}

static void
testPBKDF2(void)
{
    /*
     * Test vectors from RFC 3962
     */
    static struct test_vector {
        u_int rounds;
        const char *pass;
        const char *salt;
        const unsigned char expected[32];
    } test_vectors[] = {
        {
            1,
            "password",
            "ATHENA.MIT.EDUraeburn",
            {
                0xcd, 0xed, 0xb5, 0x28, 0x1b, 0xb2, 0xf8, 0x01,
                0x56, 0x5a, 0x11, 0x22, 0xb2, 0x56, 0x35, 0x15,
                0x0a, 0xd1, 0xf7, 0xa0, 0x4b, 0xb9, 0xf3, 0xa3,
                0x33, 0xec, 0xc0, 0xe2, 0xe1, 0xf7, 0x08, 0x37
            },
        }, {
            2,
            "password",
            "ATHENA.MIT.EDUraeburn",
            {
                0x01, 0xdb, 0xee, 0x7f, 0x4a, 0x9e, 0x24, 0x3e,
                0x98, 0x8b, 0x62, 0xc7, 0x3c, 0xda, 0x93, 0x5d,
                0xa0, 0x53, 0x78, 0xb9, 0x32, 0x44, 0xec, 0x8f,
                0x48, 0xa9, 0x9e, 0x61, 0xad, 0x79, 0x9d, 0x86
            },
        }, {
            1200,
            "password",
            "ATHENA.MIT.EDUraeburn",
            {
                0x5c, 0x08, 0xeb, 0x61, 0xfd, 0xf7, 0x1e, 0x4e,
                0x4e, 0xc3, 0xcf, 0x6b, 0xa1, 0xf5, 0x51, 0x2b,
                0xa7, 0xe5, 0x2d, 0xdb, 0xc5, 0xe5, 0x14, 0x2f,
                0x70, 0x8a, 0x31, 0xe2, 0xe6, 0x2b, 0x1e, 0x13
            },
        }, {
            5,
            "password",
            "\0224VxxV4\022",     /* 0x1234567878563412 */
            {
                0xd1, 0xda, 0xa7, 0x86, 0x15, 0xf2, 0x87, 0xe6,
                0xa1, 0xc8, 0xb1, 0x20, 0xd7, 0x06, 0x2a, 0x49,
                0x3f, 0x98, 0xd2, 0x03, 0xe6, 0xbe, 0x49, 0xa6,
                0xad, 0xf4, 0xfa, 0x57, 0x4b, 0x6e, 0x64, 0xee
            },
        }, {
            1200,
            "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
            "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
            "pass phrase equals block size",
            {
                0x13, 0x9c, 0x30, 0xc0, 0x96, 0x6b, 0xc3, 0x2b,
                0xa5, 0x5f, 0xdb, 0xf2, 0x12, 0x53, 0x0a, 0xc9,
                0xc5, 0xec, 0x59, 0xf1, 0xa4, 0x52, 0xf5, 0xcc,
                0x9a, 0xd9, 0x40, 0xfe, 0xa0, 0x59, 0x8e, 0xd1
            },
        }, {
            1200,
            "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
            "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
            "pass phrase exceeds block size",
            {
                0x9c, 0xca, 0xd6, 0xd4, 0x68, 0x77, 0x0c, 0xd5,
                0x1b, 0x10, 0xe6, 0xa6, 0x87, 0x21, 0xbe, 0x61,
                0x1a, 0x8b, 0x4d, 0x28, 0x26, 0x01, 0xdb, 0x3b,
                0x36, 0xbe, 0x92, 0x46, 0x91, 0x5e, 0xc8, 0x2a
            },
        }, {
            50,
            "\360\235\204\236",     /* g-clef (0xf09d849e) */
            "EXAMPLE.COMpianist",
            {
                0x6b, 0x9c, 0xf2, 0x6d, 0x45, 0x45, 0x5a, 0x43,
                0xa5, 0xb8, 0xbb, 0x27, 0x6a, 0x40, 0x3b, 0x39,
                0xe7, 0xfe, 0x37, 0xa0, 0xc4, 0x1e, 0x02, 0xc2,
                0x81, 0xff, 0x30, 0x69, 0xe1, 0xe9, 0x4f, 0x52
            },
        }
    };

    for (uint i = 0; i < sizeof(test_vectors) / sizeof(test_vectors[0]);
         i++) {
        struct test_vector *vec = &test_vectors[i];
        LOG(test) << "vector " << i;
        for (uint j = 1; j < 32; j += 3) {
            string k = pbkdf2(string(vec->pass),
                              string(vec->salt),
                              j, vec->rounds);
            if (memcmp(&k[0], vec->expected, j) != 0) {
                printhex(" got", (uint8_t *) &k[0], j);
                printhex("want", vec->expected, j);
                cerr << "pbkdf2 mismatch\n";
            } else {
                // LOG(test) << "pbkdf2 " << i << " " << j << " ok";
            }
        }
    }
}

unsigned int * to_vec(const list<unsigned int> & lst);

unsigned int *
to_vec(const list<unsigned int> & lst)
{
    unsigned int * vec = new unsigned int[lst.size()];

    unsigned int index = 0;
    for (list<unsigned int>::const_iterator it = lst.begin(); it != lst.end();
         it++) {
        vec[index] = *it;
        index++;
    }

    return vec;
}

static void
testSWPSearch()
{
    LOG(test) << "   -- test Song-Wagner-Perrig crypto ...";

    Binary mediumtext = Binary::toBinary("hello world!");
    Binary smalltext = Binary::toBinary("hi");
    Binary emptytext = Binary::toBinary("");
    Binary exacttext = Binary::toBinary("123456789012345");

    LOG(test) << "        + test encrypt/decrypt";

    list<Binary> lst = {mediumtext, smalltext, emptytext, exacttext};

    Binary key = Binary::toBinary("this is a secret key").subbinary(0, 16);

    //test encryption/decryption

    list<Binary> * result = SWP::encrypt(key, lst);
    list<Binary> * decs = SWP::decrypt(key, *result);

    list<Binary>::iterator lstit = lst.begin();

    unsigned int index = 0;
    for (list<Binary>::iterator it = decs->begin(); it!=decs->end(); it++) {
        assert_s((*it) == (*lstit), "incorrect decryption at " +
                 StringFromVal(
                     index));
        index++;
        lstit++;
    }

    //test searchability

    LOG(test) << "        + test searchability";

    Binary word1 = Binary::toBinary("ana");
    Binary word2 = Binary::toBinary("dana");
    Binary word3 = Binary::toBinary("n");
    Binary word4 = Binary::toBinary("");
    Binary word5 = Binary::toBinary("123ana");

    list<Binary> vec1 =
    {word1, word2, word1, word3, word4, word5, word1, word1};
    list<Binary> vec2 = {};
    list<Binary> vec3 = {word2, word3, word4, word5};
    list<Binary> vec4 = {word1};

    list<Binary> * encs = SWP::encrypt(key, vec1);
    Token token = SWP::token(key, word1);

    list<unsigned int> * indexes = SWP::search(token, *encs);
    assert_s(indexes->size() == 4,
             string(
                 "incorrect number of findings in vec1, expected 4, returned ")
             +
             StringFromVal(indexes->size()));
    unsigned int * vec_ind = to_vec(*indexes);
    assert_s(vec_ind[0] == 0, "incorrect index found for entry 0");
    assert_s(vec_ind[1] == 2, "incorrect index found for entry 1");
    assert_s(vec_ind[2] == 6, "incorrect index found for entry 2");
    assert_s(vec_ind[3] == 7, "incorrect index found for entry 3");

    indexes = SWP::search(SWP::token(key, word1), *SWP::encrypt(key, vec2));
    assert_s(indexes->size() == 0, "incorrect number of findings in vec2");

    indexes = SWP::search(SWP::token(key, word1), *SWP::encrypt(key, vec3));
    assert_s(indexes->size() == 0, "incorrect number of findings in vec3");

    indexes = SWP::search(SWP::token(key, word1), *SWP::encrypt(key, vec4));
    assert_s(indexes->size() == 1, "incorrect number of findings in vec4");
    assert_s(
        indexes->front() == 0, "incorrect index found for entry 0 in vec4");

    //test encrypt/decrypt wrappers

    LOG(test) << "        + test wrappers";

    list<Binary> lstw = {mediumtext, smalltext, emptytext,  exacttext};

    Binary encw = CryptoManager::encryptSWP(key, lstw);
    list<Binary> * decw = CryptoManager::decryptSWP(key, encw);

    list<Binary>::iterator wit = lstw.begin();

    index = 0;
    for (list<Binary>::iterator it = decw->begin(); it!=decw->end(); it++) {
        assert_s((*it) == (*wit), "incorrect decryption at " +
                 StringFromVal(index));
        index++;
        wit++;
    }

    //test searchability

    Binary overall_ciph = CryptoManager::encryptSWP(key, vec1);
    token = CryptoManager::token(key, word1);

    indexes = CryptoManager::searchSWP(token, overall_ciph);
    assert_s(indexes->size() == 4,
             string(
                 "incorrect number of findings in vec1, expected 4, returned ")
             +
             StringFromVal(indexes->size()));
    vec_ind = to_vec(*indexes);
    assert_s(vec_ind[0] == 0, "incorrect index found for entry 0");
    assert_s(vec_ind[1] == 2, "incorrect index found for entry 1");
    assert_s(vec_ind[2] == 6, "incorrect index found for entry 2");
    assert_s(vec_ind[3] == 7, "incorrect index found for entry 3");
    assert_s(CryptoManager::searchExists(CryptoManager::token(key,
                                                              word1),
                                         CryptoManager::encryptSWP(key,
                                                                   vec1)),
             "incorrect found flad in vec2");

    indexes = CryptoManager::searchSWP(CryptoManager::token(key,
                                                            word1),
                                       CryptoManager::encryptSWP(key, vec2));
    assert_s(
        indexes != NULL && indexes->size() == 0,
        "incorrect number of findings in vec2");
    assert_s(!CryptoManager::searchExists(CryptoManager::token(key,
                                                               word1),
                                          CryptoManager::encryptSWP(key,
                                                                    vec2)),
             "incorrect found flad in vec2");

    indexes = CryptoManager::searchSWP(CryptoManager::token(key,
                                                            word1),
                                       CryptoManager::encryptSWP(key, vec3));
    assert_s(
        indexes != NULL && indexes->size() == 0,
        "incorrect number of findings in vec3");
    assert_s(!CryptoManager::searchExists(CryptoManager::token(key,
                                                               word1),
                                          CryptoManager::encryptSWP(key,
                                                                    vec3)),
             "incorrect found flag in vec3");

    indexes = CryptoManager::searchSWP(CryptoManager::token(key,
                                                            word1),
                                       CryptoManager::encryptSWP(key, vec4));
    assert_s(
        indexes != NULL && indexes->size() == 1,
        "incorrect number of findings in vec4");
    assert_s(
        indexes->front() == 0, "incorrect index found for entry 0 in vec4");
    assert_s(CryptoManager::searchExists(CryptoManager::token(key,
                                                              word1),
                                         CryptoManager::encryptSWP(key,
                                                                   vec4)),
             "incorrect found flag in vec4");

    LOG(test) << "   -- OK";



}

static void
testECJoin() {

       LOG(test) << "   -- EC setup";

    ECJoin * ecj = new ECJoin();

    AES_KEY * baseKey = get_AES_KEY("secret key master");
    ECJoinSK * sk1 = ecj->getSKey(baseKey, "secret key for col 1");
    ECJoinSK * sk2 = ecj->getSKey(baseKey, "secret key for col 2");
    ECJoinSK * sk3 = ecj->getSKey(baseKey, "secret key for col 3");

    string data1 = "hello world";
    string data2 = "data2";
    string data3 = "hello world";

    /** test encryption **/

        LOG(test) << "   -- test EC encryption";

    string c1sk1 = ecj->encrypt(sk1, data1);
    string c1sk2 = ecj->encrypt(sk2, data1);
    string c2sk1 = ecj->encrypt(sk1, data2);
    string c2sk2 = ecj->encrypt(sk2, data2);
    string c3sk1 = ecj->encrypt(sk1, data3);
    string c3sk2 = ecj->encrypt(sk2, data3);

    assert_s(c1sk1 != c1sk2, "encryption returns same thing for two different keys");
    assert_s(c1sk1 != c2sk1, "encryption the same for two different items");
    assert_s(c1sk1 == c3sk1, "encryption is different for the same value");
    assert_s(c1sk2 == c3sk2, "encryption is different for the same value");


    /*** test adjustability **/

    LOG(test) << "   -- adjust forward";

    ECDeltaSK * delta = ecj->getDeltaKey(sk1, sk2);

    /* adjust from k1 --> k2 */

    string c1sk1TOsk2 = ECJoin::adjust(delta, c1sk1);
    string c3sk1TOsk2 = ECJoin::adjust(delta, c3sk1);

    assert_s(c1sk1TOsk2 == c1sk2, "adjusting does not work properly");
    assert_s(c1sk1TOsk2 != c2sk2, "adjusting is incorrect");
    assert_s(c1sk1TOsk2 == c3sk2, "adjusting does not work well");
    assert_s(c3sk1TOsk2 == c1sk2, "adjusting does not work correctly");


    /* test backwards adjustability k2 --> k1 */

        LOG(test) << "   -- adjust backward";

    ECDeltaSK * deltaBack = ecj->getDeltaKey(sk2, sk1);

    string c1sk2TOsk1 = ECJoin::adjust(deltaBack, c1sk2);
    string c3sk2TOsk1 = ECJoin::adjust(deltaBack, c3sk2);


    assert_s(c1sk2TOsk1 == c1sk1, "adjusting back incorrect");
    assert_s(c1sk2TOsk1 !=  c1sk2, "adjusting back incorrect");
    assert_s(c3sk2TOsk1 == c1sk1, "adjusting back is incorrect");


    string c1sk1TOsk2TOsk1 = ECJoin::adjust(deltaBack, c1sk1TOsk2);
    assert_s(c1sk1 == c1sk1TOsk2TOsk1, "adjusting forward/backward does not cancel");

    /* test composable adjustability k1 -> k2 -> k3 == k1 --> k3 */

        LOG(test) << "   -- adjust composability";

    ECDeltaSK * deltask1TOsk3 = ecj->getDeltaKey(sk1, sk3);
    ECDeltaSK * deltask2TOsk3 = ecj->getDeltaKey(sk2, sk3);

    string c1sk1TOsk2TOsk3 = ECJoin::adjust(deltask2TOsk3, c1sk1TOsk2);
    string c2sk1TOsk3 = ECJoin::adjust(deltask1TOsk3, c2sk1);
    string c3sk1TOsk3 = ECJoin::adjust(deltask1TOsk3, c3sk1);

    assert_s(c1sk1TOsk2TOsk3 == c3sk1TOsk3, "adjusting not composable");
    assert_s(c1sk1TOsk2TOsk3 != c2sk1TOsk3, "adjust composability flawed");

    delete delta;
    delete deltaBack;
    delete deltask1TOsk3;
    delete deltask2TOsk3;
    delete sk1;
    delete sk2;
    delete sk3;
    delete ecj;

    LOG(test) << "   -- done!";

}
/*
static void
testEncTables() {

    uint64_t mkey = 113341234;
    string masterKey = BytesFromInt(mkey, AES_KEY_BYTES);

    cerr << "testing encryption tables ...  \n";

    unsigned int low = 3500;
    unsigned int high = 3600;
    string fieldname = "table4.field2OPE";

    CryptoManager * cm1 = new CryptoManager(masterKey);
    CryptoManager * cm2 = new CryptoManager(masterKey);

    cm1->loadEncTables("enc_tables_w1");
    bool isBin;
    uint64_t salt = 38290385;

    for (unsigned int v = low; v < high; v++) {

        string data = StringFromVal(v);

        string enc1 = cm1->crypt(cm1->getmkey(), data, OLID_NUM, fieldname, SECLEVEL::PLAIN_OPE, SECLEVEL::OPE, isBin, 0);
        string enc2 = cm2->crypt(cm2->getmkey(), data, OLID_NUM, fieldname, SECLEVEL::PLAIN_OPE, SECLEVEL::OPE, isBin, 0);
        //cerr << "enc 1 is " << enc1 << " and enc2 " << enc2 << "\n";
        assert_s(enc1 == enc2, "enc with tables does not match enc without enc tables ");

        enc1 = cm1->crypt(cm1->getmkey(), StringFromVal(v), OLID_NUM, fieldname, SECLEVEL::PLAIN_OPE, SECLEVEL::SEMANTIC_OPE, isBin, salt);
        enc2 = cm2->crypt(cm2->getmkey(), StringFromVal(v), OLID_NUM, fieldname, SECLEVEL::PLAIN_OPE, SECLEVEL::SEMANTIC_OPE, isBin, salt);

        assert_s(enc1 == enc2, "enc with tables does not match enc without tables for whole ope onion");

        string dec1 = cm1->crypt(cm1->getmkey(), enc1, OLID_NUM, fieldname, SECLEVEL::SEMANTIC_OPE, SECLEVEL::PLAIN_OPE, isBin, salt);
        //cerr << "dec 1 is " << dec1 << "\n";
        assert_s(dec1 == data, "decryption with enc tables incorrect");

    }

    cerr << "DONE.\n";

}
*/

static void
latency_join(unsigned int notests) {
    ECJoin * ecj = new ECJoin();

        AES_KEY * baseKey = get_AES_KEY("secret key master");
        ECJoinSK * sk1 = ecj->getSKey(baseKey, "secret key for col 1");
        ECJoinSK * sk2 = ecj->getSKey(baseKey, "secret key for col 2");

        string data = "value";

        //eval encryption
        size_t prevent_compiler_optimiz = 0;
        string enc_sk1 = "";

        Timer t;

        for (unsigned int i = 0; i < notests ; i++) {
            enc_sk1 = ecj->encrypt(sk1, data);
            prevent_compiler_optimiz += enc_sk1.size();
        }

        double timeEnc = t.lap_ms() / notests;

        if (prevent_compiler_optimiz % 297973 == 0) {
            cerr << "lucky case\n";
        }

        //eval adjusting

        ECDeltaSK * delta = ecj->getDeltaKey(sk1, sk2);
        string enc_sk2 = ecj->encrypt(sk2, data);

        prevent_compiler_optimiz = 0;
        string enc_sk1TOsk2 = "";

        t.lap();

        for (unsigned int i = 0; i < notests ; i++) {
            enc_sk1TOsk2 = ECJoin::adjust(delta, enc_sk1);
            prevent_compiler_optimiz += enc_sk1TOsk2.size();
        }

        double timeJoin = t.lap_ms() / notests;

        if (prevent_compiler_optimiz % 297973 == 0) {
                cerr << "lucky case\n";
        }

        //just making sure everything worked
        assert_s(enc_sk1TOsk2 == enc_sk2, "something went wrong");

        cerr << "join encrypt " << timeEnc << "ms join adjust " << timeJoin << "ms \n";

}

static void
latency_search(unsigned int notests) {

    Binary key("secret key maste");

    list<Binary> * words = new list<Binary>();

    for (unsigned int i = 0; i < notests; i++) {
        words->push_back(Binary(randomBytes(SWPCiphSize-1)));
    }


    //eval encryption

    Timer t;

    list<Binary> * encs = SWP::encrypt(key, *words);

    double timeEnc = t.lap_ms() / notests;

    //eval decryption

    t.lap();

    list<Binary> * decs = SWP::decrypt(key, *encs);

    double timeDec = t.lap_ms() / notests;

    //sanity check
    assert_s(words->front() == decs->front(), "something went wrong");

    //evaluate search

    Token token = SWP::token(key, words->back());

    t.lap();

    list<unsigned int> * search_res = SWP::search(token, *encs);

    double timeSearch = t.lap_ms()/ notests;

    //sanity check
    assert_s(search_res->size() > 0 && search_res->back() == words->size() - 1, "did not find the word");

    cerr << "SWP encrypt " << timeEnc << "ms SWP decrypt " << timeDec << "ms SWP search " << timeSearch << "ms \n";

}

static void
testPaillier()
{
    for (uint i = 0; i < 10; i++) {
        CryptoManager cm(randomBytes(128));
        uint32_t v0 = randomValue() & 0xffffffff;
        uint32_t v1 = randomValue() & 0xffffffff;
        string c0 = cm.encrypt_Paillier(v0);
        string c1 = cm.encrypt_Paillier(v1);
        assert(v0 == (uint32_t) cm.decrypt_Paillier(c0));
        assert(v1 == (uint32_t) cm.decrypt_Paillier(c1));

        string cs = homomorphicAdd(c0, c1, cm.getPKInfo());
        assert(v0 + v1 == (uint32_t) cm.decrypt_Paillier(cs));
    }
}

static void
latency_Paillier(unsigned int notests, unsigned int notestsagg) {

    int data = 4389839;

    CryptoManager * cm = new CryptoManager("secret key maste");

    string enc;
    //eval encryption

    size_t prevent_compiler_optimiz = 0;

    Timer t;

    for (unsigned int i = 0; i < notests ; i++) {
        enc = cm->encrypt_Paillier(data);
        prevent_compiler_optimiz += enc.size();
    }

    double timeEnc = t.lap_ms() / notests;

    if (prevent_compiler_optimiz % 297973 == 0) {
        cerr << "lucky case\n";
    }
    //eval decryption

    int dec;
    t.lap();

    for (unsigned int i = 0; i < notests ; i++) {
            dec = cm->decrypt_Paillier(enc);
            prevent_compiler_optimiz += dec % 100;
    }

    double timeDec = t.lap_ms() / notests;

    //sanity check
    assert_s(data == dec, "something went wrong");

    //evaluate aggregation

    string res;
    t.lap();

    ZZ a = ZZFromStringFast(padForZZ(enc));
    ZZ b = ZZFromStringFast(padForZZ(enc));
    ZZ n2 = ZZFromStringFast(padForZZ(cm->getPKInfo()));

    for (unsigned int i = 0; i < notestsagg ; i++) {
        a = MulMod(a, b, n2);
        prevent_compiler_optimiz += a.size();
    }

    double timeAdd = t.lap_ms()/ notestsagg;

    if (prevent_compiler_optimiz % 297973 == 0) {
        cerr << "lucky case\n";
    }

    cerr << "HOM encrypt " << timeEnc << "ms HOM decrypt " << timeDec << "ms HOM add "
            << timeAdd << "ms \n";

}


static void
latency_OPE(unsigned int notests) {

    uint32_t data = 4389839;

    CryptoManager * cm = new CryptoManager("secret key maste");

    uint64_t enc;
    //eval encryption

    uint64_t prevent_compiler_optimiz = 0;

    OPE * opeKey = cm->get_key_OPE("field0");

    Timer t;

    for (unsigned int i = 0; i < notests ; i++) {
        enc = cm->encrypt_OPE(data, opeKey);
        prevent_compiler_optimiz += enc % 10;
    }

    double timeEnc = t.lap_ms() / notests;

    if (prevent_compiler_optimiz % 297973 == 0) {
        cerr << "lucky case\n";
    }
    //eval decryption

    uint32_t dec;
    t.lap();

    for (unsigned int i = 0; i < notests ; i++) {
            dec = cm->decrypt_OPE(enc, opeKey);
            prevent_compiler_optimiz += dec % 10;
    }

    double timeDec = t.lap_ms() / notests;

    if (prevent_compiler_optimiz % 297973 == 0) {
        cerr << "lucky case\n";
    }
    //sanity check
    assert_s(data == dec, "something went wrong");

    //evaluate aggregation

   uint64_t a = 3289234;

    t.lap();


    for (unsigned int i = 0; i < notests ; i++) {

        prevent_compiler_optimiz += (a>enc);
    }

    double timeAdd = t.lap_ms()/ notests;

    if (prevent_compiler_optimiz % 297973 == 0) {
        cerr << "lucky case\n";
    }

    cerr << "OPE encrypt " << timeEnc << "ms OPE decrypt " << timeDec << "ms OPE compare "
            << timeAdd << "ms \n";

}



static void
latency_basics(unsigned int notests) {

    uint64_t int_data = 2742935345345384;
    string str_data = randomBytes(1024);
    string salt = "2742935345345384";
    uint64_t int_enc, int_dec;
    string   str_enc, str_dec;
    string key = "secret key maste";

    AES_KEY * enckey = get_AES_enc_key(key);
    AES_KEY * deckey = get_AES_dec_key(key);

    blowfish * bf = new blowfish(key);


    //============ BLOWFISH ==============

    uint64_t prevent_compiler_optimiz = 0;

    Timer t;

    for (unsigned int i = 0; i < notests ; i++) {
        int_enc = bf->encrypt(int_data);
        prevent_compiler_optimiz += int_enc % 10;
    }

    double timeEnc = t.lap_ms() / notests;

    if (prevent_compiler_optimiz % 297973 == 0) {
        cerr << "lucky case\n";
    }
    //eval decryption

    t.lap();

    for (unsigned int i = 0; i < notests ; i++) {
        int_dec = bf->decrypt(int_enc);
        prevent_compiler_optimiz += int_dec % 10;
    }

    double timeDec = t.lap_ms() / notests;

    if (prevent_compiler_optimiz % 297973 == 0) {
        cerr << "lucky case\n";
    }
    //sanity check
    assert_s(int_data == int_dec, "something went wrong with blowfish");

    cerr << "Blowfish encrypt " << timeEnc << "ms Blowfish decrypt " << timeDec << "ms \n";

    //============= CBC = RND ===================

    prevent_compiler_optimiz = 0;

    t.lap();

    for (unsigned int i = 0; i < notests ; i++) {
        str_enc = encrypt_AES_CBC(str_data, enckey, salt);
        prevent_compiler_optimiz += (int)str_enc[0];
    }

    timeEnc = t.lap_ms() / notests;

    if (prevent_compiler_optimiz % 297973 == 0) {
        cerr << "lucky case\n";
    }
    //eval decryption

    t.lap();

    for (unsigned int i = 0; i < notests ; i++) {
        str_dec = decrypt_AES_CBC(str_enc, deckey, salt);
        prevent_compiler_optimiz += (int)str_dec[0];
    }

    timeDec = t.lap_ms() / notests;

    if (prevent_compiler_optimiz % 297973 == 0) {
        cerr << "lucky case\n";
    }
    //sanity check
    assert_s(str_data == str_dec, "something went wrong with cbc");

    cerr << "CBC encrypt " << timeEnc << "ms CBC decrypt " << timeDec << "ms \n";



    //============== CMC = DET ====================

    prevent_compiler_optimiz = 0;

    t.lap();

    for (unsigned int i = 0; i < notests ; i++) {
        str_enc = encrypt_AES_CMC(str_data, enckey);
        prevent_compiler_optimiz += (int)str_enc[0];
    }

    timeEnc = t.lap_ms() / notests;

    if (prevent_compiler_optimiz % 297973 == 0) {
        cerr << "lucky case\n";
    }
    //eval decryption

    t.lap();

    for (unsigned int i = 0; i < notests ; i++) {
        str_dec = decrypt_AES_CMC(str_enc, deckey);
        prevent_compiler_optimiz += (int)str_dec[0];
    }


    timeDec = t.lap_ms() / notests;

    if (prevent_compiler_optimiz % 297973 == 0) {
        cerr << "lucky case\n";
    }

    //sanity check
    assert_s(str_data == str_dec, "something went wrong with cmc");

    cerr << "CMC encrypt " << timeEnc << "ms CMC decrypt " << timeDec << "ms \n";



}


static void
latency() {

    latency_join(3000);
    latency_search(10000);
    latency_Paillier(100, 10000);
    latency_OPE(100);
    latency_basics(10000);
}

void
TestCrypto::run(const TestConfig &tc, int argc, char ** argv)
{

    if (argc == 2) {
        if (strcmp(argv[1], "latency") == 0) {
            latency();
            return;
        }
    }
    cerr << "TESTING CRYPTO" << endl;
    cerr << "Testing basics.." << endl;
    testBasics();
    cerr << "Onion tests .. " << endl;
    testOnions();
    cerr << "Testing OPE..." << endl;
    testOPE();
    cerr << "Testing HGD..." << endl;
    testHGD();
    cerr << "Testing PKCS..." << endl;
    testPKCS();
    cerr << "Testing SWP Search ... " << endl;
    testSWPSearch();
    cerr << "Testing PBKDF2" << endl;
    testPBKDF2();
    cerr << "Testing ECJoin " << endl;
    testECJoin();
    cerr << "Testing Paillier... " << endl;
    testPaillier();

    //testEncTables();
    cerr << "Done! All crypto tests passed." << endl;
}
