// H.264 stream handler
// Author: Max Schwarz <Max@x-quadraht.de>

#include "h264.h"

#define DEBUG 1
#define LOG_PREFIX "[H264]"
#include <common/log.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

// ffmpeg h264 decoder internals
#include <libavutil/imgutils.h>
#include <libavcodec/internal.h>

#define restrict __restrict
#include <libavcodec/dsputil.h>
#undef restrict

#define class class_
#include <libavutil/log.h>
#include <libavcodec/h264.h>
#include <libavcodec/get_bits.h>
#include <libavcodec/golomb.h>
#include <unistd.h>
#undef class
}

const int ENCODE_BUFSIZE = 10 * 1024 * 1024;

static AVPacket copyPacket(const AVPacket& input)
{
	AVPacket ret;
	av_init_packet(&ret);
	ret.data = (uint8_t*)av_malloc(input.size);
	ret.size = input.size;
	
	memcpy(ret.data, input.data, input.size);
	
	ret.flags = input.flags;
	ret.pts = input.pts;
	ret.dts = input.dts;
	ret.stream_index = input.stream_index;
	
	return ret;
}

static const char* tstoa(int64_t ts)
{
	const int BUFSIZE = 50;
	static char buf[BUFSIZE+1];
	
	if(ts == AV_NOPTS_VALUE)
		strcpy(buf, "N/A");
	else
		snprintf(buf, BUFSIZE, "%'5lld", ts);
	
	return buf;
}

static const char* dump_picture(Picture* p)
{
	const int BUFSIZE = 200;
	static char buf[BUFSIZE+1];
	
	int b = 0;
	
	b = snprintf(buf, BUFSIZE,
		"%c f.pts=%5s, frame_num=%3d, key=%d, ref=%c%c, poc=%4hd, ref0 = [",
		av_get_picture_type_char(p->f.pict_type),
		tstoa(p->f.pts), p->frame_num, p->f.key_frame,
		(p->f.reference & PICT_FRAME) ? 'F' : '_',
		(p->f.reference & DELAYED_PIC_REF) ? 'D' : '_',
		p->field_poc[0]
	);
	
	for(int i = 0; i < p->ref_count[0][0]; ++i)
	{
		b += snprintf(buf + b, BUFSIZE - b,
			"%2hd ", p->ref_poc[0][0][i] / 4
		);
	}
	
	b += snprintf(buf + b, BUFSIZE - b, "] ref1 = [");
	
	for(int i = 0; i < p->ref_count[0][1]; ++i)
	{
		b += snprintf(buf + b, BUFSIZE - b,
			"%2hd ", p->ref_poc[0][1][i] / 4
		);
	}
	
	strncpy(buf + b, "]", BUFSIZE - b);
	
	return buf;
}

static void dump_state(H264Context* h)
{
	log_debug("h264 internals:");
	log_debug(" - long_ref:");
	for(int i = 0; i < h->long_ref_count; ++i)
		log_debug("    %2d: %10s", i, dump_picture(h->long_ref[i]));
	
	log_debug(" - ref_list 0:");
	for(int i = 0; i < h->ref_count[0]; ++i)
		log_debug("    %2d: %10s", i, dump_picture(&h->ref_list[0][i]));
	log_debug(" - ref_list 1:");
	for(int i = 0; i < h->ref_count[1]; ++i)
		log_debug("    %2d: %10s", i, dump_picture(&h->ref_list[1][i]));
	
	log_debug(" - s.current_picture: %s", dump_picture(h->s.current_picture_ptr));
	
	log_debug(" - s.picture[]:");
	for(int i = 0; i < h->s.picture_count; ++i)
	{
		const int BUFSIZE = 100;
		char qualifiers[BUFSIZE+1];
		Picture* p = h->s.picture + i;
		
		qualifiers[0] = 0;
		
		if(h->s.current_picture_ptr == p)
			strncat(qualifiers, "(current) ", BUFSIZE);
		if(h->s.next_picture_ptr == p)
			strncat(qualifiers, "(next) ", BUFSIZE);
		if(h->s.last_picture_ptr == p)
			strncat(qualifiers, "(last) ", BUFSIZE);
		
		for(int j = 0; j < 2; ++j)
		{
			for(int k = 0; k < h->ref_count[k]; ++k)
			{
				if(h->ref_list[j][k].poc == p->poc
					&& h->ref_list[j][k].frame_num == p->frame_num
					&& p->f.pict_type != AV_PICTURE_TYPE_NONE)
				{
					const int R_BUFSIZE = 15;
					char buf[R_BUFSIZE+1];
					snprintf(buf, R_BUFSIZE, "(ref_%d_%02d) ", j, k);
					strncat(qualifiers, buf, BUFSIZE);
				}
			}
		}
		
		log_debug("    %2d: %s %s",
			i, dump_picture(p),
			qualifiers
 		);
	}
}

