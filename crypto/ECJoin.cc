/*
 * ECJoin.cpp
 *
 */

#include <crypto/ECJoin.hh>
#include <util/cryptdb_log.hh>
#include <util/util.hh>


using namespace std;

static EC_POINT *
my_EC_POINT_new(EC_GROUP * group) {

    EC_POINT* point = EC_POINT_new(group);
    throw_c(point, "could not create point");
    return point;

}

static BIGNUM *
my_BN_new() {

    BIGNUM* num = BN_new();
    throw_c(num, "could not create BIGNUM");
    return num;

}

static BIGNUM *
my_BN_mod(const BIGNUM * a, const BIGNUM * n, BN_CTX * bn_ctx) {

    BIGNUM * res = my_BN_new();
    BN_mod(res, a, n, bn_ctx);
    throw_c(res, "failed to compute mod");
    return res;
}

static EC_POINT *
str2Point(const EC_GROUP * group, const string & indata) {

    EC_POINT * point = EC_POINT_new(group);

    throw_c(EC_POINT_oct2point(group, point, (const unsigned char *)indata.data(), indata.length(), NULL),
            "cannot convert from ciphertext to point");

    return point;
}

EC_POINT *
ECJoin::randomPoint() {

    EC_POINT * point = my_EC_POINT_new(group);

    BIGNUM *x = my_BN_new(), *y = my_BN_new(), * rem = my_BN_new();

    bool found = false;

    while (!found) {

        BN_rand_range(x, order);

        if (EC_POINT_set_compressed_coordinates_GFp(group, point, x, 1, bn_ctx)) {
            throw_c(EC_POINT_get_affine_coordinates_GFp(group, point, x, y, bn_ctx),"issue getting coordinates");

            if(BN_is_zero(x) || BN_is_zero(y)) {
                found = false;
                continue;
            }

            if (EC_POINT_is_on_curve(group, point, bn_ctx)) {
                LOG(crypto) << "found correct random point, P";
                found = true;
            } else {
                LOG(crypto) << "P not on curve; try again";
            }
        } else {
            LOG(crypto) << "P is bad random point; try again";
        }
    }

    BN_free(x);
    BN_free(y);
    BN_free(rem);
    return point;
}


ECJoin::ECJoin()
{

    group = EC_GROUP_new_by_curve_name(NID);
    throw_c(group, "issue creating new curve");

    bn_ctx = BN_CTX_new();
    throw_c(bn_ctx, "failed to create big number context");

    order = my_BN_new();

    throw_c(EC_GROUP_get_order(group, order, bn_ctx), "failed to retrieve the order");

    Infty = my_EC_POINT_new(group);
    throw_c(EC_POINT_set_to_infinity(group, Infty), "could not create point at infinity");

    P = randomPoint();

    ZeroBN = BN_new();
    throw_c(ZeroBN != NULL, "cannot create big num");
    BN_zero(ZeroBN);


}

static EC_POINT *
mul(const EC_GROUP * group, const BIGNUM * ZeroBN, const EC_POINT * Point, const BIGNUM * Scalar, BN_CTX * bn_ctx) {

    EC_POINT * ans = EC_POINT_new(group);
    throw_c(ans, "cannot create point ");

    //ans = sk->kp * cbn
    throw_c(EC_POINT_mul(group, ans, ZeroBN, Point, Scalar, bn_ctx), "issue when multiplying ec");

    return ans;
}

static BIGNUM *
my_BN_bin2bn(string data) {
    BIGNUM * res = BN_bin2bn((unsigned char *) data.data(), (int) data.length(), NULL);
    throw_c(res, "could not convert from binary to BIGNUM ");

    return res;
}

ECJoinSK *
ECJoin::getSKey(const AES_KEY * baseKey, const string & columnKey) {


    ECJoinSK * skey = new ECJoinSK();

    skey->aesKey = baseKey;

    BIGNUM * bnkey = my_BN_bin2bn(columnKey);

    skey->k = my_BN_mod(bnkey, order, bn_ctx);

    skey->kP = mul(group, ZeroBN, P, skey->k, bn_ctx);

    BN_free(bnkey);

    return skey;
}

