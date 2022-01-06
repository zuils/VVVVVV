#include "Archipelago.h"

#include "Graphics.h"
#include "Vlogging.h"

#include "ixwebsocket/IXNetSystem.h"
#include "ixwebsocket/IXWebSocket.h"
#include "ixwebsocket/IXUserAgent.h"

#include "json/json.h"
#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>
#include <queue>
#include <string>
#include <chrono>
#include <cstdlib>

#define ADD_TO_MSGQUEUE(x,y) messageQueue.push(std::pair<std::string,int>(x,y))

#define SET_PLAYER_POS(x,y) obj.entities[obj.getplayer()].xp = (x) * 8; \
                obj.entities[obj.getplayer()].yp = ((y) * 8) - obj.entities[obj.getplayer()].h; \
                obj.entities[obj.getplayer()].lerpoldxp = obj.entities[obj.getplayer()].xp; \
                obj.entities[obj.getplayer()].lerpoldyp = obj.entities[obj.getplayer()].yp; \

bool init = false;
bool auth = false;
int v6mw_player_id;
std::string v6mw_player_name;
std::string v6mw_ip;
std::string v6mw_passwd;
int v6mw_uuid = 0;
bool trinketsCollected[V6MW_NUM_CHECKS];
bool trinketsPending[V6MW_NUM_CHECKS];
int door_unlock_cost = 0;
bool deathlinkstat = false;
bool enable_deathlink = false;
int deathlink_amnesty = 0;
int cur_deathlink_amnesty = 0;
bool location_checks[V6MW_NUM_CHECKS];
std::queue<std::pair<std::string,int>> messageQueue;
std::map<int, std::string> map_player_id_name;
std::map<int, std::string> map_location_id_name;
std::map<int, std::string> map_item_id_name;

ix::WebSocket webSocket;
Json::Reader reader;
Json::FastWriter writer;

bool parse_response(std::string msg, std::string &request);
void APSend(std::string req);

void V6MW_Init(std::string ip, std::string player_name, std::string passwd) {
    if (init) {
        return;
    }

    std::srand(std::time(nullptr)); // use current time as seed for random generator

    v6mw_player_name = player_name;
    v6mw_ip = ip;
    v6mw_passwd = passwd;

    vlog_info("V6MW: Initializing...");

    //Connect to server
    ix::initNetSystem();
    std::string url("ws://" + v6mw_ip);
    webSocket.setUrl(url);
    webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg)
        {
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                std::string request;
                vlog_debug("V6MW: Receiving from Archipelago");
                if (parse_response(msg->str, request)) {
                    APSend(request);
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Open)
            {
                vlog_info("V6MW: Connected to Archipelago");
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                auth = false;
                vlog_error("V6MW: Error connecting to Archipelago. Retries: %d", msg->errorInfo.retries-1);
            }
        }
    );

    //Initialize checked locations, collected and pending trinkets
    for (int i = 0; i < V6MW_NUM_CHECKS; i++) {
        location_checks[i] = false;
        trinketsCollected[i] = false;
        trinketsPending[i] = false;
    }

    init = true;
    webSocket.start();
}

void V6MW_SendItem(int idx) {
    if (idx >= V6MW_NUM_CHECKS || location_checks[idx] || !auth) {
        vlog_warn("V6MW: Something funky is happening...");
        return;
    }
    location_checks[idx] = true;
    vlog_info(("V6MW: Checked " + map_location_id_name.at(idx + 2515000) + ". Informing Archipelago...").c_str());
    Json::Value req_t;
    req_t[0]["cmd"] = "LocationChecks";
    req_t[0]["locations"][0] = idx + 2515000;
    APSend(writer.write(req_t));
}

bool V6MW_RecvItem() {
    for (int i = 0; i < V6MW_NUM_CHECKS; i++) {
        if (trinketsPending[i]) return true;
    }
    return false;
}

