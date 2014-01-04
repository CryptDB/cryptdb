#pragma once

/*
 * util.h
 *
 * A set of useful constants, data structures and functions.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <list>
#include <map>
#include <stdint.h>
#include <vector>
#include <set>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <assert.h>

#include <sys/time.h>

#include <NTL/ZZ.h>

#include <util/errstream.hh>
#include <util/params.hh>

// ==== CONSTANTS ============== //

#define SVAL2(s) #s
#define SVAL(s) SVAL2(s)

#if MYSQL_S
#define TN_I32 "integer"
#define TN_I64 "bigint unsigned"
#define TN_TEXT "blob"
#define TN_HOM "varbinary(" SVAL(PAILLIER_LEN_BYTES) ")"
#define TN_PTEXT "text"
#define TN_SALT "bigint unsigned"
#endif

#define TN_SYM_KEY "varbinary(32)"
#define TN_PK_KEY  "varbinary(1220)"

#define psswdtable "activeusers"

const unsigned int bitsPerByte = 8;
const unsigned int bytesPerInt = 4;

const uint32_t MAX_UINT32_T = -1;
const uint64_t MAX_UINT64_T = -1;

const unsigned int SALT_LEN_BYTES = 8;

const unsigned int AES_BLOCK_BITS = 128;
const unsigned int AES_BLOCK_BYTES = AES_BLOCK_BITS/bitsPerByte;
const unsigned int AES_KEY_SIZE = 128;
const unsigned int AES_KEY_BYTES = AES_KEY_SIZE/bitsPerByte;

const unsigned int MASTER_KEY_SIZE = AES_KEY_SIZE; //master key

const unsigned int OPE_KEY_SIZE = AES_KEY_SIZE;

const unsigned int EncryptedIntSize = 128;

const unsigned int bytesOfTextForOPE = 20; //texts may be ordered
                                           // alphabetically -- this variable
                                           // indicates how many of the first
                                           // bytes should be used for sorting


const std::string PWD_TABLE_PREFIX = "pwdcryptdb__";

//maps the name of an annotation we want to process to the number of fields
// after this annotation relevant to it
const std::set<std::string> annotations =
{"enc", "search", "encfor", "equals", "givespsswd", "speaksfor"};

// ============= DATA STRUCTURES ===================================//


const std::string BASE_SALT_NAME = "cdb_salt";
typedef uint64_t salt_type;

typedef struct QueryMeta {
    std::map<std::string, std::string> tabToAlias, aliasToTab;
    std::list<std::string> tables;
    std::map<std::string, std::string> aliasToField;

    void cleanup();
} QueryMeta;

typedef struct Predicate {
    std::string name;
    std::list<std::string> fields;
} Predicate;

/********* Data structures for multi-key CryptDB -- should not be used by
   single-principal ****/



typedef struct AccessRelation {
    AccessRelation(const std::string &hacc, const std::string &acct) {
        hasAccess = hacc;
        accessTo = acct;
    }
    std::string hasAccess;
    std::string accessTo;
} AccessRelation;


typedef struct AccessRelationComp {
  bool operator() (const AccessRelation& lhs, const AccessRelation& rhs) const {
         if (lhs.hasAccess < rhs.hasAccess) {
              return true;
          }
          if (lhs.hasAccess > rhs.hasAccess) {
              return false;
          }

          if (lhs.accessTo < rhs.accessTo) {
              return true;
          } else {
              return false;
          }
      }
} AccessRelationComp;

//permanent metadata for multi-key CryptDB - stores which field is encrypted
// for which field
typedef struct MultiKeyMeta {
    //e.g., msg_text encrypted for principal u.id
    std::map<std::string, std::string> encForMap;
    //contains an element if that element has some field encrypted to it
    std::map<std::string, bool > reverseEncFor;
    std::map<AccessRelation, Predicate *, AccessRelationComp> condAccess;     //maps a field having accessto to
                                             // any conditional predicate it
                                             // may have
    MultiKeyMeta() {}
    ~MultiKeyMeta() {
        for (auto i = condAccess.begin(); i != condAccess.end(); i++) {
           delete i->second;
        }
    }
} MKM;

//temporary metadata for multi-key CryptDB that belongs to the query or result
// being processed
typedef struct TempMKM {
    //maps a field (fullname) that has another field encrypted for it to its
    // value
    // groups.gid    23
    std::map<std::string, std::string> encForVal;

    //maps a field that has another field encrypted for it to the index in the
    // response std::list of values containing its value
    // groups.gid 5
    std::map<std::string, int> encForReturned;

    // contains fullnames of principals that were seen already in a response
    std::map<std::string, bool> principalsSeen;

    //true if current processing is query rather
    bool processingQuery;

    //some fields will be selected in order to be able to decrypt others, but
    // should not
    // be returned in the response to the application
    // maps position in raw DBMS response to whether it should be returned to
    // user or not
    std::map<unsigned int, bool> returnBitMap;
} TMKM;

//=============  Useful functions =========================//

// extracts (nobytes) bytes from int by placing the most significant bits at
// the end
std::string BytesFromInt(uint64_t value, unsigned int noBytes);
uint64_t IntFromBytes(const unsigned char * bytes, unsigned int noBytes);

