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
// File: loading.cpp
// Desc: Definition for displaying and updating a loading screen
// Author: David St-Louis
//-----------------------------------------------------------------------------

// App includes
#include "globals.h"

// Third party includes
#include "imgui.h"

// Loading dialog dimensions
#define DIALOG_WIDTH 250.0f
#define DIALOG_HEIGHT 100.0f

// Text to be displayed inthe dialog.
std::string loading_text = "Loading...";

// Draws a loading dialog
void loading_update()
{
    // Center it
    ImGui::SetNextWindowPos(ImVec2(
        (float)width / 2.0f - DIALOG_WIDTH / 2.0f,
        (float)height / 2.0f - DIALOG_HEIGHT / 2.0f));
    ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, DIALOG_HEIGHT));
    ImGui::Begin("Name", nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar);
    ImGui::Text("%s", loading_text.c_str());
    ImGui::End();
}
