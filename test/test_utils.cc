/*
 * test_utils.cc
 * -- useful functions for testing
 *
 *
 */

#include <test/test_utils.hh>

void
PrintRes(const ResType &res)
{
    for (auto i = res.names.begin(); i != res.names.end(); i++)
        std::cerr << *i << " | ";
    std::cerr << std::endl;
    for (auto outer = res.rows.begin(); outer != res.rows.end(); outer++) {
        for (auto inner = outer->begin(); inner != outer->end(); inner++)
            std::cerr << *inner << " | ";
        std::cerr << std::endl;
    }
    std::cerr << std::endl;
}

/*ResType
myExecute(EDBProxy * cl, string query)
{
    if (PLAIN)
        return cl->plain_execute(query);
    else
        return cl->execute(query);
        }*/

 /*ResType
myCreate(EDBProxy *cl, string annotated_query, string plain_query)
{
    if (PLAIN)
        return cl->plain_execute(plain_query);
    else
        return cl->execute(annotated_query);
}
 */

