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

#include "mainMenu.h"

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
        if (ImGui::Combo("Protocol", (int *)&settings.protocol, "UDP\0TCP\0WS\0WSS\0"))
        {
            saveConfigs();
        }

        // Lobby type
        static const char *LOBBY_TYPE_LABELS =
            "CursorParty\0"
            "CursorPartyEdgeGap\0"
            "CursorPartyGameLift\0"
            "CursorPartyI3D\0"
            "CursorPartyV2\0"
            "CursorPartyV2_Ire\0"
            "CursorPartyV2_NoRoomServer\0"
            "CursorPartyV2_RoomServer\0"
            "CursorPartyV2Backfill\0"
            "CursorPartyV2DisbandOnStart\0"
            "CursorPartyV2LongLive\0"
            "SinglePlayerServerTest\0"
            "TeamCursorPartyV2\0"
            "TeamCursorPartyV2Backfill\0"
            "\0";
        static const char *LOBBY_TYPE_IDS[] = {
            "CursorParty",
            "CursorPartyEdgeGap",
            "CursorPartyGameLift",
            "CursorPartyI3D",
            "CursorPartyV2",
            "CursorPartyV2_Ire",
            "CursorPartyV2_NoRoomServer",
            "CursorPartyV2_RoomServer",
            "CursorPartyV2Backfill",
            "CursorPartyV2DisbandOnStart",
            "CursorPartyV2LongLive",
            "SinglePlayerServerTest",
            "TeamCursorPartyV2",
            "TeamCursorPartyV2Backfill",
        };
        static const int LOBBY_TYPE_COUNT = sizeof(LOBBY_TYPE_IDS) / sizeof(LOBBY_TYPE_IDS[0]);
        int currentChoice = 0; // default: CursorParty
        for (int i = 0; i < LOBBY_TYPE_COUNT; ++i)
        {
            if (settings.lobbyType == LOBBY_TYPE_IDS[i])
            {
                currentChoice = i;
                break;
            }
        }
        if (ImGui::Combo("Lobby Type", &currentChoice, LOBBY_TYPE_LABELS))
        {
            settings.lobbyType = LOBBY_TYPE_IDS[currentChoice];
            // Auto-set team code based on lobby type
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

        // Team selection (only for Team lobby types)
        if (settings.lobbyType.find("Team") == 0)
        {
            int teamChoice = (settings.teamCode == "beta") ? 1 : 0;
            if (ImGui::Combo("Team", &teamChoice, "Alpha\0Beta\0\0"))
            {
                settings.teamCode = (teamChoice == 0) ? "alpha" : "beta";
                saveConfigs();
            }
        }

        // Join a game
        if (ImGui::Button("Play") || settings.autoJoin)
        {
            app_play(settings.protocol);
        }

        // Right-aligned item separator
        float rightAlignWidth = ImGui::GetContentRegionAvail().x - 350;
        ImGui::Dummy(ImVec2(rightAlignWidth, 0.0f));
        ImGui::SameLine();

        std::string app_text = "Client: ";
        if (pBCWrapper && pBCWrapper->getBCClient()->isInitialized())
        {
            app_text += pBCWrapper->getBCClient()->getBrainCloudClientVersion();
        }

        if (pBCWrapper && pBCWrapper->getBCClient()->isInitialized())
        {
            app_text += " Server: " + serverVersion;
        }

        app_text += " Project: ";
        app_text += VERSION;

        ImGui::MenuItem(app_text.c_str());

        ImGui::End();
    }
}