void assert_s (bool value, const std::string &msg)
    throw (CryptDBError);
void myassert(bool value, const std::string &mess = "assertion failed");

double timeInSec(struct timeval tvstart, struct timeval tvend);

//parsing
const std::set<char> delimsStay = {'(', ')', '=', ',', '>', '<'};
const std::set<char> delimsGo   = {';', ' ', '\t', '\n'};
const std::set<char> keepIntact = {'\''};

#define NELEM(array) (sizeof((array)) / sizeof((array)[0]))
const std::set<std::string> commands =
    { "select", "create", "insert", "update", "delete", "drop", "alter" };
const std::set<std::string> aggregates = { "max", "min", "sum", "count" };
const std::set<std::string> createMetaKeywords = { "primary", "key", "unique" };
const std::set<std::string> comparisons = { ">", "<", "=" };

const std::string math[]=
{"+","-","(",")","*","/",".","0","1","2","3","4","5","6","7","8","9"};
const unsigned int noMath = NELEM(math);

std::string randomBytes(unsigned int len);
uint64_t randomValue();

std::string stringToByteInts(const std::string &s);
std::string angleBrackets(const std::string &s);
static inline std::string id_op(const std::string &x) { return x; }

char *
getCStr(const std::string & x);

/*
 * Turn a std::list (of type C) into a std::string, applying op to each element.
 * Handy ops are id_op, angleBrackets, and std::stringToByteInts.
 */
template<class C, class T>
std::string
toString(const C &l, T op)
{
    std::stringstream ss;
    bool first = true;
    for (auto i = l.begin(); i != l.end(); i++) {
        if (first)
            ss << "(";
        else
            ss << ", ";
        ss << op(*i);
        first = false;
    }
    ss << ")";
    return ss.str();
}

// given binary string h, returns a hex readable string
std::string
fromHex(const std::string & h);

// returns a binary string from a hex readable string
std::string
toHex(const std::string & b);

// tries to represent value in minimum no of bytes, avoiding the \0 character
std::string StringFromVal(uint64_t value, unsigned int padLen = 0);


uint64_t uint64FromZZ(NTL::ZZ val);
//to_ZZ func may not work for 64 bits
NTL::ZZ ZZFromUint64(uint64_t val);


std::string StringFromZZ(const NTL::ZZ &x);
NTL::ZZ ZZFromString(const std::string &s);

std::string padForZZ(std::string s);

std::string StringFromZZFast(const NTL::ZZ& x);

void ZZFromStringFast(NTL::ZZ& x, const std::string& s);
void ZZFromBytesFast(NTL::ZZ& x, const unsigned char *p, long n);

inline NTL::ZZ ZZFromStringFast(const std::string& s) {
  NTL::ZZ z; ZZFromStringFast(z, s); return z;
}
inline NTL::ZZ ZZFromBytesFast(const unsigned char *p, long n) {
  NTL::ZZ z; ZZFromBytesFast(z, p, n); return z;
}

//rolls an interator forward
template<typename T> void
roll(typename std::list<T>::iterator & it,  int count)
{
    if (count < 0) {
        for (int i = 0; i < -count; i++) {
            it--;
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            it++;
        }
    }
}

template<typename A, typename B>
const B &constGetAssert(const std::map<A, B> &m, const A &x,
                        const std::string &str = "")
{
    auto it = m.find(x);
    if (m.end() == it) {
        std::cerr << "item not present in map " << x << ". " << str
                  << std::endl;
        assert(false);
    }
    return it->second;
}

template<typename A, typename B>
B &getAssert(std::map<A, B> &m, const A &x,
             const std::string &str = "")
{
    return const_cast<B &>(constGetAssert(m, x, str));
}

//returns true if x is in m and sets y=m[x] in that case
template<typename A, typename B>
bool contains_get(const std::map<A, B> & m, const A & x, B & y) {
    auto it = m.find(x);
    if (it == m.end()) {
        return false;
    } else {
        y = it->second;
        return true;
    }
}


template <typename T>
bool
isLastIterator(typename std::list<T>::iterator it,
               typename std::list<T>::iterator endit)
{
    roll<T>(it, 1);
    return it == endit;
}

//returns a Postgres bigint representation in std::string form for x
std::string strFromVal(uint64_t x);
std::string strFromVal(uint32_t x);



uint64_t valFromStr(const std::string & str);


void consolidate(std::list<std::string> & words);


std::list<std::string>
split(const std::string &s, const char * const separators);


//returns a std::string representing a value pointed to by it and advances it
std::string getVal(std::list<std::string>::iterator & it);


//acts only if the first field is "(";
//returns position after matching ")" mirroring all contents
std::string processParen(std::list<std::string>::iterator & it, const std::list<std::string> & words);


//returns the contents of str before the first encounter with c
std::string getBeforeChar(const std::string &str, char c);

//performs a case insensitive search
bool isOnly(const std::string &token, const std::string * values, unsigned int noValues);

