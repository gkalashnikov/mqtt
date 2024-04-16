#include "mqtt_store_publish_unit.h"
#include "mqtt_encoder.h"
#include "mqtt_decoder.h"

using namespace Mqtt;
using namespace Mqtt::Store;

PublishUnit::PublishUnit(const QString & key, const QString & clientId, const PublishPacket & packet)
    :m_loaded(true)
    ,m_expiry_interval(0)
    ,m_initial_time(QDateTime::currentSecsSinceEpoch())
    ,m_client_id(clientId)
    ,m_key(key)
    ,m_packet()
{
    setPacket(packet);
}

PublishUnit::~PublishUnit()
{

}

void PublishUnit::setPacket(const PublishPacket & packet)
{
    m_packet = packet;
    QVariant v = ControlPacket::property(m_packet.properties(), PropertyId::MessageExpiryInterval);
    if (v.isValid()) {
        m_expiry_interval = v.toLongLong();
    }
}

void PublishUnit::beforeSend()
{
    QVariant v = ControlPacket::property(m_packet.properties(), PropertyId::MessageExpiryInterval);
    if (v.isValid()) {
        v = (m_expiry_interval - elapsed());
        ControlPacket::setProperty(m_packet.properties(), PropertyId::MessageExpiryInterval, v);
    }
}

QByteArray PublishUnit::serialize() const
{
    QByteArray data;
    data.reserve(128);

    data.append(Encoder::encodeUTF8(m_key));
    data.append(Encoder::encodeUTF8(m_client_id));
    data.append(Encoder::encodeVariableByteInteger(static_cast<quint64>(m_expiry_interval)));
    data.append(Encoder::encodeVariableByteInteger(static_cast<quint64>(m_initial_time)));
    data.append(packet().serialize(Version::Ver_5_0));

    return data.toHex();
}

void PublishUnit::unserialize(const QByteArray & hex)
{
    QByteArray data  = QByteArray::fromHex(hex);

    const quint8 * p = reinterpret_cast<const quint8*>(data.constData());
    qint64 rl = data.length();
    size_t bc = 0;

    m_key             = Decoder::decodeUTF8(p, &rl, &bc);                                     p += bc;
    m_client_id       = Decoder::decodeUTF8(p, &rl, &bc);                                     p += bc;
    m_expiry_interval = static_cast<qint64>(Decoder::decodeVariableByteInteger(p, &rl, &bc)); p += bc;
    m_initial_time    = static_cast<qint64>(Decoder::decodeVariableByteInteger(p, &rl, &bc)); p += bc;

    m_packet.unserialize(QByteArray::fromRawData(reinterpret_cast<const char*>(p), rl), Version::Ver_5_0);
}

void PublishUnit::unload()
{
    m_packet = PublishPacket();
    setLoaded(false);
}

