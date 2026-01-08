#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>

#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>

#include <osg/Geometry>
#include <osg/Geode>
#include <osg/Texture2D>
#include <osg/Program>
#include <osg/Shader>
#include <osg/Material>
#include <osgSim/ShapeAttribute>
#include <osg/Depth>

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <filesystem>

#include "common.h"

using namespace osg;

static const char* vertSource = R"(
    #version 420 compatibility
    attribute vec3 a_tangent; 
    out vec2 v_texCoord;
    out vec3 v_normal;
    out vec3 v_tangent;
    out vec3 v_ecp;

    void main() {
        v_texCoord = gl_MultiTexCoord0.xy;
        v_ecp = vec3(gl_ModelViewMatrix * gl_Vertex);
        v_normal = gl_Normal;
        v_tangent = a_tangent;

        gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    }
)";

static const char* fragSource = R"(
    #version 420 compatibility
    uniform sampler2D diffuseMap;
    uniform sampler2D normalMap;
    in vec2 v_texCoord;
    in vec3 v_normal;
    in vec3 v_tangent;
    in vec3 v_ecp;

    void main() {
        vec4 texColor = texture2D(diffuseMap, v_texCoord);
        vec3 N = normalize(texture2D(normalMap, v_texCoord).rgb * 2.0 - 1.0);
        vec3 n = normalize(gl_NormalMatrix * v_normal);
        vec3 t = normalize(gl_NormalMatrix * v_tangent);
        vec3 b = cross(n, t);
        mat3 tbn = mat3(t, b, n);

        N = normalize(tbn * N);     
                                                                       
        vec3 L = normalize(gl_LightSource[0].position.xyz);
        vec3 V = normalize(-v_ecp);
        
        float NdotL = max(dot(N, L), 0.0);
        vec3 H = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        
        vec3 ambient = gl_LightSource[0].ambient.rgb * texColor.rgb * 0.3;
        vec3 diffuse = gl_LightSource[0].diffuse.rgb * texColor.rgb * NdotL;
        vec3 specular = gl_LightSource[0].specular.rgb * pow(NdotH, 64.0);

        gl_FragColor = vec4(ambient + diffuse + specular, texColor.a);
    }
)";

inline std::string trim(const std::string& str)
{
    const char* ws = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(ws);
    return str.substr(start, end - start + 1);
}

osg::StateSet* createTextureStateSet(osg::Program* program,
                                     const std::string& diffPath,
                                     const std::string& normPath, int order = 0)
{
    osg::StateSet* ss = new osg::StateSet();
    ss->setAttributeAndModes(program, osg::StateAttribute::ON);
    ss->addUniform(new osg::Uniform("diffuseMap", 0));
    ss->addUniform(new osg::Uniform("normalMap", 1));

    ss->setAttributeAndModes(new osg::Depth(osg::Depth::LESS, 0, 1, false));
    ss->setRenderBinDetails(order, "RenderBin");
    ss->setNestRenderBins(false);

    // wczyt tekstury
    auto loadTexture = [](const std::string& path,
                          bool isNormal) -> osg::Image* {
        osg::Image* img = osgDB::readImageFile(path);
        if (!img)
        {
            std::cout << "Brakujacy: " << path << std::endl;
            img = new osg::Image;
            // jesli brak pliku tworzymy domyslna mape normalnych
            img->allocateImage(1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE);
            if (isNormal)
            {
                unsigned char* d = img->data();
                d[0] = 128; // x = 0
                d[1] = 128; // y = 0
                d[2] = 255; // z = 1
            }
            else
            {
                memset(img->data(), 128, 3);
            }
        }
        return img;
    };

    osg::Image* imgD = loadTexture(diffPath, false);
    osg::Image* imgN = loadTexture(normPath, true);

    auto setupTexture = [](osg::Image* img) -> osg::Texture2D* {
        osg::Texture2D* tex = new osg::Texture2D(img);
        tex->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
        tex->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
        tex->setFilter(osg::Texture::MIN_FILTER,
                       osg::Texture::LINEAR_MIPMAP_LINEAR);
        tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        tex->setMaxAnisotropy(8.0f);
        tex->setUseHardwareMipMapGeneration(true);
        return tex;
    };

    ss->setTextureAttributeAndModes(0, setupTexture(imgD),
                                    osg::StateAttribute::ON);
    ss->setTextureAttributeAndModes(1, setupTexture(imgN),
                                    osg::StateAttribute::ON);

    // ust charakterystyki swiatla
    osg::Material* mat = new osg::Material;
    mat->setColorMode(osg::Material::OFF);
    mat->setAmbient(osg::Material::FRONT_AND_BACK,
                    osg::Vec4(0.2, 0.2, 0.2, 1.0));
    mat->setDiffuse(osg::Material::FRONT_AND_BACK,
                    osg::Vec4(0.8, 0.8, 0.8, 1.0));
    mat->setSpecular(osg::Material::FRONT_AND_BACK,
                     osg::Vec4(0.15, 0.15, 0.15, 1.0));
    mat->setShininess(osg::Material::FRONT_AND_BACK, 32.0f);

    ss->setAttributeAndModes(mat, osg::StateAttribute::ON);

    return ss;
}

