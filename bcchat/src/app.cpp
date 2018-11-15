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
// File: app.cpp
// Desc: Main application logic
// Author: David St-Louis
//-----------------------------------------------------------------------------

// App includes
#include "app.h"
#include "globals.h"
#include "loading.h"
#include "chat.h"
#include "login.h"
#include "askForName.h"
#include "BCCallback.h"

// Third party includes
#include <braincloud/BrainCloudWrapper.h>
#include <braincloud/IRTTCallback.h>
#include <imgui.h>

// C/C++ includes
#include <stdlib.h>

// Prototypes for private functions
void initBC();
void handlePlayerState(const Json::Value& result);
void onLoggedIn();
void onRTTConnected();
void onReceivedSubscribedChannels(const Json::Value& result);
void connectToChannel(const Json::Value& channel);
void onChannelConnected(const Json::Value& jsonChannel, 
                        const Json::Value& result);
void extractMessageHistory(ChannelRef pChannel, 
                           const Json::Value& result);
void onReceivedtMyGroups(const Json::Value& result);
void loadGroup(const Json::Value& jsonGroup);
void onGroupConnected(ChannelRef pChannel, 
                      const Json::Value& jsonGroup, 
                      const Json::Value& result);
void onGroupMembersFetched(ChannelRef pChannel,
                           const std::string& groupId,
                           const Json::Value& result);
ChannelRef getChannelById(const std::string& id);
void onIncomingMessage(const Json::Value& jsonMessage);
void onRemoveMessage(const Json::Value& jsonMessage);
void onUpdateMessage(const Json::Value& jsonMessage);
void onPresenceUpdate(const Json::Value& jsonPresence);
void dieWithMessage(const std::string& message);
void uninitBC();
void resetState();

// brainCloud RTT Connection callbacks
class RTTConnectCallback final : public BrainCloud::IRTTConnectCallback
{
public:
    void rttConnectSuccess() override
    {
        onRTTConnected();
    }

    void rttConnectFailure(const std::string& errorMessage) override
    {
        dieWithMessage("Disconnected from RTT:\n" + errorMessage);
    }
};

// brainCloud RTT callbacks
class RTTCallback final : public BrainCloud::IRTTCallback
{
public:
    void rttCallback(const Json::Value& eventJson) override
    {
        auto service = eventJson["service"];
        auto operation = eventJson["operation"];

        if (service == BrainCloud::ServiceName::Chat.getValue())
        {
            if (operation == "INCOMING")
            {
                onIncomingMessage(eventJson["data"]);
            }
            else if (operation == "DELETE")
            {
                onRemoveMessage(eventJson["data"]);
            }
            else if (operation == "UPDATE")
            {
                onUpdateMessage(eventJson["data"]);
            }
        }
        else if (service == BrainCloud::ServiceName::Presence.getValue())
        {
            if (operation == "INCOMING")
            {
                onPresenceUpdate(eventJson["data"]);
            }
        }
    }
};

//-----------------------------------------------------------------------------
// Privates
//-----------------------------------------------------------------------------
BrainCloud::BrainCloudWrapper *pBCWrapper = nullptr;
std::string errorMessage;

RTTConnectCallback bcRTTConnectCallback;
RTTCallback bcRTTCallback;

// Initialize brainCloud
void initBC()
{
    if (!pBCWrapper)
    {
        pBCWrapper = new BrainCloud::BrainCloudWrapper("BCChat");
    }
    pBCWrapper->initialize(BRAINCLOUD_SERVER_URL, 
                           BRAINCLOUD_APP_SECRET, 
                           BRAINCLOUD_APP_ID, 
                           VERSION, 
                           "bitHeads inc.", 
                           "BCChat");
    pBCWrapper->getBCClient()->enableLogging(true);
}

// User authenticated, handle the result
void handlePlayerState(const Json::Value& result)
{
    state.user.id = result["data"]["profileId"].asString();
    state.user.pic = result["data"]["pictureUrl"].asString();

    // If no username is set for this user, ask for it
    const auto& userName = result["data"]["playerName"].asString();
    if (userName.empty())
    {
        state.screenState = ScreenState::AskForName;
    }
    else
    {
        state.user.name = userName;
        onLoggedIn();
    }
}

// User fully logged in. Enable RTT and listen for chat messages
void onLoggedIn()
{
    pBCWrapper->getBCClient()->registerRTTChatCallback(&bcRTTCallback);
    pBCWrapper->getBCClient()->registerRTTPresenceCallback(&bcRTTCallback);
    pBCWrapper->getBCClient()->enableRTT(&bcRTTConnectCallback, true);
}

