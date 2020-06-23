//-----------------------------------------------------------------------------
// Copyright 2018 bitHeads inc.
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
#include <braincloud/IRTTCallback.h>
#include <braincloud/IRelayConnectCallback.h>
#include <braincloud/IRelayCallback.h>
#include <braincloud/IRelaySystemCallback.h>
#include <braincloud/reason_codes.h>
#include <imgui.h>

// C/C++ includes
#include <iostream>
#include <stdlib.h>

// Prototypes for private functions
static void initBC();
static void handlePlayerState(const Json::Value& result);
static void onLoggedIn();
static void onRTTConnected();
static void dieWithMessage(const std::string& message);
static void uninitBC();
static void resetState();
static void submitName(const char* username);
static void onLobbyEvent(const Json::Value& eventJson);
static Lobby parseLobby(const Json::Value& lobbyJson, const std::string& lobbyId);
static Server parseServer(const Json::Value& serverJson);
static void startGame();
static void onRelaySystemMessage(const Json::Value& json);
static void onRelayMessage(int netId, const Json::Value& json);

// brainCloud RTT Connection callbacks
class RTTConnectCallback final : public BrainCloud::IRTTConnectCallback
{
public:
    void rttConnectSuccess() override
    {
        onRTTConnected();
    }

    void rttConnectFailure(const std::string& errorMessage) override
    {
        dieWithMessage("Disconnected from RTT:\n" + errorMessage);
    }
};

// brainCloud RTT callbacks
class RTTCallback final : public BrainCloud::IRTTCallback
{
public:
    void rttCallback(const std::string& dataJson) override
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
    void relayConnectSuccess(const std::string& jsonResponse) override
    {
        state.screenState = ScreenState::Game;
    }

    void relayConnectFailure(const std::string& errorMessage) override
    {
        if (!ignoreFailure)
        {
            dieWithMessage("Failed to connect to server, msg: " + errorMessage);
        }
    }

    bool ignoreFailure = false;
};

// brainCloud Relay callbacks
class RelayCallback final : public BrainCloud::IRelayCallback
{
public:
    void relayCallback(int netId, const uint8_t* bytes, int size) override
    {
        Json::Value json;
        Json::Reader reader;
        std::string str((const char*)bytes, size);
        reader.parse(str, json);
        //std::cout << "App Relay: " << str << std::endl;
        onRelayMessage(netId, json);
    }
};

