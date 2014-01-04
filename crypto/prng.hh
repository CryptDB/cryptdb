#pragma once

#include <string.h>
#include <string>
#include <vector>
#include <fstream>
#include <util/errstream.hh>
#include <NTL/ZZ.h>
#include <crypto/bn.hh>

class PRNG {
 public:
    template<class T>
    T rand() {
        T v;
        rand_bytes(sizeof(T), (uint8_t*) &v);
        return v;
    }

    std::string rand_string(size_t nbytes) {
        std::string s;
        s.resize(nbytes);
        rand_bytes(nbytes, (uint8_t*) &s[0]);
        return s;
    }

    template<class T>
    std::vector<T> rand_vec(size_t nelem) {
        std::vector<T> buf(nelem);
        rand_bytes(nelem * sizeof(T), (uint8_t*) &buf[0]);
        return buf;
    }

    NTL::ZZ rand_zz_mod(const NTL::ZZ &max) {
        uint8_t buf[NumBits(max)/8 + 1];
        rand_bytes(sizeof(buf), buf);
        return NTL::ZZFromBytes(buf, sizeof(buf)) % max;
    }

    NTL::ZZ rand_zz_nbits(size_t nbits);
    NTL::ZZ rand_zz_prime(size_t nbits);

    bignum rand_bn_mod(const bignum &max) {
        uint8_t buf[BN_num_bytes(max.bn())];
        rand_bytes(sizeof(buf), buf);
        return bignum(buf, sizeof(buf)) % max;
    }

    virtual ~PRNG() {}
    virtual void rand_bytes(size_t nbytes, uint8_t *buf) = 0;
    virtual void seed_bytes(size_t nbytes, uint8_t *buf) {
        throw_c(false, "seed not implemented");
    }
};

class urandom : public PRNG {
 public:
    urandom();
    virtual ~urandom() {}
    virtual void rand_bytes(size_t nbytes, uint8_t *buf);
    virtual void seed_bytes(size_t nbytes, uint8_t *buf);

 private:
    std::fstream f;
};

template<class StreamCipher>
class streamrng : public PRNG {
 public:
    template<typename... ArgTypes>
    streamrng(ArgTypes... args) : c(args...) {}

    virtual void rand_bytes(size_t nbytes, uint8_t *buf) {
        for (size_t i = 0; i < nbytes; i++)
            buf[i] = c.getbyte();
    }

 private:
    StreamCipher c;
};

template<class BlockCipher>
class blockrng : public PRNG {
 public:
    template<typename... ArgTypes>
    blockrng(ArgTypes... args) : bc(args...) {
        memset(ctr, 0, sizeof(ctr));
    }

    virtual void rand_bytes(size_t nbytes, uint8_t *buf) {
        for (size_t i = 0; i < nbytes; i += bc.blocksize) {
            for (uint j = 0; j < BlockCipher::blocksize; j++) {
                ctr[j]++;
                if (ctr[j] != 0)
                    break;
            }

            uint8_t ct[bc.blocksize];
            bc.block_encrypt(ctr, ct);

            memcpy(&buf[i], ct, bc.blocksize < (nbytes - i) ? bc.blocksize : (nbytes - i));
        }
    }

    void set_ctr(const std::string &v) {
        throw_c(v.size() == BlockCipher::blocksize);
        memcpy(ctr, v.data(), BlockCipher::blocksize);
    }

 private:
    BlockCipher bc;
    uint8_t ctr[BlockCipher::blocksize];
};

template<>
inline bool
PRNG::rand<bool>()
{
    uint8_t v;
    rand_bytes(1, &v);
    return v & 1;
}

template<>
inline std::vector<bool>
PRNG::rand_vec<bool>(size_t nelem)
{
    uint8_t buf[nelem];
    rand_bytes(nelem, &buf[0]);
    for (size_t i = 0; i < nelem; i++)
        buf[i] &= 1;

    return std::vector<bool>(&buf[0], &buf[nelem]);
}
