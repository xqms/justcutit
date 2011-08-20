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
#include <queue>
#include <unistd.h>

#include "../justcutit_editor/gldisplay.h"

struct CutPoint
{
	typedef std::vector<CutPoint> List;
	
	float time;
	enum Direction { IN, OUT } direction;
};

typedef std::map<int, int> StreamMap;

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

bool start_encoder(AVStream* stream)
{
	AVCodec* codec = (AVCodec*)stream->codec->opaque;
	
	if(avcodec_open2(stream->codec, codec, 0) != 0)
	{
		fprintf(stderr, "Fatal: Could not open video encoder\n");
		return false;
	}
	
	printf("[ENCODER] Encoder opened.\n");
	
	return true;
}

bool flush_out_encoder(AVStream* stream, AVFormatContext* ctx, AVPacket* packet, int BUFSIZE, int64_t* lastCodedPTS)
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
		
		*lastCodedPTS = packet->pts;
		
		printf("[ENCODE] Flushing packet out of encoder: %10lld\n", packet->pts);
		
		av_interleaved_write_frame(ctx, packet);
		av_init_packet(packet);
	}
	
	avcodec_close(stream->codec);
}

bool openInputStreams(AVFormatContext* ctx, int* videoIdx)
{
	*videoIdx = -1;
	
	for(int i = 0; i < ctx->nb_streams; ++i)
	{
		AVStream* stream = ctx->streams[i];
		
		// We only need codecs for the video stream
		if(stream->codec->codec_type != AVMEDIA_TYPE_VIDEO)
			continue;
		
		if(*videoIdx != -1)
		{
			fprintf(stderr, "Fatal: Multiple video streams, this is unhandled at this point\n");
			return false;
		}
		
		*videoIdx = i;
		
		AVCodec* codec = avcodec_find_decoder(stream->codec->codec_id);
		if(!codec)
		{
			fprintf(stderr, "Fatal: Could not find codec for decoding stream %d\n", i);
			return false;
		}
		
		if(avcodec_open2(stream->codec, codec, 0) != 0)
		{
			fprintf(stderr, "Fatal: Could not open codec for decoding stream %d\n", i);
			return false;
		}
	}
	
	if(*videoIdx == -1)
	{
		fprintf(stderr, "Fatal: No video streams found\n");
		return false;
	}
	
	return true;
}

