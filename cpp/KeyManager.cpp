#include <KeyManager.hpp>
#include <Action.hpp>

#include <QJsonDocument>
#include <QSettings>

// In this file alone, we can use FC to do our crypto and serializations
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/private_key.hpp>
#include <fc/crypto/signature.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/bitutil.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filter/zlib.hpp>

// Types from EOSIO, to serialize and sign
// EOSIO code used under MIT license:
//
// Copyright (c) 2017-2019 block.one and its contributors.  All rights reserved.
//
// The MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
namespace bio = boost::iostreams;

using digest_type = fc::sha256;
using signature_type = fc::crypto::signature;
using private_key_type = fc::crypto::private_key;
using public_key_type = fc::crypto::public_key;
using extensions_type = std::vector<std::pair<uint16_t,std::vector<char>>>;
using transaction_id_type = fc::sha256;
using block_id_type = fc::sha256;
using bytes = std::vector<char>;
using std::vector;
using std::string;

struct chain_id_type : public fc::sha256 {
    using fc::sha256::sha256;

    template<typename T>
    inline friend T& operator>>( T& ds, chain_id_type& cid ) {
        ds.read( cid.data(), cid.data_size() );
        return ds;
    }

    void reflector_init()const;

    chain_id_type() = default;
};

struct name;

namespace fc {
class variant;
void to_variant(const name& c, fc::variant& v);
void from_variant(const fc::variant& v, name& check);
} // fc

static constexpr uint64_t char_to_symbol(char c) {
    if(c >= 'a' && c <= 'z')
        return (c - 'a') + 6;
    if(c >= '1' && c <= '5')
        return (c - '1') + 1;
    return 0;
}

static constexpr uint64_t string_to_uint64_t(const char str[]) {
    uint64_t n = 0;
    int i = 0;
    for ( ; str[i] && i < 12; ++i) {
        // NOTE: char_to_symbol() returns char type, and without this explicit
        // expansion to uint64 type, the compilation fails at the point of usage
        // of string_to_name(), where the usage requires constant (compile time) expression.
        n |= (char_to_symbol(str[i]) & 0x1f) << (64 - 5 * (i + 1));
    }

    // The for-loop encoded up to 60 high bits into uint64 'name' variable,
    // if (strlen(str) > 12) then encode str[12] into the low (remaining)
    // 4 bits of 'name'
    if (i == 12)
        n |= char_to_symbol(str[12]) & 0x0F;
    return n;
}

struct name {
private:
    uint64_t value = 0;

    friend struct fc::reflector<name>;
    friend void fc::from_variant(const fc::variant& v, name& check);

    void set(std::string str) {
        const auto len = str.size();
        if (len > 13) {
            qCritical("Asked to set name from string, but string is too long!");
            return;
        }
        value = string_to_uint64_t(str.c_str());
    }

public:
    explicit name(std::string str) { set(str); }
    constexpr name() = default;

    std::string to_string()const {
        static const char* charmap = ".12345abcdefghijklmnopqrstuvwxyz";

        std::string str(13,'.');

        uint64_t tmp = value;
        for(uint32_t i = 0; i <= 12; ++i) {
            char c = charmap[tmp & (i == 0 ? 0x0f : 0x1f)];
            str[12-i] = c;
            tmp >>= (i == 0 ? 4 : 5);
        }

        boost::algorithm::trim_right_if(str, [](char c){ return c == '.'; });
        return str;
    }

    constexpr uint64_t to_uint64_t()const { return value; }
};
using account_name = name;
using action_name = name;
using permission_name = name;

namespace fc {
  void to_variant(const name& c, fc::variant& v) { v = c.to_string(); }
  void from_variant(const fc::variant& v, name& check) { check.set( v.get_string() ); }
} // fc

// Types from Pollaris contract, to serialize
using Tags = vector<string>;
using Timestamp = fc::time_point_sec;
struct ContestantDescriptor {
    string name;
    string description;
    Tags tags;

