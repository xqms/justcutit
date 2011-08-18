// Cutpoint model
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef CUTPOINT_H
#define CUTPOINT_H

#include <QtCore/QObject>
#include <QtCore/QList>

extern "C"
{
struct AVFrame;
}

struct CutPoint
{
	float time;
	enum Direction {
		CUT_IN,
		CUT_OUT
	} direction;
	AVFrame* img;
	
	inline bool operator<(const CutPoint& rhs) const
	{
		return time < rhs.time;
	}
};

class CutPointList : public QObject
{
	Q_OBJECT
	public:
		int addCutPoint(const CutPoint& point);
		int addCutPoint(float time, CutPoint::Direction dir, AVFrame* img);
		
		int nextCutPoint(float time);
		int lastCutPoint(float time);
		
		inline const CutPoint& operator[](int idx) const
		{ return m_list[idx]; }
		
		inline const CutPoint& at(int idx) const
		{ return m_list[idx]; }
		
		inline CutPoint& at(int idx)
		{ return m_list[idx]; }
		
		inline int count() const
		{ return m_list.count(); }
		
		CutPoint& operator[](int idx);
	signals:
		void aboutToInsert(int idx);
		void inserted(int idx);
	private:
		QList<CutPoint> m_list;
};

#endif // CUTPOINT_H
