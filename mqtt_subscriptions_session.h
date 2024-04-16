#ifndef MQTT_SUBSCRIPTIONS_SESSION_H
#define MQTT_SUBSCRIPTIONS_SESSION_H

#include "mqtt_subscriptions.h"
#include "mqtt_subscribe_packet.h"

namespace Mqtt
{
    class SessionSubscriptionData : public SubscriptionNodeData
    {
    public:
        SessionSubscriptionData() = default;
        ~SessionSubscriptionData() override;

    public:
        const SubscribeOptions & options() const;
        SubscribeOptions & options();
        void setOptions(const SubscribeOptions & opts);

        quint32 identifier() const;
        void setIdentifier(quint32 identifier);

        Mqtt::QoS maxQoS() const;

        bool isNew() const;
        void setNew(bool value);

    private:
        bool m_new = true;
        quint32 m_identifier = 0;
        SubscribeOptions m_subopts = { QoS::Value_0, false, false, SubscribeOptions::RetainHandling::SendAtSubscribe };
    };

    inline const SubscribeOptions & SessionSubscriptionData::options() const       { return m_subopts;              }
    inline SubscribeOptions & SessionSubscriptionData::options()                   { return m_subopts;              }
    inline void SessionSubscriptionData::setOptions(const SubscribeOptions & opts) { m_subopts = opts;              }
    inline quint32 SessionSubscriptionData::identifier() const                     { return m_identifier;           }
    inline void SessionSubscriptionData::setIdentifier(quint32 identifier)         { m_identifier = identifier;     }
    inline Mqtt::QoS SessionSubscriptionData::maxQoS() const                       { return m_subopts.maximumQoS(); }
    inline bool SessionSubscriptionData::isNew() const                             { return m_new;                  }
    inline void SessionSubscriptionData::setNew(bool value)                        { m_new = value;                 }

    typedef Subscriptions SessionSubscriptions;
}

#endif // MQTT_SUBSCRIPTIONS_SESSION_H
