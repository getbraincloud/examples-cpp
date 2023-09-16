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
// File: chat.cpp
// Desc: Definition for displaying and updating the chat screen with its
//       multiple panels.
// Author: David St-Louis
//-----------------------------------------------------------------------------

// App includes
#include "globals.h"
#include "app.h"
#include "menuBar.h"

// Thirdparty includes
#include "imgui.h"

// Panels dimension defines
#define MENU_BAR_HEIGHT 19.0f
#define LEFT_SIDE_PANEL_WIDTH 200.0f
#define RIGHT_SIDE_PANEL_WIDTH 200.0f
#define TEXT_BAR_HEIGHT 40.0f

#define SEND_BUFFER_SIZE 2048
char sendBuffer[SEND_BUFFER_SIZE] = { '\0' };

// Because of the way ImGui works, we need to keep track if we want
// to regain focus to the textbox on the next frame.
bool giveTextBarFocus = false;

// Draws the application's chat screen GUI and updates its logic
void chat_update()
{
    // Main menu
    menuBar_update();

    // Left panel
    { //  global channels
        ImGui::SetNextWindowPos(ImVec2(0.0f, MENU_BAR_HEIGHT));
        ImGui::SetNextWindowSize(ImVec2(
            LEFT_SIDE_PANEL_WIDTH,
            ((float)height - MENU_BAR_HEIGHT) / 2.0f));
        ImGui::Begin("Channels", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize);
        for (auto& pChannel : state.chatData.globalChannels)
        {
            bool isSelected = pChannel == state.chatData.pActiveChannel;
            ImGui::Selectable(pChannel->name.c_str(), &isSelected);
            if (isSelected)
            {
                state.chatData.pActiveChannel = pChannel;
            }
        }
        ImGui::End();
    }
    { // Private groups
        ImGui::SetNextWindowPos(ImVec2(
            0.0f, 
            MENU_BAR_HEIGHT + ((float)height - MENU_BAR_HEIGHT) / 2.0f));
        ImGui::SetNextWindowSize(ImVec2(
            LEFT_SIDE_PANEL_WIDTH,
            ((float)height - MENU_BAR_HEIGHT) / 2.0f));
        ImGui::Begin("Private Groups", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize);
        for (auto& pChannel : state.chatData.groups)
        {
            bool isSelected = pChannel == state.chatData.pActiveChannel;
            ImGui::Selectable(pChannel->name.c_str(), &isSelected);
            if (isSelected)
            {
                state.chatData.pActiveChannel = pChannel;
            }
        }
        ImGui::End();
    }

    // Right panel
    { // Group's members
        ImGui::SetNextWindowPos(ImVec2(
            (float)width - RIGHT_SIDE_PANEL_WIDTH,
            MENU_BAR_HEIGHT));
        ImGui::SetNextWindowSize(ImVec2(
            RIGHT_SIDE_PANEL_WIDTH,
            (float)height - MENU_BAR_HEIGHT));
        ImGui::Begin("Group Users", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize);
        if (state.chatData.pActiveChannel)
        {
            for (auto& pUser : state.chatData.pActiveChannel->members)
            {
                auto zsUserName = pUser->name.c_str();
                if (pUser->online)
                {
                    // Draw online users green
                    ImGui::TextColored(ImVec4(.5f, 1, .5f, 1), 
                        "%s", zsUserName);
                }
                else
                {
                    // Draw offline users grayed out a bit
                    ImGui::TextColored(ImVec4(.6f, .6f, .6f, 1),
                        "%s", zsUserName);
                }
            }
        }
        ImGui::End();
    }

    // Center view
    { // Chat message history
        if (state.chatData.pActiveChannel)
        {
            ImGui::SetNextWindowPos(ImVec2(
                LEFT_SIDE_PANEL_WIDTH,
                MENU_BAR_HEIGHT));
            ImGui::SetNextWindowSize(ImVec2(
                (float)width - LEFT_SIDE_PANEL_WIDTH - RIGHT_SIDE_PANEL_WIDTH,
                (float)height - MENU_BAR_HEIGHT - TEXT_BAR_HEIGHT));
            
            // Having a unique window id on the window based on the
            // channel id allows it to remember the scroll bar position
            // for each.
            auto feedId = "Messages Feed##" + state.chatData.pActiveChannel->id;
            ImGui::Begin(feedId.c_str(), nullptr,
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize);
            for (auto& pMessage : state.chatData.pActiveChannel->messages)
            {
                ImGui::Spacing();
                
                // If our user, show our name in green
                ImVec4 nameColor = (pMessage->user.id == state.user.id) 
                    ? ImVec4(.2f, 1, 0.75f, 1) 
                    : ImVec4(.2f, .6f, 0.75f, 1);
                ImGui::TextColored(
                    nameColor,
                    "%s",
                    (pMessage->user.name + ":").c_str());
                
                ImGui::SameLine();
                ImGui::Text("%s", pMessage->text.c_str());
            }

            // Force the window to always be scrolled to the bottom
            ImGui::SetScrollY(ImGui::GetScrollMaxY());
            ImGui::End();
        }
    }

    // Text bar
    {
        ImGui::SetNextWindowPos(ImVec2(
            LEFT_SIDE_PANEL_WIDTH,
            (float)height - TEXT_BAR_HEIGHT));
        ImGui::SetNextWindowSize(ImVec2(
            (float)width - LEFT_SIDE_PANEL_WIDTH - RIGHT_SIDE_PANEL_WIDTH,
            TEXT_BAR_HEIGHT));
        ImGui::Begin("Text Bar", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoTitleBar);
        
        // Textbox
        bool enter = ImGui::InputText("Label", sendBuffer, SEND_BUFFER_SIZE,
            ImGuiInputTextFlags_EnterReturnsTrue);
        if (giveTextBarFocus)
        {
            giveTextBarFocus = false;
            ImGui::SetKeyboardFocusHere();
        }

        // Send button
        ImGui::SameLine();
        if (ImGui::Button("Send") || enter)
        {
            if (strlen(sendBuffer) > 0)
            {
                app_sendMessage(sendBuffer);
                sendBuffer[0] = '\0';
                giveTextBarFocus = true;
            }
        }

        ImGui::End();
    }
}
