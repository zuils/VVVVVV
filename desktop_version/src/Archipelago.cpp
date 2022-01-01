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

bool init = false;
bool auth = false;
int v6mw_player_id;
std::string v6mw_player_name;
std::string v6mw_ip;
std::string v6mw_passwd;
int trinketsCollected = 0;
int trinketsPending = 0;
bool deathlinkstat = false;
bool enable_deathlink = false;
std::string location_strings[V6MW_NUM_CHECKS];
bool location_checks[V6MW_NUM_CHECKS];
std::queue<std::string> messageQueue;

ix::WebSocket webSocket;
Json::Reader reader;
Json::FastWriter writer;

bool parse_response(std::string msg, std::string &request);
void APSend(std::string req);

void V6MW_Init(std::string ip, std::string player_name, std::string passwd) {
    if (init) {
        return;
    }
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

    //Initialize checked locations
    for (int i = 0; i < V6MW_NUM_CHECKS; i++) {
        location_checks[i] = false;
    }

    init = true;
    webSocket.start();
}

void V6MW_SendItem(int idx) {
    if (trinketsCollected >= V6MW_NUM_CHECKS || idx >= V6MW_NUM_CHECKS || location_checks[idx]) {
        vlog_warn("V6MW: Something funky is happening...");
        return;
    }
    location_checks[idx] = true;
    vlog_info("V6MW: Checked Trinket %d. Informing Archipelago...", idx);
    Json::Value req_t;
    req_t[0]["cmd"] = "LocationChecks";
    req_t[0]["locations"][0] = idx + 2515000;
    APSend(writer.write(req_t));
}

bool V6MW_RecvItem() {
    return trinketsPending > 0;
}

void V6MW_RecvClear() {
    trinketsPending = trinketsPending <= 0 ? 0 : trinketsPending-1;
    trinketsCollected++;
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
    return trinketsCollected;
}

bool V6MW_DeathLinkRecv() {
    return deathlinkstat;
}

void V6MW_DeathLinkClear() {
    deathlinkstat = false;
}

void V6MW_DeathLinkSend() {
    if (auth) {
        std::chrono::time_point<std::chrono::system_clock> timestamp = std::chrono::system_clock::now();
        Json::Value req_t;
        req_t[0]["cmd"] = "Bounce";
        req_t[0]["data"]["time"] = std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count();
        req_t[0]["data"]["source"] = v6mw_player_name; // Name and Shame >:D
        req_t[0]["tags"][0] = "DeathLink";
        APSend(writer.write(req_t));
    }
}

bool parse_response(std::string msg, std::string &request) {
    Json::Value root;
    reader.parse(msg, root);
    for (uint i = 0; i < root.size(); i++) {
        const char* cmd = root[0]["cmd"].asCString();
        if (!strcmp(cmd,"RoomInfo")) {
            if (!auth) {
                Json::Value req_t;
                req_t[i]["cmd"] = "Connect";
                req_t[i]["game"] = "VVVVVV";
                req_t[i]["name"] = v6mw_player_name;
                req_t[i]["password"] = v6mw_passwd;
                req_t[i]["uuid"] = "1234";
                if (enable_deathlink) {
                    req_t[i]["tags"][0] = "DeathLink";
                } else {
                    req_t[i]["tags"] = "";
                }
                req_t[i]["version"]["major"] = "0";
                req_t[i]["version"]["minor"] = "2";
                req_t[i]["version"]["build"] = "2";
                req_t[i]["version"]["class"] = "Version";
                request = writer.write(req_t);
                auth = false;
                return true;
            }
        } else if (!strcmp(cmd,"Connected")) {
            vlog_info("V6MW: Authenticated");
            v6mw_player_id = root[i]["slot"].asInt();
            for (uint j = 0; j < root[i]["checked_locations"].size(); j++) {
                //Sync checks with server
                int loc_id = root[i]["checked_locations"][j].asInt() - 2515000;
                location_checks[loc_id] = true;
                vlog_debug("V6MW: Archipelago reports Trinket %d as checked.", loc_id);
            }
            vlog_debug("V6MW: %d total already checked.", root[i]["checked_locations"].size());
            enable_deathlink = root[i]["slot_data"]["DeathLink"].asBool();
            if (enable_deathlink) vlog_info("V6MW: Enabled DeathLink.");
            auth = true;
            Json::Value req_t;
            req_t[0]["cmd"] = "Sync";
            request = writer.write(req_t);
            return true;
        } else if (!strcmp(cmd,"Print")) {
            vlog_info("AP: %s", root[i]["text"].asCString());
            messageQueue.push(root[i]["text"].asString());
        } else if (!strcmp(cmd,"PrintJSON")) {
            std::string out("");
            for (uint j = 0; j < root[i]["data"].size(); j++) {
                if (!strcmp(root[i]["data"][j].get("type","").asCString(),"player_id")) {
                    out += root[i]["data"][j]["text"].asString();
                }
                out += root[i]["data"][j].get("text","").asString();
            }
            vlog_info("AP: %s", out.c_str());
            messageQueue.push(out);
        } else if (!strcmp(cmd, "LocationInfo")) {
            //Uninteresting for now.
        } else if (!strcmp(cmd, "ReceivedItems")) {
            for (uint j = 0; j < root[i]["items"].size(); j++) {
                int item_id = root[i]["items"][j]["item"].asInt() - 2515000;
                trinketsPending++;
            }
        } else if (!strcmp(cmd, "RoomUpdate")) {
            for (uint j = 0; j < root[i]["checked_locations"].size(); j++) {
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
            for (uint j = 0; j < root[i]["tags"].size(); j++) {
                if (!strcmp(root[i]["tags"][j].asCString(), "DeathLink")) {
                    // Suspicions confirmed ;-; But maybe we died, not them?
                    if (!strcmp(root[i]["data"]["source"].asCString(), v6mw_player_name.c_str())) break; // We already paid our penance
                    deathlinkstat = true;
                    std::string out = root[i]["data"]["source"].asString() + " killed you ;-;";
                    messageQueue.push(out);
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

const bool* V6MW_Locations() {
    return location_checks;
}

void V6MW_PrintNext() {
    if (messageQueue.empty()) return;
    if (graphics.flipmode) {
        graphics.createtextbox(messageQueue.front(), -1, 25, 174, 174, 174);
    } else {
        graphics.createtextbox(messageQueue.front(), -7, -7, 174, 174, 174);
    }
    graphics.textboxtimer(60);
    messageQueue.pop();
}