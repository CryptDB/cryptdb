#include <crypto/ecjoin.hh>
#include <crypto/sha.hh>
#include <crypto/prng.hh>
#include <crypto/arc4.hh>

ecjoin::ecjoin(int curve_id)
{
    group = EC_GROUP_new_by_curve_name(curve_id);
    throw_c(group);

    throw_c(EC_GROUP_get_order(group, order.bn(), _bignum_ctx::the_ctx()));
}

ecjoin::~ecjoin()
{
    EC_GROUP_free(group);
}

ec_point
ecjoin::adjust(const ec_point &p, const bignum &delta_k)
{
    return p * delta_k;
}


ecjoin_priv::ecjoin_priv(const std::string &base_key, int curve_id)
    : ecjoin(curve_id), base(sha256::hash(base_key)), basept(group)
{
    streamrng<arc4> r(base_key);

    for (;;) {
        bignum x = r.rand_bn_mod(order);
        if (!EC_POINT_set_compressed_coordinates_GFp(group, basept.p(),
                                                     x.bn(), 1,
                                                     _bignum_ctx::the_ctx()))
            continue;

        bignum y;
        throw_c(EC_POINT_get_affine_coordinates_GFp(group, basept.p(),
                                                   x.bn(), y.bn(),
                                                   _bignum_ctx::the_ctx()));
        if (x == 0 || y == 0)
            continue;

        if (EC_POINT_is_on_curve(group, basept.p(), _bignum_ctx::the_ctx()))
            break;
    }
}

ec_point
ecjoin_priv::hash(const std::string &ptext, const std::string &k)
{
    auto hash = sha1::hash(ptext);
    throw_c(hash.size() >= base.blocksize);
    hash.resize(base.blocksize);

    std::string enc;
    enc.resize(base.blocksize);
    base.block_encrypt(&hash[0], &enc[0]);

    bignum kn(sha256::hash(k));
    bignum enc_bn(enc);

    return basept * (kn % order) * (enc_bn % order);
}

bignum
ecjoin_priv::delta(const std::string &k0, const std::string &k1)
{
    bignum kn0(sha256::hash(k0));
    bignum kn1(sha256::hash(k1));

    return ((kn1 % order) * kn0.invmod(order)) % order;
}
