#pragma once

#include <vector>
#include <crypto/arc4.hh>

template<typename T>
class gfe {
 public:
    gfe(size_t q)
        : k_(sizeof(T) * 8), q_(q),
          kq_(kq(k_, q_)),
          e0_(e0(kq_, k_)),
          e1_(e1(kq_, k_)) {}

    static std::vector<std::pair<int, T>>
    cover_prefixes(T v)
    {
        std::vector<std::pair<int, T>> r;
        for (uint i = 0; i < sizeof(T) * 8; i++) {
            r.push_back(make_pair(i, v >> i));
        }
        return r;
    }

    static std::vector<std::pair<int, T>>
    right_prefixes(T v)
    {
        std::vector<std::pair<int, T>> r;
        for (uint i = 0; i < sizeof(T) * 8; i++) {
            if (v & (((T)1) << i)) {
                r.push_back(make_pair(-1, 0));
            } else {
                r.push_back(make_pair(i, (v >> i) | 1));
            }
        }
        return r;
    }

    static uint64_t
    dotproduct(const std::vector<uint8_t> &v1, const std::vector<uint8_t> &v2)
    {
        uint64_t count = 0;
        throw_c(v1.size() == v2.size());
        for (size_t i = 0; i < v1.size(); i++)
            count += __builtin_popcount(v1[i] & v2[i]);
        return count;
    }

    const size_t k_;
    const size_t q_;
    const size_t kq_;   // k**q

    const size_t e0_;   // expected dot product for no match
    const size_t e1_;   // expected dot product for one match

 private:
    static size_t kq(size_t k, size_t q) {
        size_t v = 1;
        for (uint i = 0; i < q; i++)
            v *= k;
        return v;
    }

    static size_t e0(size_t kq, size_t k) {
        return kq * k / 4;
    }

    static size_t e1(size_t kq, size_t k) {
        return kq * k / 4 + kq / 4;
    }
};

template<typename T>
class gfe_priv : public gfe<T> {
 public:
    gfe_priv(const std::string &key, size_t q)
        : gfe<T>(q), key_(key), urand_(), prand_(urand_.rand_string(32)) {}

    typedef std::vector<uint8_t> bitvec;

    std::vector<uint8_t>
    prf(const std::pair<int, T> &p)
    {
        if (p.first == -1) {
            /* Hole in the prefix list, generate a random value */
            return prand_.rand_vec<uint8_t>(gfe<T>::kq_ / 8);
        } else {
            hmac<sha256> hm(key_.data(), key_.size());
            hm.update(&p.first,  sizeof(p.first));
            hm.update(&p.second, sizeof(p.second));
            std::string phash = hm.final();

            blockrng<AES> r(phash);
            return r.rand_vec<uint8_t>(gfe<T>::kq_ / 8);
        }
    }

    std::vector<uint8_t>
    prfvec(const std::vector<std::pair<int, T>> &pv)
    {
        std::vector<uint8_t> rv(gfe<T>::kq_ * gfe<T>::k_ / 8);
        throw_c(pv.size() == gfe<T>::k_);
        for (uint i = 0; i < gfe<T>::k_; i++) {
            auto x = prf(pv[i]);
            memcpy(&rv[i * gfe<T>::kq_ / 8], &x[0], gfe<T>::kq_ / 8);
        }
        return rv;
    }

 private:
    std::string key_;
    urandom urand_;
    streamrng<arc4> prand_;
};
