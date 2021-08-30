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

// C/C++ includes
#include <imgui.h>
#include <stdio.h>

// Login dialog dimensions
#define DIALOG_WIDTH 400.0f
#define DIALOG_HEIGHT 150.0f

// Draws a login dialog and update its logic
void mainMenu_update()
{
    // Main menu window, centered
    {
        ImGui::SetNextWindowPos(ImVec2(
            (float)width / 2.0f - DIALOG_WIDTH / 2.0f,
            (float)height / 2.0f - DIALOG_HEIGHT / 2.0f));
        ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, DIALOG_HEIGHT));
        ImGui::Begin("Main Menu", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize);

        // Protocol choice
        if (ImGui::Combo("Protocol", (int*)&settings.protocol, "UDP\0TCP\0"))
        {
            saveConfigs();
        }

        // Lobby type
        int currentChoice = 0;
        if (settings.lobbyType == "CursorPartyV2") currentChoice = 0;
        else if (settings.lobbyType == "CursorPartyV2Backfill") currentChoice = 1;
        if (ImGui::Combo("Lobby Type", &currentChoice, "Normal\0Backfill\0"))
        {
            switch (currentChoice)
            {
                case 0: settings.lobbyType = "CursorPartyV2"; break;
                case 1: settings.lobbyType = "CursorPartyV2Backfill"; break;
            }
        }

        // Join a game
        if (ImGui::Button("Play") || settings.autoJoin)
        {
            app_play(settings.protocol);
        }

        ImGui::End();
    }
}
