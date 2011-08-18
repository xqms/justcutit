// Slider showing cut areas
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef MOVIESLIDER_H
#define MOVIESLIDER_H

#include <QtGui/QSlider>

class CutPointList;

class MovieSlider : public QSlider
{
	public:
		MovieSlider(QWidget* parent);
		virtual ~MovieSlider();
		
		void setList(CutPointList* list);
		
		virtual void paintEvent(QPaintEvent* ev);
	private:
		CutPointList* m_list;
		
		void paintRange(float begin, float end, QPainter* painter, const QBrush& brush, QStyleOptionSlider* option);
};

#endif // MOVIESLIDER_H
