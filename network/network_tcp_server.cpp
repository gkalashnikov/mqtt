#include "network_server.h"
#include <QTimer>
#include <QWebSocketServer>
#include <QWebSocket>

#include <logger.h>

using namespace Network;

TcpServer::TcpServer(Server * parent, SecureMode mode, QHostAddress listenIp, quint16 listenPort, const QString & serverName, const QStringList & supportedSubprotocols)
    :QTcpServer(parent)
    ,m_secure(mode)
    ,m_ip(listenIp)
    ,m_port(listenPort)
    ,m_ws_server(new QWebSocketServer(serverName, mode == SecureMode::Secured ? QWebSocketServer::SecureMode : QWebSocketServer::NonSecureMode, this))
    ,m_server(parent)
{
    if (!supportedSubprotocols.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
        m_ws_server->setSupportedSubprotocols(supportedSubprotocols);
#else
        log_important << "QWebSocketServer not supported subprotocols in this version of Qt, ws client should disconnects if needs subproto. To avoid this, use Qt version 6.4.0 or higher." << end_log;
#endif
    }
    setMaxPendingConnections(0x7FFFFFFF);
    QTimer::singleShot(0, this, &TcpServer::initialize);
}

TcpServer::~TcpServer()
{
    close();
}

void TcpServer::initialize()
{
    connect(m_ws_server, &QWebSocketServer::newConnection, this, &TcpServer::websocketConnected);

    if (listen(m_ip, m_port)) {
        listeningBeingOn();
    } else {
        listeningError();
    }
}

void TcpServer::listeningBeingOn()
{
    log_important << printString(QStringLiteral("interface(%1:%2), socket=%3, \"listening\"")
                              .arg(serverAddress().toString())
                              .arg(serverPort())
                              .arg(socketDescriptor())
                              )
                  << "[" << secureMode() << "]"
#                 ifndef QT_NO_OPENSSL
                  << (secureMode() == SecureMode::Secured
                      ? printString(QStringLiteral("ssl %1, lib version: %2, used version: %3")
                                    .arg(QSslSocket::supportsSsl() ? QStringLiteral("ON") : QStringLiteral("OFF"))
                                    .arg(QSslSocket::sslLibraryBuildVersionString())
                                    .arg(QSslSocket::sslLibraryVersionString()))
                      : "")
#                 endif
                  << end_log;

    emit listeningStarted(serverAddress(), serverPort());
}

void TcpServer::listeningError()
{
    log_important << printString(QStringLiteral("interface(%1:%2), socket=%3, can't start listening, %4")
                              .arg(m_ip.toString())
                              .arg(m_port)
                              .arg(socketDescriptor())
                              .arg(errorString())
                              )
                  << "[" << secureMode() << "]"
                  << end_log;
    close();
    emit cantStartListening(errorString());
}

void TcpServer::receiveData(QObject * socket, QHostAddress ip, quint16 port, ConnectionType type, const QByteArray & data)
{
    m_server->increaseReceived(data.size());
    log_trace_1 << "<<" << ServerClient { type, secureMode(), ip, port, reinterpret_cast<quintptr>(socket), m_server } << printByteArrayPartly(data, 60) << end_log;
    m_server->clientReadData(reinterpret_cast<quintptr>(socket), data);
}

void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket * socket = createSocket();

    if (!socket->setSocketDescriptor(socketDescriptor)) {
        log_trace << printString(QStringLiteral("can't set socket descriptor (%1) -> cannot initialize socket: %2").arg(socketDescriptor).arg(socket->errorString())) << end_log;
        socket->deleteLater();
        return;
    }

    configureSocket(socket);

    m_connections[reinterpret_cast<quintptr>(socket)] = std::move(ServerClientSocket { ConnectionType::TCP, secureMode(), socket->peerAddress(), socket->peerPort(), socket, m_server });

    log_note << m_connections[reinterpret_cast<quintptr>(socket)] <<  "new client connected" << end_log;

    m_server->clientConnected(ServerClient { ConnectionType::TCP, secureMode(), socket->peerAddress(), socket->peerPort(), reinterpret_cast<quintptr>(socket), m_server });

#   ifndef QT_NO_OPENSSL
    if (SecureMode::Secured == secureMode())
        if (QSslSocket * ssl = qobject_cast<QSslSocket*>(socket))
            ssl->startServerEncryption();
#   endif

    connect(socket, &QTcpSocket::readyRead, this, &TcpServer::socketFirstRead);
    connect(socket, &QTcpSocket::disconnected, this, &TcpServer::socketDisconnected);
    connect(socket, &QTcpSocket::stateChanged, this, &TcpServer::socketStateChanged);
}

