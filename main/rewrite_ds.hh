#pragma once

#include <list>
#include <vector>
#include <memory>

#include <util/onions.hh>

class FieldMeta;
/**
 * Field here is either:
 * A) empty string, representing any field or
 * B) the field that the onion is key-ed on.
 */
typedef std::pair<SECLEVEL, FieldMeta *> LevelFieldPair;

typedef std::map<SECLEVEL, FieldMeta *> LevelFieldMap;
typedef std::pair<onion, LevelFieldPair> OnionLevelFieldPair;
typedef std::map<onion, LevelFieldPair> OnionLevelFieldMap;

class Analysis;
// onion-level-key: all the information needed to know how to encrypt a
// constant
class OLK {
public:
    OLK(onion o, SECLEVEL l, FieldMeta *const key) : o(o), l(l),
        key(key) {}
    // FIXME: Default constructor required so we can use make_pair.
    OLK() {;}
    onion o;
    SECLEVEL l;
    FieldMeta *key; // a field meta is a key because each encryption key
                    // ever used in CryptDB belongs to a field; a field
                    // contains the encryption and decryption handlers
                    // for its keys (see layers)
    bool operator<(const OLK &olk) const {
        return (o < olk.o) || ((o == olk.o) && (l < olk.l));
    }
    bool operator==(const OLK &olk) const {
        return (o == olk.o) && (l == olk.l);
    }
    static OLK invalidOLK() {
        return OLK(oINVALID, SECLEVEL::INVALID, NULL);
    }
    // WARN: This semantic can not reliably tell you if an OLK can
    // support operations. Such a meaning is contextual.
    static bool isNotInvalid(const OLK &olk) {
        return olk.o != oINVALID && olk.l != SECLEVEL::INVALID;
    }
};

/**
 * Used to keep track of encryption constraints during
 * analysis
 */
class EncSet {
public:
    EncSet(OnionLevelFieldMap input) : osl(input) {}
    EncSet(Analysis &a, FieldMeta *const fm);
    explicit EncSet(const OLK &olk);

    /**
     * decides which encryption scheme to use out of multiple in a set
     */
    OLK chooseOne() const;
    bool contains(const OLK &olk) const;
    bool hasSecLevel(SECLEVEL level) const;
    EncSet intersect(const EncSet &es2) const;
    SECLEVEL onionLevel(onion o) const;
    bool available() const;
    bool singleton() const {return osl.size() == 1;}
    bool single_crypted_and_or_plainvals() const;
    OLK extract_singleton() const;

    OnionLevelFieldMap osl; //max level on each onion
};

std::ostream&
operator<<(std::ostream &out, const EncSet &es);

const EncSet EQ_EncSet = {
    {
        {oPLAIN, LevelFieldPair(SECLEVEL::PLAINVAL, NULL)},
        {oDET,   LevelFieldPair(SECLEVEL::DET, NULL)},
        {oOPE,   LevelFieldPair(SECLEVEL::OPE, NULL)},
    }
};

const EncSet JOIN_EncSet = {
    {
        {oPLAIN, LevelFieldPair(SECLEVEL::PLAINVAL, NULL)},
        {oDET,   LevelFieldPair(SECLEVEL::DETJOIN, NULL)},
    }
};

const EncSet ORD_EncSet = {
    {
        {oPLAIN, LevelFieldPair(SECLEVEL::PLAINVAL, NULL)},
        {oOPE, LevelFieldPair(SECLEVEL::OPE, NULL)},
    }
};

const EncSet PLAIN_EncSet = {
    {
        {oPLAIN, LevelFieldPair(SECLEVEL::PLAINVAL, NULL)},
    }
};

//todo: there should be a map of FULL_EncSets depending on item type
const EncSet FULL_EncSet = {
    {
        {oPLAIN, LevelFieldPair(SECLEVEL::RND, NULL)},
        {oDET, LevelFieldPair(SECLEVEL::RND, NULL)},
        {oOPE, LevelFieldPair(SECLEVEL::RND, NULL)},
        {oAGG, LevelFieldPair(SECLEVEL::HOM, NULL)},
        {oSWP, LevelFieldPair(SECLEVEL::SEARCH, NULL)},
    }
};

