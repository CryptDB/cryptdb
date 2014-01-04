#include <algorithm>
#include <crypto/search.hh>
#include <crypto/sha.hh>
#include <crypto/hmac.hh>
#include <crypto/prng.hh>

using namespace std;

static string
xor_pad(const string &word_key, size_t csize)
{
    auto v = sha256::hash(word_key);
    throw_c(v.size() >= csize);
    v.resize(csize);
    return v;
}

bool
search::match(const string &ctext,
              const string &word_key)
{
    throw_c(ctext.size() == csize);
    string cx;

    auto xorpad = xor_pad(word_key, csize);
    for (size_t i = 0; i < csize; i++)
        cx.push_back(ctext[i] ^ xorpad[i]);

    string salt = cx;
    salt.resize(csize/2);

    string cf(cx.begin() + csize/2, cx.end());
    auto f = hmac<sha1>::mac(salt, word_key);
    f.resize((csize + 1) / 2);

    return (f == cf);
}

bool
search::match(const vector<string> &ctexts, const string &word_key)
{
    for (auto &c: ctexts)
        if (match(c, word_key))
            return true;
    return false;
}

string
search_priv::transform(const string &word)
{
    auto word_key = wordkey(word);

    urandom r;
    auto salt = r.rand_string(csize / 2);

    auto f = hmac<sha1>::mac(salt, word_key);
    f.resize((csize + 1) / 2);

    string x;
    x.insert(x.end(), salt.begin(), salt.end());
    x.insert(x.end(), f.begin(), f.end());

    auto xorpad = xor_pad(word_key, csize);
    for (size_t i = 0; i < csize; i++)
        x[i] ^= xorpad[i];

    return x;
}

vector<string>
search_priv::transform(const vector<string> &words)
{
    vector<string> res;
    for (auto &w: words)
        res.push_back(transform(w));
    sort(res.begin(), res.end());
    return res;
}

string
search_priv::wordkey(const string &word)
{
    return hmac<sha1>::mac(word, master_key);
}
