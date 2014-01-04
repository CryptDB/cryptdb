#include <crypto/prng.hh>

using namespace NTL;

ZZ
PRNG::rand_zz_nbits(size_t nbits)
{
    if (nbits == 0)
        return to_ZZ(0);

    uint8_t buf[(nbits + 7) / 8];
    rand_bytes(sizeof(buf), buf);

    ZZ r = ZZFromBytes(buf, sizeof(buf));
    SetBit(r, nbits - 1);
    return r;
}

ZZ
PRNG::rand_zz_prime(size_t nbits)
{
    for (;;) {
        ZZ r = rand_zz_nbits(nbits);
        SetBit(r, 0);

        // XXX assume ProbPrime is perfect
        if (ProbPrime(r, 10))
            return r;
    }
}
