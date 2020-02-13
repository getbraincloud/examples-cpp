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
// File: askForName.cpp
// Desc: Definition for displaying and updating the AskForName dialog
// Author: David St-Louis
//-----------------------------------------------------------------------------

// App includes
#include "globals.h"
#include "app.h"
#include "menuBar.h"

// Third party libs
#include "imgui.h"

// Loading dialog dimensions
#define DIALOG_WIDTH 400.0f
#define DIALOG_HEIGHT 350.0f

// For name fields
#define MAX_NAME_CHAR 32
char firstName[MAX_NAME_CHAR] = { '\0' };
char lastName[MAX_NAME_CHAR] = { '\0' };

// Draws dialog GUI and update it's logic
void askForName_update()
{
    // Draw menu bar
    menuBar_update();

    // Login window
    {
        ImGui::SetNextWindowPos(ImVec2(
            (float)width / 2.0f - DIALOG_WIDTH / 2.0f,
            (float)height / 2.0f - DIALOG_HEIGHT / 2.0f));
        ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, DIALOG_HEIGHT));
        ImGui::Begin("Enter Name", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize);
        ImGui::Text("Please enter your full name.");
        ImGui::InputText("First name", firstName, MAX_NAME_CHAR);
        ImGui::InputText("Last name", lastName, MAX_NAME_CHAR);

        // Greyed out login button if first/last not yet entered
        auto canSubmit = strlen(firstName) && strlen(lastName);
        if (!canSubmit)
        {
            //ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(
                ImGuiStyleVar_Alpha,
                ImGui::GetStyle().Alpha * 0.5f);
        }

        // Login button
        if (ImGui::Button("Submit") && canSubmit)
        {
            // Attempt login
            app_submitName(firstName, lastName);
        }

        // Restore style for grayed out button
        if (!canSubmit)
        {
            ImGui::PopStyleVar();
        }

        ImGui::End();
    }
}
