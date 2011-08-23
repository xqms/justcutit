// Cutpoint model
// Author: Max Schwarz <Max@x-quadraht.de>

#include "cutpointmodel.h"

#include "cutpointlist.h"

#include <QtCore/QDebug>

#include <stdio.h>

CutPointModel::CutPointModel(CutPointList* list)
 : m_list(list)
{
	connect(list, SIGNAL(aboutToInsert(int)), SLOT(aboutToInsert(int)));
	connect(list, SIGNAL(inserted(int)), SLOT(inserted(int)));
	connect(list, SIGNAL(reset()), SLOT(listReset()));
}

CutPointModel::~CutPointModel()
{
}

int CutPointModel::rowCount(const QModelIndex&) const
{
	return m_list->count();
}

void CutPointModel::aboutToInsert(int idx)
{
	beginInsertRows(QModelIndex(), idx, idx);
}

void CutPointModel::inserted(int idx)
{
	endInsertRows();
}

void CutPointModel::listReset()
{
	reset();
}

QVariant CutPointModel::data(const QModelIndex& index, int role) const
{
	int pos = index.row();
	
	if(pos < 0 || pos >= m_list->count())
		return QVariant();
	
	const CutPoint& point = m_list->at(pos);
	
	switch(index.column())
	{
		case 0:
			if(role != Qt::DisplayRole)
				return QVariant();
			
			switch(point.direction)
			{
				case CutPoint::CUT_IN:
					return QString("Cut in at %1").arg(point.time, 7, 'f', 3);
				case CutPoint::CUT_OUT:
					return QString("Cut out at %1").arg(point.time, 7, 'f', 3);
			}
			break;
	}
	
	return QVariant();
}

CutPoint* CutPointModel::cutPointForIdx(QModelIndex idx)
{
	return &m_list->at(idx.row());
}

QModelIndex CutPointModel::idxForNum(int num)
{
	return index(num);
}

int CutPointModel::numForIdx(const QModelIndex& idx)
{
	return idx.row();
}

bool CutPointModel::removeRows(int row, int count, const QModelIndex& parent)
{
	beginRemoveRows(parent, row, row+count-1);
	
	for(; count > 0; --count)
		m_list->remove(row);
	
	endRemoveRows();
}


#include "cutpointmodel.moc"
