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
// File: menuBar.cpp
// Desc: Definition for displaying a menuBar
// Author: David St-Louis
//-----------------------------------------------------------------------------

// App includes
#include "app.h"
#include "globals.h"

// Third party includes
#include "imgui.h"

#include "menuBar.h"

// Draws menu bar GUI and update its logic
void menuBar_update()
{
    extern BrainCloud::BrainCloudWrapper *pBCWrapper;
    // Menu Bar
    ImGui::BeginMainMenuBar();

    // File
    std::string app_text = "brainCloud ";
    if(pBCWrapper && pBCWrapper->getBCClient()->isInitialized()) {
        app_text += pBCWrapper->getBCClient()->getBrainCloudClientVersion();
    }
    if (ImGui::BeginMenu(app_text.c_str()))
    {
        if (ImGui::MenuItem("Exit", "ALT + F4"))
        {
            app_exit();
        }
        ImGui::EndMenu();
    }

    // Theme
    if (ImGui::BeginMenu("Theme"))
    {
        if (ImGui::MenuItem("Classic"))
        {
            theme = 0;
            saveConfigs();
        }
        if (ImGui::MenuItem("Dark"))
        {
            theme = 1;
            saveConfigs();
        }
        if (ImGui::MenuItem("Light"))
        {
            theme = 2;
            saveConfigs();
        }
        ImGui::EndMenu();
    }

    // If user is logged-in, show a log out option
    if (!state.user.id.empty())
    {
        if (ImGui::BeginMenu(state.user.name.c_str()))
        {
            if (ImGui::MenuItem("Log Out"))
            {
                app_logOut();
            }
            ImGui::EndMenu();
        }
    }

    ImGui::EndMainMenuBar();
}
