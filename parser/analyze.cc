#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>
#include <stdio.h>

#include <parser/embedmysql.hh>
#include <sql_select.h>
#include <sql_delete.h>
#include <sql_insert.h>
#include <sql_update.h>
#include <handler.h>


#include <parser/stringify.hh>

#include <util/errstream.hh>
#include <util/cleanup.hh>
#include <util/rob.hh>


using namespace std;

static bool just_strings = false;

#define CIPHER_TYPES(m)                                                 \
    m(none)     /* no data needed (blind writes) */                     \
    m(any)      /* just need to decrypt the result */                   \
    m(plain)    /* evaluate Item on the server, e.g. for WHERE */       \
    m(order)    /* evaluate order on the server, e.g. for SORT BY */    \
    m(equal)    /* evaluate dups on the server, e.g. for GROUP BY */    \
    m(like)     /* need to do LIKE */                                   \
    m(homadd)   /* addition */

enum class cipher_type {
#define __temp_m(n) n,
CIPHER_TYPES(__temp_m)
#undef __temp_m
};

static const string cipher_type_names[] = {
#define __temp_m(n) #n,
CIPHER_TYPES(__temp_m)
#undef __temp_m
};

static ostream&
operator<<(ostream &out, const cipher_type &t)
{
    return out << cipher_type_names[(int) t];
}

class cipher_type_reason {
 public:
    cipher_type_reason(cipher_type t_arg,
                       const std::string &why_t_arg,
                       Item *why_t_item_arg,
                       const cipher_type_reason *parent_arg)
    : t(t_arg), soft(false),
      why_t(why_t_arg), why_t_item(why_t_item_arg),
      parent(parent_arg)
    {
        if (parent && parent->soft)
            soft = true;
    }

    cipher_type_reason(cipher_type t_arg,
                       const std::string &why_t_arg,
                       Item *why_t_item_arg,
                       const cipher_type_reason *parent_arg,
                       bool soft_arg)
    : t(t_arg), soft(soft_arg),
      why_t(why_t_arg), why_t_item(why_t_item_arg),
      parent(parent_arg)
    {
    }

    cipher_type t;
    bool soft;      /* can be evaluated at proxy */

    string why_t;
    Item *why_t_item;

    const cipher_type_reason *parent;
};

static ostream&
operator<<(ostream &out, const cipher_type_reason &r)
{
    out << r.t;
    if (r.soft)
        out << "(soft)";
    out << " NEEDED FOR " << r.why_t;
    if (r.why_t_item)
        out << " in " << *r.why_t_item;
    if (r.parent)
        out << " BECAUSE " << *r.parent;
    return out;
}


class CItemType {
 public:
    virtual void do_analyze(Item *, const cipher_type_reason&) const = 0;
};


/*
 * Directories for locating an appropriate CItemType for a given Item.
 */
template <class T>
class CItemTypeDir : public CItemType {
 public:
    void reg(T t, CItemType *ct) {
        auto x = types.find(t);
        if (x != types.end())
            thrower() << "duplicate key " << t;
        types[t] = ct;
    }

    void do_analyze(Item *i, const cipher_type_reason &tr) const {
        lookup(i)->do_analyze(i, tr);
    }

 protected:
    virtual CItemType *lookup(Item *i) const = 0;

    CItemType *do_lookup(Item *i, T t, const char *errname) const {
        auto x = types.find(t);
        if (x == types.end())
            thrower() << "missing " << errname << " " << t << " in " << *i;
        return x->second;
    }

 private:
    std::map<T, CItemType*> types;
};

static class ANON : public CItemTypeDir<Item::Type> {
    CItemType *lookup(Item *i) const {
        return do_lookup(i, i->type(), "type");
    }
} itemTypes;

