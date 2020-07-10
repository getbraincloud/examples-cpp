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
// File: login.cpp
// Desc: Definition for displaying a login screen and updating its logic
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
void login_update()
{
    // Login window, centered
    {
        ImGui::SetNextWindowPos(ImVec2(
            (float)width / 2.0f - DIALOG_WIDTH / 2.0f,
            (float)height / 2.0f - DIALOG_HEIGHT / 2.0f));
        ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, DIALOG_HEIGHT));
        ImGui::Begin("Log In", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize);
        ImGui::Text("A new user will be created if it doesn't exist.");
        ImGui::InputText("Username", settings.username, MAX_CREDENTIAL_CHAR);
        ImGui::InputText("Password", settings.password, MAX_CREDENTIAL_CHAR,
            ImGuiInputTextFlags_Password);

        // Greyed out login button if user/pass not yet entered
        auto canLogin = strlen(settings.username) && strlen(settings.password);
        if (!canLogin)
        {
            settings.autoJoin = false;
            ImGui::PushStyleVar(
                ImGuiStyleVar_Alpha,
                ImGui::GetStyle().Alpha * 0.5f);
        }

        // Login button
        if ((ImGui::Button("Login") || settings.autoJoin) && canLogin)
        {
            // Save user/pass locally for next time
            saveConfigs();

            // Attempt login
            app_login(settings.username, settings.password);
        }

        // Restore style for grayed out button
        if (!canLogin)
        {
            ImGui::PopStyleVar();
        }

        ImGui::End();
    }
}
