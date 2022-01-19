#include "v6ap.h"
#include "Archipelago.h"

#include "Graphics.h"
#include "Vlogging.h"
#include "Entity.h"

#include <vector>
#include <string>

#define SET_PLAYER_POS(x,y) obj.entities[obj.getplayer()].xp = (x) * 8; \
                obj.entities[obj.getplayer()].yp = ((y) * 8) - obj.entities[obj.getplayer()].h; \
                obj.entities[obj.getplayer()].lerpoldxp = obj.entities[obj.getplayer()].xp; \
                obj.entities[obj.getplayer()].lerpoldyp = obj.entities[obj.getplayer()].yp; \

bool trinketsCollected[V6AP_NUM_CHECKS];
bool trinketsPending[V6AP_NUM_CHECKS];
int door_unlock_cost = 0;
bool location_checks[V6AP_NUM_CHECKS];

void V6AP_RecvItem(int);
void V6AP_CheckLocation(int);
void none() {}

void V6AP_SetDoorCost(int cost) {
    door_unlock_cost = cost;
}

void V6AP_ResetItems() {
    for (int i = 0; i < V6AP_NUM_CHECKS; i++) {
        location_checks[i] = false;
        trinketsCollected[i] = false;
        trinketsPending[i] = false;
    }
}

void V6AP_Init(const char* ip, const char* player_name, const char* passwd) {
    if (AP_IsInit()) {
        return;
    }

    AP_Init(ip, "VVVVVV", player_name, passwd);
    AP_SetDeathLinkSupported(true);
    AP_SetItemClearCallback(&V6AP_ResetItems);
    AP_SetItemRecvCallback(&V6AP_RecvItem);
    AP_SetLocationCheckedCallback(&V6AP_CheckLocation);
    AP_RegisterSlotDataIntCallback("DoorCost", &V6AP_SetDoorCost);
    AP_SetDeathLinkRecvCallback(&none);
    AP_Start();
}

void V6AP_SendItem(int idx) {
    if (idx >= V6AP_NUM_CHECKS || location_checks[idx]) {
        vlog_warn("V6AP: Something funky is happening...");
        return;
    }
    location_checks[idx] = true;
    AP_SendItem(idx + V6AP_ID_OFFSET);
}

bool V6AP_ItemPending() {
    for (int i = 0; i < V6AP_NUM_CHECKS; i++) {
        if (trinketsPending[i]) return true;
    }
    return false;
}

void V6AP_RecvItem(int item_id) {
    trinketsPending[item_id - V6AP_ID_OFFSET] = true;
}

void V6AP_CheckLocation(int loc_id) {
    location_checks[loc_id - V6AP_ID_OFFSET] = true;
}

void V6AP_RecvClear() {
    int i = 0;
    for (; i < V6AP_NUM_CHECKS; i++) {
        if (trinketsPending[i]) {
            trinketsPending[i] = false;
            break;
        }
    }
    trinketsCollected[i] = true;
}

void V6AP_StoryComplete() {
    AP_StoryComplete();
}

int V6AP_GetTrinkets() {
    int c = 0;
    for (int i = 0; i < V6AP_NUM_CHECKS; i++) {
        if (trinketsCollected[i]) c++;
    }
    return c;
}

void V6AP_DeathLinkSend() {
    return AP_DeathLinkSend();
}

bool V6AP_DeathLinkPending() {
    return AP_DeathLinkPending();
}

void V6AP_DeathLinkClear() {
    AP_DeathLinkClear();
}

const bool* V6AP_Locations() {
    return location_checks;
}

const bool* V6AP_Trinkets() {
    return trinketsCollected;
}

void printMsg(std::vector<std::string> msg) {
    graphics.createtextbox(msg.at(0), -7, -7, 174, 174, 174);
    for (unsigned int i = 1; i < msg.size(); i++) {
        graphics.addline(msg.at(i));
    }
    graphics.textboxtimer(60);
}

void V6AP_PrintNext() {
    if (!AP_IsMessagePending()) return;
    std::vector<std::string> msg = AP_GetLatestMessage();
    printMsg(msg);
    AP_ClearLatestMessage();
}



bool V6AP_RoomAvailable(int x, int y) {
    if (door_unlock_cost == 0) return true;
    switch (x) {
        case 102:
            if (y == 116 && game.roomx == 101 && game.roomy == 116) { //Get Ready to Bounce (Entrance Laboratory)
                for (int i = 0; i < door_unlock_cost; i++) {
                    if (!trinketsCollected[i]) {
                        std::vector<std::string> msg;
                        msg.push_back("You need Trinkets");
                        msg.push_back(std::to_string(1) + " through " + std::to_string(door_unlock_cost));
                        msg.push_back("to continue here");
                        printMsg(msg);
                        SET_PLAYER_POS(36, 16);
                        return false;
                    }
                }
                return true;
            }
        break;
        case 108:
            if (y == 109 && game.roomx == 107 && game.roomy == 109) { //Teleporter Divot (Entrance Tower)
                for (int i = door_unlock_cost; i < door_unlock_cost*2; i++) {
                    if (!trinketsCollected[i]) {
                        std::vector<std::string> msg;
                        msg.push_back("You need Trinkets");
                        msg.push_back(std::to_string(door_unlock_cost+1) + " through " + std::to_string(door_unlock_cost*2));
                        msg.push_back("to continue here");
                        printMsg(msg);
                        SET_PLAYER_POS(36, 20);
                        return false;
                    }
                }
                return true;
            }
        break;
        case 111:
            if (y == 113 && game.roomx == 111 && game.roomy == 114) { //Something filter? (Entrance Space Station 2)
                for (int i = door_unlock_cost*2; i < door_unlock_cost*3; i++) {
                    if (!trinketsCollected[i]) {
                        std::vector<std::string> msg;
                        msg.push_back("You need Trinkets");
                        msg.push_back(std::to_string(door_unlock_cost*2+1) + " through " + std::to_string(door_unlock_cost*3));
                        msg.push_back("to continue here");
                        printMsg(msg);
                        SET_PLAYER_POS(15, 10);
                        return false;
                    }
                }
                return true;
            }
        break;
        case 114:
            if (y == 101 && game.roomx == 113 && game.roomy == 101) { // This is how it is (Entrance Warp Zone)
                for (int i = door_unlock_cost*3; i < door_unlock_cost*4; i++) {
                    if (!trinketsCollected[i]) {
                        std::vector<std::string> msg;
                        msg.push_back("You need Trinkets");
                        msg.push_back(std::to_string(door_unlock_cost*3+1) + " through " + std::to_string(door_unlock_cost*4));
                        msg.push_back("to continue here");
                        printMsg(msg);
                        SET_PLAYER_POS(35, 11);
                        return false;
                    }
                }
                return true;
            }
        break;
    }
    return true;
}