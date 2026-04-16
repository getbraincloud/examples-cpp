//-----------------------------------------------------------------------------
// Copyright 2021 bitHeads inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//-----------------------------------------------------------------------------
// File: app.cpp
// Desc: Main application logic
// Author: David St-Louis
//-----------------------------------------------------------------------------

// App includes
#include "app.h"
#include "game.h"
#include "globals.h"
#include "loading.h"
#include "lobby.h"
#include "login.h"
#include "mainMenu.h"
#include "BCCallback.h"

// Third party includes
#include <braincloud/BrainCloudWrapper.h>
#include <braincloud/BrainCloudGlobalApp.h>
#include <braincloud/IRTTCallback.h>
#include <braincloud/IRelayConnectCallback.h>
#include <braincloud/IRelayCallback.h>
#include <braincloud/IRelaySystemCallback.h>
#include <braincloud/reason_codes.h>
#include <imgui.h>

// C/C++ includes
#include <iostream>
#include <stdlib.h>

std::string appVersion = VERSION;
std::string serverVersion = "";

// Prototypes for private functions
static void initBC();
static void handlePlayerState(const Json::Value &result);
static void onLoggedIn();
static void onRTTConnected();
static void dieWithMessage(const std::string &message);
static void errorAndReturnToMenu(const std::string &message);
static void uninitBC();
static void resetState();
static void submitName(const char *username);
static void onLobbyEvent(const Json::Value &eventJson);
static Lobby parseLobby(const Json::Value &lobbyJson, const std::string &lobbyId);
static Server parseServer(const Json::Value &serverJson);
static void startGame();
static void onRelaySystemMessage(const Json::Value &json);
static void onRelayMessage(int netId, const Json::Value &json);
static uint64_t getPlayerMask();
static void sendGameStartToMask(uint64_t playerMask);
static void sendSplotchSyncToMask(uint64_t mask);
static void onRelayConnected();

static bool isDisconnecting = false;

// Tracks the EdgeGap beacon region chosen for the current lobby attempt.
// Set when we pick the best un-tested region; recorded to geoTestedRegions on ROOM_READY.
static std::string s_geoTestRegion;

// brainCloud RTT Connection callbacks
class RTTConnectCallback final : public BrainCloud::IRTTConnectCallback
{
public:
    void rttConnectSuccess() override
    {
        onRTTConnected();
    }

    void rttConnectFailure(const std::string &errorMessage) override
    {
        // Ignore failure if we intentionally disconnected (avoids re-entrant loop)
        if (isDisconnecting)
            return;
        errorAndReturnToMenu("Disconnected from RTT:\n" + errorMessage);
    }
};

// brainCloud RTT callbacks
class RTTCallback final : public BrainCloud::IRTTCallback
{
public:
    void rttCallback(const std::string &dataJson) override
    {
        Json::Reader reader;
        Json::Value eventJson;
        reader.parse(dataJson, eventJson);
        auto service = eventJson["service"];

        if (service == BrainCloud::ServiceName::Lobby.getValue())
        {
            onLobbyEvent(eventJson);
        }
    }
};

// brainCloud Relay Connection callbacks
class RelayConnectCallback final : public BrainCloud::IRelayConnectCallback
{
public:
    void relayConnectSuccess(const std::string &jsonResponse) override
    {
        printf("[%d][DEBUG] Relay connect SUCCESS\n", settings.instanceIndex);
        loading_status = "";
        state.screenState = ScreenState::Game;
        onRelayConnected();
    }

    void relayConnectFailure(const std::string &errorMessage) override
    {
        printf("[%d][DEBUG] Relay connect FAILURE: %s\n", settings.instanceIndex, errorMessage.c_str());
        if (!isDisconnecting)
        {
            errorAndReturnToMenu("Failed to connect to relay server:\n" + errorMessage);
        }
    }
};

// brainCloud Relay callbacks
class RelayCallback final : public BrainCloud::IRelayCallback
{
public:
    void relayCallback(int netId, const uint8_t *bytes, int size) override
    {
        if (isDisconnecting) return;
        Json::Value json;
        Json::Reader reader;
        std::string str((const char *)bytes, size);
        reader.parse(str, json);
        onRelayMessage(netId, json);
    }
};

// brainCloud Relay System callbacks
class RelaySystemCallback final : public BrainCloud::IRelaySystemCallback
{
public:
    void relaySystemCallback(const std::string &jsonResponse) override
    {
        if (isDisconnecting) return;
        Json::Value json;
        Json::Reader reader;
        reader.parse(jsonResponse, json);
        onRelaySystemMessage(json);
    }
};

//-----------------------------------------------------------------------------
// Privates
//-----------------------------------------------------------------------------
BrainCloud::BrainCloudWrapper *pBCWrapper = nullptr;
static std::string errorMessage;
static bool dead = false;

static RTTConnectCallback bcRTTConnectCallback;
static RTTCallback bcRTTCallback;
static RelayConnectCallback bcRelayConnectCallback;
static RelayCallback bcRelayCallback;
static RelaySystemCallback bcRelaySystemCallback;
static bool reconnectAttempted = false;
static bool relayConnectInitiated = false;

// Initialize brainCloud
static void initBC()
{
    if (!pBCWrapper)
    {
        // Use the instance index as the wrapper name so each instance gets its
        // own isolated SaveDataHelper file (bc_profile_0.txt, bc_profile_1.txt …).
        // Single-instance mode uses "default".
        std::string wrapperName = settings.multiInstance
            ? std::to_string(settings.instanceIndex)
            : "default";
        pBCWrapper = new BrainCloud::BrainCloudWrapper(wrapperName.c_str());
    }
    dead = false;

    // Always use the wrapper's initialize() so BrainCloudClient is properly
    // constructed (the wrapper lazily allocates it there — calling getBCClient()
    // before initialize() returns nullptr and crashes).
    pBCWrapper->initialize(BRAINCLOUD_SERVER_URL,
                           BRAINCLOUD_APP_SECRET,
                           BRAINCLOUD_APP_ID,
                           appVersion.c_str(),
                           "bitheads",
                           "RelayTestApp");

    if (settings.multiInstance)
    {
        // initialize() unconditionally calls initializeIdentity() which generates
        // a random anonymous ID and stores it — causing reconnect() to authenticate
        // anonymously on subsequent calls.  Undo that here: clear both the
        // SaveDataHelper store and the client's in-memory identity so the client
        // has no anonymous context.  app_login() will authenticate via universal
        // credentials only.
        pBCWrapper->clearIds();
        pBCWrapper->getBCClient()->initializeIdentity("", "");
    }
    pBCWrapper->getBCClient()->enableLogging(true);

    pBCWrapper->getBCClient()->getAuthenticationService()->getServerVersion(new BCCallback(
        [=](const Json::Value &result) // Success
        {
            serverVersion += result["data"]["serverVersion"].asString();
        },
        [](const std::string &status_message) // Error
        {

        }));
}

