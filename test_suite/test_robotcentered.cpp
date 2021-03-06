/**
 * \file demo_slam.cpp
 *
 * ## Add brief description here ##
 *
 * \author jsola@laas.fr
 * \date 28/04/2010
 *
 *  ## Add a description here ##
 *
 * \ingroup rtslam
 */

#if 0
// TODO should be included in demo_slam

#include <iostream>
#include <boost/shared_ptr.hpp>
#include <time.h>
#include <map>

// jafar debug include
#include "kernel/jafarDebug.hpp"
#include "kernel/jafarTestMacro.hpp"
#include "kernel/timingTools.hpp"
#include "jmath/random.hpp"
#include "jmath/matlab.hpp"

#include "correl/explorer.hpp"
#include "rtslam/quickHarrisDetector.hpp"

#include "jmath/ublasExtra.hpp"

#include "rtslam/rtSlam.hpp"
//#include "rtslam/robotOdometry.hpp"
#include "rtslam/robotCenteredConstantVelocity.hpp"
//#include "rtslam/robotInertial.hpp"
#include "rtslam/sensorPinhole.hpp"
#include "rtslam/landmarkAnchoredHomogeneousPoint.hpp"
//#include "rtslam/landmarkEuclideanPoint.hpp"
#include "rtslam/observationFactory.hpp"
#include "rtslam/observationMakers.hpp"
#include "rtslam/activeSearch.hpp"
#include "rtslam/featureAbstract.hpp"
#include "rtslam/rawImage.hpp"
#include "rtslam/descriptorImagePoint.hpp"
#include "rtslam/dataManagerActiveSearch.hpp"

#include "rtslam/hardwareSensorCameraFirewire.hpp"
//#include "rtslam/hardwareEstimatorMti.hpp"

#include "rtslam/display_qt.hpp"

using namespace jblas;
using namespace jafar;
using namespace jafar::jmath;
using namespace jafar::jmath::ublasExtra;
using namespace jafar::rtslam;
using namespace boost;

typedef ImagePointObservationMaker<ObservationPinHoleEuclideanPoint, SensorPinhole, LandmarkEuclideanPoint,
    SensorAbstract::PINHOLE, LandmarkAbstract::PNT_EUC> PinholeEucpObservationMaker;
typedef ImagePointObservationMaker<ObservationPinholeAnchoredHomogeneousPoint, SensorPinhole,
    LandmarkAnchoredHomogeneousPoint, SensorAbstract::PINHOLE, LandmarkAbstract::PNT_AH> PinholeAhpObservationMaker;

int mode = 0;
std::string dump_path = ".";
time_t rseed;