class RoadGeneratorVisitor : public osg::NodeVisitor {
public:
    osg::ref_ptr<osg::StateSet> _highwayState;
    osg::ref_ptr<osg::StateSet> _cityState;
    osg::ref_ptr<osg::StateSet> _pathState;

    RoadGeneratorVisitor(osg::StateSet* highway, osg::StateSet* city,
                         osg::StateSet* path)
        : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), _highwayState(highway),
          _cityState(city), _pathState(path)
    {}

    // wydobywamy fclass z drawable
    inline std::string extractFClass(osg::Drawable* drawable)
    {
        if (!drawable) return "";

        osgSim::ShapeAttributeList* sal =
            dynamic_cast<osgSim::ShapeAttributeList*>(drawable->getUserData());

        if (!sal) return "";

        for (unsigned int i = 0; i < sal->size(); ++i)
        {
            const osgSim::ShapeAttribute& attr = (*sal)[i];
            if (attr.getName() == "fclass"
                && attr.getType() == osgSim::ShapeAttribute::STRING)
            {
                const char* str = attr.getString();
                return str ? trim(std::string(str)) : "";
            }
        }
        return "";
    }

    inline float getWidthForFClass(const std::string& fclass)
    {
        static std::unordered_map<std::string, float> mapping;
        auto it = mapping.find(fclass);
        if (it != mapping.end()) return it->second;
        float width = 13.5f;
        if (fclass == "motorway" || fclass == "trunk")
            width = 19.0f;
        else if (fclass == "motorway_link" || fclass == "trunk_link")
            width = 18.0f;
        else if (fclass == "primary")
            width = 17.0f;
        else if (fclass == "secondary" || fclass == "primary_link")
            width = 16.0f;
        else if (fclass == "secondary_link" || fclass == "tertiary")
            width = 14.0f;
        else if (fclass == "residential" || fclass == "living_street"
                 || fclass == "tertiary_link")
            width = 13.0f;
        else if (fclass == "service" || fclass == "unclassified")
            width = 11.0f;
        else if (fclass == "path" || fclass == "footway"
                 || fclass == "cycleway")
            width = 11.5f;
        else if (fclass == "track" || fclass == "steps"
                 || fclass == "pedestrian")
            width = 10.5f;
        mapping[fclass] = width;
        return width;
    }

    inline osg::StateSet* selectStateSetForWidth(float width)
    {
        return (width >= 18.0f) ? _highwayState.get()
            : (width >= 12.0f)  ? _cityState.get()
                                : _pathState.get();
    }

    void apply(osg::Geode& geode) override
    {
        static std::vector<osg::Drawable*> toRemove;
        static std::vector<osg::Drawable*> toAdd;

        toRemove.clear();
        toAdd.clear();

        for (unsigned int i = 0; i < geode.getNumDrawables(); ++i)
        {
            osg::Geometry* lineGeom = geode.getDrawable(i)->asGeometry();
            if (!lineGeom || !lineGeom->getVertexArray()) continue;
            if (lineGeom->getNumPrimitiveSets() == 0) continue;

            GLenum mode = lineGeom->getPrimitiveSet(0)->getMode();
            if (mode != GL_LINE_STRIP && mode != GL_LINE_LOOP
                && mode != GL_LINES)
                continue;

            std::string fclass = extractFClass(lineGeom);
            if (fclass.empty()) continue;

            float width = getWidthForFClass(fclass);
            osg::StateSet* selectedState = selectStateSetForWidth(width);

            osg::Geometry* roadMesh = createRoadMesh(lineGeom, width);
            if (roadMesh)
            {
                roadMesh->setStateSet(selectedState);
                toRemove.push_back(lineGeom);
                toAdd.push_back(roadMesh);
            }
        }

        for (auto&& d : toRemove) geode.removeDrawable(d);
        for (auto&& d : toAdd) geode.addDrawable(d);

        traverse(geode);
    }

    struct RoadProfile
    {
        osg::Vec3 left, right;
        osg::Vec3 tangent;
        float vCoord = 0.0f;
    };

    osg::Geometry* createRoadMesh(osg::Geometry* line, float width)
    {
        osg::Vec3Array* points =
            dynamic_cast<osg::Vec3Array*>(line->getVertexArray());
        if (!points || points->size() < 2) return nullptr;

        const size_t numPoints = points->size();
        const float halfWidth = width * 0.5f;
        const float zOffset = 0.4f;
        const osg::Vec3 up(0, 0, 1);

        // rezerwacja pamieci
        static std::vector<RoadProfile> profiles;
        profiles.clear();

        // wierzcholki
        float currentV = 0.0f;
        for (size_t i = 0; i < numPoints; ++i)
        {
            osg::Vec3 p = (*points)[i];
            p.z() += zOffset;

            const osg::Vec3 normal = up;

            if (i > 0)
            {
                currentV += ((*points)[i] - (*points)[i - 1]).length()
                    * 0.1f; // jak daleko od pocz drogi
            }

            osg::Vec3 sideVector;
            osg::Vec3 tangent;

            if (i == 0)
            {
                // poczatek drogi
                osg::Vec3 d1 = (*points)[i + 1] - (*points)[i];
                d1.normalize();
                sideVector = d1 ^ normal;
                sideVector.normalize();
                tangent = d1;
            }
            else if (i == numPoints - 1)
            {
                // koniec drogi
                osg::Vec3 d1 = (*points)[i] - (*points)[i - 1];
                d1.normalize();
                sideVector = d1 ^ normal;
                sideVector.normalize();
                tangent = d1;
            }
            else
            {
                // srodek drogi - MITRING alg

                osg::Vec3 d1 = (*points)[i] - (*points)[i - 1];
                d1.normalize();

                osg::Vec3 d2 = (*points)[i + 1] - (*points)[i];
                d2.normalize();

                osg::Vec3 r1 = d1 ^ normal;
                osg::Vec3 r2 = d2 ^ normal;

                sideVector = r1 + r2;
                sideVector.normalize();

                tangent = d1 + d2;
                tangent.normalize();

                // korekcja szerokosci dla ostrych zakretow
                float cosAngle = d1 * d2;
                if (cosAngle > -0.99f) // zabezpieczenie przed dziel przez 0
                {
                    float miterScale = 1.0f / sqrt((1.0f + cosAngle) * 0.5f);

                    // ograniczenie maksymalnego rozszerzenia
                    miterScale = std::min(miterScale, 3.0f);

                    sideVector *= miterScale;
                }
            }

            RoadProfile prof;
            prof.left = p + sideVector * halfWidth;
            prof.right = p - sideVector * halfWidth;
            prof.tangent = tangent;
            prof.vCoord = currentV;

            profiles.push_back(prof);
        }

        const size_t numSegments = numPoints - 1;
        const size_t numVertices = numSegments * 2 + 2;

        osg::Vec3Array* vertices = new osg::Vec3Array(numVertices);
        osg::Vec3Array* normals = new osg::Vec3Array(1);
        osg::Vec2Array* texCoords = new osg::Vec2Array(numVertices);
        osg::Vec3Array* tangents = new osg::Vec3Array(numVertices);

        (*normals)[0] = up;

        (*vertices)[0] = profiles[0].left;
        (*vertices)[1] = profiles[0].right;

        (*tangents)[0] = profiles[0].tangent;
        (*tangents)[1] = profiles[0].tangent;

        (*texCoords)[0] = osg::Vec2(0.0f, profiles[0].vCoord);
        (*texCoords)[1] = osg::Vec2(1.0f, profiles[0].vCoord);

        size_t idx = 2;
        for (size_t i = 1; i <= numSegments; ++i)
        {
            const RoadProfile& p0 = profiles[i];

            // trojkat 1: L0, R0, L1
            (*vertices)[idx + 0] = p0.left;
            (*vertices)[idx + 1] = p0.right;

            (*tangents)[idx + 0] = p0.tangent;
            (*tangents)[idx + 1] = p0.tangent;

            (*texCoords)[idx + 0] = osg::Vec2(0.0f, p0.vCoord);
            (*texCoords)[idx + 1] = osg::Vec2(1.0f, p0.vCoord);

            idx += 2;
        }

        osg::Geometry* mesh = new osg::Geometry();
        mesh->setVertexArray(vertices);
        mesh->setNormalArray(normals, osg::Array::BIND_OVERALL);
        mesh->setTexCoordArray(0, texCoords, osg::Array::BIND_PER_VERTEX);
        mesh->setVertexAttribArray(6, tangents, osg::Array::BIND_PER_VERTEX);

        mesh->addPrimitiveSet(
            new osg::DrawArrays(GL_TRIANGLE_STRIP, 0, numVertices));

        // optymalizacja renderowania
        mesh->setDataVariance(osg::Object::STATIC);
        // VBO szybsze niz Display Lists
        mesh->setUseDisplayList(false);
        mesh->setUseVertexBufferObjects(true);

        return mesh;
    }
};

