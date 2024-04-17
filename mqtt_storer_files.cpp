#include "mqtt_storer_files.h"
#include "mqtt_special_symbols.h"

#include <QDirIterator>
#include <QFile>

#include <QDebug>

using namespace Mqtt;
using namespace Mqtt::Store;

class FileOpen
{
public:
    FileOpen(QFile * file, QFile::OpenMode flags);
    ~FileOpen();

    bool isOpen() const;

private:
    QFile * file;
    bool opened;
};

FileOpen::FileOpen(QFile * file, QIODevice::OpenMode flags)
    :file(file)
    ,opened(file->open(flags))
{

}

FileOpen::~FileOpen()
{
    file->close();
}

bool FileOpen::isOpen() const
{
    return opened;
}

bool existsFile(const QString & pathToFile)
{
    static thread_local QFileInfo f;
    f.setFile(pathToFile);
    return f.exists();
}

QByteArray loadFile(const QString & pathToFile)
{
    static thread_local QFile file;

    QByteArray data;
    if (existsFile(pathToFile)) {
        file.setFileName(pathToFile); {
            FileOpen f(&file, QIODevice::ReadOnly);
            if (f.isOpen()) {
                data.resize(file.size());
                file.read(data.data(), data.size());
            }
        }
    }
    return data;
}

bool saveFile(const QString & pathToFile, const QByteArray & data)
{
    static thread_local QFile file;

    bool result = false;
    file.setFileName(pathToFile);
    {
        FileOpen f(&file, QIODevice::WriteOnly);
        if (f.isOpen())
            result = (file.write(data.constData(), data.size()) == data.size());
    }
    return result;
}

bool removeFile(const QString & pathToFile)
{
    static thread_local QFile file;

    file.setFileName(pathToFile);
    if (!file.exists())
        return true;
    return file.remove();
}

FilesStorer::PathBuilder::PathBuilder(const QString & workDir)
    :workDir()
{
    changePath(workDir);
}

bool FilesStorer::PathBuilder::canStore(const QString & key)
{
    return (!key.contains(QChar(SpecialSymbols::Plus)) && !key.contains(QChar(SpecialSymbols::Hash)));
}

QString FilesStorer::PathBuilder::makePath(const QString & key)
{
    QString file_name = key;
    file_name.replace(QChar(SpecialSymbols::Slash), QChar(SpecialSymbols::Plus)).replace(':', QChar(SpecialSymbols::Hash)).append(QStringLiteral(".hex"));
    return workDir.filePath(file_name);
}

QString FilesStorer::PathBuilder::makeKey(const QString & path)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    auto parts = path.splitRef(QChar(SpecialSymbols::Slash), Qt::KeepEmptyParts, Qt::CaseSensitive);
#else
    auto parts = QStringView{path}.split(QChar(SpecialSymbols::Slash), Qt::KeepEmptyParts, Qt::CaseSensitive);
#endif
    if (!parts.isEmpty()) {
        QString key = parts.last().toString();
        return key.replace(QChar(SpecialSymbols::Plus), QChar(SpecialSymbols::Slash)).replace(QChar(SpecialSymbols::Hash), QChar(':')).remove(QStringLiteral(".hex"));
    }
    return QString();
}

void FilesStorer::PathBuilder::changePath(const QString & path)
{
    auto parts = QDir::toNativeSeparators(path).split(QDir::separator());
    for (auto part: parts)
        ensurePathExists(workDir, part);
}

void FilesStorer::PathBuilder::ensurePathExists(QDir & dir, const QString & path)
{
    if (!dir.cd(path)) {
        if (!dir.mkpath(path)) {
            qCritical() << "path builder: can't create directory" << path;
        }
        dir.cd(path);
    }
}


FilesStorer::FilesStorer(const QString & workDir)
    :builder(workDir)
{

}

FilesStorer::~FilesStorer()
{

}

bool FilesStorer::canStore(const QString & key)
{
    return builder.canStore(key);
}

QByteArray FilesStorer::load(const QString & key)
{
    return loadFile(builder.makePath(key));
}

void FilesStorer::store(const QString & key, const QByteArray & data)
{
    saveFile(builder.makePath(key), data);
}

void FilesStorer::remove(const QString & key)
{
    removeFile(builder.makePath(key));
}

void FilesStorer::beginReadKeys()
{
    iterator = QSharedPointer<QDirIterator>(
                   new QDirIterator(builder.workDir.absolutePath()
                                    ,QStringList() << QStringLiteral("*.hex")
                                    ,QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks
                                    ,QDirIterator::Subdirectories
                                    )
                   );
}

bool FilesStorer::nextKeyAvailable()
{
    return iterator->hasNext();
}

QString FilesStorer::nextKey()
{
    return builder.makeKey(iterator->next());
}

void FilesStorer::endReadKeys()
{
    iterator.clear();
}
