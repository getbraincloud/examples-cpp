#include <condition_variable>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <aws/gamelift/server/GameLiftServerAPI.h>
#include <aws/gamelift/server/LogParameters.h>
#include <aws/gamelift/server/ProcessParameters.h>

#include <brainclouds2s.h>
#include <json/json.h>

using namespace std;
using namespace Aws::GameLift;
using namespace Aws::GameLift::Server;
using namespace Aws::GameLift::Server::Model;

// Prototypes
void onStartGameSession(GameSession gameSession);
void onUpdateGameSession(UpdateGameSession updateGameSession);
void onProcessTerminate();
bool onHealthCheck();

Json::Value sendLobbyRequest(const string &sysCall);

// Game Properties passed to us from brainCloud, through GameLift.
static string APP_ID;
static string SERVER_HOST;
static string SERVER_PORT;
static string SERVER_NAME;
static string SERVER_SECRET;
static string LOBBY_ID;

// The port used for players to connect to your server. We should pass
// this as an argument to the server launch.
static int port = 0;

// Synchronisation objects used to wait on Game Session Activation
static bool gameReady = false;
static mutex gameReadyMutex;
static condition_variable gameReadyCV;

// brainCloud S2S
static S2SContextRef s2s;
static Json::Value lobbyJson;

// Main server entrance
int main(int argc, char **argv)
{
    // Grab the players port from the arguments
    if (argc > 1) port = atoi(argv[argc - 1]);

    // Initialize GameLift SDK.
    // Some GameLift calls seem to return an error with
    // ALREADY_INITIALIZED. So make sure to check for that too.
    auto initOutcome = InitSDK();
    if (!initOutcome.IsSuccess() &&
        initOutcome.GetError().GetErrorType() != 
            GAMELIFT_ERROR_TYPE::ALREADY_INITIALIZED)
    {
        return 1;
    }

    // Create a list of log files that GameLift will copy after the
    // server Shutdown.
    vector<string> logPaths;
    logPaths.push_back("log_file.txt");

    // Game process parameters and callbacks
    ProcessParameters processParameters(onStartGameSession,
                                        onUpdateGameSession,
                                        onProcessTerminate,
                                        onHealthCheck,
                                        port,
                                        LogParameters(logPaths));

    // Let GameLift know that the process is ready.
    // Some GameLift calls seem to return an error with
    // ALREADY_INITIALIZED. So make sure to check for that too.
    auto readyOutcome = ProcessReady(processParameters);
    if (!readyOutcome.IsSuccess() && 
        readyOutcome.GetError().GetErrorType() !=
            GAMELIFT_ERROR_TYPE::ALREADY_INITIALIZED)
    {
        return 1;
    }

    // Wait for game ready
    {
        unique_lock<mutex> lock(gameReadyMutex);
        gameReadyCV.wait(lock, []() -> bool { return gameReady; });
    }

    // Initialize brainCloud S2S
    {
        auto url = "https://" + SERVER_HOST + 
                   ":" + SERVER_PORT + "/s2sdispatcher";
        s2s = S2SContext::create(APP_ID, 
                                 SERVER_NAME, 
                                 SERVER_SECRET, 
                                 url, 
                                 true);
        s2s->setLogEnabled(true);   
    }

    // Get Lobby from S2S
    lobbyJson = sendLobbyRequest("GET_LOBBY_DATA")["data"];

    // Initialize your socket listeners here, load level, etc.
    // ... TCP/UDP/Whatever

    // Now we are fully ready to accept players' connections,
    // let GameLift know.
    ActivateGameSession();

    // Now let brainCloud know through S2S. This will disband the
    // lobby and send players our way.
    sendLobbyRequest("SYS_ROOM_READY");

    // Game main loop.
    bool done = false;
    while (!done)
    {
        // ...
    }

    // Once gameplay is done, send stats to brainCloud through S2S
    // ... collect and send player stats, leaderboard, etc.

    // If the Lobby is backfille (Join in progress), we need to let
    // brainCloud know we're stopping the server. As a simple rule,
    // always do this.
    sendLobbyRequest("SYS_ROOM_STOPPED");

    // Let GameLift know we're about to shutdown. After this, 
    // GameLift will give us about 30sec to cleanup properly and 
    // then exit with code 0.
    ProcessEnding();

    return 0;
}

// Called by GameLift, after brainCloud created a new game session.
// Here we should grab the Game Properties.
void onStartGameSession(GameSession gameSession)
{
    // Get passed properties
    const auto &gameProperties = gameSession.GetGameProperties();
    for (const auto &game_property : gameProperties)
    {
        const auto &key = game_property.GetKey();
        const auto &value = game_property.GetValue();

        if (key == "APP_ID") APP_ID = value;
        if (key == "SERVER_HOST") SERVER_HOST = value;
        if (key == "SERVER_PORT") SERVER_PORT = value;
        if (key == "SERVER_NAME") SERVER_NAME = value;
        if (key == "SERVER_SECRET") SERVER_SECRET = value;
        if (key == "LOBBY_ID") LOBBY_ID = value;
    }

    // Notify our gameloop that we are ready
    unique_lock<mutex> lock(gameReadyMutex);
    gameReady = true;
    gameReadyCV.notify_one();
}

void onUpdateGameSession(UpdateGameSession updateGameSession) { }

// If GameLift decides to terminate your server
void onProcessTerminate()
{
    // .. do any shuting down steps required
    exit(0); // Terminate cleanly
}

bool onHealthCheck()
{
    return true; // Make sure we let GameLift know we're ok
}

// Helper function to serialize a lobby request and return its result
Json::Value sendLobbyRequest(const string &sysCall)
{
    Json::Value messageJson(Json::ValueType::objectValue);
    Json::Value dataJson(Json::ValueType::objectValue);

    // Construct our request JSON
    dataJson["lobbyId"] = LOBBY_ID;
    messageJson["service"] = "lobby";
    messageJson["operation"] = sysCall;
    messageJson["data"] = dataJson;

    // Convert JSON to string
    stringstream requestStream;
    requestStream << messageJson;
    string request = requestStream.str();

    // Send request sync. This is for simplicity of this example.
    // It is recommended to use async requests and rely on callbacks.
    string result = s2s->requestSync(request);

    // Convert string result to JSON
    Json::Value resultJson;
    stringstream resultStream(result);
    resultStream >> resultJson;

    return resultJson;
}
