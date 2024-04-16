#include "mqtt_decoder.h"
#include <QByteArray>
#include <QString>

using namespace Mqtt;

quint64 Decoder::decodeVariableByteInteger(const quint8 * buf, qint64 * rl, size_t * bytesCount)
{
    *bytesCount = 0;
    quint64 x = 0;

    for (quint32 shift = 0, n = 0; ((shift < 64) && (*rl > 0)); shift += 7) {
        quint64 b = quint64(buf[n]);
        n++; --(*rl);
        x |= (b & 0x7F) << shift;
        if ((b & 0x80) == 0) {
            *bytesCount = n;
            return x;
        }
    }

    return 0;
}

QString Decoder::decodeUTF8(const quint8 * buf, qint64 * rl, size_t * bc)
{
    *bc  = 0;
    quint16 len = ((*rl >= sizeof(quint16)) ? Decoder::decodeInteger<quint16>(buf, rl, bc) : 0);
    QString val = ((*rl >= len) ? QString::fromUtf8(reinterpret_cast<const char*>(buf + sizeof(len)), len) : QString());
    *bc += len; *rl -= len;
    return val;
}

QByteArray Decoder::decodeBinaryData(const quint8 * buf, qint64 * rl, size_t * bc)
{
    *bc  = 0;
    quint16    len = ((*rl >= sizeof(quint16)) ? Decoder::decodeInteger<quint16>(buf, rl, bc) : 0);
    QByteArray val = ((*rl >= len) ? QByteArray(reinterpret_cast<const char*>(buf + sizeof(len)), len) : QByteArray());
    *bc += len; *rl -= len;
    return val;
}
