/* Poruszanie si� po mapie,
    aby m�c si� pochyli� :
Naci�nij klawisz 5 na klawiaturze, aby zmieni� tryb poruszania si�
Naci�ni�cie scrolla s�u�y do poruszania si� po mapie
Naci�ni�cie LPM s�u�y do nachylania si� na mapie podmr�nymi k�tami
Naci�ni�cie PPM s�u�y do oddalania i przybli�ania.
Aby uzyska� wi�cej informacji o nawigowaniu po mapie naci�nij h.
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
#include <osg/PolygonOffset>
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

// Warstwy terenu - kazdy typ na OSOBNEJ warstwie (wyzsze = blizej kamery)
// 0:residential,1:industrial,2:commercial,3:retail,4:farmland,5:farmyard,
// 6:quarry,7:military,8:grass,9:meadow,10:scrub,11:heath,12:forest,
// 13:orchard,14:nature_reserve,15:allotments,16:park,17:recreation_ground,
// 18:cemetery (najwyzej)
void apply_layer_offset(osg::StateSet* ss, int layer)
{
    if (!ss) return;
    // PolygonOffset: wieksze wartosci = dalej od kamery
    // Warstwa 0 ma offset 18, warstwa 18 ma offset 0
    float offset = static_cast<float>(18 - layer);
    osg::ref_ptr<osg::PolygonOffset> po = new osg::PolygonOffset(offset, offset);
    ss->setAttributeAndModes(po, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
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

        // Kazdy typ na OSOBNEJ warstwie (0 = najnizej, 17 = najwyzej)
        // === WARSTWA 0-3: TERENY MIEJSKIE (na spodzie) ===
        if (name.find("residential") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/concrete.jpg");
            setup_standard_shader(ss, file_path);
            apply_layer_offset(ss, 0);
        }
        else if (name.find("industrial") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/concrete.jpg");
            setup_standard_shader(ss, file_path);
            apply_layer_offset(ss, 1);
        }
        else if (name.find("commercial") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/concrete.jpg");
            setup_standard_shader(ss, file_path);
            apply_layer_offset(ss, 2);
        }
        else if (name.find("retail") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/concrete.jpg");
            setup_standard_shader(ss, file_path);
            apply_layer_offset(ss, 3);
        }
        // === WARSTWA 4-6: ROLNICTWO, WOJSKO, KAMIENIOŁOMY ===
        else if (name.find("farmland") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/farmland.jpg");
            setup_standard_shader(ss, file_path);
            apply_layer_offset(ss, 4);
        }
        else if (name.find("farmyard") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/farmland.jpg");
            setup_standard_shader(ss, file_path);
            apply_layer_offset(ss, 5);
        }
        else if (name.find("quarry") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/rock.jpg");
            setup_standard_shader(ss, file_path);
            apply_layer_offset(ss, 6);
        }
        else if (name.find("military") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/military.jpg");
            setup_standard_shader(ss, file_path);
            apply_layer_offset(ss, 7);
        }
        // === WARSTWA 8-10: TRAWA, ŁĄKI, ZAROŚLA ===
        else if (name.find("grass") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/grass.jpg");
            setup_wind_shader(ss, file_path);
            apply_layer_offset(ss, 8);
        }
        else if (name.find("meadow") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/grass.jpg");
            setup_wind_shader(ss, file_path);
            apply_layer_offset(ss, 9);
        }
        else if (name.find("scrub") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/scrub.jpg");
            setup_wind_shader(ss, file_path);
            apply_layer_offset(ss, 10);
        }
        else if (name.find("heath") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/scrub.jpg");
            setup_wind_shader(ss, file_path);
            apply_layer_offset(ss, 11);
        }
        // === WARSTWA 12: LASY ===
        else if (name.find("forest") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/forest.jpg");
            setup_wind_shader(ss, file_path);
            apply_layer_offset(ss, 12);
        }
        // === WARSTWA 13-15: SADY, DZIAŁKI, REZERWATY ===
        else if (name.find("orchard") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/orchard.jpg");
            setup_wind_shader(ss, file_path);
            apply_layer_offset(ss, 13);
        }
        else if (name.find("nature_reserve") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/orchard.jpg");
            setup_wind_shader(ss, file_path);
            apply_layer_offset(ss, 14);
        }
        else if (name.find("allotments") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/allotments.jpg");
            setup_wind_shader(ss, file_path);
            apply_layer_offset(ss, 15);
        }
        // === WARSTWA 16-17: PARKI, TERENY REKREACYJNE ===
        else if (name.find("park") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/grass.jpg");
            setup_wind_shader(ss, file_path);
            apply_layer_offset(ss, 16);
        }
        else if (name.find("recreation_ground") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/sport_green.jpg");
            setup_wind_shader(ss, file_path);
            apply_layer_offset(ss, 17);
        }
        // === WARSTWA 18: CMENTARZ (najwyżej - parallax) ===
        else if (name.find("cemetery") != std::string::npos)
        {
            apply_texture(ss, file_path + "/textures/cemetery.jpg");
            setup_parallax_shader(ss, file_path);
            apply_layer_offset(ss, 18);
        }
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            if (nodes[i].valid()) nodes[i]->setStateSet(ss);
        }
    }
    // Depth test włączony z zapisem do depth buffer (true)
    // Dziala razem z PolygonOffset dla rozwiazania Z-fighting
    land_model->getOrCreateStateSet()->setAttributeAndModes(
        new osg::Depth(osg::Depth::LESS, 0, 1, true));

    std::cout << "--- Koniec Landuse ---" << std::endl;
    return land_group.release();
}