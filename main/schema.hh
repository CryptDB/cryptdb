#pragma once

#include <util/onions.hh>
#include <util/enum_text.hh>
#include <parser/embedmysql.hh>
#include <parser/stringify.hh>
#include <main/CryptoHandlers.hh>
#include <main/Translator.hh>
#include <main/dbobject.hh>
#include <main/macro_util.hh>
#include <string>
#include <map>
#include <list>
#include <iostream>
#include <sstream>
#include <functional>

// > FIXME: We are Memleaking
// SchemaInfo/TableMeta/FieldMeta/OnionMeta/EncLayer and the keys.

class Analysis;
class FieldMeta;

/*
 * The name must be unique as it is used as a unique identifier when
 * generating the encryption layers.
 * 
 * OnionMeta is a bit different than the other AbstractMeta derivations.
 * > It's children aren't of the same class.  Each EncLayer does
 *   inherit from EncLayer, but they are still distinct classes. This
 *   is problematic because DBMeta::deserialize<ConcreteClass> relies
 *   on us being able to provide a concrete class.  We can't pick a
 *   specific class for our OnionMeta as it must support multiple classes.
 * > Also note that like FieldMeta, OnionMeta's children have an explicit
 *   order that must be encoded.
 */
typedef class OnionMeta : public DBMeta {
public:
    // New.
    OnionMeta(onion o, std::vector<SECLEVEL> levels,
              const AES_KEY * const m_key, Create_field * const cf,
              unsigned long uniq_count);

    // Restore.
    static std::unique_ptr<OnionMeta>
        deserialize(unsigned int id, const std::string &serial);
    OnionMeta(unsigned int id, const std::string &onionname,
              unsigned long uniq_count)
        : DBMeta(id), onionname(onionname), uniq_count(uniq_count) {}

    std::string serialize(const DBObject &parent) const;
    std::string getAnonOnionName() const;
    // FIXME: Use rtti.
    std::string typeName() const {return type_name;}
    static std::string instanceTypeName() {return type_name;}
    std::vector<DBMeta *>
        fetchChildren(const std::unique_ptr<Connect> &e_conn);
    bool applyToChildren(std::function<bool(const DBMeta &)>) const;
    UIntMetaKey const &getKey(const DBMeta &child) const;
    EncLayer *getLayerBack() const;
    EncLayer *getLayer(const SECLEVEL &sl) const;
    bool hasEncLayer(const SECLEVEL &sl) const;
    unsigned long getUniq() const {return uniq_count;}

    // Need access to layers.
    friend class Analysis;
    friend class FieldMeta;
    friend class OnionMetaAdjustor;
    friend bool sanityCheck(FieldMeta &);
    friend Item *decrypt_item_layers(Item *const, const FieldMeta *const,
                                     onion, uint64_t);

private:
    std::vector<std::unique_ptr<EncLayer>> layers; // first in list is
                                                   // lowest layer
    constexpr static const char *type_name = "onionMeta";
    const std::string onionname;
    unsigned long uniq_count;
    mutable std::list<std::unique_ptr<UIntMetaKey>> generated_keys;

    SECLEVEL getSecLevel() const;
} OnionMeta;

