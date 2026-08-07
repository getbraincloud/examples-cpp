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
// File: mainMenu.cpp
// Desc: Definition for displaying a main menu screen and updating its logic
// Author: David St-Louis
//-----------------------------------------------------------------------------

// App includes
#include "app.h"
#include "globals.h"
#include "globalChat.h"
#include "leaderboardPanel.h"
#include "BCCallback.h"

// C/C++ includes
#include <algorithm>
#include <imgui.h>
#include <stdio.h>

#include "mainMenu.h"

// Main menu card widths (height auto-sizes to content)
#define LOBBY_CARD_WIDTH 400.0f
#define LEADERBOARD_CARD_WIDTH 380.0f
#define CARD_GAP 32.0f
#define RIGHT_TAB_HEIGHT 44.0f
#define CHAT_CARD_HEIGHT 460.0f

// 0 = Leaderboard tab active, 1 = Chat tab active.
static int s_activeRightTab = 0;

// Small floating tab strip sitting above the LEADERBOARD/CHAT panel — matches the
// reference mockup's outlined-toggle look, active tab highlighted.
static void drawRightTabs(float panelX, float panelY)
{
    ImGui::SetNextWindowPos(ImVec2(panelX, panelY - RIGHT_TAB_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##right_tabs", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);

    bool lbActive = (s_activeRightTab == 0);
    if (lbActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.45f, 0.90f, 1.0f));
    if (ImGui::Button("LEADERBOARD", ImVec2(150.0f, 32.0f))) s_activeRightTab = 0;
    if (lbActive) ImGui::PopStyleColor();

    ImGui::SameLine();
    bool chatActive = (s_activeRightTab == 1);
    if (chatActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.45f, 0.90f, 1.0f));
    if (ImGui::Button("CHAT", ImVec2(100.0f, 32.0f))) s_activeRightTab = 1;
    if (chatActive) ImGui::PopStyleColor();

    ImGui::End();
}

//-----------------------------------------------------------------------------
// Lobby card — protocol/lobby-type/ping-data setup, unchanged functionality,
// relabeled/reordered to match the reference layout (title + win-condition
// tagline up top, captioned dropdowns, "Find / Create Lobby" as the primary CTA).
//-----------------------------------------------------------------------------

