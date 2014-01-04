#include "Annotation.hh"

using namespace std;

//removes character c from the end of s, if it's the last character
// if c isn't the last character, returns without doing anything
void
end_prune(string &str, char c) {
    if (*str.rbegin() == c) {
        str.erase(str.length() - 1);
    }
}

//removes character c from the beginning of s, if it's the first character
// if c isn't the first character, returns without doing anything
void
start_prune(string &str, char c) {
    if (*str.begin() == c) {
        str.erase(0, 1);
    }
}

void
prune(string &str, char c) {
    start_prune(str, c);
    end_prune(str, c);
}

//splits string s into a list of words deliminated by c
list<string>
split(string &str, char c) {
    string s = str;
    list<string> result;
    size_t delim = 0;
    int i = 0;
    while(delim != string::npos) {
        delim = s.find(c);
        string word = s.substr(0,delim);
        result.push_back(word);
        s = s.substr(delim+1,string::npos);
        i++;
    }
    return result;
}

Annotation::Annotation(const string &q) {
    query = q;
    DETenclevel = SECLEVEL::INVALID;
    OPEenclevel = SECLEVEL::INVALID;
    AGGenclevel = SECLEVEL::INVALID;
    SWPenclevel = SECLEVEL::INVALID;
    pred = NULL;
    parse();
    return;
}

Annotation::~Annotation() {
    if (pred) {
        delete pred;
    }
}

PrincType
Annotation::getLeft() {
    if (type == SPEAKSFOR) {
        return left;
    }
    //LOG(error) << "Annotation asked for left when not SPEAKSFOR";
    PrincType empty;
    empty.princtype = "";
    empty.column = "";
    return empty;
}

PrincType
Annotation::getRight() {
    if (type == SPEAKSFOR || type == ENCFOR) {
        return right;
    }
    //LOG(error) << "Annotation asked for right when not SPEAKSFOR or ENCFOR";
    PrincType empty;
    empty.princtype = "";
    empty.column = "";
    return empty;
}

string
Annotation::getPrimitive() {
    if (type != SPEAKSFOR) {
        return primitive;
    }
    //LOG(error) << "Annotation asked for primitive when SPEAKSFOR";
    return "";
}

string
Annotation::getLeftTableName() {
    if (type != SPEAKSFOR) {
        //LOG(error) << "Annotation asked for left when not SPEAKSFOR";
        return "";
    }
    list<string> left_words = split(left.column, '.');
    assert_s(left_words.size() == 2, "table or column name has a . in it");
    return left_words.front();
}

string
Annotation::getLeftFieldName() {
    if (type != SPEAKSFOR) {
        //LOG(error) << "Annotation asked for left when not SPEAKSFOR";
        return "";
    }
    list<string> left_words = split(left.column, '.');
    assert_s(left_words.size() == 2, "table or column name has a . in it");
    return left_words.back();
}

string
Annotation::getRightTableName() {
    if (type != SPEAKSFOR && type != ENCFOR) {
        //LOG(error) << "Annotation asked for right when not SPEAKSFOR or ENCFOR";
        return "";
    }
    list<string> right_words = split(right.column, '.');
    assert_s(right_words.size() == 2, "table or column name has a . in it");
    return right_words.front();
}

string
Annotation::getRightFieldName() {
    if (type != SPEAKSFOR && type != ENCFOR) {
        //LOG(error) << "Annotation asked for right when not SPEAKSFOR or ENCFOR";
        return "";
    }
    list<string> right_words = split(right.column, '.');
    assert_s(right_words.size() == 2, "table or column name has a . in it");
    return right_words.back();
}

string
Annotation::getPrimitiveTableName() {
    if (type != ENCFOR && type != SINGLE_ENC) {
        //LOG(error) << "Annotation asked for primitive when not ENCFOR";
        return "";
    }
    list<string> prim_words = split(primitive, '.');
    assert_s(prim_words.size() == 2, "table or column name has a . in it");
    return prim_words.front();
}

string
Annotation::getPrimitiveFieldName() {
    if (type != ENCFOR && type != SINGLE_ENC) {
        //LOG(error) << "Annotation asked for primitive when not ENCFOR";
        return "";
    }
    list<string> prim_words = split(primitive, '.');
    assert_s(prim_words.size() == 2, "table or column name has a . in it");
    return prim_words.back();
}

SECLEVEL
Annotation::hasOnion(onion o) {
    if (type != ENCFOR) {
        return SECLEVEL::INVALID;
    }

    switch (o) {
    case oDET:
        return DETenclevel;
    case oOPE:
        return OPEenclevel;
    case oAGG:
        return AGGenclevel;
    case oSWP:
        return SWPenclevel;
    default:
        //LOG(error) << "unknown onion";
        return SECLEVEL::INVALID;
    }
}

Predicate *
Annotation::getPredicate() {
    if (type != SPEAKSFOR) {
        //LOG(error) << "Annotation asked for predicate when not SPEAKSFOR";
        return NULL;
    }
    return pred;
}

