#pragma once

#include <util/util.hh>
#include <vector>
#include <string>
#include <memory>

#include <sql_select.h>
#include <sql_delete.h>
#include <sql_insert.h>
#include <sql_update.h>


// must be called before we can use any MySQL AP
void
init_mysql(const std::string & embed_db);

class ResType {
public:
    bool ok;  // query executed successfully
    std::vector<std::string> names;
    std::vector<enum_field_types> types;
    std::vector<std::vector<std::shared_ptr<Item> > > rows;

    explicit ResType(bool okflag = true) : ok(okflag) {}
    bool success() const {return this->ok;}
};

bool isTableField(std::string token);
std::string fullName(std::string field, std::string name);

char * make_thd_string(const std::string &s, size_t *lenp = 0);

std::string ItemToString(const Item &i);
std::string ItemToStringWithQuotes(const Item &i);
