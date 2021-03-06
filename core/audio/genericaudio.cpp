// Generic audio stream handler
// Author: Max Schwarz <Max@x-quadraht.de>

#include "genericaudio.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
}

#define DEBUG 0
#define LOG_PREFIX "[AUDIO]"
#include <common/log.h>
#include <string.h>

const int BUFSIZE = 10 * 1024 * 1024;

static AVCodec* findCodec(AVCodecID id, AVSampleFormat fmt)
{
	for(AVCodec* p = av_codec_next(0); p; p = av_codec_next(p))
	{
		if(p->id != id || (!p->encode && !p->encode2))
			continue;
		
		for(const AVSampleFormat* f = p->sample_fmts; *f != -1; ++f)
		{
			if(*f == fmt)
				return p;
		}
	}
	
	return 0;
}

GenericAudio::GenericAudio(AVStream* stream)
 : StreamHandler(stream)
 , m_outputErrorCount(0)
 , m_saved_samples(0)
{
}

GenericAudio::~GenericAudio()
{
	av_free(m_cutout_buf);
	av_free(m_cutin_buf);
}

int GenericAudio::init()
{
	AVCodec* codec = avcodec_find_decoder(stream()->codec->codec_id);
	
	if(!codec)
		return error("Could not find decoder");
	
	if(avcodec_open2(stream()->codec, codec, 0) != 0)
		return error("Could not open decoder");
	
	// avcodec_find_decoder does not take sample_fmt into account,
	// so we have to find the decoder ourself...
	AVCodec* encoder = findCodec(
		stream()->codec->codec_id,
		stream()->codec->sample_fmt
	);
	
	if(!encoder)
		return error("Could not find encoder");
	
	outputStream()->disposition = stream()->disposition;
	av_dict_copy(&outputStream()->metadata, stream()->metadata, 0);
	
	outputStream()->codec = avcodec_alloc_context3(encoder);
	avcodec_copy_context(outputStream()->codec, stream()->codec);
	
	if(avcodec_open2(outputStream()->codec, encoder, 0) != 0)
		return error("Could not open encoder");
	
	// Allocate sample buffer
	m_cutout_buf = (int16_t*)av_malloc(BUFSIZE);
	m_cutin_buf = (int16_t*)av_malloc(BUFSIZE);
	if(!m_cutout_buf || !m_cutin_buf)
		return error("Could not allocate sample buffer");
	
	m_nc = cutList().nextCutPoint(0);
	m_cutout = m_nc->direction == CutPoint::IN;
	
	return 0;
}

int GenericAudio::handlePacket(AVPacket* packet)
{
	packet->pts = pts_rel(packet->pts);
	int64_t current_time = packet->pts;
	
	if(m_nc && current_time + packet->duration > m_nc->time
		&& m_nc->direction == CutPoint::OUT
		&& current_time < m_nc->time)
	{
		log_debug("%'10lld: Packet across the cut-out point", current_time);
		
		int frame_size = BUFSIZE;
		if(avcodec_decode_audio3(stream()->codec, m_cutout_buf, &frame_size, packet) < 0)
			return error("Could not decode audio stream");
		
		int64_t total_samples = frame_size / sizeof(int16_t);
		int64_t needed_time = m_nc->time - current_time;
		int64_t needed_samples = av_rescale(needed_time, total_samples, packet->duration);
		
		log_debug("%'10lld: taking %lld of %lld samples", current_time, needed_samples, total_samples);
		
		m_saved_samples = needed_samples;
		
		return 0;
	}
	
	if(m_nc && current_time + packet->duration > m_nc->time
		&& m_nc->direction == CutPoint::IN
		&& current_time < m_nc->time)
	{
		log_debug("%'10lld: Packet across cut-in point", current_time);
		
		int frame_size = BUFSIZE;
		if(avcodec_decode_audio3(stream()->codec, m_cutin_buf, &frame_size, packet) < 0)
			return error("Could not decode audio stream");
		
		int64_t total_samples = frame_size / sizeof(int16_t);
		int64_t time_off = m_nc->time - current_time;
		int64_t needed_time = packet->duration - time_off;
		int64_t sample_off = av_rescale(time_off, total_samples, packet->duration);
		int64_t needed_samples = total_samples - sample_off;
		
		log_debug("%'10lld: taking %lld of %lld samples", current_time, needed_samples, total_samples);
		memcpy(m_cutin_buf, m_cutout_buf, sample_off);
		
		if(sample_off < m_saved_samples)
			log_warning("Dropping %lld samples to preserve packet flow",
				m_saved_samples - sample_off
			);
		else
		{
			log_warning("Inserting %lld silence samples to preserve packet flow",
				sample_off - m_saved_samples
			);
			for(int i = m_saved_samples; i < sample_off; ++i)
				m_cutin_buf[i] = 0;
		}
		
		int bytes = avcodec_encode_audio(outputStream()->codec, packet->data, packet->size, m_cutin_buf);
		
		if(bytes < 0)
			return error("Could not encode audio frame");
		
		packet->size = bytes;
	}
	
	if(m_nc && current_time > m_nc->time
		&& !m_cutout && m_nc->direction == CutPoint::OUT)
	{
		m_cutout = true;
		int64_t cutout_time = m_nc->time;
		m_nc = cutList().nextCutPoint(current_time);
		
		log_debug("CUT-OUT at %'10lld", current_time);
		
		if(m_nc)
			setTotalCutout(m_nc->time - (cutout_time - totalCutout()));
		else
		{
			log_debug("No next cutpoint, deactivating...");
			setActive(false);
		}
	}
	
	if(m_nc && current_time >= m_nc->time
		&& m_cutout && m_nc->direction == CutPoint::IN)
	{
		log_debug("CUT-IN at %'10lld", current_time);
		m_cutout = false;
		m_nc = cutList().nextCutPoint(current_time);
	}
	
	if(!m_cutout)
	{
		if(writeInputPacket(packet) != 0)
		{
			if(++m_outputErrorCount > 50)
			{
				return error("Could not write input packet");
			}
		}
		else
		{
			m_outputErrorCount = 0;
		}
	}
	
	return 0;
}

REGISTER_STREAM_HANDLER(CODEC_ID_AC3, GenericAudio)
REGISTER_STREAM_HANDLER(CODEC_ID_MP2, GenericAudio)

