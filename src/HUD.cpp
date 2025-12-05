#include "HUD.h"


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
    tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
    tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);

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


    osg::notify(osg::WARN) << "Done " << std::endl;

    g_hudText = new osgText::Text;
    g_hudText->setCharacterSize(28.0f);
    g_hudText->setColor(osg::Vec4(1, 1, 1, 1));
    g_hudText->setPosition(osg::Vec3(20, winHeight - 40, 0));
    g_hudText->setText("No data"); // default text
   

    geode->addDrawable(g_hudText);

    hudCamera->addChild(geode);

    return hudCamera;
}

void hudSetText(const std::string& text)
{
    if (g_hudText.valid()) g_hudText->setText(text);
}

bool castCameraRayIntersection(osgViewer::Viewer* viewer, osg::Node* scene,
                               osg::Vec3d& out_point, osg::Vec3d& out_normal)
{
    osg::Camera* cam = viewer->getCamera();

    // 1. Get view matrix and extract eye/center/up
    osg::Vec3d eye, center, up;
    cam->getViewMatrixAsLookAt(eye, center, up);

    osg::Vec3d dir = center - eye;
    dir.normalize();

    // 2. Build long ray
    osg::Vec3d start = eye;
    osg::Vec3d end = eye + dir * 1e6; // long ray

    // 3. Create intersector
    osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector =
        new osgUtil::LineSegmentIntersector(start, end);

    osgUtil::IntersectionVisitor iv(intersector.get());
    scene->accept(iv);

    if (!intersector->containsIntersections()) return false;

    const auto& result = *intersector->getIntersections().begin();

    out_point = result.getWorldIntersectPoint();
    out_normal = result.getWorldIntersectNormal();

    return true;
}

std::string getLandInfoAtIntersection(osg::Node* sceneRoot,
                                      const osg::Vec3d& hitPoint)
{
    // Find which geometry was hit using an intersection visitor
    osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector =
        new osgUtil::LineSegmentIntersector(hitPoint - osg::Vec3d(0, 0, 0.1),
                                            hitPoint + osg::Vec3d(0, 0, 0.1));

    osgUtil::IntersectionVisitor iv(intersector.get());
    sceneRoot->accept(iv);

    if (intersector->containsIntersections())
    {
        const osgUtil::LineSegmentIntersector::Intersection& hit =
            intersector->getFirstIntersection();

        // Get the drawable that was hit
        osg::Node* hitNode = nullptr;
        if (!hit.nodePath.empty())
        {
            // Walk up the node path to find a node with UserData
            for (auto it = hit.nodePath.rbegin(); it != hit.nodePath.rend();
                 ++it)
            {
                osgSim::ShapeAttributeList* sal =
                    dynamic_cast<osgSim::ShapeAttributeList*>(
                        (*it)->getUserData());

                if (sal)
                {
                    hitNode = *it;
                    std::ostringstream info;

                    // Extract all attributes
                    for (unsigned j = 0; j < sal->size(); j++)
                    {
                        const osgSim::ShapeAttribute& attr = (*sal)[j];
                        info << attr.getName() << ": ";

                        switch (attr.getType())
                        {
                            case osgSim::ShapeAttribute::STRING:
                                info << attr.getString();
                                break;
                            case osgSim::ShapeAttribute::INTEGER:
                                info << attr.getInt();
                                break;
                            case osgSim::ShapeAttribute::DOUBLE:
                                info << attr.getDouble();
                                break;
                            default: info << "unknown type";
                        }
                        info << "\n";
                    }
                    std::cout << info.str();
                    return info.str();
                }
            }
        }
    }

    return "No land data found at intersection";
}
