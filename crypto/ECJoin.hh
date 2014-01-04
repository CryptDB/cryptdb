#pragma once

/*
 * ECJoin.h
 *
 * Implements CryptDB's adjustable join encryption scheme.
 * It is based on the elliptic-curve DDH assumption.
 *
 * Currently, it is using the NIST curve denoted NID_X9_62_prime192v1 believed to satisfy ECDDH
 * To use a different NIST curve, simply modify the NID field below.
 *
 *
 * Public parameters:
 *              G: elliptic curve group of prime order
 *              P: a random point on the elliptic curve
 *              order: the order of G
 *
 * To encrypt v, we compute E_k[v] := PRF_k(v) * k * P \in G, where
 *              k: secret key
 *
 * To adjust encryption of v under k1, to encryption under k2, we perform:
 *              \delta k = k2 * k1^{-1} mod order
 *              E_k2[v] = E_k1[v]*\delta k \in G
 *
 * TODO: may speed up by:
 * - passing BN_CTX * to some functions (and CTX for point, group, if any)
 * - using ssl's preprocessing functions
 * - use NIST curves with less bits representation, e.g., NID_secp160r1; there are even shorter
 */

#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <openssl/ec.h>

#include <crypto/BasicCrypto.hh>


struct ECJoinSK{
    const AES_KEY * aesKey;
    BIGNUM * k; //secret key
    EC_POINT * kP;
};

struct ECDeltaSK {
    BIGNUM * deltaK;
    EC_GROUP * group;
    BIGNUM * ZeroBN;
};

class ECJoin
{
public:
    //setups the elliptic curve and systems parameters
    ECJoin();

    ECJoinSK * getSKey(const AES_KEY * baseKey, const std::string & columnKey);

    //returns secret key needed to adjust from encryption with key 1 to encryption with key 2
    ECDeltaSK * getDeltaKey(const ECJoinSK * key1, const ECJoinSK *  key2);

    std::string encrypt(const ECJoinSK * sk, const std::string & ptext);
    static std::string adjust(const ECDeltaSK * deltaSK, const std::string & ctext);

    virtual
    ~ECJoin();

private:
    //setup parameters
    EC_GROUP * group;
    EC_POINT * P;


    //helper parameters
    BIGNUM * order; //order of the group
    EC_POINT * Infty;
    BIGNUM * ZeroBN;
    BN_CTX * bn_ctx;


    //using curve of 160 bits prime field \n";
    static const int NID = NID_X9_62_prime192v1;
    static const unsigned int bytesLong = 24;
    static const unsigned int MAX_BUF = 256;

    /*** Helper Functions ***/

    //returns a random point on the EC
    EC_POINT * randomPoint();
    // a PRF with 128 bits security, but bytesLong output
    static std::string PRFForEC(const AES_KEY * sk, const std::string & ptext);
    static std::string point2Str(const EC_GROUP * group, const EC_POINT * point);
};
