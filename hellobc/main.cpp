// BasicDemo.cpp : Defines the entry point for the application.
//

#include <iostream>
#include <thread>
#include "braincloud/BrainCloudWrapper.h"
#include "ids.h"
#include "lws_config.h"

using namespace BrainCloud;
using namespace std;

static BrainCloud::BrainCloudWrapper* pBCWrapper = nullptr;
static std::string status("");
static std::string prevStatus;
static char input;
static int result = -1;

//##############################################################################

class RTTConnectCallback final : public IRTTConnectCallback
{
public:
    void rttConnectSuccess() override
    {
        status += "RTT enabled\n\n";
        result = 0;
    }

    void rttConnectFailure(const std::string& errorMessage) override
    {
        status += "ERROR: enableRTT: " + errorMessage  + "\n\n";
        result = 2;
    }
};
static RTTConnectCallback rttConnectCallback;

//##############################################################################

class AuthCallback final : public IServerCallback
{
public:
    void serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string& jsonData) override
    {
        status += "Authenticated\n\n";
        Json::Reader reader;
        Json::Value data;
        reader.parse(jsonData, data);

        std::string profileId = data["data"]["profileId"].asString();

        std::string isNew = data["data"]["newUser"].asString();
        if (isNew.compare("true") == 0) status += "Created new profile: " + profileId + "\n";
        else status += "Logged in existing profile: " + profileId + "\n";

        status += "Login count: ";
        status += data["data"]["loginCount"].asString();
        status += "\n\n";
        
        // now try rtt
        pBCWrapper->getBCClient()->getRTTService()->enableRTT(&rttConnectCallback, true);
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string& jsonError) override
    {
        status += "ERROR: authenticateUniversal: " + jsonError + "\n\n";
        
        result = 1;
    }
};

static AuthCallback authCallback;

//##############################################################################
// CALLBACK LOOP
void app_update()
{
    do {
        if (pBCWrapper)
        {
            pBCWrapper->runCallbacks();
        }

        if (status.length() > 0) {
            cout << status;
            status = "";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // run callbacks loop every 1/10 second
    } while(result<0);
}

//##############################################################################
// MAIN GAME LOOP
int main()
{
    cout << "Welcome to BrainCloud!" << endl;

    // Initialize brainCloud
    if (!pBCWrapper) {

        // Initialize
        pBCWrapper = new BrainCloud::BrainCloudWrapper("");

        pBCWrapper->initialize(
            BRAINCLOUD_SERVER_URL,
            BRAINCLOUD_APP_SECRET,
            BRAINCLOUD_APP_ID,
            pBCWrapper->getBCClient()->getBrainCloudClientVersion().c_str(),
            "bitHeads inc.",
            "AndroidSaveData");

        status += "Initialized BrainCloud version ";
        status += pBCWrapper->getBCClient()->getBrainCloudClientVersion().c_str();
        status += "\n\n";
        
        status += "Using libwebsocket version ";
        status += std::to_string(LWS_LIBRARY_VERSION_MAJOR)
            + "." +std::to_string(LWS_LIBRARY_VERSION_MINOR)
            + "." + std::to_string(LWS_LIBRARY_VERSION_PATCH);
        status += "\n\n";
        
        pBCWrapper->getBCClient()->enableLogging(true);

    }

    // create an update loop for RunCallbacks()
    std::thread(app_update).detach();
    
    // to make sure there's a fresh start for this test
    pBCWrapper->resetStoredProfileId();

    // Authenticate
    status += "Authenticating...\n";
    pBCWrapper->authenticateAnonymous(&authCallback);
    
    // keep app alive
    do {
    } while (result < 0); // callbacks will change this value


    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // short sleep to wrap things up

    switch(result){
        case 0:
            cout << "Successful test run. Good-bye." << endl;
            break;
        case 1:
            cout << "Run failed in authentication callback." << endl;
            break;
        case 2:
            cout << "Run failed in RTT callback." << endl;
            break;
    }

	return result;
}