    friend bool operator<(const ContestantDescriptor& a, const ContestantDescriptor& b) {
        return std::tie(a.name, a.description) < std::tie(b.name, b.description);
    }
};
FC_REFLECT(ContestantDescriptor, (name)(description)(tags))
struct FullOpinions {
    std::map<uint64_t, int32_t> contestantOpinions;
    std::map<ContestantDescriptor, int32_t> writeInOpinions;
};
FC_REFLECT(FullOpinions, (contestantOpinions)(writeInOpinions))
// Contract action argument lists
struct voter_add {
    string groupName;
    name voter;
    uint32_t weight;
    Tags tags;
};
FC_REFLECT(voter_add, (groupName)(voter)(weight)(tags))
struct voter_remove {
    string groupName;
    name voter;
};
FC_REFLECT(voter_remove, (groupName)(voter))
struct group_copy {
    string groupName;
    string newName;
};
FC_REFLECT(group_copy, (groupName)(newName))
struct group_rename {
    string groupName;
    string newName;
};
FC_REFLECT(group_rename, (groupName)(newName))
struct cntst_new {
    uint64_t groupId;
    string name;
    string description;
    vector<ContestantDescriptor> contestants;
    Timestamp begin;
    Timestamp end;
    Tags tags;
};
FC_REFLECT(cntst_new, (groupId)(name)(description)(contestants)(begin)(end)(tags))
struct cntst_modify {
    uint64_t groupId;
    uint64_t contestId;
    fc::optional<string> newName;
    fc::optional<string> newDescription;
    fc::optional<Tags> newTags;
    vector<uint64_t> deleteContestants;
    vector<ContestantDescriptor> addContestants;
    fc::optional<Timestamp> newBegin;
    fc::optional<Timestamp> newEnd;
};
FC_REFLECT(cntst_modify, (groupId)(contestId)(newName)(newDescription)(newTags)
                         (deleteContestants)(addContestants)(newBegin)(newEnd))
struct cntst_tally {
    uint64_t groupId;
    uint64_t contestId;
};
FC_REFLECT(cntst_tally, (groupId)(contestId))
struct cntst_delete {
    uint64_t groupId;
    uint64_t contestId;
};
FC_REFLECT(cntst_delete, (groupId)(contestId))
struct dcsn_set {
    uint64_t groupId;
    uint64_t contestId;
    name voterName;
    FullOpinions opinions;
    Tags tags;
};
FC_REFLECT(dcsn_set, (groupId)(contestId)(voterName)(opinions)(tags))
// End Pollaris contract types, resume EOSIO types

struct permission_level {
    account_name actor;
    permission_name permission;

    permission_level(){}
    // Add a conversion constructor from our "actor@permission" QString format
    permission_level(QString p) {
        auto atIndex = p.indexOf('@');
        if (atIndex == -1) {
            qCritical("Asked to create eosio::permission_level from QString, but QString has no @ in it!");
            return;
        }
        actor = name(p.left(atIndex).toStdString());
        permission = name(p.mid(atIndex+1).toStdString());
    }
};

struct action {
    account_name account;
    action_name name;
    vector<permission_level> authorization;
    bytes data;

