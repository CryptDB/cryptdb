/*
 * Handlers for rewriting aggregation functions, classes
 * inheriting from Item_sum, such as sum, max, min. 
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
#define ANON                ANON_NAME(__anon_id_sum_)

// rewrites the arguments of aggregators
// no_args specifies a certain number of arguments that I must have
// if negative, i can have any no. of arguments
static std::list<Item *>
rewrite_agg_args(const Item_sum &oldi, const OLK &constr,
                 const RewritePlanOneOLK &rp, Analysis &a,
                 int no_args = -1)
{
    if (no_args >= 0) {
        TEST_BadItemArgumentCount(oldi.type(), no_args,
                                  RiboldMYSQL::get_arg_count(oldi));
    } else {
        no_args = RiboldMYSQL::get_arg_count(oldi);
    }

    std::list<Item *> res = std::list<Item *>();
    for (int j = 0; j < no_args; j++) {
        const Item *const child_item = RiboldMYSQL::get_arg(oldi, j);
        Item *const out_child_item =
            itemTypes.do_rewrite(*child_item, rp.olk,
                                 *rp.childr_rp[j].get(), a);
        res.push_back(out_child_item);
    }

    return res;
}


template<Item_sum::Sumfunctype SFT>
class CItemCount : public CItemSubtypeST<Item_sum_count, SFT> {
    virtual RewritePlan *
    do_gather_type(const Item_sum_count &i, Analysis &a) const
    {
        const unsigned int arg_count =
            RiboldMYSQL::get_arg_count(i);
        TEST_BadItemArgumentCount(i.type(), 1, arg_count);
        const Item *const child = RiboldMYSQL::get_arg(i, 0);

        std::vector<std::shared_ptr<RewritePlan> >
            childr_rp({std::shared_ptr<RewritePlan>(gather(*child, a))});
        const EncSet needed = EQ_EncSet;
        const EncSet solution = childr_rp[0]->es_out.intersect(needed);

        std::string why = "count";
        if (i.has_with_distinct()) {
            why += " distinct";
            TEST_NoAvailableEncSet(solution, i.type(), needed, why,
                                   childr_rp);
        }

        const EncSet out_enc_set = PLAIN_EncSet;
        const reason rsn(out_enc_set, why, i);

        return new RewritePlanOneOLK(out_enc_set, solution.chooseOne(),
                                     childr_rp, rsn);
    }

    virtual Item *
    do_rewrite_type(const Item_sum_count &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        std::list<Item *> args =
            rewrite_agg_args(i, constr,
                             static_cast<const RewritePlanOneOLK &>(rp),
                             a, 1);
        auto out_item = new Item_sum_count(args.front());
        out_item->set_distinct(i.has_with_distinct());
        return out_item;
    }
};

static CItemCount<Item_sum::Sumfunctype::COUNT_FUNC> ANON;
static CItemCount<Item_sum::Sumfunctype::COUNT_DISTINCT_FUNC> ANON;



template<Item_sum::Sumfunctype SFT, class IT>
class CItemChooseOrder : public CItemSubtypeST<Item_sum_hybrid, SFT> {
    virtual RewritePlan *
    do_gather_type(const Item_sum_hybrid &i, Analysis &a) const
    {
        const unsigned int arg_count = RiboldMYSQL::get_arg_count(i);
        TEST_BadItemArgumentCount(i.type(), 1, arg_count);
        const Item *const child = RiboldMYSQL::get_arg(i, 0);

        std::vector<std::shared_ptr<RewritePlan> >
            childr_rp({std::shared_ptr<RewritePlan>(gather(*child, a))});
        const EncSet needed = ORD_EncSet;
        const EncSet supported = needed.intersect(childr_rp[0]->es_out);
        const std::string why = "min/max";
        TEST_NoAvailableEncSet(supported, i.type(), needed, why,
                               childr_rp);
        const OLK olk = supported.chooseOne();
        const EncSet out = EncSet(olk);
        const reason rsn(out, why, i);

        // INVESTIGATE: Should 'out' be 'supported'?
        return new RewritePlanOneOLK(out, olk, childr_rp, rsn);
    }

    virtual Item *
    do_rewrite_type(const Item_sum_hybrid &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        std::list<Item *> args =
            rewrite_agg_args(i, constr,
                             static_cast<const RewritePlanOneOLK &>(rp),
                             a, 1);
        return new IT(args.front());
    }
};

static CItemChooseOrder<Item_sum::Sumfunctype::MIN_FUNC, Item_sum_min> ANON;
static CItemChooseOrder<Item_sum::Sumfunctype::MAX_FUNC, Item_sum_max> ANON;

template<Item_sum::Sumfunctype SFT>
class CItemSum : public CItemSubtypeST<Item_sum_sum, SFT> {
    virtual RewritePlan *
    do_gather_type(const Item_sum_sum &i, Analysis &a) const
    {
        LOG(cdb_v) << "gather Item_sum_sum " << i << std::endl;

        const unsigned int arg_count = RiboldMYSQL::get_arg_count(i);
        TEST_BadItemArgumentCount(i.type(), 1, arg_count);
        const Item *const child_item = RiboldMYSQL::get_arg(i, 0);

        std::vector<std::shared_ptr<RewritePlan> >
            childr_rp({std::shared_ptr<RewritePlan>(gather(*child_item,
                                                           a))});

        if (i.has_with_distinct()) {
            UNIMPLEMENTED;
        }

        const EncSet my_es = ADD_EncSet;
        const EncSet solution = my_es.intersect(childr_rp[0]->es_out);
        const std::string why = "summation";
        TEST_NoAvailableEncSet(solution, i.type(), my_es, why,
                               childr_rp);

        const OLK olk = solution.chooseOne();
        const EncSet return_es = EncSet(olk);
        const reason rsn(return_es, why, i);

        return new RewritePlanOneOLK(return_es, olk, childr_rp, rsn);
    }

    virtual Item *
    do_rewrite_type(const Item_sum_sum &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        LOG(cdb_v) << "Item_sum_sum rewrite " << i << std::endl;

        std::list<Item *> args =
            rewrite_agg_args(i, constr,
                             static_cast<const RewritePlanOneOLK &>(rp),
                             a, 1);

        if (oAGG == constr.o) {
            OnionMeta *const om = constr.key->getOnionMeta(oAGG);
            assert(om);
            EncLayer const &el = a.getBackEncLayer(*om);
            TEST_UnexpectedSecurityLevel(oAGG, SECLEVEL::HOM,
                                         el.level());
            return static_cast<const HOM &>(el).sumUDA(args.front());
        } else {
            TEST_UnexpectedSecurityLevel(constr.o, SECLEVEL::PLAINVAL,
                                         constr.l);

            Item *const new_arg =
                RiboldMYSQL::clone_item(*RiboldMYSQL::get_arg(i, 0));
            return new Item_sum_sum(new_arg, i.has_with_distinct());
        }
    }
};

//TODO: field OPE should not be blob for text either

static CItemSum<Item_sum::Sumfunctype::SUM_FUNC> ANON;
static CItemSum<Item_sum::Sumfunctype::SUM_DISTINCT_FUNC> ANON;
static CItemSum<Item_sum::Sumfunctype::AVG_FUNC> ANON;
static CItemSum<Item_sum::Sumfunctype::AVG_DISTINCT_FUNC> ANON;

static class ANON : public CItemSubtypeST<Item_sum_bit, Item_sum::Sumfunctype::SUM_BIT_FUNC> {
    virtual RewritePlan *
    do_gather_type(const Item_sum_bit &i, Analysis &a) const
    {
        /* LOG(cdb_v) << "do_a_t Item_sum_bit reason " << tr;
            analyze(i->get_arg(0), reason(EMPTY_EncSet, "bitagg", i, &tr, false), a);
            return tr.encset;
        */
        UNIMPLEMENTED;
    }
} ANON;

