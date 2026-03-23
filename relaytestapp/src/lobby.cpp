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

        // Color picker: 10 per row, centered in the window
        {
            float buttonSize = ImGui::GetFrameHeight();
            float spacing    = ImGui::GetStyle().ItemSpacing.x;
            float gridWidth  = 10.0f * buttonSize + 9.0f * spacing;
            float startX     = (ImGui::GetWindowSize().x - gridWidth) * 0.5f;
            for (int i = 0; i < NUM_COLORS; ++i)
            {
                if (i % 10 == 0)
                    ImGui::SetCursorPosX(startX);
                else
                    ImGui::SameLine();
                if (ImGui::ColorButton(("Col" + std::to_string(i)).c_str(), COLORS[i]))
                    app_changeUserColor(i);
            }
        }

        // Column width — recomputed only when the member list changes
        static float colWidth = 80.0f;
        static size_t lastMemberCount = 0;
        static std::string lastOwnerCxId;
        if (state.lobby.members.size() != lastMemberCount ||
            state.lobby.ownerCxId != lastOwnerCxId)
        {
            lastMemberCount = state.lobby.members.size();
            lastOwnerCxId   = state.lobby.ownerCxId;
            const float COL_PADDING = 24.0f;
            colWidth = 80.0f;
            for (const auto& member : state.lobby.members)
            {
                std::string label = member.name;
                if (member.cxId == state.lobby.ownerCxId) label += " [Host]";
                float w = ImGui::CalcTextSize(label.c_str()).x + COL_PADDING;
                if (w > colWidth) colWidth = w;
            }
        }

        // Dummy forces AlwaysAutoResize to expand the window to fit all 3 columns
        float totalColW = colWidth * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        ImGui::Dummy(ImVec2(totalColW, 0.0f));

        // Helper: center a line of text in the current window
        auto centerText = [](const std::string& s) {
            float tw = ImGui::CalcTextSize(s.c_str()).x;
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - tw) * 0.5f);
            ImGui::TextUnformatted(s.c_str());
        };
        auto centerTextDisabled = [](const std::string& s) {
            float tw = ImGui::CalcTextSize(s.c_str()).x;
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - tw) * 0.5f);
            ImGui::TextDisabled("%s", s.c_str());
        };

        // Lobby info — centered
        ImGui::Separator();
        int maxMembers = maxLobbyMembers(settings.lobbyType);
        centerText("Lobby: " + state.lobby.lobbyId);
        centerTextDisabled("Players: " + std::to_string((int)state.lobby.members.size()) +
                           " / " + std::to_string(maxMembers));
        if (state.roundNumber > 0)
            centerTextDisabled("Round: " + std::to_string(state.roundNumber));

        // Member columns — each name centered within its cell
        ImGui::Columns(3, 0, true);
        for (int i = 0; i < 3; ++i)
            ImGui::SetColumnWidth(i, colWidth);

        for (const auto& member : state.lobby.members)
        {
            std::string label = member.name;
            if (member.cxId == state.lobby.ownerCxId) label += " [Host]";
            float textW  = ImGui::CalcTextSize(label.c_str()).x;
            float indent = (colWidth - textW) * 0.5f;
            auto  pos    = ImGui::GetCursorPos();
            // Drop shadow offset by 1px
            ImGui::SetCursorPos({pos.x + indent + 1, pos.y + 1});
            ImGui::TextColored(ImVec4(0, 0, 0, 0.75f), "%s", label.c_str());
            ImGui::SetCursorPos({pos.x + indent, pos.y});
            ImGui::TextColored(COLORS[member.colorIndex % NUM_COLORS], "%s", label.c_str());
            ImGui::NextColumn();
        }
        ImGui::Columns();

        ImGui::End();
    }
}
