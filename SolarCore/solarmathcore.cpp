#include "solarmathcore.h"
#include <QtMath>
#include <QTransform>
#include <Qt3DRender/QCamera>
#include <Parser/solarparser.h>
#include <SolarCore/utils.h>
#include <SolarCore/cameracontroller.h>
#include <SolarCore/planetscontainer.h>
#include <SolarCore/solarobjectscontainer.h>

//Main Math data
struct SolarSystem::SolarMathCore::Data
{
    bool ready = false;
    float cameraDistance = 1.0f;
    QVector3D oldCameraPosition;
    QVector3D oldFocusedPlanetPosition;
    QVector3D startPos;

    //Time variables
    int year = SolarValues::year;
    int month = SolarValues::month;
    int day = SolarValues::day;
    int hours = 0;
    int minutes = 0;
    float seconds = 0;

    //main date and time
    QDateTime solarTime = QDateTime(QDate(year, month, day), QTime(hours, minutes));

    //frame dt
    float deltaTime = 0;

    // Time scale formula based on http://www.stjarnhimlen.se/comp/ppcomp.html
    double startD;
    double oldTimeD;
    double currentTimeD;
    double deltaTimeD = 0;
    double daysPerFrame = 0;
    double daysPerFrameScale = 0;
    float planetScale;
    bool focusedScaling = false;
    int focusedMinimumScale = 20;
    double actualScale;
    double ultraSpeed = 1.0;
    float ultraSpeedStep = 2.0f;
    double ultraSpeedMax = 64.0;

    //inner and outer radius
    double saturnRingInnerRadius = 0;
    double saturnRingOuterRadius = 0;
    double uranusRingInnerRadius = 0;
    double uranusRingOuterRadius = 0;

    //earth cloud
    float earthCloudRModifier = 1.010f;     //1.010f

    //view
    Qt3DRender::QCamera* camera = nullptr;

    //math solar objects
    SolarSystem::SolarObjectsContainer solarContainer;

    //planet container elements
    SolarSystem::PlanetsContainer* container = nullptr;

    //orbit camera controller
    SolarSystem::CameraController* cameraController = nullptr;

    Data();
    ~Data();
};

SolarSystem::SolarMathCore::Data::Data()
{
    //calculating start time
    startD = Utils::calculateTimeScale(year, month, day);
    startD += Utils::calculateUT(hours, minutes, seconds);
    oldTimeD = startD;
    currentTimeD = startD;

    //calcualting saturn and uranus rings
    auto saturn = solarContainer.solarObject(SolarObjects::Saturn);
    saturnRingOuterRadius = saturn->radius() + SolarValues::saturnOuterRadius;
    saturnRingInnerRadius = saturn->radius() + 6.630;

    auto uranus = solarContainer.solarObject(SolarObjects::Uranus);
    uranusRingOuterRadius = uranus->radius() + SolarValues::uranusOuterRadius;
    uranusRingInnerRadius = uranus->radius() + 2.0;
}

SolarSystem::SolarMathCore::Data::~Data()
{
    if (camera)
        camera = nullptr;

    if (cameraController)
        cameraController = nullptr;
}

SolarSystem::SolarMathCore::SolarMathCore(QObject* parent):
    QObject(parent),
    m_data(new Data())
{
}

SolarSystem::SolarMathCore::~SolarMathCore()
{
    delete m_data;
}

SolarSystem::SolarMathCore* SolarSystem::SolarMathCore::instance()
{
    static SolarMathCore core;
    return &core;
}

void SolarSystem::SolarMathCore::setSolarView(Qt3DRender::QCamera* camera)
{
    m_data->camera = camera;
}

Qt3DRender::QCamera* SolarSystem::SolarMathCore::solarView() const
{
    return m_data->camera;
}

float SolarSystem::SolarMathCore::getOuterRadius(SolarSystem::SolarObjects object)
{
    double outerRadius = SolarValues::solarDistance;

    switch (object) {

        case SolarObjects::Mercury:
            outerRadius += SolarObjectsValues::Mercury::radius;
            break;

        case SolarObjects::Venus:
            outerRadius += SolarObjectsValues::Venus::radius;
            break;

        case SolarObjects::Earth:
            outerRadius += SolarObjectsValues::Earth::radius;
            break;

        case SolarObjects::Mars:
            outerRadius += SolarObjectsValues::Mars::radius;
            break;

        case SolarObjects::Jupiter:
            outerRadius += SolarObjectsValues::Jupier::radius;
            break;

        case SolarObjects::Neptune:
            outerRadius += SolarObjectsValues::Neptune::radius;
            break;

        case SolarObjects::Saturn:
            outerRadius += SolarObjectsValues::Saturn::radius + SolarValues::saturnOuterRadius;
            break;

        case SolarObjects::Uranus:
            outerRadius += SolarObjectsValues::Uranus::radius + SolarValues::uranusOuterRadius;
            break;

        case SolarObjects::Moon:
            outerRadius += SolarObjectsValues::Moon::radius;
            break;

        case SolarObjects::Pluto:
            outerRadius += SolarObjectsValues::Pluto::radius;
            break;

        case SolarObjects::Sun:
            outerRadius = SolarObjectsValues::Sun::radius / 100.0;
            break;

        default:
            break;

    }

    return static_cast<float>(outerRadius);
}

