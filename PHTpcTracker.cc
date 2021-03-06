/*!
 *  \file       PHTpcTracker.cc
 *  \brief      
 *  \author     Dmitry Arkhipkin <arkhipkin@gmail.com>
 */

#include "externals/kdfinder.hpp"
#include "PHTpcTracker.h"
#include "PHTpcSeedFinder.h"
#include "PHTpcTrackFollower.h"
#include "PHTpcVertexFinder.h"
#include "PHTpcEventExporter.h"
#include "PHTpcLookup.h"

#include <fun4all/Fun4AllReturnCodes.h>
#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>
#include <phool/PHNode.h>
#include <phool/PHNodeIterator.h>
#include <phool/PHObject.h>
#include <phool/getClass.h>
#include <phool/phool.h>
#include <phool/PHLog.h>

#include <phfield/PHField.h>
#include <phfield/PHFieldUtility.h>
#include <phfield/PHFieldConfigv2.h>
#include <trackbase/TrkrClusterContainer.h>
#include <CLHEP/Units/SystemOfUnits.h>

// GenFit
#include "Fitter.h"
#include <phgeom/PHGeomUtility.h>
#include <GenFit/GFRaveTrackParameters.h>
#include <GenFit/GFRaveVertex.h>
#include <GenFit/GFRaveVertexFactory.h>

PHTpcTracker::PHTpcTracker(const std::string& name) 
	: SubsysReco(name),
//		mSeedFinder(nullptr), mTrackFollower(nullptr),
//		mVertexFinder(nullptr), mEventExporter(nullptr), mLookup(nullptr),
		mFitter(nullptr), mTGeoManager(nullptr), mField(nullptr), mB(1.4),
		mEnableVertexing(false), mEnableJsonExport(false)
{
//	if ( !mSeedFinder ) {
		mSeedFinder = new PHTpcSeedFinder();
//	}
//	if ( !mTrackFollower ) {
		mTrackFollower = new PHTpcTrackFollower();
//	}
//	if ( !mVertexFinder ) {
		mVertexFinder = new PHTpcVertexFinder();
//	}
//	if ( !mEventExporter ) {
		mEventExporter = new PHTpcEventExporter();
//	}
//	if ( !mLookup ) {
		mLookup = new PHTpcLookup();
//	}
}

PHTpcTracker::~PHTpcTracker() {
	delete mSeedFinder;
	delete mTrackFollower;
	delete mVertexFinder;
	delete mEventExporter;
	delete mLookup;
	delete mFitter;
	delete mField;
}

int PHTpcTracker::process_event(PHCompositeNode* topNode)
{
	LOG_INFO("tracking.PHTpcTracker.process_event") << "---- process event started -----";

// ----- magnetic field -----
	if ( !mField ) {
		mField = getMagField( topNode, mB );
	}

// ----- Setup Geometry and Fitter -----
	// FIXME: get TGeoManager only once per file?
	if ( !mTGeoManager ) {
		mTGeoManager = PHGeomUtility::GetTGeoManager(topNode);
		LOG_ERROR_IF("tracking.PHTpcTracker.process_event", !mTGeoManager) << "Cannot find TGeoManager, track propagation will fail";
	}

	if ( !mFitter && mField && mTGeoManager ) {
		mFitter = new PHGenFit2::Fitter( mTGeoManager, mField );
	}

	// ---- timer -----
	auto tracking_timer_start = std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now().time_since_epoch() ).count();

// ----- Seed finding -----
	TrkrClusterContainer* cluster_map = findNode::getClass<TrkrClusterContainer>(topNode, "TRKR_CLUSTER");
	std::vector<kdfinder::TrackCandidate<double>*> candidates;
	candidates = mSeedFinder->findSeeds( cluster_map, mB );
	LOG_INFO("tracking.PHTpcTracker.process_event") << "SeedFinder produced " << candidates.size() << " track seeds";

// ----- Track Following -----
	mLookup->init( cluster_map );
	std::vector<PHGenFit2::Track*> gtracks;
	gtracks = mTrackFollower->followTracks( cluster_map, candidates, mField, mLookup, mFitter );
	LOG_INFO("tracking.PHTpcTracker.process_event") << "TrackFollower reconstructed " << gtracks.size() << " tracks";

	// ---- timer -----
	auto tracking_timer_stop = std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now().time_since_epoch() ).count();

	auto timediff = (tracking_timer_stop - tracking_timer_start) / 1000.0;
	auto tracks_per_second = gtracks.size() / timediff;
	LOG_INFO("tracking.PHTpcTracker.process_event") << "Track Seeding + Track Following took " << timediff << " seconds, " << tracks_per_second << " tracks per second";

