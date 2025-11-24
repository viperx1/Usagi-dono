#include "main.h"

// Global pointer to the AniDB API instance
// This is initialized in window.cpp and used by various components
myAniDBApi *adbapi = nullptr;

myAniDBApi::myAniDBApi(QString client_, int clientver_)
    : AniDBApi(client_, clientver_)
{
}

myAniDBApi::~myAniDBApi()
{
}
