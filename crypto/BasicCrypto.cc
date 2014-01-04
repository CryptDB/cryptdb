/*
 * BasicCrypto.cc
 *
 *
 */

#include <climits>

#include <crypto/BasicCrypto.hh>
#include <util/ctr.hh>
#include <util/util.hh>
#include <util/cryptdb_log.hh>


using namespace std;


bool
rounded_len(unsigned long len, uint block_size, bool dopad,
            unsigned long *const out) {
    assert(out);
    if (dopad || (len % block_size)) {
        if (ULONG_MAX / block_size < (len/block_size + 1)) {
            return false;
        }
        *out = (len/block_size + 1) * block_size;
        return true;
    } else {
        *out = len;
        return true;
    }
}


string
getKey(const AES_KEY * const masterKeyArg, const string &uniqueFieldName,
                      SECLEVEL sec)
{
    string id = uniqueFieldName + strFromVal((unsigned int) sec);

    unsigned char shaDigest[SHA_DIGEST_LENGTH];
    SHA1((const uint8_t *) &id[0], id.length(), shaDigest);

    string result;
    result.resize(AES_BLOCK_BYTES);
    AES_encrypt(shaDigest, (uint8_t *) &result[0], masterKeyArg);
    return result;
}

string
marshallKey(const string &key)
{
    // we will be sending key as two big nums
    string res = "";

    for (unsigned int i = 0; i < AES_KEY_BYTES; i++) {
        res = res + strFromVal((unsigned int)(key[i])) + ",";
    }

    //remove last comma
    res.resize(res.length() - 1);
    return res;
}

// FIXME: Memleak.
AES_KEY *
getKey(const string & key) {
    AES_KEY * resKey = new AES_KEY();
    string mkey = key;

    // PAD KEY to be AES_KEY_BYTES bytes long
    if (mkey.size() < AES_KEY_BYTES) {
      char buf[AES_KEY_BYTES];
      memset(buf, 0, sizeof(buf));
      memcpy(buf, mkey.data(), mkey.size());
      mkey = string(buf, sizeof(buf));
    }

    AES_set_encrypt_key(
            (const uint8_t *) mkey.data(), AES_KEY_BYTES*8, resKey);

    return resKey;
}

AES_KEY *
get_AES_KEY(const string &key)
{
    return get_AES_enc_key(key);

}

AES_KEY *
get_AES_enc_key(const string &key)
{
    ANON_REGION(__func__, &perf_cg);

    AES_KEY * aes_key = new AES_KEY();

    throw_c(AES_set_encrypt_key((const uint8_t*) key.c_str(),
                                AES_KEY_BYTES*8, aes_key) >= 0);

    return aes_key;
}


AES_KEY *
get_AES_dec_key(const string &key)
{
    //ANON_REGION(__func__, &perf_cg);

    AES_KEY *const aes_key = new AES_KEY();

    if (key.size() != AES_KEY_BYTES) {
        throw CryptoError("AES key is the wrong size!");
    }

    throw_c(AES_set_decrypt_key((const unsigned char*)key.c_str(),
                                AES_KEY_BYTES*8, aes_key) >= 0);

    return aes_key;

}


template<typename SIZE_T>
static SIZE_T
getBlocks(unsigned int unit, SIZE_T len) {
    SIZE_T blocks = len / unit;
    if (len > blocks * unit) {
        blocks++;
    }
    return blocks;
}


vector<unsigned char>
getXorVector(size_t len, const AES_KEY * key, uint64_t salt)
{
    size_t AESBlocks = len / AES_BLOCK_BYTES;
    if (AESBlocks * AES_BLOCK_BYTES < len) {
        AESBlocks++;
    }

    //construct vector with which we will XOR
    vector<unsigned char> v(AESBlocks * AES_BLOCK_BYTES);

    for (unsigned int i = 0; i < AESBlocks; i++) {
        AES_encrypt((const uint8_t*) BytesFromInt(salt+i,
                                                  AES_BLOCK_BYTES).c_str(),
                    &v[i*AES_BLOCK_BYTES], key);
    }
    return v;
}


string
encrypt_AES(const string &ptext, const AES_KEY * key, uint64_t salt)
{
    vector<unsigned char> xorVector = getXorVector(ptext.length(), key, salt);

    stringstream ss;
    for (unsigned int i = 0; i < ptext.length(); i++) {
        ss << (uint8_t) (((uint8_t)ptext[i]) ^ xorVector[i]);
    }

    return ss.str();
}

