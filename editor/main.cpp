
#include <QtGui/QApplication>

#include "editor.h"

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	
	av_register_all();
	
	Editor* editor = new Editor();
	editor->show();

	return app.exec();
}

