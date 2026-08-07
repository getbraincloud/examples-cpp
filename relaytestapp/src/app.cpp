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
#include "coverage.h"
#include "game.h"
#include "globals.h"
#include "loading.h"
#include "lobby.h"
#include "matchSummary.h"
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
#include <climits>

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
static void startLobbySearchFlow();
static uint64_t getPlayerMask();
static void sendGameStartToMask(uint64_t playerMask);
static void sendSplotchSyncToMask(uint64_t mask);
static void sendMatchResultToMask(uint64_t mask, int round, const std::vector<CoverageEntry> &coverage);
static std::vector<MatchResultEntry> toMatchResultEntries(const std::vector<CoverageEntry> &coverage);
static void postMatchScoresAndComputeDeltas(const MatchResultEntry &mine);
static void applyMatchResult(int round, const std::vector<MatchResultEntry> &entries);
static void onRelayConnected();
static void sendLeaderboardDeltaToMask(uint64_t mask, const LeaderboardDelta &delta);

static bool isDisconnecting = false;

// Chunk accumulator for the in-progress "match_result" reassembly (see onRelayMessage).
// Reset on "first":true and whenever a new round starts (onRelayConnected).
static std::vector<MatchResultEntry> s_pendingMatchResult;

// A player's "lb_result" can arrive before match_result has populated state.matchResult.
// entries for this round (they're broadcast by different senders on different channels,
// so relative ordering isn't guaranteed) — buffered here by cxId and drained into the
// matching entry as soon as applyMatchResult() sets entries. Reset every round.
static std::map<std::string, LeaderboardDelta> s_pendingLbResults;

// Incremented on every app_play() call. Each ping-flow lambda captures this value and
// checks it before acting — stale callbacks from a previous session are silently dropped.
static int s_playGeneration = 0;

// True only when RTT was enabled to start an actual lobby search (via app_play()) —
// as opposed to being enabled just so main-menu chat has a live RTT connection
// (chat's REST-style calls all fail with RTT_NOT_ENABLED otherwise). Checked in
// onRTTConnected() so reaching the main menu doesn't silently auto-join a lobby.
static bool s_wantsLobbySearch = false;

// True from the moment enableRTT() is called until rttConnectSuccess/Failure fires.
// getRTTEnabled() alone isn't enough to guard re-entry: it only flips true once the
// connection actually completes, so anything that calls enableRTT() every frame while
// disconnected (e.g. ensureChatChannel()'s "just in case" retry) would otherwise fire
// enableRTT() again on every frame of that connecting window — the SDK's RTTComms::
// connect() then runs concurrently on more than one background thread against the same
// unsynchronized internal Json::Value state, which is what was crashing with a SIGSEGV
// deep in JsonCpp's tree code. Checked/set at both enableRTT() call sites below.
static bool s_rttConnecting = false;

// Tracks the region chosen for the current geo test lobby attempt.
// Set when we pick the best un-tested region; recorded to geoTestedRegions on ROOM_READY.
static std::string s_geoTestRegion;

// brainCloud RTT Connection callbacks
class RTTConnectCallback final : public BrainCloud::IRTTConnectCallback
{
public:
    void rttConnectSuccess() override
    {
        s_rttConnecting = false;
        onRTTConnected();
    }

    void rttConnectFailure(const std::string &errorMessage) override
    {
        s_rttConnecting = false;
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
        state.isProvisioning = false;
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
                           "Cursor Party");

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

    // Colours: comma-separated hex RGB values (same format as GDScript / JS clients)
    const auto &coloursProp = result["data"]["Colours"]["value"];
    if (!coloursProp.isNull())
    {
        g_colors.clear();
        std::string csv = coloursProp.asString();
        size_t start = 0;
        while (start < csv.size())
        {
            size_t end = csv.find(',', start);
            if (end == std::string::npos) end = csv.size();
            std::string token = csv.substr(start, end - start);
            // Trim whitespace
            while (!token.empty() && (token.front() == ' ' || token.front() == '\t')) token.erase(token.begin());
            while (!token.empty() && (token.back()  == ' ' || token.back()  == '\t')) token.pop_back();
            if (token.size() >= 6)
            {
                unsigned int r = 0, g = 0, b = 0;
                if (sscanf(token.c_str(), "%02x%02x%02x", &r, &g, &b) == 3)
                    g_colors.push_back(ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f));
            }
            start = end + 1;
        }
        if (g_colors.empty())
            g_colors.clear(); // parse failed — getColor() falls back to COLORS[]
    }

    // SplotchDuration: seconds a splotch persists on the canvas (-1 = forever)
    const auto &durProp = result["data"]["SplotchDuration"]["value"];
    if (!durProp.isNull())
        state.splotchDurationSec = durProp.asInt();
    else
        state.splotchDurationSec = -1;

    // Leaderboard ids — optional global properties, so the boards can be created/renamed
    // in the portal with no client rebuild. Falls back to the compiled-in defaults in
    // globals.h when a property is absent.
    auto readLeaderboardId = [&](const char *propName, std::string &out)
    {
        const auto &prop = result["data"][propName]["value"];
        if (!prop.isNull() && !prop.asString().empty())
            out = prop.asString();
    };
    readLeaderboardId("CoverageLeaderboardId", state.coverageLeaderboardId);
    readLeaderboardId("CoverageLeaderboardIdQuarterly", state.coverageLeaderboardIdQuarterly);
    readLeaderboardId("PointsLeaderboardId", state.pointsLeaderboardId);
    readLeaderboardId("PointsLeaderboardIdQuarterly", state.pointsLeaderboardIdQuarterly);

    state.screenState = ScreenState::MainMenu;
    app_enableChatRTT();
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
                app_enableChatRTT();
            }));
}