void SolarSystem::SolarMathCore::solarObjectPosition(SolarSystem::SolarObjects object)
{
    //get planet
    auto solarObj = m_data->solarContainer.solarObject(object);

    //object exists
    if (solarObj)
    {
        //calculation only for solar system planets
        if (object != SolarObjects::Sun)
        {
            // Calculate the planet orbital elements from the current time in days
            auto N = (solarObj->N1() + solarObj->N2() * m_data->currentTimeD) * M_PI/ 180;
            auto iPlanet = (solarObj->i1() + solarObj->i2() * m_data->currentTimeD) * M_PI / 180;
            auto w = (solarObj->w1() + solarObj->w2() * m_data->currentTimeD) * M_PI / 180;
            auto a = solarObj->a1() + solarObj->a2() * m_data->currentTimeD;
            auto e = solarObj->e1() + solarObj->e2() * m_data->currentTimeD;
            auto M = (solarObj->M1() + solarObj->M2() * m_data->currentTimeD) * M_PI / 180;
            auto E = M + e * std::sin(M) * (1.0 + e * std::cos(M));

            auto xv = a * (std::cos(E) - e);
            auto yv = a * (std::sqrt(1.0 - e * e) * std::sin(E));
            auto v = std::atan2(yv, xv);

            // Calculate the distance (radius)
            auto r = std::sqrt(xv * xv + yv * yv);

            // From http://www.davidcolarusso.com/astro/
            // Modified to compensate for the right handed coordinate system of OpenGL
            auto xh = r * (std::cos(N) * std::cos(v + w)
                           - std::sin(N) * std::sin(v + w) * std::cos(iPlanet));
            auto zh = -r * (std::sin(N) * std::cos(v + w)
                            + std::cos(N) * std::sin(v + w) * std::cos(iPlanet));
            auto yh = r * (std::sin(w + v) * std::sin(iPlanet));

            // Apply the position offset from the center of orbit to the bodies
            SolarObjects centerOfOrbit = solarObj->centerOfOrbit();
            auto centerObj = m_data->solarContainer.solarObject(centerOfOrbit);

            solarObj->setX(centerObj->x() + xh * SolarValues::auScale);
            solarObj->setY(centerObj->y() + yh * SolarValues::auScale);
            solarObj->setZ(centerObj->z() + zh * SolarValues::auScale);
        }

        solarObj->setRoll((solarObj->roll() + m_data->deltaTimeD/ solarObj->period() * 360.0));

        //recalculation to 3D objects
        SolarObject3D* visualSolarObject = m_data->container->planets()[object];

        //if interface in container
        if (visualSolarObject != nullptr)
        {
            visualSolarObject->setX(solarObj->x());
            visualSolarObject->setY(solarObj->y());
            visualSolarObject->setZ(solarObj->z());
            visualSolarObject->setRoll(solarObj->roll());
        }
    }
}

void SolarSystem::SolarMathCore::advanceTime(SolarSystem::SolarObjects object)
{
    if (object == SolarObjects::SolarSystemView)
        m_data->daysPerFrame = m_data->daysPerFrameScale; //*10
    else if (object == SolarObjects::Mercury || object == SolarObjects::Venus)
        m_data->daysPerFrame = m_data->daysPerFrameScale * m_data->solarContainer.solarObject(object)->period()/15000.0;
    else
        m_data->daysPerFrame = m_data->daysPerFrameScale * m_data->solarContainer.solarObject(object)->period()/100.0;

    //add solar time
    m_data->solarTime = m_data->solarTime.addMSecs(m_data->deltaTime * 1000.0f * m_data->daysPerFrame * m_data->ultraSpeed);

    //save helpers values
    m_data->hours = m_data->solarTime.time().hour();
    m_data->minutes = m_data->solarTime.time().minute();
    m_data->seconds = m_data->solarTime.time().second();
    m_data->year = m_data->solarTime.date().year();
    m_data->month = m_data->solarTime.date().month();
    m_data->day = m_data->solarTime.date().day();

    //Advance the time in days
    m_data->oldTimeD = m_data->currentTimeD;

    //update currentTimeD
    m_data->currentTimeD = Utils::calculateTimeScale(m_data->year, m_data->month, m_data->day);
    m_data->currentTimeD += Utils::calculateUT(m_data->hours, m_data->minutes, m_data->seconds);

    //get deltaD
    m_data->deltaTimeD = m_data->currentTimeD - m_data->oldTimeD;
}

