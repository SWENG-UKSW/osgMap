/* Poruszanie siê po mapie,
    aby móc siê pochyliæ :
Naciœnij klawisz 5 na klawiaturze, aby zmieniæ tryb poruszania siê
Naciœniêcie scrolla s³u¿y do poruszania siê po mapie
Naciœniêcie LPM s³u¿y do nachylania siê na mapie podmró¿nymi k¹tami
Naciœniêcie PPM s³u¿y do oddalania i przybli¿ania.
Aby uzyskaæ wiêcej informacji o nawigowaniu po mapie naciœnij h.
    */
#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>
#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osg/Depth>
#include <osg/Material>
#include <osg/StateSet>
#include <osg/Texture2D>
#include <osg/TexGen>
#include <osgSim/ShapeAttribute>
#include <osg/Program>
#include <osg/Shader>
#include <osg/Uniform>
#include <osg/Light>
#include <osg/LightSource>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include "common.h"

using namespace osg;

typedef std::map<std::string, std::vector<osg::ref_ptr<osg::Node>>> Mapping;

void parse_meta_data(osg::Node* model, Mapping& umap)
{
    if (!model) return;
    osg::Group* group = model->asGroup();
    if (!group) return;
    for (unsigned i = 0; i < group->getNumChildren(); i++)
    {
        osg::Node* kido = group->getChild(i);
        if (!kido) continue;
        osgSim::ShapeAttributeList* sal =
            (osgSim::ShapeAttributeList*)kido->getUserData();
        if (!sal) continue;
        for (unsigned j = 0; j < sal->size(); j++)
        {
            if ((*sal)[j].getName().find("fclass") != std::string::npos)
            {
                if ((*sal)[j].getType() == osgSim::ShapeAttribute::STRING)
                {
                    umap[(*sal)[j].getString()].push_back(kido);
                }
            }
        }
    }
}

void apply_texture(osg::StateSet* ss, const std::string& path)
{
    if (!ss) return;
    osg::ref_ptr<osg::Image> image = osgDB::readRefImageFile(path);
    if (image.valid())
    {
        osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D;
        texture->setImage(image);

        texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
        texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);

        texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);

        ss->setTextureAttributeAndModes(0, texture,
                                        osg::StateAttribute::ON
                                            | osg::StateAttribute::OVERRIDE);

        osg::ref_ptr<osg::TexGen> tg = new osg::TexGen;
        tg->setMode(osg::TexGen::OBJECT_LINEAR);

        tg->setPlane(osg::TexGen::S, osg::Plane(0.2, 0.0, 0.0, 0.0));
        tg->setPlane(osg::TexGen::T, osg::Plane(0.0, 0.2, 0.0, 0.0));

        ss->setTextureAttributeAndModes(
            0, tg, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    }
}

