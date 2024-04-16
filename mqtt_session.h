#ifndef MQTT_SESSION_H
#define MQTT_SESSION_H

#include "network_client.h"
#include "mqtt_connect_packet.h"
#include "mqtt_chunk_data_controller.h"
#include "mqtt_subscriptions_session.h"
#include "mqtt_store_publish_container.h"
#include "mqtt_packet_identifier_controller.h"
#include "mqtt_constants.h"
#include "average/move.h"

#include <QDateTime>
#include <QSharedPointer>

class QTimer;

namespace Mqtt
{
    class ConnectPacket;

    class Session : public QObject
    {
        Q_OBJECT
    public:
        explicit Session(QObject * parent = Q_NULLPTR);
        Session(const Session & other) = delete;
        ~Session();

        bool operator ==(const Network::ServerClient & other) const;

    signals:
        void expired();

    protected slots:
        void oneSecondTimer();

    public:
        static constexpr quint32 FlowControlWindowSize = 5;
        typedef QMap<quint16, QString> InFligthContainer;
        typedef QMap<quint16, QString> BrokerAliasContainer;
        typedef QMap<QString, quint16> ClientAliasContainer;

    public:
        void beforeDelete();
        bool hasBeenExpired() const;

        const QString & clientId() const;
        Version protocolVersion() const;
        bool isClean() const;

        bool expectsConnectPacket() const;
        const ConnectPacket & connectPacket() const;
        void setConnectPacket(ConnectPacketPtr connectPacket);

        void setConnection(const Network::ServerClient & conn);
        Network::ServerClient & connection();
        const Network::ServerClient & connection() const;
        bool isConnected() const;
        void clearConnection();

        bool isResponseInformationRequested() const;
        const QString & responseInformation() const;
        bool isProblemInformationRequested() const;

        qint64 elapsed() const;
        qint64 restartElapsed();

        void setClientIdProvidedByServer(bool provided = true);
        bool isClientIdProvideByServer() const;

        void setNormalDisconnectWas(bool normal);
        bool isNormalDisconnected();
        void setExpiryInterval(quint32 seconds);

        ChunkDataController & dataController();

        SessionSubscriptions & subscriptions();

        qint32 maxPacketSize() const;
        quint16 receiveMaximum() const;

        bool hasQuota();

        bool hasPendingPacketsStorer() const;
        void setPendingPacketsStorer(Store::IStorer * storer);
        Store::IStorer * pendingPacketsStorer();
        void addPendingPacket(const PublishPacket & packet);

        void removeAllStoredPackets();
        void cancelAllInFligthPackets();

        bool storePacketId(quint16 id);
        void freePacketId(quint16 id);
        bool isPacketIdUsed(quint16 id);

        Store::PublishUnit * nextPacketToFligth();
        void putPacketToFligth(quint16 id, Store::PublishUnit * unit);
        void packetDelivered(quint16 id, ReasonCodeV5 code, bool freeId);

        quint16 generateId();

        QByteArray serialize() const;
        void unserialize(const QByteArray & data);

        bool isPresent() const;
        void setPresent(bool isPresent);

        void increaseFlowRate(QoS qos);
        double flowRate(QoS qos) const;

        bool isBanned() const;
        void ban(quint32 timeoutSeconds, bool accumulative = true);
        quint32 banTimeout() const;
        void decreaseBanTimeout();
        void syncBanDuration(const Session & other);

        bool processBrokerAlias(PublishPacket & pub);
        bool processClientAlias(PublishPacket & pub);

    private:
        void startTimer();
        void decreaseQuota();
        void increaseQuota();

    private:
        bool m_is_present;
        bool m_is_conn_packet_expects;
        bool m_is_client_id_provided_by_server;
        bool m_is_normal_disconnect;
        bool m_request_response_information;
        bool m_request_problem_information;

        qint64  m_keep_alive_interval;
        qint64  m_expiry_interval;
        qint32  m_packet_size_max;
        quint16 m_receive_maximum;
        quint16 m_quota;
        qint64  m_last_activity_time;
        quint32 m_ban_duration;
        quint32 m_ban_timeout;
        quint16 m_topic_alias_maximum;
        QString m_response_information;

        Network::ServerClient   m_conn;
        ConnectPacketPtr        m_conn_packet;
        ChunkDataController     m_data_controller;
        SessionSubscriptions    m_subscriptions;
        Store::PublishContainer m_pending_packets;
        InFligthContainer       m_in_fligth_packets;
        BrokerAliasContainer    m_broker_aliases;
        ClientAliasContainer    m_client_aliases;

