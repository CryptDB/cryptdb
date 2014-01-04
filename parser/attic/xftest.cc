#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>

#include <stdio.h>

#include <errstream.hh>
#include <cleanup.hh>
#include "mysql_glue.hh"
#include "stringify.hh"

#include "sql_priv.h"
#include "unireg.h"
#include "strfunc.h"
#include "sql_class.h"
#include "set_var.h"
#include "sql_base.h"
#include "rpl_handler.h"
#include "sql_parse.h"
#include "sql_plugin.h"
#include "derror.h"
#include "item.h"

enum class Cipher   { AES, OPE, Paillier, SWP };
enum class DataType { integer, string };
std::set<Cipher> pkCiphers = { Cipher::Paillier };

struct EncKey {
 public:
    EncKey(int i) : id(i) {}
    int id;     // XXX not right, but assume 0 is any key
};

class EncType {
 public:
    EncType(bool c = false) : isconst(c) {}
    EncType(std::vector<std::pair<Cipher, EncKey> > o)
        : onion(o), isconst(false) {}

    std::vector<std::pair<Cipher, EncKey> > onion;  // last is outermost
    bool isconst;

    /* Find a common enctype between two onions */
    bool match(const EncType &other, EncType *out) const {
        std::vector<const EncType*> v = { this, &other };
        std::sort(v.begin(), v.end(), [](const EncType *a, const EncType *b) {
                  return a->onion.size() > b->onion.size(); });

        const EncType *a = v[0];    // longer, if any
        const EncType *b = v[1];
        EncType res;

        auto ai = a->onion.begin();
        auto ae = a->onion.end();
        auto bi = b->onion.begin();
        auto be = b->onion.end();

        for (; ai != ae; ai++, bi++) {
            if (bi == be) {
                if (a->isconst || pkCiphers.find(ai->first) != pkCiphers.end()) {
                    res.onion.push_back(*ai);   // can add layers
                } else {
                    return false;
                }
            } else {
                if (ai->first != bi->first)
                    return false;

                struct EncKey k = ai->second;
                if (k.id == 0) {
                    k = bi->second;
                } else {
                    if (k.id != bi->second.id)
                        return false;
                }

                res.onion.push_back(make_pair(ai->first, k));
            }
        }

        *out = res;
        return true;
    }

    bool match(const std::vector<EncType> &encs, EncType *out) const {
        for (auto i = encs.begin(); i != encs.end(); i++)
            if (match(*i, out))
                return true;
        return false;
    }
};

class ColType {
 public:
    ColType(DataType dt) : type(dt) {}
    ColType(DataType dt, std::vector<EncType> e) : type(dt), encs(e) {}

    DataType type;
    std::vector<EncType> encs;
};

class CItemType {
 public:
    virtual Item *do_rewrite(Item *) const = 0;
    virtual ColType do_enctype(Item *) const = 0;
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

    Item *do_rewrite(Item *i) const {
        return lookup(i)->do_rewrite(i);
    }

    ColType do_enctype(Item *i) const {
        return lookup(i)->do_enctype(i);
    }

 protected:
    virtual CItemType *lookup(Item *i) const = 0;

    CItemType *do_lookup(Item *i, T t, const char *errname) const {
        auto x = types.find(t);
        if (x == types.end())
            thrower() << "missing " << errname << " " << t;
        return x->second;
    }

 private:
    std::map<T, CItemType*> types;
};

static class CItemBaseDir : public CItemTypeDir<Item::Type> {
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

static class CItemFuncNameDir : public CItemTypeDir<std::string> {
    CItemType *lookup(Item *i) const {
        return do_lookup(i, ((Item_func *) i)->func_name(), "func name");
    }
 public:
    CItemFuncNameDir() { funcTypes.reg(Item_func::Functype::UNKNOWN_FUNC, this); }
} funcNames;

/*
 * Helper functions to look up via directory & invoke method.
 */
static Item *
rewrite(Item *i)
{
    Item *n = itemTypes.do_rewrite(i);
    n->name = i->name;
    return n;
}

static ColType
enctype(Item *i)
{
    return itemTypes.do_enctype(i);
}

/*
 * CItemType classes for supported Items: supporting machinery.
 */
template<class T>
class CItemSubtype : public CItemType {
    virtual Item *do_rewrite(Item *i) const { return do_rewrite((T*) i); }
    virtual ColType do_enctype(Item *i) const { return do_enctype((T*) i); }
 private:
    virtual Item *do_rewrite(T *) const = 0;
    virtual ColType do_enctype(T *) const = 0;
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

template<class T, const char *TYPE>
class CItemSubtypeFN : public CItemSubtype<T> {
 public:
    CItemSubtypeFN() { funcNames.reg(std::string(TYPE), this); }
};

/*
 * Actual item handlers.
 */
static class CItemField : public CItemSubtypeIT<Item_field, Item::Type::FIELD_ITEM> {
    Item *do_rewrite(Item_field *i) const {
        return
            new Item_field(0,
                           i->db_name,
                           strdup((std::string("anontab_") +
                                   i->table_name).c_str()),
                           strdup((std::string("anonfld_") +
                                   i->field_name).c_str()));
    }

