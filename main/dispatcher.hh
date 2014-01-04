#pragma once

#include <map>

#include <main/Analysis.hh>
#include <main/sql_handler.hh>
#include <main/alter_sub_handler.hh>

#include <sql_lex.h>

template <typename Input, typename FetchMe>
class Dispatcher {
public:
    virtual ~Dispatcher() {}

    bool addHandler(long long cmd, FetchMe *const h) {
        if (NULL == h) {
            return false;
        }

        auto it = handlers.find(cmd);
        if (handlers.end() != it) {
            return false;
        }

        handlers[cmd] = std::unique_ptr<FetchMe>(h);
        return true;
    }

    virtual bool canDo(LEX *const lex) const = 0;

protected:
    std::map<long long, std::unique_ptr<FetchMe>> handlers;
};

class SQLDispatcher : public Dispatcher<LEX*, SQLHandler> {
public:
    bool canDo(LEX *const lex) const;
    const SQLHandler &dispatch(LEX *const lex) const;

private:
    virtual long long extract(LEX *const lex) const;
};

class AlterDispatcher : public Dispatcher<LEX*, AlterSubHandler> {
public:
    std::vector<AlterSubHandler *> dispatch(LEX *const lex) const;
    bool canDo(LEX *const lex) const;
};