static class CItemFuncDir : public CItemTypeDir<Item_func::Functype> {
    CItemType *lookup(Item *i) const {
        return do_lookup(i, ((Item_func *) i)->functype(), "func type");
    }
 public:
    CItemFuncDir() {
        itemTypes.reg(Item::Type::FUNC_ITEM, this);
        itemTypes.reg(Item::Type::COND_ITEM, this);
    }
} funcTypes;

static class CItemSumFuncDir : public CItemTypeDir<Item_sum::Sumfunctype> {
    CItemType *lookup(Item *i) const {
        return do_lookup(i, ((Item_sum *) i)->sum_func(), "sumfunc type");
    }
 public:
    CItemSumFuncDir() {
        itemTypes.reg(Item::Type::SUM_FUNC_ITEM, this);
    }
} sumFuncTypes;

static class CItemFuncNameDir : public CItemTypeDir<std::string> {
    CItemType *lookup(Item *i) const {
        return do_lookup(i, ((Item_func *) i)->func_name(), "func name");
    }
 public:
    CItemFuncNameDir() {
        funcTypes.reg(Item_func::Functype::UNKNOWN_FUNC, this);
        funcTypes.reg(Item_func::Functype::NOW_FUNC, this);
    }
} funcNames;


/*
 * Helper functions to look up via directory & invoke method.
 */
static void
analyze(Item *i, const cipher_type_reason &tr)
{
    if (tr.t != cipher_type::none && !i->const_item())
        itemTypes.do_analyze(i, tr);
}


/*
 * CItemType classes for supported Items: supporting machinery.
 */
template<class T>
class CItemSubtype : public CItemType {
    virtual void do_analyze(Item *i, const cipher_type_reason &tr) const {
        do_analyze_type((T*) i, tr);
    }
 private:
    virtual void do_analyze_type(T *, const cipher_type_reason&) const = 0;
};

template<class T, Item::Type TYPE>
class CItemSubtypeIT : public CItemSubtype<T> {
 public:
    CItemSubtypeIT() { itemTypes.reg(TYPE, this); }
};

template<class T, Item_func::Functype TYPE>
class CItemSubtypeFT : public CItemSubtype<T> {
 public:
    CItemSubtypeFT() { funcTypes.reg(TYPE, this); }
};

template<class T, Item_sum::Sumfunctype TYPE>
class CItemSubtypeST : public CItemSubtype<T> {
 public:
    CItemSubtypeST() { sumFuncTypes.reg(TYPE, this); }
};

template<class T, const char *TYPE>
class CItemSubtypeFN : public CItemSubtype<T> {
 public:
    CItemSubtypeFN() { funcNames.reg(std::string(TYPE), this); }
};


/*
 * Actual item handlers.
 */
static void process_select_lex(st_select_lex *select_lex, const cipher_type_reason &tr);

static class ANON : public CItemSubtypeIT<Item_field, Item::Type::FIELD_ITEM> {
    virtual void do_analyze_type(Item_field *i, const cipher_type_reason &tr) const {
        if (just_strings && i->result_type() != STRING_RESULT)
            return;
        cout << "FIELD " << *i << " CIPHER " << tr << endl;
    }
} ANON;

static class ANON : public CItemSubtypeIT<Item_string, Item::Type::STRING_ITEM> {
    virtual void do_analyze_type(Item_string *i, const cipher_type_reason &tr) const {
        /* constant strings are always ok */
    }
} ANON;

static class ANON : public CItemSubtypeIT<Item_num, Item::Type::INT_ITEM> {
    virtual void do_analyze_type(Item_num *i, const cipher_type_reason &tr) const {
        /* constant ints are always ok */
    }
} ANON;

static class ANON : public CItemSubtypeIT<Item_decimal, Item::Type::DECIMAL_ITEM> {
    virtual void do_analyze_type(Item_decimal *i, const cipher_type_reason &tr) const {
        /* constant decimals are always ok */
    }
} ANON;

