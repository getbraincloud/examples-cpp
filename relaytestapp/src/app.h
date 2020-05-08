//-----------------------------------------------------------------------------
// Copyright 2018 bitHeads inc.
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

// brainCloud
#include <braincloud/BrainCloudRelay.h>

struct Point;

// Draws the application's GUI and update brainCloud
void app_update();

// Logs out the current user and goes back to login screen
void app_logOut();

// Shutdowns the application
void app_exit();

// Attempt login with the specific username/password
void app_login(const char* username, const char* password);

// Find lobby
void app_play(BrainCloud::eRelayConnectionType protocol);

// Cleanly close the game. Go back to main menu but don't log 
void app_closeGame();

// Ready up and signals RTT service we can start the game
void app_startGame();

// User changes his player color
void app_changeUserColor(int colorIndex);

// User moved mouse in the play area
void app_mouseMoved(const Point& pos);

// User clicked mouse in the play area
void app_shockwave(const Point& pos);
