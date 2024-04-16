#ifndef MQTT_CONTROL_PACKET_H
#define MQTT_CONTROL_PACKET_H

#include "mqtt_enums.h"

#include <QPair>
#include <QVariant>
#include <QByteArray>

namespace Mqtt
{
    typedef QPair<QString, QString> UserProperty;
    typedef QPair<PropertyId, QVariant> Property;
    typedef QList<Property> Properties;

    class ControlPacket
    {
    public:
        struct FixedHeader
        {
            Flags      flags      : 4;
            PacketType packetType : 4;
        };

    public:
        ControlPacket(FixedHeader fixedHeader);
        virtual ~ControlPacket();

    public:
        static PacketType extractType(const QByteArray & data);
        static qint64 packetFullLength(const QByteArray & data);
        static QVariant property(const Properties & source, PropertyId target);
        static void setProperty(Properties & source, PropertyId target, QVariant value);
        static void removeProperty(Properties & source, PropertyId target);
        static QByteArray serializeProperties(const Properties & props);
        static bool unserializeProperties(const quint8 * buf, qint64 * remainingLength, size_t * bytesCount, Properties & props);

     public:
        PacketType type() const;
        ReasonCodeV5 unserializeReasonCode() const;
        QString unserializeReasonString() const;

    protected:
        qint64 unserialize(const QByteArray & data, size_t * bytesCount);

    protected:
        FixedHeader  m_headerFix;
        ReasonCodeV5 m_unserializeReasonCode;
        QString      m_unserializeReasonString;
    };

    typedef QSharedPointer<ControlPacket> ControlPacketPtr;

    inline PacketType ControlPacket::extractType(const QByteArray & data) { return reinterpret_cast<const FixedHeader*>(data.constData())->packetType; }
    inline PacketType ControlPacket::type() const                         { return m_headerFix.packetType;    }
    inline ReasonCodeV5 ControlPacket::unserializeReasonCode() const      { return m_unserializeReasonCode;   }
    inline QString ControlPacket::unserializeReasonString() const         { return m_unserializeReasonString; }
}

#endif // MQTT_CONTROL_PACKET_H