void SolarSystem::SolarMathCore::setSolarObjectsScale(float scale, bool focused)
{
    if (!focused)
        m_data->actualScale = scale;

    if (scale <= m_data->focusedMinimumScale && (m_data->focusedScaling || focused))
        m_data->planetScale = m_data->focusedMinimumScale;
    else
        m_data->planetScale = m_data->actualScale;
}

void SolarSystem::SolarMathCore::updateSolarView(SolarSystem::SolarObjects object)
{
    SolarObject3D* solarObj = nullptr;
    PlanetArray& planets = m_data->container->planets();

    if (object != SolarObjects::SolarSystemView)
        solarObj = planets[object];
    else
        solarObj = planets[SolarObjects::Sun];

    if (solarObj != nullptr)
        m_data->camera->setViewCenter(QVector3D(solarObj->x(), solarObj->y(), solarObj->z()));
}

void SolarSystem::SolarMathCore::setSolarSystemSpeed(float speed)
{
    m_data->daysPerFrameScale = speed;
}

void SolarSystem::SolarMathCore::setPlanetsContainer(PlanetsContainer* planetsContainer)
{
    m_data->container = planetsContainer;
}

void SolarSystem::SolarMathCore::changeSolarSystemScale(float scale, bool focused)
{
    setSolarObjectsScale(scale, focused);

    auto scaling = m_data->planetScale;

    for (auto& planet : m_data->container->planets())
    {
        switch (planet.first)
        {
        case SolarObjects::Sun:
            planet.second->setR(SolarParser::parseSolarObjectRadius(planet.first) * scaling/80.0f);
            break;

        case SolarObjects::Mercury:
        case SolarObjects::Venus:
        case SolarObjects::Earth:
        case SolarObjects::Mars:
        case SolarObjects::Jupiter:
        case SolarObjects::Saturn:
        case SolarObjects::Uranus:
        case SolarObjects::Neptune:
        case SolarObjects::Pluto:
        case SolarObjects::Moon:
            planet.second->setR(SolarParser::parseSolarObjectRadius(planet.first) * scaling);
            break;

        case SolarObjects::SaturnRing:
            m_data->saturnRingOuterRadius = m_data->saturnRingOuterRadius * scaling;
            m_data->saturnRingInnerRadius = m_data->saturnRingInnerRadius * scaling;
            break;

        case SolarObjects::UranusRing:
            m_data->uranusRingInnerRadius = m_data->uranusRingInnerRadius * scaling;
            m_data->uranusRingOuterRadius = m_data->uranusRingOuterRadius * scaling;
            break;
        default:
            break;
        }
    }
}

void SolarSystem::SolarMathCore::setDeltaTime(float dt)
{
    m_data->deltaTime = dt;
}

QDateTime SolarSystem::SolarMathCore::getTime() const
{
    return m_data->solarTime;
}

void SolarSystem::SolarMathCore::additionalCalculation()
{
    setupPlanetRings();
    atmosphereCalculations();
}

void SolarSystem::SolarMathCore::setCameraController(SolarSystem::CameraController *controller)
{
    m_data->cameraController = controller;
}

SolarSystem::CameraController* SolarSystem::SolarMathCore::viewController() const
{
    return m_data->cameraController;
}

void SolarSystem::SolarMathCore::updateSolarViewZoomLimit(SolarSystem::SolarObjects object)
{
    if (object == SolarObjects::SolarSystemView)
    {
        if (m_data->cameraController)
        {
            m_data->cameraController->setDefaultZoomLimit();
            m_data->cameraController->setDefaultZoomSpeed();
        }
    }
    else
    {
        //get radius
        auto solarObjRadius = SolarParser::parseSolarObjectRadius(object);
        auto zoomLimit = m_data->planetScale * solarObjRadius * 4.0f;

        //empiricic calculations
        zoomLimit = calculateZoomLimit(object, zoomLimit);

        if (m_data->cameraController)
        {
            m_data->cameraController->setZoomLimit(zoomLimit);
            m_data->cameraController->setZoomSpeed(m_data->cameraController->defaultZoomSpeed()/3.0f);
        }
    }
}

QVector3D SolarSystem::SolarMathCore::objectPosition(SolarSystem::SolarObjects object)
{
    QVector3D pos {0, 0, 0};

    if (object != SolarObjects::SolarSystemView)
    {
        auto obj = m_data->container->planets()[object];
        pos = QVector3D(obj->transform()->translation());
    }

    return pos;
}