void V6MW_RecvClear() {
    int i = 0;
    for (; i < V6MW_NUM_CHECKS; i++) {
        if (trinketsPending[i]) {
            trinketsPending[i] = false;
            break;
        }
    }
    trinketsCollected[i] = true;
}

void V6MW_StoryComplete() {
    if (auth) {
        Json::Value req_t;
        req_t[0]["cmd"] = "StatusUpdate";
        req_t[0]["status"] = 30; //CLIENT_GOAL
        APSend(writer.write(req_t));
    }
}

int V6MW_GetTrinkets() {
    int c = 0;
    for (int i = 0; i < V6MW_NUM_CHECKS; i++) {
        if (trinketsCollected[i]) c++;
    }
    return c;
}

bool V6MW_DeathLinkRecv() {
    return deathlinkstat;
}

void V6MW_DeathLinkClear() {
    deathlinkstat = false;
    cur_deathlink_amnesty = deathlink_amnesty;
}

void V6MW_DeathLinkSend() {
    if (cur_deathlink_amnesty > 0) {
        cur_deathlink_amnesty--;
        return;
    }
    if (auth && enable_deathlink) {
        std::chrono::time_point<std::chrono::system_clock> timestamp = std::chrono::system_clock::now();
        Json::Value req_t;
        req_t[0]["cmd"] = "Bounce";
        req_t[0]["data"]["time"] = std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count();
        req_t[0]["data"]["source"] = v6mw_player_name; // Name and Shame >:D
        req_t[0]["tags"][0] = "DeathLink";
        APSend(writer.write(req_t));
    }
}

const bool* V6MW_Locations() {
    return location_checks;
}

const bool* V6MW_Trinkets() {
    return trinketsCollected;
}

void V6MW_PrintNext() {
    if (messageQueue.empty()) return;
    graphics.createtextbox(messageQueue.front().first, -7, -7, 174, 174, 174);
    int amount = messageQueue.front().second;
    messageQueue.pop();
    for (int i = 0; i < amount; i++) {
        graphics.addline(messageQueue.front().first);
        messageQueue.pop();
    }
    graphics.textboxtimer(60);
}

