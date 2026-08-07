//-----------------------------------------------------------------------------
// Copyright 2021 bitHeads inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//-----------------------------------------------------------------------------
// File: globals.h
// Desc: Defines global application state, data and constants
// Author: David St-Louis
//-----------------------------------------------------------------------------
#pragma once

// Imgui
#include <imgui.h>

// brainCloud
#include <braincloud/RelayConnectionType.h>

// C/C++ includes
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include <memory>

// brainCloud App settings. Create ids.h and define BRAINCLOUD_SERVER_URL, BRAINCLOUD_APP_ID and BRAINCLOUD_APP_SECRET
#include "ids.h"

// Max character count that can be used for username and password
#define MAX_CREDENTIAL_CHAR 32

// Total number of distinct player colors (supports up to this many simultaneous players)
#define NUM_COLORS 40

// Matches the vendored brainCloud C++ SDK's RelayComms::MAX_PLAYERS / INVALID_NET_ID (40) —
// a netId in [0, MAX_LOBBY_MEMBERS) is valid; MAX_LOBBY_MEMBERS itself is the SDK's
// "not found" sentinel. Used when packing a netId onto the wire (e.g. splotch ownership).
static constexpr int MAX_LOBBY_MEMBERS = 40;

// Color palette matching JS (#RRGGBB) and Java (Color.decode) exactly.
// Uses IM_COL32(r,g,b,a) which correctly packs into ImGui's ABGR uint32 format.
// Palette is spread across the hue wheel in four tonal rows:
//   Row 1 ( 0- 9): vivid saturated
//   Row 2 (10-19): vivid-medium / complementary hues
//   Row 3 (20-29): pastel / light
//   Row 4 (30-39): medium-depth / muted
static const ImVec4 COLORS[NUM_COLORS] = {
    // --- Row 1: vivid saturated ---
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0x33, 0x33, 0xFF)), // vivid red
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0x88, 0x00, 0xFF)), // vivid orange
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0xD7, 0x00, 0xFF)), // gold
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x88, 0xFF, 0x00, 0xFF)), // vivid lime
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x00, 0xEE, 0x44, 0xFF)), // vivid green
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x00, 0xDD, 0xDD, 0xFF)), // vivid cyan
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x00, 0xAA, 0xFF, 0xFF)), // vivid sky blue
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x33, 0x55, 0xFF, 0xFF)), // vivid blue
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xAA, 0x00, 0xFF, 0xFF)), // vivid purple
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0x00, 0xBB, 0xFF)), // vivid magenta
    // --- Row 2: vivid-medium / complementary hues ---
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0x55, 0x66, 0xFF)), // coral
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0xAA, 0x00, 0xFF)), // amber
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xAA, 0xDD, 0x00, 0xFF)), // yellow-green
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x00, 0xFF, 0x88, 0xFF)), // spring green
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x00, 0xFF, 0xCC, 0xFF)), // aqua
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x00, 0x88, 0xFF, 0xFF)), // azure
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x88, 0x33, 0xFF, 0xFF)), // violet
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0x44, 0xAA, 0xFF)), // hot pink
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x77, 0xFF, 0x33, 0xFF)), // chartreuse
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0x66, 0x88, 0xFF)), // rose
    // --- Row 3: pastel / light (readable on dark) ---
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0x99, 0x99, 0xFF)), // light red
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0xCC, 0x88, 0xFF)), // peach
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0xFF, 0x88, 0xFF)), // pale yellow
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xAA, 0xFF, 0xAA, 0xFF)), // pale green
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x88, 0xFF, 0xEE, 0xFF)), // pale cyan
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xAA, 0xBB, 0xFF, 0xFF)), // periwinkle
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xDD, 0xBB, 0xFF, 0xFF)), // lavender
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0xBB, 0xDD, 0xFF)), // light pink
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xCC, 0xFF, 0xDD, 0xFF)), // mint
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xFF, 0xEE, 0xCC, 0xFF)), // cream
    // --- Row 4: medium-depth / muted ---
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xCC, 0x11, 0x33, 0xFF)), // crimson
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xCC, 0x55, 0x00, 0xFF)), // burnt orange
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x88, 0xAA, 0x00, 0xFF)), // olive
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x22, 0x88, 0x55, 0xFF)), // forest green
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x00, 0x99, 0x99, 0xFF)), // deep teal
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x33, 0x66, 0xAA, 0xFF)), // steel blue
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x77, 0x44, 0xCC, 0xFF)), // medium purple
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xAA, 0x33, 0x66, 0xFF)), // dark rose
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0xAA, 0x66, 0x33, 0xFF)), // brown
    ImGui::ColorConvertU32ToFloat4(IM_COL32(0x77, 0x88, 0xAA, 0xFF)), // slate
};

