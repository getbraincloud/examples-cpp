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
enum errorstatus{
    none,
    pass,
    timeout,
    complete,
    initialize,
    authenticate,
    logout,
    rttEnable
};
static errorstatus result = none;
static std::chrono::duration<double, std::milli> fp_ms;
bool ForgetUser = false;
bool EnableRTT = false;
int Repeat = 1;

//##############################################################################

class RTTConnectCallback final : public IRTTConnectCallback
{
public:
    void rttConnectSuccess() override
    {
        status += "---- RTT enabled after ";
        status += std::to_string(fp_ms.count());
        status += "ms\n\n";
        result = pass;
    }

    void rttConnectFailure(const std::string& errorMessage) override
    {
        status += "---- ERROR: enableRTT: " + errorMessage  + "\n\n";
        result = rttEnable;
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

        status += "Server Country Code: ";
        status += data["data"]["countryCode"].asString();
        status += "\n";
        status += "Server Language Code: ";
        status += data["data"]["languageCode"].asString();
        status += "\n";
        status += "Server Timezone Offset: ";
        status += std::to_string(data["data"]["timeZoneOffset"].asFloat());
        status += "\n";

        result = pass;
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string& jsonError) override
    {
        status += "---- ERROR: authenticateUniversal: " + jsonError + "\n\n";
        
        result = authenticate;
    }
};

static AuthCallback authCallback;
//##############################################################################

class LogoutCallback final : public IServerCallback
{
public:
    void serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string &jsonData) override
    {
        status += "---- Logged out of BrainCloud\n";
        result = pass;
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "**** ERROR: logout: " + jsonError + "\n\n";
        result = logout;
    }

};
static LogoutCallback logoutCallback;

//##############################################################################
void flush_status() {
    if (status.length() > 0) {
        cout << status;
        status = "";
    }
}
// CALLBACK LOOP
void app_update()
{
    result = none;

    auto t1 = std::chrono::high_resolution_clock::now();
    do {
        auto t2 = std::chrono::high_resolution_clock::now();

        // timeout just in case
        fp_ms = t2 - t1;

        if (pBCWrapper)
        {
            pBCWrapper->runCallbacks();
        }

        flush_status();

        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // run callbacks loop every 1/10 second
        if(fp_ms.count() > 15000){
            // optional: logout if there's a timeout
            result = timeout;
        }
    } while(result == none);
}
//##############################################################################
// MAIN GAME LOOP
// Example argument: ./hellobc "{\"TestSaveData\": {\"ForgetUser\": false, \"Repeat\": 1}}"
int main(int argc, char* argv[])
{
    std::string serverUrl = BRAINCLOUD_SERVER_URL;
    std::string secretKey = BRAINCLOUD_APP_SECRET;
    std::string appId = BRAINCLOUD_APP_ID;

    status += "---- Welcome to BrainCloud!\n";


    // check if there is more than one argument and use the second one
    //  (the first argument is the executable)
    if (argc > 1)
    {
        std::string arg1(argv[1]);
        // do stuff with arg1
        Json::Reader reader;
        Json::Value data;
        reader.parse(arg1, data);

        ForgetUser = data["TestSaveData"]["ForgetUser"].asBool();
        Repeat = data["TestSaveData"]["Repeat"].asInt();
    }

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
    flush_status();

    do {
        if (pBCWrapper->getBCClient()->isInitialized()) {

            // optional: create an update loop for RunCallbacks() if you do not want to wait for results
            // make sure to logout on exit
            // std::thread(app_update).detach();

            status += "Stored Profile Id:";
            status += pBCWrapper->getStoredProfileId();
            status += "\n";
            status += "Stored Anon Id:";
            status += pBCWrapper->getStoredAnonymousId();
            status += "\n\n";

            status += "System Country Code:";
            status += pBCWrapper->client->getCountryCode();
            status += "\n";
            status += "System Language Code:";
            status += pBCWrapper->client->getLanguageCode();
            status += "\n";
            status += "System Timezone Offset:";
            status += std::to_string(pBCWrapper->client->getTimezoneOffset());
            status += "\n\n";
            flush_status();

            // Authenticate
            status += "---- Authenticating...\n";
            pBCWrapper->authenticateAnonymous(&authCallback);
            app_update(); // call update in this thread if you do want to wait for results
            if(result != pass) break;

            status += "Stored Profile Id:";
            status += pBCWrapper->getStoredProfileId();
            status += "\n";
            status += "Stored Anon Id:";
            status += pBCWrapper->getStoredAnonymousId();
            status += "\n\n";

            // Enable RTT
            if (EnableRTT) {
                pBCWrapper->getBCClient()->getRTTService()->enableRTT(&rttConnectCallback, true);
                app_update(); // call update in this thread if you do want to wait for results
                if(result != pass) break;
            }

            // Logout
            status += "---- Logging out...\n";
            pBCWrapper->logout(ForgetUser, &logoutCallback);
            app_update(); // call update in this thread if you do want to wait for results
            if(result != pass) break;

            // mark complete on last run
            if(Repeat == 1) result = complete;

            status += "Stored Profile Id:";
            status += pBCWrapper->getStoredProfileId();
            status += "\n";
            status += "Stored Anon Id:";
            status += pBCWrapper->getStoredAnonymousId();
            status += "\n\n";

            flush_status();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // short sleep to wrap things up
        } else {
            result = initialize;
        }
    }while(--Repeat > 0); // execute one time, no repeat

    switch(result){
        case complete:
            cout << "\n\n---- Successful test run. Good-bye." << endl;
            break;
        case initialize:
            cout << "\n\n---- Run failed in initialization." << endl;
            break;
        case authenticate:
            cout << "\n\n---- Run failed in authentication callback." << endl;
            break;
        case rttEnable:
            cout << "\n\n---- Run failed in RTT callback." << endl;
            break;
        case logout:
            cout << "\n\n---- Run failed in logout." << endl;
            break;
        case none:
        default:
            cout << "\n\n---- Test ended. Probable timeout." << endl;
            break;
    }

	return result;
}