// User authenticated, handle the result
static void handlePlayerState(const Json::Value &result)
{
    state.user = User();

    // In multi-instance mode always force the name from the config so each
    // instance shows its assigned username regardless of what's stored server-side.
    // In single-instance mode only set the name when the account has none yet.
    const auto &userName = result["data"]["playerName"].asString();
    if (userName.empty() || settings.multiInstance)
    {
        submitName(settings.username);
    }
    else
    {
        state.user.name = userName;
        onLoggedIn();
    }
}

// Populate state.appLobbies from the parsed AllLobbyTypes global property.
// Mirrors the JS RelayTestApp exactly:
//   readProperties() -> AllLobbyTypes.value (JSON string)
//   -> Object.values() -> each entry's "lobby" field
// Falls back to {"CursorParty"} if the property is absent or unparseable.
static void applyLobbyTypes(const Json::Value &result)
{
    state.appLobbies.clear();
    const auto &prop = result["data"]["AllLobbyTypes"]["value"];
    if (!prop.isNull())
    {
        Json::Value parsed;
        Json::Reader reader;
        if (reader.parse(prop.asString(), parsed) && parsed.isObject())
        {
            // Each value is an object {"lobby": "LobbyTypeName", ...}
            // matching the JS: Object.values(parsedValue) -> lobby.lobby
            for (const auto &key : parsed.getMemberNames())
            {
                const auto &entry = parsed[key];
                if (entry.isObject() && entry.isMember("lobby"))
                    state.appLobbies.push_back(entry["lobby"].asString());
                else if (entry.isString())
                    state.appLobbies.push_back(entry.asString());
            }
        }
    }
    if (state.appLobbies.empty())
        state.appLobbies.push_back(DEFAULT_LOBBY_TYPE);

    // Ensure saved lobbyType is still valid; reset to first if not
    bool found = false;
    for (const auto &lt : state.appLobbies)
        if (lt == settings.lobbyType) { found = true; break; }
    if (!found)
        settings.lobbyType = state.appLobbies[0];

    // SplotchDuration: seconds a splotch persists on the canvas (-1 = forever)
    const auto &durProp = result["data"]["SplotchDuration"]["value"];
    if (!durProp.isNull())
        state.splotchDurationSec = durProp.asInt();
    else
        state.splotchDurationSec = -1;

    state.screenState = ScreenState::MainMenu;
}

// User fully logged in — fetch AllLobbyTypes then show main menu.
// Uses readProperties() (full read) matching the JS RelayTestApp flow.
void onLoggedIn()
{
    loading_text = "Loading lobby types...";
    pBCWrapper->getBCClient()->getGlobalAppService()->readProperties(
        new BCCallback(
            [](const Json::Value &result) { applyLobbyTypes(result); },
            [](const std::string &) {
                // Property missing or network error — fall back gracefully
                if (state.appLobbies.empty())
                    state.appLobbies.push_back(DEFAULT_LOBBY_TYPE);
                state.screenState = ScreenState::MainMenu;
            }));
}

// Shared parameters used by both findOrCreateLobby and findOrCreateLobbyWithPingData
static const char* LOBBY_ALGO   = "{\"strategy\":\"ranged-absolute\",\"alignment\":\"center\",\"ranges\":[1000]}";
static const char* LOBBY_FILTER = "{}";
static const char* LOBBY_SETTINGS = "{}";

// Build the extra JSON for lobby join/ready calls.
// Always includes colorIndex; includes per-region ping results when available.
static std::string buildExtraJson()
{
    Json::Value extra;
    extra["colorIndex"] = state.user.colorIndex;
    if (!state.pingData.empty())
    {
        Json::Value pings;
        for (const auto& kv : state.pingData)
            pings[kv.first] = kv.second;
        extra["pings"] = pings;
    }
    Json::FastWriter writer;
    auto s = writer.write(extra);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
        s.pop_back();
    return s;
}

// Standard findOrCreateLobby (no ping-aware matchmaking)
static void doFindOrCreateLobby(const std::string &lobbyType)
{
    pBCWrapper->getLobbyService()->findOrCreateLobby(
        lobbyType,
        0,             // rating
        1,             // max steps
        LOBBY_ALGO,
        LOBBY_FILTER,
        {},            // other users
        LOBBY_SETTINGS,
        false,         // ready
        buildExtraJson(),
        settings.teamCode,
        new BCCallback(
            [](const Json::Value &) { /* success comes via RTT onLobbyEvent */ },
            [](const std::string &status_message)
            {
                errorAndReturnToMenu("Failed to find lobby:\n" + status_message);
            }));
}

// findOrCreateLobby using collected ping data for region-aware matchmaking
static void doFindOrCreateLobbyWithPingData(const std::string &lobbyType)
{
    pBCWrapper->getLobbyService()->findOrCreateLobbyWithPingData(
        lobbyType,
        0,                    // rating
        1,                    // max steps
        LOBBY_ALGO,
        LOBBY_FILTER,
        {},                   // other users
        LOBBY_SETTINGS,
        false,                // ready
        buildExtraJson(),
        settings.teamCode,
        new BCCallback(
            [](const Json::Value &) { /* success comes via RTT onLobbyEvent */ },
            [](const std::string &status_message)
            {
                errorAndReturnToMenu("Failed to find lobby:\n" + status_message);
            }));
}

