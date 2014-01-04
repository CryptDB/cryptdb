#include <NTL/ZZ.h>
#include <crypto/prng.hh>

/*
 * KK is the number of elements drawn from an urn where there are NN1 white
 * balls and NN2 black balls; the result is the number of white balls in
 * the KK sample.
 *
 * The implementation is based on an adaptation of the H2PEC alg for large
 * numbers; see hgd.cc for details
 */
NTL::ZZ HGD(const NTL::ZZ &KK,
            const NTL::ZZ &NN1,
            const NTL::ZZ &NN2,
            PRNG *prng);