static void drawLobbyCard(float x, float y)
{
    // Fixed size matching the right-side panel (CHAT_CARD_HEIGHT), so the two cards
    // are always the same height. Content lives in a scrollable child so the
    // variable-height geo-test panel can't grow the outer window — it just scrolls,
    // and shorter content leaves the padded gap below it that a fixed-size window
    // naturally gives for free.
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(LOBBY_CARD_WIDTH, CHAT_CARD_HEIGHT), ImGuiCond_Always);
    ImGui::Begin("Cursor Party", nullptr,
                 ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize);
    ImGui::BeginChild("lobby_card_scroll", ImVec2(0.0f, 0.0f), false);

    // Win-condition tagline (BCLOUD-14472) — sets expectations before Play is clicked.
    {
        const char *tagline = "Paint more of the board than anyone else -- cover it, and you win the party.";
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + LOBBY_CARD_WIDTH - 20.0f);
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.55f, 1.0f), "%s", tagline);
        ImGui::PopTextWrapPos();
    }
    ImGui::Separator();

    // Lobby type — captioned like the reference (label above, not beside, the combo).
    ImGui::TextDisabled("LOBBY TYPE");
    if (ImGui::BeginCombo("##LobbyType", settings.lobbyType.c_str()))
    {
        for (const auto &lobbyType : state.appLobbies)
        {
            bool selected = (lobbyType == settings.lobbyType);
            if (ImGui::Selectable(lobbyType.c_str(), selected))
            {
                settings.lobbyType = lobbyType;
                if (settings.lobbyType.find("Team") == 0)
                {
                    if (settings.teamCode == "all")
                        settings.teamCode = "alpha";
                }
                else
                {
                    settings.teamCode = "all";
                }
                saveConfigs();
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Team selection (only for Team lobby types)
    if (settings.lobbyType.find("Team") == 0)
    {
        ImGui::TextDisabled("TEAM");
        int teamChoice = (settings.teamCode == "beta") ? 1 : 0;
        if (ImGui::Combo("##Team", &teamChoice, "Alpha\0Beta\0\0"))
        {
            settings.teamCode = (teamChoice == 0) ? "alpha" : "beta";
            saveConfigs();
        }
    }

    // Network protocol
    ImGui::TextDisabled("NETWORK PROTOCOL");
    if (ImGui::Combo("##Protocol", (int *)&settings.protocol, "UDP\0TCP\0WS\0WSS\0"))
    {
        saveConfigs();
    }

    // Use ping region data toggle
    if (ImGui::Checkbox("With Ping Region Data", &settings.usePingData))
    {
        saveConfigs();
    }

    // Auto geo test: EdgeGap/GameLift cycle through all regions (client-side routing);
    // V2/others connect once and record whichever region the server chose.
    if (ImGui::Checkbox("Auto Geo Test", &settings.autoGeoTest))
        saveConfigs();
    if (settings.autoGeoTest)
    {
        ImGui::SameLine();
        if (isRegionalCyclingLobby(settings.lobbyType))
            ImGui::TextDisabled("(cycles all regions)");
        else
            ImGui::TextDisabled("(records server-chosen region)");
    }

    // Stop condition differs by type:
    //   EdgeGap/GameLift — every region that has a defined specific lobby type has been tested
    //   V2/others        — at least one region confirmed (server always picks the same fastest)
    bool geoTestComplete = false;
    if (settings.autoGeoTest && !state.pingData.empty())
    {
        if (isRegionalCyclingLobby(settings.lobbyType))
        {
            int mappable = 0;
            for (const auto &kv : state.pingData)
                if (!regionToSpecificLobbyType(settings.lobbyType, kv.first).empty())
                    ++mappable;
            geoTestComplete = mappable > 0 && (int)state.geoTestedRegions.size() >= mappable;
        }
        else
        {
            geoTestComplete = !state.geoTestedRegions.empty();
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Not sure what any of this means? Just tap below.");

    // Join a game — primary call to action
    bool autoPlay = settings.autoJoin || (settings.autoGeoTest && !geoTestComplete);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.40f, 0.45f, 0.95f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.48f, 0.53f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.40f, 0.85f, 1.0f));
    bool clicked = ImGui::Button("Find / Create Lobby", ImVec2(LOBBY_CARD_WIDTH - 20.0f, 40.0f));
    ImGui::PopStyleColor(3);
    if (clicked || autoPlay)
    {
        app_play(settings.protocol);
    }

    if (geoTestComplete)
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Geo test complete!");

    // ---- Geo Region Test Panel (visible once ping data is available) ----------
    if (!state.pingData.empty())
    {
        ImGui::Separator();
        if (isRegionalCyclingLobby(settings.lobbyType))
            ImGui::TextDisabled("Geo Region Test  (cycles all regions)");
        else
            ImGui::TextDisabled("Geo Region Test  (records server-chosen region)");

        // Sort all known regions by ping
        std::vector<std::pair<int, std::string>> sorted;
        for (const auto &kv : state.pingData)
            sorted.push_back({kv.second, kv.first});
        std::sort(sorted.begin(), sorted.end());

        const auto &tested = state.geoTestedRegions;

        if (isRegionalCyclingLobby(settings.lobbyType))
        {
            // Regional cycling: only show regions that have a defined specific lobby type
            std::vector<std::pair<int, std::string>> mapped;
            for (const auto &p : sorted)
                if (!regionToSpecificLobbyType(settings.lobbyType, p.second).empty())
                    mapped.push_back(p);

            bool allTested = !mapped.empty();
            for (const auto &p : mapped)
                if (std::find(tested.begin(), tested.end(), p.second) == tested.end())
                {
                    allTested = false;
                    break;
                }
            if (allTested && !tested.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "All lobbies tested — will wrap around");

            std::string nextRegion;
            for (const auto &p : mapped)
                if (std::find(tested.begin(), tested.end(), p.second) == tested.end())
                {
                    nextRegion = p.second;
                    break;
                }

            for (const auto &p : mapped)
            {
                bool wasTested = std::find(tested.begin(), tested.end(), p.second) != tested.end();
                if (wasTested)
                {
                    auto resIt = state.geoTestResults.find(p.second);
                    if (resIt != state.geoTestResults.end() && resIt->second > 0)
                    {
                        int relayMs = resIt->second;
                        bool pass = relayMs <= p.first + 100;
                        if (pass)
                            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                                "[done] %s  beacon:%dms  relay:%dms  PASS", p.second.c_str(), p.first, relayMs);
                        else
                            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                "[done] %s  beacon:%dms  relay:%dms  FAIL", p.second.c_str(), p.first, relayMs);
                    }
                    else
                        ImGui::TextDisabled("[done] %s  (%dms)", p.second.c_str(), p.first);
                }
                else if (p.second == nextRegion)
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), ">>> %s  (%dms)", p.second.c_str(), p.first);
                else
                    ImGui::Text("[    ] %s  (%dms)", p.second.c_str(), p.first);
            }
        }
        else
        {
            // V2 / specific regional: show ping table; highlight which region the server confirmed
            if (tested.empty())
                ImGui::TextDisabled("Run test to confirm server-chosen region");
            for (const auto &p : sorted)
            {
                bool confirmed = std::find(tested.begin(), tested.end(), p.second) != tested.end();
                if (confirmed)
                {
                    auto resIt = state.geoTestResults.find(p.second);
                    if (resIt != state.geoTestResults.end() && resIt->second > 0)
                    {
                        int relayMs = resIt->second;
                        bool pass = relayMs <= p.first + 100;
                        if (pass)
                            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                                "[confirmed] %s  beacon:%dms  relay:%dms  PASS", p.second.c_str(), p.first, relayMs);
                        else
                            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                "[confirmed] %s  beacon:%dms  relay:%dms  FAIL", p.second.c_str(), p.first, relayMs);
                    }
                    else
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[confirmed] %s  (%dms)", p.second.c_str(), p.first);
                }
                else
                    ImGui::TextDisabled("[  ?  ] %s  (%dms)", p.second.c_str(), p.first);
            }
        }

        if (!tested.empty())
        {
            if (ImGui::Button("Reset Geo Test"))
            {
                state.geoTestedRegions.clear();
                state.geoTestResults.clear();
            }
        }
    }
    // -------------------------------------------------------------------------

    ImGui::EndChild();
    ImGui::End();
}

// Draws the main menu screen: the lobby-setup card and, beside it, the leaderboard
// viewer for the boards this app posts to (BCLOUD-14472).
void mainMenu_update()
{
    float totalWidth = LOBBY_CARD_WIDTH + CARD_GAP + LEADERBOARD_CARD_WIDTH;
    float startX = (float)width / 2.0f - totalWidth / 2.0f;
    float y = (float)height / 2.0f - 100.0f; // anchor ~100px above center; cards grow down
    float rightX = startX + LOBBY_CARD_WIDTH + CARD_GAP;

    drawLobbyCard(startX, y);
    drawRightTabs(rightX, y);
    if (s_activeRightTab == 0)
        drawLeaderboardPanel("##leaderboard_mainmenu", rightX, y, LEADERBOARD_CARD_WIDTH, CHAT_CARD_HEIGHT);
    else
        drawGlobalChatPanel("##global_chat_mainmenu", rightX, y, LEADERBOARD_CARD_WIDTH, CHAT_CARD_HEIGHT);
}
