// MPEG-2 video stream handler
// Author: Max Schwarz <Max@x-quadraht.de>

#include "mp2v.h"

#include <stdarg.h>
#include <unistd.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

// Debug - log packet information near cutpoints
#define DEBUG 0

// Debug - dump packets during cut-in to pwd
#define DUMP_CUTIN_PACKETS 0

#define LOG_PREFIX "[MP2V]"
#include <common/log.h>

const int OUTPUT_BUFFER_SIZE = 10 * 1024 * 1024;

#if DUMP_CUTIN_PACKETS
static void dump_cutin_packet(const char* ext, int64_t pts, AVPacket* packet)
{
	char filename[40];
	snprintf(filename, 40, "%lld.%s", pts, ext);
	
	FILE* out = fopen(filename, "w");
	fwrite(packet->data, packet->size, 1, out);
	fclose(out);
}
#else
inline void dump_cutin_packet(const char* ext, int64_t pts, AVPacket* packet)
{
}
#endif

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

static bool bufferContainsPTS(const MP2V::PacketBuffer& buffer, int64_t pts)
{
	for(MP2V::PacketBuffer::const_iterator it = buffer.begin();
		it != buffer.end(); ++it)
	{
		if(it->pts == pts)
			return true;
	}
	
	return false;
}

static void writePPM(const char* filename, AVPicture* src, PixelFormat src_fmt, int w, int h)
{
	AVFrame* out = avcodec_alloc_frame();
	uint8_t* buf = (uint8_t*)av_malloc(sizeof(uint8_t) * 1024 * 1024 * 10);
	
	avpicture_fill((AVPicture*)out,
		buf,
		PIX_FMT_RGB24,
		w, h
	);
	
	SwsContext* sws;
	
	sws = sws_getContext(
		w, h, src_fmt,
		w, h, PIX_FMT_RGB24,
		SWS_BICUBIC, NULL, NULL, NULL
	);
	
	sws_scale(sws,
		src->data, src->linesize,
		0, h,
		out->data, out->linesize
	);
	
	FILE* f = fopen(filename, "wb");
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	
	for(int y = 0; y < h; ++y)
		fwrite(out->data[0] + y * out->linesize[0], 1, w * 3, f);
	
	fclose(f);
	
	av_free(out);
	av_free(buf);
}

MP2V::MP2V(AVStream* stream)
 : StreamHandler(stream)
 , m_lastDirectPTS(0)
 , m_encoding(false)
 , m_decoding(false)
{
	m_startDecodeOffset = av_rescale(2, stream->time_base.den, stream->time_base.num);
}

MP2V::~MP2V()
{
}

