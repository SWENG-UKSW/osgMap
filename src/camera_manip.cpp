#include <osgGA/CameraManipulator>
#include <osgGA/GUIEventAdapter>
#include <osgGA/GUIActionAdapter>
#include <osg/Matrix>
#include <osg/Vec3d>
#include <osg/BoundingSphere>
#include <osg/Node>
#include <algorithm>

class GoogleMapsManipulator : public osgGA::CameraManipulator {
public:
    GoogleMapsManipulator()
        : _distance(100.0), _lastX(0), _lastY(0), _center(0, 0, 1),
          _isMoving(false), _lastMoveTime(0.0), _movementTimeout(0.2)
    {}

    bool isMoving() const
    {
        double now = osg::Timer::instance()->time_s();
        return _isMoving && (now - _lastMoveTime < _movementTimeout);
    }

    void setMovementTimeout(double seconds) { _movementTimeout = seconds; }

    void resetFromBounds()
    {
        if (_node.valid())
        {
            const osg::BoundingSphere bs = _node->getBound();
            _center = bs.center();
            _distance = bs.radius() * 0.5;
        }
    }

    void setNode(osg::Node* node) override
    {
        _node = node;
        osgGA::CameraManipulator::setNode(node);
        resetFromBounds();
    }
    void home(double) override { resetFromBounds(); }
    void home(const osgGA::GUIEventAdapter&, osgGA::GUIActionAdapter&) override
    {
        resetFromBounds();
    }

    // Build the view matrix each frame: eye along the center direction at a given distance, global Z as up.
    osg::Matrixd getInverseMatrix() const override
    {
        // Local tangent frame at `_center`: up is radial from origin, east is Z x up, north is up x east.
        osg::Vec3d up = _center; // radial up
        up.normalize();
        osg::Vec3d east = osg::Vec3d(0, 0, 1) ^ up;
        if (east.length2() == 0.0) {
            // Avoid degeneracy at poles: pick arbitrary east
            east.set(1.0, 0.0, 0.0);
        }
        east.normalize();
        osg::Vec3d north = up ^ east; // geographic north
        north.normalize();

        // Eye lies in the plane spanned by local north and up, controlled by tilt angle [45,90] deg.
        const double tiltRad = osg::DegreesToRadians(_tiltDeg);
        osg::Vec3d offset = (-north * std::sin(tiltRad)) + (up * std::cos(tiltRad));
        offset.normalize();
        osg::Vec3d eye = _center + offset * _distance;
        // Keep screen up aligned to geographic north (no yaw/rotation).
        return osg::Matrixd::lookAt(eye, _center, north);
    }

    // Forward matrix required by the base class.
    osg::Matrixd getMatrix() const override
    {
        return osg::Matrixd::inverse(getInverseMatrix());
    }

    // Initialize camera manipulator state from an arbitrary view matrix.
    // Extracts camera position and orientation, then computes the manipulator's
    // internal parameters (_center, _distance, _tiltDeg) by ray-sphere intersection
    // with the scene's bounding sphere.
    void setByMatrix(const osg::Matrixd& matrix) override
    {
        // Extract camera position from matrix translation component
        osg::Vec3d eye = matrix.getTrans();

        // Validate eye position and use default if invalid
        if (eye.isNaN())
        {
            eye.set(0, 0, 100);
        }

        // Extract look direction from the negative Z-axis of the view matrix
        // (third row of rotation component, negated)
        osg::Vec3d lookVector(-matrix(2, 0), -matrix(2, 1), -matrix(2, 2));
        lookVector.normalize();
        if (lookVector.isNaN())
        {
            lookVector.set(0, 0, -1);
        }

        // Compute local "up" direction as the radial direction from world origin
        // This assumes a spherical earth model where "up" points away from center
        osg::Vec3d localUp = eye;
        localUp.normalize();
        
        if (localUp.isNaN())
        {
            localUp.set(0, 0, 1);
        }
        
        // Local "down" points toward the earth center
        osg::Vec3d localDown = -localUp;

        // Calculate tilt angle from the angle between look direction and local down
        // dot product gives cos(angle) between vectors
        double dot = lookVector * localDown;
        if (dot > 1.0) dot = 1.0;   // Clamp to valid range for acos
        if (dot < -1.0) dot = -1.0;
        _tiltDeg = osg::RadiansToDegrees(std::acos(dot));

        // Enforce tilt constraints: 0° (top-down) to 45° (maximum oblique)
        if (_tiltDeg < 0.0) _tiltDeg = 0.0;
        if (_tiltDeg > 45.0) _tiltDeg = 45.0;

        // Determine earth radius from scene bounds (default: Earth's mean radius in meters)
        double earthRadius = 6371000.0;
        if (_node.valid())
        {
            double r = _node->getBound().center().length();
            // Only use scene radius if it represents a planetary-scale object
            if (r > 1000.0)
            {
                earthRadius = r;
            }
        }

        // Ray-sphere intersection to find where the look vector hits the earth surface
        // Solve: |eye + t*lookVector|^2 = earthRadius^2
        // Expanding: a*t^2 + b*t + c = 0
        double a = 1.0;  // lookVector is normalized, so (lookVector · lookVector) = 1
        double b = 2.0 * (eye * lookVector);
        double c = (eye * eye) - (earthRadius * earthRadius);

        double discriminant = b * b - 4 * a * c;

        // Parameter t along ray where intersection occurs
        double t = -1.0;

        if (discriminant >= 0)
        {
            // Two intersection points exist (entry and exit)
            double t1 = (-b - std::sqrt(discriminant)) / (2.0 * a);
            double t2 = (-b + std::sqrt(discriminant)) / (2.0 * a);

            // Choose nearest positive intersection (camera looking toward sphere)
            if (t1 > 0 && t2 > 0)
                t = std::min(t1, t2);
            else if (t1 > 0)
                t = t1;
            else if (t2 > 0)
                t = t2;
        }

        double minDistance = 10.0;
        
        // Update manipulator state if valid intersection found
        if (t > 0)
        {
            // Intersection point becomes the new focus center
            osg::Vec3d groundPoint = eye + lookVector * t;
            _center = groundPoint;
            // Distance from camera to center (enforcing minimum)
            _distance = std::max(t, minDistance);
        }
        else
        {
            // No valid intersection: reset to default scene bounds
            resetFromBounds();
        }
    }

