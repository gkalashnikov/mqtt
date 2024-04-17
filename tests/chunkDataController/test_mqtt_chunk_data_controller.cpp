#include <QTest>
#include <mqtt_chunk_data_controller.h>
#include <mqtt_constants.h>

namespace Test
{
    namespace Mqtt
    {
        class ChunkDataController : public QObject
        {
            Q_OBJECT
        private slots:
            void initTestCase();
            void testSinglePacket();
            void testSplitPackets();
            void testJoinPacket();
            void testOneSecondTimer();
            void testMaxDataSize();
            void cleanupTestCase();

        private:
            ::Mqtt::ChunkDataController * controller;
        };
    }
}

using namespace Test::Mqtt;

void ChunkDataController::initTestCase()
{
    controller = new ::Mqtt::ChunkDataController();
}

void ChunkDataController::testSinglePacket()
{
    QByteArray packet = QByteArray::fromHex("101000044D5154540402003C000444494749");

    controller->append(packet);

    QVERIFY2(controller->packetAvailable()     , "mqtt chunk data controller must has packet at this place");
    QVERIFY2(controller->takePacket() == packet, "mqtt chunk data controller must has same packet as source packet");
    QVERIFY2(controller->isEmpty()             , "mqtt chunk data controller must be empty after take packet at this place");
}

void ChunkDataController::testSplitPackets()
{
    QByteArrayList packets = QByteArrayList()
                             << QByteArray::fromHex("101000044D5154540402003C000444494749")
                             << QByteArray::fromHex("102400044d51545405c2003c05110000070800046d7950790006636c69656e74000a70617373")
                             << QByteArray::fromHex("101e00044d51545405c2003c00046d7950790006636c69656e74000a70617373")
                             << QByteArray::fromHex("101e00044d51545405c2003c00046d7950790006636c69656e74000a70617373")
                             << QByteArray::fromHex("820d00010000076d79746f70696301")
                             << QByteArray::fromHex("820c000100076d79746f70696301")
                             << QByteArray::fromHex("33140004696e666f000205020000001e436564616c6f")
                             << QByteArray::fromHex("330e0004696e666f0002436564616c6f");

    controller->append(packets.join());

    for (QByteArrayList::size_type i = 0; i < packets.size(); ++i)
    {
        QVERIFY2(controller->packetAvailable(),             "mqtt chunk data controller must has packet");
        QVERIFY2(controller->takePacket() == packets.at(i), "mqtt chunk data controller must has packet same as source packet");
    }

    QVERIFY2(controller->isEmpty(), "mqtt chunk data controller must be empty after take all packets from");
}

void ChunkDataController::testJoinPacket()
{
    QByteArray packet = QByteArray::fromHex("102400044d51545405c2003c05110000070800046d7950790006636c69656e74000a70617373");

    for (QByteArray::size_type i = 0; i < packet.size() - 1; ++i) {
        controller->append(QByteArray(1, packet.at(i)));
        QVERIFY2(!controller->packetAvailable(), "mqtt chunk data controller must has no packet");
    }

    controller->append(packet.last(1));

    QVERIFY2(controller->packetAvailable(),      "mqtt chunk data controller must has packet");
    QVERIFY2(controller->takePacket() == packet, "mqtt chunk data controller must has packet same as source packet");
    QVERIFY2(controller->isEmpty(),              "mqtt chunk data controller must be empty after take packet from");
}

void ChunkDataController::testOneSecondTimer()
{
    controller->setTimeout(1);
    controller->append(QByteArray::fromHex("330e0004696e666f0002436564616c6f"));

    QThread::sleep(std::chrono::milliseconds(1200));

    controller->oneSecondTimer();

    QVERIFY2(controller->isEmpty(), "mqtt chunk data controller must be empty after timeout has been reached");
}

void ChunkDataController::testMaxDataSize()
{
    QByteArrayList packets = QByteArrayList()
                             << QByteArray::fromHex("820d00010000076d79746f70696301")
                             << QByteArray::fromHex("820c000100076d79746f70696301")
                             << QByteArray::fromHex("330e0004696e666f0002436564616c6f");

    controller->setMaxDataSize(30);
    QVERIFY2(controller->maxDataSize() == 30, "mqtt chunk data controller max data size must be equal 30");

    controller->append(packets.at(0));
    QVERIFY2(!controller->isEmpty(), "mqtt chunk data controller must be not empty");

    controller->append(packets.at(1));
    QVERIFY2(!controller->isEmpty(), "mqtt chunk data controller must be not empty");

    controller->append(packets.at(2));
    QVERIFY2(controller->isEmpty(),  "mqtt chunk data controller must be empty");

    controller->setMaxDataSize(::Mqtt::Constants::MaxIncomingDataLength);
    QVERIFY2(controller->maxDataSize() == ::Mqtt::Constants::MaxIncomingDataLength, "mqtt chunk data controller max data size must be equal ::Mqtt::Constants::MaxIncomingDataLength");

    controller->append(packets.at(0));
    QVERIFY2(!controller->isEmpty(), "mqtt chunk data controller must be not empty");

    controller->append(packets.at(1));
    QVERIFY2(!controller->isEmpty(), "mqtt chunk data controller must be not empty");

    controller->append(packets.at(2));
    QVERIFY2(!controller->isEmpty(), "mqtt chunk data controller must be not empty");
}

void ChunkDataController::cleanupTestCase()
{
    delete controller;
}

QTEST_MAIN(Test::Mqtt::ChunkDataController)
#include "test_mqtt_chunk_data_controller.moc"