class TableMeta;
//TODO: FieldMeta and TableMeta are partly duplicates with the original
// FieldMetadata an TableMetadata
// which contains data we want to add to this structure soon
typedef class FieldMeta : public MappedDBMeta<OnionMeta, OnionMetaKey> {
public:
    const std::string fname;
    const std::string salt_name;

    // New.
    FieldMeta(const std::string &name, Create_field * const field,
              const AES_KEY * const mKey, SECURITY_RATING sec_rating,
              unsigned long uniq_count);
    // Restore (WARN: Creates an incomplete type as it will not have it's
    // OnionMetas until they are added by the caller).
    static std::unique_ptr<FieldMeta>
        deserialize(unsigned int id, const std::string &serial);
    FieldMeta(unsigned int id, const std::string &fname, bool has_salt,
              const std::string &salt_name, onionlayout onion_layout,
              SECURITY_RATING sec_rating, unsigned long uniq_count,
              unsigned long counter, bool has_default,
              const std::string &default_value)
        : MappedDBMeta(id), fname(fname), salt_name(salt_name),
          onion_layout(onion_layout), has_salt(has_salt),
          sec_rating(sec_rating), uniq_count(uniq_count),
          counter(counter), has_default(has_default),
          default_value(default_value) {}
    ~FieldMeta() {;}

    std::string serialize(const DBObject &parent) const;
    std::string stringify() const;
    std::vector<std::pair<const OnionMetaKey *, OnionMeta *>>
        orderedOnionMetas() const;
    std::string getSaltName() const;
    unsigned long getUniq() const {return uniq_count;}

    OnionMeta *getOnionMeta(onion o) const;
    // FIXME: Use rtti.
    std::string typeName() const {return type_name;}
    static std::string instanceTypeName() {return type_name;}

    SECURITY_RATING getSecurityRating() const {return sec_rating;}
    unsigned long leaseIncUniq() {return counter++;}
    // FIXME: Change name.
    unsigned long getCurrentUniqCounter() const {return counter;}
    bool hasOnion(onion o) const;
    bool hasDefault() const {return has_default;}
    std::string defaultValue() const {return default_value;}
    const onionlayout &getOnionLayout() const {return onion_layout;}
    bool getHasSalt() const {return has_salt;}

private:
    constexpr static const char *type_name = "fieldMeta";
    const onionlayout onion_layout;
    const bool has_salt; //whether this field has its own salt
    const SECURITY_RATING sec_rating;
    unsigned long uniq_count;
    unsigned long counter;
    const bool has_default;
    const std::string default_value;

    SECLEVEL getOnionLevel(onion o) const;
    static onionlayout determineOnionLayout(const AES_KEY *const m_key,
                                            const Create_field *const f,
                                            SECURITY_RATING sec_rating);
    static bool determineHasDefault(const Create_field *const cf);
    static std::string determineDefaultValue(bool has_default,
                                             const Create_field *const cf);
} FieldMeta;

typedef class TableMeta : public MappedDBMeta<FieldMeta, IdentityMetaKey> {
public:
    const bool hasSensitive;
    const bool has_salt;
    const std::string salt_name;
    const std::string anon_table_name;

    // New TableMeta.
    TableMeta(bool has_sensitive, bool has_salt)
        : hasSensitive(has_sensitive), has_salt(has_salt),
          salt_name("tableSalt_" + getpRandomName()),
          anon_table_name("table_" + getpRandomName()),
          counter(0) {}
    // Restore.
    static std::unique_ptr<TableMeta>
        deserialize(unsigned int id, const std::string &serial);
    TableMeta(unsigned int id, const std::string &anon_table_name,
              bool has_sensitive, bool has_salt,
              const std::string &salt_name, unsigned int counter)
        : MappedDBMeta(id), hasSensitive(has_sensitive),
          has_salt(has_salt), salt_name(salt_name),
          anon_table_name(anon_table_name), counter(counter) {}
    ~TableMeta() {;}

    std::string serialize(const DBObject &parent) const;
    std::string getAnonTableName() const;
    std::vector<FieldMeta *> orderedFieldMetas() const;
    std::vector<FieldMeta *> defaultedFieldMetas() const;
    // FIXME: Use rtti.
    std::string typeName() const {return type_name;}
    static std::string instanceTypeName() {return type_name;}
    unsigned long leaseIncUniq() {return counter++;}
    unsigned long getCurrentUniqCounter() {return counter;}

    friend class Analysis;

private:
    constexpr static const char *type_name = "tableMeta";
    unsigned int counter;

    std::string getAnonIndexName(const std::string &index_name,
                                 onion o) const;
} TableMeta;

class DatabaseMeta : public MappedDBMeta<TableMeta, IdentityMetaKey> {
public:
    // New DatabaseMeta.
    DatabaseMeta() : MappedDBMeta(0) {}
    // Restore.
    static std::unique_ptr<DatabaseMeta>
        deserialize(unsigned int id, const std::string &serial);
    DatabaseMeta(unsigned int id) : MappedDBMeta(id) {}

    ~DatabaseMeta() {}

    std::string serialize(const DBObject &parent) const;
    // FIXME: rtti
    std::string typeName() const {return type_name;}
    static std::string instanceTypeName() {return type_name;}

private:
    constexpr static const char *type_name = "databaseMeta";
};

// AWARE: Table/Field aliases __WILL NOT__ be looked up when calling from
// this level or below. Use Analysis::* if you need aliasing.
typedef class SchemaInfo : public MappedDBMeta<DatabaseMeta,
                                               IdentityMetaKey> {
public:
    SchemaInfo() : MappedDBMeta(0) {}
    ~SchemaInfo() {}

    std::string typeName() const {return type_name;}
    static std::string instanceTypeName() {return type_name;}

private:
    constexpr static const char *type_name = "schemaInfo";

    std::string serialize(const DBObject &parent) const
    {
        FAIL_TextMessageError("SchemaInfo can not be serialized!");
    }
} SchemaInfo;

bool
IsMySQLTypeNumeric(enum_field_types t);

