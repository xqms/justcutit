// JustCutIt editor
// Author: Max Schwarz <Max@x-quadraht.de>

#include "editor.h"

#include <QtGui/QVBoxLayout>

#include <QtCore/QTimer>
#include <QtGui/QImage>

#include "ui_editor.h"
#include "gldisplay.h"

#include <stdio.h>

Editor::Editor(QWidget* parent)
 : QWidget(parent)
 , m_frameIdx(0)
 , m_headFrame(0)
{
	m_ui = new Ui_Editor;
	m_ui->setupUi(this);
	
	for(int i = 0; i < NUM_FRAMES; ++i)
		m_frameBuffer[i] = avcodec_alloc_frame();
	
	QTimer::singleShot(0, this, SLOT(loadFile()));
	
	connect(m_ui->nextButton, SIGNAL(clicked()), SLOT(seek_nextFrame()));
	connect(m_ui->nextSecondButton, SIGNAL(clicked()), SLOT(seek_plus1Second()));
	connect(m_ui->next30SecButton, SIGNAL(clicked()), SLOT(seek_plus30Sec()));
	connect(m_ui->prevButton, SIGNAL(clicked()), SLOT(seek_prevFrame()));
	
	connect(m_ui->timeSlider, SIGNAL(sliderMoved(int)), SLOT(seek_slider(int)));
}

Editor::~Editor()
{
	for(int i = 0; i < NUM_FRAMES; ++i)
		av_free(m_frameBuffer[i]);
}

void Editor::loadFile()
{
	if(av_open_input_file(&m_stream, "/home/max/Downloads/Ben Hur.ts", NULL, 0, NULL) != 0)
	{
		fprintf(stderr, "Fatal: Could not open input file\n");
		return;
	}
	
	if(av_find_stream_info(m_stream) < 0)
	{
		fprintf(stderr, "Fatal: Could not find stream information\n");
		return;
	}
	
	dump_format(m_stream, 0, "/home/max/Downloads/Ben Hur.ts", false);
	
	m_videoCodecCtx = 0;
	for(int i = 0; i < m_stream->nb_streams; ++i)
	{
		AVStream* stream = m_stream->streams[i];
		
		if(stream->codec->codec_type == CODEC_TYPE_VIDEO)
		{
			m_videoID = i;
			m_videoCodecCtx = stream->codec;
			break;
		}
	}
	
	if(!m_videoCodecCtx)
	{
		fprintf(stderr, "Fatal: Could not find video stream\n");
		return;
	}
	
	m_videoCodec = avcodec_find_decoder(m_videoCodecCtx->codec_id);
	if(!m_videoCodec)
	{
		fprintf(stderr, "Fatal: Unsupported codec\n");
		return;
	}
	
	if(avcodec_open(m_videoCodecCtx, m_videoCodec) < 0)
	{
		fprintf(stderr, "Fatal: Could not open codec\n");
		return;
	}
	
	readFrame(true);
	
	initBuffer();
	resetBuffer();
	
	m_timeStampStart = m_frameTimestamps[0];
	m_videoTimeBase = av_q2d(m_stream->streams[m_videoID]->time_base);
	
	printf("File duration is % 5.2fs\n", (float)m_stream->duration / AV_TIME_BASE);
	m_ui->timeSlider->setMaximum(m_stream->duration / AV_TIME_BASE);
	
	displayCurrentFrame();
}

void Editor::readFrame(bool needKeyFrame)
{
	AVPacket packet;
	AVFrame frame;
	int frameFinished;
	
	while(av_read_frame(m_stream, &packet) == 0)
	{
		if(packet.stream_index != m_videoID)
			continue;
		
		if(avcodec_decode_video2(m_videoCodecCtx, &frame, &frameFinished, &packet) < 0)
		{
			fprintf(stderr, "Fatal: Could not decode packet\n");
			return;
		}
		
		if(!frameFinished)
			continue;
		
		if(m_videoCodecCtx->pix_fmt != PIX_FMT_YUV420P)
		{
			printf("Fatal: Format %d is unsupported.\n", m_videoCodecCtx->pix_fmt);
			return;
		}
		
		m_frameTimestamps[m_headFrame] = packet.dts;
		av_picture_copy(
			(AVPicture*)m_frameBuffer[m_headFrame],
			(AVPicture*)&frame,
			PIX_FMT_YUV420P,
			m_videoCodecCtx->width,
			m_videoCodecCtx->height
		);
		
		av_free_packet(&packet);
		
		if(!needKeyFrame)
			return;
		
		if(frame.key_frame)
			return;
	}
}

