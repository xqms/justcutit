// Slider showing cut areas
// Author: Max Schwarz <Max@x-quadraht.de>

#include "movieslider.h"

#include "cutpointlist.h"

#include <QtGui/QPainter>
#include <QtGui/QStyleOptionSlider>

MovieSlider::MovieSlider(QWidget* parent)
 : QSlider(parent)
 , m_list(0)
{
}

MovieSlider::~MovieSlider()
{
}

void MovieSlider::setList(CutPointList* list)
{
	m_list = list;
	connect(list, SIGNAL(inserted(int)), SLOT(update()));
	connect(list, SIGNAL(reset()), SLOT(update()));
	connect(list, SIGNAL(removed(int)), SLOT(update()));
}

void MovieSlider::paintRange(float begin, float end, QPainter* painter, const QBrush& brush, QStyleOptionSlider* option)
{
	QRect grooveRect = style()->subControlRect(
		QStyle::CC_Slider,
		option,
		QStyle::SC_SliderGroove,
		this
	);
	QRect sliderRect = style()->subControlRect(
		QStyle::CC_Slider,
		option,
		QStyle::SC_SliderHandle,
		this
	);
	
	int min = grooveRect.x();
	int max = grooveRect.right() - sliderRect.width() + 1;
	
	int pix_begin = QStyle::sliderPositionFromValue(0, maximum(), begin, grooveRect.width());
	int pix_end = QStyle::sliderPositionFromValue(0, maximum(), end, grooveRect.width());
	
	QRect rect(
		grooveRect.x() + pix_begin,
		grooveRect.y(),
		pix_end - pix_begin,
		grooveRect.height()
	);
	
	painter->fillRect(rect, brush);
}

void MovieSlider::paintEvent(QPaintEvent* ev)
{
	QPainter painter(this);
	
	QStyleOptionSlider styleOption;
	initStyleOption(&styleOption);
	
	QBrush inBrush(Qt::green);
	QBrush outBrush(Qt::red);
	
	painter.setPen(Qt::NoBrush);
	
	if(!m_list || !m_list->count())
		return QSlider::paintEvent(ev);
	
	int i = 0;
	CutPoint* p = &m_list->at(i);
	CutPoint* last = 0;
	
	if(p->direction == CutPoint::CUT_IN)
		paintRange(0, p->time, &painter, outBrush, &styleOption);
	
	for(; i < m_list->count(); ++i)
	{
		p = &m_list->at(i);
		
		if(last)
		{
			paintRange(last->time, p->time, &painter,
				(last->direction == CutPoint::CUT_IN) ? inBrush : outBrush,
				&styleOption
			);
		}
		
		last = p;
	}
	
	paintRange(p->time, maximum(), &painter,
		(p->direction == CutPoint::CUT_IN) ? inBrush : outBrush, &styleOption);
	
	QSlider::paintEvent(ev);
}
