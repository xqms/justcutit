// H.264 dumper
// Author: Max Schwarz <Max@x-quadraht.de>

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

#include <libavcodec/h264.h>
}

#include <stdarg.h>

#define DEBUG 1
#define LOG_PREFIX "[H264]"
#include <common/log.h>

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
	for(int i = 0; i < 8; ++i)
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
	
	log_debug(" - pps_buffer[]:");
	for(int i = 0; i < MAX_PPS_COUNT; ++i)
	{
		PPS* p = h->pps_buffers[i];
		
		if(!p)
			continue;
		
		log_debug("    %2d: %s",
			i,
			(p->sps_id == h->pps.sps_id) ? "(current)" : ""
		);
	}
	
	log_debug(" - sps_buffer[]:");
	for(int i = 0; i < MAX_SPS_COUNT; ++i)
	{
		SPS* s = h->sps_buffers[i];
		
		if(!s)
			continue;
		
		if(i != 0)
			getc(stdin);
		
		log_debug("    %2d: %s",
			i,
			(i == h->pps.sps_id) ? "(current)" : ""
		);
	}
}

int main(int argc, char** argv)
{
	AVFormatContext* ctx = 0;
	AVCodec* decoder;
	AVStream* stream = 0;
	
	av_register_all();
	
	if(avformat_open_input(&ctx, argv[1], NULL, NULL) != 0)
		return error("Could not open input file");
	
	if(avformat_find_stream_info(ctx, NULL) < 0)
		return error("Could not find stream information");
	
	av_dump_format(ctx, 0, argv[1], 0);
	
	for(int i = 0; i < ctx->nb_streams; ++i)
	{
		AVStream* s = ctx->streams[i];
		
		if(s->codec->codec_id == CODEC_ID_H264)
		{
			stream = s;
			break;
		}
		
		s->discard = AVDISCARD_ALL;
	}
	
	if(!stream)
		return error("No H.264 stream found");
	
	decoder = avcodec_find_decoder(stream->codec->codec_id);
	if(!decoder)
		return error("No H.264 support in ffmpeg");
	
	if(avcodec_open2(stream->codec, decoder, NULL) != 0)
		return error("Could not open decoder");
	
	AVPacket packet;
	AVFrame frame;
	int gotFrame;
	avcodec_get_frame_defaults(&frame);
	
	H264Context* h264 = (H264Context*)stream->codec->priv_data;
	
	while(av_read_frame(ctx, &packet) == 0)
	{
		if(packet.stream_index != stream->index)
			continue;
		
		FILE* f = fopen("packet.264", "wb");
		fwrite(packet.data, sizeof(uint8_t), packet.size, f);
		fclose(f);
		
		if(avcodec_decode_video2(stream->codec, &frame, &gotFrame, &packet) < 0)
			return error("Could not decode video packet");
		
		log_debug("%d frames to recovery point", h264->sei_recovery_frame_cnt);
		dump_state(h264);
		
		getc(stdin);
		
// 		if(h264->mmco[h264->mmco_index].opcode == MMCO_RESET
// 			|| h264->sei_recovery_frame_cnt != -1
// 		)
// 		{
// 			getc(stdin);
// 		}
	}
}
