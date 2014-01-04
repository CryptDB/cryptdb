#pragma once

#include <vector>
#include <stdint.h>
#include <openssl/blowfish.h>

class blowfish {
 public:
    blowfish(const std::string &key) {
        BF_set_key(&k, key.size(), (const uint8_t*) key.data());
    }

    void block_encrypt(const void *ptext, void *ctext) const {
        BF_ecb_encrypt((const uint8_t*) ptext, (uint8_t*) ctext, &k, BF_ENCRYPT);
    }

    void block_decrypt(const void *ctext, void *ptext) const {
        BF_ecb_encrypt((const uint8_t*) ctext, (uint8_t*) ptext, &k, BF_DECRYPT);
    }

    uint64_t encrypt(uint64_t pt) const {
        uint64_t ct;
        block_encrypt(&pt, &ct);
        return ct;
    }

    uint64_t decrypt(uint64_t ct) const {
        uint64_t pt;
        block_decrypt(&ct, &pt);
        return pt;
    }

    static const size_t blocksize = 8;

 private:
    BF_KEY k;
};
