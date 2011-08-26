// Kathrein index file, found on model UFS-910
// Author: Max Schwarz <Max@x-quadraht.de>

#include "kathrein.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern "C"
{
#include <libavutil/avutil.h>
}

#define LOG_PREFIX "[kathrein]"
#define DEBUG 1
#include <common/log.h>

const char* const FILE_NAME = "index.timeidx";

KathreinIndexFile::KathreinIndexFile(AVFormatContext* ctx)
 : IndexFile(ctx)
 , m_table(0)
{
}

KathreinIndexFile::~KathreinIndexFile()
{
	free(m_table);
}

char* KathreinIndexFile::fabricateFilename(const char* stream_filename)
{
	const char* tsfile = strrchr(stream_filename, '/');
	
	if(!tsfile)
		return false;
	
	int slash = tsfile - stream_filename;
	
	char* filename = (char*)malloc(
		slash + 1 + strlen(FILE_NAME) + 1
	);
	
	if(!filename)
		return false;
	
	strncpy(filename, stream_filename, slash+1);
	strcpy(filename+slash+1, FILE_NAME);
	
	return filename;
}

bool KathreinIndexFile::detect(AVFormatContext*,
	const char* stream_filename)
{
	char* filename = fabricateFilename(stream_filename);
	
	log_debug("fabricated file name: '%s'", filename);
	
	// Try if file is present
	FILE* f = fopen(filename, "r");
	
	free(filename);
	
	bool ret = f;
	
	if(!ret)
		log_debug_perror("Could not open kathrein index file");
	
	fclose(f);
	
	return ret;
}

bool KathreinIndexFile::open(const char* filename, const char* stream_filename)
{
	char* my_filename;
	if(!filename)
		filename = my_filename = fabricateFilename(stream_filename);
	
	FILE* f = fopen(filename, "rb");
	
	free(my_filename);
	
	if(!f)
		return false;
	
	if(fread(&m_count, sizeof(m_count), 1, f) != 1)
		return false;
	
	if(m_count == 0)
	{
		log_debug_perror("Could not read table size");
		return false;
	}
	
	m_table = (TableEntry*)malloc(sizeof(TableEntry) * m_count);
	if(!m_table)
	{
		log_debug_perror("Could not allocate memory");
		return false;
	}
	
	if(fread((void*)m_table, sizeof(TableEntry), m_count, f) != m_count)
	{
		log_debug_perror("Could not read table");
		return false;
	}
	
	return true;
}

loff_t KathreinIndexFile::bytePositionForPTS(int64_t pts)
{
	int64_t msec = pts / (AV_TIME_BASE / 1000);
	
	// Perform binary search in m_table
	//  we want the last entry whose time is lower than msec
	int idx_start = 0;
	int idx_end = m_count;
	
	int cmp;
	while(1)
	{
		cmp = (idx_start + idx_end) / 2;
		
		if(idx_start == idx_end)
			break;
		if(idx_end - idx_start == 1)
			break;
		
		if(m_table[cmp].time_ms > msec)
			idx_end = cmp;
		else
			idx_start = cmp;
	}
	
	if(cmp > 0)
		cmp -= 1;
	
	return m_table[cmp].offset;
}

REGISTER_INDEX_FILE("kathrein", KathreinIndexFile)