void TcpServer::websocketConnected()
{
    QWebSocket * socket = m_ws_server->nextPendingConnection();

    m_connections[reinterpret_cast<quintptr>(socket)] = std::move(ServerClientSocket{ ConnectionType::WS, secureMode(), socket->peerAddress(), socket->peerPort(), socket, m_server });

    log_note << m_connections[reinterpret_cast<quintptr>(socket)] << "client upgraded to websocket" << end_log;

    m_server->clientConnected(ServerClient{ ConnectionType::WS, secureMode(), socket->peerAddress(), socket->peerPort(), reinterpret_cast<quintptr>(socket), m_server });

    connect(socket, &QWebSocket::disconnected, this, &TcpServer::websocketDisconnected);
    connect(socket, &QWebSocket::textMessageReceived, this, &TcpServer::websocketTextMessageReceived);
    connect(socket, &QWebSocket::binaryMessageReceived, this, &TcpServer::websocketBinaryMessageReceived);
    connect(socket, &QWebSocket::stateChanged, this, &TcpServer::websocketStateChanged);
#   if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &TcpServer::websocketError);
#   else
    connect(socket, &QWebSocket::errorOccurred, this, &TcpServer::websocketError);
#   endif
}

void TcpServer::socketFirstRead()
{
    QTcpSocket * socket = qobject_cast<QTcpSocket*>(sender());

    qint64 len = static_cast<qint32>(socket->bytesAvailable());

    if (len > 0)
    {
        disconnect(socket, &QTcpSocket::readyRead, this, &TcpServer::socketFirstRead);
        QByteArray data(len, Qt::Initialization::Uninitialized);
        socket->peek(data.data(), data.length());
        if (data.contains(QByteArrayLiteral("Upgrade: websocket\r\n"))) {
            disconnect(socket, &QTcpSocket::stateChanged, this, &TcpServer::socketStateChanged);
            m_connections.remove(reinterpret_cast<quintptr>(socket));
            m_ws_server->handleConnection(socket);
            m_server->clientUpgraded(reinterpret_cast<quintptr>(socket));
            return;
        }
        connect(socket, &QTcpSocket::readyRead, this, &TcpServer::socketRead);
        socketRead();
    }
}

void TcpServer::socketRead()
{
    QTcpSocket * socket = qobject_cast<QTcpSocket*>(sender());
    qint64 len = socket->bytesAvailable();
    if (len > 0)
    {
        QByteArray data(len, Qt::Initialization::Uninitialized);
        socket->read(data.data(), data.length());
        receiveData(socket, socket->peerAddress(), socket->peerPort(), ConnectionType::TCP, data);
    }
}

template<class SocketType>
SocketType * extractConnectedSocketOtherwiseRemove(QMap<quintptr, ServerClientSocket>::iterator & it, QMap<quintptr, ServerClientSocket> & container)
{
    if (SocketType * socket = qobject_cast<SocketType*>((*it).socket.data())) {
        if (socket->state() == QAbstractSocket::ConnectedState)
            return socket;
        socket->deleteLater();
    }
    container.erase(it);
    return Q_NULLPTR;
}

void TcpServer::writeData(quintptr connectionId, QByteArray data)
{
    auto it = m_connections.find(connectionId);

    if (it == m_connections.end())
        return;

    switch ((*it).type())
    {
        case ConnectionType::Unknown:
        { Q_UNREACHABLE(); return; }

        case ConnectionType::TCP: {
            if (QTcpSocket * socket = extractConnectedSocketOtherwiseRemove<QTcpSocket>(it, m_connections)) {
                socket->write(data);
                break;
            }
            return;
        }

        case ConnectionType::WS: {
            if (QWebSocket * socket = extractConnectedSocketOtherwiseRemove<QWebSocket>(it, m_connections)) {
                socket->sendBinaryMessage(data);
                break;
            }
            return;
        }
    }

    m_server->increaseSent(data.size());

    log_trace_1 << ">>" << (*it) << printByteArrayPartly(data, 60) << end_log;
}

template <class SocketType>
void closeSocket(SocketType * socket)
{
    while (socket->bytesToWrite() > 0) {
        QEventLoop loop;
        QObject::connect(socket, &SocketType::bytesWritten, &loop, &QEventLoop::exit);
        loop.exec();
    }
    socket->close();
}

void TcpServer::closeConnection(quintptr connectionId)
{
    auto it = m_connections.find(connectionId);

    if (it == m_connections.end())
        return;

    switch ((*it).type())
    {
        case ConnectionType::Unknown:
        { Q_UNREACHABLE(); break; }

        case ConnectionType::TCP:
            if (QTcpSocket * socket = extractConnectedSocketOtherwiseRemove<QTcpSocket>(it, m_connections))
                closeSocket<QTcpSocket>(socket);
            break;

        case ConnectionType::WS:
            if (QWebSocket * socket = extractConnectedSocketOtherwiseRemove<QWebSocket>(it, m_connections))
                closeSocket<QWebSocket>(socket);
            break;
    }
}

template<class SocketType>
void processDisconnectedSocket(QMap<quintptr, ServerClientSocket> & container, QObject * sender, Server * server)
{
    SocketType * socket = qobject_cast<SocketType*>(sender);
    socket->deleteLater();
    auto it = container.find(reinterpret_cast<quintptr>(socket));
    if (it != container.end()) {
        log_trace << *it << "removed" << end_log;
        container.erase(it);
        server->clientDisconnected(reinterpret_cast<quintptr>(socket));
    }
}


void TcpServer::socketDisconnected()
{
    processDisconnectedSocket<QTcpSocket>(m_connections, sender(), m_server);
}

