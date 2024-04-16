#ifndef MQTT_ENCODER_H
#define MQTT_ENCODER_H

#include <QtGlobal>
#include <QtEndian>

namespace Mqtt
{
    class Encoder
    {
    public:
        static QByteArray encodeVariableByteInteger(quint64 value);

        template<typename T>
        static QByteArray encodeInteger(T value)
        {
            QByteArray buf;
            buf.resize(sizeof(value));
            value = qToBigEndian<T>(value);
            memcpy(buf.data(), &value, buf.length());
            return buf;
        }

        static QByteArray encodeTwoByteInteger(quint16 value)
        { return Encoder::encodeInteger<quint16>(value); }

        static QByteArray encodeFourByteInteger(quint32 value)
        { return Encoder::encodeInteger<quint32>(value); }

        static QByteArray encodeBinaryData(const QByteArray & bin);

        static QByteArray encodeUTF8(const QString & string)
        { return Encoder::encodeBinaryData(string.toUtf8()); }
    };
}

#endif // MQTT_ENCODER_H
