// JustCutIt editor
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef EDITOR_H
#define EDITOR_H

#include <QtGui/QWidget>

#include "cutpointlist.h"
#include "cutpointmodel.h"

#include "indexfile.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

const int NUM_FRAMES = 60;

class Ui_Editor;

class Editor : public QWidget
{
	Q_OBJECT
	public:
		Editor(QWidget* parent = 0);
		virtual ~Editor();
		
		inline AVFormatContext* formatContext()
		{ return m_stream; }
		
		void takeIndexFile(IndexFile* file);
		void autoDetectIndexFile();
		
		void proceedTo(const QString& filename);
	public slots:
		int loadFile(const QString& filename = QString::null);
		void pause();
		
		void seek_nextFrame(bool display = true);
		void seek_time(float seconds, bool display = true);
		void seek_timeExact(float seconds, bool display = true);
		void seek_timeExactBefore(float seconds, bool display = true);
		void seek_plus5Frame();
		void seek_plus1Second();
		void seek_plus30Sec();
		void seek_prevFrame(bool display = true);
		void seek_minus5Frame();
		void seek_minus1Second();
		void seek_minus30Sec();
		void seek_slider(int value);
		
		void cut_cut(CutPoint::Direction dir);
		void cut_cutOutHere();
		void cut_cutInHere();
		void cut_pointActivated(QModelIndex idx);
		void cut_deletePoint();
		void cut_openList();
		bool cut_saveList(const QString& filename = QString::null);
		
		void proceed();
	private:
		Ui_Editor* m_ui;
		
		QString m_filename;
		QString m_proceedTo;
		
		AVFormatContext* m_stream;
		
		int m_videoID;
		AVCodecContext* m_videoCodecCtx;
		AVCodec* m_videoCodec;
		
		AVFrame* m_frameBuffer[NUM_FRAMES];
		int64_t m_frameTimestamps[NUM_FRAMES];
		int m_frameIdx;
		int m_headFrame;
		int64_t m_timeStampStart;
		int64_t m_timeStampFirstKey;
		float m_videoTimeBase;
		AVRational m_videoTimeBase_q;
		bool m_fullBuffer;
		
		CutPointList m_cutPoints;
		CutPointModel m_cutPointModel;
		
		IndexFile* m_indexFile;
		
		int64_t m_timeFudge;
		
		void readFrame(bool needKeyFrame = false);
		void displayCurrentFrame();
		float frameTime(int idx = -1);
		void resetBuffer();
		void initBuffer();
		int64_t pts_val(int64_t value) const;
};

#endif
