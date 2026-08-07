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
#include "BCCallback.h"

// C/C++ includes
#include <algorithm>
#include <chrono>
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

//-----------------------------------------------------------------------------
// Leaderboard viewer (BCLOUD-14472 follow-up) — top 5 + "you" row, toggleable
// between the two boards this app posts to (points / coverage) and Lifetime vs
// Quarterly. File-static since this is pure main-menu display state, not
// something any other screen or the relay wire protocol needs.
//-----------------------------------------------------------------------------

struct LeaderboardRow
{
    std::string name;
    int64_t score = 0;
    int rank = 0;
};

static std::vector<LeaderboardRow> s_lbTop;
static LeaderboardRow s_lbSelf;
static bool s_lbHasSelf = false;
static int s_lbBoardType = 0; // 0 = Most Opponents Beaten (points), 1 = Highest Coverage %
static int s_lbPeriod = 0;    // 0 = Lifetime, 1 = Quarterly
static int s_lbFetchedKey = -1; // -1 = never fetched; otherwise boardType*2+period already requested this session

static std::string currentLeaderboardId()
{
    bool coverage = (s_lbBoardType == 1);
    bool quarterly = (s_lbPeriod == 1);
    if (coverage)
        return quarterly ? state.coverageLeaderboardIdQuarterly : state.coverageLeaderboardId;
    return quarterly ? state.pointsLeaderboardIdQuarterly : state.pointsLeaderboardId;
}

// The score's user-defined "data" carries the display name (postMatchScores in
// app.cpp sets it) — GetGlobalLeaderboardPage/View don't otherwise return a usable
// name for arbitrary (non-friend) entries.
static LeaderboardRow parseLeaderboardEntry(const Json::Value &entry)
{
    LeaderboardRow row;
    row.score = entry["score"].asInt64();
    row.rank = entry["rank"].asInt();
    row.name = entry["data"]["name"].asString();
    if (row.name.empty())
        row.name = "Player";
    return row;
}

// Fetches the top 5 + the local player's own rank for the currently-selected board/
// period combo. Guarded by s_lbFetchedKey so it only ever fires once per combo per
// session — switching tabs back and forth re-shows cached results, not a re-fetch.
static void fetchLeaderboardIfNeeded()
{
    int key = s_lbBoardType * 2 + s_lbPeriod;
    if (s_lbFetchedKey == key || !pBCWrapper) return;
    s_lbFetchedKey = key;

    std::string leaderboardId = currentLeaderboardId();

    pBCWrapper->getSocialLeaderboardService()->getGlobalLeaderboardPage(
        leaderboardId.c_str(), BrainCloud::HIGH_TO_LOW, 0, 4,
        new BCCallback(
            [key](const Json::Value &result)
            {
                if (s_lbFetchedKey != key) return; // stale — user switched tabs since this was requested
                s_lbTop.clear();
                for (const auto &entry : result["data"]["leaderboard"])
                    s_lbTop.push_back(parseLeaderboardEntry(entry));
            },
            [key](const std::string &)
            {
                if (s_lbFetchedKey != key) return;
                s_lbTop.clear();
            }));

    // Pro-tip from the brainCloud docs: beforeCount=0/afterCount=0 on
    // GetGlobalLeaderboardView returns just the current player's own entry.
    pBCWrapper->getSocialLeaderboardService()->getGlobalLeaderboardView(
        leaderboardId.c_str(), BrainCloud::HIGH_TO_LOW, 0, 0,
        new BCCallback(
            [key](const Json::Value &result)
            {
                if (s_lbFetchedKey != key) return;
                const auto &arr = result["data"]["leaderboard"];
                s_lbHasSelf = !arr.empty();
                if (s_lbHasSelf)
                    s_lbSelf = parseLeaderboardEntry(arr[0]);
            },
            [key](const std::string &)
            {
                if (s_lbFetchedKey != key) return;
                s_lbHasSelf = false;
            }));
}

// "4,821" style thousands separator — scores can be into the thousands for the
// cumulative points board.
static std::string formatScore(int64_t v)
{
    std::string s = std::to_string(v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3)
        s.insert(i, ",");
    return s;
}

// The coverage board's raw score is basis points (postMatchScores in app.cpp posts
// coveragePct*100 as an int, since brainCloud leaderboard scores are int64 — there's
// no float score type) — divide back down to a percentage for display. The points
// board's raw score is already the real value (players beaten + completion bonus).
static std::string formatBoardScore(int64_t v)
{
    if (s_lbBoardType == 1)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f%%", v / 100.0);
        return buf;
    }
    return formatScore(v);
}