const EncSet FULL_EncSet_Str = {
    {
        {oPLAIN, LevelFieldPair(SECLEVEL::RND, NULL)},
        {oDET, LevelFieldPair(SECLEVEL::RND, NULL)},
        {oOPE, LevelFieldPair(SECLEVEL::RND, NULL)},
        {oSWP, LevelFieldPair(SECLEVEL::SEARCH, NULL)},
    }
};

const EncSet FULL_EncSet_Int = {
    {
        {oPLAIN, LevelFieldPair(SECLEVEL::RND, NULL)},
        {oDET, LevelFieldPair(SECLEVEL::RND, NULL)},
        {oOPE, LevelFieldPair(SECLEVEL::RND, NULL)},
        {oAGG, LevelFieldPair(SECLEVEL::HOM, NULL)},
    }
};

const EncSet Search_EncSet = {
    {
        {oPLAIN, LevelFieldPair(SECLEVEL::PLAINVAL, NULL)},
        {oSWP, LevelFieldPair(SECLEVEL::SEARCH, NULL)},
    }
};

const EncSet ADD_EncSet = {
    {
        {oPLAIN, LevelFieldPair(SECLEVEL::PLAINVAL, NULL)},
        {oAGG, LevelFieldPair(SECLEVEL::HOM, NULL)},
    }
};

// FIXME: Determine why the empty brackets result in one element.
const EncSet EMPTY_EncSet {
    OnionLevelFieldMap()
};

bool
needsSalt(OLK olk);

// returns true if any of the layers in ed
// need salt
bool
needsSalt(EncSet ed);

class Item;

class reason {
public:
    reason(const EncSet &es, const std::string &why,
           const Item &item)
        : encset(es), why(why), item(item) {}

    const EncSet encset;
    const std::string why;
    const Item &item;
};

std::ostream&
operator<<(std::ostream &out, const reason &r);

// The rewrite plan of a lex node: the information a
// node remembers after gather, to be used during rewrite
// Other more specific RewritePlan-s inherit from this class
class RewritePlan {
public:
    const reason r;
    // HACK: Should be const.
    EncSet es_out; // encset that this item can output

    RewritePlan(const EncSet &es, reason r) : r(r), es_out(es) {};
    reason getReason() const {return r;}

    //only keep plans that have parent_olk in es
//    void restrict(const EncSet & es);

};

//rewrite plan in which we only need to remember one olk
// to know how to rewrite
class RewritePlanOneOLK: public RewritePlan {
public:
    const OLK olk;
    // the following store how to rewrite children
    std::vector<std::shared_ptr<RewritePlan> > childr_rp;

    RewritePlanOneOLK(const EncSet &es_out, const OLK &olk,
                      const std::vector<std::shared_ptr<RewritePlan> >
                        &childr_rp, reason r)
        : RewritePlan(es_out, r), olk(olk),
          childr_rp(childr_rp) {}
};

class RewritePlanPerChildOLK : public RewritePlan {
public:
    const std::vector<std::pair<std::shared_ptr<RewritePlan>, OLK>>
        child_olks;

    RewritePlanPerChildOLK(const EncSet &es_out,
                const std::vector<std::pair<std::shared_ptr<RewritePlan>,
                                          OLK>> &child_olks, reason r)
        : RewritePlan(es_out, r), child_olks(child_olks) {}
};

class RewritePlanWithAnalysis : public RewritePlan {
public:
    const std::unique_ptr<Analysis> a;
    RewritePlanWithAnalysis(const EncSet &es_out, reason r,
                            std::unique_ptr<Analysis> a);
};

std::ostream&
operator<<(std::ostream &out, const RewritePlan *const rp);