static class ANON : public CItemSubtypeFT<Item_func_neg, Item_func::Functype::NEG_FUNC> {
    virtual void do_analyze_type(Item_func_neg *i, const cipher_type_reason &tr) const {
        analyze(i->arguments()[0], tr);
    }
} ANON;

static class ANON : public CItemSubtypeFT<Item_func_not, Item_func::Functype::NOT_FUNC> {
    virtual void do_analyze_type(Item_func_not *i, const cipher_type_reason &tr) const {
        analyze(i->arguments()[0], tr);
    }
} ANON;

static class ANON : public CItemSubtypeIT<Item_subselect, Item::Type::SUBSELECT_ITEM> {
    virtual void do_analyze_type(Item_subselect *i, const cipher_type_reason &tr) const {
        st_select_lex *select_lex = i->get_select_lex();
        process_select_lex(select_lex, tr);
    }
} ANON;

extern const char str_in_optimizer[] = "<in_optimizer>";
static class ANON : public CItemSubtypeFN<Item_in_optimizer, str_in_optimizer> {
    virtual void do_analyze_type(Item_in_optimizer *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        analyze(args[0], cipher_type_reason(cipher_type::any, "in_opt", i, &tr));
        analyze(args[1], cipher_type_reason(cipher_type::any, "in_opt", i, &tr));
    }
} ANON;

static class ANON : public CItemSubtypeIT<Item_cache, Item::Type::CACHE_ITEM> {
    virtual void do_analyze_type(Item_cache *i, const cipher_type_reason &tr) const {
        Item *example = (*i).*rob<Item_cache, Item*, &Item_cache::example>::ptr();
        if (example)
            analyze(example, tr);
    }
} ANON;

template<Item_func::Functype FT, class IT>
class CItemCompare : public CItemSubtypeFT<Item_func, FT> {
    virtual void do_analyze_type(Item_func *i, const cipher_type_reason &tr) const {
        cipher_type t2;
        if (FT == Item_func::Functype::EQ_FUNC ||
            FT == Item_func::Functype::EQUAL_FUNC ||
            FT == Item_func::Functype::NE_FUNC)
        {
            t2 = cipher_type::equal;
        } else {
            t2 = cipher_type::order;
        }

        Item **args = i->arguments();
        const char *reason = "compare_func";
        if (!args[0]->const_item() && !args[1]->const_item())
            reason = "compare_func_join";
        analyze(args[0], cipher_type_reason(t2, reason, i, &tr));
        analyze(args[1], cipher_type_reason(t2, reason, i, &tr));
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
    virtual void do_analyze_type(Item_cond *i, const cipher_type_reason &tr) const {
        auto it = List_iterator<Item>(*i->argument_list());
        for (;;) {
            Item *argitem = it++;
            if (!argitem)
                break;

            analyze(argitem, cipher_type_reason(cipher_type::plain, "cond", i, &tr));
        }
    }
};

static CItemCond<Item_func::Functype::COND_AND_FUNC, Item_cond_and> ANON;
static CItemCond<Item_func::Functype::COND_OR_FUNC,  Item_cond_or>  ANON;

template<Item_func::Functype FT>
class CItemNullcheck : public CItemSubtypeFT<Item_bool_func, FT> {
    virtual void do_analyze_type(Item_bool_func *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], cipher_type_reason(cipher_type::any, "nullcheck", i, &tr));
    }
};

static CItemNullcheck<Item_func::Functype::ISNULL_FUNC> ANON;
static CItemNullcheck<Item_func::Functype::ISNOTNULL_FUNC> ANON;

static class ANON : public CItemSubtypeFT<Item_func_get_system_var, Item_func::Functype::GSYSVAR_FUNC> {
    virtual void do_analyze_type(Item_func_get_system_var *i, const cipher_type_reason &tr) const {}
} ANON;

