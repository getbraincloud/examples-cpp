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
#include <algorithm>
#include <imgui.h>
#include <stdio.h>
#include <chrono>

// Urgency thresholds for the match timer color (BCLOUD-14490 item 14).
static constexpr long long TIMER_URGENT_SEC = 10;
static constexpr long long TIMER_WARN_SEC   = 30;

// How long a rank-swap highlight/arrow stays visible after a player's rank changes
// (BCLOUD-14490 item 13). Driven by CoverageEntry::rankChangedAtMs, set once per
// recompute in app_tickMatch() — never per-frame, so it can't strobe.
static constexpr long long RANK_FLASH_MS = 600;

// Fixed width of the docked scoreboard sidebar (matches the reference mockup's layout:
// a full-height left sidebar, canvas + timer/ping/exit-match to its right).
static constexpr float SIDEBAR_WIDTH = 220.0f;

static const ImVec4 COLOR_ME(0.35f, 1.0f, 0.45f, 1.0f);


// The RANK / PLAYER / COVERAGE sidebar — driven by state.coverage, which app_tickMatch()
// keeps live-sorted best-first (BCLOUD-14490 items 6-10, 13). Docked full-height on the
// left, matching the reference mockup.
static void drawScoreboardSidebar(long long nowMs)
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetFrameHeight()), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(SIDEBAR_WIDTH, (float)height - ImGui::GetFrameHeight()), ImGuiCond_Always);
    ImGui::Begin("##scoreboard", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings);

    if (ImGui::BeginTable("scoreboard_table", 2,
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("RANK / PLAYER", ImGuiTableColumnFlags_WidthStretch, 0.68f);
        ImGui::TableSetupColumn("COVERAGE", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_IndentDisable, 0.32f);
        ImGui::TableHeadersRow();

        for (const auto& entry : state.coverage)
        {
            const User* pMember = nullptr;
            for (const auto& m : state.lobby.members)
            {
                if (m.cxId == entry.cxId) { pMember = &m; break; }
            }
            if (!pMember) continue;

            bool isMe = (entry.cxId == state.user.cxId);
            bool flashing = entry.rankChangedAtMs > 0 && (nowMs - entry.rankChangedAtMs) < RANK_FLASH_MS;
            bool improved = entry.rank < entry.prevRank;

            ImGui::TableNextRow();
            if (isMe)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImColor(ImVec4(1.0f, 1.0f, 1.0f, 0.08f)));
            else if (flashing)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    ImColor(improved ? ImVec4(0.3f, 1.0f, 0.3f, 0.20f) : ImVec4(1.0f, 0.3f, 0.3f, 0.20f)));

            // RANK / PLAYER — "#N" (gold/silver/bronze, item 7) + color dot + name
            // (green for "me", item 8) + a "YOU" bubble + rank-swap arrow (item 13).
            ImGui::TableNextColumn();
            {
                ImVec4 rankColor(1.0f, 1.0f, 1.0f, 1.0f); // plain white for 4th place and below (item 7)
                if (entry.rank == 1)      rankColor = ImVec4(1.00f, 0.84f, 0.00f, 1.0f); // gold
                else if (entry.rank == 2) rankColor = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);  // silver
                else if (entry.rank == 3) rankColor = ImVec4(0.80f, 0.50f, 0.20f, 1.0f);  // bronze

                const char* arrow = !flashing ? "" : (improved ? "^" : "v");
                ImGui::TextColored(rankColor, "%s#%d", arrow, entry.rank);
                ImGui::SameLine();
                ImGui::TextColored(getColor(pMember->colorIndex % colorCount()), "\xE2\x97\x8F"); // "●" color dot
                ImGui::SameLine();
                ImGui::TextColored(isMe ? COLOR_ME : ImVec4(1, 1, 1, 1), "%s", pMember->name.c_str());
                if (isMe)
                {
                    ImGui::SameLine();
                    ImVec2 textSize = ImGui::CalcTextSize("YOU");
                    ImVec2 p0 = ImGui::GetCursorScreenPos();
                    ImVec2 p1 = ImVec2(p0.x + textSize.x + 8.0f, p0.y + textSize.y + 2.0f);
                    ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, ImColor(ImVec4(1.0f, 1.0f, 1.0f, 0.15f)), 4.0f);
                    ImGui::SetCursorScreenPos(ImVec2(p0.x + 4.0f, p0.y + 1.0f));
                    ImGui::TextUnformatted("YOU");
                }
            }

            // COVERAGE — as a % (item 9), green for "me" to match the rank/player color
            ImGui::TableNextColumn();
            ImGui::TextColored(isMe ? COLOR_ME : ImVec4(1, 1, 1, 1), "%.0f%%", entry.coveragePct);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

// Debug/connection tooling — kept (per the CLAUDE.md RTA checklist's documented
// Reliable/Ordered/Channel controls) but out of the way of the scoreboard sidebar,
// which the reference mockup keeps clean.
static void drawDebugPanel()
{
    ImGui::SetNextWindowPos(ImVec2((float)width - 8.0f, (float)height - 8.0f), ImGuiCond_FirstUseEver, ImVec2(1.0f, 1.0f));
    ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    if (!state.lobby.regionId.empty())
    {
        bool isActual = !regionFromLobbyId(state.lobby.lobbyId).empty();
        ImGui::TextDisabled("%s: %s", isActual ? "Region" : "Est. region", state.lobby.regionId.c_str());
    }

    ImGui::Text("Reliable options");
    ImGui::Indent();
    ImGui::TextDisabled("Only affect position");
    ImGui::Text("Channel");
    ImGui::Indent();
    ImGui::BeginGroup();
    for (int i = 0; i < 4; ++i)
    {
        bool active = settings.sendChannel == i;
        if (ImGui::RadioButton(std::to_string(i).c_str(), active))
            settings.sendChannel = i;
        if (i % 2 == 0) ImGui::SameLine();
    }
    ImGui::EndGroup();
    ImGui::Unindent();
    ImGui::Checkbox("Reliable", &settings.sendReliable);
    ImGui::Checkbox("Ordered", &settings.sendOrdered);
    ImGui::Unindent();

    ImGui::Separator();
    ImGui::Text("Round: %d", state.roundNumber);
    ImGui::Text("Lobby: %s", state.lobby.lobbyId.c_str());

    ImGui::End();
}