ECDeltaSK *
ECJoin::getDeltaKey(const ECJoinSK * key1, const ECJoinSK *  key2) {

    ECDeltaSK * delta = new ECDeltaSK();

    delta->group = group;
    delta->ZeroBN = ZeroBN;

    BIGNUM * key1Inverse = BN_mod_inverse(NULL, key1->k, order, bn_ctx);
    throw_c(key1Inverse, "could not compute inverse of key 1");

    delta->deltaK = BN_new();
    BN_mod_mul(delta->deltaK, key1Inverse, key2->k, order, bn_ctx);
    throw_c(delta->deltaK, "failed to multiply");

    BN_free(key1Inverse);

    return delta;

}

// a PRF with 128 bits security, but 160 bit output
string
ECJoin::PRFForEC(const AES_KEY * sk, const string & ptext) {

    string nptext = ptext;

    unsigned int len = (uint) ptext.length();

    if (bytesLong > len) {
        for (unsigned int i = 0 ; i < bytesLong - len; i++) {
            nptext = nptext + "0";
        }
    }

    // should collapse down to 1 AES_BLOCK_SIZE using SHA1

    return encrypt_AES(nptext, sk, 1).substr(0, bytesLong);

}

string
ECJoin::point2Str(const EC_GROUP * group, const EC_POINT * point) {
    unsigned char buf[ECJoin::MAX_BUF+1];
    memset(buf, 0, ECJoin::MAX_BUF);

    size_t len = 0;
    len = EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED, buf, MAX_BUF, NULL);

    throw_c(len, "cannot serialize EC_POINT ");

    return string((char *)buf, len);
}

string
ECJoin::encrypt(const ECJoinSK * sk, const string & ptext) {

    // CONVERT ptext in PRF(ptext)
    string ctext = PRFForEC(sk->aesKey, ptext);

    //cbn = PRF(ptext)
    BIGNUM * cbn = my_BN_bin2bn(ctext);

    BIGNUM * cbn2 = my_BN_mod(cbn, order, bn_ctx);

    //ans = sk->kp * cbn
    EC_POINT * ans = mul(group, ZeroBN, sk->kP, cbn2, bn_ctx);

    string res = point2Str(group, ans);

    EC_POINT_free(ans);
    BN_free(cbn);
    BN_free(cbn2);

    return res;
}

string
ECJoin::adjust(const ECDeltaSK * delta, const string & ctext) {

    EC_POINT * point = str2Point(delta->group, ctext);

    EC_POINT * res = mul(delta->group, delta->ZeroBN, point, delta->deltaK, NULL);

    string result = point2Str(delta->group, res);

    EC_POINT_free(res);
    EC_POINT_free(point);

    return result;
}

ECJoin::~ECJoin()
{
    BN_free(order);
    EC_POINT_free(P);
    EC_GROUP_clear_free(group);
    BN_CTX_free(bn_ctx);
}




/*** some internal test code ..
    cerr << "is key null? " << pretty_bn2str(key1->k) << "\n";
    cerr << "comp key oder " << BN_cmp(key1->k, order) << "\n";
    BIGNUM * key1Inverse = BN_mod_inverse(NULL, key1->k, order, bn_ctx);
    throw_c(key1Inverse, "could not compute inverse of key 1");

    delta->deltaK = BN_new();
    BN_mod_mul(delta->deltaK, key1Inverse, key2->k, order, bn_ctx);
    throw_c(delta->deltaK, "failed to multiply");

    //delta * k1 = k2?
    cerr << "(remove this code) \n checking delta \n";
    BIGNUM * temp = BN_new();
    BN_mod_mul(temp, delta->deltaK, key1->k, order, bn_ctx);
    cerr << "cmp temp key2 " << BN_cmp(temp, key2->k) << "\n";
    BN_free(temp);


    //let's now check that key1->kP * deltaK = key2->Kp
    cerr << "remove code here\n";
    cerr << "are the two points equal? " << EC_POINT_cmp(group, mul(group, ZeroBN, key1->kP, delta->deltaK, bn_ctx), key2->kP, bn_ctx) << "\n";

    //let's now check with the ciphertexts

    cerr << "are the points with ciph equal? " << EC_POINT_cmp(group,
                                                mul(group, ZeroBN, str2Point(group, c1), delta->deltaK, bn_ctx),
                                                str2Point(group, c2),
                                                bn_ctx) << "\n";
*/

/* static string
pretty_bn2str(BIGNUM * bn) {
        unsigned char * tov = new unsigned char[256];
        int len = BN_bn2bin(bn, tov);

        throw_c(len, "cannot convert from bn to binary ");

        string res = "";

        for (int i = 0 ; i < len ; i++) {
                res = res + StringFromVal(tov[i]) + " ";
        }
        return res;
}
*/
