#ifndef MQTT_SUBSCRIPTIONS_SHARED_H
#define MQTT_SUBSCRIPTIONS_SHARED_H

#include "mqtt_subscriptions.h"
#include "mqtt_subscribe_packet.h"
#include "mqtt_session.h"

namespace Mqtt
{
    template <typename T>
    class RoundRobinList : public QList<T>
    {
    public:
        // prevent QList<->RoundRobinList swaps
        inline void swap(RoundRobinList<T> &other) noexcept { QList<T>::swap(other); std::swap(head_index, other.head_index); }
        inline void swap(int i, int j) { QList<T>::swapItemsAt(i, j); }

        inline T &head()             { return QList<T>::operator [](head_index); }
        inline const T &head() const { return QList<T>::at(head_index); }
        inline void selectNext()     { head_index++; if (head_index >= QList<T>::size()) { head_index = QList<T>::size() <= 0 ? -1 : 0; } }

    private:
        qint32 head_index = -1;
    };

    class SubscriptionOptionsSubscriptionIdPair
    {
    public:
        SubscriptionOptionsSubscriptionIdPair() = default;
        SubscriptionOptionsSubscriptionIdPair(const SubscribeOptions & opts, quint32 id) : options(opts), identifier(id) { }

    public:
        SubscribeOptions options = { QoS::Value_0, false, false, SubscribeOptions::RetainHandling::SendAtSubscribe };
        quint32 identifier = 0;
    };

    class SharedSubscriptionSessionData
    {
    public:
        SessionWPtr session;
        SubscriptionOptionsSubscriptionIdPair data;
    };

    typedef RoundRobinList<SharedSubscriptionSessionData> SharedSubscriptionSessionsList;
    typedef QMap<QString, SharedSubscriptionSessionsList> SharedConsumersSessionsList;

    class SharedSubscriptionData: public SubscriptionNodeData
    {
    public:
        SharedSubscriptionData() = default;
        ~SharedSubscriptionData() override;

        void add(const QString & consumer, SessionPtr session, SubscribeOptions options, quint32 subscriptionId);
        int remove(const QString & consumer, SessionPtr session);
        SharedConsumersSessionsList & consumers();
        bool isEmpty() const;

    private:
        SharedConsumersSessionsList m_consumers;
    };

    typedef Subscriptions SharedSubscriptions;

    inline SharedConsumersSessionsList & SharedSubscriptionData::consumers() { return m_consumers;           }
    inline bool SharedSubscriptionData::isEmpty() const                      { return m_consumers.isEmpty(); }
}

#endif // MQTT_SUBSCRIPTIONS_SHARED_H
