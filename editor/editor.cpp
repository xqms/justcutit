// JustCutIt editor
// Author: Max Schwarz <Max@x-quadraht.de>

#include "editor.h"

#include <QtGui/QVBoxLayout>

#include <QtCore/QTimer>
#include <QtCore/QFile>
#include <QtGui/QImage>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include "ui_editor.h"
#include "gldisplay.h"
#include "io_http.h"

#define DEBUG 0
#define LOG_PREFIX "[editor]"
#include <common/log.h>

#include <stdio.h>

Editor::Editor(QWidget* parent)
 : QWidget(parent)
 , m_frameIdx(0)
 , m_headFrame(0)
 , m_cutPointModel(&m_cutPoints)
{
	m_ui = new Ui_Editor;
	m_ui->setupUi(this);
	
	for(int i = 0; i < NUM_FRAMES; ++i)
		m_frameBuffer[i] = avcodec_alloc_frame();
	
	connect(m_ui->nextButton, SIGNAL(clicked()), SLOT(seek_nextFrame()));
	connect(m_ui->nextSecondButton, SIGNAL(clicked()), SLOT(seek_plus1Second()));
	connect(m_ui->next30SecButton, SIGNAL(clicked()), SLOT(seek_plus30Sec()));
	connect(m_ui->prevButton, SIGNAL(clicked()), SLOT(seek_prevFrame()));
	connect(m_ui->prevSecondButton, SIGNAL(clicked()), SLOT(seek_minus1Second()));
	connect(m_ui->prev30SecButton, SIGNAL(clicked()), SLOT(seek_minus30Sec()));
	
	connect(m_ui->timeSlider, SIGNAL(sliderMoved(int)), SLOT(seek_slider(int)));
	
	connect(m_ui->cutOutButton, SIGNAL(clicked()), SLOT(cut_cutOutHere()));
	connect(m_ui->cutInButton, SIGNAL(clicked()), SLOT(cut_cutInHere()));
	
	m_ui->cutPointView->setModel(&m_cutPointModel);
	m_ui->cutPointView->setRootIndex(QModelIndex());
	
	connect(m_ui->cutPointView, SIGNAL(activated(QModelIndex)), SLOT(cut_pointActivated(QModelIndex)));
	
	QStyle* style = QApplication::style();
	
	m_ui->cutlistOpenButton->setIcon(QIcon::fromTheme("document-open"));
	m_ui->cutlistSaveButton->setIcon(QIcon::fromTheme("document-save"));
	m_ui->cutlistDelItemButton->setIcon(QIcon::fromTheme("list-remove"));
	
	connect(m_ui->cutlistOpenButton, SIGNAL(clicked()), SLOT(cut_openList()));
	connect(m_ui->cutlistSaveButton, SIGNAL(clicked()), SLOT(cut_saveList()));
	connect(m_ui->cutlistDelItemButton, SIGNAL(clicked()), SLOT(cut_deletePoint()));
	
	m_ui->timeSlider->setList(&m_cutPoints);
}

Editor::~Editor()
{
	for(int i = 0; i < NUM_FRAMES; ++i)
		av_free(m_frameBuffer[i]);
}

void Editor::takeIndexFile(IndexFile* file)
{
	m_indexFile = file;
}

void Editor::autoDetectIndexFile()
{
	IndexFileFactory factory;
	m_indexFile = factory.detectIndexFile(
		m_stream, m_filename.toAscii().constData()
	);
}

int Editor::loadFile(const QString& filename)
{
	if(filename.isNull())
	{
		m_filename = QFileDialog::getOpenFileName(
			this,
			"Open file",       // caption
			QString(),         // dir
			"TS files (*.ts)"  // filter
		);
	}
	else
		m_filename = filename;
	
	if(m_filename.isNull())
		return false;
	
// 	m_stream = avformat_alloc_context();
// 	m_stream->pb = io_http_create(filename);
	m_stream = 0;
	
	if(avformat_open_input(&m_stream, m_filename.toAscii().constData(), NULL, NULL) != 0)
		return error("Could not open input stream");
	
	if(avformat_find_stream_info(m_stream, NULL) < 0)
		return error("Could not find stream information");
	
	av_dump_format(m_stream, 0, m_filename.toAscii().constData(), false);
	
	m_videoCodecCtx = 0;
	for(int i = 0; i < m_stream->nb_streams; ++i)
	{
		AVStream* stream = m_stream->streams[i];
		
		if(stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			m_videoID = i;
			m_videoCodecCtx = stream->codec;
			break;
		}
	}
	
	if(!m_videoCodecCtx)
		return error("Could not find video stream");
	
	// Try to decode as fast as possible
	m_videoCodecCtx->flags2 |= CODEC_FLAG2_FAST;
	m_videoCodecCtx->flags2 |= CODEC_FLAG2_FASTPSKIP;
	m_videoCodecCtx->flags |= CODEC_FLAG_LOW_DELAY;
	m_videoCodecCtx->skip_loop_filter = AVDISCARD_ALL;
	
	m_videoCodec = avcodec_find_decoder(m_videoCodecCtx->codec_id);
	if(!m_videoCodec)
		return error("Unsupported video codec");
	
	if(avcodec_open2(m_videoCodecCtx, m_videoCodec, NULL) < 0)
		return error("Could not open video codec");
	
	initBuffer();
	resetBuffer();
	
	m_timeStampStart = m_frameTimestamps[0];
	
	m_videoTimeBase_q = m_stream->streams[m_videoID]->time_base;
	m_videoTimeBase = av_q2d(m_videoTimeBase_q);
	
	log_debug("File duration is % 5.2fs\n", (float)m_stream->duration / AV_TIME_BASE);
	m_ui->timeSlider->setMaximum(m_stream->duration / AV_TIME_BASE);
	
	if(!m_indexFile)
		log_debug("No index file present.\n");
	
	int w = m_videoCodecCtx->width;
	int h = m_videoCodecCtx->height;
	
	m_ui->videoWidget->setSize(w, h);
	m_ui->cutVideoWidget->setSize(w, h);
	
	displayCurrentFrame();
	
	return 0;
}

