/*
 * Handlers for rewriting functions and operations.
 */

#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <list>
#include <algorithm>
#include <stdio.h>
#include <typeinfo>

#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <main/CryptoHandlers.hh>
#include <main/macro_util.hh>
#include <util/cryptdb_log.hh>
#include <util/enum_text.hh>
#include <parser/lex_util.hh>

// gives names to classes and objects we don't care to know the name of 
#define ANON                ANON_NAME(__anon_id_func_)

template <class T>
static Item *
do_optimize_type_self_and_args(T *i, Analysis &a) {
    UNIMPLEMENTED;
    /*
    Item *i0 = do_optimize_const_item(i, a);
    if (i0 == i) {
        // no optimizations done at top level
        // try children
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++) {
            optimize(&args[x], a);
        }
        return i;
    } else {
        return i0;
    }*/
}



template <class T>
static T *
rewrite_args_FN(const T &i, const OLK &constr,
                const RewritePlanOneOLK &rp, Analysis &a)
{
    const uint count = i.argument_count();
    T *const out_i = copyWithTHD(&i);
    List<Item> *const arg_list =
        dptrToListWithTHD(i.arguments(), count);
    out_i->set_arguments(*arg_list);

    Item **const args = out_i->arguments();
    for (uint x = 0; x < count; x++) {
        args[x] =
            itemTypes.do_rewrite(*args[x], rp.olk,
                                 *rp.childr_rp[x].get(), a);
        args[x]->name = NULL; // args should never have aliases...
    }

    return out_i;
}

// An implementation of gather for the common case operation
// Works for Item_func with two arguments, solution encset is intersect of
// children and my_es
// Only works with nodes with one outgoing encset, which could be other_encset
// if encset_from_intersection is false, or else is the intersection with the children
static RewritePlan *
typical_gather(Analysis &a, const Item_func &i, const EncSet &my_es,
               const std::string &why,
               bool encset_from_intersection,
               const EncSet &other_encset = PLAIN_EncSet)
{
    const unsigned int arg_count = i.argument_count();
    TEST_BadItemArgumentCount(i.type(), 2, arg_count);

    Item *const *const args = i.arguments();
    std::vector<std::shared_ptr<RewritePlan> >
        childr_rp({std::shared_ptr<RewritePlan>(gather(*args[0], a)),
                   std::shared_ptr<RewritePlan>(gather(*args[1], a))});

    const EncSet solution =
        my_es.intersect(childr_rp[0]->es_out).
              intersect(childr_rp[1]->es_out);
    TEST_NoAvailableEncSet(solution, i.type(), my_es, why,
                           childr_rp);

    std::function<EncSet ()> getEncSet =
        [encset_from_intersection, solution] ()
    {
        if (encset_from_intersection) {
            assert_s(solution.single_crypted_and_or_plainvals(),
                     "can't use typical_gather with more outgoing"
                        " encsets");
            return solution;
        } else {
            return PLAIN_EncSet;
        }
    };

    const EncSet out_es = getEncSet();
    const reason rsn(out_es, why, i);

    return new RewritePlanOneOLK(out_es,
                                 solution.chooseOne(), childr_rp,
                                 rsn);
}

static RewritePlan *
iterateGather(const Item_func &i, const EncSet &out_es, EncSet child_es,
              const std::string &why, Analysis &a)
{
    const reason rsn(out_es, why, i);

    const unsigned int arg_count = i.argument_count();
    std::vector<std::shared_ptr<RewritePlan> > childr_rp(arg_count);
    Item *const * const args = i.arguments();
    for (unsigned int index = 0; index < arg_count; ++index) {
        childr_rp[index] =
            std::shared_ptr<RewritePlan>(gather(*args[index],a));
        child_es = child_es.intersect(childr_rp[index]->es_out);
    }

    return new RewritePlanOneOLK(out_es, child_es.chooseOne(),
                                 childr_rp, rsn);
}

static RewritePlan *
allPlainIterateGather(const Item_func &i, const std::string &why,
                      Analysis &a)
{
    const EncSet out_es = PLAIN_EncSet;
    const EncSet child_es = PLAIN_EncSet;

    return iterateGather(i, out_es, child_es, why, a);
}

static class ANON : public CItemSubtypeFT<Item_func_neg, Item_func::Functype::NEG_FUNC> {
    virtual RewritePlan *
    do_gather_type(const Item_func_neg &i, Analysis &a) const
    {
        TEST_BadItemArgumentCount(i.type(), 1, i.argument_count());

        auto arg = i.arguments()[0];
        if (arg->type() == Item::Type::FIELD_ITEM) {
            const EncSet out_es = PLAIN_EncSet;
            const EncSet child_es = PLAIN_EncSet;
            const std::string why = "neg";

            return iterateGather(i, out_es, child_es, why, a);
        } else if (arg->type() == Item::Type::INT_ITEM
                   || arg->type() == Item::Type::FIELD_ITEM) {
            const EncSet out_es = FULL_EncSet_Int;
            const EncSet child_es = FULL_EncSet_Int;
            const std::string why = "neg";

            return iterateGather(i, out_es, child_es, why, a);
        } else {
            FAIL_TextMessageError("Unsupported NEG!");
        }
    }

