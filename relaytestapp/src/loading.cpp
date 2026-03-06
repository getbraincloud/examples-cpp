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
// File: loading.cpp
// Desc: Definition for displaying and updating a loading screen
// Author: David St-Louis
//-----------------------------------------------------------------------------

// App includes
#include "app.h"
#include "globals.h"

// Third party includes
#include <imgui.h>

// C/C++ includes
#include <chrono>
#include <stdio.h>

// Loading dialog dimensions
#define DIALOG_WIDTH 280.0f
#define DIALOG_HEIGHT 155.0f

// Text to be displayed in the dialog.
std::string loading_text = "Loading...";

// Optional secondary status line updated by lobby events.
std::string loading_status = "";

// Timer start point, reset when entering a lobby/loading flow.
static std::chrono::steady_clock::time_point loading_timer_start =
    std::chrono::steady_clock::now();

// Reset the elapsed timer and clear status.
void loading_reset_timer()
{
    loading_timer_start = std::chrono::steady_clock::now();
    loading_status = "";
}

// Draws a loading dialog
void loading_update()
{
    bool canCancel = (state.screenState == ScreenState::JoiningLobby ||
                      state.screenState == ScreenState::Starting);

    // Center it
    ImGui::SetNextWindowPos(ImVec2(
        (float)width / 2.0f - DIALOG_WIDTH / 2.0f,
        (float)height / 2.0f - DIALOG_HEIGHT / 2.0f));
    ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, DIALOG_HEIGHT));
    ImGui::Begin("foo", nullptr,
                 ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoTitleBar);

    auto centerText = [](const char* text) {
        float w = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((DIALOG_WIDTH - w) / 2.0f);
        ImGui::Text("%s", text);
    };
    auto centerTextDisabled = [](const char* text) {
        float w = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((DIALOG_WIDTH - w) / 2.0f);
        ImGui::TextDisabled("%s", text);
    };

    if (canCancel)
    {
        // Elapsed time since entering the lobby/loading flow
        auto elapsed = std::chrono::steady_clock::now() - loading_timer_start;
        int totalSecs = (int)std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        char timerBuf[16];
        snprintf(timerBuf, sizeof(timerBuf), "%d:%02d", totalSecs / 60, totalSecs % 60);

        centerText(loading_text.c_str());
        centerTextDisabled(timerBuf);

        if (!loading_status.empty())
        {
            ImGui::Spacing();
            centerTextDisabled(loading_status.c_str());
        }

        // Push cancel button to the bottom center
        float buttonWidth = 120.0f;
        float buttonHeight = ImGui::GetFrameHeightWithSpacing();
        float remainingY = ImGui::GetContentRegionAvail().y - buttonHeight;
        if (remainingY > 0)
            ImGui::Dummy(ImVec2(0, remainingY));
        ImGui::SetCursorPosX((DIALOG_WIDTH - buttonWidth) / 2.0f);
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
        {
            app_cancelLobby();
        }
    }
    else
    {
        centerText(loading_text.c_str());
    }

    ImGui::End();
}
