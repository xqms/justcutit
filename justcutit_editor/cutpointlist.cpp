// Cutpoint model
// Author: Max Schwarz <Max@x-quadraht.de>

#include "cutpointlist.h"

#include <QtCore/qalgorithms.h>

int CutPointList::addCutPoint(const CutPoint& point)
{
	int idx = qLowerBound(m_list, point) - m_list.begin();
	
	emit aboutToInsert(idx);
	m_list.insert(idx, point);
	emit inserted(idx);
	
	return idx;
}

int CutPointList::addCutPoint(float time, CutPoint::Direction dir, AVFrame* img)
{
	CutPoint point;
	point.time = time;
	point.direction = dir;
	point.img = img;
	
	return addCutPoint(point);
}

int CutPointList::lastCutPoint(float time)
{
	int idx = nextCutPoint(time);
	if(idx <= 0)
		return -1;
	return idx - 1;
}

int CutPointList::nextCutPoint(float time)
{
	CutPoint dummy;
	dummy.time = time;
	
	return qLowerBound(m_list, dummy) - m_list.begin();
}

#include "cutpointlist.moc"
