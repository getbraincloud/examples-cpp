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
// File: app.h
// Desc: Interface for main application logic
// Author: David St-Louis
//-----------------------------------------------------------------------------
#pragma once

// brainCloud
#include <braincloud/BrainCloudRelay.h>
#include <braincloud/BrainCloudWrapper.h>

extern std::string appVersion;
extern std::string serverVersion;
extern BrainCloud::BrainCloudWrapper *pBCWrapper;

struct Point;

// Draws the application's GUI and update brainCloud
void app_update();

// Logs out the current user and goes back to login screen
void app_logOut();

// Shutdowns the application
void app_exit();

// Attempt login with the specific username/password
void app_login(const char* username, const char* password);

// Attempt reconnect with saved profile
void app_reconnect();

// Find lobby
void app_play(BrainCloud::eRelayConnectionType protocol);

// Enables RTT so main-menu chat works (idempotent — safe to call any time the
// MainMenu screen is reached; no-ops if RTT is already connected).
void app_enableChatRTT();

// Sends a chat message to everyone in the current lobby, via Lobby service signals.
void app_sendLobbySignal(const std::string &text);

// Fetches this player's own worldwide rank (coverage leaderboard) and shares it via
// the lobby's extra field. Safe to call repeatedly.
void app_fetchWorldwideRank();

// Cancel lobby search or leave lobby. Go back to main menu without logging out.
void app_cancelLobby();

// Cleanly close the game. Go back to main menu but don't log
void app_closeGame();

// End the current match and return all players to the lobby (owner only)
void app_endMatch();

// Ready up and signals RTT service we can start the game
void app_startGame();

// Marks this player queued for a rematch and takes them back to the Lobby screen —
// used by both the Match Summary screen's button and its own auto-timeout.
void app_setRematchReady(bool ready);

// Host-only: starts the next round once everyone has queued for a rematch, or the
// 15s auto-rematch timer elapses. Called once per frame from both lobby_update() and
// matchSummary_update() (whichever screen the host is currently on).
void app_tickRematchGate();

// User changes his player color
void app_changeUserColor(int colorIndex);

// User moved mouse in the play area
void app_mouseMoved(const Point& pos);

// User clicked mouse in the play area
void app_shockwave(const Point& pos);

// Drives coverage/ranking recompute + the host-authoritative match-end + leaderboard-post
// flow. Called once per frame from game_update() while on the Game screen.
void app_tickMatch();
