/*
 * Crypto.cpp
 *
 *
 *      Author: raluca
 */

#include <iostream>
#include <fstream>

#include <crypto/SWPSearch.hh>
#include <util/util.hh>


using namespace std;

unsigned char fixedIV[] =
{34, 145, 42, 12, 56, 13, 111, 100, 98, 6, 2, 63, 88, 4, 22, 74};

static string iv_det;

static const bool DEBUG = false;

string
SWP::PRP(const string & key, const string & val)
{

    return encryptSym(key, val, key);

}

/*************** Sym key crypto ******************/

//pads with 10..00 making length of result be a multiple of len (bytes)
static string
pad(const string & vec, unsigned int len)
{

    string result;

    unsigned int newlen;
    if (vec.length() % len == 0) {
        newlen = vec.length() + len;
    } else {
        newlen = (vec.length() / len + 1) * len;
    }

    result = vec;
    result.resize(newlen);

    result[vec.length()] = 1;

    for (unsigned int i = vec.length()+1; i < result.length(); i++) {
        result[i] = 0;
    }

    return result;

}

static string
unpad(const string & vec)
{

    int index = vec.length() - 1;

    while ((index > 0) && (vec[index] == 0)) {
        index--;
    }

    throw_c((index>=0) && (vec[index] == 1),
            "input was not padded correctly");

    return vec.substr(0, index);
}

string
SWP::encryptSym(const string & key, const string & val, const string & iv)
{

    throw_c(key.length() == AES_BLOCK_SIZE, "key has incorrect length");
    throw_c(iv.length() == AES_BLOCK_SIZE, "iv has incorrect length");


    AES_KEY aes_key;

    AES_set_encrypt_key((const unsigned char *)key.c_str(), AES_BLOCK_BITS, &aes_key);

    string val2 = pad(val, AES_BLOCK_SIZE);

    unsigned int newlen = val2.length();

    auto result = vector<unsigned char>(newlen);

    unsigned char * newiv = new unsigned char[iv.length()];
    memcpy(newiv, iv.c_str(), iv.length());

    AES_cbc_encrypt((const unsigned char *)val2.c_str(), &result[0], newlen, &aes_key,
                    newiv, AES_ENCRYPT);

    return string((char*)&result[0], newlen);
}

string
SWP::decryptSym(const string & key, const string & ciph, const string & iv)
{

    throw_c(iv.length() == AES_BLOCK_SIZE, "iv has incorrect length");
    throw_c(key.length() == AES_BLOCK_SIZE, "key has incorrect length");

    AES_KEY aes_key;

    AES_set_decrypt_key((const unsigned char *)key.c_str(), AES_BLOCK_BITS, &aes_key);

    unsigned int ciphlen = ciph.length();
    auto result = new unsigned char[ciphlen];

    unsigned char * newiv = new unsigned char[iv.length()];
    memcpy(newiv, iv.c_str(), iv.length());

    AES_cbc_encrypt((const unsigned char*)ciph.c_str(), result, ciphlen, &aes_key,
                    newiv, AES_DECRYPT);

    return unpad(string((const char *)result, ciphlen));

}

string
SWP::encryptSym(const string & key, const string & val)
{
    string salt = random(SWP_SALT_LEN);
    string iv = PRP(key, salt);
    string ciph = encryptSym(key, val, iv);

    return salt+ciph;
}

string
SWP::decryptSym(const string & key, const string & ciph)
{
    string salt = ciph.substr(0, SWP_SALT_LEN);
    string iv = PRP(key, salt);
    string ciph2 = ciph.substr(SWP_SALT_LEN, ciph.length() - SWP_SALT_LEN);

    return decryptSym(key, ciph2, iv);
}

string
SWP::random(unsigned int nobytes)
{
    unsigned char * bin = new unsigned char[nobytes];
    RAND_bytes(bin, nobytes);

    return string((const char *)bin, nobytes);
}

/**************************** SWP ****************/

// this function performs half of the encrypt job up to the point at which
// tokens are generated
void
SWP::SWPHalfEncrypt(const string & key, string word, string & ciph,
                    string & wordKey)
{
    //encryption of word E[W_i]
    string IV((const char *)fixedIV, AES_BLOCK_SIZE);
    ciph = encryptSym(key, word, IV);

    //L_i and R_i
    string L_i;
    if (SWP::canDecrypt) {
        L_i = ciph.substr(0, SWPr);
    }

    //wordKey: k_i = PRP_{key}(E[W_i])

    if (!SWP::canDecrypt) {
        wordKey = PRP(key, ciph);
    } else {
        wordKey = PRP(key, L_i);
    }
}

