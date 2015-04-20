#ifndef MAIN_H
#define MAIN_H
#include "anidbapi.h"

struct settings_struct
{
	QString login;
	QString password;
};

class myAniDBApi : public AniDBApi
{
	Q_OBJECT
public:
	void Debug(QString msg);
	myAniDBApi(QString client_, int clientver_): AniDBApi(client_, clientver_)
	{

	}
signals:
	void notifyLogAppend(QString);
};

#endif // MAIN_H
