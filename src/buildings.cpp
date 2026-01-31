#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>

#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>

#include <osgSim/ShapeAttribute>

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/FrontFace>
#include <osg/CopyOp>
#include <osgUtil/Tessellator>

#include <osg/Texture2D>
#include <osg/StateSet>

#include <filesystem>
#include <system_error>

#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <cmath>

#include "common.h"

using namespace osg;

namespace {
std::vector<osg::ref_ptr<osg::StateSet>> g_roofTextures;

class GeomVisitor : public osg::NodeVisitor {
public:
    GeomVisitor(): osg::NodeVisitor(NodeVisitor::TRAVERSE_ALL_CHILDREN) {}

    osg::Geode* getFirstGeode() const { return _firstGeode; }

    void apply(osg::Node& node) override
    {
        if (!_firstGeode) traverse(node);
    }

    void apply(osg::Geode& geode) override
    {
        if (!_firstGeode) _firstGeode = &geode;
    }

private:
    osg::Geode* _firstGeode = nullptr;
};

const char* vertSource = R"(
#version 420 compatibility

out vec3 v_texCoord;
out vec3 v_normal;
out vec3 v_ecp;

void main() {
    v_texCoord = gl_MultiTexCoord0.xyz;
    v_ecp = vec3(gl_ModelViewMatrix * gl_Vertex);
    v_normal = gl_Normal;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
)";

const char* fragSource = R"(
#version 420 compatibility

uniform sampler2D diffuseMap;

in vec3 v_texCoord;
in vec3 v_normal;
in vec3 v_ecp;

void main() {
    vec4 texColor = texture2D(diffuseMap, v_texCoord.xy);

    vec3 N = normalize(gl_NormalMatrix * v_normal);
    vec3 L = normalize(gl_LightSource[0].position.xyz);
    vec3 V = normalize(-v_ecp);

    float NdotL = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);

    float bld_amb=0.5;

    vec3 ambient = gl_LightSource[0].ambient.rgb * texColor.rgb + vec3(v_texCoord.z);
    vec3 diffuse = gl_LightSource[0].diffuse.rgb * texColor.rgb * NdotL;
    vec3 specular = gl_LightSource[0].specular.rgb * pow(NdotH, 2.0);

    gl_FragColor = vec4(ambient + diffuse + specular, texColor.a);
}
)";

osg::ref_ptr<osg::StateSet> loadRoofTexture(const std::string& texPath)
{
    osg::ref_ptr<osg::Image> img = osgDB::readRefImageFile(texPath);
    if (!img.valid())
    {
        std::cout << "[ROOF] WARN: cannot load texture: " << texPath << "\n";
        return nullptr;
    }
    osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(img.get());
    tex->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    tex->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
    tex->setFilter(osg::Texture::MIN_FILTER,
                   osg::Texture::LINEAR_MIPMAP_LINEAR);
    tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    tex->setMaxAnisotropy(8.0f);
    tex->setUseHardwareMipMapGeneration(true);
    osg::ref_ptr<osg::StateSet> pss = new osg::StateSet();
    pss->setTextureAttributeAndModes(0, tex);
    osg::Program* program = new osg::Program;
    program->addShader(new osg::Shader(osg::Shader::VERTEX, vertSource));
    program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragSource));
    pss->setAttribute(program);
    pss->addUniform(new osg::Uniform("diffuseMap", 0));
    pss->setMode(GL_CULL_FACE, osg::StateAttribute::ON);
    pss->setAttributeAndModes(new osg::FrontFace(osg::FrontFace::CLOCKWISE));
    return pss;
}


/* ============================================================
   Extrusion (roof + walls)
   ============================================================ */

