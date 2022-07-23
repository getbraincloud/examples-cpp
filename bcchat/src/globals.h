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
// File: globals.h
// Desc: Defines global application state, data and constants
// Author: David St-Louis
//-----------------------------------------------------------------------------

// C/C++ includes
#include <string>
#include <vector>
#include <memory>

// brainCloud App settings. Create ids.h and define BRAINCLOUD_SERVER_URL, BRAINCLOUD_APP_ID and BRAINCLOUD_APP_SECRET
#include "ids.h"

// Max message count to be fetched and displayed per channel
#define MAX_HISTORY 100

// Max character count that can be used for username and password
#define MAX_CREDENTIAL_CHAR 32

// Typedefs
struct Message;
using MessageRef = std::shared_ptr<Message>;
using Messages = std::vector<MessageRef>;
struct Channel;
using ChannelRef = std::shared_ptr<Channel>;
using Channels = std::vector<ChannelRef>;
struct User;
using UserRef = std::shared_ptr<User>;
using Users = std::vector<UserRef>;

// Screen state enum.
enum class ScreenState : int
{
    Login,      /* Login screen */
    Chat,       /* Main chat screen with side panels */
    Loading,    /* Loading dialog */
    AskForName  /* Ask for full name dialog */
};

// A brainCloud user
struct User
{
    std::string id;         /* Profile Id */
    std::string pic;        /* Profile picture (unused) */
    std::string name;       /* User name */
    bool online = false;    /* Wether the user is online or not */
};

// A chat message
struct Message
{
    User user;          /* User that sent that message */
    std::string text;   /* Main message text */
    std::string msgId;  /* Message Id */
    uint64_t date;      /* Sent time (unused) */
    int ver;            /* Message version */
};

// Global or Group channel
struct Channel
{
    std::string id;     /* Channel Id */
    std::string name;   /* Channel display name */
    Messages messages;  /* Recent MAX_HISTORY Messages */
    Users members;      /* For groups only, user list */
};

// Data used for the chat screen
struct ChatData
{
    Channels globalChannels;    /* Global channels */
    Channels groups;            /* Private groups */
    ChannelRef pActiveChannel;  /* Current selected channel */
};

// Main application state. This contain all of the "live" data.
struct State
{
    ScreenState screenState = ScreenState::Login;   /* Current screen */
    ChatData chatData;  /* Chat data if in the chat screen */
    User user;          /* Logged-in user */
};

// Load/Save configuration file from/to disk (./config.txt)
void loadConfigs();
void saveConfigs();

// Main application state instance
extern State state;

// Credentials
extern char username[MAX_CREDENTIAL_CHAR];
extern char password[MAX_CREDENTIAL_CHAR];

// GUI theme. 0 = classic, 1 = dark, 2 = light
extern int theme;

// Main window's dimensions
extern int width;
extern int height;