H264::H264(AVStream* stream)
 : StreamHandler(stream)
{
	avcodec_get_frame_defaults(&m_frame);
	
	m_pps.data = 0;
	m_sps.data = 0;
}

H264::~H264()
{
}

int H264::init()
{
	AVCodec* decoder = avcodec_find_decoder(stream()->codec->codec_id);
	if(!decoder)
		return error("Could not find decoder");
	
	if(avcodec_open2(stream()->codec, decoder, NULL) != 0)
		return error("Could not open decoder");
	
	m_h = (H264Context*)stream()->codec->priv_data;
	
	m_codec = avcodec_find_encoder(stream()->codec->codec_id);
	if(!m_codec)
		return error("Could not find encoder");
	
	outputStream()->codec = avcodec_alloc_context3(m_codec);
	avcodec_copy_context(outputStream()->codec, stream()->codec);
	
	outputStream()->sample_aspect_ratio = outputStream()->codec->sample_aspect_ratio;
	outputStream()->codec->thread_type = 0;
	outputStream()->codec->thread_count = 1;
	
	AVCodecContext* ctx = outputStream()->codec;
	ctx->bit_rate = 3 * 500 * 1024;
	ctx->rc_max_rate = 0;
	ctx->rc_buffer_size = 0;
	ctx->gop_size = 40;
	ctx->coder_type = 1;
	ctx->me_cmp = 1;
	ctx->me_range = 16;
	ctx->colorspace = AVCOL_SPC_BT709;
// 	ctx->flags2 |= CODEC_FLAG2_8X8DCT;
	
	m_nc = cutList().nextCutPoint(0);
	m_isCutout = m_nc->direction == CutPoint::IN;
	
	m_startDecodeOffset = av_rescale_q(10, (AVRational){1,1}, stream()->time_base);
	
	m_encodeBuffer = (uint8_t*)av_malloc(ENCODE_BUFSIZE);
	
	m_encoding = false;
	m_decoding = false;
	m_syncing = false;
	m_syncPoint = -1;
	
	return 0;
}

