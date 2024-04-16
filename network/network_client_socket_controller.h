#ifndef NETWORK_CLIENT_SOCKET_CONTROLLER_H
#define NETWORK_CLIENT_SOCKET_CONTROLLER_H

#include "network_client.h"
#include "network_event.h"

#include <QThread>
#include <QSharedPointer>
#include <QCoreApplication>

#ifndef QT_NO_OPENSSL
#include <QSslConfiguration>
#endif

class QTcpSocket;
class QWebSocket;

namespace Network
{
    class ClientSocketController : public QThread
    {
        Q_OBJECT
    public:
        ClientSocketController();
        ~ClientSocketController() override;

    private slots:
        void socketConnected();
        void socketDisconnected();
        void socketRead();
        void socketStateChanged(QAbstractSocket::SocketState state);

        void websocketConnected();
        void websocketDisconnected();
        void websocketBinaryMessageReceived(const QByteArray & message);
        void websocketTextMessageReceived(const QString & message);
        void websocketStateChanged(QAbstractSocket::SocketState state);

    public:
#ifndef QT_NO_OPENSSL
        void setSslConfiguration(QSharedPointer<QSslConfiguration> ssl);
#endif

    protected:
        bool event(QEvent *event) override;
        void run() override;

    private:
        void handleOpenConnection(const Client & connection);
        void handleWriteData(quintptr connectionId, const QByteArray & data);
        void handleCloseConnection(quintptr connectionId);

        QObject * createSocket(const Client & connection);
        QTcpSocket * createTcpSocket(const Client & connection);
        QWebSocket * createWebSocket(const Client & connection);

        void openConnection(const Client & connection, QSharedPointer<QObject> socket);
        void closeConnection(const Client & connection, QSharedPointer<QObject> socket);

        void connectionEstablished(const Client & connection);
        void connectionClosed(const Client & connection);
        void connectionReceiveData(const Client & connection, const QByteArray & data);

    private:
        QMap<quintptr, QPair<Client, QSharedPointer<QObject> > > m_clients;
#ifndef QT_NO_OPENSSL
        QWeakPointer<QSslConfiguration> m_ssl;
#endif
    };

    typedef QSharedPointer<ClientSocketController> ClientSocketControllerPtr;
    typedef QWeakPointer<ClientSocketController> ClientSocketControllerWPtr;

    inline void ClientSocketController::connectionEstablished(const Client & connection)
    { QCoreApplication::postEvent(connection.handler(), new Event::ConnectionEstablished(connection.id()), Qt::HighEventPriority); }

    inline void ClientSocketController::connectionClosed(const Client & connection)
    { QCoreApplication::postEvent(connection.handler(), new Event::CloseConnection(connection.id())      , Qt::HighEventPriority); }

    inline void ClientSocketController::connectionReceiveData(const Client & connection, const QByteArray & data)
    { QCoreApplication::postEvent(connection.handler(), new Event::Data(connection.id(), data)           , Qt::HighEventPriority); }
}

#endif // NETWORK_CLIENT_SOCKET_CONTROLLER_H
