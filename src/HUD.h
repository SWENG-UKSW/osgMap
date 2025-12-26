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
#include <unordered_map>

static osg::ref_ptr<osgText::Text> g_hudText;
osg::Camera* createHUD(const std::string& logoFile, float scale = 0.3f,
                       int winWidth = 1920,
                       int winHeight = 1080);
void hudSetText(const std::string& text);
std::string getLandInfoAtIntersection(osg::Node* sceneRoot,
                                      const osg::Vec3d& hitPoint);
const std::unordered_map<std::string, std::string> fclassPL = {
    { "residential", "osiedle" },
    { "living_street", "ulica mieszkalna" },
    { "primary", u8"droga główna" },
    { "primary_link", u8"droga główna łącząca" },
    { "secondary", u8"droga drugorzędna" },
    { "secondary_link", u8"droga drugorzędna łącząca" },
    { "tertiary", "droga lokalna" }, 
    { "tertiary_link", "droga lokalna łącząca" }, 
    { "motorway", u8"autostrada" },
    { "motorway_link", u8"autostrada - połączenie" },
    { "footway", u8"chodnik" },
    { "nature_reserve", "rezerwat" },
    { "commercial", u8"przedsiębiorstwo" },
    { "forest", u8"las"},
    { "reservoir", u8"zbiornik" },
    { "wetland", u8"mokradło" },
    { "riverbank", u8"brzeg rzeki" },
    { "industrial", u8"zakład przemysłowy" },
    { "path", u8"ścieżka" },
    { "unclassified", u8"niezaklasyfikowane" },
    { "trunk", u8"zrąb" },
    { "scrub", u8"zarośla" },
    { "service", u8"połączenie" },
    { "water", u8"wody" },
    { "retail", u8"sprzedawca" },
    { "service", u8"połączenie" },
    { "track_grade3", u8"droga trzeciorzędna" },
    { "track_grade4", u8"droga czwartorzędna" },
    { "track_grade5", u8"droga piątorzędna" },
    { "track", u8"trasa" },
    { "cemetery", "cmentarz" },
    { "allotments", u8"działki" },
    { "grass", u8"polana" },
    { "quarry", u8"kamieniołom" },
    { "recreation_ground", u8"pole rekreacyjne" },
    { "meadow", u8"łąka" },
    { "grass", u8"łąka" },
    { "grass", u8"łąka" },
};
#endif
