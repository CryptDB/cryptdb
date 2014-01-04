/*
 * Translator.cpp
 *
 *  Created on: Aug 13, 2010
 *      Author: raluca
 */

#include <main/Translator.hh>
#include <util/cryptdb_log.hh>

#include <functional>
#include <ctime>

// TODO: Make length longer.
// TODO: Ensure some level of collision resistance.
std::string
getpRandomName()
{
    // FIXME: Not using numbers because.
    //  'CREATE TABLE a (2e integer); <---- succeeds
    //  'CREATE TABLE b (2e5 integer); <--- fails
    static const char valids[] =
        // "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static int out_length = 10;
    char output[out_length + 1];

    std::function<bool()> wrap_srand =[](){srand(time(NULL)); return true;};
    std::function<void(bool)> do_nothing = [] (bool b) {return;};
    static bool danger_will_robinson = wrap_srand();
    do_nothing(danger_will_robinson);

    for (int i = 0; i < out_length; ++i) {
        output[i] = valids[rand() % strlen(valids)];
    }
    output[out_length] = 0;

    return std::string(output);
}

std::string
nextAutoInc(std::map<std::string, unsigned int > & autoInc,
            std::string fullname)
{
    std::string val;
    if (autoInc.find(fullname) == autoInc.end()) {
        val = "1";
        autoInc[fullname] = 1;
    } else {
        autoInc[fullname] += 1;
        val = strFromVal(autoInc[fullname]);
    }

    return val;
}

std::string
getTableSalt(std::string anonTableName) {
    return BASE_SALT_NAME + "_t_" + anonTableName;
}

bool
isSalt(std::string id, bool & isTableSalt)
{
    if (id.find(BASE_SALT_NAME) == 0 || (isTableField(id) && (getField(id).find(BASE_SALT_NAME) == 0))) {
        if (id.find(BASE_SALT_NAME+"_t_") == 0) {
            isTableSalt = true;
        } else {
            isTableSalt = false;
        }
        return true;
    }

    return false;
}

std::string
getTableOfSalt(std::string salt_name) {

    return salt_name.substr(BASE_SALT_NAME.length() + 3, salt_name.length() - 3 - BASE_SALT_NAME.length());
}



std::string
getFieldsItSelect(std::list<std::string> & words,
                  std::list<std::string>::iterator & it)
{
    it = words.begin();
    it++;
    std::string res = "SELECT ";

    if (equalsIgnoreCase(*it, "distinct")) {
        LOG(edb_v) << "has distinct!";
        it++;
        res += "DISTINCT ";
    }

    return res;
}

/*
 * The following functions return field name and table name.
 * Require the data to be in the format table.field or field.
 *
 */
std::string
getField(std::string tablefield)
{
    if (isTableField(tablefield)) {
        size_t pos = tablefield.find(".");
        return tablefield.substr(pos+1, tablefield.length() - pos - 1);
    } else {
        return tablefield;
    }
}

std::string
getTable(std::string tablefield)
{
    if (isTableField(tablefield)) {
        size_t pos = tablefield.find(".");
        return tablefield.substr(0, pos);
    } else {
        return "";
    }
}

