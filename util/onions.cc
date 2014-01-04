#include "util/onions.hh"
#include "util/util.hh"

/*
std::ostream&
operator<<(std::ostream &out, const EncDesc & ed)
{
    if (ed.olm.size() == 0) {
        out << "empty encdesc";
    }
    for (auto it : ed.olm) {
        out << "(onion " << it.first
            << ", level " << levelnames[(int)it.second]
            << ") ";
    }
    return out;
}
*/

