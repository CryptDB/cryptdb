#include <string>
#include <vector>
#include <cstdlib>

inline std::string
serialize_string(std::string str)
{
    return std::string(std::to_string(str.length()) + "_" + str);
}

// TESTME.
// Must perserve order.
inline std::vector<std::string>
unserialize_string(std::string serial)
{
    std::vector<std::string> output;
    std::size_t start = 0;
    std::size_t under_pos = serial.find_first_of("_");
    while (under_pos != std::string::npos) {
        std::size_t length =
            atoi(serial.substr(start, under_pos-start).c_str());
        output.push_back(serial.substr(under_pos+1, length)); 
        start = under_pos + 1 + length;
        under_pos = serial.find_first_of("_", start);
    }

    // TODO: Sanity check no leftover characters.

    return output;
}

inline std::string
unserialize_one_string(std::string serial)
{
    auto vec = unserialize_string(serial);
    return vec[0];
}

inline unsigned int
serial_to_uint(std::string serial)
{
    return atoi(unserialize_one_string(serial).c_str());
}


