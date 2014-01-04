#include <string>
#include <NTL/ZZ.h>
#include <sstream>

inline std::string
StringFromZZ(const NTL::ZZ &x)
{
    std::string s;
    s.resize(NumBytes(x), 0);
    NTL::BytesFromZZ((uint8_t*) &s[0], x, s.length());
    return s;
}

// returns ZZ from a string representing the ZZ in base 256
inline NTL::ZZ
ZZFromString(const std::string &s)
{
    return NTL::ZZFromBytes((const uint8_t *) s.data(), s.length());
}


// converts ZZ to and from a string representing the number in decimal

inline std::string
DecStringFromZZ(const NTL::ZZ & x) {
    std::stringstream ss;
    ss << x;
    return ss.str();
}

inline NTL::ZZ
ZZFromDecString(const std::string &s) {
    NTL::ZZ v;
    std::stringstream ss(s);
    ss >> v;
    return v;
}