static ImVec4 rankColorFor(int rank)
{
    if (rank == 1) return ImVec4(1.00f, 0.84f, 0.00f, 1.0f); // gold
    if (rank == 2) return ImVec4(0.75f, 0.75f, 0.75f, 1.0f); // silver
    if (rank == 3) return ImVec4(0.80f, 0.50f, 0.20f, 1.0f); // bronze
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

static void drawLeaderboardCard(float x, float y)
{
    fetchLeaderboardIfNeeded();

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(LEADERBOARD_CARD_WIDTH, CHAT_CARD_HEIGHT), ImGuiCond_Always);
    ImGui::Begin("Leaderboard", nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize);

    // Board-type toggle: Most Opponents Beaten <-> Highest Coverage %
    {
        bool pointsActive = (s_lbBoardType == 0);
        if (pointsActive) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
        if (ImGui::Button("Most Opponents Beaten")) s_lbBoardType = 0;
        if (pointsActive) ImGui::PopStyleColor();
        ImGui::SameLine();
        bool coverageActive = (s_lbBoardType == 1);
        if (coverageActive) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
        if (ImGui::Button("Highest Coverage %")) s_lbBoardType = 1;
        if (coverageActive) ImGui::PopStyleColor();
    }

    // Period toggle: Lifetime <-> Quarterly
    {
        bool lifetimeActive = (s_lbPeriod == 0);
        if (lifetimeActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.35f, 1.0f));
        if (ImGui::Button("Lifetime")) s_lbPeriod = 0;
        if (lifetimeActive) ImGui::PopStyleColor();
        ImGui::SameLine();
        bool quarterlyActive = (s_lbPeriod == 1);
        if (quarterlyActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.35f, 1.0f));
        if (ImGui::Button("Quarterly")) s_lbPeriod = 1;
        if (quarterlyActive) ImGui::PopStyleColor();
    }

    ImGui::Separator();

    if (s_lbTop.empty())
    {
        ImGui::TextDisabled("No scores yet — be the first!");
    }
    else
    {
        for (const auto &row : s_lbTop)
        {
            ImGui::TextColored(rankColorFor(row.rank), "#%d", row.rank);
            ImGui::SameLine(50.0f);
            ImGui::TextUnformatted(row.name.c_str());
            std::string scoreStr = formatBoardScore(row.score);
            float tw = ImGui::CalcTextSize(scoreStr.c_str()).x;
            ImGui::SameLine(LEADERBOARD_CARD_WIDTH - tw - 32.0f);
            ImGui::Text("%s", scoreStr.c_str());
        }
    }

    // "You" row — only when it's not already visible in the top 5, mirroring the
    // "top N + you" pattern from the reference design.
    if (s_lbHasSelf)
    {
        bool alreadyShown = false;
        for (const auto &row : s_lbTop)
            if (row.rank == s_lbSelf.rank) { alreadyShown = true; break; }

        if (!alreadyShown)
        {
            if (!s_lbTop.empty())
                ImGui::TextDisabled("...");
            ImGui::TextColored(rankColorFor(s_lbSelf.rank), "#%d", s_lbSelf.rank);
            ImGui::SameLine(50.0f);
            std::string label = s_lbSelf.name + " (You)";
            ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.45f, 1.0f), "%s", label.c_str());
            std::string scoreStr = formatBoardScore(s_lbSelf.score);
            float tw = ImGui::CalcTextSize(scoreStr.c_str()).x;
            ImGui::SameLine(LEADERBOARD_CARD_WIDTH - tw - 32.0f);
            ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.45f, 1.0f), "%s", scoreStr.c_str());
        }
    }

    ImGui::End();
}

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
// Chat (main menu) — a single app-wide global channel. brainCloud's chat calls all
// require RTT to be enabled (RTT_NOT_ENABLED otherwise); app_enableChatRTT() (app.cpp)
// keeps RTT connected on every path that reaches this screen specifically so this
// works. This is poll-based (explicit fetch after send / on opening the tab), not
// live RTT push — see the summary for what a live-push version would need
// (registerRTTChatCallback + handling the Chat service in the RTT dispatch).
//-----------------------------------------------------------------------------

// Must match a channel Code pre-registered in the portal (App > Design > Messaging >
// Chat Channels) — global ("gl") chat channels aren't created ad hoc by getChannelId,
// they resolve an existing registration or fail with CHAT_UNRECOGNIZED_CHANNEL (40603).
static const char *CHAT_CHANNEL_SUB_ID = "gl";

struct ChatMessage
{
    std::string fromName;
    std::string text;
};

static std::string s_chatChannelId;
static bool s_chatChannelResolving = false;
static bool s_chatChannelReady = false;
static std::vector<ChatMessage> s_chatMessages;
static bool s_chatFetchInFlight = false;
static bool s_chatFetchedOnce = false;
static char s_chatInputBuf[240] = {0};
static bool s_chatSending = false;

static ChatMessage parseChatMessage(const Json::Value &m)
{
    ChatMessage msg;
    msg.fromName = m["from"]["name"].asString();
    if (msg.fromName.empty())
        msg.fromName = "Player";
    msg.text = m["content"]["text"].asString();
    return msg;
}

