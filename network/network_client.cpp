#include "network_client.h"
#include "network_server.h"

static constexpr char kTcpNonSecured[] = "tcp";
static constexpr char kTcpSecured[]    = "tls";
static constexpr char kWsNonSecured[]  = "ws ";
static constexpr char kWsSecured[]     = "wss";

QDebug operator << (QDebug d, const Network::ServerClient & conn)
{
    const bool space = d.autoInsertSpaces();
    d.setAutoInsertSpaces(false);
    d << '[';
    switch (conn.type())
    {
        case Network::ConnectionType::TCP:
            switch (conn.mode())
            {
                case Network::SecureMode::NonSecured: d << kTcpNonSecured; break;
                case Network::SecureMode::Secured:    d << kTcpSecured;    break;
                default:                              d << conn.mode();    break;
            }
            break;
        case Network::ConnectionType::WS:
            switch (conn.mode())
            {
                case Network::SecureMode::NonSecured: d << kWsNonSecured; break;
                case Network::SecureMode::Secured:    d << kWsSecured;    break;
                default:                              d << conn.mode();   break;
            }
            break;
        default:                                      d << conn.type();   break;
    }
    d << ", (" << conn.ip().toString() << ':' << conn.port() << "), " << "id=" << conn.id() << ']';
    if (space)
    { d << ' '; }
    d.setAutoInsertSpaces(space);
    return d;
}

QDebug operator << (QDebug d, const Network::Client & conn)
{
    const bool space = d.autoInsertSpaces();
    d.setAutoInsertSpaces(false);
    d << '[';
    switch (conn.type())
    {
        case Network::ConnectionType::TCP:
            switch (conn.mode())
            {
                case Network::SecureMode::NonSecured: d << kTcpNonSecured; break;
                case Network::SecureMode::Secured:    d << kTcpSecured;    break;
                default:                              d << conn.mode();    break;
            }
            break;
        case Network::ConnectionType::WS:
            switch (conn.mode())
            {
                case Network::SecureMode::NonSecured: d << kWsNonSecured; break;
                case Network::SecureMode::Secured:    d << kWsSecured;    break;
                default:                              d << conn.mode();   break;
            }
            break;
        default:                                      d << conn.type();   break;
    }
    d << ", (" << conn.address() << ':' << conn.port() << "), " << "id=" << conn.id() << ']';
    if (space)
    { d << ' '; }
    d.setAutoInsertSpaces(space);
    return d;
}

using namespace Network;

Client::Client()
    :connectionType(ConnectionType::Unknown)
    ,secureMode(SecureMode::Unknown)
    ,hostAddress()
    ,hostPort(0)
    ,connectionId(0)
    ,client(Q_NULLPTR)
{

}

Client::Client(ConnectionType connType, SecureMode mode, QString hostAddress, quint16 hostPort, quintptr connectionId, QObject * client)
    :connectionType(connType)
    ,secureMode(mode)
    ,hostAddress(hostAddress)
    ,hostPort(hostPort)
    ,connectionId(connectionId)
    ,client(client)
{

}

Client::~Client()
{
    client = Q_NULLPTR;
}

ServerClient::ServerClient()
    :connectionType(ConnectionType::Unknown)
    ,secureMode(SecureMode::Unknown)
    ,clientIp(QHostAddress())
    ,clientPort(0)
    ,connectionId(0)
    ,server(Q_NULLPTR)
{

}

ServerClient::ServerClient(ConnectionType connType, SecureMode mode, QHostAddress ip, quint16 port, quintptr connectionId, Server * server)
    :connectionType(connType)
    ,secureMode(mode)
    ,clientIp(ip)
    ,clientPort(port)
    ,connectionId(connectionId)
    ,server(server)
{

}

ServerClient::~ServerClient()
{
    server = Q_NULLPTR;
}

void ServerClient::close() const
{
    Q_ASSERT(server != Q_NULLPTR);
    QCoreApplication::postEvent(server, new Event::CloseConnection(this->id()), Qt::HighEventPriority);
}

void ServerClient::write(const QByteArray & data) const
{
    Q_ASSERT(server != Q_NULLPTR);
    QCoreApplication::postEvent(server, new Event::Data(this->id(), data), Qt::HighEventPriority);
}

ServerClientSocket::ServerClientSocket()
    :ServerClient()
    ,socket(Q_NULLPTR)
{

}

ServerClientSocket::ServerClientSocket(ConnectionType connType, SecureMode mode, QHostAddress ip, quint16 port, QObject * socket, Server * server)
    :ServerClient(connType, mode, ip, port, reinterpret_cast<quintptr>(socket), server)
    ,socket(QPointer<QObject>(socket))
{

}

ServerClientSocket::~ServerClientSocket()
{

}
