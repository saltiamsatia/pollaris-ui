#include <SignableTransaction.hpp>

#include <QSet>
#include <QDebug>

SignableTransaction::SignableTransaction(QJsonObject json) : QObject(nullptr), m_json(json) {
    auto keys = json.keys();
    if (QSet<QString>(keys.begin(), keys.end()) != QSet<QString>{
            QStringLiteral("expiration"),
            QStringLiteral("ref_block_num"),
            QStringLiteral("ref_block_prefix"),
            QStringLiteral("max_net_usage_words"),
            QStringLiteral("max_cpu_usage_ms"),
            QStringLiteral("delay_sec"),
            QStringLiteral("context_free_actions"),
            QStringLiteral("actions"),
            QStringLiteral("transaction_extensions"),
            QStringLiteral("signatures"),
            QStringLiteral("context_free_data")})
        qCritical() << "Creating SignableTransaction from invalid JSON:" << json;
}
