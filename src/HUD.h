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
extern osg::ref_ptr<osg::Uniform> g_hudAlpha;
extern float g_targetAlpha;
extern float g_currentAlpha;

osg::Camera* createHUD(const std::string& logoFile, float scale = 0.3f,
                       int winWidth = 1920, int winHeight = 1080);
void hudSetText(const std::string& text);
std::string getLandInfoAtIntersection(osg::Node* sceneRoot,
                                      const osg::Vec3d& hitPoint);
const std::unordered_map<std::string, std::string> fclassPL = {
    { "residential", "osiedle" },
    { "living_street", "ulica mieszkalna" },
    { "primary", "droga główna" },
    { "primary_link", "droga główna łącząca" },
    { "secondary", "droga drugorzędna" },
    { "secondary_link", "droga drugorzędna łącząca" },
    { "tertiary", "droga lokalna" },
    { "tertiary_link", "droga lokalna łącząca" },
    { "motorway", "autostrada" },
    { "motorway_link", "autostrada - połączenie" },
    { "footway", "chodnik" },
    { "nature_reserve", "rezerwat" },
    { "commercial", u8"przedsiębiorstwo" },
    { "forest", "las" },
    { "reservoir", "zbiornik" },
    { "wetland", "mokradło" },
    { "riverbank", "brzeg rzeki" },
    { "industrial", "zakład przemysłowy" },
    { "path", "ścieżka" },
    { "unclassified", "niezaklasyfikowane" },
    { "trunk", "zrąb" },
    { "scrub", "zarośla" },
    { "service", "połączenie" },
    { "water", "wody" },
    { "retail", "sprzedawca" },
    { "service", "połączenie" },
    { "track_grade1", "droga" },
    { "track_grade2", "droga drugorzędna" },
    { "track_grade3", "droga trzeciorzędna" },
    { "track_grade4", "droga czwartorzędna" },
    { "track_grade5", "droga piątorzędna" },
    { "track", "trasa" },
    { "cemetery", "cmentarz" },
    { "allotments", "działki" },
    { "grass", "polana" },
    { "quarry", "kamieniołom" },
    { "recreation_ground", u8"pole rekreacyjne" },
    { "meadow", "łąka" },
    { "grass", "łąka" },
    { "grass", "łąka" },
    { "cycleway", "ścieżka rowerowa" }
};
// Create a resize handler class
class HUDResizeHandler : public osgGA::GUIEventHandler {
public:
    HUDResizeHandler(osg::Camera* hudCamera, osg::Geode* geode,
                     const std::string& logoFile, float scale)
        : _hudCamera(hudCamera), _geode(geode), _logoFile(logoFile),
          _scale(scale)
    {}

    virtual bool handle(const osgGA::GUIEventAdapter& ea,
                        osgGA::GUIActionAdapter& aa)
    {
        if (ea.getEventType() == osgGA::GUIEventAdapter::RESIZE)
        {
            int width = ea.getWindowWidth();
            int height = ea.getWindowHeight();

            // Update HUD camera projection
            _hudCamera->setProjectionMatrix(
                osg::Matrix::ortho2D(0, width, 0, height));

            // Update logo and text positions
            updateHUDElements(width, height);

            return false; // Allow other handlers to process this event too
        }
        return false;
    }

private:
    void updateHUDElements(int winWidth, int winHeight)
    {
        // Find the logo quad and text in the geode
        for (unsigned int i = 0; i < _geode->getNumDrawables(); i++)
        {
            osg::Drawable* drawable = _geode->getDrawable(i);

            // Update logo quad
            if (osg::Geometry* quad = dynamic_cast<osg::Geometry*>(drawable))
            {
                if (osg::Vec3Array* verts =
                        dynamic_cast<osg::Vec3Array*>(quad->getVertexArray()))
                {
                    // Check if this is the logo quad (has 4 vertices and
                    // texture)
                    if (verts->size() == 4 && quad->getStateSet()
                        && quad->getStateSet()->getTextureAttribute(
                            0, osg::StateAttribute::TEXTURE))
                    {
                        // Get texture to calculate size
                        osg::Texture2D* tex = dynamic_cast<osg::Texture2D*>(
                            quad->getStateSet()->getTextureAttribute(
                                0, osg::StateAttribute::TEXTURE));

                        if (tex && tex->getImage())
                        {
                            float w = tex->getImage()->s() * _scale;
                            float h = tex->getImage()->t() * _scale;
                            float x1 = winWidth - w - 20;
                            float y1 = winHeight - h - 20;
                            float x2 = x1 + w;
                            float y2 = y1 + h;

                            // Update vertex positions
                            (*verts)[0] = osg::Vec3(x1, y1, 0);
                            (*verts)[1] = osg::Vec3(x2, y1, 0);
                            (*verts)[2] = osg::Vec3(x2, y2, 0);
                            (*verts)[3] = osg::Vec3(x1, y2, 0);

                            verts->dirty();
                            quad->dirtyBound();
                        }
                    }
                }
            }

            // Update text position
            if (osgText::Text* text = dynamic_cast<osgText::Text*>(drawable))
            {
                text->setPosition(osg::Vec3(20, winHeight - 40, 0));
            }
        }
    }

    osg::Camera* _hudCamera;
    osg::Geode* _geode;
    std::string _logoFile;
    float _scale;
};
#endif

