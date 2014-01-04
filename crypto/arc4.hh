#pragma once

#include <string>

class arc4 {
 public:
    arc4(const std::string &key);
    uint8_t getbyte();

 private:
    arc4(const arc4 &);

    void reset();
    void addkey(const uint8_t *key, size_t keylen);

    uint8_t i;
    uint8_t j;
    uint8_t s[256];
};
