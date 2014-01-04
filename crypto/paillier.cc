#include <crypto/paillier.hh>
#include <sstream>

using namespace std;
using namespace NTL;

/*
 * Public-key operations
 */

Paillier::Paillier() : nbits(0) {
}

Paillier::Paillier(const vector<ZZ> &pk)
    : n(pk[0]), g(pk[1]),
      nbits(NumBits(n)), n2(n*n)
{
    throw_c(pk.size() == 2);
}

void
Paillier::rand_gen(size_t niter, size_t nmax)
{
    if (rqueue.size() >= nmax)
        niter = 0;
    else
        niter = min(niter, nmax - rqueue.size());

    for (uint i = 0; i < niter; i++) {
        ZZ r = RandomLen_ZZ(nbits) % n;
        ZZ rn = PowerMod(g, n*r, n2);
        rqueue.push_back(rn);
    }
}

ZZ
Paillier::encrypt(const ZZ &plaintext)
{
    auto i = rqueue.begin();
    if (i != rqueue.end()) {
        ZZ rn = *i;
        rqueue.pop_front();

        return (PowerMod(g, plaintext, n2) * rn) % n2;
    } else {
        ZZ r = RandomLen_ZZ(nbits) % n;
        return PowerMod(g, plaintext + n*r, n2);
    }
}

ZZ
Paillier::add(const ZZ &c0, const ZZ &c1) const
{
    return MulMod(c0, c1, n2);
}

ZZ
Paillier::mul(const ZZ &ciphertext, const ZZ &constval) const
{
    return PowerMod(ciphertext, constval, n2);
}


/*
 * Private-key operations
 */

static inline ZZ
L(const ZZ &u, const ZZ &n)
{
    return (u - 1) / n;
}

static inline ZZ
Lfast(const ZZ &u, const ZZ &ninv, const ZZ &two_n, const ZZ &n)
{
    return (((u - 1) * ninv) % two_n) % n;
}

static inline ZZ
LCM(const ZZ &a, const ZZ &b)
{
    return (a * b) / GCD(a, b);
}

Paillier_priv::Paillier_priv(const vector<ZZ> &sk)
    : Paillier({sk[0]*sk[1], sk[2]}), p(sk[0]), q(sk[1]), a(sk[3]),
      fast(a != 0),
      p2(p * p), q2(q * q),
      two_p(power(to_ZZ(2), NumBits(p))),
      two_q(power(to_ZZ(2), NumBits(q))),
      pinv(InvMod(p, two_p)),
      qinv(InvMod(q, two_q)),
      hp(InvMod(Lfast(PowerMod(g % p2, fast ? a : (p-1), p2),
                      pinv, two_p, p), p)),
      hq(InvMod(Lfast(PowerMod(g % q2, fast ? a : (q-1), q2),
                      qinv, two_q, q), q))
{
    throw_c(sk.size() == 4);
}

std::vector<NTL::ZZ>
Paillier_priv::keygen(PRNG *rng, uint nbits, uint abits)
{
    ZZ p, q, n, g, a;

    do {
        if (abits) {
            a = rng->rand_zz_prime(abits);

            ZZ cp = rng->rand_zz_nbits(nbits/2-abits);
            ZZ cq = rng->rand_zz_nbits(nbits/2-abits);

            p = a * cp + 1;
            while (!ProbPrime(p))
                p += a;

            q = a * cq + 1;
            while (!ProbPrime(q))
                q += a;
        } else {
            a = 0;
            p = rng->rand_zz_prime(nbits/2);
            q = rng->rand_zz_prime(nbits/2);
        }
        n = p * q;
    } while ((nbits != (uint) NumBits(n)) || p == q);

    if (p > q)
        swap(p, q);

    ZZ lambda = LCM(p-1, q-1);

    if (abits) {
        g = PowerMod(to_ZZ(2), lambda / a, n);
    } else {
        g = 1;
        do {
            g++;
        } while (GCD(L(PowerMod(g, lambda, n*n), n), n) != to_ZZ(1));
    }

    return { p, q, g, a };
}

ZZ
Paillier_priv::decrypt(const ZZ &ciphertext) const
{
    ZZ mp = (Lfast(PowerMod(ciphertext % p2, fast ? a : (p-1), p2),
                   pinv, two_p, p) * hp) % p;
    ZZ mq = (Lfast(PowerMod(ciphertext % q2, fast ? a : (q-1), q2),
                   qinv, two_q, q) * hq) % q;

    ZZ m, pq;
    pq = 1;
    CRT(m, pq, mp, p);
    CRT(m, pq, mq, q);

    return m;
}
