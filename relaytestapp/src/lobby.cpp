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
// File: lobby.cpp
// Desc: Definition for displaying a lobby screen and updating its logic
// Author: David St-Louis
//-----------------------------------------------------------------------------

// App includes
#include "app.h"
#include "globals.h"

// C/C++ includes
#include <imgui.h>
#include <stdio.h>

// Draws a login dialog and update its logic
void lobby_update()
{
    // Lobby window: auto-sized, centered via pivot
    {
        ImGui::SetNextWindowPos(
            ImVec2((float)width / 2.0f, (float)height / 2.0f + 10.0f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("Lobby", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoTitleBar);

        // Leave lobby
        if (ImGui::Button("Leave"))
        {
            app_cancelLobby();
        }

        // We're the boss, so we can start the game
        if (state.user.cxId == state.lobby.ownerCxId)
        {
            ImGui::SameLine();
            if (ImGui::Button("Start"))
            {
                app_startGame();
            }
        }

        // Color picker: 10 per row to fit up to 40 colors in a compact grid
        for (int i = 0; i < NUM_COLORS; ++i)
        {
            if (i % 10 != 0) ImGui::SameLine();
            if (ImGui::ColorButton(("Col" + std::to_string(i)).c_str(), COLORS[i]))
            {
                app_changeUserColor(i);
            }
        }

        // Lobby info
        ImGui::Separator();
        int maxMembers = maxLobbyMembers(settings.lobbyType);
        ImGui::Text("Lobby: %s", state.lobby.lobbyId.c_str());
        ImGui::TextDisabled("Players: %d / %d",
            (int)state.lobby.members.size(), maxMembers);
        if (state.roundNumber > 0)
            ImGui::TextDisabled("Round: %d", state.roundNumber);

        // Draw users in 4 columns (scales naturally to any member count)
        ImGui::Columns(4, 0, true);
        for (const auto& member : state.lobby.members)
        {
            std::string label = member.name;
            if (member.cxId == state.lobby.ownerCxId) label += " [Host]";
            auto pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos({pos.x + 1, pos.y + 1});
            ImGui::TextColored(ImVec4(0, 0, 0, 0.75f), "%s", label.c_str());
            ImGui::SetCursorPos(pos);
            ImGui::TextColored(COLORS[member.colorIndex % NUM_COLORS], "%s", label.c_str());
            ImGui::NextColumn();
        }
        ImGui::Columns();

        ImGui::End();
    }
}
