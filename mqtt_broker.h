#ifndef MQTT_BROKER_H
#define MQTT_BROKER_H

#include "mqtt_control_packet.h"
#include "mqtt_session.h"
#include "mqtt_chunk_data_controller.h"
#include "mqtt_subscriptions_shared.h"
#include "mqtt_store_publish_container.h"
#include "mqtt_storer_factory_interface.h"
#include "mqtt_statistic.h"
#include "network_server.h"

#include <QObject>

namespace Mqtt
{
    class PasswordFile;
    class PublishPacket;

    typedef QHash<QString, SessionPtr>  SessionsStore;
    typedef QMap<quintptr, SessionWPtr> SessionsStoreByConn;
    typedef std::vector<quint32> SubscriptionIdentifiersArray;

    class Broker: public QObject
    {
        Q_OBJECT
    public:
        Broker(Store::IFactory * storerFactory, QObject * parent = Q_NULLPTR);
        ~Broker() override;

    public:
        static QString generateClientId(Version version);

    public:
        void handleIncomingConnection(Network::ServerClient connection);
        void handleNetworkData(quintptr connectionId, const QByteArray & data);
        void handleCloseConnection(quintptr connectionId);
        void handleConnectionUpgraded(quintptr connectionId);

    private slots:
        void initialize();
        void sessionExpired();
        void publishStatistic();

    protected:
        bool event(QEvent * event) override;

    public:
        bool isQoS0OfflineEnabled() const;
        void setQoS0OfflineEnabled(bool enabled = true);
        quint32 maxFlowPerSecond(QoS qos) const;
        void setMaxFlowPerSecond(QoS qos, quint32 messagesCount);
        void setBanDuration(quint32 seconds, bool accumulative);
        bool setPasswordFile(const QString & filePath);
        PasswordFile * passwordFile();
        void addListener(Network::ServerPtr listener);

    private:
        void handleControlPacket(SessionPtr & session, const QByteArray & data);
        void handleConnectPacket(SessionPtr & session, const QByteArray & data);
        void handleAuthPacket(SessionPtr & session, const QByteArray & data);
        void handlePingRequestPacket(SessionPtr & session, const QByteArray & data);
        void handlePublishPacket(SessionPtr & session, const QByteArray & data);
        void handlePubAckPacket(SessionPtr & session, const QByteArray & data);
        void handlePubRecPacket(SessionPtr & session, const QByteArray & data);
        void handlePubRelPacket(SessionPtr & session, const QByteArray & data);
        void handlePubCompPacket(SessionPtr & session, const QByteArray & data);
        void handleDisconnectPacket(SessionPtr & session, const QByteArray & data);
        void handleSubscribePacket(SessionPtr & session, const QByteArray & data);
        void handleUnsubscribePacket(SessionPtr & session, const QByteArray & data);

        void processTwiceConnection(SessionPtr & session);
        void processOtherConnectionWithSameClientId(SessionPtr & session, SessionPtr & otherSession);
        void finalizeSuccessfulConnect(SessionPtr & session);

        void sendConnackSuccess(SessionPtr & session);
        void sendConnackError(SessionPtr & session, quint8 reasonCode, const QString & reasonString = QString());
        void sendDisconnect(SessionPtr & session, ReasonCodeV5 reason);

        void publish(const QString & fromClientId, const Topic & topic, const PublishPacket & packet);
        void publishWill(const ConnectPacket & connectPacket, bool immediately = false);
        void executePublishWill(const QString & clientId, const PublishPacket & packet);

        void publishRetainedPackets(SessionPtr & session, const SubscriptionNode::List & newSubscriptions);
        void publishSystemPackets(SessionPtr & session, const SubscriptionNode::List & newSubscriptions);
        void processPublishPacket(SessionPtr & session, const PublishPacket & sourcePacket, SubscribeOptions subscribeOptions, const SubscriptionIdentifiersArray & identifiers);
        void publishPendingPackets(SessionPtr & session);

        PublishPacket makeSystemInfoPacket(const QString & topic, const QByteArray & payload);
        void publishSystemPacket(const QString & topic, const QByteArray & payload);

        SessionPtr createSession();
        void sessionDeleter(Session * session);
        SessionPtr findSession(quintptr connectionId) const;

        void loadSessions();
        void storeSharedSubscriptions();
        void loadSharedSubscriptions();

        void updateClientsStatistic();

        void startPublishStatisticTimer();

        QByteArray makeNetworkLoadInfoPayload() const;
        QByteArray makeBrokerInfoPayload() const;
        QByteArray makeMqttClientsInfoPayload() const;
        QByteArray makeSubscriptionsInfoPayload() const;

        void publishBrokerInfo();
        void publishMqttClientsInfo();
        void publishSubscriptionsInfo();
        void publishMqttMessRecvInfo();
        void publishMqttMessSentInfo();
        void publishMqttMessDropInfo();
        void publishMqttPublishRecvInfo();
        void publishMqttPublishSentInfo();
        void publishMqttPublishDropInfo();
        void publishNetworkLoadInfo();

    private:
        SessionSubscriptionData * selectSubscriptionDataWithMaximumQoS(const SubscriptionNode::List & nodes, SubscriptionIdentifiersArray & outSubscriptionIdentifiers);

        typedef std::vector<SharedSubscriptionSessionData> SharedSubscriptionSessionsArray;
        const SharedSubscriptionSessionsArray & selectSharedSubscriptionSessions(const SubscriptionNode::List & nodes);

        void removeSharedSubscriptions(Session * session);

    private:
        bool                       isQoS0QueueEnabled;
        quint32                    maxFlowMessages[3];
        quint32                    banDuration;
        bool                       banAccumulative;
        Statistic                * statistic;
        int                        subcount;
        Store::IFactory          * storerFactory;
        QList<SessionPtr>          sessionsNew;
        SessionsStore              sessions;
        SessionsStoreByConn        sessionsByConn;
        SharedSubscriptions        sharedSubscriptions;
        Store::PublishContainer    retainPackets;
        Store::IStorer           * sessionsStorer;
        Store::IStorer           * sharedSubscriptionsStorer;
        QList<Network::ServerWPtr> listeners;
        PasswordFile             * passFile;
    };

    typedef QSharedPointer<Broker> BrokerPtr;
    typedef QWeakPointer<Broker> BrokerWPtr;

    inline bool Broker::isQoS0OfflineEnabled() const        { return isQoS0QueueEnabled;    }
    inline void Broker::setQoS0OfflineEnabled(bool enabled) { isQoS0QueueEnabled = enabled; }
}

#endif // MQTT_BROKER_H