// Shader dla betonu (statyczny)
void setup_standard_shader(osg::StateSet* ss, const std::string& data_path)
{
    if (!ss) return;
    osg::ref_ptr<osg::Shader> vert = osgDB::readRefShaderFile(
        osg::Shader::VERTEX, data_path + "/standard.vert");
    osg::ref_ptr<osg::Shader> frag = osgDB::readRefShaderFile(
        osg::Shader::FRAGMENT, data_path + "/standard.frag");
    if (vert.valid() && frag.valid())
    {
        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addShader(vert);
        program->addShader(frag);
        ss->setAttributeAndModes(
            program, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
        ss->addUniform(new osg::Uniform("baseTexture", 0));
        std::cout << "  [OK] Shader Standard (Beton) zaladowany." << std::endl;
    }
}

// Shader dla roslinnosci (falowanie)
void setup_wind_shader(osg::StateSet* ss, const std::string& data_path)
{
    if (!ss) return;
    osg::ref_ptr<osg::Shader> vert =
        osgDB::readRefShaderFile(osg::Shader::VERTEX, data_path + "/wind.vert");
    osg::ref_ptr<osg::Shader> frag = osgDB::readRefShaderFile(
        osg::Shader::FRAGMENT, data_path + "/wind.frag");
    if (vert.valid() && frag.valid())
    {
        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addShader(vert);
        program->addShader(frag);
        ss->setAttributeAndModes(
            program, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
        ss->addUniform(new osg::Uniform("baseTexture", 0));
    }
}

// Shader dla cmentarza (Parallax 3D)
void setup_parallax_shader(osg::StateSet* ss, const std::string& data_path)
{
    if (!ss) return;
    osg::ref_ptr<osg::Shader> vert = osgDB::readRefShaderFile(
        osg::Shader::VERTEX, data_path + "/parallax.vert");
    osg::ref_ptr<osg::Shader> frag = osgDB::readRefShaderFile(
        osg::Shader::FRAGMENT, data_path + "/parallax.frag");
    osg::ref_ptr<osg::Image> h_img =
        osgDB::readRefImageFile(data_path + "/textures/cemetery_height.jpg");

    if (vert.valid() && frag.valid() && h_img.valid())
    {
        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addShader(vert);
        program->addShader(frag);
        ss->setAttributeAndModes(
            program, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

        osg::ref_ptr<osg::Texture2D> h_tex = new osg::Texture2D(h_img);
        h_tex->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
        h_tex->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
        h_tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        h_tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);

        ss->setTextureAttributeAndModes(
            1, h_tex, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

        ss->addUniform(new osg::Uniform("baseTexture", 0));
        ss->addUniform(new osg::Uniform("heightMap", 1));
        std::cout << "  [OK] Cmentarz 3D: Gotowy" << std::endl;
    }
}

osg::Node* process_landuse(osg::Matrixd& ltw, osg::BoundingBox& wbb,
                           const std::string& file_path)
{
    std::cout << "--- Start Landuse (Urbanizacja) ---" << std::endl;
    osg::ref_ptr<osg::Node> land_model =
        osgDB::readRefNodeFile(file_path + "/gis_osm_landuse_a_free_1.shp");
    if (!land_model) return nullptr;

    osg::ref_ptr<osg::Group> land_group = new osg::Group;
    osg::ref_ptr<osg::Light> light = new osg::Light;
    light->setLightNum(1);
    light->setPosition(osg::Vec4(1.0, 1.0, 1.0, 0.0));
    light->setDiffuse(osg::Vec4(1.0, 1.0, 0.9, 1.0));
    light->setAmbient(osg::Vec4(0.4, 0.4, 0.4, 1.0));

    osg::ref_ptr<osg::LightSource> lightSource = new osg::LightSource;
    lightSource->setLight(light);
    land_group->addChild(lightSource);
    land_group->addChild(land_model);

    osg::StateSet* rootSS = land_group->getOrCreateStateSet();
    rootSS->setMode(GL_LIGHTING,
                    osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    rootSS->setMode(GL_LIGHT1, osg::StateAttribute::ON);
    rootSS->setMode(GL_LIGHT0, osg::StateAttribute::OFF);

    osg::BoundingBox mgbb;
    ComputeBoundsVisitor cbv(mgbb);
    land_model->accept(cbv);
    if (ellipsoid.valid())
    {
        ellipsoid->computeLocalToWorldTransformFromLatLongHeight(
            osg::DegreesToRadians(mgbb.center().y()),
            osg::DegreesToRadians(mgbb.center().x()), 0.0, ltw);
    }
    ConvertFromGeoProjVisitor<true> cfgp;
    land_model->accept(cfgp);
    wbb = cfgp._box;
    WorldToLocalVisitor ltwv(ltw, true);
    land_model->accept(ltwv);

    Mapping umap;
    parse_meta_data(land_model, umap);
    for (Mapping::iterator it = umap.begin(); it != umap.end(); ++it)
    {
        std::string name = it->first;
        std::vector<osg::ref_ptr<osg::Node>>& nodes = it->second;
        osg::ref_ptr<osg::StateSet> ss = new osg::StateSet();

        // 1. LASY
        if (name.find("forest") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/forest.jpg");
            setup_wind_shader(ss, file_path);
        }
        // 2. TRAWA I PARKI
        else if (name.find("grass") != std::string::npos
                 || name.find("park") != std::string::npos
                 || name.find("meadow") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/grass.jpg");
            setup_wind_shader(ss, file_path);
        }
        // 3. CMENTARZ
        else if (name.find("cemetery") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/cemetery.jpg");
            setup_parallax_shader(ss, file_path);
        }
        // 4. TERENY MIEJSKIE - Beton
        else if (name.find("residential") != std::string::npos
                 || name.find("commercial") != std::string::npos
                 || name.find("industrial") != std::string::npos
                 || name.find("retail") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/concrete.jpg");
            setup_standard_shader(ss, file_path);
        }

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            if (nodes[i].valid()) nodes[i]->setStateSet(ss);
        }
    }
    land_model->getOrCreateStateSet()->setAttributeAndModes(
        new osg::Depth(osg::Depth::LESS, 0, 1, false));

    std::cout << "--- Koniec Landuse ---" << std::endl;
    return land_group.release();
}