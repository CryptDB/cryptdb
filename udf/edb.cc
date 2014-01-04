/*
 * Handling NULL value
 * - Encryption routines should never get a NULL value due to the way
 *   rewriting is handled for NULL.
 * - Decryption routines should forgo the decryption of NULL values.
 */

#define DEBUG 1

#include <memory>

#include <crypto/BasicCrypto.hh>
#include <crypto/blowfish.hh>
#include <crypto/SWPSearch.hh>
#include <crypto/paillier.hh>
#include <util/params.hh>
#include <util/util.hh>
#include <util/version.hh>

using namespace NTL;

extern "C" {

typedef unsigned long long ulonglong;
typedef long long longlong;
// Use the standard mysql dev headers.
#include <mysql/mysql.h>
#include <ctype.h>

my_bool   cryptdb_decrypt_int_sem_init(UDF_INIT *const initid,
                                       UDF_ARGS *const args,
                                       char *const message);
ulonglong cryptdb_decrypt_int_sem(UDF_INIT *const initid,
                                  UDF_ARGS *const args,
                                  char *const is_null, char *const error);

my_bool   cryptdb_decrypt_int_det_init(UDF_INIT *const initid,
                                       UDF_ARGS *const args,
                                       char *const message);
ulonglong cryptdb_decrypt_int_det(UDF_INIT *const initid, UDF_ARGS *const args,
                                  char *const is_null, char *const error);

my_bool   cryptdb_decrypt_text_sem_init(UDF_INIT *const initid,
                                        UDF_ARGS *const args, char *const message);
void      cryptdb_decrypt_text_sem_deinit(UDF_INIT *const initid);
char *    cryptdb_decrypt_text_sem(UDF_INIT *const initid, UDF_ARGS *const args,
                                   char *const result, unsigned long *const length,
                                   char *const is_null, char *const error);

my_bool   cryptdb_decrypt_text_det_init(UDF_INIT *const initid,
                                        UDF_ARGS *const args,
                                        char *const message);
void      cryptdb_decrypt_text_det_deinit(UDF_INIT *const initid);
char *    cryptdb_decrypt_text_det(UDF_INIT *const initid, UDF_ARGS *const args,
                                   char *const result, unsigned long *const length,
                                   char *const is_null, char *const error);

my_bool   cryptdb_searchSWP_init(UDF_INIT *const initid, UDF_ARGS *const args,
                                 char *const message);
void      cryptdb_searchSWP_deinit(UDF_INIT *const initid);
ulonglong cryptdb_searchSWP(UDF_INIT *const initid, UDF_ARGS *const args,
                            char *const is_null, char *const error);

my_bool   cryptdb_agg_init(UDF_INIT *const initid, UDF_ARGS *const args,
                           char *const message);
void      cryptdb_agg_deinit(UDF_INIT *const initid);
void      cryptdb_agg_clear(UDF_INIT *const initid, char *const is_null,
                            char *const error);
my_bool   cryptdb_agg_add(UDF_INIT *const initid, UDF_ARGS *const args,
                          char *const is_null, char *const error);
char *    cryptdb_agg(UDF_INIT *const initid, UDF_ARGS *const args,
                      char *const result, unsigned long *const length,
                      char *const is_null, char *const error);

my_bool   cryptdb_func_add_set_init(UDF_INIT *const initid, UDF_ARGS *const args,
                                    char *const message);
void      cryptdb_func_add_set_deinit(UDF_INIT *const initid);
char *    cryptdb_func_add_set(UDF_INIT *const initid, UDF_ARGS *const args,
                               char *const result, unsigned long *const length,
                               char *const is_null, char *const error);

my_bool   cryptdb_version_init(UDF_INIT *const initid, UDF_ARGS *const args,
                               char *const message);
void      cryptdb_version_deinit(UDF_INIT *const initid);
char *    cryptdb_version(UDF_INIT *const initid, UDF_ARGS *const args,
                          char *const result, unsigned long *const length,
                          char *const is_null, char *const error);
} /* extern "C" */


