//-----------------------------------------------------------------------------
// File: leaderboardPanel.h
// Desc: Shared leaderboard viewer (top 5 + "you" row, board/period toggles) — used
//       by both the main menu and the lobby's Leaderboards tab.
//-----------------------------------------------------------------------------
#pragma once

#include <imgui.h>

// Draws the leaderboard panel (board-type + period toggles, top-5 + "you" row) at
// the given rect. windowId must be unique per call site.
void drawLeaderboardPanel(const char *windowId, float x, float y, float w, float h);

// Gold/silver/bronze/white — shared with the lobby member list's rank display.
ImVec4 rankColorFor(int rank);