template<const char *NAME>
class CItemAdditive : public CItemSubtypeFN<Item_func_additive_op, NAME> {
    virtual void do_analyze_type(Item_func_additive_op *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        if (tr.t == cipher_type::any) {
            analyze(args[0], cipher_type_reason(cipher_type::homadd, "additive", i, &tr));
            analyze(args[1], cipher_type_reason(cipher_type::homadd, "additive", i, &tr));
        } else {
            analyze(args[0], cipher_type_reason(cipher_type::plain, "additivex", i, &tr));
            analyze(args[1], cipher_type_reason(cipher_type::plain, "additivex", i, &tr));
        }
    }
};

extern const char str_plus[] = "+";
static CItemAdditive<str_plus> ANON;

extern const char str_minus[] = "-";
static CItemAdditive<str_minus> ANON;

template<const char *NAME>
class CItemMath : public CItemSubtypeFN<Item_func, NAME> {
    virtual void do_analyze_type(Item_func *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], cipher_type_reason(cipher_type::plain, "math", i, &tr));
    }
};

extern const char str_mul[] = "*";
static CItemMath<str_mul> ANON;

extern const char str_div[] = "/";
static CItemMath<str_div> ANON;

extern const char str_idiv[] = "div";
static CItemMath<str_idiv> ANON;

extern const char str_sqrt[] = "sqrt";
static CItemMath<str_sqrt> ANON;

extern const char str_round[] = "round";
static CItemMath<str_round> ANON;

extern const char str_sin[] = "sin";
static CItemMath<str_sin> ANON;

extern const char str_cos[] = "cos";
static CItemMath<str_cos> ANON;

extern const char str_acos[] = "acos";
static CItemMath<str_acos> ANON;

extern const char str_pow[] = "pow";
static CItemMath<str_pow> ANON;

extern const char str_log[] = "log";
static CItemMath<str_log> ANON;

extern const char str_radians[] = "radians";
static CItemMath<str_radians> ANON;

extern const char str_if[] = "if";
static class ANON : public CItemSubtypeFN<Item_func_if, str_if> {
    virtual void do_analyze_type(Item_func_if *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        analyze(args[0], cipher_type_reason(cipher_type::plain, "if_cond", i, &tr));
        analyze(args[1], tr);
        analyze(args[2], tr);
    }
} ANON;

extern const char str_nullif[] = "nullif";
static class ANON : public CItemSubtypeFN<Item_func_nullif, str_nullif> {
    virtual void do_analyze_type(Item_func_nullif *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], cipher_type_reason(cipher_type::equal, "nullif", i, &tr));
    }
} ANON;

extern const char str_coalesce[] = "coalesce";
static class ANON : public CItemSubtypeFN<Item_func_coalesce, str_coalesce> {
    virtual void do_analyze_type(Item_func_coalesce *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], tr);
    }
} ANON;

extern const char str_case[] = "case";
static class ANON : public CItemSubtypeFN<Item_func_case, str_case> {
    virtual void do_analyze_type(Item_func_case *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        int first_expr_num = (*i).*rob<Item_func_case, int,
            &Item_func_case::first_expr_num>::ptr();
        int else_expr_num = (*i).*rob<Item_func_case, int,
            &Item_func_case::else_expr_num>::ptr();
        uint ncases = (*i).*rob<Item_func_case, uint,
            &Item_func_case::ncases>::ptr();

        if (first_expr_num >= 0)
            analyze(args[first_expr_num],
                    cipher_type_reason(cipher_type::equal, "case_first", i, &tr));
        if (else_expr_num >= 0)
            analyze(args[else_expr_num], tr);

        for (uint x = 0; x < ncases; x += 2) {
            if (first_expr_num < 0)
                analyze(args[x],
                        cipher_type_reason(cipher_type::plain, "case_nofirst", i, &tr));
            else
                analyze(args[x],
                        cipher_type_reason(cipher_type::equal, "case_w/first", i, &tr));
            analyze(args[x+1], tr);
        }
    }
} ANON;

