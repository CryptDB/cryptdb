#pragma once

#include <string.h>

template<class Hash>
class hmac {
 public:
    hmac(const void *keydata, size_t keylen) {
        static_assert(Hash::blocksize >= Hash::hashsize,
                      "Hack::blocksize should be >= Hash::hashsize");

        uint8_t k[Hash::blocksize];
        memset(k, 0, sizeof(k));
        if (keylen <= Hash::blocksize) {
            memcpy(k, keydata, keylen);
        } else {
            Hash kh;
            kh.update(keydata, keylen);
            kh.final(k);
        }

        for (size_t i = 0; i < Hash::blocksize; i++) {
            opad[i] = k[i] ^ 0x5c;
            ipad[i] = k[i] ^ 0x36;
        }

        h.update(ipad, sizeof(ipad));
    }

    void update(const void *data, size_t len) {
        h.update(data, len);
    }

    void final(uint8_t *buf) {
        uint8_t inner[Hash::hashsize];
        h.final(inner);

        Hash outer;
        outer.update(opad, sizeof(opad));
        outer.update(inner, sizeof(inner));
        outer.final(buf);
    }

    std::string final() {
        std::string v;
        v.resize(Hash::hashsize);
        final((uint8_t*) v.data());
        return v;
    }

    static std::string mac(const std::string &v, const std::string &key) {
        hmac x(&key[0], key.size());
        x.update(&v[0], v.size());
        return x.final();
    }

 private:
    uint8_t opad[Hash::blocksize];
    uint8_t ipad[Hash::blocksize];
    Hash h;
};
