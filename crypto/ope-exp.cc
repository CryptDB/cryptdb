/*
 * Some experimental version of OPE.
 */

#include <crypto/ope.hh>
#include <crypto/prng.hh>
#include <crypto/hgd.hh>
#include <crypto/aes.hh>
#include <crypto/sha.hh>
#include <crypto/hmac.hh>
#include <util/zz.hh>

using namespace std;
using namespace NTL;

/*
 * A gap is represented by the next integer value _above_ the gap.
 */
static ZZ
domain_gap(const ZZ &ndomain, const ZZ &nrange, const ZZ &rgap, PRNG *prng)
{
    return HGD(rgap, ndomain, nrange-ndomain, prng);
}

template<class CB>
ope_domain_range
OPE::lazy_sample(const ZZ &d_lo, const ZZ &d_hi,
                 const ZZ &r_lo, const ZZ &r_hi,
                 CB go_low, blockrng<AES> *prng)
{
    ZZ ndomain = d_hi - d_lo + 1;
    ZZ nrange  = r_hi - r_lo + 1;
    throw_c(nrange >= ndomain);

    if (ndomain == 1)
        return ope_domain_range(d_lo, r_lo, r_hi);

    /*
     * Deterministically reset the PRNG counter, regardless of
     * whether we had to use it for HGD or not in previous round.
     */
    auto v = hmac<sha256>::mac(StringFromZZ(d_lo) + "/" +
                               StringFromZZ(d_hi) + "/" +
                               StringFromZZ(r_lo) + "/" +
                               StringFromZZ(r_hi), key);
    v.resize(AES::blocksize);
    prng->set_ctr(v);

    ZZ rgap = nrange/2;
    if (nrange > ndomain * 2) {
        /*
         * XXX for high bits, we are fighting against the law of large
         * numbers, because dgap (the number of marked balls out of a
         * large rgap sample) will be very near to the well-known
         * proportion of marked balls (i.e., ndomain/nrange).
         *
         * In other words, for two plaintexts p_0 and p_1, the variance of
         * the difference between corresponding ciphertexts c_0 and c_1 is
         * not high.
         *
         * Perhaps we need to add some high-variance randomness at each
         * level where we divide the ciphertext range.
         *
         * This could fit nicely with the window-one-wayness notion of
         * OPE security from Boldyerva's crypto 2011 paper.
         *
         * The current hack randomly moves the range gap up/down within
         * the middle part of the range.  Need to more formally argue
         * for what the resulting variance is, and why it's higher than
         * with HGD.
         *
         * At this rate, if we aren't strictly following HGD, it might
         * make sense to ditch the expensive HGD computation altogether?
         */

        rgap = nrange/4 + prng->rand_zz_mod(nrange/2);
    }

    ZZ dgap;

    auto ci = dgap_cache.find(r_lo + rgap);
    if (ci == dgap_cache.end()) {
        dgap = domain_gap(ndomain, nrange, nrange / 2, prng);
        dgap_cache[r_lo + rgap] = dgap;
    } else {
        dgap = ci->second;
    }

    if (go_low(d_lo + dgap, r_lo + rgap))
        return lazy_sample(d_lo, d_lo + dgap - 1, r_lo, r_lo + rgap - 1, go_low, prng);
    else
        return lazy_sample(d_lo + dgap, d_hi, r_lo + rgap, r_hi, go_low, prng);
}

template<class CB>
ope_domain_range
OPE::search(CB go_low)
{
    blockrng<AES> r(aesk);

    return lazy_sample(to_ZZ(0), to_ZZ(1) << pbits,
                       to_ZZ(0), to_ZZ(1) << cbits,
                       go_low, &r);
}

ZZ
OPE::encrypt(const ZZ &ptext, int offset)
{
    ope_domain_range dr =
        search([&ptext](const ZZ &d, const ZZ &) { return ptext < d; });

    blockrng<AES> aesrand(aesk);
    auto v = sha256::hash(StringFromZZ(ptext));
    v.resize(16);
    aesrand.set_ctr(v);

    ZZ nrange = dr.r_hi - dr.r_lo + 1;
    if (nrange < 4 || det)
        return dr.r_lo + aesrand.rand_zz_mod(nrange);

    ZZ nrquad = nrange / 4;
    static urandom urand;

    switch (offset) {
    case -1:
        return dr.r_lo + urand.rand_zz_mod(nrquad);
    case 0:
        return dr.r_lo + nrquad + urand.rand_zz_mod(nrquad * 2);
    case 1:
        return dr.r_lo + nrquad * 3 + urand.rand_zz_mod(nrquad);
    default:
        throw_c(0);
    }
}

ZZ
OPE::decrypt(const ZZ &ctext)
{
    ope_domain_range dr =
        search([&ctext](const ZZ &, const ZZ &r) { return ctext < r; });
    return dr.d;
}
