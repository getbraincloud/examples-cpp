#include <jni.h>
#include <string>

#include <json/json.h>

#include "braincloud/BrainCloudWrapper.h"
#include "braincloud/IRTTCallback.h"

#include "ids.h"

using namespace BrainCloud;

static BrainCloud::BrainCloudWrapper *pBCWrapper = nullptr;
static std::string status = "Authenticating...";
static std::string channelId;

//##############################################################################

class ChatCallback final : public IRTTCallback
{
public:
    void rttCallback(const Json::Value& eventJson) override
    {
        Json::FastWriter fastWriter;
        std::string output = fastWriter.write(eventJson);

        status = "RTT message callback:\n" + output;
    }
};
ChatCallback chatCallback;

//##############################################################################

class ChannelConnectCallback final : public IServerCallback
{
public:
    void serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string &jsonData) override
    {
        status = "channelConnect:\n" + jsonData;

        pBCWrapper->getBCClient()->registerRTTChatCallback(&chatCallback);
        pBCWrapper->getChatService()->postChatMessageSimple(channelId, "Hello from Android", true);
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status = "ERROR: channelConnect\n" + jsonError;
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

        status = "Channel id:\n" + channelId;

        pBCWrapper->getChatService()->channelConnect(channelId, 50, &channelConnectCallback);
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status = "ERROR: getChannelId\n" + jsonError;
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
        status = "RTT enabled!";
        pBCWrapper->getChatService()->getChannelId("gl", "valid", &getChannelIdCallback);
    }

    void rttConnectFailure(const std::string& errorMessage) override
    {
        status = "ERROR: enableRTT\n" + errorMessage;
    }
};
static RTTConnectCallback rttConnectCallback;

//##############################################################################

class AuthCallback final : public IServerCallback
{
public:
    void serverCallback(ServiceName serviceName, ServiceOperation serviceOperation, const std::string &jsonData) override
    {
        status = "Authenticated!\n" + jsonData;
        pBCWrapper->getBCClient()->enableRTT(&rttConnectCallback, false);
    }

    void serverError(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, const std::string &jsonError) override
    {
        status = "ERROR: authenticateUniversal\n" + jsonError;
    }

    void serverWarning(ServiceName serviceName, ServiceOperation serviceOperation, int statusCode, int reasonCode, int numRetries, const std::string &statusMessage) override {}
};
static AuthCallback authCallback;

//##############################################################################

extern "C" JNIEXPORT jstring JNICALL Java_com_bitheads_braincloud_android_MainActivity_mainLoopJNI(JNIEnv *env, jobject /* this */)
{
    // Initialize brainCloud
    if (!pBCWrapper)
    {
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
    }

    // Update braincloud
    pBCWrapper->runCallbacks();

    return env->NewStringUTF(status.c_str());
}
