#ifndef NETWORK_TCP_SERVER_H
#define NETWORK_TCP_SERVER_H

#include "network_client.h"

#include <QTcpServer>
#include <QTcpSocket>
#ifndef QT_NO_OPENSSL
#include <QSslSocket>
#include <QSslConfiguration>
#endif
#include <QMetaMethod>

class QWebSocketServer;

namespace Network
{
    class Server;

    class TcpServer : public QTcpServer
    {
        Q_OBJECT
    private:
        TcpServer(Server * parent, SecureMode mode, QHostAddress listenIp, quint16 listenPort, const QString & serverName, const QStringList & supportedSubprotocols);
        ~TcpServer();

    signals:
        void cantStartListening();
        void ready();

    private slots:
        void initialize();
        void listeningBeingOn();
        void listeningError();

        void socketFirstRead();
        void socketRead();
        void socketDisconnected();
        void socketStateChanged(QAbstractSocket::SocketState state);

        void websocketConnected();
        void websocketDisconnected();
        void websocketTextMessageReceived(const QString & message);
        void websocketBinaryMessageReceived(const QByteArray & data);
        void websocketStateChanged(QAbstractSocket::SocketState state);
        void websocketError(QAbstractSocket::SocketError error);

    public:
        SecureMode secureMode() const { return m_secure; }

#ifndef QT_NO_OPENSSL
        void setSslConfiguration(QSharedPointer<QSslConfiguration> ssl);
#endif

    protected:
        void incomingConnection(qintptr socketDescriptor) override;
        void receiveData(QObject * socket, QHostAddress ip, quint16 port, ConnectionType type, const QByteArray & data);

    private:
        QTcpSocket * createSocket() const;
        void configureSocket(QTcpSocket * socket) const;

        void writeData(quintptr connectionId, QByteArray data);
        void closeConnection(quintptr connectionId);

    private:
        SecureMode         m_secure;
        QHostAddress       m_ip;
        quint16            m_port;
        QWebSocketServer * m_ws_server;
        Server           * m_server;

        QMap<quintptr, ServerClientSocket> m_connections;

#       ifndef QT_NO_OPENSSL
        QWeakPointer<QSslConfiguration> m_ssl;
#       endif

    private:
        friend class Server;
    };
}

#endif // NETWORK_TCP_SERVER_H
