#include "network_client_socket_controller.h"
#include "network_event.h"
#include <QTimer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QEventLoop>

#include "logger.h"

static const char kConnectionIdPropertyName[] = "connectionId";

using namespace Network;

ClientSocketController::ClientSocketController()
    :QThread(Q_NULLPTR)
    ,m_ssl(Q_NULLPTR)
{
    moveToThread(this);
}

ClientSocketController::~ClientSocketController()
{
    exit();
    wait(60000);
}

void ClientSocketController::socketConnected()
{
    QTcpSocket * socket = qobject_cast<QTcpSocket*>(sender());
    quintptr connectionId = socket->property(kConnectionIdPropertyName).toULongLong();
    auto it = m_clients.find(connectionId);
    if (it == m_clients.end()) {
        socket->close();
        return;
    }
    connect(socket, &QTcpSocket::readyRead, this, &ClientSocketController::socketRead);
    connectionEstablished(it.value().first);
}

void ClientSocketController::socketDisconnected()
{
    QTcpSocket * socket = qobject_cast<QTcpSocket*>(sender());
    quintptr connectionId = socket->property(kConnectionIdPropertyName).toULongLong();
    auto it = m_clients.find(connectionId);
    if (it == m_clients.end())
        return;
    connectionClosed(it.value().first);
    m_clients.erase(it);
}

void ClientSocketController::socketStateChanged(QAbstractSocket::SocketState state)
{
    if (QTcpSocket * socket = qobject_cast<QTcpSocket*>(sender()))
    {
        quintptr connectionId = socket->property(kConnectionIdPropertyName).toULongLong();
        auto it = m_clients.find(connectionId);
        if (it == m_clients.end()) {
            socket->close();
            return;
        }
        switch (state)
        {
            case QAbstractSocket::UnconnectedState: { log_trace_6 << it.value().first << "state changed to \"UnconnectedState\"" << end_log; break; }
            case QAbstractSocket::HostLookupState:  { log_trace_6 << it.value().first << "state changed to \"HostLookupState\""  << end_log; break; }
            case QAbstractSocket::ConnectingState:  { log_trace_6 << it.value().first << "state changed to \"ConnectingState\""  << end_log; break; }
            case QAbstractSocket::ConnectedState:   { log_trace_6 << it.value().first << "state changed to \"ConnectedState\""   << end_log; break; }
            case QAbstractSocket::BoundState:       { log_trace_6 << it.value().first << "state changed to \"BoundState\""       << end_log; break; }
            case QAbstractSocket::ListeningState:   { log_trace_6 << it.value().first << "state changed to \"ListeningState\""   << end_log; break; }
            case QAbstractSocket::ClosingState:     { log_trace_6 << it.value().first << "state changed to \"ClosingState\""     << end_log; break; }
        }
    }
}

void ClientSocketController::socketRead()
{
    QTcpSocket * socket = qobject_cast<QTcpSocket*>(sender());
    quintptr connectionId = socket->property(kConnectionIdPropertyName).toULongLong();
    auto it = m_clients.find(connectionId);
    if (it == m_clients.end()) {
        socket->close();
        return;
    }
    qint64 len = socket->bytesAvailable();
    if (len > 0)
    {
        QByteArray data(len, Qt::Initialization::Uninitialized);
        socket->read(data.data(), data.length());
        connectionReceiveData(it.value().first, data);
    }
}

void ClientSocketController::websocketConnected()
{
    QWebSocket * socket = qobject_cast<QWebSocket*>(sender());
    quintptr connectionId = socket->property(kConnectionIdPropertyName).toULongLong();

    auto it = m_clients.find(connectionId);
    if (it == m_clients.end()) {
        socket->close();
        return;
    }

    connect(socket, &QWebSocket::binaryMessageReceived, this, &ClientSocketController::websocketBinaryMessageReceived);
    connect(socket, &QWebSocket::textMessageReceived, this, &ClientSocketController::websocketTextMessageReceived);

    connectionEstablished(it.value().first);
}

void ClientSocketController::websocketDisconnected()
{
    QWebSocket * socket = qobject_cast<QWebSocket*>(sender());
    quintptr connectionId = socket->property(kConnectionIdPropertyName).toULongLong();
    auto it = m_clients.find(connectionId);
    if (it == m_clients.end())
        return;
    connectionClosed(it.value().first);
    m_clients.erase(it);
}

