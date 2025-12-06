#ifndef MAIN_H
#define MAIN_H
#include "anidbapi.h"

class myAniDBApi : public AniDBApi
{
	Q_OBJECT
public:
	myAniDBApi(QString client_, int clientver_);
	virtual ~myAniDBApi();
signals:
	void notifyLogAppend(QString);
};

#endif // MAIN_H
