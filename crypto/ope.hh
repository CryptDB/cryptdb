#pragma once

#include <string>
#include <map>
#include <crypto/prng.hh>
#include <crypto/aes.hh>
#include <crypto/sha.hh>
#include <NTL/ZZ.h>

class ope_domain_range {
 public:
    ope_domain_range(const NTL::ZZ &d_arg,
                     const NTL::ZZ &r_lo_arg,
                     const NTL::ZZ &r_hi_arg)
        : d(d_arg), r_lo(r_lo_arg), r_hi(r_hi_arg) {}
    NTL::ZZ d, r_lo, r_hi;
};

class OPE {
 public:
    OPE(const std::string &keyarg, size_t plainbits, size_t cipherbits)
    : key(keyarg), pbits(plainbits), cbits(cipherbits), aesk(aeskey(key)) {}

    NTL::ZZ encrypt(const NTL::ZZ &ptext);
    NTL::ZZ decrypt(const NTL::ZZ &ctext);

 private:
    static std::string aeskey(const std::string &key) {
        auto v = sha256::hash(key);
        v.resize(16);
        return v;
    }

    std::string key;
    size_t pbits, cbits;

    AES aesk;
    std::map<NTL::ZZ, NTL::ZZ> dgap_cache;

    template<class CB>
    ope_domain_range search(CB go_low);

    template<class CB>
    ope_domain_range lazy_sample(const NTL::ZZ &d_lo, const NTL::ZZ &d_hi,
                                 const NTL::ZZ &r_lo, const NTL::ZZ &r_hi,
                                 CB go_low, blockrng<AES> *prng);
};