    action(){}
    // Add a conversion constructor from our Qt Action type
    action(const QObject* a) {
        if (a == nullptr) {
            qCritical("Asked to create eosio::action from Action, Action is nullptr!");
            return;
        }
        account = ::name(a->property("account").toString().toStdString());
        name = ::name(a->property("actionName").toString().toStdString());
        auto auths = a->property("authorizations").toStringList();
        authorization.reserve(auths.length());
        authorization.assign(auths.begin(), auths.end());

        // Load arguments into structs for serialization
        auto jsonArguments = QJsonDocument(a->property("arguments").toJsonObject()).toJson(QJsonDocument::Compact);
        switch(name.to_uint64_t()) {
        case string_to_uint64_t("voter.add"): {
            auto args = fc::json::from_string(jsonArguments.toStdString()).as<voter_add>();
            data = fc::raw::pack(args);
            break;
        }
        case string_to_uint64_t("voter.remove"): {
            auto args = fc::json::from_string(jsonArguments.toStdString()).as<voter_remove>();
            data = fc::raw::pack(args);
            break;
        }
       case string_to_uint64_t("group.copy"): {
            auto args = fc::json::from_string(jsonArguments.toStdString()).as<group_copy>();
            data = fc::raw::pack(args);
            break;
        }
       case string_to_uint64_t("group.rename"): {
            auto args = fc::json::from_string(jsonArguments.toStdString()).as<group_rename>();
            data = fc::raw::pack(args);
            break;
        }
       case string_to_uint64_t("cntst.new"): {
            auto args = fc::json::from_string(jsonArguments.toStdString()).as<cntst_new>();
            data = fc::raw::pack(args);
            break;
        }
       case string_to_uint64_t("cntst.modify"): {
            auto args = fc::json::from_string(jsonArguments.toStdString()).as<cntst_modify>();
            data = fc::raw::pack(args);
            break;
        }
       case string_to_uint64_t("cntst.tally"): {
            auto args = fc::json::from_string(jsonArguments.toStdString()).as<cntst_tally>();
            data = fc::raw::pack(args);
            break;
        }
       case string_to_uint64_t("cntst.delete"): {
            auto args = fc::json::from_string(jsonArguments.toStdString()).as<cntst_delete>();
            data = fc::raw::pack(args);
            break;
        }
       case string_to_uint64_t("dcsn.set"): {
            auto args = fc::json::from_string(jsonArguments.toStdString()).as<dcsn_set>();
            data = fc::raw::pack(args);
            break;
        }
        default:
            qCritical("Asked to create eosio::action from Action, but Action has unknown name");
        }
    }
    static vector<action> fromQList(QList<QObject*> list) {
        return vector<action>(list.begin(), list.end());
    }
};

struct transaction_header {
    fc::time_point_sec expiration; ///< the time at which a transaction expires
    uint16_t ref_block_num = 0U; ///< specifies a block num in the last 2^16 blocks.
    uint32_t ref_block_prefix = 0UL; ///< specifies the lower 32 bits of the blockid at get_ref_blocknum
    /// upper limit on total network bandwidth (in 8 byte words) billed for this transaction
    fc::unsigned_int max_net_usage_words = 0UL;
    uint8_t max_cpu_usage_ms = 0; ///< upper limit on the total CPU time billed for this transaction
    /// number of seconds to delay this transaction for during which it may be canceled.
    fc::unsigned_int delay_sec = 0UL;
};

struct transaction : public transaction_header {
    vector<action> context_free_actions;
    vector<action> actions;
    extensions_type transaction_extensions;

   transaction_id_type id()const {
       digest_type::encoder enc;
       fc::raw::pack( enc, *this );
       return enc.result();
   }

    void set_reference_block( const block_id_type& reference_block ) {
        ref_block_num = fc::endian_reverse_u32(reference_block._hash[0]);
        ref_block_prefix = reference_block._hash[1];
    }
    digest_type sig_digest(const chain_id_type& chain_id, const vector<bytes>& cfd = vector<bytes>())const;
};

struct signed_transaction : public transaction {
   signed_transaction() = default;

   vector<signature_type> signatures;
   vector<bytes> context_free_data; ///< for each context-free action, there is an entry here

   signature_type sign(const private_key_type& key, const chain_id_type& chain_id)const {
       return key.sign(sig_digest(chain_id, context_free_data));
   }
};

struct packed_transaction : fc::reflect_init {
    enum class compression_type {
        none = 0,
        zlib = 1,
    };

    explicit packed_transaction(const signed_transaction& t, compression_type _compression = compression_type::none)
        :signatures(t.signatures), compression(_compression), unpacked_trx(t), trx_id(unpacked_trx.id()) {
        local_pack_transaction();
        local_pack_context_free_data();
    }

    explicit packed_transaction(signed_transaction&& t, compression_type _compression = compression_type::none)
        :signatures(t.signatures), compression(_compression), unpacked_trx(std::move(t)), trx_id(unpacked_trx.id()) {
        local_pack_transaction();
        local_pack_context_free_data();
    }

private:
    void local_pack_transaction();
    void local_pack_context_free_data();

