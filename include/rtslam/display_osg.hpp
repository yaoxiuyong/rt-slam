/**
 * \file display_osg.hpp
 * \date 22/03/2012
 * \author Paul Molodowitch
 * File defining a display architecture for OpenSceneGraph in a QT window
 * \ingroup rtslam
 */

#ifndef DISPLAY_OSG__HPP_
#define DISPLAY_OSG__HPP_

#include "jafarConfig.h"

#ifdef HAVE_OSG

#define HAVE_DISP_OSG

#include <osgViewer/Viewer>
#include <osg/PositionAttitudeTransform>

#include "rtslam/display.hpp"


namespace jafar {
namespace rtslam {
namespace display {

	class WorldOsg;
	class MapOsg;
	class RobotOsg;
	class SensorOsg;
	class LandmarkOsg;
	class ObservationOsg;
	
	class ViewerOsg: public Viewer<WorldOsg,MapOsg,RobotOsg,SensorOsg,LandmarkOsg,ObservationOsg,boost::variant<int*> >
	{
		public:
			typedef Viewer<WorldOsg,MapOsg,RobotOsg,SensorOsg,LandmarkOsg,ObservationOsg,boost::variant<int*> > BaseViewerClass;

		public:
			double ellipsesScale;
			static const double DEFAULT_ELLIPSES_SCALE;

		protected:
			osg::ref_ptr<osgViewer::Viewer> _viewer;
		private:
			osg::ref_ptr<osg::Group> _root;

		public:
			// some configuration parameters
			ViewerOsg(double _ellipsesScale = DEFAULT_ELLIPSES_SCALE);
			void render();
			osg::ref_ptr<osg::Group> root();
	};

	class OsgViewerHolder
	{
	protected:
		ViewerOsg *viewerOsg;
	public:
		OsgViewerHolder(ViewerAbstract *_viewer):
			viewerOsg(PTR_CAST<ViewerOsg*>(_viewer))
		{}
	};

	class OsgGroupHolder : public OsgViewerHolder
	{
	protected:
		osg::ref_ptr<osg::Group> group;
	public:
		OsgGroupHolder(ViewerAbstract *_viewer):
			OsgViewerHolder(_viewer)
		{
			group = new osg::Group;
			group->setDataVariance(osg::Object::DYNAMIC);
			viewerOsg->root()->addChild(group);
		}

		~OsgGroupHolder()
			{
					viewerOsg->root()->removeChild(group);
			}

			unsigned int numShapes()
			{
					return group->getNumChildren();
			}

			void clearShapes()
			{
					group->removeChildren(0, group->getNumChildren());
			}
	};

	class WorldOsg : public WorldDisplay, public OsgViewerHolder
	{
		public:
			WorldOsg(ViewerAbstract *_viewer, rtslam::WorldAbstract *_slamWor, WorldDisplay *garbage);
			void bufferize() {}
			void render() {}
	};



	class MapOsg : public MapDisplay, public OsgViewerHolder
	{
		protected:
			// bufferized data
			jblas::vec poseQuat;
		public:
			MapOsg(ViewerAbstract *_viewer, rtslam::MapAbstract *_slamMap, WorldOsg *_dispWorld);
			void bufferize();
			void render() {}
	};

	class RobotOsg : public RobotDisplay, public OsgGroupHolder
	{
		protected:
			// bufferized data
			jblas::vec poseQuat;
			jblas::sym_mat poseQuatUncert;
		public:
			RobotOsg(ViewerAbstract *_viewer, rtslam::RobotAbstract *_slamRob, MapOsg *_dispMap);
			void bufferize();
			void render();
		protected:
			osg::ref_ptr<osg::PositionAttitudeTransform> makeRobotGeo();
	};

	class SensorOsg : public SensorDisplay, public OsgViewerHolder
	{
		public:
			SensorOsg(ViewerAbstract *_viewer, rtslam::SensorExteroAbstract *_slamSen, RobotOsg *_dispRob);
			void bufferize() {}
			void render() {}
	};

	class LandmarkOsg : public LandmarkDisplay, public OsgGroupHolder
	{
		protected:
			// buffered data
			ObservationAbstract::Events events_;
			jblas::vec state_;
			jblas::sym_mat cov_;
			unsigned int id_;
			LandmarkAbstract::type_enum lmkType_;
		public:
			LandmarkOsg(ViewerAbstract *_viewer, rtslam::LandmarkAbstract *_slamLmk, MapOsg *_dispMap);
			void bufferize();
			void render();

		protected:
			// Some utility functions
			inline colorRGB getColor();
			inline void setColor(osg::ref_ptr<osg::Group> transform, double r, double g, double b);
			inline void setColor(osg::ref_ptr<osg::Group> transform, colorRGB rgb);
			osg::ref_ptr<osg::PositionAttitudeTransform> makeSphere();
			osg::ref_ptr<osg::PositionAttitudeTransform> makeLine();
	};

	class ObservationOsg : public ObservationDisplay, public OsgViewerHolder
	{
		public:
			ObservationOsg(ViewerAbstract *_viewer, rtslam::ObservationAbstract *_slamObs, SensorOsg *_dispSen);
			void bufferize() {}
			void render() {}
	};


}}}

#endif //HAVE_OSG
#endif //DISPLAY_OSG__HPP_