template<const char *NAME>
class CItemStrconv : public CItemSubtypeFN<Item_str_conv, NAME> {
    virtual void do_analyze_type(Item_str_conv *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], cipher_type_reason(cipher_type::plain, "strconv", i, &tr));
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

template<const char *NAME>
class CItemLeafFunc : public CItemSubtypeFN<Item_func, NAME> {
    virtual void do_analyze_type(Item_func *i, const cipher_type_reason &tr) const {}
};

extern const char str_found_rows[] = "found_rows";
static CItemLeafFunc<str_found_rows> ANON;

extern const char str_last_insert_id[] = "last_insert_id";
static CItemLeafFunc<str_last_insert_id> ANON;

extern const char str_rand[] = "rand";
static CItemLeafFunc<str_rand> ANON;

static class ANON : public CItemSubtypeFT<Item_extract, Item_func::Functype::EXTRACT_FUNC> {
    virtual void do_analyze_type(Item_extract *i, const cipher_type_reason &tr) const {
        /* XXX perhaps too conservative */
        analyze(i->arguments()[0], cipher_type_reason(cipher_type::plain, "extract", i, &tr));
    }
} ANON;

template<const char *NAME>
class CItemDateExtractFunc : public CItemSubtypeFN<Item_int_func, NAME> {
    virtual void do_analyze_type(Item_int_func *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++) {
            /* assuming we separately store different date components */
            analyze(args[x], tr);
        }
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

extern const char str_dayofmonth[] = "dayofmonth";
static CItemDateExtractFunc<str_dayofmonth> ANON;

extern const char str_unix_timestamp[] = "unix_timestamp";
static CItemDateExtractFunc<str_unix_timestamp> ANON;

extern const char str_date_add_interval[] = "date_add_interval";
static class ANON : public CItemSubtypeFN<Item_date_add_interval, str_date_add_interval> {
    virtual void do_analyze_type(Item_date_add_interval *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++) {
            /* XXX perhaps too conservative */
            analyze(args[x], cipher_type_reason(cipher_type::plain, "date_add", i, &tr));
        }
    }
} ANON;

template<const char *NAME>
class CItemDateNow : public CItemSubtypeFN<Item_func_now, NAME> {
    virtual void do_analyze_type(Item_func_now *i, const cipher_type_reason &tr) const {}
};

extern const char str_now[] = "now";
static CItemDateNow<str_now> ANON;

extern const char str_utc_timestamp[] = "utc_timestamp";
static CItemDateNow<str_utc_timestamp> ANON;

extern const char str_sysdate[] = "sysdate";
static CItemDateNow<str_sysdate> ANON;

template<const char *NAME>
class CItemBitfunc : public CItemSubtypeFN<Item_func_bit, NAME> {
    virtual void do_analyze_type(Item_func_bit *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], cipher_type_reason(cipher_type::plain, "bitfunc", i, &tr));
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
    virtual void do_analyze_type(Item_func_like *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        if (args[1]->type() == Item::Type::STRING_ITEM) {
            string s(args[1]->str_value.ptr(), args[1]->str_value.length());
            if (s.find('%') == s.npos && s.find('_') == s.npos) {
                /* some queries actually use LIKE as an equality check.. */
                analyze(args[0], cipher_type_reason(cipher_type::equal, "like-eq", i, &tr));
            } else {
                /* XXX check if pattern is one we can support? */
                stringstream ss;
                ss << "like:'" << s << "'";
                analyze(args[0], cipher_type_reason(cipher_type::like, ss.str(), i, &tr));
            }
        } else {
            /* we cannot support non-constant search patterns */
            for (uint x = 0; x < i->argument_count(); x++)
                analyze(args[x], cipher_type_reason(cipher_type::plain, "like-non-const", i, &tr));
        }
    }
} ANON;

static class ANON : public CItemSubtypeFT<Item_func, Item_func::Functype::FUNC_SP> {
    void error(Item_func *i) const __attribute__((noreturn)) {
        thrower() << "unsupported store procedure call " << *i;
    }

