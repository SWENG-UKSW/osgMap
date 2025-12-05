#ifndef HUD_H
#define HUD_H

#include <osg/Camera>
#include <osg/Geode>
#include <osg/Texture2D>
#include <osgDB/ReadFile>
#include <osg/Geometry>
#include <osgText/Text>
#include <osgText/Font>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <sstream>
#include <osgSim/ShapeAttribute>
#include <codecvt>
#include <locale>

static osg::ref_ptr<osgText::Text> g_hudText;
osg::Camera* createHUD(const std::string& logoFile, float scale = 0.3f,
                       int winWidth = 1920,
                       int winHeight = 1080);
void hudSetText(const std::string& text);
bool castCameraRayIntersection(osgViewer::Viewer* viewer, osg::Node* scene,
                               osg::Vec3d& out_point, osg::Vec3d& out_normal);
std::string getLandInfoAtIntersection(osg::Node* sceneRoot,
                                      const osg::Vec3d& hitPoint);
std::string convertToUTF8(const std::string& input);

#endif
