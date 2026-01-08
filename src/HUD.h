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
    { "mall", "centrum handlowe" },
    { "christian", "łąka" },
    { "cycleway", "ścieżka rowerowa" },
    { "military", "obiekt wojskowy" },
    { "farmyard", "podwórze gospodarskie" },
    { "swimming_pool", "basen" },
    { "kindergarten", "przedszkole" },
    { "school", "szkoła" },
    { "pitch", "boisko" },
    { "theatre", "teatr" },
    { "car_wash", "myjnia" },
    { "attraction", "atrakcja turystyczna" },
    { "graveyard", "cmentarz" },
    { "fountain", "fontanna" },
    { "bakery", "piekarnia" },
    { "castle", "zamek" },
    { "dentist", "dentysta" },

    // ---- Added from CSV ----
    { "airport", "lotnisko" },
    { "apron", "płyta lotniska" },
    { "arts_centre", "centrum sztuki" },
    { "artwork", "dzieło sztuki" },
    { "bank", "bank" },
    { "bar", "bar" },
    { "beach", "plaża" },
    { "beauty_shop", "salon kosmetyczny" },
    { "bench", "ławka" },
    { "beverages", "napoje" },
    { "bicycle_rental", "wypożyczalnia rowerów" },
    { "bicycle_shop", "sklep rowerowy" },
    { "biergarten", "ogródek piwny" },
    { "bookshop", "księgarnia" },
    { "buddhist", "obiekt buddyjski" },
    { "bus_station", "dworzec autobusowy" },
    { "butcher", "rzeźnik" },
    { "cafe", "kawiarnia" },
    { "camp_site", "pole namiotowe" },
    { "car_repair", "warsztat samochodowy" },
    { "car_sharing", "carsharing" },
    { "charging_station", "stacja ładowania" },
    { "cinema", "kino" },
    { "college", "uczelnia" },
    { "community_centre", "centrum społecznościowe" },
    { "construction", "teren budowy" },
    { "convenience", "sklep spożywczy" },
    { "courthouse", "sąd" },
    { "fast_food", "fast food" },
    { "fire_station", "straż pożarna" },
    { "florist", "kwiaciarnia" },
    { "fuel", "stacja paliw" },
    { "hospital", "szpital" },
    { "hotel", "hotel" },
    { "information", "informacja" },
    { "library", "biblioteka" },
    { "marketplace", "targowisko" },
    { "museum", "muzeum" },
    { "parking", "parking" },
    { "pharmacy", "apteka" },
    { "place_of_worship", "miejsce kultu" },
    { "police", "policja" },
    { "post_office", "poczta" },
    { "pub", "pub" },
    { "restaurant", "restauracja" },
    { "supermarket", "supermarket" },
    { "taxi", "postój taksówek" },
    { "townhall", "ratusz" },
    { "university", "uniwersytet" },
    { "viewpoint", "punkt widokowy" },
    { "zoo", "zoo" }
};

// Create a resize handler class
// Simplified HUDResizeHandler class - add this to HUD.h
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

            // Get the parent (should be the root group)
            if (_hudCamera && _hudCamera->getNumParents() > 0)
            {
                osg::Group* parent = _hudCamera->getParent(0);

                // Remove old HUD
                parent->removeChild(_hudCamera);

                // Create new HUD with updated dimensions
                _hudCamera = createHUD(_logoFile, _scale, width, height);

                // Update geode reference (it's the first child of the new
                // camera)
                _geode = dynamic_cast<osg::Geode*>(_hudCamera->getChild(0));

                // Add new HUD back to parent
                parent->addChild(_hudCamera);
            }

            return false; // Allow other handlers to process this event
        }
        return false;
    }

private:
    osg::Camera* _hudCamera;
    osg::Geode* _geode;
    std::string _logoFile;
    float _scale;
};
#endif