    ColType do_enctype(Item_field *i) const {
        /* XXX
         * need to look up current schema state.
         * return one EncType for each onion level
         * that can be exposed for this column.
         */
        return ColType(DataType::string, {
                        EncType({ make_pair(Cipher::AES, 123) }),
                        EncType({ make_pair(Cipher::OPE, 124),
                                  make_pair(Cipher::AES, 125) }),
                        EncType({ make_pair(Cipher::Paillier, 126) }),
                       });
    }
} ANON;

static class CItemString : public CItemSubtypeIT<Item_string, Item::Type::STRING_ITEM> {
    Item *do_rewrite(Item_string *i) const {
        std::string s("ENCRYPTED:");
        s += std::string(i->str_value.ptr(), i->str_value.length());
        return new Item_string(strdup(s.c_str()), s.size(),
                               i->str_value.charset());
    }

    ColType do_enctype(Item_string *i) const {
        return ColType(DataType::string, {EncType(true)});
    }
} ANON;

static class CItemInt : public CItemSubtypeIT<Item_num, Item::Type::INT_ITEM> {
    Item *do_rewrite(Item_num *i) const {
        return new Item_int(i->val_int() + 1000);
    }

    ColType do_enctype(Item_num *i) const {
        return ColType(DataType::integer, {EncType(true)});
    }
} ANON;

static class CItemSubselect : public CItemSubtypeIT<Item_subselect, Item::Type::SUBSELECT_ITEM> {
    Item *do_rewrite(Item_subselect *i) const {
        // XXX handle sub-selects
        return i;
    }

    ColType do_enctype(Item_subselect *i) const {
        /* XXX */
        return DataType::integer;
    }
} ANON;

template<Item_func::Functype FT, class IT>
class CItemCompare : public CItemSubtypeFT<Item_func, FT> {
    Item *do_rewrite(Item_func *i) const {
        Item **args = i->arguments();
        return new IT(rewrite(args[0]), rewrite(args[1]));
    }

    ColType do_enctype(Item_func *i) const { return DataType::integer; }
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
    Item *do_rewrite(Item_cond *i) const {
        List<Item> *arglist = i->argument_list();
        List<Item> newlist;

        auto it = List_iterator<Item>(*arglist);
        for (;; ) {
            Item *argitem = it++;
            if (!argitem)
                break;

            newlist.push_back(rewrite(argitem));
        }

        return new IT(newlist);
    }

    ColType do_enctype(Item_cond *i) const { return DataType::integer; }
};

static CItemCond<Item_func::Functype::COND_AND_FUNC, Item_cond_and> ANON;
static CItemCond<Item_func::Functype::COND_OR_FUNC,  Item_cond_or>  ANON;

extern const char str_plus[] = "+";
static class CItemPlus : public CItemSubtypeFN<Item_func, str_plus> {
    Item *do_rewrite(Item_func *i) const {
        Item **args = i->arguments();
        return new Item_func_plus(rewrite(args[0]), rewrite(args[1]));
    }

