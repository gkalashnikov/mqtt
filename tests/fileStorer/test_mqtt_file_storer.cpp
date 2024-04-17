#include <QTest>
#include <mqtt_storer_files.h>
#include <mqtt_storer_factory_files.h>

namespace Test
{
    namespace Mqtt
    {
        class FilesStorer : public QObject
        {
            Q_OBJECT
        private slots:
            void initTestCase();
            void testRelativeStore();
            void testAbsoluteStore();
            void testInvalidKeys();
            void cleanupTestCase();

        private:
            QDir root_relative_dir;
            QDir root_absolute_dir;

            ::Mqtt::Store::IFactory * factory_relative;
            ::Mqtt::Store::IFactory * factory_absolute;

            ::Mqtt::Store::IStorer  * folder_relative;
            ::Mqtt::Store::IStorer  * folder_absolute;

            // keys for this project (publish topics (without '+' and '#'), host address, justified numeric value)
            QStringList keys {
                                 QStringLiteral("/a/b/c/d/")
                                ,QStringLiteral("/a/b/c/d")
                                ,QStringLiteral("a/b/c/d/")
                                ,QStringLiteral("a/b/c/d")
                                ,QStringLiteral("127.0.0.1:1883")
                                ,QStringLiteral("localhost:8883")
                                ,QStringLiteral("mqtt://127.0.0.1:1883")
                                ,QStringLiteral("wss://localhost:8883")
                                ,QStringLiteral("000000001")
                                ,QStringLiteral("000000002")
                                ,QStringLiteral("000000003")
                             };
        };
    }
}

using namespace Test::Mqtt;

void FilesStorer::initTestCase()
{
    const QString folder = QStringLiteral("folder");

    root_relative_dir = QDir(QStringLiteral("./tmp_files_storer"));
    root_absolute_dir = QDir(root_relative_dir.absolutePath());

    if (root_relative_dir.exists())
        root_relative_dir.removeRecursively();

    QVERIFY2(!root_relative_dir.exists(), "the dir path must not exist");
    QVERIFY2(!root_absolute_dir.exists(), "the dir path must not exist");

    QDir relative(root_relative_dir.path().append(QStringLiteral("/relative")));
    factory_relative = new ::Mqtt::Store::FilesStorerFactory(relative.path());
    folder_relative = factory_relative->createStorer(folder);

    QVERIFY2(relative.exists(folder), QString("\"%1/%2\" must exist").arg(relative.path()).arg(folder).toUtf8().constData());
    QVERIFY2(QDir(relative.filePath(folder)).isEmpty(), QString("\"%1/%2\" must be empty").arg(relative.path()).arg(folder).toUtf8().constData());

    QDir absolute(root_absolute_dir.path().append(QStringLiteral("/absolute/")));
    factory_absolute = new ::Mqtt::Store::FilesStorerFactory(absolute.path());
    folder_absolute = factory_absolute->createStorer(folder);

    QVERIFY2(absolute.exists(folder), QString("\"%1/%2\" must exist").arg(absolute.path()).arg(folder).toUtf8().constData());
    QVERIFY2(QDir(absolute.filePath(folder)).isEmpty(), QString("\"%1/%2\" must be empty").arg(absolute.path()).arg(folder).toUtf8().constData());
}

void FilesStorer::testRelativeStore()
{
    for (auto k: keys) {
        folder_relative->store(k, k.toUtf8());
    }

    folder_relative->beginReadKeys();
    while (folder_relative->nextKeyAvailable()) {
        QString key = folder_relative->nextKey();
        QByteArray data = folder_relative->load(key);
        QVERIFY2(QString::fromUtf8(data) == key, "key and data must be equals");
    }
    folder_relative->endReadKeys();

    for (auto k: keys) {
        folder_relative->remove(k);
        QVERIFY2(folder_relative->load(k).isEmpty(), "data must be empty after remove");
    }
}

void FilesStorer::testAbsoluteStore()
{
    for (auto k: keys) {
        folder_absolute->store(k, k.toUtf8());
    }

    folder_absolute->beginReadKeys();
    while (folder_absolute->nextKeyAvailable()) {
        QString key = folder_absolute->nextKey();
        QByteArray data = folder_absolute->load(key);
        QVERIFY2(QString::fromUtf8(data) == key, "key and data must be equals");
    }
    folder_absolute->endReadKeys();

    for (auto k: keys) {
        folder_absolute->remove(k);
        QVERIFY2(folder_absolute->load(k).isEmpty(), "data must be empty after remove");
    }
}

void FilesStorer::testInvalidKeys()
{
    QStringList invalid_keys = QStringList() << QStringLiteral("fd/+") << QStringLiteral("+") << QStringLiteral("/#") << QStringLiteral("#");

    for (auto k: invalid_keys)
        QVERIFY2(!folder_absolute->canStore(k), QString("\"%1\" must be invalid for store").arg(k).toUtf8().constData());

}

void FilesStorer::cleanupTestCase()
{
    if (root_relative_dir.exists())
        root_relative_dir.removeRecursively();

    delete factory_relative;
    delete factory_absolute;

    delete folder_relative;
    delete folder_absolute;

    QVERIFY2(!root_relative_dir.exists(), "the dir path must not exist");
    QVERIFY2(!root_absolute_dir.exists(), "the dir path must not exist");
}

QTEST_MAIN(Test::Mqtt::FilesStorer)
#include "test_mqtt_file_storer.moc"
