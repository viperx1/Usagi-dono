#include "ed2k.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cmath>

ed2k::ed2k()
{
};

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
	QFile file(fileinfo.absoluteFilePath());
	file.open(QIODevice::ReadOnly);

	ed2khashstr.clear();
	Init();
	if(file.isOpen())
	{
        fileSize = file.size();
        qint64 a = fileSize;
        qint64 b = a/102400;
        qint64 c = ceil(b);
        int parts = c;
		int partsdone = 0;
		Init();
		do
		{
			if(dohash == 0)
			{
				return 3; // hashing stopped by user
			}
			i = file.read(buffer, 102400);
			Update((unsigned char *)buffer, i);
			emit notifyPartsDone(parts, ++partsdone);
		}while(!file.atEnd());
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

void ed2k::Debug(QString msg)
{
	std::cout << "debug" << std::endl;
}
