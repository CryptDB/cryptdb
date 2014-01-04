#pragma once

static inline void
pad_blocksize(std::string *ptext, size_t blocksize)
{
    size_t plen = ptext->size();
    uint8_t pad = blocksize - (plen % blocksize);
    ptext->resize(plen + pad);
    (*ptext)[plen + pad - 1] = pad;
}

static inline void
unpad_blocksize(std::string *ptext, size_t blocksize)
{
    size_t flen = ptext->size();
    uint8_t pad = (*ptext)[flen - 1];
    ptext->resize(flen - pad);
}
