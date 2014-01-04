#pragma once

#include <list>
#include <vector>
#include <NTL/ZZ.h>
#include <crypto/prng.hh>

#define PAILLIER_LEN_BYTES 256
const unsigned int Paillier_len_bytes = PAILLIER_LEN_BYTES;
const unsigned int Paillier_len_bits = Paillier_len_bytes * 8;


class Paillier {
 public:
    Paillier(); //HACK: we should not need this
    Paillier(const std::vector<NTL::ZZ> &pk);
    std::vector<NTL::ZZ> pubkey() const { return { n, g }; }
    NTL::ZZ hompubkey() const { return n2; }

    NTL::ZZ encrypt(const NTL::ZZ &plaintext);
    NTL::ZZ add(const NTL::ZZ &c0, const NTL::ZZ &c1) const;
    NTL::ZZ mul(const NTL::ZZ &ciphertext, const NTL::ZZ &constval) const;

    void rand_gen(size_t niter = 100, size_t nmax = 1000);

    /*
     * For packing, choose a PackT such that addition will never overflow.
     * E.g., for 32-bit values, using uint64_t as PackT may be a good idea.
     */
    template<class PackT>
    uint32_t pack_count() {
        return (nbits - 1 + sizeof(PackT)*8) / 2 / (sizeof(PackT)*8);
    }

    template<class PackT>
    NTL::ZZ encrypt_pack(const std::vector<PackT> &items) {
        uint32_t npack = pack_count<PackT>();
        throw_c(items.size() == npack);

        NTL::ZZ sum = NTL::to_ZZ(0);
        for (uint i = 0; i < npack; i++)
            sum += NTL::to_ZZ(items[i]) << (i*sizeof(PackT)*8);
        return encrypt(sum);
    }

    template<class PackT>
    NTL::ZZ add_pack(const NTL::ZZ &agg, const NTL::ZZ &pack, uint32_t packidx) {
        uint32_t npack = pack_count<PackT>();
        throw_c(packidx < npack);

        NTL::ZZ s = mul(pack, NTL::to_ZZ(1) << (npack-1-packidx) * (sizeof(PackT)*8));
        return add(agg, s);
    }

    /*
     * A different packing scheme that achieves 2x the density, at the
     * cost of a larger aggregate value.
     */
    template<class PackT>
    class pack2_agg {
     public:
        std::vector<NTL::ZZ> aggs;

        pack2_agg(Paillier *parg)
            : aggs(parg->pack2_count<PackT>()), p(parg)
        {
            for (uint32_t i = 0; i < aggs.size(); i++)
                aggs[i] = NTL::to_ZZ(1);
        }

        void add(const NTL::ZZ &pack, uint32_t packidx) {
            throw_c(packidx <= aggs.size());
            aggs[packidx] = p->add(aggs[packidx], pack);
        }

     private:
        Paillier *p;
    };

    template<class PackT>
    uint32_t pack2_count() {
        return (nbits - 1) / (sizeof(PackT) * 8);
    }

    template<class PackT>
    NTL::ZZ encrypt_pack2(const std::vector<PackT> &items) {
        uint32_t npack = pack2_count<PackT>();
        throw_c(items.size() == npack);

        NTL::ZZ sum = NTL::to_ZZ(0);
        for (uint i = 0; i < npack; i++)
            sum += NTL::to_ZZ(items[i]) << (i*sizeof(PackT)*8);
        return encrypt(sum);
    }

 protected:
    /* Public key */
    const NTL::ZZ n, g;

    /* Cached values */
    const uint nbits;
    const NTL::ZZ n2;

    /* Pre-computed randomness */
    std::list<NTL::ZZ> rqueue;
};

class Paillier_priv : public Paillier {
 public:
    Paillier_priv() : fast(false) {} //HACK: should not need this
    Paillier_priv(const std::vector<NTL::ZZ> &sk);
    std::vector<NTL::ZZ> privkey() const { return { p, q, g, a }; }

    NTL::ZZ decrypt(const NTL::ZZ &ciphertext) const;

    static std::vector<NTL::ZZ> keygen(PRNG*, uint nbits = 1024, uint abits = 256);

    template<class PackT>
    PackT decrypt_pack(const NTL::ZZ &pack) {
        uint32_t npack = pack_count<PackT>();
        NTL::ZZ plain = decrypt(pack);
        PackT result;
        NTL::conv(result, plain >> (npack - 1) * sizeof(PackT) * 8);
        return result;
    }

    template<class PackT>
    PackT decrypt_pack2(const pack2_agg<PackT> &agg) {
        uint32_t npack = pack2_count<PackT>();
        PackT result = 0, tmp;
        for (uint32_t i = 0; i < npack; i++) {
            NTL::conv(tmp, decrypt(agg.aggs[i]) >> (i*sizeof(PackT)*8));
            result += tmp;
        }
        return result;
    }

 private:
    /* Private key, including g from public part; n=pq */
    const NTL::ZZ p, q;
    const NTL::ZZ a;      /* non-zero for fast mode */

    /* Cached values */
    const bool fast;
    const NTL::ZZ p2, q2;
    const NTL::ZZ two_p, two_q;
    const NTL::ZZ pinv, qinv;
    const NTL::ZZ hp, hq;
};