// brainCloud Relay System callbacks
class RelaySystemCallback final : public BrainCloud::IRelaySystemCallback
{
public:
    void relaySystemCallback(const std::string& jsonResponse) override
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
static BrainCloud::BrainCloudWrapper *pBCWrapper = nullptr;
static std::string errorMessage;
static bool dead = false;
static bool isDisconnecting = false;

static RTTConnectCallback bcRTTConnectCallback;
static RTTCallback bcRTTCallback;
static RelayConnectCallback bcRelayConnectCallback;
static RelayCallback bcRelayCallback;
static RelaySystemCallback bcRelaySystemCallback;

// Initialize brainCloud
static void initBC()
{
    if (!pBCWrapper)
    {
        pBCWrapper = new BrainCloud::BrainCloudWrapper("RelayTestApp");
    }
    dead = false;
    pBCWrapper->initialize(BRAINCLOUD_SERVER_URL, 
                           BRAINCLOUD_APP_SECRET, 
                           BRAINCLOUD_APP_ID, 
                           VERSION, 
                           "bitHeads inc.", 
                           "RelayTestApp");
    pBCWrapper->getBCClient()->enableLogging(true);
}

// User authenticated, handle the result
static void handlePlayerState(const Json::Value& result)
{
    state.user = User();
    state.user.id = result["data"]["profileId"].asString();

    // If no username is set for this user, ask for it
    const auto& userName = result["data"]["playerName"].asString();
    if (userName.empty())
    {
        submitName(settings.username);
    }
    else
    {
        state.user.name = userName;
        onLoggedIn();
    }
}

// User fully logged in. Enable RTT and listen for chat messages
void onLoggedIn()
{
    // Go to main menu screen
    state.screenState = ScreenState::MainMenu;
}

// RTT connected. Go to main menu screen
void onRTTConnected()
{
    // Find lobby
    pBCWrapper->getLobbyService()->findOrCreateLobby(
        "CursorPartyV2",// lobby type
        0,              // rating
        1,              // max steps
        "{\"strategy\":\"ranged-absolute\",\"alignment\":\"center\",\"ranges\":[1000]}", // algorithm
        "{}",           // filters
        {},             // other users
        "{}",           // settings
        false,          // ready
        "{\"colorIndex\":" + std::to_string(state.user.colorIndex) + "}", // extra
        "all",          // team code
        new BCCallback( // callback
        [](const Json::Value& result) // Success
        {
            // Success of lobby found will be in the event onLobbyEvent
        },
        [](const std::string& status_message) // Error
        {
            dieWithMessage("Failed to find lobby:\n" + status_message);
        })
    );
}

// Go back to login screen, with an error message
void dieWithMessage(const std::string& message)
{
    isDisconnecting = true;

    pBCWrapper->getRelayService()->deregisterRelayCallback();
    pBCWrapper->getRelayService()->deregisterSystemCallback();
    pBCWrapper->getRelayService()->disconnect();
    pBCWrapper->getRTTService()->deregisterAllRTTCallbacks();
    pBCWrapper->getRTTService()->disableRTT();

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

static void onRelaySystemMessage(const Json::Value& json)
{
    if (json["op"].asString() == "DISCONNECT") // A member has disconnected from the game
    {
        const auto& profileId = json["profileId"].asString();
        for (auto& member : state.lobby.members)
        {
            if (member.id == profileId)
            {
                member.isAlive = false; // This will stop displaying this member
                break;
            }
        }
    }
}

static void onRelayMessage(int netId, const Json::Value& json)
{
    const auto& memberProfileId = pBCWrapper->getRelayService()->getProfileIdForNetId(netId);
    for (auto& member : state.lobby.members)
    {
        if (member.id == memberProfileId)
        {
            auto op = json["op"].asString();
            if (op == "move")
            {
                member.isAlive = true;
                member.pos.x = json["data"]["x"].asInt();
                member.pos.y = json["data"]["y"].asInt();
            }
            else if (op == "shockwave")
            {
                Shockwave shockwave;
                shockwave.pos.x = json["data"]["x"].asInt();
                shockwave.pos.y = json["data"]["y"].asInt();
                shockwave.colorIndex = member.colorIndex;
                shockwave.startTime = std::chrono::high_resolution_clock::now();
                state.shockwaves.push_back(shockwave);
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
    if (pBCWrapper)
    {
        pBCWrapper->runCallbacks();
    }
    if (dead)
    {
        dead = false;
        uninitBC(); // We differ destroying BC because we cannot destroy it within a callback (yet)
        BCCallback::destroyAll();
    }

    // Add a menu at the top with an exit option to cleanly quit the app,
    // so we can test for exit crashes at any point
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("App"))
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
                if (ImGui::MenuItem("Leave"))
                {
                    app_closeGame();
                }
                ImGui::EndMenu();
            }
        }
    }
    ImGui::EndMainMenuBar();

    // Display the proper screen
    switch (state.screenState)
    {
        case ScreenState::Login:
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
    uninitBC();
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
//#if defined(RELAYTESTAPP_UWP)
//    Windows::ApplicationModel::Core::CoreApplication::Exit();
//#else
//    exit(0);
//#endif
}

// Attempt login with the specific username/password
void app_login(const char* username, const char* password)
{
    initBC();

    // Show loading screen
    loading_text = "Logging in ...";
    state.screenState = ScreenState::LoggingIn;

    // Authenticate with brainCloud
    pBCWrapper->authenticateUniversal(
        username,
        password,
        true, // Create if user doesn't exist
        new BCCallback(
        [](const Json::Value& result) // Success
        {
            handlePlayerState(result);
        },
        [](const std::string& status_message) // Error
        {
            dieWithMessage("Login Failed:\n" + status_message);
        })
    );
}

// Submit user name to brainCloud to be assosiated with the current user
static void submitName(const char* username)
{
    state.user.name = username;

    // Update name
    pBCWrapper->getPlayerStateService()->updateName(
        state.user.name.c_str(),
        new BCCallback(
        [](const Json::Value& result) // Success
        {
            onLoggedIn();
        },
        [](const std::string& status_message) // Error
        {
            dieWithMessage("Failed to update username to brainCloud:\n" +
                status_message);
        })
    );
}

// Start finding a lobby
void app_play(BrainCloud::eRelayConnectionType in_protocol)
{
    settings.protocol = in_protocol;
    isDisconnecting = false;
    state.user.colorIndex = settings.colorIndex;

    // Show loading screen
    loading_text = "Joining lobby ...";
    state.screenState = ScreenState::JoiningLobby;

    // Enable RTT
    pBCWrapper->getRTTService()->registerRTTLobbyCallback(&bcRTTCallback);
    pBCWrapper->getRTTService()->enableRTT(&bcRTTConnectCallback, true);
}

// Take in lobby json and id and build a lobby object
static Lobby parseLobby(const Json::Value& lobbyJson, const std::string& lobbyId)
{
    Lobby lobby;

    lobby.lobbyId = lobbyId;
    lobby.ownerId = lobbyJson["owner"].asString();
    const auto& jsonMembers = lobbyJson["members"];
    for (const auto& jsonMember : jsonMembers)
    {
        User user;
        user.id = jsonMember["profileId"].asString();
        user.name = jsonMember["name"].asString();
        user.colorIndex = jsonMember["extra"]["colorIndex"].asInt();
        if (user.id == state.user.id) user.allowSendTo = false;
        lobby.members.push_back(user);
    }

    return lobby;
}

// Take in server json and build a server object
static Server parseServer(const Json::Value& serverJson)
{
    Server server;

    server.host = serverJson["connectData"]["address"].asString();
    server.wsPort = serverJson["connectData"]["ports"]["ws"].asInt();
    server.tcpPort = serverJson["connectData"]["ports"]["tcp"].asInt();
    server.udpPort = serverJson["connectData"]["ports"]["udp"].asInt();
    server.passcode = serverJson["passcode"].asString();
    server.lobbyId = serverJson["lobbyId"].asString();

    return server;
}

// We received a lobby event through RTT
static void onLobbyEvent(const Json::Value& eventJson)
{
    const auto& jsonData = eventJson["data"];

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
        }
    }

