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

// Clamps the tilt angle to valid range [0, maxTilt]
double clampTilt(double tilt, double maxTilt)
{
    return std::clamp(tilt, 0.0, maxTilt);
}

/**
 * Computes a local coordinate frame at a given point on a sphere.
 *
 * This function establishes a right-handed coordinate system at the specified
 * center position on a spherical surface. The coordinate frame consists of:
 * - up: Radial direction pointing away from sphere center (surface normal)
 * - east: Tangent vector pointing eastward
 * - north: Tangent vector pointing northward
 *
 * The coordinate system is computed using cross products to ensure
 * orthogonality. Special handling is included for polar regions where the
 * standard calculation would fail.
 *
 * @param center The position on the sphere's surface
 * @param[out] up Output vector pointing radially outward (normalized)
 * @param[out] east Output vector pointing east (normalized)
 * @param[out] north Output vector pointing north (normalized)
 */
void computeLocalFrame(const osg::Vec3d& center, osg::Vec3d& up,
                       osg::Vec3d& east, osg::Vec3d& north)
{
    // Up vector is the normalized position (radial direction from sphere
    // center)
    up = center;
    up.normalize();

    // East is computed as cross product of Z-axis with up vector
    // This gives a vector tangent to the sphere pointing eastward
    east = osg::Vec3d(0, 0, 1) ^ up;

    // Handle special case at poles where cross product would be zero
    if (east.length2() == 0.0) east.set(1.0, 0.0, 0.0);
    east.normalize();

    // North completes the right-handed coordinate system
    north = up ^ east;
    north.normalize();
}
}

GoogleMapsManipulator::GoogleMapsManipulator()
    : _distance(100.0), _lastX(0), _lastY(0), _center(0, 0, 1), _tiltDeg(0.0),
      _isMoving(false), _lastMoveTime(0.0), _movementTimeout(0.2),
      _maxTiltDeg(75.0)
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

double GoogleMapsManipulator::getMaxTiltDeg() const { return _maxTiltDeg; }

