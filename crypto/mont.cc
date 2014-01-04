#include <algorithm>
#include <crypto/mont.hh>

#include <gmp.h>

using namespace std;
using namespace NTL;

ZZ
montgomery::to_mont(const ZZ &a)
{
    return MulMod(a, _r, _m);
}

ZZ
montgomery::from_mont(const ZZ &a)
{
    return MulMod(a, _rinv, _m);
}

#define SIZE(p) (((long *) (p))[1])
#define DATA(p) ((mp_limb_t *) (((long *) (p)) + 2))

ZZ
montgomery::mmul(const ZZ &a, const ZZ &b)
{
    static_assert(sizeof(mp_limb_t) == sizeof(long), "mp_limb_t not long");
    ZZ ab = a * b;
    if (ab == 0) return ab; // to avoid testing in loop

    for (uint i = 0; i < _mbits; i += sizeof(long) * 8) {
        uint thisbits = std::min((uint) sizeof(long) * 8, _mbits - i);
        mp_limb_t* abdata = DATA(ab.rep);
        long l = abdata[0];
        long c = _minusm_inv_modr * l;
        if (thisbits < sizeof(long) * 8)
            c &= (((long)1) << thisbits) - 1;
        ab += _m * c;

        // throw_c(trunc_long(ab, thisbits) == 0);
        ab >>= thisbits;
    }

    while (ab >= _m)
        ab = ab - _m;
    return ab;
}
