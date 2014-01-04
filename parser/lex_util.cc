#include <parser/lex_util.hh>


using namespace std;

/* Functions for constructing/copying MySQL structures  */

Item_string *
dup_item(const Item_string &i)
{
    assert(i.type() == Item::Type::STRING_ITEM);
    const std::string s = ItemToString(i);
    return new (current_thd->mem_root) Item_string(make_thd_string(s),
                                                   s.length(),
                                                   i.default_charset());
}

Item_int *
dup_item(const Item_int &i)
{
    assert(i.type() == Item::Type::INT_ITEM);
    return new (current_thd->mem_root) Item_int(i.value);
}

Item_null *
dup_item(const Item_null &i)
{
    assert(i.type() == Item::Type::NULL_ITEM);
    return new (current_thd->mem_root) Item_null(i.name);
}

Item_func *
dup_item(const Item_func &i)
{
    assert(i.type() == Item::Type::FUNC_ITEM);
    switch (i.functype()) {
        case Item_func::Functype::NEG_FUNC:
            return new (current_thd->mem_root)
                Item_func_neg(i.arguments()[0]);
        default:
            cryptdb_err() << "Can't clone function type: " << i.type();
    }
}

Item_decimal *
dup_item(const Item_decimal &i)
{
    assert(i.type() == Item::Type::DECIMAL_ITEM);
    // FIXME: Memleak, should be allocated on THD.
    return static_cast<Item_decimal *>(const_cast<Item_decimal &>(i).clone_item());
}

Item_float *
dup_item(const Item_float &i)
{
    assert(i.type() == Item::Type::REAL_ITEM);
    return new (current_thd->mem_root) Item_float(i.name, i.value,
                                                i.decimals, i.max_length);
}

Item *
dup_item(const Item &i)
{
    switch (i.type()) {
        case Item::Type::STRING_ITEM:
            return dup_item(static_cast<const Item_string &>(i));
        case Item::Type::INT_ITEM:
            return dup_item(static_cast<const Item_int &>(i));
        case Item::Type::NULL_ITEM:
            return dup_item(static_cast<const Item_null &>(i));
        case Item::Type::FUNC_ITEM:
            return dup_item(static_cast<const Item_func &>(i));
        case Item::Type::DECIMAL_ITEM:
            return dup_item(static_cast<const Item_decimal &>(i));
        case Item::Type::REAL_ITEM:
            assert(i.field_type() == MYSQL_TYPE_DOUBLE);
            return dup_item(static_cast<const Item_float &>(i));
        default:
            throw CryptDBError("Unable to clone: " +
                               std::to_string(i.type()));
    }
}

static void
init_new_ident(Item_ident *const i, const std::string &table_name,
               const std::string &field_name)
{
    assert(!table_name.empty() && !field_name.empty());

    // clear out alias
    i->name = NULL;

    i->table_name = make_thd_string(table_name);
    i->field_name = make_thd_string(field_name);
}

Item_field *
make_item_field(const Item_field &i, const std::string &table_name,
                const std::string &field_name)
{
    assert(i.type() == Item::Type::FIELD_ITEM);

    assert(current_thd);

    Item_field *const i0 =
        new (current_thd->mem_root)
            Item_field(current_thd, &const_cast<Item_field &>(i));

    init_new_ident(i0, table_name, field_name);

    return i0;
}

Item_ref *
make_item_ref(const Item_ref &i, Item *const new_ref,
              const std::string &table_name,
              const std::string &field_name)
{
    assert(i.type() == Item::Type::REF_ITEM);
    assert(field_name.size() > 0 && table_name.size() > 0);

    assert(current_thd);

    Item_ref *const i0 =
        new (current_thd->mem_root)
            Item_ref(current_thd, &const_cast<Item_ref &>(i));

    // Don't try to use overloaded new here because we are allocating
    // a pointer and C++ will resolve the global placement new.
    i0->ref = static_cast<Item **>(current_thd->alloc(sizeof(Item **)));
    *i0->ref = new_ref;

    init_new_ident(i0, table_name, field_name);

    return i0;
}

Item_insert_value *
make_item_insert_value(const Item_insert_value &i,
                       Item_field *const field)
{
    assert(isItem_insert_value(i));
    return new (current_thd->mem_root)
               Item_insert_value(i.context, field);
}

Item_string *
make_item_string(const std::string &s)
{
    return new (current_thd->mem_root) Item_string(make_thd_string(s),
                                                   s.length(),
                                                Item::default_charset());
}

ORDER *
make_order(const ORDER *const old_order, Item *const i)
{
    ORDER *const new_order =
        static_cast<ORDER *>(current_thd->calloc(sizeof(ORDER)));
    memcpy(new_order, old_order, sizeof(ORDER));
    new_order->next = NULL;
    new_order->item_ptr = i;
    new_order->item =
        static_cast<Item **>(current_thd->calloc(sizeof(Item *)));
    *new_order->item = i;

    return new_order;
}

bool
isItem_insert_value(const Item &i)
{
    if (i.type() != Item::Type::FIELD_ITEM) {
        return false;
    }

    const Item_field &i_field =
        static_cast<const Item_field &>(i);

    return i_field.is_insert_value();
}

void
set_select_lex(LEX *const lex, SELECT_LEX *const select_lex)
{
    lex->select_lex = *select_lex;
    lex->unit.*rob<st_select_lex_node, st_select_lex_node *,
                   &st_select_lex_node::slave>::ptr() =
        &lex->select_lex;
    lex->unit.global_parameters = lex->current_select =
    lex->all_selects_list = &lex->select_lex;
}

void
set_where(st_select_lex *const sl, Item *const where)
{
    sl->where = where;
    if (sl->join) {
        sl->join->conds = where;
    }
}

void
set_having(st_select_lex *const sl, Item *const having)
{
    sl->having = having;
    if (sl->join) {
        sl->join->having = having;
    }
}

bool RiboldMYSQL::is_null(const Item &i)
{
    return const_cast<Item &>(i).is_null();
}

Item *RiboldMYSQL::clone_item(const Item &i)
{
    return const_cast<Item &>(i).clone_item();
}

const List<Item> *RiboldMYSQL::argument_list(const Item_cond &i)
{
    return const_cast<Item_cond &>(i).argument_list();
}

uint RiboldMYSQL::get_arg_count(const Item_sum &i)
{
    return const_cast<Item_sum &>(i).get_arg_count();
}

const Item *RiboldMYSQL::get_arg(const Item_sum &item, uint i)
{
    return const_cast<Item_sum &>(item).get_arg(i);
}

Item_subselect::subs_type RiboldMYSQL::substype(const Item_subselect &i)
{
    return const_cast<Item_subselect &>(i).substype();
}

const st_select_lex *
RiboldMYSQL::get_select_lex(const Item_subselect &i)
{
    return const_cast<Item_subselect &>(i).get_select_lex();
}

// Item::val_str(...) modifies/returns an internal buffer sometimes.
std::string RiboldMYSQL::val_str(const Item &i, bool *is_null)
{
    static const std::string empty_string = "";
    String s;
    String *const ret_s = const_cast<Item &>(i).val_str(&s);
    if (NULL == ret_s) {
        *is_null = true;
        return empty_string;
    }

    *is_null = false;
    return std::string(ret_s->ptr(), ret_s->length());
}

ulonglong RiboldMYSQL::val_uint(const Item &i)
{
    return const_cast<Item &>(i).val_uint();
}

double RiboldMYSQL::val_real(const Item &i)
{
    return const_cast<Item &>(i).val_real();
}
