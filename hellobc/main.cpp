// BasicDemo.cpp : Defines the entry point for the application.
//

#include <iostream>
#include <thread>
#include "braincloud/BrainCloudWrapper.h"
#include "ids.h"

using namespace BrainCloud;
using namespace std;

static BrainCloud::BrainCloudWrapper* pBCWrapper = nullptr;
static std::string status("");
static std::string prevStatus;
static char input;
static int result = -1;
static std::chrono::duration<double, std::milli> fp_ms;

//##############################################################################

class RTTConnectCallback final : public IRTTConnectCallback
{
public:
    void rttConnectSuccess() override
    {
        status += "---- RTT enabled after ";
        status += std::to_string(fp_ms.count());
        status += "ms\n\n";
        result = 0;
    }

    void rttConnectFailure(const std::string& errorMessage) override
    {
        status += "---- ERROR: enableRTT: " + errorMessage  + "\n\n";
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
        status += "---- Authenticated\n\n";
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
        status += "---- ERROR: authenticateUniversal: " + jsonError + "\n\n";
        
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
    } while(result < 0);
}

//##############################################################################
// MAIN GAME LOOP
int main()
{
    std::string serverUrl = BRAINCLOUD_SERVER_URL;
    std::string secretKey = BRAINCLOUD_APP_SECRET;
    std::string appId = BRAINCLOUD_APP_ID;
    
    cout << "---- Welcome to BrainCloud!" << endl;

    // Initialize brainCloud
    if (!pBCWrapper) {

        // Initialize
        pBCWrapper = new BrainCloud::BrainCloudWrapper("");

        pBCWrapper->initialize(
            serverUrl.c_str(),
            secretKey.c_str(),
            appId.c_str(),
            "1.0",
            "bitHeads inc.",
            "Hello BrainCloud");

        status += "---- Initialized BrainCloud version ";
        status += pBCWrapper->getBCClient()->getBrainCloudClientVersion().c_str();
        status += "\n\n";

        pBCWrapper->getBCClient()->enableLogging(true);

    }
    
    if(pBCWrapper->getBCClient()->isInitialized())
    {
        // create an update loop for RunCallbacks()
        std::thread(app_update).detach();
        
        // to make sure there's a fresh start for this test
        pBCWrapper->resetStoredProfileId();
        
        // Authenticate
        status += "---- Authenticating...\n";
        pBCWrapper->authenticateAnonymous(&authCallback);
        
        // keep app alive
        
        auto t1 = std::chrono::high_resolution_clock::now();
        do {
            
            auto t2 = std::chrono::high_resolution_clock::now();
            
            // timeout just in case
            fp_ms = t2 - t1;
            
            // callbacks will change this result value
        } while (result < 0 && fp_ms.count() < 15000);
        
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // short sleep to wrap things up
    }
    else{
        result = 3;
    }
    
    switch(result){
        case 0:
            cout << "\n\n---- Successful test run. Good-bye." << endl;
            break;
        case 1:
            cout << "\n\n---- Run failed in authentication callback." << endl;
            break;
        case 2:
            cout << "\n\n---- Run failed in RTT callback." << endl;
            break;
        case 3:
            cout << "\n\n---- Run failed in initialization." << endl;
            break;
        default:
            cout << "\n\n---- Test ended. Probable timeout." << endl;
            break;
    }

	return result;
}