void demo_slam01_main(world_ptr_t *world) {
	// time
	const unsigned N_FRAMES = 500000;

	// map
	const unsigned MAP_SIZE = 250;

	// robot uncertainties and perturbations
	const double UNCERT_VLIN = .01; // m/s
	const double UNCERT_VANG = .01; // rad/s
	const double PERT_VLIN = .2; // m/s per sqrt(s)
	const double PERT_VANG = 1; // rad/s per sqrt(s)

	// pin-hole:
	const unsigned IMG_WIDTH = 640;
	const unsigned IMG_HEIGHT = 480;
	const double INTRINSIC[4] = { 301.27013,   266.86136,   497.28243,   496.81116 };
	const double DISTORTION[2] = { -0.23193,   0.11306 }; //{-0.27965, 0.20059, -0.14215}; //{-0.27572, 0.28827};
	const double PIX_NOISE = 0.5;

	// lmk management
	const double D_MIN = 0.1;
	const double REPARAM_TH = 0.1;

	// data manager: quick Harris detector
	const unsigned HARRIS_CONV_SIZE = 3;
	const double HARRIS_TH = 10.0;
	const double HARRIS_EDDGE = 3.0;
	const unsigned PATCH_DESC = 45;

	// data manager: zncc matcher
	const unsigned PATCH_SIZE = 15;
	const double MATCH_TH = 0.95;
	const double SEARCH_SIGMA = 2.5;
	const double MAHALANOBIS_TH = 2.5;
	const unsigned N_UPDATES = 20;

	// data manager: active search
	const unsigned GRID_VCELLS = 3;
	const unsigned GRID_HCELLS = 4;
	const unsigned GRID_MARGIN = 11;
	const unsigned GRID_SEPAR = 20;

//	const bool SHOW_PATCH = true;


	// pin-hole parameters in BOOST format
	vec intrinsic = createVector<4> (INTRINSIC);
	vec distortion = createVector<sizeof(DISTORTION)/sizeof(double)> (DISTORTION);
	cout << "D: " << distortion << endl;

	boost::shared_ptr<ObservationFactory> obsFact(new ObservationFactory());
	obsFact->addMaker(boost::shared_ptr<ObservationMakerAbstract>(new PinholeEucpObservationMaker(PATCH_SIZE, D_MIN)));
	obsFact->addMaker(boost::shared_ptr<ObservationMakerAbstract>(new PinholeAhpObservationMaker(PATCH_SIZE, D_MIN, REPARAM_TH)));


	// ---------------------------------------------------------------------------
	// --- INIT ------------------------------------------------------------------
	// ---------------------------------------------------------------------------
	// INIT : 1 map and map-manager, 2 robs, 3 sens and data-manager.
	world_ptr_t worldPtr = *world;
	worldPtr->display_mutex.lock();


	// 1. Create maps.
	map_ptr_t mapPtr(new MapAbstract(MAP_SIZE));
	worldPtr->addMap(mapPtr);
	mapPtr->clear();
	// 1b. Create map manager.
	boost::shared_ptr<MapManager<LandmarkAnchoredHomogeneousPoint, LandmarkEuclideanPoint> > mmPoint(new MapManager<
	    LandmarkAnchoredHomogeneousPoint, LandmarkEuclideanPoint> ());
	mmPoint->linkToParentMap(mapPtr);


	// 2. Create robots.
	robcenteredconstvel_ptr_t robPtr1(new RobotCenteredConstantVelocity(mapPtr));
	robPtr1->setId();
	robPtr1->linkToParentMap(mapPtr);
	robPtr1->state.clear();
	robPtr1->pose.x(quaternion::originFrame());

	double _v[6] = { PERT_VLIN, PERT_VLIN, PERT_VLIN, PERT_VANG, PERT_VANG, PERT_VANG };
	robPtr1->perturbation.clear();
	robPtr1->perturbation.set_std_continuous(createVector<6> (_v));
	robPtr1->setVelocityStd(UNCERT_VLIN,UNCERT_VANG);
	robPtr1->constantPerturbation = false;

	// 3. Create sensors.
	pinhole_ptr_t senPtr11(new SensorPinhole(robPtr1, MapObject::UNFILTERED));
	senPtr11->setId();
	senPtr11->linkToParentRobot(robPtr1);
	senPtr11->state.clear();
	senPtr11->pose.x(quaternion::originFrame());
	senPtr11->params.setImgSize(IMG_WIDTH, IMG_HEIGHT);
	senPtr11->params.setIntrinsicCalibration(intrinsic, distortion, distortion.size());
	senPtr11->params.setMiscellaneous(1.0, 0.1);
//	senPtr11->params.patchSize = -1; // FIXME: See how to propagate patch size properly (obs factory needs it to be in sensor)

	// 3b. Create data manager.
	boost::shared_ptr<ActiveSearchGrid> asGrid(new ActiveSearchGrid(IMG_WIDTH, IMG_HEIGHT, GRID_HCELLS, GRID_VCELLS, GRID_MARGIN, GRID_SEPAR));
	boost::shared_ptr<QuickHarrisDetector> harrisDetector(new QuickHarrisDetector(HARRIS_CONV_SIZE, HARRIS_TH, HARRIS_EDDGE));
	boost::shared_ptr<correl::FastTranslationMatcherZncc> znccMatcher(new correl::FastTranslationMatcherZncc(0.8,0.25));
	boost::shared_ptr<DataManagerActiveSearch<RawImage, SensorPinhole, QuickHarrisDetector, correl::FastTranslationMatcherZncc> > dmPt11(new DataManagerActiveSearch<RawImage,
			SensorPinhole, QuickHarrisDetector, correl::FastTranslationMatcherZncc> ());
	dmPt11->linkToParentSensorSpec(senPtr11);
	dmPt11->linkToParentMapManager(mmPoint);
	dmPt11->setActiveSearchGrid(asGrid);
	dmPt11->setDetector(harrisDetector, PATCH_DESC, PIX_NOISE);
	dmPt11->setMatcher(znccMatcher, PATCH_SIZE, SEARCH_SIGMA, MATCH_TH, MAHALANOBIS_TH, PIX_NOISE);
	dmPt11->setAlgorithmParams(N_UPDATES);
	dmPt11->setObservationFactory(obsFact);

	#ifdef HAVE_VIAM
	viam_hwmode_t hwmode = { VIAM_HWSZ_640x480, VIAM_HWFMT_MONO8, VIAM_HW_FIXED, VIAM_HWFPS_60, VIAM_HWTRIGGER_INTERNAL };
	hardware::hardware_sensorext_ptr_t hardSen11(new hardware::HardwareSensorCameraFirewire("0x00b09d01006fb38f", hwmode, mode, dump_path));
	senPtr11->setHardwareSensor(hardSen11);
	#else
	if (mode == 2)
	{
		hardware::hardware_sensorext_ptr_t hardSen11(new hardware::HardwareSensorCameraFirewire(cv::Size(640,480),dump_path));
		senPtr11->setHardwareSensor(hardSen11);
	}
	#endif

	// Show empty map
	cout << *mapPtr << endl;

	worldPtr->display_mutex.unlock();


	// ---------------------------------------------------------------------------
	// --- LOOP ------------------------------------------------------------------
	// ---------------------------------------------------------------------------
	// INIT : complete observations set
	// loop all sensors
	// loop all lmks
	// create sen--lmk observation
	// Temporal loop

	kernel::Chrono chrono;
	kernel::Chrono total_chrono;
	kernel::Chrono mutex_chrono;
	double max_dt = 0;
	for (; (*world)->t <= N_FRAMES;) {
		bool had_data = false;

			worldPtr->display_mutex.lock();
			// cout << "\n************************************************** " << endl;
			// cout << "\n                 FRAME : " << t << " (blocked "
			//      << mutex_chrono.elapsedMicrosecond() << " us)" << endl;
			chrono.reset();
			int total_match_time = 0;
			int total_update_time = 0;


			// FIRST LOOP FOR MEASUREMENT SPACES - ALL DM
			// foreach robot
			for (MapAbstract::RobotList::iterator robIter = mapPtr->robotList().begin(); robIter != mapPtr->robotList().end(); robIter++) {
				robot_ptr_t robPtr = *robIter;


				// cout << "\n================================================== " << endl;
				// cout << *robPtr << endl;

				// foreach sensor
				for (RobotAbstract::SensorList::iterator senIter = robPtr->sensorList().begin(); senIter
				    != robPtr->sensorList().end(); senIter++) {
					sensor_ptr_t senPtr = *senIter;
					//					cout << "\n________________________________________________ " << endl;
					//					cout << *senPtr << endl;


					// get raw-data
					if (senPtr->acquireRaw() < 0)
						continue;
					else had_data=true;
					//std::cout << chronototal.elapsed() << " has acquired" << std::endl;
//cout << "\nNEW FRAME\n" << endl;
					// move the filter time to the data raw.
					vec u(robPtr->mySize_control()); // TODO put some real values in u.
					fillVector(u, 0.0);
					robPtr->move(u, senPtr->getRaw()->timestamp);

//					cout << *robPtr << endl;

					// foreach dataManager
					for (SensorAbstract::DataManagerList::iterator dmaIter = senPtr->dataManagerList().begin(); dmaIter
					    != senPtr->dataManagerList().end(); dmaIter++) {
						data_manager_ptr_t dmaPtr = *dmaIter;
						dmaPtr->process(senPtr->getRaw());
					} // foreach dataManager

			} // for each sensor
		} // for each robot


			// NOW LOOP FOR STATE SPACE - ALL MM
		if (had_data) {
			(*world)->t++;

			if (robPtr1->dt_or_dx > max_dt) max_dt = robPtr1->dt_or_dx;

			// Output info
//						cout << endl;
//						cout << "dt: " << (int) (1000 * robPtr1->dt_or_dx) << "ms (match "
//						<< total_match_time/1000 << " ms, update " << total_update_time/1000 << "ms). Lmk: [";
//						cout << mmPoint->landmarkList().size() << "] ";
//						for (MapManagerAbstract::LandmarkList::iterator lmkIter =
//								mmPoint->landmarkList().begin(); lmkIter
//								!= mmPoint->landmarkList().end(); lmkIter++) {
//							cout << (*lmkIter)->id() << " ";
//						}



			// TODO foreach mapMgr {manage();}
			for (MapAbstract::MapManagerList::iterator mmIter = mapPtr->mapManagerList().begin(); mmIter
	    != mapPtr->mapManagerList().end(); mmIter++){
				map_manager_ptr_t mapMgr = *mmIter;
				mapMgr->manage();
			}
		} // if had_data

		worldPtr->display_mutex.unlock();
		mutex_chrono.reset();
		if (mode == 2 && had_data) getchar();
	} // temporal loop

//	cout << "time avg(max): " << total_chrono.elapsed()/N_FRAMES << "(" << (int)(1000*max_dt) <<") ms" << endl;
	std::cout << "\nFINISHED ! Press a key to terminate." << std::endl;
	getchar();
} // demo_slam01_main