string
Annotation::getLeftStr() {
    if (type == SPEAKSFOR) {
        return left.princtype + "=" + left.column;
    }
    //LOG(error) << "Annotation asked for left when not SPEAKSFOR";
    return "";
}

string
Annotation::getRightStr() {
    if (type == SPEAKSFOR || ENCFOR) {
        return right.princtype + "=" + right.column;
    }
    //LOG(error) << "Annotation asked for left when not SPEAKSFOR";
    return "";
}

void
Annotation::parse() {
    //remove trailing semicolon
    end_prune(query, ';');
    list<string> query_list = split(query, ' ');

    //type
    if (toLowerCase(query).find("princtype",0) != string::npos) {
        if (toLowerCase(query).find("external",0) != string::npos) {
            type = PRINCTYPE_EXTERNAL;
        } else {
            type = PRINCTYPE;
        }
    } else if (toLowerCase(query).find("encfor",0) != string::npos) {
        type = ENCFOR;
    } else if (toLowerCase(query).find("speaksfor",0) != string::npos) {
        type = SPEAKSFOR;
    } else if (toLowerCase(query).find("enc",0) != string::npos) {
        type = SINGLE_ENC;
    } else {
        assert_s(false, "annotation is not PRINCTYPE, PRINCTYPE EXTERNAL, SPEAKSFOR, ENCFOR, or SINGLE_ENC");
    }

    auto word = query_list.begin();
    if (toLowerCase(*word) == "cryptdb") {
        word++;
    }

    //first word/pair
    switch (type) {
    case PRINCTYPE:
    case PRINCTYPE_EXTERNAL:
        assert_s(toLowerCase(*word) == "princtype", "PRINCTYPE annotation does not begin with PRINCTYPE");
        break;
    case ENCFOR:
    case SINGLE_ENC:
        primitive = *word;
        break;
    case SPEAKSFOR:
        left.column = *word;
        word++;
        left.princtype = *word;
        break;
    }

    //middle word
    word++;
    switch(type) {
    case PRINCTYPE:
    case PRINCTYPE_EXTERNAL:
        primitive = *word;
        break;
    case ENCFOR:
        assert_s(toLowerCase(*word) == "encfor", "middle of ENCFOR annotation is not ENCFOR");
        break;
    case SPEAKSFOR:
        assert_s(toLowerCase(*word) == "speaksfor", "middle of SPEAKSFOR annotation is not SPEAKSFOR");
        break;
    case SINGLE_ENC:
        assert_s(toLowerCase(*word) == "enc", "middle of single princ ENC annotation is not ENC");
    }

    //last word/pair
    word++;
    switch(type) {
    case PRINCTYPE:
        assert_s(word == query_list.end(), "PRINCTYPE annotation has too many words");
        return;
    case PRINCTYPE_EXTERNAL:
        assert_s(toLowerCase(*word) == "external", "PRINCTYPE EXERNAL annotation does not have EXTERNAL after primitive");
        break;
    case ENCFOR:
    case SPEAKSFOR:
        right.column = *word;
        word++;
        right.princtype = *word;
        break;
    case SINGLE_ENC:
        assert_s(word == query_list.end(), "SINGLE_ENC annotation has too many words");
        return;
    }

    //encryption level or predicate, if exists
    word++;
    switch(type) {
    case PRINCTYPE:
    case SINGLE_ENC:
        assert_s(false, "annotation has too many words");
    case PRINCTYPE_EXTERNAL:
        //this should be the end
        assert_s(word == query_list.end(), "annotation has too many words");
        return;
    case ENCFOR:
        if (word == query_list.end()) {
            DETenclevel = SECLEVEL::RND;
            OPEenclevel = SECLEVEL::RND;
        }
        while (word != query_list.end()) {
            throw CryptDBError("Need to get enclevel!");
            /*
            SECLEVEL enclevel =
                TypeText<SECLEVEL>::toType(toUpperCase(*word));
            switch (enclevel) {
            case SECLEVEL::DETJOIN:
            case SECLEVEL::DET:
                DETenclevel = enclevel;
                break;
            // case SECLEVEL::OPEJOIN:
            case SECLEVEL::OPE:
                OPEenclevel = enclevel;
                break;
            case SECLEVEL::SEARCH:
                SWPenclevel = enclevel;
                break;
            case SECLEVEL::HOM:
                AGGenclevel = enclevel;
                break;
            default:
                assert_s(false, "unknown onion level annotation on ENC_FOR");
            }
            word++;
            */
        }
        break;
    case SPEAKSFOR:
        if (word == query_list.end()) {
            break;
        }
        assert_s(toLowerCase(*word) == "if", "SPEAKSFOR annotation predicate does not start with if");
        word++;
        //pred name and fields
        pred = new Predicate();
        assert_s(word != query_list.end(), "no predicate after IF");
        list<string> pred_split = split(*word, '(');
        assert_s(pred_split.size() == 2, "predicate has too many (");
        pred->name = pred_split.front();
        end_prune(pred_split.back(),')');
        list<string> pred_fields = split(pred_split.back(),',');
        for(auto field = pred_fields.begin(); field != pred_fields.end(); field++) {
            pred->fields.push_back(*field);
        }

    }
    return;
}
