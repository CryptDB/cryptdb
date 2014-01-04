#pragma once

/*
 * rewrite_main.hh
 *
 *
 *  TODO: need to integrate it with util.h: some declarations are repeated
 */

#include <map>

#include <main/Translator.hh>
#include <main/Connect.hh>
#include <main/dispatcher.hh>

#include <sql_select.h>
#include <sql_delete.h>
#include <sql_insert.h>
#include <sql_update.h>

#include "field.h"

#include <main/Analysis.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <parser/Annotation.hh>
#include <parser/stringify.hh>
#include <parser/lex_util.hh>

#include <util/errstream.hh>
#include <util/cleanup.hh>
#include <util/rob.hh>

extern std::string global_crash_point;

void
crashTest(const std::string &current_point);

class FieldReturned {
public:
    bool encrypted;
    bool includeInResult;
    std::string key;
    unsigned int SaltIndex;
    std::string nameForReturn;
};

void
printRes(const ResType & r);

//contains the results of a query rewrite:
// - rewritten queries
// - data structure needed to decrypt results
class QueryRewrite {
public:
    QueryRewrite(bool wasRes, ReturnMeta rmeta, RewriteOutput *output)
        : rmeta(rmeta), output(std::unique_ptr<RewriteOutput>(output)) {}
    QueryRewrite(QueryRewrite &&other_qr) : rmeta(other_qr.rmeta),
        output(std::move(other_qr.output)) {}
    const ReturnMeta rmeta;
    std::unique_ptr<RewriteOutput> output;
};

// Main class processing rewriting
class Rewriter {
    Rewriter();
    ~Rewriter();

public:

    static QueryRewrite
        rewrite(const ProxyState &ps, const std::string &q,
                SchemaInfo const &schema, const std::string &default_db);
    static ResType
        decryptResults(const ResType &dbres, const ReturnMeta &rm);

private:
    static RewriteOutput *
        dispatchOnLex(Analysis &a, const ProxyState &ps,
                      const std::string &query);
    static RewriteOutput *
        handleDirective(Analysis &a, const ProxyState &ps,
                        const std::string &query);

    static const bool translator_dummy;
    static const std::unique_ptr<SQLDispatcher> dml_dispatcher;
    static const std::unique_ptr<SQLDispatcher> ddl_dispatcher;
};

class SchemaCache;
class EpilogueResult;
EpilogueResult
executeQuery(const ProxyState &ps, const std::string &q,
             const std::string &default_db,
             SchemaCache *const schema_cache, bool pp=true);

#define UNIMPLEMENTED \
        throw std::runtime_error(std::string("Unimplemented: ") + \
                        std::string(__PRETTY_FUNCTION__))

class reason;
class OLK;

class CItemType {
 public:
    virtual RewritePlan *do_gather(const Item &, Analysis &)
        const = 0;
    virtual Item *do_optimize(Item *, Analysis &) const = 0;
    virtual Item *do_rewrite(const Item &, const OLK &constr,
                             const RewritePlan &rp, Analysis &)
        const = 0;
    virtual void do_rewrite_insert(const Item &, const FieldMeta &fm,
                                   Analysis &,
                                   std::vector<Item *> *) const = 0;
};

/*
 * CItemType classes for supported Items: supporting machinery.
 */
template<class T>
class CItemSubtype : public CItemType {
    virtual RewritePlan *do_gather(const Item &i, Analysis &a)
       const
    {
        return do_gather_type(static_cast<const T &>(i), a);
    }

    virtual Item *do_optimize(Item *i, Analysis & a) const
    {
        return do_optimize_type((T*) i, a);
    }

    virtual Item *do_rewrite(const Item &i, const OLK &constr,
                             const RewritePlan &rp,
                             Analysis &a) const
    {
        return do_rewrite_type(static_cast<const T &>(i), constr, rp, a);
    }

    virtual void do_rewrite_insert(const Item &i, const FieldMeta &fm,
                                   Analysis &a,
                                   std::vector<Item *> *l) const
    {
        do_rewrite_insert_type(static_cast<const T &>(i), fm, a, l);
    }

 private:
    virtual RewritePlan *do_gather_type(const T &i, Analysis &a) const
    {
        UNIMPLEMENTED;
    }

    virtual Item *do_optimize_type(T *i, Analysis & a) const
    {
        UNIMPLEMENTED;
        // do_optimize_const_item(i, a);
    }

    virtual Item *do_rewrite_type(const T &i, const OLK &constr,
                                  const RewritePlan &rp,
                                  Analysis &a) const
    {
        UNIMPLEMENTED;
    }

    virtual void do_rewrite_insert_type(const T &i, const FieldMeta &fm,
                                        Analysis &a,
                                        std::vector<Item *> *l) const
    {
        UNIMPLEMENTED;
    }
};



