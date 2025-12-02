#include <osgGA/CameraManipulator>
#include <osgGA/GUIEventAdapter>
#include <osgGA/GUIActionAdapter>
#include <osg/Matrix>
#include <osg/Vec3d>
#include <osg/BoundingSphere>
#include <osg/Node>
#include <algorithm>
#include <cmath>

// A Google-Maps-like camera manipulator for large (ECEF) coordinates.
// Panning follows local tangent (East/North) frame, using a fixed local frame.
class GoogleMapsManipulator : public osgGA::CameraManipulator {
public:
    GoogleMapsManipulator()
        : _center(0.0, 0.0, 0.0), _distance(100.0), _lastX(0.0f), _lastY(0.0f),
          _fixedUp(0.0, 0.0, 1.0), _fixedNorth(0.0, 1.0, 0.0), _fixedEast(1.0, 0.0, 0.0)
    {}

    const char* className() const override { return "GoogleMapsManipulator"; }

private:
    // --- Constants ---
    static constexpr double kMinDistance = 10.0; // Minimum camera-ground distance
    static constexpr double kScrollFactor = 0.8; // Zoom multiplier
    static constexpr double kPanScale = 1; // Pan speed scale per unit distance
    static constexpr double kDenomEpsilon = 1e-3; // Ray/plane denom epsilon
    static constexpr double kTinyEpsilon2 = 1e-12; // Squared length epsilon

    // --- Math helpers ---
    static osg::Vec3d normalizeOr(const osg::Vec3d& v, const osg::Vec3d& fallback)
    {
        if (v.length2() <= kTinyEpsilon2) return fallback;
        osg::Vec3d out = v;
        out.normalize();
        return out;
    }

    // Compute and cache a fixed local tangent basis using the current center.
    void computeFixedFrame()
    {
        // Up approximated as Z+ for local, or radial if sufficiently far.
        osg::Vec3d up = (_center.length2() > 1000.0) ? normalizeOr(_center, osg::Vec3d(0.0, 0.0, 1.0))
                                                     : osg::Vec3d(0.0, 0.0, 1.0);
        const osg::Vec3d globalNorth(0.0, 0.0, 1.0);

        osg::Vec3d east = globalNorth ^ up;
        if (east.length2() <= kTinyEpsilon2)
            east = osg::Vec3d(1.0, 0.0, 0.0) ^ up;
        east.normalize();

        osg::Vec3d north = up ^ east;
        north = normalizeOr(north, globalNorth);

        _fixedUp = up;
        _fixedEast = east;
        _fixedNorth = north;
    }

    // Clamp distance to sane minimums.
    void clampDistance()
    {
        if (_distance < kMinDistance) _distance = kMinDistance;
    }

    // Reset center/distance to scene bounds when available, and recompute frame.
    void resetFromBounds()
    {
        if (!_node.valid()) return;
        osg::BoundingSphere bs = _node->getBound();
        if (!bs.valid()) return;
        _center = bs.center();
        _distance = std::max(100.0, bs.radius() * 2.5);
        computeFixedFrame();
    }

public:
    // --- CameraManipulator overrides ---

    void setNode(osg::Node* node) override
    {
        _node = node;
        osgGA::CameraManipulator::setNode(node);
        if (_node.valid())
        {
            resetFromBounds();
        }
    }

    osg::Node* getNode() override { return _node.get(); }
    const osg::Node* getNode() const override { return _node.get(); }

    void setByMatrix(const osg::Matrixd& matrix) override
    {
        const osg::Vec3d eye = matrix.getTrans();
        osg::Vec3d lookDir(-matrix(2, 0), -matrix(2, 1), -matrix(2, 2));
        lookDir = normalizeOr(lookDir, osg::Vec3d(0.0, 0.0, -1.0));

        // Ray cast against fixed tangent plane at current center to find ground point.
        const osg::Vec3d planeNormal = _fixedUp;
        const double denom = lookDir * planeNormal;

        if (std::abs(denom) > kDenomEpsilon)
        {
            const double t = ((_center - eye) * planeNormal) / denom;
            if (t > 0.0)
            {
                _center = eye + lookDir * t; // intersection on ground
                _distance = (eye - _center).length(); // camera altitude
                clampDistance();
                return;
            }
        }

        // Fallback: keep tangent direction and place center below eye.
        _center = eye - (planeNormal * 1000.0);
        _distance = 1000.0;
        clampDistance();
    }

    void setByInverseMatrix(const osg::Matrixd& inv) override
    {
        setByMatrix(osg::Matrixd::inverse(inv));
    }

    osg::Matrixd getMatrix() const override
    {
        return osg::Matrixd::inverse(getInverseMatrix());
    }

    osg::Matrixd getInverseMatrix() const override
    {
        // Eye is located along fixed Up at the current distance.
        const osg::Vec3d eye = _center + (_fixedUp * _distance);
        return osg::Matrixd::lookAt(eye, _center, _fixedNorth);
    }

    void home(double /*currentTime*/) override
    {
        resetFromBounds();
        clampDistance();
    }

    void home(const osgGA::GUIEventAdapter&, osgGA::GUIActionAdapter&) override
    {
        home(0.0);
    }

    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa) override
    {
        switch (ea.getEventType())
        {
            case osgGA::GUIEventAdapter::PUSH:
                _lastX = ea.getXnormalized();
                _lastY = ea.getYnormalized();
                return true;

            case osgGA::GUIEventAdapter::DRAG: {
                if (!(ea.getButtonMask() & osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON))
                    return false;

                const float x = ea.getXnormalized();
                const float y = ea.getYnormalized();
                const float dx = x - _lastX;
                const float dy = y - _lastY;

                const double scale = _distance * kPanScale;
                _center -= (_fixedEast * static_cast<double>(dx) * scale);
                _center -= (_fixedNorth * static_cast<double>(dy) * scale);

                _lastX = x;
                _lastY = y;
                aa.requestRedraw();
                return true;
            }

            case osgGA::GUIEventAdapter::KEYDOWN:
                if (ea.getKey() == osgGA::GUIEventAdapter::KEY_Home)
                {
                    home(ea, aa);
                    aa.requestRedraw();
                    return true;
                }
                return false;

            case osgGA::GUIEventAdapter::SCROLL: {
                switch (ea.getScrollingMotion())
                {
                    case osgGA::GUIEventAdapter::SCROLL_UP:
                        _distance *= kScrollFactor;
                        break;
                    case osgGA::GUIEventAdapter::SCROLL_DOWN:
                        _distance /= kScrollFactor;
                        break;
                    default: break;
                }
                clampDistance();
                aa.requestRedraw();
                return true;
            }

            default: return false;
        }
    }

private:
    osg::observer_ptr<osg::Node> _node;
    osg::Vec3d _center;
    double _distance;
    float _lastX, _lastY;

    // Cached local frame
    osg::Vec3d _fixedUp;
    osg::Vec3d _fixedNorth;
    osg::Vec3d _fixedEast;
};