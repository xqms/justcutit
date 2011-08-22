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

#include "streamhandler.h"
#include "cutlist.h"

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
	fprintf(dest, "Usage: justcutit <file> <cutlist> <output-file>\n");
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

bool setupHandlers(AVFormatContext* input, AVFormatContext* output, const CutPointList& cutlist, StreamMap* map)
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
				oprogram->stream_index, oprogram->nb_stream_indexes
			);
			oprogram->stream_index[oprogram->nb_stream_indexes-1] = ostream->index;
			
			// Setup stream handler
			handler->setCutList(cutlist);
			handler->setOutputContext(output);
			handler->setOutputStream(ostream);
			
			if(handler->init() != 0)
			{
				fprintf(stderr, "Error: Could not initialize stream handler for stream %d\n",
					istream->index
				);
				return false;
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
	
	if(!setupHandlers(ctx, output_ctx, cutlist, &stream_mapping))
		return 1;
	
	printf(" [+] Output streams:\n");
	av_dump_format(output_ctx, 0, argv[3], true);
	
	
	avformat_write_header(output_ctx, 0);
	
	QApplication app(argc, argv);
	
	AVPacket packet;
	while(av_read_frame(ctx, &packet) == 0)
	{
		StreamMap::iterator it = stream_mapping.find(packet.stream_index);
		if(it == stream_mapping.end())
			continue;
		
		if(it->second->handlePacket(&packet) != 0)
			return 1;
		
		av_free_packet(&packet);
	}
	
	av_write_trailer(output_ctx);
	
	avio_close(output_ctx->pb);
	avformat_free_context(output_ctx);
}