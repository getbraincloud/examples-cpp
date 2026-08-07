//-----------------------------------------------------------------------------
// Copyright 2021 bitHeads inc.
//-----------------------------------------------------------------------------
// File: matchSummary.cpp
// Desc: Post-match results + rematch-queue screen (BCLOUD-14489). Shows how everyone
//       placed this round and what it did to their leaderboard ranks, then either
//       auto-rematches after MATCH_SUMMARY_REMATCH_MS or as soon as everyone queues up.
//-----------------------------------------------------------------------------

#include "matchSummary.h"
#include "app.h"
#include "globals.h"
#include "leaderboardPanel.h"

#include <imgui.h>
#include <stdio.h>
#include <algorithm>

static constexpr float PANEL_WIDTH = 940.0f;
static constexpr float PANEL_HEIGHT = 640.0f;

static const ImVec4 COLOR_ME(0.35f, 1.0f, 0.45f, 1.0f);
static const ImVec4 COLOR_GOOD(0.45f, 0.95f, 0.55f, 1.0f);
static const ImVec4 COLOR_BEST(1.00f, 0.84f, 0.30f, 1.0f);
static const ImVec4 COLOR_DIM(0.55f, 0.58f, 0.65f, 1.0f);

// A small rounded-rect "pill" with text inside, drawn at the current cursor position and
// advancing it — the same AddRectFilled-behind-text trick used for the "YOU"/"HOST"
// badges elsewhere (lobby.cpp/game.cpp), just wrapped into a helper since this screen
// needs several distinct pill styles side by side.
static void drawPill(const char *text, const ImVec4 &bg, const ImVec4 &fg)
{
    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + textSize.x + 16.0f, p0.y + textSize.y + 8.0f);
    ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, ImColor(bg), 6.0f);
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 8.0f, p0.y + 4.0f));
    ImGui::TextColored(fg, "%s", text);
    ImGui::SetCursorScreenPos(ImVec2(p1.x + 6.0f, p0.y));
}

// Builds the "Lifetime #before->#after · Quarterly #before->#after" tail for whichever
// periods actually improved on one board (points OR coverage) — periods that didn't
// improve are simply omitted, matching the reference mockup (e.g. one player's badge
// only mentions Quarterly because their Lifetime rank didn't move that round).
static std::string periodsText(const LeaderboardPeriodDelta &lifetime, const LeaderboardPeriodDelta &quarterly)
{
    std::string out;
    char buf[64];
    // Plain ASCII only — ImGui's default font atlas doesn't cover the Unicode arrow/dot
    // glyphs this used to use (they rendered as "?" — a missing-glyph fallback, not a bug
    // in the badge logic itself).
    if (lifetime.improved)
    {
        snprintf(buf, sizeof(buf), "Lifetime #%d->#%d", lifetime.rankBefore, lifetime.rankAfter);
        out += buf;
    }
    if (quarterly.improved)
    {
        if (!out.empty()) out += "  |  ";
        snprintf(buf, sizeof(buf), "Quarterly #%d->#%d", quarterly.rankBefore, quarterly.rankAfter);
        out += buf;
    }
    return out;
}

