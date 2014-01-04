#pragma once

#include <map>

#include <main/Analysis.hh>
#include <main/sql_handler.hh>
#include <main/dispatcher.hh>

#include <sql_lex.h>

// Abstract base class for query handler.
class DMLHandler : public SQLHandler {
public:
    virtual LEX *transformLex(Analysis &a, LEX *lex,
                               const ProxyState &ps) const;

private:
    virtual void gather(Analysis &a, LEX *lex, const ProxyState &ps)
        const = 0;
    virtual LEX *rewrite(Analysis &a, LEX *lex, const ProxyState &ps)
        const = 0;

protected:
    DMLHandler() {;}
    virtual ~DMLHandler() {;}
};

SQLDispatcher *buildDMLDispatcher();

