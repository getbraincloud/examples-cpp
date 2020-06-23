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

#define RESEND_AT_60_FPS 1

// App includes
#include "app.h"
#include "globals.h"

// C/C++ includes
#include <imgui.h>
#include <stdio.h>
#include <chrono>


// Draws a game dialog and update its logic
void game_update()
{
    const auto& style = ImGui::GetStyle();

    // Send options
    {
        ImGui::Begin("Settings", 0, 
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Player mask");
        {
            ImGui::Indent();
            ImGui::TextDisabled("Only affect shockwaves");
            for (auto& user : state.lobby.members)
            {
                auto color = COLORS[user.colorIndex];
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                if (user.id == state.user.id)
                {
                    ImGui::Checkbox((user.name + " (Echo)").c_str(), &user.allowSendTo);
                }
                else
                {
                    ImGui::Checkbox(user.name.c_str(), &user.allowSendTo);
                }
                ImGui::PopStyleColor();
            }
            ImGui::Unindent();
        }

        ImGui::Separator();
        ImGui::Text("Reliable options");
        {
            ImGui::Indent();
            ImGui::TextDisabled("Only affect position");
            ImGui::Text("Channel");
            {
                ImGui::Indent();
                ImGui::BeginGroup();
                for (int i = 0; i < 4; ++i)
                {
                    bool active = settings.sendChannel == i;
                    if (ImGui::RadioButton(std::to_string(i).c_str(), active))
                    {
                        settings.sendChannel = i;
                    }
                    if (i % 2 == 0) ImGui::SameLine();
                }
                ImGui::EndGroup();
                ImGui::Unindent();
            }
            ImGui::Checkbox("Reliable", &settings.sendReliable);
            ImGui::Checkbox("Ordered", &settings.sendOrdered);
            ImGui::Unindent();
        }

        ImGui::End();
    }

    // Main menu window, centered
    {
        float gameWidth = 800;
        float gameHeight = 600;
        float scale = 1.0f;
        if (settings.gameUIIScale == 0)
        {
            scale = 0.25f;
        }
        if (settings.gameUIIScale == 1)
        {
            scale = 0.5f;
        }
        gameWidth *= scale;
        gameHeight *= scale;
        ImGui::SetNextWindowPos(ImVec2(
            (float)width / 5.0f * 3.0f - gameWidth / 2.0f,
            (float)height / 2.0f - gameHeight / 2.0f));
        ImGui::Begin("Game", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize);

        // Play area
        ImGui::BeginChildFrame(1, ImVec2(gameWidth, gameHeight));
        {
            ImVec2 framePos = ImGui::GetCursorScreenPos();
            ImDrawList* pDrawList = ImGui::GetWindowDrawList();

            // Check if moved
            static ImVec2 lastMousePos;
            ImVec2 mousePos = ImGui::GetMousePos();
            mousePos.x -= framePos.x;
            mousePos.y -= framePos.y;
            if (mousePos.x != lastMousePos.x ||
                mousePos.y != lastMousePos.y)
            {
                if (mousePos.x >= 0.0f && mousePos.x <= gameWidth &&
                    mousePos.y >= 0.0f && mousePos.y <= gameHeight)
                {
                    state.mouseX = (int)(mousePos.x / scale);
                    state.mouseY = (int)(mousePos.y / scale);
#if !RESEND_AT_60_FPS
                    app_mouseMoved({state.mouseX, state.mouseY});
#endif
                }
            }
            lastMousePos = mousePos;

            // Check if clicked
            static bool lastMouseDown = false;
            auto mouseDown = ImGui::IsMouseDown(0);
            if (mouseDown && !lastMouseDown)
            {
                if (mousePos.x >= 0.0f && mousePos.x <= 800.0f &&
                    mousePos.y >= 0.0f && mousePos.y <= 600.0f)
                {
                    app_shockwave({ (int)(mousePos.x / scale), (int)(mousePos.y / scale) });
                }
            }
            lastMouseDown = mouseDown;

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
                    ImVec2(framePos.x + (float)shockwave.pos.x * scale, framePos.y + (float)shockwave.pos.y * scale),
                    size * scale, ImColor(color), 64);

                ++it;
            }

            // Arrows
            for (const auto& member : state.lobby.members)
            {
                if (member.isAlive)
                {
                    pDrawList->AddImage(
                        ARROWS[member.colorIndex],
                        ImVec2(framePos.x + (float)member.pos.x * scale, 
                               framePos.y + (float)member.pos.y * scale),
                        ImVec2(framePos.x + (float)member.pos.x * scale + 64 * scale, 
                               framePos.y + (float)member.pos.y * scale + 64 * scale));
                }
            }
        }
        ImGui::EndChildFrame();
        ImGui::IsMouseDown(0);

        ImGui::End();
    }

#if RESEND_AT_60_FPS
    // Send mouse position at 60 fps
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();;
    if (now - lastTime >= std::chrono::microseconds(1000000 / 60))
    {
        lastTime = now;
        app_mouseMoved({state.mouseX, state.mouseY});
    }
#endif
}