// Runtime palette fetched from braincloud "Colours" property; empty = use COLORS[] fallback.
extern std::vector<ImVec4> g_colors;

inline int colorCount() { return g_colors.empty() ? NUM_COLORS : (int)g_colors.size(); }
inline ImVec4 getColor(int i) {
    if (!g_colors.empty()) return g_colors[i % (int)g_colors.size()];
    return COLORS[i % NUM_COLORS];
}

// Display diameter of the splotch sprite in game-world units — matches _DISPLAY_SIZE in Splotch.gd
static constexpr float SPLOTCH_DISPLAY_SIZE = 64.0f;

// Canvas / coverage-scoring geometry. All clients must agree on these exact values —
// they define the wire-normalized 0..1 coordinate space AND the coverage % denominator.
static constexpr float CANVAS_W = 800.0f;
static constexpr float CANVAS_H = 600.0f;
static constexpr float SPLOTCH_RADIUS = SPLOTCH_DISPLAY_SIZE * 0.5f; // 32 — splotch stamp radius for coverage scoring

// computeCoverage()'s ownership-grid cell size in canvas pixels — the resolution at which
// "who currently owns this bit of the board" is tracked. Small enough (relative to
// SPLOTCH_RADIUS) that the per-player % is a close read of the actual painted area, not a
// coarse approximation. All ports should use the same value so scores/behavior match.
static constexpr float COVERAGE_GRID_CELL_SIZE = 2.0f;

// Match timing (moved here from game.cpp so app_tickMatch() and the HUD can both see them
// regardless of where the timer widget is drawn).
static constexpr long long MATCH_DURATION_MS = 90000LL;
static constexpr long long RESULT_GRACE_MS = 1000LL;      // delay between match_result broadcast and endMatch()
static constexpr long long COVERAGE_RECOMPUTE_MS = 250LL; // live-board recompute throttle

// How long the post-match summary screen waits for everyone to queue for a rematch
// before the host starts the next round anyway (BCLOUD-14489).
static constexpr long long MATCH_SUMMARY_REMATCH_MS = 45000LL;

// Screen state enum.
enum class ScreenState : int
{
    Login, /* Login screen */
    LoggingIn,
    MainMenu, /* Main menu */
    JoiningLobby,
    Lobby, /* Lobby screen */
    Starting,
    Game, /* Game screen */
    MatchSummary /* Post-match results + rematch-queue screen (BCLOUD-14489) */
};

// A point in 2D space
struct Point
{
    int x, y;
};

// A brainCloud user
struct User
{
    std::string cxId; /* RTT Connection Id */
    std::string profileId; /* brainCloud profileId — needed server-side (postScoreToLeaderboardOnBehalfOf takes a profileId, not a cxId) */
    std::string name; /* User name */
    int colorIndex = 7;
    bool isReady = false;
    bool isAlive = false;
    Point pos = {0, 0};
    std::map<std::string, int> pings; /* per-region ping data shared via lobby extra */
    int activePing = -1;              /* live relay RTT broadcast during gameplay (ms); -1 = not yet received */
    int worldwideRank = -1;           /* this player's own rank on the coverage leaderboard, shared via lobby extra; -1 = unknown/no score yet */
};

// A chat message — used for both the global (brainCloud Chat service) channel and
// this-lobby (Lobby service SendSignal) chat.
struct ChatMessage
{
    std::string fromName;
    std::string text;
};

