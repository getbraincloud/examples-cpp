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

// Login dialog dimensions
#define DIALOG_WIDTH 275.0f
#define DIALOG_HEIGHT 150.0f

// Draws a login dialog and update its logic
void lobby_update()
{
    // Main menu window, centered
    {
        ImGui::SetNextWindowPos(ImVec2(
            (float)width / 2.0f - DIALOG_WIDTH / 2.0f,
            (float)height / 2.0f - DIALOG_HEIGHT / 2.0f + 10.0f));
        ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, DIALOG_HEIGHT));
        ImGui::Begin("Lobby", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoTitleBar);
        
        // Leave game
        if (ImGui::Button("Leave"))
        {
            app_closeGame();
        }

        // We're the boss, so we can start the game
        if (state.user.id == state.lobby.ownerId)
        {
            ImGui::SameLine();
            if (ImGui::Button("Start"))
            {
                app_startGame();
            }
        }

        // Color picker buttons
        for (int i = 0; i < 8; ++i)
        {
            if (i) ImGui::SameLine();
            if (ImGui::ColorButton(("Col" + std::to_string(i)).c_str(), COLORS[i]))
            {
                app_changeUserColor(i);
            }
        }

        // Draw users
        int i = 0;
        ImGui::Columns(4, 0, true);
        for (const auto& member : state.lobby.members)
        {
            auto pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos({pos.x + 1, pos.y + 1});
            ImGui::TextColored(ImVec4(0, 0, 0, 0.75f), "%s", member.name.c_str());
            ImGui::SetCursorPos(pos);
            ImGui::TextColored(COLORS[member.colorIndex], "%s", member.name.c_str());
            ImGui::NextColumn();
            ++i;
        }
        ImGui::Columns();

        ImGui::End();
    }
}