void addIfNotContained(const std::string &token, std::list<std::string> & lst);
void addIfNotContained(const std::string &token1, const std::string &token2,
                       std::list<std::pair<std::string, std::string> > & lst);

std::string removeApostrophe(const std::string &data);
bool hasApostrophe(const std::string &data);

std::string homomorphicAdd(const std::string &val1, const std::string &val2,
                      const std::string &valN2);

std::string toLowerCase(const std::string &token);
std::string toUpperCase(const std::string &token);

bool equalsIgnoreCase(const std::string &s1, const std::string &s2);

//performs a case insensitive search
template<class T>
bool contains(const std::string &token, const T &values)
{
    for (auto i = values.begin(); i != values.end(); i++)
        if (equalsIgnoreCase(token, *i))
            return true;
    return false;
}


/**** HELPERS FOR EVAL **************/

std::string getQuery(std::ifstream & qFile);


class Timer {
 private:
    Timer(const Timer &t);  /* no reason to copy timer objects */

 public:
    Timer() { lap(); }

    //microseconds
    uint64_t lap() {
        uint64_t t0 = start;
        uint64_t t1 = cur_usec();
        start = t1;
        return t1 - t0;
    }

    //milliseconds
    double lap_ms() {
        return ((double)lap()) / 1000.0;
    }

 private:
    static uint64_t cur_usec() {
        struct timeval tv;
        gettimeofday(&tv, 0);
        return ((uint64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
    }

    uint64_t start;
};

template <typename T>
class AssignOnce {
private:
    AssignOnce(const AssignOnce &other) = delete;
    AssignOnce(AssignOnce &&other) = delete;
    AssignOnce &operator=(AssignOnce &&other) = delete;

public:
    AssignOnce() : frozen(false) {}
    ~AssignOnce() {;}
    const AssignOnce& operator=(T value) {
        if (true == frozen) {
            throw CryptDBError("Object has already been assigned to!");
        } else {
            this->value = value;
            frozen = true;
            return *this;
        }
    }

    const T get() const {
        if (false == frozen) {
            throw CryptDBError("First assign to AssignOnce template!");
        }
        return value;
    }

    bool assigned() const {
        return frozen;
    }

private:
    T value;
    bool frozen;
};

template <typename T>
class AssignFirst {
private:
    AssignFirst(const AssignFirst &other) = delete;
    AssignFirst(AssignFirst &&other) = delete;
    AssignFirst &operator=(AssignFirst &&other) = delete;

public:
    AssignFirst() : assigned(false) {}
    ~AssignFirst() {}
    const AssignFirst &operator=(T value) {
        this->value = value;
        this->assigned = true;

        return *this;
    }

    const T get() const {
        if (false == this->assigned) {
            throw CryptDBError("First assign to AssignFirst object!");
        }

        return this->value;
    }

private:
    T value;
    bool assigned;
};

template <typename T>
class MaxOneReadPerAssign {
public:
    MaxOneReadPerAssign(T value) : value(value), available(true) {}
    ~MaxOneReadPerAssign() {;}

    T get() const
    {
        if (false == available) {
            throw CryptDBError("MaxOneReadPerAssign is unavailable!");
        }
        // NOTE: mutable so that 'get()' can be used in the getters of
        // other classes without forcing the classes to declare the
        // getters as non-const.
        available = false;
        return value;
    }

    const MaxOneReadPerAssign& operator=(T new_value)
    {
        value = new_value;
        available = true;

        return *this;
    }

private:
    T value;
    mutable bool available;
};

template <typename T>
class CarefulClear {
public:
    CarefulClear(T value) : value(value), is_set(true) {}
    CarefulClear() : is_set(false) {}

    T get() const {
        if (false == is_set) {
            throw CryptDBError("CarefulClear : must set (=) before get");
        }

        return value;
    }

    const CarefulClear &operator=(T new_value)
    {
        if (true == is_set) {
            throw CryptDBError("CarefulClear : must clear before set (=)");
        }
        value = new_value;
        is_set = true;

        return *this;
    }

    void clear() {
        if (false == is_set) {
            throw CryptDBError("CarefulClear : already cleared");
        }

        is_set = false;
    }

    bool isSet() const {return is_set;}

private:
    T value;
    bool is_set;
};

class OnUnscope {
    OnUnscope() = delete;
    OnUnscope(const OnUnscope &a) = delete;
    OnUnscope(OnUnscope &&a) = delete;
    OnUnscope &operator=(const OnUnscope &a) = delete;
    OnUnscope &operator=(OnUnscope &&a) = delete;

public:
    OnUnscope(std::function<void(void)> fn) : fn(fn) {}
    ~OnUnscope() {fn();}

private:
    const std::function<void(void)> fn;
};


// Taken from jsmith @ cplusplus.com
template <typename T>
std::vector<T> vectorDifference(const std::vector<T> &model,
                                const std::vector<T> &pattern)
{
    std::set<T> s_model(model.begin(), model.end());
    std::set<T> s_pattern(pattern.begin(), pattern.end());
    std::vector<T> result;

    std::set_difference(s_model.begin(), s_model.end(),
                        s_pattern.begin(), s_pattern.end(),
                        std::back_inserter(result));

    return result;
}
