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
// Panning follows local tangent (East/North) frame, camera up is local North.
class GoogleMapsManipulator : public osgGA::CameraManipulator {
public:
    GoogleMapsManipulator()
        : _center(0.0, 0.0, 0.0), _distance(100.0), _lastX(0.0f), _lastY(0.0f)
    {}

    const char* className() const override { return "GoogleMapsManipulator"; }

    // --- Constants ---
private:
    static constexpr double kMinDistance =
        10.0; // Minimum camera-ground distance
    static constexpr double kScrollInFactor = 0.8; // Zoom in multiplier
    static constexpr double kScrollOutFactor = 1.25; // Zoom out multiplier
    static constexpr double kPanScale =
        1.5; // Pan speed scale per unit distance
    static constexpr double kDenomEpsilon = 1e-3; // Ray/plane denom epsilon
    static constexpr double kTinyEpsilon2 = 1e-12; // Squared length epsilon

    // --- Math helpers ---
    // Normalize v; if too small, return fallback.
    static osg::Vec3d normalizeOr(const osg::Vec3d& v,
                                  const osg::Vec3d& fallback)
    {
        if (v.length2() <= kTinyEpsilon2) return fallback;
        osg::Vec3d out = v;
        out.normalize();
        return out;
    }

    // Calculate the true "Up" vector for the given point on Earth.
    // For ECEF coordinates, up is the vector from Earth's center to the point.
    osg::Vec3d getSurfaceNormal(const osg::Vec3d& point) const
    {
        // If point is far from origin, assume ECEF and use radial up. Otherwise
        // fallback to Z+.
        if (point.length2() > 1000.0)
            return normalizeOr(point, osg::Vec3d(0.0, 0.0, 1.0));
        return osg::Vec3d(0.0, 0.0, 1.0);
    }

    // Compute local tangent basis at point: Right (East) and North vectors.
    // Handles degeneracy near the poles by picking an alternate reference axis.
    void getTangentBasis(const osg::Vec3d& point, osg::Vec3d& outRight,
                         osg::Vec3d& outNorth) const
    {
        const osg::Vec3d up = getSurfaceNormal(point);
        const osg::Vec3d globalNorth(0.0, 0.0, 1.0); // Earth axis

        // Try to compute East from globalNorth x up. If degenerate (near pole),
        // use X-axis instead.
        osg::Vec3d east = globalNorth ^ up;
        if (east.length2() <= kTinyEpsilon2)
            east = osg::Vec3d(1.0, 0.0, 0.0) ^ up;
        east.normalize();

        osg::Vec3d north = up ^ east;
        north = normalizeOr(north, globalNorth);

        outRight = east;
        outNorth = north;
    }

    // Clamp distance to sane minimums.
    void clampDistance()
    {
        if (_distance < kMinDistance) _distance = kMinDistance;
    }

    // Reset center/distance to scene bounds when available.
    void resetFromBounds()
    {
        if (!_node.valid()) return;
        osg::BoundingSphere bs = _node->getBound();
        if (!bs.valid()) return;
        _center = bs.center();
        _distance = std::max(100.0, bs.radius() * 2.5);
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
        // Eye position and viewing direction from matrix.
        const osg::Vec3d eye = matrix.getTrans();
        osg::Vec3d lookDir(-matrix(2, 0), -matrix(2, 1), -matrix(2, 2));
        lookDir = normalizeOr(lookDir, osg::Vec3d(0.0, 0.0, -1.0));

        // Ray cast against tangent plane at current center to find ground
        // point.
        const osg::Vec3d planeNormal = getSurfaceNormal(_center);
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
        // Eye is located along local Up (surface normal) at the current
        // distance.
        const osg::Vec3d up = getSurfaceNormal(_center);
        const osg::Vec3d eye = _center + (up * _distance);

        // Ensure on-screen up is local North.
        osg::Vec3d right, north;
        getTangentBasis(_center, right, north);
        return osg::Matrixd::lookAt(eye, _center, north);
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

    bool handle(const osgGA::GUIEventAdapter& ea,
                osgGA::GUIActionAdapter& aa) override
    {
        switch (ea.getEventType())
        {
            case osgGA::GUIEventAdapter::PUSH:
                _lastX = ea.getXnormalized();
                _lastY = ea.getYnormalized();
                return true;

            case osgGA::GUIEventAdapter::DRAG: {
                if (!(ea.getButtonMask()
                      & osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON))
                    return false;

                const float x = ea.getXnormalized();
                const float y = ea.getYnormalized();
                const float dx = x - _lastX;
                const float dy = y - _lastY;

                osg::Vec3d right, north;
                getTangentBasis(_center, right, north);

                const double scale = _distance * kPanScale;
                _center -= (right * static_cast<double>(dx) * scale);
                _center -= (north * static_cast<double>(dy) * scale);

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
                        _distance *= kScrollInFactor;
                        break;
                    case osgGA::GUIEventAdapter::SCROLL_DOWN:
                        _distance *= kScrollOutFactor;
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
};