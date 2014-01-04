#include <crypto/ffx.hh>

void ffx_mem_to_u64(const uint8_t *p,
                    uint64_t *a, uint64_t *b,
                    uint abits, uint bbits)
{
    throw_c(abits <= 64 && bbits <= 64);

    *a = 0;
    *b = 0;

    while (abits >= 8) {
        *a = *a << 8 | *p;
        p++;
        abits -= 8;
    }

    if (abits) {
        *a = *a << abits | *p >> (8 - abits);
        uint8_t pleft = *p & ((1 << (8 - abits)) - 1);
        if (bbits < 8 - abits) {
            *b = pleft >> (8 - bbits);
            bbits = 0;
        } else {
            *b = pleft;
            bbits -= 8 - abits;
        }
        p++;
    }

    while (bbits >= 8) {
        *b = *b << 8 | *p;
        p++;
        bbits -= 8;
    }

    if (bbits)
        *b = *b << bbits | *p >> (8 - bbits);
}

void ffx_u64_to_mem(uint64_t a, uint64_t b,
                    uint64_t abits, uint64_t bbits,
                    uint8_t *p)
{
    throw_c(abits <= 64 && bbits <= 64);

    while (abits >= 8) {
        *p = a >> (abits - 8);
        p++;
        abits -= 8;
    }

    if (abits) {
        *p = a & ((1 << abits) - 1);
        if (bbits < 8 - abits) {
            *p = (*p << bbits | (b & ((1 << bbits) - 1))) << (8 - abits - bbits);
            bbits = 0;
        } else {
            *p = *p << (8 - abits) | b >> (bbits - (8 - abits));
            bbits -= (8 - abits);
        }
        p++;
    }

    while (bbits >= 8) {
        *p = b >> (bbits - 8);
        p++;
        bbits -= 8;
    }

    if (bbits)
        *p = b << (8 - bbits);
}