    virtual Item * do_optimize_type(Item_func_neg *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }

    virtual void
    do_rewrite_insert_type(const Item_func_neg &i, const FieldMeta &fm,
                           Analysis &a, std::vector<Item *> *l) const
    {
        return typical_rewrite_insert_type(i, fm, a, l);
    }

    virtual Item *
    do_rewrite_type(const Item_func_neg &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        const RewritePlanOneOLK &rp_one =
            static_cast<const RewritePlanOneOLK &>(rp);

        if (oDET == constr.o) {
            const Item *const arg = i.arguments()[0];
            TEST_TextMessageError(arg->type() == Item::Type::INT_ITEM,
                                  "Must use Integer type with NEG!");
            const Item_int *const int_arg =
                static_cast<const Item_int * >(arg);

            // Negate the old value, so that it can be encrypted.
            const Item *const neg_i = new Item_int(-int_arg->value);

            return itemTypes.do_rewrite(*neg_i, constr,
                                        *rp_one.childr_rp[0].get(), a);
        } else if (oPLAIN == constr.o) {
            return rewrite_args_FN(i, constr, rp_one, a);
        } else {
            FAIL_TextMessageError("Bad onion for NEG!");
        }
    }
} ANON;

static class ANON : public CItemSubtypeFT<Item_func_not,
                                          Item_func::Functype::NOT_FUNC> {
    virtual RewritePlan *
    do_gather_type(const Item_func_not &i, Analysis &a) const
    {
        const EncSet out_es = PLAIN_EncSet;
        const EncSet child_es = EQ_EncSet;
        const std::string why = "not";

        return iterateGather(i, out_es, child_es, why, a);
    }

    virtual Item * do_optimize_type(Item_func_not *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }

    virtual Item *
    do_rewrite_type(const Item_func_not &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
} ANON;


template<Item_func::Functype FT, class IT>
class CItemCompare : public CItemSubtypeFT<Item_func, FT> {
    virtual RewritePlan *
    do_gather_type(const Item_func &i, Analysis &a) const
    {
        LOG(cdb_v) << "CItemCompare (L1139) do_gather func " << i;

        std::string why;

        std::function<EncSet ()> getEncSet =
            [&why, &i] ()
        {
            if (FT == Item_func::Functype::EQ_FUNC ||
                FT == Item_func::Functype::EQUAL_FUNC ||
                FT == Item_func::Functype::NE_FUNC) {
                why = "compare equality";

                Item *const *const args = i.arguments();
                if (!args[0]->const_item() && !args[1]->const_item()) {
                    why = why + "; join";
                    std::cerr << "join";
                    return JOIN_EncSet;     /* lambda */
                } else {
                    return EQ_EncSet;       /* lambda */
                }
            } else {
                why = "compare order";
                return ORD_EncSet;          /* lambda */
            }
        };
        const EncSet my_es = getEncSet();

        TEST_BadItemArgumentCount(i.type(), 2, i.argument_count());
        return typical_gather(a, i, my_es, why, false, PLAIN_EncSet);
    }

    virtual Item * do_optimize_type(Item_func *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }

    virtual Item *
    do_rewrite_type(const Item_func &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        LOG(cdb_v) << "do_rewrite_type Item_func " << i << " constr "
                   << EncSet(constr) << std::endl;
        TEST_BadItemArgumentCount(i.type(), 2, i.argument_count());
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
};

static CItemCompare<Item_func::Functype::EQ_FUNC,    Item_func_eq>    ANON;
static CItemCompare<Item_func::Functype::EQUAL_FUNC, Item_func_equal> ANON;
static CItemCompare<Item_func::Functype::NE_FUNC,    Item_func_ne>    ANON;
static CItemCompare<Item_func::Functype::GT_FUNC,    Item_func_gt>    ANON;
static CItemCompare<Item_func::Functype::GE_FUNC,    Item_func_ge>    ANON;
static CItemCompare<Item_func::Functype::LT_FUNC,    Item_func_lt>    ANON;
static CItemCompare<Item_func::Functype::LE_FUNC,    Item_func_le>    ANON;

template<Item_func::Functype FT, class IT>
class CItemCond : public CItemSubtypeFT<Item_cond, FT> {
    virtual RewritePlan *
    do_gather_type(const Item_cond &i, Analysis &a) const
    {
        const unsigned int arg_count =
            RiboldMYSQL::argument_list(i)->elements;
        // > Multiple ANDs in a query are represented with a single AND
        //   in the LEX.
        // TEST_BadItemArgumentCount(i->type(), 2, arg_count);

        const EncSet out_es = PLAIN_EncSet;
        EncSet child_es = EQ_EncSet;

        std::vector<std::pair<std::shared_ptr<RewritePlan>, OLK>>
            out_child_olks(arg_count);

        auto it =
            RiboldMYSQL::constList_iterator<Item>(*RiboldMYSQL::argument_list(i));
        unsigned int index = 0;
        for (;;) {
            const Item * const argitem = it++;
            if (!argitem)
                break;
            assert(index < arg_count);

            std::shared_ptr<RewritePlan>
                temp_childrp(gather(*argitem, a));
            const OLK &olk =
                EQ_EncSet.intersect(temp_childrp->es_out).chooseOne();
            out_child_olks[index] = std::make_pair(temp_childrp, olk);
            ++index;
        }

        const std::string why = "and/or";
        const reason rsn(out_es, why, i);

        // Must be an OLK for each argument.
        return new RewritePlanPerChildOLK(out_es, out_child_olks, rsn);
    }

    virtual Item * do_optimize_type(Item_cond *i, Analysis & a) const {
        return do_optimize_type_self_and_args(i, a);
    }

    virtual Item *
    do_rewrite_type(const Item_cond &i, const OLK &olk,
                    const RewritePlan &rp, Analysis &a) const
    {
        const unsigned int arg_count =
            RiboldMYSQL::argument_list(i)->elements;

        const RewritePlanPerChildOLK &rp_per_child =
            static_cast<const RewritePlanPerChildOLK &>(rp);
        auto it = RiboldMYSQL::constList_iterator<Item>(*RiboldMYSQL::argument_list(i));
        List<Item> out_list;
        unsigned int index = 0;
        for (;;) {
            const Item *const argitem = it++;
            if (!argitem) {
                break;
            }
            assert(index < arg_count);

            const std::pair<std::shared_ptr<RewritePlan>, OLK>
                &rp_olk = rp_per_child.child_olks[index];
            const std::shared_ptr<RewritePlan> &c_rp =
                rp_olk.first;
            const OLK &olk = rp_olk.second;
            Item *const out_item =
                itemTypes.do_rewrite(*argitem, olk, *c_rp.get(), a);
            out_item->name = NULL;
            out_list.push_back(out_item);
            ++index;
        }

        return new IT(out_list);
    }
};

static CItemCond<Item_func::Functype::COND_AND_FUNC, Item_cond_and> ANON;
static CItemCond<Item_func::Functype::COND_OR_FUNC,  Item_cond_or>  ANON;

template<Item_func::Functype FT>
class CItemNullcheck : public CItemSubtypeFT<Item_bool_func, FT> {
    virtual RewritePlan *
    do_gather_type(const Item_bool_func &i, Analysis &a) const
    {
        Item *const *const args = i.arguments();
        TEST_BadItemArgumentCount(i.type(), 1, i.argument_count());

        std::vector<std::shared_ptr<RewritePlan> >
            child_rp({std::shared_ptr<RewritePlan>(gather(*args[0], a))});

        const EncSet solution = child_rp[0]->es_out;
        const EncSet out_es = PLAIN_EncSet;

        const std::string why = "nullcheck";
        const reason rsn(out_es, why, i);

        return new RewritePlanOneOLK(out_es, solution.chooseOne(),
                                     child_rp, rsn);
    }

    virtual Item *
    do_rewrite_type(const Item_bool_func &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }

    virtual Item * do_optimize_type(Item_bool_func *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }
};

static CItemNullcheck<Item_func::Functype::ISNULL_FUNC> ANON;
static CItemNullcheck<Item_func::Functype::ISNOTNULL_FUNC> ANON;

static class ANON : public CItemSubtypeFT<Item_func_get_system_var, Item_func::Functype::GSYSVAR_FUNC> {

    virtual RewritePlan *
    do_gather_type(const Item_func_get_system_var &i, Analysis &a) const
    {
        const std::string why = "system var";
        const reason r = reason(PLAIN_EncSet, why, i);
        return new RewritePlan(PLAIN_EncSet, r);
    }

    virtual Item *
    do_rewrite_type(const Item_func_get_system_var &i,
                    const OLK &constr, const RewritePlan &_rp,
                    Analysis &a) const
    {
        return &const_cast<Item_func_get_system_var &>(i);
    }
} ANON;

template<class IT, const char *NAME>
class CItemAdditive : public CItemSubtypeFN<IT, NAME> {
    virtual RewritePlan *
    do_gather_type(const IT &i, Analysis &a) const
    {
        const std::string why = NAME;
        return typical_gather(a, i, ADD_EncSet, why, true);
    }

    virtual Item * do_optimize_type(IT *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }

    virtual Item *
    do_rewrite_type(const IT &i, const OLK &constr,
                    const RewritePlan &_rp, Analysis &a) const
    {
        LOG(cdb_v) << "do_rewrite_type Item_func_additive_op"
                   << i << std::endl;

        //rewrite children
        TEST_BadItemArgumentCount(i.type(), 2, i.argument_count());
        Item *const *const args = i.arguments();

        const RewritePlanOneOLK &rp =
            static_cast<const RewritePlanOneOLK &>(_rp);

        LOG(cdb_v) << "Rewrite plan is " << &rp << std::endl;

        Item *const arg0 =
            itemTypes.do_rewrite(*args[0], constr,
                                 *rp.childr_rp[0].get(), a);
        Item *const arg1 =
            itemTypes.do_rewrite(*args[1], constr,
                                 *rp.childr_rp[1].get(), a);

        if (oAGG == constr.o) {
            OnionMeta *const om = rp.olk.key->getOnionMeta(oAGG);
            assert(om);
            EncLayer const &el = a.getBackEncLayer(*om);
            TEST_UnexpectedSecurityLevel(oAGG, SECLEVEL::HOM,
                                         el.level());
            return static_cast<const HOM &>(el).sumUDF(arg0, arg1);
        } else {
            return new IT(arg0, arg1);
        }
    }
};

extern const char str_plus[] = "+";
static CItemAdditive<Item_func_plus, str_plus> ANON;

extern const char str_minus[] = "-";
static CItemAdditive<Item_func_minus, str_minus> ANON;

template<class IT, const char *NAME>
class CItemMath : public CItemSubtypeFN<IT, NAME> {
    virtual RewritePlan *
    do_gather_type(const IT &i, Analysis & a) const
    {
        const std::string why = NAME;
        return typical_gather(a, i, PLAIN_EncSet, why, true);
    }

    virtual Item * do_optimize_type(IT *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }

    virtual Item *
    do_rewrite_type(const IT &i, const OLK &constr,
                    const RewritePlan &_rp, Analysis & a) const
    {
        TEST_BadItemArgumentCount(i.type(), 2, i.argument_count());
        Item *const *const args = i.arguments();

        const RewritePlanOneOLK &rp =
            static_cast<const RewritePlanOneOLK &>(_rp);

        Item *const arg0 =
            itemTypes.do_rewrite(*args[0], rp.olk,
                                 *rp.childr_rp[0].get(), a);
        Item *const arg1 =
            itemTypes.do_rewrite(*args[1], rp.olk,
                                 *rp.childr_rp[1].get(), a);

        return new IT(arg0, arg1);
    }
};

extern const char str_mul[] = "*";
static CItemMath<Item_func_mul, str_mul> ANON;

extern const char str_div[] = "/";
static CItemMath<Item_func_div, str_div> ANON;

extern const char str_idiv[] = "div";
static CItemMath<Item_func_int_div, str_idiv> ANON;

/*
extern const char str_sqrt[] = "sqrt";
static CItemMath<Item_func_sqrt, str_sqrt> ANON;

extern const char str_round[] = "round";
static CItemMath<Item_func_round, str_round> ANON;

extern const char str_sin[] = "sin";
static CItemMath<str_sin> ANON;

extern const char str_cos[] = "cos";
static CItemMath<str_cos> ANON;

extern const char str_acos[] = "acos";
static CItemMath<str_acos> ANON;
*/

extern const char str_pow[] = "pow";
static CItemMath<Item_func_pow, str_pow> ANON;

// FIXME: Supports one argument version as well.
extern const char str_log[] = "log";
static CItemMath<Item_func_log, str_log> ANON;

// extern const char str_radians[] = "radians";
// static CItemMath<str_radians> ANON;


// FIXME: Use encryption/rewriting.
// FIXME: Determine if we are dealing with arguments correctly.
template<const char *NAME>
class CItemLeafFunc : public CItemSubtypeFN<Item_func, NAME> {
    virtual RewritePlan *
    do_gather_type(const Item_func &i, Analysis &a) const
    {
        const std::string why = "leaf func (" + std::string(NAME) + ")";
        return allPlainIterateGather(i, why, a);
    }

    virtual Item *
    do_rewrite_type(const Item_func &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
};

extern const char str_found_rows[] = "found_rows";
static CItemLeafFunc<str_found_rows> ANON;

extern const char str_last_insert_id[] = "last_insert_id";
static CItemLeafFunc<str_last_insert_id> ANON;

extern const char str_rand[] = "rand";
static CItemLeafFunc<str_rand> ANON;

extern const char str_database[] = "database";
static CItemLeafFunc<str_database> ANON;


static class ANON : public CItemSubtypeFT<Item_extract, Item_func::Functype::EXTRACT_FUNC> {
    virtual RewritePlan *
    do_gather_type(const Item_extract &i, Analysis &a) const
    {
    /*  analyze(i->arguments()[0], reason(EMPTY_EncSet, "extract", i, &tr), a);
        return tr.encset;
    */
        UNIMPLEMENTED;
    }
    virtual Item * do_optimize_type(Item_extract *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }
} ANON;

// FIXME: Use encryption/rewriting.
template<const char *NAME>
class CItemDateExtractFunc : public CItemSubtypeFN<Item_func, NAME> {
    virtual RewritePlan *
    do_gather_type(const Item_func &i, Analysis &a) const
    {
        const std::string why = NAME;
        return allPlainIterateGather(i, why, a);
    }

    virtual Item *do_optimize_type(Item_func *i, Analysis &a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }

    virtual Item *
    do_rewrite_type(const Item_func &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
};

extern const char str_second[] = "second";
static CItemDateExtractFunc<str_second> ANON;

extern const char str_minute[] = "minute";
static CItemDateExtractFunc<str_minute> ANON;

extern const char str_hour[] = "hour";
static CItemDateExtractFunc<str_hour> ANON;

extern const char str_to_days[] = "to_days";
static CItemDateExtractFunc<str_to_days> ANON;

extern const char str_year[] = "year";
static CItemDateExtractFunc<str_year> ANON;

extern const char str_month[] = "month";
static CItemDateExtractFunc<str_month> ANON;

extern const char str_week[] = "week";
static CItemDateExtractFunc<str_week> ANON;

extern const char str_dayofmonth[] = "dayofmonth";
static CItemDateExtractFunc<str_dayofmonth> ANON;

extern const char str_dayofweek[] = "dayofweek";
static CItemDateExtractFunc<str_dayofweek> ANON;

extern const char str_unix_timestamp[] = "unix_timestamp";
static CItemDateExtractFunc<str_unix_timestamp> ANON;

extern const char str_date_format[] = "date_format";
static CItemDateExtractFunc<str_date_format> ANON;

template<const char *NAME>
class CItemBitfunc : public CItemSubtypeFN<Item_func_bit, NAME> {
    virtual RewritePlan *
    do_gather_type(const Item_func_bit &i, Analysis &a) const
    {
        /*Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], reason(EMPTY_EncSet, "bitfunc", i, &tr), a);
        return tr.encset;
        */
        UNIMPLEMENTED;
    }
    virtual Item * do_optimize_type(Item_func_bit *i, Analysis & a) const {
        return do_optimize_type_self_and_args(i, a);
    }
};

extern const char str_bit_not[] = "~";
static CItemBitfunc<str_bit_not> ANON;

extern const char str_bit_or[] = "|";
static CItemBitfunc<str_bit_or> ANON;

extern const char str_bit_xor[] = "^";
static CItemBitfunc<str_bit_xor> ANON;

extern const char str_bit_and[] = "&";
static CItemBitfunc<str_bit_and> ANON;

static class ANON : public CItemSubtypeFT<Item_func_like, Item_func::Functype::LIKE_FUNC> {
    virtual RewritePlan *
    do_gather_type(const Item_func_like &i, Analysis &a) const
    {
        TEST_BadItemArgumentCount(i.type(), 2, i.argument_count());
        const std::string why = "like";
        return allPlainIterateGather(i, why, a);

	/*
	LOG(cdb_v) << "Item_func do_gather_type " << *i;

	Item **args = i->arguments();
	assert_s(i->argument_count() == 2, "expecting LIKE to have two arguments");

	if ((args[0]->type() == Item::Type::FIELD_ITEM) && (args[1]->type() == Item::Type::STRING_ITEM)) {

	    string s(args[1]->str_value.ptr(), args[1]->str_value.length());

	    if (s.find('%') == s.npos && s.find('_') == s.npos) {
                // some queries actually use LIKE as an equality check..
		return typical_gather(a, i, EQ_EncSet, "LIKE equality", tr, false, PLAIN_EncSet);

            } else {

		// XXX check if pattern is one we can support?

		reason r1;
		RewritePlan * rp1 = gather(args[0], r1, a);

		EncSet solution = rp1->es_out.intersect(Search_EncSet);

		if (solution.empty()) {
		    cerr << "cannot support " << i << " BECAUSE it needs any of " << Search_EncSet << "\n" \
			 << " but field only supports  " << rp1->es_out << "\n";
		}

		stringstream ss;
		ss << "like:'" << s << "'";
		tr = reason(PLAIN_EncSet, ss.str(), i);
		tr.add_child(r1);

		return new RewritePlan(PLAIN_OLK, solution.chooseOne(), tr);
            }
        } else {
            // we cannot support non-constant search patterns
	    assert_s(false, "we cannot support search patterns not of the form (field like const string)");
	    //for (uint x = 0; x < i->argument_count(); x++)
            //    analyze(args[x], reason(EMPTY_EncSet, "like-non-const", i, &tr), a);
        }


	*/
    }

    virtual Item * do_optimize_type(Item_func_like *i, Analysis & a) const {
        return do_optimize_type_self_and_args(i, a);
    }
    virtual Item *
    do_rewrite_type(const Item_func_like &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        const RewritePlanOneOLK &one_rp =
            static_cast<const RewritePlanOneOLK &>(rp);
        return rewrite_args_FN(i, constr, one_rp, a);
/*	LOG(cdb_v) << "Item_func_like do_rewrite_type " << *i;

	assert_s(i->argument_count() == 2, "expecting LIKE to have two arguements");
	Item **args = i->arguments();

	if ((args[0]->type() == Item::Type::FIELD_ITEM) && (args[1]->type() == Item::Type::STRING_ITEM)) {

	    string s(args[1]->str_value.ptr(), args[1]->str_value.length());

	    if (s.find('%') == s.npos && s.find('_') == s.npos) {
                // some queries actually use LIKE as an equality check..

		Item_func_like * res = new Item_func_like(args[0], args[1], NULL, false);
		do_rewrite_type_args(i, res, constr, a);

		return res;

            } else {
		// XXX check if pattern is one we can support?

		RewritePlan * plan = getAssert(a.itemRewritePlans, (Item*) i);
		auto childr_plan = getAssert(plan->plan, constr);
		OLK child_OLK = getAssert(childr_plan, args[0]);

		if (child_OLK == PLAIN_OLK) {
		    return new Item_func_like(args[0], args[1], NULL, false);
		}

		Item * field = itemTypes.do_rewrite(args[0], child_OLK, a);
		args[0]->name = NULL; //no alias

		Item * expr = args[1];
		FieldMeta * fm = child_OLK.key;

		EncLayer * el = getAssert(fm->onions, oSWP)->layers.back();
		assert_s(el->level() == SECLEVEL::SEARCH, "incorrect onion  level on onion oSWP");

		Item * res = ((Search *) el)->searchUDF(field, expr);
		cerr << "result is " << *res << "\n";

		return res;
            }
        }

	// we cannot support non-constant search patterns
	assert_s(false, "we cannot support search patterns not of the form (field like constant string)");
*/
    }
} ANON;


static class ANON : public CItemSubtypeFT<Item_func, Item_func::Functype::FUNC_SP> {
    void error(const Item_func &i) const __attribute__((noreturn)) {
        thrower() << "unsupported stored procedure call "
                  << i << std::endl;
    }
    virtual RewritePlan *
    do_gather_type(const Item_func &i, Analysis &a) const
    {
        error(i);
    }
} ANON;

static class ANON : public CItemSubtypeFT<Item_func_in, Item_func::Functype::IN_FUNC> {
    virtual RewritePlan *
    do_gather_type(const Item_func_in &i, Analysis &a) const
    {
        const EncSet out_es = PLAIN_EncSet;
        const EncSet child_es = EQ_EncSet; 
        const std::string why = "in";

        return iterateGather(i, out_es, child_es, why, a);
    }

    virtual Item * do_optimize_type(Item_func_in *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }
    virtual Item *
    do_rewrite_type(const Item_func_in &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
} ANON;

static class ANON : public CItemSubtypeFT<Item_func_between,
                                        Item_func::Functype::BETWEEN> {
    virtual RewritePlan *
    do_gather_type(const Item_func_between &i, Analysis &a) const
    {
        const EncSet out_es = PLAIN_EncSet;
        const EncSet child_es = ORD_EncSet; 
        const std::string why = "between";

        return iterateGather(i, out_es, child_es, why, a);
    }
    virtual Item * do_optimize_type(Item_func_between *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }

    virtual Item *
    do_rewrite_type(const Item_func_between &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
} ANON;

//TODO: use this func in other places as well
/*static List<Item> *
getArgs(Item_func * itf) {
    List<Item> * res = new List<Item>();
    Item ** args = itf->arguments();
    for (uint j = 0; j < itf->argument_count(); j++)  {
	res->push_back(args[j]);
    }
    return res;
}
*/

template<const char *FN, class IT>
class CItemMinMax : public CItemSubtypeFN<Item_func_min_max, FN> {
    virtual RewritePlan *
    do_gather_type(const Item_func_min_max &i, Analysis &a) const
    {
        const unsigned int arg_count = i.argument_count();
        TEST_BadItemArgumentCount(i.type(), 2, arg_count);

        Item *const *const args = i.arguments();
        //at least one has to be a constant as we don't support join now
        assert_s(args[0]->const_item() || args[1]->const_item(),
                 "ope join not yet implemented");

        std::vector<std::shared_ptr<RewritePlan> >
            childr_rp({
                std::shared_ptr<RewritePlan>(gather(*args[0], a)),
                std::shared_ptr<RewritePlan>(gather(*args[1], a))});
        const EncSet es1 = childr_rp[0]->es_out;
        const EncSet es2 = childr_rp[1]->es_out;
        const EncSet needed_es = ORD_EncSet;
        const EncSet supported_es =
            needed_es.intersect(es1).intersect(es2);

        const std::string why = "min_max func";
        TEST_NoAvailableEncSet(supported_es, i.type(), needed_es, why,
                               childr_rp);

        const EncSet out_es = es1.intersect(es2);
        const reason rsn(out_es, why, i);

        //prepare rewrite plans
        return new RewritePlanOneOLK(out_es, supported_es.chooseOne(),
                                     childr_rp, rsn);
    }

    virtual Item *do_optimize_type(Item_func_min_max *i,
                                   Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }

    //FIXME: Cleanup.
    virtual Item *
    do_rewrite_type(const Item_func_min_max &i, const OLK &constr,
                    const RewritePlan &_rp, Analysis &a) const
    {
        const RewritePlanOneOLK &rp =
            static_cast<const RewritePlanOneOLK &>(_rp);

        if (SECLEVEL::PLAINVAL == rp.olk.l) { // no change
            return &const_cast<Item_func_min_max &>(i);
        }

        // replace with IF( cond_arg0 cond cond_arg1, args0, args1)
        Item *const *const args = i.arguments();
        Item *const cond_arg0 =
            itemTypes.do_rewrite(*args[0], rp.olk,
                                 *rp.childr_rp[0].get(), a);
        Item *const cond_arg1 =
            itemTypes.do_rewrite(*args[1], rp.olk,
                                 *rp.childr_rp[1].get(), a);

        const int cmp_sign =
            i.*rob<Item_func_min_max, int,
                    &Item_func_min_max::cmp_sign>::ptr();

        Item *const cond =
            cmp_sign ? static_cast<Item *>(new
                            Item_func_gt(cond_arg0, cond_arg1))
                     : static_cast<Item *>(new
                            Item_func_lt(cond_arg0, cond_arg1));

        return new Item_func_if(cond,
                                itemTypes.do_rewrite(*args[0], constr,
                                                *rp.childr_rp[0].get(),
                                                a),
                                itemTypes.do_rewrite(*args[1], constr,
                                                *rp.childr_rp[1].get(),
                                                a));
    }
};

//TODO: do we still need the file analyze.cc?

extern const char str_greatest[] = "greatest";
static CItemMinMax<str_greatest, Item_func_max> ANON;

extern const char str_least[] = "least";
static CItemMinMax<str_least, Item_func_min> ANON;


extern const char str_strcmp[] = "strcmp";
static class ANON : public CItemSubtypeFN<Item_func_strcmp, str_strcmp> {
    virtual RewritePlan *
    do_gather_type(const Item_func_strcmp &i, Analysis &a) const
    {
        //cerr << "do_a_t Item_func_strcmp reason " << tr << "\n";
	/* Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], reason(EQ_EncSet, "strcmp", i, &tr), a);
        return tr.encset;
	*/
        UNIMPLEMENTED;
    }
    virtual Item * do_optimize_type(Item_func_strcmp *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }
} ANON;



extern const char str_if[] = "if";

static class ANON : public CItemSubtypeFN<Item_func_if, str_if> {
    virtual RewritePlan *
    do_gather_type(const Item_func_if &i, Analysis &a) const
    {
        /*
        Item **args = i->arguments();
        assert(i->argument_count() == 3);
        analyze(args[0], a);
        analyze(args[1], a);
        analyze(args[2], a);
        return a.rewritePlans.find(i)->second;
        */
        UNIMPLEMENTED;
    }

    virtual Item * do_optimize_type(Item_func_if *i, Analysis & a) const {
        return do_optimize_type_self_and_args(i, a);
    }
} ANON;

// FIXME: This query must be rewritten into a proper IF().
// > Otherwise the value it produces must be rewritten in the same way
// > that the value it compares was rewritten.
extern const char str_nullif[] = "nullif";
static class ANON : public CItemSubtypeFN<Item_func_nullif, str_nullif> {
    virtual RewritePlan *
    do_gather_type(const Item_func_nullif &i, Analysis &a) const
    {
        TEST_BadItemArgumentCount(i.type(), 2, i.argument_count());
        Item *const *const args = i.arguments();
        std::vector<std::shared_ptr<RewritePlan> >
            childr_rp({
                std::shared_ptr<RewritePlan>(gather(*args[0], a)),
                std::shared_ptr<RewritePlan>(gather(*args[1], a))});

        const EncSet child_es =
            EQ_EncSet.intersect(childr_rp[0]->es_out)
                     .intersect(childr_rp[1]->es_out);

        // HACK.
        const EncSet out_es = EncSet(child_es.chooseOne());

        const std::string why = "nullif";
        const reason rsn(out_es, why, i);

        return new RewritePlanOneOLK(out_es, child_es.chooseOne(),
                                     childr_rp, rsn);
    }

    virtual Item * do_optimize_type(Item_func_nullif *i, Analysis & a)
        const
    {
        return do_optimize_type_self_and_args(i, a);
    }

    virtual Item *
    do_rewrite_type(const Item_func_nullif &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
} ANON;


extern const char str_coalesce[] = "coalesce";
static class ANON : public CItemSubtypeFN<Item_func_coalesce,
                                          str_coalesce> {
    virtual RewritePlan *
    do_gather_type(const Item_func_coalesce &i, Analysis &a) const
    {
        /*
	 Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], a);
	    return a.rewritePlans.find(i)->second;
        */
        UNIMPLEMENTED;
    }
    virtual Item * do_optimize_type(Item_func_coalesce *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }
} ANON;

extern const char str_case[] = "case";
static class ANON : public CItemSubtypeFN<Item_func_case, str_case> {
    virtual RewritePlan *
    do_gather_type(const Item_func_case &i, Analysis &a) const
    {
	/*     Item **args = i->arguments();
        int first_expr_num = i->*rob<Item_func_case, int,
                &Item_func_case::first_expr_num>::ptr();
        int else_expr_num = i->*rob<Item_func_case, int,
                &Item_func_case::else_expr_num>::ptr();
        uint ncases = i->*rob<Item_func_case, uint,
                &Item_func_case::ncases>::ptr();

        if (first_expr_num >= 0)
            analyze(args[first_expr_num],
                    reason(EQ_EncSet, "case_first", i, &tr), a);
        if (else_expr_num >= 0)
            analyze(args[else_expr_num], tr, a);

        for (uint x = 0; x < ncases; x += 2) {
            if (first_expr_num < 0)
            analyze(args[x],
                    reason(EMPTY_EncSet, "case_nofirst", i, &tr), a);
            else
            analyze(args[x],
                    reason(EQ_EncSet, "case_w/first", i, &tr), a);
            analyze(args[x+1], tr, a);
        }
        return tr.encset;
	*/
        UNIMPLEMENTED;
    }

    virtual Item * do_optimize_type(Item_func_case *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }
} ANON;



extern const char str_in_optimizer[] = "<in_optimizer>";
static class ANON : public CItemSubtypeFN<Item_in_optimizer,
                                          str_in_optimizer> {
    virtual RewritePlan *
    do_gather_type(const Item_in_optimizer &i, Analysis &a) const
    {
        UNIMPLEMENTED;

        /*
        LOG(cdb_v) << "CItemSubtypeFN (L1107) do_gather " << *i;
        const std::string why = str_in_optimizer;
        return allPlainIterateGather(i, why, tr, a);
        */

        /*
        //Item **args = i->arguments();
        //analyze(args[0], reason(EMPTY_EncSet, "in_opt", i, &tr), a);
        //analyze(args[1], reason(EMPTY_EncSet, "in_opt", i, &tr), a);
        return NULL;
        */
    }

    virtual Item * do_optimize_type(Item_in_optimizer *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }

    virtual Item *
    do_rewrite_type(const Item_in_optimizer &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
} ANON;



template<const char *NAME>
class CItemStrconv : public CItemSubtypeFN<Item_str_conv, NAME> {
    virtual RewritePlan *
    do_gather_type(const Item_str_conv &i, Analysis &a) const
    {
        const std::string why = "strconv";
        return allPlainIterateGather(i, why, a);
    }

    virtual Item * do_optimize_type(Item_str_conv *i, Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }

    virtual Item *
    do_rewrite_type(const Item_str_conv &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
};

extern const char str_lcase[] = "lcase";
static CItemStrconv<str_lcase> ANON;

extern const char str_ucase[] = "ucase";
static CItemStrconv<str_ucase> ANON;

extern const char str_length[] = "length";
static CItemStrconv<str_length> ANON;

extern const char str_char_length[] = "char_length";
static CItemStrconv<str_char_length> ANON;

extern const char str_substr[] = "substr";
static CItemStrconv<str_substr> ANON;

extern const char str_concat[] = "concat";
static CItemStrconv<str_concat> ANON;

extern const char str_concat_ws[] = "concat_ws";
static CItemStrconv<str_concat_ws> ANON;

extern const char str_md5[] = "md5";
static CItemStrconv<str_md5> ANON;

extern const char str_left[] = "left";
static CItemStrconv<str_left> ANON;

extern const char str_regexp[] = "regexp";
static CItemStrconv<str_regexp> ANON;
 
extern const char str_date_add_interval[] = "date_add_interval";
// Use encryption/rewriting.
static class ANON : public CItemSubtypeFN<Item_date_add_interval, str_date_add_interval> {
    virtual RewritePlan *
    do_gather_type(const Item_date_add_interval &i, Analysis &a) const
    {
        const EncSet out_es = PLAIN_EncSet;
        const EncSet child_es = PLAIN_EncSet;
        const std::string why = "date add interval";

        return allPlainIterateGather(i, why, a);
    }

    virtual Item * do_optimize_type(Item_date_add_interval *i,
                                    Analysis & a) const
    {
        return do_optimize_type_self_and_args(i, a);
    }

    virtual Item *
    do_rewrite_type(const Item_date_add_interval &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
} ANON;

// FIXME: Use encryption/rewriting.
template<const char *NAME>
class CItemDateNow : public CItemSubtypeFN<Item_func_now, NAME> {
    virtual RewritePlan *
    do_gather_type(const Item_func_now &i, Analysis &a) const
    {
        const std::string why = "now";
        return allPlainIterateGather(i, why, a);
    }

    virtual Item *
    do_rewrite_type(const Item_func_now &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
};

extern const char str_now[] = "now";
static CItemDateNow<str_now> ANON;

extern const char str_utc_timestamp[] = "utc_timestamp";
static CItemDateNow<str_utc_timestamp> ANON;

extern const char str_sysdate[] = "sysdate";
static CItemDateNow<str_sysdate> ANON;

// FIXME: What does Item_char_typecast do?
// FIXME: Use encryption/rewriting.
static class ANON: public CItemSubtypeFT<Item_char_typecast,
                            Item_func::Functype::CHAR_TYPECAST_FUNC> {
    virtual RewritePlan *
    do_gather_type(const Item_char_typecast &i, Analysis &a) const
    {
        const std::string why = "char_typecast";
        return allPlainIterateGather(i, why, a);
    }

    virtual Item *
    do_rewrite_type(const Item_char_typecast &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
} ANON;

template <const char *NAME>
class CItemFuncMiscPlain : public CItemSubtypeFN<Item_func, NAME> {
    virtual RewritePlan *
    do_gather_type(const Item_func &i, Analysis &a) const
    {
        const std::string why = NAME;
        return allPlainIterateGather(i, why, a);
    }

    virtual Item *
    do_rewrite_type(const Item_func &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        return rewrite_args_FN(i, constr,
                               static_cast<const RewritePlanOneOLK &>(rp),
                               a);
    }
};

extern const char str_field[] = "field";
static CItemFuncMiscPlain<str_field> ANON;

extern const char str_cast_as_unsigned[] = "cast_as_unsigned";
static CItemFuncMiscPlain<str_cast_as_unsigned> ANON;

extern const char str_cast_as_signed[] = "cast_as_signed";
static CItemFuncMiscPlain<str_cast_as_signed> ANON;
