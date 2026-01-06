#include "camera_manip.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr double kDefaultEarthRadius = 6371000.0;
    constexpr double kMinDistance = 10.0;
    constexpr double kMinRadiusThreshold = 1000.0;
    constexpr double kZoomInFactor = 0.8;
    constexpr double kZoomOutFactor = 1.25;
    constexpr double kTiltSensitivity = 100.0;

    double clampTilt(double tilt, double maxTilt)
    {
        return std::clamp(tilt, 0.0, maxTilt);
    }

    double getEarthRadius(const osg::observer_ptr<osg::Node>& node)
    {
        if (node.valid())
        {
            double r = node->getBound().radius();
            if (r > kMinRadiusThreshold)
                return r;
        }
        return kDefaultEarthRadius;
    }

    void computeLocalFrame(const osg::Vec3d& center, osg::Vec3d& up, osg::Vec3d& east, osg::Vec3d& north)
    {
        up = center;
        up.normalize();

        east = osg::Vec3d(0, 0, 1) ^ up;
        if (east.length2() == 0.0)
            east.set(1.0, 0.0, 0.0);
        east.normalize();

        north = up ^ east;
        north.normalize();
    }
}

GoogleMapsManipulator::GoogleMapsManipulator()
    : _distance(100.0)
    , _lastX(0)
    , _lastY(0)
    , _center(0, 0, 1)
    , _tiltDeg(0.0)
    , _isMoving(false)
    , _lastMoveTime(0.0)
    , _movementTimeout(0.2)
    , _maxTiltDeg(75.0)
{}

bool GoogleMapsManipulator::isMoving() const
{
    double now = osg::Timer::instance()->time_s();
    return _isMoving && (now - _lastMoveTime < _movementTimeout);
}

void GoogleMapsManipulator::setMovementTimeout(double seconds)
{
    _movementTimeout = seconds;
}

void GoogleMapsManipulator::setMaxTiltDeg(double degrees)
{
    _maxTiltDeg = std::clamp(degrees, 0.0, 90.0);
}

double GoogleMapsManipulator::getMaxTiltDeg() const
{
    return _maxTiltDeg;
}

void GoogleMapsManipulator::resetFromBounds()
{
    if (!_node.valid())
        return;

    const osg::BoundingSphere bs = _node->getBound();
    _center = bs.center();
    _distance = bs.radius() * 0.5;
}

void GoogleMapsManipulator::setNode(osg::Node* node)
{
    _node = node;
    osgGA::CameraManipulator::setNode(node);
    resetFromBounds();
}

void GoogleMapsManipulator::home(double)
{
    resetFromBounds();
}

void GoogleMapsManipulator::home(const osgGA::GUIEventAdapter&, osgGA::GUIActionAdapter&)
{
    resetFromBounds();
}

osg::Matrixd GoogleMapsManipulator::getInverseMatrix() const
{
    osg::Vec3d up, east, north;
    computeLocalFrame(_center, up, east, north);

    const double tiltRad = osg::DegreesToRadians(_tiltDeg);
    osg::Vec3d offset = (-north * std::sin(tiltRad)) + (up * std::cos(tiltRad));
    offset.normalize();

    osg::Vec3d eye = _center + offset * _distance;

    double earthRadius = getEarthRadius(_node);
    double minEyeDistance = earthRadius + kMinDistance;

    if (eye.length() < minEyeDistance)
    {
        eye.normalize();
        eye *= minEyeDistance;
    }

    return osg::Matrixd::lookAt(eye, _center, north);
}

osg::Matrixd GoogleMapsManipulator::getMatrix() const
{
    return osg::Matrixd::inverse(getInverseMatrix());
}

void GoogleMapsManipulator::setByMatrix(const osg::Matrixd& matrix)
{
    osg::Vec3d eye = matrix.getTrans();
    if (eye.isNaN())
        eye.set(0, 0, 100);

    osg::Vec3d lookVector(-matrix(2, 0), -matrix(2, 1), -matrix(2, 2));
    lookVector.normalize();
    if (lookVector.isNaN())
        lookVector.set(0, 0, -1);

    osg::Vec3d localUp = eye;
    localUp.normalize();
    if (localUp.isNaN())
        localUp.set(0, 0, 1);

    double dot = std::clamp(lookVector * (-localUp), -1.0, 1.0);
    _tiltDeg = clampTilt(osg::RadiansToDegrees(std::acos(dot)), _maxTiltDeg);

    double earthRadius = _node.valid() && _node->getBound().center().length() > kMinRadiusThreshold
        ? _node->getBound().center().length()
        : kDefaultEarthRadius;

    // Ray-sphere intersection: solve at^2 + bt + c = 0
    double b = 2.0 * (eye * lookVector);
    double c = (eye * eye) - (earthRadius * earthRadius);
    double discriminant = b * b - 4.0 * c;

    if (discriminant >= 0)
    {
        double sqrtDisc = std::sqrt(discriminant);
        double t1 = (-b - sqrtDisc) * 0.5;
        double t2 = (-b + sqrtDisc) * 0.5;

        double t = -1.0;
        if (t1 > 0 && t2 > 0)
            t = std::min(t1, t2);
        else if (t1 > 0)
            t = t1;
        else if (t2 > 0)
            t = t2;

        if (t > 0)
        {
            _center = eye + lookVector * t;
            _distance = std::max(t, kMinDistance);
            return;
        }
    }

    resetFromBounds();
}

void GoogleMapsManipulator::setByInverseMatrix(const osg::Matrixd& matrix)
{
    setByMatrix(osg::Matrixd::inverse(matrix));
}

bool GoogleMapsManipulator::handle(const osgGA::GUIEventAdapter& ea,
                                   osgGA::GUIActionAdapter& aa)
{
    auto markMovement = [this]() {
        _isMoving = true;
        _lastMoveTime = osg::Timer::instance()->time_s();
    };

    switch (ea.getEventType())
    {
    case osgGA::GUIEventAdapter::PUSH:
        _lastX = ea.getXnormalized();
        _lastY = ea.getYnormalized();
        markMovement();
        return true;

    case osgGA::GUIEventAdapter::DRAG:
    {
        const int buttonMask = ea.getButtonMask();
        const bool leftButton = buttonMask & osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON;
        const bool rightButton = buttonMask & osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON;

        if (!leftButton && !rightButton)
            return false;

        const float x = ea.getXnormalized();
        const float y = ea.getYnormalized();
        const float dx = x - _lastX;
        const float dy = y - _lastY;

        if (leftButton)
        {
            osg::Vec3d up, east, north;
            computeLocalFrame(_center, up, east, north);
            _center -= (east * dx + north * dy) * _distance;
        }

        if (rightButton)
        {
            _tiltDeg = clampTilt(_tiltDeg - dy * kTiltSensitivity, _maxTiltDeg);
        }

        _lastX = x;
        _lastY = y;
        markMovement();
        aa.requestRedraw();
        return true;
    }

    case osgGA::GUIEventAdapter::SCROLL:
    {
        double factor = (ea.getScrollingMotion() == osgGA::GUIEventAdapter::SCROLL_UP)
            ? kZoomInFactor : kZoomOutFactor;
        _distance = std::max(_distance * factor, kMinDistance);
        markMovement();
        aa.requestRedraw();
        return true;
    }

    case osgGA::GUIEventAdapter::KEYDOWN:
        if (ea.getKey() == osgGA::GUIEventAdapter::KEY_Home)
        {
            resetFromBounds();
            markMovement();
            aa.requestRedraw();
            return true;
        }
        return false;

    default:
        return false;
    }
}
