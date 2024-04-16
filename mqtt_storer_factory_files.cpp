#include "mqtt_storer_factory_files.h"
#include "mqtt_storer_files.h"

using namespace Mqtt::Store;

FilesStorerFactory::FilesStorerFactory(const QString & rootWorkDir)
    :IFactory()
    ,rootWorkDir(rootWorkDir)
{

}

FilesStorerFactory::~FilesStorerFactory()
{

}

IStorer * FilesStorerFactory::createStorer(const QString & key)
{
    QString work_dir = rootWorkDir;
    return new FilesStorer(work_dir.append('/').append(key));
}
