#include <stdio.h>
#include <sstream>
#include "RoomServer.h"

bool RoomServer::init()
{
    if (!loadEnvironmentVariables())
        return false;

    createS2S();

    if (!loadLobbyJson())
        return false;

    return true;
}

void RoomServer::readyUp()
{
    m_s2s->request(buildRequest("SYS_ROOM_READY"));
}

bool RoomServer::validatePasscode(const char* passcode)
{
    for (const auto& memberJson : m_lobbyJson["data"]["members"])
    {
        if (memberJson["passcode"].asString() == passcode) return true;
    }

    return false;
}

bool RoomServer::loadEnvironmentVariables()
{
    // Get environement variables passed from brainCloud to our container.
    const char* SERVER_PORT     = getenv("SERVER_PORT");
    const char* SERVER_HOST     = getenv("SERVER_HOST");
    const char* APP_ID          = getenv("APP_ID");
    const char* SERVER_SECRET   = getenv("SERVER_SECRET");
    const char* SERVER_NAME     = getenv("SERVER_NAME");
    const char* LOBBY_ID        = getenv("LOBBY_ID");

    printf("SERVER_PORT:   %s\n", SERVER_PORT);
    printf("SERVER_HOST:   %s\n", SERVER_HOST);
    printf("APP_ID:        %s\n", APP_ID);
    printf("SERVER_SECRET: %s\n", SERVER_SECRET);
    printf("SERVER_NAME:   %s\n", SERVER_NAME);
    printf("LOBBY_ID:      %s\n", LOBBY_ID);

    if (!SERVER_PORT || !SERVER_HOST || !APP_ID || 
        !SERVER_SECRET || !SERVER_NAME || !LOBBY_ID)
    {
        printf("Invalid parameters\n");
        return false;
    }

    m_serverPort    = SERVER_PORT;
    m_serverHost    = SERVER_HOST;
    m_appId         = APP_ID;
    m_serverSecret  = SERVER_SECRET;
    m_serverName    = SERVER_NAME;
    m_lobbyId       = LOBBY_ID;

    return true;
}

std::string RoomServer::getS2SUrl() const
{
    std::stringstream url;
    url << "https://" << m_serverHost << ":" 
        << m_serverPort << "/s2sdispatcher";

    return url.str();
}

void RoomServer::createS2S()
{
    auto s2sUrl = getS2SUrl();
    printf("S2S URL: %s\n", s2sUrl.c_str());

    m_s2s = S2SContext::create(m_appId, m_serverName, m_serverSecret, s2sUrl);
    m_s2s->setLogEnabled(true);
}

std::string RoomServer::buildRequest(const std::string& operation) const
{
    std::string request;

    Json::Value requestJson;
    requestJson["service"] = "lobby";
    requestJson["operation"] = operation;
    requestJson["data"]["lobbyId"] = m_lobbyId;

    std::stringstream ss;
    ss << requestJson;
    request = ss.str();

    return std::move(request);
}

bool RoomServer::loadLobbyJson()
{
    auto response = m_s2s->requestSync(buildRequest("GET_LOBBY_DATA"));

    std::stringstream ss;
    ss.str(response);
    ss >> m_lobbyJson;

    return (m_lobbyJson["status"].asInt() == 200);
}