// Lobby
struct Lobby
{
    std::string lobbyId;
    std::string ownerCxId;
    std::string regionId; /* region extracted from lobbyId prefix (e.g. "na-east") */
    std::vector<User> members;
    std::vector<ChatMessage> chatMessages;                        /* this-lobby chat, via SendSignal — resets whenever Lobby is reset (state.lobby = Lobby()) */
    std::chrono::steady_clock::time_point arrivalTime;            /* when we entered this lobby, for the INFO tab's "time in lobby" */
};

// Server info
struct Server
{
    std::string host;
    int wsPort = -1;
    int tcpPort = -1;
    int udpPort = -1;
    int gameLiftPort = -1;
    int i3dPort = -1;
    std::string passcode;
    std::string lobbyId;
};

// Shockwaves user created with the mouse
struct Shockwave
{
    Point pos;
    int colorIndex;
    std::chrono::high_resolution_clock::time_point startTime;
};

// Permanent color splotch left behind by a shockwave
struct Splotch
{
    Point       pos;
    int         colorIndex;
    long long   startTimeMs;  /* ms since epoch — used for expiry and JIP sync */
    float       rotation;     /* radians — transmitted in relay message so all clients match */
    std::string ownerCxId;    /* match-scoped owner key for coverage attribution; empty = unattributed (legacy sender) */
};

static constexpr float SPLOTCH_TAU = 6.28318530f; // 2π — upper bound for random rotation

// One player's live/final standing from computeCoverage() (see coverage.h).
// coveragePct is an ABSOLUTE canvas-area percentage (not a share of painted area) —
// this keeps the "highest coverage ever" leaderboard meaningful for solo matches too.
struct CoverageEntry
{
    std::string cxId;
    int         colorIndex      = -1;
    int         visibleCount    = 0;    /* ownership-grid cells currently painted by this player (see coverage.h) */
    float       coveragePct     = 0.0f; /* clamped [0,100] */
    int         rank            = 1;    /* 1-based; ties share a rank */
    int         prevRank        = 1;    /* previous recompute's rank, for the rank-swap flash */
    long long   rankChangedAtMs = 0;    /* set when rank last differed from prevRank */
    int         beaten          = 0;    /* players strictly below this one (ties don't count) */
};

// One leaderboard period's (Lifetime or Quarterly) rank-before/after for a single board.
// "improved" is the trigger for whether the summary screen shows a badge at all — for
// the points boards that means the rank got numerically better; for the coverage boards
// it means this round's score replaced a lower personal best (see postMatchScoresAndComputeDeltas).
struct LeaderboardPeriodDelta
{
    bool improved   = false;
    int  rankBefore = -1; /* -1 = unranked/no score yet before this round */
    int  rankAfter  = -1;
};

// Personal leaderboard-rank movement from posting this round's score, across all four
// boards. Computed by each client for ITSELF only (there's no API to fetch an arbitrary
// other player's before/after rank) and broadcast to the rest of the match via the
// "lb_result" relay op — see postMatchScoresAndComputeDeltas / sendLeaderboardDeltaToMask.
struct LeaderboardDelta
{
    bool ready = false; /* true once this player's own delta has been computed (self) or received (others) */
    LeaderboardPeriodDelta pointsLifetime;
    LeaderboardPeriodDelta pointsQuarterly;
    LeaderboardPeriodDelta coverageLifetime;  /* "improved" = new coverage personal best */
    LeaderboardPeriodDelta coverageQuarterly;
};

// One player's entry in a host-broadcast, authoritative match_result.
struct MatchResultEntry
{
    std::string cxId;
    int         rank        = 0;
    float       coveragePct = 0.0f;
    int         beaten      = 0;
    LeaderboardDelta lbDelta; /* filled in asynchronously — see State::matchResult */
};

// Authoritative snapshot of a finished match's standings, broadcast by the (possibly
// migrated) host. Every client applies this once per round — see State::leaderboardPostedRound.
struct MatchResult
{
    bool                           valid = false;
    int                            round = -1;
    std::vector<MatchResultEntry>  entries;
};