    virtual void do_analyze_type(Item_func *i, const cipher_type_reason &tr) const __attribute__((noreturn)) { error(i); }
} ANON;

static class ANON : public CItemSubtypeFT<Item_func_in, Item_func::Functype::IN_FUNC> {
    virtual void do_analyze_type(Item_func_in *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], cipher_type_reason(cipher_type::equal, "in", i, &tr));
    }
} ANON;

static class ANON : public CItemSubtypeFT<Item_func_in, Item_func::Functype::BETWEEN> {
    virtual void do_analyze_type(Item_func_in *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], cipher_type_reason(cipher_type::order, "between", i, &tr));
    }
} ANON;

template<const char *FN>
class CItemMinMax : public CItemSubtypeFN<Item_func_min_max, FN> {
    virtual void do_analyze_type(Item_func_min_max *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], cipher_type_reason(cipher_type::order, "min/max", i, &tr));
    }
};

extern const char str_greatest[] = "greatest";
static CItemMinMax<str_greatest> ANON;

extern const char str_least[] = "least";
static CItemMinMax<str_least> ANON;

extern const char str_strcmp[] = "strcmp";
static class ANON : public CItemSubtypeFN<Item_func_strcmp, str_strcmp> {
    virtual void do_analyze_type(Item_func_strcmp *i, const cipher_type_reason &tr) const {
        Item **args = i->arguments();
        for (uint x = 0; x < i->argument_count(); x++)
            analyze(args[x], cipher_type_reason(cipher_type::equal, "strcmp", i, &tr));
    }
} ANON;

template<Item_sum::Sumfunctype SFT>
class CItemCount : public CItemSubtypeST<Item_sum_count, SFT> {
    virtual void do_analyze_type(Item_sum_count *i, const cipher_type_reason &tr) const {
        if (i->has_with_distinct())
            analyze(i->get_arg(0), cipher_type_reason(cipher_type::equal, "sum", i, &tr, false));
    }
};

static CItemCount<Item_sum::Sumfunctype::COUNT_FUNC> ANON;
static CItemCount<Item_sum::Sumfunctype::COUNT_DISTINCT_FUNC> ANON;

template<Item_sum::Sumfunctype SFT>
class CItemChooseOrder : public CItemSubtypeST<Item_sum_hybrid, SFT> {
    virtual void do_analyze_type(Item_sum_hybrid *i, const cipher_type_reason &tr) const {
        analyze(i->get_arg(0), cipher_type_reason(cipher_type::order, "min/max_agg", i, &tr, false));
    }
};

static CItemChooseOrder<Item_sum::Sumfunctype::MIN_FUNC> ANON;
static CItemChooseOrder<Item_sum::Sumfunctype::MAX_FUNC> ANON;

template<Item_sum::Sumfunctype SFT>
class CItemSum : public CItemSubtypeST<Item_sum_sum, SFT> {
    virtual void do_analyze_type(Item_sum_sum *i, const cipher_type_reason &tr) const {
        if (i->has_with_distinct())
            analyze(i->get_arg(0), cipher_type_reason(cipher_type::equal, "agg_distinct", i, &tr, false));
        if (tr.t == cipher_type::any || tr.t == cipher_type::homadd)
            analyze(i->get_arg(0), cipher_type_reason(cipher_type::homadd, "sum/avg", i, &tr, false));
        else
            analyze(i->get_arg(0), cipher_type_reason(cipher_type::plain, "sum/avg_x", i, &tr, false));
    }
};

static CItemSum<Item_sum::Sumfunctype::SUM_FUNC> ANON;
static CItemSum<Item_sum::Sumfunctype::SUM_DISTINCT_FUNC> ANON;
static CItemSum<Item_sum::Sumfunctype::AVG_FUNC> ANON;
static CItemSum<Item_sum::Sumfunctype::AVG_DISTINCT_FUNC> ANON;

