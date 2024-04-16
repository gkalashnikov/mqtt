#ifndef MQTT_SUBSCRIBE_PACKET_H
#define MQTT_SUBSCRIBE_PACKET_H

#include "mqtt_control_packet.h"

namespace Mqtt
{
    class SubscribeOptions
    {
    public:
        enum class RetainHandling : quint8
        {
             SendAtSubscribe                   = 0 /*! Send retained messages at the time of the subscribe */
            ,SendAtSubscribeForNewSubscription = 1 /*! Send retained messages at subscribe only if the subscription does not currently exist */
            ,NotSendAtSubscribe                = 2 /*! Do not send retained messages at the time of the subscribe */
        };

    public:
        inline SubscribeOptions()
            :maxQoS(0)
            ,NL(0)
            ,RAP(0)
            ,retainHandling(0)
            ,reserved(0)
        {

        }

        inline SubscribeOptions(QoS maxQoS, bool noLocal, bool retainAsPublished, RetainHandling retainHandling)
            :maxQoS(static_cast<quint8>(maxQoS))
            ,NL(noLocal)
            ,RAP(retainAsPublished)
            ,retainHandling(static_cast<quint8>(retainHandling))
            ,reserved(0)
        {

        }

    public:
        /*!
         * maxQoS represent Maximum QoS field.
         * This gives the maximum QoS level at which the Server can send Application Messages to the Client.
         * It is a Protocol Error if the Maximum QoS field has the value 3
         */
        inline QoS maximumQoS() const        { return static_cast<QoS>(maxQoS);     }
        inline void setMaximumQoS(QoS value) { maxQoS = static_cast<quint8>(value); }

        /*!
         * NL represents the No Local option.
         * If the value is 1, Application Messages MUST NOT be forwarded to a connection with a ClientID equal to the ClientID of the publishing connection [MQTT-3.8.3-3].
         * It is a Protocol Error to set the No Local bit to 1 on a Shared Subscription
         */
        inline bool noLocal() const        { return NL;  }
        inline void setNoLocal(bool value) { NL = value; }

        /*!
         * RAP represents the Retain As Published option.
         * If 1, Application Messages forwarded using this subscription keep the RETAIN flag they were published with.
         * If 0, Application Messages forwarded using this subscription have the RETAIN flag set to 0.
         * Retained messages sent when the subscription is established have the RETAIN flag set to 1
         */
        inline bool retainAsPublished() const        { return RAP;  }
        inline void setRetainAsPublished(bool value) { RAP = value; }

        /*!
         * retainHandling represents the Retain Handling option.
         * This option specifies whether retained messages are sent when the subscription is established.
         * This does not affect the sending of retained messages at any point after the subscribe.
         * If there are no retained messages matching the Topic Filter, all of these values act the same.
         * The values are:
         * 0 = Send retained messages at the time of the subscribe
         * 1 = Send retained messages at subscribe only if the subscription does not currently exist
         * 2 = Do not send retained messages at the time of the subscribe
         * It is a Protocol Error to send a Retain Handling value of 3.
         */
        inline RetainHandling retainHandlingType() const        { return static_cast<SubscribeOptions::RetainHandling>(retainHandling); }
        inline void setRetainHandlingType(RetainHandling value) { retainHandling = static_cast<quint8>(value); }

        /*!
         * Subscription Options bits are reserved for future use.
         * The Server MUST treat a SUBSCRIBE packet as malformed if any of Reserved bits in the Payload are non-zero
         */
        inline quint8 reservedValue() const { return reserved; }

    private:
        quint8 maxQoS         : 2;
        quint8 NL             : 1;
        quint8 RAP            : 1;
        quint8 retainHandling : 2;
        quint8 reserved       : 2;
    };

    class SubscribeVariableHeader
    {
    public:
        quint16 packetId;
        Properties props;
    };

    class SubscribePacket : public ControlPacket
    {
    public:
        SubscribePacket();
        ~SubscribePacket() override;

    public:
        quint16 packetId() const;
        void setPacketId(quint16 id);

        Properties & properties();
        QList<QPair<QString, SubscribeOptions> > & topics();

    public:
        QByteArray serialize(Version protocolVersion) const;
        bool unserialize(const QByteArray & data, Version protocolVersion);

    private:
        bool isPropertiesValid(const Properties & props);

    private:
        SubscribeVariableHeader m_header;
        QList<QPair<QString, SubscribeOptions> > m_topics;
    };

    inline quint16 SubscribePacket::packetId() const                            { return m_header.packetId; }
    inline void SubscribePacket::setPacketId(quint16 id)                        { m_header.packetId = id;   }
    inline Properties & SubscribePacket::properties()                           { return m_header.props;    }
    inline QList<QPair<QString, SubscribeOptions> > & SubscribePacket::topics() { return m_topics;          }

    typedef QPair<QString, SubscribeOptions> SubscriptionPair;

    class UnsubscribePacket : public ControlPacket
    {
    public:
        UnsubscribePacket();
        ~UnsubscribePacket() override;

    public:
        quint16 packetId() const;
        void setPacketId(quint16 id);

        Properties & properties();
        QStringList & topics();

    public:
        QByteArray serialize(Version protocolVersion) const;
        bool unserialize(const QByteArray & data, Version protocolVersion);

    private:
        bool isPropertiesValid(const Properties & props);

    private:
        SubscribeVariableHeader m_header;
        QStringList m_topics;
    };

    inline quint16 UnsubscribePacket::packetId() const      { return m_header.packetId; }
    inline void UnsubscribePacket::setPacketId(quint16 id)  { m_header.packetId = id;   }
    inline Properties & UnsubscribePacket::properties()     { return m_header.props;    }
    inline QStringList & UnsubscribePacket::topics()        { return m_topics;          }


    class SubscribeAckPacket : public ControlPacket
    {
    protected:
        SubscribeAckPacket(FixedHeader fixedHeader);

    public:
        SubscribeAckPacket();
        ~SubscribeAckPacket() override;

    public:
        quint16 packetId() const;
        void setPacketId(quint16 id);

        Properties & properties();
        QList<ReasonCodeV5> & returnCodes();

    public:
        QByteArray serialize(Version protocolVersion, qint32 maxPacketSize) const;
        bool unserialize(const QByteArray & data, Version protocolVersion);

    private:
        bool isPropertiesValid(const Properties & props);

    private:
        mutable SubscribeVariableHeader m_header;
        QList<ReasonCodeV5> m_returnCodes;
    };

    inline quint16 SubscribeAckPacket::packetId() const            { return m_header.packetId; }
    inline void SubscribeAckPacket::setPacketId(quint16 id)        { m_header.packetId = id;   }
    inline Properties & SubscribeAckPacket::properties()           { return m_header.props;    }
    inline QList<ReasonCodeV5> & SubscribeAckPacket::returnCodes() { return m_returnCodes;     }


    class UnsubscribeAckPacket : public SubscribeAckPacket
    {
    public:
        UnsubscribeAckPacket();
        ~UnsubscribeAckPacket();
    };
}

#endif // MQTT_SUBSCRIBE_PACKET_H