void extrude_simple(osg::Geode* geode, osg::Geometry* baseGeom, float hMeters,
                    int roofIdx, float roofTile = 8.0f)
{
    if (!geode || !baseGeom || hMeters <= 0.f) return;

    osg::Vec3Array* v =
        dynamic_cast<osg::Vec3Array*>(baseGeom->getVertexArray());
    if (!v || v->size() < 3) return;


    osg::ref_ptr<osg::Vec3Array> roofVerts = v;
    osg::ref_ptr<osg::Geometry> roof = baseGeom;
    roof->dirtyDisplayList();

    // -----------------------
    // Dach - UV (planarne XY + tiling)
    // -----------------------
    float minX = (*v)[0].x(), maxX = minX;
    float minY = (*v)[0].y(), maxY = minY;

    for (unsigned i = 1; i < v->size(); ++i)
    {
        minX = std::min(minX, (*v)[i].x());
        maxX = std::max(maxX, (*v)[i].x());
        minY = std::min(minY, (*v)[i].y());
        maxY = std::max(maxY, (*v)[i].y());
    }

    float dx = std::max(1e-6f, maxX - minX);
    float dy = std::max(1e-6f, maxY - minY);

    osg::ref_ptr<osg::Vec3Array> roofUV = new osg::Vec3Array;
    roofUV->reserve(roofVerts->size());

    for (unsigned i = 0; i < roofVerts->size(); ++i)
    {
        const osg::Vec3& p = (*roofVerts)[i];
        float u = (p.x() - minX) / dx * roofTile;
        float vv = (p.y() - minY) / dy * roofTile;
        roofUV->push_back(osg::Vec3(u, vv, 0.f));
    }

    roof->setTexCoordArray(0, roofUV.get(), osg::Array::BIND_PER_VERTEX);

    // Normalna dachu
    osg::ref_ptr<osg::Vec3Array> roofN = new osg::Vec3Array;
    roofN->push_back(osg::Vec3(0.f, 0.f, 1.f));
    roof->setNormalArray(roofN.get(), osg::Array::BIND_OVERALL);

    // -----------------------
    // Ściany
    // -----------------------
    osg::ref_ptr<osg::Vec3Array> wallVerts = new osg::Vec3Array;
    wallVerts->reserve(v->size() * 6);

    osg::ref_ptr<osg::Vec3Array> wallTC = new osg::Vec3Array;
    wallTC->reserve(v->size() * 6);


    for (unsigned i = 0; i < v->size(); ++i)
    {
        osg::Vec3 b0 = (*v)[i];
        osg::Vec3 b1 = (*v)[(i + 1) % v->size()];
        osg::Vec3 t0 = b0;
        t0.z() += hMeters;
        osg::Vec3 t1 = b1;
        t1.z() += hMeters;

        float wrand = float(rand() % 256) / 256;

        wallTC->push_back(osg::Vec3(1, 1, wrand * 0.1f));
        wallTC->push_back(osg::Vec3(1, 0, wrand * 0.1f));
        wallTC->push_back(osg::Vec3(0, 0, wrand * 0.1f));
        wallTC->push_back(osg::Vec3(0, 1, wrand * 0.1f));
        wallTC->push_back(osg::Vec3(1, 1, wrand * 0.1f));
        wallTC->push_back(osg::Vec3(0, 0, wrand * 0.1f));

        wallVerts->push_back(t1);
        wallVerts->push_back(b1);
        wallVerts->push_back(b0);
        wallVerts->push_back(t0);
        wallVerts->push_back(t1);
        wallVerts->push_back(b0);
    }

    for (unsigned i = 0; i < v->size(); ++i)
        (*v)[i].set((*v)[i][0], (*v)[i][1], (*v)[i][2] + hMeters);

    osg::ref_ptr<osg::Geometry> walls = new osg::Geometry;
    walls->setVertexArray(wallVerts.get());
    walls->setTexCoordArray(0, wallTC.get());

    walls->addPrimitiveSet(
        new osg::DrawArrays(GL_TRIANGLES, 0, wallVerts->size()));

    osg::ref_ptr<osg::Vec3Array> wallN = new osg::Vec3Array;
    wallN->reserve(wallVerts->size());
    for (unsigned i = 0; i < wallVerts->size(); i += 3)
    {
        osg::Vec3 a = (*wallVerts)[i + 0];
        osg::Vec3 b = (*wallVerts)[i + 1];
        osg::Vec3 c = (*wallVerts)[i + 2];
        osg::Vec3 n = (b - a) ^ (c - a);
        if (n.length() > 1e-6f) n.normalize();
        wallN->push_back(n);
        wallN->push_back(n);
        wallN->push_back(n);
    }
    walls->setNormalArray(wallN.get(), osg::Array::BIND_PER_VERTEX);

    // -----------------------
    // StateSety
    // -----------------------
    walls->setStateSet(g_roofTextures.back());

    if (roofIdx >= 0 && roofIdx < (int)g_roofTextures.size()
        && g_roofTextures[roofIdx].valid())
    {
        roof->setStateSet(g_roofTextures[roofIdx]);
    }
    geode->addDrawable(walls);
    geode->addDrawable(roof);
}