    void setByInverseMatrix(const osg::Matrixd& matrix) override
    {
        setByMatrix(osg::Matrixd::inverse(matrix));
    }

    bool handle(const osgGA::GUIEventAdapter& ea,
                osgGA::GUIActionAdapter& aa) override
    {
        // Record movement helpers
        // Keeps track of activity for inertia or idle timers
        auto markMovement = [&]() {
            _isMoving = true;
            _lastMoveTime = osg::Timer::instance()->time_s();
        };

        if (ea.getEventType() == osgGA::GUIEventAdapter::PUSH)
        {
            _lastX = ea.getXnormalized();
            _lastY = ea.getYnormalized();
            markMovement();
            return true;
        }

        if (ea.getEventType() == osgGA::GUIEventAdapter::DRAG
            && (ea.getButtonMask()
                & (osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON
                   | osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON)))
        {
            float x = ea.getXnormalized();
            float y = ea.getYnormalized();
            const float dx = x - _lastX;
            const float dy = y - _lastY;

            // Panning
            if (ea.getButtonMask() & osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
            {
                // Pan: move center along local east/north, maintaining north-up
                // orientation.
                osg::Vec3d up = _center;
                up.normalize();

                osg::Vec3d east = osg::Vec3d(0, 0, 1) ^ up;
                // Handle case where up vector is
                // effectively vertical
                if (east.length2() == 0.0) east.set(1.0, 0.0, 0.0);
                east.normalize();

                osg::Vec3d north = up ^ east;
                north.normalize(); // Explicit
                                   // normalization

                _center -= (east * dx * _distance) + (north * dy * _distance);
            }

            // Tilting
            if (ea.getButtonMask() & osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON)
            {
                // Tilt without rotation: adjust tilt angle based on vertical
                // drag. Positive dy (drag up) decreases tilt towards 0 (more
                // top-down).
                const double sensitivity = 100.0; // scale dy to degrees
                _tiltDeg -= dy * sensitivity;

                // Clamp limits
                if (_tiltDeg < 0.0) _tiltDeg = 0.0; // top-down limit
                if (_tiltDeg > 45.0) _tiltDeg = 45.0; // flat limit
            }

            _lastX = x;
            _lastY = y;

            markMovement(); 
            aa.requestRedraw();
            return true;
        }

        if (ea.getEventType() == osgGA::GUIEventAdapter::SCROLL)
        {
            _distance *=
                (ea.getScrollingMotion() == osgGA::GUIEventAdapter::SCROLL_UP
                     ? 0.8
                     : 1.25);
            _distance = std::max(_distance, 10.0);

            markMovement(); 
            aa.requestRedraw();
            return true;
        }

        // Home key resets center/distance to node bounds.
        if (ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN
            && ea.getKey() == osgGA::GUIEventAdapter::KEY_Home)
        {
            resetFromBounds();
            markMovement(); 
            aa.requestRedraw();
            return true;
        }

        return false;
    }

private:
    osg::observer_ptr<osg::Node> _node;
    osg::Vec3d _center;
    double _distance;
    float _lastX, _lastY;
    double _tiltDeg;

    bool _isMoving;
    double _lastMoveTime;
    double _movementTimeout;
};






