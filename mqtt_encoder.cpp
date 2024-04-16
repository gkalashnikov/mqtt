#include "mqtt_encoder.h"

#define MAX_VARINT_BYTES 10

using namespace Mqtt;

QByteArray Encoder::encodeVariableByteInteger(quint64 value)
{
    QByteArray buf;
    buf.reserve(MAX_VARINT_BYTES + 1);
    int n = 0;
    for ( ; value > 127; n++) {
        quint8 byte = (0x80 | quint8(value & 0x7F));
        buf.append(reinterpret_cast<const char*>(&byte), 1);
        value >>= 7;
    }
    buf.append(reinterpret_cast<const char*>(&value), 1);
    return buf;
}

QByteArray Encoder::encodeBinaryData(const QByteArray & bin)
{
    QByteArray result;
    result.reserve(bin.length() + 2);
    return result.append(Encoder::encodeInteger<quint16>(bin.length())).append(bin);
}
