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
#include "globalChat.h"
#include "leaderboardPanel.h"

// C/C++ includes
#include <imgui.h>
#include <stdio.h>

#define LOBBY_LEFT_WIDTH 420.0f
#define LOBBY_RIGHT_WIDTH 460.0f
#define LOBBY_PANEL_HEIGHT 560.0f
#define LOBBY_GAP 32.0f
#define LOBBY_TAB_HEIGHT 44.0f

// 0 = Chat, 1 = Leaderboards, 2 = Info
static int s_lobbyRightTab = 0;
// 0 = This Lobby (signals), 1 = Global (Chat service) — only meaningful while
// s_lobbyRightTab == 0.
static int s_lobbyChatSubTab = 0;

static const ImVec4 LOBBY_COLOR_ME(0.35f, 1.0f, 0.45f, 1.0f);

//-----------------------------------------------------------------------------
// Left panel — member list (colour swatch, name, YOU/HOST badges, ready state,
// Worldwide Rank) + Leave/Start. Colour is changed via a popup on your own row now
// (BCLOUD-14490 follow-up), not an always-visible 40-swatch grid.
//-----------------------------------------------------------------------------

static void drawColorPickerPopup()
{
    if (!ImGui::BeginPopup("lobby_color_picker")) return;

    ImGui::TextDisabled("Choose your colour");
    ImGui::Separator();
    for (int i = 0; i < colorCount(); ++i)
    {
        if (i % 8 != 0)
            ImGui::SameLine();
        if (ImGui::ColorButton(("Col" + std::to_string(i)).c_str(), getColor(i),
                ImGuiColorEditFlags_NoTooltip, ImVec2(24.0f, 24.0f)))
        {
            app_changeUserColor(i);
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndPopup();
}

static void drawMemberRow(const User &member)
{
    bool isMe = (member.cxId == state.user.cxId);
    bool isHost = (member.cxId == state.lobby.ownerCxId);

    // Per-widget unique IDs (cxId embedded directly) instead of PushID/PopID —
    // OpenPopup("lobby_color_picker") below must resolve to the SAME id as
    // drawColorPickerPopup()'s BeginPopup("lobby_color_picker"), which is called
    // outside any PushID scope. ImGui hashes popup ids together with whatever's on
    // the id stack at the time, so wrapping this row in PushID(member.cxId) would
    // silently make OpenPopup target a different id than BeginPopup ever looks for
    // — the popup would never appear, with no error, exactly what happened here.
    ImVec4 color = getColor(member.colorIndex % colorCount());
    if (isMe)
    {
        // My own swatch is clickable — opens the colour picker popup.
        if (ImGui::ColorButton("##mycolor", color, ImGuiColorEditFlags_NoTooltip, ImVec2(20.0f, 20.0f)))
            ImGui::OpenPopup("lobby_color_picker");
    }
    else
    {
        std::string colId = "##col_" + member.cxId;
        ImGui::ColorButton(colId.c_str(), color,
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(20.0f, 20.0f));
    }
    ImGui::SameLine();

    ImGui::TextColored(isMe ? LOBBY_COLOR_ME : ImVec4(1, 1, 1, 1), "%s", member.name.c_str());

    if (isMe)
    {
        ImGui::SameLine();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 sz = ImGui::CalcTextSize("YOU");
        ImGui::GetWindowDrawList()->AddRectFilled(p0, ImVec2(p0.x + sz.x + 8.0f, p0.y + sz.y + 2.0f),
            ImColor(ImVec4(1, 1, 1, 0.15f)), 4.0f);
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 4.0f, p0.y + 1.0f));
        ImGui::TextUnformatted("YOU");
    }
    if (isHost)
    {
        ImGui::SameLine();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 sz = ImGui::CalcTextSize("HOST");
        ImGui::GetWindowDrawList()->AddRectFilled(p0, ImVec2(p0.x + sz.x + 8.0f, p0.y + sz.y + 2.0f),
            ImColor(ImVec4(1.0f, 0.84f, 0.0f, 0.25f)), 4.0f);
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 4.0f, p0.y + 1.0f));
        ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "HOST");
    }

    // Worldwide Rank — right-aligned. Each member fetched their OWN rank and shared
    // it via the lobby's extra field (see app_fetchWorldwideRank in app.cpp); there's
    // no client API to look up an arbitrary other player's rank directly.
    std::string rankStr = (member.worldwideRank >= 0) ? ("#" + std::to_string(member.worldwideRank)) : "Unranked";
    float rankW = ImGui::CalcTextSize(rankStr.c_str()).x;
    ImGui::SameLine(LOBBY_LEFT_WIDTH - rankW - 40.0f);
    ImGui::TextColored(member.worldwideRank >= 0 ? rankColorFor(member.worldwideRank) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
        "%s", rankStr.c_str());

    // Status line below the name (ready state).
    ImGui::TextDisabled(member.isReady ? "Ready" : "Not ready");

    ImGui::Separator();
}

