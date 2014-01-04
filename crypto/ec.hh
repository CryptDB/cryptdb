#pragma once

#include <ostream>
#include <openssl/ec.h>
#include <openssl/bn.h>

class ec_point {
 public:
    ec_point(const EC_GROUP *group) {
        gr = group;
        pt = EC_POINT_new(gr);
    }

    ec_point(const ec_point &other) {
        gr = other.gr;
        pt = EC_POINT_dup(other.pt, gr);
    }

    ~ec_point() {
        EC_POINT_free(pt);
    }

    ec_point operator*(const bignum &n) const {
        bignum zero(0);
        ec_point res(gr);
        throw_c(EC_POINT_mul(gr, res.p(), zero.bn(),
                             pt, n.bn(), _bignum_ctx::the_ctx()));
        return res;
    }

    bool operator==(const ec_point &other) const {
        return EC_POINT_cmp(gr, pt, other.pt, _bignum_ctx::the_ctx()) == 0;
    }

    bool operator!=(const ec_point &other) const {
        return EC_POINT_cmp(gr, pt, other.pt, _bignum_ctx::the_ctx()) != 0;
    }

    std::string to_string(point_conversion_form_t form =
                          POINT_CONVERSION_UNCOMPRESSED) const {
        char *s = EC_POINT_point2hex(gr, pt, form, _bignum_ctx::the_ctx());
        std::string r(s);
        free(s);
        return r;
    }

    EC_POINT *p() { return pt; }

 private:
    EC_POINT *pt;
    const EC_GROUP *gr;
};

static inline std::ostream&
operator<<(std::ostream &out, const ec_point &p)
{
    return out << p.to_string();
}
