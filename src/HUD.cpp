#include "HUD.h"
#include "common.h"


osg::Camera* createHUD(const std::string& logoFile, float scale, int winWidth,
                       int winHeight)
{
    // --- HUD Camera ---
    osg::Camera* hudCamera = new osg::Camera;

    hudCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    hudCamera->setClearMask(GL_DEPTH_BUFFER_BIT);
    hudCamera->setRenderOrder(osg::Camera::POST_RENDER);
    hudCamera->setProjectionMatrix(
        osg::Matrix::ortho2D(0, winWidth, 0, winHeight));
    hudCamera->setViewMatrix(osg::Matrix::identity());
    hudCamera->setAllowEventFocus(false);

    // --- Load Texture ---
    osg::ref_ptr<osg::Image> image = osgDB::readRefImageFile(logoFile);
    if (!image)
    {
        osg::notify(osg::WARN)
            << "Could not load HUD image: " << logoFile << std::endl;
        return hudCamera;
    }

    osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(image);

    // --- Quad Geometry (top-right corner) ---
    float w = image->s() * scale;
    float h = image->t() * scale;
    float x1 = winWidth - w - 20; // 20px margin from right
    float y1 = winHeight - h - 20; // 20px margin from top
    float x2 = x1 + w;
    float y2 = y1 + h;

    osg::ref_ptr<osg::Geometry> quad = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;

    verts->push_back(osg::Vec3(x1, y1, 0));
    verts->push_back(osg::Vec3(x2, y1, 0));
    verts->push_back(osg::Vec3(x2, y2, 0));
    verts->push_back(osg::Vec3(x1, y2, 0));

    quad->setVertexArray(verts);

    osg::ref_ptr<osg::Vec2Array> texcoords = new osg::Vec2Array;
    texcoords->push_back(osg::Vec2(0, 0));
    texcoords->push_back(osg::Vec2(1, 0));
    texcoords->push_back(osg::Vec2(1, 1));
    texcoords->push_back(osg::Vec2(0, 1));

    quad->setTexCoordArray(0, texcoords);
    quad->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 4));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(quad);

    osg::ref_ptr<osg::StateSet> ss = geode->getOrCreateStateSet();
    ss->setTextureAttributeAndModes(0, tex.get(), osg::StateAttribute::ON);
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    ss->setMode(GL_BLEND, osg::StateAttribute::ON);
    ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    g_hudText = new osgText::Text;
    g_hudText->setFont("font/OpenSans-VariableFont_wdth,wght.ttf");
    g_hudText->setCharacterSize(28.0f);
    g_hudText->setColor(osg::Vec4(1, 1, 1, 1));
    g_hudText->setPosition(osg::Vec3(20, winHeight - 40, 0));
    g_hudText->setText("Move the camera in order to gather data about terrain"); // default text


    geode->addDrawable(g_hudText);

    hudCamera->addChild(geode);

    return hudCamera;
}

void hudSetText(const std::string& text)
{
    if (g_hudText.valid()) 
        g_hudText->setText(osgText::String(text, osgText::String::ENCODING_UTF8));
}

static inline void ltrim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
}
static inline void rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
}
static inline void trim(std::string& s)
{
    ltrim(s);
    rtrim(s);
}

std::string translateFclass(const std::string& fclass)
{
    auto it = fclassPL.find(fclass);
    if (it != fclassPL.end()) return it->second;
    return fclass;
}


std::string getLandInfoAtIntersection(osg::Node* sceneRoot,
                                      const osg::Vec3d& hitPoint)
{
    std::ostringstream allInfo;
    int hitCount = 0;

    // Define a box around the hit point
    osg::Viewport* viewport = viewer->getCamera()->getViewport();
    double dx = viewport->width() * 0.05;
    double dy = viewport->height() * 0.05;
    double y = viewport->x() + viewport->width() * 0.5;
    double x = viewport->x() + viewport->height() * 0.5;
    osg::ref_ptr<osgUtil::PolytopeIntersector> picker =
        new osgUtil::PolytopeIntersector(osgUtil::Intersector::WINDOW,
                                         x - dx * 0.5, y - dy * 0.5,
                                         x + dx * 0.5, y + dy * 0.5);
    osgUtil::IntersectionVisitor iv(picker.get());
    viewer->getCamera()->accept(iv);

    if (picker->containsIntersections())
    {
        std::set<osgSim::ShapeAttributeList*> processedSAL;
        std::set<std::pair<std::string, std::string>> globalRecords;
        constexpr std::size_t MAX_RECORDS = 3;
        std::size_t collectedCount = 0;
        for (const auto& intersection : picker->getIntersections())
        {
            if (intersection.drawable.valid())
            {
                osgSim::ShapeAttributeList* sal =
                    dynamic_cast<osgSim::ShapeAttributeList*>(
                        intersection.drawable->getUserData());
                if (sal && processedSAL.find(sal) == processedSAL.end())
                {
                    processedSAL.insert(sal);
                    hitCount++;
                    bool hasData = false;
                    std::string fclass;
                    for (unsigned j = 0; j < sal->size(); j++)
                    {
                        const osgSim::ShapeAttribute& attr = (*sal)[j];
                        std::string attrName = attr.getName();
                        if (attrName != "fclass") continue;
                        if (attr.getType() == osgSim::ShapeAttribute::STRING)
                        {
                            fclass = attr.getString();
                            trim(fclass);
                        }
                        break;
                    }
                    if (fclass.empty()) continue;
                    for (unsigned j = 0; j < sal->size(); j++)
                    {
                        const osgSim::ShapeAttribute& attr = (*sal)[j];
                        std::string attrName = attr.getName();
                        if (attrName != "name") continue;
                        if (attr.getType() == osgSim::ShapeAttribute::STRING)
                        {
                            std::string att = attr.getString();
                            trim(att);
                            if (!att.empty())
                            {   
                                if (collectedCount >= MAX_RECORDS)
                                    break; // stop processing intersections
                                auto key = std::make_pair(fclass, att);

                                if (!globalRecords.insert(key).second)
                                    continue;
               
                                std::string fclassPL = translateFclass(fclass);
                                allInfo << fclassPL << ": " << att;
                                ++collectedCount;
                                hasData = true;
                            }
                        }
                        break;
                    }
                    if (hasData) allInfo << "\n";
                }
            }
        }
    }

    if (hitCount > 0) return allInfo.str();

    return "No land data found at intersection";
}