static string
bytewise_xor(const string & a, const string & b) {
    throw_c(a.length() == b.length(), "bytewise_xor expects equal lengths");

    unsigned int len = a.length();

    char * res = new char[len];
    for (unsigned int i = 0; i < len; i++) {
	res[i] = a[i] xor b[i];
    }
    return string(res, len);
}

string
SWP::SWPencrypt(const string & key, string word, unsigned int index)
{

    if (DEBUG) {cerr << "encrypting " << word << "\n "; }
    string ciph, wordKey;

    SWPHalfEncrypt(key, word, ciph, wordKey);

    //S_i
    string salt;
    if (SWP::canDecrypt) {
        salt = PRP(key, strFromVal(index));
        salt = salt.substr(salt.length() - SWPr, SWPr);
    } else {
        salt = random(SWPr);
    }
    //F_{k_i} (S_i)
    string func = PRP(wordKey, salt);

    if (SWP::canDecrypt) {
        func = func.substr(SWPr, SWPm);
    }

    return bytewise_xor(ciph, salt + func);

}

Token
SWP::token(const string & key, const string & word)
{
    Token t = Token();

    SWPHalfEncrypt(key, word, t.ciph, t.wordKey);

    return t;

}

bool
SWP::SWPsearch(const Token & token, const string & ciph)
{

    if (DEBUG) { cerr << "searching! \n"; }

    //remove E[W_i]
    string ciph2 = bytewise_xor(ciph, token.ciph);

    //remains salt, PRP(wordkey, salt)
    string salt = ciph2.substr(0, SWPr);
    string funcpart = ciph2.substr(SWPr, ciph2.length() - SWPr);

    string func = PRP(token.wordKey, salt);
    if (SWP::canDecrypt) {
        func = func.substr(SWPr, SWPm);
    }

    if (func == funcpart) {
        return true;
    }

    return false;
}

list<string> *
SWP::encrypt(const string & key, const list<string> & words)
{

    list<string> * result = new list<string>();

    unsigned int index = 0;

    for (list<string>::const_iterator it = words.begin(); it != words.end();
         it++) {
        index++;

        string word = *it;

        throw_c(word.length() < SWPCiphSize, string(
                     " given word ") + word +
                 " is longer than SWPCiphSize");

        result->push_back(SWPencrypt(key, word, index));
    }

    return result;
}

string
SWP::SWPdecrypt(const string & key, const string & word, unsigned int index)
{

    //S_i
    string salt = PRP(key, strFromVal(index));
    salt = salt.substr(salt.length() - SWPr, SWPr);

    //L_i
    string L_i = bytewise_xor(salt, word.substr(0, SWPr));

    //k_i
    string wordKey = PRP(key, L_i);

    //F_{k_i} (S_i)
    string func = PRP(wordKey, salt).substr(SWPr, SWPm);

    string R_i = bytewise_xor(func, word.substr(SWPr, SWPm));

    return decryptSym(key, L_i + R_i, string((const char *)fixedIV, AES_BLOCK_SIZE));

}

list<string> *
SWP::decrypt(const string & key, const list<string>  & ciph)
{
    list<string> * result = new list<string>();

    throw_c(
        canDecrypt,
        "the current choice of parameters for SWP does not allow decryption");

    unsigned int index = 0;

    for (list<string>::const_iterator it = ciph.begin(); it != ciph.end();
         it++) {
        index++;

        string word = *it;

        throw_c(word.length() == SWPCiphSize,
                " given ciphertext with invalid length ");

        result->push_back(SWPdecrypt(key, word, index));

    }

    return result;
}

list<unsigned int> *
SWP::search(const Token & token, const list<string> & ciphs)
{
    list<unsigned int> * res = new list<unsigned int>();

    unsigned int index = 0;
    for (list<string>::const_iterator cit = ciphs.begin(); cit != ciphs.end();
         cit++) {
        if (SWPsearch(token, *cit)) {
            res->push_back(index);
        }
        index++;
    }

    return res;
}

bool
SWP::searchExists(const Token & token, const list<string> & ciphs)
{

    for (list<string>::const_iterator cit = ciphs.begin(); cit != ciphs.end();
            cit++) {
        if (SWPsearch(token, *cit)) {
            return true;
        }
    }

    return false;
}

