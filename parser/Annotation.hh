#pragma once

/***************************************************/
//for multi-princ annoation parsing

#include <util/util.hh>
#include <util/onions.hh>

typedef enum annoType {
    PRINCTYPE,
    PRINCTYPE_EXTERNAL,
    ENCFOR,
    SPEAKSFOR,
    SINGLE_ENC,
} annoType;

// PrincTypes are the part of an annotation that describe which principal
//   a column name is included in
typedef struct PrincType {
    std::string princtype;
    std::string column;
} PrincType;

void end_prune(std::string &s, char c);
void start_prune(std::string &s, char c);
void prune(std::string &s, char c);
std::list<std::string> split(std::string &s, char c);

/*
 * Types of Annotations:
 *  CRYPTDB PRINCTYPE princtypename;
 *  CRYPTDB PRINCTYPE princtypename EXTERNAL;
 *  CRYPTDB primitive ENCFOR right [enclevel];
 *  CRYPTDB left SPEAKSFOR right [IF predicate];
 *
 *  CRYPTDB primitive ENC
 *
 * primitive := tablename.columnname
 * princtype := tablename.columnname princtypename
 * left      := princtype
 * right     := princtype
 *
 * For example:
 *  CRYPTDB PRINCTYPE msg
 *  CRYPTDB PRINCTYPE user
 *  CRYPTDB PRINCTYPE username EXTERNAL
 *  CRYPTDB privmsg.senderid user SPEAKSFOR privmsg.msgid msg
 *  CRYPTDB msgs.msgtext ENCFOR msgs.msgid msg
 *  CRYPTDB users.username username SPEAKSFOR users.userid user
 *
 */
class Annotation {
public:
    Annotation(const std::string &q);
    annoType type;
    //accessor methods for the private fields -- prints an error and returns
    //  an empty object if this type doesn't have that field
    //XXX should it do something more exciting?
    std::string getPrimitive();
    PrincType getLeft();
    PrincType getRight();

    //PrincType converted to strings: princtype=column
    std::string getLeftStr();
    std::string getRightStr();

    //accessor methods for the table names for various fields
    //requires: fields requested (primitive or princtype.column) to be of the
    //          form tablename.fieldname
    std::string getLeftTableName();
    std::string getLeftFieldName();
    std::string getPrimitiveTableName();
    std::string getPrimitiveFieldName();
    std::string getRightTableName();
    std::string getRightFieldName();

    //returns the encryption level for primitive
    //requires: type to be ENCFOR
    SECLEVEL hasOnion(onion o);

    //return the predicate, if it exists
    //requires: type to be SPEAKSFOR
    Predicate * getPredicate();

    //parses query into left, right, primitive and type
    void parse();
    ~Annotation();

private:
    std::string query;

    PrincType left;
    PrincType right;
    std::string primitive;

    Predicate *pred;
    SECLEVEL DETenclevel;
    SECLEVEL OPEenclevel;
    SECLEVEL AGGenclevel;
    SECLEVEL SWPenclevel;
};
