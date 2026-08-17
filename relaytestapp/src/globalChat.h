//-----------------------------------------------------------------------------
// File: globalChat.h
// Desc: Shared app-wide "Global" chat channel (brainCloud Chat service) — used by
//       both the main menu's Chat tab and the lobby's Chat > Global sub-tab, so the
//       channel resolution / message history / send logic lives in one place.
//-----------------------------------------------------------------------------
#pragma once

#include "globals.h"
#include <json/json.h>

// Draws the global-chat panel (message scroll + input box) at the given rect, as
// its own standalone window. windowId must be unique per call site (e.g.
// "##global_chat_mainmenu" vs "##global_chat_lobby") since ImGui windows are
// identified by their label.
void drawGlobalChatPanel(const char *windowId, float x, float y, float w, float h);

// Same content (message scroll + input box), but assumes the caller has already
// opened an ImGui window/child — for embedding inside another tabbed panel (e.g.
// the lobby's Chat tab, which has its own "This Lobby / Global" sub-toggle above
// this content).
void drawGlobalChatContent();

// Dispatches an RTT "chat" event to the global chat channel. Called from the app's
// central RTT callback whenever eventJson["service"] == "chat" — see
// knowledge-articles/01-chat.md for why this replaces polling after every send.
void chat_onRTTChatEvent(const Json::Value &eventJson);
