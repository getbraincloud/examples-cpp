#include <jni.h>
#include <string>
#include <sstream>
#include <iostream>
#include <android/log.h>
#include <json/json.h>

#include "braincloud/BrainCloudWrapper.h"
#include "braincloud/IRTTCallback.h"

#include "braincloud/internal/android/AndroidGlobals.h" // to store java native interface env and context for app
#include "ids.h"
#include <libwebsockets.h>
#include "lws_config.h"

using namespace BrainCloud;

enum completionstage{
    none,
    timeout,
    complete,
    initialize,
    authenticate,
    logout,
    logout1,
    rttEnable,
    getChannelId,
    channelConnect,
    rttMessage
};

static BrainCloud::BrainCloudWrapper *pBCWrapper = nullptr;
static std::string status = "";
static std::string jsonResponse = "";
static std::string appVersion = "2.0";
static completionstage stagepassed = none;
static completionstage result = none;
static bool waiting = false;
bool done = false;
static std::string channelId;
static std::chrono::duration<double, std::milli> fp_ms; // to check elapsed time anywhere
static std::chrono::time_point<std::chrono::high_resolution_clock> t1;
static std::chrono::time_point<std::chrono::high_resolution_clock> t2;
static double startwait = 0;
static bool retry = false;
static bool running_repeat = false;
static int attempts = 0;
int count_success = 0;
int count_fail = 0;

// *** user-defined test parameters ***
// set a timeout after n seconds
static double maxrun = 360;
// number of times to repeat (counts down to 0)
static int repeat = 2;
// how many attempts to try on rtt fail (at least 1)
static int max_attempts = 1;
// how long to wait seconds between repeat tests/retries
static double spin = 3;



class ConsoleStream : public std::stringbuf
{
public:
      virtual std::streamsize xsputn(const char *_Ptr, std::streamsize _Count);
}; // CLASS CONSOLESTREAM

std::streamsize ConsoleStream::xsputn(const char *_Ptr, std::streamsize _Count)
{
    __android_log_print(ANDROID_LOG_DEBUG, "brainCloudAPI", "%.*s", (int)_Count, _Ptr);
    return std::basic_streambuf<char, std::char_traits<char> >::xsputn(_Ptr,_Count);
}

ConsoleStream consoleStream;

//##############################################################################

class ChatCallback final : public IRTTCallback
{
public:
    void rttCallback(const std::string& jsonData) override
    {
        Json::Value root;
        Json::Reader reader;
        reader.parse(jsonData, root);
        std::string message = root["data"]["content"]["text"].asString();

        status += "---- RTT message callback \n\n>> " + message + "\n\n";

        // end of the test
        stagepassed = rttMessage;
        waiting = false;
    }
};
ChatCallback chatCallback;

//##############################################################################

class ChannelConnectCallback final : public IServerCallback
{
public:
    void serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string &jsonData) override
    {
        status += "---- Connected to channel\n\n";


        stagepassed = channelConnect;
        waiting = false;
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "**** ERROR: channelConnect: " + jsonError + "\n\n";
        result = channelConnect;
        waiting = false;
    }
};
static ChannelConnectCallback channelConnectCallback;

//##############################################################################

class GetChannelIdCallback final : public IServerCallback
{
public:
    void serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string &jsonData) override
    {
        Json::Value root;
        Json::Reader reader;
        reader.parse(jsonData, root);
        channelId = root["data"]["channelId"].asString();

        status += "---- Chat service enabled\n\n";
        status += "Channel id: " + channelId + "\n\n";

        stagepassed = getChannelId;
        waiting = false;
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "**** ERROR: getChannelId: " + jsonError + "\n\n";
        result = getChannelId;
        waiting = false;
    }
};
static GetChannelIdCallback getChannelIdCallback;

//##############################################################################

class RTTConnectCallback final : public IRTTConnectCallback
{
public:
    void rttConnectSuccess() override
    {
        status += "---- RTT service enabled\n\n";


        stagepassed = rttEnable;
        waiting = false;
    }

    void rttConnectFailure(const std::string& errorMessage) override
    {
        status += "**** ERROR: rttEnable: " + errorMessage  + "\n\n";
        count_fail ++;
        result = rttEnable;
        waiting = false;
    }
};
static RTTConnectCallback rttConnectCallback;

//##############################################################################

