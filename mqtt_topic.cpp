#include "mqtt_topic.h"
#include "mqtt_special_symbols.h"

using namespace Mqtt;

Topic::Topic(const QString & topic)
    :m_destination(All)
    ,m_consumer()
    ,m_value(topic)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    ,m_parts(m_value.splitRef(QChar(SpecialSymbols::Slash), Qt::KeepEmptyParts, Qt::CaseSensitive))
#else
    ,m_parts(QStringView{m_value}.split(QChar(SpecialSymbols::Slash), Qt::KeepEmptyParts, Qt::CaseSensitive))
#endif
{
    if (!m_parts.isEmpty() && m_parts[0] == QStringLiteral("$share")) {
        m_destination = Shared;
        m_parts.erase(m_parts.begin());
        if (!m_parts.isEmpty()) {
            m_consumer = m_parts[0].toString();
            m_parts.erase(m_parts.begin());
        }
    }
    else if (m_value.startsWith(SpecialSymbols::Dollar)) {
        m_destination = System;
    }
}

bool Topic::isValidForSubscribe() const
{
    if (m_value.isEmpty() || m_value.contains(SpecialSymbols::Null))
        return false;

    if (Shared == m_destination) {
        if (m_consumer.isEmpty()
              || m_consumer.contains(SpecialSymbols::Hash)
                || m_consumer.contains(SpecialSymbols::Plus))
            return false;
        if (m_parts.isEmpty() || (m_parts[0].isEmpty() && m_parts.count() == 1))
            return false;
    }

    int hash_count = 0;
    for (auto & p: m_parts)
        if (p == SpecialSymbols::Hash)
            ++hash_count;

    return (hash_count == 0 || (hash_count == 1 && m_parts.last() == SpecialSymbols::Hash));
}

bool Topic::isValidForPublish() const
{
    if (m_value.isEmpty()
          || m_value.startsWith(QStringLiteral("$share"))
            || m_value.contains(SpecialSymbols::Null))
        return false;

    for (auto & p: m_parts)
        if (p == SpecialSymbols::Hash || p == SpecialSymbols::Plus)
            return false;

    return true;
}
