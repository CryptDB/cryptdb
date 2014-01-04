#pragma once

#include <sstream>
#include <string>

#include <util/onions.hh>

class EncSet;
class RewritePlan;

class AbstractException {
public:
    AbstractException(const std::string &file_name,
                      unsigned int line_number)
        : file_name(file_name), line_number(line_number) {}
    virtual ~AbstractException() {}

    virtual std::string to_string() const = 0;
    std::string getFileName() const {return file_name;}
    unsigned int lineNumber() const {return line_number;}

private:
    const std::string file_name;
    const unsigned int line_number;
};

std::ostream &operator<<(std::ostream &out,
                         const AbstractException &abstract_error);

class BadItemArgumentCount : public AbstractException {
public:
    BadItemArgumentCount(const std::string &file_name, int line_number,
                         unsigned int type, int expected, int actual)
        : AbstractException(file_name, line_number), type(type),
          expected(expected), actual(actual) {}
    ~BadItemArgumentCount() {}

    // final requires g++ 4.7
    // std::string to_string() const final;
    std::string to_string() const;

private:
    // const Item::Type type;
    const unsigned int type;
    const int expected;
    const int actual;
};

class UnexpectedSecurityLevel : public AbstractException {
public:
    UnexpectedSecurityLevel(const std::string &file_name, int line_number,
                            onion o, SECLEVEL expected, SECLEVEL actual)
        : AbstractException(file_name, line_number), o(o),
          expected(expected), actual(actual) {}
    ~UnexpectedSecurityLevel() {}

    // std::string to_string() const final;
    std::string to_string() const;

private:
    const onion o;
    const SECLEVEL expected;
    const SECLEVEL actual;
};

class NoAvailableEncSet : public AbstractException {
public:
    NoAvailableEncSet(const std::string &file_name, int line_number,
                      unsigned int type, const EncSet &req_enc_set,
                      const std::string &why,
                      const std::vector<std::shared_ptr<RewritePlan> >
                        &childr_rp)
        : AbstractException(file_name, line_number), type(type),
          req_enc_set(req_enc_set), why(why), childr_rp(childr_rp) {}
    ~NoAvailableEncSet() {}

    // std::string to_string() const final;
    std::string to_string() const;

private:
    // const Item *const i;
    const unsigned int type;
    const EncSet req_enc_set;
    const std::string why;
    const std::vector<std::shared_ptr<RewritePlan> > childr_rp;
};

class TextMessageError : public AbstractException {
public:
    TextMessageError(const std::string &file_name, int line_number,
                     const std::string &message)
        : AbstractException(file_name, line_number),
          message(message) {}
    ~TextMessageError() {}

    // std::string to_string() const final;
    std::string to_string() const;

private:
    const std::string message;
};

class IdentifierNotFound : public AbstractException {
public:
    IdentifierNotFound(const std::string &file_name, int line_number,
                       const std::string &identifier_name)
        : AbstractException(file_name, line_number),
          identifier_name(identifier_name) {}
    ~IdentifierNotFound() {}

    std::string to_string() const;

private:
    const std::string identifier_name;
};

// Do not derive this from AbstractException; their handling is too
// divergent and we don't want to miss cases in catch graphs.
class SynchronizationException {
public:
    SynchronizationException(const std::string &file_name,
                             unsigned long line_number,
                             const std::string &details)
        : error(TextMessageError(file_name, line_number, details)) {}
    ~SynchronizationException() {}

    std::string to_string() const;

private:
    const TextMessageError error;
};

std::ostream &operator<<(std::ostream &out,
                         const SynchronizationException &error);