// Host-side match lifecycle, driven by app_tickMatch(). Reset to Running on every new
// round (onRelayConnected) — non-host clients just sit in Running the whole match; only
// whichever client currently satisfies isHost drives the ResultsBroadcast/Ended steps.
enum class MatchPhase : int
{
    Running,          /* match clock ticking */
    ResultsBroadcast, /* match_result sent; waiting RESULT_GRACE_MS before endMatch() */
    Ended             /* endMatch() called for this round */
};

// Main application state. This contain all of the "live" data.
struct State
{
    ScreenState screenState = ScreenState::Login; /* Current screen we are on */
    User user;                                    /* Our user */
    Lobby lobby;                                  /* Lobby with its members as received from brainCloud Lobby Service */
    Server server;                                /* Server info (IP, port, protocol, passcode) */
    std::vector<Shockwave> shockwaves;            /* Players' created shockwaves */
    std::vector<Splotch> splotches;               /* Persistent splotches left by shockwaves */
    std::vector<std::string> appLobbies;          /* Lobby types fetched from AllLobbyTypes global property */
    std::vector<std::string> expectedPingRegions; /* Regions currently being pinged; empty when not in ping phase */
    std::vector<std::string> geoTestedRegions;    /* Regions already launched into during geo test cycling */
    std::map<std::string, int> geoTestResults;    /* region → relay RTT (ms) observed during geo test soak; -1 = not captured */
    std::map<std::string, int> pingData;          /* local player's own per-region ping measurements */
    int mouseX = 0;
    int mouseY = 0;
    long long gameStartTime = 0;  /* ms since epoch when current round started (0 = not in game) */
    int roundNumber = 0;          /* Increments each relay round within the same lobby session */
    bool pendingEndMatch = false; /* Deferred END_MATCH disconnect (cannot call disconnect inside relay callback) */
    bool pendingGeoTestDisconnect = false;                          /* Deferred auto-geo-test disconnect after relay connect confirmed */
    std::chrono::steady_clock::time_point geoTestLobbyArrivalTime; /* When we entered Lobby state during a geo test (for 1.5s auto-start delay) */
    std::chrono::steady_clock::time_point geoTestRelayConnectTime; /* When relay connected during a geo test (for 2.5s soak before disconnect) */
    int splotchDurationSec = -1;  /* -1 = forever; from SplotchDuration global property */

    // Coverage scoring / leaderboards (BCLOUD-14472 / BCLOUD-14490)
    unsigned long long splotchGeneration = 0;    /* bumped on every splotch add/clear/expiry-prune */
    unsigned long long coverageComputedGen = (unsigned long long)-1; /* generation coverage[] was last computed at */
    long long coverageComputedAtMs = 0;          /* wall-clock of last recompute, for the throttle */
    std::vector<CoverageEntry> coverage;         /* live, sorted rank board */

    MatchPhase matchPhase = MatchPhase::Running;
    long long resultsSentAtMs = 0;               /* when match_result was broadcast, for the grace period */
    MatchResult matchResult;                     /* authoritative result once applied (host or non-host) */
    int leaderboardPostedRound = -1;              /* guards against double-posting the CUMULATIVE points board */
    std::chrono::steady_clock::time_point matchSummaryArrivalTime; /* when the MatchSummary screen appeared, for the 15s auto-rematch countdown */
    bool awaitingRematch = false;   /* true from END_MATCH until the next round actually starts — gates app_tickRematchGate() */

    // Non-blocking "starting the next round" feedback (BCLOUD-14489 follow-up): the Lobby
    // screen stays up (chat/etc. still usable) through the whole STARTING->ROOM_READY
    // provisioning sequence instead of jumping to a blocking loading/cancel screen; this
    // is what the Lobby screen renders as a small inline status line while it's true.
    bool isProvisioning = false;
    std::string provisioningStatus;

