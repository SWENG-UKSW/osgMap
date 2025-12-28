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

extern osg::ref_ptr<osgText::Text> g_hudText;
extern osg::ref_ptr<osg::Uniform> g_hudAlpha;
extern osg::ref_ptr<osgText::Text> g_hudTextShd;
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
          _scale(scale), _textMarginLeft(20.0f), _textMarginTop(40.0f),
          _logoMarginRight(20.0f), _logoMarginTop(20.0f)
    {}

    virtual bool handle(const osgGA::GUIEventAdapter& ea,
                        osgGA::GUIActionAdapter& aa)
    {
        if (ea.getEventType() == osgGA::GUIEventAdapter::RESIZE)
        {
            int width = ea.getWindowWidth();
            int height = ea.getWindowHeight();

            // Update HUD camera projection to match new window size
            _hudCamera->setProjectionMatrix(
                osg::Matrix::ortho2D(0, width, 0, height));

            // Reposition and resize all HUD elements
            updateHUDElements(width, height);

            return false; // Allow other handlers to process this event
        }
        return false;
    }

    // Allow customization of margins
    void setTextMargins(float left, float top)
    {
        _textMarginLeft = left;
        _textMarginTop = top;
    }

    void setLogoMargins(float right, float top)
    {
        _logoMarginRight = right;
        _logoMarginTop = top;
    }

private:
    void updateHUDElements(int winWidth, int winHeight)
    {
        // Process all drawables in the geode
        for (unsigned int i = 0; i < _geode->getNumDrawables(); i++)
        {
            osg::Drawable* drawable = _geode->getDrawable(i);

            // Update logo quad (geometry with texture)
            if (osg::Geometry* quad = dynamic_cast<osg::Geometry*>(drawable))
            {
                updateLogoQuad(quad, winWidth, winHeight);
            }

            // Update text position (upper left corner)
            if (osgText::Text* text = dynamic_cast<osgText::Text*>(drawable))
            {
                updateTextPosition(text, winWidth, winHeight);
            }
        }
    }

    void updateLogoQuad(osg::Geometry* quad, int winWidth, int winHeight)
    {
        osg::Vec3Array* verts =
            dynamic_cast<osg::Vec3Array*>(quad->getVertexArray());

        if (!verts || verts->size() != 4) return;

        // Check if this geometry has a texture (identifies it as the logo)
        osg::StateSet* stateSet = quad->getStateSet();
        if (!stateSet) return;

        osg::Texture2D* tex = dynamic_cast<osg::Texture2D*>(
            stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));

        if (!tex || !tex->getImage()) return;

        // Calculate scaled logo dimensions
        float logoWidth = tex->getImage()->s() * _scale;
        float logoHeight = tex->getImage()->t() * _scale;

        // Position in upper right corner
        // x1 is left edge, x2 is right edge
        float x1 = winWidth - logoWidth - _logoMarginRight;
        float x2 = winWidth - _logoMarginRight;

        // y1 is bottom edge, y2 is top edge
        float y1 = winHeight - logoHeight - _logoMarginTop;
        float y2 = winHeight - _logoMarginTop;

        // Update vertex positions (counter-clockwise quad)
        (*verts)[0] = osg::Vec3(x1, y1, 0); // bottom-left
        (*verts)[1] = osg::Vec3(x2, y1, 0); // bottom-right
        (*verts)[2] = osg::Vec3(x2, y2, 0); // top-right
        (*verts)[3] = osg::Vec3(x1, y2, 0); // top-left

        // Mark arrays as modified
        verts->dirty();
        quad->dirtyBound();
    }

    void updateTextPosition(osgText::Text* text, int winWidth, int winHeight)
    {
        // Position text in upper left corner
        // Y coordinate is measured from bottom, so we subtract margin from
        // height
        float xPos = _textMarginLeft;
        float yPos = winHeight - _textMarginTop;

        text->setPosition(osg::Vec3(xPos, yPos, 0));

        // Optionally adjust text alignment to ensure it stays within bounds
        text->setAlignment(osgText::Text::LEFT_TOP);
    }

    osg::Camera* _hudCamera;
    osg::Geode* _geode;
    std::string _logoFile;
    float _scale;

    // Configurable margins
    float _textMarginLeft;
    float _textMarginTop;
    float _logoMarginRight;
    float _logoMarginTop;
};
#endif
