#include <algorithm>
#include <string>
#include <iomanip>
#include <stdexcept>
#include <assert.h>
#include <memory>

#include <gmp.h>

#include <openssl/rand.h>

#include <util/util.hh>
#include <util/cryptdb_log.hh>

using namespace NTL;

void
myassert(bool value, const std::string &mess)
{
    if (ASSERTS_ON) {
        if (!value) {
            LOG(warn) << "ERROR: " << mess;
            throw std::runtime_error(mess);
        }
    }
}

void
assert_s(bool value, const std::string &msg)
throw (CryptDBError)
{
    if (ASSERTS_ON) {
        if (!value) {
            LOG(warn) << "ERROR: " << msg;
            throw CryptDBError(msg);
        }
    }
}

double
timeInSec(struct timeval tvstart, struct timeval tvend)
{
    return
        ((double) (tvend.tv_sec - tvstart.tv_sec) +
         ((double) (tvend.tv_usec - tvstart.tv_usec)) / 1000000.0);
}

std::string
randomBytes(unsigned int len)
{
    std::string s;
    s.resize(len);
    RAND_bytes(reinterpret_cast<uint8_t *>(&s[0]), len);
    return s;
}

uint64_t
randomValue()
{
    return IntFromBytes(reinterpret_cast<const uint8_t *>(randomBytes(8).c_str()), 8);
}

std::string
stringToByteInts(const std::string &s)
{
    std::stringstream ss;
    bool first = true;
    for (size_t i = 0; i < s.length(); i++) {
        if (!first)
            ss << " ";
        ss << (uint) (uint8_t) s[i];
        first = false;
    }
    return ss.str();
}

std::string
angleBrackets(const std::string &s)
{
    return "<" + s + ">";
}

std::string
BytesFromInt(uint64_t value, unsigned int noBytes)
{
    std::string result;
    result.resize(noBytes);

    for (uint i = 0; i < noBytes; i++) {
        result[noBytes-i-1] = ((unsigned char) value) % 256;
        value = value / 256;
    }

    return result;
}

uint64_t
IntFromBytes(const unsigned char * bytes, unsigned int noBytes)
{
    uint64_t value = 0;

    for (unsigned int i = 0; i < noBytes; i++) {
        unsigned int bval = (unsigned int)bytes[i];
        value = value * 256 + bval;
    }

    return value;
}

uint64_t
uint64FromZZ(ZZ val)
{
    uint64_t res = 0;
    uint64_t mul = 1;
    while (val > 0) {
        res = res + mul*(to_int(val % 10));
        mul = mul * 10;
        val = val / 10;
    }
    return res;
}



std::string
StringFromZZ(const ZZ &x)
{
    std::string s;
    s.resize(NumBytes(x), 0);
    BytesFromZZ(reinterpret_cast<uint8_t *>(&s[0]), x, s.length());
    return s;
}

ZZ
ZZFromString(const std::string &s)
{
    return ZZFromBytes(reinterpret_cast<const uint8_t *>(s.data()),
                       s.length());
}

// based on _ntl_gcopy from g_lip_impl.h
// we duplicate this so we can avoid having to do a copy
#define ALLOC(p) (((long *) (p))[0])
#define SIZE(p) (((long *) (p))[1])
#define DATA(p) ((mp_limb_t *) (((long *) (p)) + 2))
#define STORAGE(len) ((long)(2*sizeof(long) + (len)*sizeof(mp_limb_t)))
#define MustAlloc(c, len)  (!(c) || (ALLOC(c) >> 2) < (len))

static void _ntl_gcopy_mp(mp_limb_t* a, long sa, _ntl_gbigint *bb)
{
   _ntl_gbigint b;
   long abs_sa, i;
   mp_limb_t *adata, *bdata;

   b = *bb;

   if (!a || sa == 0) {
      if (b) SIZE(b) = 0;
   }
   else {
      if (a != b) {
         if (sa >= 0)
            abs_sa = sa;
         else
            abs_sa = -sa;

         if (MustAlloc(b, abs_sa)) {
            _ntl_gsetlength(&b, abs_sa);
            *bb = b;
         }

         adata = a;
         bdata = DATA(b);

         for (i = 0; i < abs_sa; i++)
            bdata[i] = adata[i];

         SIZE(b) = sa;
      }
   }
}

#ifndef NTL_GMP_LIP
#error "NTL is not compiled with GMP"
#endif

std::string padForZZ(std::string s) {
    if (s.size() % sizeof(mp_limb_t) == 0) {return s;}
    int n = ((s.size()/sizeof(mp_limb_t)) + 1)*sizeof(mp_limb_t);
    s.resize(n);
    return s;
}

std::string StringFromZZFast(const ZZ& x) {
    long sa = SIZE(x.rep);
    long abs_sa;
    if (sa >= 0)
      abs_sa = sa;
    else
      abs_sa = -sa;
    mp_limb_t* data = DATA(x.rep);
    return std::string(reinterpret_cast<char *>(data),
                       abs_sa * sizeof(mp_limb_t));
}

void ZZFromStringFast(ZZ& x, const std::string& s) {
    assert(s.size() % sizeof(mp_limb_t) == 0);
    _ntl_gcopy_mp(
        const_cast<mp_limb_t *>(reinterpret_cast<const mp_limb_t *>(s.data())),
        s.size() / sizeof(mp_limb_t),
        &x.rep);
}

