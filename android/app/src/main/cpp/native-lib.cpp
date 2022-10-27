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

static BrainCloud::BrainCloudWrapper *pBCWrapper = nullptr;
static std::string status = "";
static int result = -1;
bool done = false;
static std::string channelId;
static std::chrono::duration<double, std::milli> fp_ms; // to check elapsed time anywhere
static std::chrono::time_point<std::chrono::high_resolution_clock> t1;
static std::chrono::time_point<std::chrono::high_resolution_clock> t2;
static double startwait = 0;
static bool retry = false;
static int attempts = 0;
int count_success = 0;
int count_fail = 0;

// *** user-defined test parameters ***
// set a timeout after n seconds
static double maxrun = 360;
// number of times to repeat (counts down to 0)
static int repeat = 100;
// how many attempts to try on rtt fail (at least 1)
static int max_attempts = 1;

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

        count_success ++;
        result = 0;
        //pBCWrapper->getChatService()->getChannelId("gl", "valid", &getChannelIdCallback);
    }

    void rttConnectFailure(const std::string& errorMessage) override
    {
        status += "**** ERROR: enableRTT: " + errorMessage  + "\n\n";
        count_fail ++;
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
        status += "---- Logged out of BrainCloud\n";
        status += "Passed  " + std::to_string(count_success) + " Failed  " + std::to_string(count_fail) +"\n\n";
        done = true;

        if(--repeat > 0 && result != 6) {
            status += "Repeat test T-minus " + std::to_string(repeat) + "\n\n";
            result = -1;
            done = false;
            startwait = 0;
            retry = false;
            attempts = 0;

            pBCWrapper->authenticateAnonymous(&authCallback);
        }
        else if(repeat == 0){
            status += "---- All runs have been executed.\n\n";
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

            // Authenticate
            status += "Authenticating...\n\n";
            pBCWrapper->authenticateEmailPassword("testAndroidUser", "qwertY123", true,
                                                  &authCallback);
            //pBCWrapper->authenticateAnonymous(&authCallback);
        } else {
            status += "**** Failed to initialize. Check header file ids.h \n\n";
            result = 7;
        }
    }

    if (!done)
    {
        // Update braincloud
        pBCWrapper->runCallbacks();

        // check for timeout
        t2 = std::chrono::high_resolution_clock::now();
        fp_ms = t2 - t1;
        double elapsed = fp_ms.count();

        if (result < 0 && elapsed > maxrun * 1000) {
            status += "**** ERROR: test is timing out after " + std::to_string(elapsed) + "ms \n\n";
            result = 6; // timeout error
        }

        if (result == 2 && max_attempts > 1) {
            if (!retry) {
                startwait = elapsed;
                status += "Attempting retry #" + std::to_string(attempts + 1) + " in 5s\n\n";
                retry = true;
            }
            if (elapsed - startwait > 5000) {
                if (++attempts < max_attempts) {
                    result = -1;
                    pBCWrapper->getBCClient()->getRTTService()->enableRTT(&rttConnectCallback, true);
                }
                retry = false;
                lastresult = -1;
            }
        }

        // check if done testing
        if (!retry && result >= 0 && lastresult != result) {
            if (result == 0)
                status += "---- Successfully completed test run.\n\n";
            else
                status += "**** Failed with code " + std::to_string(result) + "\n\n";

            pBCWrapper->getBCClient()->getPlayerStateService()->logout(&logoutCallback);
        }
    }

    return env->NewStringUTF(status.c_str());
}