        QTimer                * m_timer;
        PacketIdController      m_idctrl;

        Average::Move<FlowControlWindowSize> m_flow[3];
    };

    inline bool Session::operator ==(const Network::ServerClient & other) const       { return this->m_conn == m_conn; }
    inline bool Session::isPresent() const                                            { return m_is_present; }
    inline void Session::setPresent(bool isPresent)                                   { m_is_present = isPresent; }
    inline bool Session::isBanned() const                                             { return m_ban_timeout > 0; }
    inline quint32 Session::banTimeout() const                                        { return m_ban_timeout; }
    inline void Session::decreaseBanTimeout()                                         { --m_ban_timeout; }
    inline void Session::syncBanDuration(const Session & other)                       { m_ban_duration = other.m_ban_duration; }
    inline bool Session::hasBeenExpired() const                                       { return ((Constants::ForeverSessionInterval == m_expiry_interval) ? false : (elapsed() >= m_expiry_interval)); }
    inline const QString & Session::clientId() const                                  { return m_conn_packet->clientId(); }
    inline Version Session::protocolVersion() const                                   { return m_conn_packet->protocolVersion(); }
    inline bool Session::isClean() const                                              { return m_conn_packet->cleanSession(); }
    inline bool Session::expectsConnectPacket() const                                 { return m_is_conn_packet_expects; }
    inline Network::ServerClient & Session::connection()                              { return m_conn; }
    inline const Network::ServerClient & Session::connection() const                  { return m_conn; }
    inline bool Session::isConnected() const                                          { return !m_conn.isNull(); }
    inline bool Session::isResponseInformationRequested() const                       { return m_request_response_information; }
    inline const QString & Session::responseInformation() const                       { return m_response_information; }
    inline bool Session::isProblemInformationRequested() const                        { return m_request_problem_information; }
    inline const ConnectPacket & Session::connectPacket() const                       { return *m_conn_packet.data();   }
    inline void Session::setConnection(const Network::ServerClient & conn)            { m_conn = conn; restartElapsed(); }
    inline qint64 Session::elapsed() const                                            { return (QDateTime::currentSecsSinceEpoch() - m_last_activity_time); }
    inline qint64 Session::restartElapsed()                                           { return m_last_activity_time = QDateTime::currentSecsSinceEpoch(); }
    inline void Session::setClientIdProvidedByServer(bool provided)                   { m_is_client_id_provided_by_server = provided; }
    inline bool Session::isClientIdProvideByServer() const                            { return m_is_client_id_provided_by_server; }
    inline void Session::setNormalDisconnectWas(bool normal)                          { m_is_normal_disconnect = normal; }
    inline bool Session::isNormalDisconnected()                                       { return m_is_normal_disconnect;   }
    inline void Session::setExpiryInterval(quint32 seconds)                           { m_expiry_interval = seconds ? seconds : Constants::DefaultSessionTimeout; }
    inline ChunkDataController & Session::dataController()                            { return m_data_controller; }
    inline SessionSubscriptions & Session::subscriptions()                            { return m_subscriptions; }
    inline qint32 Session::maxPacketSize() const                                      { return m_packet_size_max; }
    inline quint16 Session::receiveMaximum() const                                    { return m_receive_maximum; }
    inline bool Session::hasQuota()                                                   { return m_quota > 0; }
    inline void Session::decreaseQuota()                                              { --m_quota; }
    inline void Session::increaseQuota()                                              { if (m_quota < m_receive_maximum) ++m_quota; }
    inline bool Session::hasPendingPacketsStorer() const                              { return m_pending_packets.hasStorer(); }
    inline void Session::setPendingPacketsStorer(Store::IStorer * storer)             { m_pending_packets.setStorer(storer); }
    inline Store::IStorer * Session::pendingPacketsStorer()                           { return m_pending_packets.storer(); }
    inline quint16 Session::generateId()                                              { return m_idctrl.generateId(); }
    inline bool Session::storePacketId(quint16 id)                                    { return m_idctrl.addId(id); }
    inline void Session::freePacketId(quint16 id)                                     { m_idctrl.removeId(id); }
    inline bool Session::isPacketIdUsed(quint16 id)                                   { return m_idctrl.contains(id); }

    typedef QSharedPointer<Session> SessionPtr;
    typedef QWeakPointer<Session>   SessionWPtr;
}

#endif // MQTT_SESSION_H
