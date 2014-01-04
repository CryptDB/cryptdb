#pragma once

#include <sstream>
#include <string>
#include <stdexcept>

#include <parser/Annotation.hh>

#include <mysql.h>
#include <sql_base.h>

class embedmysql {
 public:
    embedmysql(const std::string &dir);
    virtual ~embedmysql();

    MYSQL *conn();

 private:
    MYSQL *m;
};

class mysql_thrower : public std::stringstream {
 public:
    ~mysql_thrower() __attribute__((noreturn));
};

class query_parse {
 public:
    query_parse(const std::string &db, const std::string &q);
    virtual ~query_parse();
    LEX *lex();
    Annotation *annot;

 private:
    void cleanup();

    THD *t;
    Parser_state ps;
};