void Editor::readFrame(bool needKeyFrame)
{
	AVPacket packet;
	AVFrame frame;
	int frameFinished;
	bool gotKeyFramePacket = false;
	
	while(av_read_frame(m_stream, &packet) == 0)
	{
		if(packet.stream_index != m_videoID)
			continue;
		
		if(needKeyFrame && !gotKeyFramePacket)
		{
			if(packet.flags & AV_PKT_FLAG_KEY)
				gotKeyFramePacket = true;
			else
				continue;
		}
		
		if(avcodec_decode_video2(m_videoCodecCtx, &frame, &frameFinished, &packet) < 0)
		{
			error("Could not decode packet");
			return;
		}
		
		if(!frameFinished)
			continue;
		
		if(m_videoCodecCtx->pix_fmt != PIX_FMT_YUV420P)
		{
			error("Pixel format %d is unsupported.", m_videoCodecCtx->pix_fmt);
			return;
		}
		
		if(needKeyFrame && !frame.key_frame)
		{
			av_free_packet(&packet);
			continue;
		}
		
		m_frameTimestamps[m_headFrame] = packet.dts;
		av_picture_copy(
			(AVPicture*)m_frameBuffer[m_headFrame],
			(AVPicture*)&frame,
			PIX_FMT_YUV420P,
			m_videoCodecCtx->width,
			m_videoCodecCtx->height
		);
		
		m_frameBuffer[m_headFrame]->pict_type = frame.pict_type;
		
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
		m_frameBuffer[m_frameIdx]
	);
	
	m_ui->frameTypeLabel->setText(QString::number(m_frameBuffer[m_frameIdx]->pict_type));
	m_ui->timeStampLabel->setText(QString("%1s").arg(frameTime(m_frameIdx), 7, 'f', 4));
	m_ui->rawPTSLabel->setText(QString("%1").arg(m_frameTimestamps[m_frameIdx]));
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

void Editor::seek_nextFrame(bool display)
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
	
	if(display)
		displayCurrentFrame();
}

float Editor::frameTime(int idx)
{
	if(idx == -1)
		idx = m_frameIdx;
	
	return m_videoTimeBase *
		(m_frameTimestamps[idx] - m_timeStampStart);
}

void Editor::seek_time(float seconds, bool display)
{
	int64_t ts_rel = seconds / m_videoTimeBase;
	int64_t ts = m_timeStampStart + ts_rel;
	int64_t min_ts = ts - 2.0 / m_videoTimeBase;
	int64_t max_ts = ts;
	
	int64_t pts_base = av_rescale_q(ts_rel, m_videoTimeBase_q, AV_TIME_BASE_Q);
	loff_t byte_offset = (loff_t)-1;
	
	avcodec_flush_buffers(m_videoCodecCtx);
	
	// If we got an index file, use it
	if(m_indexFile)
		byte_offset = m_indexFile->bytePositionForPTS(pts_base);
	
	if(byte_offset != (loff_t)-1)
	{
		if(avformat_seek_file(m_stream, -1, 0, byte_offset, byte_offset, AVSEEK_FLAG_BYTE) < 0)
			byte_offset = (loff_t)-1;
	}
	
	// Fallback to binary search
	if((byte_offset == (loff_t)-1)
		&& avformat_seek_file(m_stream, m_videoID, min_ts, ts, max_ts, 0) < 0)
	{
		error("could not seek");
		return;
	}
	
	resetBuffer();
	
	if(display)
		displayCurrentFrame();
}

void Editor::seek_timeExact(float seconds, bool display)
{
	for(int i = 0; i == 0 || frameTime() >= seconds - 0.5; ++i)
		seek_time(seconds - 1.0 * i, false);
	
	if(seconds - frameTime() > 5.0)
	{
		fprintf(stderr, "WARNING: Big gap: dest is %f, frameTime is %f\n",
			seconds, frameTime());
	}
	
	while(frameTime() < seconds - 0.002)
		seek_nextFrame(false);
	
	if(display)
		displayCurrentFrame();
}

