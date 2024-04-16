#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "mqtt_connection.h"
#include "mqtt_publish_packet.h"

#include <QObject>
#include <QQueue>

namespace Mqtt
{
    class Client : public Connection, public Connection::IPacketHandler
    {
        Q_OBJECT
    public:
        explicit Client(QObject * socketController, QObject * parent = Q_NULLPTR);
        ~Client() override;

    signals:
        void messageReceived(const QString & topic, const QByteArray & payload, const Properties & properties, QoS qos, bool retain);
        void messageDelivered(qint32 id, ReasonCodeV5 code);
        void messageNotDelivered(qint32 id, ReasonCodeV5 code);

    private slots:
        void stateChanged(State state);

    public:
        qint32 publish(const QString & topic, const QByteArray & payload, QoS qos = QoS::Value_0, bool retain = false);
        qint32 publish(const QString & topic, const QByteArray & payload, const Properties & properties, QoS qos = QoS::Value_0, bool retain = false);

    protected:
        void timerEvent(QTimerEvent * event) override;

        bool handleControlPacket(PacketType type, const QByteArray & data) override;
        void oneSecondTimerEvent() override;

    private:
        void handlePublishPacket(const QByteArray & data);
        void handlePubAckPacket(const QByteArray & data);
        void handlePubRecPacket(const QByteArray & data);
        void handlePubRelPacket(const QByteArray & data);
        void handlePubCompPacket(const QByteArray & data);

    private:
        void scheduleCheckMessageQueue();
        void executeCheckMessageQueue();

        void enqueuePublishMessage(const PublishPacket & packet);
        void packetDelivered(quint16 id, ReasonCodeV5 code, bool endOfDelivery = true);

        void checkMessageQueue();

        bool processBrokerAlias(PublishPacket & pub);
        bool processClientAlias(PublishPacket & pub);

    private:
        qint32 m_timer_id;

        QQueue<PublishPacket> m_publish_queue;
        QMap<quint16, PublishPacket> m_publish_in_fligth;
        QSet<quint16> m_publish_rec_received;

        typedef QMap<quint16, QString> ClientAliasContainer;
        typedef QMap<QString, quint16> BrokerAliasContainer;

        BrokerAliasContainer m_broker_aliases;
        ClientAliasContainer m_client_aliases;
    };

    typedef QSharedPointer<Client> ClientPtr;
    typedef QWeakPointer<Client> ClientWPtr;
}

#endif // MQTT_CLIENT_H