// One player's row: rank/color/name/coverage header line, then a badges line for
// points earned + any leaderboard movement (BCLOUD-14489's "player cards").
static void drawPlayerCard(const MatchResultEntry &entry, float width)
{
    const User *pMember = nullptr;
    for (const auto &m : state.lobby.members)
    {
        if (m.cxId == entry.cxId) { pMember = &m; break; }
    }
    std::string name = pMember ? pMember->name : "?";
    int colorIndex = pMember ? pMember->colorIndex : 0;
    bool isMe = (entry.cxId == state.user.cxId);

    // How many badge lines this card needs, so it gets an explicit height instead of
    // BeginChild's height=0 — inside a scrolling parent that means "fill ALL remaining
    // space", not "auto-fit to content", which is what was making every card after the
    // first one collapse to zero height (present in state.matchResult.entries, just
    // invisible).
    int badgeLines = 1; // "+N pts ..." always shows
    if (!entry.lbDelta.ready)
        badgeLines += 1; // "Updating leaderboards..."
    else
    {
        bool anyPointsUp = entry.lbDelta.pointsLifetime.improved || entry.lbDelta.pointsQuarterly.improved;
        bool anyCoverageBest = entry.lbDelta.coverageLifetime.improved || entry.lbDelta.coverageQuarterly.improved;
        badgeLines += anyPointsUp ? 1 : 0;
        badgeLines += anyCoverageBest ? 1 : 0;
        if (!anyPointsUp && !anyCoverageBest)
            badgeLines += 1; // "No leaderboard rank change this round"
    }
    float lineH = ImGui::GetTextLineHeightWithSpacing();
    float cardHeight = lineH * (2 /* header + coverage caption */ + badgeLines) + 28.0f /* spacing + child padding */;

    ImGui::BeginChild(("##card_" + entry.cxId).c_str(), ImVec2(width, cardHeight), true,
        ImGuiWindowFlags_NoScrollbar);
    if (isMe)
        ImGui::GetWindowDrawList()->AddRect(ImGui::GetWindowPos(),
            ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y),
            ImColor(COLOR_ME), 6.0f, 0, 1.5f);

    // Header: #rank, color dot, name (+YOU), coverage % right-aligned. The dot is drawn
    // directly (not a text glyph) — font-independent, unlike a Unicode "●" character.
    ImGui::TextColored(rankColorFor(entry.rank), "#%d", entry.rank);
    ImGui::SameLine();
    {
        float r = ImGui::GetTextLineHeight() * 0.3f;
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 center(p.x + r, p.y + ImGui::GetTextLineHeight() * 0.5f);
        ImGui::GetWindowDrawList()->AddCircleFilled(center, r, ImColor(getColor(colorIndex % colorCount())));
        ImGui::Dummy(ImVec2(r * 2.0f + 4.0f, ImGui::GetTextLineHeight()));
    }
    ImGui::SameLine();
    ImGui::TextColored(isMe ? COLOR_ME : ImVec4(1, 1, 1, 1), "%s", name.c_str());
    if (isMe)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(YOU)");
    }

    // Coverage %, with a small "COVERAGE" caption stacked underneath — both right-
    // aligned to the same X, matching the mockup's stacked "41% / COVERAGE" block.
    {
        char covBuf[16];
        snprintf(covBuf, sizeof(covBuf), "%.0f%%", entry.coveragePct);
        float blockW = std::max(ImGui::CalcTextSize(covBuf).x, ImGui::CalcTextSize("COVERAGE").x);
        float rightX = width - blockW - 20.0f;
        float headerY = ImGui::GetCursorPosY() - ImGui::GetTextLineHeightWithSpacing();

        ImGui::SetCursorPos(ImVec2(rightX, headerY));
        ImGui::TextColored(isMe ? COLOR_ME : ImVec4(1, 1, 1, 1), "%s", covBuf);
        ImGui::SetCursorPos(ImVec2(rightX, headerY + ImGui::GetTextLineHeightWithSpacing()));
        ImGui::TextDisabled("COVERAGE");
    }

    ImGui::Spacing();

    // Badges row: points earned, then leaderboard movement (or "no change").
    char ptsBuf[64];
    snprintf(ptsBuf, sizeof(ptsBuf), "+%d pts", entry.beaten + 1);
    drawPill(ptsBuf, ImVec4(0.20f, 0.24f, 0.34f, 1.0f), ImVec4(0.75f, 0.82f, 1.0f, 1.0f));
    char subBuf[64];
    snprintf(subBuf, sizeof(subBuf), "%d beaten + 1 for playing", entry.beaten);
    ImGui::TextDisabled("%s", subBuf);

    if (!entry.lbDelta.ready)
    {
        ImGui::TextDisabled("Updating leaderboards...");
    }
    else
    {
        bool anyPointsUp = entry.lbDelta.pointsLifetime.improved || entry.lbDelta.pointsQuarterly.improved;
        bool anyCoverageBest = entry.lbDelta.coverageLifetime.improved || entry.lbDelta.coverageQuarterly.improved;

        if (anyPointsUp)
        {
            std::string tail = periodsText(entry.lbDelta.pointsLifetime, entry.lbDelta.pointsQuarterly);
            std::string label = "^ Rank up - Opponents Beaten  " + tail;
            drawPill(label.c_str(), ImVec4(0.16f, 0.32f, 0.20f, 1.0f), COLOR_GOOD);
            ImGui::NewLine();
        }
        if (anyCoverageBest)
        {
            std::string tail = periodsText(entry.lbDelta.coverageLifetime, entry.lbDelta.coverageQuarterly);
            std::string label = "* Personal best - Coverage %  " + tail;
            drawPill(label.c_str(), ImVec4(0.34f, 0.28f, 0.10f, 1.0f), COLOR_BEST);
            ImGui::NewLine();
        }
        if (!anyPointsUp && !anyCoverageBest)
            drawPill("No leaderboard rank change this round", ImVec4(0.20f, 0.20f, 0.24f, 1.0f), COLOR_DIM);
    }

    ImGui::EndChild();
}