QVector3D SolarSystem::SolarMathCore::viewPositionOfObject(SolarSystem::SolarObjects object)
{
    auto solarObj = m_data->container->planets()[object];
    auto pos = QVector3D {0, 0, 0};

    if (solarObj != nullptr)
    {
        auto solarObjPos = QVector3D(solarObj->x(), solarObj->y(), solarObj->z());

        //vector on object
        auto onTarget = solarObjPos - m_data->camera->position();

        //get dist
        auto dist = onTarget.length();

        //calculate need dist to camera
        auto limit = calculateZoomLimit(object);
        auto needDist = dist - limit;

        if (needDist <= 0)
            needDist = limit - dist;

        //get position
        auto onTargetLimit = onTarget.normalized() * needDist;

        //get need cam pos
        pos = onTargetLimit + m_data->camera->position();
    }

    return pos;
}

float SolarSystem::SolarMathCore::solarSystemSpeed() const
{
    return m_data->daysPerFrameScale;
}

void SolarSystem::SolarMathCore::changeExtraSpeed() const
{
    if (m_data->ultraSpeed * m_data->ultraSpeedStep <= m_data->ultraSpeedMax)
        m_data->ultraSpeed *= m_data->ultraSpeedStep;
    else
        m_data->ultraSpeed = 1.0;
}

double SolarSystem::SolarMathCore::extraSpeed() const
{
    return m_data->ultraSpeed;
}

void SolarSystem::SolarMathCore::resetExtraSpeed() const
{
    m_data->ultraSpeed = 1.0;
}

void SolarSystem::SolarMathCore::calculateAllSolarObjectsPosiitons()
{
    auto updateCount = m_data->container->planetsNumber();

    //update solar objects position
    for (int i = 0; i < updateCount; ++i)
        solarObjectPosition((SolarObjects)i);
}

void SolarSystem::SolarMathCore::setupPlanetRings()
{
    PlanetArray& planets = m_data->container->planets();

    if (planets.count(SolarObjects::SaturnRing) && planets.count(SolarObjects::Saturn))
    {
        SolarObject3D* saturn = planets[SolarObjects::Saturn];
        SolarObject3D* saturnRing = planets[SolarObjects::SaturnRing];

        saturnRing->setX(saturn->x());
        saturnRing->setY(saturn->y());
        saturnRing->setZ(saturn->z());
        saturnRing->setTilt(saturn->tilt());
        saturnRing->setRoll(saturn->roll()/10.0f);
        saturnRing->setR((m_data->saturnRingInnerRadius + m_data->saturnRingOuterRadius)/1.75);
    }

    if (planets.count(SolarObjects::UranusRing) && planets.count(SolarObjects::Uranus))
    {
        SolarObject3D* uranus = planets[SolarObjects::Uranus];
        SolarObject3D* uranusRings = planets[SolarObjects::UranusRing];

        uranusRings->setX(uranus->x());
        uranusRings->setY(uranus->y());
        uranusRings->setZ(uranus->z());
        uranusRings->setTilt(uranus->tilt());
        uranusRings->setRoll(uranus->roll()/10.0f);
        uranusRings->setR((m_data->uranusRingInnerRadius + m_data->uranusRingOuterRadius)/1.75);
    }
}

void SolarSystem::SolarMathCore::atmosphereCalculations()
{
    PlanetArray& planets = m_data->container->planets();

    if (planets.count(SolarObjects::EarthCloud) && planets.count(SolarObjects::Earth))
    {
        SolarObject3D* earth = planets[SolarObjects::Earth];
        SolarObject3D* earthCloud = planets[SolarObjects::EarthCloud];

        earthCloud->setX(earth->x());
        earthCloud->setY(earth->y());
        earthCloud->setZ(earth->z());
        earthCloud->setTilt(earth->tilt());
        earthCloud->setRoll(earth->roll()/1.2f);
        earthCloud->setR(earth->r() * m_data->earthCloudRModifier);
    }
}

float SolarSystem::SolarMathCore::calculateZoomLimit(SolarSystem::SolarObjects object, float limit)
{
    float finalLimit = limit;

    switch (object) {
    case SolarObjects::Sun:
        finalLimit = m_data->cameraController->defaultZoomLimit();
        break;

    case SolarObjects::Mercury:
        finalLimit *= 2.0f;
        break;

    case SolarObjects::Jupiter:
        finalLimit /= 1.5f;
        break;

    case SolarObjects::Pluto:
        finalLimit *= 1.5f;
        break;

    default:
        break;
    }

    return finalLimit;
}

float SolarSystem::SolarMathCore::calculateZoomLimit(SolarSystem::SolarObjects object)
{
    //get radius
    auto solarObjRadius = SolarParser::parseSolarObjectRadius(object);
    auto zoomLimit = m_data->planetScale * solarObjRadius * 4.0f;

    //empiricic calculations
    zoomLimit = calculateZoomLimit(object, zoomLimit);

    return zoomLimit;
}
