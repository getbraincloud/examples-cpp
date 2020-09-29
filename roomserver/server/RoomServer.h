#pragma once

#include <string>
#include <json/json.h>
#include <brainclouds2s.h>

class RoomServer
{
public:
    bool init();
    void ready();
    bool validatePasscode(const char* passcode);

private:
    bool loadEnvironmentVariables();
    void createS2S();
    std::string getS2SUrl() const;
    std::string buildRequest(const std::string& operation) const;
    bool loadLobbyJson();

    std::string m_serverPort;
    std::string m_serverHost;
    std::string m_appId;
    std::string m_serverSecret;
    std::string m_serverName;
    std::string m_lobbyId;

    S2SContextRef m_s2s;
    Json::Value m_lobbyJson;
};
