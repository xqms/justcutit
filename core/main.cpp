// MPEG-TS cutter with frame precision
// Author: Max Schwarz <Max@x-quadraht.de>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <QtGui/QApplication>
#include <QtGui/QBoxLayout>

#include <stdio.h>
#include <vector>
#include <map>
#include <unistd.h>

#include "../justcutit_editor/gldisplay.h"

struct CutPoint
{
	typedef std::vector<CutPoint> List;
	
	float time;
	enum Direction { IN, OUT } direction;
};

void usage(FILE* dest)
{
	fprintf(dest, "Usage: justcutit <file> <cutlist> <output-file>\n");
}

bool readCutlist(FILE* file, CutPoint::List* dest)
{
	while(1)
	{
		CutPoint point;
		char inout[4];
		
		int ret = fscanf(file, "%f %3s", &point.time, inout);
		
		if(ret == EOF)
		{
			if(ferror(file))
			{
				perror("Could not read from cutlist");
				return false;
			}
			
			break;
		}
		
		if(ret != 2)
		{
			fprintf(stderr, "Error in cutlist\n");
			return false;
		}
		
		if(strcmp(inout, "IN") == 0)
			point.direction = CutPoint::IN;
		else if(strcmp(inout, "OUT") == 0)
			point.direction = CutPoint::OUT;
		else
		{
			fprintf(stderr, "Invalid specifier '%s' in cutlist\n", inout);
		}
		
		dest->push_back(point);
	}
	
	return true;
}

void flush_out_encoder(AVStream* stream, AVFormatContext* ctx, AVPacket* packet, int BUFSIZE)
{
	AVCodecContext* codec = stream->codec;
	
	av_init_packet(packet);
	
	while(1)
	{
		int bytes = avcodec_encode_video(codec, packet->data, BUFSIZE, NULL);
		
		if(bytes < 0)
		{
			fprintf(stderr, "Could not encode video frame\n");
			exit(1);
		}
		
		if(bytes == 0)
			break;
		
		packet->pts = av_rescale_q(codec->coded_frame->pts, codec->time_base, stream->time_base);
		packet->size = bytes;
		packet->stream_index = stream->index;
		
		if(codec->coded_frame->key_frame)
			packet->flags |= AV_PKT_FLAG_KEY;
		
		av_interleaved_write_frame(ctx, packet);
		av_init_packet(packet);
	}
}

