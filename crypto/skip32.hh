#pragma once

#include <string.h>
#include <stdint.h>
#include <vector>

#include <util/errstream.hh>

class skip32 {
 public:
    skip32(const std::vector<uint8_t> &k) {
        throw_c(k.size() == 10);
        key = k;
    }

    void block_encrypt(const void *ptext, void *ctext) const {
        memcpy(ctext, ptext, 4);
        process((uint8_t*) ctext, 1);
    }

    void block_decrypt(const void *ctext, void *ptext) const {
        memcpy(ptext, ctext, 4);
        process((uint8_t*) ptext, 0);
    }

    static const size_t blocksize = 4;

 private:
    void process(uint8_t *data, int encrypt) const;

    std::vector<uint8_t> key;
};