int H264::handlePacket(AVPacket* packet)
{
	int gotFrame;
	int bytes;
	H264Context* h = (H264Context*)stream()->codec->priv_data;
	
	// Transform timestamps to relative timestamps
	packet->dts = pts_rel(packet->dts);
	packet->pts = pts_rel(packet->pts);
	
	if(m_decoding && !m_syncing)
		parseNAL(packet->data, packet->size);
	
	if(!m_decoding)
	{
		if(m_nc && m_nc->time - packet->pts < m_startDecodeOffset)
		{
			log_debug("Switching decoder on at PTS %'10lld (m_nc: %'10lld)",
				packet->pts, m_nc->time);
			m_decoding = true;
			
			avcodec_flush_buffers(stream()->codec);
		}
	}
	
	if(m_decoding)
	{
		if(avcodec_decode_video2(stream()->codec, &m_frame, &gotFrame, packet) < 0)
			return error("Could not decode packet");
	}
	
	if(!m_encoding && m_nc)
	{
		if(m_nc->direction == CutPoint::OUT && m_nc->time < packet->dts)
		{
			m_decoding = false;
			m_isCutout = true;
			
			int64_t current_time = m_nc->time;
			m_nc = cutList().nextCutPoint(packet->dts);
			
			if(m_nc)
				setTotalCutout(m_nc->time - (current_time - totalCutout()));
			else
				setActive(false); // last cutpoint reached
		}
		else if(m_nc->direction == CutPoint::IN && m_nc->time <= packet->dts)
		{
			m_encoding = true;
			m_encFrameCount = 0;
			
			log_debug("Opening encoder for frame with PTS %'10lld", packet->dts);
			if(avcodec_open2(outputStream()->codec, m_codec, NULL) != 0)
				return error("Could not open encoder");
		}
	}
	
	if(m_encoding && m_encFrameCount > 20 && packet->flags & AV_PKT_FLAG_KEY && h->s.current_picture_ptr)
	{
		m_syncing = true;
		m_syncPoint = packet->pts;
		
		log_debug("SYNC: start with keyframe packet PTS %'10lld", m_syncPoint);
// 		log_debug("SYNC: frame_num of first original frame is %d",
// 				h->s.current_picture_ptr->frame_num
// 		);
	}

	if(m_syncing)
	{
		log_debug("decode=%d, gotFrame=%d, keyframe=%d, t=%d", m_decoding, gotFrame, m_frame.key_frame, m_frame.pict_type);
	}
	
	if(m_syncing && gotFrame && m_frame.pict_type == 1)
	{
		log_debug("SYNC: Flushing out encoder");
		// Flush out encoder
		while(1)
		{
			bytes = avcodec_encode_video(
				outputStream()->codec,
				m_encodeBuffer, ENCODE_BUFSIZE,
				NULL
			);
			outputStream()->codec->has_b_frames = 6;
			
			if(!bytes)
				break;
			
			int64_t pts = av_rescale_q(outputStream()->codec->coded_frame->pts,
					outputStream()->codec->time_base, outputStream()->time_base
				);
			
			if(pts + totalCutout() >= m_syncPoint)
			{
				log_debug("SYNC: (encoder) Skipping PTS %'10lld >= sync point %'10lld",
					pts + totalCutout(), m_syncPoint
				);
				continue;
			}
			
			if(writeOutputPacket(m_encodeBuffer, bytes, pts) != 0)
				return error("SYNC: (encoder) Could not write packet");
		}
		avcodec_close(outputStream()->codec);
		
		// Flush out sync buffer
		for(int i = 0; i < m_syncBuffer.size(); ++i)
		{
			AVPacket* packet = &m_syncBuffer[i];
			if(packet->pts < m_syncPoint)
			{
				log_debug("SYNC: (buffer) Skipping PTS %'10lld < sync point %'10lld",
					packet->pts, m_syncPoint
				);
				continue;
			}
			
			if(writeInputPacket(packet) != 0)
				return error("SYNC: (buffer) Could not write packet");
		}
		m_syncBuffer.clear();
		
		m_encoding = false;
		m_isCutout = false;
		m_decoding = false;
		m_syncing = false;
		
		log_debug("SYNC: finished, got keyframe from decoder with PTS %'10lld", packet->dts);
		
		m_nc = cutList().nextCutPoint(packet->dts);
	}
	
	if(m_syncing)
	{
		m_syncBuffer.push_back(copyPacket(*packet));
	}
	
	if(m_encoding && gotFrame)
	{
		setFrameFields(&m_frame, packet->dts - totalCutout());
		
		bytes = avcodec_encode_video(
			outputStream()->codec,
			m_encodeBuffer, ENCODE_BUFSIZE,
			&m_frame
		);
		outputStream()->codec->has_b_frames = 6;
		
		if(bytes)
		{
			writeOutputPacket(
				m_encodeBuffer, bytes,
				av_rescale_q(outputStream()->codec->coded_frame->pts,
					outputStream()->codec->time_base, outputStream()->time_base
				)
			);
			m_encFrameCount++;
		}
	}
	
	if(!m_isCutout && !m_encoding)
	{
		if(m_syncPoint > 0 && packet->pts < m_syncPoint)
		{
			log_debug("COPY: Skipping packet with PTS %'10lld", packet->pts);
			return 0;
		}
		
		if(m_sps.data || m_pps.data)
		{
			int size = packet->size + m_sps.size + m_pps.size;
			uint8_t* buf = (uint8_t*)malloc(size);
			int off = 0;
			
			memcpy(buf + off, m_sps.data, m_sps.size);
			off += m_sps.size;
			memcpy(buf + off, m_pps.data, m_pps.size);
			off += m_pps.size;
			
			memcpy(buf + off, packet->data, packet->size);
			
			writeOutputPacket(buf, size, packet->pts - totalCutout());
			
			free(m_sps.data); m_sps.data = 0;
			free(m_pps.data); m_pps.data = 0;
			free(buf);
			return 0;
		}
		
// 		log_debug("COPY: packet with PTS %'10lld", packet->pts);
		outputStream()->codec->has_b_frames = 6;
		if(writeInputPacket(packet) != 0)
		{
			log_debug("PTS buffer:");
			
			for(int i = 0; i < outputStream()->codec->has_b_frames; ++i)
				log_debug(" %s", tstoa(outputStream()->pts_buffer[i]));
			
			return error("Could not copy input packet (has_b_frames: %d, max_b_frames: %d)",
				outputStream()->codec->has_b_frames, outputStream()->codec->max_b_frames
			);
		}
	}
	
	return 0;
}