bool V6MW_RoomAvailable(int x, int y) {
    if (!auth) return false;
    if (door_unlock_cost == 0) return true;
    switch (x) {
        case 102:
            if (y == 116 && game.roomx == 101 && game.roomy == 116) { //Get Ready to Bounce (Entrance Laboratory)
                for (int i = 0; i < door_unlock_cost; i++) {
                    if (!trinketsCollected[i]) {
                        ADD_TO_MSGQUEUE("You need Trinkets", 2);
                        ADD_TO_MSGQUEUE(std::to_string(1) + " through " + std::to_string(door_unlock_cost), 1);
                        ADD_TO_MSGQUEUE("to continue here", 0);
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
                        ADD_TO_MSGQUEUE("You need Trinkets", 2);
                        ADD_TO_MSGQUEUE(std::to_string(door_unlock_cost+1) + " through " + std::to_string(door_unlock_cost*2), 1);
                        ADD_TO_MSGQUEUE("to continue here", 0);
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
                        ADD_TO_MSGQUEUE("You need Trinkets", 2);
                        ADD_TO_MSGQUEUE(std::to_string(door_unlock_cost*2+1) + " through " + std::to_string(door_unlock_cost*3), 1);
                        ADD_TO_MSGQUEUE("to continue here", 0);
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
                        ADD_TO_MSGQUEUE("You need Trinkets", 2);
                        ADD_TO_MSGQUEUE(std::to_string(door_unlock_cost*3+1) + " through " + std::to_string(door_unlock_cost*4), 1);
                        ADD_TO_MSGQUEUE("to continue here", 0);
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

bool parse_response(std::string msg, std::string &request) {
    Json::Value root;
    reader.parse(msg, root);
    for (unsigned int i = 0; i < root.size(); i++) {
        const char* cmd = root[0]["cmd"].asCString();
        if (!strcmp(cmd,"RoomInfo")) {
            if (!auth) {
                Json::Value req_t;
                v6mw_uuid = std::rand();
                req_t[i]["cmd"] = "Connect";
                req_t[i]["game"] = "VVVVVV";
                req_t[i]["name"] = v6mw_player_name;
                req_t[i]["password"] = v6mw_passwd;
                req_t[i]["uuid"] = v6mw_uuid;
                req_t[i]["tags"][0] = "DeathLink"; // Send Tag even though we don't know if we want these packages, just in case
                req_t[i]["version"]["major"] = "0";
                req_t[i]["version"]["minor"] = "2";
                req_t[i]["version"]["build"] = "2";
                req_t[i]["version"]["class"] = "Version";
                request = writer.write(req_t);
                auth = false;
                return true;
            }
        } else if (!strcmp(cmd,"Connected")) {
            // Avoid inconsistency if we disconnected before
            for (int i = 0; i < V6MW_NUM_CHECKS; i++) {
                location_checks[i] = false;
                trinketsCollected[i] = false;
                trinketsPending[i] = false;
            }

            vlog_info("V6MW: Authenticated");
            v6mw_player_id = root[i]["slot"].asInt();
            for (unsigned int j = 0; j < root[i]["checked_locations"].size(); j++) {
                //Sync checks with server
                int loc_id = root[i]["checked_locations"][j].asInt() - 2515000;
                location_checks[loc_id] = true;
                vlog_debug("V6MW: Archipelago reports Trinket %d as checked.", loc_id);
            }
            vlog_debug("V6MW: %d total already checked.", root[i]["checked_locations"].size());
            for (unsigned int j = 0; j < root[i]["players"].size(); j++) {
                map_player_id_name.insert(std::pair<int,std::string>(root[i]["players"][j]["slot"].asInt(),root[i]["players"][j]["alias"].asString()));
            }
            enable_deathlink = root[i]["slot_data"]["DeathLink"].asBool();
            deathlink_amnesty = root[i]["slot_data"]["DeathLink_Amnesty"].asInt();
            cur_deathlink_amnesty = deathlink_amnesty;
            if (enable_deathlink) vlog_info("V6MW: Enabled DeathLink.");
            door_unlock_cost = root[i]["slot_data"]["DoorCost"].asInt();
            if (door_unlock_cost > 0) vlog_info("V6MW: Enabled locked doors.");
            Json::Value req_t;
            req_t[0]["cmd"] = "GetDataPackage";
            request = writer.write(req_t);
            return true;
        } else if (!strcmp(cmd,"DataPackage")) {
            for (unsigned int j = 0; j < root[i]["data"]["games"].size(); j++) {
                for (auto itr : root[i]["data"]["games"]) {
                    for (auto itr2 : itr["item_name_to_id"].getMemberNames()) {
                        map_item_id_name.insert(std::pair<int,std::string>(itr["item_name_to_id"][itr2.c_str()].asInt(), itr2));
                    }
                    for (auto itr2 : itr["location_name_to_id"].getMemberNames()) {
                        map_location_id_name.insert(std::pair<int,std::string>(itr["location_name_to_id"][itr2.c_str()].asInt(), itr2));
                    }
                }
            }
            vlog_debug("V6MW: Built Data Maps.");
            Json::Value req_t;
            req_t[0]["cmd"] = "Sync";
            request = writer.write(req_t);
            auth = true;
            return true;
        } else if (!strcmp(cmd,"Print")) {
            vlog_info("AP: %s", root[i]["text"].asCString());
            ADD_TO_MSGQUEUE(root[i]["text"].asString(), 0);
        } else if (!strcmp(cmd,"PrintJSON")) {
            vlog_debug("Received PrintJSON");
            if (!strcmp(root[i].get("type","").asCString(),"ItemSend")) {
                ADD_TO_MSGQUEUE(map_item_id_name.at(root[i]["item"]["item"].asInt()) + " was sent!", 2);
                ADD_TO_MSGQUEUE("Sender: " + map_player_id_name.at(root[i]["item"]["player"].asInt()), 1);
                ADD_TO_MSGQUEUE("Receiving: " + map_player_id_name.at(root[i]["receiving"].asInt()), 0);
                vlog_info("Item from %s to %s", map_player_id_name.at(root[i]["item"]["player"].asInt()).c_str(), map_player_id_name.at(root[i]["receiving"].asInt()).c_str());
            } else if(!strcmp(root[i].get("type","").asCString(),"Hint")) {
                std:: string prompt("Hint Received. Item: " + map_item_id_name.at(root[i]["item"]["item"].asInt()));
                ADD_TO_MSGQUEUE(prompt, 4);
                ADD_TO_MSGQUEUE("Item From: " + map_player_id_name.at(root[i]["item"]["player"].asInt()), 3);
                ADD_TO_MSGQUEUE("Item To: " + map_player_id_name.at(root[i]["receiving"].asInt()), 2);
                ADD_TO_MSGQUEUE("Location: " + map_location_id_name.at(root[i]["item"]["location"].asInt()), 1);
                ADD_TO_MSGQUEUE((root[i]["found"].asBool() ? " (Checked)" : " (Unchecked)"), 0);
                vlog_info("Hint: Item %s from %s to %s at %s %s", map_item_id_name.at(root[i]["item"]["item"].asInt()).c_str(), map_player_id_name.at(root[i]["item"]["player"].asInt()).c_str(),
                                                                  map_player_id_name.at(root[i]["receiving"].asInt()).c_str(), map_location_id_name.at(root[i]["item"]["location"].asInt()).c_str(),
                                                                  (root[i]["found"].asBool() ? " (Checked)" : " (Unchecked)"));
            }
        } else if (!strcmp(cmd, "LocationInfo")) {
            //Uninteresting for now.
        } else if (!strcmp(cmd, "ReceivedItems")) {
            for (unsigned int j = 0; j < root[i]["items"].size(); j++) {
                int item_id = root[i]["items"][j]["item"].asInt() - 2515000;
                trinketsPending[item_id] = true;
            }
        } else if (!strcmp(cmd, "RoomUpdate")) {
            for (unsigned int j = 0; j < root[i]["checked_locations"].size(); j++) {
                //Sync checks with server
                int loc_id = root[i]["checked_locations"][j].asInt() - 2515000;
                location_checks[loc_id] = true;
                vlog_debug("V6MW: Archipelago reports %d as checked.", loc_id);
            }
        } else if (!strcmp(cmd, "ConnectionRefused")) {
            auth = false;
            vlog_error("V6MW: Archipelago Server has refused connection. Check Password / Name / IP and restart the Game.");
            webSocket.stop();
        } else if (!strcmp(cmd, "Bounced")) {
            // Only expected Packages are DeathLink Packages. RIP
            if (!enable_deathlink) continue;
            for (unsigned int j = 0; j < root[i]["tags"].size(); j++) {
                if (!strcmp(root[i]["tags"][j].asCString(), "DeathLink")) {
                    // Suspicions confirmed ;-; But maybe we died, not them?
                    if (!strcmp(root[i]["data"]["source"].asCString(), v6mw_player_name.c_str())) break; // We already paid our penance
                    deathlinkstat = true;
                    std::string out = root[i]["data"]["source"].asString() + " killed you ;-;";
                    ADD_TO_MSGQUEUE(out, 0);
                    vlog_info(("V6MW: " + out).c_str());
                    break;
                }
            }
        }
        
        else {
            vlog_warn("V6MW: Unhandled Packet. Command: %s", cmd);
        }
    }
    return false;
}

void APSend(std::string req) {
    vlog_debug("V6MW: Sending %s", req.c_str());
    webSocket.send(req);
}