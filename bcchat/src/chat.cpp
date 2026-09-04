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
#include "mediaPreview.h"
#include "emoji.h"

// Thirdparty includes
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <ctime>

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

namespace
{
    std::string formatMessageTime(uint64_t milliseconds)
    {
        std::time_t value = static_cast<std::time_t>(
            milliseconds > 100000000000ULL ? milliseconds / 1000ULL : milliseconds);
        std::tm localTime = {};
#if defined(_WIN32)
        localtime_s(&localTime, &value);
#else
        localtime_r(&value, &localTime);
#endif
        char text[32] = {};
        std::strftime(text, sizeof(text), "%Y-%m-%d %I:%M %p", &localTime);
        std::string result = text;
        if (result.size() > 11 && result[11] == '0') result.erase(11, 1);
        return result;
    }

    bool isMediaOnlyMessage(const std::string& text, const std::vector<std::string>& urls)
    {
        if (urls.empty()) return false;
        std::string remainder = text;
        for (const std::string& url : urls)
        {
            if (!mediaPreview_isDirectMediaUrl(url)) return false;
            size_t position = remainder.find(url);
            if (position != std::string::npos) remainder.erase(position, url.size());
        }
        return std::all_of(remainder.begin(), remainder.end(), [](unsigned char character)
        {
            return std::isspace(character) != 0;
        });
    }
}

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

            // Keep following new messages only while the user is already at
            // the bottom. Once they scroll up, preserve their reading position.
            const float scrollBottomThreshold = 2.0f;
            bool followLatest = ImGui::GetScrollY() >=
                ImGui::GetScrollMaxY() - scrollBottomThreshold;

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
                
                std::vector<std::string> messageUrls = mediaPreview_findUrls(pMessage->text);
                bool mediaOnly = isMediaOnlyMessage(pMessage->text, messageUrls);
                ImGui::SameLine();
                if (mediaOnly)
                    ImGui::TextDisabled("%s", formatMessageTime(pMessage->date).c_str());
                else
                {
                    std::string displayText = emoji_expandShortcodes(pMessage->text);
                    ImGui::TextWrapped("%s", displayText.c_str());
                }

                // Attachments and links are relayed as URLs in the
                // message text. Render each one as an inline rich preview.
                ImGui::PushID(pMessage->msgId.c_str());
                for (const std::string& url : messageUrls)
                    mediaPreview_draw(url);
                ImGui::PopID();
            }

            if (followLatest)
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
