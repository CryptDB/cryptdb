#include <sstream>

#include <main/rewrite_ds.hh>
#include <main/error.hh>
#include <util/onions.hh>
#include <util/enum_text.hh>

std::ostream &operator<<(std::ostream &out,
                         const AbstractException &abstract_error)
{
    out << abstract_error.to_string();
    return out;
}

std::string AbstractException::to_string() const
{
    return "FILE: " + file_name + "\n"
           "LINE: " + std::to_string(line_number) + "\n";
}

// FIXME: Format the output.
std::string BadItemArgumentCount::to_string() const
{
    return "Item has bad argument count\n" +
           AbstractException::to_string() +
           // FIXME: Use TypeText.
           "ITEM TYPE: " + std::to_string(type) + "\n" +
           "EXPECTED COUNT: " + std::to_string(expected) + "\n" +
           "ACTUAL COUNT: " + std::to_string(actual) + "\n";
}

// FIXME: Format the output.
std::string UnexpectedSecurityLevel::to_string() const
{
    return "Unexpected security level for onion\n" +
           AbstractException::to_string() +
           "ONION TYPE: " + TypeText<onion>::toText(o) + "\n" +
           "EXPECTED LEVEL: " + TypeText<SECLEVEL>::toText(expected)+"\n" +
           "ACTUAL LEVEL: " + TypeText<SECLEVEL>::toText(actual) + "\n";
}

// FIXME: Format the output.
// FIXME: Children EncSet would be nice.
std::string NoAvailableEncSet::to_string() const
{
    std::stringstream s;
    s <<
        "Current crypto schemes do not support this query" << std::endl
        << AbstractException::to_string()
        // << "ITEM TYPE " + TypeText<>::toText();
        << "ITEM TYPE: " << std::to_string(type) << std::endl
        << "OPERATION: " << why << std::endl
        << "REQUIRED ENCSET: " << req_enc_set << std::endl
        << "***** CHILDREN REASONS *****" << std::endl;
    for (auto it = childr_rp.begin(); it != childr_rp.end(); it++) {
        s << "[" << std::to_string(std::distance(childr_rp.begin(), it))
                 << "] "
          << (*it)->getReason() << std::endl;
    }

    return s.str();
}

std::string TextMessageError::to_string() const
{
    return "Error: " + message + "\n"
           + AbstractException::to_string();
}

std::string
IdentifierNotFound::to_string() const
{
    return "Identifier not found: '" + this->identifier_name + "'\n"
           + AbstractException::to_string();
}

std::string
SynchronizationException::to_string() const
{
    return "** Synchronization Failure **\n"
           + error.to_string();
}

std::ostream &operator<<(std::ostream &out,
                         const SynchronizationException &error)
{
    out << error.to_string();
    return out;
}

