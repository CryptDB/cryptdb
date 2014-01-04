#pragma once

#include <string>
#include <cstring>
#include <crypto/cbc.hh>


template<class BlockCipher>
void
cmc_encrypt(const BlockCipher *c,
            const std::string &ptext,
            std::string *ctext)
{
    throw_c(ptext.size() % BlockCipher::blocksize == 0);
    ctext->resize(ptext.size());
    uint8_t x[BlockCipher::blocksize];

    memset(x, 0, BlockCipher::blocksize);
    for (size_t i = 0; i < ptext.size(); i += BlockCipher::blocksize) {
        uint8_t y[BlockCipher::blocksize];
        xor_block<BlockCipher>(y, &ptext[i], x);
        c->block_encrypt(y, &(*ctext)[i]);
        memcpy(x, &(*ctext)[i], BlockCipher::blocksize);
    }

    uint8_t m[BlockCipher::blocksize];
    uint8_t carry = 0;
    for (size_t j = BlockCipher::blocksize; j != 0; j--) {
        uint16_t a = (*ctext)[j - 1] ^
                     (*ctext)[j - 1 + ptext.size() - BlockCipher::blocksize];
        m[j - 1] = carry | (uint8_t) (a << 1);
        carry = a >> 7;
    }
    m[BlockCipher::blocksize-1] |= carry;

    for (size_t i = 0; i < ptext.size(); i += BlockCipher::blocksize)
        for (size_t j = 0; j < BlockCipher::blocksize; j++)
            (*ctext)[i+j] ^= m[j];

    memset(x, 0, BlockCipher::blocksize);
    for (size_t i = ptext.size(); i != 0; i -= BlockCipher::blocksize) {
        uint8_t y[BlockCipher::blocksize];
        c->block_encrypt(&(*ctext)[i - BlockCipher::blocksize], y);

        uint8_t z[BlockCipher::blocksize];
        xor_block<BlockCipher>(z, y, x);

        memcpy(x, &(*ctext)[i - BlockCipher::blocksize], BlockCipher::blocksize);
        memcpy(&(*ctext)[i - BlockCipher::blocksize], z, BlockCipher::blocksize);
    }
}

template<class BlockCipher>
void
cmc_decrypt(const BlockCipher *c,
            const std::string &ctext,
            std::string *ptext)
{
    throw_c(ctext.size() % BlockCipher::blocksize == 0);
    ptext->resize(ctext.size());
    uint8_t x[BlockCipher::blocksize];

    memset(x, 0, BlockCipher::blocksize);
    for (size_t i = ctext.size(); i != 0; i -= BlockCipher::blocksize) {
        uint8_t y[BlockCipher::blocksize];
        xor_block<BlockCipher>(y, &ctext[i-BlockCipher::blocksize], x);
        c->block_decrypt(y, &(*ptext)[i - BlockCipher::blocksize]);
        memcpy(x, &(*ptext)[i - BlockCipher::blocksize], BlockCipher::blocksize);
    }

    uint8_t m[BlockCipher::blocksize];
    uint8_t carry = 0;
    for (size_t j = BlockCipher::blocksize; j != 0; j--) {
        uint16_t a = (*ptext)[j - 1] ^
                     (*ptext)[j - 1 + ctext.size() - BlockCipher::blocksize];
        m[j - 1] = carry | (uint8_t) (a << 1);
        carry = a >> 7;
    }

    m[BlockCipher::blocksize-1] |= carry;
    for (size_t i = 0; i < ctext.size(); i += BlockCipher::blocksize) {
        for (size_t j = 0; j < BlockCipher::blocksize; j++)
            (*ptext)[i+j] ^= m[j];
    }
    memset(x, 0, BlockCipher::blocksize);

    for (size_t i = 0; i < ctext.size(); i += BlockCipher::blocksize) {
        uint8_t y[BlockCipher::blocksize];
        c->block_decrypt(&(*ptext)[i], y);

        uint8_t z[BlockCipher::blocksize];
        xor_block<BlockCipher>(z, y, x);

        memcpy(x, &(*ptext)[i], BlockCipher::blocksize);
        memcpy(&(*ptext)[i], z, BlockCipher::blocksize);
    }
}
