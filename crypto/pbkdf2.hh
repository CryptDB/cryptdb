#pragma once

#include <string>

std::string pbkdf2(const std::string &pass, const std::string &salt,
                   uint key_len, uint rounds);