void demo_slam01_display(world_ptr_t *world) {
	//(*world)->display_mutex.lock();
	static unsigned prev_t = 0;
	qdisplay::qtMutexLock<kernel::FifoMutex>((*world)->display_mutex);
	if ((*world)->t != prev_t)
	{
		prev_t = (*world)->t;
		display::ViewerQt *viewerQt = static_cast<display::ViewerQt*> ((*world)->getDisplayViewer(display::ViewerQt::id()));
		if (viewerQt == NULL) {
			viewerQt = new display::ViewerQt();
			(*world)->addDisplayViewer(viewerQt, display::ViewerQt::id());
		}
		viewerQt->bufferize(*world);
		(*world)->display_mutex.unlock();
		
		viewerQt->render();
	} else
	{
		(*world)->display_mutex.unlock();
	}
}

	void demo_slam01(bool display) {
		world_ptr_t worldPtr(new WorldAbstract());

		rseed = time(NULL);
		if (mode == 1) {
			std::fstream f((dump_path + std::string("/rseed.log")).c_str(), std::ios_base::out);
			f << rseed << std::endl;
			f.close();
		}
		else if (mode == 2) {
			std::fstream f((dump_path + std::string("/rseed.log")).c_str(), std::ios_base::in);
			f >> rseed;
			f.close();
		}
		std::cout << __FILE__ << ":" << __LINE__ << " rseed " << rseed << std::endl;
		rtslam::srand(rseed); // FIXME does not work in multithread...


		// to start with qt display
		const int slam_priority = -20; // needs to be started as root to be < 0
		const int display_priority = 10;
		const int display_period = 100; // ms
		if (display)
		{
			qdisplay::QtAppStart((qdisplay::FUNC)&demo_slam01_display,display_priority,(qdisplay::FUNC)&demo_slam01_main,slam_priority,display_period,&worldPtr);
		}
		else
		{
			kernel::setCurrentThreadPriority(slam_priority);
			demo_slam01_main(&worldPtr);
		}

		JFR_DEBUG("Terminated");
	}

		/**
		 * Function call usage:
		 *
		 * 	demo_slam DISP DUMP PATH
		 *
		 * If you want display, pass a first argument DISP="1" to the executable, otherwise "0".
		 * If you want to dump images, pass a second argument to the executable DUMP="1" and a path where
		 * you want the processed images be saved. If you want to replay the last execution, change 1 to 2
		 *
		 * Example 1: demo_slam 1 1 /home/you/rtslam dumps images to /home/you/rtslam
		 * example 2: demo_slam 1 2 /home/you/rtslam replays the saved sequence
		 * example 3: demo_slam 1 0 /anything does not dump
		 * example 4: demo_slam 0 any /anything does not display nor dump.
		 */
		int main(int argc, const char* argv[])
		{
			bool display = 1;
			if (argc == 4)
			{
				display = atoi(argv[1]);
				mode = atoi(argv[2]);
				dump_path = argv[3];
			}
			else if (argc != 0)
			std::cout << "Usage: demo_slam <display-enabled=1> <image-mode=0> <dump-path=.>" << std::endl;

			demo_slam01(display);
		}
		
#else

#endif
