// AC3 audio stream handler
// Author: Max Schwarz <Max@x-quadraht.de>

#include "ac3.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#define DEBUG 1
#define LOG_PREFIX "[AC3]"
#include "../log.h"

static AVCodec* findCodec(CodecID id, SampleFormat fmt)
{
	for(AVCodec* p = av_codec_next(0); p; p = av_codec_next(p))
	{
		if(p->id != id)
			continue;
		
		for(const SampleFormat* f = p->sample_fmts; *f != -1; ++f)
		{
			if(*f == fmt)
				return p;
		}
	}
	
	return 0;
}

AC3::AC3(AVStream* stream): StreamHandler(stream)
{
}

AC3::~AC3()
{
}

int AC3::init()
{
	// avcodec_find_decoder does not take sample_fmt into account,
	// so we have to find the decoder ourself...
	AVCodec* codec = findCodec(
		stream()->codec->codec_id,
		stream()->codec->sample_fmt
	);
	
	if(!codec)
		return error("Could not find decoder");
	
	if(avcodec_open2(stream()->codec, codec, 0) != 0)
		return error("Could not open decoder");
	
	m_nc = cutList().nextCutPoint(0);
	m_cutout = m_nc->direction == CutPoint::IN;
	
	return 0;
}

int AC3::handlePacket(AVPacket* packet)
{
	int64_t current_time = packet->pts;
	
	if(m_nc && current_time > m_nc->time
		&& !m_cutout && m_nc->direction == CutPoint::OUT)
	{
		m_cutout = true;
		m_nc = cutList().nextCutPoint(current_time);
		
		if(m_nc)
			setTotalCutout(m_nc->time - (current_time - totalCutout()));
		else
			setActive(false);
	}
	
	if(m_nc && current_time >= m_nc->time
		&& m_cutout && m_nc->direction == CutPoint::IN)
	{
		m_cutout = false;
		m_nc = cutList().nextCutPoint(current_time);
	}
	
	if(!m_cutout)
		writeInputPacket(packet);
	
	return 0;
}

REGISTER_STREAM_HANDLER(CODEC_ID_AC3, AC3)