static void drawLobbyMembersPanel(float x, float y)
{
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(LOBBY_LEFT_WIDTH, LOBBY_PANEL_HEIGHT), ImGuiCond_Always);
    ImGui::Begin("Lobby", nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize);

    ImGui::Text("Lobby");
    ImGui::Separator();

    // Header row — "Members" on the left, "Worldwide Rank" right-aligned, matching
    // the member rows' own column layout.
    ImGui::TextDisabled("Members");
    {
        const char *rankHeader = "WORLDWIDE RANK";
        float w = ImGui::CalcTextSize(rankHeader).x;
        ImGui::SameLine(LOBBY_LEFT_WIDTH - w - 40.0f);
        ImGui::TextDisabled("%s", rankHeader);
    }

    // Scrollable member list — up to 40 players in a CursorParty lobby, so this
    // must scroll rather than grow the (fixed-height) window.
    ImGui::BeginChild("lobby_members_scroll", ImVec2(0.0f, -48.0f), true);
    for (const auto &member : state.lobby.members)
        drawMemberRow(member);
    drawColorPickerPopup();
    ImGui::EndChild();

    // Leave / Start — pinned at the bottom.
    if (ImGui::Button("Leave"))
        app_cancelLobby();

    bool isHost = (state.user.cxId == state.lobby.ownerCxId);
    if (isHost)
    {
        if (settings.autoGeoTest)
        {
            auto elapsed = std::chrono::steady_clock::now() - state.geoTestLobbyArrivalTime;
            if (elapsed >= std::chrono::milliseconds(1500))
                app_startGame();
        }
        else if (state.awaitingRematch)
        {
            // Rematch flow is fully automatic (app_tickRematchGate, ticked above) — no
            // manual override here, so a host who returns early can't skip the "wait for
            // stragglers or 15s" window the user asked for.
            ImGui::SameLine();
            ImGui::TextDisabled("Waiting for other players to return...");
        }
        else
        {
            ImGui::SameLine();
            if (ImGui::Button("Start"))
                app_startGame();
        }
    }

    ImGui::End();
}

//-----------------------------------------------------------------------------
// Right panel — CHAT (This Lobby / Global sub-toggle) | LEADERBOARDS | INFO.
//-----------------------------------------------------------------------------

static void drawLobbyRightTabs(float panelX, float panelY)
{
    ImGui::SetNextWindowPos(ImVec2(panelX, panelY - LOBBY_TAB_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##lobby_right_tabs", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);

    auto tab = [](const char *label, int idx, float w)
    {
        bool active = (s_lobbyRightTab == idx);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.45f, 0.90f, 1.0f));
        if (ImGui::Button(label, ImVec2(w, 32.0f))) s_lobbyRightTab = idx;
        if (active) ImGui::PopStyleColor();
    };

    tab("CHAT", 0, 90.0f);
    ImGui::SameLine();
    tab("LEADERBOARDS", 1, 150.0f);
    ImGui::SameLine();
    tab("INFO", 2, 90.0f);

    ImGui::End();
}