static void __attribute__((unused))
log(const std::string &s)
{
    /* Writes to the server's error log */
    fprintf(stderr, "%s\n", s.c_str());
}


static std::string
decrypt_SEM(const unsigned char *const eValueBytes, uint64_t eValueLen,
            AES_KEY *const aesKey, uint64_t salt)
{
    std::string c(reinterpret_cast<const char *>(eValueBytes),
                  static_cast<unsigned int>(eValueLen));
    return decrypt_AES_CBC(c, aesKey, BytesFromInt(salt, SALT_LEN_BYTES),
                           false);
}


static std::unique_ptr<std::list<std::string>>
split(const std::string &s, unsigned int plen)
{
    const unsigned int len = s.length();
    if (len % plen != 0) {
        thrower() << "split receives invalid input";
    }

    const unsigned int num = len / plen;
    std::unique_ptr<std::list<std::string>>
        res(new std::list<std::string>());

    for (unsigned int i = 0; i < num; i++) {
        res.get()->push_back(s.substr(i*plen, plen));
    }

    return res;
}


static bool
searchExists(const Token &token, const std::string &overall_ciph)
{
    const std::unique_ptr<std::list<std::string>>
        l(split(overall_ciph, SWPCiphSize));
    return SWP::searchExists(token, *l.get());
}


static bool
search(const Token &token, const std::string &overall_ciph)
{
   return searchExists(token, overall_ciph);
}


static uint64_t
getui(UDF_ARGS *const args, int i)
{
    return static_cast<uint64_t>(*reinterpret_cast<ulonglong *>(args->args[i]));
}

static char *
getba(UDF_ARGS *const args, int i, uint64_t &len)
{
    len = args->lengths[i];
    return args->args[i];
}

my_bool
cryptdb_decrypt_int_sem_init(UDF_INIT *const initid, UDF_ARGS *const args,
                             char *const message)
{
    if (args->arg_count != 3 ||
        args->arg_type[0] != INT_RESULT ||
        args->arg_type[1] != STRING_RESULT ||
        args->arg_type[2] != INT_RESULT)
    {
        strcpy(message, "Usage: cryptdb_decrypt_int_sem(int ciphertext, string key, int salt)");
        return 1;
    }

    initid->maybe_null = 1;
    return 0;
}

ulonglong
cryptdb_decrypt_int_sem(UDF_INIT *const initid, UDF_ARGS *const args,
                        char *const is_null, char *const error)
{
    AssignFirst<uint64_t> value;
    if (NULL == args->args[0]) {
        value = 0;
        *is_null = 1;
    } else {
        try {
            const uint64_t eValue = getui(args, 0);

            uint64_t keyLen;
            char *const keyBytes = getba(args, 1, keyLen);
            const std::string key = std::string(keyBytes, keyLen);

            const uint64_t salt = getui(args, 2);

            blowfish bf(key);
            value = bf.decrypt(eValue) ^ salt;
        } catch (const CryptoError &e) {
            std::cerr << e.msg << std::endl;
            value = 0;
        }
    }

    //cerr << "udf: encVal " << eValue << " key " << (int)key[0] << " " << (int)key[1] << " " << (int) key[3]  << " salt " << salt  << " obtains: " << value << " and cast to ulonglong " << (ulonglong) value << "\n";


     return static_cast<ulonglong>(value.get());
}



my_bool
cryptdb_decrypt_int_det_init(UDF_INIT *const initid, UDF_ARGS *const args,
                             char *const message)
{
    if (args->arg_count != 3 ||
        args->arg_type[0] != INT_RESULT ||
        args->arg_type[1] != STRING_RESULT ||
        args->arg_type[2] != INT_RESULT)
    {
        strcpy(message, "Usage: cryptdb_decrypt_int_det(int ciphertext, string key)");
        return 1;
    }

    initid->maybe_null = 1;
    return 0;
}