    ColType do_enctype(Item_func *i) const {
        Item **args = i->arguments();
        ColType t0 = enctype(args[0]);
        ColType t1 = enctype(args[0]);

        if (t0.type != DataType::integer || t1.type != DataType::integer)
            thrower() << "non-integer plus " << *args[0] << ", " << *args[1];

        EncType e;
        e.onion = {};
        if (e.match(t0.encs, &e) && e.match(t1.encs, &e))
            return ColType(DataType::integer, {e});

        e.onion = { make_pair(Cipher::Paillier, 0) };
        if (e.match(t0.encs, &e) && e.match(t1.encs, &e))
            return ColType(DataType::integer, {e});

        thrower() << "no common HOM type: " << *args[0] << ", " << *args[1];
    }
} ANON;

static class CItemLike : public CItemSubtypeFT<Item_func_like, Item_func::Functype::LIKE_FUNC> {
    Item *do_rewrite(Item_func_like *i) const {
        return i;
    }

    ColType do_enctype(Item_func_like *i) const { return DataType::integer; }
} ANON;

static class CItemSP : public CItemSubtypeFT<Item_func, Item_func::Functype::FUNC_SP> {
    void error(Item_func *i) const __attribute__((noreturn)) {
        thrower() << "unsupported store procedure call " << *i;
    }

    Item *do_rewrite(Item_func *i) const __attribute__((noreturn)) { error(i); }
    ColType do_enctype(Item_func *i) const __attribute__((noreturn)) { error(i); }
} ANON;

/*
 * Test harness.
 */
static void
xftest(void)
{
    THD *t = new THD;
    if (t->store_globals())
        printf("store_globals error\n");

    const char *q =
        "SELECT x.a, y.b + 2, y.c, y.cc AS ycc "
        "FROM x, y as yy1, y as yy2, (SELECT x, y FROM z WHERE q=7) as subt "
        "WHERE x.bb = yy1.b AND yy1.k1 = yy2.k2 AND "
        "(yy2.d > 7 OR yy2.e = (3+4)) AND (yy1.f='hello') AND "
        "yy2.cc = 9 AND yy2.gg = (SELECT COUNT(*) FROM xxc) AND "
        "yy2.ss LIKE '%foo%'";
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", q);
    size_t len = strlen(buf);

    alloc_query(t, buf, len + 1);

    Parser_state ps;
    if (!ps.init(t, buf, len)) {
        LEX lex;
        t->lex = &lex;

        lex_start(t);
        mysql_reset_thd_for_next_command(t);

        string db = "current_db";
        t->set_db(db.data(), db.length());

        printf("input query: %s\n", buf);
        bool error = parse_sql(t, &ps, 0);
        if (error) {
            printf("parse error: %d %d %d\n", error, t->is_fatal_error,
                   t->is_error());
            printf("parse error: h %p\n", t->get_internal_handler());
            printf("parse error: %d %s\n", t->is_error(), t->stmt_da->message());
        } else {
            //printf("command %d\n", lex.sql_command);

            // iterate over the entire select statement..
            // based on st_select_lex::print in mysql-server/sql/sql_select.cc

            // iterate over the items that select will actually return
            List<Item> new_item_list;
            auto item_it = List_iterator<Item>(lex.select_lex.item_list);
            for (;; ) {
                Item *item = item_it++;
                if (!item)
                    break;

                Item *newitem = rewrite(item);
                new_item_list.push_back(newitem);
            }
            lex.select_lex.item_list = new_item_list;

            auto join_it = List_iterator<TABLE_LIST>(
                lex.select_lex.top_join_list);
            List<TABLE_LIST> new_join_list;
            for (;; ) {
                TABLE_LIST *t = join_it++;
                if (!t)
                    break;

                TABLE_LIST *nt = new TABLE_LIST();
                std::string db(t->db, t->db_length);
                std::string table_name(t->table_name, t->table_name_length);
                std::string alias(t->alias);
                table_name = "anontab_" + table_name;
                alias = "anontab_" + alias;

                // XXX handle sub-selects..
                nt->init_one_table(strdup(db.c_str()), db.size(),
                                   strdup(
                                       table_name.c_str()), table_name.size(),
                                   strdup(alias.c_str()), t->lock_type);
                new_join_list.push_back(nt);
            }
            lex.select_lex.top_join_list = new_join_list;
            if (lex.select_lex.where)
                lex.select_lex.where = rewrite(lex.select_lex.where);

            cout << "output query: " << lex << endl;
        }

        t->end_statement();
    } else {
        printf("parser init error\n");
    }

    t->cleanup_after_query();
    delete t;
}

int
main(int ac, char **av)
{
    mysql_glue_init();
    xftest();
}