int main(int argc, char** argv)
{
	AVFormatContext* ctx = 0;
	AVFormatContext* output_ctx = 0;
	CutPoint::List cutlist;
	std::map<int, int> stream_mapping;
	int videoIdx;
	
	if(argc != 4)
	{
		usage(stderr);
		return 1;
	}
	
	av_register_all();
	
	printf("Opening file '%s'\n", argv[1]);
	if(avformat_open_input(&ctx, argv[1], NULL, NULL) != 0)
	{
		fprintf(stderr, "Fatal: Could not open input stream\n");
		return 1;
	}
	
	if(avformat_find_stream_info(ctx, 0) != 0)
	{
		fprintf(stderr, "Fatal: Could not find stream information\n");
		return 1;
	}
	
	av_dump_format(ctx, 0, argv[1], false);
	
	printf("Reading cutlist\n");
	FILE* cutlist_file = fopen(argv[2], "r");
	if(!cutlist_file)
	{
		perror("Could not open cutlist");
		return 1;
	}
	
	if(!readCutlist(cutlist_file, &cutlist))
	{
		fprintf(stderr, "Fatal: Could not read cutlist\n");
		return 1;
	}
	
	fclose(cutlist_file);
	
	if(!cutlist.size())
	{
		printf("Cutlist contains no cutpoints. Nothing to do!\n");
		return 2;
	}
	
	printf("Opening output file\n");
	if(avformat_alloc_output_context2(&output_ctx, 0, "mpegts", argv[3]) != 0)
	{
		fprintf(stderr, "Could not allocate output context\n");
		return 1;
	}
	if(avio_open(&output_ctx->pb, argv[3], AVIO_FLAG_WRITE) != 0)
	{
		fprintf(stderr, "Could not open output file\n");
		return 1;
	}
	
	output_ctx->oformat->flags |= AVFMT_TS_NONSTRICT;
	AVCodec* videoCodec;
	for(int i = 0; i < ctx->nb_programs; ++i)
	{
		AVProgram* prog = ctx->programs[i];
		
		if(!prog->nb_stream_indexes)
			continue;
		
		AVProgram* oprog = av_new_program(output_ctx, prog->id);
		
		for(int j = 0; j < prog->nb_stream_indexes; ++j)
		{
			AVStream* stream = ctx->streams[prog->stream_index[j]];
			AVMediaType type = stream->codec->codec_type;
			
			if(type != AVMEDIA_TYPE_VIDEO /*&& type != AVMEDIA_TYPE_AUDIO*/)
			{
// 				stream->discard = AVDISCARD_ALL;
				continue;
			}
			
			if(type == AVMEDIA_TYPE_VIDEO)
				videoIdx = stream->index;
			
			// Init codec
			AVCodec* codec = avcodec_find_decoder(stream->codec->codec_id);
			if(!codec)
			{
				fprintf(stderr, "Fatal: Could not find decoder for stream %d\n", stream->index);
				return 1;
			}
			if(avcodec_open2(stream->codec, codec, 0) != 0)
			{
				fprintf(stderr, "Fatal: Could not open encoder for stream %d\n", stream->index);
				return 1;
			}
			
			AVStream* ostream = av_new_stream(output_ctx, stream->id);
			
			ostream->codec = avcodec_alloc_context3(stream->codec->codec);
			avcodec_copy_context(ostream->codec, stream->codec);
			
			ostream->codec->rc_buffer_size = 0;
			ostream->codec->rc_initial_buffer_occupancy = ostream->codec->rc_buffer_size*3/4;
			ostream->codec->max_b_frames = 1;
			ostream->codec->time_base = (AVRational){1,25};
			printf("Rate control settings:\n");
			printf(" - min_rate: %d\n", ostream->codec->rc_min_rate);
			printf(" - max_rate: %d\n", ostream->codec->rc_max_rate);
			printf(" - bit_rate: %d\n", ostream->codec->bit_rate);
			printf(" - bit_rate_tolerance: %d\n", ostream->codec->bit_rate_tolerance);
			printf(" - gop_size: %d\n", ostream->codec->gop_size);
			printf("FPS settings:\n");
			printf(" - time_base: %d / %d\n", ostream->codec->time_base.num, ostream->codec->time_base.den);
			videoCodec = avcodec_find_encoder(stream->codec->codec_id);
			if(!videoCodec)
			{
				fprintf(stderr, "Fatal: Could not find encoder for stream %d\n", ostream->index);
				return 1;
			}
			
			ostream->sample_aspect_ratio = stream->codec->sample_aspect_ratio;
			
			oprog->nb_stream_indexes++;
			oprog->stream_index = (unsigned int*)av_realloc(
				(void*)oprog->stream_index,
				sizeof(int) * oprog->nb_stream_indexes
			);
			
			oprog->stream_index[oprog->nb_stream_indexes-1] = ostream->index;
			
			stream_mapping[stream->index] = ostream->index;
		}
	}
	
	av_dump_format(output_ctx, 0, argv[3], true);
	avformat_write_header(output_ctx, 0);
	
	
	QApplication app(argc, argv);
	
	
	AVPacket packet;
	int64_t start_dts;
	bool first_packet = true;
	bool skipping = (cutlist[0].direction == CutPoint::IN);
// 	bool skipping = false;
	bool encoding = false;
	int waitingForKeyFrame = 0;
	int cutlist_idx = 0;
	AVStream* videoStream = ctx->streams[videoIdx];
	AVFrame* frame = avcodec_alloc_frame();
	avpicture_fill(
		(AVPicture*)frame,
		(uint8_t*)av_malloc(
			avpicture_get_size(
				videoStream->codec->pix_fmt,
				videoStream->codec->width,
				videoStream->codec->height
			)
		),
		videoStream->codec->pix_fmt,
		videoStream->codec->width,
		videoStream->codec->height
	);
	AVFrame* output_frame = avcodec_alloc_frame();
	avpicture_fill(
		(AVPicture*)output_frame,
		(uint8_t*)av_malloc(
			avpicture_get_size(
				videoStream->codec->pix_fmt,
				videoStream->codec->width,
				videoStream->codec->height
			)
		),
		videoStream->codec->pix_fmt,
		videoStream->codec->width,
		videoStream->codec->height
	);
	
	AVPacket output_packet;
	const int BUFSIZE = 100 * 1024;
	av_new_packet(&output_packet, BUFSIZE);
	int64_t totalSkipDTS = 0;
	int64_t cutoffDTS = 0;
	int64_t encodeOffsetDTS = 0;
	
	QWidget w;
	GLDisplay display(&w);
	QHBoxLayout* layout = new QHBoxLayout(&w);
	layout->addWidget(&display);
	w.show();
	display.setSize(videoStream->codec->width, videoStream->codec->height);
	
	QCoreApplication::processEvents();
	
	while(av_read_frame(ctx, &packet) == 0)
	{
		std::map<int, int>::iterator it = stream_mapping.find(packet.stream_index);
		if(it == stream_mapping.end())
			continue;
		
		AVStream* stream = ctx->streams[packet.stream_index];
		AVStream* ostream = output_ctx->streams[it->second];
		
		if(stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			float d = av_q2d(stream->time_base);
			
			int gotFrame;
			if(avcodec_decode_video2(stream->codec, frame, &gotFrame, &packet) < 0)
			{
				fprintf(stderr, "Error while decoding video frame\n");
				return 1;
			}
			
			if(first_packet && gotFrame)
			{
				cutoffDTS = start_dts = packet.dts;
				if(frame->key_frame)
					first_packet = false;
			}
			
			if(gotFrame)
				display.paintFrame(frame);
			else
				printf("packet without frame\n");
			
			float time = d * (packet.dts - start_dts);
			bool nextState = false;
			
			CutPoint* nc = 0;
			
			if(cutlist_idx < cutlist.size())
			{
				nc = &cutlist[cutlist_idx];
				nextState = nc->direction == CutPoint::OUT;
			}
			
			if(nc && !encoding && time + 1.0 > nc->time && nextState != skipping)
			{
				printf("% 7.3f: Time to re-encode!\n", time);
				
				if(avcodec_open2(ostream->codec, videoCodec, 0) != 0)
				{
					fprintf(stderr, "Fatal: Could not open encoder for stream %d\n", ostream->index);
	// 				return 1;
				}
				
				encoding = true;
			}
			
			if(encoding)
			{
				time = d * (packet.dts - start_dts);
				
				if(gotFrame)
				{
					bool forceKeyFrame = false;
					
					if(frame->key_frame)
						printf("% 7.3f: [ENCODE] keyframe\n", time);
					
					if(nc && time >= nc->time)
					{
						if(!skipping && nextState) // CUT OUT
						{
							printf("% 7.3f: [ENCODE] [CUT_OUT] Now at cutpoint %d, directly breaking...\n", time, cutlist_idx);
							encoding = false;
							cutoffDTS = packet.dts;
							flush_out_encoder(ostream, output_ctx, &output_packet, BUFSIZE);
							avcodec_flush_buffers(ostream->codec);
							avcodec_close(ostream->codec);
							getchar();
						}
						else // CUT IN
						{
							printf("% 7.3f: [ENCODE] [CUT_IN] Now at cutpoint %d, waiting for next keyframe\n", time, cutlist_idx);
							waitingForKeyFrame = 5;
							
							printf("startDTS = %lld, cutoffDTS = %lld, current DTS = %lld\n", start_dts, cutoffDTS, packet.dts);
							totalSkipDTS += packet.dts - cutoffDTS;
							printf(" => total skip: %lld\n", totalSkipDTS);
							getchar();
						}
						
						skipping = nextState;
						forceKeyFrame = true;
						cutlist_idx++;
						
						if(cutlist_idx == cutlist.size() && skipping)
							break;
					}
					
					if(encoding && packet.flags & AV_PKT_FLAG_KEY && waitingForKeyFrame)
					{
						waitingForKeyFrame--;
						
						if(!waitingForKeyFrame)
						{
							printf("% 7.3f: [ENCODE] found keyframe, giving over to copy algorithm\n", time);
							getchar();
							
							encoding = false;
							waitingForKeyFrame = false;
							
							flush_out_encoder(ostream, output_ctx, &output_packet, BUFSIZE);
							avcodec_flush_buffers(ostream->codec);
							avcodec_close(ostream->codec);
						}
					}
					else if(encoding && !skipping)
					{
						av_init_packet(&output_packet);
						
						output_frame->pts = av_rescale_q(packet.dts - totalSkipDTS, stream->time_base, ostream->codec->time_base);
						
						printf("[ENCODE] Input PTS: %lld\n", output_frame->pts);
						
						if(forceKeyFrame)
							frame->key_frame = true;
						
						printf("[ENCODE] Encoding, key_frame = %d\n", frame->key_frame);
						av_picture_copy((AVPicture*)output_frame, (AVPicture*)frame, PIX_FMT_YUV420P, stream->codec->width, stream->codec->height);
						output_frame->best_effort_timestamp = AV_NOPTS_VALUE;
						output_frame->pkt_pos = -1;
						output_frame->key_frame = 1;
						
						int bytes = avcodec_encode_video(ostream->codec, output_packet.data, BUFSIZE, output_frame);
						
						if(bytes < 0)
						{
							fprintf(stderr, "Error while encoding video frame: bytes = %d\n", bytes);
							return 1;
						}
						
						if(bytes)
						{
							printf("[OUTPUT] Got coded frame with PTS %lld\n", ostream->codec->coded_frame->pts);
							output_packet.dts = AV_NOPTS_VALUE;
							output_packet.pts = av_rescale_q(ostream->codec->coded_frame->pts, ostream->codec->time_base, ostream->time_base);
							output_packet.size = bytes;
							output_packet.stream_index = it->second;
							
							if(ostream->codec->coded_frame->key_frame)
							{
								printf("[OUTPUT] keyframe!\n");
								output_packet.flags |= AV_PKT_FLAG_KEY;
							}
							
							printf("[OUTPUT] writing packet with PTS %lld\n", output_packet.pts);
							
							av_interleaved_write_frame(output_ctx, &output_packet);
							av_init_packet(&output_packet);
						}
					}
				}
			}
		}
		
		packet.stream_index = it->second;
		packet.dts -= totalSkipDTS;
		packet.pts -= totalSkipDTS;
		
		if(!skipping && !encoding)
			av_interleaved_write_frame(output_ctx, &packet);
		
		av_free_packet(&packet);
		
		QCoreApplication::processEvents();
	}
	
	av_free_packet(&packet);
	
	av_write_trailer(output_ctx);
	
	avio_close(output_ctx->pb);
	avformat_free_context(output_ctx);
	
// 	perform_cutting(ctx, cutlist);
}
