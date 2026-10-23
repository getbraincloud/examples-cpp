//-----------------------------------------------------------------------------
// File: globalChat.cpp
// Desc: Shared app-wide "Global" chat channel — see globalChat.h. brainCloud's chat
//       calls all require RTT to be enabled (RTT_NOT_ENABLED otherwise);
//       app_enableChatRTT() (app.cpp) keeps RTT connected on every path that reaches
//       the main menu, which covers both call sites (main menu itself, and the lobby,
//       which is only reachable after passing through the main menu). Live RTT push:
//       channelConnect both registers the listener AND returns initial history in one
//       call; every message after that (including our own sends, edits, and deletes)
//       arrives via the RTT "chat" event dispatched to chat_onRTTChatEvent — see
//       knowledge-articles/01-chat.md. No re-fetch after posting.
//-----------------------------------------------------------------------------

#include "globalChat.h"
#include "app.h"
#include "BCCallback.h"

#include <algorithm>
#include <chrono>
#include <imgui.h>

// Must match a channel Code pre-registered in the portal (App > Design > Messaging >
// Chat Channels) — global ("gl") chat channels aren't created ad hoc by getChannelId,
// they resolve an existing registration or fail with CHAT_UNRECOGNIZED_CHANNEL (40603).
static const char *CHAT_CHANNEL_SUB_ID = "gl";

static std::string s_chatChannelId;
static bool s_chatChannelResolving = false;
static bool s_chatChannelReady = false;
static std::vector<ChatMessage> s_chatMessages;
static char s_chatInputBuf[240] = {0};
static bool s_chatSending = false;

static ChatMessage parseChatMessage(const Json::Value &m)
{
    ChatMessage msg;
    msg.msgId = m["msgId"].asString();
    msg.fromName = m["from"]["name"].asString();
    if (msg.fromName.empty())
        msg.fromName = "Player";
    msg.text = m["content"]["text"].asString();
    return msg;
}

// Backoff after a failed getChannelId, so a persistent failure (bad channel code,
// network hiccup) can't turn into a same-call-every-frame loop — brainCloud's abuse
// detection disables the client after enough repeated failures on one API call
// (reason_code 90200), which is exactly what happened here without this guard.
static long long s_chatChannelRetryAtMs = 0;

// Resolves the shared global channel once RTT is up, connects (which also returns
// initial history in the same response), and registers for live push. Safe to call
// every frame a Chat/Global tab is open — no-ops once resolved, in flight, or backing
// off after a recent failure.
static void ensureChatChannel()
{
    if (s_chatChannelReady || s_chatChannelResolving || !pBCWrapper) return;
    if (!pBCWrapper->getRTTService()->getRTTEnabled())
    {
        app_enableChatRTT(); // should already be on by the time the main menu is reached; just in case
        return;
    }

    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (nowMs < s_chatChannelRetryAtMs) return;

    s_chatChannelResolving = true;
    pBCWrapper->getChatService()->getChannelId(
        "gl", CHAT_CHANNEL_SUB_ID,
        new BCCallback(
            [](const Json::Value &result)
            {
                std::string channelId = result["data"]["channelId"].asString();
                if (channelId.empty())
                {
                    s_chatChannelResolving = false;
                    return;
                }

                pBCWrapper->getChatService()->channelConnect(
                    channelId, 30,
                    new BCCallback(
                        [channelId](const Json::Value &connectResult)
                        {
                            s_chatChannelResolving = false;
                            s_chatChannelId = channelId;
                            s_chatChannelReady = true;

                            s_chatMessages.clear();
                            for (const auto &m : connectResult["data"]["messages"])
                                s_chatMessages.push_back(parseChatMessage(m));
                        },
                        [](const std::string &)
                        {
                            s_chatChannelResolving = false;
                            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();
                            s_chatChannelRetryAtMs = now + 5000;
                        }));
            },
            [](const std::string &)
            {
                s_chatChannelResolving = false;
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                s_chatChannelRetryAtMs = now + 5000; // back off 5s before retrying
            }));
}

// Delivered to every connected member, including the sender — so sending never needs
// a follow-up fetch (see chat_onRTTChatEvent below).
static void sendChatMessage()
{
    if (s_chatChannelId.empty() || s_chatInputBuf[0] == '\0' || s_chatSending) return;
    s_chatSending = true;
    pBCWrapper->getChatService()->postChatMessageSimple(
        s_chatChannelId.c_str(), s_chatInputBuf, true,
        new BCCallback(
            [](const Json::Value &) { s_chatSending = false; },
            [](const std::string &) { s_chatSending = false; }));
    s_chatInputBuf[0] = '\0';
}

// New messages, edits, and deletes all arrive on the same "chat" RTT event, keyed by
// msgId — a message updated or deleted after it's scrolled out of the visible/fetched
// window is simply not found below and the event is a no-op, which is correct (there's
// nothing on screen to change).
void chat_onRTTChatEvent(const Json::Value &eventJson)
{
    const std::string operation = eventJson["operation"].asString();
    const Json::Value &data = eventJson["data"];

    if (operation == "INCOMING")
    {
        s_chatMessages.push_back(parseChatMessage(data));
    }
    else if (operation == "UPDATE")
    {
        // Same shape as INCOMING (full message, edited content) — find by msgId and
        // replace in place so it doesn't jump to the bottom of the scroll.
        ChatMessage updated = parseChatMessage(data);
        for (auto &m : s_chatMessages)
        {
            if (m.msgId == updated.msgId)
            {
                m = updated;
                break;
            }
        }
    }
    else if (operation == "DELETE")
    {
        // DELETE's payload is just {chId, msgId} — no content/from — so only msgId is
        // usable here.
        std::string msgId = data["msgId"].asString();
        s_chatMessages.erase(
            std::remove_if(s_chatMessages.begin(), s_chatMessages.end(),
                [&msgId](const ChatMessage &m) { return m.msgId == msgId; }),
            s_chatMessages.end());
    }
}

void drawGlobalChatContent()
{
    ensureChatChannel();

    if (!s_chatChannelReady)
    {
        ImGui::TextDisabled(s_chatChannelResolving ? "Connecting..." : "Chat unavailable.");
    }
    else
    {
        ImGui::BeginChild("chat_scroll", ImVec2(0.0f, -32.0f), true);
        bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f;
        for (const auto &m : s_chatMessages)
        {
            ImGui::TextColored(ImVec4(0.6f, 0.75f, 1.0f, 1.0f), "%s:", m.fromName.c_str());
            ImGui::SameLine();
            ImGui::TextWrapped("%s", m.text.c_str());
        }
        // Jump to the newest message whenever the list just grew (first history
        // fetch included) as well as the usual "stick to bottom" case — otherwise
        // a fetch that lands while scrollY is still at its initial 0 leaves the
        // view stuck on the oldest messages instead of the most recent ones.
        static size_t s_lastCount = 0;
        if (wasAtBottom || s_chatMessages.size() > s_lastCount)
            ImGui::SetScrollHereY(1.0f);
        s_lastCount = s_chatMessages.size();
        ImGui::EndChild();

        ImGui::PushItemWidth(-70.0f);
        bool enterPressed = ImGui::InputText("##chatInput", s_chatInputBuf, sizeof(s_chatInputBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        bool sendClicked = ImGui::Button("Send", ImVec2(60.0f, 0.0f));
        if ((enterPressed || sendClicked) && !s_chatSending)
            sendChatMessage();
    }
}

void drawGlobalChatPanel(const char *windowId, float x, float y, float w, float h)
{
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    ImGui::Begin(windowId, nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar);

    drawGlobalChatContent();

    ImGui::End();
}
