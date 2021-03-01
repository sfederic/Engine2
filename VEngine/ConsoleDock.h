#pragma once

#include <qdockwidget.h>

class QTextEdit;

//Console for debug information in-editor
class ConsoleDock : public QDockWidget
{
public:
	ConsoleDock();
	void Print(const char* string);

	QTextEdit* consoleMessageBox;
};

