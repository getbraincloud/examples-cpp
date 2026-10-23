//-----------------------------------------------------------------------------
// File: leaderboardPanel.cpp
// Desc: Shared leaderboard viewer — see leaderboardPanel.h. Top 5 + "you" row,
//       toggleable between the two boards this app posts to (points / coverage)
//       and Lifetime vs Quarterly.
//-----------------------------------------------------------------------------

#include "leaderboardPanel.h"
#include "app.h"
#include "globals.h"
#include "BCCallback.h"

#include <stdio.h>

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

ImVec4 rankColorFor(int rank)
{
    if (rank == 1) return ImVec4(1.00f, 0.84f, 0.00f, 1.0f); // gold
    if (rank == 2) return ImVec4(0.75f, 0.75f, 0.75f, 1.0f); // silver
    if (rank == 3) return ImVec4(0.80f, 0.50f, 0.20f, 1.0f); // bronze
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

void drawLeaderboardPanel(const char *windowId, float x, float y, float w, float h)
{
    fetchLeaderboardIfNeeded();

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    ImGui::Begin(windowId, nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar);

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
            ImGui::SameLine(w - tw - 32.0f);
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
            ImGui::SameLine(w - tw - 32.0f);
            ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.45f, 1.0f), "%s", scoreStr.c_str());
        }
    }

    ImGui::End();
}
