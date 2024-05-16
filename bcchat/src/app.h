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

// Draws the application's GUI and update brainCloud
void app_update();

// Logs out the current user and goes back to login screen
void app_logOut();

// Shutdowns the application
void app_exit();

// Attempt reconnect with saved profile
void app_reconnect();

// Attempt login with the specific username/password
void app_login(const char* username, const char* password);

// Submit user name to brainCloud to be assosiated with the current user
void app_submitName(const char* firstName, const char* lastName);

// Sends message to the active channel
void app_sendMessage(const char* message);