string
decrypt_AES(const string &ctext, const AES_KEY * key, uint64_t salt)
{
    vector<unsigned char> xorVector = getXorVector(ctext.length(), key, salt);

    stringstream ss;
    for (unsigned int i = 0; i < ctext.length(); i++)
        ss << (uint8_t) (((uint8_t)ctext[i]) ^ xorVector[i]);

    return ss.str();
}

static vector<unsigned char>
getIVec(string salt)
{
    vector<unsigned char> ivec(AES_BLOCK_BYTES);

    memcpy(&ivec[0], salt.data(), min(salt.length(), (size_t) AES_BLOCK_BYTES));

    return ivec;
}

static vector<unsigned char>
pad(vector<unsigned char> data, unsigned int unit)
{
    // pad does not work for padding unit more than 256 bytes
    throw_c(unit < 256);

    size_t blocks = getBlocks(unit, data.size());
    size_t multipleLen = blocks * unit;
    size_t padding;
    if (multipleLen == data.size()) {
        padding = unit;
    } else {
        padding = multipleLen - data.size();
    }
    throw_c(padding > 0 && padding <= AES_BLOCK_BYTES);
    size_t paddedLen = data.size() + padding;
    throw_c((paddedLen > 0) && ((paddedLen % AES_BLOCK_BYTES) == 0));

    // cerr << "length of padding " << padding << " length of padded data " << paddedLen << "\n";

    vector<unsigned char> res(paddedLen, 0);
    res[paddedLen-1] = (unsigned char)padding;
    memcpy(&res[0], &data[0], data.size());
    return res;
}

static vector<unsigned char>
unpad(vector<unsigned char> data)
{
    const size_t len = data.size();
    throw_c((len > 0) && ((len % AES_BLOCK_BYTES) == 0));
    const size_t pad_count = static_cast<int>(data[len-1]);
    //cerr << "padding to remove " << (int)data[len-1] << "\n";
    const size_t actualLen = len - pad_count;
    //cerr << " len is " << len << " and data[len-1] " << (int)data[len-1] << "\n";
    // Padding will never be larger than a block.
    if (false == ((pad_count > 0) && (pad_count <= AES_BLOCK_BYTES))) {
        throw CryptoError("AES padding is wrong size!");
    }
    // Tells us when we have a bad length.
    throw_c(pad_count <= len);
    vector<unsigned char> res(actualLen);
    memcpy(&res[0], &data[0], actualLen);
    return res;
}


string
encrypt_AES_CBC(const string &ptext, const AES_KEY * enckey, string salt, bool dopad)
{
    //TODO: separately for numbers to avoid need for padding

    throw_c(dopad || ((ptext.size() % AES_BLOCK_BYTES) == 0));

    vector<unsigned char> ptext_buf;
    if (dopad) {
        ptext_buf = pad(vector<unsigned char>(ptext.begin(), ptext.end()), AES_BLOCK_BYTES);
        // cerr << "padded data is " << stringToByteInts(string((char *) &ptext_buf[0], ptext_buf.size())) << "\n";
    } else {
        ptext_buf = vector<unsigned char>(ptext.begin(), ptext.end());
    }
    auto ctext_buf = vector<unsigned char>(ptext_buf.size());
    auto ivec = getIVec(salt);

    AES_cbc_encrypt(&ptext_buf[0], &ctext_buf[0], ptext_buf.size(), enckey, &ivec[0], AES_ENCRYPT);

    // cerr << "encrypted data is " << stringToByteInts(string((char *) &ctext_buf[0], ctext_buf.size())) << '\n';

    return string((char *) &ctext_buf[0], ctext_buf.size());
}

string
decrypt_AES_CBC(const string &ctext, const AES_KEY * deckey, string salt, bool dounpad)
{
    throw_c((ctext.size() > 0) && ((ctext.size() % AES_BLOCK_BYTES) == 0));

    vector<unsigned char> ptext_buf(ctext.size());
    auto ivec = getIVec(salt);

    AES_cbc_encrypt((const unsigned char *) ctext.data(), &ptext_buf[0], ctext.size(), deckey, &ivec[0], AES_DECRYPT);


    if (dounpad) {
        auto res = unpad(ptext_buf);
        // cerr << "unpadded dec data is " << stringToByteInts(string((char * ) &res[0], res.size())) << "\n";
        return string((char *)&res[0], res.size());
    } else {
        return string((char *)&ptext_buf[0], ptext_buf.size());
    }
}

//TODO: have some helpers that only manipulate unsigned char * and convert in string at the end