static class ANON : public CItemSubtypeST<Item_sum_bit, Item_sum::Sumfunctype::SUM_BIT_FUNC> {
    virtual void do_analyze_type(Item_sum_bit *i, const cipher_type_reason &tr) const {
        analyze(i->get_arg(0), cipher_type_reason(cipher_type::plain, "bitagg", i, &tr, false));
    }
} ANON;

static class ANON : public CItemSubtypeST<Item_func_group_concat, Item_sum::Sumfunctype::GROUP_CONCAT_FUNC> {
    virtual void do_analyze_type(Item_func_group_concat *i, const cipher_type_reason &tr) const {
        uint arg_count_field = (*i).*rob<Item_func_group_concat, uint,
            &Item_func_group_concat::arg_count_field>::ptr();
        for (uint x = 0; x < arg_count_field; x++) {
            /* XXX could perform in the proxy.. */
            analyze(i->get_arg(x), cipher_type_reason(cipher_type::plain, "group_concat", i, &tr));
        }

        /* XXX order, unused in trace queries.. */
    }
} ANON;

static class ANON : public CItemSubtypeFT<Item_char_typecast, Item_func::Functype::CHAR_TYPECAST_FUNC> {
    virtual void do_analyze_type(Item_char_typecast *i, const cipher_type_reason &tr) const {
        thrower() << "what does Item_char_typecast do?";
    }
} ANON;

extern const char str_cast_as_signed[] = "cast_as_signed";
static class ANON : public CItemSubtypeFN<Item_func_signed, str_cast_as_signed> {
    virtual void do_analyze_type(Item_func_signed *i, const cipher_type_reason &tr) const {
        analyze(i->arguments()[0], tr);
    }
} ANON;

static class ANON : public CItemSubtypeIT<Item_ref, Item::Type::REF_ITEM> {
    virtual void do_analyze_type(Item_ref *i, const cipher_type_reason &tr) const {
        if (i->ref) {
            analyze(*i->ref, tr);
        } else {
            thrower() << "how to resolve Item_ref::ref?";
        }
    }
} ANON;


/*
 * Some helper functions.
 */
static void
process_select_lex(st_select_lex *select_lex, const cipher_type_reason &tr)
{
    auto item_it = List_iterator<Item>(select_lex->item_list);
    for (;;) {
        Item *item = item_it++;
        if (!item)
            break;

        analyze(item, tr);
    }

    if (select_lex->where) {
	cerr << "going in where! \n";
        analyze(select_lex->where, cipher_type_reason(cipher_type::plain, "where", select_lex->where, 0));
    }
    if (select_lex->having)
        analyze(select_lex->having, cipher_type_reason(cipher_type::plain, "having", select_lex->having, 0));

    for (ORDER *o = select_lex->group_list.first; o; o = o->next)
        analyze(*o->item, cipher_type_reason(cipher_type::equal, "group", *o->item, 0));

    for (ORDER *o = select_lex->order_list.first; o; o = o->next)
        analyze(*o->item, cipher_type_reason(cipher_type::order,
                "order", *o->item, 0, select_lex->select_limit ? false : true));
}