class AuthCallback final : public IServerCallback
{
public:
    void serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string &jsonData) override
    {
        status += "---- Authenticated\n\n";
        Json::Reader reader;
        Json::Value data;
        reader.parse(jsonData, data);

        std::string profileId = data["data"]["profileId"].asString();

        std::string isNew = data["data"]["newUser"].asString();
        if(isNew.compare("true")==0) status += "Created new profile: \n\t" + profileId + "\n";
        else status += "Logged in existing profile: \n\t" + profileId + "\n";

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

        stagepassed = authenticate;
        waiting = false;
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "**** ERROR: authenticate: " + jsonError + "\n\n";
        result = authenticate;
        waiting = false;
    }
};
static AuthCallback authCallback;

//##############################################################################
class Logout1Callback final : public IServerCallback
{
public:
    void serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string &jsonData) override
    {
        stagepassed = logout1;
        waiting = false;
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        stagepassed = logout1;
        waiting = false;
    }
};
static Logout1Callback logout1Callback;
//##############################################################################
class LogoutCallback final : public IServerCallback
{
public:
    void serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string &jsonData) override
    {
        status += "---- Logged out of BrainCloud\n";
        stagepassed = logout;
        waiting = false;
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "**** ERROR: logout: " + jsonError + "\n\n";
        result = logout;
        waiting = false;
    }
};
static LogoutCallback logoutCallback;

//##############################################################################
void TestChatFeature() {
    if(!waiting) {
        switch (stagepassed) {
            case initialize:
                // Authenticate
                status += "Authenticating...\n\n";
                pBCWrapper->authenticateAnonymous(&authCallback);
                waiting = true;
                break;
            case authenticate:
                pBCWrapper->getBCClient()->getRTTService()->enableRTT(&rttConnectCallback, true);
                waiting = true;
                break;
            case rttEnable:
                pBCWrapper->getChatService()->getChannelId("gl", "valid", &getChannelIdCallback);
                waiting = true;
                break;
            case getChannelId:
                pBCWrapper->getChatService()->channelConnect(channelId, 50,
                                                             &channelConnectCallback);
                waiting = true;
                break;
            case channelConnect:
                pBCWrapper->getBCClient()->getRTTService()->registerRTTChatCallback(&chatCallback);
                pBCWrapper->getChatService()->postChatMessageSimple(channelId, "Hello from Android",
                                                                    true);
                waiting = true;
                break;
            case rttMessage:
                // Logout
                status += "Logging out...\n\n";
                pBCWrapper->logout(false, &logoutCallback);
                waiting = true;
                break;
            case logout:
                result = complete;
                break;
            default:
                break;
        }
    }
}

void TestSaveData(bool ForgetUser)
{
    if(!waiting) {
        switch (stagepassed) {
            case initialize:
                status += "Stored Profile Id:";
                status += pBCWrapper->getStoredProfileId();
                status += "\n";
                status += "Stored Anon Id:";
                status += pBCWrapper->getStoredAnonymousId();
                status += "\n\n";

                status += "Authenticating...\n\n";
                pBCWrapper->authenticateAnonymous(&authCallback, true);
                waiting = true;
                break;
            case authenticate:
                status += "Stored Profile Id:";
                status += pBCWrapper->getStoredProfileId();
                status += "\n";
                status += "Stored Anon Id:";
                status += pBCWrapper->getStoredAnonymousId();
                status += "\n\n";

                status += "Logging out...\n\n";
                pBCWrapper->logout(ForgetUser, &logoutCallback);
                waiting = true;
                break;
            case logout:
                status += "Stored Profile Id:";
                status += pBCWrapper->getStoredProfileId();
                status += "\n";
                status += "Stored Anon Id:";
                status += pBCWrapper->getStoredAnonymousId();
                status += "\n\n";

                result = complete;
                break;
            default:
                break;
        }
    }
}

void TestCountryCode()
{
    if(!waiting) {
        switch (stagepassed) {
            case initialize:
                pBCWrapper->clearIds();

                status += "System Country Code:";
                status += pBCWrapper->client->getCountryCode();
                status += "\n";
                status += "System Language Code:";
                status += pBCWrapper->client->getLanguageCode();
                status += "\n";
                status += "System Timezone Offset:";
                status += std::to_string(pBCWrapper->client->getTimezoneOffset());
                status += "\n\n";

                status += "Authenticating...\n\n";
                pBCWrapper->authenticateAnonymous(&authCallback, true);
                waiting = true;
                break;
            case authenticate:
                status += "Logging out...\n\n";
                pBCWrapper->logout(true, &logoutCallback);
                waiting = true;
                break;
            case logout:
                result = complete;
                break;
            default:
                break;
        }
    }

}

