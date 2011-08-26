// Cutpoint model
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef CUTPOINTMODEL_H
#define CUTPOINTMODEL_H

#include <QtCore/QAbstractListModel>

class CutPointList;
class CutPoint;

class CutPointModel : public QAbstractListModel
{
	Q_OBJECT
	public:
		CutPointModel(CutPointList* list);
		virtual ~CutPointModel();
		
		virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;
		virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
		
		virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex());
		
		CutPoint* cutPointForIdx(QModelIndex idx);
		QModelIndex idxForNum(int num);
		int numForIdx(const QModelIndex& idx);
	public slots:
		 void aboutToInsert(int idx);
		 void inserted(int idx);
		 void listReset();
	private:
		CutPointList* m_list;
};

#endif // CUTPOINTMODEL_H