    // Leaderboard ids — read from brainCloud global properties in applyLobbyTypes(), same
    // mechanism as AllLobbyTypes/Colours/SplotchDuration. Defaults let the app run before
    // the boards exist in the portal (posts will just fail server-side until configured).
    std::string coverageLeaderboardId          = "CursorParty_HighestCoverage";
    std::string coverageLeaderboardIdQuarterly = "CursorParty_HighestCoverage_Quarterly";
    std::string pointsLeaderboardId            = "CursorParty_Points";
    std::string pointsLeaderboardIdQuarterly   = "CursorParty_Points_Quarterly";
};

// Change this one line to switch the default lobby type everywhere.
static const std::string DEFAULT_LOBBY_TYPE = "CursorPartyV2";

struct Settings
{
    char username[MAX_CREDENTIAL_CHAR] = {'\0'};
    char password[MAX_CREDENTIAL_CHAR] = {'\0'};
    int colorIndex = 0;
    int gameUIIScale = 2;
    int sendChannel = 0;
    bool sendReliable = false;
    bool sendOrdered = true;
    int instanceIndex = 0;
    bool multiInstance = false; /* true when launched with instance/count args */
    bool autoJoin = false;
    bool autoLogin = true;
    bool usePingData = false;
    bool autoGeoTest = false; /* automatically cycle through all regions; disconnects after each relay connect */
    BrainCloud::eRelayConnectionType protocol = BrainCloud::eRelayConnectionType::UDP;
    std::string lobbyType = DEFAULT_LOBBY_TYPE;
    std::string teamCode = "all"; /* "all" for non-team lobbies, "alpha"/"beta" for team lobbies */
};

extern Settings settings;

// Load/Save configuration file from/to disk (./configs.txt or ./configs_N.txt)
// Returns true if a per-instance config file (configs_N.txt) was loaded.
bool loadConfigs();
void saveConfigs();

// Shared dark theme (rounded panels, navy palette) — call once after
// ImGui::StyleColorsDark(), from every entry point (mainSDL.cpp, mainUWP.cpp).
void applyTheme();

// True for any lobby type in the CursorParty family (name starts with "CursorParty").
// Add new CursorParty variants without touching any other file.
inline bool isCursorPartyLobby(const std::string &lobbyType)
{
    return lobbyType.find("CursorParty") == 0;
}

// Returns max lobby member count for the given lobby type.
// CursorParty variants support up to 40; all other types cap at 8.
inline int maxLobbyMembers(const std::string &lobbyType)
{
    return isCursorPartyLobby(lobbyType) ? 40 : 8;
}

// Extracts the region prefix from a brainCloud lobbyId (format: "region:LobbyType:N").
// Returns empty string if the lobbyId doesn't follow that convention.
inline std::string regionFromLobbyId(const std::string &lobbyId)
{
    auto pos = lobbyId.find(':');
    return (pos != std::string::npos && pos > 0) ? lobbyId.substr(0, pos) : "";
}

// True when the lobby type is the generic EdgeGap umbrella type.
// Selecting this type causes the app to ping EdgeGap beacon regions and then
// route to the best region-specific CP_E_* lobby type automatically.
inline bool isEdgeGapLobby(const std::string &lobbyType)
{
    return lobbyType == "CursorPartyEdgeGap";
}

// Maps an EdgeGap beacon region name to its corresponding specific lobby type.
// Returns an empty string if the region is unknown (caller should fall back to
// the original lobby type in that case).
inline std::string edgeGapRegionToLobbyType(const std::string &region)
{
    static const std::map<std::string, std::string> kMap = {
        {"asia-east", "CursorPartyEdgeGap_AsiaEast"},
        {"asia-south", "CursorPartyEdgeGap_AsiaSouth"},
        {"europe-central", "CursorPartyEdgeGap_Europe_Central"},
        {"na-east", "CursorPartyEdgeGap_NorthAmerica_East"},
        {"na-west", "CursorPartyEdgeGap_NorthAmerica_West"},
        {"sa-central", "CursorPartyEdgeGap_SouthAmerica_Central"},
        {"us-south", "CursorPartyEdgeGap_UnitedStates_South"},
    };
    auto it = kMap.find(region);
    return it != kMap.end() ? it->second : "";
}

