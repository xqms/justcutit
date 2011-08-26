
#include <QtGui/QApplication>
#include <QtGui/QMessageBox>

#include <getopt.h>

#include "editor.h"
#include "indexfile.h"

void dump_indexFormats()
{
	printf("Available index formats:\n");
	for(int i = 0; i < IndexFileFactory::formatCount(); ++i)
	{
		printf(" - %s\n", IndexFileFactory::formatName(i));
	}
}

void usage(FILE* f)
{
	fprintf(f,
		"Usage: justcutit_editor [options] [file]\n"
		"\n"
		"Options:\n"
		" --index FILE        Use FILE as index file\n"
		" --index-fmt FORMAT  Use FORMAT as index file format\n"
		"                     This is required if you use --index!\n"
		" --index-fmt help    Display available index formats\n"
	);
}

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	
	av_register_all();
	
	// Command line parsing
	const char* file = 0;
	const char* indexFormat = 0;
	const char* indexFile = 0;
	
	while(1)
	{
		int option_index = 0;
		static struct option long_options[] = {
			{"index", required_argument, 0, 'i'},
			{"index-fmt", required_argument, 0, 'f'},
			{"help", no_argument, 0, 'h'},
			{0, 0, 0, 0}
		};
		
		int c = getopt_long(argc, argv, "h", long_options, &option_index);
		if(c == -1)
			break;
		
		switch(c)
		{
			case 'i':
				indexFile = optarg;
				break;
			case 'f':
				if(strcmp(optarg, "help") == 0)
				{
					dump_indexFormats();
					return 0;
				}
				
				indexFormat = optarg;
				break;
			case 'h':
				usage(stdout);
				return 0;
			default:
				usage(stderr);
				return 1;
		}
	}
	
	if(argc - optind > 1)
	{
		usage(stderr);
		return 1;
	}
	
	if((indexFormat || indexFile) && (!indexFormat || !indexFile))
	{
		fprintf(stderr, "Error: need both --index and --index-fmt, fallback "
			"to auto detection.\n");
		
		indexFormat = indexFile = 0;
	}
	
	Editor* editor = new Editor();
	editor->show();
	
	// Display editor, so OpenGL initialization takes place
	QCoreApplication::processEvents();
	
	if(argc > optind)
	{
		file = argv[optind];
		
		if(editor->loadFile(file) != 0)
		{
			fprintf(stderr, "Could not load input file\n");
			return 1;
		}
	}
	else if(editor->loadFile() != 0)
	{
		QMessageBox::critical(0, "Error", "Could not load input file");
		return 1;
	}
	
	if(indexFile)
	{
		IndexFileFactory factory;
		IndexFile* index = factory.openWith(
			indexFormat,
			indexFile,
			editor->formatContext(),
			file
		);
		
		if(!index)
			return 1;
		
		editor->takeIndexFile(index);
	}
	else
		editor->autoDetectIndexFile();
	
	editor->show();
	
	return app.exec();
}

