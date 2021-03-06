/**
 * \file worldAbstract.hpp
 *
 * \date 2010/05/15
 * \author croussil
 *
 * Header file for abstract world
 *
 * \ingroup rtslam
 */

#ifndef WORLDABSTRACT_HPP_
#define WORLDABSTRACT_HPP_

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include "kernel/threads.hpp"
#include "jmath/jblas.hpp"
#include "rtslam/rtSlam.hpp"
#include "rtslam/objectAbstract.hpp"

#include "rtslam/parents.hpp"

namespace jafar {
	/**
	 * Namespace rtslam for real-time slam module.
	 * \ingroup rtslam
	 */
	namespace rtslam {

		// some forward declarations.
		class MapAbstract;
		namespace display { class ViewerAbstract; }


		/** Base class for all world types defined in the module rtslam.
		 *
		 * \author croussil
		 * \ingroup rtslam
		 */
		class WorldAbstract: public ObjectAbstract, public ParentOf<MapAbstract> {

			ENABLE_ACCESS_TO_CHILDREN(MapAbstract,Map,map);
			std::vector<display::ViewerAbstract*> display_viewers;

			public:

				/**
				 * Constructor
				 */
				WorldAbstract(): t(0), display_rendered(true), display_t(-1), slam_blocked(false), exit(false) {}

				/**
				 * Mandatory virtual destructor - Map is used as-is, non-abstract by now
				 */
				virtual ~WorldAbstract() {
				}
				
				//kernel::FifoMutex display_mutex;
				unsigned t;
				//unsigned display_rendered;
				
				bool display_rendered;
				unsigned display_t;
				boost::mutex display_mutex;
				boost::condition_variable display_condition;

				kernel::VariableMutex<bool> slam_blocked;
				kernel::VariableMutex<bool> exit;
				
				void addDisplayViewer(display::ViewerAbstract *viewer, unsigned id);
				display::ViewerAbstract* getDisplayViewer(unsigned id);
		};

	}
}

#endif /* MAPABSTRACT_HPP_ */

