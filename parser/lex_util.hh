#pragma once

#include <string>
#include <parser/sql_utils.hh>
#include <util/rob.hh>

#include <sql_select.h>
#include <sql_delete.h>
#include <sql_insert.h>
#include <sql_update.h>

/*
 * lex_util.hh
 *
 * Helper methods for rewriting parts of lex.
 *
 *
 *  Approach for rewriting any data structure of the lex:
 * -- copy old data structure
 * -- replace any data fields that needs to change with their rewritten versions
 * -- update pointers in other parts of lex pointing to these
 * (hence, old and new data structure share items that need not change)
 */

//copies any data structure shallowly
template <typename T>
T *copyWithTHD(const T *const x) {
    return static_cast<T *>(current_thd->memdup(x, sizeof(T)));
}

/* Makes a new item based on
 * information from an old item */
Item_string *dup_item(const Item_string &i);
Item_int *dup_item(const Item_int &i);
Item_null *dup_item(const Item_null &i);
Item_func *dup_item(const Item_func &i);
Item_decimal *dup_item(const Item_decimal &i);
Item *dup_item(const Item &i);
Item_float *dup_item(const Item_float &i);

Item_field *make_item_field(const Item_field &t,
                            const std::string &table_name = "",
                            const std::string &field_name = "");
Item_ref *make_item_ref(const Item_ref &t, Item *const new_ref,
                        const std::string &table_name = "",
                        const std::string &field_name = "");
Item_string *make_item_string(const std::string &s);
Item_insert_value *make_item_insert_value(const Item_insert_value &i,
                                          Item_field *const field);
ORDER *make_order(const ORDER *const old_order, Item *const i);

bool
isItem_insert_value(const Item &i);

// sets the select_lex in a lex
void
set_select_lex(LEX *const lex, SELECT_LEX *const select_lex);

void
set_where(st_select_lex *const sl, Item *const where);

void
set_having(st_select_lex *const sl, Item *const having);

namespace RiboldMYSQL {
    bool is_null(const Item &i);
    Item *clone_item(const Item &i);
    const List<Item> *argument_list(const Item_cond &i);
    uint get_arg_count(const Item_sum &i);
    const Item *get_arg(const Item_sum &item, uint i);
    Item_subselect::subs_type substype(const Item_subselect &i);
    const st_select_lex *get_select_lex(const Item_subselect &i);
    std::string val_str(const Item &i, bool *is_null);
    ulonglong val_uint(const Item &i);
    double val_real(const Item &i);

    template <typename T>
    class constList_iterator {
        List_iterator<T> iter;
    public:
        constList_iterator(const List<T> &list)
            : iter(List_iterator<T>(const_cast<List<T> &>(list))) {}
        const T* operator++(int)
        {
            return iter++;
        }
    };
};

// Creates a SQL_I_List that contains one element
template <typename T>
SQL_I_List<T> *
oneElemListWithTHD(T *const elem) {
    SQL_I_List<T> *const res = new (current_thd->mem_root) SQL_I_List<T>();
    res->elements = 1;
    res->first = elem;
    res->next = NULL;

    return res;
}

template <typename T>
List<T> *
dptrToListWithTHD(T **const es, unsigned int count)
{
    List<T> *const out = new (current_thd->mem_root) List<T>();
    for (unsigned int i = 0; i < count; ++i) {
        out->push_back(es[i]);
    }

    return out;
}

// Helper functions for doing functional things to List<T> structures.
template <typename Type> void
eachList(List_iterator<Type> it, std::function<void(Type *)> op)
{
    for (Type *element = it++; element ; element = it++) {
        op(element);
    }

    return;
}

template <typename InType, typename OutType> List<OutType>
mapList(List_iterator<InType> it, std::function<OutType *(InType *)> op)
{
    List<OutType> newList;
    for (InType *element; element ; element = it++) {
        newList.push_back(op(element));
    }

    return newList;
}

template <typename Type> List<Type>
accumList(List_iterator<Type> it,
          std::function<List<Type>(List<Type>, Type *)> op)
{
    List<Type> accum;

    for (Type *element = it++; element ; element = it++) {
        accum = op(accum, element);
    }

    return accum;
}

template <typename T> List<T> *
vectorToListWithTHD(std::vector<T *> v)
{
    List<T> *const lst = new (current_thd->mem_root) List<T>;
    for (auto it : v) {
        lst->push_back(it);
    }

    return lst;
}

template <typename Type> List<Type>
filterList(List_iterator<Type> it, std::function<bool(Type *)> op)
{
    List<Type> new_list;

    for (Type *element = it++; element ; element = it++) {
        if (true == op(element)) {
            new_list.push_back(element);
        }
    }

    return new_list;
}

