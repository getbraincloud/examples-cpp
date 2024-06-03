#include <stdio.h>
#include <chrono>
#include <functional>
#include <memory>
#include <sstream>
#include <thread>
#include <braincloud/BrainCloudWrapper.h>
#include <braincloud/IRTTCallback.h>
#include <braincloud/reason_codes.h>
#include <braincloud/internal/IRelayTCPSocket.h>

// Create this file in examples-cpp/roomserver/client/ids.h
// which should define BRAINCLOUD_SERVER_URL, BRAINCLOUD_APP_ID and BRAINCLOUD_APP_SECRET
#include "ids.h"

using namespace BrainCloud;
using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;

// Globals
bool isGameRunning = true; // If false, the main loop will exit
shared_ptr<BrainCloudWrapper> bc;
Json::Value serverConnectionInfo;

std::string appVersion= "1.1";
bool bClearIds = false;

// Prototypes
void connectToServer();

// brainCloud Callback hooks
class AuthCallback : public IServerCallback
{
    void serverError(ServiceName serviceName, 
                     ServiceOperation serviceOperation, 
                     int statusCode, int reasonCode, 
                     const string& jsonError) override
    {
        printf("Auth error: %s\n", jsonError.c_str());
        isGameRunning = false;
    }

    void serverCallback(ServiceName serviceName, 
                        ServiceOperation serviceOperation, 
                        const string& jsonData) override;
};

class FindLobbyCallback : public IServerCallback
{
    void serverError(ServiceName serviceName, 
                     ServiceOperation serviceOperation, 
                     int statusCode, int reasonCode, 
                     const string& jsonError) override
    {
        printf("Find lobby error: %s\n", jsonError.c_str());
        isGameRunning = false;
    }

    void serverCallback(ServiceName serviceName, 
                        ServiceOperation serviceOperation, 
                        const string& jsonData) override {}
};

class RTTConnectCallback final : public IRTTConnectCallback
{
    void rttConnectSuccess() override;

    void rttConnectFailure(const string& errorMessage) override
    {
        printf("RTT connect error: %s\n", errorMessage.c_str());
        isGameRunning = false;
    }
};

class LobbyCallback final : public IRTTCallback
{
    void rttCallback(const std::string& jsonData) override;
};

AuthCallback authCallback;
RTTConnectCallback rttConnectCallback;
FindLobbyCallback findLobbyCallback;
LobbyCallback lobbyCallback;

//---------------------------------------------------------------------------
// Main
//---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Create our brainCloud client
    bc = make_shared<BrainCloudWrapper>("");
    bc->initialize(BRAINCLOUD_SERVER_URL,
                   BRAINCLOUD_APP_SECRET,
                   BRAINCLOUD_APP_ID,
                   appVersion.c_str(),
                   "bitheads", "RoomServerExample");
 
    
    printf("-- Room Server Example Client -- \n\tApp ID: %s\n\tApp Version: %s\n\tClient Version: %s\n\n", BRAINCLOUD_APP_ID, appVersion.c_str(), bc->getBCClient()->getBrainCloudClientVersion().c_str());

    
    bc->getBCClient()->enableLogging(true);

    // call to clear profile id on device
    if(bClearIds)
        bc->clearIds();

    // Authenticate
    bc->authenticateAnonymous(&authCallback);

    // Run brainCloud callbacks in a loop
    while (isGameRunning)
    {
        // Update braincloud
        bc->runCallbacks();

        this_thread::sleep_for(100ms);
    }

    bc->logout(false, nullptr);

    bc = nullptr;
    return 0;
}

//--------------------------------------------------------------------------
// Callback implementations
//--------------------------------------------------------------------------

void AuthCallback::serverCallback(ServiceName serviceName, 
                                  ServiceOperation serviceOperation, 
                                  const string& jsonData)
{
    // Enable the RTT service
    bc->getRTTService()->enableRTT(&rttConnectCallback, true);
}

void RTTConnectCallback::rttConnectSuccess()
{
    // Find lobby
    bc->getRTTService()->registerRTTLobbyCallback(&lobbyCallback);

    string lobbyType = "CppCustomGame";
    int rating = 0;
    int maxSteps = 1;
    string algo = "{\"strategy\":\"ranged-absolute\","
                  "\"alignment\":\"center\",\"ranges\":[1000]}";
    string filter = "{}";
    vector<string> otherUsers = {};
    string settings = "{}";
    bool startReady = true;
    string extra = "{}";
    string teamCode = "all";
    bc->getLobbyService()->findOrCreateLobby(lobbyType, rating, maxSteps, 
                                             algo, filter, otherUsers, 
                                             settings, startReady, extra,
                                             teamCode, &findLobbyCallback);
}

void LobbyCallback::rttCallback(const std::string& jsonData)
{
    // Convert string to json
    Json::Value json;
    stringstream ss(jsonData);
    ss >> json;

    auto operation = json["operation"].asString();

    if (operation == "ROOM_READY")
    {
        serverConnectionInfo = json["data"];
        connectToServer();
    }

    if (operation == "DISBANDED")
    {
        if (json["data"]["reason"]["code"].asInt() != RTT_ROOM_READY)
        {
            // This means the room was disbanded for the wrong reasons
            isGameRunning = false;
        }
    }
}

void connectToServer()
{
    // Create our TCP socket
    auto address = serverConnectionInfo["connectData"]["address"].asString();
    auto port = serverConnectionInfo["connectData"]["ports"]["7777/tcp"].asInt();
    unique_ptr<IRelayTCPSocket> tcpSocket(IRelayTCPSocket::create(address, port));

    // Connect
    while (!tcpSocket->isConnected())
    {
        if (!tcpSocket->isValid())
        {
            printf("Socket connection failed\n");
            isGameRunning = false;
        }

        tcpSocket->updateConnection();
        this_thread::sleep_for(10ms);
    }
    printf("Socket connected\n");

    // Send the passcode
    auto passcode = serverConnectionInfo["passcode"].asString();
    tcpSocket->send((const uint8_t*)passcode.data(), (int)passcode.size() + 1);

    // Wait for the response in a loop.
    int size;
    while (tcpSocket->isValid())
    {
        auto data = tcpSocket->peek(size);
        if (data)
        {
            string message = (const char*)(data + 2);
            printf("Received: %s\n", message.c_str());
            if (message == "CONNECTED")
            {
                printf("SUCCESS!\n");
                isGameRunning = false;
                return;
            }
        }
    }

    printf("Socket reading failed\n");
    isGameRunning = false;
}
