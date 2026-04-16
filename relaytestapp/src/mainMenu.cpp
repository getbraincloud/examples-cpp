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

// C/C++ includes
#include <imgui.h>
#include <stdio.h>

#include "mainMenu.h"

// Main menu dialog width (height auto-sizes to content)
#define DIALOG_WIDTH 400.0f

// Draws a login dialog and update its logic
void mainMenu_update()
{
    // Main menu window, horizontally centered, near vertical center.
    // AlwaysAutoResize lets it grow when the geo test panel is visible.
    {
        ImGui::SetNextWindowPos(ImVec2(
                                    (float)width / 2.0f - DIALOG_WIDTH / 2.0f,
                                    (float)height / 2.0f - 100.0f), // anchor ~100px above center; window grows down
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, 0)); // 0 height = auto
        ImGui::Begin("Main Menu", nullptr,
                     ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_AlwaysAutoResize);

        // Protocol choice
        if (ImGui::Combo("Protocol", (int *)&settings.protocol, "UDP\0TCP\0WS\0WSS\0"))
        {
            saveConfigs();
        }

        // Lobby type — populated dynamically from AllLobbyTypes global property
        if (!state.appLobbies.empty())
        {
            if (ImGui::BeginCombo("Lobby Type", settings.lobbyType.c_str()))
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
        }

        // Team selection (only for Team lobby types)
        if (settings.lobbyType.find("Team") == 0)
        {
            int teamChoice = (settings.teamCode == "beta") ? 1 : 0;
            if (ImGui::Combo("Team", &teamChoice, "Alpha\0Beta\0\0"))
            {
                settings.teamCode = (teamChoice == 0) ? "alpha" : "beta";
                saveConfigs();
            }
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

        // Join a game
        bool autoPlay = settings.autoJoin || (settings.autoGeoTest && !geoTestComplete);
        if (ImGui::Button("Play") || autoPlay)
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
                        ImGui::TextDisabled("[done] %s  (%dms)", p.second.c_str(), p.first);
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
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[confirmed] %s  (%dms)", p.second.c_str(), p.first);
                    else
                        ImGui::TextDisabled("[  ?  ] %s  (%dms)", p.second.c_str(), p.first);
                }
            }

            if (!tested.empty())
            {
                if (ImGui::Button("Reset Geo Test"))
                    state.geoTestedRegions.clear();
            }
        }
        // -------------------------------------------------------------------------

        ImGui::End();
    }
}