// RTT connected — always collect ping data so all members share their latencies
// in the lobby/game UI. usePingData only controls whether ping-aware matchmaking is used.
void onRTTConnected()
{
    state.user.cxId = pBCWrapper->getRTTService()->getRTTConnectionId();

    // If RTT auto-reconnects while we are already in the lobby or game (e.g. after
    // a brief network hiccup), do not restart the region-ping + findOrCreateLobby
    // flow — the player is already placed and we just need the RTT channel back.
    if (state.screenState != ScreenState::JoiningLobby)
        return;

    loading_status = "Getting regions...";
    pBCWrapper->getLobbyService()->getRegionsForLobbies(
        {settings.lobbyType},
        new BCCallback(
            [](const Json::Value &result)
            {
                if (isDisconnecting) return;

                // Collect region names so app_update() can show per-region progress
                state.expectedPingRegions.clear();
                const auto &regionPingData = result["data"]["regionPingData"];
                if (!regionPingData.isNull())
                {
                    for (const auto &region : regionPingData.getMemberNames())
                        state.expectedPingRegions.push_back(region);
                    std::sort(state.expectedPingRegions.begin(), state.expectedPingRegions.end());
                }

                pBCWrapper->getLobbyService()->pingRegions(
                    new BCCallback(
                        [](const Json::Value &)
                        {
                            if (isDisconnecting) return;
                            state.pingData = pBCWrapper->getLobbyService()->getPingData();
                            state.expectedPingRegions.clear(); // stop per-frame polling

                            // For EdgeGap, route to the best region-specific lobby type.
                            // EdgeGap geo-test: cycle through only the regions that have a
                            // defined specific lobby type. Unmapped ping regions are ignored
                            // entirely — they have no EdgeGap lobby to route to.
                            std::string lobbyType = settings.lobbyType;
                            s_geoTestRegion.clear();
                            if (isEdgeGapLobby(settings.lobbyType) && !state.pingData.empty())
                            {
                                std::string bestRegion;
                                int bestPing = INT_MAX;
                                // Pick fastest un-tested region that has a defined EdgeGap lobby
                                for (const auto &kv : state.pingData)
                                {
                                    if (edgeGapRegionToLobbyType(kv.first).empty()) continue;
                                    const auto &tested = state.geoTestedRegions;
                                    bool alreadyTested = std::find(tested.begin(), tested.end(), kv.first) != tested.end();
                                    if (!alreadyTested && kv.second < bestPing)
                                    {
                                        bestPing = kv.second;
                                        bestRegion = kv.first;
                                    }
                                }
                                // All defined lobbies tested — wrap around to global fastest mapped region
                                if (bestRegion.empty())
                                {
                                    bestPing = INT_MAX;
                                    for (const auto &kv : state.pingData)
                                    {
                                        if (edgeGapRegionToLobbyType(kv.first).empty()) continue;
                                        if (kv.second < bestPing) { bestPing = kv.second; bestRegion = kv.first; }
                                    }
                                    printf("[GeoTest] All EdgeGap lobbies tested — wrapping to %s (%dms)\n",
                                           bestRegion.c_str(), bestPing);
                                }
                                else
                                {
                                    printf("[GeoTest] Routing to %s (%dms), %d/%d tested\n",
                                           bestRegion.c_str(), bestPing,
                                           (int)state.geoTestedRegions.size(),
                                           (int)state.pingData.size());
                                }
                                s_geoTestRegion = bestRegion;
                                lobbyType = edgeGapRegionToLobbyType(bestRegion);
                            }

                            if (settings.usePingData)
                                doFindOrCreateLobbyWithPingData(lobbyType);
                            else
                                doFindOrCreateLobby(lobbyType);
                        },
                        [](const std::string &)
                        {
                            // Ping failed — fall back gracefully to standard lobby creation
                            if (isDisconnecting) return;
                            state.expectedPingRegions.clear();
                            doFindOrCreateLobby(settings.lobbyType);
                        }));
            },
            [](const std::string &)
            {
                // Region lookup failed — proceed without ping data
                if (isDisconnecting) return;
                doFindOrCreateLobby(settings.lobbyType);
            }));
}

// Show error and go back to MainMenu without logging out.
// Use this for relay/lobby errors where the user is still authenticated.
static void errorAndReturnToMenu(const std::string &message)
{
    isDisconnecting = true;
    pBCWrapper->getRelayService()->deregisterRelayCallback();
    pBCWrapper->getRelayService()->deregisterSystemCallback();
    pBCWrapper->getRelayService()->disconnect();
    pBCWrapper->getRTTService()->deregisterAllRTTCallbacks();
    pBCWrapper->getRTTService()->disableRTT();

    // Reset state but keep the user logged in
    User user = state.user;
    auto pingData = state.pingData;
    auto geoTestedRegions = state.geoTestedRegions;
    s_geoTestRegion.clear();
    state = State();
    state.user = user;
    state.pingData = pingData;
    state.geoTestedRegions = geoTestedRegions;
    state.screenState = ScreenState::MainMenu;

    errorMessage = message;
    ImGui::OpenPopup("Error");
}

// Go back to login screen, with an error message (auth failures only)
static void dieWithMessage(const std::string &message)
{
    isDisconnecting = true;

    pBCWrapper->getRelayService()->deregisterRelayCallback();
    pBCWrapper->getRelayService()->deregisterSystemCallback();
    pBCWrapper->getRelayService()->disconnect();
    pBCWrapper->getRTTService()->deregisterAllRTTCallbacks();
    pBCWrapper->getRTTService()->disableRTT();

    pBCWrapper->logout(false, nullptr);

    errorMessage = message;
    ImGui::OpenPopup("Error");
    dead = true;
    resetState();
}

// Uninitialize brainCloud
void uninitBC()
{
    delete pBCWrapper;
    pBCWrapper = nullptr;
}

// Reset application state, back to login screen
void resetState()
{
    state = State();
}

// Send game start time and round number to specific players via relay
static void sendGameStartToMask(uint64_t playerMask)
{
    if (playerMask == 0) return;

    Json::Value json;
    json["op"] = "game_start";
    json["data"]["startTime"] = (Json::Int64)state.gameStartTime;
    json["data"]["round"] = state.roundNumber;

    Json::FastWriter writer;
    auto str = writer.write(json);
    pBCWrapper->getRelayService()->sendToPlayers(
        (const uint8_t *)str.data(), (int)str.length(),
        playerMask,
        true,  // reliable
        false, // unordered ok
        (BrainCloud::eRelayChannel)0);
}

// Send all current splotches to a player mask in chunked packets that stay under
// the 1024-byte relay message limit.  The first packet carries "first":true so the
// receiver knows to clear its local canvas before appending; subsequent packets
// carry "first":false and are appended.
static void sendSplotchSyncToMask(uint64_t mask)
{
    if (state.splotches.empty() || mask == 0) return;

    static const int MAX_RELAY_BYTES = 900; // conservative margin below 1024

    // Overhead of the outer envelope with "first":false (longest variant):
    // {"op":"splotch_sync","data":{"first":false,"splotches":[]}}  ~60 chars
    static const int ENVELOPE_OVERHEAD = 65;

    Json::FastWriter writer;
    bool isFirst = true;
    std::vector<Json::Value> chunk;
    int currentSize = ENVELOPE_OVERHEAD;

    auto flushChunk = [&]()
    {
        if (chunk.empty()) return;
        Json::Value syncJson;
        syncJson["op"] = "splotch_sync";
        syncJson["data"]["first"] = isFirst;
        Json::Value arr(Json::arrayValue);
        for (const auto &entry : chunk)
            arr.append(entry);
        syncJson["data"]["splotches"] = arr;
        auto str = writer.write(syncJson);
        pBCWrapper->getRelayService()->sendToPlayers(
            (const uint8_t *)str.data(), (int)str.length(),
            mask, true, false, (BrainCloud::eRelayChannel)0);
        isFirst = false;
        chunk.clear();
        currentSize = ENVELOPE_OVERHEAD;
    };

    for (const auto &s : state.splotches)
    {
        Json::Value entry;
        entry["x"] = s.pos.x / 800.0f;
        entry["y"] = s.pos.y / 600.0f;
        entry["c"] = s.colorIndex;
        entry["t"] = (Json::Int64)s.startTimeMs;

        // Measure this entry's serialized size (+1 for the separating comma)
        int entrySize = (int)writer.write(entry).size() + 1;
        if (currentSize + entrySize > MAX_RELAY_BYTES && !chunk.empty())
            flushChunk();

        chunk.push_back(std::move(entry));
        currentSize += entrySize;
    }
    flushChunk();
}

