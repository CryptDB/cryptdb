#include <crypto/arc4.hh>

using namespace std;

arc4::arc4(const std::string &key)
{
    reset();
    for (size_t n = 0; n < key.size(); n += 128)
        addkey((const uint8_t *) &key[n], min((size_t) 128, key.size() - n));
    j = i;

    /* discard first bytes */
    for (int n = 0; n < 4096; n++)
        getbyte();
}

void
arc4::reset()
{
    i = 0xff;
    j = 0;
    for (int n = 0; n < 0x100; n++)
        s[n] = n;
}

void
arc4::addkey(const uint8_t *key, size_t keylen)
{
    size_t n, keypos;
    uint8_t si;
    for (n = 0, keypos = 0; n < 256; n++, keypos++) {
        if (keypos >= keylen)
            keypos = 0;
        i = (i + 1) & 0xff;
        si = s[i];
        j = (j + si + key[keypos]) & 0xff;
        s[i] = s[j];
        s[j] = si;
    }
}

uint8_t
arc4::getbyte()
{
    uint8_t si, sj;
    i = (i + 1) & 0xff;
    si = s[i];
    j = (j + si) & 0xff;
    sj = s[j];
    s[i] = sj;
    s[j] = si;
    return s[(si + sj) & 0xff];
}