int MP2V::handlePacket(AVPacket* packet)
{
	int gotFrame;
	
	packet->dts = pts_rel(packet->dts);
	packet->pts = pts_rel(packet->pts);
	
	if(!m_encoding)
	{
		// Normal passthrough operation
		int64_t time = packet->pts;
		
		if(m_nc && m_nc->time - m_startDecodeOffset < time && packet->flags & AV_PKT_FLAG_KEY)
		{
			log_debug("NOTE:  %'10lld, switching to decoder for %s",
				packet->dts - totalCutout(),
				(m_nc->direction == CutPoint::IN) ? "CUT_IN" : "CUT_OUT"
			);
			
			m_decoding = true;
			m_waitKeyFrames = 2;
		}
	}
	
	if(m_decoding)
	{
		if(avcodec_decode_video2(stream()->codec, m_frame, &gotFrame, packet) < 0)
			return error("Could not decode packet");
		
		if(gotFrame && m_frame->interlaced_frame)
		{
			if(!(outputStream()->codec->flags & CODEC_FLAG_INTERLACED_DCT))
			{
				log_debug("Got interlaced frame, enabling interlaced output");
				outputStream()->codec->flags |= CODEC_FLAG_INTERLACED_DCT;
			}
		}
		
		if(m_currentIsCutout && gotFrame && m_nc && packet->dts >= m_nc->time)
		{
			m_encoding = true;
		}
		
		if(!m_currentIsCutout)
		{
			if(gotFrame && packet->flags & AV_PKT_FLAG_KEY)
				m_encoding = true;
			
			if(gotFrame && m_nc && packet->dts >= m_nc->time)
			{
				m_encoding = false;
				m_decoding = false;
				
				m_currentIsCutout = true;
				
				int64_t current_time = m_nc->time;
				m_nc = cutList().nextCutPoint(packet->dts);
				
				if(m_nc)
					setTotalCutout(m_nc->time - (current_time - totalCutout()));
				else
					setActive(false); // last cutpoint reached
				
				avcodec_flush_buffers(stream()->codec);
				
				return 0;
			}
		}
		
		int bytes = 0;
		
		if(m_encoding)
		{
			if(!outputStream()->codec->codec)
			{
				if(avcodec_open2(outputStream()->codec, m_encoder, 0) != 0)
					return error("Could not open encoder");
				log_debug("NOTE:  %'10lld, encoder opened", packet->dts);
			}
			
			// Reset some frame settings
			m_frame->pict_type = AV_PICTURE_TYPE_I;
			m_frame->key_frame = 0;
			m_frame->pkt_pts = AV_NOPTS_VALUE;
			m_frame->pkt_dts = AV_NOPTS_VALUE;
			m_frame->pts = av_rescale_q(packet->dts - totalCutout(), stream()->time_base, outputStream()->codec->time_base);
			
			if(gotFrame)
				bytes = avcodec_encode_video(outputStream()->codec, m_outputPacket.data, OUTPUT_BUFFER_SIZE, m_frame);
			else
				log_debug("NOTE:  %'10lld, decoder not running yet", packet->dts);
			
			if(bytes < 0)
				return error("Could not encode video frame");
			
			if(bytes)
			{
				m_outputPacket.size = bytes;
				m_outputPacket.pts = packet->dts - totalCutout();
				m_outputPacket.dts = AV_NOPTS_VALUE;
				m_outputPacket.flags = outputStream()->codec->coded_frame->key_frame ? AV_PKT_FLAG_KEY : 0;
			}
		}
		
		if(m_nc->direction == CutPoint::OUT)
		{
			if(!m_currentIsCutout && bytes && m_outputPacket.pts > m_lastDirectPTS)
			{
				log_debug("WRITE: %'10lld, from encoder (cutout)", m_outputPacket.pts);
				
				if(av_interleaved_write_frame(outputContext(), &m_outputPacket) != 0)
					return error("Could not write from cutout encoder (values after write: PTS = %'10lld, DTS = %'10lld\n",
						m_outputPacket.pts, m_outputPacket.dts);
			}
		}
		else if(gotFrame && m_nc && packet->dts >= m_nc->time)
		{
			// This is more complicated. Need to wait for start of next GOP
			// to determine it's starting PTS. Meanwhile, we start outputting
			// our encoded frames.
			
			m_encoding = true;
			
			if(m_waitKeyFrames)
			{
				// Wait for two key_frames to make sure we are encoding the
				// same content. This ensures back-referencing B/P-Frames
				// reference data from after the cut.
				if(packet->flags & AV_PKT_FLAG_KEY)
				{
					if(--m_waitKeyFrames == 0)
					{
						m_gopMinPTS = packet->pts;
					}
				}
				
				if(bytes && m_waitKeyFrames)
				{
					log_debug("WRITE: %'10lld, key=%d, Waiting for key frames",
							m_outputPacket.pts, m_outputPacket.flags);
					av_interleaved_write_frame(outputContext(), &m_outputPacket);
				}
				
				if(!bytes)
					log_debug("Encoder lag!");
				
				if(m_waitKeyFrames)
					return 0;
			}
			
			// Input packets need also to be cached to replay them later on
			packet->stream_index = outputStream()->index;
			AVPacket copy = copyPacket(*packet);
			copy.dts = AV_NOPTS_VALUE;
			if(copy.pts >= m_gopMinPTS)
				m_copyPacketBuffer.push_back(copy);
			else
			{
				dump_cutin_packet("drop_input", packet->pts - totalCutout(), packet);
				
				log_debug("SKIP:  %'10lld from input, probably a B-Frame", packet->pts - totalCutout());
			}
			
			// Only save those encoded packets, that are not provided by the later
			// input packets.
			if(bytes)
			{
				if(!bufferContainsPTS(m_copyPacketBuffer, m_outputPacket.pts + totalCutout()))
					m_encodedPacketBuffer.push_back(copyPacket(m_outputPacket));
				else
				{
					dump_cutin_packet("drop_enc", m_outputPacket.pts, &m_outputPacket);
					log_debug("SKIP:  %'10lld from encoder, already in GOP buffer", m_outputPacket.pts);
				}
			}
			
			if(m_copyPacketBuffer.size() > 1 && packet->flags & AV_PKT_FLAG_KEY)
			{
				// End of GOP. Now we need to output all encoded packets before the
				// first packet of the passthrough GOP
				
				int lastPTS = 0;
				
				for(PacketBuffer::iterator it = m_encodedPacketBuffer.begin();
					it != m_encodedPacketBuffer.end(); ++it)
				{
					AVPacket& p = *it;
					
					if(p.pts > lastPTS)
						lastPTS = p.pts;
					
					if(!bufferContainsPTS(m_copyPacketBuffer, p.pts + totalCutout()))
					{
						log_debug("WRITE: %'10lld, from encoder buffer", p.pts);
						dump_cutin_packet("enc", p.pts, &p);
						
						if(av_interleaved_write_frame(outputContext(), &p) != 0)
							return error("Could not write packet from encoder buffer\n");
					}
					
					av_free_packet(&p);
				}
				m_encodedPacketBuffer.clear();
				
				// Now replay the buffered GOP
				for(PacketBuffer::iterator it = m_copyPacketBuffer.begin();
					it != m_copyPacketBuffer.end(); ++it)
				{
					AVPacket& p = *it;
					
					log_debug("WRITE: %'10lld, key=%d, from GOP buffer",
							p.pts - totalCutout(), p.flags);
					
					dump_cutin_packet("input", p.pts - totalCutout(), &p);
					
					if(writeInputPacket(&p) != 0)
						return error("Could not write packet from GOP buffer");
					av_free_packet(&p);
				}
				m_copyPacketBuffer.clear();
				
				m_encoding = false;
				m_decoding = false;
				
				avcodec_flush_buffers(outputStream()->codec);
				avcodec_flush_buffers(stream()->codec);
				
				avcodec_close(outputStream()->codec);
				
				log_debug("Everything flushed.");
				
				m_currentIsCutout = false;
				
				m_nc = cutList().nextCutPoint(packet->dts);
				
				return 0;
			}
		}
	}
	
	if(!m_encoding)
	{
		if(!m_currentIsCutout)
		{
			if(writeInputPacket(packet) != 0)
				return error("Could not write direct input packet");
			
			int64_t real_pts = packet->dts - totalCutout();
			
			if(real_pts > m_lastDirectPTS)
				m_lastDirectPTS = real_pts;
		}
	}
	
	return 0;
}