static void
process_table_list(List<TABLE_LIST> *tll)
{
    /*
     * later, need to rewrite different joins, e.g.
     * SELECT g2_ChildEntity.g_id, IF(ai0.g_id IS NULL, 1, 0) AS albumsFirst, g2_Item.g_originationTimestamp FROM g2_ChildEntity LEFT JOIN g2_AlbumItem AS ai0 ON g2_ChildEntity.g_id = ai0.g_id INNER JOIN g2_Item ON g2_ChildEntity.g_id = g2_Item.g_id INNER JOIN g2_AccessSubscriberMap ON g2_ChildEntity.g_id = g2_AccessSubscriberMap.g_itemId ...
     */

    List_iterator<TABLE_LIST> join_it(*tll);
    for (;;) {
        TABLE_LIST *t = join_it++;
        if (!t)
            break;

        if (t->nested_join) {
            process_table_list(&t->nested_join->join_list);
            return;
        }

        if (t->on_expr)
            analyze(t->on_expr, cipher_type_reason(cipher_type::plain, "join_cond", t->on_expr, 0));

        std::string db(t->db, t->db_length);
        std::string table_name(t->table_name, t->table_name_length);
        std::string alias(t->alias);

        if (t->derived) {
            st_select_lex_unit *u = t->derived;
            /*
             * Not quite right, in terms of softness:
             * should really come from the items that eventually
             * reference columns in this derived table.
             */
            process_select_lex(u->first_select(), cipher_type_reason(cipher_type::any, "sub-select", 0, 0, false));
        }
    }
}

static void
query_analyze(const std::string &db, const std::string &q)
{
    query_parse p(db, q);
    LEX *lex = p.lex();

    // iterate over the entire select statement..
    // based on st_select_lex::print in mysql-server/sql/sql_select.cc
    process_table_list(&lex->select_lex.top_join_list);
    process_select_lex(&lex->select_lex,
                       cipher_type_reason(
                            lex->sql_command == SQLCOM_SELECT ? cipher_type::any
                                                              : cipher_type::none,
                            "select", 0, 0, true));

    if (lex->sql_command == SQLCOM_UPDATE) {
        auto item_it = List_iterator<Item>(lex->value_list);
        for (;;) {
            Item *item = item_it++;
            if (!item)
                break;
            analyze(item, cipher_type_reason(cipher_type::any, "update", item, 0, false));
        }
    }
}

static string
unescape(string s)
{
    stringstream ss;

    for (;;) {
        size_t bs = s.find_first_of('\\');
        if (bs == s.npos)
            break;

        ss << s.substr(0, bs);
        s = s.substr(bs+1);

        if (s.size() == 0)
            break;
        if (s[0] == 'x' && s.size() >= 3) {
            stringstream hs(s.substr(1, 2));
            int v;
            hs >> hex >> v;
            ss << (char) v;
            s = s.substr(3);
        } else {
            ss << s[0];
            s = s.substr(1);
        }
    }
    ss << s;

    return ss.str();
}

int
main(int ac, char **av)
{
    if (ac != 3) {
        cerr << "Usage: " << av[0] << " schema-db trace-file" << endl;
        exit(1);
    }

    char dir_arg[1024];
    snprintf(dir_arg, sizeof(dir_arg), "--datadir=%s", av[1]);

    const char *mysql_av[] =
        { "progname",
          "--skip-grant-tables",
          dir_arg,
          /* "--skip-innodb", */
          /* "--default-storage-engine=MEMORY", */
          "--character-set-server=utf8",
          "--language=" MYSQL_BUILD_DIR "/sql/share/"
        };
    assert(0 == mysql_server_init(sizeof(mysql_av) / sizeof(mysql_av[0]),
                                  (char**) mysql_av, 0));
    assert(0 == mysql_thread_init());

    ifstream f(av[2]);
    int nquery = 0;
    int nerror = 0;
    int nskip = 0;

    for (;;) {
        string s;
        getline(f, s);
        if (f.eof())
            break;

        size_t space = s.find_first_of(' ');
        if (space == s.npos) {
            cerr << "malformed " << s << endl;
            continue;
        }

        string db = s.substr(0, space);
        string q = s.substr(space + 1);

        if (db == "") {
            nskip++;
        } else {
            string unq = unescape(q);
            try {
                query_analyze(db, unq);
            } catch (std::runtime_error &e) {
                cout << "ERROR: " << e.what() << " in query " << unq << endl;
                nerror++;
            }
        }

        nquery++;
        if (!(nquery % 100))
            cout << " nquery: " << nquery
                 << " nerror: " << nerror
                 << " nskip: " << nskip
                 << endl;
    }
}
