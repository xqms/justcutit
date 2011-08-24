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

int CutPointList::addCutPoint(float time, CutPoint::Direction dir, AVFrame* img, int64_t pts)
{
	CutPoint point;
	point.time = time;
	point.direction = dir;
	point.img = img;
	point.pts = pts;
	
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

void CutPointList::remove(int idx)
{
	m_list.removeAt(idx);
	emit removed(idx);
}

// SERIALIZING

bool CutPointList::readFrom(QIODevice* device)
{
	m_list.clear();
	
	while(!device->atEnd())
	{
		QString line = device->readLine().trimmed();
		
		if(line.isEmpty())
			continue;
		
		QTextStream stream(&line);
		CutPoint point;
		stream >> point;
		
		if(stream.status() != QTextStream::Ok)
			return false;
		
		addCutPoint(point);
	}
	
	emit reset();
	
	return true;
}

void CutPointList::writeTo(QIODevice* device) const
{
	QTextStream stream(device);
	
	foreach(const CutPoint& p, m_list)
		stream << p << "\n";
}

QTextStream& operator<<(QTextStream& stream, const CutPoint& point)
{
	stream << point.pts
	       << " "
	       << ((point.direction == CutPoint::CUT_IN) ? "IN" : "OUT");
	return stream;
}

QTextStream& operator>>(QTextStream& stream, CutPoint& point)
{
	QString inout;
	
	stream >> point.pts >> inout;
	
	if(inout == "IN")
		point.direction = CutPoint::CUT_IN;
	else if(inout == "OUT")
		point.direction = CutPoint::CUT_OUT;
	else
		qFatal("Invalid direction specification: '%s' for cutpoint %f",
			inout.toAscii().constData(), point.time);
	
	return stream;
}

#include "cutpointlist.moc"
