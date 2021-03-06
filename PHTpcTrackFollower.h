/*!
 *  \file       PHTpcTrackFollower.h
 *  \brief      
 *  \author     Dmitry Arkhipkin <arkhipkin@gmail.com>
 */

#ifndef PHTPCTRACKFOLLOWER_H_
#define PHTPCTRACKFOLLOWER_H_

#include "PHTpcLookup.h"
#include "externals/kdfinder.hpp"

#include <phfield/PHField.h>
#include "Track.h"
#include <trackbase/TrkrClusterContainer.h>
#include "Fitter.h"
#include "SpacepointMeasurement2.h"
#include <GenFit/KalmanFitter.h>

/// \class PHTpcTrackFollower
///
/// \brief 
///
class PHTpcTrackFollower
{
	public:
		PHTpcTrackFollower();
		virtual ~PHTpcTrackFollower() {}

		std::vector<PHGenFit2::Track*> followTracks( TrkrClusterContainer* cluster_map,
				std::vector< kdfinder::TrackCandidate<double>* >& candidates,
				PHField* B, PHTpcLookup* lookup, PHGenFit2::Fitter* fitter );

		PHGenFit2::Track* propagateTrack( kdfinder::TrackCandidate<double>* candidate,
				PHTpcLookup* lookup, PHGenFit2::Fitter* fitter );

		void set_optimization_helix( bool opt = true ) { mOptHelix = opt; }
		void set_optimization_precise_fit( bool opt = true ) { mOptPreciseFit = opt; }

	protected:
		PHGenFit2::Track* candidate_to_genfit( kdfinder::TrackCandidate<double>* candidate );
		PHGenFit::SpacepointMeasurement2* hit_to_measurement( std::vector<double>& hit );
		int get_track_layer( PHGenFit2::Track* track, int dir = 1 );
		int get_track_layer( genfit::Track* gftrack, int dir = 1 );
		std::pair<genfit::MeasuredStateOnPlane*,double> get_projected_coordinate( genfit::Track* gftrack, int dir, double radius );
		std::pair<genfit::MeasuredStateOnPlane*,double> get_projected_coordinate( genfit::Track* gftrack, int dir, const TVector3& point );
		int followTrack( PHGenFit2::Track* track, PHTpcLookup* lookup, PHGenFit2::Fitter* fitter, int dir = 1 );
	
	private:
		bool mOptHelix;
		bool mOptPreciseFit;

};

#endif /* PHTPCTRACKFOLLOWER_H_ */
