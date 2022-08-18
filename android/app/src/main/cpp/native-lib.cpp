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

using namespace BrainCloud;

static BrainCloud::BrainCloudWrapper *pBCWrapper = nullptr;
static std::string status;
static std::string channelId;

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
        Json::FastWriter fastWriter;
        Json::Value eventJson(jsonData);
        std::string output = fastWriter.write(eventJson);

        status += "RTT message callback: " + output + "\n\n";
    }
};
ChatCallback chatCallback;

//##############################################################################

class ChannelConnectCallback final : public IServerCallback
{
public:
    void serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string &jsonData) override
    {
        status += "Connected to channel\n\n";

        pBCWrapper->getBCClient()->getRTTService()->registerRTTChatCallback(&chatCallback);
        pBCWrapper->getChatService()->postChatMessageSimple(channelId, "Hello from Android", true);
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "ERROR: channelConnect: " + jsonError + "\n\n";
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

        status += "Channel id: " + channelId + "\n\n";

        pBCWrapper->getChatService()->channelConnect(channelId, 50, &channelConnectCallback);
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "ERROR: getChannelId: " + jsonError + "\n\n";
    }
};
static GetChannelIdCallback getChannelIdCallback;

//##############################################################################

class RTTConnectCallback final : public IRTTConnectCallback
{
public:
    void rttConnectSuccess() override
    {
        status += "RTT enabled!\n\n";
        pBCWrapper->getChatService()->getChannelId("gl", "valid", &getChannelIdCallback);
    }

    void rttConnectFailure(const std::string& errorMessage) override
    {
        status += "ERROR: enableRTT: " + errorMessage  + "\n\n";
    }
};
static RTTConnectCallback rttConnectCallback;

//##############################################################################

class AuthCallback final : public IServerCallback
{
public:
    void serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string &jsonData) override
    {
        status += "Authenticated\n\n";
        Json::Reader reader;
        Json::Value data;
        reader.parse(jsonData, data);

        std::string profileId = data["data"]["profileId"].asString();

        std::string isNew = data["data"]["newUser"].asString();
        if(isNew.compare("true")==0) status += "Created new profile: " + profileId + "\n";
        else status += "Logged in existing profile: " + profileId + "\n";

        status += "Login count: ";
        status += data["data"]["loginCount"].asString();
        status += "\n";

        // TODO: make RTT work
        //pBCWrapper->getBCClient()->getRTTService()->enableRTT(&rttConnectCallback, true);
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "ERROR: authenticateUniversal: " + jsonError + "\n\n";
    }
};
static AuthCallback authCallback;

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

    // Initialize brainCloud
    if (!pBCWrapper) {
        std::cout.rdbuf(&consoleStream);

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

        pBCWrapper->getBCClient()->enableLogging(true);

        // Authenticate
        status += "Authenticating...\n";
        //pBCWrapper->authenticateEmailPassword("testAndroidUser", "qwertY123", true, &authCallback);
        pBCWrapper->authenticateAnonymous(&authCallback);
    }

    // Update braincloud
    pBCWrapper->runCallbacks();

    return env->NewStringUTF(status.c_str());
}
