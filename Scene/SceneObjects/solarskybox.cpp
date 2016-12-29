#include "solarskybox.h"

SolarSystem::SolarSkyBox::SolarSkyBox(Qt3DCore::QNode *parent):
    Qt3DExtras::QSkyboxEntity(parent),
    positionTimer(new QTimer(this))
{
    //create skybox from file
    setBaseName(QStringLiteral("qrc:/Resources/Skybox/stars"));
    setExtension(QStringLiteral(".webp"));

    QObject::connect(positionTimer, &QTimer::timeout, [&] {

        if (_camera)
            setCameraPosition(_camera->position());

    });
}

SolarSystem::SolarSkyBox::~SolarSkyBox()
{
    if (_camera)
        positionTimer->stop();
}

void SolarSystem::SolarSkyBox::setCamera(Qt3DRender::QCamera *camera)
{
    _camera = camera;
    positionTimer->start(updateInterval);
}
