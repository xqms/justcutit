// TS Index file base class
// Author: Max Schwarz <Max@x-quadraht.de>

#include "indexfile.h"

#include <vector>

#include <string.h>
#include <stdio.h>

IndexFile::IndexFile(AVFormatContext* ctx)
 : m_ctx(ctx)
{
}

IndexFile::~IndexFile()
{
}

// FACTORY
struct RegEntry
{
	const char* name;
	IndexFileFactory::Creator creator;
	IndexFileFactory::Detector detector;
};
typedef std::vector<RegEntry> RegEntryList;
RegEntryList* g_entryList;

void IndexFileFactory::registerIndexFile(
	const char* name,
	IndexFileFactory::Detector detector,
	IndexFileFactory::Creator creator)
{
	if(!g_entryList)
		g_entryList = new RegEntryList;
	
	RegEntry entry = {name, creator, detector};
	g_entryList->push_back(entry);
}

IndexFileFactory::Registerer::Registerer(
	const char* name,
	IndexFileFactory::Detector detector,
	IndexFileFactory::Creator creator)
{
	IndexFileFactory::registerIndexFile(name, detector, creator);
}

IndexFile* IndexFileFactory::detectIndexFile(
	AVFormatContext* input, const char* filename)
{
	if(!g_entryList)
		return NULL;
	
	for(int i = 0; i < g_entryList->size(); ++i)
	{
		const RegEntry& entry = g_entryList->at(i);
		
		if(!entry.detector(input, filename))
			continue;
		
		IndexFile* f = entry.creator(input);
		
		if(f->open(NULL, filename))
			return f;
		
		delete f;
	}
	
	return NULL;
}

int IndexFileFactory::formatCount()
{
	if(!g_entryList)
		return 0;
	
	return g_entryList->size();
}

const char* IndexFileFactory::formatName(int idx)
{
	if(!g_entryList)
		return NULL;
	
	return g_entryList->at(idx).name;
}

static const RegEntry* getEntry(const char* name)
{
	if(!g_entryList)
		return NULL;
	
	for(int i = 0; i < g_entryList->size(); ++i)
	{
		const RegEntry& entry = g_entryList->at(i);
		if(strcmp(entry.name, name) == 0)
			return &entry;
	}
	
	return NULL;
}

IndexFile* IndexFileFactory::openWith(const char* format_name,
	const char* filename, AVFormatContext* input, const char* stream_filename)
{
	const RegEntry* entry = getEntry(format_name);
	
	if(!entry)
	{
		fprintf(stderr, "Index file format '%s' is not supported\n",
			format_name);
		return NULL;
	}
	
	IndexFile* f = entry->creator(input);
	
	if(!f->open(filename, stream_filename))
	{
		delete f;
		return NULL;
	}
	
	return f;
}