int MP2V::init()
{
	AVStream* ostream = outputStream();
	
	// Decoder init
	AVCodec* decoder = avcodec_find_decoder(stream()->codec->codec_id);
	if(!decoder)
		return error("Could not find decoder, MPEG-2 support in ffmpeg disabled?");
	
	if(avcodec_open2(stream()->codec, decoder, 0) != 0)
		return error("Could not open decoder");
	
	// Output stream settings
	ostream->time_base = stream()->time_base;
	
	// Encoder init
	m_encoder = avcodec_find_encoder(stream()->codec->codec_id);
	if(!m_encoder)
		return error("Could not find encoder, MPEG-2 support in ffmpeg disabled?");
	
	ostream->codec = avcodec_alloc_context3(m_encoder);
	avcodec_copy_context(ostream->codec, stream()->codec);
	
	// Copy codec settings to muxer level
	ostream->sample_aspect_ratio = ostream->codec->sample_aspect_ratio;
	
	// Disable rate control
	ostream->codec->rc_buffer_size = 0;
	
	// Disable b-frames (this makes encoding more predictable)
	ostream->codec->max_b_frames = 0;
	
	// MPEG-2 has time_base=1/50 and ticks_per_frame=1
	//  but the encoder expects 1/fps
	ostream->codec->time_base = av_mul_q(
		ostream->codec->time_base, (AVRational){ostream->codec->ticks_per_frame, 1}
	);
	ostream->codec->ticks_per_frame = 1;
	
	// Cut state
	m_nc = cutList().nextCutPoint(0);
	m_currentIsCutout = m_nc->direction == CutPoint::IN;
	
	// Decode buffer
	int w = stream()->codec->width;
	int h = stream()->codec->height;
	PixelFormat f = stream()->codec->pix_fmt;
	
	m_frame = avcodec_alloc_frame();
	avpicture_fill(
		(AVPicture*)m_frame,
		(uint8_t*)av_malloc(avpicture_get_size(f, w, h)),
		f, w, h
	);
	
	// Encoder buffer
	av_init_packet(&m_outputPacket);
	m_outputPacket.stream_index = ostream->index;
	m_outputPacket.dts = AV_NOPTS_VALUE;
	m_outputPacket.data = (uint8_t*)av_malloc(OUTPUT_BUFFER_SIZE);
	if(!m_outputPacket.data)
		return error("Could not allocate output buffer");
	
	return 0;
}

REGISTER_STREAM_HANDLER(CODEC_ID_MPEG2VIDEO, MP2V)
