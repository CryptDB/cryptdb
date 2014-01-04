#pragma once

/*
 * BasicCrypto.h
 *
 *  Basic symmetric key crypto.
 */

#include <map>
#include <string>
#include <vector>
#include <stdio.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <crypto/prng.hh>

#include <util/onions.hh>

std::string getLayerKey(const AES_KEY * const mKey,
                        std::string uniqueFieldName, SECLEVEL l);

AES_KEY * getKey(const std::string & key);
/**
 * Returns the key corresponding to the security level given for some
 * master key and some unique field name. Result will be AES_KEY_SIZE
 * long.
 */
std::string getKey(const AES_KEY * const mkey,
                   const std::string &uniqueFieldName, SECLEVEL sec);

AES_KEY * get_AES_KEY(const std::string &key);
AES_KEY * get_AES_enc_key(const std::string & key);
AES_KEY * get_AES_dec_key(const std::string & key);

bool rounded_len(unsigned long len, uint block_size, bool dopad,
                 unsigned long *const out);

std::string marshallKey(const std::string &key);
//std::string unmarshallKey(const std::string &key);

std::vector<unsigned char>
getXorVector(size_t len, const AES_KEY * key, uint64_t salt);

std::string
encrypt_AES(const std::string & plaintext, const AES_KEY * key,  uint64_t salt);

std::string
decrypt_AES(const std::string & ciphertext, const AES_KEY * key,  uint64_t salt);

std::string
encrypt_AES_CBC(const std::string &ptext, const AES_KEY * enckey, std::string salt, bool pad = true);

std::string
decrypt_AES_CBC(const std::string &ctext, const AES_KEY * deckey, std::string salt, bool pad = true);

//only works for padding unit < 255 bytes
//std::vector<unsigned char> pad(std::vector<unsigned char> data, unsigned int unit);
//std::vector<unsigned char> unpad(std::vector<unsigned char> data);

std::string
encrypt_AES_CMC(const std::string &ptext, const AES_KEY * enckey, bool dopad = true);

std::string
decrypt_AES_CMC(const std::string &ctext, const AES_KEY * deckey, bool dopad = true);


//**** Public Key Cryptosystem (PKCS) *****//

typedef RSA PKCS;

const unsigned int PKCS_bytes_size = 256;     //this is the size in
// openssl

//generates a new key
void generateKeys(PKCS * & pk, PKCS * & sk);

//marshalls a key
//if ispk is true, it returns the binary of the public key and sets let to
// the length returned
//is !ispk, it does the same for secret key
std::string marshallKey(PKCS * mkey, bool ispk);

//from a binary key of size keylen, it returns a public key if ispk, else
// a secret key
PKCS * unmarshallKey(const std::string &key, bool ispk);

//using key, it encrypts the data in from of size fromlen
//the result is len long
std::string encrypt(PKCS * key, const std::string &from);

//using key, it decrypts data at fromcipher, and returns the decrypted
// value
std::string decrypt(PKCS * key, const std::string &fromcipher);

//frees memory allocated by this keyb
void freeKey(PKCS * key);


