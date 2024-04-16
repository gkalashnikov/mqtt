#ifndef MQTT_STATISTIC_H
#define MQTT_STATISTIC_H

#include <QObject>
#include "average/move.h"
#include "version.h"

namespace Mqtt
{
    class Statistic : public QObject
    {
        Q_OBJECT
    public:
        explicit Statistic(QObject * parent = Q_NULLPTR);
        ~Statistic() override;

    public slots:
        void oneSecondTimer();

    public:

        class Broker
        {
        public:
            const char * description = PROJECT_DESCRIPTION;
            const char * version     = PROJECT_VERSION_STR;
            const char * timestamp   = PROJECT_BUILD_TIMESTAMP;
            const char * gitrevision = PROJECT_GIT_REVISION_STR;
            quint64      uptime      = 0;
        };

        class Clients
        {
        public:
            qint32 total        = 0;
            qint32 maximum      = 0;
            qint32 disconnected = 0;
            qint32 connected    = 0;
            qint32 expired      = 0;
        };

        class Messages
        {
        public:
            Messages();
            void set(Average::Move<900> & s);
            void oneSecondTimer();

            Average::Move<900> received;
            Average::Move<900> sent;
            Average::Move<900> dropped;
        };

    public:
        void increaseMessages();
        void increaseDroppedMessages();
        void increaseSentMessages();

        void increasePublishMessages();
        void increaseDroppedPublishMessages();
        void increaseSentPublishMessages();

        void increaseSubscriptionCount(qint32 count = 1);
        void decreaseSubscriptionCount(qint32 count = 1);

        void setTotalClients(int count);
        void increaseExpiredClients();
        void setConnectedClients(int count);
        void setDisconnectedClients(int count);

    public:
        Broker   broker;
        Clients  clients;
        qint32   subscriptionsCount = 0;
        Messages allmessages;
        Messages publish;
    };

    inline void Statistic::increaseMessages()                      { allmessages.received.increase(1); }
    inline void Statistic::increaseDroppedMessages()               { allmessages.dropped.increase(1);  }
    inline void Statistic::increaseSentMessages()                  { allmessages.sent.increase(1);     }
    inline void Statistic::increasePublishMessages()               { publish.received.increase(1);     }
    inline void Statistic::increaseSubscriptionCount(qint32 count) { subscriptionsCount += count;      }
    inline void Statistic::decreaseSubscriptionCount(qint32 count) { subscriptionsCount -= count;      }
    inline void Statistic::increaseExpiredClients()                { ++clients.expired;                }
    inline void Statistic::setConnectedClients(int count)          { clients.connected = count;        }
    inline void Statistic::setDisconnectedClients(int count)       { clients.disconnected = count;     }
    inline void Statistic::increaseDroppedPublishMessages()        { publish.dropped.increase(1);
                                                                     allmessages.dropped.increase(1);  }
    inline void Statistic::increaseSentPublishMessages()           { publish.sent.increase(1);
                                                                     allmessages.sent.increase(1);     }
}

#endif // MQTT_STATISTIC_H
