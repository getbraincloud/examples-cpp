#include <jni.h>
#include <string>
#include <sstream>
#include <iostream>
#include <android/log.h>

#include <json/json.h>

#include "braincloud/BrainCloudWrapper.h"
#include "braincloud/IRTTCallback.h"

#include "ids.h"

using namespace BrainCloud;

static BrainCloud::BrainCloudWrapper *pBCWrapper = nullptr;
static std::string status = "Authenticating...\n\n";
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
    void rttCallback(const Json::Value& eventJson) override
    {
        Json::FastWriter fastWriter;
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

        pBCWrapper->getBCClient()->registerRTTChatCallback(&chatCallback);
        pBCWrapper->getChatService()->postChatMessageSimple(channelId, "Hello from Android", true);
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "ERROR: channelConnect: " + jsonError + "\n\n";
    }

    void serverWarning(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, int numRetries, const std::string &statusMessage) override {}
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

    void serverWarning(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, int numRetries, const std::string &statusMessage) override {}
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
        pBCWrapper->getBCClient()->enableRTT(&rttConnectCallback, true);
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status += "ERROR: authenticateUniversal: " + jsonError + "\n\n";
    }

    void serverWarning(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, int numRetries, const std::string &statusMessage) override {}
};
static AuthCallback authCallback;

//##############################################################################

extern "C" JNIEXPORT jstring JNICALL Java_com_bitheads_braincloud_android_MainActivity_mainLoopJNI(JNIEnv *env, jobject /* this */)
{
    status = "";

    // Initialize brainCloud
    if (!pBCWrapper)
    {
        std::cout.rdbuf(&consoleStream);

        pBCWrapper = new BrainCloud::BrainCloudWrapper("TestApp");
        pBCWrapper->initialize(
                BRAINCLOUD_SERVER_URL,
                BRAINCLOUD_APP_SECRET,
                BRAINCLOUD_APP_ID,
                "1.0",
                "bitHeads inc.",
                "TestApp");
        pBCWrapper->getBCClient()->enableLogging(true);

        // Authenticate
        pBCWrapper->authenticateUniversal("testAndroidUser", "qwertY123", true, &authCallback);

        status += "Authenticating...\n\n";
    }

    // Update braincloud
    pBCWrapper->runCallbacks();

    return env->NewStringUTF(status.c_str());
}
