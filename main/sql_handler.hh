#pragma once

#include <main/Analysis.hh>

#include <sql_lex.h>

#include <map>

class SQLHandler {
public:
    SQLHandler() {;}
    virtual LEX *transformLex(Analysis &a, LEX *lex, const ProxyState &ps)
        const = 0;
    virtual ~SQLHandler() {;}
};