void Editor::displayCurrentFrame()
{
	m_ui->videoWidget->paintFrame(
		m_frameBuffer[m_frameIdx],
		m_videoCodecCtx->width,
		m_videoCodecCtx->height
	);
	
	m_ui->frameTypeLabel->setText(QString::number(m_frameBuffer[m_frameIdx]->key_frame));
	m_ui->timeStampLabel->setText(QString("%1s").arg(frameTime(m_frameIdx), 7, 'f', 4));
	m_ui->headIdxLabel->setText(QString::number(m_headFrame));
	m_ui->frameIdxLabel->setText(QString::number(m_frameIdx));
	
	if(!m_ui->timeSlider->isSliderDown())
	{
		m_ui->timeSlider->blockSignals(true);
		m_ui->timeSlider->setValue((m_frameTimestamps[m_frameIdx] - m_timeStampStart) * m_videoTimeBase);
		m_ui->timeSlider->blockSignals(false);
	}
}

void Editor::pause()
{
}

void Editor::seek_nextFrame()
{
	if(++m_frameIdx == NUM_FRAMES)
		m_frameIdx = 0;
	
	if(m_frameIdx == m_headFrame)
	{
		readFrame();
		if(++m_headFrame == NUM_FRAMES)
		{
			m_headFrame = 0;
			m_fullBuffer = true;
		}
	}
	
	displayCurrentFrame();
}

float Editor::frameTime(int idx)
{
	if(idx == -1)
		idx = m_frameIdx;
	
	return m_videoTimeBase *
		(m_frameTimestamps[idx] - m_timeStampStart);
}

void Editor::seek_time(float seconds)
{
	int ts = m_timeStampStart + seconds / m_videoTimeBase;
	int min_ts = ts - 2.0 / m_videoTimeBase;
	int max_ts = ts;
	
	avcodec_flush_buffers(m_videoCodecCtx);
	
	if(avformat_seek_file(m_stream, m_videoID, min_ts, ts, max_ts, 0) < 0)
	{
		fprintf(stderr, "Fatal: could not seek\n");
		return;
	}
	
	resetBuffer();
	displayCurrentFrame();
}

void Editor::seek_timeExactBefore(float seconds)
{
	for(int i = 0; frameTime() >= seconds; ++i)
		seek_time(seconds - 0.5 * i);
	
	while(frameTime() < seconds)
		seek_nextFrame();
	
	seek_prevFrame();
}

void Editor::resetBuffer()
{
	m_headFrame = 0;
	m_frameIdx = 0;
	m_fullBuffer = false;
	
	readFrame(true);
	m_headFrame++;
}

void Editor::seek_prevFrame()
{
	float time = frameTime();
	
	if(time == 0)
		return;
	
	if(--m_frameIdx < 0 && m_fullBuffer)
		m_frameIdx = NUM_FRAMES - 1;
	
	if(m_frameIdx == m_headFrame || m_frameIdx < 0)
	{
		seek_timeExactBefore(time);
		return;
	}
	
	displayCurrentFrame();
}

void Editor::seek_plus1Second()
{
	seek_time(frameTime() + 1.0);
}

void Editor::seek_plus30Sec()
{
	seek_time(frameTime() + 30.0);
}

void Editor::seek_minus1Second()
{
	seek_time(frameTime() - 1.0);
}

void Editor::seek_minus30Sec()
{
	seek_time(frameTime() - 30.0);
}

void Editor::initBuffer()
{
	for(int i = 0; i < NUM_FRAMES; ++i)
	{
		avpicture_fill(
			(AVPicture*)m_frameBuffer[i],
			(uint8_t*)av_malloc(avpicture_get_size(
				PIX_FMT_YUV420P,
				m_videoCodecCtx->width,
				m_videoCodecCtx->height
			)),
			PIX_FMT_YUV420P,
			m_videoCodecCtx->width,
			m_videoCodecCtx->height
		);
	}
}

void Editor::seek_slider(int value)
{
	float time = value;
	
	printf("seeking to % 3.3f\n", time);
	
	seek_time(time);
	
	printf(" => % 3.3f\n", frameTime());
}

#include "editor.moc"