    friend struct fc::reflector<packed_transaction>;
    friend struct fc::reflector_init_visitor<packed_transaction>;
    friend struct fc::has_reflector_init<packed_transaction>;
    void reflector_init();
private:
    vector<signature_type> signatures;
    fc::enum_type<uint8_t,compression_type> compression;
    bytes packed_context_free_data;
    bytes packed_trx;

    // cache unpacked trx, for thread safety do not modify after construction
    signed_transaction unpacked_trx;
    transaction_id_type trx_id;
};


FC_REFLECT(transaction_header, (expiration)(ref_block_num)(ref_block_prefix)
           (max_net_usage_words)(max_cpu_usage_ms)(delay_sec))
FC_REFLECT_DERIVED(transaction, (transaction_header),
                   (context_free_actions)(actions)(transaction_extensions))
FC_REFLECT_DERIVED(signed_transaction, (transaction), (signatures)(context_free_data))
FC_REFLECT(permission_level, (actor)(permission))
FC_REFLECT(action, (account)(name)(authorization)(data))
FC_REFLECT(name, (value))
FC_REFLECT_ENUM(packed_transaction::compression_type, (none)(zlib))
FC_REFLECT(packed_transaction, (signatures)(compression)(packed_context_free_data)(packed_trx))


digest_type transaction::sig_digest(const chain_id_type& chain_id, const vector<bytes>& cfd)const {
    digest_type::encoder enc;
    fc::raw::pack(enc, chain_id);
    fc::raw::pack(enc, *this);
    if(cfd.size()) {
        fc::raw::pack(enc, digest_type::hash(cfd));
    } else {
        fc::raw::pack(enc, digest_type());
    }
    return enc.result();
}

static bytes pack_transaction(const transaction& t) {
    return fc::raw::pack(t);
}

static bytes pack_context_free_data(const vector<bytes>& cfd ) {
    if( cfd.size() == 0 )
        return bytes();

    return fc::raw::pack(cfd);
}

static bytes zlib_compress_context_free_data(const vector<bytes>& cfd ) {
   if( cfd.size() == 0 )
      return bytes();

   bytes in = pack_context_free_data(cfd);
   bytes out;
   bio::filtering_ostream comp;
   comp.push(bio::zlib_compressor(bio::zlib::best_compression));
   comp.push(bio::back_inserter(out));
   bio::write(comp, in.data(), in.size());
   bio::close(comp);
   return out;
}

static bytes zlib_compress_transaction(const transaction& t) {
   bytes in = pack_transaction(t);
   bytes out;
   bio::filtering_ostream comp;
   comp.push(bio::zlib_compressor(bio::zlib::best_compression));
   comp.push(bio::back_inserter(out));
   bio::write(comp, in.data(), in.size());
   bio::close(comp);
   return out;
}

void packed_transaction::local_pack_transaction()
{
    try {
        switch(compression) {
        case compression_type::none:
            packed_trx = pack_transaction(unpacked_trx);
            break;
        case compression_type::zlib:
            packed_trx = zlib_compress_transaction(unpacked_trx);
            break;
        }
    } FC_CAPTURE_AND_RETHROW((compression))
}

void packed_transaction::local_pack_context_free_data()
{
    try {
        switch(compression) {
        case compression_type::none:
            packed_context_free_data = pack_context_free_data(unpacked_trx.context_free_data);
            break;
        case compression_type::zlib:
            packed_context_free_data = zlib_compress_context_free_data(unpacked_trx.context_free_data);
            break;
        }
    } FC_CAPTURE_AND_RETHROW((compression))
}
// End EOSIO import code

KeyManager::KeyManager(QObject *parent) : QObject(parent) {}

