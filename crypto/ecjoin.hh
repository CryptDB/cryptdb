#pragma once

#include <openssl/ec.h>
#include <openssl/objects.h>
#include <crypto/aes.hh>
#include <crypto/bn.hh>
#include <crypto/ec.hh>

static int ecjoin_default_curve = NID_X9_62_prime192v1;

class ecjoin {
 public:
    ecjoin(int curve_id = ecjoin_default_curve);
    ~ecjoin();

    static ec_point adjust(const ec_point &p, const bignum &delta_k);

 protected:
    EC_GROUP *group;
    bignum order;
};

class ecjoin_priv : public ecjoin {
 public:
    ecjoin_priv(const std::string &base_key,
                 int curve_id = ecjoin_default_curve);

    ec_point hash(const std::string &ptext, const std::string &k);
    bignum delta(const std::string &k0, const std::string &k1);

 private:
    AES base;
    ec_point basept;
};
