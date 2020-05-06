//-----------------------------------------------------------------------------
// Copyright 2020 bitHeads inc.
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
// File: game.cpp
// Desc: Definition for displaying a game screen and updating its logic
// Author: David St-Louis
//-----------------------------------------------------------------------------

// App includes
#include "app.h"
#include "globals.h"

// C/C++ includes
#include <imgui.h>
#include <stdio.h>

// Login dialog dimensions
#define DIALOG_WIDTH 900.0f
#define DIALOG_HEIGHT 700.0f

// Draws a game dialog and update its logic
void game_update()
{
    // Main menu window, centered
    {
        ImGui::SetNextWindowPos(ImVec2(
            (float)width / 2.0f - DIALOG_WIDTH / 2.0f,
            (float)height / 2.0f - DIALOG_HEIGHT / 2.0f));
        ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, DIALOG_HEIGHT));
        ImGui::Begin("Game", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize);

        // Play area
        ImGui::BeginChildFrame(1, ImVec2(800, 600));
        {
            ImVec2 framePos = ImGui::GetCursorScreenPos();
            ImDrawList* pDrawList = ImGui::GetWindowDrawList();
            static ImVec2 lastMousePos;
            ImVec2 mousePos = ImGui::GetMousePos();
            mousePos.x -= framePos.x;
            mousePos.y -= framePos.y;
            if (mousePos.x != lastMousePos.x ||
                mousePos.y != lastMousePos.y)
            {
                if (mousePos.x >= 0.0f && mousePos.x <= 800.0f &&
                    mousePos.y >= 0.0f && mousePos.y <= 600.0f)
                {
                    app_mouseMoved({ (int)mousePos.x, (int)mousePos.y });
                }
            }
            lastMousePos = mousePos;

            // Shockwaves
            auto now = std::chrono::high_resolution_clock::now();
            for (auto it = state.shockwaves.begin(); it != state.shockwaves.end();)
            {
                auto& shockwave = *it;
                auto elapsed = now - shockwave.startTime;

                // Kill it if too old
                if (elapsed >= std::chrono::seconds(1))
                {
                    it = state.shockwaves.erase(it);
                    continue;
                }

                // Calculate percent
                float percent = (float)(
                    (double)std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() /
                    (double)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::seconds(1)).count()
                );

                // East out
                percent = 1.0f - (1.0f - percent) * (1.0f - percent);

                // Display
                auto size = 64.0f * percent;
                auto color = COLORS[shockwave.colorIndex];
                color.w = 1.0f - percent;
                pDrawList->AddCircleFilled(
                    ImVec2(framePos.x + (float)shockwave.pos.x, framePos.y + (float)shockwave.pos.y),
                    size, ImColor(color), 64);

                ++it;
            }

            // Arrows
            for (const auto& member : state.lobby.members)
            {
                if (member.isAlive)
                {
                    pDrawList->AddImage(
                        ARROWS[member.colorIndex],
                        ImVec2(framePos.x + (float)member.pos.x, framePos.y + (float)member.pos.y),
                        ImVec2(framePos.x + (float)member.pos.x + 64, framePos.y + (float)member.pos.y + 64));
                }
            }
        }
        ImGui::EndChildFrame();
        ImGui::IsMouseDown(0);

        // Leave game
        if (ImGui::Button("Leave"))
        {
            app_closeGame();
        }

        ImGui::End();
    }
}