// This-lobby chat, via Lobby service signals (state.lobby.chatMessages is appended
// to by app_sendLobbySignal on send and the LOBBY_SIGNAL_DATA handler in
// onLobbyEvent on receive — see app.cpp for both).
static void drawLobbySignalChat()
{
    ImGui::BeginChild("lobby_signal_scroll", ImVec2(0.0f, -32.0f), true);
    bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f;
    for (const auto &m : state.lobby.chatMessages)
    {
        bool isMe = (m.fromName == state.user.name);
        ImGui::TextColored(isMe ? LOBBY_COLOR_ME : ImVec4(0.6f, 0.75f, 1.0f, 1.0f), "%s:", m.fromName.c_str());
        ImGui::SameLine();
        ImGui::TextWrapped("%s", m.text.c_str());
    }
    // Same "jump to bottom on growth" fix as the global chat — a lobby chat history
    // can arrive all at once too (join-in-progress + backlog), not just one at a time.
    static size_t s_lastCount = 0;
    if (wasAtBottom || state.lobby.chatMessages.size() > s_lastCount)
        ImGui::SetScrollHereY(1.0f);
    s_lastCount = state.lobby.chatMessages.size();
    ImGui::EndChild();

    static char buf[240] = {0};
    ImGui::PushItemWidth(-70.0f);
    bool enterPressed = ImGui::InputText("##lobbyChatInput", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    bool sendClicked = ImGui::Button("Send", ImVec2(60.0f, 0.0f));
    if ((enterPressed || sendClicked) && buf[0] != '\0')
    {
        app_sendLobbySignal(buf);
        buf[0] = '\0';
    }
}

static void drawLobbyChatTab(float x, float y, float w, float h)
{
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    ImGui::Begin("##lobby_chat_tab", nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar);

    bool lobbyActive = (s_lobbyChatSubTab == 0);
    if (lobbyActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.35f, 1.0f));
    if (ImGui::Button("THIS LOBBY")) s_lobbyChatSubTab = 0;
    if (lobbyActive) ImGui::PopStyleColor();
    ImGui::SameLine();
    bool globalActive = (s_lobbyChatSubTab == 1);
    if (globalActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.35f, 1.0f));
    if (ImGui::Button("GLOBAL")) s_lobbyChatSubTab = 1;
    if (globalActive) ImGui::PopStyleColor();
    ImGui::Separator();

    if (s_lobbyChatSubTab == 0)
        drawLobbySignalChat();
    else
        drawGlobalChatContent();

    ImGui::End();
}

