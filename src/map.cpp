#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>

#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osgGA/StateSetManipulator>
#include <osgGA/AnimationPathManipulator>
#include <osgGA/TerrainManipulator>
#include <osgGA/SphericalManipulator>

#include <osgGA/Device>

#include <iostream>

#include "common.h"

#include "camera_manip.cpp"

using namespace osg;

osg::ref_ptr<osgViewer::Viewer> viewer;
osg::ref_ptr<osg::EllipsoidModel> ellipsoid;

int main(int argc, char** argv)
{
    // use an ArgumentParser object to manage the program arguments.
    osg::ArgumentParser arguments(&argc,argv);

    arguments.getApplicationUsage()->setApplicationName(arguments.getApplicationName());
    arguments.getApplicationUsage()->setDescription(arguments.getApplicationName()+" is the standard OpenSceneGraph example which loads and visualises 3d models.");
    arguments.getApplicationUsage()->setCommandLineUsage(arguments.getApplicationName()+" [options] filename ...");
    arguments.getApplicationUsage()->addCommandLineOption("--image <filename>","Load an image and render it on a quad");
    arguments.getApplicationUsage()->addCommandLineOption("--dem <filename>","Load an image/DEM and render it on a HeightField");
    arguments.getApplicationUsage()->addCommandLineOption("--login <url> <username> <password>","Provide authentication information for http file access.");
    arguments.getApplicationUsage()->addCommandLineOption("-p <filename>","Play specified camera path animation file, previously saved with 'z' key.");
    arguments.getApplicationUsage()->addCommandLineOption("--speed <factor>","Speed factor for animation playing (1 == normal speed).");
    arguments.getApplicationUsage()->addCommandLineOption("--device <device-name>","add named device to the viewer");
    arguments.getApplicationUsage()->addCommandLineOption("--stats","print out load and compile timing stats");

    ellipsoid = new osg::EllipsoidModel;
    viewer = new osgViewer::Viewer (arguments);

    unsigned int helpType = 0;
    if ((helpType = arguments.readHelpType()))
    {
        arguments.getApplicationUsage()->write(std::cout, helpType);
        return 1;
    }

    // report any errors if they have occurred when parsing the program arguments.
    if (arguments.errors())
    {
        arguments.writeErrorMessages(std::cout);
        return 1;
    }

    if (arguments.argc()<=1)
    {
        arguments.getApplicationUsage()->write(std::cout,osg::ApplicationUsage::COMMAND_LINE_OPTION);
        return 1;
    }

    bool printStats = arguments.read("--stats");

    std::string url, username, password;
    while(arguments.read("--login",url, username, password))
    {
        osgDB::Registry::instance()->getOrCreateAuthenticationMap()->addAuthenticationDetails(
            url,
            new osgDB::AuthenticationDetails(username, password)
        );
    }

    std::string device;
    while(arguments.read("--device", device))
    {
        osg::ref_ptr<osgGA::Device> dev = osgDB::readRefFile<osgGA::Device>(device);
        if (dev.valid())
        {
            viewer->addDevice(dev);
        }
    }

    std::string file_path;
    {
        if (!arguments.read("-path", file_path))
        {
            std::cout << arguments.getApplicationName() << ": please provide database path (-path [path])" << std::endl;
            return 0;
        }
    }

    // set up the camera manipulators.
    {
        osg::ref_ptr<osgGA::KeySwitchMatrixManipulator> keyswitchManipulator = new osgGA::KeySwitchMatrixManipulator;

        keyswitchManipulator->addMatrixManipulator( '1', "GoogleMaps", new GoogleMapsManipulator());
        keyswitchManipulator->addMatrixManipulator( '2', "Trackball", new osgGA::TrackballManipulator() );
        keyswitchManipulator->addMatrixManipulator( '3', "Flight", new osgGA::FlightManipulator() );
        keyswitchManipulator->addMatrixManipulator( '4', "Drive", new osgGA::DriveManipulator() );
        keyswitchManipulator->addMatrixManipulator( '5', "Terrain", new osgGA::TerrainManipulator() );
        keyswitchManipulator->addMatrixManipulator( '6', "Orbit", new osgGA::OrbitManipulator() );
        keyswitchManipulator->addMatrixManipulator( '7', "FirstPerson", new osgGA::FirstPersonManipulator() );
        keyswitchManipulator->addMatrixManipulator( '8', "Spherical", new osgGA::SphericalManipulator() );

        std::string pathfile;
        double animationSpeed = 1.0;
        while(arguments.read("--speed",animationSpeed) ) {}
        char keyForAnimationPath = '8';
        while (arguments.read("-p",pathfile))
        {
            osgGA::AnimationPathManipulator* apm = new osgGA::AnimationPathManipulator(pathfile);
            if (apm && !apm->getAnimationPath()->empty())
            {
                apm->setTimeScale(animationSpeed);

                unsigned int num = keyswitchManipulator->getNumMatrixManipulators();
                keyswitchManipulator->addMatrixManipulator( keyForAnimationPath, "Path", apm );
                keyswitchManipulator->selectMatrixManipulator(num);
                ++keyForAnimationPath;
            }
        }

        viewer->setCameraManipulator( keyswitchManipulator.get() );
    }

    // add the state manipulator
    viewer->addEventHandler( new osgGA::StateSetManipulator(viewer->getCamera()->getOrCreateStateSet()) );

    // add the thread model handler
    viewer->addEventHandler(new osgViewer::ThreadingHandler);

    // add the window size toggle handler
    viewer->addEventHandler(new osgViewer::WindowSizeHandler);

    // add the stats handler
    viewer->addEventHandler(new osgViewer::StatsHandler);

    // add the help handler
    viewer->addEventHandler(new osgViewer::HelpHandler(arguments.getApplicationUsage()));

    // add the record camera path handler
    viewer->addEventHandler(new osgViewer::RecordCameraPathHandler);

    // add the LOD Scale handler
    viewer->addEventHandler(new osgViewer::LODScaleHandler);

    // add the screen capture handler
    viewer->addEventHandler(new osgViewer::ScreenCaptureHandler);


    osg::ElapsedTime elapsedTime;
    if (printStats)
    {
        double loadTime = elapsedTime.elapsedTime_m();
        std::cout<<"Load time "<<loadTime<<"ms"<<std::endl;

        viewer->getStats()->collectStats("compile", true);
    }


    /////////////////////////////////////////////////////////////////////
    //////////////////////////////////// CREATE MAP SCENE ///////////////
    /////////////////////////////////////////////////////////////////////

    osg::MatrixTransform * root = new osg::MatrixTransform;
    osg::Matrixd ltw;
    osg::BoundingBox wbb;
    osg::ref_ptr<osg::Node> land_model = process_landuse(ltw, wbb, file_path);
    root->setMatrix(ltw);
    root->addChild(land_model);

    osg::ref_ptr<osg::Node> water_model = process_water(ltw, file_path);
    root->addChild(water_model);

    osg::ref_ptr<osg::Node> roads_model = process_roads(ltw, file_path);
    root->addChild(roads_model);

    osg::ref_ptr<osg::Node> buildings_model = process_buildings(ltw, file_path);
    root->addChild(buildings_model);

    osg::ref_ptr<osg::Node> labels_model = process_labels(ltw, file_path);
    root->addChild(labels_model);

    osg::Vec3d wtrans = wbb.center();
    wtrans.normalize();
    viewer->setLightingMode(osg::View::LightingMode::SKY_LIGHT);
    viewer->getLight()->setPosition(osg::Vec4(wtrans[0], wtrans[1], wtrans[2], 0.f));
    viewer->getLight()->setDirection(osg::Vec3(wtrans[0], wtrans[1], wtrans[2]));
    viewer->getLight()->setAmbient(osg::Vec4(0.2f,0.2f,0.2f,1.0f));
    viewer->getLight()->setDiffuse(osg::Vec4(0.8f,0.8f,0.8f,1.0f));
    viewer->getLight()->setSpecular(osg::Vec4(0.5f,0.5f,0.5f,1.0f));

    // any option left unread are converted into errors to write out later.
    arguments.reportRemainingOptionsAsUnrecognized();

    // report any errors if they have occurred when parsing the program arguments.
    if (arguments.errors())
    {
        arguments.writeErrorMessages(std::cout);
        return 1;
    }

    viewer->setSceneData(root);

    viewer->realize();

    while(!viewer->done())
    {
        viewer->frame();
    }

    return 0;

}