//Faster function, if p is 8-byte aligned; if not go for the slower one
// TODO: figure out how to pad non-8-byte aligned p
void ZZFromBytesFast(ZZ& x, const unsigned char *p, long n) {
    if (n % sizeof(mp_limb_t) != 0) {
        x = ZZFromBytes(static_cast<const uint8_t *>(p), n);
        return;
    }
    _ntl_gcopy_mp(
        (mp_limb_t*) p,
        n / sizeof(mp_limb_t),
        &x.rep);
}

ZZ
ZZFromUint64 (uint64_t value)
{
    unsigned int unit = 256;
    ZZ power;
    power = 1;
    ZZ res;
    res = 0;

    while (value > 0) {
        res = res + ((long int)value % unit) * power;
        power = power * unit;
        value = value / unit;
    }
    return res;

};

char *
getCStr(const std::string & x) {
    char *  res = static_cast<char *>(malloc(x.size()));
    memcpy(res, x.c_str(), x.size());
    return res;
}

std::string
strFromVal(uint64_t x)
{
    std::stringstream ss;
    ss <<  x;
    return ss.str();
}

std::string
strFromVal(uint32_t x)
{
    std::stringstream ss;
    ss << (int32_t) x;
    return ss.str();
}

std::string
StringFromVal(uint64_t value, uint padLen)
{
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(padLen) << value;
    return ss.str();
}

//Transforms a string that contains an integer (in decimal form) into the
//integer
// Does not work as wanted if str codifies a number in binary.
uint64_t
valFromStr(const std::string &str)
{
    std::stringstream ss(str);
    uint64_t val;
    ss >> val;

    return  val;
}

static uint hexval(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else {
        return c - 'a'+10;
    }
}

std::string
fromHex(const std::string & h) {
    std::string res(h.length()/2, '0');

    for (uint i = 0; i < h.length(); i = i +2) {
        res[i/2] = (unsigned char)(hexval(h[i])*16+hexval(h[i+1]));
    }

    return res;
}

const std::string hextable = "0123456789abcdef";

std::string
toHex(const std::string  & x) {
    uint len = x.length();
    std::string result(len*2, '0');
    
    for (uint i = 0; i < len; i++) {
        uint v = (uint)x[i];
        result[2*i] = hextable[v / 16];
        result[2*i+1] = hextable[v % 16];
    }

    return result;
}



std::list<std::string>
split(const std::string &s, const char * const separators) {
    std::unique_ptr<char, void (*)(void *)>
        cp_s(strdup(s.c_str()), &std::free);
    if (!cp_s) {
        throw CryptDBError("insufficient memory!");
    }
    char* tok = strtok(cp_s.get(), separators);
    std::list<std::string> parts = std::list<std::string>();
    while (tok != NULL) {
        parts.push_back(std::string(tok));
        tok = strtok(NULL, separators);
    }
    return parts;
}

std::string
getQuery(std::ifstream & qFile)
{

    std::string query = "";
    std::string line = "";

    while ((!qFile.eof()) && (line.find(';') == std::string::npos)) {
        getline(qFile, line);
        query = query + line;
    }


    return query;

}

std::string
getBeforeChar(const std::string &str, char c)
{
    size_t pos = str.find(c);
    if (pos != std::string::npos) {
        return str.substr(0, pos);
    } else {
        return "";
    }
}


bool
isOnly(const std::string &token, const std::string * values,
       unsigned int noValues)
{
    for (unsigned int j = 0; j < token.size(); j++) {
        bool is_value = false;
        for (unsigned int i = 0; i < noValues; i++) {
            std::string test = "";
            test += token[j];
            if (equalsIgnoreCase(test, values[i])) {
                is_value = true;
            }
        }
        if (!is_value) {
            std::cerr << token[j] << std::endl;
            return false;
        }
    }
    return true;
}

static bool
contains(const std::string &token1, const std::string &token2,
         std::list<std::pair<std::string, std::string> > &lst)
{
    for (auto it = lst.begin(); it != lst.end(); it++)
        if ((it->first.compare(token1) == 0) &&
            (it->second.compare(token2) == 0))
            return true;

    return false;
}

void
addIfNotContained(const std::string &token, std::list<std::string> & lst)
{
    if (!contains(token, lst)) {
        lst.push_back(token);
    }
}

void
addIfNotContained(const std::string &token1, const std::string &token2,
                  std::list<std::pair<std::string, std::string> > &lst)
{

    if (!contains(token1, token2, lst)) {
        lst.push_back(std::pair<std::string, std::string>(token1, token2));
    }
}

std::string
removeApostrophe(const std::string &data)
{
    if (data[0] == '\'') {
        assert_s(data[data.length()-1] == '\'', "not matching ' ' \n");
        return data.substr(1, data.length() - 2);
    } else {
        return data;
    }
}

bool
hasApostrophe(const std::string &data)
{
    return ((data[0] == '\'') && (data[data.length()-1] == '\''));
}

std::string
homomorphicAdd(const std::string &val1, const std::string &val2,
               const std::string &valn2)
{
    ZZ z1 = ZZFromStringFast(padForZZ(val1));
    ZZ z2 = ZZFromStringFast(padForZZ(val2));
    ZZ n2 = ZZFromStringFast(padForZZ(valn2));
    ZZ res = MulMod(z1, z2, n2);
    return StringFromZZ(res);
}

std::string
toLowerCase(const std::string &token)
{
    std::string s = token;
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

std::string
toUpperCase(const std::string &token)
{
    std::string s = token;
    transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

bool
equalsIgnoreCase(const std::string &s1, const std::string &s2)
{
    return toLowerCase(s1) == toLowerCase(s2);
}