// RTT connected. Go to main chat screen and fetch channels data.
void onRTTConnected()
{
    // Connect to global channels
    pBCWrapper->getChatService()->getSubscribedChannels("gl", new BCCallback(
        [](const Json::Value& result) // Success
        {
            onReceivedSubscribedChannels(result);
        },
        [](const std::string& status_message) // Error
        {
            dieWithMessage("Failed to get global channels:\n" + status_message);
        })
    );

    // Fetch private groups
    pBCWrapper->getGroupService()->getMyGroups(new BCCallback(
        [](const Json::Value& result) // Success
        {
            onReceivedtMyGroups(result);
        },
        [](const std::string& status_message) // Error
        {
            dieWithMessage("Failed to retrieve my groups:\n" + status_message);
        })
    );

    // Go to chat screen even if we haven't populated the state.
    // It will fill itself as things come in.
    state.screenState = ScreenState::Chat;
}

// Received global channels, now attempt to connect to all of them
void onReceivedSubscribedChannels(const Json::Value& result)
{
    const auto& jsonChannels = result["data"]["channels"];
    for (int i = 0; i < (int)jsonChannels.size(); ++i)
    {
        connectToChannel(jsonChannels[i]);
    }
}

// Connect to a channel, then add it to our app data
void connectToChannel(const Json::Value& channel)
{
    pBCWrapper->getChatService()->channelConnect(
        channel["id"].asString(), 
        MAX_HISTORY, 
        new BCCallback(
        [=](const Json::Value& result) // Success
        {
            onChannelConnected(channel, result);
        },
        [](const std::string& status_message) // Error
        {
            dieWithMessage("Failed to connect to channel:\n" + status_message);
        })
    );
}

// Connected to channel. Add it to our app data
void onChannelConnected(const Json::Value& jsonChannel, 
                        const Json::Value& result)
{
    auto pChannel = std::make_shared<Channel>();
    pChannel->id = jsonChannel["id"].asString();
    pChannel->name = jsonChannel["name"].asString();
    extractMessageHistory(pChannel, result);

    // Insert it sorted by name
    int i = 0;
    for (; i < (int)state.chatData.globalChannels.size(); ++i)
    {
        auto& pOtherChannel = state.chatData.globalChannels[i];
        if (pChannel->name < pOtherChannel->name)
        {
            break;
        }
    }
    state.chatData.globalChannels.insert(
        state.chatData.globalChannels.begin() + i, pChannel);
}

// Extract messages from incoming "result" json, and append them to pChannel
void extractMessageHistory(ChannelRef pChannel, 
                           const Json::Value& result)
{
    const auto& jsonMessages = result["data"]["messages"];
    for (int i = 0; i < (int)jsonMessages.size(); ++i)
    {
        const auto& jsonMessage = jsonMessages[i];
        auto pMessage = std::make_shared<Message>();
        pMessage->msgId = jsonMessage["msgId"].asString();
        pMessage->text = jsonMessage["content"]["text"].asString();
        pMessage->user.id = jsonMessage["from"]["id"].asString();
        pMessage->user.name = jsonMessage["from"]["name"].asString();
        pMessage->user.pic = jsonMessage["from"]["pic"].asString();
        pMessage->date = jsonMessage["date"].asUInt64();
        pMessage->ver = jsonMessage["ver"].asInt();
        pChannel->messages.push_back(pMessage);
    }
}

// Received private groups we are part of. Load them.
void onReceivedtMyGroups(const Json::Value& result)
{
    const auto& jsonGroups = result["data"]["groups"];
    for (int i = 0; i < (int)jsonGroups.size(); ++i)
    {
        loadGroup(jsonGroups[i]);
    }
}

// Load a private group.
void loadGroup(const Json::Value& jsonGroup)
{
    // Get channel id for this group
    pBCWrapper->getChatService()->getChannelId(
        "gr",
        jsonGroup["groupId"].asString(),
        new BCCallback(
        [=](const Json::Value& result) // Success
        {
            auto pChannel = std::make_shared<Channel>();
            pChannel->id = result["data"]["channelId"].asString();
            pChannel->name = jsonGroup["name"].asString();

            pBCWrapper->getChatService()->channelConnect(
                pChannel->id,
                MAX_HISTORY,
                new BCCallback(
                [=](const Json::Value& result) // Success
                {
                    onGroupConnected(pChannel, jsonGroup, result);
                },
                [](const std::string& status_message) // Error
                {
                    dieWithMessage("Failed to connect to group:\n" + 
                        status_message);
                })
            );
        },
        [=](const std::string& status_message) // Error
        {
            dieWithMessage("Failed to connect to group: " + 
                jsonGroup["name"].asString() + "\n" + status_message);
        })
    );
}

