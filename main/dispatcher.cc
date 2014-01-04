#include <main/dispatcher.hh>

bool
SQLDispatcher::canDo(LEX *const lex) const
{
    return handlers.end() != handlers.find(extract(lex));
}

const SQLHandler &
SQLDispatcher::dispatch(LEX *const lex) const
{
    auto it = handlers.find(extract(lex));
    assert(handlers.end() != it);

    assert(it->second);
    return *it->second;
}

long long
SQLDispatcher::extract(LEX *const lex) const
{
    return lex->sql_command;
}

// FIXME: Implement.
bool
AlterDispatcher::canDo(LEX *const lex) const
{
    return true;
}

std::vector<AlterSubHandler *>
AlterDispatcher::dispatch(LEX *const lex) const
{
    std::vector<AlterSubHandler *> out;
    for (auto it = handlers.begin(); it != handlers.end(); it++) {
        long long extract = lex->alter_info.flags & (*it).first;
        if (extract) {
            auto it_handler = handlers.find(extract);
            assert(handlers.end() != it_handler && it_handler->second);
            out.push_back(it_handler->second.get());
        }
    }

    return out;
}

