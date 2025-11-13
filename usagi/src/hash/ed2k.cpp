#include "ed2k.h"
#include "../logger.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <QMutexLocker>

// Initialize static members
QMutex ed2k::fileIOMutex;
bool ed2k::useSerializedIO = false;

ed2k::ed2k()
{
}

void ed2k::setSerializedIO(bool enabled)
{
	useSerializedIO = enabled;
	if (enabled)
	{
		LOG("ed2k: Serialized I/O enabled - optimized for HDD performance");
	}
	else
	{
		LOG("ed2k: Serialized I/O disabled - optimized for SSD/parallel I/O");
	}
}

bool ed2k::getSerializedIO()
{
	return useSerializedIO;
}

qint64 ed2k::calculateHashParts(qint64 fileSize)
{
	// Calculate number of 102400-byte parts needed for this file
	qint64 numParts = ceil(static_cast<double>(fileSize) / 102400.0);
	if (numParts == 0) numParts = 1; // At least 1 part for empty files
	return numParts;
}

void ed2k::Init()
{
	MD4::Init(&context1);
	MD4::Init(&context2);
	b = block = 0;
}

void ed2k::Update(unsigned char *input, unsigned int inputLen)
{
	MD4::Update(&context1, input, inputLen);
	b++;
	if(b == 95)
	{
		MD4::Final(digest1, &context1);
		MD4::Update(&context2, (unsigned char*)digest1, 16);
		MD4::Init(&context1);
		b = 0;
		block++;
	}
}

void ed2k::Final()
{
	if(block == 0)
	{
		MD4::Final(digest1, &context1);
		memcpy(digest, digest1, 16);
	}
	else if(b == 0)
	{
	    MD4::Final(digest2, &context2);
		memcpy(digest, digest2, 16);
	}
	else
	{
	    MD4::Final(digest1, &context1);
		MD4::Update(&context2, (unsigned char*)digest1, 16);
		MD4::Final(digest2, &context2);
		memcpy(digest, digest2, 16);
	}
}

int ed2k::ed2khash(QString filepath)
{
	dohash = 1;
	QFileInfo fileinfo(filepath);
	unsigned int i = 0;
	
	ed2khashstr.clear();
	Init();
	
	// Phase 1: File I/O (potentially serialized for HDD performance)
	// Read entire file into memory in chunks, computing hash as we go
	QFile file(fileinfo.absoluteFilePath());
	
	// Optional: Lock mutex for file I/O to prevent disk head thrashing on HDDs
	QMutex *ioMutex = useSerializedIO ? &fileIOMutex : nullptr;
	if (ioMutex)
	{
		ioMutex->lock();
	}
	
	(void)file.open(QIODevice::ReadOnly);
	
	if(file.isOpen())
	{
        fileSize = file.size();
        // Calculate number of parts correctly
        int parts = (fileSize + 102399) / 102400; // Ceiling division
		int partsdone = 0;
		
		do
		{
			if(dohash == 0)
			{
				file.close();
				if (ioMutex)
				{
					ioMutex->unlock();
				}
				return 3; // hashing stopped by user
			}
			i = file.read(buffer, 102400);
			Update((unsigned char *)buffer, i);
			partsdone++;
			
			// Emit progress signal for each part
			emit notifyPartsDone(parts, partsdone);
		}while(!file.atEnd());
		
		file.close();
		
		// Unlock mutex after file I/O is complete
		// This allows other threads to start reading their files
		// while this thread completes the MD4 finalization (CPU work)
		if (ioMutex)
		{
			ioMutex->unlock();
		}
		
		// Phase 2: Finalization (parallel CPU work, not locked)
		Final();
		ed2kfilestruct hash;
		hash.filename = fileinfo.fileName();
		hash.size = fileinfo.size();
		hash.hexdigest = HexDigest().c_str();
		emit notifyFileHashed(hash);
		ed2khashstr = QString("ed2k://|file|%1|%2|%3|/").arg(fileinfo.fileName()).arg(fileinfo.size()).arg(HexDigest().c_str());
		return 1; // ok
	}
	else
	{
		if (ioMutex)
		{
			ioMutex->unlock();
		}
		ed2khashstr = QString("File %1 does not exist.").arg(fileinfo.absoluteFilePath());
		return 2; // error opening file
	}
	return 0;
}

std::string ed2k::HexDigest()
{
	std::string hash;
	std::stringstream ss;

	for(int i = 0; i < 16; i++)
	{
		ss << std::setfill('0') << std::setw(2) << std::hex << (int)digest[i];
	}
	hash = ss.str();
	return hash;
}

QString ed2k::FileName()
{
	return fileName;
}

qint64 ed2k::FileSize()
{
	return fileSize;
}

void ed2k::getNotifyStopHasher()
{
	dohash = 0;
}