extern "C" JNIEXPORT jstring JNICALL
Java_com_bitheads_braincloud_android_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */ context) {

    // these are passed in from MainActivity.java
    // we need them for SaveDataHelper to access SharedPreferences
    // context may change while running callbacks so set each time
    if(appEnv != env)
        appEnv = env;
    if(appContext != context)
        appContext = context;

    completionstage lastresult = result; // checking test results to quit on fail

    // Initialize brainCloud
    if (!pBCWrapper) {
        std::cout.rdbuf(&consoleStream);
        t1 = std::chrono::high_resolution_clock::now();

        // Initialize
        pBCWrapper = new BrainCloud::BrainCloudWrapper("");

        pBCWrapper->initialize(
                BRAINCLOUD_SERVER_URL,
                BRAINCLOUD_APP_SECRET,
                BRAINCLOUD_APP_ID,
                appVersion.c_str(),
                "bitheads",
                "android");

        if(pBCWrapper->getBCClient()->isInitialized()) {
            pBCWrapper->getBCClient()->enableLogging(true);
            status += "---- Initialized brainCloud Version ";
            status += pBCWrapper->getBCClient()->getBrainCloudClientVersion().c_str();
            status += "\n     Portal App ";
            status += BRAINCLOUD_APP_ID;
            status += "\n     App Version ";
            status += appVersion.c_str();
            status += "\n\n";

            status += "Using libwebsocket version ";
            status += std::to_string(LWS_LIBRARY_VERSION_MAJOR)
                      + "." + std::to_string(LWS_LIBRARY_VERSION_MINOR)
                      + "." + std::to_string(LWS_LIBRARY_VERSION_PATCH)
                      + "\n";

#if defined(SSL_ALLOW_SELFSIGNED)
            status += "RTT skipping certificates\n";
#else
            status += "RTT certificates included\n";
#endif
            status += "\n";

            std::cout<<status<<std::endl;

            // logging options include: LLL_DEBUG | LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
            lws_set_log_level(
                    LLL_ERR, [](int level, const char *line) {
                        __android_log_print(ANDROID_LOG_ERROR, "brainCloudLWS", "%s", line);
                    });
            stagepassed = initialize;
        } else {
            status += "**** Failed to initialize. Check header file ids.h \n\n";
            result = initialize;
        }
    }

    if (!done)
    {
        //TestChatFeature();
        //TestSaveData(false);
        TestCountryCode();

        // Update braincloud
        pBCWrapper->runCallbacks();

        // check for timeout
        t2 = std::chrono::high_resolution_clock::now();
        fp_ms = t2 - t1;
        double elapsed = fp_ms.count();

        if (result == none && elapsed > maxrun * 1000) {
            status += "**** ERROR: test is timing out after " + std::to_string(elapsed) + "ms \n\n";
            result = timeout; // timeout error
        }

        if (result == rttEnable && max_attempts > 1) {
            if (!retry) {
                startwait = elapsed;
                status += "Attempting retry #" + std::to_string(attempts + 1) + " in 5s\n\n";
                retry = true;
            }
            if (elapsed - startwait > spin * 1000) {
                if (++attempts < max_attempts) {
                    result = none;
                    stagepassed = authenticate;
                    pBCWrapper->getBCClient()->getRTTService()->enableRTT(&rttConnectCallback, true);
                }
                retry = false;
                lastresult = none;
            }
        }

        if(running_repeat && (elapsed - startwait > spin * 1000)){
            startwait = elapsed;
            running_repeat = false;
            stagepassed = initialize;
        }

        // check if done testing
        if (!retry && result != none) {
            if (result == complete) {
                status += "---- Successfully completed test run.\n\n";
                count_success ++;
            }
            else {
                status += "**** Failed with code " + std::to_string(result) + "\n\n";
                count_fail ++;
            }
            done = true;

            if(--repeat > 0 && result != timeout) {
                status += "Repeating test T-minus " + std::to_string(repeat) + "\n\n";
                result = none;
                done = false;
                stagepassed = none;
                //startwait = 0;
                retry = false;
                attempts = 0;

                running_repeat = true;

            }
            else if(repeat == 0){
                status += "---- All runs have been executed.\n\n";
            }
        }
    }

    return env->NewStringUTF(status.c_str());
}
