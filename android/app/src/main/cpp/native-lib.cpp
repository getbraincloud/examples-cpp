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
#include "lws_config.h"

using namespace BrainCloud;

static BrainCloud::BrainCloudWrapper *pBCWrapper = nullptr;
static std::string status = "";
static int result = -1;
bool done = false;
static std::string channelId;
static std::chrono::duration<double, std::milli> fp_ms; // to check elapsed time anywhere
static std::chrono::time_point<std::chrono::high_resolution_clock> t1;
static std::chrono::time_point<std::chrono::high_resolution_clock> t2;
static double startwait = 0;
static double maxrun = 180000;
static bool retry = false;
static int attempts = 0;
static int repeat = 12;
static int max_attempts = 1;

class ConsoleStream : public std::stringbuf
{
public:
      virtual std::streamsize xsputn(const char *_Ptr, std::streamsize _Count);
}; // CLASS CONSOLESTREAM

std::streamsize ConsoleStream::xsputn(const char *_Ptr, std::streamsize _Count)
{
    __android_log_print(ANDROID_LOG_DEBUG, "NATIVE", "%.*s", (int)_Count, _Ptr);
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
        result = 0;
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

        pBCWrapper->getBCClient()->getRTTService()->registerRTTChatCallback(&chatCallback);
        pBCWrapper->getChatService()->postChatMessageSimple(channelId, "Hello from Android", true);
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "**** ERROR: channelConnect: " + jsonError + "\n\n";
        result = 4;
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

        pBCWrapper->getChatService()->channelConnect(channelId, 50, &channelConnectCallback);
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "**** ERROR: getChannelId: " + jsonError + "\n\n";
        result = 3;
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
        pBCWrapper->getChatService()->getChannelId("gl", "valid", &getChannelIdCallback);
    }

    void rttConnectFailure(const std::string& errorMessage) override
    {
        status += "**** ERROR: enableRTT: " + errorMessage  + "\n\n";
        result = 2;
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

        pBCWrapper->getBCClient()->getRTTService()->enableRTT(&rttConnectCallback, true);
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "**** ERROR: authenticateUniversal: " + jsonError + "\n\n";
        result = 1;
    }
};
static AuthCallback authCallback;
//##############################################################################

class LogoutCallback final : public IServerCallback
{
public:
    void serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string &jsonData) override
    {
        status += "---- Logged out of BrainCloud\n\n";
        done = true;

        if(repeat-- > 0 && result != 6) {
            status += "Running test T-minus " + std::to_string(repeat) + "\n\n";
            result = -1;
            done = false;
            startwait = 0;
            retry = false;
            attempts = 0;

            pBCWrapper->authenticateAnonymous(&authCallback);
        }
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "**** ERROR: logout: " + jsonError + "\n\n";
        done = true;
    }
};
static LogoutCallback logoutCallback;
//##############################################################################

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

    int lastresult = result; // checking test results to quit on fail

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
                "1.0",
                "bitHeads inc.",
                "ClientTest");

        if(pBCWrapper->getBCClient()->isInitialized()) {
            pBCWrapper->getBCClient()->enableLogging(true);

            status += "---- Initialized BrainCloud version ";
            status += pBCWrapper->getBCClient()->getBrainCloudClientVersion().c_str();
            status += "\n\n";

            status += "Using libwebsocket version ";
            status += std::to_string(LWS_LIBRARY_VERSION_MAJOR)
                      + "." + std::to_string(LWS_LIBRARY_VERSION_MINOR)
                      + "." + std::to_string(LWS_LIBRARY_VERSION_PATCH);
            status += "\n\n";

            // Authenticate
            status += "Authenticating...\n\n";
            pBCWrapper->authenticateEmailPassword("testAndroidUser", "qwertY123", true, &authCallback);
            //pBCWrapper->authenticateAnonymous(&authCallback);
        }
        else{
            status += "**** Failed to initialize. Check header file ids.h \n\n";
            result = 7;
        }
    }

    // Update braincloud
    if(!done)
        pBCWrapper->runCallbacks();

    // check for timeout
    t2 = std::chrono::high_resolution_clock::now();
    fp_ms = t2 - t1;
    double elapsed = fp_ms.count();

    if(!done && result < 0 && elapsed > maxrun) {
        status += "**** ERROR: test is timing out after " + std::to_string(elapsed) + "ms \n\n";
        result = 6; // timeout error
    }

    if(result == 2) {
        if (!retry) {
            startwait = elapsed;
            retry = true;
        }
        if (elapsed - startwait > 5000) {
            if (++attempts < max_attempts) {
                result = -1;
                status += "Attempting to connect #" + std::to_string(attempts) + "\n\n";
                pBCWrapper->getBCClient()->getRTTService()->enableRTT(&rttConnectCallback, true);
            }
            retry = false;
            lastresult = -1;
        }
    }

    // check if done testing
    if(!done && !retry && result >= 0 && lastresult != result){
        if(result == 0)
            status += "---- Successfully completed tests.\n\n";
        else
            status += "**** Failed with code " + std::to_string(result) + "\n\n";

        pBCWrapper->getBCClient()->getPlayerStateService()->logout(&logoutCallback);
    }

    return env->NewStringUTF(status.c_str());
}
