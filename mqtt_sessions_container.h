#ifndef MQTT_SESSIONS_CONTAINER_H
#define MQTT_SESSIONS_CONTAINER_H

#include <QObject>
#include "mqtt_session.h"
#include "mqtt_storer_factory_interface.h"

namespace Mqtt
{
    typedef QHash<QString, SessionPtr> SessionContainerBase;

    class SessionsContainer : public QObject, private SessionContainerBase
    {
        Q_OBJECT
    public:
        SessionsContainer(Store::IFactory * storerFactory, QObject * parent = Q_NULLPTR);
        ~SessionsContainer();

    signals:
        void sessionExpired(Session * session);
        void sessionBeforeDelete(Session * session);

    private slots:
        void expired();

    public:
        enum class Placing : quint8
        {
             AmongNew
            ,AmongConneted
        };

    public:
        using SessionContainerBase::iterator;
        using SessionContainerBase::const_iterator;
        using SessionContainerBase::size_type;

        using SessionContainerBase::begin;
        using SessionContainerBase::constBegin;
        using SessionContainerBase::end;
        using SessionContainerBase::constEnd;

        using SessionContainerBase::find;
        using SessionContainerBase::constFind;
        using SessionContainerBase::size;

        using SessionContainerBase::remove;

    public:
        bool canStore(const QString & key);
        void store(SessionPtr session);

        void loadAll();
        bool load(const QString & key);

    public:
        SessionPtr createForIncomingConnection(const Network::ServerClient & connection);

        void     insert(quintptr connectionId, Placing place, SessionPtr session);
        SessionPtr find(quintptr connectionId, Placing place);
        SessionPtr take(quintptr connectionId, Placing place);

        void     insert(const QString & key, SessionPtr session);
        SessionPtr find(const QString & key);

    public:
        size_t totalCount() const;
        size_t connectedCount() const;
        size_t disconnectedCount() const;

    private:
        SessionPtr createSession();
        void setSessionPacketsStorer(Session * session);
        void deleteSession(Session * session);
        void storeSession(Session * session);

    private:
        typedef QList<SessionPtr>           SessionsList;
        typedef QMap<quintptr, SessionWPtr> SessionsByConn;

    private:
        Store::IFactory   * storerFactory;
        SessionsList        sessionsNew;
        SessionsByConn      sessionsByConn;
        Store::IStorer    * storer;
    };

    inline size_t SessionsContainer::totalCount() const         { return SessionContainerBase::count();   }
    inline size_t SessionsContainer::connectedCount() const     { return sessionsByConn.count();          }
    inline size_t SessionsContainer::disconnectedCount() const  { return totalCount() - connectedCount(); }
}

#endif // MQTT_SESSIONS_CONTAINER_H
