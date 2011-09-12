// MPEG-TS cutter with frame precision
// Author: Max Schwarz <Max@x-quadraht.de>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <getopt.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <queue>
#include <unistd.h>

#include "streamhandler.h"
#include "cutlist.h"
#include "io_split.h"

#if 0
#define LOG_DEBUG printf
#else
inline void LOG_DEBUG(const char*, ...)
{
}
#endif

typedef std::map<int, StreamHandler*> StreamMap;

void usage(FILE* dest)
{
	fprintf(dest, "Usage: justcutit [options] <file> <cutlist> <output-file>\n"
		"\n"
		"Options:\n"
		"  -s, --size COUNT  Split output files after COUNT MiB. output-file\n"
		"                    needs to be a template like \"output_%%d.ts\"\n"
		"  -v, --verbose     Provide progress info more often\""
		"  -a, --audio TYPE  Take audio stream of type TYPE (ffmpeg decoder name)\n"
	);
}

bool readCutlist(FILE* file, CutPointList* dest)
{
	while(1)
	{
		CutPoint point;
		char inout[4];
		
		int ret = fscanf(file, "%lld %3s", &point.time, inout);
		
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

bool setupHandlers(AVFormatContext* input, AVFormatContext* output,
	const CutPointList& cutlist, StreamMap* map, const char* audio_decoder = 0)
{
	StreamHandlerFactory factory;
	
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
			
			if(istream->codec->codec_type == AVMEDIA_TYPE_AUDIO && audio_decoder)
			{
				AVCodec* codec = avcodec_find_decoder(istream->codec->codec_id);
				if(!codec || strcmp(codec->name, audio_decoder) != 0)
				{
					istream->discard = AVDISCARD_ALL;
					continue;
				}
			}
			
			StreamHandler* handler = factory.createHandlerForStream(istream);
			
			if(!handler)
			{
				printf("Stream %d is unhandled.\n", istream->index);
				istream->discard = AVDISCARD_ALL;
				continue;
			}
			
			AVStream* ostream = av_new_stream(output, istream->id);
			
			// Register with program
			oprogram->nb_stream_indexes++;
			oprogram->stream_index = (unsigned int*)av_realloc(
				oprogram->stream_index, oprogram->nb_stream_indexes * sizeof(*oprogram->stream_index)
			);
			oprogram->stream_index[oprogram->nb_stream_indexes-1] = ostream->index;
			
			// Setup stream handler
			handler->setCutList(cutlist);
			handler->setOutputContext(output);
			handler->setOutputStream(ostream);
			handler->setStartPTS_AV(input->start_time);
			
			if(handler->init() != 0)
			{
				fprintf(stderr, "Error: Could not initialize stream handler for stream %d\n",
					istream->index
				);
				return false;
			}
			
			if(ostream->codec->codec)
			{
				printf("Using encoder '%s' for output of stream %d\n",
					   ostream->codec->codec->name, istream->index
				);
			}
			
			(*map)[istream->index] = handler;
		}
	}
	
	return true;
}

int main(int argc, char** argv)
{
	AVFormatContext* ctx = 0;
	AVFormatContext* output_ctx = 0;
	CutPointList cutlist;
	StreamMap stream_mapping;
	int64_t duration;
	int last_percent_done = 0;
	uint64_t split_size = 0;
	bool verbose = false;
	const char* audio_decoder = 0;
	
	av_register_all();
	
	while(1)
	{
		int option_index;
		struct option long_options[] = {
			{"split", required_argument, 0, 's'},
			{"verbose", no_argument, 0, 'v'},
			{"help", no_argument, 0, 'h'},
			{"audio", no_argument, 0, 'a'},
			{0, 0, 0, 0}
		};
		
		int c = getopt_long(argc, argv, "hs:a:v", long_options, &option_index);
		
		if(c == -1)
			break;
		
		switch(c)
		{
			case 'h':
				usage(stdout);
				return 0;
			case 's':
				split_size = atoll(optarg) * 1024 * 1024;
				break;
			case 'v':
				verbose = true;
				break;
			case 'a':
				audio_decoder = optarg;
				break;
			default:
				usage(stderr);
				return 1;
		}
	}
	
	if(argc - optind != 3)
	{
		usage(stderr);
		return 1;
	}
	
	av_log_set_level(AV_LOG_DEBUG);
	
	
	
	printf("Opening file '%s'\n", argv[optind]);
	int ret = avformat_open_input(&ctx, argv[optind], NULL, NULL);
	if(ret != 0)
	{
		fprintf(stderr, "Fatal: Could not open input stream (ret=%d => %s)\n", ret, strerror(-ret));
		return 1;
	}
	
	if(avformat_find_stream_info(ctx, 0) < 0)
	{
		fprintf(stderr, "Fatal: Could not find stream information\n");
		return 1;
	}
	
	printf(" [+] Input file duration: %.2f\n", (float)ctx->duration / AV_TIME_BASE);
	av_dump_format(ctx, 0, argv[optind], false);
	
	printf("Reading cutlist\n");
	FILE* cutlist_file = fopen(argv[optind+1], "r");
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
		fprintf(stderr, "Cutlist contains no cutpoints. Nothing to do!\n");
		return 2;
	}
	
	printf("Opening output file\n");
	if(avformat_alloc_output_context2(&output_ctx, 0, "mpegts", argv[3]) != 0)
	{
		fprintf(stderr, "Could not allocate output context\n");
		return 1;
	}
	if(split_size == 0)
	{
		if(avio_open(&output_ctx->pb, argv[optind+2], AVIO_FLAG_WRITE) != 0)
		{
			fprintf(stderr, "Could not open output file\n");
			return 1;
		}
	}
	else
	{
		output_ctx->pb = io_split_create(argv[optind+2], split_size);
		if(!output_ctx->pb)
		{
			fprintf(stderr, "Could not open output file\n");
			return 1;
		}
	}
	
	output_ctx->oformat->flags |= AVFMT_TS_NONSTRICT;
	
	if(!setupHandlers(ctx, output_ctx, cutlist, &stream_mapping, audio_decoder))
		return 1;
	
	printf(" [+] Output streams:\n");
	av_dump_format(output_ctx, 0, argv[optind+2], true);
	
	avformat_write_header(output_ctx, 0);
	
	AVPacket packet;
	while(av_read_frame(ctx, &packet) == 0)
	{
		StreamMap::iterator it = stream_mapping.find(packet.stream_index);
		if(it == stream_mapping.end())
			continue;
		
		AVStream* stream = ctx->streams[packet.stream_index];
		int percent = av_rescale(packet.dts - stream->start_time, 100, stream->duration);
		
		int granularity = verbose ? 1 : 10;
		
		if(percent / granularity != last_percent_done / 10 && percent > last_percent_done)
		{
			printf("%02d%% done (approximation)\n", percent);
			if(verbose)
				fflush(stdout);
			
			last_percent_done = percent;
		}
		
		if(it->second->handlePacket(&packet) != 0)
			return 1;
		
		av_free_packet(&packet);
		
		bool allFinished = true;
		for(StreamMap::const_iterator it = stream_mapping.begin();
			it != stream_mapping.end(); ++it)
		{
			if(it->second->active())
				allFinished = false;
		}
		if(allFinished)
			break;
	}
	
	av_write_trailer(output_ctx);
	
	if(split_size == 0)
		avio_close(output_ctx->pb);
	else
		io_split_close(output_ctx->pb);
	avformat_free_context(output_ctx);
}