SignableTransaction* KeyManager::prepareForSigning(MutableTransaction* transaction) {
    if (transaction == nullptr) {
        qDebug() << "Asked to prepare transaction for signing, but transaction is nullptr!";
        return nullptr;
    }
    if (m_blockchain == nullptr) {
        qDebug() << "Asked to prepare transaction for signing, but blockchain has not been set! "
                    "Set blockchain property first.";
        return nullptr;
    }

    if (transaction->refBlockId().isNull())
        transaction->setRefBlockId(m_blockchain->headBlockId());
    if (transaction->expiration().isNull())
        transaction->setExpiration(QDateTime::currentDateTimeUtc().addSecs(10));


    signed_transaction trx;
    trx.expiration = fc::time_point_sec((transaction->expiration().toSecsSinceEpoch()));
    trx.set_reference_block(fc::sha256(transaction->refBlockId().toStdString()));
    trx.max_net_usage_words = transaction->maxNetWords();
    trx.max_cpu_usage_ms = transaction->maxCpuMs();
    trx.delay_sec = transaction->delaySeconds();
    trx.actions = action::fromQList(transaction->actions());

    QByteArray jtrx = fc::json::to_string(trx, fc::time_point::now()+fc::milliseconds(1)).c_str();
    return new SignableTransaction(QJsonDocument::fromJson(jtrx).object());
}

void KeyManager::signTransaction(SignableTransaction* transaction) {
    // TODO: make this for real (actual key management, checking permissions on chain, checking transaction auths...)
    auto key = QString("5KXAfKzbKoBAPCAMbHN4gkwCu3EeidTMvxrVBFqebjs3MmEwxzk"); // Key for followmyvote on private testnet

    signTransaction(transaction, key);
}

void KeyManager::signTransaction(SignableTransaction *transaction, QString privateKey) {
    if (transaction == nullptr) {
        qDebug() << "Asked to sign transaction, but transaction is nullptr!";
        return;
    }

    fc::crypto::private_key fcKey(privateKey.toStdString());

    // Bump the expiration just before signing, as we want short expirations
    transaction->setExpiration(QDateTime::currentDateTimeUtc().addSecs(10));

    auto trxJson = QJsonDocument(transaction->json());
    auto trx = fc::json::from_string(trxJson.toJson().toStdString()).as<signed_transaction>();

    auto sig = trx.sign(fcKey, chain_id_type(fc::sha256(blockchain()->chainId().toStdString())));
    transaction->addSignature(QString::fromStdString(sig.to_string()));
}

BroadcastableTransaction* KeyManager::prepareForBroadcast(SignableTransaction* transaction) {
    if (transaction == nullptr) {
        qDebug() << "Asked to prepare transaction for broadcast, but transaction is nullptr!";
        return nullptr;
    }

    auto t= fc::json::from_string(QJsonDocument(transaction->json()).toJson().toStdString()).as<signed_transaction>();
    packed_transaction ptrx(t, packed_transaction::compression_type::zlib);
    auto id = QByteArray::fromStdString(t.id().str());
    auto json = QByteArray::fromStdString(fc::json::to_string(ptrx, fc::time_point::now() + fc::milliseconds(1)));
    return new BroadcastableTransaction(m_blockchain, id, QJsonDocument::fromJson(json).object());
}

QString KeyManager::createNewKey() {
    QSettings wallet;
    auto key = fc::ecc::private_key::generate();
    auto publicKey = QString::fromStdString(key.get_public_key().to_base58());
    auto secret = key.get_secret();
    wallet.beginGroup("wallet");
    wallet.beginWriteArray("keys");
    wallet.setValue(publicKey, QByteArray(secret.data(), secret.data_size()));
    wallet.sync();
    return publicKey;
}

bool KeyManager::hasPrivateKey(QString publicKey) {
    QSettings wallet;
    return wallet.contains("wallet/keys/" + publicKey);
}

