#include <parser/sql_utils.hh>
#include <parser/lex_util.hh>
#include <mysql.h>



using namespace std;

static bool lib_initialized = false;

void
init_mysql(const string & embed_db)
{
    if (lib_initialized) {
        return;
    }
    char dir_arg[1024];
    snprintf(dir_arg, sizeof(dir_arg), "--datadir=%s", embed_db.c_str());

    const char *mysql_av[] =
    { "progname",
            "--skip-grant-tables",
            dir_arg,
            /* "--skip-innodb", */
            /* "--default-storage-engine=MEMORY", */
            "--character-set-server=utf8",
            "--language=" MYSQL_BUILD_DIR "/sql/share/"
    };

    assert(0 == mysql_library_init(sizeof(mysql_av) / sizeof(mysql_av[0]),
                                   (char**) mysql_av, 0));

    assert(0 == mysql_thread_init());

    lib_initialized = true;
}

bool
isTableField(string token)
{
    size_t pos = token.find(".");

    if (pos == string::npos) {
        return false;
    } else {
        return true;
    }
}

// NOTE: Use FieldMeta::fullName if you know what onion's full name you
// need.
string
fullName(string field, string table)
{
    if (isTableField(field)) {
        return field;
    } else {
        return table + "." + field;
    }
}

char *
make_thd_string(const string &s, size_t *lenp)
{
    THD *thd = current_thd;
    assert(thd);
    if (lenp)
        *lenp = s.size();
    return thd->strmake(s.data(), s.size());
}

string
ItemToString(const Item &i) {
    if (RiboldMYSQL::is_null(i)) {
        return std::string("NULL");
    }

    bool is_null;
    const std::string &s0 = RiboldMYSQL::val_str(i, &is_null);
    assert(false == is_null);

    return s0;
}

string
ItemToStringWithQuotes(const Item &i) {
    if (RiboldMYSQL::is_null(i)) {
        return std::string("NULL");
    }

    bool is_null;
    const std::string &s0 = RiboldMYSQL::val_str(i, &is_null);
    assert(false == is_null);
    if (i.type() != Item::Type::STRING_ITEM) {
        return s0;
    }

    return "\"" + s0 + "\"";
}
