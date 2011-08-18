// Cutpoint model
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef CUTPOINT_H
#define CUTPOINT_H

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QTextStream>

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
		
		void writeTo(QIODevice* device) const;
		bool readFrom(QIODevice* device);
	signals:
		void aboutToInsert(int idx);
		void inserted(int idx);
		void reset();
	private:
		QList<CutPoint> m_list;
};

QTextStream& operator<<(QTextStream& stream, const CutPoint& point);
QTextStream& operator>>(QTextStream& stream, CutPoint& point);

#endif // CUTPOINT_H
