#pragma once

#include <map>

#include <main/Analysis.hh>
#include <main/sql_handler.hh>
#include <main/dispatcher.hh>

#include <sql_lex.h>

// Abstract base class for command handler.
class DDLHandler : public SQLHandler {
public:
    virtual LEX *transformLex(Analysis &analysis, LEX *lex, 
                              const ProxyState &ps) const;

private:
    virtual LEX *rewriteAndUpdate(Analysis &a, LEX *lex,
                                  const ProxyState &ps) const = 0;

protected:
    DDLHandler() {;}
    virtual ~DDLHandler() {;}
};

SQLDispatcher *buildDDLDispatcher();