void Editor::seek_timeExactBefore(float seconds, bool)
{
	seek_timeExact(seconds, false);
	
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
	int w = m_videoCodecCtx->width;
	int h = m_videoCodecCtx->height;
	
	for(int i = 0; i < NUM_FRAMES; ++i)
	{
		avpicture_fill(
			(AVPicture*)m_frameBuffer[i],
			(uint8_t*)av_malloc(avpicture_get_size(
				PIX_FMT_YUV420P,
				w, h
			)),
			PIX_FMT_YUV420P,
			w, h
		);
	}
}

void Editor::seek_slider(int value)
{
	float time = value;
	
	seek_time(time);
}

void Editor::cut_cut(CutPoint::Direction dir)
{
	int w = m_videoCodecCtx->width;
	int h = m_videoCodecCtx->height;
	
	AVFrame* frame = avcodec_alloc_frame();
	
	avpicture_fill(
		(AVPicture*)frame,
		(uint8_t*)av_malloc(avpicture_get_size(
			PIX_FMT_YUV420P,
			w, h
		)),
		PIX_FMT_YUV420P,
		w, h
	);
	
	av_picture_copy(
		(AVPicture*)frame,
		(AVPicture*)m_frameBuffer[m_frameIdx],
		PIX_FMT_YUV420P,
		w, h
	);
	
	int64_t pts = av_rescale_q(
		m_frameTimestamps[m_frameIdx],
		m_videoTimeBase_q,
		AV_TIME_BASE_Q
	);
	
	int num = m_cutPoints.addCutPoint(frameTime(), dir, frame, pts);
	QModelIndex idx = m_cutPointModel.idxForNum(num);
	m_ui->cutPointView->setCurrentIndex(idx);
	cut_pointActivated(idx);
}

void Editor::cut_cutOutHere()
{
	cut_cut(CutPoint::CUT_OUT);
}

void Editor::cut_cutInHere()
{
	cut_cut(CutPoint::CUT_IN);
}

void Editor::cut_pointActivated(QModelIndex idx)
{
	CutPoint* point = m_cutPointModel.cutPointForIdx(idx);
	
	m_ui->cutVideoWidget->paintFrame(point->img);
	if(fabs(frameTime() - point->time) > 0.005)
	{
		seek_timeExactBefore(point->time);
		seek_nextFrame();
	}
}

void Editor::cut_openList()
{
	QString filename = QFileDialog::getOpenFileName(
		this,
		"Open cutlist",
		QString(),
		"Cutlists (*.cut)"
	);
	
	if(filename.isNull())
		return;
	
	QFile file(filename);
	if(!file.open(QIODevice::ReadOnly))
	{
		QMessageBox::critical(this, "Error", "Could not open cutlist file");
		return;
	}
	
	if(!m_cutPoints.readFrom(&file))
	{
		QMessageBox::critical(this, "Error", "Cutlist file is damaged");
		return;
	}
	
	file.close();
	
	// Generate images
	int w = m_videoCodecCtx->width;
	int h = m_videoCodecCtx->height;
	
	for(int i = 0; i < m_cutPoints.count(); ++i)
	{
		CutPoint& p = m_cutPoints.at(i);
		
		int64_t stream_pts = av_rescale_q(
			p.pts,
			AV_TIME_BASE_Q,
			m_videoTimeBase_q
		);
		p.time = m_videoTimeBase * (stream_pts - m_timeStampStart);
		
		log_debug("CutPoint %d has stream PTS %10lld", i, stream_pts);
		
		seek_timeExact(p.time, false);
		
		p.img = avcodec_alloc_frame();
		avpicture_fill(
			(AVPicture*)p.img,
			(uint8_t*)av_malloc(avpicture_get_size(
				PIX_FMT_YUV420P,
				w, h
			)),
			PIX_FMT_YUV420P,
			w, h
		);
		
		av_picture_copy(
			(AVPicture*)p.img,
			(AVPicture*)m_frameBuffer[m_frameIdx],
			PIX_FMT_YUV420P,
			w, h
		);
	}
	
	if(m_cutPoints.count())
	{
		QModelIndex first = m_cutPointModel.idxForNum(0);
		m_ui->cutPointView->setCurrentIndex(first);
		cut_pointActivated(first);
	}
}

void Editor::cut_saveList()
{
	QString filename = QFileDialog::getSaveFileName(
		this,
		"Open cutlist",
		QString(),
		"Cutlists (*.cut)"
	);
	
	if(filename.isNull())
		return;
	
	QFile file(filename);
	if(!file.open(QIODevice::WriteOnly))
	{
		QMessageBox::critical(this, "Error", "Could not open output file");
		return;
	}
	
	m_cutPoints.writeTo(&file);
	
	file.close();
}

void Editor::cut_deletePoint()
{
	QModelIndex idx = m_ui->cutPointView->currentIndex();
	if(!idx.isValid())
		return;
	
	m_cutPointModel.removeRow(idx.row());
}

#include "editor.moc"