// ----- Vertex Reco -----
	std::vector<genfit::GFRaveVertex*> vertices;

	if ( mEnableVertexing ) {
		vertices = mVertexFinder->findVertices( gtracks );
		LOG_INFO("tracking.PHTpcTracker.process_event") << "VertexFinder reconstructed " << vertices.size() << " vertices";
		for ( int i = 0, ilen = vertices.size(); i < ilen; i++ ) {
			TVector3 pos = vertices[i]->getPos();
			LOG_INFO_IF("tracking.PHTpcTracker.process_event", i == 0 ) << "vertex has " << vertices[i]->getNTracks() << " tracks"
				<< ", pos: " << pos.X() << ", " << pos.Y() << ", " << pos.Z();
			LOG_DEBUG_IF("tracking.PHTpcTracker.process_event", i != 0 ) << "vertex has " << vertices[i]->getNTracks() << " tracks"
				<< ", pos: " << pos.X() << ", " << pos.Y() << ", " << pos.Z();
		}
	}

// ----- Event Export ----- 
	if ( mEnableJsonExport ) {
		//std::time_t now = std::time(nullptr);
		auto now = std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now().time_since_epoch() ).count();
		std::string filename = std::string("event-hits-gtracks-") + std::to_string(now) + std::string(".json");
		mEventExporter->exportEvent( cluster_map, gtracks, mB, filename );
		LOG_INFO("tracking.PHTpcTracker.process_event") << "EventExporter dumped hits and tracks to json file";
	}

// ----- cleanup -----

	// candidates cleanup
	for ( auto it = candidates.begin() ; it != candidates.end(); ++it ) { delete (*it); }

	// genfit tracks cleanup
	for ( auto it = gtracks.begin() ; it != gtracks.end(); ++it ) { delete (*it); }

	// hit lookup cleanup
	mLookup->clear();

	// rave vertices cleanup
	for ( auto it = vertices.begin() ; it != vertices.end(); ++it ) { delete (*it); }

	LOG_INFO("tracking.PHTpcTracker.process_event") << "---- process event finished -----";

	return Fun4AllReturnCodes::EVENT_OK;
}

PHField* PHTpcTracker::getMagField( PHCompositeNode* topNode, double& B ) {
	PHField* field = nullptr;
	PHFieldConfigv2 bconfig( 0, 0, B );
	// ----- check file -----
	PHField* field_file = PHFieldUtility::GetFieldMapNode(nullptr, topNode);
	if ( field_file ) {
		const double point[] = { 0, 0, 0, 0 }; // x,y,z,t
		double Bx = 0, By = 0, Bz = 0;
		double Bfield[] = {std::numeric_limits<double>::signaling_NaN(),
                     std::numeric_limits<double>::signaling_NaN(),
                     std::numeric_limits<double>::signaling_NaN(),
                     std::numeric_limits<double>::signaling_NaN(),
                     std::numeric_limits<double>::signaling_NaN(),
                     std::numeric_limits<double>::signaling_NaN()};
		field_file->GetFieldValue(point, Bfield);
		Bx = Bfield[0];
		By = Bfield[1];
		Bz = Bfield[2];
		B = Bz / CLHEP::tesla;
		LOG_DEBUG("tracking.PHTpcTracker.process_event") << "Importing B field from file, Bx,By,Bz Tesla = " << Bx / CLHEP::tesla << "," << By / CLHEP::tesla << "," << Bz / CLHEP::tesla;
		bconfig.set_field_mag_z( B );
	} else {
		LOG_WARN("tracking.PHTpcTracker.process_event") << "No field found in file, using default Bz value = " << B << " Tesla";
	}
	field = PHFieldUtility::BuildFieldMap( &bconfig, 1 );
	return field;
}

void PHTpcTracker::set_seed_finder_options( double maxdistance1, double tripletangle1, size_t minhits1,
            double maxdistance2, double tripletangle2, size_t minhits2, size_t nthreads ) {
	mSeedFinder->set_options( maxdistance1, tripletangle1, minhits1,
		maxdistance2, tripletangle2, minhits2, nthreads );
}

void PHTpcTracker::set_seed_finder_optimization_remove_loopers( bool opt, double minr, double maxr ) {
	mSeedFinder->set_optimization_remove_loopers( opt, minr, maxr );
}

void PHTpcTracker::set_track_follower_optimization_helix( bool opt ) {
	mTrackFollower->set_optimization_helix( opt );
}

void PHTpcTracker::set_track_follower_optimization_precise_fit( bool opt ) {
	mTrackFollower->set_optimization_precise_fit( opt );
}