// glowna funkcja
osg::Node* process_roads(osg::Matrixd& ltw, const std::string& file_path)
{
    std::string roads_file_path = file_path + "/gis_osm_roads_free_1.shp";

    std::error_code ec;
    uintmax_t fileSize = std::filesystem::file_size(roads_file_path, ec);
    if (ec)
    {
        std::cout << "Blad: Nie mozna odczytac rozmiaru pliku "
                  << roads_file_path << std::endl;
        return nullptr;
    }

    std::string cacheFileName = "roads_" + std::to_string(fileSize) + ".osgb";

    if (std::filesystem::exists(cacheFileName))
    {
        std::cout << "Znaleziono cache [" << cacheFileName
                  << "]. Pomijam generowanie..." << std::endl;
        return osgDB::readNodeFile(cacheFileName);
    }

    osg::ref_ptr<osg::Node> roads_model =
        osgDB::readRefNodeFile(roads_file_path);
    if (!roads_model) return nullptr;

    ConvertFromGeoProjVisitor<true> cfgp;
    roads_model->accept(cfgp);

    WorldToLocalVisitor ltwv(ltw, true);
    roads_model->accept(ltwv);

    osg::Program* program = new osg::Program;
    program->addShader(new osg::Shader(osg::Shader::VERTEX, vertSource));
    program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragSource));
    program->addBindAttribLocation("a_tangent", 6);

    std::string images_path = "images";
    osg::StateSet* ssHighway =
        createTextureStateSet(program, images_path + "/highway_d.dds",
                              images_path + "/highway_n.dds", -7);
    osg::StateSet* ssCity = createTextureStateSet(
        program, images_path + "/city_d.dds", images_path + "/city_n.dds", -8);
    osg::StateSet* ssPath = createTextureStateSet(
        program, images_path + "/path_d.dds", images_path + "/path_n.dds", -9);

    std::cout << "Generuje geometrie drog (brak cache)..." << std::endl;
    RoadGeneratorVisitor generator(ssHighway, ssCity, ssPath);
    roads_model->accept(generator);

    osgUtil::Optimizer optimizer;
    optimizer.optimize(roads_model, osgUtil::Optimizer::ALL_OPTIMIZATIONS);

    // 4. Zapisz wygenerowany model do pliku cache przed zwróceniem
    std::cout << "Zapisuje cache: " << cacheFileName << std::endl;
    osgDB::writeNodeFile(*roads_model, cacheFileName);

    std::cout << "Przetwarzanie zakonczone\n" << std::endl;
    return roads_model.release();
}