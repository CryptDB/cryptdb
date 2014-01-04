#pragma once

#include <vector>
#include <string>

class search {
 public:
    static const size_t defsize = 16;
    search(size_t csize_arg = defsize) : csize(csize_arg) {}

    bool match(const std::vector<std::string> &ctext,
               const std::string &wordkey);

 protected:
    bool match(const std::string &ctext,
               const std::string &wordkey);
    size_t csize;
};

class search_priv : public search {
 public:
    search_priv(const std::string &key, size_t csize_arg = defsize)
        : search(csize_arg), master_key(key) {}

    std::vector<std::string>
        transform(const std::vector<std::string> &words);
    std::string
        wordkey(const std::string &word);

 private:
    std::string
        transform(const std::string &word);
    std::string master_key;
};