static string
reverse(const string & vec)
{
    size_t len = vec.length();
    size_t noBlocks = len /AES_BLOCK_BYTES;

    throw_c(len == noBlocks * AES_BLOCK_BYTES);
    string rev;
    rev.resize(len);

    for (unsigned int i = 0; i < noBlocks; i++) {
        memcpy(&rev[i * AES_BLOCK_BYTES],
               &vec[(noBlocks-i-1)*AES_BLOCK_BYTES], AES_BLOCK_BYTES);
    }

    return rev;
}

//DID WE DECIDE ON ONE OR TWO KEYS?!
string
encrypt_AES_CMC(const string &ptext, const AES_KEY * enckey, bool dopad)
{
    string firstenc = encrypt_AES_CBC(ptext, enckey, "0", dopad);

    string rev = reverse(firstenc);

    return encrypt_AES_CBC(rev, enckey, "0", false);
}

string
decrypt_AES_CMC(const string &ctext, const AES_KEY * deckey, bool dopad)
{
    string firstdec = decrypt_AES_CBC(ctext, deckey, "0", false);

    string reversed = reverse(firstdec);

    return decrypt_AES_CBC(reversed, deckey, "0", dopad);
}


//**************** Public Key Cryptosystem (PKCS)
// ****************************************/

//marshall key
static string
DER_encode_RSA_public(RSA *rsa)
{
    string s;
    s.resize(i2d_RSAPublicKey(rsa, 0));

    uint8_t *next = (uint8_t *) &s[0];
    i2d_RSAPublicKey(rsa, &next);
    return s;
}

static RSA *
DER_decode_RSA_public(const string &s)
{
    const uint8_t *buf = (const uint8_t*) s.data();
    return d2i_RSAPublicKey(0, &buf, s.length());
}

//marshall key
static string
DER_encode_RSA_private(RSA *rsa)
{
    string s;
    s.resize(i2d_RSAPrivateKey(rsa, 0));

    uint8_t *next = (uint8_t *) &s[0];
    i2d_RSAPrivateKey(rsa, &next);
    return s;
}

static RSA *
DER_decode_RSA_private(const string &s)
{
    const uint8_t *buf = (const uint8_t*) s.data();
    return d2i_RSAPrivateKey(0, &buf, s.length());
}

static void
remove_private_key(RSA *r)
{
    r->d = r->p = r->q = r->dmp1 = r->dmq1 = r->iqmp = 0;
}

//Credits: the above five functions are from "secure programming cookbook for
// C++"

void
generateKeys(PKCS * & pk, PKCS * & sk)
{
    LOG(crypto) << "pkcs generate";
    PKCS * key =  RSA_generate_key(PKCS_bytes_size*8, 3, NULL, NULL);

    sk = RSAPrivateKey_dup(key);

    pk = key;
    remove_private_key(pk);

}

string
marshallKey(PKCS * mkey, bool ispk)
{
    LOG(crypto) << "pkcs encrypt";
    string key;
    if (!ispk) {
        key = DER_encode_RSA_private(mkey);
    } else {
        key = DER_encode_RSA_public(mkey);
    }
    throw_c(key.length() >= 1);  // issue with RSA pk
    return key;
}

PKCS *
unmarshallKey(const string &key, bool ispk)
{
    LOG(crypto) << "pkcs decrypt";
    //cerr << "before \n";
    if (ispk) {
        return DER_decode_RSA_public(key);
    } else {
        return DER_decode_RSA_private(key);
    }
}

string
encrypt(PKCS * key, const string &s)
{
    string tocipher;
    tocipher.resize(RSA_size(key));

    RSA_public_encrypt((int) s.length(),
                       (const uint8_t*) s.data(), (uint8_t*) &tocipher[0],
                       key,
                       RSA_PKCS1_OAEP_PADDING);

    return tocipher;
}

string
decrypt(PKCS * key, const string &s)
{
    throw_c(s.length() == (uint)RSA_size(key));
    string toplain;
    toplain.resize(RSA_size(key));

    uint len =
        RSA_private_decrypt((int) s.length(),
                            (const uint8_t*) s.data(),
                            (uint8_t*) &toplain[0], key,
                            RSA_PKCS1_OAEP_PADDING);
    toplain.resize(len);

    return toplain;
}

void
freeKey(PKCS * key)
{
    RSA_free(key);
}


string
getLayerKey(const AES_KEY * const mKey, string uniqueFieldName,
            SECLEVEL l) {
    if (l == SECLEVEL::DETJOIN) {
        return getKey(mKey, "joinjoin", l);
    }
    return getKey(mKey, uniqueFieldName, l);
}