void H264::setFrameFields(AVFrame* frame, int64_t pts)
{
	frame->pict_type = AV_PICTURE_TYPE_I;
	frame->key_frame = 0;
	frame->pkt_pts = AV_NOPTS_VALUE;
	frame->pkt_dts = AV_NOPTS_VALUE;
	frame->pts = av_rescale_q(
		pts,
		stream()->time_base, outputStream()->codec->time_base
	);
}

int H264::writeOutputPacket(uint8_t* buf, int size, int64_t pts)
{
	AVPacket packet;
	av_init_packet(&packet);
	
	packet.data = buf;
	packet.size = size;
	packet.pts = pts;
	packet.dts = AV_NOPTS_VALUE;
	packet.flags = AV_PKT_FLAG_KEY;
	
	return av_interleaved_write_frame(outputContext(), &packet);
}

static const int find_startCode(uint8_t* buf, int off, int size)
{
	// Search for start code
	for(; off < size - 4; ++off)
	{
		if(buf[off] == 0 && buf[off+1] == 0 && buf[off+2] == 0 && buf[off+3] == 1)
			return off;
	}
	
	return -1;
}

void H264::parseNAL(uint8_t* buf, int size)
{
	int off = find_startCode(buf, 0, size) + 4;
	int next_start = off;
	
	for(; next_start > 0; off = next_start + 4)
	{
		next_start = find_startCode(buf, off, size);
		
		int type = buf[off] & 0x1F;
		int id_off = (type == NAL_PPS) ? 1 : 4;
		
		if(type != NAL_SPS && type != NAL_PPS)
			continue;
		
		GetBitContext gb;
		init_get_bits(&gb, buf + off + id_off, 8*(size - off - id_off));
		
		int id = get_ue_golomb(&gb);
		int nal_size = (next_start < 0) ? (size - off) : (next_start - off);
		
		switch(type)
		{
			case NAL_SPS:
				if(id >= MAX_SPS_COUNT)
					continue;
				
				free(m_sps.data);
				m_sps.data = (uint8_t*)av_malloc(nal_size+4);
				memcpy(m_sps.data, buf + off - 4, nal_size+4);
				m_sps.size = nal_size+4;
				
				log_debug("NAL_SPS (id=%d)", id);
				break;
			case NAL_PPS:
				if(id >= MAX_PPS_COUNT)
					continue;
				
				free(m_pps.data);
				m_pps.data = (uint8_t*)av_malloc(nal_size+4);
				memcpy(m_pps.data, buf + off - 4, nal_size+4);
				m_pps.size = nal_size+4;
				
				log_debug("NAL_PPS (id=%d)", id);
				break;
		}
	}
}


REGISTER_STREAM_HANDLER(CODEC_ID_H264, H264)