bool setupOutputStreams(AVFormatContext* input, AVFormatContext* output, StreamMap* map)
{
	for(int i = 0; i < input->nb_programs; ++i)
	{
		AVProgram* program = input->programs[i];
		
		// Skip empty programs
		if(program->nb_stream_indexes == 0)
			continue;
		
		AVProgram* oprogram = av_new_program(output, program->id);
		
		for(int j = 0; j < program->nb_stream_indexes; ++j)
		{
			AVStream* istream = input->streams[program->stream_index[j]];
			
			// Only do video stream for now
			if(istream->codec->codec_type != AVMEDIA_TYPE_VIDEO && istream->codec->codec_type != AVMEDIA_TYPE_AUDIO)
				continue;
			
			AVStream* ostream = av_new_stream(output, istream->id);
			
			// Register with program
			oprogram->nb_stream_indexes++;
			oprogram->stream_index = (unsigned int*)av_realloc(
				oprogram->stream_index, oprogram->nb_stream_indexes
			);
			oprogram->stream_index[oprogram->nb_stream_indexes-1] = ostream->index;
			
// 			if(istream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
// 			{
				// Get codec
				AVCodec* codec = avcodec_find_encoder(istream->codec->codec_id);
				if(!codec)
				{
					fprintf(stderr, "Fatal: Could not find codec for encoding stream %d\n",
						program->stream_index[j]
					);
					return false;
				}
				
				// Copy codec settings
				ostream->codec = avcodec_alloc_context3(codec);
				avcodec_copy_context(ostream->codec, istream->codec);
				
				// Codec/stream parameters
				ostream->time_base = istream->time_base;
				
				// MPEG-2 comes with time_base=1/50 and ticks_per_frame=2
				//  the encoder expects time_base=1/fps
				ostream->codec->time_base = av_mul_q(ostream->codec->time_base, (AVRational){istream->codec->ticks_per_frame,1});
				ostream->codec->ticks_per_frame = 1;
				ostream->sample_aspect_ratio = istream->codec->sample_aspect_ratio;
				
				ostream->codec->rc_buffer_size = 0;
				ostream->codec->max_b_frames = 0;
				
				// Save codec pointer for later opening
				ostream->codec->opaque = (void*)codec;
// 			}
			
			(*map)[istream->index] = ostream->index;
		}
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
	
	if(!openInputStreams(ctx, &videoIdx))
		return 1;
	
	if(!setupOutputStreams(ctx, output_ctx, &stream_mapping))
		return 1;
	
	printf(" [+] Output streams:\n");
	av_dump_format(output_ctx, 0, argv[3], true);
	
	
	avformat_write_header(output_ctx, 0);
	
	QApplication app(argc, argv);
	
	
	enum State
	{
		ST_SKIPPING                          = 0,
		ST_COPY                              = 1,
		ST_ENCODE                            = 2,
	} state;
	
	enum EncoderState
	{
		EST_ENCODE_WAIT_FOR_BEGIN            = 0,
		EST_ENCODE_ENCODING                  = (1 << 1),
		EST_ENCODE_WAIT_FOR_KEYFRAME_PACKET  = (1 << 2) | (1 << 1),
		EST_ENCODE_WAIT_FOR_KEYFRAME_FRAME   = (1 << 3) | (1 << 1)
	} encoderState;
	
	state = (cutlist[0].direction == CutPoint::IN) ? ST_SKIPPING : ST_COPY;
	
	AVPacket packet;
	int64_t start_dts;
	bool first_packet = true;
	int keyframePacketCount = 0;
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
	
	AVPacket output_packet;
	const int BUFSIZE = 100 * 1024;
	av_new_packet(&output_packet, BUFSIZE);
	int64_t totalSkipDTS = 0;
	int64_t cutoffDTS = 0;
	int64_t cutInPTS = 0;
	int64_t lastCodedPTS = 0;
	int64_t minReplayPTS = -1;
	int64_t encodeOffsetDTS = 0;
	int printPackets = 0;
	std::queue<AVPacket> replayPackets;
	
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
		
		AVStream* istream = ctx->streams[packet.stream_index];
		AVStream* ostream = output_ctx->streams[it->second];
		
		if(istream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			float d = av_q2d(istream->time_base);
			
			int gotFrame;
			if(avcodec_decode_video2(istream->codec, frame, &gotFrame, &packet) < 0)
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
			State nextState = state;
			
			CutPoint* nc = 0;
			
			if(cutlist_idx < cutlist.size())
			{
				nc = &cutlist[cutlist_idx];
				nextState = (nc->direction == CutPoint::OUT) ? ST_SKIPPING : ST_COPY;
			}
			
			if(nc && state != ST_ENCODE && time + 1.0 > nc->time && nextState != state)
			{
				if(!start_encoder(ostream))
					return 1;
				state = ST_ENCODE;
				encoderState = EST_ENCODE_WAIT_FOR_BEGIN;
			}
			
			if(state == ST_ENCODE)
			{
				time = d * (packet.dts - start_dts);
				
				if(gotFrame)
				{
					if(nc && time >= nc->time)
					{
						if(nextState == ST_SKIPPING) // CUT OUT
						{
							printf("% 7.3f: [ENCODE] [CUT_OUT] Now at cutpoint %d, directly breaking...\n", time, cutlist_idx);
							state = nextState;
							encoderState = EST_ENCODE_WAIT_FOR_BEGIN;
							cutoffDTS = packet.dts;
							flush_out_encoder(ostream, output_ctx, &output_packet, BUFSIZE, &lastCodedPTS);
							getchar();
						}
						else // CUT IN
						{
							printf("% 7.3f: [ENCODE] [CUT_IN] Now at cutpoint %d, waiting for next keyframe\n", time, cutlist_idx);
							encoderState = EST_ENCODE_WAIT_FOR_KEYFRAME_PACKET;
							keyframePacketCount = 5;
							
							printf("startDTS = %lld, cutoffDTS = %lld, current DTS = %lld\n", start_dts, cutoffDTS, packet.dts);
							totalSkipDTS += packet.dts - cutoffDTS;
							printf(" => total skip: %lld\n", totalSkipDTS);
							cutInPTS = packet.dts;
							getchar();
						}
						
						cutlist_idx++;
						
						if(cutlist_idx == cutlist.size() && state == ST_SKIPPING)
							break;
					}
					
					if(encoderState == EST_ENCODE_WAIT_FOR_KEYFRAME_PACKET
						&& packet.flags & AV_PKT_FLAG_KEY)
					{
						keyframePacketCount--;
						if(!keyframePacketCount)
						{
							printf("Got keyframe packet with DTS = %lld and PTS = %lld\n", packet.dts, packet.pts);
							
#if 1
							encoderState = EST_ENCODE_WAIT_FOR_KEYFRAME_FRAME;
							minReplayPTS = -1;
#else
							encoderState = EST_ENCODE_WAIT_FOR_BEGIN;
							state = ST_COPY;
							printPackets = 10;
							
							flush_out_encoder(ostream, output_ctx, &output_packet, BUFSIZE, &lastCodedPTS);
							printf("Last coded PTS = %lld\n", lastCodedPTS);
							getchar();
#endif
						}
					}
					
					if(encoderState == EST_ENCODE_WAIT_FOR_KEYFRAME_FRAME)
					{
						if(frame->key_frame)
						{
							printf("% 7.3f: [ENCODE] found keyframe with pts = %lld, pkt_pts = %lld, pkt_dts = %lld, packet->dts = %lld, giving over to copy algorithm\n",
								time, frame->pts, frame->pkt_dts, frame->pkt_dts, packet.dts);
							getchar();
							
							encoderState = EST_ENCODE_WAIT_FOR_BEGIN;
							state = ST_COPY;
							
							printPackets = 10;
							
							flush_out_encoder(ostream, output_ctx, &output_packet, BUFSIZE, &lastCodedPTS);
							
							int keyFramePTS = replayPackets.front().pts;
							/*
							// Timing:
							//  packet.dts is the PTS of current frame
							totalSkipDTS -= packet.dts - cutInDTS;*/
							int64_t frameTime = av_rescale_q(1, ostream->codec->time_base, ostream->time_base);
							totalSkipDTS =  keyFramePTS - lastCodedPTS;
							
							while(!replayPackets.empty())
							{
								AVPacket packet = replayPackets.front();
								replayPackets.pop();
								
								
								
								printf("Replaying packet with PTS %lld, key frame: %d\n", packet.pts, packet.flags & AV_PKT_FLAG_KEY);
								
								packet.pts -= totalSkipDTS;
								
// 								if(packet.pts >= keyFramePTS)
// 								{
									av_pkt_dump2(stdout, &packet, 0, ostream);
									av_interleaved_write_frame(output_ctx, &packet);
// 								}
								av_free_packet(&packet);
							}
						}
						else
						{
							AVPacket save_packet;
							av_init_packet(&save_packet);
							save_packet.data = (uint8_t*)av_malloc(packet.size);
							save_packet.size = packet.size;
							memcpy((void*)save_packet.data, (void*)packet.data, packet.size);
							
							save_packet.stream_index = ostream->index;
							save_packet.dts = AV_NOPTS_VALUE;
							save_packet.pts = packet.pts;
							save_packet.flags = packet.flags;
							
							if(minReplayPTS < 0 || packet.pts < minReplayPTS)
								minReplayPTS = packet.pts;
							
							replayPackets.push(save_packet);
						}
					}
					
					if(encoderState & EST_ENCODE_ENCODING)
					{
						av_init_packet(&output_packet);
						
						frame->pts = av_rescale_q(frame->pkt_pts - cutInPTS, istream->time_base, ostream->codec->time_base);
						frame->pict_type = AV_PICTURE_TYPE_NONE; // Let the codec handle picture types
						
						printf("[ENCODE] Input PTS: %lld\n", frame->pts);
						
						printf("[ENCODE] Encoding, key_frame = %d\n", frame->key_frame);
						
						int bytes = avcodec_encode_video(ostream->codec, output_packet.data, BUFSIZE, frame);
						
						if(bytes < 0)
						{
							fprintf(stderr, "Error while encoding video frame: bytes = %d\n", bytes);
							return 1;
						}
						
						if(bytes)
						{
							output_packet.dts = AV_NOPTS_VALUE;
							output_packet.pts = av_rescale_q(ostream->codec->coded_frame->pts, ostream->codec->time_base, ostream->time_base) + cutInPTS - totalSkipDTS;
							output_packet.size = bytes;
							output_packet.stream_index = it->second;
							
							if(ostream->codec->coded_frame->key_frame)
								output_packet.flags |= AV_PKT_FLAG_KEY;
							
							lastCodedPTS = output_packet.pts;
							
							av_pkt_dump2(stdout, &output_packet, 0, ostream);
							av_interleaved_write_frame(output_ctx, &output_packet);
							av_init_packet(&output_packet);
						}
					}
					
					printf("[ENCODE] Packet stream: pts=%10lld dts=%10lld key=%d | Frame stream: pts=%10lld pict_type=%d | Encoded stream: pts=%10lld, pict_type=%d\n",
						packet.pts - totalSkipDTS, packet.dts - totalSkipDTS, packet.flags & AV_PKT_FLAG_KEY,
						frame->pkt_pts - totalSkipDTS, frame->pict_type,
						lastCodedPTS, ostream->codec->coded_frame ? ostream->codec->coded_frame->key_frame : 2,
						ostream->codec->coded_frame ? ostream->codec->coded_frame->pict_type : 10
					);
				}
			}
		}
		
		if(state == ST_COPY || (state == ST_ENCODE && encoderState & EST_ENCODE_ENCODING && istream->codec->codec_type != AVMEDIA_TYPE_VIDEO))
		{
			packet.stream_index = it->second;
			packet.dts = AV_NOPTS_VALUE;
			packet.pts -= totalSkipDTS;
			
			if(printPackets > 0)
			{
				printf("[ COPY ] Writing packet with PTS %lld\n", packet.pts);
				av_pkt_dump2(stdout, &packet, 0, ostream);
				printPackets--;
			}
			
			av_interleaved_write_frame(output_ctx, &packet);
		}
		
		av_free_packet(&packet);
		
		QCoreApplication::processEvents();
	}
	
	av_free_packet(&packet);
	
	av_write_trailer(output_ctx);
	
	avio_close(output_ctx->pb);
	avformat_free_context(output_ctx);
	
// 	perform_cutting(ctx, cutlist);
}