/* ============================================================
   Metadata + extrusion loop
   ============================================================ */

void parse_meta_data(osg::Node* model)
{
    osg::Group* group = model ? model->asGroup() : nullptr;
    if (!group) return;

    // wysokość z metadanych
    std::vector<float> heights;
    heights.reserve(group->getNumChildren());

    for (unsigned i = 0; i < group->getNumChildren(); i++)
    {
        osg::Node* kido = group->getChild(i);
        osgSim::ShapeAttributeList* sal =
            dynamic_cast<osgSim::ShapeAttributeList*>(kido->getUserData());

        float height = 0.f;
        if (sal)
        {
            for (unsigned j = 0; j < sal->size(); j++)
            {
                if ((*sal)[j].getName().find("height") != std::string::npos)
                {
                    if ((*sal)[j].getType() == osgSim::ShapeAttribute::DOUBLE)
                        height = float((*sal)[j].getDouble()) / 100.f;
                    else if ((*sal)[j].getType()
                             == osgSim::ShapeAttribute::INTEGER)
                        height = float((*sal)[j].getInt()) / 100.f;
                    break;
                }
            }
        }
        heights.push_back(height);
    }

    GeomVisitor gv;
    model->accept(gv);
    osg::Geode* geode = gv.getFirstGeode();
    if (!geode) return;


    std::vector<osg::ref_ptr<osg::Geometry>> geoms;
    geoms.reserve(geode->getNumDrawables());

    for (unsigned i = 0; i < geode->getNumDrawables(); ++i)
    {
        osg::Geometry* g = dynamic_cast<osg::Geometry*>(geode->getDrawable(i));
        if (g) geoms.push_back(g);
    }

    unsigned K = std::min((unsigned)heights.size(), (unsigned)geoms.size());
    std::cout << "[INFO] Extruding buildings...\n";
    geode->removeChildren(0, geode->getNumChildren());

    for (unsigned i = 0; i < K; ++i)
    {
        float h = heights[i];
        if (h <= 0.f) continue;

        // losowe wybieranie tekstur dachów
        int roofIdx = -1;
        if (!g_roofTextures.empty())
        {
            uint32_t seed = 2166136261u;


            seed ^= (uint32_t)i + 0x9e3779b9u + (seed << 6) + (seed >> 2);


            uint32_t hc = (uint32_t)std::lround(h * 100.0f);
            seed ^= hc + 0x9e3779b9u + (seed << 6) + (seed >> 2);

            roofIdx = (int)(seed % (uint32_t)g_roofTextures.size() - 1);
        }

        extrude_simple(geode, geoms[i].get(), h, roofIdx, 8.0f);


        if (i > 0 && (i % 10000) == 0)
            std::cout << "[INFO] Extruded " << i << " buildings\n";
    }

    geode->dirtyBound();
    std::cout << "[INFO] Extrusion finished.\n";
}

}

/* ============================================================
   process_buildings + cache
   ============================================================ */

