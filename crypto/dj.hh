/*
 * Damgard-Jurik (a generalization of Paillier) provides a ciphertext
 * size that is asymptotically close to the plaintext size, rather
 * than a factor of 2 larger in standard Paillier.
 *
 * This may be especially useful in applications where ciphertext space
 * is the bottleneck, where in combination with packing, this should
 * provide almost no ciphertext storage overheads (except for spacing
 * for aggregate overflow).
 *
 * "A Generalisation, a Simplification and some Applications of
 * Paillier's Probabilistic Public-Key System", by Damgard and Jurik.
 * <http://www.daimi.au.dk/~ivan/papers/GeneralPaillier.ps>
 */