/*
 * Directories for locating an appropriate CItemType for a given Item.
 */
template <class T>
class CItemTypeDir : public CItemType {
 public:
    void reg(T t, const CItemType &ct)
    {
        auto x = types.find(t);
        if (x != types.end()) {
            thrower() << "duplicate key " << t << std::endl;
        }
        types.insert(std::make_pair(t, &ct));
    }

    RewritePlan *do_gather(const Item &i, Analysis &a) const
    {
        return lookup(i).do_gather(i, a);
    }

    Item* do_optimize(Item *i, Analysis &a) const
    {
        return lookup(*i).do_optimize(i, a);
    }

    Item *do_rewrite(const Item &i, const OLK &constr,
                     const RewritePlan &rp, Analysis &a) const
    {
        return lookup(i).do_rewrite(i, constr, rp, a);
    }

    void do_rewrite_insert(const Item &i, const FieldMeta &fm,
                           Analysis &a, std::vector<Item *> *l) const
    {
        lookup(i).do_rewrite_insert(i, fm, a, l);
    }

protected:
    virtual const CItemType &lookup(const Item &i) const = 0;

    const CItemType &do_lookup(const Item &i, const T &t,
                               const std::string &errname) const
    {
        auto x = types.find(t);
        if (x == types.end()) {
            thrower() << "missing " << errname << " " << t << " in "
                      << i << std::endl;
        }
        return *x->second;
    }

 private:
    std::map<T, const CItemType *const > types;
};

class CItemTypesDir : public CItemTypeDir<Item::Type> {
    const CItemType &lookup(const Item &i) const
    {
        return do_lookup(i, i.type(), "type");
    }
};

extern CItemTypesDir itemTypes;

class CItemFuncDir : public CItemTypeDir<Item_func::Functype> {
    const CItemType &lookup(const Item &i) const
    {
        return do_lookup(i, static_cast<const Item_func &>(i).functype(),
                         "func type");
    }

public:
    CItemFuncDir()
    {
        itemTypes.reg(Item::Type::FUNC_ITEM, *this);
        itemTypes.reg(Item::Type::COND_ITEM, *this);
    }
};

extern CItemFuncDir funcTypes;

class CItemSumFuncDir : public CItemTypeDir<Item_sum::Sumfunctype> {
    const CItemType &lookup(const Item &i) const
    {
        return do_lookup(i, static_cast<const Item_sum &>(i).sum_func(),
                         "sumfunc type");
    }

public:
    CItemSumFuncDir()
    {
        itemTypes.reg(Item::Type::SUM_FUNC_ITEM, *this);
    }
};

extern CItemSumFuncDir sumFuncTypes;


class CItemFuncNameDir : public CItemTypeDir<std::string> {
    const CItemType &lookup(const Item &i) const
    {
        return do_lookup(i, static_cast<const Item_func &>(i).func_name(),
                         "func name");
    }

public:
    CItemFuncNameDir() {
        funcTypes.reg(Item_func::Functype::UNKNOWN_FUNC, *this);
        funcTypes.reg(Item_func::Functype::NOW_FUNC, *this);
    }
};

extern CItemFuncNameDir funcNames;


template<class T, Item::Type TYPE>
class CItemSubtypeIT : public CItemSubtype<T> {
public:
    CItemSubtypeIT() { itemTypes.reg(TYPE, *this); }
};

template<class T, Item_func::Functype TYPE>
class CItemSubtypeFT : public CItemSubtype<T> {
public:
    CItemSubtypeFT() { funcTypes.reg(TYPE, *this); }
};

template<class T, Item_sum::Sumfunctype TYPE>
class CItemSubtypeST : public CItemSubtype<T> {
public:
    CItemSubtypeST() { sumFuncTypes.reg(TYPE, *this); }
};

template<class T, const char *TYPE>
class CItemSubtypeFN : public CItemSubtype<T> {
public:
    CItemSubtypeFN() { funcNames.reg(std::string(TYPE), *this); }
};

SchemaInfo *
loadSchemaInfo(const std::unique_ptr<Connect> &conn,
               const std::unique_ptr<Connect> &e_conn);

class OnionMetaAdjustor {
public:
    OnionMetaAdjustor(OnionMeta const &om) : original_om(om),
        duped_layers(pullCopyLayers(om)) {}
    ~OnionMetaAdjustor() {}

    EncLayer &getBackEncLayer() const;
    EncLayer &popBackEncLayer();
    SECLEVEL getSecLevel() const;
    const OnionMeta &getOnionMeta() const;
    std::string getAnonOnionName() const;

private:
    OnionMeta const &original_om;
    std::vector<EncLayer *> duped_layers;

    static std::vector<EncLayer *> pullCopyLayers(OnionMeta const &om);
};
