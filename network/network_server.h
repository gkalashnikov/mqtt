#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

#include "network_tcp_server.h"
#include "network_client.h"
#include "network_event.h"
#include "average/move.h"
#include <QMutex>
#include <QThread>
#include <QCoreApplication>

class QTimer;

namespace Network
{
    class TcpServer;

    class Server : public QThread
    {
        Q_OBJECT
    public:
        Server(SecureMode type, QHostAddress listenIp, quint16 listenPort, const QString & serverName, const QStringList & supportedSubprotocols);
        ~Server() override;

    signals:
        void listeningStarted(QHostAddress address, quint16 port);
        void cantStartListening(QString error);

    private slots:
        void initialize();
        void deinitialize();
        void oneSecondTimer();

    protected:
        bool event(QEvent *event) override;
        void run() override;

    public:
        SecureMode secureMode() const;
        QHostAddress listenIp() const;
        quint16 port() const;

        void setNetworkEventsHandler(QObject * handler);
        void clientConnected(ServerClient && connection);
        void clientUpgraded(quintptr connectionId);
        void clientReadData(quintptr connectionId, const QByteArray & data);
        void clientWriteData(quintptr connectionId, const QByteArray & data);
        void clientDisconnected(quintptr connectionId);
        void closeConnection(quintptr connectionId);

        Average::Load receivedStats();
        Average::Load sentStats();

#ifndef QT_NO_OPENSSL
        void setSslConfiguration(QSharedPointer<QSslConfiguration> ssl);
#endif

    protected:
        void increaseSent(int count);
        void increaseReceived(int count);

    private:
        class Statistics
        {
        public:
            Statistics();

            Average::Move<900> br; // bytes received
            Average::Move<900> bs; // bytes sent
            mutable QMutex   m;
        };

    private:
        TcpServer    * m_tcp_server;
        QObject      * m_handler;
        QTimer       * m_timer;
        SecureMode     m_secure;
        QHostAddress   m_ip;
        quint16        m_port;
        Statistics     m_stats;
        QString        m_server_name;
        QStringList    m_subprotocols;

        Average::Load m_recv;
        Average::Load m_sent;

#ifndef QT_NO_OPENSSL
        QWeakPointer<QSslConfiguration> m_ssl;
#endif
    private:
        friend class TcpServer;
    };

    typedef QSharedPointer<Server> ServerPtr;
    typedef QWeakPointer<Server> ServerWPtr;

    inline void Server::setNetworkEventsHandler(QObject * handler) { m_handler = handler; }
    inline SecureMode Server::secureMode() const                   { return m_secure;     }
    inline QHostAddress Server::listenIp() const                   { return m_ip;         }
    inline quint16 Server::port() const                            { return m_port;       }

    inline void Server::clientConnected(ServerClient && connection)
    { QCoreApplication::postEvent(m_handler, new Event::IncomingConnection(std::move(connection)), Qt::HighEventPriority); }

    inline void Server::clientUpgraded(quintptr connectionId)
    { QCoreApplication::postEvent(m_handler, new Event::WillUpgraded(connectionId),                Qt::HighEventPriority); }

    inline void Server::clientReadData(quintptr connectionId, const QByteArray & data)
    { QCoreApplication::postEvent(m_handler, new Event::Data(connectionId, data),                  Qt::HighEventPriority); }

    inline void Server::clientWriteData(quintptr connectionId, const QByteArray & data)
    { QCoreApplication::postEvent(this,      new Event::Data(connectionId, data),                  Qt::HighEventPriority); }

    inline void Server::clientDisconnected(quintptr connectionId)
    { QCoreApplication::postEvent(m_handler, new Event::CloseConnection(connectionId),             Qt::HighEventPriority); }

    inline void Server::closeConnection(quintptr connectionId)
    { QCoreApplication::postEvent(this,      new Event::CloseConnection(connectionId),             Qt::HighEventPriority); }
}

#endif // NETWORK_SERVER_H
