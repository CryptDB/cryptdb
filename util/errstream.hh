#pragma once

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

class CryptDBError {
public:
    CryptDBError(const std::string &m) : msg(m) {}
    std::string msg;
};

class CryptoError : public CryptDBError {
public:
    CryptoError(const std::string &m) : CryptDBError(m) {}
};

inline void
throw_c(bool test, const std::string &msg = "crypto fail")
{
    if (false == test) {
        throw CryptoError(msg);
    }

    return;
}
class fatal : public std::stringstream {
 public:
    ~fatal() __attribute__((noreturn)) {
        std::cerr << str() << std::endl;
        exit(-1);
    }
};

class cryptdb_err : public std::stringstream {
 public:
    ~cryptdb_err() __attribute__((noreturn)) {
        std::cerr << str() << std::endl;
        throw CryptDBError(str());
    }
};

class thrower : public std::stringstream {
 public:
    ~thrower() __attribute__((noreturn)) {
        throw std::runtime_error(str());
    }
};