void GoogleMapsManipulator::resetFromBounds()
{
    if (!_node.valid()) return;

    // Reset camera to view the entire scene based on bounding sphere
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

void GoogleMapsManipulator::home(double) { resetFromBounds(); }

void GoogleMapsManipulator::home(const osgGA::GUIEventAdapter&,
                                 osgGA::GUIActionAdapter&)
{
    resetFromBounds();
}

osg::Matrixd GoogleMapsManipulator::getInverseMatrix() const
{
    // Compute local coordinate frame at the current center point
    osg::Vec3d up, east, north;
    computeLocalFrame(_center, up, east, north);

    // Calculate eye position based on tilt angle
    // Blend between 'up' (0° tilt) and 'north' (90° tilt) directions
    const double tiltRad = osg::DegreesToRadians(_tiltDeg);
    osg::Vec3d offset = (-north * std::sin(tiltRad)) + (up * std::cos(tiltRad));
    offset.normalize();

    osg::Vec3d eye = _center + offset * _distance;

    // Ensure camera doesn't go below the surface
    // The "Earth Radius" is simply the distance from (0,0,0) to the city.
    // If the node is invalid, default to standard Earth radius.
    double earthRadius = (_node.valid()) ? _node->getBound().center().length()
                                         : kDefaultEarthRadius;
    double minEyeDistance = earthRadius + kMinDistance;

    if (eye.length() < minEyeDistance)
    {
        eye.normalize();
        eye *= minEyeDistance;
    }

    // Return view matrix (camera looks from eye to center, with north as up)
    return osg::Matrixd::lookAt(eye, _center, north);
}

osg::Matrixd GoogleMapsManipulator::getMatrix() const
{
    return osg::Matrixd::inverse(getInverseMatrix());
}

/**
 * Sets the camera manipulator state from a transformation matrix.
 *
 * This function reverses the camera matrix to extract the camera position,
 * view direction, and tilt angle. It performs ray-sphere intersection to
 * determine where the camera is looking on the spherical Earth surface,
 * setting the center point and distance accordingly.
 *
 * The algorithm:
 * 1. Extracts eye position and look direction from the matrix
 * 2. Computes tilt angle from the angle between look vector and local up
 * 3. Performs ray-sphere intersection to find the point on Earth being viewed
 * 4. Sets center and distance based on intersection, or resets if no valid
 * intersection
 *
 * @param matrix The transformation matrix to set the manipulator from
 */
void GoogleMapsManipulator::setByMatrix(const osg::Matrixd& matrix)
{
    // Extract eye position from translation component
    osg::Vec3d eye = matrix.getTrans();
    if (eye.isNaN()) eye.set(0, 0, 100);

    // Extract look direction from the negative Z column of rotation matrix
    osg::Vec3d lookVector(-matrix(2, 0), -matrix(2, 1), -matrix(2, 2));
    lookVector.normalize();
    if (lookVector.isNaN()) lookVector.set(0, 0, -1);

    // Compute local up vector (radial direction from Earth center)
    osg::Vec3d localUp = eye;
    localUp.normalize();
    if (localUp.isNaN()) localUp.set(0, 0, 1);

    // Calculate tilt angle from dot product between look vector and down
    // direction
    double dot = std::clamp(lookVector * (-localUp), -1.0, 1.0);
    _tiltDeg = clampTilt(osg::RadiansToDegrees(std::acos(dot)), _maxTiltDeg);

    // The "Earth Radius" is simply the distance from (0,0,0) to the city.
    // If the node is invalid, default to standard Earth radius.
    double earthRadius = (_node.valid()) ? _node->getBound().center().length()
                                         : kDefaultEarthRadius;

    // Perform ray-sphere intersection to find where camera is looking on Earth
    // Solve quadratic equation: ||eye + t*lookVector||^2 = earthRadius^2
    // Expanding: (eye + t*lookVector)·(eye + t*lookVector) = earthRadius^2
    // Results in: t^2 + 2(eye·lookVector)t + (eye·eye - earthRadius^2) = 0
    // Using standard form at^2 + bt + c = 0 where a=1
    double b = 2.0 * (eye * lookVector);
    double c = (eye * eye) - (earthRadius * earthRadius);
    double discriminant = b * b - 4.0 * c;

    if (discriminant >= 0)
    {
        // Two intersection points (entry and exit of sphere)
        double sqrtDisc = std::sqrt(discriminant);
        double t1 = (-b - sqrtDisc) * 0.5; // Near intersection
        double t2 = (-b + sqrtDisc) * 0.5; // Far intersection

        // Choose the nearest positive intersection point
        double t = -1.0;
        if (t1 > 0 && t2 > 0)
            t = std::min(t1, t2);
        else if (t1 > 0)
            t = t1;
        else if (t2 > 0)
            t = t2;

        // If valid intersection found, set center and distance
        if (t > 0)
        {
            _center = eye + lookVector * t;
            _distance = std::max(t, kMinDistance);
            return;
        }
    }

    // No valid intersection found - reset to default view
    resetFromBounds();
}

void GoogleMapsManipulator::setByInverseMatrix(const osg::Matrixd& matrix)
{
    setByMatrix(osg::Matrixd::inverse(matrix));
}

/**
 * Handles user input events for camera manipulation.
 *
 * This function processes mouse and keyboard events to control the camera:
 * - Left mouse drag: Pan the camera across the surface
 * - Right mouse drag: Adjust the tilt angle (vertical rotation)
 * - Mouse scroll: Zoom in/out by adjusting distance
 * - Home key: Reset camera to default view
 *
 * The function also tracks movement state for animation and rendering purposes.
 *
 * @param ea The GUI event adapter containing input event data
 * @param aa The GUI action adapter for requesting redraws
 * @return true if the event was handled, false otherwise
 */
bool GoogleMapsManipulator::handle(const osgGA::GUIEventAdapter& ea,
                                   osgGA::GUIActionAdapter& aa)
{
    // Lambda to mark that user is actively moving the camera
    auto markMovement = [this]() {
        _isMoving = true;
        _lastMoveTime = osg::Timer::instance()->time_s();
    };

    switch (ea.getEventType())
    {
        case osgGA::GUIEventAdapter::PUSH:
            // Record initial mouse position when button is pressed
            _lastX = ea.getXnormalized();
            _lastY = ea.getYnormalized();
            markMovement();
            return true;

        case osgGA::GUIEventAdapter::DRAG: {
            // Check which mouse buttons are being held
            const int buttonMask = ea.getButtonMask();
            const bool leftButton =
                buttonMask & osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON;
            const bool rightButton =
                buttonMask & osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON;

            if (!leftButton && !rightButton) return false;

            // Calculate mouse movement delta
            const float x = ea.getXnormalized();
            const float y = ea.getYnormalized();
            const float dx = x - _lastX;
            const float dy = y - _lastY;

            if (leftButton)
            {
                // Pan: Move the center point based on mouse drag
                // Movement is in the local tangent plane (east-north coordinate
                // system)
                osg::Vec3d up, east, north;
                computeLocalFrame(_center, up, east, north);
                _center -= (east * dx + north * dy) * _distance;
            }

            if (rightButton)
            {
                // Tilt: Adjust vertical viewing angle
                _tiltDeg =
                    clampTilt(_tiltDeg - dy * kTiltSensitivity, _maxTiltDeg);
            }

            // Update last mouse position for next frame
            _lastX = x;
            _lastY = y;
            markMovement();
            aa.requestRedraw();
            return true;
        }

        case osgGA::GUIEventAdapter::SCROLL: {
            // Zoom: Adjust camera distance based on scroll direction
            double factor =
                (ea.getScrollingMotion() == osgGA::GUIEventAdapter::SCROLL_UP)
                ? kZoomInFactor
                : kZoomOutFactor;
            _distance = std::max(_distance * factor, kMinDistance);
            markMovement();
            aa.requestRedraw();
            return true;
        }

        case osgGA::GUIEventAdapter::KEYDOWN:
            // Home key: Reset camera to default view of entire scene
            if (ea.getKey() == osgGA::GUIEventAdapter::KEY_Home)
            {
                resetFromBounds();
                markMovement();
                aa.requestRedraw();
                return true;
            }
            return false;

        default: return false;
    }
}
