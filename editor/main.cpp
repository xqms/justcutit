
#include <QtGui/QApplication>
#include <QtGui/QMessageBox>
#include <QtCore/QTranslator>
#include <QtCore/QLibraryInfo>

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
		" --proceed-to PATH   Display \"Proceed\" button which saves the\n"
		"                     cutlist to PATH and exits with status 10\n"
	);
}

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	
	// i18n
	QTranslator qtTranslator;
	qtTranslator.load("qt_" + QLocale::system().name(),
		QLibraryInfo::location(QLibraryInfo::TranslationsPath));
	app.installTranslator(&qtTranslator);
	
	QTranslator appTranslator;
	appTranslator.load("editor_" + QLocale::system().name());
	app.installTranslator(&appTranslator);
	
	av_register_all();
	
	// Command line parsing
	QString file;
	QString proceedTo;
	const char* indexFormat = 0;
	const char* indexFile = 0;
	
	while(1)
	{
		int option_index = 0;
		static struct option long_options[] = {
			{"index", required_argument, 0, 'i'},
			{"index-fmt", required_argument, 0, 'f'},
			{"proceed-to", required_argument, 0, 'p'},
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
			case 'p':
				proceedTo = optarg;
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
		file = argv[optind];
	
	if(editor->loadFile(file) != 0)
	{
		QMessageBox::critical(0, QApplication::tr("Error"),
			QApplication::tr("Could not load input file"));
		return 1;
	}
	
	if(indexFile)
	{
		IndexFileFactory factory;
		IndexFile* index = factory.openWith(
			indexFormat,
			indexFile,
			editor->formatContext(),
			file.toLocal8Bit().constData()
		);
		
		if(!index)
			return 1;
		
		editor->takeIndexFile(index);
	}
	else
		editor->autoDetectIndexFile();
	
	if(!proceedTo.isNull())
		editor->proceedTo(proceedTo);
	
	editor->show();
	
	return app.exec();
}

