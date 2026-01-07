#ifndef CAMERA_MANIP_H
#define CAMERA_MANIP_H

#include <osgGA/CameraManipulator>
#include <osgGA/GUIEventAdapter>
#include <osgGA/GUIActionAdapter>
#include <osg/Matrix>
#include <osg/Vec3d>
#include <osg/BoundingSphere>
#include <osg/Node>

class GoogleMapsManipulator : public osgGA::CameraManipulator {
public:
    GoogleMapsManipulator();

    bool isMoving() const;

    void setMovementTimeout(double seconds);

    void setMaxTiltDeg(double degrees);

    double getMaxTiltDeg() const;

    void resetFromBounds();

    void setNode(osg::Node* node) override;

    void home(double) override;

    void home(const osgGA::GUIEventAdapter&, osgGA::GUIActionAdapter&) override;

    osg::Matrixd getInverseMatrix() const override;

    osg::Matrixd getMatrix() const override;

    void setByMatrix(const osg::Matrixd& matrix) override;

    void setByInverseMatrix(const osg::Matrixd& matrix) override;

    bool handle(const osgGA::GUIEventAdapter& ea,
                osgGA::GUIActionAdapter& aa) override;

private:
    osg::observer_ptr<osg::Node> _node;
    osg::Vec3d _center;
    double _distance;
    float _lastX, _lastY;
    double _tiltDeg;

    bool _isMoving;
    double _lastMoveTime;
    double _movementTimeout;
    double _maxTiltDeg;
};

#endif // CAMERA_MANIP_H