void matchSummary_update()
{
    app_tickRematchGate();

    // Per-player auto-queue: if this player hasn't clicked "Queue for Rematch" themselves
    // by MATCH_SUMMARY_REMATCH_MS, queue them automatically and send them back to the
    // Lobby (app_setRematchReady handles the screen transition) — matches the "if players
    // do nothing, auto rematch" requirement without waiting on anyone else.
    if (isCursorPartyLobby(settings.lobbyType) && !state.user.isReady)
    {
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - state.matchSummaryArrivalTime).count();
        if (elapsedMs >= MATCH_SUMMARY_REMATCH_MS)
            app_setRematchReady(true);
    }

    // PANEL_WIDTH/HEIGHT are a ceiling, not a fixed size — shrink to fit whenever the
    // window is smaller than that (e.g. multiple instances tiled on one screen), floored
    // so the layout doesn't collapse into something unreadable.
    const float MARGIN = 24.0f;
    float panelW = std::min(PANEL_WIDTH, std::max(480.0f, (float)width - MARGIN * 2.0f));
    float panelH = std::min(PANEL_HEIGHT, std::max(360.0f, (float)height - ImGui::GetFrameHeight() - MARGIN * 2.0f));

    ImGui::SetNextWindowPos(ImVec2((float)width / 2.0f, (float)height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);
    ImGui::Begin("##match_summary", nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar);

    ImGui::SetWindowFontScale(1.4f);
    ImGui::Text("Match Summary");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextDisabled("Lobby %s - %d Players", state.lobby.lobbyId.c_str(), (int)state.lobby.members.size());
    ImGui::Spacing();

    const auto &entries = state.matchResult.entries;
    const MatchResultEntry *winner = entries.empty() ? nullptr : &entries.front();

    // Winner banner
    if (winner)
    {
        const User *pWinner = nullptr;
        for (const auto &m : state.lobby.members)
            if (m.cxId == winner->cxId) { pWinner = &m; break; }
        std::string winnerName = pWinner ? pWinner->name : "?";

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + ImGui::GetContentRegionAvail().x, p0.y + 40.0f);
        ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, ImColor(ImVec4(0.30f, 0.24f, 0.08f, 1.0f)), 6.0f);
        ImGui::GetWindowDrawList()->AddRect(p0, p1, ImColor(COLOR_BEST), 6.0f, 0, 1.5f);
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 14.0f, p0.y + 10.0f));
        ImGui::TextColored(COLOR_BEST, "%s wins the round, covering %.0f%% of the board.",
            winnerName.c_str(), winner->coveragePct);
        ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + 12.0f));
    }
    else
    {
        ImGui::TextDisabled("Waiting for results...");
    }

    ImGui::TextDisabled("RANK / PLAYER");
    ImGui::SameLine(panelW - 200.0f);
    ImGui::TextDisabled("LEADERBOARD RESULT");
    ImGui::Separator();

    float listHeight = panelH - ImGui::GetCursorPosY() - 90.0f;
    ImGui::BeginChild("##summary_scroll", ImVec2(0.0f, std::max(80.0f, listHeight)), false);
    for (const auto &entry : entries)
    {
        drawPlayerCard(entry, ImGui::GetContentRegionAvail().x - 4.0f);
        ImGui::Spacing();
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Countdown + actions
    long long remainingSec = 0;
    if (isCursorPartyLobby(settings.lobbyType))
    {
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - state.matchSummaryArrivalTime).count();
        long long remainingMs = MATCH_SUMMARY_REMATCH_MS - elapsedMs;
        if (remainingMs < 0) remainingMs = 0;
        remainingSec = (remainingMs + 999) / 1000;
    }
    ImGui::TextDisabled("Next Round: %lld:%02lld", remainingSec / 60, remainingSec % 60);

    int readyCount = 0;
    for (const auto &m : state.lobby.members)
        if (m.isReady) ++readyCount;

    bool iAmReady = state.user.isReady;
    char rematchLabel[64];
    snprintf(rematchLabel, sizeof(rematchLabel), "%s  %d/%d",
        iAmReady ? "Queued for Rematch" : "Queue for Rematch",
        readyCount, (int)state.lobby.members.size());

    if (iAmReady) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.30f, 1.0f));
    if (ImGui::Button(rematchLabel, ImVec2(panelW * 0.6f, 36.0f)))
        app_setRematchReady(!iAmReady);
    if (iAmReady) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (ImGui::Button("Main Menu", ImVec2(-1.0f, 36.0f)))
        app_cancelLobby();

    ImGui::End();
}