osg::Node* process_buildings(osg::Matrixd& ltw, const std::string& file_path)
{
    const std::string buildings_file_path = file_path + "/buildings_levels.shp";


    std::error_code ec;
    uintmax_t fileSize = std::filesystem::file_size(buildings_file_path, ec);
    if (ec)
    {
        std::cout << "Blad: Nie mozna odczytac rozmiaru pliku "
                  << buildings_file_path << std::endl;
        return nullptr;
    }
    std::string cacheFileName =
        "buildings_" + std::to_string(fileSize) + ".osgb";
    const std::filesystem::path cachePath =
        std::filesystem::current_path() / cacheFileName;

    // 1) Cache
    if (std::filesystem::exists(cachePath))
    {
        std::cout << "[BUILDINGS] Znaleziono cache [" << cachePath.string()
                  << "]\n";
        osg::ref_ptr<osg::Node> cached =
            osgDB::readRefNodeFile(cachePath.string());
        if (cached.valid())
        {
            std::cout << "[BUILDINGS] Cache OK, pomijam generowanie\n";
            return cached.release();
        }
        std::cout << "[BUILDINGS] Cache uszkodzony, generuje od nowa\n";
    }

    // 2) Wczytaj SHP
    osg::ref_ptr<osg::Node> buildings_model =
        osgDB::readRefNodeFile(buildings_file_path);
    if (!buildings_model.valid())
    {
        std::cout << "[BUILDINGS] Cannot load " << buildings_file_path << "\n";
        return nullptr;
    }

    // 3) Transformacje
    ConvertFromGeoProjVisitor<true> cfgp;
    buildings_model->accept(cfgp);

    WorldToLocalVisitor ltwv(ltw, true);
    buildings_model->accept(ltwv);

    // 4) Tekstury dachów
    g_roofTextures.clear();
    g_roofTextures.reserve(5);

    g_roofTextures.push_back(loadRoofTexture("images/roof_1.dds"));
    g_roofTextures.push_back(loadRoofTexture("images/roof_2.dds"));
    g_roofTextures.push_back(loadRoofTexture("images/roof_3.dds"));
    g_roofTextures.push_back(loadRoofTexture("images/roof_4.dds"));
    g_roofTextures.push_back(loadRoofTexture("images/roof_5.dds"));

    g_roofTextures.erase(
        std::remove_if(
            g_roofTextures.begin(), g_roofTextures.end(),
            [](const osg::ref_ptr<osg::StateSet>& t) { return !t.valid(); }),
        g_roofTextures.end());

    if (g_roofTextures.empty())
        std::cout << "[ROOF] WARN: brak zaladowanych tekstur dachow (dds)\n";


    // one more default material to handle walls with the same shader as roof
    {
        osg::Image* image = new osg::Image;
        image->allocateImage(1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE);
        *(osg::Vec4ub*)image->data() = osg::Vec4ub(0xFF, 0xFF, 0xFF, 0xFF);
        osg::StateSet* pss = new osg::StateSet;
        osg::Texture2D* fakeTex = new osg::Texture2D(image);
        fakeTex->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::REPEAT);
        fakeTex->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::REPEAT);
        fakeTex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::NEAREST);
        fakeTex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::NEAREST);
        pss->setTextureAttribute(0, fakeTex, osg::StateAttribute::ON);
        osg::Program* program = new osg::Program;
        program->addShader(new osg::Shader(osg::Shader::VERTEX, vertSource));
        program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragSource));
        pss->setAttribute(program);
        pss->addUniform(new osg::Uniform("diffuseMap", 0));
        pss->setMode(GL_CULL_FACE, osg::StateAttribute::ON);
        g_roofTextures.push_back(pss);
    }


    // 5) Extrusion
    std::cout << "[BUILDINGS] Extruding buildings...\n";
    parse_meta_data(buildings_model.get());

    // 6) Zapis cache
    std::cout << "[BUILDINGS] Zapisuje cache: " << cachePath.string() << "\n";
    bool ok = osgDB::writeNodeFile(*buildings_model, cachePath.string());
    std::cout << "[BUILDINGS] writeNodeFile -> " << (ok ? "OK" : "FAIL")
              << "\n";

    return buildings_model.release();
}
