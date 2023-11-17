#include "anidbapi.h"

void AniDBApi::setUsername(QString username)
{
	if(username.length() > 0)
	{
		AniDBApi::username = username;
		QSqlQuery query;
		QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'username', '%1');").arg(username);
		query.exec(q);
//		Debug(AniDBApi::username);
	}
}
void AniDBApi::setPassword(QString password)
{
	if(password.length() > 0)
	{
		AniDBApi::password = password;
		QSqlQuery query;
        QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'password', '%1');").arg(password);
		query.exec(q);
//		Debug(AniDBApi::password);
	}
}

void AniDBApi::setLastDirectory(QString directory)
{
	if(directory.length() > 0)
	{
		AniDBApi::lastdirectory = directory;
		QSqlQuery query;
		QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'lastdirectory', '%1');").arg(directory);
		query.exec(q);
//		Debug(AniDBApi::lastDirectory);
	}
}

QString AniDBApi::getUsername()
{
	return AniDBApi::username;
}

QString AniDBApi::getPassword()
{
	return AniDBApi::password;
}

QString AniDBApi::getLastDirectory()
{
	return AniDBApi::lastdirectory;
}