    auto operation = eventJson["operation"].asString();

    if (operation == "DISBANDED")
    {
        if (jsonData["reason"]["code"].asInt() == RTT_ROOM_READY)
        {
            startGame();
        }
        else
        {
            // Disbanded for any other reason than ROOM_READY, means we failed to launch the game.
            app_closeGame();
        }
    }
    else if (operation == "STARTING")
    {
        // Save our picked color index
        settings.colorIndex = state.user.colorIndex;
        saveConfigs();

        // Go to loading screen
        state.screenState = ScreenState::Starting;
        loading_text = "Starting...";
    }
    else if (operation == "ROOM_READY")
    {
        state.server = parseServer(jsonData);
    }
}

// Connect to the Relay server and start the game
static void startGame()
{
    //pBCWrapper->getRTTService()->deregisterAllRTTCallbacks();
    //pBCWrapper->getRTTService()->disableRTT();

    state.screenState = ScreenState::Starting;

    pBCWrapper->getRelayService()->registerRelayCallback(&bcRelayCallback);
    pBCWrapper->getRelayService()->registerSystemCallback(&bcRelaySystemCallback);

    int port = 0;
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
    }

    pBCWrapper->getRelayService()->connect(settings.protocol, state.server.host, port, state.server.passcode, state.server.lobbyId, &bcRelayConnectCallback);
}