// Draws a game dialog and update its logic
void game_update()
{
    // Drive the shared coverage/ranking recompute and the host-authoritative
    // match-end + leaderboard-post flow. Safe to call every frame — internally
    // throttled (COVERAGE_RECOMPUTE_MS) and phase-guarded.
    app_tickMatch();

    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    drawScoreboardSidebar(nowMs);
    drawDebugPanel();

    // Main game window, centered in the area to the right of the sidebar
    {
        float gameWidth = CANVAS_W;
        float gameHeight = CANVAS_H;
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
        float rightAreaX = SIDEBAR_WIDTH;
        float rightAreaW = (float)width - SIDEBAR_WIDTH;
        ImGui::SetNextWindowPos(ImVec2(
            rightAreaX + rightAreaW / 2.0f - gameWidth / 2.0f,
            (float)height / 2.0f - gameHeight / 2.0f));
        ImGui::Begin("Game", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoTitleBar); // item 1: remove game title from gameplay view

        // Timer (left, colored by urgency) + "Exit Match" (right) — above the canvas
        // (item 12), replacing the old menu-bar "Leave"/"End Match" items (item 3).
        {
            long long remainingSec = 0;
            ImVec4 timerColor(1.0f, 1.0f, 1.0f, 1.0f);
            if (state.gameStartTime != 0 && isCursorPartyLobby(settings.lobbyType))
            {
                long long elapsedMs = nowMs - state.gameStartTime;
                long long remainingMs = MATCH_DURATION_MS - elapsedMs;
                if (remainingMs < 0) remainingMs = 0;
                remainingSec = (remainingMs + 999) / 1000;

                if (remainingSec <= TIMER_URGENT_SEC)
                    timerColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                else if (remainingSec <= TIMER_WARN_SEC)
                    timerColor = ImVec4(1.0f, 0.75f, 0.2f, 1.0f);

                ImGui::TextColored(timerColor, "%lld:%02lld", remainingSec / 60, remainingSec % 60);
            }

            ImGui::SameLine(gameWidth - 110.0f);
            if (ImGui::Button("\xE2\x86\xA9 Exit Match")) // "↩ Exit Match"
            {
                app_closeGame();
            }

            // Single self-ping readout (item 11 — ping lives outside the scoreboard's
            // main column entirely now, rather than a per-row column).
            if (pBCWrapper)
            {
                int ping = pBCWrapper->getRelayService()->getPing();
                ImVec4 pingColor = ping < 0 ? ImVec4(0.6f, 0.6f, 0.6f, 1.0f)
                                  : ping < 100 ? ImVec4(0.4f, 0.9f, 0.5f, 1.0f)
                                  : ping < 200 ? ImVec4(0.95f, 0.8f, 0.3f, 1.0f)
                                  : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                ImGui::TextColored(pingColor, "\xE2\x97\x8F"); // "●"
                ImGui::SameLine();
                if (ping < 0)
                    ImGui::TextDisabled("Ping: ...");
                else if (ping >= 999)
                    ImGui::TextDisabled("Ping: T/O");
                else
                    ImGui::TextDisabled("Ping: %d ms", ping);
            }
        }

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
                if (mousePos.x >= 0.0f && mousePos.x <= CANVAS_W &&
                    mousePos.y >= 0.0f && mousePos.y <= CANVAS_H)
                {
                    app_shockwave({ (int)(mousePos.x / scale), (int)(mousePos.y / scale) });
                }
            }
            lastMouseDown = mouseDown;

            // Splotches — persistent marks left by shockwaves, drawn under the transient rings.
            // Expiry is pruned in one remove_if pass (was an O(N^2) per-frame erase loop)
            // before the draw pass, bumping splotchGeneration exactly once when it changes
            // so app_tickMatch()'s coverage recompute notices.
            if (state.splotchDurationSec >= 0)
            {
                size_t before = state.splotches.size();
                state.splotches.erase(
                    std::remove_if(state.splotches.begin(), state.splotches.end(),
                        [&](const Splotch& s) {
                            long long ageSec = (nowMs - s.startTimeMs) / 1000LL;
                            return ageSec >= state.splotchDurationSec;
                        }),
                    state.splotches.end());
                if (state.splotches.size() != before)
                    ++state.splotchGeneration;
            }

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

                for (const auto &splotch : state.splotches)
                {
                    long long ageSec = (nowMs - splotch.startTimeMs) / 1000LL;

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

                // Live coverage % next to the cursor, in-canvas (in addition to the
                // sidebar scoreboard) — drawn before the arrow so the arrow renders on top.
                for (const auto& covEntry : state.coverage)
                {
                    if (covEntry.cxId != member.cxId) continue;
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%.0f%%", covEntry.coveragePct);
                    ImVec2 textPos(p.x + s * 0.9f, p.y - 16.0f * scale);
                    pDrawList->AddText(ImVec2(textPos.x + 1, textPos.y + 1), shadow, buf);
                    pDrawList->AddText(textPos, col, buf);
                    break;
                }

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