void ClientSocketController::websocketBinaryMessageReceived(const QByteArray & data)
{
    QWebSocket * socket = qobject_cast<QWebSocket*>(sender());
    quintptr connectionId = socket->property(kConnectionIdPropertyName).toULongLong();
    auto it = m_clients.find(connectionId);
    if (it == m_clients.end()) {
        socket->close();
        return;
    }
    connectionReceiveData(it.value().first, data);
}

void ClientSocketController::websocketTextMessageReceived(const QString & message)
{
    QWebSocket * socket = qobject_cast<QWebSocket*>(sender());
    quintptr connectionId = socket->property(kConnectionIdPropertyName).toULongLong();
    auto it = m_clients.find(connectionId);
    if (it == m_clients.end()) {
        socket->close();
        return;
    }
    QByteArray data = message.toUtf8();
    connectionReceiveData(it.value().first, data);
}

void ClientSocketController::websocketStateChanged(QAbstractSocket::SocketState state)
{
    QWebSocket * socket = qobject_cast<QWebSocket*>(sender());
    quintptr connectionId = socket->property(kConnectionIdPropertyName).toULongLong();
    auto it = m_clients.find(connectionId);
    if (it == m_clients.end()) {
        socket->close();
        return;
    }
    switch (state)
    {
        case QAbstractSocket::UnconnectedState: { log_trace_6 << it.value().first << "state changed to \"UnconnectedState\", reason:" << socket->closeReason() << end_log; break; }
        case QAbstractSocket::HostLookupState:  { log_trace_6 << it.value().first << "state changed to \"HostLookupState\""  << end_log; break; }
        case QAbstractSocket::ConnectingState:  { log_trace_6 << it.value().first << "state changed to \"ConnectingState\""  << end_log; break; }
        case QAbstractSocket::ConnectedState:   { log_trace_6 << it.value().first << "state changed to \"ConnectedState\""   << end_log; break; }
        case QAbstractSocket::BoundState:       { log_trace_6 << it.value().first << "state changed to \"BoundState\""       << end_log; break; }
        case QAbstractSocket::ListeningState:   { log_trace_6 << it.value().first << "state changed to \"ListeningState\""   << end_log; break; }
        case QAbstractSocket::ClosingState:     { log_trace_6 << it.value().first << "state changed to \"ClosingState\", reason:"     << socket->closeReason() << end_log; break; }
    }
}

#ifndef QT_NO_OPENSSL
void ClientSocketController::setSslConfiguration(QSharedPointer<QSslConfiguration> ssl)
{
    m_ssl = ssl;
}
#endif

bool ClientSocketController::event(QEvent * event)
{
    switch (static_cast<Event::Type>(event->type()))
    {
        case Event::Type::OpenConnection:
        {
            if (Event::OpenConnection * e = dynamic_cast<Event::OpenConnection*>(event)) {
                e->accept();
                handleOpenConnection(e->connection);
                return true;
            }
            break;
        }
        case Event::Type::Data:
        {
            if (Event::Data * e = dynamic_cast<Event::Data*>(event)) {
                e->accept();
                handleWriteData(e->connectionId, e->data);
                return true;
            }
            break;
        }
        case Event::Type::CloseConnection:
        {
            if (Event::CloseConnection * e = dynamic_cast<Event::CloseConnection*>(event)) {
                e->accept();
                handleCloseConnection(e->connectionId);
                return true;
            }
            break;
        }
        default: break;
    }
    return QThread::event(event);
}

void ClientSocketController::run()
{
    QThread::run();

    for (auto it = m_clients.begin(); it != m_clients.end(); ) {
        it.value().second.data()->disconnect(this);
        closeConnection(it.value().first, it.value().second);
        connectionClosed(it.value().first);
        it = m_clients.erase(it);
    }
}

void ClientSocketController::handleOpenConnection(const Client & connection)
{
    auto socket = QSharedPointer<QObject>(createSocket(connection), &QObject::deleteLater);

    if (socket.isNull())
        return;

    socket->setProperty(kConnectionIdPropertyName, connection.id());

    auto it = m_clients.find(connection.id());
    if (it != m_clients.end()) {
        it.value().second->setProperty(kConnectionIdPropertyName, 0);
        closeConnection(it.value().first, it.value().second);
        m_clients.erase(it);
    }

    m_clients.insert(connection.id(), { connection, socket });

    openConnection(connection, socket);
}