static void drawLobbyInfoTab(float x, float y, float w, float h)
{
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    ImGui::Begin("##lobby_info_tab", nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar);

    int maxMembers = maxLobbyMembers(settings.lobbyType);
    ImGui::Text("Lobby: %s", state.lobby.lobbyId.c_str());
    if (!state.lobby.regionId.empty())
        ImGui::Text("Region: %s", state.lobby.regionId.c_str());
    ImGui::Text("Players: %d / %d", (int)state.lobby.members.size(), maxMembers);

    auto secondsInLobby = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - state.lobby.arrivalTime).count();
    ImGui::Text("Time in lobby: %02lld:%02lld", (long long)(secondsInLobby / 60), (long long)(secondsInLobby % 60));

    bool isHost = (state.user.cxId == state.lobby.ownerCxId);
    ImGui::Spacing();
    if (isHost)
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Press Start when ready.");
    else
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Waiting for the host to start...");

    // Last match results — shown once a round finishes (state.matchResult, set by
    // app_tickMatch()/applyMatchResult()) until the next round starts (cleared in
    // onRelayConnected()). Entries are already in rank order from computeCoverage().
    if (state.matchResult.valid)
    {
        ImGui::Separator();
        ImGui::TextDisabled("Last Match Results");
        for (const auto &entry : state.matchResult.entries)
        {
            const User *pMember = nullptr;
            for (const auto &m : state.lobby.members)
                if (m.cxId == entry.cxId) { pMember = &m; break; }
            std::string name = pMember ? pMember->name : "?";
            bool isMe = (entry.cxId == state.user.cxId);
            ImVec4 color = isMe ? LOBBY_COLOR_ME : ImVec4(1, 1, 1, 1);
            ImGui::TextColored(rankColorFor(entry.rank), "#%d", entry.rank);
            ImGui::SameLine(50.0f);
            ImGui::TextColored(color, "%s  %.1f%%  (+%d pts)", name.c_str(), entry.coveragePct, entry.beaten + 1);
        }
    }

    // Ping data — only shown when ping region data is enabled.
    if (settings.usePingData)
    {
        std::vector<std::string> regions;
        auto addRegion = [&](const std::string &r)
        {
            if (std::find(regions.begin(), regions.end(), r) == regions.end())
                regions.push_back(r);
        };
        for (const auto &kv : state.pingData)
            addRegion(kv.first);
        for (const auto &m : state.lobby.members)
            for (const auto &kv : m.pings)
                addRegion(kv.first);
        std::sort(regions.begin(), regions.end());

        if (!regions.empty())
        {
            ImGui::Separator();
            ImGui::TextDisabled("Ping Data (ms)");
            for (const auto &member : state.lobby.members)
            {
                const std::map<std::string, int> *pPings = &member.pings;
                std::map<std::string, int> selfPings;
                if (pPings->empty() && member.cxId == state.user.cxId && !state.pingData.empty())
                {
                    selfPings = state.pingData;
                    pPings = &selfPings;
                }
                if (pPings->empty())
                    continue;

                std::string label = member.name + ": ";
                for (const auto &r : regions)
                {
                    auto it = pPings->find(r);
                    char buf[32];
                    if (it != pPings->end())
                        snprintf(buf, sizeof(buf), "%s %s  ", r.c_str(), it->second >= 999 ? "T/O" : std::to_string(it->second).c_str());
                    else
                        snprintf(buf, sizeof(buf), "%s -  ", r.c_str());
                    label += buf;
                }
                if (member.cxId == state.user.cxId)
                    ImGui::TextColored(getColor(member.colorIndex % colorCount()), "%s", label.c_str());
                else
                    ImGui::TextDisabled("%s", label.c_str());
            }
        }
    }

    ImGui::End();
}

// Small non-blocking status line shown while a round is being provisioned (STARTING ->
// ROOM_READY) — the Lobby screen (chat, member list, etc.) stays fully usable underneath
// it instead of being replaced by a blocking loading/cancel screen (BCLOUD-14489 follow-up).
static void drawProvisioningBanner()
{
    if (!state.isProvisioning) return;

    ImGui::SetNextWindowPos(ImVec2((float)width / 2.0f, ImGui::GetFrameHeight() + 8.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::Begin("##provisioning_banner", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "Starting round... %s", state.provisioningStatus.c_str());
    ImGui::End();
}

// Draws the lobby screen and updates its logic.
void lobby_update()
{
    // Keeps the host's auto-rematch decision progressing even after the host itself has
    // already returned here from the Match Summary screen while other players haven't yet.
    app_tickRematchGate();

    float totalWidth = LOBBY_LEFT_WIDTH + LOBBY_GAP + LOBBY_RIGHT_WIDTH;
    float startX = (float)width / 2.0f - totalWidth / 2.0f;
    float y = (float)height / 2.0f - LOBBY_PANEL_HEIGHT / 2.0f;
    float rightX = startX + LOBBY_LEFT_WIDTH + LOBBY_GAP;

    drawLobbyMembersPanel(startX, y);
    drawLobbyRightTabs(rightX, y);
    drawProvisioningBanner();

    switch (s_lobbyRightTab)
    {
    case 0:
        drawLobbyChatTab(rightX, y, LOBBY_RIGHT_WIDTH, LOBBY_PANEL_HEIGHT);
        break;
    case 1:
        drawLeaderboardPanel("##lobby_leaderboards", rightX, y, LOBBY_RIGHT_WIDTH, LOBBY_PANEL_HEIGHT);
        break;
    case 2:
        drawLobbyInfoTab(rightX, y, LOBBY_RIGHT_WIDTH, LOBBY_PANEL_HEIGHT);
        break;
    }
}