static void fetchChatMessages()
{
    if (s_chatChannelId.empty() || s_chatFetchInFlight) return;
    s_chatFetchInFlight = true;
    pBCWrapper->getChatService()->getRecentChatMessages(
        s_chatChannelId.c_str(), 30,
        new BCCallback(
            [](const Json::Value &result)
            {
                s_chatFetchInFlight = false;
                s_chatFetchedOnce = true;
                s_chatMessages.clear();
                for (const auto &m : result["data"]["messages"])
                    s_chatMessages.push_back(parseChatMessage(m));
                // Server returns newest-first; flip to oldest-first for natural
                // top-to-bottom reading order.
                std::reverse(s_chatMessages.begin(), s_chatMessages.end());
            },
            [](const std::string &) { s_chatFetchInFlight = false; }));
}

// Backoff after a failed getChannelId, so a persistent failure (bad channel code,
// network hiccup) can't turn into a same-call-every-frame loop — brainCloud's abuse
// detection disables the client after enough repeated failures on one API call
// (reason_code 90200), which is exactly what happened here without this guard.
static long long s_chatChannelRetryAtMs = 0;

// Resolves the shared global channel once RTT is up, then fetches history.
// Safe to call every frame the Chat tab is open — no-ops once resolved, in flight,
// or backing off after a recent failure.
static void ensureChatChannel()
{
    if (s_chatChannelReady || s_chatChannelResolving || !pBCWrapper) return;
    if (!pBCWrapper->getRTTService()->getRTTEnabled())
    {
        app_enableChatRTT(); // should already be on by the time MainMenu is reached; just in case
        return;
    }

    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (nowMs < s_chatChannelRetryAtMs) return;

    s_chatChannelResolving = true;
    pBCWrapper->getChatService()->getChannelId(
        "gl", CHAT_CHANNEL_SUB_ID,
        new BCCallback(
            [](const Json::Value &result)
            {
                s_chatChannelResolving = false;
                s_chatChannelId = result["data"]["channelId"].asString();
                s_chatChannelReady = !s_chatChannelId.empty();
                if (s_chatChannelReady)
                    fetchChatMessages();
            },
            [](const std::string &)
            {
                s_chatChannelResolving = false;
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                s_chatChannelRetryAtMs = now + 5000; // back off 5s before retrying
            }));
}

static void sendChatMessage()
{
    if (s_chatChannelId.empty() || s_chatInputBuf[0] == '\0' || s_chatSending) return;
    s_chatSending = true;
    pBCWrapper->getChatService()->postChatMessageSimple(
        s_chatChannelId.c_str(), s_chatInputBuf, true,
        new BCCallback(
            [](const Json::Value &) { s_chatSending = false; fetchChatMessages(); },
            [](const std::string &) { s_chatSending = false; }));
    s_chatInputBuf[0] = '\0';
}

static void drawChatCard(float x, float y)
{
    ensureChatChannel();

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(LEADERBOARD_CARD_WIDTH, CHAT_CARD_HEIGHT), ImGuiCond_Always);
    ImGui::Begin("Chat", nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize);

    if (!s_chatChannelReady)
    {
        ImGui::TextDisabled(s_chatChannelResolving || !s_chatFetchedOnce ? "Connecting..." : "Chat unavailable.");
    }
    else
    {
        ImGui::BeginChild("chat_scroll", ImVec2(0.0f, -32.0f), true);
        for (const auto &m : s_chatMessages)
        {
            ImGui::TextColored(ImVec4(0.6f, 0.75f, 1.0f, 1.0f), "%s:", m.fromName.c_str());
            ImGui::SameLine();
            ImGui::TextWrapped("%s", m.text.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f)
            ImGui::SetScrollHereY(1.0f); // stick to bottom as new messages arrive
        ImGui::EndChild();

        ImGui::PushItemWidth(-70.0f);
        bool enterPressed = ImGui::InputText("##chatInput", s_chatInputBuf, sizeof(s_chatInputBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        bool sendClicked = ImGui::Button("Send", ImVec2(60.0f, 0.0f));
        if ((enterPressed || sendClicked) && !s_chatSending)
            sendChatMessage();
    }

    ImGui::End();
}

//-----------------------------------------------------------------------------
// Lobby card — protocol/lobby-type/ping-data setup, unchanged functionality,
// relabeled/reordered to match the reference layout (title + win-condition
// tagline up top, captioned dropdowns, "Find / Create Lobby" as the primary CTA).
//-----------------------------------------------------------------------------

static void drawLobbyCard(float x, float y)
{
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(LOBBY_CARD_WIDTH, 0)); // 0 height = auto
    ImGui::Begin("Cursor Party", nullptr,
                 ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize);

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
        drawLeaderboardCard(rightX, y);
    else
        drawChatCard(rightX, y);
}
