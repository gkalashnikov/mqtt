#include "network_server.h"
#include "network_event.h"
#include <QTimer>
#include <QCoreApplication>

#include <logger.h>

using namespace Network;

Server::Statistics::Statistics()
{
    br.load().valuesReserve(3);
    br.load().valueAppend(60);
    br.load().valueAppend(300);
    br.load().valueAppend(900);

    bs.load().valuesReserve(3);
    bs.load().valueAppend(60);
    bs.load().valueAppend(300);
    bs.load().valueAppend(900);
}

Server::Server(SecureMode type, QHostAddress listenIp, quint16 listenPort, const QString & serverName, const QStringList & supportedSubprotocols)
    :QThread(Q_NULLPTR)
    ,m_tcp_server(Q_NULLPTR)
    ,m_timer(Q_NULLPTR)
    ,m_secure(type)
    ,m_ip(listenIp)
    ,m_port(listenPort)
    ,m_server_name(serverName)
    ,m_subprotocols(supportedSubprotocols)
{
    moveToThread(this);
}

Server::~Server()
{
    exit();
    wait(60000);
}

void Server::initialize()
{
    m_tcp_server = new TcpServer(this, m_secure, m_ip, m_port, m_server_name, m_subprotocols);
#ifndef QT_NO_OPENSSL
    m_tcp_server->setSslConfiguration(m_ssl);
#endif
    connect(m_tcp_server, &TcpServer::ready, this, &Server::ready);
    connect(m_tcp_server, &TcpServer::cantStartListening, this, &Server::cantStartListening);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &Server::oneSecondTimer);
    m_timer->start(1000);
}

void Server::deinitialize()
{
    if (m_tcp_server) {
        delete m_tcp_server;
        m_tcp_server = Q_NULLPTR;
    }

    if (m_timer) {
        m_timer->stop();
        delete m_timer;
        m_timer = Q_NULLPTR;
    }
}

void Server::run()
{
    QTimer::singleShot(0, this, &Server::initialize);
    exec();
    deinitialize();
}

bool Server::event(QEvent * event)
{
    switch (static_cast<Event::Type>(event->type()))
    {
        case Event::Type::Data:
        {
            event->accept();
            Event::Data * e = dynamic_cast<Event::Data*>(event);
            m_tcp_server->writeData(e->connectionId, e->data);
            return true;
        }
        case Event::Type::CloseConnection:
        {
            event->accept();
            Event::CloseConnection * e = dynamic_cast<Event::CloseConnection*>(event);
            m_tcp_server->closeConnection(e->connectionId);
            return true;
        }
        default: break;
    }
    return QThread::event(event);
}

#ifndef QT_NO_OPENSSL
void Server::setSslConfiguration(QSharedPointer<QSslConfiguration> ssl)
{
    m_ssl = ssl;
    if (m_tcp_server)
        m_tcp_server->setSslConfiguration(ssl);
}
#endif

Average::Load Server::receivedStats()
{
    QMutexLocker lock(&m_stats.m);
    Average::Load copy = m_stats.br.load();
    return copy;
}

Average::Load Server::sentStats()
{
    QMutexLocker lock(&m_stats.m);
    Average::Load copy = m_stats.bs.load();
    return copy;
}

void Server::oneSecondTimer()
{
    QMutexLocker lock(&m_stats.m);
    m_stats.br.oneSecondTimer();
    m_stats.bs.oneSecondTimer();
}

void Server::increaseSent(int count)
{
    QMutexLocker lock(&m_stats.m);
    m_stats.bs.increase(count);
}

void Server::increaseReceived(int count)
{
    QMutexLocker lock(&m_stats.m);
    m_stats.br.increase(count);
}
