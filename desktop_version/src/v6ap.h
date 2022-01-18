#pragma once

#define V6AP_NUM_CHECKS 20
#define V6AP_NUM_LOCKS 4

#define V6AP_ID_OFFSET 2515000

void V6AP_Init(const char*, const char*, const char*);

// Sends LocationCheck for given index
void V6AP_SendItem(int);

// Local Trinket Amount
int V6AP_GetTrinkets();

// Print Next Message to Screen
void V6AP_PrintNext();

// Returns true if there is an item queued up
bool V6AP_ItemPending();
// Receive one item, if available
void V6AP_RecvClear();

// DeathLink functions
void V6AP_DeathLinkSend();
bool V6AP_DeathLinkPending();
void V6AP_DeathLinkClear();

// Differentiate locations and trinkets
const bool* V6AP_Locations();
const bool* V6AP_Trinkets();

// Called when Story completed, sends StatusUpdate
void V6AP_StoryComplete();

// Check if accessible under current MultiWorld ruleset, if not resets player position to safe value
bool V6AP_RoomAvailable(int,int);