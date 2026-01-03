#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>

#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osg/Depth>

#include <osgSim/ShapeAttribute>

#include <iostream>
#include <map>

#include "common.h"

using namespace osg;

using Mapping = std::map<std::string, std::vector<osg::ref_ptr<osg::Node>>>;

void parse_meta_data(osg::Node* model, Mapping& umap)
{
    osg::Group* group = model->asGroup();

    for (unsigned i = 0; i < group->getNumChildren(); i++)
    {
        osg::Node* kido = group->getChild(i);
        osgSim::ShapeAttributeList* sal =
            (osgSim::ShapeAttributeList*)kido->getUserData();
        if (!sal) continue;

        for (unsigned j = 0; j < sal->size(); j++)
        {
            // sprawdzamy czy atrybut nazywa się "fclass"
            // dla terenu to opis typu kultury
            if ((*sal)[j].getName().find("fclass") != std::string::npos)
            {
                if ((*sal)[j].getType() == osgSim::ShapeAttribute::STRING)
                    umap[(*sal)[j].getString()].push_back(kido);
            }
        }
    }
}

osg::Node* process_landuse(osg::Matrixd& ltw, osg::BoundingBox& wbb,
                           const std::string& file_path)
{
    std::string land_file_path = file_path + "/gis_osm_landuse_a_free_1.shp";

    // load the data
    osg::ref_ptr<osg::Node> land_model = osgDB::readRefNodeFile(land_file_path);
    if (!land_model)
    {
        std::cout << "Cannot load file " << land_file_path << std::endl;
        return nullptr;
    }

    osg::BoundingBox mgbb;

    ComputeBoundsVisitor cbv(mgbb);
    land_model->accept(cbv);

    ellipsoid->computeLocalToWorldTransformFromLatLongHeight(
        osg::DegreesToRadians(mgbb.center().y()),
        osg::DegreesToRadians(mgbb.center().x()), 0.0, ltw);


    // Transformacja ze współrzędnych geograficznych (GEO) do współrzędnych
    // świata (WGS)
    ConvertFromGeoProjVisitor<true> cfgp;
    land_model->accept(cfgp);

    wbb = cfgp._box;

    WorldToLocalVisitor ltwv(ltw, true);
    land_model->accept(ltwv);

#if 0
    // dokonuj dodatkowego przetwarzania wierzchołków po transformacji z układu Geo do WGS
    Mapping umap;
    parse_meta_data(land_model, umap);
#endif

    // requirement from water geometry to avoid z-fighting
    // do not write to depth buffer - zmask set to false
    land_model->getOrCreateStateSet()->setAttributeAndModes(
        new osg::Depth(osg::Depth::LESS, 0, 1, false));
    // draw terrain first
    land_model->getOrCreateStateSet()->setRenderBinDetails(-10, "RenderBin");
    // do not nest this render bin
    land_model->getOrCreateStateSet()->setNestRenderBins(false);


    // GOOD LUCK!


    return land_model.release();
}