// True when the lobby type is the generic GameLift umbrella type.
// Selecting this type causes the app to ping GameLift regions and then
// route to the best region-specific CursorPartyGameLift_* lobby type.
inline bool isGameLiftLobby(const std::string &lobbyType)
{
    return lobbyType == "CursorPartyGameLift";
}

// Maps a GameLift ping-region name to its corresponding specific lobby type.
// Returns an empty string if the region is unknown.
// NOTE: verify these region key strings against actual pingRegions() data for GameLift.
inline std::string gameLiftRegionToLobbyType(const std::string &region)
{
    static const std::map<std::string, std::string> kMap = {
        {"ca-central-1", "CursorPartyGameLift_canada"},
        {"eu-central-1", "CursorPartyGameLift_frankfurt"},
        {"eu-west-1", "CursorPartyGameLift_ireland"},
        {"us-west-2", "CursorPartyGameLift_oregon"},
    };
    auto it = kMap.find(region);
    return it != kMap.end() ? it->second : "";
}

// True when the lobby type is the generic CursorPartyV2 umbrella type.
// Selecting this type causes the app to ping V2 regions and then
// route to the best region-specific CursorPartyV2_* lobby type.
inline bool isV2RegionalLobby(const std::string &lobbyType)
{
    return lobbyType == "CursorPartyV2";
}

// Maps a V2 ping-region name to its corresponding specific lobby type.
// Returns an empty string if the region is unknown.
// NOTE: verify these region key strings against actual pingRegions() data for CursorPartyV2.
inline std::string v2RegionToLobbyType(const std::string &region)
{
    static const std::map<std::string, std::string> kMap = {
        // prod regions
        {"us-west-1", "CursorPartyV2_california"},
        {"ca-central-1", "CursorPartyV2_canada"},
        {"eu-central-1", "CursorPartyV2_frankfurt"},
        {"eu-south-1", "CursorPartyV2_milan"},
        {"ap-south-1", "CursorPartyV2_mumbai"},
        {"us-east-2", "CursorPartyV2_ohio"},
        {"eu-west-3", "CursorPartyV2_paris"},
        {"sa-east-1", "CursorPartyV2_south_america"},
        {"eu-south-2", "CursorPartyV2_spain"},
        {"eu-north-1", "CursorPartyV2_stockholm"},
        {"ap-southeast-2", "CursorPartyV2_sydney"},
        {"ap-northeast-1", "CursorPartyV2_tokyo"},
        // internal regions (absent on prod — skipped automatically when not returned by pingRegions)
        {"eu-west-1", "CursorPartyV2_ireland"},
        {"us-west-2", "CursorPartyV2_oregon"},

        {"canadacentral", "CursorPartyV2_canada"},
    };
    auto it = kMap.find(region);
    return it != kMap.end() ? it->second : "";
}

// True when the lobby type is a regional-cycling umbrella (EdgeGap, GameLift, or V2).
// These types use ping-based region cycling in the auto geo test.
// All other lobby types use passive recording (server-chosen region).
inline bool isRegionalCyclingLobby(const std::string &lobbyType)
{
    return isEdgeGapLobby(lobbyType) || isGameLiftLobby(lobbyType) || isV2RegionalLobby(lobbyType);
}

// Maps a ping region to the specific lobby type for the given umbrella lobby.
// Dispatches to the correct regional map based on the umbrella type.
// Returns empty string if the region has no defined specific lobby.
inline std::string regionToSpecificLobbyType(const std::string &umbrellaLobby, const std::string &region)
{
    if (isEdgeGapLobby(umbrellaLobby))
        return edgeGapRegionToLobbyType(region);
    if (isGameLiftLobby(umbrellaLobby))
        return gameLiftRegionToLobbyType(region);
    if (isV2RegionalLobby(umbrellaLobby))
        return v2RegionToLobbyType(region);
    return "";
}

// Main application state instance
extern State state;

// Main window's dimensions
extern int width;
extern int height;

// Arrow textures
extern ImTextureID ARROWS[8];

// Splotch texture (PaintSplatter1.png — white-on-transparent, tinted at draw time)
extern ImTextureID SPLOTCH_TEX;
