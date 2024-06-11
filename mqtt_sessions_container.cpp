#include "mqtt_sessions_container.h"
#include <functional>
#include <QTimer>

using namespace Mqtt;

SessionsContainer::SessionsContainer(Store::IFactory * storerFactory, QObject * parent)
    :QObject(parent)
    ,storerFactory(storerFactory)
    ,storer(storerFactory->createStorer(QStringLiteral("sessions")))
{

}

SessionsContainer::~SessionsContainer()
{
    delete storer;
    storer = Q_NULLPTR;

    sessionsByConn.clear();
    sessionsNew.clear();
    SessionContainerBase::clear();
}

SessionPtr SessionsContainer::createForIncomingConnection(const Network::ServerClient & connection)
{
    SessionPtr session = createSession();
    session->setConnection(connection);
    return session;
}

void SessionsContainer::insert(quintptr connectionId, Placing place, SessionPtr session)
{
    switch (place)
    {
        case Placing::AmongNew: {
            sessionsNew.append(session);
            return;
        }

        case Placing::AmongConneted: {
            sessionsByConn[connectionId] = session.toWeakRef();
            return;
        }

        default: break;
    }
}

SessionPtr SessionsContainer::find(quintptr connectionId, Placing place)
{
    switch (place)
    {
        case Placing::AmongNew:
        {
            auto it = std::find_if(sessionsNew.begin(), sessionsNew.end(),
                                      [&connectionId] (const SessionPtr & session) -> bool
            { return (session->connection() == connectionId); });
            return (it != sessionsNew.end() ? *it : SessionPtr(Q_NULLPTR));
        }

        case Placing::AmongConneted:
        {
            auto it = sessionsByConn.find(connectionId);
            if (it != sessionsByConn.end()) {
                SessionPtr session = (*it).toStrongRef();
                if (!session.isNull())
                    return session;
                sessionsByConn.erase(it);
            }
            return SessionPtr(Q_NULLPTR);
        }

        default: break;
    }

    return SessionPtr(Q_NULLPTR);
}

SessionPtr SessionsContainer::find(const QString & key)
{
    quint32 attempts = 1;

    do {
        auto it = SessionContainerBase::find(key);

        if (it != SessionContainerBase::end())
            return *it;

        if (attempts != 0 && !load(key))
            break;
    }
    while (attempts--);

    return SessionPtr(Q_NULLPTR);
}

SessionPtr SessionsContainer::take(quintptr connectionId, Placing place)
{
    switch (place)
    {
        case Placing::AmongNew:
        {
            auto it = std::find_if(sessionsNew.begin(), sessionsNew.end(),
                                      [&connectionId] (const SessionPtr & session) -> bool
            { return (session->connection() == connectionId); });
            if (it != sessionsNew.end()) {
                SessionPtr session = *it;
                sessionsNew.erase(it);
                return session;
            }
            break;
        }

        case Placing::AmongConneted:
        {
            auto it = sessionsByConn.find(connectionId);
            if (it != sessionsByConn.end()) {
                SessionPtr session = (*it).toStrongRef();
                sessionsByConn.erase(it);
                return session;
            }
            break;
        }

        default: break;
    }

    return SessionPtr(Q_NULLPTR);
}

void SessionsContainer::insert(const QString & key, SessionPtr session)
{
    SessionContainerBase::insert(key, session);
    storeSession(session.data());
    setSessionPacketsStorer(session.data());
}

void SessionsContainer::loadAll()
{
    storer->beginReadKeys();
    while (storer->nextKeyAvailable()) {
        load(storer->nextKey());
    }
    storer->endReadKeys();
}

bool SessionsContainer::load(const QString & key)
{
    QByteArray data = storer->load(key);

    if (data.isEmpty())
        return false;

    SessionPtr session = createSession();
    session->unserialize(data);

    if (session->clientId().isEmpty())
        return false;

    SessionContainerBase::insert(session->clientId(), session);
    setSessionPacketsStorer(session.data());

    return true;
}

bool SessionsContainer::canStore(const QString & key)
{
    return storer->canStore(key);
}

void SessionsContainer::store(SessionPtr session)
{
    if (!session.isNull())
        storeSession(session.data());
}

SessionPtr SessionsContainer::createSession()
{
    Session * s = new Mqtt::Session(this);
    SessionPtr session = SessionPtr(s, std::bind(&SessionsContainer::deleteSession, this, s));
    connect(s, &Session::expired, this, &SessionsContainer::expired);
    return session;
}

void SessionsContainer::setSessionPacketsStorer(Session * session)
{
    if (!session->hasPendingPacketsStorer() && !session->clientId().isEmpty())
        session->setPendingPacketsStorer(storerFactory->createStorer(QStringLiteral("pending/") + session->clientId()));
}

void SessionsContainer::expired()
{
    Session * session = qobject_cast<Session*>(sender());
    if (session != Q_NULLPTR)
        QTimer::singleShot(0, Qt::TimerType::CoarseTimer, std::bind(&SessionContainerBase::remove, static_cast<SessionContainerBase*>(this), (session->clientId())));
    emit sessionExpired(session);
}

void SessionsContainer::deleteSession(Session * session)
{
    session->beforeDelete();
    emit sessionBeforeDelete(session);
    storeSession(session);
    delete session;
}

void SessionsContainer::storeSession(Session * session)
{
    if (!session->clientId().isEmpty()) {
        if (session->hasBeenExpired() || session->isClean()) {
            storer->remove(session->clientId());
        } else {
            storer->store(session->clientId(), session->serialize());
        }
    }
}