ulonglong
cryptdb_decrypt_int_det(UDF_INIT *const initid, UDF_ARGS *const args,
                        char *const is_null, char *const error)
{
    AssignFirst<uint64_t> value;
    if (NULL == args->args[0]) {
        value = 0;
        *is_null = 1;
    } else {
        try {
            const uint64_t eValue = getui(args, 0);

            uint64_t keyLen;
            char *const keyBytes = getba(args, 1, keyLen);
            const std::string key = std::string(keyBytes, keyLen);

            const uint64_t shift = getui(args, 2);

            blowfish bf(key);
            value = bf.decrypt(eValue) - shift;
        } catch (const CryptoError &e) {
            std::cerr << e.msg << std::endl;
            value = 0;
        }
    }


    return static_cast<ulonglong>(value.get());
}



my_bool
cryptdb_decrypt_text_sem_init(UDF_INIT *const initid, UDF_ARGS *const args,
                              char *const message)
{
    if (args->arg_count != 3 ||
        args->arg_type[0] != STRING_RESULT ||
        args->arg_type[1] != STRING_RESULT ||
        args->arg_type[2] != INT_RESULT)
    {
        strcpy(message, "Usage: cryptdb_decrypt_text_sem(string ciphertext, string key, int salt)");
        return 1;
    }

    initid->maybe_null = 1;
    return 0;
}

void
cryptdb_decrypt_text_sem_deinit(UDF_INIT *const initid)
{
    /*
     * in mysql-server/sql/item_func.cc, udf_handler::fix_fields
     * initializes initid.ptr=0 for us.
     */
    if (initid->ptr)
        delete[] initid->ptr;
}

char *
cryptdb_decrypt_text_sem(UDF_INIT *const initid, UDF_ARGS *const args,
                         char *const result, unsigned long *const length,
                         char *const is_null, char *const error)
{
    AssignFirst<std::string> value;
    if (NULL == args->args[0]) {
        value = "";
        *is_null = 1;
    } else {
        try {
            uint64_t eValueLen;
            char *const eValueBytes = getba(args, 0, eValueLen);

            uint64_t keyLen;
            char *const keyBytes = getba(args, 1, keyLen);
            const std::string key = std::string(keyBytes, keyLen);

            uint64_t salt = getui(args, 2);

            const std::unique_ptr<AES_KEY> aesKey(get_AES_dec_key(key));
            value =
                decrypt_SEM(reinterpret_cast<unsigned char *>(eValueBytes),
                            eValueLen, aesKey.get(), salt);
        } catch (const CryptoError &e) {
            std::cerr << e.msg << std::endl;
            value = "";
        }
    }

    // NOTE: This is not creating a proper C string, no guarentee of NUL
    // termination.
    char *const res = new char[value.get().length()];
    initid->ptr = res;
    memcpy(res, value.get().data(), value.get().length());
    *length = value.get().length();

    return static_cast<char*>(initid->ptr);
}


my_bool
cryptdb_decrypt_text_det_init(UDF_INIT *const initid, UDF_ARGS *const args,
                              char *const message)
{
    if (args->arg_count != 2 ||
        args->arg_type[0] != STRING_RESULT ||
        args->arg_type[1] != STRING_RESULT)
    {
        strcpy(message, "Usage: cryptdb_decrypt_text_det(string ciphertext, string key)");
        return 1;
    }

    initid->maybe_null = 1;
    return 0;
}

void
cryptdb_decrypt_text_det_deinit(UDF_INIT *const initid)
{
    /*
     * in mysql-server/sql/item_func.cc, udf_handler::fix_fields
     * initializes initid.ptr=0 for us.
     */
    if (initid->ptr)
        delete[] initid->ptr;
}

