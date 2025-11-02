#include "hasherthread.h"
#include "main.h"
#include "logger.h"
#include <iostream>

extern myAniDBApi *adbapi;

void HasherThread::run()
{
	Logger::log("HasherThread started processing files [hasherthread.cpp]");
	shouldStop = false;
	
	// Request the first file to hash
	emit requestNextFile();
	
	// The thread will remain running until stop() is called or no more files are available
	// Processing happens in hashFile() slot which is called from the main thread
	exec(); // Enter event loop to process signals
}

void HasherThread::stop()
{
	shouldStop = true;
	quit(); // Exit the event loop
	wait(); // Wait for thread to finish
}

void HasherThread::hashFile(QString filePath)
{
	if (shouldStop || filePath.isEmpty())
	{
		// No more files to hash or stop requested
		quit(); // Exit the event loop, which will end the thread
		return;
	}
	
	currentFile = filePath;
	
	switch(adbapi->ed2khash(filePath))
	{
	case 1:
		emit sendHash(adbapi->ed2khashstr);
		// Request the next file after successfully hashing this one
		emit requestNextFile();
		break;
	case 2:
		// Error occurred, but continue with next file
		emit requestNextFile();
		break;
	case 3:
		// Stop requested during hashing
		shouldStop = true;
		quit();
		break;
	default:
		// Unknown result, request next file
		emit requestNextFile();
		break;
	}
}