// Cleanly close the game. Go back to main menu but don't log 
void app_closeGame()
{
    pBCWrapper->getRelayService()->deregisterRelayCallback();
    pBCWrapper->getRelayService()->deregisterSystemCallback();
    pBCWrapper->getRelayService()->disconnect();
    pBCWrapper->getRTTService()->deregisterAllRTTCallbacks();
    pBCWrapper->getRTTService()->disableRTT();

    // Reset state but keep the user around
    User user = state.user;
    state = State();
    state.user = user;
    state.user.isAlive = false;
    state.user.isReady = false;
    state.screenState = ScreenState::MainMenu;
}

// Ready up and signals RTT service we can start the game
void app_startGame()
{
    state.user.isReady = true;
    state.screenState = ScreenState::Starting;
    loading_text = "Starting...";
    pBCWrapper->getLobbyService()->updateReady(
        state.lobby.lobbyId,
        state.user.isReady,
        "{\"colorIndex\":" + std::to_string(state.user.colorIndex) + "}"
    );
}

// User changes his player color
void app_changeUserColor(int colorIndex)
{
    state.user.colorIndex = colorIndex;
    for (auto& member : state.lobby.members)
    {
        if (state.user.id == member.id)
        {
            member.colorIndex = colorIndex;
            break;
        }
    }

    pBCWrapper->getLobbyService()->updateReady(
        state.lobby.lobbyId,
        state.user.isReady,
        "{\"colorIndex\":" + std::to_string(state.user.colorIndex) + "}"
    );
}

static uint64_t getPlayerMask()
{
    uint64_t playerMask = 0;

    for (const auto& user : state.lobby.members)
    {
        if (!user.allowSendTo) continue;
        auto netId = pBCWrapper->getRelayService()->getNetIdForProfileId(user.id);
        playerMask |= (uint64_t)1 << (uint64_t)netId;
    }

    return playerMask;
}

// User moved mouse in the play area
void app_mouseMoved(const Point& pos)
{
    state.user.isAlive = true;
    state.user.pos = pos;
    for (auto& member : state.lobby.members)
    {
        if (state.user.id == member.id)
        {
            member.isAlive = true;
            member.pos = pos;
            break;
        }
    }

    // Send to other players
    Json::Value json;
    json["op"] = "move";
    json["data"]["x"] = pos.x;
    json["data"]["y"] = pos.y;

    Json::FastWriter writer;
    auto str = writer.write(json);

    pBCWrapper->getRelayService()->sendToAll(
        (const uint8_t*)str.data(), (int)str.length(), 
        settings.sendReliable, // Unreliable
        settings.sendOrdered, // Ordered
        (BrainCloud::eRelayChannel)settings.sendChannel);
}

// User clicked mouse in the play area
void app_shockwave(const Point& pos)
{
    // Send to other players
    Json::Value json;
    json["op"] = "shockwave";
    json["data"]["x"] = pos.x;
    json["data"]["y"] = pos.y;

    Json::FastWriter writer;
    auto str = writer.write(json);

    pBCWrapper->getRelayService()->sendToPlayers(
        (const uint8_t*)str.data(), (int)str.length(), 
        getPlayerMask(),
        true, // Reliable
        false, // Unordered
        (BrainCloud::eRelayChannel)settings.sendChannel);

    // Create a local shockwave so we can see it
    Shockwave shockwave;
    shockwave.pos = pos;
    shockwave.colorIndex = state.user.colorIndex;
    shockwave.startTime = std::chrono::high_resolution_clock::now();
    state.shockwaves.push_back(shockwave);
}
