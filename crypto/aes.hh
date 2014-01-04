#pragma once

#include <stdint.h>
#include <openssl/aes.h>
#include <string>
#include <cstring>

#include <util/errstream.hh>

class AES {
 public:
    AES(const std::string &key) {
        throw_c(key.size() == 16 || key.size() == 24 || key.size() == 32);
        AES_set_encrypt_key((const uint8_t*) key.data(), key.size() * 8, &enc);
        AES_set_decrypt_key((const uint8_t*) key.data(), key.size() * 8, &dec);
    }

    void block_encrypt(const void *ptext, void *ctext) const {
        AES_encrypt((const uint8_t*) ptext, (uint8_t*) ctext, &enc);
    }

    void block_decrypt(const void *ctext, void *ptext) const {
        AES_decrypt((const uint8_t*) ctext, (uint8_t*) ptext, &dec);
    }

    static const size_t blocksize = 16;

 private:
    AES_KEY enc;
    AES_KEY dec;
};

