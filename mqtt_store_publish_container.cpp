#include "mqtt_store_publish_container.h"
#include "mqtt_storer_interface.h"
#include <QTimer>
#include <QTimerEvent>
#include <QElapsedTimer>
#include <QDebug>

using namespace Mqtt;
using namespace Mqtt::Store;

class EmptyStorer : public IStorer
{
public:
    EmptyStorer();
    ~EmptyStorer() override;

public:
    QByteArray load(const QString & key) override;
    void store(const QString & key, const QByteArray & data) override;
    void remove(const QString & key) override;
    void beginReadKeys() override;
    bool nextKeyAvailable() override;
    QString nextKey() override;
    void endReadKeys() override;
};

EmptyStorer::EmptyStorer()  { }
EmptyStorer::~EmptyStorer() { }
QByteArray EmptyStorer::load(const QString & key) { return QByteArray(); }
void EmptyStorer::store(const QString &, const QByteArray &) { }
void EmptyStorer::remove(const QString &) { }
void EmptyStorer::beginReadKeys() { }
bool EmptyStorer::nextKeyAvailable() { return false; }
QString EmptyStorer::nextKey() { return QString(); }
void EmptyStorer::endReadKeys() { }

Q_GLOBAL_STATIC(EmptyStorer, kEmptyStorer)


PublishContainer::PublishContainer(QObject * parent)
    :QObject(parent)
    ,m_storer(kEmptyStorer)
{

}

PublishContainer::PublishContainer(IStorer * storer, QObject * parent)
    :QObject(parent)
    ,m_storer(storer != Q_NULLPTR ? storer : kEmptyStorer)
{
    preload();
}

PublishContainer::~PublishContainer()
{
    executeSync();
    if (m_storer != kEmptyStorer) {
        delete m_storer;
        m_storer = kEmptyStorer;
    }
}

PublishContainer::size_type PublishContainer::remove(const QString & key)
{
    scheduleSync(key);
    return BaseContainer::remove(key);
}

void PublishContainer::add(const QString & key, const QString & clientId, const PublishPacket & packet)
{
    if (packet.payload().isEmpty())
        remove(key);
    else
        operator[](key) = std::move(PublishUnit(key, clientId, packet));
}

PublishUnit & PublishContainer::operator[](const QString & key)
{
    auto it = BaseContainer::find(key);
    if (it == end())
        return *insert(key, PublishUnit());
    scheduleSync(key);
    return *it;
}

void PublishContainer::clear()
{
    m_sync.clear();
    BaseContainer::clear();
}

void PublishContainer::preload()
{
    QString client_id;
    PublishPacket packet;

    m_storer->beginReadKeys();
    while (m_storer->nextKeyAvailable()) {
        QString key = m_storer->nextKey();
        PublishUnit unit = PublishUnit(key, client_id, packet);
        unit.setLoaded(false);
        BaseContainer::insert(key, unit);
    }
    m_storer->endReadKeys();
}

BaseContainer::iterator PublishContainer::erase(iterator it)
{
    scheduleSync(it.key());
    return BaseContainer::erase(it);
}

BaseContainer::iterator PublishContainer::insert(const QString & key, const PublishUnit & value)
{
    scheduleSync(key);
    return BaseContainer::insert(key, value);
}

BaseContainer::iterator PublishContainer::insert(const_iterator pos, const QString & key, const PublishUnit & value)
{
    scheduleSync(key);
    return BaseContainer::insert(pos, key, value);
}

void PublishContainer::removeAll()
{
    auto it = begin();
    while (it != end())
        it = PublishContainer::erase(it);
}

QString PublishContainer::nextOrderedKey() const
{
    int index = isEmpty() ? 1 : (last().key().toInt() + 1);
    return QString::asprintf("%010d", index);
}

bool PublishContainer::hasStorer() const
{
    return  (m_storer != kEmptyStorer);
}

void PublishContainer::setStorer(IStorer * storer)
{
    if (m_storer != kEmptyStorer)
        delete m_storer;
    m_storer = (storer != Q_NULLPTR ? storer : kEmptyStorer);
    clear();
    preload();
}

void PublishContainer::executeSync()
{
    if (m_sync_timer_id != 0) {
        killTimer(m_sync_timer_id);
        m_sync_timer_id = 0;
    }

    if (!m_sync.isEmpty()) {
        sync(m_sync.dequeue());
        scheduleSync(0);
    }
}

void PublishContainer::scheduleSync(quint32 msDelay)
{
    if (m_sync_timer_id == 0)
        m_sync_timer_id = startTimer(msDelay);
}

void PublishContainer::scheduleSync(const QString & key)
{
    m_sync.enqueue(key);
    scheduleSync(0);
}

void PublishContainer::loadUnit(const QString & key, PublishUnit & unit)
{
    unit.unserialize(storer()->load(key));
    unit.setKey(key);
    unit.setLoaded(true);
}

void PublishContainer::timerEvent(QTimerEvent * event)
{
    if (event->timerId() == m_sync_timer_id) {
        event->accept();
        executeSync();
        return;
    }

    QObject::timerEvent(event);
}

void PublishContainer::syncAll()
{
    while (!m_sync.isEmpty())
        sync(m_sync.dequeue());
}

void PublishContainer::sync(const QString & key)
{
    auto it = find(key);
    if (it != end()) {
        m_storer->store(key, (*it).serialize());
        (*it).unload();
        return;
    }
    m_storer->remove(key);
}
