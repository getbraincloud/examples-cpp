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
// File: globals.h
// Desc: Defines global application state, data and constants
// Author: David St-Louis
//-----------------------------------------------------------------------------

// Imgui
#include <imgui/imgui.h>

// brainCloud
#include <braincloud/RelayConnectionType.h>

// C/C++ includes
#include <chrono>
#include <string>
#include <vector>
#include <memory>

// brainCloud App settings. Create ids.h and define BRAINCLOUD_SERVER_URL, BRAINCLOUD_APP_ID and BRAINCLOUD_APP_SECRET
#include "ids.h"

// Application's version
#define VERSION "4.11.0"

// Max character count that can be used for username and password
#define MAX_CREDENTIAL_CHAR 32

// Color choices for the game
static const ImVec4 COLORS[] = {
    ImGui::ColorConvertU32ToFloat4(0xFF000000),
    ImGui::ColorConvertU32ToFloat4(0xFF5f4155),
    ImGui::ColorConvertU32ToFloat4(0xFF646964),
    ImGui::ColorConvertU32ToFloat4(0xFF5573d7),
    ImGui::ColorConvertU32ToFloat4(0xFFd78c50),
    ImGui::ColorConvertU32ToFloat4(0xFF64b964),
    ImGui::ColorConvertU32ToFloat4(0xFF6ec8e6),
    ImGui::ColorConvertU32ToFloat4(0xFFfff5dc)
};

// Screen state enum.
enum class ScreenState : int
{
    Login,          /* Login screen */
    LoggingIn,
    MainMenu,       /* Main menu */
    JoiningLobby,
    Lobby,          /* Lobby screen */
    Starting,
    Game            /* Game screen */
};

// A point in 2D space
struct Point
{
    int x, y;
};

// A brainCloud user
struct User
{
    std::string cxId;       /* RTT Connection Id */
    std::string name;       /* User name */
    int colorIndex = 7;
    bool isReady = false;
    bool isAlive = false;
    bool allowSendTo = true;
    Point pos = {0, 0};
};

// Lobby
struct Lobby
{
    std::string lobbyId;
    std::string ownerCxId;
    std::vector<User> members;
};

// Server info
struct Server
{
    std::string host;
    int wsPort = -1;
    int tcpPort = -1;
    int udpPort = -1;
    std::string passcode;
    std::string lobbyId;
};

// Shockwaves user created with the mouse
struct Shockwave
{
    Point pos;
    int colorIndex;
    std::chrono::high_resolution_clock::time_point startTime;
};

// Main application state. This contain all of the "live" data.
struct State
{
    ScreenState screenState = ScreenState::Login;   /* Current screen we are on */
    User user;                                      /* Our user */
    Lobby lobby;                                    /* Lobby with its members as received from brainCloud Lobby Service */
    Server server;                                  /* Server info (IP, port, protocol, passcode) */
    std::vector<Shockwave> shockwaves;              /* Players' created shockwaves */
    int mouseX = 0;
    int mouseY = 0;
};

struct Settings
{
    char username[MAX_CREDENTIAL_CHAR] = {'\0'};
    char password[MAX_CREDENTIAL_CHAR] = {'\0'};
    int colorIndex = 0;
    int gameUIIScale = 2;
    int sendChannel = 0;
    bool sendReliable = false;
    bool sendOrdered = true;
    int instanceIndex = 0;
    bool autoJoin = false;
    BrainCloud::eRelayConnectionType protocol = BrainCloud::eRelayConnectionType::UDP;
    std::string lobbyType = "CursorPartyV2";
};

extern Settings settings;

// Load/Save configuration file from/to disk (./config.txt)
void loadConfigs();
void saveConfigs();

// Main application state instance
extern State state;

// Main window's dimensions
extern int width;
extern int height;

// Arrow textures
extern ImTextureID ARROWS[8];
