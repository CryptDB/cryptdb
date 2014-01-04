#pragma once

#include <openssl/sha.h>
#include <string>

template<class State,
         int OutBytes,
         int BlockBytes,
         int (*Init)(State*),
         int (*Update)(State*, const void*, size_t),
         int (*Final)(uint8_t*, State*)>
class sha {
 public:
    sha() { Init(&s); }
    void update(const void *data, size_t len) { Update(&s, data, len); }

    void final(uint8_t *buf) {
        Final(buf, &s);
    }

    std::string final() {
        std::string v;
        v.resize(hashsize);
        final((uint8_t*) v.data());
        return v;
    }

    static std::string hash(const std::string &s) {
        sha x;
        x.update(s.data(), s.length());
        return x.final();
    }

    static const size_t hashsize  = OutBytes;
    static const size_t blocksize = BlockBytes;

 private:
    State s;
};

typedef sha<SHA_CTX,    20, 64,  SHA1_Init,   SHA1_Update,   SHA1_Final>   sha1;
typedef sha<SHA256_CTX, 28, 64,  SHA224_Init, SHA224_Update, SHA224_Final> sha224;
typedef sha<SHA256_CTX, 32, 64,  SHA256_Init, SHA256_Update, SHA256_Final> sha256;
typedef sha<SHA512_CTX, 48, 128, SHA384_Init, SHA384_Update, SHA384_Final> sha384;
typedef sha<SHA512_CTX, 64, 128, SHA512_Init, SHA512_Update, SHA512_Final> sha512;
