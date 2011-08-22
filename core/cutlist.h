// Cutlist representations
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef CUTLIST_H
#define CUTLIST_H

#include <stdint.h>
#include <vector>

extern "C"
{
#include <libavutil/rational.h>
}

/**
 * @brief Input CutPoint
 * 
 * Represents a cut point that is specified in seconds
 **/
struct CutPoint
{
	/**
	 * @brief Cut point time
	 * 
	 * Specifies the cut time PTS in AV_TIME_BASE units. In both directions (see direction)
	 * time is specified inclusively */
	int64_t time;
	
	//! @brief Cut semantics
	enum Direction
	{
		IN, //!< Cut in here
		OUT //!< Cut out here
	} direction; //!< Cut semantics
	
	inline bool operator<(const CutPoint& right) const
	{ return time < right.time; }
};

class CutPointList : public std::vector<CutPoint>
{
	public:
		const CutPoint* nextCutPoint(int64_t time) const;
		
		CutPointList rescale(AVRational from, AVRational to) const;
};

#endif // CUTLIST_H