// Called when relay connection succeeds. Owner sets and broadcasts the authoritative game start time.
static void onRelayConnected()
{
    ++state.roundNumber;

    // Auto geo test: relay connect confirms the region is reachable.
    // Record the connect time; the update loop will set pendingGeoTestDisconnect
    // after a 2.5s soak so we confirm the connection is fully stable.
    if (settings.autoGeoTest)
    {
        printf("[GeoTest] Relay connected — soaking for 2.5s before disconnect\n");
        state.geoTestRelayConnectTime = std::chrono::steady_clock::now();
        return;
    }

    if (state.lobby.ownerCxId == state.user.cxId)
    {
        // Owner is the authoritative source for start time
        auto now = std::chrono::system_clock::now();
        state.gameStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        sendGameStartToMask(getPlayerMask());
    }
    // Non-owner players will receive game_start from the owner via relay message
}

static void onRelaySystemMessage(const Json::Value &json)
{
    if (json["op"].asString() == "DISCONNECT") // A member has disconnected from the game
    {
        const auto &cxId = json["cxId"].asString();
        for (auto &member : state.lobby.members)
        {
            if (member.cxId == cxId)
            {
                member.isAlive = false; // This will stop displaying this member
                break;
            }
        }
    }
    else if (json["op"].asString() == "CONNECT") // A new player joined mid-game (backfill)
    {
        // Owner re-sends game start time and current splotch canvas so the JIP player syncs up
        if (state.lobby.ownerCxId == state.user.cxId && state.gameStartTime != 0)
        {
            const auto &cxId = json["cxId"].asString();
            auto netId = pBCWrapper->getRelayService()->getNetIdForCxId(cxId);
            uint64_t mask = (uint64_t)1 << (uint64_t)netId;
            sendGameStartToMask(mask);

            // Send existing splotches in size-bounded chunks so the JIP canvas matches everyone else's
            sendSplotchSyncToMask(mask);
        }
    }
    else if (json["op"].asString() == "END_MATCH") // Match ended, return all players to lobby
    {
        // Reset per-round state immediately
        state.user.isAlive = false;
        state.user.isReady = false;
        state.shockwaves.clear();
        state.splotches.clear();
        state.gameStartTime = 0;
        state.screenState = ScreenState::Lobby;

        // Defer relay disconnect — cannot safely call deregister/disconnect from inside a relay callback
        state.pendingEndMatch = true;
    }
}

static void onRelayMessage(int netId, const Json::Value &json)
{
    const auto &memberCxId = pBCWrapper->getRelayService()->getCxIdForNetId(netId);
    for (auto &member : state.lobby.members)
    {
        if (member.cxId == memberCxId)
        {
            auto op = json["op"].asString();
            if (op == "move")
            {
                member.isAlive = true;
                member.pos.x = (int)(json["data"]["x"].asFloat() * 800.0f);
                member.pos.y = (int)(json["data"]["y"].asFloat() * 600.0f);
            }
            else if (op == "shockwave")
            {
                Shockwave shockwave;
                shockwave.pos.x = (int)(json["data"]["x"].asFloat() * 800.0f);
                shockwave.pos.y = (int)(json["data"]["y"].asFloat() * 600.0f);
                shockwave.colorIndex = member.colorIndex;
                shockwave.startTime = std::chrono::high_resolution_clock::now();
                state.shockwaves.push_back(shockwave);

                // Leave a persistent splotch at the same location
                Splotch splotch;
                splotch.pos = shockwave.pos;
                splotch.colorIndex = member.colorIndex;
                splotch.startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                state.splotches.push_back(splotch);
            }
            else if (op == "splotch_sync")
            {
                // JIP: host sends canvas state in one or more chunked packets.
                // "first":true means this is the opening packet — clear before appending.
                if (json["data"]["first"].asBool())
                    state.splotches.clear();
                for (const auto &entry : json["data"]["splotches"])
                {
                    Splotch s;
                    s.pos         = {(int)(entry["x"].asFloat() * 800.0f), (int)(entry["y"].asFloat() * 600.0f)};
                    s.colorIndex  = entry["c"].asInt();
                    s.startTimeMs = entry["t"].asInt64();
                    state.splotches.push_back(s);
                }
            }
            else if (op == "clear_splotches")
            {
                state.splotches.clear();
            }
            else if (op == "game_start")
            {
                // Owner's authoritative start time — sync for non-owners and JIP players
                state.gameStartTime = json["data"]["startTime"].asInt64();
                state.roundNumber = json["data"]["round"].asInt();
            }
            else if (op == "relay_ping")
            {
                member.activePing = json["data"]["ping"].asInt();
            }
            break;
        }
    }
}

//-----------------------------------------------------------------------------
// Public functions
//-----------------------------------------------------------------------------