void ClientSocketController::handleWriteData(quintptr connectionId, const QByteArray & data)
{
    auto it = m_clients.find(connectionId);

    if (it == m_clients.end())
        return;

    switch (it.value().first.type())
    {
        case ConnectionType::TCP: {
            if (QTcpSocket * socket = qobject_cast<QTcpSocket*>(it.value().second.data()))
                socket->write(data);
            return;
        }

        case ConnectionType::WS: {
            if (QWebSocket * socket = qobject_cast<QWebSocket*>(it.value().second.data()))
                socket->sendBinaryMessage(data);
            return;
        }
    }
}

void ClientSocketController::handleCloseConnection(quintptr connectionId)
{
    auto it = m_clients.find(connectionId);

    if (it == m_clients.end())
        return;

    closeConnection(it.value().first, it.value().second);
}

QObject * ClientSocketController::createSocket(const Client & connection)
{
    switch (connection.type())
    {
        case ConnectionType::WS:
        {
            QWebSocket * socket = createWebSocket(connection);
            connect(socket, &QWebSocket::connected, this, &ClientSocketController::websocketConnected);
            connect(socket, &QWebSocket::disconnected, this, &ClientSocketController::websocketDisconnected);
            connect(socket, &QWebSocket::stateChanged, this, &ClientSocketController::websocketStateChanged);
            return socket;
        }
        case ConnectionType::TCP:
        {
            QTcpSocket * socket = createTcpSocket(connection);
            connect(socket, &QTcpSocket::connected, this, &ClientSocketController::socketConnected);
            connect(socket, &QTcpSocket::disconnected, this, &ClientSocketController::socketDisconnected);
            connect(socket, &QTcpSocket::stateChanged, this, &ClientSocketController::socketStateChanged);
            return socket;
        }
    }
    return Q_NULLPTR;
}

QTcpSocket * ClientSocketController::createTcpSocket(const Client & connection)
{
    switch (connection.mode())
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

QWebSocket * ClientSocketController::createWebSocket(const Client & connection)
{
    QWebSocket * socket = new QWebSocket();

    switch (connection.mode())
    {
        case SecureMode::Secured:
        {
#           ifndef QT_NO_OPENSSL
            if (QSslSocket::supportsSsl())
            {
                QSharedPointer<QSslConfiguration> ssl_conf = m_ssl;
                if (ssl_conf && !ssl_conf->isNull())
                {
                    socket->setSslConfiguration(*ssl_conf);
                }
            }
#           endif
        }
        default: break;
    }

    return socket;
}

void ClientSocketController::openConnection(const Client & connection, QSharedPointer<QObject> socketptr)
{
    switch (connection.type())
    {
        case ConnectionType::WS:
        {
            QWebSocket * socket = qobject_cast<QWebSocket*>(socketptr.data());
            socket->open(QUrl(QString("ws%1://%2:%3")
                              .arg(connection.mode() == SecureMode::Secured ? QString("s") : QString())
                              .arg(connection.address())
                              .arg(connection.port())));
            return;
        }
        case ConnectionType::TCP:
        {
            switch (connection.mode())
            {
                case SecureMode::Secured:
                {
#                   ifndef QT_NO_OPENSSL
                    if (QSslSocket::supportsSsl()) {
                        if (QSslSocket * socket = dynamic_cast<QSslSocket*>(socketptr.data())) {
                            socket->connectToHostEncrypted(connection.address(), connection.port(), QString());
                            break;
                        }
                    }
#                   endif
                }
                case SecureMode::NonSecured:
                {
                    QTcpSocket * socket = qobject_cast<QTcpSocket*>(socketptr.data());
                    socket->connectToHost(connection.address(), connection.port());
                    break;
                }
            }
            return;
        }
    }
}

template <class SocketType>
void closeSocket(SocketType * socket)
{
    if (socket->state() == QAbstractSocket::ConnectedState) {
        while (socket->bytesToWrite() > 0) {
            QEventLoop loop;
            QObject::connect(socket, &SocketType::bytesWritten, &loop, &QEventLoop::exit);
            loop.exec();
        }
        socket->close();
    }
}

void ClientSocketController::closeConnection(const Client & connection, QSharedPointer<QObject> socketptr)
{
    switch (connection.type())
    {
        case ConnectionType::Unknown:
        { Q_UNREACHABLE(); break; }

        case ConnectionType::TCP:
            if (QTcpSocket * socket = qobject_cast<QTcpSocket*>(socketptr.data()))
                closeSocket<QTcpSocket>(socket);
            break;

        case ConnectionType::WS:
            if (QWebSocket * socket = qobject_cast<QWebSocket*>(socketptr.data()))
                closeSocket<QWebSocket>(socket);
            break;
    }
}
