#pragma once

#include <vector>
#include <stdint.h>
#include <string.h>

template<class BlockCipher>
void
xor_block(void *dstv, const void *av, const void *bv)
{
    uint8_t *dst = (uint8_t*) dstv;
    const uint8_t *a = (const uint8_t*) av;
    const uint8_t *b = (const uint8_t*) bv;

    for (size_t j = 0; j < BlockCipher::blocksize; j++)
        dst[j] = a[j] ^ b[j];
}

template<class BlockCipher>
void
cbc_encrypt(const BlockCipher *c,
            const std::string &ivec,
            const std::string &ptext,
            std::string *ctext)
{
    throw_c(ivec.size() == BlockCipher::blocksize);
    size_t ptsize = ptext.size();
    throw_c(ptsize >= BlockCipher::blocksize);
    ctext->resize(ptsize);

    const uint8_t *iv = (const uint8_t*) ivec.data();
    const uint8_t *pt = (const uint8_t*) ptext.data();
    uint8_t *ct = (uint8_t*) ctext->data();

    for (size_t i = 0;
         i <= ptsize - BlockCipher::blocksize;
         i += BlockCipher::blocksize)
    {
        uint8_t x[BlockCipher::blocksize];
        const uint8_t *y = (i == 0) ? &iv[0] : &ct[i-BlockCipher::blocksize];
        xor_block<BlockCipher>(x, &pt[i], y);
        c->block_encrypt(x, &ct[i]);
    }

    // Ciphertext stealing (CTS)
    size_t leftover = ptsize % BlockCipher::blocksize;
    if (leftover) {
        uint8_t x[BlockCipher::blocksize];
        const uint8_t *y = &ct[ptsize-leftover-BlockCipher::blocksize];
        uint8_t z[BlockCipher::blocksize];

        memset(z, 0, BlockCipher::blocksize);
        memcpy(z, &pt[ptsize-leftover], leftover);
        xor_block<BlockCipher>(x, z, y);

        memcpy(&ct[ptsize-leftover], &ct[ptsize-leftover-BlockCipher::blocksize], leftover);
        c->block_encrypt(x, &ct[ptsize-leftover-BlockCipher::blocksize]);
    }
}

template<class BlockCipher>
void
cbc_decrypt(const BlockCipher *c,
            const std::string &ivec,
            const std::string &ctext,
            std::string *ptext)
{
    throw_c(ivec.size() == BlockCipher::blocksize);
    size_t ctsize = ctext.size();
    throw_c(ctsize >= BlockCipher::blocksize);
    ptext->resize(ctsize);

    const uint8_t *iv = (const uint8_t*) ivec.data();
    const uint8_t *ct = (const uint8_t*) ctext.data();
    uint8_t *pt = (uint8_t*) ptext->data();

    for (size_t i = 0;
         i <= ctsize - BlockCipher::blocksize;
         i += BlockCipher::blocksize)
    {
        uint8_t x[BlockCipher::blocksize];
        c->block_decrypt(&ct[i], x);
        const uint8_t *y = (i == 0) ? &iv[0] : &ct[i-BlockCipher::blocksize];
        xor_block<BlockCipher>(&pt[i], x, y);
    }

    // Ciphertext stealing (CTS)
    size_t leftover = ctsize % BlockCipher::blocksize;
    if (leftover) {
        uint8_t xlast[BlockCipher::blocksize];
        c->block_decrypt(&ct[ctsize-leftover-BlockCipher::blocksize], xlast);

        uint8_t ctprev[BlockCipher::blocksize];
        memcpy(&ctprev[0], &ct[ctsize-leftover], leftover);
        memcpy(&ctprev[leftover], &xlast[leftover], BlockCipher::blocksize-leftover);

        uint8_t ptlast[BlockCipher::blocksize];
        xor_block<BlockCipher>(ptlast, xlast, ctprev);
        memcpy(&pt[ctsize-leftover], ptlast, leftover);

        uint8_t xprev[BlockCipher::blocksize];
        c->block_decrypt(ctprev, xprev);
        const uint8_t *y = (ctsize < 2*BlockCipher::blocksize) ?
                           &iv[0] :
                           &ct[ctsize-leftover-2*BlockCipher::blocksize];
        xor_block<BlockCipher>(&pt[ctsize-leftover-BlockCipher::blocksize],
                               xprev, y);
    }
}
