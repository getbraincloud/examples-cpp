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
                auto color = getColor(user.colorIndex % colorCount());
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                std::string label = user.name;
                if (user.cxId == state.lobby.ownerCxId) label += " [Host]";
                if (user.cxId == state.user.cxId)       label += " (Echo)";
                ImGui::Checkbox(label.c_str(), &user.allowSendTo);
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

        ImGui::Separator();

        // Game session info
        bool isHost = state.user.cxId == state.lobby.ownerCxId;
        if (isHost)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "HOST");
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear Splotches"))
                app_clearSplotches();
        }

        if (state.gameStartTime != 0)
        {
            auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            long long elapsedMs = nowMs - state.gameStartTime;
            long long elapsedSec = elapsedMs / 1000;
            int minutes = (int)(elapsedSec / 60);
            int seconds = (int)(elapsedSec % 60);
            ImGui::Text("Game Time: %d:%02d", minutes, seconds);

            // CursorParty: 1:30 round with 10-second countdown then auto end-match
            if (isCursorPartyLobby(settings.lobbyType))
            {
                static const long long MATCH_DURATION_MS = 90000LL;
                static const long long COUNTDOWN_FROM_MS = 80000LL;
                static int matchEndRound = -1; // tracks which round end was already sent

                if (elapsedMs >= MATCH_DURATION_MS)
                {
                    if (isHost && matchEndRound != state.roundNumber)
                    {
                        matchEndRound = state.roundNumber;
                        app_endMatch();
                    }
                }
                else if (elapsedMs >= COUNTDOWN_FROM_MS)
                {
                    long long remaining = (MATCH_DURATION_MS - elapsedMs + 999LL) / 1000LL;
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                                       "Ending in %lld...", remaining);
                }
            }
        }
        ImGui::Text("Round: %d", state.roundNumber);
        ImGui::Text("Lobby: %s", state.lobby.lobbyId.c_str());

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

            // Splotches — persistent marks left by shockwaves, drawn under the transient rings
            if (SPLOTCH_TEX)
            {
                // Spring-overshoot pop: 0→~1.4 peak→1.0 settle over 0.3s (matches Unity AnimateSplatter)
                auto splatSize = [](float t) -> float {
                    if (t <= 0.0f) return 0.0f;
                    if (t >= 1.0f) return 1.0f;
                    const float a = 0.6f, b = 0.4f;
                    float grow   = (1.0f + b) * t / a;
                    float shrink = -(((1.0f + b) * t) - ((2.0f + b) * a)) / a;
                    return std::max(0.0f, std::min(1.0f + b, std::min(grow, shrink)));
                };

                auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                for (auto it = state.splotches.begin(); it != state.splotches.end();)
                {
                    auto &splotch = *it;
                    long long ageSec = (nowMs - splotch.startTimeMs) / 1000LL;

                    if (state.splotchDurationSec >= 0 && ageSec >= state.splotchDurationSec)
                    {
                        it = state.splotches.erase(it);
                        continue;
                    }

                    float alpha = 0.55f;
                    if (state.splotchDurationSec > 0)
                    {
                        long long remaining = (long long)state.splotchDurationSec - ageSec;
                        if (remaining <= 3)
                            alpha *= (float)remaining / 3.0f;
                    }

                    auto base = getColor(splotch.colorIndex % colorCount());
                    ImVec4 tint(base.x, base.y, base.z, alpha);

                    float elapsedSec = (float)(nowMs - splotch.startTimeMs) / 1000.0f;
                    float t         = std::min(elapsedSec / 0.3f, 1.0f);
                    float halfSize  = SPLOTCH_DISPLAY_SIZE * 0.5f * scale * splatSize(t);

                    ImVec2 center(framePos.x + (float)splotch.pos.x * scale,
                                  framePos.y + (float)splotch.pos.y * scale);

                    // Build rotated quad corners from center + half-size + rotation angle
                    float c = cosf(splotch.rotation), s2 = sinf(splotch.rotation);
                    auto rot = [&](float px, float py) -> ImVec2 {
                        return ImVec2(center.x + px * c - py * s2,
                                      center.y + px * s2 + py * c);
                    };
                    ImVec2 p1 = rot(-halfSize, -halfSize);
                    ImVec2 p2 = rot( halfSize, -halfSize);
                    ImVec2 p3 = rot( halfSize,  halfSize);
                    ImVec2 p4 = rot(-halfSize,  halfSize);

                    pDrawList->AddImageQuad(SPLOTCH_TEX, p1, p2, p3, p4,
                        {0,0}, {1,0}, {1,1}, {0,1}, ImColor(tint));

                    ++it;
                }
            }

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
                auto color = getColor(shockwave.colorIndex % colorCount());
                color.w = 1.0f - percent;
                pDrawList->AddCircleFilled(
                    ImVec2(framePos.x + (float)shockwave.pos.x * scale, framePos.y + (float)shockwave.pos.y * scale),
                    size * scale, ImColor(color), 64);

                ++it;
            }

            // Arrows — drawn as colored vector cursors so the color always
            // matches the player's selection exactly (arrow PNGs are black-alpha
            // masks and cannot be tinted to arbitrary palette colors).
            for (const auto& member : state.lobby.members)
            {
                if (!member.isAlive) continue;

                float s  = 14.0f * scale;
                ImVec2 p(framePos.x + (float)member.pos.x * scale,
                         framePos.y + (float)member.pos.y * scale);
                ImU32  col     = ImColor(getColor(member.colorIndex % colorCount()));
                ImU32  shadow  = IM_COL32(0, 0, 0, 160);

                // NW-pointing cursor arrow built from three triangles:
                //   - main body (tip → shaft)
                //   - corner fill (shaft → shoulder)
                //   - diagonal tail
                ImVec2 tip(p.x,             p.y);
                ImVec2 bl (p.x,             p.y + s);
                ImVec2 sh (p.x + s * 0.35f, p.y + s * 0.65f);
                ImVec2 t0 (p.x + s * 0.42f, p.y + s * 0.90f);
                ImVec2 t1 (p.x + s * 0.62f, p.y + s * 0.75f);

                // Drop shadow (offset 1.5px)
                const float d = 1.5f;
                pDrawList->AddTriangleFilled({tip.x+d,tip.y+d},{bl.x+d,bl.y+d},{sh.x+d,sh.y+d}, shadow);
                pDrawList->AddTriangleFilled({sh.x+d,sh.y+d},{bl.x+d,bl.y+d},{t0.x+d,t0.y+d},  shadow);
                pDrawList->AddTriangleFilled({sh.x+d,sh.y+d},{t0.x+d,t0.y+d},{t1.x+d,t1.y+d},  shadow);

                // Colored fill
                pDrawList->AddTriangleFilled(tip, bl,  sh,  col);
                pDrawList->AddTriangleFilled(sh,  bl,  t0,  col);
                pDrawList->AddTriangleFilled(sh,  t0,  t1,  col);
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
