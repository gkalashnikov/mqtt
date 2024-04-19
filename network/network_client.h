#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include "network_connection_type.h"
#include "network_secure_mode.h"
#include <QHostAddress>
#include <QPointer>

namespace Network
{
    class Server;

    class Client
    {
    public:
        Client();
        Client(ConnectionType connType, SecureMode mode, QString hostAddress, quint16 hostPort, quintptr connectionId, QObject * client);
        ~Client();

    public:
        ConnectionType type() const;
        void setType(ConnectionType type);
        SecureMode mode() const;
        void setMode(SecureMode mode);
        QString address() const;
        void setAddress(const QString & host);
        quint16 port() const;
        void setPort(quint16 port);
        quintptr id() const;
        bool isNull() const;
        QObject * handler() const;

        inline bool operator ==(const Client & other) const
        { return (this->connectionId == other.connectionId); }

        inline bool operator ==(quintptr clientId) const
        { return (this->connectionId == clientId); }

    private:
        ConnectionType connectionType;
        SecureMode     secureMode;
        QString        hostAddress;
        quint16        hostPort;
        quintptr       connectionId;
        QObject      * client;
    };

    inline ConnectionType Client::type() const           { return connectionType;      }
    inline void Client::setType(ConnectionType type)     { connectionType = type;      }
    inline SecureMode Client::mode() const               { return secureMode;          }
    inline void Client::setMode(SecureMode mode)         { secureMode = mode;          }
    inline QString Client::address() const               { return hostAddress;         }
    inline void Client::setAddress(const QString & host) { hostAddress = host;         }
    inline quint16 Client::port() const                  { return hostPort;            }
    inline void Client::setPort(quint16 port)            { hostPort = port;            }
    inline quintptr Client::id() const                   { return connectionId;        }
    inline bool Client::isNull() const                   { return (connectionId == 0); }
    inline QObject * Client::handler() const             { return client;              }

    class ServerClient
    {
    public:
        ServerClient();
        ServerClient(ConnectionType connType, SecureMode mode, QHostAddress ip, quint16 port, quintptr connectionId, Server * server);
        ~ServerClient();

    public:
        ConnectionType type() const;
        SecureMode mode() const;
        QHostAddress ip() const;
        quint16 port() const;
        quintptr id() const;
        bool isNull() const;
        const Server * serverInstance() const;

        void close() const;
        void write(const QByteArray & data) const;

        inline bool operator ==(const ServerClient & other) const
        { return (this->connectionId == other.connectionId); }

        inline bool operator ==(quintptr socket) const
        { return (this->connectionId == socket); }

    private:
        ConnectionType connectionType;
        SecureMode     secureMode;
        QHostAddress   clientIp;
        quint16        clientPort;
        quintptr       connectionId;
        Server       * server;
    };

    inline ConnectionType ServerClient::type() const           { return connectionType;      }
    inline SecureMode ServerClient::mode() const               { return secureMode;          }
    inline QHostAddress ServerClient::ip() const               { return clientIp;            }
    inline quint16 ServerClient::port() const                  { return clientPort;          }
    inline quintptr ServerClient::id() const                   { return connectionId;        }
    inline bool ServerClient::isNull() const                   { return (connectionId == 0); }
    inline const Server * ServerClient::serverInstance() const { return server;              }

    class ServerClientSocket : public ServerClient
    {
    public:
        ServerClientSocket();
        ServerClientSocket(ConnectionType connType, SecureMode mode, QHostAddress ip, quint16 port, QObject * socket, Server * server);
        ~ServerClientSocket();

    public:
        QPointer<QObject> socket;
    };
}

class QDebug;
QDebug operator << (QDebug d, const Network::ServerClient & conn);
QDebug operator << (QDebug d, const Network::Client & conn);

#endif // NETWORK_CLIENT_H
