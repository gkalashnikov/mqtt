#ifndef MQTT_STORE_PUBLISH_UNIT_H
#define MQTT_STORE_PUBLISH_UNIT_H

#include "mqtt_publish_packet.h"
#include "mqtt_enum_qos.h"
#include <QDateTime>

namespace Mqtt
{
    namespace Store
    {
        class PublishUnit
        {
        public:
            PublishUnit() = default;
            PublishUnit(const QString & key, const QString & clientId, const PublishPacket & packet);
            ~PublishUnit();

        public:
            Mqtt::QoS QoS() const;
            bool expired() const;
            qint64 elapsed() const;
            const PublishPacket & packet() const;
            void setPacket(const PublishPacket & packet);
            const QString & clientId() const;
            const QString & key() const;
            void setKey(const QString & key);
            void beforeSend();
            QByteArray serialize() const;
            void unserialize(const QByteArray & data);
            bool isLoaded();
            void setLoaded(bool loaded);
            void unload();

        private:
            bool          m_loaded          = true;
            qint64        m_expiry_interval = 0;
            qint64        m_initial_time    = 0;
            QString       m_client_id;
            QString       m_key;
            PublishPacket m_packet;
        };

        inline QoS PublishUnit::QoS() const                              { return m_packet.QoS(); }
        inline const QString & PublishUnit::clientId() const             { return m_client_id;    }
        inline const QString & PublishUnit::key() const                  { return m_key;          }
        inline void PublishUnit::setKey(const QString & key)             { m_key = key;           }
        inline bool PublishUnit::isLoaded()                              { return m_loaded;       }
        inline void PublishUnit::setLoaded(bool loaded)                  { m_loaded = loaded;     }
        inline const PublishPacket & PublishUnit::packet() const         { return m_packet;       }
        inline bool PublishUnit::expired() const                         { return (m_expiry_interval != 0 && (elapsed() >= m_expiry_interval)); }
        inline qint64 PublishUnit::elapsed() const                       { return (QDateTime::currentSecsSinceEpoch() - m_initial_time);        }
    }
}

#endif // MQTT_STORE_PUBLISH_UNIT_H
