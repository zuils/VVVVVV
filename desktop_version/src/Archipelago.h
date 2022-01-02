#pragma once

#include "Entity.h"
#include <queue>
#define V6MW_NUM_CHECKS 20

void V6MW_Init(std::string, std::string,std::string);

// Sends LocationCheck for given index
void V6MW_SendItem(int);

// Local Trinket Amount
int V6MW_GetTrinkets();

//Print Next Message to Screen
void V6MW_PrintNext();

//Returns true if there is an item queued up
bool V6MW_RecvItem();
//Clear RecvItem flag
void V6MW_RecvClear();

//DeathLink functions
void V6MW_DeathLinkSend();
bool V6MW_DeathLinkRecv();
void V6MW_DeathLinkClear();

// Currently checked locations
const bool* V6MW_Locations();

// Called when Story completed, sends StatusUpdate
void V6MW_StoryComplete();