char *
cryptdb_decrypt_text_det(UDF_INIT *const initid, UDF_ARGS *const args,
                         char *const result, unsigned long *const length,
                         char *const is_null, char *const error)
{
    AssignFirst<std::string> value;
    if (NULL == args->args[0]) {
        value = "";
        *is_null = 1;
    } else {
        try {
            uint64_t eValueLen;
            char *const eValueBytes = getba(args, 0, eValueLen);

            uint64_t keyLen;
            char *const keyBytes = getba(args, 1, keyLen);
            const std::string key = std::string(keyBytes, keyLen);

            const std::unique_ptr<AES_KEY> aesKey(get_AES_dec_key(key));
            value =
                decrypt_AES_CMC(std::string(eValueBytes,
                                    static_cast<unsigned int>(eValueLen)),
                                aesKey.get(), true);
        } catch (const CryptoError &e) {
            std::cerr << e.msg << std::endl;
            value = "";
        }
    }

    char *const res = new char[value.get().length()];
    initid->ptr = res;
    memcpy(res, value.get().data(), value.get().length());
    *length = value.get().length();
    return initid->ptr;
}

/*
 * given field of the form:   len1 word1 len2 word2 len3 word3 ...,
 * where each len is the length of the following "word",
 * search for word which is of the form len word_body where len is
 * the length of the word body
 */


my_bool
cryptdb_searchSWP_init(UDF_INIT *const initid, UDF_ARGS *const args,
                       char *const message)
{
    if (args->arg_count != 3 ||
        args->arg_type[0] != STRING_RESULT ||
        args->arg_type[1] != STRING_RESULT ||
        args->arg_type[2] != STRING_RESULT)
    {
        strcpy(message, "Usage: cryptdb_searchSWP(string ciphertext, string ciph, string wordKey)");
        return 1;
    }

    Token *const t = new Token();

    uint64_t ciphLen;
    char *const ciph = getba(args, 1, ciphLen);

    uint64_t wordKeyLen;
    char *const wordKey = getba(args, 2, wordKeyLen);

    t->ciph = std::string(ciph, ciphLen);
    t->wordKey = std::string(wordKey, wordKeyLen);

    initid->ptr = reinterpret_cast<char *>(t);

    return 0;
}

void
cryptdb_searchSWP_deinit(UDF_INIT *const initid)
{
    Token *const t = reinterpret_cast<Token *>(initid->ptr);
    delete t;
}

ulonglong
cryptdb_searchSWP(UDF_INIT *const initid, UDF_ARGS *const args,
                  char *const is_null, char *const error)
{
    uint64_t allciphLen;
    char *const allciph = getba(args, 0, allciphLen);
    const std::string overallciph = std::string(allciph, allciphLen);

    Token *const t = reinterpret_cast<Token *>(initid->ptr);

    return search(*t, overallciph);
}


struct agg_state {
    ZZ sum;
    ZZ n2;
    bool n2_set;
    void *rbuf;
};

my_bool
cryptdb_agg_init(UDF_INIT *const initid, UDF_ARGS *const args,
                 char *const message)
{
    if (args->arg_count != 2 ||
        args->arg_type[0] != STRING_RESULT ||
        args->arg_type[1] != STRING_RESULT)
    {
        strcpy(message, "Usage: cryptdb_agg(string ciphertext, string pubkey)");
        return 1;
    }

    agg_state *const as = new agg_state();
    as->rbuf = malloc(Paillier_len_bytes);
    initid->ptr = reinterpret_cast<char *>(as);
    initid->maybe_null = 1;
    return 0;
}

void
cryptdb_agg_deinit(UDF_INIT *const initid)
{
    agg_state *const as = reinterpret_cast<agg_state *>(initid->ptr);
    free(as->rbuf);
    delete as;
}

// When we want to add by zero for HOM values we can multiply our value
// by 1.
void
cryptdb_agg_clear(UDF_INIT *const initid, char *const is_null, char *const error)
{
    agg_state *const as = reinterpret_cast<agg_state *>(initid->ptr);
    as->sum = to_ZZ(1);
    as->n2_set = 0;
}