// Connected to private group. Attempt connect to it and fetch members.
void onGroupConnected(ChannelRef pChannel, 
                      const Json::Value& jsonGroup, 
                      const Json::Value& result)
{
    extractMessageHistory(pChannel, result);

    // Insert it sorted by name
    int i = 0;
    for (; i < (int)state.chatData.groups.size(); ++i)
    {
        auto& pOtherChannel = state.chatData.groups[i];
        if (pChannel->name < pOtherChannel->name)
        {
            break;
        }
    }
    state.chatData.groups.insert(state.chatData.groups.begin() + i, pChannel);

    // Fetch members
    const auto& groupId = jsonGroup["groupId"].asString();
    pBCWrapper->getGroupService()->readGroupMembers(groupId.c_str(), new BCCallback(
        [=](const Json::Value& result) // Success
        {
            onGroupMembersFetched(pChannel, groupId, result);
        },
        [](const std::string& status_message) // Error
        {
            // Presence won't work
        })
    );
}

// Fetch group members for a private group
void onGroupMembersFetched(ChannelRef pChannel,
                           const std::string& groupId,
                           const Json::Value& result)
{
    const auto& jsonMembers = result["data"];
    auto memberIds = jsonMembers.getMemberNames();
    for (auto& memberId : memberIds)
    {
        const auto& member = jsonMembers[memberId];
        auto pUser = std::make_shared<User>();
        pUser->id = memberId;
        pUser->name = member["playerName"].asString();
        pUser->online = state.user.id == memberId;
        pChannel->members.push_back(pUser);
    }

    // Register group presence
    pBCWrapper->getPresenceService()->registerListenersForGroup(groupId, true, new BCCallback(
        [=](const Json::Value& result) // Success
        {
            const auto& jsonPresences = result["data"]["presence"];
            for (int i = 0; i < (int)jsonPresences.size(); ++i)
            {
                const auto& jsonPresence = jsonPresences[i];
                const auto& jsonUser = jsonPresence["user"];
                for (auto pUser : pChannel->members)
                {
                    if (pUser->id == jsonUser["id"].asString())
                    {
                        pUser->pic = jsonUser["pic"].asString();
                        pUser->online = jsonPresence["online"].asBool();
                    }
                }
            }
        },
        [](const std::string& status_message) // Error
        {
            // Presence won't work
        })
    );
}

// Get channel by id by looking into channels and groups.
ChannelRef getChannelById(const std::string& id)
{
    for (auto pChannel : state.chatData.globalChannels)
    {
        if (pChannel->id == id) return pChannel;
    }
    for (auto pChannel : state.chatData.groups)
    {
        if (pChannel->id == id) return pChannel;
    }
    return nullptr;
}

// Received a chat message. Add it to the proper channel
void onIncomingMessage(const Json::Value& jsonMessage)
{
    auto pChannel = getChannelById(jsonMessage["chId"].asString());
    if (!pChannel)
    {
        // Maybe we are not registered to this channel anymore. Move on...
        return;
    }

    auto pMessage = std::make_shared<Message>();
    pMessage->msgId = jsonMessage["msgId"].asString();
    pMessage->text = jsonMessage["content"]["text"].asString();
    pMessage->user.id = jsonMessage["from"]["id"].asString();
    pMessage->user.name = jsonMessage["from"]["name"].asString();
    pMessage->user.pic = jsonMessage["from"]["pic"].asString();
    pMessage->date = jsonMessage["date"].asUInt64();
    pMessage->ver = jsonMessage["ver"].asInt();
    pChannel->messages.push_back(pMessage);

    // Erase messages older than MAX_HISTORY
    while (pChannel->messages.size() > MAX_HISTORY)
    {
        pChannel->messages.erase(pChannel->messages.begin());
    }
}

// Someone deleted a message. Find it and remove it
void onRemoveMessage(const Json::Value& jsonMessage)
{
    auto pChannel = getChannelById(jsonMessage["chId"].asString());
    if (!pChannel)
    {
        // Maybe we are not registered to this channel anymore. Move on...
        return;
    }

    const auto& msgId = jsonMessage["msgId"].asString();
    for (auto it = pChannel->messages.begin(); 
         it != pChannel->messages.end(); ++it)
    {
        if ((*it)->msgId == msgId)
        {
            pChannel->messages.erase(it);
            return;
        }
    }
}

