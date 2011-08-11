// JustCutIt editor
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef EDITOR_H
#define EDITOR_H

#include <QtGui/QWidget>

const int NUM_FRAMES = 60;

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

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
		
		void seek_nextFrame();
		void seek_time(float seconds);
		void seek_timeExactBefore(float seconds);
		void seek_plus1Second();
		void seek_plus30Sec();
		void seek_prevFrame();
		void seek_minus1Second();
		void seek_minus30Sec();
		void seek_slider(int value);
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
		
		void readFrame(bool needKeyFrame = false);
		void displayCurrentFrame();
		float frameTime(int idx = -1);
		void resetBuffer();
		void initBuffer();
};

#endif