// Draws the application's GUI and update brainCloud
void app_update()
{
    if (dead)
    {
        dead = false;
        uninitBC(); // We differ destroying BC because we cannot destroy it within a callback (yet)
        BCCallback::destroyAll();
    }
    else
    {
        if (pBCWrapper)
        {
            pBCWrapper->runCallbacks();

            // While ping phase is active, poll incremental results each frame and
            // update the loading status so the user sees per-region progress live.
            if (!state.expectedPingRegions.empty())
            {
                auto snapshot = pBCWrapper->getLobbyService()->getPingData();
                std::string status;
                for (const auto &region : state.expectedPingRegions)
                {
                    if (!status.empty()) status += "\n";
                    auto it = snapshot.find(region);
                    if (it != snapshot.end())
                    {
                        if (it->second >= 999)
                            status += "  " + region + ": T/O";
                        else
                            status += "  " + region + ": " + std::to_string(it->second) + " ms";
                    }
                    else
                    {
                        status += "  " + region + ": pinging...";
                    }
                }
                loading_status = status;
            }

            // Geo test soak timer: wait 2.5s after relay connect before disconnecting,
            // so the connection is confirmed fully stable before moving to the next region.
            if (state.geoTestRelayConnectTime != std::chrono::steady_clock::time_point{})
            {
                auto elapsed = std::chrono::steady_clock::now() - state.geoTestRelayConnectTime;
                if (elapsed >= std::chrono::milliseconds(2500))
                {
                    printf("[GeoTest] 2.5s soak complete — triggering disconnect\n");
                    state.geoTestRelayConnectTime = {};
                    state.pendingGeoTestDisconnect = true;
                }
            }

            // Deferred auto geo test disconnect — relay connect was confirmed and the
            // soak period has elapsed. Tear down gracefully so the next region test
            // starts with a clean slate:
            //   1. endMatch()    — signal the relay server we are done (graceful close)
            //   2. deregister + relay disconnect
            //   3. leaveLobby() — remove us from the brainCloud lobby so the next
            //                     findOrCreateLobby can place us in a fresh one
            //   4. RTT teardown + state reset
            if (state.pendingGeoTestDisconnect)
            {
                state.pendingGeoTestDisconnect = false;
                isDisconnecting = true; // suppress any in-flight relay callbacks

                printf("[GeoTest] Graceful disconnect — %d region(s) tested so far\n",
                       (int)state.geoTestedRegions.size());

                // 1. Gracefully end the relay match
                app_endMatch();

                // 2. Deregister relay callbacks and disconnect from relay server
                pBCWrapper->getRelayService()->deregisterRelayCallback();
                pBCWrapper->getRelayService()->deregisterSystemCallback();
                pBCWrapper->getRelayService()->disconnect();

                // 3. Leave the brainCloud lobby so the next test gets a fresh lobby
                if (!state.lobby.lobbyId.empty())
                    pBCWrapper->getLobbyService()->leaveLobby(state.lobby.lobbyId, nullptr);

                // 4. Tear down RTT
                pBCWrapper->getRTTService()->deregisterAllRTTCallbacks();
                pBCWrapper->getRTTService()->disableRTT();

                // Preserve geo test data and user info through the state reset
                User user = state.user;
                auto appLobbies = state.appLobbies;
                int splotchDurationSec = state.splotchDurationSec;
                auto pingData = state.pingData;
                auto geoTestedRegions = state.geoTestedRegions;
                s_geoTestRegion.clear();
                state = State();
                state.user = user;
                state.user.isAlive = false;
                state.user.isReady = false;
                state.appLobbies = appLobbies;
                state.splotchDurationSec = splotchDurationSec;
                state.pingData = pingData;
                state.geoTestedRegions = geoTestedRegions;
                state.screenState = ScreenState::MainMenu;

                return; // state has been reset; skip rest of this frame's game update
            }

            // Deferred END_MATCH disconnect — safe to call here, after callbacks have returned
            if (state.pendingEndMatch)
            {
                state.pendingEndMatch = false;
                isDisconnecting = true;
                pBCWrapper->getRelayService()->deregisterRelayCallback();
                pBCWrapper->getRelayService()->deregisterSystemCallback();
                pBCWrapper->getRelayService()->disconnect();
                isDisconnecting = false;

                // Non-host users re-ready for the next round now that we're back in the lobby.
                // The host does NOT auto-ready — the host controls when the next match starts.
                if (state.user.cxId != state.lobby.ownerCxId)
                {
                    state.user.isReady = true;
                    pBCWrapper->getLobbyService()->updateReady(
                        state.lobby.lobbyId, true,
                        buildExtraJson(),
                        nullptr);
                }
            }
        }
        else
        {
            initBC();

            if (pBCWrapper->getBCClient()->isInitialized() == false)
            {
                loading_text = "Initialize failed. Check ids.";
                // Show loading screen
                state.screenState = ScreenState::LoggingIn;
            }
        }
    }

    // Add a menu at the top with an exit option to cleanly quit the app,
    // so we can test for exit crashes at any point
    if (ImGui::BeginMainMenuBar())
    {
        std::string app_text = "brainCloud ";
        if (pBCWrapper && pBCWrapper->getBCClient()->isInitialized())
        {
            app_text += pBCWrapper->getBCClient()->getBrainCloudClientVersion();
        }
        if (ImGui::BeginMenu(app_text.c_str()))
        {
            if (ImGui::MenuItem("Log Out"))
            {
                app_logOut();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
            {
                app_exit();
            }
            ImGui::EndMenu();
        }
        if (state.screenState == ScreenState::Game)
        {
            if (ImGui::BeginMenu("Game"))
            {
                if (ImGui::BeginMenu("Scale"))
                {
                    {
                        bool selected = settings.gameUIIScale == 0;
                        if (ImGui::MenuItem("0.25x", 0, &selected))
                        {
                            settings.gameUIIScale = 0;
                            saveConfigs();
                        }
                    }
                    {
                        bool selected = settings.gameUIIScale == 1;
                        if (ImGui::MenuItem("0.5x", 0, &selected))
                        {
                            settings.gameUIIScale = 1;
                            saveConfigs();
                        }
                    }
                    {
                        bool selected = settings.gameUIIScale == 2;
                        if (ImGui::MenuItem("1x", 0, &selected))
                        {
                            settings.gameUIIScale = 2;
                            saveConfigs();
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (state.lobby.ownerCxId == state.user.cxId)
                {
                    if (ImGui::MenuItem("End Match"))
                    {
                        app_endMatch();
                    }
                    ImGui::Separator();
                }
                if (ImGui::MenuItem("Leave"))
                {
                    app_closeGame();
                }
                ImGui::EndMenu();
            }
        }

        // Right-aligned username: show brainCloud profile name when logged in,
        // otherwise the local username from the config/login form.
        {
            const std::string& displayName = !state.user.name.empty()
                ? state.user.name
                : (settings.username[0] ? settings.username : "");
            if (!displayName.empty())
            {
                auto color = COLORS[settings.colorIndex % NUM_COLORS];
                std::string label = displayName;
                if (settings.multiInstance)
                    label = "[" + std::to_string(settings.instanceIndex + 1) + "] " + label;
                float textWidth = ImGui::CalcTextSize(label.c_str()).x + ImGui::GetStyle().ItemSpacing.x * 2;
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - textWidth);
                ImGui::TextColored(color, "%s", label.c_str());
            }
        }
    }
    ImGui::EndMainMenuBar();

    // Display the proper screen
    switch (state.screenState)
    {
    case ScreenState::Login:
        if (!reconnectAttempted && settings.autoLogin && pBCWrapper->canReconnect())
            app_reconnect();
        else
            login_update();
        break;
    case ScreenState::LoggingIn:
    case ScreenState::JoiningLobby:
    case ScreenState::Starting:
        loading_update();
        break;
    case ScreenState::MainMenu:
        mainMenu_update();
        break;
    case ScreenState::Lobby:
        lobby_update();
        break;
    case ScreenState::Game:
        game_update();
        break;
    }

    // Version overlay — bottom-left, always visible on every screen
    {
        const float PAD = 8.0f;
        ImGui::SetNextWindowPos(ImVec2(PAD, (float)height - PAD), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.45f);
        ImGui::Begin("##version_overlay",nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoInputs     |
            ImGuiWindowFlags_NoNav        |
            ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("App:    %s", VERSION);
        if (pBCWrapper && pBCWrapper->getBCClient()->isInitialized())
        {
            ImGui::Text("Client: %s", pBCWrapper->getBCClient()->getBrainCloudClientVersion().c_str());
            ImGui::Text("Server: %s", serverVersion.c_str());
        }
        ImGui::End();
    }

    // Error message popup
    if (ImGui::BeginPopupModal("Error", NULL,
                               ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("%s", errorMessage.c_str());
        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// Logs out the current user and goes back to login screen
void app_logOut()
{
    reconnectAttempted = false;
    pBCWrapper->logout(true, nullptr);
    dead = true;
    resetState();
}

#if defined(RELAYTESTAPP_UWP)
#include <Windows.h>
#endif

// Shutdowns the application
void app_exit()
{
    extern bool done;
    done = true;

    if (pBCWrapper && pBCWrapper->getBCClient())
    {
        pBCWrapper->logout(false, nullptr);
    }
    // #if defined(RELAYTESTAPP_UWP)
    //     Windows::ApplicationModel::Core::CoreApplication::Exit();
    // #else
    //     exit(0);
    // #endif
}

// Attempt login with the specific username/password
void app_login(const char *username, const char *password)
{

    // Show loading screen
    loading_text = "Logging in ...";
    state.screenState = ScreenState::LoggingIn;

    auto onSuccess = new BCCallback(
        [](const Json::Value &result) { handlePlayerState(result); },
        [](const std::string &status_message) { dieWithMessage("Login Failed:\n" + status_message); });

    if (settings.multiInstance)
    {
        // In multi-instance mode skip the wrapper's initializeIdentity() flow entirely.
        // Each instance has its own wrapper name and SaveDataHelper file, so profile
        // data is already isolated. Calling the client directly means no anonymous ID
        // is generated or sent — the server authenticates purely on universal credentials.
        pBCWrapper->getBCClient()->getAuthenticationService()->authenticateUniversal(
            username, password, true, onSuccess);
    }
    else
    {
        // Single-instance: use the full wrapper flow (initializeIdentity + profile
        // caching via DefaultSaveDataHelper) so reconnect/session resume works.
        pBCWrapper->authenticateUniversal(username, password, true, onSuccess);
    }
}

// Attempt  reconnect with saved profile
void app_reconnect()
{
    reconnectAttempted = true;

    // Show loading screen
    loading_text = "Reconnecting ...";
    state.screenState = ScreenState::LoggingIn;

    // Authenticate with brainCloud
    pBCWrapper->reconnect(new BCCallback(
        [](const Json::Value &result) // Success
        {
            handlePlayerState(result);
        },
        [](const std::string &status_message) // Error
        {
            dieWithMessage("Reconnect Failed:\n" + status_message);
        }));
}

// Submit user name to brainCloud to be assosiated with the current user
static void submitName(const char *username)
{
    state.user.name = username;

    // Update name
    pBCWrapper->getPlayerStateService()->updateUserName(
        state.user.name.c_str(),
        new BCCallback(
            [](const Json::Value &result) // Success
            {
                onLoggedIn();
            },
            [](const std::string &status_message) // Error
            {
                dieWithMessage("Failed to update username to brainCloud:\n" +
                               status_message);
            }));
}

// Start finding a lobby
void app_play(BrainCloud::eRelayConnectionType in_protocol)
{
    settings.protocol = in_protocol;
    isDisconnecting = false;
    state.user.colorIndex = settings.colorIndex;

    // Clear stale lobby data so the loading screen shows a fresh lobbyId
    User savedUser = state.user;
    state.lobby = Lobby();
    state.user = savedUser;

    // Show loading screen
    loading_text = "Joining lobby ...";
    state.screenState = ScreenState::JoiningLobby;

    // Reset loading timer so elapsed time starts from when Play was clicked
    loading_reset_timer();

    // Enable RTT
    pBCWrapper->getRTTService()->registerRTTLobbyCallback(&bcRTTCallback);
    pBCWrapper->getRTTService()->enableRTT(&bcRTTConnectCallback, true);
}

// Take in lobby json and id and build a lobby object
static Lobby parseLobby(const Json::Value &lobbyJson, const std::string &lobbyId)
{
    Lobby lobby;

    lobby.lobbyId = lobbyId;
    lobby.ownerCxId = lobbyJson["ownerCxId"].asString();

    const auto &jsonMembers = lobbyJson["members"];
    for (const auto &jsonMember : jsonMembers)
    {
        User user;
        user.cxId = jsonMember["cxId"].asString();
        user.name = jsonMember["name"].asString();
        user.colorIndex = jsonMember["extra"]["colorIndex"].asInt();

        // Parse ping data shared via extra JSON (included by all clients after pinging)
        const auto &jsonPings = jsonMember["extra"]["pings"];
        if (jsonPings.isObject())
        {
            for (const auto &region : jsonPings.getMemberNames())
                user.pings[region] = jsonPings[region].asInt();
        }

        if (user.cxId == state.user.cxId)
            user.allowSendTo = false;
        lobby.members.push_back(user);
    }

    // Infer server region: region with the lowest mean ping across all members.
    // 999 (timeout) is included in the average — unreachable regions naturally score high.
    // Falls back to our own pingData for self if member.pings isn't populated yet.
    {
        std::map<std::string, std::pair<long long, int>> totals; // region -> {sum_ms, count}
        for (const auto &m : lobby.members)
        {
            const std::map<std::string, int> *pPings = m.pings.empty() ? nullptr : &m.pings;
            std::map<std::string, int> selfPings;
            if (!pPings && m.cxId == state.user.cxId && !state.pingData.empty())
            {
                selfPings = state.pingData;
                pPings = &selfPings;
            }
            if (!pPings) continue;
            for (const auto &kv : *pPings)
            {
                totals[kv.first].first  += kv.second;
                totals[kv.first].second += 1;
            }
        }
        std::string bestRegion;
        int bestAvg = 1000; // above any valid value (max real ping is 999)
        for (const auto &kv : totals)
        {
            if (kv.second.second == 0) continue;
            int avg = (int)(kv.second.first / kv.second.second);
            if (avg < bestAvg) { bestAvg = avg; bestRegion = kv.first; }
        }
        lobby.regionId = bestRegion;
    }

    return lobby;
}

// Take in server json and build a server object
static Server parseServer(const Json::Value &serverJson)
{
    Server server;

    const auto &ports = serverJson["connectData"]["ports"];
    server.host = serverJson["connectData"]["address"].asString();
    if (!ports["ws"].isNull())
        server.wsPort = ports["ws"].asInt();
    if (!ports["tcp"].isNull())
        server.tcpPort = ports["tcp"].asInt();
    if (!ports["udp"].isNull())
        server.udpPort = ports["udp"].asInt();
    if (!ports["gamelift"].isNull())
        server.gameLiftPort = ports["gamelift"].asInt();
    if (!ports["i3d"].isNull())
        server.i3dPort = ports["i3d"].asInt();
    server.passcode = serverJson["passcode"].asString();
    server.lobbyId = serverJson["lobbyId"].asString();

    printf("[DEBUG] parseServer: host=%s ws=%d tcp=%d udp=%d gamelift=%d i3d=%d\n",
           server.host.c_str(), server.wsPort, server.tcpPort, server.udpPort,
           server.gameLiftPort, server.i3dPort);

    return server;
}

// We received a lobby event through RTT
static void onLobbyEvent(const Json::Value &eventJson)
{
    const auto &jsonData = eventJson["data"];

    // If there is a lobby object present in the message, update our lobby
    // state with it.
    if (jsonData["lobby"].isObject())
    {
        state.lobby = parseLobby(jsonData["lobby"], jsonData["lobbyId"].asString());

        // If we were joining lobby, show the lobby screen. We have the information to
        // display now.
        if (state.screenState == ScreenState::JoiningLobby)
        {
            state.screenState = ScreenState::Lobby;
            state.geoTestLobbyArrivalTime = std::chrono::steady_clock::now();

            // Non-host users auto-ready when arriving at the lobby so the host can
            // start the round immediately without waiting for others to click Ready.
            // The host does NOT auto-ready — the host controls when the match starts.
            if (!state.user.isReady && state.user.cxId != state.lobby.ownerCxId)
            {
                state.user.isReady = true;
                pBCWrapper->getLobbyService()->updateReady(
                    state.lobby.lobbyId, true,
                    buildExtraJson(),
                    nullptr);
            }
        }
    }

    auto operation = eventJson["operation"].asString();
    printf("[DEBUG] onLobbyEvent: op=%s\n", operation.c_str());

    if (operation == "DISBANDED")
    {
        int reasonCode = jsonData["reason"]["code"].asInt();
        printf("[DEBUG] DISBANDED reason code=%d (RTT_ROOM_READY=%d)\n", reasonCode, RTT_ROOM_READY);
        if (reasonCode != RTT_ROOM_READY)
        {
            // Disbanded for any reason other than ROOM_READY means the server failed to launch.
            // Show the error so autoJoin doesn't silently loop back in.
            const std::string desc = jsonData["reason"]["desc"].asString();
            const std::string msg  = jsonData["msg"].asString();
            errorAndReturnToMenu(desc + (msg.empty() ? "" : "\n" + msg));
        }
    }
    else if (operation == "MATCHMAKING_IN_PROGRESS")
    {
        loading_status = "Searching...";
    }
    else if (operation == "MEMBER_JOIN")
    {
        const auto &name = jsonData["member"]["name"].asString();
        loading_status = "Joined: " + (name.empty() ? "unknown" : name);
    }
    else if (operation == "MEMBER_LEFT")
    {
        loading_status = "Player left";
    }
    else if (operation == "STARTING")
    {
        // Save our picked color index
        settings.colorIndex = state.user.colorIndex;
        saveConfigs();

        // Go to loading screen; reset timer so it counts from provisioning start
        state.screenState = ScreenState::Starting;
        loading_text = "Starting...";
        loading_reset_timer();
        loading_status = "Provisioning server...";
    }
    else if (operation == "ROOM_PROGRESS")
    {
        int curStep = jsonData["curStep"].asInt();
        int ofStep = jsonData["ofStep"].asInt();
        const auto &msg = jsonData["msg"].asString();
        char buf[128];
        snprintf(buf, sizeof(buf), "%d/%d: %s", curStep, ofStep, msg.c_str());
        loading_status = buf;
    }
    else if (operation == "ROOM_ASSIGNED")
    {
        loading_status = "Server assigned...";
    }
    else if (operation == "ROOM_READY")
    {
        loading_status = "Connecting...";
        state.server = parseServer(jsonData);

        // Record which region was actually launched for the geo test.
        // EdgeGap: region was chosen client-side and stored in s_geoTestRegion.
        // V2 / GameLift / others: extract the region prefix from the lobbyId
        //   (format "region:LobbyType:N") — the server chose it via ping-aware routing.
        {
            std::string region = s_geoTestRegion.empty()
                                 ? regionFromLobbyId(state.server.lobbyId)
                                 : s_geoTestRegion;
            if (!region.empty())
            {
                // Only record once per region
                const auto &tested = state.geoTestedRegions;
                if (std::find(tested.begin(), tested.end(), region) == tested.end())
                {
                    state.geoTestedRegions.push_back(region);
                    printf("[GeoTest] Recorded region: %s (total: %d)\n",
                           region.c_str(), (int)state.geoTestedRegions.size());
                }
                else
                {
                    printf("[GeoTest] Region %s already recorded — server routed to same region\n",
                           region.c_str());
                }
            }
            s_geoTestRegion.clear();
        }

        startGame();
    }
}

// Connect to the Relay server and start the game
static void startGame()
{
    state.screenState = ScreenState::Starting;

    pBCWrapper->getRelayService()->registerRelayCallback(&bcRelayCallback);
    pBCWrapper->getRelayService()->registerSystemCallback(&bcRelaySystemCallback);

    printf("[DEBUG] startGame: host=%s ws=%d tcp=%d udp=%d gamelift=%d i3d=%d protocol=%d\n",
           state.server.host.c_str(), state.server.wsPort, state.server.tcpPort,
           state.server.udpPort, state.server.gameLiftPort, state.server.i3dPort,
           (int)settings.protocol);

    // GameLift and i3D relay servers are WS-only; their single port is always a WS port.
    // For standard servers the port is chosen by the user-selected protocol.
    auto connectProtocol = settings.protocol;
    int port = 0;
    if (state.server.gameLiftPort != -1)
    {
        port = state.server.gameLiftPort;
        connectProtocol = BrainCloud::eRelayConnectionType::WS;
    }
    else if (state.server.i3dPort != -1)
    {
        port = state.server.i3dPort;
        connectProtocol = BrainCloud::eRelayConnectionType::WS;
    }
    else
    {
        switch (settings.protocol)
        {
        case BrainCloud::eRelayConnectionType::WS:
            port = state.server.wsPort;
            break;
        case BrainCloud::eRelayConnectionType::TCP:
            port = state.server.tcpPort;
            break;
        case BrainCloud::eRelayConnectionType::UDP:
            port = state.server.udpPort;
            break;
        case BrainCloud::eRelayConnectionType::WSS:
            break;
        }
    }

    printf("[DEBUG] relay connect: protocol=%d port=%d\n", (int)connectProtocol, port);
    pBCWrapper->getRelayService()->connect(connectProtocol,
                                           state.server.host,
                                           port,
                                           state.server.passcode,
                                           state.server.lobbyId,
                                           &bcRelayConnectCallback);
}

// Cancel lobby search or leave lobby. Go back to main menu without logging out.
void app_cancelLobby()
{
    isDisconnecting = true;

    // Notify server we're leaving the lobby if we have one
    if (!state.lobby.lobbyId.empty())
    {
        pBCWrapper->getLobbyService()->leaveLobby(state.lobby.lobbyId, nullptr);
    }

    pBCWrapper->getRTTService()->deregisterAllRTTCallbacks();
    pBCWrapper->getRTTService()->disableRTT();

    // Reset state but keep the user and app-level config around
    User user = state.user;
    auto appLobbies = state.appLobbies;
    int splotchDurationSec = state.splotchDurationSec;
    auto pingData = state.pingData;
    auto geoTestedRegions = state.geoTestedRegions;
    s_geoTestRegion.clear();
    state = State();
    state.user = user;
    state.user.isAlive = false;
    state.user.isReady = false;
    state.appLobbies = appLobbies;
    state.splotchDurationSec = splotchDurationSec;
    state.pingData = pingData;
    state.geoTestedRegions = geoTestedRegions;
    state.screenState = ScreenState::MainMenu;
}

// Cleanly close the game. Go back to main menu but don't log
void app_closeGame()
{
    isDisconnecting = true;
    pBCWrapper->getRelayService()->deregisterRelayCallback();
    pBCWrapper->getRelayService()->deregisterSystemCallback();
    pBCWrapper->getRelayService()->disconnect();
    pBCWrapper->getRTTService()->deregisterAllRTTCallbacks();
    pBCWrapper->getRTTService()->disableRTT();

    // Reset state but keep the user and app-level config around
    User user = state.user;
    auto appLobbies = state.appLobbies;
    int splotchDurationSec = state.splotchDurationSec;
    auto pingData = state.pingData;
    auto geoTestedRegions = state.geoTestedRegions;
    s_geoTestRegion.clear();
    state = State();
    state.user = user;
    state.user.isAlive = false;
    state.user.isReady = false;
    state.appLobbies = appLobbies;
    state.splotchDurationSec = splotchDurationSec;
    state.pingData = pingData;
    state.geoTestedRegions = geoTestedRegions;
    state.screenState = ScreenState::MainMenu;
}

// Ready up and signals RTT service we can start the game
void app_startGame()
{
    state.user.isReady = true;
    state.screenState = ScreenState::Starting;
    loading_text = "Starting...";
    loading_reset_timer();
    pBCWrapper->getLobbyService()->updateReady(
        state.lobby.lobbyId,
        state.user.isReady,
        buildExtraJson());
}

// User changes his player color
void app_changeUserColor(int colorIndex)
{
    state.user.colorIndex = colorIndex;
    for (auto &member : state.lobby.members)
    {
        if (state.user.cxId == member.cxId)
        {
            member.colorIndex = colorIndex;
            break;
        }
    }

    pBCWrapper->getLobbyService()->updateReady(
        state.lobby.lobbyId,
        state.user.isReady,
        buildExtraJson());
}

static uint64_t getPlayerMask()
{
    uint64_t playerMask = 0;

    for (const auto &user : state.lobby.members)
    {
        if (!user.allowSendTo)
            continue;
        auto netId = pBCWrapper->getRelayService()->getNetIdForCxId(user.cxId);
        playerMask |= (uint64_t)1 << (uint64_t)netId;
    }

    return playerMask;
}

// User moved mouse in the play area
void app_mouseMoved(const Point &pos)
{
    state.user.isAlive = true;
    state.user.pos = pos;
    for (auto &member : state.lobby.members)
    {
        if (state.user.cxId == member.cxId)
        {
            member.isAlive = true;
            member.pos = pos;
            break;
        }
    }

    // Send to other players
    Json::Value json;
    json["op"] = "move";
    json["data"]["x"] = pos.x / 800.0f;
    json["data"]["y"] = pos.y / 600.0f;

    Json::FastWriter writer;
    auto str = writer.write(json);

    pBCWrapper->getRelayService()->sendToAll(
        (const uint8_t *)str.data(), (int)str.length(),
        settings.sendReliable, // Unreliable
        settings.sendOrdered,  // Ordered
        (BrainCloud::eRelayChannel)settings.sendChannel);
}

// End the current match and return all players to the lobby for another round.
// Only the lobby owner should call this. RTT stays alive so the lobby persists.
void app_endMatch()
{
    Json::Value extraJson;
    extraJson["cxId"] = state.user.cxId;
    extraJson["lobbyId"] = state.lobby.lobbyId;
    extraJson["op"] = "END_MATCH";

    Json::FastWriter writer;
    pBCWrapper->getRelayService()->endMatch(writer.write(extraJson));
}

// User clicked mouse in the play area
void app_shockwave(const Point &pos)
{
    // Send to other players
    Json::Value json;
    json["op"] = "shockwave";
    json["data"]["x"] = pos.x / 800.0f;
    json["data"]["y"] = pos.y / 600.0f;
    json["data"]["teamCode"] = 0;

    Json::FastWriter writer;
    auto str = writer.write(json);

    pBCWrapper->getRelayService()->sendToPlayers(
        (const uint8_t *)str.data(), (int)str.length(),
        getPlayerMask(),
        true,  // Reliable
        false, // Unordered
        (BrainCloud::eRelayChannel)settings.sendChannel);

    // Create a local shockwave so we can see it
    Shockwave shockwave;
    shockwave.pos = pos;
    shockwave.colorIndex = state.user.colorIndex;
    shockwave.startTime = std::chrono::high_resolution_clock::now();
    state.shockwaves.push_back(shockwave);

    // Leave a persistent splotch at the same location
    Splotch splotch;
    splotch.pos = pos;
    splotch.colorIndex = state.user.colorIndex;
    splotch.startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    state.splotches.push_back(splotch);
}

// Host clears all splotches on every client
void app_clearSplotches()
{
    state.splotches.clear();

    Json::Value json;
    json["op"] = "clear_splotches";
    Json::FastWriter writer;
    auto str = writer.write(json);
    pBCWrapper->getRelayService()->sendToAll(
        (const uint8_t *)str.data(), (int)str.length(),
        true, false, (BrainCloud::eRelayChannel)0);
}

// Broadcast our current relay RTT to all other players.
// Called periodically from game_update() so every client can see each other's live ping.
void app_broadcastRelayPing()
{
    int ping = pBCWrapper->getRelayService()->getPing();

    // Update own entry immediately — no need to wait for a relay echo
    for (auto &member : state.lobby.members)
    {
        if (member.cxId == state.user.cxId)
        {
            member.activePing = ping;
            break;
        }
    }

    Json::Value json;
    json["op"] = "relay_ping";
    json["data"]["ping"] = ping;
    Json::FastWriter writer;
    auto str = writer.write(json);
    pBCWrapper->getRelayService()->sendToAll(
        (const uint8_t *)str.data(), (int)str.length(),
        false, // unreliable — stale pings are harmless to drop
        false,
        (BrainCloud::eRelayChannel)0);
}
