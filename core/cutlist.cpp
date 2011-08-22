// Cutlist representations
// Author: Max Schwarz <Max@x-quadraht.de>

#include "cutlist.h"

extern "C"
{
#include <libavutil/mathematics.h>
}

#include <algorithm>

const CutPoint* CutPointList::nextCutPoint(int64_t time) const
{
	CutPoint dummy;
	dummy.time = time;
	
	const_iterator it = std::upper_bound(begin(), end(), dummy);
	
	if(it == end())
		return 0;
	
	return &(*it);
}

CutPointList CutPointList::rescale(AVRational from, AVRational to) const
{
	CutPointList ret(*this);
	
	for(int i = 0; i < size(); ++i)
		ret[i].time = av_rescale_q(ret[i].time, from, to);
	
	return ret;
}

