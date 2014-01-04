#pragma once

#include <vector>
#include <stdint.h>

template<class BlockCipher>
class cbcmac {
 public:
    cbcmac(const BlockCipher *cx) {
        c = cx;
        memset(v, 0, BlockCipher::blocksize);
        mbytes = 0;
    }

    cbcmac(const BlockCipher *cx, void *iv) {
        c = cx;
        memcpy(v, iv, BlockCipher::blocksize);
        mbytes = 0;
    }

    void update(const void *data, size_t len) {
        const uint8_t *d = static_cast<const uint8_t *> (data);

        if (mbytes) {
            size_t ncopy = std::min(len, BlockCipher::blocksize - mbytes);
            memcpy(&m[mbytes], d, ncopy);
            d += ncopy;
            len -= ncopy;
            mbytes += ncopy;
        }

        if (mbytes == BlockCipher::blocksize) {
            do_block(m);
            mbytes = 0;
        }

        while (len >= BlockCipher::blocksize) {
            do_block(d);
            d += BlockCipher::blocksize;
            len -= BlockCipher::blocksize;
        }

        if (len) {
            memcpy(m, d, len);
            mbytes = len;
        }
    }

    void update(const std::string &v) {
        update(v.data(), v.size());
    }

    void final(uint8_t *buf) {
        if (mbytes) {
            memset(&m[mbytes], 0, BlockCipher::blocksize - mbytes);
            do_block(m);
        }

        memcpy(buf, v, BlockCipher::blocksize);
    }

    std::string final() {
        std::string f;
        f.resize(BlockCipher::blocksize);
        final((uint8_t*) f.data());
        return f;
    }

    static const size_t blocksize = BlockCipher::blocksize;

 private:
    void do_block(const uint8_t *p) {
        uint8_t x[BlockCipher::blocksize];
        for (size_t i = 0; i < BlockCipher::blocksize; i++)
            x[i] = v[i] ^ p[i];
        c->block_encrypt(x, v);
    }

    uint8_t v[BlockCipher::blocksize];
    uint8_t m[BlockCipher::blocksize];
    uint8_t mbytes;

    const BlockCipher *c;
};
