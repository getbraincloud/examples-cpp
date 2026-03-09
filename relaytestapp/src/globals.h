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
#include <imgui.h>

// brainCloud
#include <braincloud/RelayConnectionType.h>

// C/C++ includes
#include <chrono>
#include <string>
#include <vector>
#include <memory>

// brainCloud App settings. Create ids.h and define BRAINCLOUD_SERVER_URL, BRAINCLOUD_APP_ID and BRAINCLOUD_APP_SECRET
#include "ids.h"

// Max character count that can be used for username and password
#define MAX_CREDENTIAL_CHAR 32

// Total number of distinct player colors (supports up to this many simultaneous players)
#define NUM_COLORS 40

// Color choices for the game (0xFFRRGGBB format).
// All colors are bright/mid-tone enough to read on a dark background.
// Palette is spread across the hue wheel in four tonal rows:
//   Row 1 ( 0- 9): vivid saturated
//   Row 2 (10-19): vivid-medium / complementary hues
//   Row 3 (20-29): pastel / light
//   Row 4 (30-39): medium-depth / muted
static const ImVec4 COLORS[NUM_COLORS] = {
    // --- Row 1: vivid saturated ---
    ImGui::ColorConvertU32ToFloat4(0xFFFF3333), // vivid red
    ImGui::ColorConvertU32ToFloat4(0xFFFF8800), // vivid orange
    ImGui::ColorConvertU32ToFloat4(0xFFFFD700), // gold
    ImGui::ColorConvertU32ToFloat4(0xFF88FF00), // vivid lime
    ImGui::ColorConvertU32ToFloat4(0xFF00EE44), // vivid green
    ImGui::ColorConvertU32ToFloat4(0xFF00DDDD), // vivid cyan
    ImGui::ColorConvertU32ToFloat4(0xFF00AAFF), // vivid sky blue
    ImGui::ColorConvertU32ToFloat4(0xFF3355FF), // vivid blue
    ImGui::ColorConvertU32ToFloat4(0xFFAA00FF), // vivid purple
    ImGui::ColorConvertU32ToFloat4(0xFFFF00BB), // vivid magenta
    // --- Row 2: vivid-medium / complementary hues ---
    ImGui::ColorConvertU32ToFloat4(0xFFFF5566), // coral
    ImGui::ColorConvertU32ToFloat4(0xFFFFAA00), // amber
    ImGui::ColorConvertU32ToFloat4(0xFFAADD00), // yellow-green
    ImGui::ColorConvertU32ToFloat4(0xFF00FF88), // spring green
    ImGui::ColorConvertU32ToFloat4(0xFF00FFCC), // aqua
    ImGui::ColorConvertU32ToFloat4(0xFF0088FF), // azure
    ImGui::ColorConvertU32ToFloat4(0xFF8833FF), // violet
    ImGui::ColorConvertU32ToFloat4(0xFFFF44AA), // hot pink
    ImGui::ColorConvertU32ToFloat4(0xFF77FF33), // chartreuse
    ImGui::ColorConvertU32ToFloat4(0xFFFF6688), // rose
    // --- Row 3: pastel / light (readable on dark) ---
    ImGui::ColorConvertU32ToFloat4(0xFFFF9999), // light red
    ImGui::ColorConvertU32ToFloat4(0xFFFFCC88), // peach
    ImGui::ColorConvertU32ToFloat4(0xFFFFFF88), // pale yellow
    ImGui::ColorConvertU32ToFloat4(0xFFAAFFAA), // pale green
    ImGui::ColorConvertU32ToFloat4(0xFF88FFEE), // pale cyan
    ImGui::ColorConvertU32ToFloat4(0xFFAABBFF), // periwinkle
    ImGui::ColorConvertU32ToFloat4(0xFFDDBBFF), // lavender
    ImGui::ColorConvertU32ToFloat4(0xFFFFBBDD), // light pink
    ImGui::ColorConvertU32ToFloat4(0xFFCCFFDD), // mint
    ImGui::ColorConvertU32ToFloat4(0xFFFFEECC), // cream
    // --- Row 4: medium-depth / muted ---
    ImGui::ColorConvertU32ToFloat4(0xFFCC1133), // crimson
    ImGui::ColorConvertU32ToFloat4(0xFFCC5500), // burnt orange
    ImGui::ColorConvertU32ToFloat4(0xFF88AA00), // olive
    ImGui::ColorConvertU32ToFloat4(0xFF228855), // forest green
    ImGui::ColorConvertU32ToFloat4(0xFF009999), // deep teal
    ImGui::ColorConvertU32ToFloat4(0xFF3366AA), // steel blue
    ImGui::ColorConvertU32ToFloat4(0xFF7744CC), // medium purple
    ImGui::ColorConvertU32ToFloat4(0xFFAA3366), // dark rose
    ImGui::ColorConvertU32ToFloat4(0xFFAA6633), // brown
    ImGui::ColorConvertU32ToFloat4(0xFF7788AA), // slate
};

// Screen state enum.
enum class ScreenState : int
{
    Login, /* Login screen */
    LoggingIn,
    MainMenu, /* Main menu */
    JoiningLobby,
    Lobby, /* Lobby screen */
    Starting,
    Game /* Game screen */
};

// A point in 2D space
struct Point
{
    int x, y;
};

// A brainCloud user
struct User
{
    std::string cxId; /* RTT Connection Id */
    std::string name; /* User name */
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
    int gameLiftPort = -1;
    int i3dPort = -1;
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
    ScreenState screenState = ScreenState::Login; /* Current screen we are on */
    User user;                                    /* Our user */
    Lobby lobby;                                  /* Lobby with its members as received from brainCloud Lobby Service */
    Server server;                                /* Server info (IP, port, protocol, passcode) */
    std::vector<Shockwave> shockwaves;            /* Players' created shockwaves */
    int mouseX = 0;
    int mouseY = 0;
    long long gameStartTime = 0;  /* ms since epoch when current round started (0 = not in game) */
    int roundNumber = 0;          /* Increments each relay round within the same lobby session */
    bool pendingEndMatch = false; /* Deferred END_MATCH disconnect (cannot call disconnect inside relay callback) */
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
    bool multiInstance = false; /* true when launched with instance/count args */
    bool autoJoin = false;
    bool autoLogin = true;
    BrainCloud::eRelayConnectionType protocol = BrainCloud::eRelayConnectionType::UDP;
    std::string lobbyType = "CursorParty";
    std::string teamCode = "all"; /* "all" for non-team lobbies, "alpha"/"beta" for team lobbies */
};

extern Settings settings;

// Load/Save configuration file from/to disk (./configs.txt or ./configs_N.txt)
// Returns true if a per-instance config file (configs_N.txt) was loaded.
bool loadConfigs();
void saveConfigs();

// Returns max lobby member count for the given lobby type.
// CursorParty supports up to 40; all other types cap at 8.
inline int maxLobbyMembers(const std::string &lobbyType)
{
    return (lobbyType == "CursorParty") ? 40 : 8;
}

// Main application state instance
extern State state;

// Main window's dimensions
extern int width;
extern int height;

// Arrow textures
extern ImTextureID ARROWS[8];