QByteArray KeyManager::getSharedSecret(QString foreignKey, QString myKey) {
    if (!hasPrivateKey(myKey)) {
        qWarning() << "Cannot get shared secret: private key not found in wallet";
        return {};
    }
    auto privateKeySecret = QSettings().value("wallet/keys/" + myKey).toByteArray();
    try {
        auto key = fc::ecc::private_key::regenerate(fc::sha256(privateKeySecret.data(), privateKeySecret.size()));
        auto publicKey = fc::ecc::public_key::from_base58(foreignKey.toStdString());
        auto secret = key.get_shared_secret(publicKey);
        return QByteArray(secret.data(), secret.data_size());
    } catch (fc::exception& e) {
        qWarning() << "Failed to get shared secret" << QString::fromStdString(e.to_detail_string());
    }
}

bool KeyManager::isPublicKey(QString maybeKey) {
    try {
        fc::ecc::public_key::from_base58(maybeKey.toStdString());
        return true;
    } catch (fc::exception&) {
        return false;
    }
}

void decodeAction(QByteArray json, Action* action) {
    if (action == nullptr) return;
    auto decoded = fc::json::from_string(json.toStdString()).as<::action>();

    // Do the easy fields...
    action->setAccount(QString::fromStdString(decoded.account.to_string()));
    action->setActionName(QString::fromStdString(decoded.name.to_string()));
    // Slightly harder...
    QStringList auths;
    auths.reserve(decoded.authorization.size());
    std::transform(decoded.authorization.begin(), decoded.authorization.end(), std::back_inserter(auths),
                   [](const permission_level& p) {
        return Strings::AuthorizationTemplate.arg(QString::fromStdString(p.actor.to_string()),
                                                  QString::fromStdString(p.permission.to_string()));
    });
    action->setAuthorizations(auths);
    // Now the annoying one
    QJsonObject args;
    auto deadline = fc::time_point::now() + fc::milliseconds(1);
    switch(decoded.name.to_uint64_t()) {
    case string_to_uint64_t("voter.add"): {
        auto data = fc::raw::unpack<voter_add>(decoded.data);
        args = QJsonDocument::fromJson(QByteArray::fromStdString(fc::json::to_string(data, deadline))).object();
        break;
    }
    case string_to_uint64_t("voter.remove"): {
        auto data = fc::raw::unpack<voter_remove>(decoded.data);
        args = QJsonDocument::fromJson(QByteArray::fromStdString(fc::json::to_string(data, deadline))).object();
        break;
    }
    case string_to_uint64_t("group.copy"): {
        auto data = fc::raw::unpack<group_copy>(decoded.data);
        args = QJsonDocument::fromJson(QByteArray::fromStdString(fc::json::to_string(data, deadline))).object();
        break;
    }
    case string_to_uint64_t("group.rename"): {
        auto data = fc::raw::unpack<group_rename>(decoded.data);
        args = QJsonDocument::fromJson(QByteArray::fromStdString(fc::json::to_string(data, deadline))).object();
        break;
    }
    case string_to_uint64_t("cntst.new"): {
        auto data = fc::raw::unpack<cntst_new>(decoded.data);
        args = QJsonDocument::fromJson(QByteArray::fromStdString(fc::json::to_string(data, deadline))).object();
        break;
    }
    case string_to_uint64_t("cntst.modify"): {
        auto data = fc::raw::unpack<cntst_modify>(decoded.data);
        args = QJsonDocument::fromJson(QByteArray::fromStdString(fc::json::to_string(data, deadline))).object();
        break;
    }
    case string_to_uint64_t("cntst.tally"): {
        auto data = fc::raw::unpack<cntst_tally>(decoded.data);
        args = QJsonDocument::fromJson(QByteArray::fromStdString(fc::json::to_string(data, deadline))).object();
        break;
    }
    case string_to_uint64_t("cntst.delete"): {
        auto data = fc::raw::unpack<cntst_delete>(decoded.data);
        args = QJsonDocument::fromJson(QByteArray::fromStdString(fc::json::to_string(data, deadline))).object();
        break;
    }
    case string_to_uint64_t("dcsn.set"): {
        auto data = fc::raw::unpack<dcsn_set>(decoded.data);
        args = QJsonDocument::fromJson(QByteArray::fromStdString(fc::json::to_string(data, deadline))).object();
        break;
    }
    }
    action->setArguments(args);
}