// Build the "extra" JSON for lobby calls — includes colorIndex + pings when available.
static std::string buildExtraJson()
{
    Json::Value extra;
    extra["colorIndex"] = state.user.colorIndex;
    extra["rank"] = state.user.worldwideRank;
    if (!state.pingData.empty())
    {
        Json::Value pings(Json::objectValue);
        for (const auto &kv : state.pingData)
            pings[kv.first] = kv.second;
        extra["pings"] = pings;
    }
    Json::FastWriter w;
    auto s = w.write(extra);
    if (!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

static void doFindOrCreateLobby(const std::string &lobbyType)
{
    loading_status = "Finding lobby...";
    pBCWrapper->getLobbyService()->findOrCreateLobby(
        lobbyType, 0, 1,
        "{\"strategy\":\"ranged-absolute\",\"alignment\":\"center\",\"ranges\":[1000]}",
        "{}", {}, "{}", false,
        buildExtraJson(),
        settings.teamCode,
        new BCCallback(
            [](const Json::Value &) {},
            [](const std::string &msg) { errorAndReturnToMenu("Failed to find lobby:\n" + msg); }));
}

static void doFindOrCreateLobbyWithPingData(const std::string &lobbyType)
{
    state.pingData = pBCWrapper->getLobbyService()->getPingData();
    loading_status = "Finding lobby...";
    pBCWrapper->getLobbyService()->findOrCreateLobbyWithPingData(
        lobbyType, 0, 1,
        "{\"strategy\":\"ranged-absolute\",\"alignment\":\"center\",\"ranges\":[1000]}",
        "{}", {}, "{}", false,
        buildExtraJson(),
        settings.teamCode,
        new BCCallback(
            [](const Json::Value &) {},
            [](const std::string &msg) { errorAndReturnToMenu("Failed to find lobby:\n" + msg); }));
}

// RTT connected — optionally ping regions before finding a lobby. Only runs when
// RTT was enabled to actually search for a lobby (see s_wantsLobbySearch) — RTT
// enabled just for main-menu chat should not auto-join anything.
static void startLobbySearchFlow()
{
    // Guard: RTT can reconnect mid-session. Only start one ping flow per app_play() call.
    static int s_pingStartedGen = -1;
    if (s_pingStartedGen == s_playGeneration)
        return;
    s_pingStartedGen = s_playGeneration;

    if (!settings.usePingData && !settings.autoGeoTest)
    {
        doFindOrCreateLobby(settings.lobbyType);
        return;
    }

    const int gen = s_playGeneration;
    loading_status = "Getting regions...";
    pBCWrapper->getLobbyService()->getRegionsForLobbies(
        {settings.lobbyType},
        new BCCallback(
            [gen](const Json::Value &result)
            {
                if (isDisconnecting || gen != s_playGeneration) return;

                state.expectedPingRegions.clear();
                const auto &regionPingData = result["data"]["regionPingData"];
                if (!regionPingData.isNull())
                    for (const auto &r : regionPingData.getMemberNames())
                        state.expectedPingRegions.push_back(r);
                std::sort(state.expectedPingRegions.begin(), state.expectedPingRegions.end());

                pBCWrapper->getLobbyService()->pingRegions(
                    new BCCallback(
                        [gen](const Json::Value &)
                        {
                            if (isDisconnecting || gen != s_playGeneration) return;
                            state.pingData = pBCWrapper->getLobbyService()->getPingData();
                            state.expectedPingRegions.clear();

                            std::string lobbyType = settings.lobbyType;
                            s_geoTestRegion.clear();
                            if (settings.autoGeoTest && isRegionalCyclingLobby(settings.lobbyType) && !state.pingData.empty())
                            {
                                std::string bestRegion;
                                int bestPing = INT_MAX;
                                for (const auto &kv : state.pingData)
                                {
                                    if (regionToSpecificLobbyType(settings.lobbyType, kv.first).empty()) continue;
                                    const auto &tested = state.geoTestedRegions;
                                    bool alreadyTested = std::find(tested.begin(), tested.end(), kv.first) != tested.end();
                                    if (!alreadyTested && kv.second < bestPing) { bestPing = kv.second; bestRegion = kv.first; }
                                }
                                if (bestRegion.empty()) // all tested — wrap to global fastest
                                {
                                    for (const auto &kv : state.pingData)
                                    {
                                        if (regionToSpecificLobbyType(settings.lobbyType, kv.first).empty()) continue;
                                        if (kv.second < bestPing) { bestPing = kv.second; bestRegion = kv.first; }
                                    }
                                }
                                s_geoTestRegion = bestRegion;
                                lobbyType = regionToSpecificLobbyType(settings.lobbyType, bestRegion);
                            }

                            if (settings.usePingData)
                                doFindOrCreateLobbyWithPingData(lobbyType);
                            else
                                doFindOrCreateLobby(lobbyType);
                        },
                        [gen](const std::string &)
                        {
                            if (isDisconnecting || gen != s_playGeneration) return;
                            state.expectedPingRegions.clear();
                            doFindOrCreateLobby(settings.lobbyType);
                        }));
            },
            [gen](const std::string &)
            {
                if (isDisconnecting || gen != s_playGeneration) return;
                doFindOrCreateLobby(settings.lobbyType);
            }));
}

// RTT connected. Always records our RTT connection id; only kicks off a lobby
// search when RTT was enabled for that purpose (app_play() sets s_wantsLobbySearch) —
// RTT enabled for main-menu chat (app_enableChatRTT()) should not auto-join anything.
void onRTTConnected()
{
    state.user.cxId = pBCWrapper->getRTTService()->getRTTConnectionId();
    if (s_wantsLobbySearch)
        startLobbySearchFlow();
}

// Enables RTT so main-menu chat works — brainCloud's chat calls (getChannelId,
// getRecentChatMessages, postChatMessageSimple) all fail with RTT_NOT_ENABLED
// otherwise. Idempotent: no-ops if RTT is already connected (e.g. a lobby search
// already turned it on). Called whenever the app reaches the MainMenu screen.
void app_enableChatRTT()
{
    // Called at every MainMenu arrival — piggyback the rank re-fetch here too
    // rather than touching every one of those call sites separately. Cheap and
    // idempotent (no-ops while a request is already in flight).
    app_fetchWorldwideRank();

    if (pBCWrapper->getRTTService()->getRTTEnabled() || s_rttConnecting)
        return;
    s_wantsLobbySearch = false;
    s_rttConnecting = true;
    pBCWrapper->getRTTService()->registerRTTLobbyCallback(&bcRTTCallback);
    pBCWrapper->getRTTService()->enableRTT(&bcRTTConnectCallback, true);
}

// Fetches this player's own rank on the coverage leaderboard, for the lobby member
// list's "Worldwide Rank" display. There's no client API to look up an ARBITRARY
// other player's rank (GetPlayersSocialLeaderboard/GetPlayerScore return score, not
// rank; GetGlobalLeaderboardView's rank is self-centric only) — so each player
// fetches their own and shares it via the lobby's "extra" field, the same mechanism
// already used for colorIndex/pings. -1 = unknown or no score posted yet.
// Idempotent-ish: safe to call repeatedly (e.g. every MainMenu arrival); a request
// already in flight is not re-issued.
static bool s_rankFetchInFlight = false;
void app_fetchWorldwideRank()
{
    if (s_rankFetchInFlight || !pBCWrapper || state.coverageLeaderboardId.empty()) return;
    s_rankFetchInFlight = true;
    pBCWrapper->getSocialLeaderboardService()->getGlobalLeaderboardView(
        state.coverageLeaderboardId.c_str(), BrainCloud::HIGH_TO_LOW, 0, 0,
        new BCCallback(
            [](const Json::Value &result)
            {
                s_rankFetchInFlight = false;
                const auto &arr = result["data"]["leaderboard"];
                int rank = (!arr.empty()) ? arr[0]["rank"].asInt() : -1;
                if (rank == state.user.worldwideRank) return;
                state.user.worldwideRank = rank;
                // If already in a lobby, push the freshly-known rank to lobby-mates
                // right away instead of waiting for some other reason to re-send extra.
                if (!state.lobby.lobbyId.empty())
                    pBCWrapper->getLobbyService()->updateReady(
                        state.lobby.lobbyId, state.user.isReady, buildExtraJson());
            },
            [](const std::string &) { s_rankFetchInFlight = false; }));
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
    s_rttConnecting = false; // callbacks just deregistered — nothing will clear this otherwise

    // Reset state but keep user, app config, and geo test results
    User user = state.user;
    auto appLobbies = state.appLobbies;
    int splotchDurationSec = state.splotchDurationSec;
    auto pingData = state.pingData;
    auto geoTestedRegions = state.geoTestedRegions;
    auto geoTestResults = state.geoTestResults;
    s_geoTestRegion.clear();
    state = State();
    state.user = user;
    state.appLobbies = appLobbies;
    state.splotchDurationSec = splotchDurationSec;
    state.pingData = pingData;
    state.geoTestedRegions = geoTestedRegions;
    state.geoTestResults = geoTestResults;
    state.screenState = ScreenState::MainMenu;
    app_enableChatRTT(); // RTT was just disabled above — re-enable it for main-menu chat

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
    s_rttConnecting = false; // callbacks just deregistered — nothing will clear this otherwise

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
        entry["x"] = s.pos.x / CANVAS_W;
        entry["y"] = s.pos.y / CANVAS_H;
        entry["c"] = s.colorIndex;
        entry["t"] = (Json::Int64)s.startTimeMs;
        entry["a"] = s.rotation;
        if (!s.ownerCxId.empty())
        {
            // Compact netId, not the ~80-char cxId, to stay inside the chunk byte budget.
            // Resolved back to a cxId at receive time (see onRelayMessage's splotch_sync
            // branch) — never deferred, since RelayComms clears its netId maps at END_MATCH.
            int ownerNetId = pBCWrapper->getRelayService()->getNetIdForCxId(s.ownerCxId);
            if (ownerNetId >= 0 && ownerNetId < MAX_LOBBY_MEMBERS)
                entry["o"] = ownerNetId;
        }

        // Measure this entry's serialized size (+1 for the separating comma)
        int entrySize = (int)writer.write(entry).size() + 1;
        if (currentSize + entrySize > MAX_RELAY_BYTES && !chunk.empty())
            flushChunk();

        chunk.push_back(std::move(entry));
        currentSize += entrySize;
    }
    flushChunk();
}

// Broadcasts the host's authoritative final coverage/ranking to a player mask, chunked
// like sendSplotchSyncToMask but sent reliable + ORDERED (splotch_sync stays unordered —
// cosmetic chunk-reassembly races there are invisible; here they'd corrupt a posted score).
static void sendMatchResultToMask(uint64_t mask, int round, const std::vector<CoverageEntry> &coverage)
{
    if (coverage.empty() || mask == 0) return;

    static const int MAX_RELAY_BYTES = 900;
    static const int ENVELOPE_OVERHEAD = 80;

    Json::FastWriter writer;
    bool isFirst = true;
    std::vector<Json::Value> chunk;
    int currentSize = ENVELOPE_OVERHEAD;

    auto flushChunk = [&](bool isLast)
    {
        if (chunk.empty() && !isLast) return;
        Json::Value resultJson;
        resultJson["op"] = "match_result";
        resultJson["data"]["round"] = round;
        resultJson["data"]["first"] = isFirst;
        resultJson["data"]["last"] = isLast;
        Json::Value arr(Json::arrayValue);
        for (const auto &entry : chunk)
            arr.append(entry);
        resultJson["data"]["e"] = arr;
        auto str = writer.write(resultJson);
        pBCWrapper->getRelayService()->sendToPlayers(
            (const uint8_t *)str.data(), (int)str.length(),
            mask,
            true, // reliable
            true, // ordered — see comment above
            (BrainCloud::eRelayChannel)0);
        isFirst = false;
        chunk.clear();
        currentSize = ENVELOPE_OVERHEAD;
    };

    // Identifies each entry by cxId directly, NOT a relay netId. This used to resolve
    // c.cxId -> netId here and skip the entry entirely if that failed ("no longer
    // connected") — but that resolution is unreliable enough in practice (root cause not
    // fully nailed down) that it was silently dropping players who were still genuinely in
    // the match, which is how a real 4-player match_result ended up being received as a
    // single entry by everyone. match_result is a single small once-per-round broadcast,
    // so the extra bytes of a full cxId per entry cost nothing — there's no reason to
    // depend on netId resolution for this at all when we already know every member's cxId
    // from state.lobby.members.
    for (const auto &c : coverage)
    {
        Json::Value entry;
        entry["cx"] = c.cxId;
        entry["r"] = c.rank;
        entry["c"] = (int)(c.coveragePct * 100.0f + 0.5f); // basis points, 0-10000
        entry["b"] = c.beaten;

        int entrySize = (int)writer.write(entry).size() + 1;
        if (currentSize + entrySize > MAX_RELAY_BYTES && !chunk.empty())
            flushChunk(false);

        chunk.push_back(std::move(entry));
        currentSize += entrySize;
    }
    flushChunk(true);
}

static std::vector<MatchResultEntry> toMatchResultEntries(const std::vector<CoverageEntry> &coverage)
{
    std::vector<MatchResultEntry> out;
    out.reserve(coverage.size());
    for (const auto &c : coverage)
    {
        MatchResultEntry e;
        e.cxId = c.cxId;
        e.rank = c.rank;
        e.coveragePct = c.coveragePct;
        e.beaten = c.beaten;
        out.push_back(e);
    }
    return out;
}

// Broadcasts this client's own computed leaderboard movement to the rest of the match.
// Small and never chunked (four small before/after rank pairs, nowhere near the ~900
// byte budget splotch_sync/match_result have to worry about).
static void sendLeaderboardDeltaToMask(uint64_t mask, const LeaderboardDelta &delta)
{
    if (mask == 0) return;
    int netId = pBCWrapper->getRelayService()->getNetIdForCxId(state.user.cxId);
    if (netId < 0 || netId >= MAX_LOBBY_MEMBERS) return;

    Json::Value json;
    json["op"] = "lb_result";
    json["data"]["n"] = netId;

    auto putPeriod = [&](const char *key, const LeaderboardPeriodDelta &pd)
    {
        if (!pd.improved) return; // absent key == "no change" on receipt
        json["data"][key]["b"] = pd.rankBefore;
        json["data"][key]["a"] = pd.rankAfter;
    };
    putPeriod("pl", delta.pointsLifetime);
    putPeriod("pq", delta.pointsQuarterly);
    putPeriod("cl", delta.coverageLifetime);
    putPeriod("cq", delta.coverageQuarterly);

    Json::FastWriter writer;
    auto str = writer.write(json);
    pBCWrapper->getRelayService()->sendToPlayers(
        (const uint8_t *)str.data(), (int)str.length(),
        mask,
        true, // reliable
        true, // ordered
        (BrainCloud::eRelayChannel)0);
}

// Writes this client's own finished delta into its match_result entry (no relay round
// trip needed for yourself) and shares it with everyone else in the match.
static void applyLeaderboardDeltaSelf(const LeaderboardDelta &delta)
{
    for (auto &e : state.matchResult.entries)
    {
        if (e.cxId == state.user.cxId)
        {
            e.lbDelta = delta;
            break;
        }
    }
    sendLeaderboardDeltaToMask(getPlayerMask(), delta);
}

// One board's full before -> post -> after chain. Fetches the player's current rank/score
// on leaderboardId (GetGlobalLeaderboardView with before/afterCount 0 is the documented way
// to get just the caller's own entry), posts the new score, then re-fetches to see what
// actually stuck. isPersonalBestStyle selects what "improved" means:
//   - coverage boards (kept-best score): improved = this score beat the previous best.
//     Posting a LOWER coverage than your best is a no-op server-side, so comparing scores
//     is the only reliable signal — rank alone could move for reasons unrelated to this
//     round (other players posting later).
//   - points boards (cumulative score): every post raises the score, so "did the score
//     increase" is always true and useless; the real signal is whether RANK improved.
// Either way rankBefore/rankAfter are captured for display regardless of which one gates
// the badge — the summary screen's "Personal best" badge still shows a rank movement.
static void chainLeaderboardBoard(
    const std::string &leaderboardId, int64_t score, const std::string &otherDataStr,
    bool isPersonalBestStyle,
    std::shared_ptr<LeaderboardPeriodDelta> outDelta,
    std::shared_ptr<int> remaining,
    std::function<void()> finalize)
{
    if (leaderboardId.empty())
    {
        if (--(*remaining) == 0) finalize();
        return;
    }

    auto onDone = [=]() { if (--(*remaining) == 0) finalize(); };

    pBCWrapper->getSocialLeaderboardService()->getGlobalLeaderboardView(
        leaderboardId.c_str(), BrainCloud::HIGH_TO_LOW, 0, 0,
        new BCCallback(
            [=](const Json::Value &beforeResult)
            {
                const auto &beforeArr = beforeResult["data"]["leaderboard"];
                int rankBefore = -1;
                int64_t scoreBefore = -1;
                if (!beforeArr.empty())
                {
                    rankBefore = beforeArr[0]["rank"].asInt();
                    scoreBefore = beforeArr[0]["score"].asInt64();
                }

                pBCWrapper->getSocialLeaderboardService()->postScoreToLeaderboard(
                    leaderboardId.c_str(), score, otherDataStr,
                    new BCCallback(
                        [=](const Json::Value &)
                        {
                            pBCWrapper->getSocialLeaderboardService()->getGlobalLeaderboardView(
                                leaderboardId.c_str(), BrainCloud::HIGH_TO_LOW, 0, 0,
                                new BCCallback(
                                    [=](const Json::Value &afterResult)
                                    {
                                        const auto &afterArr = afterResult["data"]["leaderboard"];
                                        int rankAfter = -1;
                                        int64_t scoreAfter = -1;
                                        if (!afterArr.empty())
                                        {
                                            rankAfter = afterArr[0]["rank"].asInt();
                                            scoreAfter = afterArr[0]["score"].asInt64();
                                        }
                                        outDelta->rankBefore = rankBefore;
                                        outDelta->rankAfter = rankAfter;
                                        outDelta->improved = isPersonalBestStyle
                                            ? (scoreBefore < 0 || scoreAfter > scoreBefore)
                                            : (rankAfter > 0 && (rankBefore < 0 || rankAfter < rankBefore));
                                        onDone();
                                    },
                                    [=](const std::string &) { outDelta->rankBefore = rankBefore; onDone(); }));
                        },
                        [=](const std::string &) { onDone(); }));
            },
            [=](const std::string &)
            {
                // Before-fetch failed — still try to post so the score isn't lost, just
                // without a delta to show (rankBefore/After stay -1, improved stays false).
                pBCWrapper->getSocialLeaderboardService()->postScoreToLeaderboard(
                    leaderboardId.c_str(), score, otherDataStr,
                    new BCCallback([=](const Json::Value &) { onDone(); },
                                    [=](const std::string &) { onDone(); }));
            }));
}

// Posts this client's own final standing to the four leaderboards, and computes/shares
// how that changed its own rank on each of them (BCLOUD-14489's Match Summary screen).
// Coverage score is basis points (0-10000) so the portal isn't stuck with float scores;
// points score is "players beaten" + a flat completion bonus (so a solo match — 0 beaten
// — still posts 1, per the ticket: "+1 bonus point for completing a game").
static void postMatchScoresAndComputeDeltas(const MatchResultEntry &mine)
{
    int64_t basisPoints = (int64_t)(mine.coveragePct * 100.0f + 0.5f);
    int64_t points = mine.beaten + 1;

    Json::Value otherData;
    otherData["round"] = state.roundNumber;
    otherData["rank"] = mine.rank;
    // GetGlobalLeaderboardPage/View don't return a usable "name" field for arbitrary
    // (non-friend) entries — embedding it in the score's user-defined data is the
    // standard way to show a display name in a leaderboard viewer (see mainMenu.cpp).
    otherData["name"] = state.user.name;
    Json::FastWriter writer;
    auto otherDataStr = writer.write(otherData);

    auto pointsLifetime = std::make_shared<LeaderboardPeriodDelta>();
    auto pointsQuarterly = std::make_shared<LeaderboardPeriodDelta>();
    auto coverageLifetime = std::make_shared<LeaderboardPeriodDelta>();
    auto coverageQuarterly = std::make_shared<LeaderboardPeriodDelta>();
    auto remaining = std::make_shared<int>(4);

    auto finalize = [pointsLifetime, pointsQuarterly, coverageLifetime, coverageQuarterly]()
    {
        LeaderboardDelta delta;
        delta.ready = true;
        delta.pointsLifetime = *pointsLifetime;
        delta.pointsQuarterly = *pointsQuarterly;
        delta.coverageLifetime = *coverageLifetime;
        delta.coverageQuarterly = *coverageQuarterly;
        applyLeaderboardDeltaSelf(delta);
    };

    chainLeaderboardBoard(state.pointsLeaderboardId, points, otherDataStr, false, pointsLifetime, remaining, finalize);
    chainLeaderboardBoard(state.pointsLeaderboardIdQuarterly, points, otherDataStr, false, pointsQuarterly, remaining, finalize);
    chainLeaderboardBoard(state.coverageLeaderboardId, basisPoints, otherDataStr, true, coverageLifetime, remaining, finalize);
    chainLeaderboardBoard(state.coverageLeaderboardIdQuarterly, basisPoints, otherDataStr, true, coverageQuarterly, remaining, finalize);
}

// Applies an authoritative coverage snapshot for a round — either a locally-computed one
// (host, or a no-result fallback) or one just reassembled from a "match_result" broadcast.
// Idempotent per round: a migrated host's broadcast racing the original host's (or the
// END_MATCH fallback racing a late match_result) is safe to apply/post more than once —
// only the FIRST application for a given round has any effect. This guard is mandatory
// because the points leaderboard is CUMULATIVE; a duplicate post would silently and
// permanently inflate a lifetime total with no way to detect it afterward.
static void applyMatchResult(int round, const std::vector<MatchResultEntry> &entries)
{
    if (state.matchResult.valid && state.matchResult.round == round)
        return;

    state.matchResult.valid = true;
    state.matchResult.round = round;
    state.matchResult.entries = entries;

    // Drain any "lb_result" broadcasts that arrived before this round's match_result did
    // (different senders, no relative ordering guarantee between them — see s_pendingLbResults).
    for (auto &e : state.matchResult.entries)
    {
        auto it = s_pendingLbResults.find(e.cxId);
        if (it != s_pendingLbResults.end())
        {
            e.lbDelta = it->second;
            s_pendingLbResults.erase(it);
        }
    }

    if (state.leaderboardPostedRound == round)
        return;
    state.leaderboardPostedRound = round;

    for (const auto &e : entries)
    {
        if (e.cxId == state.user.cxId)
        {
            postMatchScoresAndComputeDeltas(e);
            break;
        }
    }
}

// Drives the shared coverage/ranking calculation and the host-authoritative match-end
// flow. Called once per frame from game_update() while state.screenState == Game.
//
// isHost is re-evaluated every call from state.lobby.ownerCxId, so a mid-match host
// migration (see the MIGRATE_OWNER handling in onRelaySystemMessage) is handled with no
// special-casing here — the newly-promoted host just starts satisfying "isHost" on its
// next tick and picks up wherever the match clock currently is; it already has
// state.splotches and state.gameStartTime like every other member.
void app_tickMatch()
{
    if (state.gameStartTime == 0) return;

    long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    long long elapsedMs = nowMs - state.gameStartTime;

    // Live coverage recompute + rank-swap detection — shared by the in-match rank board
    // (game.cpp reads state.coverage) and the match-end snapshot below.
    if (state.coverageComputedGen != state.splotchGeneration &&
        nowMs - state.coverageComputedAtMs >= COVERAGE_RECOMPUTE_MS)
    {
        auto fresh = computeCoverage(state.splotches, state.lobby.members);
        for (auto &e : fresh)
        {
            int prevRank = e.rank;
            long long prevChangedAt = 0;
            for (const auto &old : state.coverage)
            {
                if (old.cxId == e.cxId)
                {
                    prevRank = old.rank;
                    prevChangedAt = old.rankChangedAtMs;
                    break;
                }
            }
            e.prevRank = prevRank;
            e.rankChangedAtMs = (prevRank != e.rank) ? nowMs : prevChangedAt;
        }
        state.coverage = std::move(fresh);
        state.coverageComputedGen = state.splotchGeneration;
        state.coverageComputedAtMs = nowMs;
    }

    if (!isCursorPartyLobby(settings.lobbyType))
        return; // auto-end / leaderboard flow is CursorParty-specific

    bool isHost = !state.lobby.ownerCxId.empty() && state.user.cxId == state.lobby.ownerCxId;

    if (state.matchPhase == MatchPhase::Running && elapsedMs >= MATCH_DURATION_MS && isHost)
    {
        if (!(state.matchResult.valid && state.matchResult.round == state.roundNumber))
        {
            auto finalCoverage = computeCoverage(state.splotches, state.lobby.members);
            applyMatchResult(state.roundNumber, toMatchResultEntries(finalCoverage));
            sendMatchResultToMask(getPlayerMask(), state.roundNumber, finalCoverage);
        }
        state.matchPhase = MatchPhase::ResultsBroadcast;
        state.resultsSentAtMs = nowMs;
    }
    else if (state.matchPhase == MatchPhase::ResultsBroadcast && isHost &&
             nowMs - state.resultsSentAtMs >= RESULT_GRACE_MS)
    {
        app_endMatch();
        state.matchPhase = MatchPhase::Ended;
    }

    // Watchdog: well past the deadline with no authoritative result at all (a dropped
    // broadcast, or a gap during host migration where no one was host for a while) —
    // compute and post locally so the round can't hang forever. Cheap and idempotent.
    if (!state.matchResult.valid && elapsedMs >= MATCH_DURATION_MS + RESULT_GRACE_MS + 3000)
    {
        auto finalCoverage = computeCoverage(state.splotches, state.lobby.members);
        applyMatchResult(state.roundNumber, toMatchResultEntries(finalCoverage));
        if (isHost && state.matchPhase != MatchPhase::Ended)
        {
            sendMatchResultToMask(getPlayerMask(), state.roundNumber, finalCoverage);
            state.matchPhase = MatchPhase::ResultsBroadcast;
            state.resultsSentAtMs = nowMs;
        }
    }
}

// Called when relay connection succeeds. Owner sets and broadcasts the authoritative game start time.
static void onRelayConnected()
{
    ++state.roundNumber;

    // Fresh round — reset all per-round match/coverage state so nothing carries over
    // from the previous round (this replaces the old file-static "matchEndRound" guard,
    // which had a live bug: it wasn't reset across lobbies, so auto-end could silently
    // stop firing on a second lobby in the same session).
    state.matchPhase = MatchPhase::Running;
    state.matchResult = MatchResult();
    state.leaderboardPostedRound = -1;
    state.coverage.clear();
    state.coverageComputedGen = (unsigned long long)-1;
    state.resultsSentAtMs = 0;
    s_pendingMatchResult.clear();
    s_pendingLbResults.clear();
    state.awaitingRematch = false;
    state.isProvisioning = false;

    // Auto geo test: relay connect confirms the region is reachable.
    // Record the connect time; app_update() disconnects after a 2.5s soak.
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
    else if (json["op"].asString() == "MIGRATE_OWNER") // Relay reassigned the host role
    {
        // Reconcile relay-level ownership into the SAME field used everywhere for isHost
        // checks (state.lobby.ownerCxId — set from the Lobby/RTT service elsewhere). The
        // Lobby service's own owner field will also catch up via the next lobby-update
        // event; this is just the faster of the two signals. app_tickMatch() re-evaluates
        // isHost every tick, so a newly-promoted host resumes match duties (auto-end,
        // match_result broadcast) with no extra state transfer — it already has
        // state.splotches and state.gameStartTime like every other member.
        const auto &newOwnerCxId = json["cxId"].asString();
        if (!newOwnerCxId.empty())
            state.lobby.ownerCxId = newOwnerCxId;
    }
    else if (json["op"].asString() == "END_MATCH") // Match ended, return all players to lobby
    {
        // Fallback: if no authoritative match_result ever arrived for this round (legacy
        // host, dropped broadcast, or a migration gap), compute locally and post using
        // local numbers before the canvas clears below. applyMatchResult() is idempotent
        // per round, so this is a no-op if a result already landed.
        if (!(state.matchResult.valid && state.matchResult.round == state.roundNumber))
        {
            auto finalCoverage = computeCoverage(state.splotches, state.lobby.members);
            applyMatchResult(state.roundNumber, toMatchResultEntries(finalCoverage));
        }

        // Reset per-round state immediately
        state.user.isAlive = false;
        state.user.isReady = false;
        state.shockwaves.clear();
        state.splotches.clear();
        ++state.splotchGeneration;
        state.gameStartTime = 0;

        // CursorParty rounds get the full Match Summary + rematch-queue screen
        // (BCLOUD-14489); every other lobby type (geo test, RoomServer, etc.) keeps the
        // old behavior of dropping straight back to the plain Lobby screen — they never
        // populate state.matchResult with anything meaningful for this screen to show.
        if (isCursorPartyLobby(settings.lobbyType) && !settings.autoGeoTest)
        {
            state.screenState = ScreenState::MatchSummary;
            state.matchSummaryArrivalTime = std::chrono::steady_clock::now();
            state.awaitingRematch = true;
            // Actually clear readiness server-side too, not just the local mirror above —
            // otherwise the "Queue for Rematch N/M" count starts from whatever everyone's
            // pre-match ready state still was, since nothing else resets it here.
            pBCWrapper->getLobbyService()->updateReady(
                state.lobby.lobbyId, false, buildExtraJson());
        }
        else
        {
            state.screenState = ScreenState::Lobby;
        }

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
                member.pos.x = (int)(json["data"]["x"].asFloat() * CANVAS_W);
                member.pos.y = (int)(json["data"]["y"].asFloat() * CANVAS_H);
            }
            else if (op == "shockwave")
            {
                Shockwave shockwave;
                shockwave.pos.x = (int)(json["data"]["x"].asFloat() * CANVAS_W);
                shockwave.pos.y = (int)(json["data"]["y"].asFloat() * CANVAS_H);
                shockwave.colorIndex = member.colorIndex;
                shockwave.startTime = std::chrono::high_resolution_clock::now();
                state.shockwaves.push_back(shockwave);

                // Leave a persistent splotch — use angle from message so all clients match
                float angle = json["data"].isMember("angle")
                    ? json["data"]["angle"].asFloat()
                    : ((float)rand() / (float)RAND_MAX) * SPLOTCH_TAU;
                Splotch splotch;
                splotch.pos        = shockwave.pos;
                splotch.colorIndex = member.colorIndex;
                splotch.startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                splotch.rotation   = angle;
                splotch.ownerCxId  = member.cxId; // sender is already resolved above — no wire field needed
                state.splotches.push_back(splotch);
                ++state.splotchGeneration;
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
                    s.pos         = {(int)(entry["x"].asFloat() * CANVAS_W), (int)(entry["y"].asFloat() * CANVAS_H)};
                    s.colorIndex  = entry["c"].asInt();
                    s.startTimeMs = entry["t"].asInt64();
                    s.rotation    = entry.isMember("a")
                        ? entry["a"].asFloat()
                        : ((float)rand() / (float)RAND_MAX) * SPLOTCH_TAU;
                    // "o" is a compact netId, resolved to a cxId now — the map that
                    // resolves it is torn down at END_MATCH, so this must happen at
                    // receive time, never deferred. Missing/unresolvable -> unattributed
                    // (coverage falls back to a colorIndex match).
                    if (entry.isMember("o"))
                    {
                        const auto &ownerCxId = pBCWrapper->getRelayService()->getCxIdForNetId(entry["o"].asInt());
                        s.ownerCxId = ownerCxId;
                    }
                    state.splotches.push_back(s);
                }
                ++state.splotchGeneration;
            }
            else if (op == "clear_splotches")
            {
                state.splotches.clear();
                ++state.splotchGeneration;
            }
            else if (op == "match_result")
            {
                int round = json["data"]["round"].asInt();
                if (!(state.matchResult.valid && state.matchResult.round == round))
                {
                    if (json["data"]["first"].asBool())
                        s_pendingMatchResult.clear();

                    for (const auto &entry : json["data"]["e"])
                    {
                        MatchResultEntry mre;
                        mre.cxId        = entry["cx"].asString();
                        mre.rank        = entry["r"].asInt();
                        mre.coveragePct = entry["c"].asInt() / 100.0f; // basis points -> %
                        mre.beaten      = entry["b"].asInt();
                        s_pendingMatchResult.push_back(mre);
                    }

                    if (json["data"]["last"].asBool())
                    {
                        applyMatchResult(round, s_pendingMatchResult);
                        s_pendingMatchResult.clear();
                    }
                }
            }
            else if (op == "lb_result")
            {
                // Each player's own leaderboard rank movement, broadcast once they've
                // finished computing it (see postMatchScoresAndComputeDeltas). Can arrive
                // before this round's match_result has populated state.matchResult.entries
                // — buffer by cxId in that case (drained in applyMatchResult).
                LeaderboardDelta delta;
                delta.ready = true;
                auto readPeriod = [&](const char *key, LeaderboardPeriodDelta &pd)
                {
                    if (!json["data"].isMember(key)) return; // absent == "no change" for that period
                    pd.improved = true;
                    pd.rankBefore = json["data"][key]["b"].asInt();
                    pd.rankAfter = json["data"][key]["a"].asInt();
                };
                readPeriod("pl", delta.pointsLifetime);
                readPeriod("pq", delta.pointsQuarterly);
                readPeriod("cl", delta.coverageLifetime);
                readPeriod("cq", delta.coverageQuarterly);

                bool applied = false;
                for (auto &e : state.matchResult.entries)
                {
                    if (e.cxId == member.cxId)
                    {
                        e.lbDelta = delta;
                        applied = true;
                        break;
                    }
                }
                if (!applied)
                    s_pendingLbResults[member.cxId] = delta;
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
            pBCWrapper->getLobbyService()->runPingCallbacks();

            // While pinging regions, show per-region progress in the loading status
            if (!state.expectedPingRegions.empty())
            {
                auto snapshot = pBCWrapper->getLobbyService()->getPingData();
                std::string status;
                for (const auto &region : state.expectedPingRegions)
                {
                    if (!status.empty()) status += "\n";
                    auto it = snapshot.find(region);
                    if (it != snapshot.end())
                        status += "  " + region + ": " + (it->second >= 999 ? "T/O" : std::to_string(it->second) + " ms");
                    else
                        status += "  " + region + ": pinging...";
                }
                loading_status = status;
            }

            // Geo test soak timer — wait 2.5s after relay connect to confirm stability
            if (state.geoTestRelayConnectTime != std::chrono::steady_clock::time_point{})
            {
                auto elapsed = std::chrono::steady_clock::now() - state.geoTestRelayConnectTime;
                if (elapsed >= std::chrono::milliseconds(2500))
                {
                    state.geoTestRelayConnectTime = {};
                    state.pendingGeoTestDisconnect = true;
                }
            }

            // Deferred geo test disconnect — tear down relay + RTT and return to main menu
            if (state.pendingGeoTestDisconnect)
            {
                state.pendingGeoTestDisconnect = false;
                isDisconnecting = true;
                int relayPingAtSoak = pBCWrapper->getRelayService()->getPing();
                printf("[GeoTest] Disconnect — %d region(s) tested (relay ping: %dms)\n",
                       (int)state.geoTestedRegions.size(), relayPingAtSoak);
                app_endMatch();
                pBCWrapper->getRelayService()->deregisterRelayCallback();
                pBCWrapper->getRelayService()->deregisterSystemCallback();
                pBCWrapper->getRelayService()->disconnect();
                pBCWrapper->getRTTService()->deregisterAllRTTCallbacks();
                pBCWrapper->getRTTService()->disableRTT();
                s_rttConnecting = false; // callbacks just deregistered — nothing will clear this otherwise
                User user = state.user;
                auto appLobbies = state.appLobbies;
                int splotchDurationSec = state.splotchDurationSec;
                auto pingData = state.pingData;
                auto geoTestedRegions = state.geoTestedRegions;
                auto geoTestResults = state.geoTestResults;
                if (!geoTestedRegions.empty())
                    geoTestResults[geoTestedRegions.back()] = (relayPingAtSoak > 0) ? relayPingAtSoak : -1;
                s_geoTestRegion.clear();
                state = State();
                state.user = user;
                state.user.isAlive = false;
                state.user.isReady = false;
                state.appLobbies = appLobbies;
                state.splotchDurationSec = splotchDurationSec;
                state.pingData = pingData;
                state.geoTestedRegions = geoTestedRegions;
                state.geoTestResults = geoTestResults;
                state.screenState = ScreenState::MainMenu;
                app_enableChatRTT(); // RTT was just disabled above — re-enable it for main-menu chat
                return;
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
                auto color = getColor(settings.colorIndex % colorCount());
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
    case ScreenState::MatchSummary:
        matchSummary_update();
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
    ++s_playGeneration;
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

    s_wantsLobbySearch = true;

    if (pBCWrapper->getRTTService()->getRTTEnabled())
    {
        // RTT is already connected (main-menu chat turned it on) — go straight to
        // the lobby search. Calling enableRTT() again here would be redundant at
        // best; onRTTConnected() won't fire a second time since we're not
        // reconnecting, so this is the only way to pick the search flow back up.
        startLobbySearchFlow();
    }
    else if (!s_rttConnecting)
    {
        // If chat already kicked off a connect (s_rttConnecting true), don't issue a
        // second concurrent enableRTT() — s_wantsLobbySearch is already set above, so
        // whichever caller's connect succeeds will pick up the lobby search from
        // onRTTConnected() regardless of who initiated it.
        s_rttConnecting = true;
        pBCWrapper->getRTTService()->registerRTTLobbyCallback(&bcRTTCallback);
        pBCWrapper->getRTTService()->enableRTT(&bcRTTConnectCallback, true);
    }
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
        // Worldwide rank — each player fetches their OWN rank (self-centric API,
        // getGlobalLeaderboardView has no "rank for an arbitrary other player" call)
        // and shares it here, the same way colorIndex/pings already propagate.
        const auto &rankJson = jsonMember["extra"]["rank"];
        user.worldwideRank = rankJson.isNull() ? -1 : rankJson.asInt();
        // Ping data shared via the member's extra field
        const auto &pingsJson = jsonMember["extra"]["pings"];
        if (pingsJson.isObject())
            for (const auto &r : pingsJson.getMemberNames())
                user.pings[r] = pingsJson[r].asInt();
        lobby.members.push_back(user);
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
    // state with it. This fires on every lobby-update event (member join/leave,
    // ready-state changes, etc.), not just the first one — parseLobby() returns a
    // fresh Lobby each time, so chatMessages/arrivalTime must be explicitly carried
    // forward or every routine update would silently wipe the chat history.
    if (jsonData["lobby"].isObject())
    {
        auto savedChatMessages = state.lobby.chatMessages;
        auto savedArrivalTime = state.lobby.arrivalTime;
        state.lobby = parseLobby(jsonData["lobby"], jsonData["lobbyId"].asString());
        state.lobby.chatMessages = savedChatMessages;
        state.lobby.arrivalTime = savedArrivalTime;

        // If we were joining lobby, show the lobby screen. We have the information to
        // display now.
        if (state.screenState == ScreenState::JoiningLobby)
        {
            state.screenState = ScreenState::Lobby;
            state.geoTestLobbyArrivalTime = std::chrono::steady_clock::now();
            state.lobby.arrivalTime = std::chrono::steady_clock::now(); // true first-arrival timestamp, for the INFO tab

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
            // Disbanded for any other reason than ROOM_READY, means we failed to launch the game.
            app_closeGame();
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

        // Stay on whatever screen we're already on (Lobby, normally) — chat and the rest
        // of the lobby UI keep working through the whole provisioning sequence instead of
        // being replaced by a blocking loading/cancel screen. isProvisioning just drives a
        // small inline status line (see lobby.cpp); the actual screen change to Game only
        // happens once relay truly connects (RelayConnectCallback::relayConnectSuccess).
        state.isProvisioning = true;
        state.provisioningStatus = "Provisioning server...";
    }
    else if (operation == "ROOM_PROGRESS")
    {
        int curStep = jsonData["curStep"].asInt();
        int ofStep = jsonData["ofStep"].asInt();
        const auto &msg = jsonData["msg"].asString();
        char buf[128];
        snprintf(buf, sizeof(buf), "%d/%d: %s", curStep, ofStep, msg.c_str());
        state.provisioningStatus = buf;
    }
    else if (operation == "ROOM_ASSIGNED")
    {
        state.provisioningStatus = "Server assigned...";
    }
    else if (operation == "ROOM_READY")
    {
        state.provisioningStatus = "Connecting...";
        state.server = parseServer(jsonData);

        // Record which region was actually launched for the geo test.
        // EdgeGap: region was chosen client-side (s_geoTestRegion).
        // V2/GameLift/others: extract the region prefix from the lobbyId.
        {
            std::string region = s_geoTestRegion.empty()
                                 ? regionFromLobbyId(state.server.lobbyId)
                                 : s_geoTestRegion;
            if (!region.empty())
            {
                const auto &tested = state.geoTestedRegions;
                if (std::find(tested.begin(), tested.end(), region) == tested.end())
                {
                    state.geoTestedRegions.push_back(region);
                    printf("[GeoTest] Recorded region: %s (total: %d)\n",
                           region.c_str(), (int)state.geoTestedRegions.size());
                }
            }
            s_geoTestRegion.clear();
        }

        startGame();
    }
    else if (operation == "SIGNAL")
    {
        // This-lobby chat, per the user's direction: implemented via SendSignal
        // (Lobby service), not the Chat service — rides the RTT connection the
        // lobby already has, no separate channel/registration needed.
        //
        // Real wire shape, confirmed from a live capture (the docs describe this
        // as "LOBBY_SIGNAL_DATA" in prose, but the actual RTT operation is
        // "SIGNAL"): data: { lobbyId, from: {id,name,pic,cxId}, signalData: <our
        // own payload> }. "from" is the server's authoritative sender info — more
        // reliable than trusting whatever our own signalData payload claims.
        const auto &fromCxId = jsonData["from"]["cxId"].asString();
        std::string fromName = jsonData["from"]["name"].asString();
        std::string text = jsonData["signalData"]["text"].asString();

        // Skip echoes of our own signal — app_sendLobbySignal already appended it
        // locally on send. Compared by cxId (not name) since two players could
        // share a display name.
        if (!text.empty() && fromCxId != state.user.cxId)
        {
            ChatMessage msg;
            msg.fromName = fromName.empty() ? "Player" : fromName;
            msg.text = text;
            state.lobby.chatMessages.push_back(msg);
        }
    }
}

// Sends a chat message to everyone currently in this lobby, via the Lobby
// service's SendSignal (not the Chat service — see the LOBBY_SIGNAL_DATA handler
// in onLobbyEvent for why). Appends locally right away — the receive handler
// skips the echo of our own signal, which the server does send back to us too.
void app_sendLobbySignal(const std::string &text)
{
    if (text.empty() || state.lobby.lobbyId.empty()) return;

    // No need to embed our own name — the server wraps every signal with
    // authoritative sender info (data.from.name/cxId) that the receive handler
    // uses instead.
    Json::Value signal;
    signal["text"] = text;
    Json::FastWriter writer;

    pBCWrapper->getLobbyService()->sendSignal(state.lobby.lobbyId, writer.write(signal), nullptr);

    ChatMessage msg;
    msg.fromName = state.user.name;
    msg.text = text;
    state.lobby.chatMessages.push_back(msg);
}

// Connect to the Relay server and start the game
static void startGame()
{
    // No screenState change here — we're already sitting on Lobby (or wherever the STARTING
    // event's isProvisioning banner started rendering) the whole way through to Game.
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
    s_rttConnecting = false; // callbacks just deregistered — nothing will clear this otherwise

    // Reset state but keep user, app config, and geo test results
    User user = state.user;
    auto appLobbies = state.appLobbies;
    int splotchDurationSec = state.splotchDurationSec;
    auto pingData = settings.autoGeoTest ? std::map<std::string,int>{} : state.pingData;
    auto geoTestedRegions = settings.autoGeoTest ? std::vector<std::string>{} : state.geoTestedRegions;
    auto geoTestResults = settings.autoGeoTest ? std::map<std::string,int>{} : state.geoTestResults;
    state = State();
    state.user = user;
    state.user.isAlive = false;
    state.user.isReady = false;
    state.appLobbies = appLobbies;
    state.splotchDurationSec = splotchDurationSec;
    state.pingData = pingData;
    state.geoTestedRegions = geoTestedRegions;
    state.geoTestResults = geoTestResults;
    state.screenState = ScreenState::MainMenu;
    app_enableChatRTT(); // RTT was just disabled above — re-enable it for main-menu chat
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
    s_rttConnecting = false; // callbacks just deregistered — nothing will clear this otherwise

    // Reset state but keep user, app config, and geo test results
    User user = state.user;
    auto appLobbies = state.appLobbies;
    int splotchDurationSec = state.splotchDurationSec;
    auto pingData = settings.autoGeoTest ? std::map<std::string,int>{} : state.pingData;
    auto geoTestedRegions = settings.autoGeoTest ? std::vector<std::string>{} : state.geoTestedRegions;
    auto geoTestResults = settings.autoGeoTest ? std::map<std::string,int>{} : state.geoTestResults;
    state = State();
    state.user = user;
    state.user.isAlive = false;
    state.user.isReady = false;
    state.appLobbies = appLobbies;
    state.splotchDurationSec = splotchDurationSec;
    state.pingData = pingData;
    state.geoTestedRegions = geoTestedRegions;
    state.geoTestResults = geoTestResults;
    state.screenState = ScreenState::MainMenu;
    app_enableChatRTT(); // RTT was just disabled above — re-enable it for main-menu chat
}

// Ready up and signals RTT service we can start the game. Stays on whatever screen the
// caller is already on (Lobby) — the STARTING lobby event that follows drives the
// non-blocking provisioning banner, not a screen change (see onLobbyEvent).
void app_startGame()
{
    state.user.isReady = true;
    state.awaitingRematch = false; // in case this was called by the rematch gate below
    pBCWrapper->getLobbyService()->updateReady(
        state.lobby.lobbyId,
        state.user.isReady,
        buildExtraJson());
}

// Marks this player as queued for a rematch AND takes them back to the Lobby screen —
// called both from the Match Summary screen's "Queue for Rematch" button and from its own
// per-player 15s auto-timeout (matchSummary_update()), so either path looks identical from
// here on: the player sits in the Lobby (chatting, etc.) waiting for app_tickRematchGate()
// below to actually start the next round.
void app_setRematchReady(bool ready)
{
    state.user.isReady = ready;
    if (ready)
        state.screenState = ScreenState::Lobby;
    pBCWrapper->getLobbyService()->updateReady(
        state.lobby.lobbyId, ready, buildExtraJson());
}

// Host-only gate on starting the next round: waits until every current lobby member has
// queued for a rematch (each auto-queues themselves within MATCH_SUMMARY_REMATCH_MS at the
// latest — see matchSummary.cpp — so this is mostly a safety net against clock skew between
// clients) OR that same deadline elapses regardless, whichever comes first. Once satisfied,
// calls the exact app_startGame() that already starts every round — no separate "begin
// round 2" mechanism needed. Non-host clients just display the shared countdown/count and
// wait for the resulting STARTING lobby event like they already do for the very first
// round. isHost is re-evaluated every call, so a host migration while some players are
// still on the Match Summary screen is picked up for free. Called once per frame from both
// lobby_update() and matchSummary_update() — whichever screen the host itself happens to be
// on, this still needs to keep evaluating for the other players who haven't returned yet.
void app_tickRematchGate()
{
    if (!state.awaitingRematch) return;

    bool isHost = !state.lobby.ownerCxId.empty() && state.user.cxId == state.lobby.ownerCxId;
    if (!isHost) return;

    bool allReady = !state.lobby.members.empty();
    for (const auto &m : state.lobby.members)
    {
        if (!m.isReady) { allReady = false; break; }
    }

    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - state.matchSummaryArrivalTime).count();

    if (allReady || elapsedMs >= MATCH_SUMMARY_REMATCH_MS)
        app_startGame();
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

// Mask of "everyone except me" — used to broadcast paint/results without echoing back
// to the sender. This used to be driven by a user-editable "allowSendTo" HUD checkbox
// (BCLOUD-14490 removes that checkbox, which was also a scoring-integrity hole: any
// client could uncheck a peer and desync that peer's canvas, and therefore their score).
// The self-exclusion itself is unconditional now, not gated by an editable flag.
static uint64_t getPlayerMask()
{
    uint64_t playerMask = 0;

    for (const auto &user : state.lobby.members)
    {
        if (user.cxId == state.user.cxId)
            continue;
        auto netId = pBCWrapper->getRelayService()->getNetIdForCxId(user.cxId);
        if (netId < 0 || netId >= MAX_LOBBY_MEMBERS)
            continue;
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
    json["data"]["x"] = pos.x / CANVAS_W;
    json["data"]["y"] = pos.y / CANVAS_H;

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
    // Random rotation angle — sent with the message so every client renders the same orientation
    float angle = ((float)rand() / (float)RAND_MAX) * SPLOTCH_TAU;

    Json::Value json;
    json["op"] = "shockwave";
    json["data"]["x"]     = pos.x / CANVAS_W;
    json["data"]["y"]     = pos.y / CANVAS_H;
    json["data"]["angle"] = angle;

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
    shockwave.pos        = pos;
    shockwave.colorIndex = state.user.colorIndex;
    shockwave.startTime  = std::chrono::high_resolution_clock::now();
    state.shockwaves.push_back(shockwave);

    // Leave a persistent splotch at the same location
    Splotch splotch;
    splotch.pos        = pos;
    splotch.colorIndex = state.user.colorIndex;
    splotch.startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    splotch.rotation   = angle;
    splotch.ownerCxId  = state.user.cxId;
    state.splotches.push_back(splotch);
    ++state.splotchGeneration;
}
