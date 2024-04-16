#ifndef MQTT_DECODER_H
#define MQTT_DECODER_H

#include <QtGlobal>
#include <QtEndian>

namespace Mqtt
{
    class Decoder
    {
    public:
        static quint64 decodeVariableByteInteger(const quint8 * buf, qint64 * remainigLenght, size_t * bytesCount);

        template<typename T>
        static T decodeInteger(const quint8 * buf, qint64 * remainigLenght, size_t * bytesCount)
        {
            T value = 0;
            memcpy(&value, buf, sizeof(value));
            *bytesCount = sizeof(value);
            *remainigLenght -= sizeof(value);
            return qFromBigEndian<T>(value);
        }

        static quint16 decodeTwoByteInteger(const quint8 * buf, qint64 * remainigLenght, size_t * bytesCount)
        { return Decoder::decodeInteger<quint16>(buf, remainigLenght, bytesCount); }

        static quint32 decodeFourByteInteger(const quint8 * buf, qint64 * remainigLenght, size_t * bytesCount)
        { return Decoder::decodeInteger<quint32>(buf, remainigLenght, bytesCount); }

        static QString decodeUTF8(const quint8 * buf, qint64 * remainingLength, size_t * bytesCount);

        static QByteArray decodeBinaryData(const quint8 * buf, qint64 * remainingLength, size_t * bytesCount);
    };
}

#endif // MQTT_DECODER_H