void TcpServer::websocketDisconnected()
{
    processDisconnectedSocket<QWebSocket>(m_connections, sender(), m_server);
}


void TcpServer::socketStateChanged(QAbstractSocket::SocketState state)
{
    QTcpSocket * socket = qobject_cast<QTcpSocket*>(sender());
    ServerClient conn { ConnectionType::TCP, secureMode(), socket->peerAddress(), socket->peerPort(), reinterpret_cast<quintptr>(socket), m_server };
    switch (state)
    {
        case QAbstractSocket::UnconnectedState: { log_trace_6 << conn << "state changed to \"UnconnectedState\"" << end_log; break; }
        case QAbstractSocket::HostLookupState:  { log_trace_6 << conn << "state changed to \"HostLookupState\""  << end_log; break; }
        case QAbstractSocket::ConnectingState:  { log_trace_6 << conn << "state changed to \"ConnectingState\""  << end_log; break; }
        case QAbstractSocket::ConnectedState:   { log_trace_6 << conn << "state changed to \"ConnectedState\""   << end_log; break; }
        case QAbstractSocket::BoundState:       { log_trace_6 << conn << "state changed to \"BoundState\""       << end_log; break; }
        case QAbstractSocket::ListeningState:   { log_trace_6 << conn << "state changed to \"ListeningState\""   << end_log; break; }
        case QAbstractSocket::ClosingState:     { log_trace_6 << conn << "state changed to \"ClosingState\""     << end_log; break; }
    }
}

void TcpServer::websocketTextMessageReceived(const QString & message)
{
    QWebSocket * socket = qobject_cast<QWebSocket*>(sender());
    QByteArray data = message.toUtf8();
    receiveData(socket, socket->peerAddress(), socket->peerPort(), ConnectionType::WS, data);
}

void TcpServer::websocketBinaryMessageReceived(const QByteArray & data)
{
    QWebSocket * socket = qobject_cast<QWebSocket*>(sender());
    receiveData(socket, socket->peerAddress(), socket->peerPort(), ConnectionType::WS, data);
}

void TcpServer::websocketStateChanged(QAbstractSocket::SocketState state)
{
    QWebSocket * socket = qobject_cast<QWebSocket*>(sender());
    ServerClient conn { ConnectionType::WS, secureMode(), socket->peerAddress(), socket->peerPort(), reinterpret_cast<quintptr>(socket), m_server };
    switch (state)
    {
        case QAbstractSocket::UnconnectedState: { log_trace_6 << conn << "state changed to \"UnconnectedState\", reason:" << socket->closeReason() << end_log; break; }
        case QAbstractSocket::HostLookupState:  { log_trace_6 << conn << "state changed to \"HostLookupState\""  << end_log; break; }
        case QAbstractSocket::ConnectingState:  { log_trace_6 << conn << "state changed to \"ConnectingState\""  << end_log; break; }
        case QAbstractSocket::ConnectedState:   { log_trace_6 << conn << "state changed to \"ConnectedState\""   << end_log; break; }
        case QAbstractSocket::BoundState:       { log_trace_6 << conn << "state changed to \"BoundState\""       << end_log; break; }
        case QAbstractSocket::ListeningState:   { log_trace_6 << conn << "state changed to \"ListeningState\""   << end_log; break; }
        case QAbstractSocket::ClosingState:     { log_trace_6 << conn << "state changed to \"ClosingState\", reason:"     << socket->closeReason() << end_log; break; }
    }
}

void TcpServer::websocketError(QAbstractSocket::SocketError error)
{
    QWebSocket * socket = qobject_cast<QWebSocket*>(sender());
    auto it = m_connections.find(reinterpret_cast<quintptr>(socket));
    if (it != m_connections.end())
        log_except << *it << "error has been occured, code:" << error << ", string:" << socket->errorString() << ", websocket server errror code:" << m_ws_server->errorString() << end_log;
}

#ifndef QT_NO_OPENSSL
void TcpServer::setSslConfiguration(QSharedPointer<QSslConfiguration> ssl)
{
    m_ssl = ssl;
}
#endif

QTcpSocket * TcpServer::createSocket() const
{
    switch (secureMode())
    {
        case SecureMode::Secured:
        {
#           ifndef QT_NO_OPENSSL
            if (QSslSocket::supportsSsl())
            {
                QSharedPointer<QSslConfiguration> ssl_conf = m_ssl;
                if (ssl_conf && !ssl_conf->isNull())
                {
                    QSslSocket * ssl_socket = new QSslSocket();
                    ssl_socket->setSslConfiguration(*ssl_conf);
                    return ssl_socket;
                }
            }
#           endif
        }

        default: return new QTcpSocket();
    }
}

void TcpServer::configureSocket(QTcpSocket * socket) const
{
    socket->setSocketOption(QTcpSocket::KeepAliveOption, qint32(1));
    socket->setSocketOption(QTcpSocket::SendBufferSizeSocketOption, quint32(-1));
    socket->setSocketOption(QTcpSocket::ReceiveBufferSizeSocketOption, quint32(-1));
}