// Someone updated a message. Find it and update it
void onUpdateMessage(const Json::Value& jsonMessage)
{
    auto pChannel = getChannelById(jsonMessage["chId"].asString());
    if (!pChannel)
    {
        // Maybe we are not registered to this channel anymore. Move on...
        return;
    }

    const auto& msgId = jsonMessage["msgId"].asString();
    for (auto& pMessage : pChannel->messages)
    {
        if (pMessage->msgId == msgId)
        {
            pMessage->text = jsonMessage["content"]["text"].asString();
            pMessage->user.id = jsonMessage["from"]["id"].asString();
            pMessage->user.name = jsonMessage["from"]["name"].asString();
            pMessage->user.pic = jsonMessage["from"]["pic"].asString();
            pMessage->date = jsonMessage["date"].asUInt64();
            pMessage->ver = jsonMessage["ver"].asInt();
            return;
        }
    }
}

// A user's presence updated. Find him in all groups and update him.
void onPresenceUpdate(const Json::Value& jsonPresence)
{
    const auto& fromId = jsonPresence["from"]["id"].asString();
    for (auto& pChannel : state.chatData.groups)
    {
        for (auto& pUser : pChannel->members)
        {
            if (pUser->id == fromId)
            {
                pUser->online = jsonPresence["online"].asBool();
                break;
            }
        }
    }
}

// Go back to login screen, with an error message
void dieWithMessage(const std::string& message)
{
    errorMessage = message;
    ImGui::OpenPopup("Error");

    uninitBC();
    resetState();
}

// Uninitialize brainCloud
void uninitBC()
{
    if (pBCWrapper)
    {
        pBCWrapper->getBCClient()->deregisterAllRTTCallbacks();
        pBCWrapper->getBCClient()->resetCommunication();
    }
    BCCallback::destroyAll();
}

// Reset application state, back to login screen
void resetState()
{
    state.chatData.globalChannels.clear();
    state.chatData.groups.clear();
    state.chatData.pActiveChannel.reset();
    state.user.id.clear();
    state.screenState = ScreenState::Login;
}

//-----------------------------------------------------------------------------
// Public functions
//-----------------------------------------------------------------------------

// Draws the application's GUI and update brainCloud
void app_update()
{
    if (pBCWrapper)
    {
        pBCWrapper->runCallbacks();
    }

    // Display the proper scree
    switch (state.screenState)
    {
        case ScreenState::Login:
            login_update();
            break;
        case ScreenState::Chat:
            chat_update();
            break;
        case ScreenState::Loading:
            loading_update();
            break;
        case ScreenState::AskForName:
            askForName_update();
            break;
    }

    // Error message popup
    if (ImGui::BeginPopupModal("Error", NULL, 
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("%s", errorMessage.c_str());
        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// Logs out the current user and goes back to login screen
void app_logOut()
{
    uninitBC();
    resetState();
}

#if defined(BCCHAT_UWP)
#include <Windows.h>
#endif

// Shutdowns the application
void app_exit()
{
#if defined(BCCHAT_UWP)
    Windows::ApplicationModel::Core::CoreApplication::Exit();
#else
    exit(0);
#endif
}

// Attempt login with the specific username/password
void app_login(const char* username, const char* password)
{
    initBC();

    // Show loading screen
    loading_text = "Logging in ...";
    state.screenState = ScreenState::Loading;

    // Authenticate with brainCloud
    pBCWrapper->authenticateUniversal(
        username,
        password,
        true, // Create if user doesn't exist
        new BCCallback(
        [](const Json::Value& result) // Success
        {
            handlePlayerState(result);
        },
        [](const std::string& status_message) // Error
        {
            dieWithMessage("Login Failed:\n" + status_message);
        })
    );
}

// Submit user name to brainCloud to be assosiated with the current user
void app_submitName(const char* firstName, const char* lastName)
{
    state.user.name = firstName + std::string(" ") + lastName;

    // Show loading screen
    loading_text = "Logging in ...";
    state.screenState = ScreenState::Loading;

    // Update name
    pBCWrapper->getPlayerStateService()->updateName(
        state.user.name.c_str(),
        new BCCallback(
        [](const Json::Value& result) // Success
        {
            onLoggedIn();
        },
        [](const std::string& status_message) // Error
        {
            dieWithMessage("Failed to update username to brainCloud:\n" +
                status_message);
        })
    );
}

// Sends message to the active channel
void app_sendMessage(const char* szMessage)
{
    if (!state.chatData.pActiveChannel) return;

    // If we type "/me " in front of the message, we don't record it
    // in history.
    std::string message = szMessage;
    auto recordInHistory = true;
    if (strncmp(szMessage, "/me ", 4) == 0)
    {
        recordInHistory = false;
    }

    // Post message
    pBCWrapper->getChatService()->postChatMessageSimple(
        state.chatData.pActiveChannel->id,
        message,
        recordInHistory,
        new BCCallback(
        [](const Json::Value& result) // Success
        {
            // Ok
        },
        [](const std::string& status_message) // Error
        {
            dieWithMessage("Failed to send message:\n" + status_message);
        })
    );
}