//args will be element to add, constant N2
my_bool
cryptdb_agg_add(UDF_INIT *const initid, UDF_ARGS *const args,
                char *const is_null, char *const error)
{
    //cerr << "in agg_add \n";
    agg_state *const as = reinterpret_cast<agg_state *>(initid->ptr);
    if (!as->n2_set) {
        //cerr << "n2 length is " << args->lengths[1] << "\n";
        //cerr << "n2 first byte is " << (int)args->args[1][0] << "\n";
        ZZFromBytes(as->n2,
                    reinterpret_cast<const uint8_t *>(args->args[1]),
                    args->lengths[1]);
        //cerr << "n2 is " << as->n2 << "\n";
        as->n2_set = 1;
    }

    ZZ e;
    if (NULL == args->args[0]) {
        e = to_ZZ(1);
    } else {
        ZZFromBytes(e, reinterpret_cast<const uint8_t *>(args->args[0]),
                    args->lengths[0]);
    }

    //cerr << "element to add " << e << "\n";
    MulMod(as->sum, as->sum, e, as->n2);
    //cerr << "sum so far " << as->sum << "\n";
    return true;
}

char *
cryptdb_agg(UDF_INIT *const initid, UDF_ARGS *const args, char *const result,
            unsigned long *const length, char *const is_null, char *const error)
{
    agg_state *const as = reinterpret_cast<agg_state *>(initid->ptr);
    BytesFromZZ(static_cast<uint8_t *>(as->rbuf), as->sum,
                Paillier_len_bytes);
    *length = Paillier_len_bytes;
    return static_cast<char *>(as->rbuf);
}

// for update with increment
// > UNUSED

my_bool
cryptdb_func_add_set_init(UDF_INIT *const initid, UDF_ARGS *const args,
                          char *const message)
{
    if (args->arg_count != 3 ||
        args->arg_type[0] != STRING_RESULT ||
        args->arg_type[1] != STRING_RESULT ||
        args->arg_type[2] != STRING_RESULT)
    {
        strcpy(message, "Usage: cryptdb_func_add_set(string ciphertext0, string ciphertext1, string pubkey)");
        return 1;
    }

    return 0;
}

void
cryptdb_func_add_set_deinit(UDF_INIT *const initid)
{
    if (initid->ptr) {
        free(initid->ptr);
        initid->ptr = NULL;
    }
}

char *
cryptdb_func_add_set(UDF_INIT *const initid, UDF_ARGS *const args,
                     char *const result, unsigned long *const length,
                     char *const is_null, char *const error)
{
    if (initid->ptr) {
        free(initid->ptr);
        initid->ptr = NULL;
    }

    AssignOnce<uint64_t> out_len;
    ZZ res;
    if (NULL == args->args[0]) {
        out_len = 0;
        *is_null = 1;
        res = 0;
    } else {
        out_len = args->lengths[2];

        ZZ field, val, n2;
        ZZFromBytes(field,
                    reinterpret_cast<const uint8_t *>(args->args[0]),
                    args->lengths[0]);
        ZZFromBytes(val, reinterpret_cast<const uint8_t *>(args->args[1]),
                    args->lengths[1]);
        ZZFromBytes(n2, reinterpret_cast<const uint8_t *>(args->args[2]),
                    args->lengths[2]);

        MulMod(res, field, val, n2);
    }

    void *const rbuf = malloc(static_cast<size_t>(out_len.get()));
    initid->ptr = static_cast<char *>(rbuf);
    BytesFromZZ(static_cast<uint8_t *>(rbuf), res,
                static_cast<size_t>(out_len.get()));

    *length = static_cast<long unsigned int>(out_len.get());
    return initid->ptr;
}

my_bool
cryptdb_version_init(UDF_INIT *const initid, UDF_ARGS *const args,
                     char *const message)
{
    if (args->arg_count != 0) {
        strcpy(message, "cryptdb_version() requires no arguments");
        return 1;
    }

    return 0;
}

void
cryptdb_version_deinit(UDF_INIT *const initid)
{
    if (initid->ptr)
        delete[] initid->ptr;
}

char *
cryptdb_version(UDF_INIT *const initid, UDF_ARGS *const args,
                char *const result, unsigned long *const length,
                char *const is_null, char *const error)
{
    const std::string value(cryptdb_version_string);
    char *const res = new char[value.length()];
    initid->ptr = res;
    memcpy(res, value.data(), value.length());
    *length = value.length();

    return static_cast<char*>(initid->ptr);
}
