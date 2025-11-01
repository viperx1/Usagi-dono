#include "hasherthread.h"
#include "main.h"
#include "logger.h"
#include <iostream>

extern myAniDBApi *adbapi;

void HasherThread::run()
{
	Logger::log("HasherThread started processing files [hasherthread.cpp]");
	while(!files.isEmpty())
	{
		switch(adbapi->ed2khash(files.first()))
		{
		case 1:
			files.pop_front();
			emit sendHash(adbapi->ed2khashstr);
			break;
		case 2:
			break;
		case 3:
			files.clear();
			break;
		default:
			break;
		}
	}
}

void HasherThread::getFiles(QStringList files_)
{
	files = files_;
	start();
}
