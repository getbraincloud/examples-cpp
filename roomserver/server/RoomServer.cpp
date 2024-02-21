#include <stdio.h>
#include <sstream>
#include "RoomServer.h"

bool RoomServer::init()
{
    if (!loadEnvironmentVariables())
        if(!loadIds())
            return false;

    createS2S();

    if (!loadLobbyJson())
        return false;

    return true;
}

void RoomServer::readyUp()
{
    m_s2s->request(buildRequest("SYS_ROOM_READY"), [](const std::string& result){});
}

bool RoomServer::validatePasscode(const char* passcode)
{
    for (const auto& memberJson : m_lobbyJson["data"]["members"])
    {
        if (memberJson["passcode"].asString() == passcode) return true;
    }

    return false;
}

bool RoomServer::loadIds()
{

    FILE * fp = fopen("ids.txt", "r");
    if (fp == NULL)
    {
        printf("ERROR: Failed to load ids.txt file!\n");
        return false;
    }
    else
    {
        printf("Found ids.txt file!\n");
        char buf[1024];
        while (fgets(buf, sizeof(buf), fp) != NULL)
        {
            char *c = strchr(buf, '\n');
            if (c) { *c = 0; }

            c = strchr(buf, '\r');
            if (c) { *c = 0; }

            std::string line(buf);
            if (line.find("appId") != std::string::npos)
            {
                m_appId = line.substr(line.find("appId") + sizeof("appId"), line.length() - 1);
            }
            else if (line.find("serverName") != std::string::npos)
            {
                m_serverName = line.substr(line.find("serverName") + sizeof("serverName"), line.length() - 1);
            }
            else if (line.find("serverSecret") != std::string::npos)
            {
                m_serverSecret = line.substr(line.find("serverSecret") + sizeof("serverSecret"), line.length() - 1);
            }
            else if (line.find("s2sUrl") != std::string::npos)
            {
                m_serverHost = line.substr(line.find("s2sUrl") + sizeof("s2sUrl"), line.length() - 1);
            }
            else
                m_lobbyId="";
        }
        fclose(fp);
    }

    if (m_appId.empty() ||
        m_serverName.empty() ||
        m_serverSecret.empty() ||
        m_serverHost.empty())
    {
        printf("ERROR: ids.txt missing S2S properties!\n");
        return false;
    }

    printf("SERVER_PORT:   %s\n", m_serverPort.c_str());
    printf("SERVER_HOST:   %s\n", m_serverHost.c_str());
    printf("APP_ID:        %s\n", m_appId.c_str());
    printf("SERVER_SECRET: %s\n", m_serverSecret.c_str());
    printf("SERVER_NAME:   %s\n", m_serverName.c_str());
    printf("LOBBY_ID:      %s\n", m_lobbyId.c_str());

    return true;
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
    return m_serverHost;
    std::stringstream url;
    url << "https://" << m_serverHost << ":" 
        << m_serverPort << "/s2sdispatcher";

    return url.str();
}

void RoomServer::createS2S()
{
    auto s2sUrl = getS2SUrl();
    printf("S2S URL: %s\n", s2sUrl.c_str());

    m_s2s = S2SContext::create(m_appId, m_serverName, m_serverSecret, s2sUrl, true);
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
