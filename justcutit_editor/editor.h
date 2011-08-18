// JustCutIt editor
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef EDITOR_H
#define EDITOR_H

#include <QtGui/QWidget>

#include "cutpointlist.h"
#include "cutpointmodel.h"

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
	public slots:
		void loadFile();
		void pause();
		
		void seek_nextFrame(bool display = true);
		void seek_time(float seconds, bool display = true);
		void seek_timeExact(float seconds, bool display = true);
		void seek_timeExactBefore(float seconds, bool display = true);
		void seek_plus1Second();
		void seek_plus30Sec();
		void seek_prevFrame();
		void seek_minus1Second();
		void seek_minus30Sec();
		void seek_slider(int value);
		
		void cut_cut(CutPoint::Direction dir);
		void cut_cutOutHere();
		void cut_cutInHere();
		void cut_pointActivated(QModelIndex idx);
		void cut_openList();
		void cut_saveList();
	private:
		Ui_Editor* m_ui;
		
		AVFormatContext* m_stream;
		
		int m_videoID;
		AVCodecContext* m_videoCodecCtx;
		AVCodec* m_videoCodec;
		
		AVFrame* m_frameBuffer[NUM_FRAMES];
		int64_t m_frameTimestamps[NUM_FRAMES];
		int m_frameIdx;
		int m_headFrame;
		int m_timeStampStart;
		float m_videoTimeBase;
		bool m_fullBuffer;
		
		CutPointList m_cutPoints;
		CutPointModel m_cutPointModel;
		
		void readFrame(bool needKeyFrame = false);
		void displayCurrentFrame();
		float frameTime(int idx = -1);
		void resetBuffer();
		void initBuffer();
};

#endif