static class ANON : public CItemSubtypeST<Item_func_group_concat, Item_sum::Sumfunctype::GROUP_CONCAT_FUNC> {
    virtual RewritePlan *
    do_gather_type(const Item_func_group_concat &i, Analysis &a) const
    {
        /*  LOG(cdb_v) << "do_a_t Item_func_group reason " << tr;
            uint arg_count_field = i->*rob<Item_func_group_concat, uint,
                    &Item_func_group_concat::arg_count_field>::ptr();
            for (uint x = 0; x < arg_count_field; x++) {
                // XXX could perform in the proxy..
                analyze(i->get_arg(x), reason(EMPTY_EncSet, "group_concat", i, &tr), a);
            }

            // XXX order, unused in trace queries..
            return tr.encset;
        */
        UNIMPLEMENTED;
    }
    // TODO(stephentu): figure out how to rob the arg fields for optimization
} ANON;

static class ANON : public CItemSubtypeIT<Item_ref, Item::Type::REF_ITEM> {
    virtual RewritePlan *
    do_gather_type(const Item_ref &i, Analysis &a) const
    {
        std::vector<std::shared_ptr<RewritePlan> >
            childr_rp({std::shared_ptr<RewritePlan>(gather(**i.ref, a))});

        const EncSet out_es = EncSet(childr_rp[0]->es_out);
        const EncSet child_es = childr_rp[0]->es_out;

        const std::string why = "ref_item";
        reason rsn(out_es, why, i);

        return new RewritePlanOneOLK(out_es, child_es.chooseOne(),
                                     childr_rp, rsn);
    }
    virtual Item *
    do_rewrite_type(const Item_ref &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        const std::string &db_name = a.getDatabaseName();
        // HACK.
        const std::string plain_table =
            static_cast<Item_field *>(*i.ref)->table_name;
        const std::string anon_table =
            a.getAnonTableName(db_name, plain_table);

        const std::string plain_field = i.field_name;
        OnionMeta const &om =
            a.getOnionMeta(db_name, plain_table, plain_field, constr.o);
        const std::string anon_field = om.getAnonOnionName();

        Item *const new_ref = itemTypes.do_rewrite(**i.ref, constr, rp, a);
        return make_item_ref(i, new_ref, anon_table, anon_field);
    }
} ANON;

static class ANON : public CItemSubtypeIT<Item_null, Item::Type::NULL_ITEM> {
    virtual RewritePlan *
    do_gather_type(const Item_null &i, Analysis &a) const
    {
        const std::string why = "is null";
        reason rsn(FULL_EncSet, why, i);
        return new RewritePlan(FULL_EncSet, rsn);
    }

    virtual Item *
    do_rewrite_type(const Item_null &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        return RiboldMYSQL::clone_item(i);
    }

    virtual void
    do_rewrite_insert_type(const Item_null &i, const FieldMeta &fm,
                           Analysis &a, std::vector<Item *> *l) const
    {
        for (uint j = 0; j < fm.children.size(); ++j) {
            l->push_back(RiboldMYSQL::clone_item(i));
        }
        if (fm.getHasSalt()) {
            const ulonglong salt = randomValue();
            l->push_back(new Item_int(static_cast<ulonglong>(salt)));
        }
    }
} ANON;

